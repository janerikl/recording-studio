#pragma once

#include <QWidget>
#include <memory>

#include "command/CommandStack.h"
#include "model/Track.h"

class QComboBox;
class QLabel;
class QScrollArea;

namespace rsd {

class PianoRollGridWidget;

// Dockable panel (same pattern as InstrumentPanel/EffectsRackPanel) showing
// an editable piano-roll grid for the currently-selected Instrument track's
// recorded MIDI notes. Shows a placeholder when the selection isn't an
// Instrument track.
class PianoRollPanel : public QWidget {
    Q_OBJECT

public:
    explicit PianoRollPanel(QWidget* parent = nullptr);

    void setTrack(std::shared_ptr<Track> track);
    void setCommandStack(CommandStack* stack);
    void setBpm(double bpm);
    void setSampleRate(int sampleRate);

private:
    std::shared_ptr<Track> m_track;
    QLabel* m_trackNameLabel = nullptr;
    QComboBox* m_snapCombo = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    PianoRollGridWidget* m_grid = nullptr;
};

} // namespace rsd
