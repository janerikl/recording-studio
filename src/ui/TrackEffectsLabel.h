#pragma once

#include <cstddef>
#include <string>

namespace rsd {

// Label text for the inline "FX" button on a track row header. Kept as a
// pure function (rather than inline in TrackWidgets.cpp) so the formatting
// is unit-testable without needing a QWidget/display.
inline std::string formatEffectsButtonLabel(std::size_t effectCount) {
    if (effectCount == 0) return "FX";
    return "FX: " + std::to_string(effectCount);
}

} // namespace rsd
