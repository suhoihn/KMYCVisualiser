# Repository guide

Start at `frontend/src/App.tsx` for the UI, `runner/job.py` for the submission pipeline, and `compiler/Compiler/main.cpp` for the compiler. Paths below are relative to this repo. Paired C++ files are listed together: `.hpp` declares the interface or data structure, `.cpp` implements it unless noted.

## Root and deployment

| File | Job |
| --- | --- |
| `README.md` | Project overview, pipeline, replay, and local setup. |
| `FILES.md` | This source map. |
| `.gitignore` | Keeps builds, bundles, caches, and local logs out of Git. |
| `.gitattributes` | Preserves Linux shell-script line endings. |
| `.dockerignore` | Excludes local caches, compiler sources, and archives from the runtime image build context. |
| `compose.yaml` | Defines web, Spring, and dispatcher services plus a build-profile job image; sets memory, network, and mode defaults. |
| `.github/workflows/playground.yml` | Builds/tests on GitHub, publishes on `main`, and optionally deploys a tested image set. |
| `bundle.ps1` | Builds or packages the Windows-to-Linux upload archives as a manual alternative to CI. |
| `dev.cmd` | Command Prompt entry point for `dev.ps1`. |
| `dev.ps1` | Starts/stops/status-checks the local React and Spring processes it owns. |
| `deploy/README.md` | Manual Docker and local WSL setup, architecture, and limits. |
| `deploy/CICD.md` | GitHub, registry, and VPS setup; release and rollback instructions. |
| `deploy/Dockerfile.web` | Packages React's built files with Nginx. |
| `deploy/Dockerfile.backend` | Packages the Spring JAR with Java 17. |
| `deploy/Dockerfile.runner` | Packages the private Python dispatcher. |
| `deploy/Dockerfile.job` | Packages `kmyc`, GCC, the KMY runtime, and worker code for disposable job containers. |
| `deploy/nginx.conf` | Serves React, exposes only the playground API, limits uploads/submissions, and forwards the response stream. |
| `deploy/deploy.sh` | Pulls a release by digest, starts Compose, smoke-tests it, and attempts rollback on failure. |
| `deploy/smoke.py` | Submits known KMY source over HTTP; checks parser trace, native compile success, and no execution in deployment mode. |

## React (`frontend/`)

| File | Job |
| --- | --- |
| `index.html` | Browser document and React mount point. |
| `package.json`, `package-lock.json` | Scripts and dependencies; lockfile makes CI installs repeatable. |
| `vite.config.ts` | Vite setup and development API proxy. The `.js` and `.d.ts` siblings and `*.tsbuildinfo` are TypeScript outputs, not separate features. |
| `tsconfig.json`, `tsconfig.app.json`, `tsconfig.node.json` | Project references, app type-checking settings, and Vite tooling settings respectively. |
| `src/main.tsx` | Boots React and loads the app. |
| `src/App.tsx` | Editor/workspace layout, submission state, stage selector, timeline controls, and result panels. |
| `src/api.ts` | Sends source and incrementally decodes newline-delimited API events. |
| `src/types.ts` | Shared frontend shapes for tokens, AST nodes, trace events, and process results. |
| `src/styles.css` | Page layout and presentation, including timeline and AST styling. |
| `src/kmyLanguage.ts` | Editor highlighting and language configuration. It is not the authoritative parser. |
| `src/moduleWorkspace.ts` | Workspace file handling and module-aware helpers. |
| `src/walkthrough.ts` | Builds the timeline and reconstructs the parser call stack and partial AST for a selected event. |
| `src/components/KmyEditor.tsx` | CodeMirror editor and source highlighting. |
| `src/components/ParserConstruction.tsx` | Displays the current parser step and partial tree. |
| `src/components/AstGraph.tsx` | Graphical AST layout and interactions. |
| `src/components/AstTree.tsx` | Text outline of the AST. |
| `src/components/LanguageGuide.tsx` | In-app KMY syntax notes and examples. |
| `scripts/check-replay.mjs` | Tests that real compiler events reconstruct the final AST and can be replayed or rewound. |

