#pragma once

#include <cstdint>
#include <vector>

#include "PunchRegion.h"

namespace rsd {

// RT-safe capture buffer for punch-in/loop recording. Sized once to the
// punch region's length so the audio callback never allocates. Each loop
// pass writes into the same buffer at (position - regionStart), so a new
// pass naturally overwrites the previous one sample-for-sample ("replace"
// takes, per design) without any extra bookkeeping.
class PunchRecorder {
public:
    // GUI thread only: (re)sizes the buffer for a new region/channel count
    // and clears prior capture state. Must be called before recording starts.
    void prepare(const PunchRegion& region, unsigned int channels) {
        m_region = region;
        m_channels = channels;
        m_hasCaptured = false;
        m_passCount = 0;
        m_buffer.assign(static_cast<size_t>(region.lengthSamples()) * channels, 0.0f);
    }

    // RT thread: writes nFrames of interleaved input starting at timeline
    // position `pos`. Frames outside the punch region are ignored. Detects
    // wraparound into a new pass (pos < previous write position) so
    // passCount() can report how many loops have completed.
    void process(int64_t pos, const float* in, unsigned int nFrames) {
        if (!m_region.isValid() || m_buffer.empty() || !in) return;

        for (unsigned int i = 0; i < nFrames; ++i) {
            int64_t framePos = pos + i;
            if (framePos < m_region.startSample || framePos >= m_region.endSample) continue;

            int64_t offsetFrames = framePos - m_region.startSample;
            if (offsetFrames < m_lastOffsetFrames) {
                // Position wrapped back to the start of the region: a new
                // loop pass has begun.
                ++m_passCount;
            }
            m_lastOffsetFrames = offsetFrames;
            m_hasCaptured = true;

            size_t dst = static_cast<size_t>(offsetFrames) * m_channels;
            for (unsigned int ch = 0; ch < m_channels; ++ch) {
                m_buffer[dst + ch] = in[static_cast<size_t>(i) * m_channels + ch];
            }
        }
    }

    bool hasCaptured() const { return m_hasCaptured; }
    int passCount() const { return m_passCount; }
    const PunchRegion& region() const { return m_region; }
    unsigned int channels() const { return m_channels; }
    const std::vector<float>& buffer() const { return m_buffer; }

private:
    PunchRegion m_region;
    unsigned int m_channels = 2;
    std::vector<float> m_buffer;
    int64_t m_lastOffsetFrames = -1;
    bool m_hasCaptured = false;
    int m_passCount = 0;
};

} // namespace rsd
