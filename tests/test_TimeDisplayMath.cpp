#include <QtTest>

#include "ui/TimeDisplayMath.h"

using rsd::formatSampleCount;
using rsd::formatTimecode;
using rsd::parseSampleCount;
using rsd::parseTimecode;

class TimeDisplayMathTests : public QObject {
    Q_OBJECT

private slots:
    void zeroSamplesFormatsAsZeroTimecode() {
        QCOMPARE(formatTimecode(0, 48000), QString("00:00:00.000"));
    }

    void formatsMinutesSecondsMillis() {
        // 1 min 2.5s @ 48000Hz
        int64_t samples = static_cast<int64_t>(62.5 * 48000);
        QCOMPARE(formatTimecode(samples, 48000), QString("00:01:02.500"));
    }

    void rollsOverAtExactlyOneHour() {
        int64_t samples = static_cast<int64_t>(3600.0 * 48000);
        QCOMPARE(formatTimecode(samples, 48000), QString("01:00:00.000"));
    }

    void roundsNonIntegerSampleRateDivisionDown() {
        // 1 sample at 48000Hz is a tiny fraction of a ms; should not round
        // up to 001ms falsely and should not crash.
        QCOMPARE(formatTimecode(1, 48000), QString("00:00:00.000"));
    }

    void zeroSampleRateDoesNotCrash() {
        QCOMPARE(formatTimecode(12345, 0), QString("00:00:00.000"));
    }

    void zeroSamplesFormatsAsZeroCount() {
        QCOMPARE(formatSampleCount(0), QString("0"));
    }

    void groupsDigitsWithCommas() {
        QCOMPARE(formatSampleCount(1234567), QString("1,234,567"));
    }

    void smallCountsHaveNoGrouping() {
        QCOMPARE(formatSampleCount(999), QString("999"));
    }

    void parsesFullTimecode() {
        auto result = parseTimecode("01:02:03.500", 48000);
        QVERIFY(result.has_value());
        int64_t expected = static_cast<int64_t>((3600.0 + 2 * 60 + 3.5) * 48000);
        QCOMPARE(*result, expected);
    }

    void parsesMinutesSecondsTimecode() {
        auto result = parseTimecode("01:02.500", 48000);
        QVERIFY(result.has_value());
        int64_t expected = static_cast<int64_t>(62.5 * 48000);
        QCOMPARE(*result, expected);
    }

    void parsesPlainSecondsTimecode() {
        auto result = parseTimecode("12.5", 48000);
        QVERIFY(result.has_value());
        QCOMPARE(*result, static_cast<int64_t>(12.5 * 48000));
    }

    void parseTimecodeRejectsGarbage() {
        QVERIFY(!parseTimecode("not a time", 48000).has_value());
    }

    void parseTimecodeRejectsEmpty() {
        QVERIFY(!parseTimecode("", 48000).has_value());
    }

    void parseTimecodeRejectsNegative() {
        QVERIFY(!parseTimecode("-5", 48000).has_value());
    }

    void parsesSampleCountWithCommas() {
        auto result = parseSampleCount("1,234,567");
        QVERIFY(result.has_value());
        QCOMPARE(*result, static_cast<int64_t>(1234567));
    }

    void parsesSampleCountWithoutCommas() {
        auto result = parseSampleCount("999");
        QVERIFY(result.has_value());
        QCOMPARE(*result, static_cast<int64_t>(999));
    }

    void parseSampleCountRejectsGarbage() {
        QVERIFY(!parseSampleCount("abc").has_value());
    }

    void parseSampleCountRejectsEmpty() {
        QVERIFY(!parseSampleCount("").has_value());
    }

    void parseSampleCountRejectsNegative() {
        QVERIFY(!parseSampleCount("-5").has_value());
    }
};

QTEST_MAIN(TimeDisplayMathTests)
#include "test_TimeDisplayMath.moc"
