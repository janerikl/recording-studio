#include <QTest>

#include "audio/PreviewPlaybackMath.h"

using namespace rsd;

class PreviewPlaybackMathTests : public QObject {
    Q_OBJECT

private slots:
    void notFinishedBeforeEnd() {
        QVERIFY(!isPreviewFinished(0, 1000));
        QVERIFY(!isPreviewFinished(999, 1000));
    }

    void finishedExactlyAtEnd() {
        QVERIFY(isPreviewFinished(1000, 1000));
    }

    void finishedPastEnd() {
        QVERIFY(isPreviewFinished(1500, 1000));
    }

    void zeroLengthIsAlreadyFinished() {
        QVERIFY(isPreviewFinished(0, 0));
    }

    void negativeLengthIsTreatedAsFinished() {
        QVERIFY(isPreviewFinished(0, -1));
    }
};

QTEST_MAIN(PreviewPlaybackMathTests)
#include "test_PreviewPlaybackMath.moc"
