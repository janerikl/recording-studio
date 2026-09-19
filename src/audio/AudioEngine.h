#pragma once

#include <RtAudio.h>
#include <atomic>
#include <memory>

#include "RingBuffer.h"
#include "TransportClock.h"
#include "model/Session.h"

namespace rsd {

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    void setSession(Session* session) { m_session = session; }

    TransportClock& transport() { return m_transport; }
    RingBuffer<float>& captureRing() { return m_captureRing; }

    // Track that recorded input should be routed to while state == Recording.
    void setRecordTargetTrack(std::shared_ptr<Track> track) { m_recordTarget = std::move(track); }

    unsigned int sampleRate() const { return m_sampleRate; }
    unsigned int channels() const { return m_channels; }

    // Peak (0..1) of the most recent audio callback buffer. Input is
    // measured whenever a mic is present (even when stopped, so users can
    // see signal before hitting Record); output reflects the current mix.
    float inputPeak() const { return m_inputPeak.load(std::memory_order_relaxed); }
    float outputPeak() const { return m_outputPeak.load(std::memory_order_relaxed); }

private:
    static int rtCallback(void* outputBuffer, void* inputBuffer, unsigned int nFrames,
                           double streamTime, RtAudioStreamStatus status, void* userData);

    void mixClipInto(float* out, unsigned int nFrames, int64_t playheadStart, const Clip& clip,
                      float trackGain) const;

    std::unique_ptr<RtAudio> m_rtAudio;
    unsigned int m_sampleRate = 48000;
    unsigned int m_channels = 2;
    bool m_running = false;

    Session* m_session = nullptr;
    TransportClock m_transport;
    RingBuffer<float> m_captureRing{48000 * 2 * 10}; // 10s headroom at 48kHz stereo
    std::shared_ptr<Track> m_recordTarget;

    std::atomic<float> m_inputPeak{0.0f};
    std::atomic<float> m_outputPeak{0.0f};
};

} // namespace rsd
