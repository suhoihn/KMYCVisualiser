#include <iostream>
#include <fstream>
#include <exception>
#include <cstdlib>
#include <cstdio>
#include <streambuf>

#include "../Utils/utils.hpp"
#include "../Utils/SymbolPrinter.hpp"
#include "../Utils/FrontendJson.hpp"
#include "../Utils/PrettyAst.hpp"
#include "../Core/lexer.hpp"
#include "../Core/newParser.hpp"
#include "../Core/Ast.hpp"
#include "../Core/errorhandler.hpp"
#include "../Semantics/SymbolScopeBuilder.hpp" // Pass 1
#include "../Semantics/DeclTypeResolver.hpp" // Pass 2
#include "../Semantics/Resolver.hpp" // Pass 3
#include "../Semantics/ReferenceChecker.hpp" // Pass 4
#include "../Semantics/ClosureAnalyser.hpp" // Pass 6
#include "../Semantics/MethodLower.hpp" // Pass 5
// #include "../Semantics/typechecker.hpp" // Planned pass 5
#include "compiler.hpp" // Code gen in pass 6
#include "../CodegenIR/IRBuilder.hpp" // Pass 6
#include <cstring>
#include <sstream>
#include "../SSA/SSABuilder.hpp" // Pass 7
#include "../MachineIR/MIRBuilder.hpp" // Pass X
#include "../X86Codegen/x86Builder.hpp" // The ultimate pass... right?
#include <queue>
#include <filesystem>

const std::string RED = "\033[31m";
const std::string RESET = "\033[0m";
const std::string BOLD = "\033[1m";

#ifdef _WIN32
// Ask WSL to translate absolute host paths; do not assume a /mnt/c mount layout.
static std::string linuxLinkPath(const std::filesystem::path& path) {
    const std::string command = "wsl.exe --exec wslpath -a \""
        + std::filesystem::absolute(path).string() + "\"";
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) throw KMYCompileError("Could not start WSL; use --no-run to emit Linux assembly only.");
    std::string result;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
    const int status = _pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
    if (status != 0 || result.empty()) {
        throw KMYCompileError("Linux linking requires a working default WSL distribution with gcc; use --no-run for assembly only.");
    }
    return result;
}
#endif

// Module-map keys use normalized paths. Imports are relative to their importing
// file, and spelling variants such as `lib/../lib/math.kmy` load only once.
static std::string normalizeModulePath(const std::filesystem::path& path) {
    return path.lexically_normal().generic_string();
}

static std::string resolveImportPath(
    const std::string& importerPath,
    const std::string& requestedPath
) {
    std::filesystem::path path(requestedPath);
    if (path.is_relative()) {
        path = std::filesystem::path(importerPath).parent_path() / path;
    }
    return normalizeModulePath(path);
}

static void printDiagnostic(
    const KMYParseError& e,
    const std::string& source
) {

    // -----------------------------
    // Find line text
    // -----------------------------
    std::istringstream ss(source);
    std::string lineStr;

    for (int i = 1; i <= e.line(); ++i) {
        if (!std::getline(ss, lineStr))
            return;
    }

    // -----------------------------
    // FIXED COLUMN CALCULATION
    // -----------------------------
    // DO NOT use e.start() directly as column.
    // Convert global offset -> line-local offset.

    int globalPos = e.start();
    int lineStart = globalPos;

    for (int i = globalPos; i >= 0; --i) {
        if (source[i] == '\n') {
            lineStart = i + 1;
            break;
        }
    }

    int column = globalPos - lineStart;

    // safety clamp
    column = std::max(0, column);

    int length = std::max(1, e.end() - e.start());

    std::cerr << RED
              << "Parse error: "
              << e.what()
              << " at line "
              << e.line()
              << ", column "
              << column + 1 // For display, column doesn't start at 0.
              << RESET
              << "\n";

    // -----------------------------
    // Print source line
    // -----------------------------
    std::cerr << RED << lineStr << RESET << "\n";

    // -----------------------------
    // Print underline
    // -----------------------------
    std::cerr << std::string(column, ' ')
              << BOLD << RED
              << std::string(length, '^')
              << RESET
              << "\n";
}

