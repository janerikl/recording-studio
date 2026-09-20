#pragma once

#include <vector>

#include "audio/AutomationMath.h"

namespace rsd {

// Which track parameter an automation lane drives.
enum class AutomationTarget { Volume, Pan };

// A draggable-breakpoint curve for one track parameter. `points` is kept
// sorted by sample (see AutomationMath.h's insertPointSorted); an empty
// lane means "no automation for this target on this track" — the track's
// static Volume/Pan atomic applies instead.
struct AutomationLane {
    AutomationTarget target = AutomationTarget::Volume;
    std::vector<AutomationPoint> points;
};

} // namespace rsd
