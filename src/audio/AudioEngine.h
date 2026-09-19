#pragma once

#include <RtAudio.h>
#include <memory>

namespace rsd {

// Stage 1: opens a duplex RtAudio stream and copies input straight to
// output (loopback), so we can verify the audio I/O path works before
// any recording/playback/mixing logic is added.
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool start();
    void stop();
    bool isRunning() const { return m_running; }

private:
    static int rtCallback(void* outputBuffer, void* inputBuffer, unsigned int nFrames,
                           double streamTime, RtAudioStreamStatus status, void* userData);

    std::unique_ptr<RtAudio> m_rtAudio;
    unsigned int m_sampleRate = 48000;
    unsigned int m_channels = 2;
    bool m_running = false;
};

} // namespace rsd
