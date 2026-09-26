#pragma once

#include <QWidget>

#include "ui/ShortcutManager.h"

class QTableWidget;

namespace rsd {

// "Keyboard Shortcuts" tab of SettingsDialog: one row per ShortcutManager
// entry, each with a QKeySequenceEdit the user can record a new chord into.
// Changes apply and persist immediately (not gated behind the dialog's
// OK/Cancel, which only governs the Audio tab) so Reset/edits are visible
// right away and survive Cancel.
class ShortcutSettingsTab : public QWidget {
    Q_OBJECT

public:
    ShortcutSettingsTab(ShortcutManager& manager, QWidget* parent = nullptr);

private:
    void rebuildRows();

    ShortcutManager& m_manager;
    QTableWidget* m_table = nullptr;
};

} // namespace rsd
