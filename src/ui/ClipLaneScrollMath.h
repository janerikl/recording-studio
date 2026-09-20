#pragma once

#include <algorithm>
#include <cstdint>

namespace rsd {

// Largest offset (in samples) a track lane can be scrolled to while still
// keeping the visible window inside the full content extent.
inline int64_t maxScrollOffsetSamples(int64_t contentExtentSamples, int64_t visibleLengthSamples) {
    return std::max<int64_t>(0, contentExtentSamples - visibleLengthSamples);
}

// Clamps a requested scroll offset to the scrollable range, so neither user
// dragging nor a shrinking content extent can push the visible window
// outside [0, contentExtent].
inline int64_t clampScrollOffset(int64_t requestedSamples, int64_t contentExtentSamples,
                                  int64_t visibleLengthSamples) {
    int64_t maxOffset = maxScrollOffsetSamples(contentExtentSamples, visibleLengthSamples);
    return std::clamp<int64_t>(requestedSamples, 0, maxOffset);
}

// The scrollbar only makes sense once the content no longer fits in the
// visible window — otherwise there's nowhere to scroll to.
inline bool scrollbarShouldBeEnabled(int64_t contentExtentSamples, int64_t visibleLengthSamples) {
    return contentExtentSamples > visibleLengthSamples;
}

} // namespace rsd
