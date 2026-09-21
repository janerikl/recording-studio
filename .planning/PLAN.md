# Plan: Bookmarks, Playback Loop, Track Select

Agreed 2026-09-21. Test-first for each item. Design evolved during manual
verification (see notes per section).

## 1. Bookmarks — done, committed
- [x] `Session`: markers storage (slot 1-9 -> sample position)
- [x] `SessionIO`: serialize/deserialize `"markers"` JSON array; round-trip test
- [x] `Ctrl+Shift+1..9`: set marker N at current playhead
- [x] `Ctrl+1..9`: jump playhead to marker N (no-op if unset)
- [x] Visual: numbered flag on the ruler at each marker's position
- [x] Unit test (SessionIOTests::roundTripsMarkers), manual verify — works

## 2. Loop — done
- [x] `TransportClock`: `setPlaybackLoop(start,end)` / `setPlaybackLoopEnabled` (separate from punch-loop)
- [x] `advance()`: wraps to loop start once position reaches loop end, when enabled
- [x] Interaction: **not** L/Shift+L as originally planned — replaced with
      **Ctrl+drag** on the ruler to define an explicit A/B loop region,
      auto-enabling on release; plain **Ctrl+click** (no drag) disables it.
      (Original "point to end of session" design was scrapped in favor of
      an explicit draggable region, matching the existing punch-region UX.)
- [x] Visual: light blue band on the ruler for the loop region
- [x] Unit test (PlaybackLoopTests, 4 cases), manual verify — works

## 3. Track select — done
- [x] `Ctrl+Alt+1..9` selects (focuses) track by position, not original Alt+1-9
      (plain Alt+1-9 is intercepted by the desktop environment/WM before
      reaching the app — confirmed via debug logging, Alt+1 never arrived).
- [x] `TimelineView::setActiveTrack(trackId)` checks the row's "Active" radio
      button so the visual indicator updates, not just the internal selection.
- [x] Manual verify — works

## Verification
- Unit tests: `ctest` in build/, 29/29 passing.
- Manual GUI pass: launched app on real display, drove each shortcut/gesture directly.

## Status
All three features implemented, tested, and manually verified working.