## Spring and runner

| File | Job |
| --- | --- |
| `backend/build.gradle.kts` | Kotlin/Spring dependencies, Java 17 target, and test configuration. |
| `backend/settings.gradle.kts` | Gradle project identity. |
| `backend/gradlew`, `backend/gradlew.bat` | Linux and Windows Gradle wrapper launchers. |
| `backend/gradle/wrapper/gradle-wrapper.jar`, `gradle-wrapper.properties` | Wrapper bootstrap binary and pinned Gradle distribution settings (both under the wrapper directory). |
| `backend/src/main/resources/application.yml` | Port, stream timeout, runner URL, and legacy compiler configuration. |
| `KmyVisualiserApplication.kt` | Spring entry point, under `backend/src/main/kotlin/dev/kmy/visualiser/`. |
| `analysis/PlaygroundController.kt` | Current API: checks the request, forwards to the private runner, streams the result. |
| `analysis/AnalysisController.kt` | Legacy direct analysis endpoint; not routed by production Nginx. |
| `analysis/AnalysisRequest.kt` | Input model for the legacy endpoint. |
| `analysis/CompilerAnalysisService.kt` | Invokes the configured compiler for legacy analysis. |
| `analysis/CompilerProperties.kt` | Typed legacy compiler settings. |
| `PlaygroundControllerTest.kt`, `CompilerAnalysisServiceTest.kt` | Backend tests for streaming/request handling and legacy compiler invocation, under `backend/src/test/`. |
| `runner/server.py` | Admission, rate limit, Docker job lifecycle, and forwarding events. This is the only service with the Docker socket. |
| `runner/job.py` | Validates source, stages project files, runs parser analysis and compilation, links, and optionally executes with resource limits. |
| `runner/heap_limit.c` | Wraps the linked program's `malloc`, `calloc`, and `free` to enforce a live allocation quota. |
| `runner/test_integration.py` | Tests actual Linux compilation, replay data, module handling, and execution/limits. |
| `runner/check_bundle.py` | Verifies manually produced Linux archives and smoke-tests their compiler. |

## Compiler snapshot (`compiler/`)

