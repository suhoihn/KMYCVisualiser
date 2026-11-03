"""One bounded native compiler job. Runs inside a disposable Linux sandbox."""
import base64
import json
import os
from pathlib import Path, PurePosixPath
import re
import resource
import selectors
import signal
import subprocess
import sys
import tempfile
import time

MAX_SOURCE = 65536
MAX_OUTPUT = 65536
MAX_RESULT = 8 * 1024 * 1024
MAX_ASSEMBLY = 256 * 1024
DEFAULT_MEMORY_BYTES = 4 * 1024 * 1024
HEAP_LIMIT_EXIT = 86


def memory_budget(name):
    value = int(os.environ.get(name, str(DEFAULT_MEMORY_BYTES)))
    if not 65536 <= value <= 64 * 1024 * 1024:
        raise ValueError(f"{name} must be between 65536 and 67108864 bytes")
    return value


def emit(event):
    line = json.dumps(event, ensure_ascii=False)
    if len(line.encode()) > MAX_RESULT:
        raise ValueError("Analysis exceeds the 8 MiB response limit")
    print(line, flush=True)


def read_assembly(path):
    with path.open("rb") as output:
        preview = output.read(MAX_ASSEMBLY + 1)
    return {"text": preview[:MAX_ASSEMBLY].decode("utf-8", errors="replace"),
            "truncated": len(preview) > MAX_ASSEMBLY}


