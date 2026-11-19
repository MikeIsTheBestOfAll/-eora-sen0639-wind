param(
    [int]$Port = 8765,
    [switch]$NoBrowser
)

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$serverScript = Join-Path $scriptDir 'serve_dashboard.py'
$pidFile = Join-Path $scriptDir '.dashboard-server.pid'
$stdoutLog = Join-Path $scriptDir 'dashboard-server.log'
$stderrLog = Join-Path $scriptDir 'dashboard-server-error.log'
$url = "http://127.0.0.1:$Port/"

function Test-DashboardReady {
    try {
        $response = Invoke-WebRequest -UseBasicParsing -Uri $url -TimeoutSec 1
        return ($response.StatusCode -eq 200 -and $response.Content -match 'SEN0639')
    }
    catch {
        return $false
    }
}

function Open-DashboardBrowser {
    if ($NoBrowser) {
        return
    }

    $browserCandidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft\Edge\Application\msedge.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft\Edge\Application\msedge.exe'),
        (Join-Path $env:ProgramFiles 'Google\Chrome\Application\chrome.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'Google\Chrome\Application\chrome.exe'),
        (Join-Path $env:LOCALAPPDATA 'Google\Chrome\Application\chrome.exe')
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }

    if ($browserCandidates.Count -gt 0) {
        Start-Process -FilePath $browserCandidates[0] -ArgumentList $url
    }
    else {
        Start-Process $url
    }
}

function Resolve-PythonCommand {
    $platformioPythonw = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\pythonw.exe'
    if (Test-Path -LiteralPath $platformioPythonw) {
        return @{ FilePath = $platformioPythonw; Prefix = @() }
    }

    $platformioPython = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
    if (Test-Path -LiteralPath $platformioPython) {
        return @{ FilePath = $platformioPython; Prefix = @() }
    }

    $pyLauncher = Get-Command py.exe -ErrorAction SilentlyContinue
    if ($pyLauncher) {
        return @{ FilePath = $pyLauncher.Source; Prefix = @('-3') }
    }

    $python = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($python) {
        return @{ FilePath = $python.Source; Prefix = @() }
    }

    throw 'Python 3 was not found. Install Python or verify the PlatformIO installation.'
}

try {
    if (Test-DashboardReady) {
        Write-Host "Dashboard is already running: $url" -ForegroundColor Green
        Open-DashboardBrowser
        exit 0
    }

    $pythonCommand = Resolve-PythonCommand
    $arguments = @()
    $arguments += $pythonCommand.Prefix
    $arguments += @($serverScript, '--no-browser', '--host', '127.0.0.1', '--port', [string]$Port)

    $serverProcess = Start-Process `
        -FilePath $pythonCommand.FilePath `
        -ArgumentList $arguments `
        -WorkingDirectory $scriptDir `
        -WindowStyle Hidden `
        -RedirectStandardOutput $stdoutLog `
        -RedirectStandardError $stderrLog `
        -PassThru

    Set-Content -LiteralPath $pidFile -Value $serverProcess.Id -Encoding ascii

    $ready = $false
    for ($attempt = 0; $attempt -lt 40; $attempt++) {
        if (Test-DashboardReady) {
            $ready = $true
            break
        }
        if ($serverProcess.HasExited) {
            break
        }
        Start-Sleep -Milliseconds 250
    }

    if (-not $ready) {
        if (-not $serverProcess.HasExited) {
            Stop-Process -Id $serverProcess.Id -Force
        }
        Remove-Item -LiteralPath $pidFile -Force -ErrorAction SilentlyContinue
        $details = if (Test-Path -LiteralPath $stderrLog) {
            (Get-Content -LiteralPath $stderrLog -Tail 12 -ErrorAction SilentlyContinue) -join [Environment]::NewLine
        } else {
            'No error log was generated.'
        }
        throw "The local dashboard server did not start.`n$details"
    }

    Write-Host "Dashboard started: $url" -ForegroundColor Green
    Write-Host 'The server is running in the background.'
    Write-Host 'Double-click stop_dashboard.bat when you want to stop it.'
    Open-DashboardBrowser
    exit 0
}
catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
