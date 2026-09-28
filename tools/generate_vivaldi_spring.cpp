// One-off generator for Vivaldi's "The Four Seasons — Spring" (La
// Primavera, RV 269), 1st movement (Allegro), reduced to exactly two
// instrument tracks: solo Violin (melody) and Piano (harmony/bass
// reduction of the orchestral tutti + continuo). No bus track, no drum
// track — both tracks mix straight to master, same as
// generate_techno_vivaldi.cpp's non-bus tracks. See PLAN.md, "Vivaldi
// Spring (piano + violin duo)".
//
// Tempo/bar math: 120 BPM, 4/4 => 1 beat = 60/120 s = 0.5 s = 24000
// samples @ 48kHz, 1 bar = 4 beats = 96000 samples = 2.0 s exactly.
// 105 bars * 2.0 s/bar = 210.0 s, matching the movement's traditional
// ~3:30 duration exactly (no rounding/fade slop needed).
//
// Ritornello form, bar counts chosen to sum to 105:
//   A1 Ritornello (full theme)      bars   0-11   (12 bars)
//   B  "Il Canto de gl'Augelli"     bars  12-29   (18 bars)  birdsong
//   A2 Ritornello (partial)         bars  30-37   ( 8 bars)
//   C  "Correnti" (murmuring brook) bars  38-55   (18 bars)  16th arpeggios
//   A3 Ritornello (partial)         bars  56-63   ( 8 bars)
//   D  "Lampi e tuoni" (storm)      bars  64-81   (18 bars)  minor, tremolo
//   A4 Ritornello (full close)      bars  82-104  (23 bars)
//   total                                          105 bars = 210.0 s
//
// Key: E major for the ritornello/birds/streams episodes; the storm
// episode borrows the relative-ish minor colors (vi=C#m, IV=A, V=B) for
// a darker "minor-inflected" sound before the final ritornello resolves
// back to E major, per the brief.
//
// Everything below the hand-composed ritornello motif and birdsong
// contour is *derived* from a per-section chord chart (chordForBar
// below), the same "derive from the chart" technique used by
// generate_jazz.cpp (comping/bass from a chord chart) and
// generate_techno_vivaldi.cpp (SectionBar table).

#include <QCoreApplication>
#include <QString>
#include <QVector>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "audio/OfflineRenderer.h"
#include "io/AudioFileIO.h"
#include "io/SessionIO.h"
#include "model/Session.h"

using namespace rsd;

