// Generator for the opening of Vivaldi's "Spring" (La Primavera, Op. 8
// No. 1, RV 269, 1st mvt, Allegro) for a single solo Violin track (GM 40),
// with notes read from a real score and an expression layer on top, so the
// playback sounds less mechanical. See PLAN.md, "Expressive solo violin
// from a real score".
//
// Score source: assets/scores/vivaldi_spring_mvt1_mutopia.mid — Mutopia
// Project, piece 301, CC BY-SA 3.0
// (https://www.mutopiaproject.org/cgibin/piece-info.cgi?id=301,
// https://creativecommons.org/licenses/by-sa/3.0/). The generated
// .rsdproj/.wav are adaptations of that score and are licensed under the
// same CC BY-SA 3.0 terms (see assets/scores/README.md).
//
// What it does:
//   - Parses the format-1 SMF with a small built-in reader (chunks, VLQ
//     delta times, running status, meta/sysex skipping, note-on velocity 0
//     = note-off). Uses the file's PPQ (384) and first tempo (115 BPM).
//   - Keeps the "solo" track's notes that start in bars 1-15 (4/4), clipped
//     to end at the end of bar 15 (~31.30s). Timing stays as written.
//   - Expression (CC11 lane): phrase dynamics — bars 1-3 forte, bars 4-6
//     the soft echo, bars 7-13 moderate with a gentle arch, bars 14-15
//     (birdsong entry) light — times a per-note bow swell on notes >= 0.4s
//     (slight dip at the attack, rise, ease off before the release). Short
//     notes are flat at the phrase level, so fast passages don't pump.
//   - Vibrato (CC1 lane): delayed — none for the first 180ms of notes
//     longer than 300ms, then a 250ms ramp to moderate depth. Short notes
//     get none.
//   - Legato: each note overlaps the next by 20ms when the next note has a
//     different pitch and follows without a rest; same-pitch neighbours
//     never overlap.
//   - Velocity: phrase base, deterministic (seeded) +-0.03 variation, and a
//     slight accent on beat 1 of each bar.
//
// FluidSynth's CC11 curve gives gain = expression^2 (40*log10(e) dB), which
// the phrase levels below account for.

#include <QCoreApplication>
#include <QString>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "audio/OfflineRenderer.h"
#include "io/AudioFileIO.h"
#include "io/SessionIO.h"
#include "model/Session.h"

using namespace rsd;

namespace {

constexpr int kSampleRate = 48000;
constexpr int kNumBars = 15;
constexpr int kBeatsPerBar = 4;
constexpr int kViolinProgram = 40;

const char* kScorePath = RSD_SOURCE_DIR "/assets/scores/vivaldi_spring_mvt1_mutopia.mid";
const char* kDefaultOutBase = "/home/janel/Music/vivaldi_spring_violin_expressive";

// ---------------------------------------------------------------------------
// Minimal Standard MIDI File reader.

struct SmfNote {
    int64_t startTick;
    int64_t endTick;
    int pitch;
    int velocity;
};

struct SmfTrack {
    std::string name;
    std::vector<SmfNote> notes;
};

struct SmfFile {
    int format = 0;
    int ppq = 0;
    std::vector<double> tempiBpm; // every tempo meta event, in file order
    std::vector<int64_t> tempoTicks;
    std::vector<SmfTrack> tracks;
};

class ByteReader {
public:
    ByteReader(const std::vector<uint8_t>& data, size_t pos, size_t end) : m_d(data), m_pos(pos), m_end(end) {}
    bool atEnd() const { return m_pos >= m_end; }
    size_t pos() const { return m_pos; }
    uint8_t peek() const { return m_pos < m_end ? m_d[m_pos] : 0; }
    uint8_t byte() {
        if (m_pos >= m_end) throw std::runtime_error("unexpected end of chunk");
        return m_d[m_pos++];
    }
    uint32_t vlq() {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            uint8_t b = byte();
            v = (v << 7) | (b & 0x7f);
            if (!(b & 0x80)) return v;
        }
        throw std::runtime_error("VLQ longer than 4 bytes");
    }
    void skip(size_t n) {
        if (n > m_end - m_pos) throw std::runtime_error("skip past end of chunk");
        m_pos += n;
    }

private:
    const std::vector<uint8_t>& m_d;
    size_t m_pos;
    size_t m_end;
};

uint32_t be32(const std::vector<uint8_t>& d, size_t i) {
    return (uint32_t(d[i]) << 24) | (uint32_t(d[i + 1]) << 16) | (uint32_t(d[i + 2]) << 8) | d[i + 3];
}
uint16_t be16(const std::vector<uint8_t>& d, size_t i) { return uint16_t((d[i] << 8) | d[i + 1]); }

