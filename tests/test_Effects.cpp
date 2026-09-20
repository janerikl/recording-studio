#include <QtTest>
#include <cmath>
#include <vector>

#include "audio/Effects.h"

using namespace rsd;

namespace {

std::vector<float> makeSine(unsigned int nFrames, float freq, double sampleRate, float amplitude = 1.0f) {
    std::vector<float> buf(nFrames);
    for (unsigned int i = 0; i < nFrames; ++i) {
        buf[i] = amplitude * std::sin(2.0 * M_PI * freq * static_cast<double>(i) / sampleRate);
    }
    return buf;
}

float rms(const std::vector<float>& v, size_t skip) {
    double sum = 0.0;
    size_t n = 0;
    for (size_t i = skip; i < v.size(); ++i) {
        sum += static_cast<double>(v[i]) * v[i];
        ++n;
    }
    return n > 0 ? static_cast<float>(std::sqrt(sum / n)) : 0.0f;
}

} // namespace

class TestEffects : public QObject {
    Q_OBJECT

private slots:
    // --- EqEffect -----------------------------------------------------

    void eqWithZeroGainLeavesSignalApproximatelyUnchanged() {
        EqEffect eq;
        eq.prepare(48000.0);
        auto input = makeSine(2000, 1000.0f, 48000.0, 0.5f);
        auto processed = input;
        eq.process(processed.data(), static_cast<unsigned int>(processed.size()), 1);

        // Skip the filter's transient settling region.
        float inRms = rms(input, 500);
        float outRms = rms(processed, 500);
        QVERIFY(std::abs(inRms - outRms) < 0.02f);
    }

    void eqMidBoostIncreasesEnergyAtMidFrequency() {
        EqEffect eq;
        eq.midFreqHz.store(1000.0f);
        eq.midGainDb.store(12.0f);
        eq.prepare(48000.0);
        auto input = makeSine(2000, 1000.0f, 48000.0, 0.2f);
        auto processed = input;
        eq.process(processed.data(), static_cast<unsigned int>(processed.size()), 1);

        QVERIFY(rms(processed, 500) > rms(input, 500) * 1.5f);
    }

    void eqMidCutDecreasesEnergyAtMidFrequency() {
        EqEffect eq;
        eq.midFreqHz.store(1000.0f);
        eq.midGainDb.store(-12.0f);
        eq.prepare(48000.0);
        auto input = makeSine(2000, 1000.0f, 48000.0, 0.5f);
        auto processed = input;
        eq.process(processed.data(), static_cast<unsigned int>(processed.size()), 1);

        QVERIFY(rms(processed, 500) < rms(input, 500) * 0.7f);
    }

    void bypassedEffectLeavesBufferUntouched() {
        EqEffect eq;
        eq.midGainDb.store(12.0f);
        eq.bypassed.store(true);
        eq.prepare(48000.0);
        auto input = makeSine(100, 1000.0f, 48000.0, 0.5f);
        auto processed = input;

        EffectChain chain{std::make_shared<EqEffect>()};
        chain[0]->bypassed.store(true);
        processEffectChain(chain, processed.data(), static_cast<unsigned int>(processed.size()), 1);
        QCOMPARE(processed, input);
    }

    // --- CompressorEffect -----------------------------------------------

    void compressorReducesGainAboveThreshold() {
        CompressorEffect comp;
        comp.thresholdDb.store(-18.0f);
        comp.ratio.store(4.0f);
        comp.attackMs.store(1.0f);
        comp.releaseMs.store(50.0f);
        comp.prepare(48000.0);

        // 0dBFS constant signal, well above the -18dB threshold.
        std::vector<float> buf(4000, 1.0f);
        comp.process(buf.data(), static_cast<unsigned int>(buf.size()), 1);

        // Expect settled output near threshold + (0 - threshold)/ratio =
        // -18 + 18/4 = -13.5dB ~= 0.211 linear.
        float settled = buf.back();
        QVERIFY(settled < 0.9f);       // definitely reduced from unity
        QVERIFY(settled > 0.05f);      // but not silenced
        QVERIFY(std::abs(settled - 0.2113f) < 0.05f);
    }

    void compressorLeavesSignalBelowThresholdUnaffected() {
        CompressorEffect comp;
        comp.thresholdDb.store(-6.0f);
        comp.ratio.store(4.0f);
        comp.attackMs.store(1.0f);
        comp.releaseMs.store(50.0f);
        comp.prepare(48000.0);

        std::vector<float> buf(2000, 0.05f); // well below threshold
        comp.process(buf.data(), static_cast<unsigned int>(buf.size()), 1);

        QVERIFY(std::abs(buf.back() - 0.05f) < 0.001f);
    }

    // --- DelayEffect ------------------------------------------------------

    void delayProducesEchoAtExpectedOffset() {
        DelayEffect delay;
        delay.delayMs.store(10.0f);
        delay.feedback.store(0.0f); // isolate a single echo, no repeats
        delay.mix.store(0.5f);
        delay.prepare(48000.0);

        size_t delaySamples = static_cast<size_t>(0.010 * 48000.0);
        std::vector<float> buf(delaySamples + 2, 0.0f);
        buf[0] = 1.0f;
        delay.process(buf.data(), static_cast<unsigned int>(buf.size()), 1);

        QVERIFY(std::abs(buf[0] - 0.5f) < 0.001f); // dry(1-mix) + wet(0, nothing delayed yet)
        QVERIFY(std::abs(buf[delaySamples] - 0.5f) < 0.001f); // wet echo of the impulse arrives here
    }

    // --- ReverbEffect -------------------------------------------------

    void reverbProducesDecayingTailAfterImpulse() {
        ReverbEffect reverb;
        reverb.mix.store(1.0f); // fully wet, easiest to measure
        reverb.prepare(48000.0);

        std::vector<float> buf(4000, 0.0f);
        buf[0] = 1.0f;
        reverb.process(buf.data(), static_cast<unsigned int>(buf.size()), 1);

        for (float v : buf) QVERIFY(std::isfinite(v));

        bool hasLateEnergy = false;
        for (size_t i = 2000; i < buf.size(); ++i) {
            if (std::abs(buf[i]) > 1e-4f) {
                hasLateEnergy = true;
                break;
            }
        }
        QVERIFY(hasLateEnergy);
    }

    // --- processEffectChain --------------------------------------------

    void chainSkipsBypassedEffectsButRunsOthers() {
        auto eq = std::make_shared<EqEffect>();
        eq->bypassed.store(true);
        eq->midGainDb.store(20.0f);
        eq->prepare(48000.0);

        auto comp = std::make_shared<CompressorEffect>();
        comp->thresholdDb.store(-60.0f); // always engaged
        comp->ratio.store(20.0f);
        comp->attackMs.store(1.0f);
        comp->prepare(48000.0);

        EffectChain chain{eq, comp};
        std::vector<float> buf(2000, 1.0f);
        processEffectChain(chain, buf.data(), static_cast<unsigned int>(buf.size()), 1);

        // The bypassed EQ shouldn't stop the compressor from heavily
        // reducing this loud constant signal.
        QVERIFY(buf.back() < 0.5f);
    }
};

QTEST_APPLESS_MAIN(TestEffects)
#include "test_Effects.moc"
