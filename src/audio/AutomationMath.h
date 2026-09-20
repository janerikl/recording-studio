#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace rsd {

// One breakpoint on an automation curve.
struct AutomationPoint {
    int64_t sample = 0;
    float value = 0.0f;
};

// Evaluates an automation curve at the given sample position: linear
// interpolation between the two nearest points, clamped to the endpoint
// value outside the curve's range. `fallback` is returned when the curve
// has no points at all (i.e. automation hasn't been touched, static
// track parameter applies instead). Assumes `points` is sorted by sample.
inline float evaluateAutomation(const std::vector<AutomationPoint>& points, int64_t sample,
                                 float fallback) {
    if (points.empty()) return fallback;
    if (points.size() == 1 || sample <= points.front().sample) return points.front().value;
    if (sample >= points.back().sample) return points.back().value;

    for (size_t i = 0; i + 1 < points.size(); ++i) {
        const auto& a = points[i];
        const auto& b = points[i + 1];
        if (sample >= a.sample && sample <= b.sample) {
            if (b.sample == a.sample) return b.value;
            float t = static_cast<float>(sample - a.sample) / static_cast<float>(b.sample - a.sample);
            return a.value + t * (b.value - a.value);
        }
    }
    return points.back().value; // unreachable given the guards above
}

// Returns the index of the point within `toleranceSamples` of `sample`
// (closest one if several qualify), or -1 if none is within tolerance.
inline int findPointNear(const std::vector<AutomationPoint>& points, int64_t sample,
                          int64_t toleranceSamples) {
    int best = -1;
    int64_t bestDist = toleranceSamples + 1;
    for (size_t i = 0; i < points.size(); ++i) {
        int64_t dist = std::abs(points[i].sample - sample);
        if (dist <= toleranceSamples && dist < bestDist) {
            bestDist = dist;
            best = static_cast<int>(i);
        }
    }
    return best;
}

// Inserts a point keeping the list sorted by sample; replaces an existing
// point at the exact same sample rather than creating a duplicate.
inline void insertPointSorted(std::vector<AutomationPoint>& points, AutomationPoint point) {
    auto it = std::lower_bound(points.begin(), points.end(), point,
                                [](const AutomationPoint& a, const AutomationPoint& b) {
                                    return a.sample < b.sample;
                                });
    if (it != points.end() && it->sample == point.sample) {
        *it = point;
    } else {
        points.insert(it, point);
    }
}

} // namespace rsd
