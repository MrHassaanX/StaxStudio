# Recording and workspace stabilization

This document records the validation evidence for the recording and workspace
stabilization checkpoint.

## Evidence and limits

The supplied `StaxStudio-20260925-151824.mkv` has 1,928 video packets and its
last video PTS is 44.900 seconds. The recording did not include a render-stage
trace. Its exact historical blocking API cannot be established from encoded
packets alone. Do not report that as proven.

The previous renderer already created its own QRhi/D3D11 device on a worker;
it did not borrow a QQuickWindow device. However, `endOffscreenFrame()` is a
synchronous GPU completion boundary. The production renderer now owns a native
D3D11 device, immediate context, shaders and program render target, without a
QRhi, QQuickWindow, swapchain, GUI callback or exposure dependency.

DXGI capture uses this same device. A three-slot staging ring supplies the
software encoder with top-left BGRA rows using `RowPitch`; CPU mapping uses
`D3D11_MAP_FLAG_DO_NOT_WAIT`. A full ring drops the requested readback instead
of waiting or growing. Qt receives an optional GPU-only shared texture under
a keyed mutex. Both devices flush their copy commands before transferring the
key; the producer never waits for a preview consumer. A DXGI timeout before
the first texture now returns unavailable rather than dereferencing null.

## Diagnostics

Each recording has a sibling `<recording>.mkv.diagnostics.jsonl`. An independent
monitor writes a snapshot approximately once per second, including scheduler,
capture, compositor, staging and recorder counters. Stop appends a summary.
Counters on the program engine are process totals; recorder counters reset
per recording. Three seconds with no submitted video explicitly fails the
recording rather than leaving an audio-only tail indefinitely.

Stages: 1 initialization, 2 scheduler wait, 3 target resize, 4 readback drain,
5 render setup, 6 DXGI acquire, 7 draw, 8 readback enqueue, 9 preview publish,
10 frame completed, 11 fatal graphics failure. `frameWorkNs`, `captureWorkNs`
and `readbackWorkNs` expose processing cost; `missedDeadlines`, `readbackDrops`
and recorder `droppedVideoFrames` distinguish three loss points. A device-loss
error requires restarting the app; automatic graphics-device recreation is
not implemented in this pass.

## Preview

The previous preview pass omitted `setViewport`, allowing inherited graphics
state to crop its target. It now sets the full item render-target viewport on
every pass and samples UV (0,0) through (1,1). Scissoring is disabled.
`StudioController::previewCanvasRect` uses `ProgramFrameMath` to fit the program
inside the padded workspace. The ProgramPreview item and all editor overlays
occupy that same rectangle. Source transforms and recording geometry are not
used to compensate for preview layout.

## Docks

`DockLayout` holds a validated binary split tree. Internal nodes store an ID,
horizontal/vertical orientation, a size ratio and two children. Leaves name
the preview or one of the five panels. Dock headers can drop on all four edges
of the preview or another panel. Removing the old leaf collapses its former
split. Splitter geometry is computed recursively with panel minimum sizes.
If the workspace is too small for those minima, it scrolls instead of overlapping.

`workspace-layout.json` is separate from the studio project. Version 1 stores
the tree, hidden panels and lock state. Writes are atomic and debounced by
400 ms; splitter release saves immediately. Reset restores the default tree.
Lock disables both panel movement and split resizing. Floating windows are
deferred; docked nesting, ordering, resizing and persistence are implemented.

## Audio

WASAPI native PCM/float is converted to interleaved float32 at 48 kHz by a
persistent libswresample context per source. PCM24 is unpacked into S32 before
conversion because FFmpeg has no packed S24 sample format. Endpoint channel
masks and channel counts are preserved; native 48 kHz float bypasses conversion.
The resampler retains filter history and uses produced sample counts for
timestamps on the shared monotonic clock. No automatic boost is applied.

Mixer gain remains linear in persisted configuration (1.0 = 0 dB); the UI
fader is logarithmic from -60 dB/silence to +20 dB, with a 0 dB mark and reset.
The existing 25 Hz meters and -90 dB floor remain. Multiple-source summation
still clips at full scale; there is no new compressor or limiter.

## Validation and manual review

Automated tests cover conversion, unity gain, duration, layout validation,
save/restore, header dragging, nested splits, horizontal/vertical resizing,
visibility, reset and lock. The production-path recording test runs for 60
real seconds with a real D3D11 preview consumer attached, detached and reattached.
Set `STAX_TEST_DESKTOP_OUTPUT=1` to run its 85-second 1080p DXGI variant.
The separate `gpuPreviewShowsAllCorners` test requires the Windows D3D11 Qt
Quick backend and checks all four colored corners at four workspace shapes.

Open `C:\Users\omenh\StaxStudio-build2\StaxStudio.exe` for manual review:

1. Record Display Capture for 90 seconds; minimize for the middle 30 seconds.
2. Confirm video reaches the end and matches the preview's framing/orientation.
3. Drag dock headers beside the preview and beside/below other docks.
4. Resize vertical and horizontal splitters, including preview versus bottom docks.
5. Restart to verify persistence; test layout Reset, Lock and panel Show/Hide.
6. Test mic and desktop audio at 0 dB, then a modest positive gain; test mute.
7. If output fails, retain both the MKV and its diagnostics JSONL.

Focused references: [Qt offscreen synchronization](https://doc.qt.io/qt-6/qrhi.html#endOffscreenFrame),
[Microsoft shared-surface synchronization](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/surface-sharing-between-windows-graphics-apis).
