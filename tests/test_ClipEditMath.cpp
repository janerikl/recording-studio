#include <QtTest>

#include "ui/ClipEditMath.h"

using namespace rsd;

class TestClipEditMath : public QObject {
    Q_OBJECT

private slots:
    void clampsToZeroMinimum() {
        QCOMPARE(clampFadeSamples(-50, 100, 0), int64_t(0));
    }

    void clampsToClipLengthWhenNoOtherFade() {
        QCOMPARE(clampFadeSamples(500, 100, 0), int64_t(100));
    }

    void leavesRoomForTheOppositeFade() {
        // A 100-sample clip with a 40-sample fade-out already set: fade-in
        // must not be allowed to eat into (or past) that region.
        QCOMPARE(clampFadeSamples(90, 100, 40), int64_t(60));
    }

    void passesThroughValuesAlreadyInRange() {
        QCOMPARE(clampFadeSamples(30, 100, 40), int64_t(30));
    }

    void oppositeFadeFillingWholeClipForcesZero() {
        QCOMPARE(clampFadeSamples(20, 100, 100), int64_t(0));
    }
};

QTEST_APPLESS_MAIN(TestClipEditMath)
#include "test_ClipEditMath.moc"
