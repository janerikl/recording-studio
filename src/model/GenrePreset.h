#pragma once

#include <QString>
#include <QVector>
#include <algorithm>
#include <cstdint>

namespace rsd {

// A genre-driven starting point for procedurally generating a song: tempo,
// key, a scale-degree chord progression, and instrumentation. Used by
// tools/generate_song.cpp. presetForGenre() matches a small table of known
// genres case-insensitively; anything else falls back to a deterministic,
// hash-derived preset so the same unrecognized genre string always yields
// the same song shape.
struct GenrePreset {
    QString genreName;
    double bpm = 120.0;
    int rootMidiNote = 60;              // key, MIDI note number (0-127)
    QVector<int> chordDegrees;          // scale-degree progression, e.g. {0,3,4,0} = I IV V I
    int leadInstrumentProgram = 0;      // GM program 0-127
    int harmonyInstrumentProgram = 0;   // GM program 0-127
    bool useDrumKit = true;
};

namespace detail {

// FNV-1a, good enough for a deterministic, well-distributed hash of an
// arbitrary (possibly empty, possibly unicode) genre name.
inline uint64_t fnv1a(const QString& s) {
    uint64_t h = 1469598103934665603ull;
    const QByteArray utf8 = s.toUtf8();
    for (unsigned char c : utf8) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

} // namespace detail

inline GenrePreset presetForGenre(const QString& genreNameIn) {
    const QString genre = genreNameIn.trimmed().toLower();
    GenrePreset p;
    p.genreName = genreNameIn;

    if (genre == "country") {
        p.bpm = 120.0;
        p.rootMidiNote = 67; // G3
        p.chordDegrees = {0, 3, 4, 0, 0, 3, 4, 0}; // I IV V I
        p.leadInstrumentProgram = 25;   // Acoustic Guitar (steel)
        p.harmonyInstrumentProgram = 105; // Banjo
        p.useDrumKit = true;
    } else if (genre == "jazz") {
        p.bpm = 96.0;
        p.rootMidiNote = 60; // C3, minor
        p.chordDegrees = {0, 3, 6, 2, 5, 1, 4, 4}; // ii-V-I-ish motion through the scale
        p.leadInstrumentProgram = 66;    // Tenor Sax
        p.harmonyInstrumentProgram = 4;  // Electric Piano 1 (Rhodes)
        p.useDrumKit = true;
    } else if (genre == "lullaby") {
        p.bpm = 66.0;
        p.rootMidiNote = 60; // C4
        p.chordDegrees = {0, 5, 3, 4}; // I vi IV V
        p.leadInstrumentProgram = 10;    // Music Box
        p.harmonyInstrumentProgram = 46; // Orchestral Harp
        p.useDrumKit = false;
    } else if (genre == "rock") {
        p.bpm = 132.0;
        p.rootMidiNote = 64; // E3
        p.chordDegrees = {0, 4, 5, 4}; // I V vi V
        p.leadInstrumentProgram = 29;    // Overdriven Guitar
        p.harmonyInstrumentProgram = 33; // Electric Bass (finger)
        p.useDrumKit = true;
    } else if (genre == "pop") {
        p.bpm = 116.0;
        p.rootMidiNote = 60; // C4
        p.chordDegrees = {0, 4, 5, 3}; // I V vi IV
        p.leadInstrumentProgram = 80;    // Lead 1 (square)
        p.harmonyInstrumentProgram = 88; // Pad 1 (new age)
        p.useDrumKit = true;
    } else if (genre == "blues") {
        p.bpm = 88.0;
        p.rootMidiNote = 57; // A2
        p.chordDegrees = {0, 0, 0, 0, 3, 3, 0, 0, 4, 3, 0, 4}; // 12-bar
        p.leadInstrumentProgram = 27;    // Electric Guitar (clean)
        p.harmonyInstrumentProgram = 22; // Harmonica
        p.useDrumKit = true;
    } else if (genre == "techno") {
        p.bpm = 138.0;
        p.rootMidiNote = 57; // A2
        p.chordDegrees = {0, 0, 5, 5};
        p.leadInstrumentProgram = 81;    // Lead 2 (sawtooth)
        p.harmonyInstrumentProgram = 38; // Synth Bass 1
        p.useDrumKit = true;
    } else if (genre == "classical") {
        p.bpm = 100.0;
        p.rootMidiNote = 62; // D4
        p.chordDegrees = {0, 3, 4, 0, 5, 1, 4, 0};
        p.leadInstrumentProgram = 40;    // Violin
        p.harmonyInstrumentProgram = 48; // String Ensemble 1
        p.useDrumKit = false;
    } else if (genre == "reggae") {
        p.bpm = 84.0;
        p.rootMidiNote = 62; // D3
        p.chordDegrees = {0, 3, 0, 4};
        p.leadInstrumentProgram = 27;    // Electric Guitar (clean), skank
        p.harmonyInstrumentProgram = 33; // Electric Bass (finger)
        p.useDrumKit = true;
    } else if (genre == "metal") {
        p.bpm = 160.0;
        p.rootMidiNote = 52; // E2
        p.chordDegrees = {0, 2, 3, 0};
        p.leadInstrumentProgram = 30;    // Distortion Guitar
        p.harmonyInstrumentProgram = 33; // Electric Bass (finger)
        p.useDrumKit = true;
    } else {
        // Deterministic fallback for any unrecognized genre string: derive
        // every field from a hash of the (trimmed, lowercased) name so the
        // same input always maps to the same preset, and no field ever
        // falls outside its valid range regardless of input content.
        uint64_t h = detail::fnv1a(genre);

        auto take = [&h](uint64_t modulus) -> uint64_t {
            uint64_t v = h % modulus;
            h /= modulus;
            h ^= 0x9E3779B97F4A7C15ull; // remix so successive takes decorrelate
            h *= 2862933555777941757ull;
            return v;
        };

        p.bpm = 70.0 + static_cast<double>(take(121)); // 70-190 bpm
        p.rootMidiNote = 36 + static_cast<int>(take(48)); // roughly C2-B5
        p.leadInstrumentProgram = static_cast<int>(take(128));
        p.harmonyInstrumentProgram = static_cast<int>(take(128));
        p.useDrumKit = (take(2) != 0);

        static const QVector<QVector<int>> kProgressionShapes = {
            {0, 3, 4, 0}, {0, 4, 5, 3}, {0, 5, 3, 4}, {0, 2, 3, 0}, {0, 3, 6, 4},
        };
        p.chordDegrees = kProgressionShapes[static_cast<int>(take(kProgressionShapes.size()))];
    }

    p.rootMidiNote = std::clamp(p.rootMidiNote, 0, 127);
    p.leadInstrumentProgram = std::clamp(p.leadInstrumentProgram, 0, 127);
    p.harmonyInstrumentProgram = std::clamp(p.harmonyInstrumentProgram, 0, 127);
    if (p.chordDegrees.isEmpty()) p.chordDegrees = {0, 3, 4, 0};
    if (p.bpm <= 0.0) p.bpm = 100.0;
    return p;
}

} // namespace rsd
