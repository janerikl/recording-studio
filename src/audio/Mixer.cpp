#include "Mixer.h"

#include <cmath>

namespace rsd {

float fadeMultiplier(int64_t pos, int64_t len, FadeCurve curve) {
    if (len <= 0) return 1.0f;
    float t = static_cast<float>(pos) / static_cast<float>(len);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    if (curve == FadeCurve::EqualPower) return std::sin(t * static_cast<float>(M_PI_2));
    return t;
}

void mixClipInto(float* out, unsigned int nFrames, unsigned int channels, int64_t playheadStart,
                  const Clip& clip, float gainL, float gainR) {
    if (clip.muted || !clip.buffer) return;

    const int64_t clipEnd = clip.sessionStartSample + clip.lengthSamples;
    const int64_t blockEnd = playheadStart + static_cast<int64_t>(nFrames);
    if (clipEnd <= playheadStart || clip.sessionStartSample >= blockEnd) return;

    for (unsigned int i = 0; i < nFrames; ++i) {
        int64_t timelinePos = playheadStart + static_cast<int64_t>(i);
        if (timelinePos < clip.sessionStartSample || timelinePos >= clipEnd) continue;

        int64_t sourceFrame = clip.sourceOffsetSamples + (timelinePos - clip.sessionStartSample);
        if (sourceFrame < 0 || sourceFrame >= clip.buffer->frameCount()) continue;

        float fade = 1.0f;
        int64_t posInClip = timelinePos - clip.sessionStartSample;
        int64_t remainingInClip = clipEnd - timelinePos;
        if (clip.fadeInSamples > 0 && posInClip < clip.fadeInSamples) {
            fade *= fadeMultiplier(posInClip, clip.fadeInSamples, clip.fadeInCurve);
        }
        if (clip.fadeOutSamples > 0 && remainingInClip <= clip.fadeOutSamples) {
            fade *= fadeMultiplier(remainingInClip - 1, clip.fadeOutSamples, clip.fadeOutCurve);
        }

        for (unsigned int ch = 0; ch < channels; ++ch) {
            unsigned int srcCh = ch % static_cast<unsigned int>(clip.buffer->channels);
            float sample = clip.buffer->samples[sourceFrame * clip.buffer->channels + srcCh];
            float g = (ch % 2 == 0) ? gainL : gainR;
            out[i * channels + ch] += sample * g * clip.gain * fade;
        }
    }
}

} // namespace rsd
