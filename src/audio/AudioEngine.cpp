#include "AudioEngine.h"

#include <cstring>
#include <iostream>

namespace rsd {

AudioEngine::AudioEngine() : m_rtAudio(std::make_unique<RtAudio>()) {}

AudioEngine::~AudioEngine() {
    stop();
}

void AudioEngine::mixClipInto(float* out, unsigned int nFrames, int64_t playheadStart,
                               const Clip& clip, float trackGain) const {
    if (clip.muted || !clip.buffer) return;

    const int64_t clipEnd = clip.sessionStartSample + clip.lengthSamples;
    const int64_t blockEnd = playheadStart + static_cast<int64_t>(nFrames);
    if (clipEnd <= playheadStart || clip.sessionStartSample >= blockEnd) return;

    for (unsigned int i = 0; i < nFrames; ++i) {
        int64_t timelinePos = playheadStart + static_cast<int64_t>(i);
        if (timelinePos < clip.sessionStartSample || timelinePos >= clipEnd) continue;

        int64_t sourceFrame = clip.sourceOffsetSamples + (timelinePos - clip.sessionStartSample);
        if (sourceFrame < 0 || sourceFrame >= clip.buffer->frameCount()) continue;

        for (unsigned int ch = 0; ch < m_channels; ++ch) {
            unsigned int srcCh = ch % static_cast<unsigned int>(clip.buffer->channels);
            float sample = clip.buffer->samples[sourceFrame * clip.buffer->channels + srcCh];
            out[i * m_channels + ch] += sample * trackGain;
        }
    }
}

int AudioEngine::rtCallback(void* outputBuffer, void* inputBuffer, unsigned int nFrames,
                             double /*streamTime*/, RtAudioStreamStatus status, void* userData) {
    auto* self = static_cast<AudioEngine*>(userData);
    auto* out = static_cast<float*>(outputBuffer);
    const auto* in = static_cast<const float*>(inputBuffer);

    if (status) {
        std::cerr << "RtAudio stream over/underflow detected\n";
    }

    std::memset(out, 0, sizeof(float) * nFrames * self->m_channels);

    const TransportState state = self->m_transport.state();

    if (state == TransportState::Recording && in) {
        self->m_captureRing.write(in, static_cast<size_t>(nFrames) * self->m_channels);
    }

    if (state == TransportState::Playing || state == TransportState::Recording) {
        int64_t pos = self->m_transport.positionSamples();
        if (self->m_session) {
            bool anySoloed = false;
            for (auto& track : self->m_session->tracks) {
                if (track->soloed.load(std::memory_order_relaxed)) { anySoloed = true; break; }
            }

            for (auto& track : self->m_session->tracks) {
                bool soloed = track->soloed.load(std::memory_order_relaxed);
                bool muted = track->muted.load(std::memory_order_relaxed);
                // Solo overrides mute for the soloed track(s); when any track
                // is soloed, every non-soloed track is implicitly silenced.
                bool audible = anySoloed ? soloed : !muted;
                if (!audible) continue;

                auto clips = track->clipsSnapshot();
                for (auto& clip : *clips) {
                    self->mixClipInto(out, nFrames, pos, *clip, track->gain);
                }
            }
        }
        self->m_transport.advance(nFrames);
    }

    return 0;
}

bool AudioEngine::start() {
    if (m_running) return true;

    if (m_rtAudio->getDeviceCount() < 1) {
        std::cerr << "No audio devices found\n";
        return false;
    }

    RtAudio::StreamParameters outParams;
    outParams.deviceId = m_rtAudio->getDefaultOutputDevice();
    outParams.nChannels = m_channels;

    RtAudio::StreamParameters inParams;
    inParams.deviceId = m_rtAudio->getDefaultInputDevice();
    inParams.nChannels = m_channels;

    const bool haveInput = inParams.deviceId != 0;
    if (!haveInput) {
        std::cerr << "No input device found; recording will be unavailable, playback only.\n";
    }

    unsigned int bufferFrames = 512;

    try {
        m_rtAudio->openStream(&outParams, haveInput ? &inParams : nullptr, RTAUDIO_FLOAT32,
                               m_sampleRate, &bufferFrames, &AudioEngine::rtCallback, this);
        m_rtAudio->startStream();
    } catch (const std::exception& e) {
        std::cerr << "RtAudio error: " << e.what() << "\n";
        return false;
    }

    m_running = true;
    return true;
}

void AudioEngine::stop() {
    if (!m_running) return;
    try {
        if (m_rtAudio->isStreamRunning()) m_rtAudio->stopStream();
        if (m_rtAudio->isStreamOpen()) m_rtAudio->closeStream();
    } catch (const std::exception& e) {
        std::cerr << "RtAudio stop error: " << e.what() << "\n";
    }
    m_running = false;
}

} // namespace rsd
