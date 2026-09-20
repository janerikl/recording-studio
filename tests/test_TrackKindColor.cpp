#include <QtTest>

#include "ui/TrackKindColor.h"

using namespace rsd;

class TestTrackKindColor : public QObject {
    Q_OBJECT

private slots:
    void eachTrackKindGetsADistinctColor() {
        QColor audio = trackKindColor(TrackKind::Audio);
        QColor instrument = trackKindColor(TrackKind::Instrument);
        QColor bus = trackKindColor(TrackKind::Bus);

        QVERIFY(audio != instrument);
        QVERIFY(audio != bus);
        QVERIFY(instrument != bus);
    }

    void colorForSameKindIsStable() {
        QCOMPARE(trackKindColor(TrackKind::Audio), trackKindColor(TrackKind::Audio));
        QCOMPARE(trackKindColor(TrackKind::Instrument), trackKindColor(TrackKind::Instrument));
        QCOMPARE(trackKindColor(TrackKind::Bus), trackKindColor(TrackKind::Bus));
    }
};

QTEST_APPLESS_MAIN(TestTrackKindColor)
#include "test_TrackKindColor.moc"