SmfFile readSmf(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (d.size() < 14 || std::string(d.begin(), d.begin() + 4) != "MThd") throw std::runtime_error("not an SMF");

    SmfFile smf;
    uint32_t headerLen = be32(d, 4);
    smf.format = be16(d, 8);
    int trackCount = be16(d, 10);
    uint16_t division = be16(d, 12);
    if (division & 0x8000) throw std::runtime_error("SMPTE time division not supported");
    smf.ppq = division;

    size_t i = 8 + headerLen;
    for (int t = 0; t < trackCount && i + 8 <= d.size(); ++t) {
        std::string id(d.begin() + i, d.begin() + i + 4);
        uint32_t len = be32(d, i + 4);
        size_t start = i + 8, end = std::min(d.size(), start + len);
        i = start + len;
        if (id != "MTrk") { --t; continue; } // unknown chunk: skip, doesn't count as a track

        SmfTrack track;
        ByteReader r(d, start, end);
        int64_t tick = 0;
        uint8_t running = 0;
        // Pending note-ons per (channel, pitch), matched FIFO to note-offs.
        std::map<int, std::vector<std::pair<int64_t, int>>> open;
        while (!r.atEnd()) {
            tick += r.vlq();
            uint8_t status = r.peek();
            if (status == 0xff) { // meta
                r.byte();
                uint8_t type = r.byte();
                uint32_t n = r.vlq();
                size_t at = r.pos();
                r.skip(n);
                if (type == 0x03 && track.name.empty()) track.name.assign(d.begin() + at, d.begin() + at + n);
                if (type == 0x51 && n == 3) {
                    uint32_t usPerQuarter = (uint32_t(d[at]) << 16) | (uint32_t(d[at + 1]) << 8) | d[at + 2];
                    smf.tempiBpm.push_back(60e6 / usPerQuarter);
                    smf.tempoTicks.push_back(tick);
                }
                if (type == 0x2f) break; // end of track
                continue;
            }
            if (status == 0xf0 || status == 0xf7) { // sysex / escape
                r.byte();
                r.skip(r.vlq());
                continue;
            }
            if (status & 0x80) {
                running = r.byte();
            } else if (!running) {
                throw std::runtime_error("running status without a previous status byte");
            }
            uint8_t hi = running & 0xf0;
            int channel = running & 0x0f;
            int dataBytes = (hi == 0xc0 || hi == 0xd0) ? 1 : 2;
            uint8_t a = r.byte();
            uint8_t b = dataBytes == 2 ? r.byte() : 0;

            bool noteOn = hi == 0x90 && b > 0;
            bool noteOff = hi == 0x80 || (hi == 0x90 && b == 0);
            int key = channel * 128 + a;
            if (noteOn) {
                open[key].push_back({tick, b});
            } else if (noteOff) {
                auto& pending = open[key];
                if (!pending.empty()) {
                    track.notes.push_back({pending.front().first, tick, a, pending.front().second});
                    pending.erase(pending.begin());
                }
            }
        }
        std::sort(track.notes.begin(), track.notes.end(), [](const SmfNote& x, const SmfNote& y) {
            return x.startTick != y.startTick ? x.startTick < y.startTick : x.pitch < y.pitch;
        });
        smf.tracks.push_back(std::move(track));
    }
    return smf;
}

// ---------------------------------------------------------------------------
// Expression layer.

// Deterministic LCG so the output is reproducible on any platform/stdlib.
class SeededRandom {
public:
    explicit SeededRandom(uint32_t seed) : m_state(seed) {}
    // Uniform in [-1, 1].
    double symmetric() {
        m_state = m_state * 1664525u + 1013904223u;
        return (m_state >> 8) / double(1u << 24) * 2.0 - 1.0;
    }

private:
    uint32_t m_state;
};

// Phrase dynamics as a CC11 level (gain = level^2). `bar` is 0-based.
double phraseLevel(int64_t tick, int ppq) {
    const int64_t barTicks = int64_t(kBeatsPerBar) * ppq;
    int bar = static_cast<int>(tick / barTicks);
    if (bar < 3) return 1.0;   // bars 1-3: forte (0 dB)
    if (bar < 6) return 0.66;  // bars 4-6: the echo, piano (~-7 dB, plus softer velocity)
    if (bar < 13) {            // bars 7-13: moderate, gentle rise and fall (~-5..-2 dB)
        double x = double(tick - 6 * barTicks) / double(7 * barTicks);
        return 0.74 + 0.14 * std::sin(M_PI * x);
    }
    return 0.66;               // bars 14-15: birdsong entry, light (~-7 dB)
}

