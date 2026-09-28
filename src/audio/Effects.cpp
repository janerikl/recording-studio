#include "Effects.h"

#include <algorithm>
#include <cmath>

namespace rsd {

void processEffectChain(const EffectChain& chain, float* buffer, unsigned int nFrames,
                         unsigned int channels) {
    for (auto& fx : chain) {
        if (fx->bypassed.load(std::memory_order_relaxed)) continue;
        fx->process(buffer, nFrames, channels);
    }
}

// --- EqEffect -----------------------------------------------------------

float EqEffect::Biquad::processSample(float x, int ch) {
    ch = ch == 0 ? 0 : 1;
    float y = b0 * x + b1 * x1[ch] + b2 * x2[ch] - a1 * y1[ch] - a2 * y2[ch];
    x2[ch] = x1[ch];
    x1[ch] = x;
    y2[ch] = y1[ch];
    y1[ch] = y;
    return y;
}

void EqEffect::Biquad::reset() {
    x1[0] = x1[1] = x2[0] = x2[1] = 0.0f;
    y1[0] = y1[1] = y2[0] = y2[1] = 0.0f;
}

void EqEffect::designPeaking(Biquad& bq, double sampleRate, float freq, float gainDb, float q) {
    double A = std::pow(10.0, gainDb / 40.0);
    double w0 = 2.0 * M_PI * freq / sampleRate;
    double alpha = std::sin(w0) / (2.0 * std::max(q, 0.01f));
    double cosw0 = std::cos(w0);

    double b0 = 1.0 + alpha * A;
    double b1 = -2.0 * cosw0;
    double b2 = 1.0 - alpha * A;
    double a0 = 1.0 + alpha / A;
    double a1 = -2.0 * cosw0;
    double a2 = 1.0 - alpha / A;

    bq.b0 = static_cast<float>(b0 / a0);
    bq.b1 = static_cast<float>(b1 / a0);
    bq.b2 = static_cast<float>(b2 / a0);
    bq.a1 = static_cast<float>(a1 / a0);
    bq.a2 = static_cast<float>(a2 / a0);
}

void EqEffect::designLowShelf(Biquad& bq, double sampleRate, float freq, float gainDb) {
    double A = std::pow(10.0, gainDb / 40.0);
    double w0 = 2.0 * M_PI * freq / sampleRate;
    double cosw0 = std::cos(w0);
    double sinw0 = std::sin(w0);
    double alpha = sinw0 / 2.0 * std::sqrt(2.0); // shelf slope S = 1
    double sqrtA = std::sqrt(A);

    double b0 = A * ((A + 1) - (A - 1) * cosw0 + 2 * sqrtA * alpha);
    double b1 = 2 * A * ((A - 1) - (A + 1) * cosw0);
    double b2 = A * ((A + 1) - (A - 1) * cosw0 - 2 * sqrtA * alpha);
    double a0 = (A + 1) + (A - 1) * cosw0 + 2 * sqrtA * alpha;
    double a1 = -2 * ((A - 1) + (A + 1) * cosw0);
    double a2 = (A + 1) + (A - 1) * cosw0 - 2 * sqrtA * alpha;

    bq.b0 = static_cast<float>(b0 / a0);
    bq.b1 = static_cast<float>(b1 / a0);
    bq.b2 = static_cast<float>(b2 / a0);
    bq.a1 = static_cast<float>(a1 / a0);
    bq.a2 = static_cast<float>(a2 / a0);
}

void EqEffect::designHighShelf(Biquad& bq, double sampleRate, float freq, float gainDb) {
    double A = std::pow(10.0, gainDb / 40.0);
    double w0 = 2.0 * M_PI * freq / sampleRate;
    double cosw0 = std::cos(w0);
    double sinw0 = std::sin(w0);
    double alpha = sinw0 / 2.0 * std::sqrt(2.0); // shelf slope S = 1
    double sqrtA = std::sqrt(A);

    double b0 = A * ((A + 1) + (A - 1) * cosw0 + 2 * sqrtA * alpha);
    double b1 = -2 * A * ((A - 1) + (A + 1) * cosw0);
    double b2 = A * ((A + 1) + (A - 1) * cosw0 - 2 * sqrtA * alpha);
    double a0 = (A + 1) - (A - 1) * cosw0 + 2 * sqrtA * alpha;
    double a1 = 2 * ((A - 1) - (A + 1) * cosw0);
    double a2 = (A + 1) - (A - 1) * cosw0 - 2 * sqrtA * alpha;

    bq.b0 = static_cast<float>(b0 / a0);
    bq.b1 = static_cast<float>(b1 / a0);
    bq.b2 = static_cast<float>(b2 / a0);
    bq.a1 = static_cast<float>(a1 / a0);
    bq.a2 = static_cast<float>(a2 / a0);
}

void EqEffect::prepare(double sampleRate) {
    m_sampleRate = sampleRate;
    m_low.reset();
    m_mid.reset();
    m_high.reset();
}

void EqEffect::process(float* buffer, unsigned int nFrames, unsigned int channels) {
    float lowGain = lowGainDb.load(std::memory_order_relaxed);
    float lowFreq = lowFreqHz.load(std::memory_order_relaxed);
    float midGain = midGainDb.load(std::memory_order_relaxed);
    float midFreq = midFreqHz.load(std::memory_order_relaxed);
    float q = midQ.load(std::memory_order_relaxed);
    float highGain = highGainDb.load(std::memory_order_relaxed);
    float highFreq = highFreqHz.load(std::memory_order_relaxed);

    designLowShelf(m_low, m_sampleRate, lowFreq, lowGain);
    designPeaking(m_mid, m_sampleRate, midFreq, midGain, q);
    designHighShelf(m_high, m_sampleRate, highFreq, highGain);

    for (unsigned int i = 0; i < nFrames; ++i) {
        for (unsigned int ch = 0; ch < channels; ++ch) {
            int slot = ch == 0 ? 0 : 1;
            float x = buffer[i * channels + ch];
            x = m_low.processSample(x, slot);
            x = m_mid.processSample(x, slot);
            x = m_high.processSample(x, slot);
            buffer[i * channels + ch] = x;
        }
    }
}

// --- CompressorEffect -----------------------------------------------------

void CompressorEffect::prepare(double sampleRate) {
    m_sampleRate = sampleRate;
    m_envelopeDb = 0.0f;
}

void CompressorEffect::process(float* buffer, unsigned int nFrames, unsigned int channels) {
    float threshold = thresholdDb.load(std::memory_order_relaxed);
    float ratioV = std::max(ratio.load(std::memory_order_relaxed), 1.0f);
    float attackMsV = std::max(attackMs.load(std::memory_order_relaxed), 0.001f);
    float releaseMsV = std::max(releaseMs.load(std::memory_order_relaxed), 0.001f);
    float makeupLin = std::pow(10.0f, makeupDb.load(std::memory_order_relaxed) / 20.0f);

    float attackCoeff = std::exp(-1.0f / (0.001f * attackMsV * static_cast<float>(m_sampleRate)));
    float releaseCoeff = std::exp(-1.0f / (0.001f * releaseMsV * static_cast<float>(m_sampleRate)));

    for (unsigned int i = 0; i < nFrames; ++i) {
        float peak = 0.0f;
        for (unsigned int ch = 0; ch < channels; ++ch) {
            peak = std::max(peak, std::abs(buffer[i * channels + ch]));
        }
        float peakDb = 20.0f * std::log10(std::max(peak, 1e-9f));
        float over = peakDb - threshold;
        // Negative dB = gain reduction; 0 = no reduction.
        float targetReductionDb = over > 0.0f ? -over * (1.0f - 1.0f / ratioV) : 0.0f;

        // Attack when reduction is increasing (target more negative than
        // current envelope), release when it's easing back toward 0.
        float coeff = (targetReductionDb < m_envelopeDb) ? attackCoeff : releaseCoeff;
        m_envelopeDb = coeff * m_envelopeDb + (1.0f - coeff) * targetReductionDb;

        float gainLin = std::pow(10.0f, m_envelopeDb / 20.0f) * makeupLin;
        for (unsigned int ch = 0; ch < channels; ++ch) {
            buffer[i * channels + ch] *= gainLin;
        }
    }
}

// --- DelayEffect ------------------------------------------------------

void NoiseGateEffect::prepare(double sampleRate) {
    m_sampleRate = sampleRate;
    m_envelope = 1.0f;
    m_holdCounter = 0;
}

void NoiseGateEffect::process(float* buffer, unsigned int nFrames, unsigned int channels) {
    float threshold = thresholdDb.load(std::memory_order_relaxed);
    float attackMsV = std::max(attackMs.load(std::memory_order_relaxed), 0.001f);
    float releaseMsV = std::max(releaseMs.load(std::memory_order_relaxed), 0.001f);
    float rangeLin = std::pow(10.0f, rangeDb.load(std::memory_order_relaxed) / 20.0f);
    size_t holdSamples =
        static_cast<size_t>(std::max(holdMs.load(std::memory_order_relaxed), 0.0f) * 0.001f *
                             static_cast<float>(m_sampleRate));

    float attackCoeff = std::exp(-1.0f / (0.001f * attackMsV * static_cast<float>(m_sampleRate)));
    float releaseCoeff = std::exp(-1.0f / (0.001f * releaseMsV * static_cast<float>(m_sampleRate)));

    for (unsigned int i = 0; i < nFrames; ++i) {
        float peak = 0.0f;
        for (unsigned int ch = 0; ch < channels; ++ch) {
            peak = std::max(peak, std::abs(buffer[i * channels + ch]));
        }
        float peakDb = 20.0f * std::log10(std::max(peak, 1e-9f));

        float target;
        if (peakDb > threshold) {
            target = 1.0f;
            m_holdCounter = holdSamples;
        } else if (m_holdCounter > 0) {
            target = 1.0f;
            --m_holdCounter;
        } else {
            target = rangeLin;
        }

        float coeff = (target > m_envelope) ? attackCoeff : releaseCoeff;
        m_envelope = coeff * m_envelope + (1.0f - coeff) * target;

        for (unsigned int ch = 0; ch < channels; ++ch) {
            buffer[i * channels + ch] *= m_envelope;
        }
    }
}

void LimiterEffect::prepare(double sampleRate) {
    m_sampleRate = sampleRate;
    float ms = std::max(lookaheadMs.load(std::memory_order_relaxed), 0.1f);
    m_lookaheadSamples = std::max<size_t>(1, static_cast<size_t>(ms * 0.001 * sampleRate));
    for (int ch = 0; ch < kMaxChannels; ++ch) {
        m_delayBuf[ch].assign(m_lookaheadSamples, 0.0f);
    }
    m_peakWindow.assign(m_lookaheadSamples, 0.0f);
    m_pos = 0;
    m_currentGain = 1.0f;
}

void LimiterEffect::process(float* buffer, unsigned int nFrames, unsigned int channels) {
    size_t n = m_lookaheadSamples;
    if (n == 0) return;

    float ceilingLin = std::pow(10.0f, ceilingDb.load(std::memory_order_relaxed) / 20.0f);
    float releaseMsV = std::max(releaseMs.load(std::memory_order_relaxed), 0.001f);
    float releaseCoeff = std::exp(-1.0f / (0.001f * releaseMsV * static_cast<float>(m_sampleRate)));

    for (unsigned int i = 0; i < nFrames; ++i) {
        float peak = 0.0f;
        for (unsigned int ch = 0; ch < channels; ++ch) {
            peak = std::max(peak, std::abs(buffer[i * channels + ch]));
        }

        // Read the sample that's about to be overwritten: it was written
        // exactly `n` steps ago, so this is the delayed output.
        float delayed[kMaxChannels];
        for (int ch = 0; ch < kMaxChannels; ++ch) delayed[ch] = m_delayBuf[ch][m_pos];

        for (unsigned int ch = 0; ch < channels; ++ch) {
            int slot = std::min(static_cast<int>(ch), kMaxChannels - 1);
            m_delayBuf[slot][m_pos] = buffer[i * channels + ch];
        }
        m_peakWindow[m_pos] = peak;

        // Deliberately simple O(lookahead) scan each sample, mirroring the
        // rest of this file's preference for clarity over micro-optimizing
        // a small, fixed-size window (a few hundred samples at most).
        float windowPeak = 0.0f;
        for (float v : m_peakWindow) windowPeak = std::max(windowPeak, v);

        float targetGain = windowPeak > 1e-9f ? std::min(1.0f, ceilingLin / windowPeak) : 1.0f;
        if (targetGain < m_currentGain) {
            // Drop immediately: the lookahead window already saw this peak
            // coming, so there's no reason to smooth the attack.
            m_currentGain = targetGain;
        } else {
            m_currentGain = releaseCoeff * m_currentGain + (1.0f - releaseCoeff) * targetGain;
        }

        for (unsigned int ch = 0; ch < channels; ++ch) {
            int slot = std::min(static_cast<int>(ch), kMaxChannels - 1);
            float out = delayed[slot] * m_currentGain;
            buffer[i * channels + ch] = std::clamp(out, -ceilingLin, ceilingLin);
        }

        m_pos = (m_pos + 1) % n;
    }
}

void DelayEffect::prepare(double sampleRate) {
    m_sampleRate = sampleRate;
    size_t maxDelaySamples = static_cast<size_t>(sampleRate * 2.0) + 1; // up to 2s
    m_bufferL.assign(maxDelaySamples, 0.0f);
    m_bufferR.assign(maxDelaySamples, 0.0f);
    m_writePos = 0;
}

void DelayEffect::process(float* buffer, unsigned int nFrames, unsigned int channels) {
    if (m_bufferL.empty()) return;

    float delayMsV = delayMs.load(std::memory_order_relaxed);
    float fb = std::clamp(feedback.load(std::memory_order_relaxed), 0.0f, 0.95f);
    float mixV = std::clamp(mix.load(std::memory_order_relaxed), 0.0f, 1.0f);

    size_t bufLen = m_bufferL.size();
    size_t delaySamples = static_cast<size_t>(std::max(1.0f, delayMsV * 0.001f * static_cast<float>(m_sampleRate)));
    delaySamples = std::min(delaySamples, bufLen - 1);

    for (unsigned int i = 0; i < nFrames; ++i) {
        size_t readPos = (m_writePos + bufLen - delaySamples) % bufLen;

        float inL = buffer[i * channels + 0];
        float inR = channels > 1 ? buffer[i * channels + 1] : inL;
        float delayedL = m_bufferL[readPos];
        float delayedR = m_bufferR[readPos];

        m_bufferL[m_writePos] = inL + delayedL * fb;
        m_bufferR[m_writePos] = inR + delayedR * fb;
        m_writePos = (m_writePos + 1) % bufLen;

        float outL = inL * (1.0f - mixV) + delayedL * mixV;
        float outR = inR * (1.0f - mixV) + delayedR * mixV;
        buffer[i * channels + 0] = outL;
        if (channels > 1) buffer[i * channels + 1] = outR;
    }
}

// --- ReverbEffect -------------------------------------------------------

float ReverbEffect::Comb::process(float input) {
    float output = buf[pos];
    filterStore = output * damp2 + filterStore * damp1;
    buf[pos] = input + filterStore * feedback;
    pos = (pos + 1) % buf.size();
    return output;
}

float ReverbEffect::Allpass::process(float input) {
    float bufOut = buf[pos];
    float output = -input + bufOut;
    buf[pos] = input + bufOut * feedback;
    pos = (pos + 1) % buf.size();
    return output;
}

void ReverbEffect::prepare(double sampleRate) {
    m_sampleRate = sampleRate;
    // Classic Freeverb comb/allpass tunings (samples at 44100Hz), scaled to
    // the actual sample rate.
    static const int kCombTunings[kNumCombs] = {1116, 1188, 1277, 1356};
    static const int kAllpassTunings[kNumAllpasses] = {556, 441};
    double scale = sampleRate / 44100.0;

    for (int i = 0; i < kNumCombs; ++i) {
        size_t len = static_cast<size_t>(kCombTunings[i] * scale);
        m_combs[i].resize(std::max<size_t>(len, 1));
    }
    for (int i = 0; i < kNumAllpasses; ++i) {
        size_t len = static_cast<size_t>(kAllpassTunings[i] * scale);
        m_allpasses[i].resize(std::max<size_t>(len, 1));
        m_allpasses[i].feedback = 0.5f;
    }
}

void ReverbEffect::process(float* buffer, unsigned int nFrames, unsigned int channels) {
    if (channels == 0) return;

    float rs = std::clamp(roomSize.load(std::memory_order_relaxed), 0.0f, 1.0f);
    float dp = std::clamp(damping.load(std::memory_order_relaxed), 0.0f, 1.0f);
    float mixV = std::clamp(mix.load(std::memory_order_relaxed), 0.0f, 1.0f);

    float fb = rs * 0.28f + 0.7f;
    float damp1 = dp * 0.4f;
    float damp2 = 1.0f - damp1;
    for (auto& c : m_combs) {
        c.feedback = fb;
        c.damp1 = damp1;
        c.damp2 = damp2;
    }

    for (unsigned int i = 0; i < nFrames; ++i) {
        float inMono = 0.0f;
        for (unsigned int ch = 0; ch < channels; ++ch) inMono += buffer[i * channels + ch];
        inMono /= static_cast<float>(channels);

        float wet = 0.0f;
        for (auto& c : m_combs) wet += c.process(inMono);
        wet /= static_cast<float>(kNumCombs);
        for (auto& a : m_allpasses) wet = a.process(wet);

        for (unsigned int ch = 0; ch < channels; ++ch) {
            float dry = buffer[i * channels + ch];
            buffer[i * channels + ch] = dry * (1.0f - mixV) + wet * mixV;
        }
    }
}

} // namespace rsd
