#pragma once

#include <cstddef>
#include <vector>

namespace rsd {

// Highlight-and-wait practice logic: advances past the expected note at
// `currentIndex` only if `playedPitch` matches it. Already-complete
// (currentIndex >= pitches.size()) stays put.
inline size_t practiceAdvance(const std::vector<int>& pitches, size_t currentIndex, int playedPitch) {
    if (currentIndex >= pitches.size()) return currentIndex;
    if (pitches[currentIndex] == playedPitch) return currentIndex + 1;
    return currentIndex;
}

inline bool practiceComplete(const std::vector<int>& pitches, size_t currentIndex) {
    return currentIndex >= pitches.size();
}

} // namespace rsd
