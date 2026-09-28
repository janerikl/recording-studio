// One-off generator for a ~2-minute smoky jazz ballad, reusing the same
// project schema (instrument tracks + MIDI notes) proven out by
// generate_lullaby.cpp / generate_country.cpp. See PLAN.md, "Generated
// smoky jazz song".
//
// 48 bars of 4/4 at 96 BPM (1 beat = 0.625s = 30000 samples, so 1 bar =
// 120000 samples) = exactly 120 seconds. C minor. Form: A A B A (head,
// 32 bars) + solo over the A changes (8 bars) + a Cm7/G7 vamp tag
// resolving to a noir C minor-major7 (8 bars). Four voices:
//   - Tenor Sax: hand-composed melody/solo with swung eighth notes.
//   - Electric Piano 1 (Rhodes): Charleston-rhythm chord comping,
//     4-note voicings (root/3rd/5th/7th) derived from the chord chart.
//   - Acoustic Bass: walking quarter-note bass line (root-3rd-5th then
//     a chromatic approach into the next bar's root) derived the same way.
//   - Drum kit (brushes feel): ride each beat, snare on 2 & 4, soft
//     kick anchoring alternate bars.

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
constexpr double kBpm = 96.0;
constexpr int64_t kBeatSamples = static_cast<int64_t>(kSampleRate * 60.0 / kBpm); // 30000
constexpr int64_t kBarSamples = 4 * kBeatSamples;

enum class Quality { Maj7, Min7, Dom7, HalfDim7, MinMaj7 };

struct ChordSym {
    int root; // pitch class, C = 0
    Quality quality;
};

// Intervals from the root: {3rd, 5th, 7th}.
void intervalsFor(Quality q, int& third, int& fifth, int& seventh) {
    switch (q) {
        case Quality::Maj7:    third = 4; fifth = 7; seventh = 11; break;
        case Quality::Min7:    third = 3; fifth = 7; seventh = 10; break;
        case Quality::Dom7:    third = 4; fifth = 7; seventh = 10; break;
        case Quality::HalfDim7: third = 3; fifth = 6; seventh = 10; break;
        case Quality::MinMaj7: third = 3; fifth = 7; seventh = 11; break;
    }
}

constexpr int C = 0, Db = 1, D = 2, Eb = 3, E = 4, F = 5, Gb = 6, G = 7, Ab = 8, A = 9, Bb = 10,
              B = 11;

// clang-format off
const std::vector<ChordSym> kProgA = {
    {C, Quality::Min7}, {F, Quality::Min7}, {Bb, Quality::Dom7}, {Eb, Quality::Maj7},
    {A, Quality::HalfDim7}, {D, Quality::Dom7}, {G, Quality::Min7}, {G, Quality::Dom7},
};
const std::vector<ChordSym> kProgB = {
    {Eb, Quality::Maj7}, {Eb, Quality::Min7}, {Ab, Quality::Dom7}, {Db, Quality::Maj7},
    {D, Quality::HalfDim7}, {G, Quality::Dom7}, {C, Quality::Min7}, {G, Quality::Dom7},
};
const std::vector<ChordSym> kProgTag = {
    {C, Quality::Min7}, {G, Quality::Dom7}, {C, Quality::Min7}, {G, Quality::Dom7},
    {C, Quality::Min7}, {G, Quality::Dom7}, {C, Quality::MinMaj7}, {C, Quality::MinMaj7},
};

struct MelodyNote {
    int pitch;
    double beats;
};

