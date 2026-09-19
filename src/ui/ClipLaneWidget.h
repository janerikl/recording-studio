#pragma once

#include <QPoint>
#include <QUuid>
#include <QWidget>
#include <memory>

class QDragEnterEvent;
class QDropEvent;

#include "model/Track.h"

namespace rsd {

// Renders a track's clips positioned along its timeline and lets the user
// select, move (drag body), trim (drag edges), split (double-click), and
// delete (via MainWindow's button, acting on the current selection) clips.
// Non-destructive: all edits only touch Clip offset/length fields via
// Track's copy-on-write mutators, never the underlying sample data.
class ClipLaneWidget : public QWidget {
    Q_OBJECT

public:
    explicit ClipLaneWidget(std::shared_ptr<Track> track, QWidget* parent = nullptr);

    void refresh(); // call after any external change to the track's clips
    QUuid selectedClipId() const { return m_selectedClipId; }
    void deleteSelected();
    void clearSelection();

    // All track lanes (and the ruler) must agree on the same sample<->pixel
    // scale, otherwise the playhead/ruler and each lane's clips would drift
    // out of alignment with each other.
    void setSharedTimelineLength(int64_t samples);
    void setPlayheadSample(int64_t sample);

signals:
    void selectionChanged(bool hasSelection);
    void seekRequested(int64_t sample);
    void editStarted(); // emitted once, right before a move/trim/split mutates the track

    // Cross-track drag support: only emitted while DragMode::Move is active.
    // TimelineView (the only object with visibility into every row's screen
    // geometry) listens to these to highlight a hovered target row and, on
    // drop, reassign the clip to a different track if it landed on one.
    void clipDraggedToGlobalPos(QUuid clipId, QPoint globalPos);
    void clipDropped(QUuid clipId, QPoint globalPos);

    // Media library drag-and-drop: a library item was dropped at the given
    // timeline sample position on this lane. MainWindow resolves the library
    // index to an AudioBuffer and creates the new Clip.
    void mediaDropped(int libraryIndex, int64_t sessionStartSample);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool event(QEvent* event) override; // handles QEvent::ToolTip for per-clip hover info

private:
    enum class DragMode { None, Move, TrimStart, TrimEnd };

    int64_t xToSample(int x) const;
    int sampleToX(int64_t sample) const;
    int64_t timelineLengthSamples() const;
    int64_t effectiveTimelineLength() const;
    std::shared_ptr<Clip> findClipAt(int64_t sample) const;
    void updateHoverCursor(const QPoint& pos);
    static QString formatDuration(int64_t samples, int sampleRate);

    std::shared_ptr<Track> m_track;
    QUuid m_selectedClipId;

    DragMode m_dragMode = DragMode::None;
    QUuid m_dragClipId;
    int m_dragStartX = 0;
    int64_t m_dragOrigStart = 0;
    int64_t m_dragOrigOffset = 0;
    int64_t m_dragOrigLength = 0;
    int64_t m_dragTotalSamples = 0; // timeline scale locked at drag start

    int64_t m_sharedTimelineLength = 0; // 0 = not set, fall back to local computation
    int64_t m_playheadSample = -1;      // -1 = hidden
    bool m_scrubbingPlayhead = false;

    static constexpr int kEdgeThresholdPx = 10;
};

} // namespace rsd
