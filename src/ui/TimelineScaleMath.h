#pragma once

#include <algorithm>
#include <cstdint>

namespace rsd {

struct TimelineScaleResult {
    int64_t totalSamples;      // visible timeline length to push to lanes/ruler
    int64_t pinnedBaseSamples; // content-extent baseline to persist for next call
};

// Computes the timeline's visible sample range from content extent + zoom,
// without letting incidental content-extent changes (e.g. dragging a clip
// further along the timeline) silently stretch/shrink a zoom level the user
// dialed in.
//
// `pinnedBaseSamples` is the caller's previously stored baseline (0 the
// first time). Passing `recaptureBaseline = true` (an explicit zoom
// in/out/reset action) recomputes the baseline from the current content
// extent, so zooming still feels responsive to what's on screen right now.
// Passing false (any other refresh, e.g. after a clip edit) only grows the
// baseline if content now exceeds it — it never shrinks or otherwise
// wanders on its own, which is what previously made an unrelated clip drag
// look like it reset the user's zoom.
inline TimelineScaleResult computeTimelineScale(int64_t contentMaxEndSamples, int64_t floorSamples,
                                                  int64_t headroomSamples, float zoomFactor,
                                                  int64_t pinnedBaseSamples, bool recaptureBaseline) {
    int64_t contentBase = std::max(floorSamples, contentMaxEndSamples + headroomSamples);
    int64_t effectiveBase =
        recaptureBaseline ? contentBase : std::max(pinnedBaseSamples, contentBase);
    int64_t total = std::max<int64_t>(1, static_cast<int64_t>(effectiveBase / zoomFactor));
    return {total, effectiveBase};
}

// Resolves the scale to lock in for the duration of a clip drag gesture.
// Must match whatever scale is currently being used to PAINT the lane
// (the shared, cross-track/zoom-aware scale when one has been pushed down
// from the parent, falling back to the lane's own local estimate only
// before any shared scale exists) — otherwise the lock captures a
// different scale than what's on screen, and the clip visibly jumps/resizes
// the instant the drag starts, before the mouse has even moved.
inline int64_t resolveDragLockSamples(int64_t sharedTimelineLength, int64_t localTimelineLength) {
    return sharedTimelineLength > 0 ? sharedTimelineLength : localTimelineLength;
}

} // namespace rsd
