#pragma once

#include <algorithm>
#include <cmath>

namespace rsd {

enum class SynthWaveform { Sine, Saw, Square, Triangle };

// phase is wrapped to [0, 1) internally. Returns a sample in [-1, 1].
inline float oscillatorSample(SynthWaveform wf, float phase) {
    phase -= std::floor(phase);
    switch (wf) {
        case SynthWaveform::Sine:
            return std::sin(phase * 2.0f * static_cast<float>(M_PI));
        case SynthWaveform::Saw:
            return 2.0f * phase - 1.0f;
        case SynthWaveform::Square:
            return phase < 0.5f ? 1.0f : -1.0f;
        case SynthWaveform::Triangle: {
            float t = phase < 0.5f ? phase : 1.0f - phase;
            return 4.0f * t - 1.0f;
        }
    }
    return 0.0f;
}

inline float phaseIncrement(float frequencyHz, float sampleRate) {
    if (sampleRate <= 0.0f) return 0.0f;
    return frequencyHz / sampleRate;
}

inline float midiNoteToFrequencyHz(int pitch) {
    return 440.0f * std::pow(2.0f, static_cast<float>(pitch - 69) / 12.0f);
}

enum class EnvelopeStage { Attack, Decay, Sustain, Release, Idle };

struct ADSRParams {
    float attackSeconds = 0.01f;
    float decaySeconds = 0.1f;
    float sustainLevel = 0.7f; // 0..1
    float releaseSeconds = 0.2f;
};

// Envelope value within a given stage, as a function of how long the stage
// has been running. The caller (SynthVoice) owns advancing between stages;
// these are pure per-stage shape functions.
inline float attackLevel(float timeInStage, const ADSRParams& p) {
    if (p.attackSeconds <= 0.0f) return 1.0f;
    return std::clamp(timeInStage / p.attackSeconds, 0.0f, 1.0f);
}

inline float decayLevel(float timeInStage, const ADSRParams& p) {
    if (p.decaySeconds <= 0.0f) return p.sustainLevel;
    float t = std::clamp(timeInStage / p.decaySeconds, 0.0f, 1.0f);
    return 1.0f + t * (p.sustainLevel - 1.0f);
}

inline float releaseLevel(float startLevel, float timeInStage, const ADSRParams& p) {
    if (p.releaseSeconds <= 0.0f) return 0.0f;
    float t = std::clamp(timeInStage / p.releaseSeconds, 0.0f, 1.0f);
    return startLevel * (1.0f - t);
}

// One-pole lowpass: coefficient closer to 1 = darker/more smoothing.
inline float onePoleLowpassCoefficient(float cutoffHz, float sampleRate) {
    if (sampleRate <= 0.0f) return 1.0f;
    float x = std::exp(-2.0f * static_cast<float>(M_PI) * cutoffHz / sampleRate);
    return std::clamp(x, 0.0f, 0.9999f);
}

// y[n] = (1-a)*x[n] + a*y[n-1]; `state` is the caller-owned y[n-1] slot.
inline float onePoleLowpassStep(float x, float& state, float coefficient) {
    state = (1.0f - coefficient) * x + coefficient * state;
    return state;
}

} // namespace rsd
