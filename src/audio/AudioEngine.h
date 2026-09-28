#pragma once

#include <RtAudio.h>
#include <QString>
#include <QUuid>
#include <atomic>
#include <map>
#include <memory>
#include <vector>

#include "Metronome.h"
#include "PunchRecorder.h"
#include "RingBuffer.h"
#include "SessionMixer.h"
#include "TransportClock.h"
#include "model/Clip.h"
#include "model/Session.h"

namespace rsd {

struct DeviceOption {
    unsigned int id = 0;
    QString name;
    unsigned int maxOutputChannels = 0;
    unsigned int maxInputChannels = 0;
    std::vector<unsigned int> sampleRates;
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    std::vector<DeviceOption> listDevices() const;

    // kUseSystemDefault = use RtAudio's default device for that role (device
    // index 0 is a real, valid device in this RtAudio version, so it can't
    // double as an "unset" sentinel). Takes effect on the next
    // start()/restart() — the stream isn't reopened automatically.
    static constexpr unsigned int kUseSystemDefault = static_cast<unsigned int>(-1);
    static constexpr unsigned int kNoInputDevice = static_cast<unsigned int>(-2);
    void setPreferredOutputDevice(unsigned int id) { m_preferredOutputDevice = id; }
    void setPreferredInputDevice(unsigned int id) { m_preferredInputDevice = id; }
    void setPreferredSampleRate(unsigned int sr) { m_preferredSampleRate = sr; }
    unsigned int preferredOutputDevice() const { return m_preferredOutputDevice; }
    unsigned int preferredInputDevice() const { return m_preferredInputDevice; }

    // Second, independent input device (e.g. a PulseAudio ".monitor" source)
    // captured via its own RtAudio stream/callback so it can record
    // concurrently with the mic stream, onto its own ring buffer. Defaults
    // to kNoInputDevice (disabled) since most sessions won't use it.
    void setPreferredSystemAudioDevice(unsigned int id) { m_preferredSystemAudioDevice = id; }
    unsigned int preferredSystemAudioDevice() const { return m_preferredSystemAudioDevice; }

    bool restart(); // stop() then start() with current preferred settings

    void setSession(Session* session) { m_session = session; }

    TransportClock& transport() { return m_transport; }
    RingBuffer<float>& captureRing() { return m_captureRing; }
    RingBuffer<float>& systemAudioCaptureRing() { return m_systemAudioCaptureRing; }
    bool systemAudioRunning() const { return m_systemAudioRunning; }

    // Track that recorded input should be routed to while state == Recording.
    void setRecordTargetTrack(std::shared_ptr<Track> track) { m_recordTarget = std::move(track); }
    std::shared_ptr<Track> recordTargetTrack() const { return m_recordTarget; }

    // Punch/loop recording: call prepare() (GUI thread, before arming) once
    // the punch region and transport's loop settings are set. While
    // transport().punchLoopEnabled() and the region is valid, the RT
    // callback captures into the recorder instead of the plain capture ring,
    // so passes replace each other rather than layering. Call
    // finalizePunchRecording() after stopping to get the undoable result.
    PunchRecorder& punchRecorder() { return m_punchRecorder; }

    // Loop browser click-to-audition: plays a buffer once, mixed straight
    // into the master output, independent of transport state/session
    // tracks. Starting a new preview replaces any currently playing one
    // (single active preview). GUI thread only.
    void previewSample(std::shared_ptr<AudioBuffer> buffer);
    void stopPreview();

    unsigned int sampleRate() const { return m_sampleRate; }
    unsigned int channels() const { return m_channels; }

    // Per-channel peak (0..1) of the most recent audio callback buffer.
    // Channel 1 mirrors channel 0 for mono streams. Input is measured
    // whenever a mic is present (even when stopped, so users can see signal
    // before hitting Record); output reflects the current mix.
    float inputPeakL() const { return m_inputPeakL.load(std::memory_order_relaxed); }
    float inputPeakR() const { return m_inputPeakR.load(std::memory_order_relaxed); }
    float outputPeakL() const { return m_outputPeakL.load(std::memory_order_relaxed); }
    float outputPeakR() const { return m_outputPeakR.load(std::memory_order_relaxed); }

