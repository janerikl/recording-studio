#pragma once

#include <QUuid>
#include <QString>
#include <cstdint>
#include <memory>

#include "AudioBuffer.h"

namespace rsd {

// Non-destructive reference into a shared AudioBuffer. Trim/cut/delete only
// ever mutate the offset/length fields here, never the underlying samples.
class Clip {
public:
    QUuid id = QUuid::createUuid();
    std::shared_ptr<AudioBuffer> buffer;
    int64_t sessionStartSample = 0;  // position on the track's timeline
    int64_t sourceOffsetSamples = 0; // trim-in point within buffer
    int64_t lengthSamples = 0;       // trimmed length; trim-out = offset+length
    QString name;
    bool muted = false;
};

} // namespace rsd
