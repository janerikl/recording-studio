#pragma once

#include <cstdint>
#include <vector>

namespace rsd {

// Interleaved float samples shared (via shared_ptr) between clips.
class AudioBuffer {
public:
    std::vector<float> samples;
    int channels = 2;
    int sampleRate = 48000;

    int64_t frameCount() const {
        return channels > 0 ? static_cast<int64_t>(samples.size()) / channels : 0;
    }
};

} // namespace rsd
