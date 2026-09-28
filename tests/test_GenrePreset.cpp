#include <QTest>

#include "model/GenrePreset.h"

using namespace rsd;

namespace {
bool inRange(int v, int lo, int hi) { return v >= lo && v <= hi; }
}

class GenrePresetTests : public QObject {
    Q_OBJECT

private slots:
    void countryMatchesExpectedTempoAndInstruments() {
        auto p = presetForGenre("country");
        QCOMPARE(p.bpm, 120.0);
        QCOMPARE(p.rootMidiNote, 67); // G
        QVERIFY(!p.useDrumKit || p.useDrumKit); // sanity: field exists
        QCOMPARE(p.leadInstrumentProgram, 25);      // Acoustic Guitar (steel)
        QCOMPARE(p.harmonyInstrumentProgram, 105);  // Banjo
    }

    void jazzMatchesExpectedFamily() {
        auto p = presetForGenre("jazz");
        QCOMPARE(p.bpm, 96.0);
        QVERIFY(p.leadInstrumentProgram == 66 || p.leadInstrumentProgram == 64); // sax family
    }

    void lullabyIsSlowAndGentle() {
        auto p = presetForGenre("lullaby");
        QVERIFY(p.bpm <= 80.0);
        QVERIFY(!p.useDrumKit);
    }

    void technoIsFast() {
        auto p = presetForGenre("techno");
        QVERIFY(p.bpm >= 120.0);
    }

    void caseInsensitiveMatch() {
        auto lower = presetForGenre("country");
        auto upper = presetForGenre("COUNTRY");
        auto mixed = presetForGenre("CoUnTrY");
        QCOMPARE(lower.bpm, upper.bpm);
        QCOMPARE(lower.bpm, mixed.bpm);
        QCOMPARE(lower.leadInstrumentProgram, upper.leadInstrumentProgram);
    }

    void allKnownGenresProduceValidPresets() {
        const QStringList genres = {"country", "jazz",  "lullaby", "rock",     "pop",
                                     "blues",   "techno", "classical", "reggae", "metal"};
        for (const auto& g : genres) {
            auto p = presetForGenre(g);
            QVERIFY2(p.bpm > 0.0, qPrintable(g));
            QVERIFY2(inRange(p.rootMidiNote, 0, 127), qPrintable(g));
            QVERIFY2(inRange(p.leadInstrumentProgram, 0, 127), qPrintable(g));
            QVERIFY2(inRange(p.harmonyInstrumentProgram, 0, 127), qPrintable(g));
            QVERIFY2(!p.chordDegrees.isEmpty(), qPrintable(g));
        }
    }

    void unrecognizedGenreIsDeterministic() {
        auto a = presetForGenre("vaporwave");
        auto b = presetForGenre("vaporwave");
        QCOMPARE(a.bpm, b.bpm);
        QCOMPARE(a.rootMidiNote, b.rootMidiNote);
        QCOMPARE(a.leadInstrumentProgram, b.leadInstrumentProgram);
        QCOMPARE(a.harmonyInstrumentProgram, b.harmonyInstrumentProgram);
        QCOMPARE(a.useDrumKit, b.useDrumKit);
        QCOMPARE(a.chordDegrees, b.chordDegrees);
    }

    void unrecognizedGenreIsValid() {
        auto p = presetForGenre("some-totally-made-up-genre-xyz");
        QVERIFY(p.bpm > 0.0);
        QVERIFY(inRange(p.rootMidiNote, 0, 127));
        QVERIFY(inRange(p.leadInstrumentProgram, 0, 127));
        QVERIFY(inRange(p.harmonyInstrumentProgram, 0, 127));
        QVERIFY(!p.chordDegrees.isEmpty());
    }

    void differentUnrecognizedGenresLikelyDiffer() {
        auto a = presetForGenre("vaporwave");
        auto b = presetForGenre("dungeon-synth");
        // Not guaranteed distinct in every field, but should not be
        // byte-identical presets for very different strings in practice.
        bool anyDifferent = a.bpm != b.bpm || a.rootMidiNote != b.rootMidiNote ||
                             a.leadInstrumentProgram != b.leadInstrumentProgram ||
                             a.harmonyInstrumentProgram != b.harmonyInstrumentProgram;
        QVERIFY(anyDifferent);
    }

    void emptyStringDoesNotCrash() {
        auto p = presetForGenre("");
        QVERIFY(p.bpm > 0.0);
        QVERIFY(inRange(p.leadInstrumentProgram, 0, 127));
        QVERIFY(inRange(p.harmonyInstrumentProgram, 0, 127));
    }

    void unicodeStringDoesNotCrash() {
        auto p = presetForGenre(QString::fromUtf8("フォークソング🎵"));
        QVERIFY(p.bpm > 0.0);
        QVERIFY(inRange(p.leadInstrumentProgram, 0, 127));
    }

    void veryLongStringDoesNotCrash() {
        QString longName(5000, QLatin1Char('x'));
        auto p = presetForGenre(longName);
        QVERIFY(p.bpm > 0.0);
        QVERIFY(inRange(p.leadInstrumentProgram, 0, 127));
    }
};

QTEST_MAIN(GenrePresetTests)
#include "test_GenrePreset.moc"
