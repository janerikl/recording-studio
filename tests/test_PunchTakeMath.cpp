#include <QTest>

#include "audio/PunchTakeMath.h"

using rsd::takeBufferIndexForPass;

class PunchTakeMathTests : public QObject {
    Q_OBJECT

private slots:
    void withinCap_mapsDirectly() {
        QCOMPARE(takeBufferIndexForPass(0, 8), 0);
        QCOMPARE(takeBufferIndexForPass(3, 8), 3);
        QCOMPARE(takeBufferIndexForPass(7, 8), 7);
    }

    void atOrBeyondCap_clampsToLastSlot() {
        QCOMPARE(takeBufferIndexForPass(8, 8), 7);
        QCOMPARE(takeBufferIndexForPass(100, 8), 7);
    }

    void negativePassCount_clampsToZero() {
        QCOMPARE(takeBufferIndexForPass(-1, 8), 0);
    }
};

QTEST_MAIN(PunchTakeMathTests)
#include "test_PunchTakeMath.moc"
