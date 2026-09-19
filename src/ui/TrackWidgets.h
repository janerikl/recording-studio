#pragma once

#include <QCheckBox>
#include <QLabel>
#include <QRadioButton>
#include <QWidget>
#include <memory>

#include "model/Track.h"
#include "ui/WaveformWidget.h"

namespace rsd {

// One row in the timeline: track header controls (name, mute/solo/arm,
// select-for-record/import) plus that track's waveform lane.
class TrackRowWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrackRowWidget(std::shared_ptr<Track> track, QWidget* parent = nullptr);

    std::shared_ptr<Track> track() const { return m_track; }
    QRadioButton* selectButton() const { return m_selectButton; }
    void setWaveformBuffer(std::shared_ptr<AudioBuffer> buffer);

signals:
    void selected(std::shared_ptr<Track> track);

private:
    std::shared_ptr<Track> m_track;
    QRadioButton* m_selectButton = nullptr;
    QCheckBox* m_muteBox = nullptr;
    QCheckBox* m_soloBox = nullptr;
    QCheckBox* m_armBox = nullptr;
    WaveformWidget* m_waveform = nullptr;
};

} // namespace rsd
