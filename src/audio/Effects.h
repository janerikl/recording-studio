#pragma once

#include <QUuid>
#include <atomic>
#include <memory>
#include <vector>

namespace rsd {

enum class EffectType { EQ, Compressor, Delay, Reverb };

// One node in a track's effect chain. Structural changes to the chain itself
// (add/remove/reorder) go through Track's copy-on-write snapshot swap, same
// as the clip list; but each effect's own parameters are plain atomics
// mutated in place, like Track::gainL/gainR, so twisting a knob doesn't
// require cloning the effect and losing its internal DSP state (filter
// memory, delay line contents, compressor envelope) mid-stream.
class Effect {
public:
    explicit Effect(EffectType t) : m_type(t) {}
    virtual ~Effect() = default;

    QUuid id = QUuid::createUuid();
    EffectType type() const { return m_type; }
    std::atomic<bool> bypassed{false};

    // Called once when the effect is prepared for playback (on construction
    // and whenever the engine's sample rate is known/changes). Resets any
    // internal DSP state.
    virtual void prepare(double sampleRate) = 0;

    // RT-safe in-place processing of one callback block. No locking, no
    // allocation. `buffer` is interleaved, `channels` frames of `nFrames`.
    virtual void process(float* buffer, unsigned int nFrames, unsigned int channels) = 0;

private:
    EffectType m_type;
};

using EffectChain = std::vector<std::shared_ptr<Effect>>;

// Runs every non-bypassed effect in order over `buffer`, in place. RT-safe:
// called once per track per audio callback.
void processEffectChain(const EffectChain& chain, float* buffer, unsigned int nFrames,
                         unsigned int channels);

// --- Concrete effects -------------------------------------------------

// Three-band EQ: low shelf, mid peaking, high shelf, each an RBJ-cookbook
// biquad. Coefficients are recomputed once per process() call from the
// current atomic parameter values (cheap relative to a full block of
// samples), so parameter changes take effect on the next callback with no
// audible zipper noise beyond that.
class EqEffect : public Effect {
public:
    EqEffect() : Effect(EffectType::EQ) {}

    std::atomic<float> lowGainDb{0.0f};
    std::atomic<float> lowFreqHz{120.0f};
    std::atomic<float> midGainDb{0.0f};
    std::atomic<float> midFreqHz{1000.0f};
    std::atomic<float> midQ{0.7f};
    std::atomic<float> highGainDb{0.0f};
    std::atomic<float> highFreqHz{8000.0f};

    void prepare(double sampleRate) override;
    void process(float* buffer, unsigned int nFrames, unsigned int channels) override;

private:
    struct Biquad {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        // Direct Form I state, one slot per channel (app is stereo today;
        // channel index beyond this is clamped to slot 1).
        float x1[2]{0.0f, 0.0f}, x2[2]{0.0f, 0.0f};
        float y1[2]{0.0f, 0.0f}, y2[2]{0.0f, 0.0f};

        float processSample(float x, int ch);
        void reset();
    };

    static void designPeaking(Biquad& bq, double sampleRate, float freq, float gainDb, float q);
    static void designLowShelf(Biquad& bq, double sampleRate, float freq, float gainDb);
    static void designHighShelf(Biquad& bq, double sampleRate, float freq, float gainDb);

    double m_sampleRate = 48000.0;
    Biquad m_low, m_mid, m_high;
};

// Feedforward peak compressor with an exponential attack/release envelope
// follower applied to the gain-reduction amount (in dB).
class CompressorEffect : public Effect {
public:
    CompressorEffect() : Effect(EffectType::Compressor) {}

    std::atomic<float> thresholdDb{-18.0f};
    std::atomic<float> ratio{4.0f};
    std::atomic<float> attackMs{10.0f};
    std::atomic<float> releaseMs{100.0f};
    std::atomic<float> makeupDb{0.0f};

    void prepare(double sampleRate) override;
    void process(float* buffer, unsigned int nFrames, unsigned int channels) override;

private:
    double m_sampleRate = 48000.0;
    float m_envelopeDb = 0.0f; // audio-thread-only state
};

// Stereo delay line with feedback and wet/dry mix.
class DelayEffect : public Effect {
public:
    DelayEffect() : Effect(EffectType::Delay) {}

    std::atomic<float> delayMs{300.0f};
    std::atomic<float> feedback{0.35f};
    std::atomic<float> mix{0.3f};

    void prepare(double sampleRate) override;
    void process(float* buffer, unsigned int nFrames, unsigned int channels) override;

private:
    double m_sampleRate = 48000.0;
    std::vector<float> m_bufferL;
    std::vector<float> m_bufferR;
    size_t m_writePos = 0;
};

// Small Freeverb-style algorithmic reverb (parallel combs + series
// allpasses), processed on the mono sum of the input and mixed back into
// every channel.
class ReverbEffect : public Effect {
public:
    ReverbEffect() : Effect(EffectType::Reverb) {}

    std::atomic<float> roomSize{0.5f}; // 0..1
    std::atomic<float> damping{0.5f};  // 0..1
    std::atomic<float> mix{0.25f};     // wet 0..1

    void prepare(double sampleRate) override;
    void process(float* buffer, unsigned int nFrames, unsigned int channels) override;

private:
    struct Comb {
        std::vector<float> buf;
        size_t pos = 0;
        float feedback = 0.5f;
        float damp1 = 0.5f;
        float damp2 = 0.5f;
        float filterStore = 0.0f;

        void resize(size_t n) {
            buf.assign(n, 0.0f);
            pos = 0;
            filterStore = 0.0f;
        }
        float process(float input);
    };
    struct Allpass {
        std::vector<float> buf;
        size_t pos = 0;
        float feedback = 0.5f;

        void resize(size_t n) {
            buf.assign(n, 0.0f);
            pos = 0;
        }
        float process(float input);
    };

    static constexpr int kNumCombs = 4;
    static constexpr int kNumAllpasses = 2;
    double m_sampleRate = 48000.0;
    Comb m_combs[kNumCombs];
    Allpass m_allpasses[kNumAllpasses];
};

} // namespace rsd
