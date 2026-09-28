// General-purpose song generator: given an arbitrary genre name, builds a
// multi-track Session (lead + harmony instrument tracks feeding a bus, plus
// a drum track when the genre calls for one), saves it as a .rsdproj, and
// renders it to a real .wav via the same OfflineRenderer path File > Export
// uses. See PLAN.md, "Genre-driven song-generation skill".
//
// Usage: generate_song <genre> <output-path-without-extension>
//   Writes <output-path>.rsdproj and <output-path>.wav.
//
// Unlike the earlier one-off generate_country/generate_jazz/generate_lullaby/
// generate_techno_vivaldi tools (which only ever wrote a .rsdproj), this
// tool also renders audio, and its instrumentation/tempo/key/progression
// come from model/GenrePreset.h's preset table (with a deterministic
// hash-based fallback for unrecognized genre strings) rather than being
// hand-composed per song.

#include <QCoreApplication>
#include <QString>
#include <QVector>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "audio/Effects.h"
#include "audio/GMDrumMap.h"
#include "audio/OfflineRenderer.h"
#include "io/AudioFileIO.h"
#include "io/SessionIO.h"
#include "model/GenrePreset.h"
#include "model/Session.h"

using namespace rsd;

namespace {

constexpr int kSampleRate = 48000;

// Major-scale intervals from the root, indexed by scale degree (0-6),
// extended past an octave for degrees >= 7 so a chordDegrees entry can
// reach a step above the tonic (e.g. degree 7 = octave-up root).
int scaleStepSemitones(int degree) {
    static const int kMajorSteps[7] = {0, 2, 4, 5, 7, 9, 11};
    int octave = degree / 7;
    int idx = degree % 7;
    if (idx < 0) { idx += 7; octave -= 1; }
    return octave * 12 + kMajorSteps[idx];
}

std::shared_ptr<MidiNote> makeNote(int pitch, float velocity, int64_t start, int64_t length) {
    auto note = std::make_shared<MidiNote>();
    note->pitch = std::clamp(pitch, 0, 127);
    note->velocity = velocity;
    note->startSample = start;
    note->lengthSamples = length;
    return note;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 3) {
        std::cerr << "usage: generate_song <genre> <output-path-without-extension>\n";
        return 1;
    }

    const QString genre = QString::fromUtf8(argv[1]);
    const QString outBase = QString::fromUtf8(argv[2]);

    GenrePreset preset = presetForGenre(genre);

    const int64_t beatSamples = static_cast<int64_t>(kSampleRate * 60.0 / preset.bpm);
    const int64_t barSamples = 4 * beatSamples;

    // Aim for roughly 60-120 seconds: pick a bar count near 90s of content,
    // clamped to at least 16 bars and at most 96.
    int bars = static_cast<int>((90.0 * kSampleRate) / static_cast<double>(barSamples));
    bars = std::clamp(bars, 16, 96);

    Session session;
    session.sampleRate = kSampleRate;
    session.bpm = preset.bpm;

    // --- Bus track: everything routes here, with one effect on it. ---
    auto busTrack = session.addTrack(genre + " Bus");
    busTrack->kind = TrackKind::Bus;
    busTrack->volume.store(0.9f);
    {
        auto reverb = std::make_shared<ReverbEffect>();
        reverb->roomSize.store(0.4f);
        reverb->mix.store(0.2f);
        reverb->prepare(kSampleRate);
        busTrack->addEffect(reverb);
    }

    // --- Lead melody track: one note per beat, walking the chord tones. ---
    auto leadTrack = session.addTrack(genre + " Lead");
    leadTrack->kind = TrackKind::Instrument;
    leadTrack->synthParams.instrumentProgram.store(preset.leadInstrumentProgram);
    leadTrack->volume.store(0.85f);
    leadTrack->pan.store(-0.15f);
    leadTrack->setSendBusId(busTrack->id);
    leadTrack->sendLevel.store(0.3f);
    {
        auto eq = std::make_shared<EqEffect>();
        eq->midGainDb.store(2.0f);
        eq->prepare(kSampleRate);
        leadTrack->addEffect(eq);
    }

