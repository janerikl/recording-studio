// One-off generator for a ~91-second techno reinterpretation of Vivaldi's
// "Winter" (L'Inverno, RV 297, mvt. 1), reusing the project schema proven
// out by generate_lullaby.cpp / generate_country.cpp / generate_jazz.cpp.
// See PLAN.md, "Generated techno Vivaldi remix".
//
// 50 bars of 4/4 at 132 BPM (1 beat = 60/132 s = 21818 samples, 1 bar =
// 87272 samples) = ~90.9 seconds. F minor, i-VI-VII-i (Fm-Db-Eb-Fm)
// ritornello. Four voices, arranged into a classic techno intro/build/
// drop/breakdown/recap/outro structure:
//   - Sawtooth lead: hand-composed melodic theme (echoing Winter's
//     driving minor-key line) during Theme/Recap sections, a sparse
//     off-beat stab during the "shiver" section (echoing the piece's
//     famous tremolo/shivering strings), and a programmatic 16th-note
//     arpeggio during the Build section.
//   - String Ensemble: 16th-note tremolo (alternating root/fifth, the
//     "shivering" effect) or sustained chords, derived from the chart.
//   - Synth Bass 1: driving 8th-note pulse on the chord root, derived
//     from the chart.
//   - Drum kit: four-on-the-floor kick, off-beat hats, clap on 2 & 4,
//     built up in the intro and stripped out for the breakdown.

#include <QCoreApplication>
#include <QVector>
#include <iostream>
#include <memory>
#include <vector>

#include "model/Session.h"
#include "io/SessionIO.h"

using namespace rsd;

namespace {

constexpr int kSampleRate = 48000;
constexpr double kBpm = 132.0;
constexpr int64_t kBeatSamples = static_cast<int64_t>(kSampleRate * 60.0 / kBpm); // 21818
constexpr int64_t kBarSamples = 4 * kBeatSamples;
constexpr int kNumBars = 50;

enum class Quality { Min, Maj };

struct ChordSym {
    int root; // pitch class, C = 0
    Quality quality;
};

void intervalsFor(Quality q, int& third, int& fifth) {
    if (q == Quality::Min) { third = 3; fifth = 7; }
    else { third = 4; fifth = 7; }
}

constexpr int C = 0, Db = 1, D = 2, Eb = 3, E = 4, F = 5, Gb = 6, G = 7, Ab = 8, A = 9, Bb = 10, B = 11;

// i - VI - VII - i in F minor, repeating every 4 bars.
const std::vector<ChordSym> kCycle = {
    {F, Quality::Min}, {Db, Quality::Maj}, {Eb, Quality::Maj}, {F, Quality::Min},
};

ChordSym chordForBar(int bar) { return kCycle[bar % 4]; }

enum class LeadMode { None, ShiverStabs, Theme, Arp, OutroTag };
enum class StringsMode { None, Tremolo, Sustained };

struct SectionBar {
    bool kick;
    bool hatsBusy;   // 16th hats instead of just off-beat 8ths
    bool hats;
    bool clap;
    bool bass;
    bool bassOctaveJump;
    StringsMode strings;
    LeadMode lead;
};

struct MelodyNote {
    int pitch;
    double beats;
};

// Hand-composed theme (8 bars), used for both the Theme section and the
// Recap (louder there via velocity, not a different melody).
const std::vector<std::vector<MelodyNote>> kTheme = {
    {{77,0.5},{75,0.5},{73,0.5},{72,0.5},{70,0.5},{68,0.5},{67,0.5},{65,0.5}},
    {{68,0.5},{70,0.5},{72,0.5},{73,0.5},{75,0.5},{73,0.5},{72,0.5},{70,0.5}},
    {{70,0.5},{72,0.5},{73,0.5},{75,0.5},{77,0.5},{75,0.5},{73,0.5},{72,0.5}},
    {{70,0.5},{68,0.5},{67,0.5},{65,1.5},{77,1.0}},
    {{77,1.0},{75,1.0},{73,1.0},{72,1.0}},
    {{72,1.0},{70,1.0},{68,1.0},{70,1.0}},
    {{72,1.0},{73,1.0},{75,1.0},{77,1.0}},
    {{75,1.0},{73,1.0},{72,1.0},{65,1.0}},
};

// Outro tag (bars 44-49): descending resolution then a held drone.
const std::vector<std::vector<MelodyNote>> kOutroTag = {
    {{77,1.0},{75,1.0},{73,1.0},{72,1.0}},
    {{70,1.0},{68,1.0},{67,1.0},{65,1.0}},
    {{65,4.0}},
    {{65,4.0}},
    {{65,4.0}},
    {{65,4.0}},
};

std::vector<SectionBar> buildSections() {
    std::vector<SectionBar> bars(kNumBars);
    // Intro (0-3): kick builds, nothing else.
    bars[0] = {false, false, false, false, false, false, StringsMode::None, LeadMode::None};
    bars[1] = {true,  false, false, false, false, false, StringsMode::None, LeadMode::None};
    bars[2] = {true,  false, true,  false, false, false, StringsMode::None, LeadMode::None};
    bars[3] = {true,  false, true,  false, false, false, StringsMode::None, LeadMode::None};
    // Shiver (4-11): tremolo strings, sparse lead stabs, driving bass.
    for (int i = 4; i <= 11; ++i)
        bars[i] = {true, false, true, true, true, false, StringsMode::Tremolo, LeadMode::ShiverStabs};
    // Theme (12-19): sustained strings, the hand-composed melody.
    for (int i = 12; i <= 19; ++i)
        bars[i] = {true, false, true, true, true, false, StringsMode::Sustained, LeadMode::Theme};
    // Build (20-27): tremolo strings, 16th arpeggio, busier hats, octave-jump bass.
    for (int i = 20; i <= 27; ++i)
        bars[i] = {true, true, true, true, true, true, StringsMode::Tremolo, LeadMode::Arp};
    // Breakdown (28-35): kick and bass drop out, soft tremolo, theme melody softly.
    for (int i = 28; i <= 35; ++i)
        bars[i] = {false, false, true, false, false, false, StringsMode::Tremolo, LeadMode::Theme};
    // Recap (36-43): full energy return of the theme.
    for (int i = 36; i <= 43; ++i)
        bars[i] = {true, true, true, true, true, true, StringsMode::Sustained, LeadMode::Theme};
    // Outro (44-49): fades out.
    bars[44] = {true,  true,  true,  true,  true,  true,  StringsMode::Sustained, LeadMode::OutroTag};
    bars[45] = {true,  false, true,  true,  true,  false, StringsMode::Sustained, LeadMode::OutroTag};
    bars[46] = {true,  false, true,  false, false, false, StringsMode::Sustained, LeadMode::OutroTag};
    bars[47] = {false, false, true,  false, false, false, StringsMode::Sustained, LeadMode::OutroTag};
    bars[48] = {false, false, false, false, false, false, StringsMode::Sustained, LeadMode::OutroTag};
    bars[49] = {false, false, false, false, false, false, StringsMode::Sustained, LeadMode::OutroTag};
    return bars;
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
        std::cerr << "usage: generate_techno_vivaldi <output.rsdproj>\n";
        return 1;
    }

