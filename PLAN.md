# Recording Studio — Feature Plan

Inspired by Soundtrap and Pro Tools. Undo/redo is already done ([[Command]] stack,
`src/command/`). This tracks the next 10 features plus ongoing UI work.

Persisted here (per user preference) so work can resume after a crash/restart
without relying on chat history. Update status as items complete.

## UI direction

- **Style:** Professional dark DAW theme (dark background, high-contrast
  waveforms/meters/text, flat modern styling — similar to Pro Tools/Ableton).
- **Approach:** No separate theming phase. Each feature below is styled to the
  dark theme as it's built, so visual consistency is enforced per-PR rather
  than retrofitted later.

## Feature order

1. [x] Effects rack per track (EQ, compressor, reverb, delay)
2. [x] Punch-in / loop recording
3. [x] Playlist comping (multiple takes per track, comp best parts) — v1: whole-take comping only, see below
4. [x] Clip-level editing tools (trim/fade/gain handles directly on clips)
5. [x] Virtual instruments (basic synth + sampler, MIDI-playable) — v1: synth only, on-screen keyboard only, see below
6. [x] MIDI piano-roll editor — v1: move/resize/draw/delete notes + velocity
    lane, no full manual pass yet, see below
7. [x] Automation lanes (volume/pan over time) — v1 scope, no full manual
    pass yet, see below
8. [x] Bus routing / sends (aux tracks, submixes) — v1: aux sends + master
    bus, including master-effects UI, see below
9. [x] Loop/sample library browser
10. [x] Export/bounce with format + stems options
11. [x] Multi-source input: mic + system audio (loopback) recorded to separate tracks (Linux/PulseAudio)
12. [x] Mixer view: dockable strip at the bottom with a vertical fader, pan,
    mute/solo, and effect slots per track (Pro Tools/Ableton-style), toggled
    via the View menu

## Completed: Clip-level gain handle (trim/fade already existed)

