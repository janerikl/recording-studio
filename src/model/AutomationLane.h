#pragma once

#include <vector>

#include "audio/AutomationMath.h"

namespace rsd {

// Which track parameter an automation lane drives. Volume/Pan are
// post-synth gain stages (any track kind). Expression and Vibrato only
// affect Instrument tracks: they're sent to the track's synth as MIDI CC11
// (expression) and CC1 (mod wheel -> FluidSynth's default vibrato-depth
// modulator), with values 0..1 mapped to CC 0..127. Without a lane they
// sit at the MIDI defaults (expression 1.0, vibrato 0.0).
enum class AutomationTarget { Volume, Pan, Expression, Vibrato };

// A draggable-breakpoint curve for one track parameter. `points` is kept
// sorted by sample (see AutomationMath.h's insertPointSorted); an empty
// lane means "no automation for this target on this track" — the track's
// static Volume/Pan atomic applies instead.
struct AutomationLane {
    AutomationTarget target = AutomationTarget::Volume;
    std::vector<AutomationPoint> points;
};

} // namespace rsd
