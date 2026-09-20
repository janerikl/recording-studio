#include "AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

#include "AutomationMath.h"
#include "BusMixMath.h"
#include "Effects.h"
#include "Mixer.h"
#include "PanLawMath.h"
#include "PreviewPlaybackMath.h"

namespace rsd {

AudioEngine::AudioEngine() : m_rtAudio(std::make_unique<RtAudio>()) {}

void AudioEngine::previewSample(std::shared_ptr<AudioBuffer> buffer) {
    if (!buffer) return;
    auto clip = std::make_shared<Clip>();
    clip->buffer = std::move(buffer);
    clip->sessionStartSample = 0;
    clip->sourceOffsetSamples = 0;
    clip->lengthSamples = clip->buffer->frameCount();
    m_previewPosition.store(0, std::memory_order_relaxed);
    m_previewClip.store(std::move(clip));
}

void AudioEngine::stopPreview() {
    m_previewClip.store(nullptr);
}

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

    const bool playbackActive = state == TransportState::Playing || state == TransportState::Recording;
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

        if (self->m_masterScratch.size() < scratchNeeded) {
            self->m_masterScratch.resize(scratchNeeded, 0.0f);
        }
        float* masterAccum = self->m_masterScratch.data();
        std::memset(masterAccum, 0, sizeof(float) * scratchNeeded);

        // Zero every bus track's aux accumulation buffer up front so
        // sends below can accumulate into them in any track order,
        // regardless of a bus's position in the track list.
        for (auto& track : self->m_session->tracks) {
            if (track->kind != TrackKind::Bus) continue;
            auto& buf = self->m_busScratch[track->id];
            if (buf.size() < scratchNeeded) buf.resize(scratchNeeded, 0.0f);
            std::fill(buf.begin(), buf.begin() + static_cast<long>(scratchNeeded), 0.0f);
        }

        for (auto& track : self->m_session->tracks) {
            if (track->kind == TrackKind::Bus) continue; // mixed in a second pass below

            // Instrument tracks always drain their live-note queue and
            // render, even while stopped, so clicking the on-screen
            // keyboard is audible without needing to hit Play first. Audio
            // tracks have nothing to do outside actual playback/recording.
            bool isInstrument = track->kind == TrackKind::Instrument;
            if (!playbackActive && !isInstrument) continue;

            bool soloed = track->soloed.load(std::memory_order_relaxed);
            bool muted = track->muted.load(std::memory_order_relaxed);
            // Solo overrides mute for the soloed track(s); when any track
            // is soloed, every non-soloed track is implicitly silenced.
            bool audible = anySoloed ? soloed : !muted;
            if (!audible) continue;

            std::memset(scratch, 0, sizeof(float) * scratchNeeded);

            if (isInstrument) {
                NoteEvent ev;
                while (track->liveNoteEvents.pop(ev)) {
                    if (ev.noteOn) {
                        track->synthEngine.noteOn(ev.pitch, ev.velocity,
                                                   static_cast<float>(self->m_sampleRate));
                    } else {
                        track->synthEngine.noteOff(ev.pitch);
                    }
                }
                if (playbackActive) {
                    // Block-level timing granularity (not sample-accurate):
                    // a note triggers/releases wherever its start/end lands
                    // within the current callback block.
                    auto notes = track->midiClipsSnapshot();
                    int64_t blockEnd = pos + static_cast<int64_t>(nFrames);
                    for (auto& note : *notes) {
                        int64_t noteEnd = note->startSample + note->lengthSamples;
                        if (note->startSample >= pos && note->startSample < blockEnd) {
                            track->synthEngine.noteOn(note->pitch, note->velocity,
                                                       static_cast<float>(self->m_sampleRate));
                        }
                        if (noteEnd >= pos && noteEnd < blockEnd) {
                            track->synthEngine.noteOff(note->pitch);
                        }
                    }
                }
                track->synthEngine.render(scratch, nFrames, self->m_channels, track->synthParams);
            } else if (playbackActive) {
                auto clips = track->clipsSnapshot();
                for (auto& clip : *clips) {
                    // Clip gain/fades only here; track gain is applied after
                    // the effect chain below (post-fader inserts).
                    mixClipInto(scratch, nFrames, self->m_channels, pos, *clip, 1.0f, 1.0f);
                }
            }

            auto effects = track->effectsSnapshot();
            processEffectChain(*effects, scratch, nFrames, self->m_channels);

            // Volume/pan for this block: an automation curve (if present for
            // that target) is evaluated at the block's start and end sample
            // and linearly ramped per-sample across the block, so fast
            // automation moves don't produce zipper noise. A target with no
            // lane falls back to the track's static atomic for both ends
            // (i.e. no ramp — same as before automation existed).
            float staticVolume = track->volume.load(std::memory_order_relaxed);
            float staticPan = track->pan.load(std::memory_order_relaxed);
            auto lanes = track->automationLanesSnapshot();
            float volumeStart = staticVolume, volumeEnd = staticVolume;
            float panStart = staticPan, panEnd = staticPan;
            for (auto& lane : *lanes) {
                if (lane->points.empty()) continue;
                if (lane->target == AutomationTarget::Volume) {
                    volumeStart = evaluateAutomation(lane->points, pos, staticVolume);
                    volumeEnd = evaluateAutomation(lane->points, pos + nFrames, staticVolume);
                } else if (lane->target == AutomationTarget::Pan) {
                    panStart = evaluateAutomation(lane->points, pos, staticPan);
                    panEnd = evaluateAutomation(lane->points, pos + nFrames, staticPan);
                }
            }
            auto [gainLStart, gainRStart] = panToGains(volumeStart, panStart);
            auto [gainLEnd, gainREnd] = panToGains(volumeEnd, panEnd);

            // Aux send: post-fader tap into a bus track's aux buffer, in
            // addition to this track's own contribution to the master mix.
            QUuid destBusId = track->sendBusId();
            float sendLevel = track->sendLevel.load(std::memory_order_relaxed);
            float* sendBuf = nullptr;
            if (!destBusId.isNull() && sendLevel > 0.0f) {
                auto it = self->m_busScratch.find(destBusId);
                if (it != self->m_busScratch.end()) sendBuf = it->second.data();
            }

            for (unsigned int i = 0; i < nFrames; ++i) {
                float t = nFrames > 1 ? static_cast<float>(i) / static_cast<float>(nFrames - 1) : 0.0f;
                float gainL = gainLStart + t * (gainLEnd - gainLStart);
                float gainR = gainRStart + t * (gainREnd - gainRStart);
                for (unsigned int ch = 0; ch < self->m_channels; ++ch) {
                    float g = (ch % 2 == 0) ? gainL : gainR;
                    float v = scratch[i * self->m_channels + ch] * g;
                    masterAccum[i * self->m_channels + ch] += v;
                    if (sendBuf) sendBuf[i * self->m_channels + ch] += applySend(v, sendLevel);
                }
            }
        }

