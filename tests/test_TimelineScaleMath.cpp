#include <QTest>

#include "ui/TimelineScaleMath.h"

using rsd::computeTimelineScale;
using rsd::resolveDragLockSamples;

namespace {
constexpr int64_t kFloor = 44100 * 30;
constexpr int64_t kHeadroom = 44100 * 10;
} // namespace

class TimelineScaleMathTests : public QObject {
    Q_OBJECT

private slots:
    void zoomAction_recapturesBaselineFromCurrentContent() {
        auto r = computeTimelineScale(44100 * 20, kFloor, kHeadroom, 2.0f, /*pinnedBase=*/0,
                                       /*recapture=*/true);
        QCOMPARE(r.pinnedBaseSamples, kFloor); // content(20s)+headroom(10s) < floor(30s)
        QCOMPARE(r.totalSamples, kFloor / 2);
    }

    void nonZoomRefresh_withContentWithinPinnedBase_keepsZoomStable() {
        // Simulates: user zoomed in (pinned base captured at some larger
        // value), then drags a clip that doesn't push content past that
        // baseline — the visible total must not change.
        int64_t pinnedBase = 44100 * 100;
        auto r = computeTimelineScale(44100 * 20, kFloor, kHeadroom, 4.0f, pinnedBase,
                                       /*recapture=*/false);
        QCOMPARE(r.pinnedBaseSamples, pinnedBase);
        QCOMPARE(r.totalSamples, pinnedBase / 4);
    }

    void nonZoomRefresh_contentExceedsPinnedBase_growsToFit() {
        int64_t pinnedBase = 44100 * 50;
        int64_t farClipEnd = 44100 * 200; // clip dragged well past the pinned baseline
        auto r = computeTimelineScale(farClipEnd, kFloor, kHeadroom, 4.0f, pinnedBase,
                                       /*recapture=*/false);
        int64_t expectedBase = farClipEnd + kHeadroom;
        QCOMPARE(r.pinnedBaseSamples, expectedBase);
        QCOMPARE(r.totalSamples, expectedBase / 4);
    }

    void zoomFactorOfOne_totalEqualsBase() {
        auto r = computeTimelineScale(0, kFloor, kHeadroom, 1.0f, 0, true);
        QCOMPARE(r.totalSamples, kFloor);
    }

    void totalNeverGoesBelowOne() {
        auto r = computeTimelineScale(0, 1, 0, 1000.0f, 0, true);
        QCOMPARE(r.totalSamples, static_cast<int64_t>(1));
    }

    void dragLock_prefersSharedScaleOverLocalEstimate() {
        // Regression test: before this fix, drag-start locked the LOCAL
        // per-track scale instead of the shared one already on screen,
        // so the clip visibly jumped/resized the instant a drag began.
        int64_t shared = 44100 * 200; // e.g. zoomed out to fit a longer track elsewhere
        int64_t local = 44100 * 40;   // this track's own (much shorter) content
        QCOMPARE(resolveDragLockSamples(shared, local), shared);
    }

    void dragLock_fallsBackToLocalWhenNoSharedScaleYet() {
        QCOMPARE(resolveDragLockSamples(0, 44100 * 40), static_cast<int64_t>(44100 * 40));
    }
};

QTEST_MAIN(TimelineScaleMathTests)
#include "test_TimelineScaleMath.moc"
