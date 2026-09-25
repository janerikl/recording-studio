#pragma once

#include <QWidget>
#include <memory>

#include "model/Track.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QStackedWidget;

namespace rsd {

class PianoKeyboardWidget;
class DrumPadWidget;
class PracticePanel;

// Dockable panel for the currently-selected Instrument track: instrument
// selection (GM program combo for melodic tracks, or a Drum Kit toggle)
// plus the matching on-screen player (piano keyboard or drum pads). Shows
// a placeholder when the selected track isn't an Instrument track.
class InstrumentPanel : public QWidget {
    Q_OBJECT

public:
    explicit InstrumentPanel(QWidget* parent = nullptr);

    void setTrack(std::shared_ptr<Track> track);

signals:
    // Forwarded from whichever player widget is active; MainWindow routes
    // these into the current track's live-note queue and (if
    // armed+recording) capture bookkeeping.
    void noteOn(int pitch, float velocity);
    void noteOff(int pitch);

private:
    void rebuild();

    std::shared_ptr<Track> m_track;
    QLabel* m_trackNameLabel = nullptr;
    QWidget* m_paramsContainer = nullptr;
    QCheckBox* m_drumKitCheck = nullptr;
    QComboBox* m_instrumentCombo = nullptr;
    QStackedWidget* m_playerStack = nullptr;
    PianoKeyboardWidget* m_keyboard = nullptr;
    DrumPadWidget* m_drumPads = nullptr;
    PracticePanel* m_practicePanel = nullptr;
};

} // namespace rsd