| File | Job |
| --- | --- |
| `build-linux.sh`, `LINUX_BINARY_README.md` | Build the Linux executable/runtime pair and document the standalone distribution. |
| `Compiler/main.cpp` | CLI orchestration, analysis JSON mode, module loading and output modes. |
| `Compiler/compiler.{hpp,cpp}` | Legacy bytecode compiler retained by the snapshot. |
| `Core/lexer.{hpp,cpp}` | Turns source characters into tokens. |
| `Core/tokens.hpp` | Token kinds and representation. |
| `Core/newParser.{hpp,cpp}` | Active parser: statements, declarations, and Pratt expression parsing. |
| `Core/parser.{hpp,cpp}` | Older parser retained in the snapshot; not the visualiser's parser. |
| `Core/ParserTrace.hpp` | Tracks parser call scopes, decisions, token consumption, and node creation. |
| `Core/Ast.{hpp,cpp}`, `Core/AstBaseForward.hpp` | AST node types/implementation and common node interfaces and declarations. |
| `Core/visitor.hpp`, `Utils/DefaultVisitor.{hpp,cpp}` | AST visitor interface and default traversal. |
| `Core/type.hpp`, `Core/value.{hpp,cpp}` | Language type and value representations. |
| `Core/Symbol.hpp`, `Core/Scope.hpp` | Symbol metadata and scoped name lookup structures. |
| `Core/operators.hpp`, `Core/errorhandler.hpp` | Operator definitions and compiler error types. |
| `Core/VariableStorage.hpp`, `Core/FunctionContext.hpp`, `Core/CodegenInfo.hpp` | Storage, function context, and lowering metadata. |
| `Semantics/SymbolScopeBuilder.{hpp,cpp}`, `Semantics/Resolver.{hpp,cpp}` | Build scopes and resolve names and uses. |
| `Semantics/DeclTypeResolver.{hpp,cpp}`, `Semantics/TypeChecker.{hpp,cpp}` | Resolve declaration types and type-check code. |
| `Semantics/TypeInterner.{hpp,cpp}`, `Semantics/TypeHelpers.hpp` | Canonicalize types and share type utilities. |
| `Semantics/ReferenceChecker.{hpp,cpp}`, `Semantics/Place.hpp` | Reference checking and representation of addressable locations. |
| `Semantics/ClosureAnalyser.{hpp,cpp}`, `Semantics/MethodLower.{hpp,cpp}` | Analyze captures and lower methods. |
| `CodegenIR/IRBuilder.{hpp,cpp}` | Lowers the checked AST into intermediate instructions. |
| `CodegenIR/IRValue.hpp`, `IROp.hpp`, `IRInstr.hpp`, `IRFunction.hpp`, `BasicBlock.hpp`, `CommonDef.hpp` (same directory) | Values, opcode definitions, instructions, functions, control-flow blocks, and shared IR definitions. |
| `CodegenIR/StringPool.{hpp,cpp}` | Stores constants for code generation. |
| `SSA/SSABuilder.{hpp,cpp}` | Builds static single assignment form. |
| `MachineIR/MIRBuilder.{hpp,cpp}`, `MachineIR/TypeLayout.hpp` | Lowers to machine-level IR and computes storage layout. |
| `MachineIR/MIROp.hpp`, `MachineIR/MIRInstr.hpp` | Machine IR opcodes and instructions. |
| `X86Codegen/x86Builder.{hpp,cpp}` | Emits x86-64 assembly. |
| `Runtime C Functions/runtime.c` | Linked native helpers for output, strings, allocation support, and file I/O. Compile-only deployment does not execute these. |
| `Utils/FrontendJson.{hpp,cpp}` | Serializes tokens, diagnostics, parser events, and ASTs for the visualiser. |
| `Utils/PrettyAst.{hpp,cpp}`, `Utils/SymbolPrinter.{hpp,cpp}` | Human-readable compiler debug dumps. |
| `Utils/NativeFunctionImpl.{hpp,cpp}`, `Utils/utils.{hpp,cpp}` | Built-in function definitions and shared compiler helpers. |
| `BytecodeVM/vm.{hpp,cpp}` | Legacy VM; the playground uses native x86-64 instead. |

`Examples/DynamicArray.kmy` exercises runtime-sized arrays; `Examples/StringOps.kmy` exercises strings. `tests/run-linux-tests.sh` compiles native fixtures and compares stdout to their `.expected` files. Under `tests/x86/`, `arrays.kmy`, `closures.kmy`, `control_flow.kmy`, `free.kmy`, `linux_abi.kmy`, and `pointers.kmy` exercise the named features; `modules/main.kmy` imports `modules/math.kmy`. The `dynamic_array.expected` and `string_ops.expected` files correspond to the examples above.

`stdlib/README.md` describes the KMY library. Under `stdlib/`, `core/{Contracts,IntMath,IntOption,IntPair,StringIntPair,StringOption}.kmy` provide shared contracts, math, and value types; `algorithms/{IntAlgorithms,IntRange,IntStatistics}.kmy` provide integer algorithms, ranges, and stats; `collections/{IntArrayList,IntDeque,IntLinkedList,IntQueue,IntStack,StringArrayList,StringQueue,StringStack}.kmy` implement their named containers; `maps/{IntIntHashMap,IntSet,StringIntHashMap,StringSet}.kmy` implement maps and sets; `strings/{StringBuilder,StringUtils}.kmy` provide string helpers; `io/FileUtils.kmy` provides file utilities. These are snapshot sources, not automatically available imports in the hosted editor: jobs only resolve submitted project files.

Generated files such as `frontend/dist/`, `backend/build/`, `release/`, and `.dev/` contain built assets, jars, archives, and local process state. They are not the source of truth.
