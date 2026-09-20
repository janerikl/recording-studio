#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include <QUuid>

#include "model/Session.h"

namespace rsd {

// Reused block-to-block scratch space so neither the live RT callback nor
// an offline render allocates mid-mix. One instance per independent mixing
// "stream" (AudioEngine owns its own; an offline export uses a fresh one).
struct SessionMixScratch {
    std::vector<float> trackScratch;
    std::vector<float> masterScratch;
    std::map<QUuid, std::vector<float>> busScratch;
};

// Renders one track's own post-fader signal for this block — clips or live
// synth, through its effects chain, through automation-ramped volume/pan —
// into `out` (overwritten, size >= nFrames*channels). Deliberately doesn't
// touch bus sends or the master bus: reused directly for a per-track stem
// render (see OfflineRenderer.h), which skips both by design. `pos` is the
// session-timeline sample at the start of this block; `playbackActive`
// mirrors AudioEngine's playing/recording check (an Instrument track still
// drains its live-note queue and renders regardless, for on-screen-keyboard
// audition, but only triggers from recorded MIDI clips when true).
void renderTrackBlock(Track& track, unsigned int sampleRate, unsigned int channels, int64_t pos,
                      unsigned int nFrames, bool playbackActive, float* out);

// Mixes the whole session for one block: every non-Bus, audible (mute/solo)
// track via renderTrackBlock, accumulated into the master buffer and (per
// any aux send) into its destination bus's buffer; each Bus track's own
// effects/volume then mix into the master buffer; finally the master bus's
// own effects/volume apply, writing the result into `out` (overwritten,
// size >= nFrames*channels). This is the exact mixing logic
// AudioEngine::rtCallback uses for live playback, extracted so an offline
// export sounds identical — see OfflineRenderer.h.
void mixSessionBlock(Session& session, unsigned int sampleRate, unsigned int channels, int64_t pos,
                     unsigned int nFrames, bool playbackActive, float* out, SessionMixScratch& scratch);

} // namespace rsd
