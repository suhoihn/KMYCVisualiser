[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('start', 'stop', 'restart', 'status')]
    [string]$Command = 'start'
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$runtimeDirectory = Join-Path $projectRoot '.dev'
$stateFile = Join-Path $runtimeDirectory 'processes.json'
$frontendDirectory = Join-Path $projectRoot 'frontend'
$backendDirectory = Join-Path $projectRoot 'backend'

function Read-DevState {
    if (-not (Test-Path -LiteralPath $stateFile)) { return $null }
    try {
        return Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
    } catch {
        Write-Warning "Ignoring an unreadable process state file: $stateFile"
        return $null
    }
}

function Get-TrackedProcess {
    param($Entry)

    if ($null -eq $Entry -or $null -eq $Entry.pid -or $null -eq $Entry.startedAt) { return $null }
    $process = Get-Process -Id ([int]$Entry.pid) -ErrorAction SilentlyContinue
    if ($null -eq $process) { return $null }

    try {
        $recordedStart = [DateTimeOffset]::Parse([string]$Entry.startedAt).UtcDateTime
        $actualStart = $process.StartTime.ToUniversalTime()
        if ([Math]::Abs(($actualStart - $recordedStart).TotalSeconds) -gt 2) { return $null }
    } catch {
        return $null
    }
    return $process
}

function Get-PortOwner {
    param([int]$Port)
    return Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue |
        Select-Object -First 1
}

function Wait-ForPort {
    param(
        [int]$Port,
        [string]$Name,
        $Entry,
        [int]$TimeoutSeconds = 60
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($null -ne (Get-PortOwner -Port $Port)) { return }
        if ($null -eq (Get-TrackedProcess -Entry $Entry)) {
            throw "$Name stopped before port $Port was ready. Check .dev/$($Name.ToLower()).out.log and .dev/$($Name.ToLower()).err.log."
        }
        Start-Sleep -Milliseconds 400
    }
    throw "Timed out waiting for $Name on port $Port."
}

function Stop-ProcessTree {
    param($Entry)

    $rootProcess = Get-TrackedProcess -Entry $Entry
    if ($null -eq $rootProcess) { return $false }

    $processSnapshot = Get-CimInstance Win32_Process | Select-Object ProcessId, ParentProcessId
    $processIds = [System.Collections.Generic.List[int]]::new()

    function Add-Children {
        param([int]$ParentId)
        foreach ($child in $processSnapshot | Where-Object { [int]$_.ParentProcessId -eq $ParentId }) {
            Add-Children -ParentId ([int]$child.ProcessId)
            $processIds.Add([int]$child.ProcessId)
        }
    }

    Add-Children -ParentId $rootProcess.Id
    $processIds.Add($rootProcess.Id)
    foreach ($processId in $processIds) {
        Stop-Process -Id $processId -Force -ErrorAction SilentlyContinue
    }
    return $true
}

function Stop-DevEnvironment {
    $state = Read-DevState
    if ($null -eq $state) {
        Write-Host 'Nothing to stop: no KMY visualiser processes are tracked.' -ForegroundColor Yellow
        return
    }

    $frontendStopped = Stop-ProcessTree -Entry $state.frontend
    $backendStopped = Stop-ProcessTree -Entry $state.backend
    Remove-Item -LiteralPath $stateFile -Force -ErrorAction SilentlyContinue

    if ($frontendStopped -or $backendStopped) {
        Write-Host 'Stopped the KMY visualiser frontend and backend.' -ForegroundColor Green
    } else {
        Write-Host 'The tracked frontend and backend were already stopped.' -ForegroundColor Yellow
    }
}

