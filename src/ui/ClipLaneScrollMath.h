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

// Scroll offset needed so `playheadSample` sits inside the visible window,
// whichever direction it fell outside of — forward (playback/recording ran
// past the right edge) or backward (a seek/rewind landed before the left
// edge, e.g. jumping back to the start after letting playback run far past
// the session's content). Returns `scrollOffsetSamples` unchanged if the
// playhead is already visible. Not clamped to a content extent — the
// caller still owns growing that (see ClipLaneWidget::setPlayheadSample).
inline int64_t scrollOffsetToRevealPlayhead(int64_t playheadSample, int64_t scrollOffsetSamples,
                                             int64_t visibleLengthSamples,
                                             double marginFraction = 0.05) {
    int64_t margin = static_cast<int64_t>(static_cast<double>(visibleLengthSamples) * marginFraction);
    int64_t rightEdge = scrollOffsetSamples + visibleLengthSamples;
    if (playheadSample > rightEdge) {
        return playheadSample - visibleLengthSamples + margin;
    }
    if (playheadSample < scrollOffsetSamples) {
        return std::max<int64_t>(0, playheadSample - margin);
    }
    return scrollOffsetSamples;
}

} // namespace rsd
