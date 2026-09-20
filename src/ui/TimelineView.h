#pragma once

#include <QButtonGroup>
#include <QPoint>
#include <QRect>
#include <QScrollArea>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>
#include <unordered_map>

#include "ui/TrackWidgets.h"

namespace rsd {

// Vertically stacked track rows inside a scroll area.
class TimelineView : public QScrollArea {
    Q_OBJECT

public:
    explicit TimelineView(QWidget* parent = nullptr);

    void addTrack(std::shared_ptr<Track> track);
    void removeTrack(const QUuid& trackId);
    void setCommandStack(CommandStack* stack);
    void clear(); // remove all rows, e.g. before loading a new session
    void refreshTrackWaveform(const QUuid& trackId);
    void refreshTrackEffectsButton(const QUuid& trackId);
    void deleteSelectedClipOn(const QUuid& trackId);
    void setSharedTimelineLength(int64_t samples);
    void setContentExtentSamples(int64_t samples);
    void setPlayheadSample(int64_t sample);
    void clearSelectionOn(const QUuid& trackId);

signals:
    void trackSelected(std::shared_ptr<Track> track);
    void clipSelectionChanged(std::shared_ptr<Track> track, bool hasSelection);
    void seekRequested(int64_t sample);
    void editStarted();
    // Emitted when a clip dropped on a DIFFERENT track's lane than the one it
    // started on; MainWindow performs the actual track reassignment.
    void clipMovedToTrack(QUuid clipId, QUuid sourceTrackId, QUuid destTrackId);
    // A media library item was dropped onto a track's lane.
    void mediaDroppedOnTrack(QUuid trackId, int libraryIndex, int64_t sessionStartSample);
    // The inline "FX" button on a track row was clicked.
    void effectsPanelRequested(std::shared_ptr<Track> track);

private:
    TrackRowWidget* rowForClipLane(QObject* clipLaneSender) const;
    TrackRowWidget* rowAtGlobalPos(const QPoint& globalPos) const;
    void onClipDraggedToGlobalPos(QUuid clipId, QPoint globalPos);
    void onClipDropped(QUuid clipId, QPoint globalPos);
    void onMediaDropped(int libraryIndex, int64_t sessionStartSample);

    QWidget* m_content = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QButtonGroup* m_selectGroup = nullptr;
    std::unordered_map<QString, TrackRowWidget*> m_rows; // keyed by QUuid::toString()
    int64_t m_lastTimelineLength = 0;
    int64_t m_lastContentExtentSamples = 0;
    int64_t m_lastPlayheadSample = 0;
    TrackRowWidget* m_highlightedRow = nullptr;
    CommandStack* m_commandStack = nullptr;
};

} // namespace rsd
