#pragma once

#include <memory>
#include <vector>

#include "model/Track.h"

namespace rsd {

// Splits armed tracks by which capture stream should feed them, so the two
// concurrent input streams (mic, system audio) each get their own clip
// instead of every armed track receiving a copy of the same audio.
struct RecordRoutingSplit {
    std::vector<std::shared_ptr<Track>> micTracks;
    std::vector<std::shared_ptr<Track>> systemAudioTracks;
};

inline RecordRoutingSplit splitTracksBySource(const std::vector<std::shared_ptr<Track>>& armedTracks) {
    RecordRoutingSplit split;
    for (auto& track : armedTracks) {
        if (track->inputSource.load() == AudioSource::SystemAudio) {
            split.systemAudioTracks.push_back(track);
        } else {
            split.micTracks.push_back(track);
        }
    }
    return split;
}

} // namespace rsd
