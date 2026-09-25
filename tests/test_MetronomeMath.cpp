#include <QTest>

#include "audio/MetronomeMath.h"

using namespace rsd;

class MetronomeMathTests : public QObject {
    Q_OBJECT

private slots:
    void beatDurationAt120Bpm48kHz() {
        // 120 BPM = 2 beats/sec = 24000 samples/beat at 48kHz.
        QCOMPARE(beatDurationSamples(120.0, 48000), 24000.0);
    }

    void tickAtBlockStart() {
        auto offset = firstClickOffsetInBlock(0, 512, 24000.0);
        QVERIFY(offset.has_value());
        QCOMPARE(*offset, 0);
    }

    void tickMidBlock() {
        // Beat boundary at sample 24000; block [23800, 24312) contains it.
        auto offset = firstClickOffsetInBlock(23800, 512, 24000.0);
        QVERIFY(offset.has_value());
        QCOMPARE(*offset, 200);
    }

    void noTickInBlock() {
        // Block [1000, 1512) is well within the first beat (next at 24000).
        auto offset = firstClickOffsetInBlock(1000, 512, 24000.0);
        QVERIFY(!offset.has_value());
    }

    void zeroBpmIsSafe() { QVERIFY(!firstClickOffsetInBlock(0, 512, 0.0).has_value()); }
};

QTEST_MAIN(MetronomeMathTests)
#include "test_MetronomeMath.moc"
