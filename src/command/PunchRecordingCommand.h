#pragma once

#include <memory>

#include "EditCommands.h"
#include "audio/PunchRecorder.h"
#include "model/AudioBuffer.h"
#include "model/Clip.h"
#include "model/Track.h"

namespace rsd {

// Builds the single undoable action for a finished punch/loop recording
// session: one new clip covering the punch region, added in one atomic
// clip-list swap so undo reverts every loop pass at once (per design —
// intermediate passes are never individually undoable). Returns nullptr if
// nothing was captured (e.g. the user stopped before reaching the region).
inline std::unique_ptr<Command> buildPunchRecordingCommand(const std::shared_ptr<Track>& track,
                                                            const PunchRecorder& recorder,
                                                            unsigned int sampleRate,
                                                            QString clipName = "Recording") {
    if (!recorder.hasCaptured() || !recorder.region().isValid()) return nullptr;

    auto buffer = std::make_shared<AudioBuffer>();
    buffer->samples = recorder.buffer();
    buffer->channels = static_cast<int>(recorder.channels());
    buffer->sampleRate = static_cast<int>(sampleRate);

    auto clip = std::make_shared<Clip>();
    clip->buffer = buffer;
    clip->sessionStartSample = recorder.region().startSample;
    clip->sourceOffsetSamples = 0;
    clip->lengthSamples = recorder.region().lengthSamples();
    clip->name = std::move(clipName);

    auto before = track->clipsSnapshot();
    track->addClip(clip);
    auto after = track->clipsSnapshot();

    return std::make_unique<TrackClipsCommand>(track, before, after, "Punch Record");
}

} // namespace rsd
