#pragma once

#include <algorithm>
#include <cstdint>

#include "MidiNoteDisplayMath.h"
#include "model/MidiNote.h"

namespace rsd {

// --- tempo / grid snapping ---

// Number of samples spanning one quarter-note beat at the given tempo.
inline int64_t samplesPerBeat(double bpm, int sampleRate) {
    if (bpm <= 0.0) return 0;
    return static_cast<int64_t>(60.0 / bpm * static_cast<double>(sampleRate));
}

// Snap grid interval in samples for a note denominator (4 = quarter, 8 =
// eighth, 16 = sixteenth, 32 = thirty-second). denominator == 0 means
// snapping is off.
inline int64_t snapIntervalSamples(int64_t beatSamples, int denominator) {
    if (denominator <= 0) return 0;
    return beatSamples * 4 / denominator;
}

// Rounds a sample position to the nearest multiple of intervalSamples.
// intervalSamples <= 0 disables snapping (value passed through unchanged,
// clamped to non-negative).
inline int64_t snapToGrid(int64_t sample, int64_t intervalSamples) {
    if (sample < 0) sample = 0;
    if (intervalSamples <= 0) return sample;
    int64_t half = intervalSamples / 2;
    int64_t snapped = ((sample + half) / intervalSamples) * intervalSamples;
    return snapped;
}

// --- pitch <-> Y for the full piano-roll grid (fixed-height rows, unlike
// the mini inline view's continuous pitchToY) ---

inline int pitchToRowY(int pitch, int laneHeight, int lowPitch, int highPitch, int rowHeight) {
    (void)laneHeight;
    int clamped = std::clamp(pitch, lowPitch, highPitch);
    return (highPitch - clamped) * rowHeight;
}

inline int yToPitch(int y, int laneHeight, int lowPitch, int highPitch, int rowHeight) {
    (void)laneHeight;
    if (rowHeight <= 0) return lowPitch;
    if (y < 0) return highPitch;
    int row = y / rowHeight;
    int pitch = highPitch - row;
    return std::clamp(pitch, lowPitch, highPitch);
}

// --- hit-testing ---

struct MidiNoteEditRect {
    int x = 0;
    int w = 0;
    int y = 0;
    int h = 0;
};

inline bool rectContainsPoint(const MidiNoteEditRect& r, int px, int py) {
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

inline bool isInResizeZone(const MidiNoteEditRect& r, int px, int resizeMarginPx) {
    int rightEdge = r.x + r.w;
    return px >= rightEdge - resizeMarginPx && px <= rightEdge;
}

// --- move / resize ---

inline int64_t applyMoveDeltaSamples(int64_t origStartSample, int64_t deltaSamples,
                                      int64_t minStartSample) {
    int64_t result = origStartSample + deltaSamples;
    return std::max(minStartSample, result);
}

inline int applyMovePitchDelta(int origPitch, int deltaPitch, int lowPitch, int highPitch) {
    return std::clamp(origPitch + deltaPitch, lowPitch, highPitch);
}

// Clamps a resize drag's requested absolute length (e.g. cursor sample minus
// note start sample) to a sane minimum so a note can't be shrunk to zero or
// negative length.
inline int64_t resizeLengthSamples(int64_t requestedLengthSamples, int64_t minLength) {
    return std::max(minLength, requestedLengthSamples);
}

// --- velocity ---

inline float clampVelocity(float velocity) { return std::clamp(velocity, 0.0f, 1.0f); }

// Velocity lane is drawn top (velocity 1.0) to bottom (velocity 0.0),
// mirroring how a vertical drag position maps to a value elsewhere in the
// app (e.g. clip gain).
inline float velocityFromLaneY(int y, int laneHeightPx) {
    if (laneHeightPx <= 0) return 1.0f;
    float t = 1.0f - static_cast<float>(y) / static_cast<float>(laneHeightPx);
    return clampVelocity(t);
}

} // namespace rsd
