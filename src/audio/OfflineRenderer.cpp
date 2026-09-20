#include "OfflineRenderer.h"

#include <algorithm>

#include "SessionMixer.h"

namespace rsd {

namespace {
constexpr int64_t kBlockFrames = 1024;
}

int64_t sessionContentLengthSamples(const Session& session) {
    int64_t maxEnd = 0;
    for (auto& track : session.tracks) {
        for (auto& clip : *track->clipsSnapshot()) {
            maxEnd = std::max(maxEnd, clip->sessionStartSample + clip->lengthSamples);
        }
        for (auto& note : *track->midiClipsSnapshot()) {
            maxEnd = std::max(maxEnd, note->startSample + note->lengthSamples);
        }
    }
    return maxEnd;
}

std::shared_ptr<AudioBuffer> renderSessionMixdown(Session& session, unsigned int sampleRate,
                                                   unsigned int channels, int64_t lengthSamples) {
    auto buffer = std::make_shared<AudioBuffer>();
    buffer->channels = static_cast<int>(channels);
    buffer->sampleRate = static_cast<int>(sampleRate);
    buffer->samples.assign(static_cast<size_t>(std::max<int64_t>(0, lengthSamples)) * channels, 0.0f);

    SessionMixScratch scratch;
    std::vector<float> blockOut(static_cast<size_t>(kBlockFrames) * channels, 0.0f);

    for (int64_t pos = 0; pos < lengthSamples; pos += kBlockFrames) {
        unsigned int nFrames = static_cast<unsigned int>(std::min<int64_t>(kBlockFrames, lengthSamples - pos));
        mixSessionBlock(session, sampleRate, channels, pos, nFrames, /*playbackActive=*/true,
                         blockOut.data(), scratch);
        std::copy(blockOut.begin(), blockOut.begin() + static_cast<long>(nFrames) * channels,
                  buffer->samples.begin() + pos * channels);
    }

    return buffer;
}

std::shared_ptr<AudioBuffer> renderTrackStem(Track& track, unsigned int sampleRate, unsigned int channels,
                                              int64_t lengthSamples) {
    auto buffer = std::make_shared<AudioBuffer>();
    buffer->channels = static_cast<int>(channels);
    buffer->sampleRate = static_cast<int>(sampleRate);
    buffer->samples.assign(static_cast<size_t>(std::max<int64_t>(0, lengthSamples)) * channels, 0.0f);

    std::vector<float> blockOut(static_cast<size_t>(kBlockFrames) * channels, 0.0f);

    for (int64_t pos = 0; pos < lengthSamples; pos += kBlockFrames) {
        unsigned int nFrames = static_cast<unsigned int>(std::min<int64_t>(kBlockFrames, lengthSamples - pos));
        renderTrackBlock(track, sampleRate, channels, pos, nFrames, /*playbackActive=*/true, blockOut.data());
        std::copy(blockOut.begin(), blockOut.begin() + static_cast<long>(nFrames) * channels,
                  buffer->samples.begin() + pos * channels);
    }

    return buffer;
}

} // namespace rsd
