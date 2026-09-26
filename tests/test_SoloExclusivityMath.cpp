#include <QtTest/QtTest>

#include "ui/SoloExclusivityMath.h"

using namespace rsd;

class SoloExclusivityMathTests : public QObject {
    Q_OBJECT

private slots:
    void soloingNewTrack_unsolosAllOthers() {
        QVector<QString> soloed{"A", "B"};
        auto result = tracksToUnsolo(soloed, "B");
        QCOMPARE(result.size(), 1);
        QCOMPARE(result[0], QString("A"));
    }

    void soloingOnlySoloedTrack_unsolosNothing() {
        QVector<QString> soloed{"A"};
        auto result = tracksToUnsolo(soloed, "A");
        QVERIFY(result.isEmpty());
    }

    void noTracksSoloed_unsolosNothing() {
        QVector<QString> soloed{};
        auto result = tracksToUnsolo(soloed, "A");
        QVERIFY(result.isEmpty());
    }

    void toggledTrackNotInSoloedList_unsolosNothing() {
        // Un-soloing: the toggled track was removed from the soloed set
        // before this is called, so it's absent — no other track's state
        // should change.
        QVector<QString> soloed{"A"};
        auto result = tracksToUnsolo(soloed, "B");
        QVERIFY(result.isEmpty());
    }

    void threeTracksSoloed_unsolosAllButToggled() {
        QVector<QString> soloed{"A", "B", "C"};
        auto result = tracksToUnsolo(soloed, "C");
        QCOMPARE(result.size(), 2);
        QVERIFY(result.contains("A"));
        QVERIFY(result.contains("B"));
    }
};

QTEST_APPLESS_MAIN(SoloExclusivityMathTests)
#include "test_SoloExclusivityMath.moc"
