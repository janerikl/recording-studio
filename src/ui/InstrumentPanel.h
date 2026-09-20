#pragma once

#include <QWidget>
#include <memory>

#include "model/Track.h"

class QComboBox;
class QLabel;
class QSlider;

namespace rsd {

class PianoKeyboardWidget;

// Dockable panel for the currently-selected Instrument track: synth
// parameter controls (waveform/ADSR/filter — live-tweaked atomics, not
// undoable, unlike effect params: a v1 simplification) plus the on-screen
// keyboard. Shows a placeholder when the selected track isn't an
// Instrument track.
class InstrumentPanel : public QWidget {
    Q_OBJECT

public:
    explicit InstrumentPanel(QWidget* parent = nullptr);

    void setTrack(std::shared_ptr<Track> track);

signals:
    // Forwarded from the keyboard; MainWindow routes these into the
    // current track's live-note queue and (if armed+recording) capture
    // bookkeeping.
    void noteOn(int pitch, float velocity);
    void noteOff(int pitch);

private:
    void rebuild();

    std::shared_ptr<Track> m_track;
    QLabel* m_trackNameLabel = nullptr;
    QWidget* m_paramsContainer = nullptr;
    QComboBox* m_waveformCombo = nullptr;
    QSlider* m_attackSlider = nullptr;
    QSlider* m_decaySlider = nullptr;
    QSlider* m_sustainSlider = nullptr;
    QSlider* m_releaseSlider = nullptr;
    QSlider* m_filterSlider = nullptr;
    PianoKeyboardWidget* m_keyboard = nullptr;
};

} // namespace rsd
