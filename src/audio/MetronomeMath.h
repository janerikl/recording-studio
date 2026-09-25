#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

namespace rsd {

inline double beatDurationSamples(double bpm, int sampleRate) {
    if (bpm <= 0.0) return 0.0;
    return 60.0 / bpm * static_cast<double>(sampleRate);
}

// If a beat boundary (a multiple of beatDurationSamples) falls within
// [pos, pos + nFrames), returns its offset from the start of the block.
// Block-level granularity (at most one tick reported per call) — same
// simplification as the existing MIDI-note block-timing in SessionMixer;
// fine at normal tempos/block sizes where a beat is much longer than a
// block, but a very short block at a very high BPM could in theory skip a
// tick. Not exercised at this codebase's actual block size (512 frames).
inline std::optional<int> firstClickOffsetInBlock(int64_t pos, unsigned int nFrames,
                                                   double beatDurationSamples) {
    if (beatDurationSamples <= 0.0) return std::nullopt;
    int64_t n = static_cast<int64_t>(std::ceil(static_cast<double>(pos) / beatDurationSamples));
    int64_t tickSample = static_cast<int64_t>(std::llround(static_cast<double>(n) * beatDurationSamples));
    if (tickSample < pos) tickSample = static_cast<int64_t>(std::llround(static_cast<double>(n + 1) * beatDurationSamples));
    int64_t blockEnd = pos + static_cast<int64_t>(nFrames);
    if (tickSample >= pos && tickSample < blockEnd) return static_cast<int>(tickSample - pos);
    return std::nullopt;
}

} // namespace rsd
