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

    void gainClampsToZeroToTwoRange() {
        QCOMPARE(clampClipGain(-1.0f), 0.0f);
        QCOMPARE(clampClipGain(3.0f), 2.0f);
        QCOMPARE(clampClipGain(1.5f), 1.5f);
    }

    void draggingUpIncreasesGain() {
        // Full-height drag upward (negative deltaY) swings by the full
        // range (2.0), clamped at the ceiling.
        QCOMPARE(gainAfterVerticalDrag(1.0f, -100, 100), 2.0f);
    }

    void draggingDownDecreasesGain() {
        QCOMPARE(gainAfterVerticalDrag(1.0f, 100, 100), 0.0f);
    }

    void smallDragGivesProportionalChange() {
        // Half the lane height dragged up = half the full 2.0 swing = +1.0.
        QCOMPARE(gainAfterVerticalDrag(0.5f, -50, 100), 1.5f);
    }

    void zeroHeightLane_doesNotDivideByZero() {
        QCOMPARE(gainAfterVerticalDrag(1.0f, -50, 0), 1.0f);
    }

    void gainLineYAtUnityIsVerticalCenter() {
        QCOMPARE(gainLineY(1.0f, 0, 100), 50);
    }

    void gainLineYAtZeroIsBottom() {
        QCOMPARE(gainLineY(0.0f, 0, 100), 100);
    }

    void gainLineYAtTwoIsTop() {
        QCOMPARE(gainLineY(2.0f, 0, 100), 0);
    }

    void gainLineYRespectsClipTopOffset() {
        QCOMPARE(gainLineY(1.0f, 20, 100), 70);
    }
};

QTEST_APPLESS_MAIN(TestClipEditMath)
#include "test_ClipEditMath.moc"
