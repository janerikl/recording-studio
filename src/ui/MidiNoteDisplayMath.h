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

} // namespace rsd
