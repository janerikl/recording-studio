#pragma once

#include <QString>

class QDialog;
class QSettings;

namespace rsd {

// Remembers a dialog's size/position across app restarts, under
// `settings` key "dialogs/<key>/geometry". Call restoreDialogGeometry() once
// right after constructing the dialog's widgets (before exec()), and connect
// QDialog::finished to saveDialogGeometry() so the size sticks whether the
// dialog was accepted or cancelled — same "remember on close, restore on
// reopen" behavior MainWindow already uses for its own window/dock layout.
// Callers pass their own QSettings (MainWindow/dialogs use
// QSettings("RecordingStudio", "RecordingStudio")) so tests can use an
// isolated store instead.
void restoreDialogGeometry(QDialog& dialog, QSettings& settings, const QString& key);
void saveDialogGeometry(QDialog& dialog, QSettings& settings, const QString& key);

} // namespace rsd
