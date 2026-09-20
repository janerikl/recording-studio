#pragma once

#include <algorithm>
#include <cstddef>

namespace rsd {

// Picks a "nice" tick spacing in seconds so ticks land roughly
// targetPxPerTick apart given the lane's pixel width and how many seconds
// it spans. Shared by the ruler, each track's clip lane, and the master
// waveform so their tick marks/gridlines can never drift out of step with
// each other (previously the ruler computed this inline and nothing else
// drew ticks at all).
inline double niceTickSeconds(double totalSeconds, int laneWidthPx, double targetPxPerTick = 80.0) {
    static const double niceSteps[] = {0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300};
    if (totalSeconds <= 0.0 || laneWidthPx <= 0) return niceSteps[0];

    double secondsPerPixel = totalSeconds / laneWidthPx;
    double rawTickSeconds = targetPxPerTick * secondsPerPixel;
    for (double step : niceSteps) {
        if (step >= rawTickSeconds) return step;
    }
    return niceSteps[std::size(niceSteps) - 1];
}

} // namespace rsd
