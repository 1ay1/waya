@echo off
rem waya.cmd - Windows launcher for the waya CLI.
rem
rem Mirrors the POSIX `waya` script so `waya <command>` works from cmd.exe and
rem PowerShell. Most commands run under a POSIX shell (Git-Bash / MSYS2 / WSL);
rem `waya dev` uses the native PowerShell watcher when no bash is available.
rem
rem   waya new <name> [dir]   waya dev [target]   waya build [target]
rem   waya run [target]       waya list           waya clean   waya doctor
rem
setlocal
set "ROOT=%~dp0"

rem The first argument is the subcommand.
set "CMD=%~1"
if "%CMD%"=="" set "CMD=help"

rem Prefer a POSIX shell if one is on PATH - it runs the full-featured `waya`.
where bash >nul 2>nul
if %errorlevel%==0 (
    bash "%ROOT%waya" %*
    exit /b %errorlevel%
)

rem No bash. `dev` can still work via the PowerShell watcher; everything else
rem needs a POSIX shell, so point the user at one.
if /I "%CMD%"=="dev" (
    rem Shift off "dev" and pass the rest (target/build-dir) to dev.ps1.
    set "ARGS=%*"
    call set "ARGS=%%ARGS:*dev=%%"
    powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%scripts\dev.ps1" %ARGS%
    exit /b %errorlevel%
)

echo waya: this command needs a POSIX shell on Windows.
echo       Install Git for Windows ^(bash^) or use WSL, then re-run:  waya %*
echo       ^(the 'dev' command works without bash via PowerShell.^)
exit /b 1
