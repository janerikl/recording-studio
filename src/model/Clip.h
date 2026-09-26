#pragma once

#include <QUuid>
#include <QString>
#include <cstdint>
#include <memory>

#include "AudioBuffer.h"

namespace rsd {

enum class FadeCurve { Linear, EqualPower };

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

    // Per-clip gain multiplier, applied on top of the track's gain.
    float gain = 1.0f;
    // Fade lengths in samples, measured from the clip's timeline start/end.
    // A crossfade is just two clips whose fade-out/fade-in regions overlap on
    // the timeline — mixClipInto already sums overlapping clips, so no
    // separate crossfade type is needed.
    int64_t fadeInSamples = 0;
    int64_t fadeOutSamples = 0;
    FadeCurve fadeInCurve = FadeCurve::Linear;
    FadeCurve fadeOutCurve = FadeCurve::Linear;

    // Transient marker for a still-recording preview clip (see MainWindow's
    // m_livePreviewClips): true only between Record and Stop, never
    // persisted by SessionIO. ClipLaneWidget draws it in a distinct color
    // so it reads as "still recording", not a finished clip.
    bool isLiveRecording = false;
};

} // namespace rsd
