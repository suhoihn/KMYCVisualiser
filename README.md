# KMY Front-end Visualiser

An interactive native KMY playground with a replayable compiler front end:

```text
source → lexer → Pratt parser + AST construction → native x86-64 compilation → Linux program output
```

React talks to Kotlin Spring Boot, which streams results from a private Linux job runner. The real KMY compiler produces tokens, parser-call events, Pratt decisions, AST creation snapshots, and diagnostics. Native execution compiles with the x86-64 Linux path, links with GCC, and runs the generated ELF program. No backend parser or KMY VM execution is used.

See the [file-by-file repository guide](FILES.md) and [GitHub CI/CD setup](deploy/CICD.md). Deployment defaults to compile-and-replay; native execution is for trusted testing only until its file-access policy is enforced.

## What happens when you submit?

| Component | What it does |
| --- | --- |
| React (`frontend/`) | Sends the project on **Submit**. Shows server progress and keeps the parser recording in browser memory for replay. |
| Nginx (`deploy/nginx.conf`) | Serves React and forwards API requests in the Docker deployment. Caps request size and rate-limits submissions per IP. |
| Spring (`PlaygroundController.kt`) | Accepts `POST /api/playground`, checks the request, and streams the runner's newline-delimited JSON back to React. It does not parse KMY. |
| Dispatcher (`runner/server.py`) | Admits one active job and up to four waiting across the whole server. Starts and cleans up a fresh Docker container for each production job. |
| Worker (`runner/job.py`) | Saves the submitted files in temporary scratch space, runs compiler analysis, compiles and links the project, then runs the generated Linux executable. |
| Compiler (`compiler/`) | In `--frontend-json` mode, records real tokens, parser calls, and AST node creation. In `--linux --no-run` mode, compiles the project from its entry file and emits x86-64 assembly. GCC links that assembly with the KMY runtime and the playground heap wrapper. |

**Analysis and execution are separate.** React can start playback once a file's analysis arrives, even while the server compiles the project. Rewind reconstructs the tree from earlier recorded events; it does not ask the server to rerun the parser. A runtime failure leaves the trace and compile result available. Semantic analysis runs during native compilation, but does not have its own replay track.

**The memory limits are per active job, not per visitor.** The generated program has a 4 MiB live-allocation quota (`runner/heap_limit.c`) and a 4 MiB main-thread stack limit (`runner/job.py`). `free` gives quota back; the compiler and linker instead share the 256 MiB Docker job budget. Waiting submissions don't start a compiler or reserve a program heap. Override the defaults with `KMY_HEAP_BYTES` and `KMY_STACK_BYTES` in the deploy environment. These limits do not ban file I/O; keep Run private until the file-access policy is enforced. See the [deployment guide](deploy/README.md#isolation-and-capacity) for details.

## Ready-to-upload Linux artifacts

Run `./bundle.ps1` on Windows with Ubuntu WSL, Node, and Java installed. This creates `release/kmyc-linux-x86_64.tar.gz` (standalone native Linux compiler and runtime) and `release/kmy-playground-linux-x86_64.tar.gz` (built frontend, backend JAR, compiler, runner, and Docker files). `-SkipBuild` packages existing artifacts.

See [deployment and data-flow instructions](deploy/README.md) for VPS commands, limits, isolation, and local runner setup. The bundled binary targets Linux x86-64 / glibc 2.35+. The instrumented compiler source is in `compiler/`, copied from the sibling `../RemoteProjects` project; that sibling was not modified.

## Prerequisites

- Ubuntu WSL with g++/gcc for local native Linux compilation, or the Docker deployment
- Java 17 or newer
- Node.js 20 or newer

The runner's `KMY_COMPILER_PATH` points to the bundled Linux compiler. Spring uses `KMY_RUNNER_URL` (default `http://127.0.0.1:8090`) to reach the private runner. The older `/api/analysis` endpoint remains available for compatibility with `KMY_COMPILER_PATH`, but React uses `/api/playground` for the complete project stream.

## Run locally

First start the loopback-only development runner in Ubuntu WSL as described in [deploy/README.md](deploy/README.md). It runs trusted local code without container isolation. Then, from Command Prompt:

```bat
dev start
dev status
dev stop
```

From PowerShell, use the same commands with `./dev.ps1`, for example `./dev.ps1 start` and `./dev.ps1 stop`.

`dev restart` stops and starts both services. The launcher waits for the frontend on port 5173 and backend on port 8080, records only the processes it created, and writes output to `.dev\*.log`.

To run the two services manually, use separate terminals:

In one terminal:

```powershell
cd backend
.\gradlew.bat bootRun
```

In another terminal:

```powershell
cd frontend
npm install
npm run dev
```

Open `http://localhost:5173`.

## Compiler integration

Inside a Linux job, analysis runs:

```powershell
kmyc <source.kmy> --frontend-json
```

Schema v2 contains tokens with source spans, parser entry/exit events with call IDs, token-consumption / Pratt decisions, shallow AST node snapshots with stable IDs, a final AST, and syntax diagnostics. Parser inspection stops before semantics; the later native compilation phase runs the compiler's semantic and code-generation passes internally. Their diagnostics and execution results are shown separately.

## Replay the front end

Edit the source, then press **Submit** (or Ctrl/Cmd+Enter). The recording starts playing as analysis arrives, while server compilation/linking progress (and execution in trusted run mode) is shown separately. Use **Play/Pause**, **Previous/Next**, **Rewind**, the position slider, or the speed selector. Choose **Lexer**, **Parser**, **AST**, **Compile**, or **Output** directly. Editing clears stale results and cancels the request. Playback never reruns the compiler or executable.

Lexer replay reveals the recorded token list (not individual character-scanning operations). Parser replay shows the real parser call stack, consumed tokens, operator decisions, and nodes at the moment the compiler constructed them. Children appear before their parent, then connect into partial trees. Rewinding reconstructs the earlier partial forest; syntax failures can still show the nodes built before the error. The AST stage is the completed parser result. Both partial and completed trees support Graph and Outline views.

## AST and modules

Choose **Graph** for connected, expandable AST cards with edge roles and zoom controls, or **Outline** for the compact text tree. Both show the actual AST returned by the compiler.

The module grammar is `import "relative/path.kmy" as alias;`, with imports at the top and qualified references such as `alias::function()`. Use **Add file** or the **Modules** example, then submit. The complete project is sent once. Each file gets its own parser recording, then native compilation links the project starting at `main.kmy`. Imports must stay inside the submitted project. Import declarations remain separate metadata outside the `Program` AST.

The compiler determines supported language features; parsing successfully does not guarantee native compilation succeeds. Source and replay data currently live in browser memory and are lost on reload. No permanent per-user server workspace is allocated.
