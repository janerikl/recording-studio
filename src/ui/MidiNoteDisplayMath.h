#pragma once

#include <algorithm>

namespace rsd {

// Maps a MIDI pitch to a Y pixel within [0, laneHeight) for the timeline's
// mini piano-roll view of an Instrument track: higher pitch draws higher on
// screen (smaller Y). Pitches outside [lowPitch, highPitch] clamp to the
// top/bottom edge instead of going off-screen.
inline int pitchToY(int pitch, int laneHeight, int lowPitch = 36, int highPitch = 96) {
    if (highPitch <= lowPitch) return 0;
    int clamped = std::clamp(pitch, lowPitch, highPitch);
    float t = static_cast<float>(highPitch - clamped) / static_cast<float>(highPitch - lowPitch);
    return static_cast<int>(t * static_cast<float>(laneHeight));
}

// Pixel height for one note rectangle in the timeline's mini piano-roll
// view: one pitch-range "row" worth of the lane, so adjacent semitones tile
// without overlapping or leaving large gaps (replaces a fixed-size tick).
// Clamped to at least 1px so notes stay visible in a very short lane.
inline int noteRowHeight(int laneHeight, int lowPitch = 36, int highPitch = 96) {
    if (highPitch <= lowPitch) return 0;
    int range = highPitch - lowPitch;
    return std::max(1, laneHeight / range);
}

} // namespace rsd
