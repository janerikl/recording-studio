#pragma once

#include <QRect>
#include <QWidget>
#include <memory>

#include "audio/Effects.h"
#include "command/CommandStack.h"
#include "model/MasterBus.h"
#include "model/Track.h"

class QComboBox;
class QLabel;
class QScrollArea;
class QVBoxLayout;

namespace rsd {

// Small popup shown next to a mixer strip's FX button, in place of the old
// docked EffectsRackPanel. Same effect-chain editing (add/remove/reorder/
// bypass/params) as the dock had; every effect's parameters are shown
// immediately, no extra click needed to reveal them.
//
// Lives as Qt::Popup: shows near the button that requested it and closes
// itself on an outside click, so callers just position + show() it rather
// than managing visibility.
class EffectsPopoverWidget : public QWidget {
    Q_OBJECT

public:
    explicit EffectsPopoverWidget(QWidget* parent = nullptr);

    void setCommandStack(CommandStack* stack) { m_commandStack = stack; }
    void setSampleRate(double sampleRate) { m_sampleRate = sampleRate; }

    // Same one-host-at-a-time behavior as EffectsRackPanel. Prefer showAt()
    // over calling these directly, which also (re)positions the popup.
    void setTrack(std::shared_ptr<Track> track);
    void setMasterBus(MasterBus* masterBus);

    // Shows the track's effect chain in a popup anchored below
    // `globalAnchorRect` (the FX button's geometry in global screen
    // coordinates). Repositions/resizes itself as needed if the chain is
    // later expanded/collapsed or effects are added/removed while open.
    void showAt(std::shared_ptr<Track> track, QRect globalAnchorRect);
    void showMasterAt(MasterBus* masterBus, QRect globalAnchorRect);

signals:
    void effectCountChanged(std::shared_ptr<Track> track);

private:
    void rebuild();
    void addEffectOfType(EffectType type);
    void repositionAndResize();

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
    QScrollArea* m_scroll = nullptr;
    QWidget* m_slotsContainer = nullptr;
    QVBoxLayout* m_slotsLayout = nullptr;

    QRect m_anchorGlobalRect;
};

} // namespace rsd
