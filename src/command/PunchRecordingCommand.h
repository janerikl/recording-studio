#pragma once

#include <memory>

#include "EditCommands.h"
#include "audio/PunchRecorder.h"
#include "model/AudioBuffer.h"
#include "model/Clip.h"
#include "model/Track.h"

namespace rsd {

// Builds the single undoable action for a finished punch/loop recording
// session: one new clip covering the punch region is added to the active
// comp (sourced from the LAST pass, same as before), and every captured
// pass is saved as a take lane for comping — bundled into one
// CompositeCommand so a single undo reverts both at once (per design —
// intermediate passes are never individually undoable, only which take is
// active). Returns nullptr if nothing was captured (e.g. the user stopped
// before reaching the region).
inline std::unique_ptr<Command> buildPunchRecordingCommand(const std::shared_ptr<Track>& track,
                                                            const PunchRecorder& recorder,
                                                            unsigned int sampleRate,
                                                            QString clipName = "Recording") {
    if (!recorder.hasCaptured() || !recorder.region().isValid()) return nullptr;

    int64_t regionStart = recorder.region().startSample;
    int64_t regionLength = recorder.region().lengthSamples();
    int takeCount = recorder.takeCount();

    // One AudioBuffer per take, built once and reused for both the take-lane
    // entry and (for the last pass) the active comp clip, so the two share
    // the same buffer identity — lets the UI detect which take is currently
    // active by comparing buffer pointers.
    std::vector<std::shared_ptr<AudioBuffer>> takeBuffers;
    takeBuffers.reserve(static_cast<size_t>(takeCount));
    for (int i = 0; i < takeCount; ++i) {
        auto buf = std::make_shared<AudioBuffer>();
        buf->samples = recorder.takeBuffer(i);
        buf->channels = static_cast<int>(recorder.channels());
        buf->sampleRate = static_cast<int>(sampleRate);
        takeBuffers.push_back(std::move(buf));
    }

    std::vector<std::unique_ptr<Command>> subCommands;

    auto compClip = std::make_shared<Clip>();
    compClip->buffer = takeBuffers.back(); // most recent pass
    compClip->sessionStartSample = regionStart;
    compClip->sourceOffsetSamples = 0;
    compClip->lengthSamples = regionLength;
    compClip->name = clipName;

    auto clipsBefore = track->clipsSnapshot();
    track->addClip(compClip);
    auto clipsAfter = track->clipsSnapshot();
    subCommands.push_back(std::make_unique<TrackClipsCommand>(track, clipsBefore, clipsAfter, "Punch Record"));

    Track::ClipList takes;
    takes.reserve(static_cast<size_t>(takeCount));
    for (int i = 0; i < takeCount; ++i) {
        auto take = std::make_shared<Clip>();
        take->buffer = takeBuffers[static_cast<size_t>(i)];
        take->sessionStartSample = regionStart;
        take->sourceOffsetSamples = 0;
        take->lengthSamples = regionLength;
        take->name = QString("Take %1").arg(i + 1);
        takes.push_back(std::move(take));
    }
    auto takesBefore = track->takesSnapshot();
    track->setTakes(std::move(takes));
    auto takesAfter = track->takesSnapshot();
    subCommands.push_back(std::make_unique<TrackTakeLanesCommand>(track, takesBefore, takesAfter, "Capture Takes"));

    return std::make_unique<CompositeCommand>(std::move(subCommands), "Punch Record");
}

} // namespace rsd
