#pragma once

#include <QKeySequence>
#include <QSettings>
#include <QString>
#include <functional>
#include <vector>

class QAction;
class QShortcut;

namespace rsd {

// Registry of every user-reassignable keyboard shortcut in the app, backing
// the "Keyboard Shortcuts" tab in SettingsDialog. Persists to QSettings
// under "shortcuts/<id>" so custom bindings survive restarts.
//
// Most entries are a single QAction or QShortcut. Three families
// (Set Bookmark, Jump to Bookmark, Select Track) are actually 9 QShortcuts
// each — one per digit 1-9 — that always share the same modifier chord.
// Those are registered as a single "family" entry: the user edits one
// representative chord (digit 1), and the modifiers from that are re-applied
// across all 9 digits.
class ShortcutManager {
public:
    struct Entry {
        QString id;
        QString label;
        QKeySequence current; // for a family, this is the digit-1 representative chord
        bool isFamily = false;
        std::function<void(const QKeySequence&)> apply; // pushes `current` out to the real QAction/QShortcut(s)
    };

    // `action`'s shortcut at registration time becomes the default.
    void registerAction(const QString& id, const QString& label, QAction* action);
    // `shortcut`'s key at registration time becomes the default.
    void registerShortcut(const QString& id, const QString& label, QShortcut* shortcut);
    // `members[i]` must be the digit-(i+1) QShortcut (members[0] = digit 1, ... members[8] = digit 9).
    // `members[0]`'s key at registration time becomes the default representative chord.
    void registerFamily(const QString& id, const QString& label, const std::vector<QShortcut*>& members);

    // Loads any saved overrides from `settings` and applies them (falls back
    // to each entry's already-applied default when nothing is saved).
    void load(QSettings& settings);

    const std::vector<Entry>& entries() const { return m_entries; }

    // Attempts to assign `seq` to the entry named `id`. Returns true and
    // applies+persists (to `settings`) on success. Returns false and, if
    // `conflictLabel` is non-null, fills it with the label of the entry
    // already using that chord, if `seq` collides with a different entry's
    // current binding.
    bool setShortcut(const QString& id, const QKeySequence& seq, QSettings& settings,
                      QString* conflictLabel = nullptr);

    void resetToDefaults(QSettings& settings);

private:
    void persist(QSettings& settings);

private:
    Entry* findById(const QString& id);
    // The concrete, comparable chord for this entry: `current` for a single
    // entry, or the digit-1 chord for a family (families only ever collide
    // with another entry through their digit-1 chord — none of the single
    // entries in this app use a bare digit key, so that's sufficient).
    static QKeySequence concreteChord(const Entry& entry);
    // Rebuilds and applies the 9-chord family from a new digit-1 chord.
    void applyFamily(Entry& entry, const QKeySequence& digit1Chord);

    std::vector<Entry> m_entries;
    std::vector<QKeySequence> m_defaults; // parallel to m_entries
};

} // namespace rsd
