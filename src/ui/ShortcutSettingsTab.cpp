#include "ShortcutSettingsTab.h"

#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>

namespace rsd {

ShortcutSettingsTab::ShortcutSettingsTab(ShortcutManager& manager, QWidget* parent)
    : QWidget(parent), m_manager(manager) {
    auto* layout = new QVBoxLayout(this);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({"Action", "Shortcut"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    auto* resetButton = new QPushButton("Reset to Defaults", this);
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        QSettings settings("RecordingStudio", "RecordingStudio");
        m_manager.resetToDefaults(settings);
        rebuildRows();
    });
    layout->addWidget(resetButton);

    rebuildRows();
}

void ShortcutSettingsTab::rebuildRows() {
    const auto& entries = m_manager.entries();
    m_table->setRowCount(static_cast<int>(entries.size()));

    for (int row = 0; row < static_cast<int>(entries.size()); ++row) {
        const QString id = entries[row].id;

        auto* label = new QTableWidgetItem(entries[row].label);
        label->setFlags(label->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 0, label);

        auto* edit = new QKeySequenceEdit(entries[row].current, m_table);
        m_table->setCellWidget(row, 1, edit);

        connect(edit, &QKeySequenceEdit::editingFinished, this, [this, id, edit]() {
            QKeySequence recorded = edit->keySequence();
            // Every shortcut in this app is a single chord (one key + modifiers);
            // QKeySequenceEdit can record up to 4 in sequence, so only the first
            // is meaningful here — drop the rest immediately.
            QKeySequence seq = recorded.isEmpty() ? recorded : QKeySequence(recorded[0]);
            edit->setKeySequence(seq);
            QSettings settings("RecordingStudio", "RecordingStudio");
            QString conflictLabel;
            if (!m_manager.setShortcut(id, seq, settings, &conflictLabel)) {
                QMessageBox::warning(this, "Shortcut Already In Use",
                                      "That shortcut is already assigned to \"" + conflictLabel + "\".");
                // Revert the field to whatever's actually still bound.
                for (auto& entry : m_manager.entries()) {
                    if (entry.id == id) {
                        edit->setKeySequence(entry.current);
                        break;
                    }
                }
            }
        });
    }

    m_table->resizeColumnsToContents();
}

} // namespace rsd
