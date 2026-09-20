#include <QTest>

#include "audio/BusMixMath.h"

using namespace rsd;

class BusMixMathTests : public QObject {
    Q_OBJECT

private slots:
    void sendLevelZeroMutesSend() {
        QCOMPARE(applySend(1.0f, 0.0f), 0.0f);
    }

    void sendLevelFullPassesSignalThrough() {
        QCOMPARE(applySend(0.5f, 1.0f), 0.5f);
    }

    void sendLevelScalesLinearly() {
        QCOMPARE(applySend(1.0f, 0.25f), 0.25f);
    }

    void sendLevelClampsBelowZero() {
        QCOMPARE(clampSendLevel(-1.0f), 0.0f);
        QCOMPARE(applySend(1.0f, -1.0f), 0.0f);
    }

    void sendLevelClampsAboveOne() {
        QCOMPARE(clampSendLevel(2.0f), 1.0f);
        QCOMPARE(applySend(1.0f, 2.0f), 1.0f);
    }

    void masterVolumeUnityPassesSignalThrough() {
        QCOMPARE(applyMasterVolume(0.75f, 1.0f), 0.75f);
    }

    void masterVolumeZeroSilences() {
        QCOMPARE(applyMasterVolume(1.0f, 0.0f), 0.0f);
    }

    void masterVolumeScalesUpToRangeLimit() {
        QCOMPARE(applyMasterVolume(1.0f, 2.0f), 2.0f);
    }

    void masterVolumeClampsBelowZero() {
        QCOMPARE(clampMasterVolume(-0.5f), 0.0f);
        QCOMPARE(applyMasterVolume(1.0f, -0.5f), 0.0f);
    }

    void masterVolumeClampsAboveTwo() {
        QCOMPARE(clampMasterVolume(3.0f), 2.0f);
        QCOMPARE(applyMasterVolume(1.0f, 3.0f), 2.0f);
    }
};

QTEST_MAIN(BusMixMathTests)
#include "test_BusMixMath.moc"
