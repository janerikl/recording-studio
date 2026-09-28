// One-off generator for a ~90-second country song, reusing the same
// project schema (instrument tracks + MIDI notes) proven out by
// generate_lullaby.cpp. See PLAN.md, "Generated country song".
//
// 45 bars of 4/4 at 120 BPM (1 beat = 0.5s = 24000 samples, so 1 bar =
// 96000 samples) = exactly 90 seconds. G major. Lead melody on Acoustic
// Guitar (steel) over sustained triads on Banjo, following a country
// I-IV-I-V progression with a ii/vi bridge for contrast.

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
constexpr double kBpm = 120.0;
constexpr int64_t kBeatSamples = static_cast<int64_t>(kSampleRate * 60.0 / kBpm); // 24000

struct MelodyNote {
    int pitch;
    double beats;
};

struct Chord {
    int root, third, fifth;
};

const Chord kG{55, 59, 62};
const Chord kC{48, 52, 55};
const Chord kD{50, 54, 57};
const Chord kEm{52, 55, 59};
const Chord kAm{57, 60, 64};

// clang-format off
const std::vector<std::vector<MelodyNote>> kMelodyBars = {
    // Verse 1 (1-8)
    {{67,1},{69,1},{71,1},{74,1}},
    {{74,1},{71,1},{69,1},{67,1}},
    {{64,1},{67,1},{71,1},{74,1}},
    {{71,2},{67,2}},
    {{67,1},{69,1},{71,1},{72,1}},
    {{71,1},{69,1},{67,1},{64,1}},
    {{62,1},{64,1},{66,1},{67,1}},
    {{67,4}},
    // Chorus 1 (9-16)
    {{74,1},{72,1},{71,1},{69,1}},
    {{67,1},{71,1},{74,1},{79,1}},
    {{79,1},{78,1},{76,1},{74,1}},
    {{72,1},{71,1},{69,1},{67,1}},
    {{69,1},{72,1},{71,1},{67,1}},
    {{69,1},{71,1},{72,1},{74,1}},
    {{71,1},{67,1},{74,1},{71,1}},
    {{67,4}},
    // Verse 2 (17-24) - same as verse 1
    {{67,1},{69,1},{71,1},{74,1}},
    {{74,1},{71,1},{69,1},{67,1}},
    {{64,1},{67,1},{71,1},{74,1}},
    {{71,2},{67,2}},
    {{67,1},{69,1},{71,1},{72,1}},
    {{71,1},{69,1},{67,1},{64,1}},
    {{62,1},{64,1},{66,1},{67,1}},
    {{67,4}},
    // Chorus 2 (25-32) - same as chorus 1
    {{74,1},{72,1},{71,1},{69,1}},
    {{67,1},{71,1},{74,1},{79,1}},
    {{79,1},{78,1},{76,1},{74,1}},
    {{72,1},{71,1},{69,1},{67,1}},
    {{69,1},{72,1},{71,1},{67,1}},
    {{69,1},{71,1},{72,1},{74,1}},
    {{71,1},{67,1},{74,1},{71,1}},
    {{67,4}},
    // Bridge (33-40)
    {{76,1},{74,1},{72,1},{71,1}},
    {{69,1},{67,1},{66,1},{64,1}},
    {{62,1},{64,1},{66,1},{67,1}},
    {{69,2},{71,2}},
    {{72,1},{71,1},{69,1},{67,1}},
    {{66,1},{67,1},{69,1},{71,1}},
    {{72,1},{74,1},{71,1},{67,1}},
    {{74,4}},
    // Outro (41-44)
    {{67,1},{71,1},{74,1},{79,1}},
    {{79,1},{74,1},{71,1},{67,1}},
    {{69,1},{71,1},{72,1},{74,1}},
    {{67,4}},
    // Final ring (45)
    {{67,4}},
};

const std::vector<Chord> kChordBars = {
    // Verse 1
    kG, kC, kG, kD, kG, kC, kD, kG,
    // Chorus 1
    kC, kG, kD, kG, kC, kG, kD, kG,
    // Verse 2
    kG, kC, kG, kD, kG, kC, kD, kG,
    // Chorus 2
    kC, kG, kD, kG, kC, kG, kD, kG,
    // Bridge
    kEm, kC, kG, kD, kAm, kC, kD, kG,
    // Outro
    kG, kD, kC, kG,
    // Final ring
    kG,
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

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::cerr << "usage: generate_country <output.rsdproj>\n";
        return 1;
    }

    Session session;
    session.sampleRate = kSampleRate;
    session.bpm = kBpm;

    auto melodyTrack = session.addTrack("Country Melody");
    melodyTrack->kind = TrackKind::Instrument;
    melodyTrack->synthParams.instrumentProgram.store(25); // Acoustic Guitar (steel)

    int64_t barStart = 0;
    for (auto& bar : kMelodyBars) {
        int64_t offset = 0;
        for (auto& n : bar) {
            int64_t length = static_cast<int64_t>(n.beats * kBeatSamples);
            float velocity = n.beats >= 2.0 ? 0.65f : 0.8f;
            melodyTrack->addMidiNote(makeNote(n.pitch, velocity, barStart + offset, length));
            offset += length;
        }
        barStart += 4 * kBeatSamples;
    }

    auto chordTrack = session.addTrack("Country Rhythm");
    chordTrack->kind = TrackKind::Instrument;
    chordTrack->synthParams.instrumentProgram.store(105); // Banjo

    barStart = 0;
    for (auto& chord : kChordBars) {
        int64_t barLength = 4 * kBeatSamples;
        chordTrack->addMidiNote(makeNote(chord.root, 0.5f, barStart, barLength));
        chordTrack->addMidiNote(makeNote(chord.third, 0.5f, barStart, barLength));
        chordTrack->addMidiNote(makeNote(chord.fifth, 0.5f, barStart, barLength));
        barStart += barLength;
    }

    QVector<LibraryEntry> emptyLibrary;
    if (!SessionIO::saveSession(argv[1], session, emptyLibrary)) {
        std::cerr << "Failed to save session.\n";
        return 1;
    }

    std::cout << "Wrote " << argv[1] << " (" << kMelodyBars.size() << " bars, "
              << (kMelodyBars.size() * 4 * kBeatSamples / static_cast<double>(kSampleRate))
              << " seconds)\n";
    return 0;
}
