# KMY compiler for Linux x86-64

This is a native Linux ELF executable built on Ubuntu 22.04 (glibc 2.35 or newer, x86-64/amd64). It is not a Windows `.exe` and does not require the KMY bytecode VM for the commands below. GCC must be installed to link native output. Keep the accompanying `Runtime C Functions/runtime.c` beside the compiler and run commands from this directory because native linking uses that relative runtime path.

```sh
chmod +x kmyc
./kmyc /path/to/main.kmy --frontend-json
./kmyc /path/to/main.kmy --linux -o /path/to/program
/path/to/program
```

`--linux` explicitly selects native Linux x86-64; the compiler's default target remains Windows. `--no-run` produces assembly without linking. `--frontend-json` emits schema v2 tokens, parser calls, token-consumption / Pratt decisions, AST creation snapshots, final AST, and syntax diagnostics. It stops before semantic analysis. Compilation and execution are separate operations.

The compiler source and full playground deployment instructions are in the KMYCVisualiser project. Programs run directly by the commands above have your user permissions; use the playground's isolated job runner for submissions from others.
