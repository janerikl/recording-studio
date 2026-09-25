#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "MetronomeMath.h"

namespace rsd {

// Simple click-track generator: a short decaying sine blip at each beat
// boundary. RT-safe (no allocation after construction), mirrors
// SynthVoice's envelope-state-carried-across-render-calls pattern so a
// click begun near the end of one block continues correctly into the
// next.
class Metronome {
public:
    void render(float* out, unsigned int nFrames, unsigned int channels, unsigned int sampleRate,
                int64_t pos, double bpm, bool enabled) {
        if (!enabled) {
            m_active = false;
            return;
        }
        double beatSamples = beatDurationSamples(bpm, static_cast<int>(sampleRate));
        auto tickOffset = firstClickOffsetInBlock(pos, nFrames, beatSamples);

        for (unsigned int i = 0; i < nFrames; ++i) {
            if (tickOffset && static_cast<int>(i) == *tickOffset) {
                m_active = true;
                m_clickSample = 0;
            }
            if (m_active) {
                float t = static_cast<float>(m_clickSample) / static_cast<float>(sampleRate);
                if (t >= kClickDurationSeconds) {
                    m_active = false;
                } else {
                    float env = 1.0f - t / kClickDurationSeconds;
                    float sample =
                        std::sin(2.0f * static_cast<float>(M_PI) * kClickFrequencyHz * t) * env * kClickGain;
                    for (unsigned int ch = 0; ch < channels; ++ch) out[i * channels + ch] += sample;
                    ++m_clickSample;
                }
            }
        }
    }

private:
    static constexpr float kClickFrequencyHz = 1000.0f;
    static constexpr float kClickDurationSeconds = 0.03f;
    static constexpr float kClickGain = 0.5f;

    bool m_active = false;
    int m_clickSample = 0;
};

} // namespace rsd
