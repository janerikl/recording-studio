#pragma once

#include <cmath>
#include <vector>

#include "model/RhythmPattern.h"

namespace rsd {

// Cumulative start beat of each non-rest note in the pattern (rests
// advance the running clock but produce no onset of their own).
inline std::vector<double> onsetBeats(const RhythmPattern& pattern) {
    std::vector<double> onsets;
    double t = 0.0;
    for (auto& note : pattern.notes) {
        if (!note.isRest) onsets.push_back(t);
        t += note.beats;
    }
    return onsets;
}

// Full duration of the pattern in beats, rests included.
inline double patternTotalBeats(const RhythmPattern& pattern) {
    double t = 0.0;
    for (auto& note : pattern.notes) t += note.beats;
    return t;
}

inline double beatsToSeconds(double beats, double bpm) {
    if (bpm <= 0.0) return 0.0;
    return beats * 60.0 / bpm;
}

enum class TapVerdict { Hit, Early, Late, Miss };

// offsetSeconds = actual tap time - expected onset time.
inline TapVerdict classifyTapOffset(double offsetSeconds, double toleranceSeconds) {
    if (std::abs(offsetSeconds) <= toleranceSeconds) return TapVerdict::Hit;
    return offsetSeconds < 0.0 ? TapVerdict::Early : TapVerdict::Late;
}

struct RhythmTapResult {
    std::vector<TapVerdict> verdicts; // one per expected onset (Miss if no tap matched it)
    int missedCount = 0;
    int extraTapCount = 0;
    double accuracyPercent = 0.0;
};

// Greedy nearest-match of tapped timestamps to expected onsets. A tap
// beyond 3x the tolerance from any expected onset isn't considered a
// match for it (that onset is scored Miss instead) — wide enough that a
// badly mistimed-but-intentional tap still gets an Early/Late verdict
// rather than being discarded, narrow enough that unrelated taps don't
// steal a match from a nearby onset.
inline RhythmTapResult scoreTaps(const std::vector<double>& expectedOnsets,
                                  const std::vector<double>& tappedTimes, double toleranceSeconds) {
    RhythmTapResult result;
    result.verdicts.assign(expectedOnsets.size(), TapVerdict::Miss);
    std::vector<bool> used(tappedTimes.size(), false);
    double matchWindow = toleranceSeconds * 3.0;

    for (size_t i = 0; i < expectedOnsets.size(); ++i) {
        int bestIdx = -1;
        double bestDist = matchWindow;
        for (size_t j = 0; j < tappedTimes.size(); ++j) {
            if (used[j]) continue;
            double dist = std::abs(tappedTimes[j] - expectedOnsets[i]);
            if (dist <= bestDist) {
                bestDist = dist;
                bestIdx = static_cast<int>(j);
            }
        }
        if (bestIdx >= 0) {
            used[static_cast<size_t>(bestIdx)] = true;
            result.verdicts[i] =
                classifyTapOffset(tappedTimes[static_cast<size_t>(bestIdx)] - expectedOnsets[i], toleranceSeconds);
        } else {
            ++result.missedCount;
        }
    }
    for (bool u : used) {
        if (!u) ++result.extraTapCount;
    }

    int hits = 0;
    for (auto v : result.verdicts) {
        if (v == TapVerdict::Hit) ++hits;
    }
    result.accuracyPercent =
        expectedOnsets.empty() ? 0.0 : (100.0 * static_cast<double>(hits) / static_cast<double>(expectedOnsets.size()));
    return result;
}

} // namespace rsd
