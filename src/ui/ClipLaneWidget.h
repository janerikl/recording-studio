#pragma once

#include <QPoint>
#include <QUuid>
#include <QWidget>
#include <memory>

class QContextMenuEvent;
class QDragEnterEvent;
class QDropEvent;
class QKeyEvent;
class QWheelEvent;
class QPainter;

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

    // Shift+drag on the lane background marks a time range (highlighted
    // across the full lane height, independent of the single-clip/note
    // selection above). Ctrl+C copies whatever it overlaps for this track's
    // kind (Clips or MidiNotes) into a shared app-wide clipboard; Ctrl+V,
    // pressed while a (possibly different) track's lane has focus, pastes
    // at that lane's current playhead position, overwriting anything the
    // pasted region lands on.
    bool hasRangeSelection() const { return m_hasRangeSelection; }
    void copyRangeSelection();
    void pasteAtPlayhead();

    // Ctrl+C/Ctrl+V also work on the single selected clip/note (no range
    // selection needed): copy captures just that item, and each Ctrl+V
    // inserts a copy immediately after the previous one (original on the
    // first paste, the just-pasted copy on every paste after that), pushing
    // any later clips/notes on the same track out of the way. Scoped to the
    // track it was copied from — pressing Ctrl+V on a different track's lane
    // does nothing, since "after the original" only means something there.
    void copySelectedItem();
    void pasteChainedSingleItem();

    // Edits made directly in this lane (move/trim/split/delete) push their
    // own undo command once the CommandStack is set; not required for the
    // widget to function without undo support.
    void setCommandStack(CommandStack* stack) { m_commandStack = stack; }

    // All track lanes (and the ruler) must agree on the same sample<->pixel
    // scale, otherwise the playhead/ruler and each lane's clips would drift
    // out of alignment with each other.
    void setSharedTimelineLength(int64_t samples);
    void setPlayheadSample(int64_t sample);
    // Needed only to label/space the background gridlines matching the
    // ruler's tick marks (see paintGridLines()); doesn't affect any
    // sample<->pixel math elsewhere in this widget.
    void setSampleRate(int sampleRate) {
        m_sampleRate = sampleRate;
        update();
    }

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
    // A plain file (not a MediaBrowserPanel entry) was dropped here —
    // either dragged straight from a file manager, or from the loop
    // browser panel.
    void externalFileDropped(QString filePath, int64_t sessionStartSample);

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
    void paintMidiNotes(QPainter& painter); // Instrument tracks: view-only note rectangles
    // Vertical gridlines at the same tick spacing as TimeRulerWidget, so a
    // track's clips can be visually lined up against the ruler's time marks.
    void paintGridLines(QPainter& painter);
    void paintRangeSelection(QPainter& painter);
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool event(QEvent* event) override; // handles QEvent::ToolTip for per-clip hover info

private:
    enum class DragMode { None, Move, TrimStart, TrimEnd, FadeIn, FadeOut, Gain };

    int64_t xToSample(int x) const;
    int sampleToX(int64_t sample) const;
    int64_t timelineLengthSamples() const;
    int64_t effectiveTimelineLength() const;
    int64_t effectiveScrollOffset() const;
    std::shared_ptr<Clip> findClipAt(int64_t sample) const;
    std::shared_ptr<MidiNote> findMidiNoteAt(int64_t sample, int y) const;
    // Right-click "Repeat..." menu action: prompts for a count, then inserts
    // that many back-to-back copies right after the clicked clip/note,
    // pushing later items out of the way, as a single undo step.
    void repeatClip(const std::shared_ptr<Clip>& clip);
    void repeatMidiNote(const std::shared_ptr<MidiNote>& note);
    void updateHoverCursor(const QPoint& pos);
    static QString formatDuration(int64_t samples, int sampleRate);

    std::shared_ptr<Track> m_track;
    QUuid m_selectedClipId;
    QUuid m_selectedMidiNoteId; // Instrument tracks only; parallels m_selectedClipId
    CommandStack* m_commandStack = nullptr;
    std::shared_ptr<const Track::ClipList> m_editBeforeSnapshot; // set while a drag/edit is in flight

    DragMode m_dragMode = DragMode::None;
    QUuid m_dragClipId;
    int m_dragStartX = 0;
    int m_dragStartY = 0;
    int64_t m_dragOrigStart = 0;
    int64_t m_dragOrigOffset = 0;
    int64_t m_dragOrigLength = 0;
    int64_t m_dragOrigFadeIn = 0;
    int64_t m_dragOrigFadeOut = 0;
    float m_dragOrigGain = 1.0f;
    int64_t m_dragTotalSamples = 0; // timeline scale locked at drag start

    int m_sampleRate = 48000; // only used to convert gridline tick seconds to samples
    int64_t m_sharedTimelineLength = 0; // 0 = not set, fall back to local computation
    int64_t m_playheadSample = -1;      // -1 = hidden
    bool m_scrubbingPlayhead = false;

    int64_t m_contentExtentSamples = 0;  // full, un-zoomed content length
    int64_t m_scrollOffsetSamples = 0;   // this lane's own horizontal scroll position
    int64_t m_dragScrollOffsetSamples = 0; // scroll offset locked at drag start

    bool m_rangeSelecting = false;    // actively dragging out a new range (Shift held)
    bool m_hasRangeSelection = false;
    int64_t m_rangeSelectionStart = 0;
    int64_t m_rangeSelectionEnd = 0;

    static constexpr int kEdgeThresholdPx = 10;
    // A fade handle only grabs the mouse within this many pixels of the top
    // of the clip, near an edge — below that band, the same edge is a trim
    // handle instead (checked in that order in mousePressEvent).
    static constexpr int kFadeHandleBandPx = 14;
    // How close (vertically, in px) the mouse must be to the gain line to
    // grab it instead of starting a move.
    static constexpr int kGainHandleBandPx = 6;
};

} // namespace rsd
