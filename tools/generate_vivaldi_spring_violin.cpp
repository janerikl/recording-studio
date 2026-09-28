// One-off generator for the first 30 seconds of Vivaldi's "The Four
// Seasons — Spring" (RV 269), 1st movement (Allegro), for a single solo
// Violin track (GM 40) — no piano, bus, drums, or effects. See PLAN.md,
// "Vivaldi Spring — solo violin, first 30s".
//
// Tempo/bar math: 96 BPM, 4/4 => 1 beat = 0.625 s = 30000 samples @ 48kHz,
// 1 bar = 120000 samples = 2.5 s. 12 bars = 30.0 s exactly. Every note
// duration is a multiple of a 32nd (3750 samples), so nothing rounds.
//
// Form (following the opening of the movement):
//   bars 0-3   Ritornello theme, forte, with double-stops on long notes
//   bars 4-7   The same theme echoed piano (the tutti's forte/piano echo)
//   bars 8-11  Start of "Il Canto de gl'Augelli" — high trills and chirps
//
// Double-stops stand in for the missing orchestra: long melody notes get a
// lower chord tone, and phrase ends get a full E major chord. Nothing goes
// below G3 (MIDI 55), the violin's lowest open string.

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
constexpr double kBpm = 96.0;
constexpr int64_t kBeatSamples = static_cast<int64_t>(kSampleRate * 60.0 / kBpm); // 30000
constexpr int64_t kBarSamples = 4 * kBeatSamples;                                 // 120000 (2.5s)
constexpr int kNumBars = 12;

// MIDI pitches used below.
constexpr int B3 = 59, Gs4 = 68, B4 = 71, Ds5 = 75, E5 = 76, Fs5 = 78, Gs5 = 80, A5 = 81,
              B5 = 83, Cs6 = 85, Ds6 = 87, E6 = 88, Fs6 = 90, Gs6 = 92, A6 = 93, B6 = 95;

constexpr float kForte = 0.9f;
constexpr float kPiano = 0.45f;
constexpr float kBirds = 0.6f;

// One violin event. `lower`/`lower2` add double/triple-stop voices under
// `pitch` (0 = none).
struct Note {
    int pitch;
    double beats;
    int lower = 0;
    int lower2 = 0;
};

// Ritornello theme, 4 bars. Each bar sums to 4 beats.
const std::vector<Note> kTheme = {
    // Bar 1: E, then the repeated-G# figure, landing on B.
    {E5, 1.0, Gs4}, {Gs5, 0.5}, {Gs5, 0.5}, {Gs5, 0.5}, {Fs5, 0.25}, {E5, 0.25}, {B5, 1.0, E5},
    // Bar 2: held B, turn, figure again.
    {B5, 1.5, E5}, {B5, 0.25}, {A5, 0.25}, {Gs5, 0.5}, {Gs5, 0.5}, {Gs5, 0.5}, {Fs5, 0.25}, {E5, 0.25},
    // Bar 3: held B, then the stepwise answer.
    {B5, 1.5, E5}, {B5, 0.25}, {A5, 0.25}, {Gs5, 0.5}, {A5, 0.5}, {B5, 0.5}, {A5, 0.5},
    // Bar 4: descent to the cadence, closing on an E major chord.
    {Gs5, 0.5}, {Fs5, 0.5}, {Ds5, 0.5}, {B4, 0.5}, {E5, 2.0, Gs4, B3},
};

class ViolinWriter {
public:
    explicit ViolinWriter(std::shared_ptr<Track> track) : m_track(std::move(track)) {}

    void play(const Note& n, float velocity) {
        int64_t len = beatsToSamples(n.beats);
        add(n.pitch, velocity, len);
        if (n.lower) add(n.lower, velocity * 0.85f, len);
        if (n.lower2) add(n.lower2, velocity * 0.8f, len);
        m_cursor += len;
    }

    // Alternating 32nd notes between `note` and its upper neighbour.
    void trill(int note, int upper, double beats, float velocity) {
        int count = static_cast<int>(beats * 8);
        for (int i = 0; i < count; ++i) play({i % 2 == 0 ? note : upper, 0.125}, velocity);
    }