function Start-DevEnvironment {
    $existingState = Read-DevState
    if ($null -ne $existingState) {
        $frontendRunning = $null -ne (Get-TrackedProcess -Entry $existingState.frontend)
        $backendRunning = $null -ne (Get-TrackedProcess -Entry $existingState.backend)
        if ($frontendRunning -or $backendRunning) {
            Write-Host 'The KMY visualiser is already running. Use "dev status" for details.' -ForegroundColor Yellow
            return
        }
        Remove-Item -LiteralPath $stateFile -Force -ErrorAction SilentlyContinue
    }

    foreach ($port in 5173, 8080) {
        $listener = Get-PortOwner -Port $port
        if ($null -ne $listener) {
            throw "Port $port is already used by untracked process $($listener.OwningProcess). Stop it before starting the visualiser."
        }
    }

    $npm = (Get-Command npm.cmd -ErrorAction Stop).Source
    $gradleWrapper = Join-Path $backendDirectory 'gradlew.bat'
    if (-not (Test-Path -LiteralPath $gradleWrapper)) { throw "Gradle wrapper not found: $gradleWrapper" }

    New-Item -ItemType Directory -Path $runtimeDirectory -Force | Out-Null
    $frontendOut = Join-Path $runtimeDirectory 'frontend.out.log'
    $frontendErr = Join-Path $runtimeDirectory 'frontend.err.log'
    $backendOut = Join-Path $runtimeDirectory 'backend.out.log'
    $backendErr = Join-Path $runtimeDirectory 'backend.err.log'

    $backendProcess = $null
    $frontendProcess = $null
    try {
        $backendProcess = Start-Process -FilePath $gradleWrapper -ArgumentList 'bootRun' `
            -WorkingDirectory $backendDirectory -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput $backendOut -RedirectStandardError $backendErr
        $backendEntry = [ordered]@{
            pid = $backendProcess.Id
            startedAt = $backendProcess.StartTime.ToUniversalTime().ToString('o')
        }

        $frontendProcess = Start-Process -FilePath $npm `
            -ArgumentList @('run', 'dev', '--', '--host', '127.0.0.1', '--port', '5173', '--strictPort') `
            -WorkingDirectory $frontendDirectory -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput $frontendOut -RedirectStandardError $frontendErr
        $frontendEntry = [ordered]@{
            pid = $frontendProcess.Id
            startedAt = $frontendProcess.StartTime.ToUniversalTime().ToString('o')
        }

        [ordered]@{ frontend = $frontendEntry; backend = $backendEntry } |
            ConvertTo-Json | Set-Content -LiteralPath $stateFile -Encoding UTF8

        Wait-ForPort -Port 5173 -Name 'Frontend' -Entry $frontendEntry
        Wait-ForPort -Port 8080 -Name 'Backend' -Entry $backendEntry

        Write-Host 'KMY visualiser is running.' -ForegroundColor Green
        Write-Host 'Frontend: http://localhost:5173'
        Write-Host 'Backend:  http://localhost:8080'
        Write-Host 'Logs:     .dev/*.log'
    } catch {
        if ($null -ne $frontendProcess) { Stop-ProcessTree -Entry $frontendEntry | Out-Null }
        if ($null -ne $backendProcess) { Stop-ProcessTree -Entry $backendEntry | Out-Null }
        Remove-Item -LiteralPath $stateFile -Force -ErrorAction SilentlyContinue
        throw
    }
}

function Show-DevStatus {
    $state = Read-DevState
    $frontendRunning = $false
    $backendRunning = $false
    if ($null -ne $state) {
        $frontendRunning = $null -ne (Get-TrackedProcess -Entry $state.frontend)
        $backendRunning = $null -ne (Get-TrackedProcess -Entry $state.backend)
    }

    $frontendPort = Get-PortOwner -Port 5173
    $backendPort = Get-PortOwner -Port 8080
    Write-Host "Frontend: $(if ($frontendRunning -and $frontendPort) { 'running at http://localhost:5173' } elseif ($frontendPort) { "port 5173 used by untracked process $($frontendPort.OwningProcess)" } else { 'stopped' })"
    Write-Host "Backend:  $(if ($backendRunning -and $backendPort) { 'running at http://localhost:8080' } elseif ($backendPort) { "port 8080 used by untracked process $($backendPort.OwningProcess)" } else { 'stopped' })"
}

switch ($Command) {
    'start' { Start-DevEnvironment }
    'stop' { Stop-DevEnvironment }
    'restart' { Stop-DevEnvironment; Start-DevEnvironment }
    'status' { Show-DevStatus }
}