    // --- Harmony track: sustained triads under the melody. ---
    auto harmonyTrack = session.addTrack(genre + " Harmony");
    harmonyTrack->kind = TrackKind::Instrument;
    harmonyTrack->synthParams.instrumentProgram.store(preset.harmonyInstrumentProgram);
    harmonyTrack->volume.store(0.6f);
    harmonyTrack->pan.store(0.15f);
    harmonyTrack->setSendBusId(busTrack->id);
    harmonyTrack->sendLevel.store(0.25f);
    {
        auto comp = std::make_shared<CompressorEffect>();
        comp->thresholdDb.store(-16.0f);
        comp->ratio.store(3.0f);
        comp->prepare(kSampleRate);
        harmonyTrack->addEffect(comp);
    }

    // --- Optional drum track. ---
    std::shared_ptr<Track> drumTrack;
    if (preset.useDrumKit) {
        drumTrack = session.addTrack(genre + " Drums");
        drumTrack->kind = TrackKind::Instrument;
        drumTrack->synthParams.isDrumKit.store(true);
        drumTrack->volume.store(0.8f);
        drumTrack->setSendBusId(busTrack->id);
        drumTrack->sendLevel.store(0.2f);
    }

    const int numDegrees = preset.chordDegrees.size();
    const int kick = 36, snare = 38, hihat = 42;

    for (int bar = 0; bar < bars; ++bar) {
        int degree = preset.chordDegrees[bar % numDegrees];
        int chordRoot = preset.rootMidiNote + scaleStepSemitones(degree);
        int third = preset.rootMidiNote + scaleStepSemitones(degree + 2);
        int fifth = preset.rootMidiNote + scaleStepSemitones(degree + 4);

        int64_t barStart = static_cast<int64_t>(bar) * barSamples;

        // Lead: arpeggiate the chord tones, one per beat, an octave up.
        const int arp[4] = {chordRoot + 12, third + 12, fifth + 12, third + 12};
        for (int beat = 0; beat < 4; ++beat) {
            int64_t start = barStart + beat * beatSamples;
            float velocity = (beat == 0) ? 0.9f : 0.7f;
            leadTrack->addMidiNote(
                makeNote(arp[beat], velocity, start, static_cast<int64_t>(beatSamples * 0.9)));
        }

        // Harmony: one sustained triad per bar.
        harmonyTrack->addMidiNote(makeNote(chordRoot, 0.5f, barStart, barSamples));
        harmonyTrack->addMidiNote(makeNote(third, 0.5f, barStart, barSamples));
        harmonyTrack->addMidiNote(makeNote(fifth, 0.5f, barStart, barSamples));

        if (drumTrack) {
            int64_t sixteenth = beatSamples / 4;
            drumTrack->addMidiNote(makeNote(kick, 0.9f, barStart, sixteenth));
            drumTrack->addMidiNote(makeNote(kick, 0.7f, barStart + 2 * beatSamples, sixteenth));
            drumTrack->addMidiNote(makeNote(snare, 0.85f, barStart + beatSamples, sixteenth));
            drumTrack->addMidiNote(
                makeNote(snare, 0.85f, barStart + 3 * beatSamples, sixteenth));
            for (int e = 0; e < 8; ++e) {
                drumTrack->addMidiNote(
                    makeNote(hihat, 0.5f, barStart + e * (beatSamples / 2), sixteenth));
            }
        }
    }

    // Markers: an "Intro" at the top and a "Main" a quarter of the way in,
    // mirroring the verse/chorus-style arrangement markers used elsewhere.
    session.markers[1] = 0;
    session.markers[2] = static_cast<int64_t>(bars / 4) * barSamples;

    QVector<LibraryEntry> emptyLibrary;
    const QString rsdPath = outBase + ".rsdproj";
    if (!SessionIO::saveSession(rsdPath, session, emptyLibrary)) {
        std::cerr << "Failed to save session: " << qPrintable(rsdPath) << "\n";
        return 1;
    }

    // Reset every Instrument track's SynthEngine before rendering, same
    // precaution MainWindow::onExportClicked takes: a fresh generator
    // process never touched the live-note queue, but this keeps the
    // rendering path identical to the app's own export.
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

    std::cout << "Genre: " << qPrintable(genre) << " (bpm=" << preset.bpm << ", bars=" << bars
               << ")\n"
               << "Wrote " << qPrintable(rsdPath) << "\n"
               << "Wrote " << qPrintable(wavPath) << " ("
               << (lengthSamples / static_cast<double>(kSampleRate)) << " seconds)\n";
    return 0;
}
