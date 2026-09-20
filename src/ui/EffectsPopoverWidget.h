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

// Small popup shown next to a mixer strip's FX button, in place of the old
// docked EffectsRackPanel. Same effect-chain editing (add/remove/reorder/
// bypass/params) as the dock had, but each effect's parameters start
// collapsed to keep the popup compact; a per-effect toggle expands them.
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

    // Same one-host-at-a-time behavior as EffectsRackPanel.
    void setTrack(std::shared_ptr<Track> track);
    void setMasterBus(MasterBus* masterBus);

signals:
    void effectCountChanged(std::shared_ptr<Track> track);

private:
    void rebuild();
    void addEffectOfType(EffectType type);

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
