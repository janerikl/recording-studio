#pragma once

#include <memory>
#include <vector>

#include "Clip.h"

namespace rsd {

using ClipPtrList = std::vector<std::shared_ptr<Clip>>;

// Promotes `takeClip` to be the active comp for [regionStart, regionEnd):
// any existing clip fully inside the region is dropped, a clip straddling
// either edge is trimmed back to end/start exactly at the region boundary
// (splitting into two pieces if it spans the whole region), and a copy of
// takeClip is inserted covering the region exactly. Non-overlapping clips
// pass through unchanged.
inline ClipPtrList promoteTakeToComp(const ClipPtrList& existingClips,
                                      const std::shared_ptr<Clip>& takeClip, int64_t regionStart,
                                      int64_t regionEnd) {
    ClipPtrList result;
    result.reserve(existingClips.size() + 1);

    for (auto& c : existingClips) {
        int64_t cStart = c->sessionStartSample;
        int64_t cEnd = c->sessionStartSample + c->lengthSamples;

        if (cEnd <= regionStart || cStart >= regionEnd) {
            result.push_back(c); // no overlap at all
            continue;
        }

        if (cStart < regionStart) {
            // Left remainder: unchanged offset/start, trimmed to end at the
            // region boundary.
            auto left = std::make_shared<Clip>(*c);
            left->lengthSamples = regionStart - cStart;
            result.push_back(left);
        }
        if (cEnd > regionEnd) {
            // Right remainder: trimmed to start at the region boundary, its
            // source-offset advanced by however much was cut from the front.
            auto right = std::make_shared<Clip>(*c);
            right->id = QUuid::createUuid();
            int64_t cutFromFront = regionEnd - cStart;
            right->sourceOffsetSamples = c->sourceOffsetSamples + cutFromFront;
            right->sessionStartSample = regionEnd;
            right->lengthSamples = cEnd - regionEnd;
            result.push_back(right);
        }
        // Fully inside the region: dropped (neither piece pushed).
    }

    auto promoted = std::make_shared<Clip>(*takeClip);
    promoted->id = QUuid::createUuid();
    promoted->sessionStartSample = regionStart;
    promoted->sourceOffsetSamples = 0;
    promoted->lengthSamples = regionEnd - regionStart;
    result.push_back(promoted);

    return result;
}

} // namespace rsd
