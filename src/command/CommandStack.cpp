#include "CommandStack.h"

namespace rsd {

void CommandStack::push(std::unique_ptr<Command> cmd) {
    cmd->redo();
    m_undoStack.push_back(std::move(cmd));
    if (m_undoStack.size() > kMaxDepth) m_undoStack.erase(m_undoStack.begin());
    m_redoStack.clear(); // a fresh edit invalidates any redo history
    if (m_onChange) m_onChange();
}

void CommandStack::undo() {
    if (m_undoStack.empty()) return;
    auto cmd = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    cmd->undo();
    m_redoStack.push_back(std::move(cmd));
    if (m_onChange) m_onChange();
}

void CommandStack::redo() {
    if (m_redoStack.empty()) return;
    auto cmd = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    cmd->redo();
    m_undoStack.push_back(std::move(cmd));
    if (m_onChange) m_onChange();
}

void CommandStack::clear() {
    m_undoStack.clear();
    m_redoStack.clear();
}

} // namespace rsd
