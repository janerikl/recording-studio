#include "WaveformCache.h"

#include <algorithm>
#include <limits>

namespace rsd {

QVector<WaveformCache::PeakPair> WaveformCache::computePeaks(const AudioBuffer& buffer,
                                                              int numColumns) {
    QVector<PeakPair> peaks(std::max(0, numColumns), {0.0f, 0.0f});

    const int64_t frameCount = buffer.frameCount();
    if (frameCount <= 0 || numColumns <= 0 || buffer.channels <= 0) return peaks;

    const double framesPerColumn = static_cast<double>(frameCount) / numColumns;

    for (int col = 0; col < numColumns; ++col) {
        int64_t startFrame = static_cast<int64_t>(col * framesPerColumn);
        int64_t endFrame = static_cast<int64_t>((col + 1) * framesPerColumn);
        endFrame = std::min(endFrame, frameCount);
        if (endFrame <= startFrame) endFrame = std::min(startFrame + 1, frameCount);

        float minV = std::numeric_limits<float>::max();
        float maxV = std::numeric_limits<float>::lowest();

        for (int64_t f = startFrame; f < endFrame; ++f) {
            float mono = 0.0f;
            for (int ch = 0; ch < buffer.channels; ++ch) {
                mono += buffer.samples[f * buffer.channels + ch];
            }
            mono /= buffer.channels;
            minV = std::min(minV, mono);
            maxV = std::max(maxV, mono);
        }

        if (minV > maxV) { minV = maxV = 0.0f; }
        peaks[col] = {minV, maxV};
    }

    return peaks;
}

} // namespace rsd
