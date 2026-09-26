#include <QTest>

#include "audio/RhythmMath.h"
#include "model/RhythmPattern.h"

using namespace rsd;

class RhythmMathTests : public QObject {
    Q_OBJECT

private slots:
    void onsetBeatsSkipsRests() {
        RhythmPattern p{"Test", {{0.5, false}, {0.25, true}, {0.25, false}}};
        auto onsets = onsetBeats(p);
        QCOMPARE(onsets.size(), size_t(2));
        QCOMPARE(onsets[0], 0.0);
        QCOMPARE(onsets[1], 0.75); // after the 0.5 note + 0.25 rest
    }

    void patternTotalBeatsIncludesRests() {
        RhythmPattern p{"Test", {{0.5, false}, {0.25, true}, {0.25, false}}};
        QCOMPARE(patternTotalBeats(p), 1.0);
    }

    void beatsToSecondsScalesWithBpm() {
        QCOMPARE(beatsToSeconds(1.0, 60.0), 1.0);
        QCOMPARE(beatsToSeconds(1.0, 120.0), 0.5);
        QCOMPARE(beatsToSeconds(2.0, 120.0), 1.0);
    }

    void classifyTapOffsetWithinToleranceIsHit() {
        QCOMPARE(classifyTapOffset(0.0, 0.1), TapVerdict::Hit);
        QCOMPARE(classifyTapOffset(0.05, 0.1), TapVerdict::Hit);
        QCOMPARE(classifyTapOffset(-0.05, 0.1), TapVerdict::Hit);
    }

    void classifyTapOffsetPastToleranceIsEarlyOrLate() {
        QCOMPARE(classifyTapOffset(-0.2, 0.1), TapVerdict::Early);
        QCOMPARE(classifyTapOffset(0.2, 0.1), TapVerdict::Late);
    }

    void scoreTapsAllHitsGivesFullAccuracy() {
        std::vector<double> expected = {0.0, 0.5, 1.0};
        std::vector<double> tapped = {0.02, 0.48, 1.01};
        auto result = scoreTaps(expected, tapped, 0.1);
        QCOMPARE(result.verdicts.size(), size_t(3));
        for (auto v : result.verdicts) QCOMPARE(v, TapVerdict::Hit);
        QCOMPARE(result.missedCount, 0);
        QCOMPARE(result.extraTapCount, 0);
        QCOMPARE(result.accuracyPercent, 100.0);
    }

    void scoreTapsMissingNoteCountsAsMissed() {
        std::vector<double> expected = {0.0, 0.5, 1.0};
        std::vector<double> tapped = {0.02, 1.01}; // skipped the middle note
        auto result = scoreTaps(expected, tapped, 0.1);
        QCOMPARE(result.missedCount, 1);
        QCOMPARE(result.extraTapCount, 0);
    }

    void scoreTapsExtraTapIsCounted() {
        std::vector<double> expected = {0.0, 0.5};
        std::vector<double> tapped = {0.0, 0.25, 0.5}; // one extra tap in the middle
        auto result = scoreTaps(expected, tapped, 0.1);
        QCOMPARE(result.missedCount, 0);
        QCOMPARE(result.extraTapCount, 1);
    }

    void scoreTapsMistimedNoteIsEarlyOrLateNotMissed() {
        std::vector<double> expected = {0.0, 0.5};
        std::vector<double> tapped = {0.0, 0.35}; // late tolerance window but still closest match
        auto result = scoreTaps(expected, tapped, 0.1);
        QCOMPARE(result.verdicts[1], TapVerdict::Early);
        QCOMPARE(result.missedCount, 0);
    }
};

QTEST_APPLESS_MAIN(RhythmMathTests)
#include "test_RhythmMath.moc"
