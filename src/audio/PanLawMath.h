#pragma once

#include <algorithm>
#include <utility>

namespace rsd {

// The mixer's linear pan law, extracted from what was previously the pan
// dial's inline gainL/gainR derivation, so both manual mixing and
// automation playback (which evaluates a pan curve every block) share the
// exact same math. pan in [-1, 1] (-1 = full left, 1 = full right), volume
// clamped to [0, 2] (matches the existing gain slider range).
inline std::pair<float, float> panToGains(float volume, float pan) {
    float v = std::clamp(volume, 0.0f, 2.0f);
    float p = std::clamp(pan, -1.0f, 1.0f);
    float gl = p <= 0.0f ? 1.0f : 1.0f - p;
    float gr = p >= 0.0f ? 1.0f : 1.0f + p;
    return {gl * v, gr * v};
}

} // namespace rsd
