#include "WaveformCache.h"

#include <algorithm>
#include <limits>

namespace rsd {

QVector<WaveformCache::PeakPair> WaveformCache::computePeaks(const AudioBuffer& buffer,
                                                              int numColumns, int channel) {
    QVector<PeakPair> peaks(std::max(0, numColumns), {0.0f, 0.0f});

    const int64_t frameCount = buffer.frameCount();
    if (frameCount <= 0 || numColumns <= 0 || buffer.channels <= 0) return peaks;

    const int fixedChannel = channel >= 0 ? std::min(channel, buffer.channels - 1) : -1;
    const double framesPerColumn = static_cast<double>(frameCount) / numColumns;

    for (int col = 0; col < numColumns; ++col) {
        int64_t startFrame = static_cast<int64_t>(col * framesPerColumn);
        int64_t endFrame = static_cast<int64_t>((col + 1) * framesPerColumn);
        endFrame = std::min(endFrame, frameCount);
        if (endFrame <= startFrame) endFrame = std::min(startFrame + 1, frameCount);

        float minV = std::numeric_limits<float>::max();
        float maxV = std::numeric_limits<float>::lowest();

        for (int64_t f = startFrame; f < endFrame; ++f) {
            float value;
            if (fixedChannel >= 0) {
                value = buffer.samples[f * buffer.channels + fixedChannel];
            } else {
                value = 0.0f;
                for (int ch = 0; ch < buffer.channels; ++ch) {
                    value += buffer.samples[f * buffer.channels + ch];
                }
                value /= buffer.channels;
            }
            minV = std::min(minV, value);
            maxV = std::max(maxV, value);
        }

        if (minV > maxV) { minV = maxV = 0.0f; }
        peaks[col] = {minV, maxV};
    }

    return peaks;
}

} // namespace rsd
