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

void WaveformCache::extendBlockPeaks(const AudioBuffer& buffer, int channel, int64_t blockFrames,
                                      BlockPeaks& state) {
    if (blockFrames <= 0 || buffer.channels <= 0) return;
    const int64_t frameCount = buffer.frameCount();
    const int fixedChannel = channel >= 0 ? std::min(channel, buffer.channels - 1) : -1;

    // Drop the previous, possibly-still-open final block so it gets redone
    // below with whatever samples have arrived since; every block before it
    // is already closed and stays untouched.
    int64_t closedBlocks = state.framesDone / blockFrames;
    if (closedBlocks < state.blocks.size()) {
        state.blocks.resize(static_cast<int>(closedBlocks));
    }

    int64_t startFrame = closedBlocks * blockFrames;
    while (startFrame < frameCount) {
        int64_t endFrame = std::min(startFrame + blockFrames, frameCount);

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
        state.blocks.push_back({minV, maxV});
        startFrame = endFrame;
    }
    state.framesDone = frameCount;
}

QVector<WaveformCache::PeakPair> WaveformCache::downsampleBlockPeaks(const QVector<PeakPair>& blocks,
                                                                       int numColumns) {
    QVector<PeakPair> peaks(std::max(0, numColumns), {0.0f, 0.0f});

    const int64_t blockCount = blocks.size();
    if (blockCount <= 0 || numColumns <= 0) return peaks;

    const double blocksPerColumn = static_cast<double>(blockCount) / numColumns;
    for (int col = 0; col < numColumns; ++col) {
        int64_t startBlock = static_cast<int64_t>(col * blocksPerColumn);
        int64_t endBlock = static_cast<int64_t>((col + 1) * blocksPerColumn);
        endBlock = std::min(endBlock, blockCount);
        if (endBlock <= startBlock) endBlock = std::min(startBlock + 1, blockCount);

        float minV = std::numeric_limits<float>::max();
        float maxV = std::numeric_limits<float>::lowest();
        for (int64_t b = startBlock; b < endBlock; ++b) {
            minV = std::min(minV, blocks[b].first);
            maxV = std::max(maxV, blocks[b].second);
        }
        if (minV > maxV) { minV = maxV = 0.0f; }
        peaks[col] = {minV, maxV};
    }

    return peaks;
}

} // namespace rsd
