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

## Notes

- Each feature gets a verification plan proposed and approved before
  implementation starts (per standing workflow rule).
- Test-first: write tests before implementation for each feature.