// Sax head, section A (bars 1-8, reused at 9-16 and 25-32).
const std::vector<std::vector<MelodyNote>> kSaxA = {
    {{60,1.0},{63,2.0/3.0},{67,1.0/3.0},{70,1.0}},
    {{68,1.0},{67,2.0/3.0},{65,1.0/3.0},{63,1.0}},
    {{65,1.0},{68,2.0/3.0},{67,1.0/3.0},{65,1.0}},
    {{63,2.0},{67,1.0},{70,1.0}},
    {{60,1.0},{63,2.0/3.0},{57,1.0/3.0},{67,1.0}},
    {{66,1.0},{69,2.0/3.0},{72,1.0/3.0},{74,2.0}},
    {{67,1.0},{70,2.0/3.0},{74,1.0/3.0},{65,2.0}},
    {{74,1.0},{71,2.0/3.0},{67,1.0/3.0},{65,2.0}},
};
// Sax head, section B (bars 17-24).
const std::vector<std::vector<MelodyNote>> kSaxB = {
    {{63,1.0},{67,2.0/3.0},{70,1.0/3.0},{74,2.0}},
    {{63,1.0},{66,2.0/3.0},{70,1.0/3.0},{73,2.0}},
    {{68,1.0},{72,2.0/3.0},{75,1.0/3.0},{66,2.0}},
    {{61,1.0},{65,2.0/3.0},{68,1.0/3.0},{72,2.0}},
    {{62,1.0},{65,2.0/3.0},{68,1.0/3.0},{72,2.0}},
    {{71,1.0},{74,2.0/3.0},{77,1.0/3.0},{67,2.0}},
    {{60,1.0},{63,2.0/3.0},{67,1.0/3.0},{70,2.0}},
    {{74,1.0},{71,2.0/3.0},{67,1.0/3.0},{65,2.0}},
};
// Sax solo over the A changes (bars 33-40) - busier, higher energy.
const std::vector<std::vector<MelodyNote>> kSaxSolo = {
    {{60,2.0/3.0},{63,1.0/3.0},{67,2.0/3.0},{70,1.0/3.0},{72,1.0},{75,1.0}},
    {{65,2.0/3.0},{68,1.0/3.0},{72,2.0/3.0},{70,1.0/3.0},{68,1.0},{65,1.0}},
    {{62,2.0/3.0},{65,1.0/3.0},{68,2.0/3.0},{70,1.0/3.0},{74,1.0},{77,1.0}},
    {{75,2.0/3.0},{74,1.0/3.0},{70,2.0/3.0},{67,1.0/3.0},{63,1.0},{58,1.0}},
    {{72,2.0/3.0},{75,1.0/3.0},{67,2.0/3.0},{69,1.0/3.0},{60,1.0},{63,1.0}},
    {{66,2.0/3.0},{69,1.0/3.0},{72,2.0/3.0},{74,1.0/3.0},{78,1.0},{74,1.0}},
    {{70,2.0/3.0},{74,1.0/3.0},{77,2.0/3.0},{79,1.0/3.0},{77,1.0},{74,1.0}},
    {{71,1.0},{67,2.0/3.0},{62,1.0/3.0},{55,2.0}},
};
// Tag/vamp ending (bars 41-48).
const std::vector<std::vector<MelodyNote>> kSaxTag = {
    {{67,1.0},{70,1.0},{72,2.0}},
    {{77,1.0},{74,1.0},{71,2.0}},
    {{67,1.0},{70,1.0},{72,2.0}},
    {{77,1.0},{74,1.0},{71,2.0}},
    {{67,1.0},{70,1.0},{75,2.0}},
    {{77,1.0},{74,1.0},{71,2.0}},
    {{72,4.0}},
    {{72,4.0}},
};
// clang-format on

std::shared_ptr<MidiNote> makeNote(int pitch, float velocity, int64_t start, int64_t length) {
    auto note = std::make_shared<MidiNote>();
    note->pitch = pitch;
    note->velocity = velocity;
    note->startSample = start;
    note->lengthSamples = length;
    return note;
}

std::vector<ChordSym> buildFullProgression() {
    std::vector<ChordSym> full;
    auto append = [&](const std::vector<ChordSym>& section) {
        full.insert(full.end(), section.begin(), section.end());
    };
    append(kProgA);   // 1-8
    append(kProgA);   // 9-16
    append(kProgB);   // 17-24
    append(kProgA);   // 25-32
    append(kProgA);   // 33-40 (solo changes)
    append(kProgTag); // 41-48
    return full;
}

