#pragma once

#include <cstdint>

namespace rsd {

// A punch in/out range on the timeline, in samples. Also doubles as the loop
// range when loop-record is enabled (same region drives both, per design).
struct PunchRegion {
    int64_t startSample = 0;
    int64_t endSample = 0;

    bool isValid() const { return endSample > startSample; }
    int64_t lengthSamples() const { return isValid() ? endSample - startSample : 0; }
};

} // namespace rsd
