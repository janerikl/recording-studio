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
