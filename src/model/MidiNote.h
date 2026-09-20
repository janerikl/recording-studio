#pragma once

#include <QUuid>
#include <cstdint>

namespace rsd {

// A recorded note on an Instrument track's timeline. Analogous to Clip, but
// for MIDI rather than audio — positioned/lengthed the same way so the
// existing sample-based timeline math (scale, scroll, zoom) applies
// unchanged.
struct MidiNote {
    QUuid id = QUuid::createUuid();
    int pitch = 60;         // MIDI note number, 0-127 (60 = middle C)
    float velocity = 1.0f;  // 0..1
    int64_t startSample = 0; // position on the track's timeline
    int64_t lengthSamples = 0;
};

} // namespace rsd
