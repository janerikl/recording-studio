#include <QTest>

#include "ui/PianoRollEditMath.h"

using namespace rsd;

class PianoRollEditMathTests : public QObject {
    Q_OBJECT

private slots:
    // --- samplesPerBeat / snap grid ---

    void samplesPerBeatAtDefaultTempo() {
        // 120 BPM, 48000 Hz -> 0.5s per beat -> 24000 samples
        QCOMPARE(samplesPerBeat(120.0, 48000), int64_t(24000));
    }

    void samplesPerBeatScalesWithSampleRate() {
        QCOMPARE(samplesPerBeat(120.0, 96000), int64_t(48000));
    }

    void samplesPerBeatHandlesNonDefaultTempo() {
        // 60 BPM -> 1s per beat -> 48000 samples
        QCOMPARE(samplesPerBeat(60.0, 48000), int64_t(48000));
    }

    void snapIntervalSamplesForDenominator() {
        // 1/4 note at 24000 samples/beat = 24000; 1/8 = 12000; 1/16 = 6000
        int64_t beat = 24000;
        QCOMPARE(snapIntervalSamples(beat, 4), int64_t(24000));
        QCOMPARE(snapIntervalSamples(beat, 8), int64_t(12000));
        QCOMPARE(snapIntervalSamples(beat, 16), int64_t(6000));
        QCOMPARE(snapIntervalSamples(beat, 32), int64_t(3000));
    }

    void snapIntervalOffReturnsZero() {
        QCOMPARE(snapIntervalSamples(24000, 0), int64_t(0));
    }

    void snapToGridRoundsToNearestInterval() {
        QCOMPARE(snapToGrid(0, 6000), int64_t(0));
        QCOMPARE(snapToGrid(2999, 6000), int64_t(0));
        QCOMPARE(snapToGrid(3001, 6000), int64_t(6000));
        QCOMPARE(snapToGrid(8999, 6000), int64_t(6000));
        QCOMPARE(snapToGrid(9001, 6000), int64_t(12000));
    }

    void snapToGridWithZeroIntervalIsNoop() {
        QCOMPARE(snapToGrid(12345, 0), int64_t(12345));
    }

    void snapToGridClampsNegativeToZero() {
        QCOMPARE(snapToGrid(-100, 6000), int64_t(0));
    }

    // --- pitch <-> Y (extends MidiNoteDisplayMath::pitchToY) ---

    void yToPitchRoundTripsAtRowCenters() {
        int laneHeight = 61 * 8; // 61 semitones in [36,96], 8px per row
        for (int pitch = 36; pitch <= 96; ++pitch) {
            int y = pitchToRowY(pitch, laneHeight, 36, 96, 8);
            QCOMPARE(yToPitch(y + 4, laneHeight, 36, 96, 8), pitch);
        }
    }

    void yToPitchClampsAboveRange() {
        QCOMPARE(yToPitch(-100, 488, 36, 96, 8), 96);
    }

    void yToPitchClampsBelowRange() {
        QCOMPARE(yToPitch(100000, 488, 36, 96, 8), 36);
    }

    // --- note hit-testing ---

    void hitTestFindsNoteContainingPoint() {
        MidiNoteEditRect r{100, 200, 50, 8}; // x, w, y, h
        QVERIFY(rectContainsPoint(r, 120, 52));
        QVERIFY(!rectContainsPoint(r, 90, 52));
        QVERIFY(!rectContainsPoint(r, 120, 500));
    }

    void hitTestNearRightEdgeIsResizeZone() {
        MidiNoteEditRect r{100, 200, 50, 8};
        QVERIFY(isInResizeZone(r, 296, 8));  // within 8px of right edge (300)
        QVERIFY(!isInResizeZone(r, 250, 8)); // middle of note, not resize zone
    }

    // --- move / resize / draw math ---

    void moveAppliesSampleAndPitchDelta() {
        MidiNote note;
        note.startSample = 10000;
        note.pitch = 60;
        int64_t newStart = applyMoveDeltaSamples(note.startSample, 5000, 0);
        int newPitch = applyMovePitchDelta(note.pitch, -3, 0, 127);
        QCOMPARE(newStart, int64_t(15000));
        QCOMPARE(newPitch, 57);
    }

    void moveClampsStartToNonNegative() {
        QCOMPARE(applyMoveDeltaSamples(1000, -5000, 0), int64_t(0));
    }

    void movePitchClampsToValidRange() {
        QCOMPARE(applyMovePitchDelta(2, -10, 0, 127), 0);
        QCOMPARE(applyMovePitchDelta(125, 10, 0, 127), 127);
    }

    void resizeClampsToMinimumLength() {
        // requestedLengthSamples is the absolute new length (from a drag
        // computing "cursor sample - note start"), clamped to minLength.
        QCOMPARE(resizeLengthSamples(/*requestedLengthSamples=*/500, /*minLength=*/100), int64_t(500));
        QCOMPARE(resizeLengthSamples(50, 100), int64_t(100));
        QCOMPARE(resizeLengthSamples(-500, 100), int64_t(100));
    }

    // --- velocity drag ---

    void velocityClamps() {
        QCOMPARE(clampVelocity(-0.5f), 0.0f);
        QCOMPARE(clampVelocity(1.5f), 1.0f);
        QCOMPARE(clampVelocity(0.42f), 0.42f);
    }

    void velocityAfterVerticalDragFullRangeSpansLaneHeight() {
        // Dragging to the very top of a velocity lane -> full velocity.
        QCOMPARE(velocityFromLaneY(0, 100), 1.0f);
        // Dragging to the very bottom -> zero velocity.
        QCOMPARE(velocityFromLaneY(100, 100), 0.0f);
        QCOMPARE(velocityFromLaneY(50, 100), 0.5f);
    }

    void velocityFromLaneYHandlesZeroHeight() {
        QCOMPARE(velocityFromLaneY(0, 0), 1.0f);
    }
};

QTEST_MAIN(PianoRollEditMathTests)
#include "test_PianoRollEditMath.moc"
