#pragma once

#include <QVector>
#include <algorithm>
#include <cmath>
#include <utility>

namespace rsd {

// Amplitude scale to apply when drawing a clip's waveform peaks so that
// quiet/moderate material still reads as a bold trace instead of a thin
// line, especially in a short lane. Boosts the drawn amplitude so the
// loudest peak in view reaches (approximately) full scale, but never boosts
// by more than kMaxBoost — near-silent noise floor shouldn't get blown up
// into a wall of jagged spikes.
inline float computeWaveformDisplayScale(const QVector<std::pair<float, float>>& peaks) {
    constexpr float kMaxBoost = 6.0f;
    constexpr float kSilenceFloor = 1e-4f;

    float peakAbs = 0.0f;
    for (const auto& [minV, maxV] : peaks) {
        peakAbs = std::max({peakAbs, std::fabs(minV), std::fabs(maxV)});
    }

    if (peakAbs <= kSilenceFloor) return 1.0f; // avoid dividing by ~0 on silence
    float scale = 1.0f / peakAbs;
    return std::clamp(scale, 1.0f, kMaxBoost);
}

} // namespace rsd