namespace {

constexpr int kSampleRate = 48000;
constexpr double kBpm = 120.0;
constexpr int64_t kBeatSamples = static_cast<int64_t>(kSampleRate * 60.0 / kBpm); // 24000
constexpr int64_t kBarSamples = 4 * kBeatSamples;                                 // 96000 (2.0s)

constexpr int C = 0, Cs = 1, D = 2, Ds = 3, E = 4, F = 5, Fs = 6, G = 7, Gs = 8, A = 9, As = 10, B = 11;

enum class Quality { Maj, Min };

struct ChordSym {
    int root; // pitch class
    Quality quality;
};

void intervalsFor(Quality q, int& third, int& fifth) {
    if (q == Quality::Min) { third = 3; fifth = 7; }
    else { third = 4; fifth = 7; }
}

enum class Section { RitornelloFull, RitornelloPartial, Birds, Streams, Storm };

struct SectionSpan {
    Section kind;
    int startBar;
    int barCount;
};

// Ritornello form: A1 - B - A2 - C - A3 - D - A4 (see header comment for bar math).
const std::vector<SectionSpan> kForm = {
    {Section::RitornelloFull,    0, 12},
    {Section::Birds,            12, 18},
    {Section::RitornelloPartial, 30, 8},
    {Section::Streams,          38, 18},
    {Section::RitornelloPartial, 56, 8},
    {Section::Storm,            64, 18},
    {Section::RitornelloFull,   82, 23},
};
constexpr int kNumBars = 105; // sum of the spans above; also see header comment.

const SectionSpan& spanForBar(int bar) {
    for (const auto& s : kForm)
        if (bar >= s.startBar && bar < s.startBar + s.barCount) return s;
    return kForm.back();
}

// Chord chart, per section, cycling by (bar - span.startBar).
ChordSym chordForBar(int bar) {
    const SectionSpan& s = spanForBar(bar);
    int i = bar - s.startBar;
    switch (s.kind) {
        case Section::RitornelloFull:
        case Section::RitornelloPartial: {
            // Vivaldi's bouncy tonic/dominant ritornello: I-I-V-V, 4-bar cycle.
            static const ChordSym cyc[4] = {
                {E, Quality::Maj}, {E, Quality::Maj}, {B, Quality::Maj}, {B, Quality::Maj}};
            return cyc[i % 4];
        }
        case Section::Birds: {
            // Long sustained I / V under the birdsong figuration, 8-bar halves.
            return (i % 16 < 8) ? ChordSym{E, Quality::Maj} : ChordSym{B, Quality::Maj};
        }
        case Section::Streams: {
            // I - IV - V - I, 4-bar cycle: classic flowing progression.
            static const ChordSym cyc[4] = {
                {E, Quality::Maj}, {A, Quality::Maj}, {B, Quality::Maj}, {E, Quality::Maj}};
            return cyc[i % 4];
        }
        case Section::Storm: {
            // Minor-inflected storm: vi - IV - V - vi, 4-bar cycle (borrowed
            // from E major's relative minor area, per the "minor-inflected" brief).
            static const ChordSym cyc[4] = {
                {Cs, Quality::Min}, {A, Quality::Maj}, {B, Quality::Maj}, {Cs, Quality::Min}};
            return cyc[i % 4];
        }
    }
    return {E, Quality::Maj};
}

struct MelodyNote {
    int pitch;
    double beats; // may be negative-free; 0 beats not used (rests are just omitted)
};

// Hand-composed ritornello theme (4-bar phrase), echoing the famous
// bouncing-eighth-note incipit of Spring's opening tutti. Violin plays it
// up an octave from the piano's right hand for a bright, doubled sound.
const std::vector<std::vector<MelodyNote>> kRitornelloTheme = {
    // Bar 1 of phrase (I): rising broken-chord bounce.
    {{76, 0.5}, {76, 0.5}, {76, 0.5}, {80, 0.5}, {83, 0.5}, {80, 0.5}, {76, 0.5}, {83, 0.5}},
    // Bar 2 (I): answering descent.
    {{83, 0.5}, {80, 0.5}, {76, 0.5}, {80, 0.5}, {76, 0.5}, {73, 0.5}, {71, 0.5}, {76, 1.0}},
    // Bar 3 (V): dominant restatement, up a step.
    {{78, 0.5}, {78, 0.5}, {78, 0.5}, {82, 0.5}, {85, 0.5}, {82, 0.5}, {78, 0.5}, {85, 0.5}},
    // Bar 4 (V): cadential close back toward tonic.
    {{85, 0.5}, {82, 0.5}, {78, 0.5}, {82, 0.5}, {78, 0.5}, {75, 0.5}, {73, 0.5}, {71, 1.0}},
};

// E major scale (pitch classes, ascending from E) used to derive the
// birdsong figuration and the streams arpeggios so they always land on
// scale/chord tones rather than free-form magic numbers.
const std::vector<int> kEMajorScale = {E, Fs, Gs, A, B, Cs, Ds};

int scaleTone(int degree, int octaveBase) {
    int len = static_cast<int>(kEMajorScale.size());
    int oct = degree / len;
    int idx = degree % len;
    if (idx < 0) { idx += len; --oct; }
    return octaveBase + oct * 12 + kEMajorScale[idx];
}

std::shared_ptr<MidiNote> makeNote(int pitch, float velocity, int64_t start, int64_t length) {
    auto note = std::make_shared<MidiNote>();
    note->pitch = pitch;
    note->velocity = velocity;
    note->startSample = start;
    note->lengthSamples = length;
    return note;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::cerr << "usage: generate_vivaldi_spring <output-path-without-extension>\n";
        return 1;
    }
    const QString outBase = QString::fromUtf8(argv[1]);

    Session session;
    session.sampleRate = kSampleRate;
    session.bpm = kBpm;

    // --- Track setup: exactly two Instrument tracks, no bus, no drums. ---
    auto violin = session.addTrack("Violin");
    violin->kind = TrackKind::Instrument;
    violin->synthParams.instrumentProgram.store(40); // GM program 40 = Violin
    violin->volume.store(0.9f);
    violin->pan.store(-0.2f); // slightly left: solo violin "steps forward" of the piano

