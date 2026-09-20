#pragma once

#include <QPoint>
#include <QUuid>
#include <QWidget>
#include <memory>

class QContextMenuEvent;
class QDragEnterEvent;
class QDropEvent;
class QWheelEvent;

#include "command/CommandStack.h"
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

    // Edits made directly in this lane (move/trim/split/delete) push their
    // own undo command once the CommandStack is set; not required for the
    // widget to function without undo support.
    void setCommandStack(CommandStack* stack) { m_commandStack = stack; }

    // All track lanes (and the ruler) must agree on the same sample<->pixel
    // scale, otherwise the playhead/ruler and each lane's clips would drift
    // out of alignment with each other.
    void setSharedTimelineLength(int64_t samples);
    void setPlayheadSample(int64_t sample);

    // Horizontal scroll support (only meaningful once zoomed in past what
    // fits in the widget's width). Scrolling is per-track by default; the
    // owning TrackRowWidget/TimelineView handle syncing multiple lanes
    // together (Shift modifier) by calling setScrollOffsetSamples() on
    // other lanes in response to this lane's signals.
    void setContentExtentSamples(int64_t samples); // full, un-zoomed content length
    void setScrollOffsetSamples(int64_t samples);
    int64_t maxScrollOffsetSamples() const;
    int64_t visibleLengthSamples() const { return effectiveTimelineLength(); }
    int64_t currentScrollOffsetSamples() const { return m_scrollOffsetSamples; }

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

    // Fired whenever the scroll offset changes (scrollbar drag or wheel), so
    // the owning row can keep its scrollbar widget's displayed value in sync.
    void scrollOffsetChanged(int64_t sampleOffset);
    // Fired whenever the scrollable range changes (zoom, content growth/
    // shrink), so the owning row can update its scrollbar's range/page step
    // and enabled state.
    void scrollRangeChanged();
    // A wheel-scroll happened with Shift held: the owning row's
    // TimelineView should apply this same absolute offset to every track.
    void syncScrollToAllRequested(int64_t sampleOffset);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool event(QEvent* event) override; // handles QEvent::ToolTip for per-clip hover info

private:
    enum class DragMode { None, Move, TrimStart, TrimEnd, FadeIn, FadeOut };

    int64_t xToSample(int x) const;
    int sampleToX(int64_t sample) const;
    int64_t timelineLengthSamples() const;
    int64_t effectiveTimelineLength() const;
    int64_t effectiveScrollOffset() const;
    std::shared_ptr<Clip> findClipAt(int64_t sample) const;
    void updateHoverCursor(const QPoint& pos);
    static QString formatDuration(int64_t samples, int sampleRate);

    std::shared_ptr<Track> m_track;
    QUuid m_selectedClipId;
    CommandStack* m_commandStack = nullptr;
    std::shared_ptr<const Track::ClipList> m_editBeforeSnapshot; // set while a drag/edit is in flight

    DragMode m_dragMode = DragMode::None;
    QUuid m_dragClipId;
    int m_dragStartX = 0;
    int64_t m_dragOrigStart = 0;
    int64_t m_dragOrigOffset = 0;
    int64_t m_dragOrigLength = 0;
    int64_t m_dragOrigFadeIn = 0;
    int64_t m_dragOrigFadeOut = 0;
    int64_t m_dragTotalSamples = 0; // timeline scale locked at drag start

    int64_t m_sharedTimelineLength = 0; // 0 = not set, fall back to local computation
    int64_t m_playheadSample = -1;      // -1 = hidden
    bool m_scrubbingPlayhead = false;

    int64_t m_contentExtentSamples = 0;  // full, un-zoomed content length
    int64_t m_scrollOffsetSamples = 0;   // this lane's own horizontal scroll position
    int64_t m_dragScrollOffsetSamples = 0; // scroll offset locked at drag start

    static constexpr int kEdgeThresholdPx = 10;
    // A fade handle only grabs the mouse within this many pixels of the top
    // of the clip, near an edge — below that band, the same edge is a trim
    // handle instead (checked in that order in mousePressEvent).
    static constexpr int kFadeHandleBandPx = 14;
};

} // namespace rsd
