#include <QtTest/QtTest>

#include "ui/MediaPreviewToggleMath.h"

using namespace rsd;

class MediaPreviewToggleMathTests : public QObject {
    Q_OBJECT

private slots:
    void clickingNewRowStartsPreview() {
        QCOMPARE(nextPreviewIndex(0, -1), 0);
        QCOMPARE(nextPreviewIndex(2, 0), 2);
    }

    void clickingSameRowStopsPreview() {
        QCOMPARE(nextPreviewIndex(1, 1), -1);
    }

    void clickingDifferentRowSwitchesPreview() {
        QCOMPARE(nextPreviewIndex(3, 1), 3);
    }
};

QTEST_APPLESS_MAIN(MediaPreviewToggleMathTests)
#include "test_MediaPreviewToggleMath.moc"
