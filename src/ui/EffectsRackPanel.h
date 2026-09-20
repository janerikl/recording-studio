#pragma once

#include <QWidget>
#include <memory>

#include "audio/Effects.h"
#include "command/CommandStack.h"
#include "model/MasterBus.h"
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
    // Clears any master-bus selection (the panel shows one host at a time).
    void setTrack(std::shared_ptr<Track> track);
    // Switches the panel to show the session's master bus chain instead of
    // a track's. Non-owning: Session (and its MasterBus) outlives this
    // panel for the app's whole lifetime. Clears any track selection.
    void setMasterBus(MasterBus* masterBus);
    void refresh();

signals:
    // An effect was added to or removed from the currently displayed
    // track's chain (reordering/param edits don't change the count, so
    // they don't need this).
    void effectCountChanged(std::shared_ptr<Track> track);

private:
    void rebuild();
    void addEffectOfType(EffectType type);

    // Effect-chain source/host: exactly one of these is active at a time.
    // Small helpers below dispatch to whichever is set instead of
    // duplicating the whole add/remove/reorder/rebuild UI per host type.
    std::shared_ptr<const EffectChain> currentChain() const;
    void addEffectToHost(std::shared_ptr<Effect> effect);
    void removeEffectFromHost(const QUuid& effectId);
    void moveEffectInHost(const QUuid& effectId, int newIndex);
    void pushChainCommand(std::shared_ptr<const EffectChain> before,
                           std::shared_ptr<const EffectChain> after, const QString& text);

    std::shared_ptr<Track> m_track;
    MasterBus* m_masterBus = nullptr;
    CommandStack* m_commandStack = nullptr;
    double m_sampleRate = 48000.0;

    QLabel* m_trackNameLabel = nullptr;
    QComboBox* m_addTypeCombo = nullptr;
    QWidget* m_slotsContainer = nullptr;
    QVBoxLayout* m_slotsLayout = nullptr;
};

} // namespace rsd
