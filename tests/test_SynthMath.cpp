#include <QTest>

#include "audio/SynthMath.h"

using namespace rsd;

namespace {
bool nearlyEqual(float a, float b, float eps = 0.001f) { return std::abs(a - b) < eps; }
} // namespace

class SynthMathTests : public QObject {
    Q_OBJECT

private slots:
    void sineAtZeroPhaseIsZero() { QVERIFY(nearlyEqual(oscillatorSample(SynthWaveform::Sine, 0.0f), 0.0f)); }

    void sineAtQuarterPhaseIsPeak() {
        QVERIFY(nearlyEqual(oscillatorSample(SynthWaveform::Sine, 0.25f), 1.0f));
    }

    void sawRampsFromMinusOneToOne() {
        QVERIFY(nearlyEqual(oscillatorSample(SynthWaveform::Saw, 0.0f), -1.0f));
        QVERIFY(nearlyEqual(oscillatorSample(SynthWaveform::Saw, 0.5f), 0.0f));
    }

    void squareIsHighThenLow() {
        QCOMPARE(oscillatorSample(SynthWaveform::Square, 0.1f), 1.0f);
        QCOMPARE(oscillatorSample(SynthWaveform::Square, 0.6f), -1.0f);
    }

    void triangleWrapsAtMidpoint() {
        QVERIFY(nearlyEqual(oscillatorSample(SynthWaveform::Triangle, 0.0f), -1.0f));
        QVERIFY(nearlyEqual(oscillatorSample(SynthWaveform::Triangle, 0.25f), 0.0f));
        QVERIFY(nearlyEqual(oscillatorSample(SynthWaveform::Triangle, 0.5f), 1.0f));
    }

    void phaseWrapsBeyondOne() {
        QVERIFY(nearlyEqual(oscillatorSample(SynthWaveform::Saw, 1.25f), oscillatorSample(SynthWaveform::Saw, 0.25f)));
    }

    void phaseIncrementScalesWithFrequency() {
        QVERIFY(nearlyEqual(phaseIncrement(440.0f, 44100.0f), 440.0f / 44100.0f));
    }

    void phaseIncrementZeroSampleRateIsSafe() { QCOMPARE(phaseIncrement(440.0f, 0.0f), 0.0f); }

    void middleCIs261Hz() { QVERIFY(nearlyEqual(midiNoteToFrequencyHz(60), 261.63f, 0.1f)); }

    void a440IsExactlyStandardPitch() { QVERIFY(nearlyEqual(midiNoteToFrequencyHz(69), 440.0f, 0.01f)); }

    void attackRampsLinearlyToOne() {
        ADSRParams p;
        p.attackSeconds = 1.0f;
        QCOMPARE(attackLevel(0.0f, p), 0.0f);
        QVERIFY(nearlyEqual(attackLevel(0.5f, p), 0.5f));
        QCOMPARE(attackLevel(1.0f, p), 1.0f);
        QCOMPARE(attackLevel(5.0f, p), 1.0f); // clamped, doesn't overshoot
    }

    void zeroAttackTimeIsInstant() {
        ADSRParams p;
        p.attackSeconds = 0.0f;
        QCOMPARE(attackLevel(0.0f, p), 1.0f);
    }

    void decayRampsFromOneToSustain() {
        ADSRParams p;
        p.decaySeconds = 1.0f;
        p.sustainLevel = 0.5f;
        QCOMPARE(decayLevel(0.0f, p), 1.0f);
        QVERIFY(nearlyEqual(decayLevel(1.0f, p), 0.5f));
    }

    void releaseRampsFromStartLevelToZero() {
        ADSRParams p;
        p.releaseSeconds = 1.0f;
        QCOMPARE(releaseLevel(0.8f, 0.0f, p), 0.8f);
        QVERIFY(nearlyEqual(releaseLevel(0.8f, 1.0f, p), 0.0f));
        QVERIFY(nearlyEqual(releaseLevel(0.8f, 0.5f, p), 0.4f));
    }

    void lowpassCoefficientHigherCutoffIsBrighter() {
        // Higher cutoff -> less smoothing -> smaller coefficient.
        float lowCutoffCoeff = onePoleLowpassCoefficient(200.0f, 44100.0f);
        float highCutoffCoeff = onePoleLowpassCoefficient(15000.0f, 44100.0f);
        QVERIFY(highCutoffCoeff < lowCutoffCoeff);
    }

    void lowpassStepConvergesTowardInput() {
        float state = 0.0f;
        float coeff = onePoleLowpassCoefficient(1000.0f, 44100.0f);
        for (int i = 0; i < 10000; ++i) onePoleLowpassStep(1.0f, state, coeff);
        QVERIFY(nearlyEqual(state, 1.0f, 0.01f));
    }
};

QTEST_MAIN(SynthMathTests)
#include "test_SynthMath.moc"
