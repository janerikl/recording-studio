#include "AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

#include "Effects.h"
#include "Mixer.h"

namespace rsd {

AudioEngine::AudioEngine() : m_rtAudio(std::make_unique<RtAudio>()) {}

AudioEngine::~AudioEngine() {
    stop();
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
        if (self->m_transport.punchLoopEnabled() && self->m_transport.punchRegion().isValid()) {
            // Punch/loop mode: capture only within the region, overwriting
            // each pass in place. Position is read before advance() below.
            self->m_punchRecorder.process(self->m_transport.positionSamples(), in, nFrames);
        } else {
            self->m_captureRing.write(in, static_cast<size_t>(nFrames) * self->m_channels);
        }
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

            size_t scratchNeeded = static_cast<size_t>(nFrames) * self->m_channels;
            if (self->m_trackScratch.size() < scratchNeeded) {
                self->m_trackScratch.resize(scratchNeeded, 0.0f);
            }
            float* scratch = self->m_trackScratch.data();

            for (auto& track : self->m_session->tracks) {
                bool soloed = track->soloed.load(std::memory_order_relaxed);
                bool muted = track->muted.load(std::memory_order_relaxed);
                // Solo overrides mute for the soloed track(s); when any track
                // is soloed, every non-soloed track is implicitly silenced.
                bool audible = anySoloed ? soloed : !muted;
                if (!audible) continue;

                std::memset(scratch, 0, sizeof(float) * scratchNeeded);

                auto clips = track->clipsSnapshot();
                for (auto& clip : *clips) {
                    // Clip gain/fades only here; track gain is applied after
                    // the effect chain below (post-fader inserts).
                    mixClipInto(scratch, nFrames, self->m_channels, pos, *clip, 1.0f, 1.0f);
                }

                auto effects = track->effectsSnapshot();
                processEffectChain(*effects, scratch, nFrames, self->m_channels);

                float gainL = track->gainL.load(std::memory_order_relaxed);
                float gainR = track->gainR.load(std::memory_order_relaxed);
                for (unsigned int i = 0; i < nFrames; ++i) {
                    for (unsigned int ch = 0; ch < self->m_channels; ++ch) {
                        float g = (ch % 2 == 0) ? gainL : gainR;
                        out[i * self->m_channels + ch] += scratch[i * self->m_channels + ch] * g;
                    }
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

// Input-only callback: writes captured frames straight to the system-audio
// ring buffer while transport is Recording. No mixing/output/punch support
// here — this stream only feeds whichever tracks are armed with
// AudioSource::SystemAudio (see RecordRouting.h / MainWindow's recording
// flow), kept deliberately simple since it runs concurrently with the mic
// stream's own callback.
int AudioEngine::rtSystemAudioCallback(void* /*outputBuffer*/, void* inputBuffer, unsigned int nFrames,
                                        double /*streamTime*/, RtAudioStreamStatus status,
                                        void* userData) {
    auto* self = static_cast<AudioEngine*>(userData);
    const auto* in = static_cast<const float*>(inputBuffer);

    if (status) {
        std::cerr << "RtAudio system-audio stream over/underflow detected\n";
    }

    if (in && self->m_transport.state() == TransportState::Recording) {
        self->m_systemAudioCaptureRing.write(in, static_cast<size_t>(nFrames) * self->m_channels);
    }

    return 0;
}

bool AudioEngine::startSystemAudioStream() {
    if (m_systemAudioRunning) return true;
    if (m_preferredSystemAudioDevice == kNoInputDevice) return false;

    m_rtAudioSys = std::make_unique<RtAudio>();
    if (m_rtAudioSys->getDeviceCount() < 1) return false;

    RtAudio::StreamParameters inParams;
    inParams.deviceId = m_preferredSystemAudioDevice;
    inParams.nChannels = m_channels;

    RtAudio::DeviceInfo devInfo = m_rtAudioSys->getDeviceInfo(inParams.deviceId);
    if (!devInfo.probed || devInfo.inputChannels == 0) {
        std::cerr << "System audio device is not a valid input device\n";
        return false;
    }

    unsigned int bufferFrames = 512;
    unsigned int sampleRate = m_preferredSampleRate;
    try {
        m_rtAudioSys->openStream(nullptr, &inParams, RTAUDIO_FLOAT32, sampleRate, &bufferFrames,
                                  &AudioEngine::rtSystemAudioCallback, this);
        m_rtAudioSys->startStream();
    } catch (const std::exception& e) {
        std::cerr << "RtAudio system-audio stream error: " << e.what() << "\n";
        return false;
    }

    m_systemAudioRunning = true;
    return true;
}

void AudioEngine::stopSystemAudioStream() {
    if (!m_systemAudioRunning) return;
    try {
        if (m_rtAudioSys->isStreamRunning()) m_rtAudioSys->stopStream();
        if (m_rtAudioSys->isStreamOpen()) m_rtAudioSys->closeStream();
    } catch (const std::exception& e) {
        std::cerr << "RtAudio system-audio stream stop error: " << e.what() << "\n";
    }
    m_systemAudioRunning = false;
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

    if (m_preferredSystemAudioDevice != kNoInputDevice && !startSystemAudioStream()) {
        std::cerr << "System audio device unavailable; that stream will be skipped.\n";
    }

    return true;
}

void AudioEngine::stop() {
    stopSystemAudioStream();

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
