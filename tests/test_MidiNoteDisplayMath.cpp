#include <QTest>

#include "ui/MidiNoteDisplayMath.h"

using rsd::pitchToY;

class MidiNoteDisplayMathTests : public QObject {
    Q_OBJECT

private slots:
    void highestPitchIsAtTop() { QCOMPARE(pitchToY(96, 100, 36, 96), 0); }

    void lowestPitchIsAtBottom() { QCOMPARE(pitchToY(36, 100, 36, 96), 100); }

    void midpointPitchIsVerticalCenter() { QCOMPARE(pitchToY(66, 100, 36, 96), 50); }

    void pitchAboveRangeClampsToTop() { QCOMPARE(pitchToY(120, 100, 36, 96), 0); }

    void pitchBelowRangeClampsToBottom() { QCOMPARE(pitchToY(10, 100, 36, 96), 100); }
};

QTEST_MAIN(MidiNoteDisplayMathTests)
#include "test_MidiNoteDisplayMath.moc"
