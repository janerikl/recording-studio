#include <QTest>

#include "audio/AutomationMath.h"

using namespace rsd;

class AutomationMathTests : public QObject {
    Q_OBJECT

private slots:
    void evaluateEmptyCurveReturnsFallback() {
        std::vector<AutomationPoint> points;
        QCOMPARE(evaluateAutomation(points, 1000, 0.75f), 0.75f);
    }

    void evaluateSinglePointIsConstant() {
        std::vector<AutomationPoint> points{{5000, 0.3f}};
        QCOMPARE(evaluateAutomation(points, 0, 1.0f), 0.3f);
        QCOMPARE(evaluateAutomation(points, 5000, 1.0f), 0.3f);
        QCOMPARE(evaluateAutomation(points, 999999, 1.0f), 0.3f);
    }

    void evaluateBeforeFirstPointClampsToFirstValue() {
        std::vector<AutomationPoint> points{{1000, 0.2f}, {2000, 0.8f}};
        QCOMPARE(evaluateAutomation(points, 0, 1.0f), 0.2f);
    }

    void evaluateAfterLastPointClampsToLastValue() {
        std::vector<AutomationPoint> points{{1000, 0.2f}, {2000, 0.8f}};
        QCOMPARE(evaluateAutomation(points, 5000, 1.0f), 0.8f);
    }

    void evaluateBetweenPointsInterpolatesLinearly() {
        std::vector<AutomationPoint> points{{1000, 0.0f}, {2000, 1.0f}};
        QCOMPARE(evaluateAutomation(points, 1500, 0.0f), 0.5f);
        QCOMPARE(evaluateAutomation(points, 1250, 0.0f), 0.25f);
        QCOMPARE(evaluateAutomation(points, 1000, 0.0f), 0.0f);
        QCOMPARE(evaluateAutomation(points, 2000, 0.0f), 1.0f);
    }

    void evaluateWithThreePointsPicksCorrectSegment() {
        std::vector<AutomationPoint> points{{0, 0.0f}, {1000, 1.0f}, {2000, 0.0f}};
        QCOMPARE(evaluateAutomation(points, 500, 0.0f), 0.5f);
        QCOMPARE(evaluateAutomation(points, 1500, 0.0f), 0.5f);
    }

    // --- hit-testing / editing ---

    void findPointNearReturnsClosestWithinTolerance() {
        std::vector<AutomationPoint> points{{1000, 0.5f}, {5000, 0.2f}};
        QCOMPARE(findPointNear(points, 1050, 100), 0);
        QCOMPARE(findPointNear(points, 4950, 100), 1);
        QCOMPARE(findPointNear(points, 3000, 100), -1);
    }

    void insertPointKeepsListSortedBySample() {
        std::vector<AutomationPoint> points{{0, 0.0f}, {2000, 1.0f}};
        insertPointSorted(points, {1000, 0.5f});
        QCOMPARE(points.size(), size_t(3));
        QCOMPARE(points[1].sample, int64_t(1000));
        QCOMPARE(points[1].value, 0.5f);
    }

    void insertPointReplacesExistingAtSameSample() {
        std::vector<AutomationPoint> points{{0, 0.0f}, {1000, 0.5f}};
        insertPointSorted(points, {1000, 0.9f});
        QCOMPARE(points.size(), size_t(2));
        QCOMPARE(points[1].value, 0.9f);
    }
};

QTEST_MAIN(AutomationMathTests)
#include "test_AutomationMath.moc"
