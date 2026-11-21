# Build Intan RHX on this Windows machine (Qt 6 + MSVC via qmake/jom), deploy the
# runtime files next to the exe, and optionally launch it.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File .claude\skills\build\build.ps1 [-Config release|debug] [-Clean] [-CloseRunning] [-Run]
#
# -CloseRunning: if the exe being built is open, ask it to close (same as clicking its X button) and wait up
#   to 15 s. IntanRHX does not prompt on close: it stops acquisition/recording and exits. It is never force-killed;
#   if it doesn't exit in time (e.g. a dialog is open) the build stops with exit code 2.
#
# Toolchain paths can be overridden with env vars: INTAN_QTDIR, INTAN_VCVARS, INTAN_JOM, INTAN_BUILDDIR.
# Exit codes: 0 ok, 1 build/tool failure, 2 the target exe is running (locked) and was not closed.

param(
    [ValidateSet('release', 'debug')][string]$Config = 'release',
    [switch]$Clean,
    [switch]$CloseRunning,
    [switch]$Run
)

# Native tools write to stderr; under PowerShell 5.1 'Stop' would turn that into a failure. Check exit codes instead.
$ErrorActionPreference = 'Continue'

function Stop-Build([string]$msg, [int]$code = 1) {
    Write-Host "BUILD_FAILED: $msg"
    exit $code
}

$Src      = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$QtDir    = if ($env:INTAN_QTDIR)    { $env:INTAN_QTDIR }    else { 'C:\Qt\6.10.3\msvc2022_64' }
$VcVars   = if ($env:INTAN_VCVARS)   { $env:INTAN_VCVARS }   else { 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat' }
$Jom      = if ($env:INTAN_JOM)      { $env:INTAN_JOM }      else { 'C:\Qt\Tools\QtCreator\bin\jom\jom.exe' }
$BuildDir = if ($env:INTAN_BUILDDIR) { $env:INTAN_BUILDDIR } else { "$Src-build" }
$OutDir   = Join-Path $BuildDir $Config
$Exe      = Join-Path $OutDir 'IntanRHX.exe'
$QMake    = Join-Path $QtDir 'bin\qmake.exe'
$Deploy   = Join-Path $QtDir 'bin\windeployqt.exe'

foreach ($p in @($QMake, $Deploy, $VcVars, (Join-Path $Src 'IntanRHX.pro'))) {
    if (-not (Test-Path $p)) { Stop-Build "not found: $p" }
}

# The linker can't overwrite a running exe. Only close it gracefully on request; never kill it: it may be recording.
$running = @(Get-Process IntanRHX -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Exe })
if ($running.Count -gt 0) {
    if (-not $CloseRunning) {
        Stop-Build "$Exe is running (PID $($running.Id -join ', ')). Close it first, or pass -CloseRunning." 2
    }
    foreach ($proc in $running) {
        Write-Host "Closing IntanRHX (PID $($proc.Id), window '$($proc.MainWindowTitle)')"
        if (-not $proc.CloseMainWindow()) {
            Stop-Build "PID $($proc.Id) has no main window to close; a dialog is probably open (e.g. 'No Intan Controllers Detected'). Close it manually." 2
        }
        if (-not $proc.WaitForExit(15000)) {
            Stop-Build "PID $($proc.Id) did not exit within 15 s (a dialog may be open). Not force-killing it; close it manually." 2
        }
        Write-Host "Closed PID $($proc.Id)"
    }
}

# Load the MSVC x64 environment into this process.
$envDump = & cmd.exe /c "call `"$VcVars`" >nul && set"
if ($LASTEXITCODE -ne 0) { Stop-Build "vcvars64.bat failed" }
foreach ($line in $envDump) {
    if ($line -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
}
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { Stop-Build "cl.exe not on PATH after vcvars64.bat" }

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Removing $BuildDir"
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Force $BuildDir | Out-Null
Push-Location $BuildDir
try {
    Write-Host "== qmake ($QtDir)"
    & $QMake (Join-Path $Src 'IntanRHX.pro')
    if ($LASTEXITCODE -ne 0) { Stop-Build "qmake failed" }

    Write-Host "== compile ($Config)"
    if (Test-Path $Jom) { & $Jom -j $env:NUMBER_OF_PROCESSORS $Config }
    else                { & nmake /nologo $Config }
    if ($LASTEXITCODE -ne 0) { Stop-Build "compile/link failed" }
}
finally {
    Pop-Location
}
if (-not (Test-Path $Exe)) { Stop-Build "build reported success but $Exe is missing" }

Write-Host "== deploy runtime files"
Copy-Item (Join-Path $Src 'kernel.cl'), (Join-Path $Src 'libraries\Windows\okFrontPanel.dll') $OutDir -Force
Copy-Item (Join-Path $Src 'FPGA-bitfiles\*.bit') $OutDir -Force
& $Deploy "--$Config" --no-translations $Exe | Out-Null
if ($LASTEXITCODE -ne 0) { Stop-Build "windeployqt failed" }

Write-Host "BUILD_OK: $Exe"

if ($Run) {
    $p = Start-Process $Exe -WorkingDirectory $OutDir -PassThru
    Start-Sleep -Seconds 5
    $p.Refresh()
    if ($p.HasExited) { Stop-Build "IntanRHX exited within 5 s (exit code $($p.ExitCode))" }
    Write-Host "RUNNING: PID $($p.Id), window '$($p.MainWindowTitle)'"
}
exit 0
