# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Intan RHX: a Qt 6 / C++17 desktop app (GPL-3) that acquires, displays, and records electrophysiology data from Intan RHD/RHS controllers over Opal Kelly USB. It is upstream Intan's code. Intan says it will not merge outside changes into public releases, so changes here are for a local fork.

## Ask before changing acquisition or saving code

Ask the user before editing any code that could affect data acquisition or data saving, and wait for their explicit approval. Do this even for a small change, a refactor, or a fix to something unrelated. Describe the change you want to make and the risk to recorded data. A bug in this code can silently corrupt or lose recordings. This covers:

- `Engine/API/` (all controllers, `RHXRegisters`, `RHXDataBlock`, `rhxglobals.h`) and `FPGA-bitfiles/`
- `Engine/Threads/usbdatathread.*`, `waveformprocessorthread.*`, `savetodiskthread.*`
- `Engine/Processing/`: `controllerinterface.*`, `datastreamfifo.*`, `waveformfifo.*`, `rhxdatareader.*`, `filter.*`, `softwarereferenceprocessor.*`, `signalsources.*`, `channel.*`, `systemstate.*`, `stateitem.*`, `stimparameters.*`, `impedancereader.*`, `XPUInterfaces/`, `SaveManagers/`, `DataFileReaders/`, plus `kernel.cl`
- Any other change to timing, buffering, sample parsing, filtering, stimulation, or file formats, wherever it lives

If you're not sure whether a file counts, ask.

## Build

- One qmake project: `IntanRHX.pro`. You add new `.cpp`/`.h` files to `SOURCES`/`HEADERS` by hand, and their directory must already be on `INCLUDEPATH`. Includes are flat (`#include "systemstate.h"`, not by path), so file names must be unique across directories.
- Build with Qt Creator, or from a shell: `qmake6 IntanRHX.pro && make` (Windows/MSVC: `qmake IntanRHX.pro && nmake` or `jom`). The README says the release binaries are built with Qt 6.10.1. The Qt modules used are core, gui, xml, multimedia, network, and widgets.
- Platform libraries are in `libraries/{Windows,Mac,Linux}` (okFrontPanel, OpenCL). Headers are in `includes/` (`okFrontPanel.h`, `CL/`). Windows delay-loads `okFrontPanel.dll`.
- On unix, several warnings are errors (`-Werror=suggest-override`, `-Werror=reorder`, `-Werror=implicit-fallthrough`, `-Werror=missing-declarations`, …). Use `override` on every overriding virtual, keep member-initializer order the same as declaration order, and mark free functions that are not declared in a header `static`. MSVC does not enforce these rules, so the Linux CI will catch what a Windows build lets through.
- These files must sit next to the built executable at runtime: `kernel.cl` (the OpenCL filter kernel, loaded from `applicationDirPath()` by `gpuinterface.cpp`), the `FPGA-bitfiles/*.bit` files, and the okFrontPanel shared library. Without them the app starts but fails on hardware or GPU paths.
- CI (`.github/workflows/build.yml`) builds only Debian/Ubuntu `.deb` packages, in podman containers (`contrib/ci/`, `contrib/debian/rules` → `dh --buildsystem=qmake6`), and validates the AppStream metainfo and desktop files. `contrib/ci/auto-build-deb.sh` refuses to run unless `CI` is set.

## Running without hardware

- At startup, `BoardSelectDialog` (in `main.cpp`) lists the connected controllers. If there are none, it offers **demo/synthetic mode** (`SyntheticRHXController`) or **playback** of a recorded `.rhd`/`.rhs` file (`PlaybackRHXController` + `DataFileReader`). Use these to exercise GUI and processing changes.
- `IntanRHX --settings=startup.ini` skips the startup dialogs. The keys are `device_serial`, `sample_rate_hz`, `stim_step_size_ua`, and `default_settings_file`, and an invalid value silently falls back to its default.

## Tests

The repository has no automated tests or test runner. To verify a change, build it and run it in synthetic or playback mode.

## Architecture

**Startup wiring** happens in `BoardSelectDialog::startSoftware()` (`GUI/Dialogs/boardselectdialog.cpp`). It creates objects in this order: an `AbstractRHXController` subclass (Live `RHXController` / `SyntheticRHXController` / `PlaybackRHXController`), then `SystemState`, `ControllerInterface`, `CommandParser`, and `ControlWindow`. It also makes most of the cross-object signal/slot connections, using the old string-based `SIGNAL()`/`SLOT()` syntax. When you add a signal between these top-level objects, connect it there.

