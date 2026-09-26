#include "RhythmClickTrack.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "RhythmMath.h"
#include "Synth.h"

namespace rsd {

namespace {
constexpr float kClickFrequencyHz = 1000.0f;
constexpr float kClickDurationSeconds = 0.03f;
constexpr float kClickGain = 0.5f;
} // namespace

std::shared_ptr<AudioBuffer> renderRhythmClickBuffer(const RhythmPattern& pattern, double bpm,
                                                      unsigned int sampleRate, unsigned int channels) {
    auto buffer = std::make_shared<AudioBuffer>();
    buffer->channels = static_cast<int>(channels);
    buffer->sampleRate = static_cast<int>(sampleRate);

    double totalSeconds = beatsToSeconds(patternTotalBeats(pattern), bpm);
    int64_t tailFrames = static_cast<int64_t>(std::ceil(kClickDurationSeconds * sampleRate));
    int64_t totalFrames = static_cast<int64_t>(std::ceil(totalSeconds * sampleRate)) + tailFrames;
    buffer->samples.assign(static_cast<size_t>(std::max<int64_t>(0, totalFrames)) * channels, 0.0f);

    for (double onsetBeat : onsetBeats(pattern)) {
        double onsetSeconds = beatsToSeconds(onsetBeat, bpm);
        int64_t startFrame = static_cast<int64_t>(std::llround(onsetSeconds * sampleRate));

        for (int64_t i = 0; i < tailFrames; ++i) {
            int64_t frame = startFrame + i;
            if (frame < 0 || frame >= totalFrames) continue;
            float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            if (t >= kClickDurationSeconds) break;
            float env = 1.0f - t / kClickDurationSeconds;
            float sample =
                std::sin(2.0f * static_cast<float>(M_PI) * kClickFrequencyHz * t) * env * kClickGain;
            for (unsigned int ch = 0; ch < channels; ++ch) {
                buffer->samples[static_cast<size_t>(frame) * channels + ch] += sample;
            }
        }
    }

    return buffer;
}

std::shared_ptr<AudioBuffer> renderRhythmPianoBuffer(const RhythmPattern& pattern, double bpm,
                                                      unsigned int sampleRate, unsigned int channels) {
    constexpr int kFixedPitch = 60; // middle C — rhythm reading doesn't depend on pitch
    constexpr float kVelocity = 0.9f;
    constexpr double kReleaseTailSeconds = 0.6;
    constexpr int64_t kBlockFrames = 256;

    auto buffer = std::make_shared<AudioBuffer>();
    buffer->channels = static_cast<int>(channels);
    buffer->sampleRate = static_cast<int>(sampleRate);

    double totalSeconds = beatsToSeconds(patternTotalBeats(pattern), bpm);
    int64_t totalFrames = static_cast<int64_t>(std::ceil((totalSeconds + kReleaseTailSeconds) * sampleRate));
    buffer->samples.assign(static_cast<size_t>(std::max<int64_t>(0, totalFrames)) * channels, 0.0f);

    struct Event {
        int64_t onFrame;
        int64_t offFrame;
    };
    std::vector<Event> events;
    double t = 0.0;
    for (auto& note : pattern.notes) {
        if (!note.isRest) {
            double onSeconds = beatsToSeconds(t, bpm);
            double offSeconds = beatsToSeconds(t + note.beats, bpm);
            events.push_back({static_cast<int64_t>(std::llround(onSeconds * sampleRate)),
                               static_cast<int64_t>(std::llround(offSeconds * sampleRate))});
        }
        t += note.beats;
    }

    SynthEngine engine;
    SynthParams params;
    params.instrumentProgram.store(0); // Acoustic Grand Piano

    std::vector<float> blockOut(static_cast<size_t>(kBlockFrames) * channels, 0.0f);
    for (int64_t pos = 0; pos < totalFrames; pos += kBlockFrames) {
        int64_t nFrames = std::min(kBlockFrames, totalFrames - pos);
        for (auto& ev : events) {
            if (ev.onFrame >= pos && ev.onFrame < pos + nFrames) engine.noteOn(kFixedPitch, kVelocity, static_cast<float>(sampleRate));
            if (ev.offFrame >= pos && ev.offFrame < pos + nFrames) engine.noteOff(kFixedPitch);
        }
        std::fill(blockOut.begin(), blockOut.begin() + nFrames * static_cast<int64_t>(channels), 0.0f);
        engine.render(blockOut.data(), static_cast<unsigned int>(nFrames), channels, params);
        std::copy(blockOut.begin(), blockOut.begin() + nFrames * static_cast<int64_t>(channels),
                  buffer->samples.begin() + pos * channels);
    }

    return buffer;
}

} // namespace rsd
