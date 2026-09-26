#include <QAction>
#include <QShortcut>
#include <QSettings>
#include <QTest>
#include <QWidget>

#include "ui/ShortcutManager.h"

using namespace rsd;

class ShortcutManagerTests : public QObject {
    Q_OBJECT

private slots:
    void init() {
        // Isolated, throwaway QSettings store per test so tests never read
        // stray state from a real "RecordingStudio" install or each other.
        QCoreApplication::setOrganizationName("RSD_ShortcutManagerTests");
        QCoreApplication::setApplicationName("RSD_ShortcutManagerTests");
        QSettings settings;
        settings.clear();
    }

    void registeringActionCapturesItsShortcutAsDefault() {
        QWidget owner;
        QAction action("Undo", &owner);
        action.setShortcut(QKeySequence(QKeySequence::Undo));

        ShortcutManager mgr;
        mgr.registerAction("undo", "Undo", &action);

        QCOMPARE(mgr.entries().size(), size_t(1));
        QCOMPARE(mgr.entries()[0].current, QKeySequence(QKeySequence::Undo));
    }

    void setShortcutAppliesToTheUnderlyingAction() {
        QWidget owner;
        QAction action("Undo", &owner);
        action.setShortcut(QKeySequence("Ctrl+Z"));

        ShortcutManager mgr;
        mgr.registerAction("undo", "Undo", &action);

        QSettings settings;
        QVERIFY(mgr.setShortcut("undo", QKeySequence("Ctrl+U"), settings));
        QCOMPARE(action.shortcut(), QKeySequence("Ctrl+U"));
    }

    void setShortcutRejectsConflictAndReportsOwner() {
        QWidget owner;
        QAction undo("Undo", &owner);
        undo.setShortcut(QKeySequence("Ctrl+Z"));
        QAction redo("Redo", &owner);
        redo.setShortcut(QKeySequence("Ctrl+Y"));

        ShortcutManager mgr;
        mgr.registerAction("undo", "Undo", &undo);
        mgr.registerAction("redo", "Redo", &redo);

        QSettings settings;
        QString conflictLabel;
        QVERIFY(!mgr.setShortcut("redo", QKeySequence("Ctrl+Z"), settings, &conflictLabel));
        QCOMPARE(conflictLabel, QString("Undo"));
        QCOMPARE(redo.shortcut(), QKeySequence("Ctrl+Y")); // unchanged
    }

    void familyAppliesModifiersAcrossAllNineDigitMembers() {
        QWidget owner;
        std::vector<QShortcut*> members;
        for (int i = 1; i <= 9; ++i) {
            members.push_back(new QShortcut(QKeySequence(QKeyCombination(Qt::CTRL | Qt::SHIFT,
                                                                           static_cast<Qt::Key>(Qt::Key_0 + i))),
                                             &owner));
        }

        ShortcutManager mgr;
        mgr.registerFamily("setBookmark", "Set Bookmark", members);

        QSettings settings;
        QVERIFY(mgr.setShortcut("setBookmark", QKeySequence("Ctrl+Alt+1"), settings));

        QCOMPARE(members[0]->key(), QKeySequence(QKeyCombination(Qt::CTRL | Qt::ALT, Qt::Key_1)));
        QCOMPARE(members[8]->key(), QKeySequence(QKeyCombination(Qt::CTRL | Qt::ALT, Qt::Key_9)));
    }

    void resetToDefaultsRestoresOriginalBinding() {
        QWidget owner;
        QAction action("Undo", &owner);
        action.setShortcut(QKeySequence("Ctrl+Z"));

        ShortcutManager mgr;
        mgr.registerAction("undo", "Undo", &action);

        QSettings settings;
        QVERIFY(mgr.setShortcut("undo", QKeySequence("Ctrl+U"), settings));
        mgr.resetToDefaults(settings);

        QCOMPARE(action.shortcut(), QKeySequence("Ctrl+Z"));
    }

    void loadAppliesPreviouslyPersistedOverride() {
        QWidget owner;
        QAction action("Undo", &owner);
        action.setShortcut(QKeySequence("Ctrl+Z"));

        {
            ShortcutManager first;
            first.registerAction("undo", "Undo", &action);
            QSettings settings;
            QVERIFY(first.setShortcut("undo", QKeySequence("Ctrl+U"), settings));
        }

        // Fresh manager, fresh action at its built-in default — load() should
        // pull the persisted "Ctrl+U" back in, as MainWindow does on startup.
        QAction freshAction("Undo", &owner);
        freshAction.setShortcut(QKeySequence("Ctrl+Z"));
        ShortcutManager second;
        second.registerAction("undo", "Undo", &freshAction);
        QSettings settings;
        second.load(settings);

        QCOMPARE(freshAction.shortcut(), QKeySequence("Ctrl+U"));
    }
};

QTEST_MAIN(ShortcutManagerTests)
#include "test_ShortcutManager.moc"
