#include <QTest>

#include "ui/ClipLaneScrollMath.h"

using rsd::clampScrollOffset;
using rsd::maxScrollOffsetSamples;
using rsd::scrollbarShouldBeEnabled;

class ClipLaneScrollMathTests : public QObject {
    Q_OBJECT

private slots:
    void maxOffset_zeroWhenContentFitsVisibleWindow() {
        QCOMPARE(maxScrollOffsetSamples(1000, 2000), static_cast<int64_t>(0));
        QCOMPARE(maxScrollOffsetSamples(2000, 2000), static_cast<int64_t>(0));
    }

    void maxOffset_positiveWhenContentExceedsVisibleWindow() {
        QCOMPARE(maxScrollOffsetSamples(5000, 2000), static_cast<int64_t>(3000));
    }

    void clamp_neverGoesNegative() {
        QCOMPARE(clampScrollOffset(-500, 5000, 2000), static_cast<int64_t>(0));
    }

    void clamp_neverExceedsMax() {
        QCOMPARE(clampScrollOffset(100000, 5000, 2000), static_cast<int64_t>(3000));
    }

    void clamp_passesThroughValidValue() {
        QCOMPARE(clampScrollOffset(1500, 5000, 2000), static_cast<int64_t>(1500));
    }

    void clamp_zeroWhenContentFitsRegardlessOfRequest() {
        // Content shrank (e.g. clips were deleted) below what was previously
        // scrolled to — must snap back into range, never leave the window
        // showing empty space past the content.
        QCOMPARE(clampScrollOffset(3000, 1000, 2000), static_cast<int64_t>(0));
    }

    void scrollbarEnabled_falseWhenContentFits() {
        QVERIFY(!scrollbarShouldBeEnabled(1000, 2000));
        QVERIFY(!scrollbarShouldBeEnabled(2000, 2000));
    }

    void scrollbarEnabled_trueWhenContentExceedsWindow() {
        QVERIFY(scrollbarShouldBeEnabled(5000, 2000));
    }
};

QTEST_MAIN(ClipLaneScrollMathTests)
#include "test_ClipLaneScrollMath.moc"