    int64_t cursor() const { return m_cursor; }

private:
    static int64_t beatsToSamples(double beats) {
        return static_cast<int64_t>(beats * kBeatSamples + 0.5);
    }

    void add(int pitch, float velocity, int64_t len) {
        auto note = std::make_shared<MidiNote>();
        note->pitch = pitch;
        note->velocity = velocity;
        note->startSample = m_cursor;
        note->lengthSamples = len;
        m_track->addMidiNote(note);
    }

    std::shared_ptr<Track> m_track;
    int64_t m_cursor = 0;
};

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::cerr << "usage: generate_vivaldi_spring_violin <output-path-without-extension>\n";
        return 1;
    }
    const QString outBase = QString::fromUtf8(argv[1]);

    Session session;
    session.sampleRate = kSampleRate;
    session.bpm = kBpm;

    auto violin = session.addTrack("Violin");
    violin->kind = TrackKind::Instrument;
    violin->synthParams.instrumentProgram.store(40); // GM program 40 = Violin
    violin->volume.store(0.9f);
    violin->pan.store(0.0f);

    ViolinWriter w(violin);

    // Bars 0-3: theme, forte. Bars 4-7: the echo, piano.
    for (const auto& n : kTheme) w.play(n, kForte);
    for (const auto& n : kTheme) w.play(n, kPiano);

    // Bars 8-11: birdsong entry — trills and repeated high chirps.
    w.trill(E6, Fs6, 2.0, kBirds);
    for (int p : {E6, B5, E6, B5}) w.play({p, 0.5}, kBirds);

    w.trill(Gs6, A6, 2.0, kBirds);
    for (int i = 0; i < 4; ++i) w.play({Gs6, 0.25}, kBirds);
    w.play({E6, 1.0, Gs5}, kBirds);

    w.trill(B5, Cs6, 1.0, kBirds);
    for (int p : {B5, E6, Gs6, B6}) w.play({p, 0.25}, kBirds);
    w.trill(E6, Fs6, 1.0, kBirds);
    w.play({Gs6, 0.5}, kBirds);
    w.play({E6, 0.5}, kBirds);

    w.trill(E6, Fs6, 1.5, kBirds);
    w.play({Ds6, 0.25}, kBirds);
    w.play({E6, 0.25}, kBirds);
    w.play({E6, 2.0, Gs5}, kBirds);

    if (w.cursor() != kNumBars * kBarSamples) {
        std::cerr << "Composition length mismatch: " << w.cursor() << " samples, expected "
                  << kNumBars * kBarSamples << "\n";
        return 1;
    }

    session.markers[1] = 0;                // Ritornello
    session.markers[2] = 4 * kBarSamples;  // Echo
    session.markers[3] = 8 * kBarSamples;  // Birds

    QVector<LibraryEntry> emptyLibrary;
    const QString rsdPath = outBase + ".rsdproj";
    if (!SessionIO::saveSession(rsdPath, session, emptyLibrary)) {
        std::cerr << "Failed to save session: " << qPrintable(rsdPath) << "\n";
        return 1;
    }

    // Same precaution as MainWindow::onExportClicked: start the synth clean.
    for (auto& track : session.tracks) track->synthEngine.reset();

    int64_t lengthSamples = sessionContentLengthSamples(session);
    auto mixdown = renderSessionMixdown(session, static_cast<unsigned int>(session.sampleRate),
                                         static_cast<unsigned int>(session.channels),
                                         lengthSamples);
    const QString wavPath = outBase + ".wav";
    if (!mixdown || !AudioFileIO::writeFile(wavPath, *mixdown, ExportFormat::Wav32Float)) {
        std::cerr << "Failed to render/write: " << qPrintable(wavPath) << "\n";
        return 1;
    }

    std::cout << "Vivaldi Spring, mvt. 1 opening (solo violin): " << kNumBars << " bars @ "
              << kBpm << " BPM\n"
              << "Wrote " << qPrintable(rsdPath) << "\n"
              << "Wrote " << qPrintable(wavPath) << " ("
              << (lengthSamples / static_cast<double>(kSampleRate)) << " seconds)\n";
    return 0;
}
