#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "PunchRegion.h"
#include "PunchTakeMath.h"

namespace rsd {

// RT-safe capture buffer for punch-in/loop recording. Pre-allocates
// kMaxTakes separate buffers up front (sized once to the punch region's
// length) so the audio callback never allocates, and each loop pass writes
// into its own buffer instead of overwriting the previous one — this is
// what lets comping offer every take, not just the last. A pass beyond the
// cap keeps overwriting the last slot (takesCapExceeded() flags this for
// the UI to warn about).
class PunchRecorder {
public:
    static constexpr int kMaxTakes = 8;

    // GUI thread only: (re)sizes the buffers for a new region/channel count
    // and clears prior capture state. Must be called before recording starts.
    void prepare(const PunchRegion& region, unsigned int channels) {
        m_region = region;
        m_channels = channels;
        m_hasCaptured = false;
        m_passCount = 0;
        m_lastOffsetFrames = -1;
        m_takesCapExceeded = false;
        size_t frameCount = static_cast<size_t>(region.lengthSamples());
        for (auto& buf : m_buffers) buf.assign(frameCount * channels, 0.0f);
    }

    // RT thread: writes nFrames of interleaved input starting at timeline
    // position `pos`. Frames outside the punch region are ignored. Detects
    // wraparound into a new pass (pos < previous write position) so
    // passCount()/takeCount() can report how many loops have completed.
    void process(int64_t pos, const float* in, unsigned int nFrames) {
        if (!m_region.isValid() || m_buffers[0].empty() || !in) return;

        for (unsigned int i = 0; i < nFrames; ++i) {
            int64_t framePos = pos + i;
            if (framePos < m_region.startSample || framePos >= m_region.endSample) continue;

            int64_t offsetFrames = framePos - m_region.startSample;
            if (offsetFrames < m_lastOffsetFrames) {
                // Position wrapped back to the start of the region: a new
                // loop pass has begun.
                ++m_passCount;
                if (m_passCount >= kMaxTakes) m_takesCapExceeded = true;
            }
            m_lastOffsetFrames = offsetFrames;
            m_hasCaptured = true;

            int takeIndex = takeBufferIndexForPass(m_passCount, kMaxTakes);
            size_t dst = static_cast<size_t>(offsetFrames) * m_channels;
            auto& buf = m_buffers[static_cast<size_t>(takeIndex)];
            for (unsigned int ch = 0; ch < m_channels; ++ch) {
                buf[dst + ch] = in[static_cast<size_t>(i) * m_channels + ch];
            }
        }
    }

    bool hasCaptured() const { return m_hasCaptured; }
    int passCount() const { return m_passCount; }
    // Number of distinct takes actually captured (1 once recording starts,
    // growing with each loop pass, capped at kMaxTakes).
    int takeCount() const { return m_hasCaptured ? takeBufferIndexForPass(m_passCount, kMaxTakes) + 1 : 0; }
    bool takesCapExceeded() const { return m_takesCapExceeded; }
    const PunchRegion& region() const { return m_region; }
    unsigned int channels() const { return m_channels; }
    // The most recent pass's buffer — what ends up as the active comp clip.
    const std::vector<float>& buffer() const {
        return m_buffers[static_cast<size_t>(takeBufferIndexForPass(m_passCount, kMaxTakes))];
    }
    // A specific take's buffer (0-indexed, < takeCount()).
    const std::vector<float>& takeBuffer(int takeIndex) const {
        return m_buffers[static_cast<size_t>(takeIndex)];
    }

private:
    PunchRegion m_region;
    unsigned int m_channels = 2;
    std::array<std::vector<float>, kMaxTakes> m_buffers;
    int64_t m_lastOffsetFrames = -1;
    bool m_hasCaptured = false;
    int m_passCount = 0;
    bool m_takesCapExceeded = false;
};

} // namespace rsd
