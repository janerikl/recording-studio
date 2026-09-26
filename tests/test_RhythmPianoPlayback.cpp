#include <QTest>

#include "audio/RhythmClickTrack.h"
#include "model/RhythmPattern.h"

using namespace rsd;

namespace {
// Same rationale as test_Synth.cpp: FluidSynth's release tail asymptotes
// toward zero rather than hitting it exactly.
bool anyNonZero(const AudioBuffer& buf, int64_t fromFrame, int64_t toFrame) {
    constexpr float kSilenceThreshold = 1e-5f;
    for (int64_t f = fromFrame; f < toFrame && f < buf.frameCount(); ++f) {
        for (int ch = 0; ch < buf.channels; ++ch) {
            if (std::abs(buf.samples[static_cast<size_t>(f) * buf.channels + ch]) > kSilenceThreshold) return true;
        }
    }
    return false;
}
} // namespace

// Integration-level (real bundled SoundFont via FluidSynth), same style as
// test_Synth.cpp/test_RhythmClickTrack.cpp.
class RhythmPianoPlaybackTests : public QObject {
    Q_OBJECT

private slots:
    void audioShortlyAfterFirstOnset() {
        RhythmPattern p{"Test", {{1.0, false}}};
        auto buffer = renderRhythmPianoBuffer(p, 120.0, 48000, 2);
        // A few ms in, well within the note's sustain.
        QVERIFY(anyNonZero(*buffer, 500, 600));
    }

    void silenceBeforeNoteAfterLeadingRest() {
        // Rest for 1 beat @ 120bpm = 0.5s = 24000 frames, then a note.
        RhythmPattern p{"Test", {{1.0, true}, {1.0, false}}};
        auto buffer = renderRhythmPianoBuffer(p, 120.0, 48000, 2);
        QVERIFY(!anyNonZero(*buffer, 0, 1000));
    }

    void bufferCoversPatternDurationPlusReleaseTail() {
        RhythmPattern p{"Test", {{1.0, false}}}; // 0.5s @ 120bpm
        auto buffer = renderRhythmPianoBuffer(p, 120.0, 48000, 2);
        QVERIFY(buffer->frameCount() > 24000); // longer than the bare pattern duration
    }
};

QTEST_MAIN(RhythmPianoPlaybackTests)
#include "test_RhythmPianoPlayback.moc"
