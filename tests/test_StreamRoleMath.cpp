#include <QTest>

#include "audio/StreamRoleMath.h"
#include "audio/TransportClock.h"

using rsd::inputStreamShouldCapture;
using rsd::outputStreamShouldMix;
using rsd::TransportState;

class StreamRoleMathTests : public QObject {
    Q_OBJECT

private slots:
    // Exactly one of the two streams should ever be "active" for a given
    // transport state, and never both — that's what prevents the split
    // output/input streams from double-advancing the transport clock or
    // (for output) playing back audio while Recording.
    void exactlyOneStreamActivePerState() {
        for (TransportState state :
             {TransportState::Stopped, TransportState::Playing, TransportState::Recording}) {
            bool bothActive = outputStreamShouldMix(state) && inputStreamShouldCapture(state);
            QVERIFY(!bothActive);
        }
    }

    void outputMixesOnlyWhilePlaying() {
        QVERIFY(outputStreamShouldMix(TransportState::Playing));
        QVERIFY(!outputStreamShouldMix(TransportState::Recording));
        QVERIFY(!outputStreamShouldMix(TransportState::Stopped));
    }

    void inputCapturesOnlyWhileRecording() {
        QVERIFY(inputStreamShouldCapture(TransportState::Recording));
        QVERIFY(!inputStreamShouldCapture(TransportState::Playing));
        QVERIFY(!inputStreamShouldCapture(TransportState::Stopped));
    }
};

QTEST_MAIN(StreamRoleMathTests)
#include "test_StreamRoleMath.moc"
