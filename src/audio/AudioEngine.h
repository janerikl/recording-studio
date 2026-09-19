#pragma once

#include <RtAudio.h>
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
};

} // namespace rsd
