#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
output="${1:-../release/linux-x86_64}"
mkdir -p "$output/Runtime C Functions"
sources=(Compiler/main.cpp Semantics/MethodLower.cpp Semantics/SymbolScopeBuilder.cpp
 Semantics/Resolver.cpp Semantics/ReferenceChecker.cpp Semantics/ClosureAnalyser.cpp
 Semantics/TypeInterner.cpp Semantics/DeclTypeResolver.cpp Compiler/compiler.cpp
 BytecodeVM/vm.cpp Utils/NativeFunctionImpl.cpp Utils/utils.cpp Utils/FrontendJson.cpp
 Utils/PrettyAst.cpp Utils/SymbolPrinter.cpp Utils/DefaultVisitor.cpp Core/lexer.cpp
 Core/newParser.cpp Core/Ast.cpp Core/value.cpp CodegenIR/StringPool.cpp
 CodegenIR/IRBuilder.cpp SSA/SSABuilder.cpp MachineIR/MIRBuilder.cpp X86Codegen/x86Builder.cpp)
g++ -std=c++20 -O1 -static-libstdc++ -static-libgcc "${sources[@]}" -o "$output/kmyc.build"
mv "$output/kmyc.build" "$output/kmyc"
cp 'Runtime C Functions/runtime.c' "$output/Runtime C Functions/runtime.c"
cp LINUX_BINARY_README.md "$output/README.md"
chmod +x "$output/kmyc"
echo "Linux compiler: $output/kmyc"
