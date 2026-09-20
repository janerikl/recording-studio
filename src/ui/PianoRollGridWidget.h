#pragma once

#include <QUuid>
#include <QWidget>
#include <memory>

class QContextMenuEvent;
class QPainter;

#include "command/CommandStack.h"
#include "model/Track.h"

namespace rsd {

// Editable piano-roll grid for an Instrument track's midiClips: draw new
// notes on empty cells, move/resize existing ones by dragging, delete via
// the Delete key, and drag a note's velocity bar in the strip below the
// grid. Mirrors ClipLaneWidget's "mutate live on every mouse move, push one
// undo command with before/after snapshots on release" pattern, reusing the
// existing TrackMidiCommand (no new command type).
class PianoRollGridWidget : public QWidget {
    Q_OBJECT

public:
    explicit PianoRollGridWidget(QWidget* parent = nullptr);

    void setTrack(std::shared_ptr<Track> track);
    void setCommandStack(CommandStack* stack) { m_commandStack = stack; }
    void setBpm(double bpm);
    void setSampleRate(int sampleRate);
    void setSnapDenominator(int denominator); // 0 = off, else 4/8/16/32

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class DragMode { None, Move, Resize, Velocity };

    int64_t xToSample(int x) const;
    int sampleToX(int64_t sample) const;
    int64_t snapIntervalSamples() const;
    std::shared_ptr<MidiNote> findNoteAt(int x, int y) const;
    void commitEdit(std::shared_ptr<const Track::MidiNoteList> before, const QString& text);
    int gridHeight() const;
    void updateContentSize();

    std::shared_ptr<Track> m_track;
    CommandStack* m_commandStack = nullptr;
    double m_bpm = 120.0;
    int m_sampleRate = 48000;
    int m_snapDenominator = 16;

    QUuid m_selectedNoteId;
    DragMode m_dragMode = DragMode::None;
    QUuid m_dragNoteId;
    int m_dragStartX = 0;
    int m_dragStartY = 0;
    int64_t m_dragOrigStart = 0;
    int m_dragOrigPitch = 0;
    int64_t m_dragOrigLength = 0;
    std::shared_ptr<const Track::MidiNoteList> m_editBeforeSnapshot;

    static constexpr int kLowPitch = 36;
    static constexpr int kHighPitch = 96;
    static constexpr int kRowHeightPx = 10;
    static constexpr int kVelocityLaneHeightPx = 60;
    static constexpr int kPixelsPerBeat = 80;
    static constexpr int kResizeMarginPx = 6;
    static constexpr int kVisibleBeats = 64; // fixed content width for v1
};

} // namespace rsd
