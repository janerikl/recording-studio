#pragma once

#include <memory>

#include "model/AudioBuffer.h"
#include "model/RhythmPattern.h"

namespace rsd {

// Renders a rhythm pattern to an in-memory click track (a short blip at
// each non-rest note's onset, silence elsewhere/at rests) for audible
// playback via AudioEngine::previewSample() — reuses the existing preview
// path (Loop Browser/Media Library) instead of new RT scheduling.
std::shared_ptr<AudioBuffer> renderRhythmClickBuffer(const RhythmPattern& pattern, double bpm,
                                                      unsigned int sampleRate, unsigned int channels);

// Renders a rhythm pattern through the real piano SoundFont instead of a
// click: every non-rest note is played at a fixed pitch (middle C) for
// its written duration, so it sounds musical rather than a metronome
// blip. Same playback path (AudioEngine::previewSample()).
std::shared_ptr<AudioBuffer> renderRhythmPianoBuffer(const RhythmPattern& pattern, double bpm,
                                                      unsigned int sampleRate, unsigned int channels);

} // namespace rsd
