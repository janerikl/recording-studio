#pragma once

#include <QString>

namespace rsd {

// A single undoable/redoable edit. Implementations wrap the same Track/Session
// mutator methods the UI already calls directly (addClip, replaceClip,
// restoreClips, ...) rather than duplicating mutation logic — a Command is
// just "capture what to call and with what before/after state."
class Command {
public:
    virtual ~Command() = default;

    // Applies the edit. Called once when the command is pushed, and again
    // whenever the user redoes it.
    virtual void redo() = 0;

    // Reverts the edit, restoring exactly the state redo() started from.
    virtual void undo() = 0;

    virtual QString text() const = 0;
};

} // namespace rsd
