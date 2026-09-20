# Recording Studio — Architecture Notes

Living reference of how the codebase actually works, so features can be
scoped without re-exploring from scratch each time. Update this file
whenever a change alters the facts below (new subsystem, refactor, new
concept like loop/punch regions once built, etc).

## Audio engine / recording

- `src/audio/AudioEngine.h/.cpp` — owns the RtAudio stream (48kHz, 512-frame
  buffer). `AudioEngine::rtCallback()` is the realtime audio callback.
  - When `transport.state() == TransportState::Recording`, captured input is
    written into `RingBuffer<float> m_captureRing`.
  - `setRecordTargetTrack(Track*)` selects which track receives capture.
  - Per callback: iterates tracks → `Mixer::mixClipInto()` per clip (handles
    clip bounds, fades, gain) → `m_transport.advance(nFrames)`.
  - **Gap (as of Phase 1):** nothing yet turns the capture ring buffer into a
    `Clip` on the track — recording capture has no finalization path.
- `src/audio/TransportClock.h` — `setState()` manages
  Recording/Playing/Stopped; `positionSamples()` / `advance()` control the
  playhead. Currently linear-only: no loop region or punch in/out logic.

## Clip / track model

- `src/model/Track.h` — `Track::m_clips` is
  `atomic<shared_ptr<const ClipList>>` (copy-on-write, RT-safe). Key methods:
  `addClip()`, `splitClip()`, `clipsSnapshot()`. `ClipList` is a vector —
  multiple clips per track are already supported.
- `src/model/Clip.h` — fields: `sessionStartSample`, `sourceOffsetSamples`,
  `lengthSamples`, `fadeInSamples`, `fadeOutSamples`.

## UI

- `src/ui/ClipLaneWidget.h/.cpp` — renders clips on a track lane; tracks
  single-clip selection via `m_selectedClipId`.
- `src/ui/TimeRulerWidget.h` — plain time ruler with playhead, no loop/punch
  region concept.
- **No loop-region, punch-region, or range-selection concept exists anywhere**
  in the model or UI as of Phase 1.
- App-wide dark Fusion theme is set in `main.cpp` (established during the
  effects-rack phase — new UI should match it).

## Undo / command system

- `src/command/Command.h` — interface: `redo()`, `undo()`, `text()`.
- `src/command/CommandStack.h` — LIFO stack, max 50 entries.
- `src/command/EditCommands.h` — e.g. `TrackClipsCommand` captures
  before/after `ClipList` snapshots and swaps between them on undo/redo.
- Pattern for new track-mutating actions: snapshot clips before → mutate →
  snapshot after → push a `TrackClipsCommand` (or similar) onto the stack.
- Effects param changes use `EffectChainCommand` + `SetEffectParamCommand<T>`
  (atomic snapshot swap via `EffectChain`) — see `src/audio/Effects.h/.cpp`.

## Session persistence

- `SessionIO` handles save/load, including effect chain serialization
  (added in the effects-rack phase). Media Library entries are also
  persisted (see git history).

## Tests

- Framework: **Qt Test** (`QObject` + `Q_OBJECT`, `QVERIFY`/`QCOMPARE`,
  private test slots).
- Location: `tests/test_*.cpp`, wired up via `tests/CMakeLists.txt`.
- Existing files: `test_Effects.cpp`, `test_EffectCommands.cpp`,
  `test_CommandStack.cpp`, `test_EditCommands.cpp`, `test_Mixer.cpp`,
  `test_SessionIO.cpp`, `test_ClipEditMath.cpp`.
- Convention: lightweight helper commands/fixtures (e.g. `IncrementCommand`
  in `test_CommandStack.cpp`) to test undo/redo without full model deps.
- Workflow rule: tests are written **before** implementation for each
  feature (see PLAN.md).

## Feature-specific state

- **Effects rack (done):** DSP in `src/audio/Effects.h/.cpp`
  (EQ/Compressor/Delay/Reverb), per-track `EffectChain` with atomic snapshot
  swap, `EffectChainCommand`/`SetEffectParamCommand<T>`, per-track
  scratch-buffer processing in `AudioEngine`, `EffectsRackPanel` dock UI.
- **Punch-in / loop recording (done):**
  - `src/audio/PunchRegion.h` — `{startSample, endSample}` value type; loop
    region and punch region are the same thing (per design decision).
  - `src/audio/PunchRecorder.h` — RT-safe capture buffer sized once to the
    region length. `process(pos, in, nFrames)` writes at
    `(pos - regionStart)`, so each loop pass overwrites the previous one
    sample-for-sample with no allocation and no explicit "clear" step.
    Tracks `passCount()` by detecting when the write offset goes backwards
    (wraparound = new pass).
  - `src/audio/TransportClock.h` — gained `setPunchRegion`,
    `setPunchLoopEnabled`, `setPreRollSamples`. `advance()` wraps the
    playhead back to `max(0, regionStart - preRoll)` once it reaches
    `regionEnd`, instead of continuing forward.
  - `src/audio/AudioEngine.h/.cpp` — owns a `PunchRecorder m_punchRecorder`.
    In `rtCallback`, when `transport.punchLoopEnabled()` and the region is
    valid, captured input goes to `m_punchRecorder.process()` instead of the
    plain `m_captureRing` (existing linear-recording path is untouched).
  - `src/command/PunchRecordingCommand.h` —
    `buildPunchRecordingCommand(track, recorder, sampleRate)` builds one
    `Clip` from the recorder's final buffer and returns a single
    `TrackClipsCommand`, so undo reverts an entire multi-pass loop-record
    session in one step (per approved design — passes are not individually
    undoable). Returns `nullptr` if nothing was captured.
  - `src/ui/TimeRulerWidget.h/.cpp` — right-mouse-drag defines the punch
    region (left-drag is still seek); paints it as a translucent band with
    in/out markers. `setPunchRegion()`/`punchRegion()` for external sync.
  - `src/ui/MainWindow.h/.cpp` — "Loop Record" checkbox + In/Out
    `QDoubleSpinBox` fields on the toolbar, two-way synced with the ruler's
    drag via `punchRegionEdited`. `onRecordClicked()` branches into the
    punch/loop path (single target track, 2s fixed pre-roll
    `kPunchPreRollSeconds`) when the checkbox is checked and the region is
    valid; `onStopClicked()` finalizes via `buildPunchRecordingCommand` and
    disables loop mode so plain recording resumes working unchanged.
  - Known limitation: punch/loop recording targets exactly one track (first
    armed/active), unlike plain recording which can target several at once.
  - Tests: `tests/test_PunchRecording.cpp` (region math, transport wrap,
    capture/overwrite, undo). UI drag/record/loop behavior was smoke-tested
    by building and launching the app, but full interactive verification
    (drag a region, record through 2+ passes, confirm playback/pre-roll/undo)
    has not been run by hand yet — do that before considering this fully
    verified per the approved verification plan.
