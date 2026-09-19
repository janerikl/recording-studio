#pragma once

#include <QVector>
#include <utility>

#include "model/AudioBuffer.h"

namespace rsd {

// Precomputes per-column (min, max) peak pairs from an AudioBuffer so
// painting never has to rescan raw samples. Channels are averaged into a
// single mono peak trace for display purposes.
class WaveformCache {
public:
    using PeakPair = std::pair<float, float>; // (min, max)

    static QVector<PeakPair> computePeaks(const AudioBuffer& buffer, int numColumns);
};

} // namespace rsd
