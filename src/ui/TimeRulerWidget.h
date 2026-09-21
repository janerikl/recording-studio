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

signals:
    void seekRequested(int64_t sample);
    // Emitted continuously while right-dragging and once more on release.
    void punchRegionEdited(PunchRegion region);

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

    std::map<int, int64_t> m_markers;
};

} // namespace rsd
