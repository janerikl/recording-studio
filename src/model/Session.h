#pragma once

#include <QString>
#include <memory>
#include <vector>

#include "Track.h"

namespace rsd {

class Session {
public:
    int sampleRate = 48000;
    int channels = 2;
    std::vector<std::shared_ptr<Track>> tracks;
    QString filePath;
    bool dirty = false;

    std::shared_ptr<Track> addTrack(const QString& name) {
        auto track = std::make_shared<Track>();
        track->name = name;
        tracks.push_back(track);
        return track;
    }
};

} // namespace rsd
