#include <QTest>

#include "audio/NoteNaming.h"

using namespace rsd;

class NoteNamingTests : public QObject {
    Q_OBJECT

private slots:
    void middleCIsC4() { QCOMPARE(midiNoteName(60), QStringLiteral("C4")); }

    void sharpsUseHashSuffix() {
        QCOMPARE(midiNoteName(61), QStringLiteral("C#4"));
        QCOMPARE(midiNoteName(66), QStringLiteral("F#4"));
    }

    void octaveBoundariesAreCorrect() {
        QCOMPARE(midiNoteName(59), QStringLiteral("B3"));
        QCOMPARE(midiNoteName(72), QStringLiteral("C5"));
    }

    void lowAndHighPitches() {
        QCOMPARE(midiNoteName(0), QStringLiteral("C-1"));
        QCOMPARE(midiNoteName(127), QStringLiteral("G9"));
    }
};

QTEST_MAIN(NoteNamingTests)
#include "test_NoteNaming.moc"
