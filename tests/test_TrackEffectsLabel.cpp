#include <QTest>

#include "ui/TrackEffectsLabel.h"

using rsd::formatEffectsButtonLabel;

class TrackEffectsLabelTests : public QObject {
    Q_OBJECT

private slots:
    void zeroEffects_showsPlainFx() {
        QCOMPARE(QString::fromStdString(formatEffectsButtonLabel(0)), QString("FX"));
    }

    void oneEffect_showsCount() {
        QCOMPARE(QString::fromStdString(formatEffectsButtonLabel(1)), QString("FX: 1"));
    }

    void multipleEffects_showsCount() {
        QCOMPARE(QString::fromStdString(formatEffectsButtonLabel(4)), QString("FX: 4"));
    }
};

QTEST_MAIN(TrackEffectsLabelTests)
#include "test_TrackEffectsLabel.moc"
