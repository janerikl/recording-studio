#include <QTest>

#include "ui/ClipLaneScrollMath.h"

using rsd::clampScrollOffset;
using rsd::maxScrollOffsetSamples;
using rsd::scrollbarShouldBeEnabled;
using rsd::scrollOffsetToRevealPlayhead;

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

    void reveal_noChangeWhenPlayheadAlreadyVisible() {
        // Window is [1000, 3000); playhead at 2000 is comfortably inside it.
        QCOMPARE(scrollOffsetToRevealPlayhead(2000, 1000, 2000), static_cast<int64_t>(1000));
    }

    void reveal_scrollsForwardWhenPlayheadPastRightEdge() {
        // Window is [0, 2000); playhead at 5000 is past the right edge —
        // scroll so it sits 5% in from the edge (margin = 100).
        int64_t result = scrollOffsetToRevealPlayhead(5000, 0, 2000);
        QCOMPARE(result, static_cast<int64_t>(5000 - 2000 + 100));
    }

    void reveal_scrollsBackwardWhenPlayheadBeforeLeftEdge() {
        // This is the reported bug: after playback ran far forward (scroll
        // offset stuck at 50000), seeking back to sample 0 must scroll the
        // view back to reveal it, not leave the lane showing empty space.
        int64_t result = scrollOffsetToRevealPlayhead(0, 50000, 2000);
        QCOMPARE(result, static_cast<int64_t>(0));
    }

    void reveal_backwardJumpNeverGoesNegative() {
        int64_t result = scrollOffsetToRevealPlayhead(50, 50000, 2000);
        QVERIFY(result >= 0);
    }

    void reveal_playheadExactlyAtLeftEdgeCountsAsVisible() {
        QCOMPARE(scrollOffsetToRevealPlayhead(1000, 1000, 2000), static_cast<int64_t>(1000));
    }
};

QTEST_MAIN(ClipLaneScrollMathTests)
#include "test_ClipLaneScrollMath.moc"
