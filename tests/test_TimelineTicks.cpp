#include <QtTest>

#include "ui/TimelineTicks.h"

using rsd::niceTickSeconds;

class TimelineTicksTests : public QObject {
    Q_OBJECT

private slots:
    void picksExactStepWhenRawTickIsExactlyANiceValue() {
        // 400px wide, 40s total => 0.1s/px => 80px target * 0.1 = 8s raw,
        // nearest nice step >= 8 is 10.
        QCOMPARE(niceTickSeconds(40.0, 400), 10.0);
    }

    void widerLaneForSameDuration_picksSmallerStep() {
        // Same 40s duration but a wider lane means smaller raw tick seconds,
        // so a finer step should be chosen than for a narrower lane.
        double wide = niceTickSeconds(40.0, 2000);
        double narrow = niceTickSeconds(40.0, 200);
        QVERIFY(wide < narrow);
    }

    void longerDurationForSameWidth_picksLargerStep() {
        double shortDur = niceTickSeconds(20.0, 400);
        double longDur = niceTickSeconds(400.0, 400);
        QVERIFY(longDur > shortDur);
    }

    void neverExceedsLargestNiceStep() {
        QCOMPARE(niceTickSeconds(100000.0, 10), 300.0);
    }

    void zeroOrNegativeInputsFallBackToSmallestStepWithoutCrashing() {
        QCOMPARE(niceTickSeconds(0.0, 400), 0.1);
        QCOMPARE(niceTickSeconds(40.0, 0), 0.1);
        QCOMPARE(niceTickSeconds(-5.0, 400), 0.1);
    }
};

QTEST_MAIN(TimelineTicksTests)
#include "test_TimelineTicks.moc"
