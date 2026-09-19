#pragma once

#include <QUuid>
#include <QWidget>
#include <memory>

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

signals:
    void selectionChanged(bool hasSelection);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    enum class DragMode { None, Move, TrimStart, TrimEnd };

    int64_t xToSample(int x) const;
    int sampleToX(int64_t sample) const;
    int64_t timelineLengthSamples() const;
    std::shared_ptr<Clip> findClipAt(int64_t sample) const;

    std::shared_ptr<Track> m_track;
    QUuid m_selectedClipId;

    DragMode m_dragMode = DragMode::None;
    QUuid m_dragClipId;
    int m_dragStartX = 0;
    int64_t m_dragOrigStart = 0;
    int64_t m_dragOrigOffset = 0;
    int64_t m_dragOrigLength = 0;

    static constexpr int kEdgeThresholdPx = 6;
};

} // namespace rsd
