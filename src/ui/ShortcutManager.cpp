#include "ShortcutManager.h"

#include <QAction>
#include <QShortcut>

namespace rsd {

void ShortcutManager::registerAction(const QString& id, const QString& label, QAction* action) {
    Entry entry;
    entry.id = id;
    entry.label = label;
    entry.current = action->shortcut();
    entry.isFamily = false;
    entry.apply = [action](const QKeySequence& seq) { action->setShortcut(seq); };
    m_entries.push_back(std::move(entry));
    m_defaults.push_back(action->shortcut());
}

void ShortcutManager::registerShortcut(const QString& id, const QString& label, QShortcut* shortcut) {
    Entry entry;
    entry.id = id;
    entry.label = label;
    entry.current = shortcut->key();
    entry.isFamily = false;
    entry.apply = [shortcut](const QKeySequence& seq) { shortcut->setKey(seq); };
    m_entries.push_back(std::move(entry));
    m_defaults.push_back(shortcut->key());
}

void ShortcutManager::registerFamily(const QString& id, const QString& label,
                                      const std::vector<QShortcut*>& members) {
    Entry entry;
    entry.id = id;
    entry.label = label;
    entry.current = members.front()->key();
    entry.isFamily = true;
    entry.apply = [members](const QKeySequence& digit1Chord) {
        if (digit1Chord.isEmpty()) return;
        Qt::KeyboardModifiers mods = digit1Chord[0].keyboardModifiers();
        for (size_t i = 0; i < members.size(); ++i) {
            auto digitKey = static_cast<Qt::Key>(Qt::Key_1 + i);
            members[i]->setKey(QKeySequence(QKeyCombination(mods, digitKey)));
        }
    };
    m_entries.push_back(std::move(entry));
    m_defaults.push_back(members.front()->key());
}

void ShortcutManager::load(QSettings& settings) {
    for (auto& entry : m_entries) {
        QString key = "shortcuts/" + entry.id;
        if (!settings.contains(key)) continue;
        QKeySequence saved = QKeySequence::fromString(settings.value(key).toString());
        if (saved.isEmpty()) continue;
        entry.current = saved;
        if (entry.isFamily) {
            applyFamily(entry, saved);
        } else {
            entry.apply(saved);
        }
    }
}

ShortcutManager::Entry* ShortcutManager::findById(const QString& id) {
    for (auto& entry : m_entries) {
        if (entry.id == id) return &entry;
    }
    return nullptr;
}

QKeySequence ShortcutManager::concreteChord(const Entry& entry) { return entry.current; }

void ShortcutManager::applyFamily(Entry& entry, const QKeySequence& digit1Chord) {
    entry.current = digit1Chord;
    entry.apply(digit1Chord);
}

void ShortcutManager::persist(QSettings& settings) {
    for (auto& entry : m_entries) {
        settings.setValue("shortcuts/" + entry.id, entry.current.toString());
    }
}

bool ShortcutManager::setShortcut(const QString& id, const QKeySequence& seq, QSettings& settings,
                                   QString* conflictLabel) {
    Entry* target = findById(id);
    if (!target) return false;

    QKeySequence candidateChord = seq;
    if (target->isFamily && !seq.isEmpty()) {
        // Normalize to the digit-1 representative so comparisons below (and
        // storage) are consistent regardless of which digit the user pressed
        // while recording the chord.
        candidateChord = QKeySequence(QKeyCombination(seq[0].keyboardModifiers(), Qt::Key_1));
    }

    for (auto& other : m_entries) {
        if (&other == target) continue;
        if (!other.current.isEmpty() && concreteChord(other) == candidateChord) {
            if (conflictLabel) *conflictLabel = other.label;
            return false;
        }
    }

    if (target->isFamily) {
        applyFamily(*target, candidateChord);
    } else {
        target->current = candidateChord;
        target->apply(candidateChord);
    }
    persist(settings);
    return true;
}

void ShortcutManager::resetToDefaults(QSettings& settings) {
    for (size_t i = 0; i < m_entries.size(); ++i) {
        auto& entry = m_entries[i];
        if (entry.isFamily) {
            applyFamily(entry, m_defaults[i]);
        } else {
            entry.current = m_defaults[i];
            entry.apply(m_defaults[i]);
        }
    }
    persist(settings);
}

} // namespace rsd
