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
};

} // namespace rsd