static void printUsage(std::ostream& out) {
    out << "KMY compiler (kmyc)\n"
        << "2026 (c) KMY\n\n"
        << "Usage:\n"
        << "  kmyc <source-file> [options]\n"
        << "  kmyc --help\n\n"
        << "Options:\n"
        << "  -h, --help             Show this help text and exit.\n"
        << "  -asm, --asm            Compile through the native x86-64 backend.\n"
        << "  --linux                Select Linux x86-64 ELF output (implies --asm; default: Windows).\n"
        << "  -o, --output <path>    Set the output executable/assembly base path.\n"
        << "  --no-run               Generate output but do not link/run it.\n"
        << "  --frontend-json        Emit frontend tokens/trace JSON and stop.\n"
        << "  -d, --debug            Print intermediate compiler information.\n"
        << "  --check-ownership      Enable experimental ownership checking (off by default).\n"
        << "  -t, --strict-types     Enable the legacy strict-type switch.\n\n"
        << "Examples:\n"
        << "  kmyc tests/x86/vm_smoke.kmy\n"
        << "  kmyc Examples/SimpleASMTest.kmy -asm -o simpleasm\n"
        << "  kmyc Examples/SimpleASMTest.kmy --frontend-json\n";
}

// Keep the older, detailed pass diagnostics, but present each completed line
// under its pass name. Without -d they stay out of normal program output.
class PassOutputFormatter : public std::streambuf {
    std::streambuf* previous;
    const char* passName;
    bool enabled;
    std::string pending;

    void flushLine() {
        if (!enabled || pending.empty()) { pending.clear(); return; }

        // printLog emits ANSI colors; pass details use a single plain style.
        std::string plain;
        for (size_t i = 0; i < pending.size(); ++i) {
            if (pending[i] == '\033' && i + 1 < pending.size() && pending[i + 1] == '[') {
                i += 2;
                while (i < pending.size() && pending[i] != 'm') ++i;
                continue;
            }
            if (pending[i] != '\r') plain += pending[i];
        }
        const auto first = plain.find_first_not_of(" \t");
        if (first != std::string::npos) {
            const auto last = plain.find_last_not_of(" \t");
            std::cerr << "  [" << passName << "] detail: "
                      << plain.substr(first, last - first + 1) << '\n';
        }
        pending.clear();
    }

    int overflow(int c) override {
        if (c == traits_type::eof()) return traits_type::not_eof(c);
        if (c == '\n') flushLine();
        else if (enabled) pending += static_cast<char>(c);
        return c;
    }

    std::streamsize xsputn(const char* data, std::streamsize size) override {
        for (std::streamsize i = 0; i < size; ++i) overflow(data[i]);
        return size;
    }

    // cerr is tied to cout; a node trace can flush cout halfway through an
    // older multi-part message. Only newline/destruction completes a line.
    int sync() override { return 0; }

public:
    PassOutputFormatter(const char* name, bool debug)
        : previous(std::cout.rdbuf(this)), passName(name), enabled(debug) {}
    ~PassOutputFormatter() { flushLine(); std::cout.rdbuf(previous); }
    PassOutputFormatter(const PassOutputFormatter&) = delete;
    PassOutputFormatter& operator=(const PassOutputFormatter&) = delete;
};

template <typename F>
auto runPass(const char* passName, bool debug, F&& pass) {
    if (debug) { std::cout.flush(); std::cerr << "\n" << passName << " pass\n"; }
    setAstTracePass(passName);
    PassOutputFormatter formatted(passName, debug);
    return pass();
}

static void printDebugHeading(const std::string& name) {
    std::cout << "\n" << name << "\n" << std::string(name.size(), '-') << "\n";
}

