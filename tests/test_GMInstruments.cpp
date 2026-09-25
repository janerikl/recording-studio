#include <QTest>

#include "audio/GMInstruments.h"

using namespace rsd;

class GMInstrumentsTests : public QObject {
    Q_OBJECT

private slots:
    void has128Programs() { QCOMPARE(gmInstrumentNames().size(), 128); }

    void program0IsAcousticGrandPiano() {
        QCOMPARE(gmInstrumentNames()[0], QStringLiteral("Acoustic Grand Piano"));
    }

    void program127IsGunshot() { QCOMPARE(gmInstrumentNames()[127], QStringLiteral("Gunshot")); }

    void noEmptyNames() {
        for (const auto& name : gmInstrumentNames()) QVERIFY(!name.isEmpty());
    }
};

QTEST_MAIN(GMInstrumentsTests)
#include "test_GMInstruments.moc"
