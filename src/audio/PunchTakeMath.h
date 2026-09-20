#pragma once

#include <algorithm>

namespace rsd {

// Picks which pre-allocated pass buffer a given loop pass writes into. The
// audio callback can't allocate memory, so PunchRecorder pre-allocates
// `maxTakes` buffers up front; once passCount reaches that cap, every
// further pass keeps overwriting the last slot instead of growing.
inline int takeBufferIndexForPass(int passCount, int maxTakes) {
    return std::clamp(passCount, 0, maxTakes - 1);
}

} // namespace rsd