    auto piano = session.addTrack("Piano");
    piano->kind = TrackKind::Instrument;
    piano->synthParams.instrumentProgram.store(0); // GM program 0 = Acoustic Grand Piano
    piano->volume.store(0.75f);
    piano->pan.store(0.15f); // slightly right, mirroring the violin for stereo separation

    for (int bar = 0; bar < kNumBars; ++bar) {
        int64_t barStart = static_cast<int64_t>(bar) * kBarSamples;
        const SectionSpan& span = spanForBar(bar);
        int i = bar - span.startBar; // bar index within this section's occurrence
        ChordSym chord = chordForBar(bar);
        int third, fifth;
        intervalsFor(chord.quality, third, fifth);
        int pRoot = 48 + chord.root;  // piano LH register (C3-ish)
        int pThird = pRoot + third;
        int pFifth = pRoot + fifth;

        switch (span.kind) {
            case Section::RitornelloFull:
            case Section::RitornelloPartial: {
                const auto& phraseBar = kRitornelloTheme[i % 4];
                // Violin carries the theme.
                int64_t offset = 0;
                for (auto& n : phraseBar) {
                    int64_t len = static_cast<int64_t>(n.beats * kBeatSamples);
                    violin->addMidiNote(makeNote(n.pitch, 0.85f, barStart + offset, len));
                    offset += len;
                }
                // Piano doubles the theme an octave down (in octaves/chords, per the
                // brief) on beats 1 and 3, plus the bass root every beat.
                piano->addMidiNote(makeNote(pRoot - 12, 0.6f, barStart, kBeatSamples * 2));
                piano->addMidiNote(makeNote(pRoot - 12, 0.6f, barStart + 2 * kBeatSamples, kBeatSamples * 2));
                piano->addMidiNote(makeNote(pThird - 12, 0.5f, barStart, kBeatSamples * 2));
                piano->addMidiNote(makeNote(pFifth - 12, 0.5f, barStart + 2 * kBeatSamples, kBeatSamples * 2));
                for (int b = 0; b < 4; ++b)
                    piano->addMidiNote(makeNote(pRoot - 24, 0.55f, barStart + b * kBeatSamples, kBeatSamples));
                break;
            }
            case Section::Birds: {
                // Violin: fast ornamented, trilling high-register figure derived
                // from the E major scale around the chord's chord tone, walking
                // up and back down across the bar in 16th notes with a
                // neighbour-tone "trill" flick on alternating notes.
                int64_t sixteenth = kBeatSamples / 4;
                int baseDegree = (chord.root == E) ? 14 : 16; // high register, chord-anchored
                for (int n16 = 0; n16 < 16; ++n16) {
                    int wobble = (n16 % 4 == 2) ? 1 : 0; // upper-neighbour trill flick
                    int degree = baseDegree + ((n16 / 2) % 3) - (n16 % 2 == 1 ? -2 : 0);
                    int pitch = scaleTone(degree, 0) + wobble;
                    float vel = (n16 % 2 == 0) ? 0.6f : 0.5f;
                    violin->addMidiNote(makeNote(pitch, vel, barStart + n16 * sixteenth, sixteenth));
                }
                // Piano: light, sustained chord underneath (whole bar).
                piano->addMidiNote(makeNote(pRoot - 12, 0.25f, barStart, kBarSamples));
                piano->addMidiNote(makeNote(pThird - 12, 0.22f, barStart, kBarSamples));
                piano->addMidiNote(makeNote(pFifth - 12, 0.22f, barStart, kBarSamples));
                break;
            }
            case Section::Streams: {
                // Both parts: flowing 16th-note arpeggiated figures ("Correnti"),
                // derived straight from the chord tones, violin an octave above
                // the piano's right hand.
                int64_t sixteenth = kBeatSamples / 4;
                int tones[4] = {pRoot, pThird, pFifth, pRoot + 12};
                for (int n16 = 0; n16 < 16; ++n16) {
                    int idx = (n16 % 2 == 0) ? (n16 / 2) % 4 : 3 - ((n16 / 2) % 4); // up-down ripple
                    int pianoPitch = tones[idx];
                    violin->addMidiNote(
                        makeNote(pianoPitch + 12, 0.55f, barStart + n16 * sixteenth, sixteenth));
                    piano->addMidiNote(
                        makeNote(pianoPitch, 0.4f, barStart + n16 * sixteenth, sixteenth));
                }
                // Piano LH: sustained bass root under the ripple.
                piano->addMidiNote(makeNote(pRoot - 12, 0.35f, barStart, kBarSamples));
                break;
            }
            case Section::Storm: {
                // "Lampi e tuoni": minor-inflected, fast tremolo/repeated notes in
                // both parts, louder dynamics.
                int64_t sixteenth = kBeatSamples / 4;
                for (int n16 = 0; n16 < 16; ++n16) {
                    int pitch = (n16 % 2 == 0) ? (pRoot + 12) : (pFifth + 12); // tremolo root/fifth
                    violin->addMidiNote(
                        makeNote(pitch, 0.95f, barStart + n16 * sixteenth, sixteenth));
                }
                // Piano: driving repeated-octave bass "thunder" on every 8th,
                // plus a full chord stab on beat 1 and 3.
                int64_t eighth = kBeatSamples / 2;
                for (int e = 0; e < 8; ++e) {
                    int pitch = (e % 2 == 0) ? (pRoot - 24) : (pRoot - 12);
                    piano->addMidiNote(makeNote(pitch, 0.9f, barStart + e * eighth, eighth));
                }
                piano->addMidiNote(makeNote(pRoot, 0.95f, barStart, kBeatSamples));
                piano->addMidiNote(makeNote(pThird, 0.9f, barStart, kBeatSamples));
                piano->addMidiNote(makeNote(pFifth, 0.9f, barStart, kBeatSamples));
                piano->addMidiNote(makeNote(pRoot, 0.95f, barStart + 2 * kBeatSamples, kBeatSamples));
                piano->addMidiNote(makeNote(pThird, 0.9f, barStart + 2 * kBeatSamples, kBeatSamples));
                piano->addMidiNote(makeNote(pFifth, 0.9f, barStart + 2 * kBeatSamples, kBeatSamples));
                break;
            }
        }
    }

