# KMY Front-end Visualiser

An interactive visualiser for the KMY compiler's first three stages:

```text
source → lexer → Pratt parser → AST
```

The React application talks to a small Kotlin Spring Boot service. The service invokes the real KMY compiler with `--frontend-json`, so the UI stays aligned with the language implementation rather than maintaining a second parser.

## Prerequisites

- The KMY compiler at `../RemoteProjects/kmyc.exe`, built with the new `--frontend-json` mode
- Java 17 or newer
- Node.js 20 or newer

Override the compiler location with `KMY_COMPILER_PATH` if needed.

## Run locally

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

The backend runs:

```powershell
kmyc.exe <temporary-source.kmy> --frontend-json
```

The compiler returns tokens with source spans, parser-consumption and Pratt binding-power events, a nested AST, and structured diagnostics. Inspection stops before semantic analysis and code generation.
