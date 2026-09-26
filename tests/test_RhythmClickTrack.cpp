#include <QTest>

#include "audio/RhythmClickTrack.h"
#include "model/RhythmPattern.h"

using namespace rsd;

namespace {
bool anyNonZero(const AudioBuffer& buf, int64_t fromFrame, int64_t toFrame) {
    for (int64_t f = fromFrame; f < toFrame && f < buf.frameCount(); ++f) {
        for (int ch = 0; ch < buf.channels; ++ch) {
            if (buf.samples[static_cast<size_t>(f) * buf.channels + ch] != 0.0f) return true;
        }
    }
    return false;
}
} // namespace

class RhythmClickTrackTests : public QObject {
    Q_OBJECT

private slots:
    void bufferLengthMatchesPatternDuration() {
        RhythmPattern p{"Test", {{0.5, false}, {0.5, false}}}; // 1 beat total @ 120bpm = 0.5s
        auto buffer = renderRhythmClickBuffer(p, 120.0, 48000, 2);
        QCOMPARE(buffer->sampleRate, 48000);
        QCOMPARE(buffer->channels, 2);
        // 1 beat @ 120bpm = 0.5s = 24000 frames, plus a small tail for the last click's decay.
        QVERIFY(buffer->frameCount() >= 24000);
    }

    void clickAtFirstOnset() {
        RhythmPattern p{"Test", {{1.0, false}}};
        auto buffer = renderRhythmClickBuffer(p, 120.0, 48000, 1);
        QVERIFY(anyNonZero(*buffer, 0, 100));
    }

    void silenceBeforeSecondOnset() {
        // Two quarter notes @ 120bpm: onsets at 0s and 0.5s (24000 frames).
        RhythmPattern p{"Test", {{1.0, false}, {1.0, false}}};
        auto buffer = renderRhythmClickBuffer(p, 120.0, 48000, 1);
        // Well past the first click's short decay, well before the second's onset.
        QVERIFY(!anyNonZero(*buffer, 2000, 23000));
    }

    void restsProduceNoClick() {
        RhythmPattern p{"Test", {{1.0, true}, {1.0, false}}}; // rest, then a note at 0.5s
        auto buffer = renderRhythmClickBuffer(p, 120.0, 48000, 1);
        QVERIFY(!anyNonZero(*buffer, 0, 100));
        QVERIFY(anyNonZero(*buffer, 24000, 24100));
    }
};

QTEST_MAIN(RhythmClickTrackTests)
#include "test_RhythmClickTrack.moc"
