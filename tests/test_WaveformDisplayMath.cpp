#include <QTest>

#include "ui/WaveformDisplayMath.h"

using rsd::computeWaveformDisplayScale;

class WaveformDisplayMathTests : public QObject {
    Q_OBJECT

private slots:
    void fullScalePeaks_noBoost() {
        QVector<std::pair<float, float>> peaks = {{-1.0f, 1.0f}, {-0.5f, 0.8f}};
        QCOMPARE(computeWaveformDisplayScale(peaks), 1.0f);
    }

    void quietPeaks_boostedToFillRange() {
        QVector<std::pair<float, float>> peaks = {{-0.1f, 0.1f}, {-0.05f, 0.08f}};
        float scale = computeWaveformDisplayScale(peaks);
        QVERIFY(scale > 1.0f);
        QVERIFY(scale * 0.1f <= 1.0001f);
    }

    void nearSilence_boostClampedNotUnbounded() {
        QVector<std::pair<float, float>> peaks = {{-0.001f, 0.001f}};
        float scale = computeWaveformDisplayScale(peaks);
        QVERIFY(scale <= 6.0f);
    }

    void trueSilence_noBoostAppliedAvoidingDivideByZero() {
        QVector<std::pair<float, float>> peaks = {{0.0f, 0.0f}, {0.0f, 0.0f}};
        QCOMPARE(computeWaveformDisplayScale(peaks), 1.0f);
    }

    void emptyPeaks_defaultsToNoScale() {
        QVector<std::pair<float, float>> peaks;
        QCOMPARE(computeWaveformDisplayScale(peaks), 1.0f);
    }

    void overScalePeaks_clampedToOne() {
        // Shouldn't normally happen (peaks should be in [-1,1]) but a
        // clip's gain could push it above; scale should never exceed 1x
        // (we only ever boost quiet audio, never shrink loud audio).
        QVector<std::pair<float, float>> peaks = {{-1.5f, 1.5f}};
        QCOMPARE(computeWaveformDisplayScale(peaks), 1.0f);
    }
};

QTEST_MAIN(WaveformDisplayMathTests)
#include "test_WaveformDisplayMath.moc"
