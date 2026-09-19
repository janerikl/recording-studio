#include "AudioEngine.h"

#include <cstring>
#include <iostream>

namespace rsd {

AudioEngine::AudioEngine() : m_rtAudio(std::make_unique<RtAudio>()) {}

AudioEngine::~AudioEngine() {
    stop();
}

int AudioEngine::rtCallback(void* outputBuffer, void* inputBuffer, unsigned int nFrames,
                             double /*streamTime*/, RtAudioStreamStatus status, void* /*userData*/) {
    if (status) {
        std::cerr << "RtAudio stream over/underflow detected\n";
    }

    auto* out = static_cast<float*>(outputBuffer);
    const auto* in = static_cast<const float*>(inputBuffer);

    if (in) {
        std::memcpy(out, in, sizeof(float) * nFrames * 2);
    } else {
        std::memset(out, 0, sizeof(float) * nFrames * 2);
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

    unsigned int bufferFrames = 512;

    try {
        m_rtAudio->openStream(&outParams, &inParams, RTAUDIO_FLOAT32, m_sampleRate,
                               &bufferFrames, &AudioEngine::rtCallback, this);
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
