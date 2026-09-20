#pragma once

#include <algorithm>

namespace rsd {

// Pure math for bus routing / aux sends. A track's post-fader signal is
// scaled by its send level before being accumulated into its destination
// bus's aux buffer; the master bus applies a final volume scale after all
// tracks and buses have been summed into it. Kept separate from
// PanLawMath since sends are mono-scalar (no pan law involved) and this
// is evaluated once per sample per send, not once per block.

// sendLevel in [0, 1] (0 = no send, 1 = full post-fader signal duplicated
// to the bus). Clamped so a corrupt/out-of-range stored value can't blow
// up the aux buffer.
inline float clampSendLevel(float sendLevel) {
    return std::clamp(sendLevel, 0.0f, 1.0f);
}

inline float applySend(float sample, float sendLevel) {
    return sample * clampSendLevel(sendLevel);
}

// Master volume in [0, 2], matching the existing track volume range.
inline float clampMasterVolume(float volume) {
    return std::clamp(volume, 0.0f, 2.0f);
}

inline float applyMasterVolume(float sample, float volume) {
    return sample * clampMasterVolume(volume);
}

} // namespace rsd
