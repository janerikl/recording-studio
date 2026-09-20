#include <QTest>

#include "audio/PanLawMath.h"

using namespace rsd;

class PanLawMathTests : public QObject {
    Q_OBJECT

private slots:
    void centerPanGivesEqualGains() {
        auto [l, r] = panToGains(1.0f, 0.0f);
        QCOMPARE(l, 1.0f);
        QCOMPARE(r, 1.0f);
    }

    void fullLeftPanMutesRight() {
        auto [l, r] = panToGains(1.0f, -1.0f);
        QCOMPARE(l, 1.0f);
        QCOMPARE(r, 0.0f);
    }

    void fullRightPanMutesLeft() {
        auto [l, r] = panToGains(1.0f, 1.0f);
        QCOMPARE(l, 0.0f);
        QCOMPARE(r, 1.0f);
    }

    void halfLeftPanIsMidway() {
        auto [l, r] = panToGains(1.0f, -0.5f);
        QCOMPARE(l, 1.0f);
        QCOMPARE(r, 0.5f);
    }

    void volumeScalesBothChannels() {
        auto [l, r] = panToGains(0.5f, 0.0f);
        QCOMPARE(l, 0.5f);
        QCOMPARE(r, 0.5f);

        auto [l2, r2] = panToGains(2.0f, -1.0f);
        QCOMPARE(l2, 2.0f);
        QCOMPARE(r2, 0.0f);
    }

    void panClampsOutOfRange() {
        auto [l, r] = panToGains(1.0f, -5.0f);
        QCOMPARE(l, 1.0f);
        QCOMPARE(r, 0.0f);

        auto [l2, r2] = panToGains(1.0f, 5.0f);
        QCOMPARE(l2, 0.0f);
        QCOMPARE(r2, 1.0f);
    }

    void volumeClampsToNonNegative() {
        auto [l, r] = panToGains(-1.0f, 0.0f);
        QCOMPARE(l, 0.0f);
        QCOMPARE(r, 0.0f);
    }
};

QTEST_MAIN(PanLawMathTests)
#include "test_PanLawMath.moc"