    // Zeroes the output meter so it doesn't keep showing the last mixed
    // block's level forever once the callback stops firing on Stop. Input
    // is left alone — it's meant to keep reflecting live mic signal even
    // while stopped (see the comment above).
    void resetOutputMeter() {
        m_outputPeakL.store(0.0f, std::memory_order_relaxed);
        m_outputPeakR.store(0.0f, std::memory_order_relaxed);
    }

private:
    // Recording (mic capture) and playback (session mix, metronome, preview
    // audition) run on two independent, always-open, single-direction
    // streams rather than one combined duplex stream — the input and output
    // devices may be different physical hardware, which RtAudio/ALSA can't
    // duplex-synchronize (this used to be one openStream(&outParams,
    // &inParams, ...) call and would intermittently fail/stall for exactly
    // that reason). See StreamRoleMath.h for the state predicates each one
    // uses to decide whether it's "active" this block.
    static int rtOutputCallback(void* outputBuffer, void* inputBuffer, unsigned int nFrames,
                                 double streamTime, RtAudioStreamStatus status, void* userData);
    static int rtInputCallback(void* outputBuffer, void* inputBuffer, unsigned int nFrames,
                                double streamTime, RtAudioStreamStatus status, void* userData);
    static int rtSystemAudioCallback(void* outputBuffer, void* inputBuffer, unsigned int nFrames,
                                      double streamTime, RtAudioStreamStatus status, void* userData);

    bool startInputStream();
    void stopInputStream();
    bool startSystemAudioStream();
    void stopSystemAudioStream();

    std::unique_ptr<RtAudio> m_rtAudio; // output-only
    unsigned int m_sampleRate = 48000;
    unsigned int m_channels = 2;
    bool m_running = false;

    // Independent input-only stream for mic capture (see rtInputCallback).
    std::unique_ptr<RtAudio> m_rtAudioIn;
    bool m_inputRunning = false;

    unsigned int m_preferredOutputDevice = kUseSystemDefault;
    unsigned int m_preferredInputDevice = kUseSystemDefault;
    unsigned int m_preferredSampleRate = 48000;

    // Second RtAudio instance: an input-only stream for the system-audio
    // (loopback) device, entirely separate from m_rtAudio so it can run
    // concurrently with the mic stream without sharing a callback.
    std::unique_ptr<RtAudio> m_rtAudioSys;
    unsigned int m_preferredSystemAudioDevice = kNoInputDevice;
    bool m_systemAudioRunning = false;
    RingBuffer<float> m_systemAudioCaptureRing{48000 * 2 * 10};

    Session* m_session = nullptr;
    TransportClock m_transport;
    RingBuffer<float> m_captureRing{48000 * 2 * 10}; // 10s headroom at 48kHz stereo
    PunchRecorder m_punchRecorder;
    std::shared_ptr<Track> m_recordTarget;
    // Mixdown scratch space (per-track/master/per-bus buffers), reused
    // every callback so mixSessionBlock() never allocates on the audio
    // thread. Sized generously above any realistic (buffer frames *
    // channels) the stream will open with (512-frame stereo in practice).
    SessionMixScratch m_mixScratch{std::vector<float>(8192 * 2, 0.0f),
                                    std::vector<float>(8192 * 2, 0.0f), {}};

    // Loop browser audition: an unattached, throwaway Clip (never on any
    // track) wrapping the clicked buffer, played from m_previewPosition
    // and mixed in every callback regardless of transport state. nullptr
    // means no preview active.
    std::atomic<std::shared_ptr<const Clip>> m_previewClip{nullptr};
    std::atomic<int64_t> m_previewPosition{0};

    // Click track, mixed straight onto the final output (post
    // mixSessionBlock/preview, same spot as the loop-browser audition)
    // during playback/recording — RT-thread-only, same trust model as a
    // Track's SynthEngine. Deliberately not part of mixSessionBlock() so
    // OfflineRenderer/export never bakes it into a bounce.
    Metronome m_metronome;

    std::atomic<float> m_inputPeakL{0.0f};
    std::atomic<float> m_inputPeakR{0.0f};
    std::atomic<float> m_outputPeakL{0.0f};
    std::atomic<float> m_outputPeakR{0.0f};
};

} // namespace rsd
