#pragma once

#include <QWidget>
#include <memory>

#include "audio/Effects.h"
#include "command/CommandStack.h"
#include "model/Track.h"

class QComboBox;
class QLabel;
class QVBoxLayout;

namespace rsd {

// Dockable panel showing the effect chain (EQ/Compressor/Delay/Reverb) for
// whichever track is currently selected. Mirrors TrackWidgets' pattern for
// undoable edits: sliders push a command on release (not on every
// intermediate value), while the atomic parameter itself is updated live
// during the drag so the audio thread hears the change immediately.
class EffectsRackPanel : public QWidget {
    Q_OBJECT

public:
    explicit EffectsRackPanel(QWidget* parent = nullptr);

    void setCommandStack(CommandStack* stack) { m_commandStack = stack; }
    // Needed to prepare() a newly-added effect's DSP state before it's
    // attached to a live track.
    void setSampleRate(double sampleRate) { m_sampleRate = sampleRate; }

    // Switches which track's chain is displayed; nullptr shows an empty
    // "no track selected" state. Call again (with the same track) after any
    // external edit (e.g. undo/redo) to refresh the displayed values.
    void setTrack(std::shared_ptr<Track> track);
    void refresh();

private:
    void rebuild();
    void addEffectOfType(EffectType type);

    std::shared_ptr<Track> m_track;
    CommandStack* m_commandStack = nullptr;
    double m_sampleRate = 48000.0;

    QLabel* m_trackNameLabel = nullptr;
    QComboBox* m_addTypeCombo = nullptr;
    QWidget* m_slotsContainer = nullptr;
    QVBoxLayout* m_slotsLayout = nullptr;
};

} // namespace rsd
