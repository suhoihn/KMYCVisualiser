# Linux x86-64 VPS bundle

The archive contains an actual Linux ELF `release/linux-x86_64/kmyc`, its native C runtime, a built Spring Boot JAR, a built React app, and Docker deployment files. The compiler was built on Ubuntu 22.04 x86-64. It uses glibc >= 2.35; the job image uses Ubuntu 22.04. This bundle targets an **amd64/x86-64 VPS**, not an ARM VPS. For image builds and automatic deployment from `main`, use the [CI/CD guide](CICD.md) instead of uploading an archive.

From Windows, rebuild and package everything with `./bundle.ps1`. Ubuntu WSL needs g++ and gcc; Windows needs Node and Java 17+. `./bundle.ps1 -SkipBuild` packages existing verified artifacts.

On a Linux VPS with Docker Engine and the Compose plugin installed:

```sh
mkdir -p kmy-playground
tar -xzf kmy-playground-linux-x86_64.tar.gz -C kmy-playground
cd kmy-playground
docker compose --parallel 1 --profile build build
docker compose up -d
docker compose logs --tail=50
```

Forward the VPS's loopback port with SSH (`-L 8088:127.0.0.1:8088`) and visit `http://localhost:8088`. Stop with `docker compose down`. This removes service containers; the dispatcher also removes its disposable job containers. The only published port binds to `127.0.0.1:8088` by default; Spring and the dispatcher are not directly exposed. Configure HTTPS through a host reverse proxy before a public launch.

## Isolation and capacity

Only the small trusted runner dispatcher has the Docker socket. Neither Spring nor submitted-code containers receive it. Keep that dispatcher private: Docker socket access controls the host. Each analysis/compile/run job is a new non-root container with no network, a read-only root, no capabilities, no new privileges, a 256 MiB memory limit (including tmpfs), 0.75 CPU, 32 process limit, no core dumps, and a 45-second watchdog. Docker's default seccomp policy remains enabled. For public anonymous native execution, install and verify gVisor `runsc`, then start with `KMY_SANDBOX_RUNTIME=runsc docker compose up -d`; the worker's default Docker isolation alone is not a promise against kernel exploits. No job can choose its image, container settings, command, environment, or host mounts.

Source is limited to 64 KiB across 16 submitted `.kmy` files. Imports must reference submitted project files and cannot escape the project. There is no general host filesystem or package installation access. Native compilation invokes KMY assembly generation and GCC linking with separate argument lists. The workspace/build scratch is capped at 32 MiB and `/tmp` at 16 MiB. These are temporary per-job limits, not a persistent RAM allocation per visitor. Program output is capped at 64 KiB, trace streams at 8 MiB, and the parser at 50,000 events / 128 nested parser calls. The global queue permits one active job and four waiting; queue waits are capped at 20 seconds. Compilation and linking each have 10-second limits; execution has 2 seconds. The dispatcher also limits a client address to 10 submissions per minute.

The generated Linux program defaults to **4 MiB of live KMY allocations** (including quota bookkeeping) and a **4 MiB main-thread stack limit**. Set `KMY_HEAP_BYTES` and `KMY_STACK_BYTES` in the Compose `.env` file to override these defaults (decimal bytes, 65536–67108864). Recreate the dispatcher with `docker compose up -d runner` after changing them. The job image must be rebuilt to pick up code changes. The heap wrapper covers generated `malloc`/`calloc` and allocations from the linked KMY runtime; `free` restores quota. The kernel limits the executable's stack; stack overflow usually appears as a segmentation fault, which can also have other causes. Neither limit applies to the compiler or linker, and neither is a bound on the process's total memory. The container cap still applies.

**Private testing only until the file-access policy is enforced.** The allocation quota isn't a security boundary for memory-unsafe programs. The current KMY runtime still allows file reads and writes inside the job container; a read-only root and gVisor don't by themselves ban file I/O. Do not expose native Run publicly until the executable has its own enforced no-file-I/O policy.

Starting service limits are web 48 MiB, Spring 320 MiB (160 MiB Java heap), dispatcher 64 MiB, and one job 256 MiB. Measure real peak usage and gVisor overhead on your VPS before increasing concurrency. Images and artifacts are built locally, keeping Node/Gradle/compiler builds off the 1 GB VPS (the Docker image installation steps still require headroom). For an extremely small host, build/load the Docker images elsewhere too.

## Local development with Ubuntu WSL

Build the compiler with `wsl -d Ubuntu --cd C:\Users\ihnsu\KMYCVisualiser --exec bash compiler/build-linux.sh`. In an Ubuntu terminal, from this project directory, start the **development-only, unsandboxed** runner:

```sh
KMY_RUNNER_MODE=local KMY_EXECUTION_MODE=run KMY_COMPILER_PATH="$PWD/release/linux-x86_64/kmyc" KMY_WORK_ROOT=/tmp python3 runner/server.py
```

It binds only to `127.0.0.1:8090`; use it only for your own trusted programs. Run the frontend/backend with the existing `dev start` launcher. Spring's default runner URL is `http://127.0.0.1:8090` and can be changed with `KMY_RUNNER_URL`. The production Compose deployment always uses Docker jobs.

## How results reach the UI

React sends the complete files map and `main.kmy` entry point to `POST /api/playground`. Spring forwards it to the private runner and streams newline-delimited JSON without interpreting the language. The job runs `kmyc file --frontend-json` for each file. Schema v2 records parser calls, token consumption, Pratt decisions, and shallow AST node snapshots with stable IDs at creation time. Partial nodes can survive a syntax failure. The frontend assembles those recorded nodes into partial trees while stepping forward, and reconstructs an earlier state when rewinding.

After analysis, the job invokes `kmyc entry --linux --no-run -o program` (assembly only), then invokes GCC with `program.s`, `runtime.c`, and the heap wrapper. The default `KMY_EXECUTION_MODE=compile` stops after linking. With `run` explicitly enabled for trusted tests, the job also runs the linked ELF executable. Each phase is streamed as it happens; compile diagnostics and program stdout/stderr remain separate. The FE displays server progress separately from its replay cursor. Replay never recompiles or re-executes a program. The AST tab shows the completed parser tree; semantic analysis happens internally for native compilation but has no detailed visualisation yet.

Compiler sources live in `compiler/`, a snapshot from the user's sibling `RemoteProjects` with a small tracing extension. Its grammar is the real KMY parser. The original sibling compiler was not edited. Changes to upstream grammar must be synced into this snapshot and the Linux binary rebuilt.