        // Second pass: mix each bus track's accumulated aux buffer (sends
        // from the first pass) through its own effects chain and
        // volume/pan, into the master accumulation buffer. Buses never
        // send to other buses, so processing order between buses doesn't
        // matter here.
        for (auto& track : self->m_session->tracks) {
            if (track->kind != TrackKind::Bus) continue;

            bool soloed = track->soloed.load(std::memory_order_relaxed);
            bool muted = track->muted.load(std::memory_order_relaxed);
            bool audible = anySoloed ? soloed : !muted;
            if (!audible) continue;

            float* busBuf = self->m_busScratch[track->id].data();

            auto effects = track->effectsSnapshot();
            processEffectChain(*effects, busBuf, nFrames, self->m_channels);

            float volume = track->volume.load(std::memory_order_relaxed);
            float pan = track->pan.load(std::memory_order_relaxed);
            auto [gainL, gainR] = panToGains(volume, pan);

            for (unsigned int i = 0; i < nFrames; ++i) {
                for (unsigned int ch = 0; ch < self->m_channels; ++ch) {
                    float g = (ch % 2 == 0) ? gainL : gainR;
                    masterAccum[i * self->m_channels + ch] += busBuf[i * self->m_channels + ch] * g;
                }
            }
        }

        // Loop browser audition: mixed straight into the master buffer,
        // independent of transport state/session tracks, one throwaway
        // Clip at a time. Cleared once it plays past its own length.
        auto previewClip = self->m_previewClip.load();
        if (previewClip) {
            int64_t previewPos = self->m_previewPosition.load(std::memory_order_relaxed);
            mixClipInto(masterAccum, nFrames, self->m_channels, previewPos, *previewClip, 1.0f, 1.0f);
            previewPos += nFrames;
            if (isPreviewFinished(previewPos, previewClip->lengthSamples)) {
                self->m_previewClip.store(nullptr);
            } else {
                self->m_previewPosition.store(previewPos, std::memory_order_relaxed);
            }
        }

        // Master bus: final effects chain + volume, then write to output.
        auto masterEffects = self->m_session->masterBus.effectsSnapshot();
        processEffectChain(*masterEffects, masterAccum, nFrames, self->m_channels);
        float masterVolume = self->m_session->masterBus.volume.load(std::memory_order_relaxed);
        for (size_t i = 0; i < scratchNeeded; ++i) {
            out[i] = applyMasterVolume(masterAccum[i], masterVolume);
        }
    }

    if (playbackActive) self->m_transport.advance(nFrames);

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
