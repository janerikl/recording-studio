#pragma once

#include <QString>
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "LibraryFolder.h"
#include "MasterBus.h"
#include "Track.h"

namespace rsd {

class Session {
public:
    int sampleRate = 48000;
    int channels = 2;
    double bpm = 120.0;
    bool metronomeEnabled = false;
    std::vector<std::shared_ptr<Track>> tracks;
    MasterBus masterBus;
    QString filePath;
    bool dirty = false;

    // Numbered bookmarks (slots 1-9), set/jumped via Ctrl+Shift+N / Ctrl+N.
    std::map<int, int64_t> markers;

    // User-organized virtual folders for the Media Browser (Project Media +
    // Loops), persisted alongside the rest of the session.
    LibraryFolderTree libraryFolders;

    std::shared_ptr<Track> addTrack(const QString& name) {
        auto track = std::make_shared<Track>();
        track->name = name;
        tracks.push_back(track);
        return track;
    }

    // Inserts an already-constructed track (e.g. one being redone/undone) at
    // a specific index so undo can restore its original position.
    void insertTrack(std::shared_ptr<Track> track, size_t index) {
        index = std::min(index, tracks.size());
        tracks.insert(tracks.begin() + static_cast<long>(index), std::move(track));
    }

    void removeTrack(const QUuid& id) {
        tracks.erase(std::remove_if(tracks.begin(), tracks.end(),
                                     [&](const auto& t) { return t->id == id; }),
                     tracks.end());
    }
};

} // namespace rsd
