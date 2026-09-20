#include <QtTest>

#include "command/CommandStack.h"

using namespace rsd;

namespace {

// Increments/decrements a shared counter so tests can assert exactly which
// commands ran without needing real model state.
class IncrementCommand : public Command {
public:
    explicit IncrementCommand(int& counter) : m_counter(counter) {}
    void redo() override { ++m_counter; }
    void undo() override { --m_counter; }
    QString text() const override { return "Increment"; }

private:
    int& m_counter;
};

} // namespace

class TestCommandStack : public QObject {
    Q_OBJECT

private slots:
    void pushAppliesAndRecords() {
        int counter = 0;
        CommandStack stack;
        stack.push(std::make_unique<IncrementCommand>(counter));
        QCOMPARE(counter, 1);
        QVERIFY(stack.canUndo());
        QVERIFY(!stack.canRedo());
    }

    void undoRevertsAndEnablesRedo() {
        int counter = 0;
        CommandStack stack;
        stack.push(std::make_unique<IncrementCommand>(counter));
        stack.undo();
        QCOMPARE(counter, 0);
        QVERIFY(!stack.canUndo());
        QVERIFY(stack.canRedo());
    }

    void redoReappliesUndoneCommand() {
        int counter = 0;
        CommandStack stack;
        stack.push(std::make_unique<IncrementCommand>(counter));
        stack.undo();
        stack.redo();
        QCOMPARE(counter, 1);
        QVERIFY(stack.canUndo());
        QVERIFY(!stack.canRedo());
    }

    void pushAfterUndoDropsRedoHistory() {
        int counter = 0;
        CommandStack stack;
        stack.push(std::make_unique<IncrementCommand>(counter));
        stack.undo();
        stack.push(std::make_unique<IncrementCommand>(counter));
        QVERIFY(!stack.canRedo());
        QCOMPARE(counter, 1);
    }

    void undoOnEmptyStackIsNoop() {
        CommandStack stack;
        stack.undo(); // must not crash
        QVERIFY(!stack.canUndo());
    }

    void redoOnEmptyStackIsNoop() {
        CommandStack stack;
        stack.redo(); // must not crash
        QVERIFY(!stack.canRedo());
    }

    void multipleUndoRedoRestoreOrder() {
        int counter = 0;
        CommandStack stack;
        stack.push(std::make_unique<IncrementCommand>(counter));
        stack.push(std::make_unique<IncrementCommand>(counter));
        stack.push(std::make_unique<IncrementCommand>(counter));
        QCOMPARE(counter, 3);

        stack.undo();
        stack.undo();
        QCOMPARE(counter, 1);

        stack.redo();
        QCOMPARE(counter, 2);
    }

    void clearDropsAllHistory() {
        int counter = 0;
        CommandStack stack;
        stack.push(std::make_unique<IncrementCommand>(counter));
        stack.undo();
        stack.clear();
        QVERIFY(!stack.canUndo());
        QVERIFY(!stack.canRedo());
    }
};

QTEST_APPLESS_MAIN(TestCommandStack)
#include "test_CommandStack.moc"
