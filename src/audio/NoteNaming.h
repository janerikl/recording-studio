#pragma once

#include <array>

#include <QString>

namespace rsd {

// Scientific pitch notation (MIDI 60 = C4, "middle C").
inline QString midiNoteName(int pitch) {
    static const std::array<const char*, 12> names = {"C",  "C#", "D",  "D#", "E",  "F",
                                                        "F#", "G",  "G#", "A",  "A#", "B"};
    int pc = ((pitch % 12) + 12) % 12;
    int octave = pitch / 12 - 1;
    if (pitch < 0 && pitch % 12 != 0) --octave; // floor division for negative pitches
    return QString("%1%2").arg(names[static_cast<size_t>(pc)]).arg(octave);
}

} // namespace rsd
