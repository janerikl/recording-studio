# Single-clip copy/paste with chained append

## Goal
Ctrl+C on a selected clip/note, Ctrl+V pastes it immediately after the
original; repeated Ctrl+V chains each new paste after the last one.
Existing clips/notes in the way get pushed later (insert semantics).
Applies to both audio Clips and Instrument MIDI notes. Reuses the existing
Ctrl+C/Ctrl+V shortcuts already used for range-selection copy/paste,
falling back to single-item mode when there's no active range selection.

## Steps
- [x] Write failing tests in tests/test_EditClipboard.cpp for `insertItemAfter`
      (push semantics) and `EditClipboardStore` single-item state + mutual
      exclusion with the range clipboard.
- [x] Implement `insertItemAfter<Item>` and `SingleItemClipboard<Item>` +
      store methods in src/model/EditClipboard.h. Tests pass.
- [x] Wire Ctrl+C/Ctrl+V in ClipLaneWidget: copy selected clip/note when no
      range selection is active; paste via insert-after-chain when the
      clipboard holds a single item scoped to this track. Added MIDI note
      click-to-select (previously view-only) so notes can be selected too.
- [ ] Manual verification via GUI: chained paste on an audio track and an
      Instrument track, confirm push-shift of later clips. (Not yet done —
      no GUI automation harness in this repo; build + unit tests verified
      instead.)

## Follow-up: right-click "Repeat..." context menu
- [x] Write failing tests for `repeatItemAfter<Item>` (N chained inserts).
- [x] Implement `repeatItemAfter<Item>` in src/model/EditClipboard.h.
- [x] Add a real QMenu on right-click: "Set Gain..." (existing dialog,
      unchanged) + "Repeat..." for audio clips; "Repeat..." only for
      Instrument-track MIDI notes (no other note menu existed). Repeat
      prompts for a count and inserts that many back-to-back copies after
      the clicked item, pushing later items out of the way, as one undo step.
- [ ] Manual verification via GUI (same caveat as above).

## Follow-up: keyboard shortcut preferences
Goal: a "Keyboard Shortcuts" tab in the existing Settings dialog letting the
user reassign the app's QAction/QShortcut-based shortcuts (Undo, Redo,
Delete, Zoom In/Out, Save, Play/Stop, Play from Start, Record, and the
digit-based Set Bookmark/Jump to Bookmark/Select Track families, edited as
one shared modifier chord each). Raw keyPressEvent shortcuts (ClipLaneWidget's
Ctrl+C/Ctrl+V) are intentionally out of scope. Conflicting assignments are
blocked with a warning naming the other action; a Reset to Defaults button
restores everything.

- [x] Write failing tests in tests/test_ShortcutManager.cpp (registration,
      apply, conflict rejection, family modifier propagation, reset, and
      load-from-QSettings on a fresh instance).
- [x] Implement `ShortcutManager` (src/ui/ShortcutManager.h/.cpp): registry
      of QAction/QShortcut/family entries, QSettings persistence under
      "shortcuts/<id>", conflict-checked `setShortcut`, `resetToDefaults`.
- [x] Implement `ShortcutSettingsTab` (src/ui/ShortcutSettingsTab.h/.cpp):
      one row per entry with a QKeySequenceEdit, applies+persists immediately
      on edit, shows a conflict warning and reverts on rejection.
- [x] Wire into SettingsDialog (now tabbed: Audio + Keyboard Shortcuts) and
      MainWindow (registers every shortcut at construction, loads saved
      overrides from QSettings at startup).
- [ ] Manual verification via GUI (same caveat as above: reassign a
      shortcut, restart the app, confirm it stuck; trigger a conflict and
      confirm the warning + revert).

## Follow-up: remember dialog size/position
MainWindow's own geometry + dock layout (Media Library, Instrument,
Instrument Roll, Loop Browser, Mixer) were already persisted. The gap was
the two modal dialogs, SettingsDialog and ExportDialog, which reset to
their default size/position every time.