int main(int argc, char *argv[]) {
    // Switches
    bool debugOutput = false;
    bool strictTypes = false;
    bool checkOwnership = false;
    bool isBuildingASM = false;
    bool targetLinux = false;
    bool frontendJson = false;
    bool run = true;
    const char* output = nullptr;
    const char* filename = nullptr;

    if (argc == 1) {
        printUsage(std::cout);
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        const char* str = argv[i];

        if (strcmp(str, "-h") == 0 || strcmp(str, "--help") == 0) {
            printUsage(std::cout);
            return 0;
        }

        if (strcmp(str, "--frontend-json") == 0) {
            if (frontendJson) {
                printLog(LogLevel::WARN, "Duplicate switch (--frontend-json) detected.");
            }
            frontendJson = true;
            run = false;
            continue;
        }

        if (strcmp(str, "-d") == 0 || strcmp(str, "--debug") == 0) {
            if (debugOutput) {
                printLog(LogLevel::WARN, "Duplicate switch (--debug) detected.");
            }
            debugOutput = true;
            continue;
        }

        if (strcmp(str, "-t") == 0 || strcmp(str, "--strict-types") == 0) {
            if (strictTypes) {
                printLog(LogLevel::WARN, "Duplicate switch (--strict-types) detected.");
            }
            strictTypes = true;
            continue;
        }

        if (strcmp(str, "--check-ownership") == 0) {
            if (checkOwnership) {
                printLog(LogLevel::WARN, "Duplicate switch (--check-ownership) detected.");
            }
            checkOwnership = true;
            continue;
        }

        if (strcmp(str, "--linux") == 0) {
            targetLinux = true;
            isBuildingASM = true;
            continue;
        }

        if (strcmp(str, "-asm") == 0 || strcmp(str, "--asm") == 0) {
            if (isBuildingASM) {
                printLog(LogLevel::WARN, "Duplicate switch (--asm) detected.");
            }
            isBuildingASM = true;
            continue;
        }

        if (strcmp(str, "--no-run") == 0) {
            if (!run) {
                printLog(LogLevel::WARN, "Duplicate switch (--no-run) detected.");
            }
            run = false;
            continue;
        }

        if (strcmp(str, "-o") == 0 || strcmp(str, "--output") == 0) {
            if (output) {
                printLog(LogLevel::WARN, "Duplicate output switch detected; using the last path.");
            }
            if (i + 1 >= argc || argv[i + 1][0] == '-') {
                printLog(LogLevel::ERROR, "Output switch requires a path: -o <path>");
                return 1;
            }
            output = argv[++i];
            continue;
        }

        if (str[0] == '-') {
            std::cerr << "Unknown switch: " << str << "\n\n";
            printUsage(std::cerr);
            return 1;
        }

        if (filename) {
            std::cerr << "Unexpected extra source file: " << str << "\n\n";
            printUsage(std::cerr);
            return 1;
        }
        filename = str;
    }

    setAstTraceEnabled(debugOutput);

    if (!filename) {
        std::cerr << "No source file specified.\n\n";
        printUsage(std::cerr);
        return 1;
    }

    const std::string entryModulePath = normalizeModulePath(filename);



    std::vector<ParserTraceEvent> parserTrace;
    std::vector<Token> tokens;
    std::string source;
    FunctionExprPtr program;

    enum class ModuleState {
        LOADING,
        LOADED,
    };
    std::unordered_map<std::string, ModuleState> moduleStates;
    // One owned AST per file. Pointers/references to unordered_map elements stay
    // valid across rehashes, so Module::imports can safely point at these values.
    std::unordered_map<std::string, Module> modules;
    try {
        std::queue<std::string> importQueue;
        importQueue.push(entryModulePath);

        while (!importQueue.empty()) {
            std::string modulePath = importQueue.front();
            importQueue.pop();

            if (moduleStates.count(modulePath) > 0) {
                switch (moduleStates[modulePath]) {
                    // The queue only collects each physical path once.
                    case ModuleState::LOADED: continue;
                    case ModuleState::LOADING: continue;
                }
            }

            moduleStates[modulePath] = ModuleState::LOADING;

            // Read first.
            std::ifstream file(modulePath);
            if (!file.is_open()) {
                std::cerr << "Could not open file: " << modulePath << "\n";
                return 1;
            }

            source = std::string((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());


            // 1. Tokenize
            Lexer lexer(source);
            tokens = lexer.tokenise();

            if (debugOutput && !frontendJson) {
                printDebugHeading("Tokens");
                printTokens(tokens);
            }

            // 2. Parse
            Parser parser(tokens, frontendJson ? &parserTrace : nullptr);

            Module module = parser.parse();
            if (modulePath == entryModulePath) {
                program = module.program;
            } else {
                // Imported files contribute callable code, but only the command
                // line module becomes the x86 `main` entry point.
                module.program->isEntry = false;
            }

            // Store normalized paths in the import graph after parsing. This
            // does not change source-level aliases or runtime semantics.
            for (ImportDecl& importDecl : module.importDecls) {
                importDecl.path = resolveImportPath(modulePath, importDecl.path);
            }

            if (frontendJson) {
                writeFrontendJson(std::cout, source, tokens, parserTrace, program);
                std::cout << '\n';
                return 0;
            }
            if (debugOutput) {
                printDebugHeading("Parsed AST");
                printPrettyAST(std::cout, program);
            }

            // Store the parsed module before queuing its dependencies. The second
            // pass below turns these raw import declarations into Module pointers.
            auto [stored, inserted] = modules.emplace(modulePath, std::move(module));
            if (!inserted) {
                throw KMYCompileError("Module was collected more than once: " + modulePath);
            }

            for (const ImportDecl& importDecl : stored->second.importDecls) {
                importQueue.push(importDecl.path);
            }
            moduleStates[modulePath] = ModuleState::LOADED;
        }

        // Reuse the state map for DFS: LOADING means "on the active path".
        // A back-edge to LOADING is an import cycle.
        moduleStates.clear();
        std::vector<std::string> declarationOrder;
        auto checkCycles = [&](auto&& self, const std::string& modulePath) -> void {
            auto state = moduleStates.find(modulePath);
            if (state != moduleStates.end()) {
                if (state->second == ModuleState::LOADING) {
                    throw KMYCompileError("Circular import involving: " + modulePath);
                }
                return; // Already checked this completed dependency.
            }

            moduleStates.emplace(modulePath, ModuleState::LOADING);
            const Module& module = modules.at(modulePath);
            for (const ImportDecl& importDecl : module.importDecls) {
                auto target = modules.find(importDecl.path);
                if (target == modules.end()) {
                    throw KMYCompileError(
                        "Imported module was not collected: " + importDecl.path +
                        " (from " + modulePath + ")"
                    );
                }
                self(self, target->first);
            }
            moduleStates[modulePath] = ModuleState::LOADED;
            // Postorder puts dependencies before importers for declaration types.
            declarationOrder.push_back(modulePath);
        };

        for (const auto& [modulePath, module] : modules) {
            checkCycles(checkCycles, modulePath);
        }

        // Link raw `path as alias` declarations after the graph is valid.
        for (auto& [modulePath, module] : modules) {
            for (const ImportDecl& importDecl : module.importDecls) {
                auto target = modules.find(importDecl.path);
                if (target == modules.end()) {
                    throw KMYCompileError(
                        "Imported module was not collected: " + importDecl.path +
                        " (from " + modulePath + ")"
                    );
                }
                if (module.imports.count(importDecl.alias) > 0) {
                    throw KMYCompileError(
                        "Duplicate import alias \"" + importDecl.alias +
                        "\" in " + modulePath
                    );
                }
                module.imports.emplace(importDecl.alias, &target->second);
            }
        }

        if (!program) {
            throw KMYCompileError("Entry module was not collected.");
        }

        // 3-1. Every module must own a scope before imported names can resolve.
        runPass("SymbolScopeBuilder", debugOutput, [&] {
            for (auto& [modulePath, module] : modules) {
                SymbolScopeBuilder builder(module);
                builder.analyse();
            }
        });

        // Reserve one stable program-wide slot for every top-level KMY value.
        // Native built-ins keep their separate runtime `globalSlot` IDs; these
        // slots are for the future persistent module-global storage area.
        int moduleGlobalSlotCount = 0;
        runPass("GlobalSlotAssignment", debugOutput, [&] {
            for (const std::string& modulePath : declarationOrder) {
                Module& module = modules.at(modulePath);
                for (const StmtPtr& statement : module.topLevelStatements) {
                    auto* declaration = dynamic_cast<Let*>(statement.get());
                    if (!declaration) continue;
                    declaration->symbol->isModuleGlobal = true;
                    declaration->symbol->moduleGlobalSlot = moduleGlobalSlotCount++;
                }
            }
        });

        // 3-2. Resolve declarations/signatures after all module scopes exist.
        runPass("DeclTypeResolver", debugOutput, [&] {
            for (const std::string& modulePath : declarationOrder) {
                DeclTypeResolver temp(modules.at(modulePath));
                temp.resolve(); // TODO: Better name
            }
        });

        // 3-3. Resolve each module body against its own module scope.
        runPass("Resolver", debugOutput, [&] {
            for (auto& [modulePath, module] : modules) {
                Resolver resolver(module);
                resolver.resolve();
            }
        });

        // Opt-in while ownership rules are incomplete; check resolved source AST.
        if (checkOwnership) {
            runPass("ReferenceChecker", debugOutput, [&] {
                for (auto& [modulePath, module] : modules) {
                    ReferenceChecker checker(module);
                    checker.check();
                }
            });
        }

        //3-3.5(?). Method lowering
        runPass("MethodLower", debugOutput, [&] {
            for (auto& [modulePath, module] : modules) {
                MethodLower lower(module);
                lower.lower();
            }
        });

        if (debugOutput) {
            printDebugHeading("Resolved AST");
            printPrettyAST(std::cout, program);
        }

        // 3-4. Closure analysis and slot allocation (VM).
        runPass("ClosureAnalyser", debugOutput, [&] {
            int nextFunctionId = 0;
            for (const std::string& modulePath : declarationOrder) {
                ClosureAnalyser analyser(modules.at(modulePath), nextFunctionId);
                analyser.analyse();
            }
        });

        // 3-4. Type check
        if (strictTypes) {
            std::cerr << "Warning: --strict-types is deprecated; the resolver already checks types.\n";
            // TypeChecker checker(program);
            // checker.check();
            // std::cout << "[DEBUG]: Type checks done." << std::endl;
        }


        if (isBuildingASM) {
            // 4-a. Code gen (CFG IR)
            StringPool stringPool;
            std::vector<HIRFunction*> funcs;
            runPass("IRBuilder", debugOutput, [&] {
                // Compile dependencies before importers. Their functions share
                // one HIR/MIR/x86 program, while only the root module is main.
                for (const std::string& modulePath : declarationOrder) {
                    std::vector<int> dependencyInitializers;
                    if (modulePath == entryModulePath) {
                        for (const std::string& priorPath : declarationOrder) {
                            if (priorPath == entryModulePath) break;
                            dependencyInitializers.push_back(modules.at(priorPath).program->functionId);
                        }
                    }
                    IRBuilder builder(modules.at(modulePath), stringPool, std::move(dependencyInitializers));
                    auto moduleFuncs = builder.compile();
                    funcs.insert(funcs.end(), moduleFuncs.begin(), moduleFuncs.end());
                }
            });
            if (debugOutput) {
                printDebugHeading("HIR (before SSA)");
                for (const auto& func : funcs) std::cout << *func << "\n";
            }

            // 4-b. Phi computation and SSA renaming
            runPass("SSABuilder", debugOutput, [&] {
                SSABuilder ssaBuilder(funcs);
                ssaBuilder.build();
            });

            if (debugOutput) {
                printDebugHeading("HIR (after SSA)");
                for (const auto& func : funcs) std::cout << *func << "\n";
            }

            MIRBuilder mirBuilder(funcs);
            auto mirFuncs = runPass("MIRBuilder", debugOutput, [&] { return mirBuilder.lower(); });
            if (debugOutput) {
                printDebugHeading("MIR");
                for (const auto& func : mirFuncs) std::cout << *func << "\n";
            }

            std::ostringstream buffer;

            X86Builder x86Builder(mirFuncs, buffer, stringPool, moduleGlobalSlotCount,
                targetLinux ? X86Target::Linux : X86Target::Windows);
            runPass("X86Builder", debugOutput, [&] { x86Builder.build(); });

            std::string assembly = buffer.str();

            if (debugOutput) {
                printDebugHeading("x86-64 assembly");
                std::cout << assembly;
            }


            if (!output) {
                output = "out";
            }

            std::string asmFile = std::string(output) + ".s";
            {
                std::ofstream file(asmFile);
                if (!file) {
                    std::cerr << "Could not write assembly file: " << asmFile << "\n";
                    return 1;
                }
                file << assembly;
            } // close file.

            std::cout << "Assembly written to " << asmFile << "\n";

            if (run) {
                std::string cmd;
                if (targetLinux) {
                    // Build runtime.c for ELF too; Windows runtime.o is not link-compatible.
#ifdef _WIN32
                    cmd = "wsl.exe --exec gcc \"" + linuxLinkPath(asmFile)
                        + "\" \"" + linuxLinkPath("Runtime C Functions/runtime.c")
                        + "\" -o \"" + linuxLinkPath(output) + "\"";
#else
                    cmd = "gcc \"" + asmFile + "\" \"Runtime C Functions/runtime.c\" -o \"" + output + "\"";
#endif
                } else {
#ifdef _WIN32
                    cmd = "gcc \"" + asmFile + "\" \"Runtime C Functions/runtime.o\" -o \"" + output + "\"";
#else
                    cmd = "x86_64-w64-mingw32-gcc \"" + asmFile
                        + "\" \"Runtime C Functions/runtime.c\" -o \"" + output + "\"";
#endif
                }

                int result = std::system(cmd.c_str());

                if (result != 0) {
                    std::cerr << (targetLinux ? "Linux gcc/WSL link failed\n" : "Windows gcc link failed\n");
                    return 1;
                }

                std::cout << "Built " << output << "\n";
            }
            return 0;
        }

        // 4. Code gen (Stack VM)
        Compiler compiler(modules.at(entryModulePath));
        auto fnProtos = runPass("VMCompiler", debugOutput, [&] { return compiler.compile(); });

        if (debugOutput) {
            printDebugHeading("VM bytecode");
            int fnProtoId = 0;
            for (auto& fnProto : fnProtos) {
                std::cout << "Function " << fnProtoId++
                          << " (" << fnProto.upValueCnt << " captures):\n"
                          << chunkToString(fnProto.chunk) << '\n';
            }
        }

        if (run) {
            // 4. run
            setAstTracePass(nullptr);
            if (debugOutput) std::cerr << "\nVM runtime\n";
            {
                std::streambuf* programOutput = std::cout.rdbuf();
                PassOutputFormatter formatted("VM", debugOutput);
                VM vm;
                vm.setProgramOutput(programOutput);
                vm.load(fnProtos);
                vm.run();
            }

            //std::exit(0);
        }
        return 0;

    } catch (const KMYParseError& e) {
        if (frontendJson) {
            writeFrontendErrorJson(
                std::cout, source, tokens, parserTrace,
                e.what(), e.line(), e.start(), e.end()
            );
            std::cout << '\n';
            return 1;
        }
        printDiagnostic(e, source);
        return 1;
    } catch (const KMYCompileError& e) {
        if (frontendJson) {
            writeFrontendErrorJson(std::cout, source, tokens, parserTrace, e.what(), 1, 0, 0);
            std::cout << '\n';
            return 1;
        }
        std::cerr << RED << "Compile error: " << e.what() << RESET << std::endl;
        return 1;
    } catch (const std::exception& e) {
        if (frontendJson) {
            writeFrontendErrorJson(std::cout, source, tokens, parserTrace, e.what(), 1, 0, 0);
            std::cout << '\n';
            return 1;
        }
        std::cerr << RED << "Runtime error: " << e.what() << RESET << std::endl;
        return 1;
    }

    printUsage(std::cerr);
    return 1;
}