float phraseVelocity(int64_t tick, int ppq) {
    int bar = static_cast<int>(tick / (int64_t(kBeatsPerBar) * ppq));
    if (bar < 3) return 0.80f;
    if (bar < 6) return 0.62f;
    if (bar < 13) return 0.72f;
    return 0.64f;
}

double vibratoDepthFor(int64_t tick, int ppq) {
    int bar = static_cast<int>(tick / (int64_t(kBeatsPerBar) * ppq));
    return bar >= 3 && bar < 6 ? 0.34 : 0.42; // CC1 127 ~ +-50 cents, so ~+-17..21 cents
}

struct ScoredNote {
    int pitch;
    int64_t startTick;
    int64_t endTick; // clipped to the end of bar 15
    int64_t start;   // samples
    int64_t end;     // written end, samples
};

// Adds a point, replacing any existing one at the same sample.
void addPoint(std::map<int64_t, float>& points, int64_t sample, double value) {
    points[std::max<int64_t>(0, sample)] = static_cast<float>(std::clamp(value, 0.0, 1.0));
}

std::vector<AutomationPoint> toPoints(const std::map<int64_t, float>& m) {
    std::vector<AutomationPoint> out;
    for (auto& [s, v] : m) out.push_back({s, v});
    return out;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const QString outBase = argc >= 2 ? QString::fromUtf8(argv[1]) : QString(kDefaultOutBase);

    SmfFile smf;
    try {
        smf = readSmf(kScorePath);
    } catch (const std::exception& e) {
        std::cerr << "Failed to read " << kScorePath << ": " << e.what() << "\n";
        return 1;
    }
    if (smf.ppq <= 0 || smf.tempiBpm.empty()) {
        std::cerr << "Score has no PPQ or tempo\n";
        return 1;
    }
    const int ppq = smf.ppq;
    // The tempo meta stores whole microseconds per quarter (521739 us =
    // 115.00003 BPM); round to the intended 115 for the session grid.
    const double bpm = std::round(smf.tempiBpm.front() * 1000.0) / 1000.0;
    const int64_t barTicks = int64_t(kBeatsPerBar) * ppq;
    const int64_t endTick = kNumBars * barTicks;
    for (size_t k = 1; k < smf.tempiBpm.size(); ++k) {
        if (smf.tempoTicks[k] < endTick && std::fabs(smf.tempiBpm[k] - bpm) > 1e-6) {
            std::cerr << "Tempo change inside bars 1-" << kNumBars << " is not supported\n";
            return 1;
        }
    }

    const SmfTrack* solo = nullptr;
    for (auto& t : smf.tracks) {
        if (t.name == "solo") solo = &t;
    }
    if (!solo) {
        std::cerr << "No track named \"solo\" in the score\n";
        return 1;
    }

    const double samplesPerTick = kSampleRate * 60.0 / (bpm * ppq);
    auto tickToSample = [&](int64_t tick) { return static_cast<int64_t>(std::llround(tick * samplesPerTick)); };
    const int64_t endSample = tickToSample(endTick);

    std::vector<ScoredNote> notes;
    for (const auto& n : solo->notes) {
        if (n.startTick >= endTick) continue;
        int64_t clippedEnd = std::min(n.endTick, endTick);
        notes.push_back({n.pitch, n.startTick, clippedEnd, tickToSample(n.startTick), tickToSample(clippedEnd)});
    }
    if (notes.empty()) {
        std::cerr << "No solo notes in bars 1-" << kNumBars << "\n";
        return 1;
    }

    Session session;
    session.sampleRate = kSampleRate;
    session.bpm = bpm;

    auto violin = session.addTrack("Violin");
    violin->kind = TrackKind::Instrument;
    violin->synthParams.instrumentProgram.store(kViolinProgram);
    violin->volume.store(1.5f); // makes up for the expression layer's attenuation
    violin->pan.store(0.0f);

    constexpr int64_t kLegatoOverlap = kSampleRate * 20 / 1000;   // 20ms
    constexpr int64_t kMaxLegatoGap = kSampleRate * 40 / 1000;    // bigger gap = a rest: no legato
    constexpr int64_t kEdge = kSampleRate * 15 / 1000;            // level-hold before a note's end
    constexpr int64_t kLongNote = kSampleRate * 400 / 1000;       // gets a bow swell
    constexpr int64_t kVibratoNote = kSampleRate * 300 / 1000;    // gets vibrato
    constexpr int64_t kVibratoDelay = kSampleRate * 180 / 1000;
    constexpr int64_t kVibratoRamp = kSampleRate * 250 / 1000;

    SeededRandom rng(0x5eed2026u);
    Track::MidiNoteList midi;
    std::map<int64_t, float> expression, vibrato;

    for (size_t i = 0; i < notes.size(); ++i) {
        const ScoredNote& n = notes[i];
        const int64_t dur = n.end - n.start;

        // Legato: overlap a different-pitch successor; never a same-pitch one.
        int64_t soundingEnd = n.end;
        if (i + 1 < notes.size()) {
            const ScoredNote& next = notes[i + 1];
            if (next.pitch != n.pitch && next.start - n.end <= kMaxLegatoGap && next.start >= n.start) {
                soundingEnd = std::max(n.end, next.start + kLegatoOverlap);
            } else if (next.pitch == n.pitch) {
                soundingEnd = std::min(soundingEnd, next.start);
            }
        }
        soundingEnd = std::min(soundingEnd, endSample);

        float velocity = phraseVelocity(n.startTick, ppq) + static_cast<float>(0.03 * rng.symmetric());
        if (n.startTick % barTicks == 0) velocity += 0.06f; // beat-1 accent
        velocity = std::clamp(velocity, 0.05f, 1.0f);

        auto note = std::make_shared<MidiNote>();
        note->pitch = n.pitch;
        note->velocity = velocity;
        note->startSample = n.start;
        note->lengthSamples = soundingEnd - n.start;
        midi.push_back(note);

        // Expression: phrase level, with a swell on long notes. Level is
        // held until just before the written end so neighbouring notes
        // don't ramp into each other.
        const double level = phraseLevel(n.startTick, ppq);
        const int64_t hold = std::max(n.start + 1, n.end - kEdge);
        if (dur >= kLongNote) {
            addPoint(expression, n.start, level * 0.84);
            addPoint(expression, n.start + dur * 45 / 100, level * 1.0);
            addPoint(expression, n.start + dur * 80 / 100, level * 0.94);
            addPoint(expression, hold, level * 0.86);
        } else {
            addPoint(expression, n.start, level);
            addPoint(expression, hold, level);
        }

        // Delayed vibrato on held notes; none on short ones.
        addPoint(vibrato, n.start, 0.0);
        if (dur > kVibratoNote) {
            const double depth = vibratoDepthFor(n.startTick, ppq);
            int64_t rampStart = n.start + kVibratoDelay;
            int64_t rampEnd = std::min(rampStart + kVibratoRamp, hold - 1);
            addPoint(vibrato, rampStart, 0.0);
            double reached = rampEnd > rampStart
                                 ? depth * std::min(1.0, double(rampEnd - rampStart) / kVibratoRamp)
                                 : 0.0;
            addPoint(vibrato, rampEnd, reached);
            addPoint(vibrato, hold, reached);
        } else {
            addPoint(vibrato, hold, 0.0);
        }
    }

    violin->setMidiClips(std::move(midi));
    violin->replaceAutomationLane(
        std::make_shared<AutomationLane>(AutomationLane{AutomationTarget::Expression, toPoints(expression)}));
    violin->replaceAutomationLane(
        std::make_shared<AutomationLane>(AutomationLane{AutomationTarget::Vibrato, toPoints(vibrato)}));

    session.markers[1] = 0;                         // Ritornello (forte)
    session.markers[2] = tickToSample(3 * barTicks); // Echo (piano)
    session.markers[3] = tickToSample(6 * barTicks); // Ritornello continues
    session.markers[4] = tickToSample(13 * barTicks); // Birdsong

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
                                         static_cast<unsigned int>(session.channels), lengthSamples);
    const QString wavPath = outBase + ".wav";
    if (!mixdown || !AudioFileIO::writeFile(wavPath, *mixdown, ExportFormat::Wav32Float)) {
        std::cerr << "Failed to render/write: " << qPrintable(wavPath) << "\n";
        return 1;
    }

    std::cout << "Vivaldi Spring, mvt. 1, bars 1-" << kNumBars << " (solo violin, expressive): "
              << notes.size() << " notes @ " << bpm << " BPM, PPQ " << ppq << "\n"
              << "Wrote " << qPrintable(rsdPath) << "\n"
              << "Wrote " << qPrintable(wavPath) << " (" << (lengthSamples / double(kSampleRate))
              << " seconds)\n";
    return 0;
}
