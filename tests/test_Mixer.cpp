#include <QtTest>
#include <cmath>

#include "audio/Mixer.h"
#include "model/Clip.h"

using namespace rsd;

namespace {

std::shared_ptr<Clip> makeConstantClip(int64_t lengthSamples, float value, int channels = 1) {
    auto clip = std::make_shared<Clip>();
    clip->buffer = std::make_shared<AudioBuffer>();
    clip->buffer->channels = channels;
    clip->buffer->sampleRate = 48000;
    clip->buffer->samples.assign(static_cast<size_t>(lengthSamples * channels), value);
    clip->sessionStartSample = 0;
    clip->sourceOffsetSamples = 0;
    clip->lengthSamples = lengthSamples;
    return clip;
}

} // namespace

class TestMixer : public QObject {
    Q_OBJECT

private slots:
    // --- fadeMultiplier -----------------------------------------------

    void linearFadeStartsAtZeroAndRampsToOne() {
        QCOMPARE(fadeMultiplier(0, 100, FadeCurve::Linear), 0.0f);
        QVERIFY(qFuzzyCompare(fadeMultiplier(50, 100, FadeCurve::Linear), 0.5f));
        QVERIFY(fadeMultiplier(99, 100, FadeCurve::Linear) > 0.9f);
    }

    void equalPowerFadeStartsAtZeroAndRampsToOne() {
        QCOMPARE(fadeMultiplier(0, 100, FadeCurve::EqualPower), 0.0f);
        // sin(pi/2 * 0.5) ~= 0.707, distinctly different from the linear 0.5.
        float mid = fadeMultiplier(50, 100, FadeCurve::EqualPower);
        QVERIFY(mid > 0.65f && mid < 0.75f);
    }

    void fadeMultiplierWithZeroLengthIsFullyOpen() {
        // A clip with no fade configured must not attenuate anything.
        QCOMPARE(fadeMultiplier(0, 0, FadeCurve::Linear), 1.0f);
    }

    void fadeMultiplierIsMonotonicIncreasing() {
        float prev = -1.0f;
        for (int64_t pos = 0; pos < 100; pos += 10) {
            float g = fadeMultiplier(pos, 100, FadeCurve::EqualPower);
            QVERIFY(g >= prev);
            prev = g;
        }
    }

    // --- mixClipInto: regression (no fades/gain=1, matches old behavior) --

    void mixClipIntoWithNoFadesAppliesPlainGain() {
        auto clip = makeConstantClip(4, 1.0f);
        std::vector<float> out(4, 0.0f);
        mixClipInto(out.data(), 4, 1, 0, *clip, 0.5f, 0.5f);
        for (float v : out) QCOMPARE(v, 0.5f);
    }

    void mixClipIntoSkipsMutedClip() {
        auto clip = makeConstantClip(4, 1.0f);
        clip->muted = true;
        std::vector<float> out(4, 0.0f);
        mixClipInto(out.data(), 4, 1, 0, *clip, 1.0f, 1.0f);
        for (float v : out) QCOMPARE(v, 0.0f);
    }

    // --- mixClipInto: per-clip gain ------------------------------------

    void mixClipIntoAppliesPerClipGainMultiplicatively() {
        auto clip = makeConstantClip(4, 1.0f);
        clip->gain = 0.5f;
        std::vector<float> out(4, 0.0f);
        mixClipInto(out.data(), 4, 1, 0, *clip, 0.5f, 0.5f); // track gain 0.5 * clip gain 0.5
        for (float v : out) QVERIFY(qFuzzyCompare(v, 0.25f));
    }

    // --- mixClipInto: fade in/out ---------------------------------------

    void mixClipIntoRampsUpDuringFadeIn() {
        auto clip = makeConstantClip(10, 1.0f);
        clip->fadeInSamples = 10;
        std::vector<float> out(10, 0.0f);
        mixClipInto(out.data(), 10, 1, 0, *clip, 1.0f, 1.0f);
        QCOMPARE(out.front(), 0.0f);
        for (size_t i = 1; i < out.size(); ++i) QVERIFY(out[i] >= out[i - 1]);
        QVERIFY(out.back() > 0.5f);
    }

    void mixClipIntoRampsDownDuringFadeOut() {
        auto clip = makeConstantClip(10, 1.0f);
        clip->fadeOutSamples = 10;
        std::vector<float> out(10, 0.0f);
        mixClipInto(out.data(), 10, 1, 0, *clip, 1.0f, 1.0f);
        for (size_t i = 1; i < out.size(); ++i) QVERIFY(out[i] <= out[i - 1]);
        QVERIFY(out.front() > 0.5f);
    }

    void mixClipIntoUnaffectedOutsideFadeRegion() {
        // A short fade at the very start/end shouldn't touch samples in the
        // untouched middle of a longer clip.
        auto clip = makeConstantClip(100, 1.0f);
        clip->fadeInSamples = 5;
        clip->fadeOutSamples = 5;
        std::vector<float> out(100, 0.0f);
        mixClipInto(out.data(), 100, 1, 0, *clip, 1.0f, 1.0f);
        QCOMPARE(out[50], 1.0f);
    }

    // --- crossfade sanity: two overlapping clips with complementary fades

    void overlappingComplementaryFadesSumNearConstantLoudness() {
        auto clipA = makeConstantClip(20, 1.0f);
        clipA->fadeOutSamples = 20; // fades out over its whole length
        clipA->fadeOutCurve = FadeCurve::EqualPower;
        auto clipB = makeConstantClip(20, 1.0f);
        clipB->sessionStartSample = 0; // fully overlapping with A for this check
        clipB->fadeInSamples = 20;     // fades in over its whole length
        clipB->fadeInCurve = FadeCurve::EqualPower;

        std::vector<float> out(20, 0.0f);
        mixClipInto(out.data(), 20, 1, 0, *clipA, 1.0f, 1.0f);
        mixClipInto(out.data(), 20, 1, 0, *clipB, 1.0f, 1.0f);

        // Equal-power crossfade keeps combined *power* roughly constant; for
        // two identical (fully correlated) signals that means amplitude
        // never dips below either endpoint's level (it may bulge above it in
        // the middle — that's expected and different from a dip/silence gap,
        // which is the failure mode this guards against).
        for (float v : out) QVERIFY(v > 0.9f);
    }

    void equalPowerFadeKeepsCombinedPowerConstant() {
        // The defining property of an equal-power curve: a fade-out value at
        // position t and a fade-in value at the mirrored position (len-1-t)
        // should have squares summing to ~1, independent of clip content.
        const int64_t len = 1000; // fine enough resolution to bound discretization error
        for (int64_t t = 0; t < len; t += 100) {
            float gOut = fadeMultiplier(t, len, FadeCurve::EqualPower);
            float gIn = fadeMultiplier(len - 1 - t, len, FadeCurve::EqualPower);
            QVERIFY(std::abs(gOut * gOut + gIn * gIn - 1.0f) < 0.05f);
        }
    }
};

QTEST_APPLESS_MAIN(TestMixer)
#include "test_Mixer.moc"
