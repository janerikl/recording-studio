#pragma once

#include <QButtonGroup>
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
    void refreshTrackWaveform(const QUuid& trackId);
    void deleteSelectedClipOn(const QUuid& trackId);

signals:
    void trackSelected(std::shared_ptr<Track> track);
    void clipSelectionChanged(std::shared_ptr<Track> track, bool hasSelection);

private:
    QWidget* m_content = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QButtonGroup* m_selectGroup = nullptr;
    std::unordered_map<QString, TrackRowWidget*> m_rows; // keyed by QUuid::toString()
};

} // namespace rsd
