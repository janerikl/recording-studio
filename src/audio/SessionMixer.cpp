#include "SessionMixer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "AutomationMath.h"
#include "BusMixMath.h"
#include "Effects.h"
#include "Mixer.h"
#include "PanLawMath.h"

namespace rsd {

void renderTrackBlock(Track& track, unsigned int sampleRate, unsigned int channels, int64_t pos,
                      unsigned int nFrames, bool playbackActive, float* out) {
    size_t needed = static_cast<size_t>(nFrames) * channels;
    std::memset(out, 0, sizeof(float) * needed);

    bool isInstrument = track.kind == TrackKind::Instrument;

    if (isInstrument) {
        bool isDrumKit = track.synthParams.isDrumKit.load(std::memory_order_relaxed);
        NoteEvent ev;
        while (track.liveNoteEvents.pop(ev)) {
            if (ev.noteOn) {
                track.synthEngine.noteOn(ev.pitch, ev.velocity, static_cast<float>(sampleRate), isDrumKit);
            } else {
                track.synthEngine.noteOff(ev.pitch, isDrumKit);
            }
        }
        if (playbackActive) {
            // Block-level timing granularity (not sample-accurate): a note
            // triggers/releases wherever its start/end lands in this block.
            auto notes = track.midiClipsSnapshot();
            int64_t blockEnd = pos + static_cast<int64_t>(nFrames);
            for (auto& note : *notes) {
                int64_t noteEnd = note->startSample + note->lengthSamples;
                if (note->startSample >= pos && note->startSample < blockEnd) {
                    track.synthEngine.noteOn(note->pitch, note->velocity,
                                              static_cast<float>(sampleRate), isDrumKit);
                }
                if (noteEnd >= pos && noteEnd < blockEnd) {
                    track.synthEngine.noteOff(note->pitch, isDrumKit);
                }
            }
        }
        track.synthEngine.render(out, nFrames, channels, track.synthParams);
    } else if (playbackActive) {
        auto clips = track.clipsSnapshot();
        for (auto& clip : *clips) {
            // Clip gain/fades only here; track gain is applied after the
            // effect chain below (post-fader inserts).
            mixClipInto(out, nFrames, channels, pos, *clip, 1.0f, 1.0f);
        }
    }

    auto effects = track.effectsSnapshot();
    processEffectChain(*effects, out, nFrames, channels);

    // Volume/pan for this block: an automation curve (if present for that
    // target) is evaluated at the block's start and end sample and linearly
    // ramped per-sample across the block, so fast automation moves don't
    // produce zipper noise. A target with no lane falls back to the
    // track's static atomic for both ends (i.e. no ramp).
    float staticVolume = track.volume.load(std::memory_order_relaxed);
    float staticPan = track.pan.load(std::memory_order_relaxed);
    auto lanes = track.automationLanesSnapshot();
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

    for (unsigned int i = 0; i < nFrames; ++i) {
        float t = nFrames > 1 ? static_cast<float>(i) / static_cast<float>(nFrames - 1) : 0.0f;
        float gainL = gainLStart + t * (gainLEnd - gainLStart);
        float gainR = gainRStart + t * (gainREnd - gainRStart);
        for (unsigned int ch = 0; ch < channels; ++ch) {
            float g = (ch % 2 == 0) ? gainL : gainR;
            out[i * channels + ch] *= g;
        }
    }
}

void mixSessionBlock(Session& session, unsigned int sampleRate, unsigned int channels, int64_t pos,
                     unsigned int nFrames, bool playbackActive, float* out, SessionMixScratch& scratch,
                     bool isRecording) {
    size_t needed = static_cast<size_t>(nFrames) * channels;

    bool anySoloed = false;
    for (auto& track : session.tracks) {
        if (track->soloed.load(std::memory_order_relaxed)) { anySoloed = true; break; }
    }

    if (scratch.trackScratch.size() < needed) scratch.trackScratch.resize(needed, 0.0f);
    float* trackBuf = scratch.trackScratch.data();

    if (scratch.masterScratch.size() < needed) scratch.masterScratch.resize(needed, 0.0f);
    float* masterAccum = scratch.masterScratch.data();
    std::memset(masterAccum, 0, sizeof(float) * needed);

    // Zero every bus track's aux accumulation buffer up front so sends
    // below can accumulate into them in any track order, regardless of a
    // bus's position in the track list.
    for (auto& track : session.tracks) {
        if (track->kind != TrackKind::Bus) continue;
        auto& buf = scratch.busScratch[track->id];
        if (buf.size() < needed) buf.resize(needed, 0.0f);
        std::fill(buf.begin(), buf.begin() + static_cast<long>(needed), 0.0f);
    }

    for (auto& track : session.tracks) {
        if (track->kind == TrackKind::Bus) continue; // mixed in a second pass below

        bool isInstrument = track->kind == TrackKind::Instrument;
        if (!playbackActive && !isInstrument) continue;

        bool soloed = track->soloed.load(std::memory_order_relaxed);
        bool muted = track->muted.load(std::memory_order_relaxed);
        bool audible = anySoloed ? soloed : !muted;
        if (!audible) {
            track->postFaderPeakL.store(0.0f, std::memory_order_relaxed);
            track->postFaderPeakR.store(0.0f, std::memory_order_relaxed);
            continue;
        }

        // While recording, only the armed track(s) should reach output —
        // see mixSessionBlock's isRecording doc comment for why.
        if (isRecording && !track->recordArmed.load(std::memory_order_relaxed)) {
            track->postFaderPeakL.store(0.0f, std::memory_order_relaxed);
            track->postFaderPeakR.store(0.0f, std::memory_order_relaxed);
            continue;
        }

        renderTrackBlock(*track, sampleRate, channels, pos, nFrames, playbackActive, trackBuf);

        // Post-fader peak for the UI's per-track meter: trackBuf already has
        // this track's volume/pan applied by renderTrackBlock above.
        {
            float peakL = 0.0f, peakR = 0.0f;
            for (size_t i = 0; i < needed; ++i) {
                float mag = std::fabs(trackBuf[i]);
                if (i % channels == 0) peakL = std::max(peakL, mag);
                else peakR = std::max(peakR, mag);
            }
            track->postFaderPeakL.store(peakL, std::memory_order_relaxed);
            track->postFaderPeakR.store(peakR, std::memory_order_relaxed);
        }

        // Aux send: post-fader tap into a bus track's aux buffer, in
        // addition to this track's own contribution to the master mix.
        QUuid destBusId = track->sendBusId();
        float sendLevel = track->sendLevel.load(std::memory_order_relaxed);
        float* sendBuf = nullptr;
        if (!destBusId.isNull() && sendLevel > 0.0f) {
            auto it = scratch.busScratch.find(destBusId);
            if (it != scratch.busScratch.end()) sendBuf = it->second.data();
        }

        for (size_t i = 0; i < needed; ++i) {
            masterAccum[i] += trackBuf[i];
            if (sendBuf) sendBuf[i] += applySend(trackBuf[i], sendLevel);
        }
    }

    // Second pass: mix each bus track's accumulated aux buffer (sends from
    // the first pass) through its own effects chain and volume/pan, into
    // the master accumulation buffer. Buses never send to other buses, so
    // processing order between buses doesn't matter here.
    for (auto& track : session.tracks) {
        if (track->kind != TrackKind::Bus) continue;

        bool soloed = track->soloed.load(std::memory_order_relaxed);
        bool muted = track->muted.load(std::memory_order_relaxed);
        bool audible = anySoloed ? soloed : !muted;
        if (!audible) {
            track->postFaderPeakL.store(0.0f, std::memory_order_relaxed);
            track->postFaderPeakR.store(0.0f, std::memory_order_relaxed);
            continue;
        }

        float* busBuf = scratch.busScratch[track->id].data();

        auto effects = track->effectsSnapshot();
        processEffectChain(*effects, busBuf, nFrames, channels);

        float volume = track->volume.load(std::memory_order_relaxed);
        float pan = track->pan.load(std::memory_order_relaxed);
        auto [gainL, gainR] = panToGains(volume, pan);

        float peakL = 0.0f, peakR = 0.0f;
        for (unsigned int i = 0; i < nFrames; ++i) {
            for (unsigned int ch = 0; ch < channels; ++ch) {
                float g = (ch % 2 == 0) ? gainL : gainR;
                float sample = busBuf[i * channels + ch] * g;
                masterAccum[i * channels + ch] += sample;
                if (ch % 2 == 0) peakL = std::max(peakL, std::fabs(sample));
                else peakR = std::max(peakR, std::fabs(sample));
            }
        }
        track->postFaderPeakL.store(peakL, std::memory_order_relaxed);
        track->postFaderPeakR.store(peakR, std::memory_order_relaxed);
    }

    // Master bus: final effects chain + volume, then write to output.
    auto masterEffects = session.masterBus.effectsSnapshot();
    processEffectChain(*masterEffects, masterAccum, nFrames, channels);
    float masterVolume = session.masterBus.volume.load(std::memory_order_relaxed);
    for (size_t i = 0; i < needed; ++i) {
        out[i] = applyMasterVolume(masterAccum[i], masterVolume);
    }
}

} // namespace rsd
