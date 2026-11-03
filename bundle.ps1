[CmdletBinding()]
param([string]$Distribution = 'Ubuntu', [switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$bundleRoot = $PSScriptRoot
Push-Location $bundleRoot
try {
    if (-not $SkipBuild) {
        & wsl.exe -d $Distribution --cd $bundleRoot --exec bash compiler/build-linux.sh
        if ($LASTEXITCODE -ne 0) { throw 'Linux compiler build failed' }
        Push-Location frontend
        try { & npm.cmd run build; if ($LASTEXITCODE -ne 0) { throw 'Frontend build failed' } } finally { Pop-Location }
        Push-Location backend
        try { & .\gradlew.bat --gradle-user-home ..\.gradle-bootstrap bootJar; if ($LASTEXITCODE -ne 0) { throw 'Backend build failed' } } finally { Pop-Location }
    }
    $required = @('release/linux-x86_64/kmyc', 'frontend/dist/index.html', 'backend/build/libs/kmy-visualiser-backend-0.1.0.jar')
    foreach ($artifact in $required) { if (-not (Test-Path -LiteralPath $artifact)) { throw "Missing artifact: $artifact" } }
    Copy-Item -LiteralPath compiler/LINUX_BINARY_README.md -Destination release/linux-x86_64/README.md -Force
    & tar.exe -czf release/kmyc-linux-x86_64.tar.gz -C release/linux-x86_64 .
    if ($LASTEXITCODE -ne 0) { throw 'Compiler archive creation failed' }
    & tar.exe -czf release/kmy-playground-linux-x86_64.tar.gz README.md FILES.md compose.yaml deploy runner/job.py runner/server.py runner/heap_limit.c release/linux-x86_64 frontend/dist backend/build/libs/kmy-visualiser-backend-0.1.0.jar
    if ($LASTEXITCODE -ne 0) { throw 'Bundle creation failed' }
    Write-Host "Upload $bundleRoot\release\kmy-playground-linux-x86_64.tar.gz to the VPS."
    Get-FileHash -LiteralPath release/kmy-playground-linux-x86_64.tar.gz -Algorithm SHA256
} finally { Pop-Location }