std::vector<std::vector<MelodyNote>> buildFullMelody() {
    std::vector<std::vector<MelodyNote>> full;
    auto append = [&](const std::vector<std::vector<MelodyNote>>& section) {
        full.insert(full.end(), section.begin(), section.end());
    };
    append(kSaxA);
    append(kSaxA);
    append(kSaxB);
    append(kSaxA);
    append(kSaxSolo);
    append(kSaxTag);
    return full;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::cerr << "usage: generate_jazz <output.rsdproj>\n";
        return 1;
    }

    Session session;
    session.sampleRate = kSampleRate;
    session.bpm = kBpm;

    auto progression = buildFullProgression();
    auto melody = buildFullMelody();

    // --- Tenor Sax: hand-composed head/solo/tag ---
    auto saxTrack = session.addTrack("Sax Melody");
    saxTrack->kind = TrackKind::Instrument;
    saxTrack->synthParams.instrumentProgram.store(66); // Tenor Sax

    {
        int64_t barStart = 0;
        for (auto& bar : melody) {
            int64_t offset = 0;
            for (auto& n : bar) {
                int64_t length = static_cast<int64_t>(n.beats * kBeatSamples);
                float velocity = n.beats >= 2.0 ? 0.7f : 0.85f;
                saxTrack->addMidiNote(makeNote(n.pitch, velocity, barStart + offset, length));
                offset += length;
            }
            barStart += kBarSamples;
        }
    }

    // --- Rhodes: Charleston-rhythm comping derived from the chart ---
    auto rhodesTrack = session.addTrack("Rhodes Comping");
    rhodesTrack->kind = TrackKind::Instrument;
    rhodesTrack->synthParams.instrumentProgram.store(4); // Electric Piano 1

    {
        int64_t barStart = 0;
        for (auto& chord : progression) {
            int third, fifth, seventh;
            intervalsFor(chord.quality, third, fifth, seventh);
            int rootPitch = 48 + chord.root;
            int tones[4] = {rootPitch, rootPitch + third, rootPitch + fifth, rootPitch + seventh};

            int64_t hit1Start = barStart;
            int64_t hit1Len = static_cast<int64_t>(1.5 * kBeatSamples);
            int64_t hit2Start = barStart + hit1Len;
            int64_t hit2Len = kBarSamples - hit1Len;

            for (int t : tones) {
                rhodesTrack->addMidiNote(makeNote(t, 0.45f, hit1Start, hit1Len));
                rhodesTrack->addMidiNote(makeNote(t, 0.4f, hit2Start, hit2Len));
            }
            barStart += kBarSamples;
        }
    }

    // --- Walking bass: root-3rd-5th-approach, derived from the chart ---
    auto bassTrack = session.addTrack("Walking Bass");
    bassTrack->kind = TrackKind::Instrument;
    bassTrack->synthParams.instrumentProgram.store(32); // Acoustic Bass

    {
        int64_t barStart = 0;
        for (size_t i = 0; i < progression.size(); ++i) {
            const ChordSym& chord = progression[i];
            int third, fifth, seventh;
            intervalsFor(chord.quality, third, fifth, seventh);
            (void)seventh;
            int rootPitch = 28 + chord.root;

            int nextRoot = (i + 1 < progression.size()) ? progression[i + 1].root : progression[0].root;
            int approach = 28 + nextRoot - 1; // chromatic approach from below

            int beatPitches[4] = {rootPitch, rootPitch + third, rootPitch + fifth, approach};
            for (int b = 0; b < 4; ++b) {
                bassTrack->addMidiNote(
                    makeNote(beatPitches[b], 0.6f, barStart + b * kBeatSamples, kBeatSamples));
            }
            barStart += kBarSamples;
        }
    }

    // --- Brushed drum kit: ride each beat, snare on 2 & 4, soft kick anchor ---
    auto drumTrack = session.addTrack("Brushed Drums");
    drumTrack->kind = TrackKind::Instrument;
    drumTrack->synthParams.isDrumKit.store(true);

    {
        constexpr int kRide = 51;
        constexpr int kSnare = 38;
        constexpr int kKick = 36;
        int64_t barStart = 0;
        int64_t hitLen = kBeatSamples / 4;
        for (size_t bar = 0; bar < progression.size(); ++bar) {
            for (int b = 0; b < 4; ++b) {
                drumTrack->addMidiNote(makeNote(kRide, 0.35f, barStart + b * kBeatSamples, hitLen));
            }
            drumTrack->addMidiNote(makeNote(kSnare, 0.3f, barStart + 1 * kBeatSamples, hitLen));
            drumTrack->addMidiNote(makeNote(kSnare, 0.3f, barStart + 3 * kBeatSamples, hitLen));
            if (bar % 2 == 0) {
                drumTrack->addMidiNote(makeNote(kKick, 0.25f, barStart, hitLen));
            }
            barStart += kBarSamples;
        }
    }

    QVector<LibraryEntry> emptyLibrary;
    if (!SessionIO::saveSession(argv[1], session, emptyLibrary)) {
        std::cerr << "Failed to save session.\n";
        return 1;
    }

    std::cout << "Wrote " << argv[1] << " (" << progression.size() << " bars, "
              << (progression.size() * kBarSamples / static_cast<double>(kSampleRate))
              << " seconds)\n";
    return 0;
}
