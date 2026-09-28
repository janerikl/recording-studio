#pragma once

#include <QVector>
#include <utility>

#include "model/AudioBuffer.h"

namespace rsd {

// Precomputes per-column (min, max) peak pairs from an AudioBuffer so
// painting never has to rescan raw samples.
class WaveformCache {
public:
    using PeakPair = std::pair<float, float>; // (min, max)

    // channel == -1 averages all channels into one mono trace; otherwise
    // picks that single channel index (clamped to the buffer's channel count).
    static QVector<PeakPair> computePeaks(const AudioBuffer& buffer, int numColumns,
                                           int channel = -1);

    // Block-level level-of-detail state for a buffer that's still growing
    // (e.g. a live recording): one (min,max) pair per `blockFrames`-frame
    // block, built up incrementally by extendBlockPeaks() below instead of
    // rescanned from scratch on every call.
    struct BlockPeaks {
        QVector<PeakPair> blocks;
        int64_t framesDone = 0;
    };

    // Extends `state` to cover `buffer`'s current frame count. Frames
    // already covered by a previously-closed block are never rescanned;
    // only the still-open tail block (redone in case it grew) and any newly
    // completed blocks cost work. Calling this every time a growing buffer
    // gains a handful of samples costs O(new frames) per call, unlike
    // computePeaks() which costs O(total frames) every call — the fix for
    // live-recording waveform repaint getting slower (and eventually
    // stalling the UI thread) as the recording grows.
    static void extendBlockPeaks(const AudioBuffer& buffer, int channel, int64_t blockFrames,
                                  BlockPeaks& state);

    // Downsamples block-level peaks (from extendBlockPeaks) to numColumns
    // display columns, combining each column's blocks via
    // min-of-mins/max-of-maxes. Cost is O(block count), not O(raw samples).
    static QVector<PeakPair> downsampleBlockPeaks(const QVector<PeakPair>& blocks, int numColumns);
};

} // namespace rsd
