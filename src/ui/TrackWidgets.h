#pragma once

#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QWidget>
#include <memory>
#include <vector>

#include "command/CommandStack.h"
#include "command/EditCommands.h"
#include "model/Track.h"
#include "ui/AutomationLaneWidget.h"
#include "ui/ClipLaneWidget.h"
#include "ui/TakeLaneWidget.h"

class QScrollBar;
class QVBoxLayout;

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
    // Sets this row's lane scroll position without re-broadcasting a sync
    // request (used by TimelineView to apply another row's Shift-synced
    // scroll to this one).
    void setLaneScrollOffset(int64_t sampleOffset);
    // Re-reads the track's take lanes (set by the most recent punch/loop
    // recording) and rebuilds the expandable take-lane widgets. Call after
    // any punch/loop recording finishes or is undone/redone.
    void refreshTakeLanes();

signals:
    void selected(std::shared_ptr<Track> track);
    void clipSelectionChanged(std::shared_ptr<Track> track, bool hasSelection);
    // This row's lane was scrolled with Shift held: TimelineView should
    // apply the same absolute sample offset to every other row.
    void syncScrollToAllRequested(int64_t sampleOffset);
    // A take lane was clicked: promote it to the active comp for its region.
    void takeSelected(std::shared_ptr<Track> track, std::shared_ptr<Clip> take);

private:
    void rebuildTakeLanes();


    std::shared_ptr<Track> m_track;
    QRadioButton* m_selectButton = nullptr;
    ClipLaneWidget* m_clipLane = nullptr;
    QScrollBar* m_laneScrollBar = nullptr;
    QPushButton* m_takesToggleButton = nullptr;
    QWidget* m_takeLanesContainer = nullptr;
    QVBoxLayout* m_takeLanesLayout = nullptr;
    std::vector<TakeLaneWidget*> m_takeLaneWidgets;
    QPushButton* m_automationToggleButton = nullptr;
    AutomationLaneWidget* m_automationLane = nullptr;
    CommandStack* m_commandStack = nullptr;
};

} // namespace rsd
