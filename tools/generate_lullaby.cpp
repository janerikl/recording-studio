// One-off generator for a ~90-second children's nighttime lullaby, built
// entirely from this app's own project schema (instrument tracks + MIDI
// notes) so it opens and plays like any hand-authored session. See
// PLAN.md, "MIDI note persistence + generated nighttime lullaby".
//
// 30 bars of 3/4 at 60 BPM (1 beat = 1 second = 48000 samples, so 1 bar =
// 144000 samples) = exactly 90 seconds. C major. Melody on Music Box (GM
// program 10) over sustained root-position triads on Piano (GM 0).

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
constexpr int64_t kBeatSamples = kSampleRate; // 60 BPM -> 1 beat == 1 second

struct MelodyNote {
    int pitch;
    double beats;
};

struct Chord {
    int root, third, fifth;
};

const Chord kC{48, 52, 55};
const Chord kF{53, 57, 60};
const Chord kG{55, 59, 62};
const Chord kAm{57, 60, 64};
const Chord kDm{50, 53, 57};

// clang-format off
const std::vector<std::vector<MelodyNote>> kMelodyBars = {
    {{60,1},{62,1},{64,1}},           // 1
    {{65,1},{64,1},{62,1}},           // 2
    {{60,1},{64,1},{67,1}},           // 3
    {{67,2},{65,1}},                  // 4
    {{64,1},{62,1},{60,1}},           // 5
    {{62,1},{64,1},{65,1}},           // 6
    {{67,1},{64,1},{60,1}},           // 7
    {{60,3}},                         // 8
    {{60,1},{64,1},{67,1}},           // 9
    {{69,1},{67,1},{65,1}},           // 10
    {{64,1},{62,1},{60,1}},           // 11
    {{67,2},{65,1}},                  // 12
    {{64,1},{65,1},{67,1}},           // 13
    {{69,1},{67,1},{64,1}},           // 14
    {{62,1},{60,1},{62,1}},           // 15
    {{60,3}},                         // 16
    {{64,1},{67,1},{72,1}},           // 17
    {{71,1},{67,1},{64,1}},           // 18
    {{65,1},{69,1},{72,1}},           // 19
    {{72,2},{71,1}},                  // 20
    {{69,1},{65,1},{62,1}},           // 21
    {{64,1},{67,1},{64,1}},           // 22
    {{62,1},{60,1},{59,1}},           // 23
    {{60,3}},                         // 24
    {{60,1},{62,1},{64,1}},           // 25
    {{65,1},{64,1},{62,1}},           // 26
    {{60,1},{64,1},{67,1}},           // 27
    {{67,2},{64,1}},                  // 28
    {{62,1},{60,1},{62,1}},           // 29
    {{60,3}},                         // 30
};

const std::vector<Chord> kChordBars = {
    kC, kF, kG, kC,
    kAm, kF, kG, kC,
    kC, kF, kG, kC,
    kAm, kF, kG, kC,
    kC, kG, kF, kC,
    kAm, kDm, kG, kC,
    kC, kF, kG, kC,
    kG, kC,
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
        std::cerr << "usage: generate_lullaby <output.rsdproj>\n";
        return 1;
    }

    Session session;
    session.sampleRate = kSampleRate;
    session.bpm = 60.0;

    auto melodyTrack = session.addTrack("Lullaby Melody");
    melodyTrack->kind = TrackKind::Instrument;
    melodyTrack->synthParams.instrumentProgram.store(10); // Music Box

    int64_t barStart = 0;
    for (auto& bar : kMelodyBars) {
        int64_t offset = 0;
        for (auto& n : bar) {
            int64_t length = static_cast<int64_t>(n.beats * kBeatSamples);
            // Long notes (bar-length or dotted-half) phrase-end softer.
            float velocity = n.beats >= 2.0 ? 0.55f : 0.7f;
            melodyTrack->addMidiNote(makeNote(n.pitch, velocity, barStart + offset, length));
            offset += length;
        }
        barStart += 3 * kBeatSamples;
    }

    auto chordTrack = session.addTrack("Lullaby Chords");
    chordTrack->kind = TrackKind::Instrument;
    chordTrack->synthParams.instrumentProgram.store(0); // Acoustic Grand Piano

    barStart = 0;
    for (auto& chord : kChordBars) {
        int64_t barLength = 3 * kBeatSamples;
        chordTrack->addMidiNote(makeNote(chord.root, 0.4f, barStart, barLength));
        chordTrack->addMidiNote(makeNote(chord.third, 0.4f, barStart, barLength));
        chordTrack->addMidiNote(makeNote(chord.fifth, 0.4f, barStart, barLength));
        barStart += barLength;
    }

    QVector<LibraryEntry> emptyLibrary;
    if (!SessionIO::saveSession(argv[1], session, emptyLibrary)) {
        std::cerr << "Failed to save session.\n";
        return 1;
    }

    std::cout << "Wrote " << argv[1] << " (" << kMelodyBars.size() << " bars, "
              << (kMelodyBars.size() * 3 * kBeatSamples / static_cast<double>(kSampleRate))
              << " seconds)\n";
    return 0;
}
