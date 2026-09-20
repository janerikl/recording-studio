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
3. [ ] Playlist comping (multiple takes per track, comp best parts)
4. [ ] Clip-level editing tools (trim/fade/gain handles directly on clips)
5. [ ] Virtual instruments (basic synth + sampler, MIDI-playable)
6. [ ] MIDI piano-roll editor
7. [ ] Automation lanes (volume/pan/filter over time)
8. [ ] Bus routing / sends (aux tracks, submixes)
9. [ ] Loop/sample library browser
10. [ ] Export/bounce with format + stems options
11. [x] Multi-source input: mic + system audio (loopback) recorded to separate tracks (Linux/PulseAudio)

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

## Notes

- Each feature gets a verification plan proposed and approved before
  implementation starts (per standing workflow rule).
- Test-first: write tests before implementation for each feature.
