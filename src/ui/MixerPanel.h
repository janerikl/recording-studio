#pragma once

#include <QUuid>
#include <QWidget>
#include <map>
#include <memory>

#include "command/CommandStack.h"
#include "model/MasterBus.h"
#include "model/Track.h"

class QHBoxLayout;
class QSlider;
class QPushButton;

namespace rsd {

class MixerStripWidget;

// Bottom-docked, side-by-side alternate view of the same per-track mixer
// controls already on each TrackRowWidget header, plus a fixed Master
// strip. Mirrors TimelineView's addTrack()/removeTrack()/clear()/
// refreshSendBusOptions() interface — MainWindow drives both from the same
// call sites so the two per-track widget collections never drift apart.
class MixerPanel : public QWidget {
    Q_OBJECT

public:
    explicit MixerPanel(QWidget* parent = nullptr);

    void setCommandStack(CommandStack* stack);
    void setMasterBus(MasterBus* masterBus);

    void addTrack(std::shared_ptr<Track> track);
    void removeTrack(const QUuid& trackId);
    void clear();
    void refreshTrackEffectsButton(const QUuid& trackId);
    void refreshSendBusOptions();
    // Polls every strip's post-fader peak into its meter. Call from the same
    // timer that drives the transport toolbar's global input/output meters.
    void updateMeters();

signals:
    void trackSelected(std::shared_ptr<Track> track);
    void effectsPanelRequested(std::shared_ptr<Track> track);
    void masterEffectsPanelRequested();

private:
    QHBoxLayout* m_stripsLayout = nullptr;
    std::map<QString, MixerStripWidget*> m_strips; // keyed by track id string
    CommandStack* m_commandStack = nullptr;
    MasterBus* m_masterBus = nullptr;

    QSlider* m_masterVolumeSlider = nullptr;
    QPushButton* m_masterFxButton = nullptr;
};

} // namespace rsd
