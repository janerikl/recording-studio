#pragma once

#include <cstdint>

namespace rsd {

// Pure math for the loop browser's click-to-audition playback. The RT
// callback advances a preview's position by nFrames each block (see
// AudioEngine::rtOutputCallback) and calls this to decide whether to clear the
// preview so it doesn't keep "finishing" silently forever once past the
// buffer's end.
inline bool isPreviewFinished(int64_t position, int64_t lengthSamples) {
    if (lengthSamples <= 0) return true;
    return position >= lengthSamples;
}

} // namespace rsd
