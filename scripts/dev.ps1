# scripts/dev.ps1 - waya live-reload dev loop (Windows / PowerShell).
#
# Watches the source tree, rebuilds on any change, and restarts the target.
# The browser refreshes itself a moment after each successful rebuild.
#
# Usage:
#   pwsh scripts/dev.ps1 [target] [build-dir]
#     target     CMake target / binary to run   (default: counter)
#     build-dir  CMake build directory          (default: build)
#
# Requires: cmake, a C++26 toolchain. (The waya runtime's networking is POSIX
# today; on Windows use WSL or build under MSYS2 until the Winsock port lands.)

param(
    [string]$Target = "counter",
    [string]$Build  = "build"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root

$Watch = @("include", "examples", "tests") | Where-Object { Test-Path $_ }
$server = $null

function Log([string]$msg) { Write-Host "waya dev " -ForegroundColor Cyan -NoNewline; Write-Host $msg }

Log "target: $Target   (pass a target to pick another: dev.ps1 <name>)"

if (-not (Test-Path "$Build/CMakeCache.txt")) { cmake -S . -B $Build | Out-Null }

function Rebuild-And-Run {
    Log "building $Target..."
    $out = cmake --build $Build --target $Target -j 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "waya dev " -ForegroundColor Cyan -NoNewline
        Write-Host "build failed - keeping the last good server up; errors:" -ForegroundColor Red
        $out | Select-Object -Last 20 | ForEach-Object { Write-Host $_ }
        return
    }
    $bin = Get-ChildItem -Path $Build -Recurse -Filter "$Target*" -File |
           Where-Object { $_.Extension -in @("", ".exe") } | Select-Object -First 1
    if (-not $bin) { Log "no binary produced - keeping the last good server up."; return }

    if ($script:server) { Stop-Process -Id $script:server.Id -ErrorAction SilentlyContinue; $script:server = $null }
    $env:WAYA_NO_OPEN = if ($script:firstDone) { "1" } else { $null }
    $script:server = Start-Process -FilePath $bin.FullName -PassThru
    $script:firstDone = $true
    Log "running $Target (pid $($script:server.Id)) - edit & save to reload"
}

Rebuild-And-Run

# FileSystemWatcher: instant, native to .NET/PowerShell on Windows.
$fsw = New-Object System.IO.FileSystemWatcher
$fsw.Path = $Root
$fsw.IncludeSubdirectories = $true
$fsw.EnableRaisingEvents = $true

Log "watching $($Watch -join ', ') (FileSystemWatcher)...  Ctrl-C to stop"
$last = Get-Date
try {
    while ($true) {
        $change = $fsw.WaitForChanged([System.IO.WatcherChangeTypes]::All, 1000)
        if ($change.TimedOut) { continue }
        if ($change.Name -notmatch '\.(hpp|cpp)$') { continue }
        # debounce: coalesce bursts of save events
        if ((Get-Date) - $last -lt [TimeSpan]::FromMilliseconds(300)) { continue }
        $last = Get-Date
        Rebuild-And-Run
    }
} finally {
    if ($script:server) { Stop-Process -Id $script:server.Id -ErrorAction SilentlyContinue }
}
