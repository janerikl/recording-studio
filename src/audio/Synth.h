#pragma once

#include <array>
#include <atomic>

#include "SynthMath.h"

namespace rsd {

// Live-tweakable synth parameters for an Instrument track — same
// atomics-mutated-in-place pattern as Effect (see Effects.h), so turning a
// knob doesn't require cloning/swapping anything mid-stream.
struct SynthParams {
    std::atomic<int> waveform{static_cast<int>(SynthWaveform::Saw)};
    std::atomic<float> attackSeconds{0.01f};
    std::atomic<float> decaySeconds{0.1f};
    std::atomic<float> sustainLevel{0.7f};
    std::atomic<float> releaseSeconds{0.2f};
    std::atomic<float> filterCutoffHz{8000.0f};
};

// One voice of polyphony: owns its own oscillator phase, envelope stage,
// and filter state. RT-safe (no allocation after construction).
class SynthVoice {
public:
    void noteOn(int pitch, float velocity, float sampleRate) {
        m_pitch = pitch;
        m_velocity = velocity;
        m_phase = 0.0f;
        m_stage = EnvelopeStage::Attack;
        m_stageTime = 0.0f;
        m_sampleRate = sampleRate;
        m_active = true;
        m_filterState = 0.0f;
    }

    void noteOff() {
        if (!m_active) return;
        m_releaseStartLevel = m_lastEnvelope;
        m_stage = EnvelopeStage::Release;
        m_stageTime = 0.0f;
    }

    bool active() const { return m_active; }
    int pitch() const { return m_pitch; }

    // Adds nFrames of this voice's output into `out` (interleaved,
    // `channels` channels, centered — same sample added to every channel).
    void render(float* out, unsigned int nFrames, unsigned int channels, const SynthParams& params) {
        if (!m_active || m_sampleRate <= 0.0f) return;

        float freq = midiNoteToFrequencyHz(m_pitch);
        float inc = phaseIncrement(freq, m_sampleRate);
        ADSRParams adsr{params.attackSeconds.load(std::memory_order_relaxed),
                         params.decaySeconds.load(std::memory_order_relaxed),
                         params.sustainLevel.load(std::memory_order_relaxed),
                         params.releaseSeconds.load(std::memory_order_relaxed)};
        auto wf = static_cast<SynthWaveform>(params.waveform.load(std::memory_order_relaxed));
        float coeff =
            onePoleLowpassCoefficient(params.filterCutoffHz.load(std::memory_order_relaxed), m_sampleRate);
        float dt = 1.0f / m_sampleRate;

        for (unsigned int i = 0; i < nFrames; ++i) {
            float env = advanceEnvelope(adsr, dt);
            if (m_stage == EnvelopeStage::Idle) {
                m_active = false;
                break;
            }
            float sample = oscillatorSample(wf, m_phase) * env * m_velocity;
            sample = onePoleLowpassStep(sample, m_filterState, coeff);
            m_phase += inc;
            for (unsigned int ch = 0; ch < channels; ++ch) out[i * channels + ch] += sample;
        }
    }

private:
    float advanceEnvelope(const ADSRParams& adsr, float dt) {
        m_stageTime += dt;
        float env = 0.0f;
        switch (m_stage) {
            case EnvelopeStage::Attack:
                env = attackLevel(m_stageTime, adsr);
                if (m_stageTime >= adsr.attackSeconds) {
                    m_stage = EnvelopeStage::Decay;
                    m_stageTime = 0.0f;
                }
                break;
            case EnvelopeStage::Decay:
                env = decayLevel(m_stageTime, adsr);
                if (m_stageTime >= adsr.decaySeconds) {
                    m_stage = EnvelopeStage::Sustain;
                    m_stageTime = 0.0f;
                }
                break;
            case EnvelopeStage::Sustain:
                env = adsr.sustainLevel;
                break;
            case EnvelopeStage::Release:
                env = releaseLevel(m_releaseStartLevel, m_stageTime, adsr);
                if (m_stageTime >= adsr.releaseSeconds) {
                    m_stage = EnvelopeStage::Idle;
                    env = 0.0f;
                }
                break;
            case EnvelopeStage::Idle:
                env = 0.0f;
                break;
        }
        m_lastEnvelope = env;
        return env;
    }

    bool m_active = false;
    int m_pitch = 60;
    float m_velocity = 1.0f;
    float m_phase = 0.0f;
    float m_sampleRate = 48000.0f;
    EnvelopeStage m_stage = EnvelopeStage::Idle;
    float m_stageTime = 0.0f;
    float m_lastEnvelope = 0.0f;
    float m_releaseStartLevel = 0.0f;
    float m_filterState = 0.0f;
};

// Fixed-size polyphonic voice pool — RT-safe, no allocation after
// construction. Only the RT thread ever touches this (Track exposes it as a
// plain, non-atomic member, same trust model as PunchRecorder).
class SynthEngine {
public:
    static constexpr int kMaxVoices = 8;

    void noteOn(int pitch, float velocity, float sampleRate) {
        for (auto& v : m_voices) {
            if (!v.active()) {
                v.noteOn(pitch, velocity, sampleRate);
                return;
            }
        }
        m_voices[0].noteOn(pitch, velocity, sampleRate); // simple oldest-slot steal
    }

    void noteOff(int pitch) {
        for (auto& v : m_voices) {
            if (v.active() && v.pitch() == pitch) v.noteOff();
        }
    }

    void render(float* out, unsigned int nFrames, unsigned int channels, const SynthParams& params) {
        for (auto& v : m_voices) {
            if (v.active()) v.render(out, nFrames, channels, params);
        }
    }

private:
    std::array<SynthVoice, kMaxVoices> m_voices;
};

} // namespace rsd
