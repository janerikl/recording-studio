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

} // namespace rsd