**Controller abstraction** (`Engine/API/`): `AbstractRHXController` is the hardware API: FPGA bitfile upload, register programming through `RHXRegisters`, and reading `RHXDataBlock`s. The synthetic and playback controllers implement the same interface, so everything downstream treats every mode the same way. Board and chip variants are selected by the `ControllerType` enum: `ControllerRecordUSB2` (RHD USB interface board), `ControllerRecordUSB3` (RHD 512/1024ch recording controller), and `ControllerStimRecord` (RHS). The `is7310` flag marks the newer Opal Kelly XEM7310 boards. Global enums and constants, including the software version, are in `Engine/API/Hardware/rhxglobals.h`.

**Data pipeline** (owned by `ControllerInterface`, `Engine/Processing/controllerinterface.cpp`):

1. `USBDataThread` reads raw USB blocks from the controller into a `DataStreamFifo`.
2. `WaveformProcessorThread` parses the blocks (`RHXDataReader`), filters them and detects spikes through `XPUController`, which picks `CPUInterface` or the OpenCL `GPUInterface`, and writes the results into a `WaveformFifo`.
3. `WaveformFifo` is one buffer with several independent readers (`ReaderDisplay`, `ReaderDisk`, `ReaderAudio`, `ReaderTCP`). `ControllerInterface::runController()` runs the display loop on the GUI thread. `SaveToDiskThread`, `AudioThread`, and `TCPDataOutputThread` each read through their own reader. Each reader calls `requestReadNewData` and then `freeOldData`, so a reader that stops consuming stalls the buffer.
4. Saving goes through `SaveManager` subclasses, one per file format: traditional Intan `.rhd`/`.rhs`, one file per signal type, one file per channel. Reading back for playback uses the matching `DataFileReaders/*Manager`.

**State system**: `SystemState` (`Engine/Processing/systemstate.*`) holds nearly all user-visible settings as typed `StateItem` objects (`BooleanItem`, `IntRangeItem`, `DiscreteItemList`, `StringItem`, filename and TCP-socket items, …; see `stateitem.h`). They are registered by name into global lists or per-channel lists (`Channel::channelItems`). Because each item has a name:

- The XML settings save/load (`XMLInterface`), the `XMLGroup` on each item, and the TCP command interface all find items by that name. A new setting usually needs only a new `StateItem` in `SystemState`/`Channel`, and it then becomes reachable by `set`/`get` over TCP and saveable to XML automatically.
- `CommandParser` is the single entry point for changing state. The GUI (`ControlWindow` emits `sendSetCommand` etc.) and remote TCP clients (`TCPCommunicator`) both go through it. Restrict functions (`setRestricted`) block changes in invalid modes, for example while running.
- `SystemState` emits `stateChanged()`, throttled to at most once every 20 ms. GUI widgets connect to it and refresh themselves in `updateFromState()`. Wrap a batch of changes in `holdUpdate()`/`releaseUpdate()`.

**GUI** (`GUI/`): `ControlWindow` is the main window. `MultiColumnDisplay` → `WaveformDisplayColumn` → `MultiWaveformPlot` draws the waveforms. The control panel tabs are the `ControlPanel*Tab` classes. The analysis tools (ISI, PSTH, spectrogram, spike sorting) are dialogs that `ControllerInterface` also holds pointers to and feeds data to. The GUI is built in code; there are no `.ui` files. Icons, images, and sounds are listed in `IntanRHX.qrc`.

## Conventions

- Every source file starts with the same GPL license header, and that header contains the version number (`Version 3.5.2`). Intan's version-bump commits update the header in all ~220 files together with `SOFTWARE_*_VERSION_NUMBER` in `rhxglobals.h`. Copy this header into any new file.
- Release history is kept in `intan_rhx_release_notes.txt`.

## Reference documentation

- `docs/Intan_RHX_user_guide.pdf` is Intan's official RHX user guide (11 Feb 2021, updated 9 Jan 2026, 35 pages). It describes the original upstream software and is not maintained alongside our local fork's changes, so it may not match the fork's current behavior (for example, features we added such as recent settings or rescan ports are not covered); when the guide and the code disagree, the code is authoritative for the fork. Consult it for the original intended user-facing behavior before changing GUI, settings, TCP, recording, or stimulation features. Read it with the Read tool's `pages` parameter (max 20 pages per request). Page map: quick start/main window/display 4-9, analysis tools 10-13, acquisition/filters/impedance/audio/config/triggered recording/file formats 14-21, stimulation (RHS) 22-29, TCP 30-32 (including command-line startup), MATLAB import 33-35.
