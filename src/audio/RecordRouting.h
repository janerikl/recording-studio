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

// Only Audio-kind tracks record from an input stream: Instrument tracks
// capture MIDI separately (PianoKeyboardWidget), and Bus tracks are fed
// only by other tracks' sends, never armed/recorded to directly.
inline std::vector<std::shared_ptr<Track>> filterRecordableTracks(
    const std::vector<std::shared_ptr<Track>>& armedTracks) {
    std::vector<std::shared_ptr<Track>> result;
    for (auto& t : armedTracks) {
        if (t->kind == TrackKind::Audio) result.push_back(t);
    }
    return result;
}

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