def process(args, cwd, seconds, limit, *, stack_bytes=None):
    started = time.monotonic()
    # job.py is a single-threaded Linux worker. Set both limits in the child
    # before exec, without changing the compiler, linker or worker's stack.
    def restrict_program():
        resource.setrlimit(resource.RLIMIT_STACK, (stack_bytes, stack_bytes))
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

    child = subprocess.Popen(args, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             stdin=subprocess.DEVNULL, start_new_session=True,
                             preexec_fn=restrict_program if stack_bytes is not None else None,
                             env={"LANG": "C"} if stack_bytes is not None else None)
    streams = selectors.DefaultSelector()
    streams.register(child.stdout, selectors.EVENT_READ, "stdout")
    streams.register(child.stderr, selectors.EVENT_READ, "stderr")
    output = {"stdout": bytearray(), "stderr": bytearray()}
    reason = None
    try:
        while streams.get_map():
            if time.monotonic() - started > seconds:
                reason = "timeout"
                break
            for key, _ in streams.select(0.05):
                data = os.read(key.fd, 16384)
                if not data:
                    streams.unregister(key.fileobj)
                else:
                    output[key.data].extend(data)
                    if sum(map(len, output.values())) > limit:
                        reason = "output_limit"
                        break
            if reason:
                break
    finally:
        if not reason:
            try:
                child.wait(timeout=0.2)
            except subprocess.TimeoutExpired:
                reason = "timeout"
        # Also kill descendants which kept output pipes open after the root exited.
        try:
            os.killpg(child.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        child.wait()
        streams.close()
    if stack_bytes is not None and reason is None and child.returncode == HEAP_LIMIT_EXIT:
        reason = "heap_limit"
    return {"exitCode": child.returncode, "stdout": bytes(output["stdout"][:limit]).decode(errors="replace"),
            "stderr": bytes(output["stderr"][:limit]).decode(errors="replace"),
            "durationMs": round((time.monotonic() - started) * 1000), "limit": reason}


def validate(request):
    files = request.get("files")
    entry = request.get("entryFile", "main.kmy")
    if not isinstance(files, dict) or not 1 <= len(files) <= 16:
        raise ValueError("Submit between 1 and 16 source files")
    total = 0
    for name, source in files.items():
        if not isinstance(name, str) or len(name) > 180 or not re.fullmatch(r"[A-Za-z0-9_./-]+\.kmy", name):
            raise ValueError("Invalid .kmy file path")
        path = PurePosixPath(name)
        if path.is_absolute() or ".." in path.parts or str(path) != name:
            raise ValueError("Paths must stay inside the project")
        if not isinstance(source, str):
            raise ValueError("Source must be text")
        total += len(source.encode())
    if total > MAX_SOURCE or entry not in files:
        raise ValueError("Project exceeds 64 KiB or entry file is missing")
    if request.get("action", "run") not in ("run", "analyse"):
        raise ValueError("Unknown action")
    return files, entry


def job(request):
    files, entry = validate(request)
    mode = os.environ.get("KMY_EXECUTION_MODE", "compile")
    if mode not in ("compile", "run"):
        raise ValueError("Invalid KMY_EXECUTION_MODE")
    compiler = Path(os.environ.get("KMY_COMPILER_PATH", "/opt/kmy/kmyc")).resolve()
    compiler_root = compiler.parent
    work_root = os.environ.get("KMY_WORK_ROOT", "/work")
    Path(work_root).mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="job-", dir=work_root) as temporary:
        root = Path(temporary)
        src = root / "src"
        src.mkdir()
        for name, source in files.items():
            target = src / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(source, encoding="utf-8")
        analyses = {}
        import_errors = []
        for name in files:
            emit({"type": "phase", "phase": "analysis", "file": name})
            result = process([str(compiler), str(src / name), "--frontend-json"], compiler_root, 10, MAX_RESULT)
            if result["limit"]:
                raise ValueError("Analysis exceeded its time or output limit")
            try:
                analysis = json.loads(result["stdout"])
            except ValueError:
                raise ValueError("Compiler did not return analysis JSON: " + result["stderr"][:1000])
            # Imports are compiler tokens, not a second grammar in the backend.
            tokens = analysis.get("tokens", [])
            for index, token in enumerate(tokens[:-1]):
                if token.get("type") == "KeywordImport":
                    imported = tokens[index + 1].get("lexeme", "")
                    resolved = (src / name).parent / imported
                    if not resolved.resolve().is_relative_to(src.resolve()):
                        import_errors.append("Import escapes the submitted project: " + imported)
                    elif resolved.resolve().relative_to(src.resolve()).as_posix() not in files:
                        import_errors.append("Imported file was not submitted: " + imported)
            analyses[name] = analysis
            emit({"type": "analysis", "file": name, "analysis": analysis})
        if request.get("action", "run") == "analyse":
            emit({"type": "result", "status": "analysed"})
            return
        if any(not analysis.get("ok") for analysis in analyses.values()):
            emit({"type": "result", "status": "syntax_error"})
            return
        if import_errors:
            emit({"type": "phase", "phase": "compile"})
            emit({"type": "compilation", "compilation": {"exitCode": 1, "stdout": "", "stderr": "\n".join(import_errors), "durationMs": 0, "limit": None}})
            emit({"type": "result", "status": "compile_error"})
            return
        executable = root / "program"
        heap_bytes = memory_budget("KMY_HEAP_BYTES")
        stack_bytes = memory_budget("KMY_STACK_BYTES")
        emit({"type": "phase", "phase": "compile"})
        # Emit assembly, then invoke GCC with an argument list (no shell command).
        compiled = process([str(compiler), str(src / entry), "--linux", "--no-run", "-o", str(executable)],
                           compiler_root, 10, MAX_OUTPUT)
        if compiled["exitCode"] == 0 and not compiled["limit"]:
            assembly_path = executable.with_suffix(".s")
            assembly = read_assembly(assembly_path) if assembly_path.is_file() else None
            emit({"type": "phase", "phase": "link"})
            linked = process(["gcc", str(assembly_path), str(compiler_root / "Runtime C Functions/runtime.c"),
                              str(Path(__file__).with_name("heap_limit.c").resolve()),
                              f"-DKMY_HEAP_BYTES={heap_bytes}",
                              "-Wl,--wrap=malloc,--wrap=calloc,--wrap=free",
                              "-o", str(executable)], compiler_root, 10, MAX_OUTPUT)
            compiled = {**linked, "stdout": compiled["stdout"] + linked["stdout"],
                        "stderr": compiled["stderr"] + linked["stderr"],
                        "durationMs": compiled["durationMs"] + linked["durationMs"]}
            if assembly is not None:
                compiled["assembly"] = assembly
        emit({"type": "compilation", "compilation": compiled})
        if compiled["exitCode"] != 0 or compiled["limit"]:
            emit({"type": "result", "status": "compile_error"})
            return
        if mode == "compile":
            emit({"type": "result", "status": "compiled"})
            return
        emit({"type": "phase", "phase": "execution"})
        executed = process([str(executable)], root, 2, MAX_OUTPUT, stack_bytes=stack_bytes)
        emit({"type": "execution", "execution": executed})
        emit({"type": "result", "status": "success" if executed["exitCode"] == 0 and not executed["limit"] else "runtime_error"})


if __name__ == "__main__":
    try:
        if "--stdin" in sys.argv:
            raw = sys.stdin.buffer.read(131073)
        else:
            encoded = "".join(os.environ.get("KMY_PAYLOAD_" + str(i), "") for i in range(8))
            raw = base64.b64decode(encoded, validate=True)
        if len(raw) > 131072:
            raise ValueError("Request is too large")
        job(json.loads(raw))
    except Exception as error:
        emit({"type": "error", "message": str(error)[:2000]})
        sys.exit(1)
