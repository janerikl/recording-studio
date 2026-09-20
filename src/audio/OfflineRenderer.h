#pragma once

#include <memory>

#include "model/Session.h"

namespace rsd {

// Total length (in samples) of the latest-ending clip or MIDI note across
// every track in the session. 0 if the session has no content. Export
// length is capped here — an effect's decay tail (reverb/delay) past this
// point isn't captured (a known v1 limitation, matching what the earlier,
// simpler offline render already had).
int64_t sessionContentLengthSamples(const Session& session);

// Renders the full session mixdown (tracks -> bus sends -> master, via
// mixSessionBlock — the exact same mixing live playback uses) to an
// in-memory buffer, block by block, with no audio device involved. Caller
// resets any Instrument track's SynthEngine first for a deterministic
// start (see Track::synthEngine / SynthEngine::reset()) and must not call
// this while AudioEngine's live stream is running (both would touch track
// state — e.g. the live-note queue — from different threads).
std::shared_ptr<AudioBuffer> renderSessionMixdown(Session& session, unsigned int sampleRate,
                                                   unsigned int channels, int64_t lengthSamples);

// Renders one track's own stem — its own effects/automation, deliberately
// NOT summed into any bus send or the master bus (see
// renderTrackBlock() in SessionMixer.h) — to an in-memory buffer.
std::shared_ptr<AudioBuffer> renderTrackStem(Track& track, unsigned int sampleRate,
                                              unsigned int channels, int64_t lengthSamples);

} // namespace rsd
