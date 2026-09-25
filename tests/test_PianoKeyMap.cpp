#include <QTest>

#include "ui/PianoKeyMap.h"

using namespace rsd;

class PianoKeyMapTests : public QObject {
    Q_OBJECT

private slots:
    void whiteRowMapsToConsecutiveWhiteNotes() {
        QCOMPARE(pitchForComputerKey(Qt::Key_A), std::optional<int>(48));  // C3
        QCOMPARE(pitchForComputerKey(Qt::Key_S), std::optional<int>(50));  // D3
        QCOMPARE(pitchForComputerKey(Qt::Key_D), std::optional<int>(52));  // E3
        QCOMPARE(pitchForComputerKey(Qt::Key_F), std::optional<int>(53));  // F3
        QCOMPARE(pitchForComputerKey(Qt::Key_G), std::optional<int>(55));  // G3
        QCOMPARE(pitchForComputerKey(Qt::Key_H), std::optional<int>(57));  // A3
        QCOMPARE(pitchForComputerKey(Qt::Key_J), std::optional<int>(59));  // B3
        QCOMPARE(pitchForComputerKey(Qt::Key_K), std::optional<int>(60));  // C4
        QCOMPARE(pitchForComputerKey(Qt::Key_L), std::optional<int>(62));  // D4
        QCOMPARE(pitchForComputerKey(Qt::Key_Odiaeresis), std::optional<int>(64)); // E4
        QCOMPARE(pitchForComputerKey(Qt::Key_Adiaeresis), std::optional<int>(65)); // F4
    }

    void blackRowMapsAboveGapsOnly() {
        QCOMPARE(pitchForComputerKey(Qt::Key_W), std::optional<int>(49)); // C#3
        QCOMPARE(pitchForComputerKey(Qt::Key_E), std::optional<int>(51)); // D#3
        QCOMPARE(pitchForComputerKey(Qt::Key_T), std::optional<int>(54)); // F#3
        QCOMPARE(pitchForComputerKey(Qt::Key_Y), std::optional<int>(56)); // G#3
        QCOMPARE(pitchForComputerKey(Qt::Key_U), std::optional<int>(58)); // A#3
        QCOMPARE(pitchForComputerKey(Qt::Key_O), std::optional<int>(61)); // C#4
        QCOMPARE(pitchForComputerKey(Qt::Key_P), std::optional<int>(63)); // D#4
    }

    void unmappedGapKeysReturnNullopt() {
        QVERIFY(!pitchForComputerKey(Qt::Key_Q).has_value());
        QVERIFY(!pitchForComputerKey(Qt::Key_R).has_value());
        QVERIFY(!pitchForComputerKey(Qt::Key_I).has_value());
        QVERIFY(!pitchForComputerKey(Qt::Key_Aring).has_value());
    }

    void unrelatedKeyReturnsNullopt() { QVERIFY(!pitchForComputerKey(Qt::Key_Z).has_value()); }

    void labelsRoundTripWithPitchLookup() {
        QCOMPARE(computerKeyLabelForPitch(48), QStringLiteral("A"));
        QCOMPARE(computerKeyLabelForPitch(49), QStringLiteral("W"));
        QCOMPARE(computerKeyLabelForPitch(64), QString::fromUtf8("\xc3\x96")); // Ö
        QCOMPARE(computerKeyLabelForPitch(65), QString::fromUtf8("\xc3\x84")); // Ä
    }

    void labelIsEmptyForUnmappedPitch() {
        QVERIFY(computerKeyLabelForPitch(72).isEmpty()); // C5, outside the mapped range
        QVERIFY(computerKeyLabelForPitch(66).isEmpty()); // F#4, a gap black key
    }
};

QTEST_MAIN(PianoKeyMapTests)
#include "test_PianoKeyMap.moc"
