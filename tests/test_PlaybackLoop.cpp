#include <QtTest>

#include "audio/TransportClock.h"

using namespace rsd;

class TestPlaybackLoop : public QObject {
    Q_OBJECT

private slots:
    void doesNotWrapWhenLoopDisabled() {
        TransportClock clock;
        clock.setPlaybackLoop(100, 200);
        clock.setPlaybackLoopEnabled(false);
        clock.setPositionSamples(190);

        clock.advance(20); // would cross loop end at 200
        QCOMPARE(clock.positionSamples(), int64_t(210));
    }

    void wrapsToLoopStartWhenEnabledAndPastLoopEnd() {
        TransportClock clock;
        clock.setPlaybackLoop(100, 200);
        clock.setPlaybackLoopEnabled(true);
        clock.setPositionSamples(190);

        clock.advance(20); // crosses loop end (200) -> wraps
        QCOMPARE(clock.positionSamples(), int64_t(100));
    }

    void ignoresLoopWhenEndNotAfterStart() {
        TransportClock clock;
        clock.setPlaybackLoop(200, 200); // degenerate/invalid region
        clock.setPlaybackLoopEnabled(true);
        clock.setPositionSamples(190);

        clock.advance(20);
        QCOMPARE(clock.positionSamples(), int64_t(210));
    }

    void punchLoopTakesPrecedenceOverPlaybackLoop() {
        TransportClock clock;
        clock.setPunchRegion({0, 50});
        clock.setPunchLoopEnabled(true);
        clock.setPlaybackLoop(1000, 2000);
        clock.setPlaybackLoopEnabled(true);
        clock.setPositionSamples(45);

        clock.advance(10); // crosses punch region end (50) first
        QCOMPARE(clock.positionSamples(), int64_t(0));
    }
};

QTEST_APPLESS_MAIN(TestPlaybackLoop)
#include "test_PlaybackLoop.moc"
