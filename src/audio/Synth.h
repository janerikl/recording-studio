#pragma once

#include <algorithm>
#include <atomic>
#include <cstring>
#include <iterator>
#include <vector>

#include <fluidsynth.h>

namespace rsd {

// Live-tweakable instrument selection for an Instrument track. Melodic
// tracks play GM program `instrumentProgram` (0-127, see GMInstruments.h)
// on MIDI channel 0; drum-kit tracks play the GM percussion kit on channel
// 9 (see GMDrumMap.h) and ignore `instrumentProgram`.
struct SynthParams {
    std::atomic<int> instrumentProgram{0}; // Acoustic Grand Piano
    std::atomic<bool> isDrumKit{false};
};

// Wraps a FluidSynth instance rendering through the bundled GM SoundFont.
// Only the RT thread ever touches this (Track exposes it as a plain,
// non-atomic member, same trust model as the previous oscillator-based
// engine and PunchRecorder). FluidSynth's noteon/noteoff/process calls are
// designed to be driven directly from a real-time audio callback.
class SynthEngine {
public:
    static constexpr int kMelodicChannel = 0;
    static constexpr int kDrumChannel = 9;

    SynthEngine() {
        std::fill(std::begin(m_lastCc), std::end(m_lastCc), -1);
        m_settings = new_fluid_settings();
        fluid_settings_setnum(m_settings, "synth.sample-rate", 48000.0);
        fluid_settings_setint(m_settings, "synth.threadsafe-api", 0);
        fluid_settings_setint(m_settings, "synth.reverb.active", 1);
        fluid_settings_setint(m_settings, "synth.chorus.active", 1);
        m_synth = new_fluid_synth(m_settings);
        // Prefer the full-quality FluidR3_GM soundfont (apt: fluid-soundfont-gm)
        // when present on the system — its multi-sampled instruments sound far
        // more authentic than the bundled TimGM6mb.sf2, which is a deliberately
        // tiny (6MB) placeholder. Fall back to the bundled one so the app still
        // runs on a machine without that package installed.
        if (fluid_synth_sfload(m_synth, "/usr/share/sounds/sf2/FluidR3_GM.sf2", 1) == -1) {
            fluid_synth_sfload(m_synth, RSD_SOURCE_DIR "/assets/soundfonts/TimGM6mb.sf2", 1);
        }
        fluid_synth_bank_select(m_synth, kDrumChannel, 128);
        fluid_synth_program_change(m_synth, kDrumChannel, 0);
    }

    ~SynthEngine() {
        if (m_synth) delete_fluid_synth(m_synth);
        if (m_settings) delete_fluid_settings(m_settings);
    }

    SynthEngine(const SynthEngine&) = delete;
    SynthEngine& operator=(const SynthEngine&) = delete;

    void noteOn(int pitch, float velocity, float /*sampleRate*/, bool isDrumKit = false) {
        int channel = isDrumKit ? kDrumChannel : kMelodicChannel;
        int vel = std::clamp(static_cast<int>(velocity * 127.0f), 1, 127);
        fluid_synth_noteon(m_synth, channel, std::clamp(pitch, 0, 127), vel);
    }

    void noteOff(int pitch, bool isDrumKit = false) {
        int channel = isDrumKit ? kDrumChannel : kMelodicChannel;
        fluid_synth_noteoff(m_synth, channel, std::clamp(pitch, 0, 127));
    }

    // Sends MIDI control change `cc` (0-127) with `value` (clamped 0-127)
    // on the melodic channel. Skips the FluidSynth call when the value
    // equals the last one sent for that controller, so callers can invoke
    // it every block/segment cheaply. Used for CC11 (expression) and CC1
    // (modulation -> vibrato depth via SF2 default modulators).
    void controlChange(int cc, int value) {
        if (cc < 0 || cc > 127) return;
        value = std::clamp(value, 0, 127);
        if (m_lastCc[cc] == value) return;
        m_lastCc[cc] = value;
        fluid_synth_cc(m_synth, kMelodicChannel, cc, value);
    }

    // Adds nFrames of rendered audio into `out` (interleaved, `channels`
    // channels). Applies any pending program change for the melodic
    // channel first.
    void render(float* out, unsigned int nFrames, unsigned int channels, const SynthParams& params) {
        int program = params.instrumentProgram.load(std::memory_order_relaxed);
        if (program != m_lastProgram) {
            fluid_synth_program_change(m_synth, kMelodicChannel, program);
            m_lastProgram = program;
        }

        if (m_scratchL.size() < nFrames) {
            m_scratchL.resize(nFrames);
            m_scratchR.resize(nFrames);
        }
        std::fill(m_scratchL.begin(), m_scratchL.begin() + nFrames, 0.0f);
        std::fill(m_scratchR.begin(), m_scratchR.begin() + nFrames, 0.0f);
        fluid_synth_write_float(m_synth, static_cast<int>(nFrames), m_scratchL.data(), 0, 1, m_scratchR.data(), 0, 1);

        for (unsigned int i = 0; i < nFrames; ++i) {
            float l = m_scratchL[i];
            float r = m_scratchR[i];
            if (channels == 1) {
                out[i] += 0.5f * (l + r);
            } else {
                out[i * channels + 0] += l;
                out[i * channels + 1] += r;
                for (unsigned int ch = 2; ch < channels; ++ch) out[i * channels + ch] += l;
            }
        }
    }

    // Silences every voice immediately (no release tail). Used before an
    // offline render starts, so export begins from deterministic silence
    // regardless of any in-progress live audition.
    void reset() {
        if (!m_synth) return;
        // all_sounds_off mutes voices immediately; system_reset only
        // triggers a normal note-off (audible release tail), which isn't
        // deterministic enough for "export starts from silence".
        fluid_synth_all_sounds_off(m_synth, kMelodicChannel);
        fluid_synth_all_sounds_off(m_synth, kDrumChannel);
    }

private:
    fluid_settings_t* m_settings = nullptr;
    fluid_synth_t* m_synth = nullptr;
    int m_lastProgram = -1;
    int m_lastCc[128]; // last value sent per controller; -1 = never sent
    std::vector<float> m_scratchL;
    std::vector<float> m_scratchR;
};

} // namespace rsd
