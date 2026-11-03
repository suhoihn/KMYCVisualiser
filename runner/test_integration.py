"""Native integration checks: run with the Linux compiler path in the environment."""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import unittest

from job import validate


class NativeIntegration(unittest.TestCase):
    def submit(self, files, **limits):
        environment = {**os.environ, "KMY_WORK_ROOT": "/tmp", "KMY_HEAP_BYTES": "4194304",
                       "KMY_STACK_BYTES": "4194304", "KMY_EXECUTION_MODE": "run", **limits}
        completed = subprocess.run([sys.executable, str(Path(__file__).with_name("job.py")), "--stdin"],
                                   input=json.dumps({"files": files, "entryFile": "main.kmy"}),
                                   text=True, capture_output=True, env=environment, timeout=35)
        events = [json.loads(line) for line in completed.stdout.splitlines()]
        errors = [event for event in events if event["type"] == "error"]
        self.assertFalse(errors, errors)
        return events

    def test_native_output_and_causal_recording(self):
        events = self.submit({"main.kmy": "print(1 + 2 * 3);"})
        analysis = next(event["analysis"] for event in events if event["type"] == "analysis")
        self.assertEqual(analysis["schemaVersion"], 2)
        stack = []
        known = set()
        for event in analysis["parser"]["events"]:
            if event["kind"] == "enter":
                self.assertEqual(event["parentCallId"], stack[-1] if stack else -1)
                stack.append(event["callId"])
            elif event["kind"] == "exit":
                self.assertEqual(stack.pop(), event["callId"])
            elif event["kind"] == "createNode":
                node = event["node"]
                for child in node["children"]:
                    if child["node"].get("ref"):
                        self.assertIn(child["node"]["id"], known, "Child must exist before its parent")
                self.assertNotIn(node["id"], known)
                known.add(node["id"])
        self.assertFalse(stack)
        self.assertIn(analysis["ast"]["id"], known)
        result = next(event["execution"] for event in events if event["type"] == "execution")
        self.assertEqual(result["stdout"], "7\n")
        self.assertEqual(result["exitCode"], 0)

    def test_imported_function_and_for_lowering(self):
        events = self.submit({"main.kmy": 'import "lib/math.kmy" as math; var total = 0; for (var i = 0; i < 3; i += 1) { total += math::twice(i); } print(total);',
                              "lib/math.kmy": "fun twice(value: int): int { return value * 2; }"})
        execution = next(event["execution"] for event in events if event["type"] == "execution")
        self.assertEqual(execution["stdout"], "6\n")
        main = next(event["analysis"] for event in events if event["type"] == "analysis" and event["file"] == "main.kmy")
        self.assertTrue(any(event.get("node", {}).get("kind") == "While" for event in main["parser"]["events"]))

    def test_syntax_failure_retains_partial_construction(self):
        events = self.submit({"main.kmy": "let good = 1; print(2 + );"})
        analysis = next(event["analysis"] for event in events if event["type"] == "analysis")
        self.assertFalse(analysis["ok"])
        self.assertTrue(any(event["kind"] == "createNode" for event in analysis["parser"]["events"]))
        self.assertTrue(any(event["kind"] == "errorExit" for event in analysis["parser"]["events"]))
        self.assertEqual(events[-1]["status"], "syntax_error")

    def test_runtime_timeout(self):
        events = self.submit({"main.kmy": "while (true) { }"})
        execution = next(event["execution"] for event in events if event["type"] == "execution")
        self.assertEqual(execution["limit"], "timeout")
        self.assertEqual(events[-1]["status"], "runtime_error")

    def test_compile_only_never_runs_the_executable(self):
        events = self.submit({"main.kmy": "while (true) { }"}, KMY_EXECUTION_MODE="compile")
        self.assertTrue(any(event["type"] == "analysis" for event in events))
        self.assertEqual(next(event["compilation"] for event in events if event["type"] == "compilation")["exitCode"], 0)
        self.assertFalse(any(event["type"] == "execution" or event.get("phase") == "execution" for event in events))
        self.assertEqual(events[-1]["status"], "compiled")

    def test_heap_limit_preserves_trace_and_compile_result(self):
        events = self.submit({"main.kmy": "var p: int* = malloc(4194305); print(1);"})
        self.assertTrue(next(event["analysis"] for event in events if event["type"] == "analysis")["parser"]["events"])
        self.assertEqual(next(event["compilation"] for event in events if event["type"] == "compilation")["exitCode"], 0)
        execution = next(event["execution"] for event in events if event["type"] == "execution")
        self.assertEqual(execution["limit"], "heap_limit")
        self.assertIn("exceeded its heap limit", execution["stderr"])
        self.assertEqual(events[-1]["status"], "runtime_error")

    def test_free_restores_configurable_heap_budget(self):
        source = "var p: int* = malloc(60000); free(p); var q: int* = malloc(60000); *q = 7; print(*q); free(q);"
        events = self.submit({"main.kmy": source}, KMY_HEAP_BYTES="65536")
        execution = next(event["execution"] for event in events if event["type"] == "execution")
        self.assertEqual(execution["stdout"], "7\n")
        self.assertEqual(events[-1]["status"], "success")

    def test_heap_covers_generated_array_allocation(self):
        events = self.submit({"main.kmy": "var length = 600000; var values = new int[length]; print(values[0]);"})
        execution = next(event["execution"] for event in events if event["type"] == "execution")
        self.assertEqual(execution["limit"], "heap_limit")

    def test_stack_limit_is_configurable_and_only_applies_to_program(self):
        source = "fun recurse(n: int): int { if (n == 0) { return 0; } return 1 + recurse(n - 1); } print(recurse(20000));"
        files = {"main.kmy": source}
        normal = self.submit(files)
        self.assertEqual(next(event["execution"] for event in normal if event["type"] == "execution")["stdout"], "20000\n")
        limited = self.submit(files, KMY_STACK_BYTES="65536")
        self.assertEqual(next(event["compilation"] for event in limited if event["type"] == "compilation")["exitCode"], 0)
        execution = next(event["execution"] for event in limited if event["type"] == "execution")
        self.assertEqual(execution["exitCode"], -signal.SIGSEGV)
        self.assertEqual(limited[-1]["status"], "runtime_error")

    def test_semantic_failure_is_a_compile_result(self):
        events = self.submit({"main.kmy": "print(unknown_name);"})
        compilation = next(event["compilation"] for event in events if event["type"] == "compilation")
        self.assertNotEqual(compilation["exitCode"], 0)
        self.assertTrue(compilation["stderr"])
        self.assertFalse(any(event["type"] == "execution" for event in events))
        self.assertEqual(events[-1]["status"], "compile_error")

    def test_output_limit(self):
        events = self.submit({"main.kmy": 'while (true) { print("hello"); }'})
        execution = next(event["execution"] for event in events if event["type"] == "execution")
        self.assertEqual(execution["limit"], "output_limit")
        self.assertLessEqual(len(execution["stdout"].encode()), 65536)

    def test_missing_and_escaping_imports_preserve_recording_without_execution(self):
        for path in ("missing.kmy", "../outside.kmy", "/etc/passwd"):
            events = self.submit({"main.kmy": f'import "{path}" as module; print(1);'})
            self.assertTrue(next(event["analysis"] for event in events if event["type"] == "analysis")["ok"])
            compilation = next(event["compilation"] for event in events if event["type"] == "compilation")
            self.assertEqual(compilation["exitCode"], 1)
            self.assertFalse(any(event["type"] == "execution" for event in events))

    def test_reject_paths_and_budget(self):
        with self.assertRaises(ValueError):
            validate({"files": {"../escape.kmy": ""}, "entryFile": "../escape.kmy"})
        with self.assertRaises(ValueError):
            validate({"files": {"main.kmy": "x" * 65537}})


if __name__ == "__main__":
    unittest.main(verbosity=2)