Goal (feature #4): trim/fade/gain handles directly on clips. Trim and fade
already had drag handles in `ClipLaneWidget` before this session; the only
gap was gain (right-click → dialog only). Added a draggable gain handle to
match.

- [x] `ui/ClipEditMath.h`: `clampClipGain`, `gainAfterVerticalDrag`,
      `gainLineY` (pure, tested) — a horizontal line across the clip (0.0 at
      the bottom edge, 2.0 at the top, unity at vertical center), dragged
      up/down like Pro Tools/Audacity clip gain.
- [x] `ClipLaneWidget`: new `DragMode::Gain`, hit-tested after trim/fade
      edges; reuses the existing generic "push TrackClipsCommand on release
      if changed" undo path already shared by move/trim/fade — no new
      command type needed. Waveform amplitude now also scales with
      `clip->gain` for live visual feedback while dragging (can visually
      clip above unity, matching real DAWs).

Verification plan (approved):
- [x] Automated: 9 new cases in `tests/test_ClipEditMath.cpp` (clamp range,
      drag-to-gain math both directions, zero-height guard, line-position
      math at 0/1/2 gain and with a non-zero clip top offset). Full suite
      (16/16) passes.
- [x] Smoke-tested: app launches clean, gain line renders (gold, brightens
      while dragging) on clips at the correct vertical position.
- [ ] Full manual: drag the gain line up/down on a clip with a visible
      waveform, confirm it changes audibly on playback and undo/redo
      reverts it. (Pixel-precise click-drag wasn't reliably reproducible
      from this session's screenshot tooling — logic is unit-tested and
      wired through the same proven pattern as trim/fade, but worth a
      manual pass.)

## Completed: Multi-source input (mic + system audio)

Goal: record microphone and system/PC-playback audio (PulseAudio `.monitor`
source) simultaneously onto two separate tracks. Linux only (PulseAudio
monitor sources); no Windows/macOS loopback support in this pass.

Current state: `AudioEngine` has exactly one RtAudio stream, one input
device, one capture ring buffer. Armed tracks all currently receive
*duplicate copies* of the same single-source audio — there is no per-track
input routing today.

Design:
- [x] `Track` gains an `AudioSource { Mic, SystemAudio }` field (default `Mic`).
- [x] `SettingsDialog` splits "Input Device" into "Microphone Device" and
      "System Audio Device" (latter lists PulseAudio `.monitor` sources from
      `AudioEngine::listDevices()`).
- [x] `AudioEngine` runs a second, independent RtAudio stream/callback
      (`rtSystemAudioCallback`) for the system-audio device, with its own
      ring buffer (`systemAudioCaptureRing()`), concurrently with the
      existing mic stream.
- [x] Per-track "source" UI control next to the existing "Rec Arm" checkbox
      (`TrackWidgets.cpp`, undoable via `TrackState`/`TrackStateCommand`).
- [x] `MainWindow::onRecordClicked()`/`onStopClicked()` reworked to route
      each armed track to the ring buffer matching *its* assigned source
      (`audio/RecordRouting.h::splitTracksBySource`), instead of fanning one
      shared clip out to all armed tracks. Punch/loop recording still only
      supports Mic-source tracks (system-audio armed tracks are skipped
      with a warning in that mode).

Verification plan (approved):
- [x] Automated: `tests/test_RecordRouting.cpp` covers track→source split
      logic (mockable, no real hardware). Full suite (9/9) passes.
- [x] Discovered during manual verification: this machine runs PipeWire,
      and RtAudio's `pulse` backend does NOT enumerate `.monitor` sources at
      all (confirmed via a probe: only 2 raw hardware devices listed, no
      monitors) — so there was nothing to pick in the System Audio dropdown.
      Fixed with `src/audio/SystemAudioLoopback.h`: MainWindow spawns
      `pw-loopback -C @DEFAULT_SINK@ --playback-props='media.class=Audio/
      Source node.name=rsd_sysaudio_src ...'` on startup, which re-exposes
      the default sink's monitor as a plain Audio/Source node — RtAudio DOES
      enumerate that kind of node. Verified live: after launching the app,
      `recording_studio` process shows a third RtAudio device, "Recording
      Studio System Audio", and it disappears (process cleanly terminated)
      when the app quits. Tests: `test_SystemAudioLoopback.cpp` (pure
      arg-building logic).
- [x] Manual verification: confirmed working by user — recording system
      audio (any app: Teams, YouTube, etc., via the shared default-sink
      mix) onto a track works end-to-end.

## Completed

- [x] Undo/redo (`src/command/Command.h`, `CommandStack`, `EditCommands.h`)
- [x] Effects rack: `src/audio/Effects.h/.cpp` (EQ/Compressor/Delay/Reverb DSP),
      `Track` effect chain (`EffectChain`, atomic snapshot swap), `EffectChainCommand`
      + `SetEffectParamCommand<T>`, `AudioEngine` per-track scratch-buffer
      processing, `SessionIO` effect serialization, `EffectsRackPanel` dock UI,
      app-wide dark Fusion theme (`main.cpp`). Tests: `test_Effects.cpp`,
      `test_EffectCommands.cpp`, `SessionIO` effect round-trip test.
- [x] Punch-in / loop recording: `src/audio/PunchRegion.h`, `PunchRecorder.h`
      (RT-safe overwrite-per-pass capture), `TransportClock` loop-wrap +
      pre-roll, `AudioEngine::rtCallback` gates capture to the punch region,
      `command/PunchRecordingCommand.h` (`buildPunchRecordingCommand` — one
      `TrackClipsCommand` for the whole multi-pass session), `TimeRulerWidget`
      right-drag region + paint, `MainWindow` Loop Record checkbox + In/Out
      spin boxes. Tests: `test_PunchRecording.cpp`. Manual UI verification
      (drag region, record through loops, confirm playback/undo/pre-roll
      audibly) still needs to be run by hand — not automatable from this
      session (no GUI driver for the native Qt app).

## Completed: Thinner tracks + clearer waveform

Goal: shrink the per-track row height (was ~180-200px, dominated by a
9-item vertical stack of controls plus a 120px-min waveform lane) and make
quiet/moderate waveforms read as bold traces instead of thin lines.

- [x] `ClipLaneWidget`: lane min height 120px → 60px (`kLaneHeight`).
- [x] `ClipLaneWidget`: waveform amplitude auto-normalized per clip
      (`ui/WaveformDisplayMath.h::computeWaveformDisplayScale` — scales the
      drawn peak up to the lane's full height, clamped to a 6x max boost so
      near-silent noise floor doesn't get blown into jagged spikes). Tests:
      `tests/test_WaveformDisplayMath.cpp`.
- [x] `TrackWidgets.cpp`: header controls regrouped from a single 9-item
      `QVBoxLayout` into a compact 4-row `QGridLayout` (name+Active /
      Mute+Solo+Arm+source-combo / pan-dial+Gain-L / Gain-R), header width
      200px → 260px, pan dial 48px → 28px, gain sliders fixed 16px tall.

Verification plan (approved):
- [x] Automated: `tests/test_WaveformDisplayMath.cpp` covers the
      normalization math (no-boost on full-scale, boost on quiet, clamp on
      near-silence, no divide-by-zero on true silence). Full suite (11/11)
      passes.
- [x] Manual: built and ran the app on the live desktop session, screenshot
      confirmed track rows are visibly thinner and waveforms render as bold
      filled traces even for moderate-amplitude clips.

## Completed: Clip drag visual jump fix + inline track FX indicator

Two small fixes bundled together (found while testing clip dragging):

- [x] Bug: grabbing a clip to drag it made it visibly jump/resize before any
      mouse movement. Root cause: `ClipLaneWidget` locked the drag-gesture
      scale to a *local*, per-track recomputation (`timelineLengthSamples()`)
      instead of the *shared* cross-track/zoom-aware scale
      (`m_sharedTimelineLength`) already used to paint the lane — so the
      effective scale changed the instant the drag started.
      Fixed via `ui/TimelineScaleMath.h::resolveDragLockSamples()` (also
      reused by `effectiveTimelineLength()` for consistency). Tests:
      `tests/test_TimelineScaleMath.cpp` (regression cases for the mismatch).
- [x] Feature: inline "FX" button on each track row header (next to the
      "Active" radio), showing the effect count (`FX` / `FX: N`). Clicking it
      selects the track and raises the (possibly tabbed-behind) Effects Rack
      dock — a shortcut to a track's effects without hunting for the panel.
      `ui/TrackEffectsLabel.h::formatEffectsButtonLabel()` (pure, tested),
      `TrackRowWidget::refreshEffectsButton()`,
      `EffectsRackPanel::effectCountChanged` signal wired through
      `TimelineView` to `MainWindow`. Tests: `tests/test_TrackEffectsLabel.cpp`.

Verification plan (approved):
- [x] Automated: both above, full suite (13/13) passes.
- [ ] Manual: build and run the app — confirm dragging a clip no longer
      visibly jumps on grab; confirm the FX button shows the right count,
      updates live on add/remove, and clicking it selects the track + raises
      the effects dock.

## Completed: Per-track horizontal scrolling when zoomed in

Goal: when zoomed in far enough that a track's content extends past the
visible window, let the user scroll to see the rest — previously content
past the window simply wasn't drawn at all, with no way to reach it.

Design (per-track by default, Shift syncs all tracks, ruler/master strip
stay fixed to the full overview range — approved):
- [x] `ui/ClipLaneScrollMath.h`: pure clamp/range logic
      (`clampScrollOffset`, `maxScrollOffsetSamples`,
      `scrollbarShouldBeEnabled`). Tests: `tests/test_ClipLaneScrollMath.cpp`.
- [x] `ClipLaneWidget`: per-lane scroll offset folded into
      `xToSample`/`sampleToX`/painting, locked during a clip-drag gesture the
      same way the timeline scale already is (reusing the pattern from the
      drag-jump fix above, so scrolling can't reintroduce that bug). Native
      horizontal wheel/trackpad swipe pans the lane
      (`ClipLaneWidget::wheelEvent`); Shift-held emits
      `syncScrollToAllRequested`.
- [x] `TrackRowWidget`: added a `QScrollBar` under each lane, wired
      bidirectionally to the lane's scroll offset/range signals; forwards
      Shift-synced scroll requests up to `TimelineView`.
- [x] `TimelineView`: broadcasts a Shift-synced scroll position from one row
      to every other row; forwards the full (un-zoomed) content extent from
      `MainWindow` down to every lane so each knows how far it can scroll.
- [x] `MainWindow::refreshTimelineScale()`: now also pushes
      `scale.pinnedBaseSamples` (full content extent) via
      `TimelineView::setContentExtentSamples()`. Ruler and master waveform
      strip were left untouched — they were never wired to per-track scroll
      state, so they stay fixed to the overview range for free.

Verification plan (approved):
- [x] Automated: `tests/test_ClipLaneScrollMath.cpp`, full suite (14/14)
      passes.
- [ ] Manual: zoom in, confirm each track's scrollbar appears/is draggable;
      confirm horizontal wheel/trackpad swipe pans a track; confirm
      Shift+scroll (wheel or scrollbar drag) moves all tracks together;
      confirm ruler/master strip stay fixed; confirm dragging a clip while
      scrolled away from sample 0 still doesn't jump.

## Completed: Playlist comping (whole-take, v1 scope)

Goal (feature #3): comp between multiple takes of a punch/loop recording.
Scoped down from full sub-region comping (approved) — v1 is **whole-take**
comping: expand a track to see every captured pass as a stacked take lane,
click one to make it the active comp for the whole punch region. Sub-region
picking (different bits of the region from different takes) is a possible
future extension, not built here.

Design (approved):
- [x] `PunchRecorder`: pre-allocate 8 separate pass buffers at `prepare()`
      (RT-safe, no allocation during capture). A 9th+ pass overwrites the
      8th slot; `takesCapExceeded()` flags this for a UI warning.
      `audio/PunchTakeMath.h::takeBufferIndexForPass()` (pure, tested) picks
      the slot.
- [x] `Track` gains a `takeLanes` list (same copy-on-write/atomic-swap
      pattern as `clips`/`effects`) — alternate takes from the most recent
      punch/loop recording. A new punch/loop recording replaces the
      previous take set (v1: one take group per track, not stacked
      per-region history).
- [x] `command/EditCommands.h::TrackTakeLanesCommand` mirrors
      `TrackClipsCommand`. Punch recording finalization pushes a
      `CompositeCommand` of the comp-clip update (unchanged behavior) +
      the take-lanes update, so one undo reverts both.
- [x] `model/CompMath.h::promoteTakeToComp()` (pure, tested): given the
      track's existing clips + a chosen take, returns the clip list with
      that region's clips removed/trimmed and the take promoted in.
- [x] UI: "Takes" toggle button per track row (next to FX) reveals stacked
      take-lane widgets (`ui/TakeLaneWidget`) when takes exist; clicking one
      promotes it to the active comp (undoable, `MainWindow::onTakeSelected`).

Verification plan (approved):
- [x] Automated: `tests/test_PunchTakeMath.cpp`, `tests/test_CompMath.cpp`.
      Full suite (16/16) passes (existing `test_PunchRecording.cpp`
      assertions on `buffer()`/`passCount()` still hold — "last pass"
      semantics preserved).
- [x] Smoke-tested: app launches and renders cleanly with the new "Takes"
      button (correctly disabled when a track has no takes yet).
- [ ] Full manual: record a punch/loop pass 3 times with different content
      each pass; confirm all 3 show up as take lanes; confirm clicking each
      swaps the active comp audibly and visibly; confirm undo/redo works;
      confirm a 9th pass warns instead of crashing. (Needs a live mic input
      to test — not done from this session.)

## Completed: Virtual instruments (basic synth, v1 scope)

Goal (feature #5). Scoped down (approved): on-screen keyboard only (no
hardware MIDI input), simple built-in synth only (oscillator + ADSR +
one-pole filter, no sampler).

Design (approved):
- [x] `Track` gains `TrackKind { Audio, Instrument }`, a `SynthParams`
      (atomics, mutated live like `Effect` params), a `midiClips` list (same
      copy-on-write pattern as `clips`), a `NoteEventQueue` (SPSC lock-free,
      GUI pushes live note-on/off, RT drains) and a `SynthEngine` (RT-owned
      voice pool).
- [x] `audio/SynthMath.h`: pure oscillator/ADSR/filter math (tested).
      `audio/NoteEventQueue.h`, `audio/Synth.h` (SynthVoice/SynthEngine,
      fixed 8-voice pool, no RT allocation).
- [x] `AudioEngine::rtCallback`: per-track loop restructured so Instrument
      tracks always drain their live-note queue (audition works even while
      stopped) and, during Playing/Recording, trigger notes from
      `midiClips` whose start/end falls in the current block (block-level
      timing granularity — not sample-accurate, acceptable for v1) —
      otherwise reuses the existing scratch/effects/gain pipeline unchanged.
- [x] `command/EditCommands.h::TrackMidiCommand` mirrors `TrackClipsCommand`.
- [x] `ui/PianoKeyboardWidget` + `ui/InstrumentPanel`: clickable on-screen
      keys, dock panel, active only when the selected track is an
      Instrument track. Note capture while armed+recording is
      GUI-thread-only bookkeeping (start/stop timestamps from
      `transport().positionSamples()` at click time), finalized into
      `midiClips` via `TrackMidiCommand` on Stop — separate from the
      live-audition RT queue, and deliberately not integrated into the
      existing mic/system-audio arm-routing logic
      (`RecordRouting.h::splitTracksBySource` only ever sees Audio-kind
      tracks, filtered in `MainWindow::onRecordClicked`).
- [x] `ClipLaneWidget`: Instrument tracks paint notes as small rectangles
      (`ui/MidiNoteDisplayMath.h::pitchToY`, pure, tested) instead of a
      waveform — view-only, no drag/edit (that's the separate piano-roll
      editor roadmap item).
- [x] "Add Instrument Track" action alongside "Add Track".
- Explicitly out of scope for v1: hardware MIDI input, sampler, note
  editing/dragging on the timeline, undo for synth param tweaks (a v1
  simplification — effect params ARE undoable elsewhere, this isn't parity).

Verification plan (approved):
- [x] Automated: pure-math tests for oscillator/ADSR/filter (`SynthMath`,
      18 cases) and note-Y-position (`MidiNoteDisplayMath`, 5 cases). Full
      suite (18/18 test binaries) passes.
- [x] Smoke-tested live in the running app: added an Instrument track,
      selected it (Instrument panel correctly showed its name/params vs.
      the "No instrument track selected" placeholder beforehand), clicked a
      piano key — key highlighted, **output level meter visibly moved**
      (confirms the full RT path: click → NoteEventQueue → SynthEngine →
      oscillator/ADSR/filter → mix → output), released — envelope tailed
      off. No crash.
- [ ] Full manual (needs actual listening + a recording pass, not done from
      this session): confirm the tone is audible/musical; arm an Instrument
      track, record a short phrase, confirm note rectangles appear and play
      back correctly positioned; confirm undo/redo of a recorded phrase.

## Completed: View menu with dock toggles

Small addition after the virtual instruments feature: a "View" menu was
added to the menu bar with checkable entries (via each `QDockWidget`'s
built-in `toggleViewAction()`, so they stay in sync with manual dock
closes too) for Media Library, Effects Rack, and Instrument — so the synth
panel (and the other docks) can be shown/hidden on demand instead of always
taking up screen space. Verified live: menu shows all three checkable
entries correctly.

## In progress: MIDI piano-roll editor

Goal (feature #6): editable piano-roll for MIDI notes recorded via the v1
synth's on-screen keyboard. Currently `midiClips`/`MidiNote` are view-only
(painted as small rectangles in `ClipLaneWidget`).

Scope (approved): move, resize (length), delete, and draw new notes; a
dockable panel (same pattern as `InstrumentPanel`/`EffectsRackPanel`)
following the selected Instrument track; velocity included via a velocity
lane; grid snapping based on a new project-level BPM field (no tempo
concept existed before this).

Design (approved):
- [x] `Session`/`SessionIO` gains a `bpm` field (default 120, persisted).
- [x] `ui/PianoRollEditMath.h` (pure, tested first): snap-to-grid math
      (samples-per-beat from BPM ÷ snap denominator), pitch↔Y conversion
      (extends `MidiNoteDisplayMath`), note hit-testing, drag-to-rect math
      for move/resize/draw, velocity clamp/drag math.
- [x] `PianoRollPanel` (new dock widget): shows notes for the selected
      Instrument track, snap-size dropdown (Off/1/4/1/8/1/16/1/32), main
      pitch/time grid + velocity lane strip below it.
- [x] Grid widget (`PianoRollGridWidget`) reuses the `ClipLaneWidget` drag
      pattern: `DragMode {None, Move, Resize, Velocity}` (draw is immediate
      on empty-cell click, no drag needed), snapshot `midiClips` on
      press/click, one `TrackMidiCommand(before, after)` pushed on release
      (existing command type, no new one needed). Delete via Del/Backspace
      on the selected note, same snapshot+command pattern.
- [x] `Track` gains `replaceMidiNote`/`addMidiNote`/`removeMidiNote`
      (same atomic copy-on-write pattern as the clip-list equivalents) so
      the grid widget can mutate live during drags and on click-to-add/delete.

Verification plan (approved):
- [x] Automated: `tests/test_PianoRollEditMath.cpp` written before the math
      implementation (22 cases: snap math at various BPM/denominators,
      pitch/Y round-trip, hit-testing, drag math, velocity clamp). Full
      suite (19/19 binaries) passes.
- [x] Smoke-tested: app builds and launches cleanly (ran for full timeout
      duration under a real X display, no crash/error output). Piano Roll
      dock added to the View menu alongside the existing docks.
- [ ] Full manual (needs real interaction, not done from this session):
      draw/move/resize/delete notes, drag velocity, undo/redo, snap-size
      changes, edited notes play back correctly.

## In progress: Automation lanes (volume/pan)

Goal (feature #7): draggable breakpoint curves per track for volume and pan,
played back with per-sample ramping (no zipper noise). Scoped to
volume+pan only for v1 (approved) — effect-parameter automation (e.g. a
filter cutoff) is a possible future extension, not built here.

Design (approved) — this replaced the existing `Track::gainL`/`gainR` model:
- [x] `audio/PanLawMath.h` (pure, tested first): `panToGains(volume, pan)`
      extracted from the pan dial's existing linear pan law so both manual
      mixing and automation playback share it.
- [x] `audio/AutomationMath.h` (pure, tested first): curve evaluation
      (linear interpolation between the two nearest breakpoints, clamped to
      the endpoint value outside the curve's range), point hit-test/insert
      math (same style as `PianoRollEditMath`).
- [x] `Track`: `gainL`/`gainR` atomics replaced by `volume` (0..2, default
      1.0) and `pan` (-1..1, default 0) atomics. New `AutomationPoint
      {sample, value}`, `AutomationTarget {Volume, Pan}`,
      `AutomationLane {target, points}` (`model/AutomationLane.h`); `Track`
      gains `automationLanes` with the same atomic copy-on-write pattern as
      `clips`/`midiClips`. **This removed the existing independent
      Gain-L/Gain-R sliders** (approved tradeoff).
- [x] `command/EditCommands.h`: new `TrackAutomationCommand` (mirrors
      `TrackMidiCommand`); `TrackState`/`TrackStateCommand` migrated from
      gainL/gainR to volume/pan.
- [x] `AudioEngine::rtCallback`: per block, if a track has a Volume/Pan
      automation lane, evaluates it at the block's start and end sample,
      converts to gainL/gainR via `panToGains`, and linearly ramps
      per-sample across the block (closes the "no smoothing exists" gap
      found in research — avoids clicks on fast automation moves). Falls
      back to the static volume/pan atomics when a target has no lane, so
      manual mixing is unaffected.
- [x] `ui/AutomationLaneWidget` (new, mirrors `TakeLaneWidget`): line-graph
      curve with draggable breakpoints, a Volume/Pan target dropdown,
      click-empty-space-to-add-point, Delete/Backspace to remove selected
      point. `TrackRowWidget` gained an "Auto" toggle button (next to
      Takes/FX) to show/hide it.
- [x] `TrackWidgets.cpp`: Gain-L/Gain-R sliders replaced by a single Volume
      slider; pan dial now writes `pan` directly instead of deriving
      gainL/gainR itself.
- [x] `SessionIO`: serializes `volume`/`pan`/`automationLanes` per track
      (replaces `gainL`/`gainR` keys — no migration shim for old save
      files, per standing no-speculative-compat preference).

Verification plan (approved):
- [x] Automated: `tests/test_PanLawMath.cpp` (7 cases) and
      `tests/test_AutomationMath.cpp` (10 cases) written before their
      implementations. Existing `test_EditCommands.cpp` updated from
      gainL to volume. Full suite (21/21 binaries) passes.
- [x] Smoke-tested: app builds and launches cleanly (ran for full timeout
      under a real X display, no crash/error output). Track header shows a
      Volume slider + pan dial (no more L/R sliders), "Auto" toggle added
      next to Takes/FX.
- [ ] Full manual (needs real interaction, not done from this session):
      record/play, automate a volume fade and a pan sweep, confirm
      audibly click-free; undo/redo of automation edits; save/reload
      preserves curves.

## In progress: Bus routing / sends (aux tracks, submixes)

Goal (feature #8): aux sends from tracks to bus tracks, plus a master bus.
Scoped down (approved): aux sends only for v1 (no full submix/group routing
where a track's *main* output moves to a bus — tracks always output
directly to master, sends are an additional post-fader tap). One send per
track (single dropdown + level), unlimited bus tracks, no bus-to-bus
sends (no cycles possible).

Design (approved):
- [x] `Track::TrackKind` gains `Bus`. Bus tracks have no clips/MIDI, only
      volume/pan/effects, fed by other tracks' sends.
- [x] `Track` gains `sendBusId()`/`setSendBusId()` (backed by
      `atomic<shared_ptr<const QUuid>>`, not `atomic<QUuid>` — QUuid is
      16 bytes and wasn't lock-free with this platform's libstdc++/
      libatomic, so it reuses the same atomic-shared_ptr pattern already
      used lock-free elsewhere in `Track`) and `sendLevel` (atomic float
      0..1, default 0/none), post-fader tap (after the sending track's
      own volume/pan/mute).
- [x] New `MasterBus` (not a `Track`, `model/MasterBus.h`): `volume`
      atomic + `EffectChain`, owned by `Session`, always present, not a
      track in the track list.
- [x] `AudioEngine::rtCallback` reordered: each Audio/Instrument track's
      post-fader buffer accumulates into (a) the master accumulation
      buffer directly and (b) its send-bus's aux buffer (scaled by
      `sendLevel`), if set. After all tracks process, each Bus track runs
      its own effects+volume/pan over its aux buffer and mixes into the
      master buffer. Master bus effects+volume apply last, before output.
- [x] `audio/BusMixMath.h` (pure, tested first): send-level clamp/scale,
      master-volume apply — mirrors `PanLawMath`/`AutomationMath`.
- [x] `TrackState`/`TrackStateCommand` (existing generic track-field
      undo command) extended with `sendBusId`/`sendLevel` rather than a
      separate `TrackSendCommand` — same snapshot/restore shape as the
      volume/pan/mute/solo/arm/input-source fields it already covers.
- [x] `SessionIO`: Track JSON gains `"kind"` (also fixed serializing this
      for Instrument tracks, previously never persisted — pre-existing
      gap, not scope creep since Bus needed it too), `"sendBusId"`,
      `"sendLevel"`; Session JSON gains `"masterVolume"` + `"masterEffects"`
      (reuses existing effect serialization).
- [x] UI: "Add Bus Track" menu action; each Audio/Instrument track row gets
      a Send-bus dropdown + send-level slider next to Volume (populated by
      `TimelineView::refreshSendBusOptions()`, called on every track add/
      remove); a Master volume slider in the toolbar, synced on session
      load/close.
- [x] Master-effects UI (follow-up, closes the v1 descope above):
      `EffectsRackPanel` generalized to show either a `Track`'s chain or
      the session's `MasterBus`'s chain (exactly one host active at a
      time) via small dispatch helpers (`currentChain()`,
      `addEffectToHost()`, `removeEffectFromHost()`, `moveEffectInHost()`,
      `pushChainCommand()`) instead of duplicating the whole per-effect
      DSP control UI (EQ/Compressor/Delay/Reverb sliders stay shared).
      New `command/EditCommands.h::SetEffectChainCommand` (function-based
      restore, mirrors `SetEffectParamCommand`'s style) covers undo/redo
      for the master-bus case since `EffectChainCommand` is hardcoded to
      `Track`; `MasterBus` gained a `moveEffect()` mirroring `Track`'s, for
      the Up/Down reorder buttons. A "Master FX" toolbar button (next to
      the master volume slider) calls `setMasterBus()` and raises the
      effects dock, same pattern as a track row's own FX button.

Verification plan (approved):
- [x] Automated (written first): `tests/test_BusMixMath.cpp` (10 cases),
      `test_EditCommands.cpp` addition for send undo/redo,
      `test_SessionIO.cpp` round-trip for bus/send/master fields,
      `test_RecordRouting.cpp` addition (`filterRecordableTracks`) to
      reject Bus tracks from the record-arm path,
      `test_EffectCommands.cpp` addition for `SetEffectChainCommand`
      against a plain `MasterBus`. Full suite (22/22 binaries) passes.
- [x] Smoke-tested: app builds and launches cleanly (ran the full timeout
      under a real X display, no crash/error output) both after the
      routing/master-bus engine change and again after the master-effects
      UI follow-up.
- [ ] Full manual (needs real interaction, not done from this session):
      add a Bus track, route a track's send to it with a reverb effect on
      the bus, confirm audible wet signal without muting the dry track;
      add an effect to the master bus via the new "Master FX" button and
      confirm it's audible on the overall mix; confirm master volume
      affects overall output; confirm undo/redo of send changes, Add Bus
      Track, and master-effect add/remove/reorder; confirm save/reload
      preserves routing/master volume/master effects in a real project
      file (not just the unit test's in-memory round-trip).

## Completed: Loop/sample library browser

Goal (feature #9): browse a user-chosen folder of audio files (a sample
pack collection), audition them by clicking, and drag them onto a track —
independent of the existing session-scoped `MediaLibraryPanel` (which only
lists buffers already in the session or dropped in from outside).

Design (approved):
- [x] Prerequisite fix: `ClipLaneWidget::dragEnterEvent`/`dropEvent`
      currently only accept `MediaLibraryPanel::kMimeType` drops — a plain
      file dragged from a file manager onto a track silently does nothing
      today. Extended to also accept a standard file-URL drop, emitting a
      new `externalFileDropped(QString path, int64_t sample)` signal,
      wired through `TimelineView` to
      `MainWindow::onExternalFileDroppedOnTrack` (mirrors the existing
      `onMediaDroppedOnTrack` undo pattern). Fixes that gap and is also
      exactly what the new browser needs — its drag source just sets
      standard `QUrl` mime data and reuses this same path, no new MIME
      type or plumbing of its own.
- [x] New `LoopBrowserPanel` dock (mirrors `MediaLibraryPanel`'s
      `QListWidget` style): "Choose Folder" button (path persisted via
      `QSettings`, like recent sessions), recursive scan
      (`QDirIterator`) for `.wav/.aiff/.aif/.flac/.ogg`, a filename
      filter box, drag source sets `QUrl` mime data. Not session-
      serialized (it's a filesystem browser, not session state) — only
      the last-used folder path persists.
- [x] Click-to-audition: clicking a list item loads the file
      (`AudioFileIO::loadFile`) and calls a new
      `AudioEngine::previewSample(buffer)`; clicking again / a Stop
      button calls `stopPreview()`.
- [x] `AudioEngine` preview playback: new
      `atomic<shared_ptr<const Clip>> m_previewClip` +
      `atomic<int64_t> m_previewPosition`. `previewSample()` wraps the
      buffer in a throwaway `Clip` (`sessionStartSample = 0`) and arms
      it. Mixed into the master accumulation buffer in `rtCallback` via
      the *existing* `mixClipInto` helper (no new mixing math), clearing
      itself at end-of-sample via a small pure helper,
      `audio/PreviewPlaybackMath.h::isPreviewFinished()` (tested first).

Verification plan (approved):
- [x] Automated (written first): `tests/test_PreviewPlaybackMath.cpp` (5
      cases) for the finished/advance edge cases. Full suite (23/23
      binaries) passes.
- [x] Smoke-tested: app builds and launches cleanly (ran the full timeout
      under a real X display, no crash/error output) with the new dock
      wired in.
- [ ] Full manual (needs real interaction, not done from this session):
      audition is actually audible and stops correctly (including
      stopping when a second file is clicked mid-preview); dragged clip
      plays back correctly positioned; folder path persists across an
      app restart; dragging a plain file from an OS file manager straight
      onto a track (the prerequisite fix) also works, not just from the
      new browser panel.

## Completed: Export/bounce with format + stems options

Goal (feature #10). Research found the existing export path
(`MainWindow::onExportClicked` → `renderTrackToBuffer`/
`renderSessionToBuffer`) predates effects/automation/bus-routing:
`renderSessionToBuffer` only applies static volume/pan, skipping effects,
automation curves, bus sends, and the master bus entirely — a bounce
sounds nothing like real playback. Approved (asked first): fix this as
part of the feature rather than layering format/stems on top of the
broken mixdown.

Scope decisions (approved):
- Stems: full mixdown + optional per-track stems (Audio/Instrument tracks
  only, not Bus tracks) in v1. No per-bus stems yet.
- Formats: WAV only, 32-bit float or 16-bit PCM (both via libsndfile
  flags already available, no new dependency). No FLAC/OGG in v1.

Pragmatic defaults (not asked, flagging here for review): stems render
every non-Bus track unconditionally, ignoring current mute/solo state
(a "give me all the raw components" export) — only the *full mixdown*
respects mute/solo, matching real playback. Export length = last
clip/MIDI-note end across the session; a long effect tail (reverb/delay)
past that point gets cut off (same limitation the old code had).

Design (approved):
- [x] `Synth.h::SynthEngine` gains a `reset()` (all voices inactive) so
      export starts from deterministic silence regardless of any
      in-progress live audition state.
- [x] New `audio/SessionMixer.h/.cpp` (not header-only pure math like
      `PanLawMath`/`BusMixMath` — this is a full subsystem, tested at the
      integration level instead): extracts the exact per-block mixing
      logic that used to be inline in `AudioEngine::rtCallback` (track
      clips/synth → effects → automation-ramped volume/pan → bus aux
      accumulation → bus effects/volume → master effects/volume) into
      `mixSessionBlock(Session&, sampleRate, channels, pos, nFrames,
      playbackActive, out, SessionMixScratch&)`, callable with no RtAudio
      stream running at all. A `renderTrackBlock()` helper factors out
      just the per-track processing step so a per-track stem render is a
      one-line wrapper around it (skips bus/master mixing entirely, per
      the stems scope decision above) instead of a second, duplicated
      mixing path.
- [x] `AudioEngine::rtCallback` refactored to call `mixSessionBlock()`
      instead of its own inline copy of this logic — same behavior,
      no duplication between playback and export (its 3 scratch-buffer
      members collapsed into one `SessionMixScratch`). Preview audition
      (loop-browser click-to-hear) stays AudioEngine-only, mixed onto
      `out` *after* `mixSessionBlock()` writes it — scaled by the master
      volume (for consistent loudness) but not master effects, so
      auditioning isn't colored by e.g. a master reverb.
- [x] New `audio/OfflineRenderer.h/.cpp`: `sessionContentLengthSamples()`
      (latest clip/MIDI-note end across the session) plus
      `renderSessionMixdown()`/`renderTrackStem()`, which drive
      `mixSessionBlock()`/`renderTrackBlock()` block-by-block (1024-frame
      internal blocks) from sample 0 to that length into an in-memory
      `AudioBuffer`, no audio device needed. `MainWindow` calls this on
      the GUI thread after stopping any live playback first
      (`onStopClicked()`) and resetting every track's `SynthEngine`, so
      there's no concurrent access to track state from a live RT
      callback.
- [x] `AudioFileIO::writeFile` gains a format parameter
      (`ExportFormat::Wav32Float` / `Wav16Pcm`, mapped to
      `SF_FORMAT_WAV | SF_FORMAT_FLOAT` / `SF_FORMAT_WAV | SF_FORMAT_PCM_16`)
      — the actual `sf_writef_float()` write call is unchanged; libsndfile
      handles the float→int16 conversion internally for the PCM_16 case.
- [x] New `ExportDialog` (format radio buttons + "Also export stems"
      checkbox), shown before the existing save-file dialog.
      `onExportClicked` now bounces the full session (the old "export
      active track only" behavior — and its `renderTrackToBuffer` helper
      — is gone; a single track's stem is just one of the per-track stem
      files now). Stems are written alongside the chosen mixdown path as
      `<name> - <trackname>.wav`. Note: `renderTrackToBuffer`'s sibling,
      `renderSessionToBuffer` (the static volume/pan-only render), is
      intentionally left as-is — it's also used for the master waveform
      overview widget, a cheap visual-only render that doesn't need full
      mixing fidelity.

Verification plan (approved):
- [x] Automated (written first): `tests/test_SessionMixer.cpp` (5 cases,
      real `Session`/`Track`/`Clip` objects, no mocking) covering: a dry
      track at unity volume/pan passes its clip through unchanged; a
      muted track is silent; an aux send scales into its bus's output on
      top of the direct contribution; the master volume scales the final
      block; `renderTrackBlock()` for a stem excludes send/master
      processing. `tests/test_OfflineRenderer.cpp` (4 cases) covers
      `sessionContentLengthSamples()` and the block-splitting loop across
      an internal block boundary for both the mixdown and a stem. Full
      suite (25/25 binaries) passes, including all pre-existing
      bus-routing/automation/effects tests unchanged after the
      `rtCallback` refactor.
- [x] Smoke-tested: app builds and launches cleanly (ran the full
      timeout under a real X display, no crash/error output) with the
      new Export dialog wired in.
- [ ] Full manual (needs real interaction, not done from this session):
      export a session with an effect, an automation curve, and a bus
      send, and confirm by ear that the exported WAV matches what
      played back live; confirm 16-bit vs 32-bit float files both open
      correctly in another tool; confirm stems open individually and
      sum (roughly) back to the full mix.

## Completed: Mixer view

Goal (feature #12): a bottom-docked strip of per-track mixer channels
(Pro Tools/Ableton-style) as an alternate, side-by-side view of the same
controls already on each track row header — no new audio logic, purely a
second UI surface over already-tested state (`TrackState`/
`TrackStateCommand`, `panToGains`, effect counts). Approved: strips include
aux-send controls too, matching the track row header exactly (not a
subset).

Design (approved):
- [x] New `ui/MixerStripWidget` (one per Audio/Instrument/Bus track):
      vertical `QSlider` (volume), `QDial` (pan), Mute/Solo checkboxes, an
      FX button (reused `formatEffectsButtonLabel`, opens/raises the
      effects rack like the track row's own FX button), and — for non-Bus
      tracks — a send-bus dropdown + level slider. Wired the same way
      `TrackWidgets.cpp` wires its header controls: mutate the atomic
      live during a drag, push one `TrackStateCommand` (before/after
      snapshot) on release/change. Duplicated wiring rather than a shared
      base class with `TrackRowWidget` — consistent with this codebase's
      existing per-widget-owns-its-wiring style, and the two controls
      sets are laid out too differently (horizontal compact row vs.
      vertical strip) to share much beyond copy-pasted signal plumbing.
- [x] New `ui/MixerPanel` (dockable): a horizontal, scrollable row of
      `MixerStripWidget`s (one per session track, in track order) plus a
      fixed Master strip (master volume fader + a "Master FX" button
      reusing `EffectsRackPanel::setMasterBus`). Mirrors `TimelineView`'s
      `addTrack()`/`removeTrack()`/`clear()`/`refreshSendBusOptions()`
      interface, driven from the exact same `MainWindow` call sites so
      the two per-track widget collections (timeline rows, mixer strips)
      never drift out of sync.
- [x] `MainWindow`: new "Mixer" dock, `Qt::BottomDockWidgetArea`, added to
      the existing View menu's toggle list (the View menu already exists
      from the virtual-instruments feature — no new menu needed).
      `EffectsRackPanel::effectCountChanged` also refreshes the matching
      mixer strip's FX label, not just the track row's. The toolbar's
      "Master FX" button and the mixer's own Master FX button both now
      call one shared `MainWindow::onMasterEffectsPanelRequested()`
      instead of duplicating that 3-line lambda.
- Known v1 limitation (not asked, flagging for review): clicking a mixer
  strip's FX button sets that track Active (via the same
  `onTrackSelected` the timeline row's radio button uses) but doesn't
  visually check that track's "Active" radio button back on the timeline
  row — the two views' notion of "selected" only syncs one direction.
  Cosmetic only; every actual control (fader/pan/mute/solo/send/FX
  content) stays fully in sync both ways.

Verification plan (approved):
- [x] Automated: none new, as planned — this feature adds no new logic,
      only UI composition over `TrackState`/`TrackStateCommand`/
      `panToGains`, already covered by `test_EditCommands.cpp`/
      `test_PanLawMath.cpp`. Full suite (25/25 binaries) still passes
      unchanged.
- [x] Smoke-tested: app builds and launches cleanly (ran the full
      timeout under a real X display, no crash/error output) with the
      Mixer dock, master strip, and initial track's strip all
      constructed at startup.
- [ ] Full manual (needs real interaction, not done from this session):
      dragging a mixer strip's fader/pan/send updates the matching track
      row header (and vice versa); mute/solo/FX-click behave identically
      from either view; undo/redo of a mixer-strip edit works; adding/
      removing a track keeps both views in sync.

## Completed: Removed duplicate mixer controls from the track row header

Follow-up after Mixer view: Mute, Solo, Pan, Volume, and Send (dropdown +
level) were now on both the track row header and the new mixer strip —
two controls fighting over the same `TrackState`. Approved: removed from
the track row header, kept only on the mixer strip. Arm and the
Mic/System Audio input-source dropdown stay on the row (not on the mixer
at all); FX/Takes/Auto also stay (Takes/Auto toggle lanes in the timeline
itself, not just mirror a value; FX was kept per the approved choice).

- [x] `TrackRowWidget`: removed `m_muteBox`/`m_soloBox`/`m_panDial`/
      `m_volumeSlider`/`m_sendBusCombo`/`m_sendLevelSlider` and their
      wiring; Arm + source dropdown reflowed into the row those used to
      occupy. `refreshSendBusOptions()` removed from `TrackRowWidget`
      (no send control left to refresh) and from `TimelineView` (its
      only caller besides `MixerPanel`, which has its own independent
      implementation).
- Automated: none new (pure removal of duplicated UI, no logic change);
  full suite (25/25 binaries) still passes. Smoke-tested: app builds and
  launches cleanly.

## Completed: Moved Arm + input-source to the mixer strip too

Follow-up: real DAW mixer strips (Pro Tools/Ableton/Logic) put record-arm
and input selection on the channel strip, not just the track header —
asked and confirmed. Moved `Arm` checkbox and the Mic/System Audio
dropdown from `TrackRowWidget` to `MixerStripWidget` (abbreviated
"System Audio" to "Sys" there to fit the 70px-wide strip). The track row
header is now just Name/Active/FX/Takes/Auto — everything else lives on
the mixer strip.

- [x] `TrackRowWidget`: removed `m_armBox`/`m_sourceCombo` and their
      wiring (now unconditional, ungated on track kind, same as before
      the move — Bus tracks still get an Arm/source control that's
      inert in practice, matching pre-existing behavior rather than
      introducing a new gate).
- [x] `MixerStripWidget`: added the same two controls, same
      `TrackStateCommand` wiring pattern as everything else on the
      strip.
- Automated: none new (pure UI relocation, no logic change); full suite
  (25/25 binaries) still passes. Smoke-tested: app builds and launches
  cleanly.

## Completed: Removed the FX button from the track row too

Follow-up: after Arm/input-source moved to the mixer, the track row's FX
button became the last remaining duplicate (mixer strip already has its
own FX button doing the same thing — select track + raise the effects
dock). Removed from `TrackRowWidget`; the row is now just
Name/Active/Takes/Auto.

- [x] `TrackRowWidget`: removed `m_effectsButton`, `refreshEffectsButton()`,
      and the `effectsPanelRequested` signal (nothing emits it anymore).
- [x] `TimelineView`: removed the now-dead `effectsPanelRequested`
      forwarding signal and `refreshTrackEffectsButton()` (its only
      caller, `MainWindow`, now refreshes just the mixer strip's FX
      label via `MixerPanel::refreshTrackEffectsButton()`).
- Automated: none new (pure removal, no logic change); full suite
  (25/25 binaries) still passes. Smoke-tested: app builds and launches
  cleanly.

## Completed: Restacked the (now much smaller) track row header

Follow-up: with the header down to just Name/Active/Takes/Auto, the old
360px single-row layout left a lot of empty horizontal space. Restacked
into a 2x2 grid (Name/Active on row 1, Takes/Auto on row 2) and shrank
`kHeaderWidth` from 360 to 160, giving the waveform lane more width.

- [x] `TrackWidgets.cpp`: regridded the four remaining controls into two
      rows of two columns instead of one row of four/six columns.
- Automated: none new (pure layout change, no logic change); full suite
  (25/25 binaries) still passes. Smoke-tested: app builds and launches
  cleanly.

- Each feature gets a verification plan proposed and approved before
  implementation starts (per standing workflow rule).
- Test-first: write tests before implementation for each feature.
