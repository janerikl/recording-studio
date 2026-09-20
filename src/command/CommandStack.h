#pragma once

#include <memory>
#include <vector>

#include "Command.h"

namespace rsd {

// Plain undo/redo history. Not a QObject: callers (MainWindow) already know
// when to refresh Undo/Redo action state, so this just exposes canUndo/canRedo
// rather than adding a signal for a single caller to connect to.
class CommandStack {
public:
    // Runs cmd->redo(), then pushes it. Any existing redo history is dropped,
    // matching the previous snapshot-based undo's behavior.
    void push(std::unique_ptr<Command> cmd);

    void undo();
    void redo();

    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }

    void clear();

private:
    std::vector<std::unique_ptr<Command>> m_undoStack;
    std::vector<std::unique_ptr<Command>> m_redoStack;
    static constexpr size_t kMaxDepth = 50;
};

} // namespace rsd
