#include <QTest>

#include "ui/MidiNoteDisplayMath.h"

using rsd::noteRowHeight;
using rsd::pitchToY;

class MidiNoteDisplayMathTests : public QObject {
    Q_OBJECT

private slots:
    void highestPitchIsAtTop() { QCOMPARE(pitchToY(96, 100, 36, 96), 0); }

    void lowestPitchIsAtBottom() { QCOMPARE(pitchToY(36, 100, 36, 96), 100); }

    void midpointPitchIsVerticalCenter() { QCOMPARE(pitchToY(66, 100, 36, 96), 50); }

    void pitchAboveRangeClampsToTop() { QCOMPARE(pitchToY(120, 100, 36, 96), 0); }

    void pitchBelowRangeClampsToBottom() { QCOMPARE(pitchToY(10, 100, 36, 96), 100); }

    void rowHeightScalesWithLaneAndRange() {
        // 60 tall lane, 60-note range (36..96) -> 1px per semitone.
        QCOMPARE(noteRowHeight(60, 36, 96), 1);
        // 120 tall lane, same range -> 2px per semitone.
        QCOMPARE(noteRowHeight(120, 36, 96), 2);
    }

    void rowHeightClampsToAtLeastOnePixel() {
        // A tiny lane would compute to 0px; must clamp up so notes stay visible.
        QCOMPARE(noteRowHeight(5, 36, 96), 1);
    }

    void rowHeightIsZeroForDegenerateRange() { QCOMPARE(noteRowHeight(100, 96, 36), 0); }
};

QTEST_MAIN(MidiNoteDisplayMathTests)
#include "test_MidiNoteDisplayMath.moc"
