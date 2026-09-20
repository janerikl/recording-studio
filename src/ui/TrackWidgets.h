#pragma once

#include <QCheckBox>
#include <QDial>
#include <QLabel>
#include <QRadioButton>
#include <QSlider>
#include <QWidget>
#include <memory>
#include <optional>

#include "command/CommandStack.h"
#include "command/EditCommands.h"
#include "model/Track.h"
#include "ui/ClipLaneWidget.h"

namespace rsd {

// One row in the timeline: track header controls (name, mute/solo/arm,
// select-for-record/import) plus that track's editable clip lane.
class TrackRowWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrackRowWidget(std::shared_ptr<Track> track, QWidget* parent = nullptr);

    std::shared_ptr<Track> track() const { return m_track; }
    QRadioButton* selectButton() const { return m_selectButton; }
    ClipLaneWidget* clipLane() const { return m_clipLane; }
    void refreshWaveform() { m_clipLane->refresh(); }
    void setDropHighlight(bool on); // visual feedback while a cross-track drag hovers this row
    void setCommandStack(CommandStack* stack);

signals:
    void selected(std::shared_ptr<Track> track);
    void clipSelectionChanged(std::shared_ptr<Track> track, bool hasSelection);

private:
    std::shared_ptr<Track> m_track;
    QRadioButton* m_selectButton = nullptr;
    QCheckBox* m_muteBox = nullptr;
    QCheckBox* m_soloBox = nullptr;
    QCheckBox* m_armBox = nullptr;
    QDial* m_panDial = nullptr;
    QSlider* m_gainLSlider = nullptr;
    QSlider* m_gainRSlider = nullptr;
    ClipLaneWidget* m_clipLane = nullptr;
    CommandStack* m_commandStack = nullptr;
    // Captured on press for the pan dial / gain sliders so a whole drag
    // gesture becomes one undo step instead of one per intermediate value.
    std::optional<TrackState> m_dragBeforeState;
};

} // namespace rsd
