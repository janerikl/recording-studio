#pragma once

#include <algorithm>
#include <cstdint>

namespace rsd {

// Clamps a requested fade length (fade-in or fade-out) to a valid range: at
// least 0, and never so long that it would eat into (or past) the space
// already claimed by the *other* fade on the same clip.
inline int64_t clampFadeSamples(int64_t requested, int64_t clipLengthSamples,
                                 int64_t otherFadeSamples) {
    int64_t maxAllowed = std::max<int64_t>(0, clipLengthSamples - otherFadeSamples);
    return std::clamp<int64_t>(requested, 0, maxAllowed);
}

// Clip gain is drawn as a horizontal line across the clip (0.0 at the
// bottom edge, 2.0 at the top, 1.0 = unity at the vertical center) and
// dragged up/down to adjust it in place, mirroring how trim/fade handles
// work. A drag spanning the full lane height swings gain across the whole
// [0, 2] range.
inline float clampClipGain(float gain) { return std::clamp(gain, 0.0f, 2.0f); }

inline float gainAfterVerticalDrag(float origGain, int deltaYPx, int laneHeightPx) {
    if (laneHeightPx <= 0) return clampClipGain(origGain);
    // Dragging up (negative deltaY) increases gain.
    float delta = -static_cast<float>(deltaYPx) / static_cast<float>(laneHeightPx) * 2.0f;
    return clampClipGain(origGain + delta);
}

// Y coordinate (in lane-local pixels, 0 = top) of the gain line for a given
// gain value, within a clip drawn at [clipTopY, clipTopY + clipHeightPx).
inline int gainLineY(float gain, int clipTopY, int clipHeightPx) {
    float t = 1.0f - (clampClipGain(gain) / 2.0f); // 0.0 -> bottom (t=1), 2.0 -> top (t=0)
    return clipTopY + static_cast<int>(t * static_cast<float>(clipHeightPx));
}

} // namespace rsd
