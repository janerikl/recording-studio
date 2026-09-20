#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDial>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QWidget>
#include <memory>
#include <optional>
#include <vector>

#include "command/CommandStack.h"
#include "command/EditCommands.h"
#include "model/Track.h"
#include "ui/LevelMeterWidget.h"

namespace rsd {

// A single vertical mixer channel strip (Pro Tools/Ableton-style): the same
// controls as TrackRowWidget's header, laid out vertically, as an
// alternate side-by-side view rather than one row per track. Wiring
// mirrors TrackWidgets.cpp control-by-control (mutate the atomic live
// during a drag, push one TrackStateCommand snapshot on release) rather
// than sharing a base class with TrackRowWidget — the two layouts are too
// different to gain much from that, and each widget already owns its own
// control wiring in this codebase.
class MixerStripWidget : public QWidget {
    Q_OBJECT

public:
    explicit MixerStripWidget(std::shared_ptr<Track> track, QWidget* parent = nullptr);

    std::shared_ptr<Track> track() const { return m_track; }
    void setCommandStack(CommandStack* stack) { m_commandStack = stack; }
    // Re-reads the track's live effect chain and updates the FX button
    // label. Call after any edit made through the effects rack panel.
    void refreshEffectsButton();
    // Repopulates the send-bus dropdown with the current set of Bus
    // tracks; no-op for a Bus track's own strip (buses don't send).
    void refreshSendBusOptions(const std::vector<std::shared_ptr<Track>>& busTracks);
    // Polls this track's post-fader peak atomics into the strip's meter.
    // Call periodically from the same timer that drives the transport
    // toolbar's global meters.
    void updateMeter();

signals:
    void selected(std::shared_ptr<Track> track);
    void effectsPanelRequested(std::shared_ptr<Track> track);

private:
    std::shared_ptr<Track> m_track;
    CommandStack* m_commandStack = nullptr;

    QLabel* m_nameLabel = nullptr;
    QPushButton* m_effectsButton = nullptr;
    QCheckBox* m_muteBox = nullptr;
    QCheckBox* m_soloBox = nullptr;
    QCheckBox* m_armBox = nullptr;
    QComboBox* m_sourceCombo = nullptr;
    QDial* m_panDial = nullptr;
    QSlider* m_volumeSlider = nullptr;
    LevelMeterWidget* m_levelMeter = nullptr;
    QComboBox* m_sendBusCombo = nullptr;
    QSlider* m_sendLevelSlider = nullptr;

    // Captured on press so a whole drag gesture becomes one undo step.
    std::optional<TrackState> m_dragBeforeState;
};

} // namespace rsd
