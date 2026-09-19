#include "AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

namespace rsd {

AudioEngine::AudioEngine() : m_rtAudio(std::make_unique<RtAudio>()) {}

AudioEngine::~AudioEngine() {
    stop();
}

void AudioEngine::mixClipInto(float* out, unsigned int nFrames, int64_t playheadStart,
                               const Clip& clip, float gainL, float gainR) const {
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
            float g = (ch % 2 == 0) ? gainL : gainR;
            out[i * m_channels + ch] += sample * g;
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

    // Measured regardless of transport state so a level meter can show
    // input signal before the user even hits Record.
    if (in) {
        float peakL = 0.0f, peakR = 0.0f;
        for (unsigned int i = 0; i < nFrames; ++i) {
            peakL = std::max(peakL, std::abs(in[i * self->m_channels]));
            unsigned int rCh = self->m_channels > 1 ? 1u : 0u;
            peakR = std::max(peakR, std::abs(in[i * self->m_channels + rCh]));
        }
        self->m_inputPeakL.store(peakL, std::memory_order_relaxed);
        self->m_inputPeakR.store(peakR, std::memory_order_relaxed);
    } else {
        self->m_inputPeakL.store(0.0f, std::memory_order_relaxed);
        self->m_inputPeakR.store(0.0f, std::memory_order_relaxed);
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
                    self->mixClipInto(out, nFrames, pos, *clip,
                                       track->gainL.load(std::memory_order_relaxed),
                                       track->gainR.load(std::memory_order_relaxed));
                }
            }
        }
        self->m_transport.advance(nFrames);
    }

    float outPeakL = 0.0f, outPeakR = 0.0f;
    for (unsigned int i = 0; i < nFrames; ++i) {
        outPeakL = std::max(outPeakL, std::abs(out[i * self->m_channels]));
        unsigned int rCh = self->m_channels > 1 ? 1u : 0u;
        outPeakR = std::max(outPeakR, std::abs(out[i * self->m_channels + rCh]));
    }
    self->m_outputPeakL.store(outPeakL, std::memory_order_relaxed);
    self->m_outputPeakR.store(outPeakR, std::memory_order_relaxed);

    return 0;
}

bool AudioEngine::start() {
    if (m_running) return true;

    if (m_rtAudio->getDeviceCount() < 1) {
        std::cerr << "No audio devices found\n";
        return false;
    }

    RtAudio::StreamParameters outParams;
    outParams.deviceId = m_preferredOutputDevice != kUseSystemDefault
                              ? m_preferredOutputDevice
                              : m_rtAudio->getDefaultOutputDevice();
    outParams.nChannels = m_channels;

    RtAudio::StreamParameters inParams;
    bool haveInput = m_preferredInputDevice != kNoInputDevice;
    if (haveInput) {
        inParams.deviceId = m_preferredInputDevice != kUseSystemDefault
                                 ? m_preferredInputDevice
                                 : m_rtAudio->getDefaultInputDevice();
        inParams.nChannels = m_channels;
        // Confirm the resolved device actually supports input — a bare
        // device index isn't enough evidence (index 0 is a real device in
        // this RtAudio version, and might be output-only).
        RtAudio::DeviceInfo devInfo = m_rtAudio->getDeviceInfo(inParams.deviceId);
        if (!devInfo.probed || devInfo.inputChannels == 0) haveInput = false;
    }
    if (!haveInput) {
        std::cerr << "No input device found; recording will be unavailable, playback only.\n";
    }

    m_sampleRate = m_preferredSampleRate;
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

bool AudioEngine::restart() {
    stop();
    return start();
}

std::vector<DeviceOption> AudioEngine::listDevices() const {
    std::vector<DeviceOption> result;
    unsigned int count = m_rtAudio->getDeviceCount();
    for (unsigned int id = 0; id < count; ++id) {
        RtAudio::DeviceInfo info = m_rtAudio->getDeviceInfo(id);
        if (!info.probed || info.name.empty()) continue;

        DeviceOption opt;
        opt.id = id;
        opt.name = QString::fromStdString(info.name);
        opt.maxOutputChannels = info.outputChannels;
        opt.maxInputChannels = info.inputChannels;
        for (unsigned int sr : info.sampleRates) opt.sampleRates.push_back(sr);
        result.push_back(std::move(opt));
    }
    return result;
}

} // namespace rsd
