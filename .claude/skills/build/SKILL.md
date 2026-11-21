---
name: build
description: Compile Intan RHX on this Windows machine (Qt 6.10.3 + Visual Studio 2026 MSVC, qmake + jom), copy the runtime files next to the exe, and optionally launch it. Use when asked to build, compile, rebuild, clean-build, or run/launch the app, or to check that a code change compiles.
---

# Build Intan RHX (Windows, this machine)

One script does everything: `.claude/skills/build/build.ps1`. Run it from the repo root and send the output to a log, because the compiler prints a lot:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .claude\skills\build\build.ps1 *> "<scratchpad>\build.log"; $code = $LASTEXITCODE
Get-Content "<scratchpad>\build.log" | Select-String "error|warning C|BUILD_OK|BUILD_FAILED|RUNNING"; "exit=$code"
```

Use a PowerShell timeout of about 10 minutes. A clean build takes a few minutes; an incremental one is faster.

Options:
- `-Config debug`: debug build (output in `debug\`). The default is `release`.
- `-Clean`: deletes the whole build directory first. Use it when you suspect stale objects, or after changing `IntanRHX.pro` include paths.
- `-CloseRunning`: if the exe being built is open, asks it to close (the same as clicking its X) and waits up to 15 s before building. IntanRHX does **not** prompt on close: it stops acquisition, including any recording, and exits. So pass this only when the user asked for the running app to be closed or restarted (e.g. "rebuild and relaunch", "close it and build"). Otherwise build without it and ask if the exe turns out to be open. The script never force-kills: if the app doesn't exit in time (usually because a dialog is open), the build stops with exit 2.
- `-Run`: launches the exe after a successful build and checks that it is still alive after 5 s. `-CloseRunning -Run` together means "restart with the new build".

## What the script does

1. Loads the MSVC x64 environment from `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat` (VS 2026, MSVC 14.52, Windows SDK 10.0.28000).
2. Runs `C:\Qt\6.10.3\msvc2022_64\bin\qmake.exe` on `IntanRHX.pro` in `c:\src\Intan-RHX-build\` (outside the repo, so git stays clean).
3. Compiles with `C:\Qt\Tools\QtCreator\bin\jom\jom.exe -j <cores>`, or falls back to `nmake` if jom is missing.
4. Copies `kernel.cl`, `libraries\Windows\okFrontPanel.dll`, and `FPGA-bitfiles\*.bit` into the output folder, then runs `windeployqt` for the Qt DLLs.

Output: `c:\src\Intan-RHX-build\release\IntanRHX.exe` (or `debug\`).

To use a different toolchain, set `INTAN_QTDIR`, `INTAN_VCVARS`, `INTAN_JOM`, or `INTAN_BUILDDIR` before running the script.

## Reading the result

- `'vswhere.exe' is not recognized` (shown as `NativeCommandError`): harmless. `vcvars64.bat` prints it because the VS Installer folder has no `vswhere.exe`, but the environment still loads, and the script checks for `cl.exe`.
- `BUILD_OK: <path>` and exit 0: success. Report the build as successful and name any `warning C` lines that come from files you changed. The `C4100 'testMode'` warning in `controllerinterface.cpp` is upstream and was already there.
- `BUILD_FAILED: ... is running` and exit 2: the user has that exe open, so the linker can't overwrite it. **Never kill IntanRHX** (`Stop-Process`, `taskkill`), because it may be recording. Ask the user to close it, or rerun with `-CloseRunning` once the user says to.
- `BUILD_FAILED: ... did not exit within 15 s` or `... no main window` and exit 2: the close request didn't take effect. This usually means a dialog is open, for example "No Intan Controllers Detected", which appears when another instance already holds the board. Tell the user to close the app themselves; don't escalate to a kill.
- `BUILD_FAILED` and exit 1: find the first `error C####` or `error LNK####` line in the log and fix that first; later errors often follow from it. An `LNK2019 unresolved external` for a new class usually means the `.cpp` was not added to `SOURCES` in `IntanRHX.pro`.

## MSVC accepts code that the Linux CI rejects

The Linux build treats some warnings as errors that MSVC does not check: `override` on every overriding virtual, member initializer order matching declaration order, `static` on free functions not declared in a header, and no implicit switch fallthrough. A clean Windows build does not prove the code passes CI, so review new code for those rules by hand.

## Running

`-Run` opens the **Select Intan Controller** dialog. You can't click through it, so tell the user to choose demo mode (synthetic data) or playback to exercise GUI changes, or a connected controller for hardware. A successful build only shows that the code compiles. Say so plainly, and do not claim that a feature works until the user has tried it.

Building and running don't change the source. The CLAUDE.md rule to ask before editing acquisition or saving code applies to fixing build errors in those files, not to running this skill.
