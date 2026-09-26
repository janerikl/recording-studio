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

## Completed: SoundFont instruments (piano, drums, full GM set) replacing the waveform synth

Goal: replace the v1 oscillator-based synth (4 waveforms + ADSR + filter)
with real SoundFont sample playback via FluidSynth, so Instrument tracks
can be a piano, a drum kit, or any other General MIDI instrument instead
of a generic tone. Approved: full replacement (not additive), FluidSynth
as the new dependency, a bundled General MIDI SoundFont, on-screen
keyboard/drum-pads only (no hardware MIDI input — same v1 scope as
before).

Design (approved):
- [x] New system dependency: `libfluidsynth-dev` (build) / `libfluidsynth3`
      (runtime), linked via `pkg_check_modules(FLUIDSYNTH ... fluidsynth)`
      in both `CMakeLists.txt` and `tests/CMakeLists.txt`.
- [x] Bundled `assets/soundfonts/TimGM6mb.sf2` (TimGM6mb, ~5.7MB, free/
      open-license General MIDI soundfont) — chosen over the larger
      FluidR3_GM (~140MB) specifically to keep it small enough to commit
      to the repo.
- [x] `audio/Synth.h` rewritten: `SynthParams` now holds
      `instrumentProgram` (GM program 0-127) and `isDrumKit` (bool)
      instead of waveform/ADSR/filter atomics. `SynthEngine` wraps a
      `fluid_synth_t`/`fluid_settings_t` pair (reverb/chorus off, fixed
      48kHz, `synth.threadsafe-api` off since it's only ever driven from
      the RT thread, same trust model as before) instead of the 8-voice
      oscillator pool — FluidSynth handles its own internal polyphony.
      Melodic notes go to MIDI channel 0 (program-changed on the fly when
      `instrumentProgram` changes); drum notes go to channel 9/bank 128
      (GM percussion). `render()` calls `fluid_synth_write_float` into a
      scratch stereo buffer and adds it into the track's output buffer
      (additive, matching the previous engine's contract). `noteOn`/
      `noteOff` gained an `isDrumKit` parameter so `SessionMixer` can
      route to the right channel.
- [x] `reset()` (used before an offline export render) switched from
      `fluid_synth_system_reset` to `fluid_synth_all_sounds_off` per
      channel — discovered during testing that `system_reset` only
      triggers a normal note-off with an audible release tail (not
      deterministic-enough silence for "export starts from silence"),
      while `all_sounds_off` mutes immediately.
- [x] Deleted `audio/SynthMath.h` (oscillator/ADSR/filter math, no longer
      used by anything) and its test.
- [x] New `audio/GMInstruments.h` (128 GM program names, pure data) and
      `audio/GMDrumMap.h` (10-pad GM percussion map: Kick/Snare/Hi-Hats/
      Crash/Ride/Toms/Clap), both tested first.
- [x] New `ui/DrumPadWidget` (grid of pad buttons, mirrors
      `PianoKeyboardWidget`'s noteOn/noteOff signal shape).
      `InstrumentPanel` reworked: waveform/ADSR/filter sliders replaced by
      an "Instrument" combo (128 GM names) + a "Drum Kit" checkbox; a
      `QStackedWidget` swaps between the piano keyboard and the drum pads
      depending on the checkbox.
- Known v1 limitation (not asked, flagging for review): instrument
  selection (`instrumentProgram`/`isDrumKit`) is not persisted in
  `SessionIO`, matching the pre-existing gap where the old waveform/ADSR
  params also weren't saved — every Instrument track reopens as Acoustic
  Grand Piano. Worth fixing in a follow-up if instrument choice turns out
  to matter across sessions.

Verification plan (approved):
- [x] Automated (written first): `tests/test_GMInstruments.cpp` (4
      cases), `tests/test_GMDrumMap.cpp` (4 cases). `tests/test_Synth.cpp`
      (6 cases, integration-level against the real bundled SoundFont —
      same style as `SessionMixer`'s tests): silence before any note,
      audio on note-on, silence again after note-off + release tail,
      immediate silence after `reset()`, drum-kit note also produces
      audio. Every test target that transitively includes `model/Track.h`
      (it directly owns a `SynthEngine`) needed `PkgConfig::FLUIDSYNTH`
      + `RSD_SOURCE_DIR` added so the real soundfont loads during tests.
      Full suite (31/31 binaries) passes.
- [x] Smoke-tested: app builds and launches cleanly (ran under a real X
      display briefly, no crash/error output beyond the pre-existing ALSA
      probe warning).
- [ ] Full manual (needs real interaction/listening, not done from this
      session): select Piano on an Instrument track, play the on-screen
      keyboard, confirm it's audibly a piano (not a synth tone); check
      "Drum Kit", confirm the keyboard swaps for drum pads and each pad
      sounds like its label (kick/snare/hi-hat/etc.); switch the
      instrument combo to a few other GM programs (e.g. a guitar, a
      string patch) and confirm the sound changes; record a short phrase
      through the piano-roll/instrument-record path and confirm playback
      still sounds correct; confirm undo/redo and existing effects/
      automation on an Instrument track still work.

## Completed: Computer-keyboard shortcuts for the on-screen piano

Goal: play the on-screen piano keyboard using the computer keyboard
(Nordic layout), not just the mouse — asdfghjklöä for white keys,
qwertyuiopå for black keys. Clarified with the user (asked first): use
the standard staggered "typing keyboard" layout (white keys on the home
row, black keys on the row above aligned over the gaps between white
keys), not a naive 1:1 sequential mapping — matches conventions like
GarageBand's Musical Typing / VMPK. There's no black key above the E-F or
B-C gaps, so Q, R, I, and Å are intentionally unmapped.

Design (approved):
- [x] New `ui/PianoKeyMap.h::pitchForComputerKey(int qtKey)` (pure,
      tested first): A-Ä → C3-F4 (11 white notes, MIDI 48-65), W E T Y U
      O P → the 7 black notes in that range (MIDI 49,51,54,56,58,61,63),
      everything else → `nullopt`.
- [x] `PianoKeyboardWidget`: `Qt::StrongFocus` (needs a click on the
      widget first to receive key events — same as any keyboard-driven
      widget); `keyPressEvent`/`keyReleaseEvent` look up
      `pitchForComputerKey`, ignore `isAutoRepeat()` events (so holding a
      key doesn't retrigger), and track currently-down keys in a
      `std::set<int>` — polyphonic, unlike the mouse's single-note glide,
      so chords can be played by holding multiple keys. Held keyboard
      notes highlight on the keyboard the same as a mouse-held note.

Verification plan (approved):
- [x] Automated (written first): `tests/test_PianoKeyMap.cpp` (5 cases:
      full white-row mapping, full black-row mapping, the 4 unmapped gap
      keys, an unrelated key). Full suite (32/32 binaries) passes.
- [x] Smoke-tested: app builds and launches cleanly.
- [ ] Full manual (needs real interaction, not done from this session):
      click the piano keyboard to focus it, play asdfghjklöä and
      qwertyuiopå, confirm the right notes sound and highlight; hold two
      keys at once and confirm both notes sound (polyphony); confirm
      holding a key down doesn't retrigger/stutter the note.

Follow-up: labeled each key with its shortcut letter directly on the
piano widget (so the mapping doesn't have to be memorized). New
`PianoKeyMap.h::computerKeyLabelForPitch()` (inverse of
`pitchForComputerKey`, tested first — 2 more cases). `paintEvent` draws
the label near the bottom of each white/black key that has one; keys
with no shortcut (outside C3-F4, or a black-key gap) are left unlabeled.
Full suite (32/32 binaries, 8/8 in `PianoKeyMapTests`) passes.

## Completed: Learn-to-play aids (note names, practice mode, metronome)

Goal: help a user learning piano — asked what to add, proposed note
names on keys / a follow-along practice mode / a metronome, user said do
all three. Clarified scope first (asked): practice mode ships with
built-in scales/songs (not user-authored), highlight-and-wait mechanics
(not Synthesia-style falling notes), and the metronome is a general
transport feature (not scoped to practice mode only).

Design (approved):
- [x] New `audio/NoteNaming.h::midiNoteName(int)` (pure, tested first) —
      scientific pitch notation (MIDI 60 = C4). `PianoKeyboardWidget`
      draws the note name on every key (not just the ones with a
      computer-keyboard shortcut), above the shortcut letter.
- [x] `PianoKeyboardWidget` gained `setExpectedPitch()`: highlights a key
      green (distinct from the blue held-note highlight) — used by the
      new practice mode to show the next expected note.
- [x] New `model/PracticeExercise.h` (pure data, tested first): built-in
      exercises (C Major Scale, Twinkle Twinkle Little Star's opening
      phrase). New `model/PracticeMath.h::practiceAdvance()`/
      `practiceComplete()` (pure, tested first): highlight-and-wait
      logic — only advances past the expected note if the pitch played
      matches it.
- [x] New `ui/PracticePanel` (exercise combo + Start button + progress
      label), embedded in `InstrumentPanel` above the keyboard. Wired to
      the keyboard's `noteOn` (checks progress) and back to the
      keyboard's `setExpectedPitch` (shows what's next). Hidden when the
      track is in Drum Kit mode — exercises are melodic (scales/songs),
      not meaningful for a drum kit.
- [x] New `audio/MetronomeMath.h::beatDurationSamples()`/
      `firstClickOffsetInBlock()` (pure, tested first — block-level
      granularity, same simplification already used for MIDI-note
      timing in `SessionMixer`) and `audio/Metronome.h` (RT-safe click
      generator: a short decaying 1kHz blip at each beat, envelope state
      carried across render calls like `SynthVoice` so a click begun
      near a block boundary isn't cut off).
- [x] `Session` gained `bpm`-adjacent `metronomeEnabled` (bool,
      persisted in `SessionIO` alongside `bpm`, which was previously
      write-only from the app's own perspective — there was no UI to
      *set* it before this, only the piano-roll grid read it). New BPM
      spinbox + "Metronome" checkbox on the main toolbar, wired directly
      to `Session::bpm`/`metronomeEnabled` (same pattern as the existing
      punch-in/out spinboxes) and synced on session load/new.
- [x] `AudioEngine`: new `Metronome m_metronome` member, rendered
      straight onto the final output in `rtCallback` (same spot as the
      loop-browser preview audition) when `playbackActive &&
      session->metronomeEnabled` — deliberately *not* part of
      `mixSessionBlock()`/`SessionMixer`, so `OfflineRenderer`/export
      never bakes the click into a bounce (matches the existing preview-
      audition precedent, which is AudioEngine-only for the same
      "shouldn't color the real mix" reason).

Verification plan (approved):
- [x] Automated (written first): `tests/test_NoteNaming.cpp` (4 cases),
      `tests/test_MetronomeMath.cpp` (5 cases), `tests/test_Metronome.cpp`
      (4 cases, integration-level RT-class test, same style as
      `test_Synth.cpp`), `tests/test_PracticeExercise.cpp` (3 cases),
      `tests/test_PracticeMath.cpp` (5 cases), plus a new
      `roundTripsBpmAndMetronome` case in `test_SessionIO.cpp`. Full
      suite (37/37 binaries) passes.
- [x] Smoke-tested: app builds and launches cleanly.
- [ ] Full manual (needs real interaction/listening, not done from this
      session): confirm every piano key shows its note name; select "C
      Major Scale" in the Practice panel, press Start, confirm the
      correct key highlights green and playing the right note (mouse,
      keyboard shortcut, or both) advances it, playing a wrong note
      doesn't, and it says "Done!" after the last note; check the
      Metronome toolbar checkbox during playback/recording and confirm
      an audible click at the BPM shown, with no click when unchecked or
      stopped; confirm changing BPM updates both the click tempo and the
      piano-roll grid snapping; confirm a session save/reload keeps the
      BPM and metronome checkbox state; confirm exporting a mix with the
      metronome checked does *not* include the click in the WAV.

## In progress: Media Library click-to-preview

Goal: `MediaLibraryPanel` should audition a sample on click, same as
`LoopBrowserPanel` already does — currently it only supports drag-to-track
and has no preview wiring.

Design (approved):
- [x] `MediaLibraryPanel` gains `previewRequested(int index)` signal, wired
      to `itemClicked`, same toggle logic as `LoopBrowserPanel`
      (`m_previewingRow`): click a different row → emit that row's index;
      click the currently-previewing row again → clear and emit `-1`.
      Emits an index (not a path) since the panel already holds buffers
      in-memory (`bufferAt(int)`) — no reason to reload from disk like the
      Loop Browser does for filesystem-only entries.
- [x] `MainWindow::onMediaPreviewRequested(int index)`: `index < 0` →
      `m_engine->stopPreview()`; else `m_engine->previewSample(m_mediaLibrary->bufferAt(index))`.
      Connected next to the existing `LoopBrowserPanel::previewRequested`
      wiring.
- Deviation from the originally-approved test target: rather than a
  `QListWidget`-driving QTest (no existing test in this project links
  `Qt6::Widgets`/constructs a `QApplication` — the established pattern is
  extracting pure math instead, e.g. `PreviewPlaybackMath`), the toggle
  logic was extracted into `ui/MediaPreviewToggleMath.h::nextPreviewIndex()`
  and tested directly; `MediaLibraryPanel`'s click handler is now a thin
  wrapper calling it.

Verification plan (approved, test target adjusted per above):
- [x] Automated (written first): `tests/test_MediaPreviewToggleMath.cpp` (3
      cases: new row starts preview, same row stops it, different row
      switches it). Full suite (38/38 binaries) passes.
- [x] Smoke-tested: app builds and launches cleanly, no errors.
- [ ] Full manual (needs real interaction/listening, not done from this
      session): add/drop a couple of entries into the Media Library, click
      one and confirm audio plays, click again to confirm it stops, click
      a different entry to confirm it switches.

## In progress: Help menu + usage guide

Goal: a Help menu (top menu bar) with a "Usage Guide" action that opens a
static HTML page documenting every feature in plain English, in logical
workflow order. No video/GIF content this pass (explicitly descoped,
approved) — placeholder comments left per section for a future pass.

Design (approved):
- [ ] New `Help` menu (`menuBar()->addMenu("&Help")`, after the existing
      File/Edit/View menus) with a "Usage Guide" `QAction`.
- [ ] `docs/usage-guide.html`: single self-contained static page (no
      external deps/build step), sections in this order: Project Basics,
      Transport/Playback, Recording, Punch/Loop Recording, Clip Editing,
      Track Management, Mixer Controls, Sends, Effects, Automation,
      MIDI/Instrument Editing, Recording Takes, Waveform Display, Media
      Management, Loop Browser, Import/Export, Zoom/Navigation, Bookmarks,
      Playback Loop, Settings, Undo/Redo, View/Panels, Status/Meters.
      Each entry: one-line plain-English description + how-to-use steps.
      `<!-- TODO: add GIF -->` placeholder comment per section.
- [ ] Help action opens the HTML file via `QDesktopServices::openUrl()`,
      resolved relative to the app's install/resource location (works from
      both build dir and installed layout).

Verification plan (approved):
- [ ] Build the app, confirm it compiles cleanly.
- [ ] Launch, click Help → Usage Guide, confirm the HTML opens in the
      default browser with correct formatting.
- [ ] Spot-check 5-6 documented entries against actual app behavior
      (e.g. trigger a bookmark shortcut, toggle loop record).
- No automated tests planned — this is UI wiring + static content with no
  new testable logic; verification is manual build + click-through only.

## In progress: Instrument Roll rename + save-to-loop-browser + track-lane fix

Goal: rename "Piano Roll" to "Instrument Roll" (UI label only), let the user
save a rendered instrument track's notes as a loop-browser entry, and fix
the instrument track's lane visualization (was sparse fixed-size 6px ticks
from `ClipLaneWidget::paintMidiNotes`, not a coherent shape).

Design (approved), corrected mid-build (asked first): "loop browser" turned
out to mean the actual folder-backed `LoopBrowserPanel` (feature #9), not
the session-scoped `MediaLibraryPanel` — confirmed with the user before
wiring it up.

- [x] Rename: "Piano Roll" dock title (`MainWindow.cpp:344`) → "Instrument
      Roll". No class/file renames (`PianoRollPanel`/`PianoRollGridWidget`
      etc. stay as-is) — confirmed no other user-facing "Piano Roll"
      strings existed.
- [x] Save to Loop Browser: `PianoRollPanel` gained a "Save to Loop
      Browser" button (enabled only when an Instrument track is selected),
      emitting `saveToLoopBrowserRequested(track)`.
      `MainWindow::onSaveToLoopBrowserRequested()` renders the track's
      notes to an `AudioBuffer` via the existing `renderTrackStem()` (same
      path per-track stem export uses — resets the track's `SynthEngine`
      first, stops live playback to avoid concurrent RT access), writes it
      as a WAV into `LoopBrowserPanel`'s currently-chosen folder, then
      calls its new `refresh()` so the new file shows up immediately.
      New pure helper `audio/OfflineRenderer.h::trackContentLengthSamples()`
      (extracted from `sessionContentLengthSamples`'s per-track loop body)
      sizes the render to just that track's own content.
      `LoopBrowserPanel` gained public `folderPath()`/`refresh()` (the
      latter just exposes the existing private `rescan()`).
- [x] Track-lane fix: `ClipLaneWidget::paintMidiNotes()` now draws each
      note as a rectangle spanning `startSample`→`startSample+lengthSamples`
      (unchanged — it already did this) with height from new
      `ui/MidiNoteDisplayMath.h::noteRowHeight()` (one pitch-range "row"
      worth of the lane, tiling adjacent semitones instead of a fixed 6px
      strip) and brightness scaled by note velocity, so it reads as a
      denser, more piano-roll-like shape instead of sparse uniform ticks.

Verification plan (approved):
- [x] Automated (test-first): `tests/test_MidiNoteDisplayMath.cpp` gained 3
      cases for `noteRowHeight()` (scales with lane/range, clamps to ≥1px,
      degenerate-range guard), written before the implementation.
      `tests/test_OfflineRenderer.cpp` gained 2 cases for
      `trackContentLengthSamples()` (per-track isolation, empty-track
      zero), also written first. Full suite (38/38 binaries) passes.
- [x] Smoke-tested: app builds and launches cleanly under a real X
      display.
- [x] Partial manual: confirmed visually (screenshot) that the dock now
      reads "Instrument Roll", and that selecting an Instrument track
      enables the "Save to Loop Browser" button (disabled with no
      selection). Did not confirm the button's actual save/refresh
      behavior or the lane's visual rectangles end-to-end — driving the
      on-screen piano keyboard via automated clicks (xdotool) proved
      unreliable in this session (clicks landed but no notes registered
      in the track's note list across several attempts) and burned
      significant time without a clear root cause (could be a click
      timing issue with the automation, not necessarily an app bug).
- [ ] Full manual still needed (by hand, not automatable from this
      session): choose a Loop Browser folder, record a short phrase on an
      Instrument track, confirm the lane shows filled note-length
      rectangles (not sparse ticks); click "Save to Loop Browser", confirm
      a new WAV appears in the Loop Browser list and sounds correct.

## In progress: Rhythm reading trainer (word-mnemonic dictation)

Goal: teach rhythm/note-duration reading using the word-mnemonic method
from a reference photo (word syllables mapped to a rhythmic notation
pattern, e.g. "Mozzarella" = four 16th notes). Approved scope: full
dictation trainer (show notation, play it audibly, user taps it back and
gets scored), extends the existing Practice panel (new "Rhythm" mode
alongside the existing pitch-exercise mode), 7 built-in word/pattern pairs
matching the photo (approximate reading, approved as "close enough"):
- Mozzarella: 4x 16th notes
- Coconut: 2x 8th + 2x 16th
- Strawberry: 2x 16th + 2x 8th
- Cucumber: 8th + 2x 16th + 8th
- Orange: 8th + 16th + 8th-rest + quarter-rest
- Lemon: 8th + 8th + quarter
- Mango: 8th + 16th + 16th + 8th

Design (approved):
- [x] `model/RhythmPattern.h` (pure data, tested first): `RhythmNote
      {beats, isRest}`, `RhythmPattern {word, notes}`,
      `builtInRhythmPatterns()` — the 7 patterns above.
- [x] `audio/RhythmMath.h` (pure, tested first): `onsetBeats()` (cumulative
      start beat of each non-rest note), `patternTotalBeats()`,
      `beatsToSeconds()`, `classifyTapOffset()` (Hit/Early/Late vs.
      tolerance), `scoreTaps()` (greedy nearest-match of tapped timestamps
      to expected onsets → per-note verdicts + miss/extra counts + accuracy
      %).
- [x] `audio/RhythmClickTrack.h/.cpp` (integration-tested, mirrors
      `Metronome`'s blip-synthesis style): renders a pattern to an
      `AudioBuffer` (click at each onset) for audible playback via the
      existing `AudioEngine::previewSample()` path — no new RT wiring
      needed, reuses the preview mechanism already used by the Loop
      Browser/Media Library.
- [x] `ui/RhythmStaffWidget` (new, paint-only like `ClipLaneWidget` —
      not unit tested): draws a simplified single-line rhythm staff
      (noteheads + beam grouping by duration, secondary beam for 16th-note
      sub-runs, rest glyphs) for the selected pattern.
- [x] `PracticePanel`: new "Mode" combo (Pitch / Rhythm). Rhythm mode
      swaps in the 7 word patterns, shows the staff widget, a "Play"
      button (renders + requests playback via a new
      `rhythmPlaybackRequested(buffer)` signal, forwarded through
      `InstrumentPanel` to `MainWindow` → `AudioEngine::previewSample()`),
      and a "Tap Back" button that arms tap capture — taps via a "Tap"
      button or Spacebar, timestamped against a `QElapsedTimer` started
      when tapping begins, auto-scored via `RhythmMath::scoreTaps()` once
      the pattern's duration elapses (`QTimer::singleShot`). Results shown
      as a per-note ✓/early/late/miss line plus overall accuracy %.
      `PracticePanel`/`InstrumentPanel` gained `setBpm()` (same pattern as
      `PianoRollPanel`), wired from both `MainWindow` BPM-sync points.

Verification plan (approved):
- [x] Automated (test-first): `tests/test_RhythmPattern.cpp` (built-in
      pattern content sanity), `tests/test_RhythmMath.cpp` (onset/total
      beat math, tap classification at tolerance boundaries, scoring:
      perfect run, early/late/miss cases, extra taps), both written before
      their implementations. `tests/test_RhythmClickTrack.cpp`
      (integration-level, same style as `test_Metronome.cpp`: silence
      before first onset, audio at each onset, correct buffer length).
      Full suite (41/41 binaries) passes.
- [x] Smoke-tested: app builds and launches cleanly under a real X
      display.
- [x] Caught via user feedback ("I don't get it, what is the user
      supposed to do here?") on the first rendered screenshot: the word
      mnemonic wasn't shown anywhere near its notation (only in the
      selector combo above), so there was no visible link between e.g.
      "Strawberry" and its rhythm. Fixed: `RhythmStaffWidget` now draws
      the word directly to the left of its staff (matching the reference
      photo's side-by-side layout); `PracticePanel` gained an intro
      sentence explaining the say-it/play-it/tap-it-back flow, and the
      status label now names the selected word. Verified by temporarily
      defaulting the Mode combo to Rhythm (reverted after), since
      automated clicks weren't reliably landing in this environment —
      screenshot confirmed "Mozzarella" now renders beside its notation
      with the new instructional text. Full suite (41/41 binaries) still
      passes after the fix.
- [x] User feedback after trying it live: "I hear some weird noise not
      piano sound when I click Play" — the click/blip sound
      (`renderRhythmClickBuffer`, a 1kHz decaying sine, same style as the
      existing `Metronome`) was working as designed but not what was
      wanted. Asked: click vs. real piano sound — user chose piano. Fixed:
      new `audio/RhythmClickTrack.h/.cpp::renderRhythmPianoBuffer()`
      (test-first: `tests/test_RhythmPianoPlayback.cpp`, integration-level
      like `test_Synth.cpp` — audio shortly after onset, silence during a
      leading rest, buffer covers duration + release tail) — renders the
      pattern through a real `SynthEngine` (Acoustic Grand Piano, fixed at
      middle C since rhythm reading doesn't depend on pitch), block-by-
      block with noteOn/noteOff triggered at each note's onset/end sample,
      mirroring `OfflineRenderer`'s block-loop style. `PracticePanel::
      onPlayRhythmClicked()` switched to call this instead of the click
      version (which stays in place, tested, unused by the UI for now).
      Full suite (42/42 binaries) passes.
- [ ] Full manual still not done this session: automated-click UI testing
      in this environment proved unreliable (coordinate/window-geometry
      mismatches across attempts, and one stale differently-named running
      instance — `~/.local/bin/recording-studio` vs. this repo's build
      output `recording_studio` — was accidentally screenshotted instead
      of the freshly built binary before that was caught). Needs, by
      hand: switch to Rhythm mode, select each of the 7 words and
      sanity-check the staff drawing, press Play and confirm it now
      sounds like piano notes (not a click) at the right rhythm, press
      Tap Back and tap along (button or Spacebar), confirm scoring
      reflects accurate vs. mistimed/missed taps, confirm switching back
      to Pitch mode still works unchanged.

## Completed: Digital playback time display

Goal: an always-visible digital readout of playback position in the top
bar, next to the transport/recording controls, click-to-toggle between
Timecode (HH:MM:SS.mmm) and raw sample count. Not a `Qt::Popup` (the
existing `EffectsPopoverWidget` floating-panel pattern auto-closes on
outside click, which is wrong for something meant to stay visible through
playback) — a plain docked widget in the top bar's `QHBoxLayout`.

Design (approved):
- [x] `ui/TimeDisplayMath.h` (pure, tested first): `formatTimecode(int64_t
      samples, unsigned sampleRate)` → "HH:MM:SS.mmm" string, and
      `formatSampleCount(int64_t samples)` → grouped digit string. Edge
      cases: 0 samples, exact 1-hour rollover, non-integer
      samples/sampleRate rounding, zero sample rate.
- [x] New `ui/PlaybackTimeDisplay` (`QLabel`-based, not a popup — plain
      widget dropped into the recording section's `QHBoxLayout`, always
      visible): `MainWindow::updatePlayhead()` (the existing ~50ms tick)
      calls `setPositionSamples()`; no new timer. Click
      (`mousePressEvent` override) toggles between the two
      `TimeDisplayMath` formats; format choice kept in-memory only. Not a
      `QObject`/no `Q_OBJECT` — no signals needed, just a virtual-method
      override, which also avoids a header-only-widget moc/link pitfall
      (`undefined reference to vtable` when a Q_OBJECT class is defined
      only in a header AUTOMOC doesn't separately compile).
- [x] `MainWindow.cpp`: added into the recording section's
      `QHBoxLayout`, next to the track-arm picker, before the trailing
      stretch.

Verification plan (approved):
- [x] Automated: `tests/test_TimeDisplayMath.cpp` (8 cases, written
      before the formatting functions) — 0 samples, 1-hour rollover,
      rounding, zero sample rate, digit grouping. Full suite (51/51
      binaries) passes.
- [x] Smoke-tested: app builds and launches cleanly (ran the full
      timeout under a real X display, no crash/error output beyond the
      pre-existing ALSA probe warning).
- [ ] Full manual (not done from this session): start playback, confirm
      digits update live and match the timeline position; click the
      widget mid-playback, confirm it toggles format instantly without
      jumping/resetting.

## Completed: Fixed stuck-scroll after playback runs past the end of content

Bug found via user report while trying the new go-to-time feature (also
reproducible without it): letting playback run far past the session's
actual content (e.g. ~3 min of clips left playing for 30 min, or typing a
sample position far ahead into the new playback display) permanently
scrolled every track lane forward to follow the playhead
(`ClipLaneWidget::setPlayheadSample`'s existing auto-follow-during-
recording logic, which also runs during Playing). Stopping and seeking
back to the start (Play-from-Start, ruler click, marker, or the new
display) moved the playhead back but never scrolled the view back —
the auto-follow logic only handled the forward direction — leaving every
track's waveform effectively invisible (scrolled off to the right) with
no way back short of restarting the app (which resets the in-memory
scroll/extent state; no data was ever lost).

Design (approved: "snap scroll to show the seek target" for either
direction):
- [x] `ui/ClipLaneScrollMath.h`: new pure `scrollOffsetToRevealPlayhead()`
      (tested first, 5 cases: already-visible/no-op, forward past right
      edge, backward before left edge — the reported bug — clamped at
      0, and the exact-left-edge boundary). Mirrors the existing
      clamp/max-offset helpers' style.
- [x] `ClipLaneWidget::setPlayheadSample()`: forward-follow still grows
      `m_contentExtentSamples` as before (unchanged, still needed for
      the continuous-recording case per the existing comment), but the
      actual scroll decision for both directions now goes through
      `scrollOffsetToRevealPlayhead()` instead of only handling the
      forward case inline.

Verification plan (approved):
- [x] Automated: `tests/test_ClipLaneScrollMath.cpp` (+5 cases, written
      before the function). Full suite (51/51 binaries) passes.
- [x] Smoke-tested: app builds and launches cleanly (ran the full
      timeout under a real X display, no crash/error output beyond the
      pre-existing ALSA probe warning).
- [ ] Full manual (not done from this session, needs the user to
      confirm): let playback run past the end of a session's content,
      stop, seek back to 0 (via Play-from-Start/ruler/marker/the digital
      display) — confirm every track's waveform is visible again without
      needing to restart the app or manually drag any scrollbar.

## Completed: Go-to-time on the playback time display

Goal: double-click the digital playback display to type a target time and
jump the playhead there — a standard DAW transport-clock feature (Pro
Tools/Logic-style click-to-edit clock).

Design (approved):
- [x] `ui/TimeDisplayMath.h`: added `parseTimecode(QString, sampleRate)`
      and `parseSampleCount(QString)`, both `std::optional<int64_t>`
      (nullopt on invalid/negative/empty input). Timecode parsing
      accepts `HH:MM:SS.mmm`, `MM:SS.mmm`, or plain seconds; sample
      parsing accepts a raw integer with optional comma grouping.
- [x] `PlaybackTimeDisplay` rebuilt as a `Q_OBJECT` widget with its own
      `.cpp` (like `EffectsPopoverWidget`) — a header-only `Q_OBJECT`
      class hit an "undefined reference to vtable" link error last time
      (AUTOMOC doesn't compile moc output for a Q_OBJECT class defined
      only in a header included from elsewhere), so this needed a real
      source file for AUTOMOC to attach to. Internally a `QStackedLayout`
      swaps between the `QLabel` (single click toggles
      timecode/samples) and a `QLineEdit` on double-click, pre-filled
      with the current display text. Enter parses per the active
      display mode and emits `seekRequested(int64_t samples)` on
      success; invalid input is ignored (stack reverts to the label, no
      seek); Escape or focus-out without Enter also just reverts, no
      seek.
- [x] `MainWindow.cpp`: connects `seekRequested` to the existing
      `onSeekRequested()` handler already shared by the ruler and
      timeline's own seek gestures — no new seek logic needed.

Verification plan (approved):
- [x] Automated: extended `tests/test_TimeDisplayMath.cpp` (12 new
      cases, written before the parse functions) — round-trip of each
      accepted timecode/sample format, and garbage/negative/empty input
      all producing nullopt. Full suite (51/51 binaries) passes.
- [x] Smoke-tested: app builds and launches cleanly (ran the full
      timeout under a real X display, no crash/error output beyond the
      pre-existing ALSA probe warning).
- [ ] Full manual (not done from this session): double-click the display
      (stopped and while playing), type a time, press Enter, confirm the
      playhead jumps there; type garbage and press Enter, confirm it's
      ignored; press Escape mid-edit, confirm no seek happens.

## Completed: "New" in the File menu

Goal: standard File > New entry, since the menu had no obvious way to
start a blank session — "Close Session" already did exactly that (reset
to one blank track) but wasn't discoverable/positioned like a typical
New command and had no shortcut.

Design (approved): added `newSessionAction` (Ctrl+N, standard position at
the top of the File menu) wired to the existing `onCloseSessionClicked` —
same confirmation dialog and reset logic, no new behavior. "Close
Session" kept as-is (still useful lower in the menu with its explicit
name/icon).

Verification plan (approved): no new automated test (pure UI wiring, no
new logic — matches this codebase's precedent for pure menu/relocation
changes). Smoke-tested: app builds and launches cleanly (ran the full
timeout under a real X display, no crash/error output beyond the
pre-existing ALSA probe warning), full suite (51/51 binaries) still
passes.
- [ ] Full manual (not done from this session): click File > New (and
      Ctrl+N), confirm the same dialog/reset behavior as Close Session.

- Each feature gets a verification plan proposed and approved before
  implementation starts (per standing workflow rule).
- Test-first: write tests before implementation for each feature.
