#!/usr/bin/env bash
# Linux/WSL runner. Default: compile/link only; --run opts into bounded execution.
set -euo pipefail
cd "$(dirname "$0")/.."
run=false
if [[ "${1:-}" == --run ]]; then run=true; shift; fi
if [[ $# != 0 ]]; then echo 'Usage: bash tests/run-linux-tests.sh [--run]' >&2; exit 2; fi
mkdir -p build/linux-tests
build_dir=$(mktemp -d "$PWD/build/linux-tests/run-XXXXXX")
sources=(
    Compiler/main.cpp Semantics/MethodLower.cpp Semantics/SymbolScopeBuilder.cpp
    Semantics/Resolver.cpp Semantics/ReferenceChecker.cpp Semantics/ClosureAnalyser.cpp
    Semantics/TypeInterner.cpp Semantics/DeclTypeResolver.cpp Compiler/compiler.cpp
    BytecodeVM/vm.cpp Utils/NativeFunctionImpl.cpp Utils/utils.cpp Utils/FrontendJson.cpp
    Utils/PrettyAst.cpp Utils/SymbolPrinter.cpp Utils/DefaultVisitor.cpp Core/lexer.cpp
    Core/newParser.cpp Core/Ast.cpp Core/value.cpp CodegenIR/StringPool.cpp
    CodegenIR/IRBuilder.cpp SSA/SSABuilder.cpp MachineIR/MIRBuilder.cpp X86Codegen/x86Builder.cpp
)
if [[ -n "${KMYC:-}" ]]; then
    compiler="$KMYC" # Optional existing native Linux compiler for quick reruns.
else
    compiler="$build_dir/kmyc"
    g++ -std=c++20 "${sources[@]}" -o "$compiler"
fi
gcc -Wall -Wextra -Werror -fsyntax-only 'Runtime C Functions/runtime.c'

# Each entry is source|expected output. Includes hidden env + overflow stack args.
tests=(
    'tests/x86/linux_abi.kmy|tests/x86/linux_abi.expected'
    'tests/x86/control_flow.kmy|tests/x86/control_flow.expected'
    'tests/x86/closures.kmy|tests/x86/closures.expected'
    'tests/x86/arrays.kmy|tests/x86/arrays.expected'
    'tests/x86/pointers.kmy|tests/x86/pointers.expected'
    'tests/x86/free.kmy|tests/x86/free.expected'
    'Examples/DynamicArray.kmy|tests/x86/dynamic_array.expected'
    'Examples/StringOps.kmy|tests/x86/string_ops.expected'
    'tests/x86/modules/main.kmy|tests/x86/modules.expected'
)
ulimit -c 0 # A failing test must not leave a potentially huge core dump.
ulimit -v 262144 # Cap generated-test address space at 256 MiB.
for test in "${tests[@]}"; do
    IFS='|' read -r source expected <<< "$test"
    name=$(basename "$expected" .expected)
    executable="$build_dir/$name"
    timeout --kill-after=1s 30s "$compiler" "$source" --linux -o "$executable" \
        > "$executable.compile.log" 2>&1
    readelf -h "$executable" > "$executable.elf.txt"
    grep -q 'ELF64' "$executable.elf.txt"
    grep -q 'Advanced Micro Devices X86-64' "$executable.elf.txt"
    if $run; then
        timeout --kill-after=1s 5s "$executable" > "$executable.stdout" 2> "$executable.stderr"
        # Repository expected files may have Windows CRLF endings.
        actual=$(tr -d '\r' < "$executable.stdout")
        expected_output=$(tr -d '\r' < "$expected")
        # Command substitution removes trailing newlines, like the Windows runner.
        if [[ "$actual" != "$expected_output" ]]; then
            diff -u <(printf '%s\n' "$expected_output") <(printf '%s\n' "$actual")
            exit 1
        fi
        echo "PASS run $name"
    else
        echo "PASS ELF link $name"
    fi
done
echo "Artifacts: $build_dir"
