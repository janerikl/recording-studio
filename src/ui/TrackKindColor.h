#pragma once

#include <QColor>

#include "model/Track.h"

namespace rsd {

// Fixed accent color per TrackKind, shown as a left-edge stripe on both
// TrackRowWidget and MixerStripWidget so a track's type is visible at a
// glance without opening its controls. Kept as a pure function (rather than
// inline in the widgets) so the mapping is unit-testable without a display.
inline QColor trackKindColor(TrackKind kind) {
    switch (kind) {
        case TrackKind::Audio: return QColor(70, 130, 220);       // blue
        case TrackKind::Instrument: return QColor(90, 180, 100);  // green
        case TrackKind::Bus: return QColor(220, 150, 60);         // orange
    }
    return QColor(120, 120, 120);
}

} // namespace rsd