    // Markers at the start of each named episode, mirroring the arrangement
    // markers other generator tools leave for navigation in the app.
    session.markers[1] = kForm[0].startBar * kBarSamples; // Ritornello (open)
    session.markers[2] = kForm[1].startBar * kBarSamples; // Birds
    session.markers[3] = kForm[3].startBar * kBarSamples; // Streams
    session.markers[4] = kForm[5].startBar * kBarSamples; // Storm
    session.markers[5] = kForm[6].startBar * kBarSamples; // Ritornello (close)

    QVector<LibraryEntry> emptyLibrary;
    const QString rsdPath = outBase + ".rsdproj";
    if (!SessionIO::saveSession(rsdPath, session, emptyLibrary)) {
        std::cerr << "Failed to save session: " << qPrintable(rsdPath) << "\n";
        return 1;
    }

    // Reset every Instrument track's SynthEngine before rendering, same
    // precaution generate_song.cpp and MainWindow::onExportClicked take.
    for (auto& track : session.tracks) track->synthEngine.reset();

    int64_t lengthSamples = sessionContentLengthSamples(session);
    if (lengthSamples <= 0) {
        std::cerr << "Generated session has no content to render.\n";
        return 1;
    }

    auto mixdown = renderSessionMixdown(session, static_cast<unsigned int>(session.sampleRate),
                                         static_cast<unsigned int>(session.channels),
                                         lengthSamples);
    const QString wavPath = outBase + ".wav";
    if (!mixdown || !AudioFileIO::writeFile(wavPath, *mixdown, ExportFormat::Wav32Float)) {
        std::cerr << "Failed to render/write: " << qPrintable(wavPath) << "\n";
        return 1;
    }

    std::cout << "Vivaldi Spring, mvt. 1 (piano + violin duo): " << kNumBars << " bars @ "
              << kBpm << " BPM\n"
              << "Wrote " << qPrintable(rsdPath) << "\n"
              << "Wrote " << qPrintable(wavPath) << " ("
              << (lengthSamples / static_cast<double>(kSampleRate)) << " seconds)\n";
    return 0;
}
