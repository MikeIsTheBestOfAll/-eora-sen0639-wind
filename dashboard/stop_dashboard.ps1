param([int]$Port = 8765)

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$pidFile = Join-Path $scriptDir '.dashboard-server.pid'
$processId = $null

try {
    if (Test-Path -LiteralPath $pidFile) {
        $storedPid = (Get-Content -LiteralPath $pidFile -Raw).Trim()
        if ($storedPid -match '^\d+$') {
            $processId = [int]$storedPid
        }
    }

    if (-not $processId) {
        $listener = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($listener) {
            $processId = [int]$listener.OwningProcess
        }
    }

    if (-not $processId) {
        Remove-Item -LiteralPath $pidFile -Force -ErrorAction SilentlyContinue
        Write-Host 'The dashboard server is not running.'
        exit 0
    }

    $processInfo = Get-CimInstance Win32_Process -Filter "ProcessId=$processId" -ErrorAction SilentlyContinue
    $expected = $processInfo -and
        $processInfo.Name -match '^python' -and
        $processInfo.CommandLine -like '*serve_dashboard.py*'

    if (-not $expected) {
        throw "Port $Port is owned by another program. Nothing was stopped."
    }

    Stop-Process -Id $processId -Force
    Remove-Item -LiteralPath $pidFile -Force -ErrorAction SilentlyContinue
    Write-Host 'The dashboard server has stopped.' -ForegroundColor Green
    exit 0
}
catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