- [x] Write failing tests in tests/test_DialogGeometry.cpp (restore no-op
      when nothing saved, round-trip through a fresh instance, distinct
      keys don't collide).
- [x] Implement `restoreDialogGeometry`/`saveDialogGeometry`
      (src/ui/DialogGeometry.h/.cpp) — thin QSettings wrapper around
      QDialog::save/restoreGeometry, taking the QSettings instance as a
      parameter (as ShortcutManager does) so tests use an isolated store.
- [x] Wire into SettingsDialog ("settingsDialog" key) and ExportDialog
      ("exportDialog" key): restore right after construction, save on
      QDialog::finished (covers both accept and cancel).
- [ ] Manual verification via GUI (same caveat as above: resize/move each
      dialog, close it, reopen, confirm it kept the new size/position).

## Status
Model/logic layers for all four features implemented and unit-tested
(17 EditClipboardTests + 8 ShortcutManagerTests + 5 DialogGeometryTests =
30 new tests; 45/45 full suite passes). Full project builds clean. GUI
behavior not manually verified for any of them — no automation harness
available in this repo; recommend a quick manual pass or setting one up via
/run-skill-generator.

# MediaBrowserPanel: fix search filter, add rename/remove

## Bug: search box doesn't filter
Root cause (src/ui/MediaBrowserPanel.cpp): `m_filterEdit`'s `textChanged`
signal is wired to `rebuildTree()`, but `rebuildTree()` reads directly from
`m_projectItems`/`m_loopItems` without applying the filter text.
`applyLoopFilter()` does the real text-matching but is only called from
`rescanLoops()`/`chooseLoopFolder()`, never from the search-box handler.
Project Media has no filtering logic at all. Confirmed visually by user
(screenshots): typing "stereo" in Loops and "take" in Project Media both
show the full unfiltered list.

- [x] Write failing tests: tests/test_LibraryFilterMath.cpp for a new pure
      `filterLibraryItems(items, filter)` helper (case-insensitive substring
      match, empty filter keeps all) — extracted rather than testing
      MediaBrowserPanel directly, following this repo's `*Math` pattern for
      testable UI logic.
- [x] Implement `filterLibraryItems()` in new src/ui/LibraryFilterMath.h/.cpp.
      Renamed the old `applyLoopFilter()` to `rebuildLoopItems()` (it now
      just rebuilds the full unfiltered m_loopItems list from m_allLoopFiles
      instead of filtering — filtering happens once, uniformly, in
      `rebuildTree()` via `filterLibraryItems()`, fixing Project Media (which
      previously had no filtering at all) in the same pass). Full build
      clean, all 48 tests pass (was 47; added LibraryFilterMathTests).
- [ ] Manual verification: build, open Media Browser, type partial name in
      both tabs, confirm list narrows to matches; clear search, confirm
      full list returns. (Screenshots from user confirmed the bug before
      the fix; still needs a post-fix manual pass.)

## Feature: rename/remove via right-click context menu
Applies to both library items (Project Media clips AND Loop-tab items) and
virtual folders, per explicit user confirmation. Remove only detaches the
reference — never deletes the file on disk. Loop items are a live
filesystem listing (not persisted in the session); their rename/remove is
in-memory only and reverts on the next rescan (Choose Folder or
refreshLoops()) — an accepted, known limitation, not a bug.

- [x] Write failing tests: `LibraryFolder::renameFolder(QUuid, QString)` in
      tests/test_LibraryFolder.cpp; new pure-function tests in
      tests/test_LibraryItemOps.cpp for `renameLibraryItemById`/
      `removeLibraryItemById` (extracted for testability, same pattern as
      LibraryFilterMath rather than testing the QWidget directly).
- [x] Implement `renameFolder()` in src/model/LibraryFolder.h/.cpp.
- [x] Implement new src/ui/LibraryItemOps.h/.cpp:
      `renameLibraryItemById`/`removeLibraryItemById`, pure functions over
      `QVector<LibraryItem>` keyed by `LibraryItem::id()`.
- [x] Implement `BrowserTreeWidget::contextMenuEvent()` in
      MediaBrowserPanel.cpp (pattern from ClipLaneWidget.cpp:604) → new
      public `MediaBrowserPanel::showContextMenu(item, globalPos)` (public,
      not private, so the nested BrowserTreeWidget can call it — same
      access pattern as the existing `mimeDataForItem`). Checks
      `kFolderRole`/`kItemIdRole` to show Rename/Remove for the right
      target; rename via QInputDialog::getText, remove via
      `removeLibraryItemById` + `m_folders.moveItem(id, nullptr)` to unfile
      + peak-cache eviction + stops preview if the removed item was
      playing. Folder rename/remove reuse `LibraryFolderTree`'s existing
      methods. Full build clean, all 49 tests pass (was 48; added
      LibraryFolderTests::renameFolder_* and LibraryItemOpsTests).
- [ ] Manual verification: right-click an item and a folder, rename each,
      remove each, confirm files on disk untouched; save + reload session,
      confirm Project Media renames/removals persisted (Loop ones won't,
      by design — confirm they revert on rescan as expected).

# Auto-save session on every edit

## Goal
Never lose unsaved work. Debounced auto-save of the current session file
whenever session state changes (tracks, clips, library folders, effects,
automation, markers), so the user never has to remember Ctrl+S. Scope:
session state only (not destructive audio edits). Autosave stays inactive
until the user does one manual Ctrl+S to establish a file path; after that
it silently keeps the same file up to date.

## Steps
- [x] Investigated mutation paths: found a central `CommandStack`
      (src/command/CommandStack.h) that ~95% of session mutations already
      flow through (tracks, clips, effects, automation, mixer state). Chose
      to hook dirty-tracking there instead of every individual call site.
      Remaining gaps handled directly: MediaBrowserPanel library
      rename/remove (bypasses CommandStack) and MainWindow::onSetMarker.
- [x] Wrote failing tests in tests/test_CommandStack.cpp for a new
      `CommandStack::setOnChange(std::function<void()>)` callback (fires on
      push/undo/redo that actually mutate, not on empty-stack no-ops).
- [x] Implemented `setOnChange` in src/command/CommandStack.h/.cpp. Tests
      pass.
- [x] Added `MediaBrowserPanel::libraryChanged()` signal, emitted from
      renameFolder/removeFolder/renameItem/removeItem (the four mutations
      that bypass CommandStack).
- [x] Added `dirty = true` at the one remaining gap, MainWindow::onSetMarker.
- [x] Added debounced auto-save in MainWindow: single-shot QTimer
      (m_autoSaveTimer, 1.5s), restarted via `requestAutoSave()` — wired to
      CommandStack::setOnChange and MediaBrowserPanel::libraryChanged.
      Inactive until m_currentSessionPath is set (i.e. until one manual
      Ctrl+S); onAutoSaveTimeout() silently calls SessionIO::saveSession()
      and clears dirty on success, no dialogs on failure. Timer
      stopped/dirty cleared on session load and Close Session so state
      from a previous session can't leak into a fresh one.
      Full build clean, all 49 tests pass (was 48 before the new
      CommandStackTests; note total count is unchanged because
      CommandStackTests already existed — 6 new cases added to it).
- [ ] Manual verification via GUI (no automation harness in this repo):
      edit a session, wait ~2s without Ctrl+S, confirm the file on disk
      updated (mtime/diff) with no prompt/block; force-quit after an
      unsaved edit and relaunch, confirm the change survived.

# Visualize Bus track lane (live meter instead of empty placeholder)

## Goal
Bus tracks have no clips of their own, so the timeline lane currently shows
the misleading "No audio — Import or Record into this track" placeholder
(src/ui/ClipLaneWidget.cpp:205-227), which only makes sense for real audio
tracks. Bus tracks already compute live post-fader peak levels
(`Track::postFaderPeakL/R`, src/model/Track.h:73-74, fed by
SessionMixer.cpp:192-203) used by the mixer strip's `LevelMeterWidget`.
Replace the placeholder for Bus-kind tracks with a live meter fill in the
lane itself, in a solid amber/orange color (distinct from the mixer's
level-based green/yellow/red) so it reads unambiguously as "not a normal
audio track." When idle (no signal / stopped), show a flat/unlit meter plus
a label naming the source track(s) currently routed to this bus (scanned
via each track's `sendBusId()` against this bus's id), replacing the
"Import or Record" text entirely for buses.

## Steps
- [x] In ClipLaneWidget.cpp/.h, branch on `m_track->kind == TrackKind::Bus`
      before the existing `clips->empty()` placeholder block, into a new
      `paintBusMeter()`. Draws two amber (220,150,60) horizontal bars (L/R)
      reading `m_busDisplayL/R` (decayed the same way LevelMeterWidget.cpp:
      25-28 does, via new `updateBusMeter(peakL, peakR)`), plus a centered
      sender label (`setSenderLabel()`/`m_senderLabel`).
- [x] Added `TimelineView::updateBusMeters()`: iterates all rows, for
      Bus-kind tracks calls `clipLane()->updateBusMeter(postFaderPeakL/R)`
      and recomputes the label by scanning all other rows for
      `sendBusId() == track->id`, joining sender track names (or "no tracks
      routed to it" when none). Wired from `MainWindow::updateMeters()`
      (already ticking every 33ms via m_meterTimer), so it's live during
      playback and idle alike, same cadence as the mixer strip meters.
- [x] Non-bus tracks untouched — the `TrackKind::Bus` branch returns before
      reaching the original `clips->empty()` placeholder code.
- [x] Full build clean (recording_studio + tests), all 50 tests pass
      (unchanged — no test coverage added for this one, per explicit user
      choice to verify manually only).
- [ ] Manual verification via GUI: route a track's send to a Bus track,
      play audio, confirm the bus lane fills with an amber meter tracking
      the source level; stop/remove the send, confirm the idle flat meter +
      correct "Bus — receives from Track X" label (multiple senders listed
      if more than one); confirm normal tracks' empty-lane placeholder is
      unchanged.

# Move master Out meter next to master volume in mixer

## Goal
The master "Out" level meter (`MainWindow.cpp:278`, `LevelMeterWidget`) is
currently in a horizontal `meterRow` at the top of the app, next to the
`In` meter, disconnected from the master volume slider in the mixer
section (`MixerPanel.cpp:36-62`, `m_masterVolumeSlider`). Move it into the
mixer's master strip as vertical L/R bars right beside the slider, matching
the look of per-track meters in `MixerStripWidget` (vertical
`LevelMeterWidget`, 120px height, paired with that track's fader). Leave
the `In` meter where it is in the top bar — a separate "recording section"
idea (In meter + digital time) was raised but is out of scope here and can
be planned separately later.

## Steps
- [x] In MainWindow.cpp, remove `m_outputMeter` from `meterRow`/
      `meterLayout` (keep `m_inputMeter` there); remove the now-unused
      `m_outputMeter` member/updates from MainWindow.h/.cpp (its level
      feed via `m_engine->outputPeakL/R()` moves to MixerPanel).
- [x] In MixerPanel.h/.cpp, add a vertical `LevelMeterWidget` inside
      `masterStrip`, placed in a horizontal `faderRow` sub-layout next to
      `m_masterVolumeSlider` — mirrors MixerStripWidget's slider+meter
      pairing.
- [x] Changed `MixerPanel::updateMeters()` to `updateMeters(float
      masterPeakL, float masterPeakR)`, wired from
      `MainWindow::updateMeters()` using `m_engine->outputPeakL/R()`
      (same source the old `m_outputMeter` used), same 33ms cadence.
- [x] Styling refinements from GUI review: `masterLayout` pinned to
      `Qt::AlignTop` (was centering in leftover stretch space, same fix
      as MixerStripWidget's Bus-strip case); strip widened 90px → 110px
      with a bold "Master" label and lighter background tint to read as
      visually distinct from track strips; removed the fixed 120px height
      on the slider/meter so `faderRow`'s stretch factor lets them expand
      to fill the strip; added a 10px gap between the title and FX button,
      and an 8px bottom margin so the slider/meter visibly stop short of
      the strip's edge.
- [x] Build clean (recording_studio + tests), all 50 tests pass.
- [x] Manual verification via GUI, confirmed by user across several
      screenshot review rounds: mixer master strip shows vertical L/R Out
      bars next to the volume slider; top bar still shows In meter alone
      in its old spot; master volume slider still controls output level.

# Recording section: In meter + digital elapsed-time readout

## Goal
Turn the top-bar `meterRow` (MainWindow.cpp:275-279, currently just the
`In` meter) into a small "recording section": the existing In meter plus a
digital clock label showing elapsed recording time (mm:ss, hh:mm:ss once
past an hour). The clock only ticks while `TransportState::Recording` is
active (per user choice — not a general playhead/transport clock); it
shows/resets to `00:00` otherwise. No existing time display exists
anywhere in the app (confirmed by research) — this is the first one.

## Steps
- [x] Added `int64_t m_recordingStartSample = 0` to MainWindow.h. Set at
      both places recording actually starts: the normal record path
      (MainWindow.cpp, in `onRecordClicked()`, right before
      `TransportState::Recording` is set) and the separate punch/loop
      recording early-return path (also in `onRecordClicked()`, right
      after `setPositionSamples()` for the pre-roll).
- [x] Added `QLabel* m_recordingTimeLabel` to MainWindow.h, created next to
      `m_inputMeter` in the `meterRow`/`meterLayout`, starting at "00:00".
- [x] Added `formatElapsedTime(int64_t totalSeconds)` static helper in
      MainWindow.cpp: `mm:ss` under an hour, `h:mm:ss` at/above.
- [x] In `updateMeters()` (already ticking every 33ms via `m_meterTimer`),
      if `m_engine->transport().state() == TransportState::Recording`,
      compute elapsed samples as `positionSamples() -
      m_recordingStartSample`, divide by `m_session->sampleRate`, format,
      and set the label text. Otherwise set the label to `00:00`.
- [x] Build clean (recording_studio + tests), all 50 tests pass; no new
      automated tests for this pure-UI/manual-timing feature, per prior
      explicit user choice to verify such things manually.
- [x] Manual verification via GUI, confirmed by user across several
      screenshot review rounds: clock counts up during Record and resets
      on Stop; recording section layout (Record/Stop/Play/Play-from-Start,
      clock, In meter, Track/Source picker) all confirmed visually.
- [x] Styled `m_recordingTimeLabel` as an LCD-style counter box, per user
      reference (Pro Tools-style transport clock digit display): dark
      background panel with border, bold monospace green digits. Single
      mm:ss/hh:mm:ss field only — not the full multi-field bars/tempo
      counter from the reference image (explicitly out of scope, confirmed
      with user).
- [x] Moved the Record button off the main toolbar (removed
      `toolbar->addAction(m_recordAction)`) into the recording section
      itself, as a `QToolButton` with `setDefaultAction(m_recordAction)` —
      same action/enabled-state wiring, just relocated. Reordered the
      section to [Record button] [time clock] [In meter] and capped
      `meterRow` at `setMaximumWidth(280)` with zero margins/tight spacing
      to keep the whole recording section compact.
- [x] Added a Stop button directly below the Record button (stacked in a
      small `transportButtons` sub-widget with its own `QVBoxLayout`),
      wired to the same `m_stopAction` already used by the main toolbar's
      Stop button — same enabled-state wiring, just a second button.
- [x] Added a Track+Source quick-picker next to the recording section
      (`m_recordTrackCombo`, `m_recordSourceCombo` in MainWindow.h/.cpp):
      lists Audio-kind tracks only (matches `filterRecordableTracks()`);
      selecting a track arms it and unarms all others (exclusive, unlike
      the per-track Arm checkboxes which allow multiple), each change
      pushed as a `TrackStateCommand` for undo/redo, mirroring
      MixerStripWidget's existing Arm/Source pattern. Source combo shows
      the selected track's Mic/System Audio setting and edits it the same
      way. `refreshRecordTrackCombo()` repopulates on every track
      add/remove/session-load call site, preserving the current selection
      by id when it still exists.
- [x] Moved Play and Play-from-Start off the main toolbar into the
      recording section too: `transportButtons` restructured into two
      stacked columns (Record/Stop, Play/Play-from-Start) side by side,
      same QActions just relocated. Widened `meterRow`'s cap 280px → 340px
      to fit the extra column.
- [x] Switched the recording section's "In" meter to vertical orientation
      (was horizontal with a built-in "In" label; vertical draws no label
      of its own, so paired it with a separate "In" `QLabel` above it in a
      new `inputMeterColumn`). Added 10px top/bottom margins (20px total)
      to `recordingSectionLayout` to give the whole recording section more
      breathing room.
- [x] Changed the In meter to only reflect live level while
      `TransportState::Recording` is active (was: tracked mic input
      continuously, even stopped — a prior deliberate choice per an old
      code comment, explicitly superseded per user request). Shows flat
      (0/0) otherwise.