    Session session;
    session.sampleRate = kSampleRate;
    session.bpm = kBpm;

    auto sections = buildSections();

    auto leadTrack = session.addTrack("Techno Lead");
    leadTrack->kind = TrackKind::Instrument;
    leadTrack->synthParams.instrumentProgram.store(81); // Lead 2 (sawtooth)

    auto stringsTrack = session.addTrack("Shiver Strings");
    stringsTrack->kind = TrackKind::Instrument;
    stringsTrack->synthParams.instrumentProgram.store(48); // String Ensemble 1

    auto bassTrack = session.addTrack("Synth Bass");
    bassTrack->kind = TrackKind::Instrument;
    bassTrack->synthParams.instrumentProgram.store(38); // Synth Bass 1

    auto drumTrack = session.addTrack("Techno Kit");
    drumTrack->kind = TrackKind::Instrument;
    drumTrack->synthParams.isDrumKit.store(true);

    constexpr int kKick = 36;
    constexpr int kClap = 39;
    constexpr int kClosedHat = 42;
    constexpr int kOpenHat = 46;

    int themeBarCursor = 0; // walks 0..7 across Theme/Recap/Breakdown occurrences

    for (int bar = 0; bar < kNumBars; ++bar) {
        int64_t barStart = static_cast<int64_t>(bar) * kBarSamples;
        const SectionBar& s = sections[bar];
        ChordSym chord = chordForBar(bar);
        int third, fifth;
        intervalsFor(chord.quality, third, fifth);

        // --- Lead ---
        switch (s.lead) {
            case LeadMode::None:
                break;
            case LeadMode::ShiverStabs: {
                int stabPitch = 60 + chord.root + 12;
                int64_t hitLen = kBeatSamples / 4;
                leadTrack->addMidiNote(makeNote(stabPitch, 0.6f, barStart + 1 * kBeatSamples, hitLen));
                leadTrack->addMidiNote(makeNote(stabPitch, 0.6f, barStart + 3 * kBeatSamples, hitLen));
                break;
            }
            case LeadMode::Theme: {
                bool softBreakdown = (bar >= 28 && bar <= 35);
                bool recap = (bar >= 36 && bar <= 43);
                const auto& melBar = kTheme[themeBarCursor % kTheme.size()];
                int64_t offset = 0;
                for (auto& n : melBar) {
                    int64_t length = static_cast<int64_t>(n.beats * kBeatSamples);
                    float velocity = softBreakdown ? 0.35f : (recap ? 0.85f : 0.7f);
                    leadTrack->addMidiNote(makeNote(n.pitch, velocity, barStart + offset, length));
                    offset += length;
                }
                ++themeBarCursor;
                break;
            }
            case LeadMode::Arp: {
                int tones[4] = {60 + chord.root, 60 + chord.root + third, 60 + chord.root + fifth,
                                60 + chord.root + 12};
                int64_t sixteenth = kBeatSamples / 4;
                for (int i = 0; i < 16; ++i) {
                    leadTrack->addMidiNote(
                        makeNote(tones[i % 4], 0.55f, barStart + i * sixteenth, sixteenth));
                }
                break;
            }
            case LeadMode::OutroTag: {
                int idx = bar - 44;
                if (idx >= 0 && idx < static_cast<int>(kOutroTag.size())) {
                    int64_t offset = 0;
                    for (auto& n : kOutroTag[idx]) {
                        int64_t length = static_cast<int64_t>(n.beats * kBeatSamples);
                        leadTrack->addMidiNote(makeNote(n.pitch, 0.6f, barStart + offset, length));
                        offset += length;
                    }
                }
                break;
            }
        }

        // --- Strings ---
        if (s.strings == StringsMode::Tremolo) {
            int rootPitch = 48 + chord.root;
            int fifthPitch = rootPitch + fifth;
            int64_t sixteenth = kBeatSamples / 4;
            bool soft = (bar >= 28 && bar <= 35);
            for (int i = 0; i < 16; ++i) {
                int pitch = (i % 2 == 0) ? rootPitch : fifthPitch;
                stringsTrack->addMidiNote(
                    makeNote(pitch, soft ? 0.2f : 0.35f, barStart + i * sixteenth, sixteenth));
            }
        } else if (s.strings == StringsMode::Sustained) {
            int rootPitch = 48 + chord.root;
            stringsTrack->addMidiNote(makeNote(rootPitch, 0.3f, barStart, kBarSamples));
            stringsTrack->addMidiNote(makeNote(rootPitch + third, 0.3f, barStart, kBarSamples));
            stringsTrack->addMidiNote(makeNote(rootPitch + fifth, 0.3f, barStart, kBarSamples));
        }

        // --- Bass ---
        if (s.bass) {
            int rootPitch = 28 + chord.root;
            int64_t eighth = kBeatSamples / 2;
            for (int i = 0; i < 8; ++i) {
                int pitch = (s.bassOctaveJump && i % 2 == 1) ? rootPitch + 12 : rootPitch;
                bassTrack->addMidiNote(makeNote(pitch, 0.6f, barStart + i * eighth, eighth));
            }
        }

        // --- Drums ---
        int64_t hitLen = kBeatSamples / 4;
        if (s.kick) {
            for (int b = 0; b < 4; ++b)
                drumTrack->addMidiNote(makeNote(kKick, 0.7f, barStart + b * kBeatSamples, hitLen));
        }
        if (s.hats) {
            int64_t eighth = kBeatSamples / 2;
            for (int i = 0; i < 8; ++i) {
                if (i % 2 == 1) { // off-beat 8ths
                    int pitch = (s.hatsBusy && i == 7) ? kOpenHat : kClosedHat;
                    drumTrack->addMidiNote(makeNote(pitch, 0.3f, barStart + i * eighth, hitLen));
                }
            }
            if (s.hatsBusy) {
                int64_t sixteenth = kBeatSamples / 4;
                for (int i = 0; i < 16; ++i) {
                    if (i % 2 == 0) continue; // leave the 8th-note grid above alone
                    drumTrack->addMidiNote(makeNote(kClosedHat, 0.2f, barStart + i * sixteenth, hitLen / 2));
                }
            }
        }
        if (s.clap) {
            drumTrack->addMidiNote(makeNote(kClap, 0.5f, barStart + 1 * kBeatSamples, hitLen));
            drumTrack->addMidiNote(makeNote(kClap, 0.5f, barStart + 3 * kBeatSamples, hitLen));
        }
    }

    QVector<LibraryEntry> emptyLibrary;
    if (!SessionIO::saveSession(argv[1], session, emptyLibrary)) {
        std::cerr << "Failed to save session.\n";
        return 1;
    }

    std::cout << "Wrote " << argv[1] << " (" << kNumBars << " bars, "
              << (kNumBars * kBarSamples / static_cast<double>(kSampleRate)) << " seconds)\n";
    return 0;
}
