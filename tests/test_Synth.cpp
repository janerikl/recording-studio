#include <QTest>

#include "audio/Synth.h"

using namespace rsd;

namespace {
// FluidSynth's release/fade tails asymptotically approach zero rather than
// hitting it exactly (float noise floor), so "silent" means below an
// inaudible threshold, not bit-exact zero.
bool anyNonZero(const std::vector<float>& buf) {
    constexpr float kSilenceThreshold = 1e-5f;
    for (float v : buf) {
        if (std::abs(v) > kSilenceThreshold) return true;
    }
    return false;
}
} // namespace

// Integration-level (not pure math): exercises the real bundled SoundFont
// via FluidSynth, same as SessionMixer's tests use real Session/Track
// objects rather than mocking the DSP. Requires assets/soundfonts/
// TimGM6mb.sf2 to be resolvable via RSD_SOURCE_DIR (set by CMake).
class SynthTests : public QObject {
    Q_OBJECT

private slots:
    void silentBeforeAnyNote() {
        SynthEngine engine;
        SynthParams params;
        std::vector<float> out(512 * 2, 0.0f);
        engine.render(out.data(), 512, 2, params);
        QVERIFY(!anyNonZero(out));
    }

    void noteOnProducesAudio() {
        SynthEngine engine;
        SynthParams params;
        engine.noteOn(60, 1.0f, 48000.0f);
        std::vector<float> out(512 * 2, 0.0f);
        engine.render(out.data(), 512, 2, params);
        QVERIFY(anyNonZero(out));
    }

    void noteOffEventuallySilences() {
        SynthEngine engine;
        SynthParams params;
        engine.noteOn(60, 1.0f, 48000.0f);
        std::vector<float> out(512 * 2, 0.0f);
        engine.render(out.data(), 512, 2, params);
        engine.noteOff(60);
        // Drain well past any release tail.
        for (int i = 0; i < 200; ++i) {
            std::fill(out.begin(), out.end(), 0.0f);
            engine.render(out.data(), 512, 2, params);
        }
        QVERIFY(!anyNonZero(out));
    }

    void resetSilencesImmediately() {
        SynthEngine engine;
        SynthParams params;
        engine.noteOn(60, 1.0f, 48000.0f);
        std::vector<float> out(512 * 2, 0.0f);
        engine.render(out.data(), 512, 2, params);
        engine.reset();
        std::fill(out.begin(), out.end(), 0.0f);
        engine.render(out.data(), 512, 2, params);
        QVERIFY(!anyNonZero(out));
    }

    void drumKitProducesAudio() {
        SynthEngine engine;
        SynthParams params;
        params.isDrumKit.store(true);
        engine.noteOn(36, 1.0f, 48000.0f); // Kick
        std::vector<float> out(512 * 2, 0.0f);
        engine.render(out.data(), 512, 2, params);
        QVERIFY(anyNonZero(out));
    }
};

QTEST_MAIN(SynthTests)
#include "test_Synth.moc"
