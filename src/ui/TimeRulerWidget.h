#pragma once

#include <QWidget>
#include <cstdint>
#include <map>

#include "audio/PunchRegion.h"

namespace rsd {

// A time ruler aligned with ClipLaneWidget's horizontal scale (same left
// margin as each track row's header). Shows second tick marks, the
// playhead, and lets the user click to seek.
//
// Right-drag defines the punch in/out (loop) region — kept on a separate
// mouse button from left-drag seek so the two gestures never conflict.
class TimeRulerWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimeRulerWidget(QWidget* parent = nullptr);

    void setLeftMargin(int px) { m_leftMargin = px; update(); }
    void setSampleRate(int sr) { m_sampleRate = sr; }
    void setTimelineLength(int64_t samples) { m_timelineLength = samples; update(); }
    void setPlayheadSample(int64_t sample) { m_playheadSample = sample; update(); }

    // Synced from the numeric punch-region fields; also reflects drags made
    // directly on the ruler.
    void setPunchRegion(PunchRegion region) { m_punchRegion = region; update(); }
    PunchRegion punchRegion() const { return m_punchRegion; }

    // Numbered bookmarks (slot -> sample position), drawn as small flags.
    void setMarkers(const std::map<int, int64_t>& markers) { m_markers = markers; update(); }

    // Playback loop region: drawn as a highlighted band when enabled,
    // independent of the punch-recording region above. Set externally (e.g.
    // on session load) or driven internally by a Ctrl+drag on this ruler,
    // which reports the finished region via loopRegionSet.
    void setLoopRegion(bool enabled, int64_t startSample, int64_t endSample) {
        m_loopEnabled = enabled;
        m_loopStart = startSample;
        m_loopEnd = endSample;
        update();
    }

signals:
    void seekRequested(int64_t sample);
    // Emitted continuously while right-dragging and once more on release.
    void punchRegionEdited(PunchRegion region);

    // Emitted once a Ctrl+left-drag on the ruler finishes: a real drag
    // (endSample > startSample) enables the loop over that region; a plain
    // Ctrl+click (no movement) disables it.
    void loopRegionSet(int64_t startSample, int64_t endSample, bool enable);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    int laneWidth() const { return std::max(0, width() - m_leftMargin); }
    int sampleToX(int64_t sample) const;
    int64_t xToSample(int x) const;

    int m_leftMargin = 160;
    int m_sampleRate = 48000;
    int64_t m_timelineLength = 1;
    int64_t m_playheadSample = 0;
    bool m_scrubbing = false;

    PunchRegion m_punchRegion;
    bool m_definingPunchRegion = false;
    int64_t m_punchDragAnchor = 0;

    bool m_definingLoopRegion = false;
    int64_t m_loopDragAnchor = 0;

    std::map<int, int64_t> m_markers;

    bool m_loopEnabled = false;
    int64_t m_loopStart = 0;
    int64_t m_loopEnd = 0;
};

} // namespace rsd
