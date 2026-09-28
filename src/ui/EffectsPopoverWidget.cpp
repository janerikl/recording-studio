#include "EffectsPopoverWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>
#include <functional>

#include "command/EditCommands.h"
#include "ui/PopoverPositioning.h"

namespace rsd {

namespace {

QString effectTypeName(EffectType t) {
    switch (t) {
        case EffectType::EQ: return "EQ";
        case EffectType::Compressor: return "Compressor";
        case EffectType::Delay: return "Delay";
        case EffectType::Reverb: return "Reverb";
        case EffectType::Limiter: return "Limiter";
        case EffectType::Gate: return "Gate";
    }
    return "Effect";
}

// Same undoable-on-release slider binding as EffectsRackPanel's.
void addFloatControl(QVBoxLayout* layout, const QString& labelText, float minV, float maxV,
                      std::function<float()> getter, std::function<void(float)> setter,
                      CommandStack* stack, const QString& cmdText) {
    auto* row = new QWidget;
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);

    auto* label = new QLabel(labelText);
    label->setMinimumWidth(80);
    auto* slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 1000);

    auto toSlider = [minV, maxV](float v) {
        return static_cast<int>((v - minV) / (maxV - minV) * 1000.0f);
    };
    auto fromSlider = [minV, maxV](int v) { return minV + (maxV - minV) * (static_cast<float>(v) / 1000.0f); };

    slider->setValue(toSlider(getter()));

    auto before = std::make_shared<float>(getter());
    QObject::connect(slider, &QSlider::sliderPressed, [before, getter]() { *before = getter(); });
    QObject::connect(slider, &QSlider::valueChanged,
                      [setter, fromSlider](int v) { setter(fromSlider(v)); });
    QObject::connect(slider, &QSlider::sliderReleased, [before, getter, setter, stack, cmdText]() {
        float after = getter();
        if (stack) stack->push(std::make_unique<SetEffectParamCommand<float>>(setter, *before, after, cmdText));
    });

    rowLayout->addWidget(label);
    rowLayout->addWidget(slider);
    layout->addWidget(row);
}

// Adds this effect type's parameter sliders into `paramsLayout`.
void addParamControls(QVBoxLayout* paramsLayout, const std::shared_ptr<Effect>& effect, CommandStack* stack) {
    if (auto* eq = dynamic_cast<EqEffect*>(effect.get())) {
        std::weak_ptr<EqEffect> w = std::static_pointer_cast<EqEffect>(effect);
        addFloatControl(
            paramsLayout, "Low Gain (dB)", -24.0f, 24.0f, [w]() { return w.lock() ? w.lock()->lowGainDb.load() : 0.0f; },
            [w](float v) { if (auto e = w.lock()) e->lowGainDb.store(v); }, stack, "Set Low Gain");
        addFloatControl(
            paramsLayout, "Mid Gain (dB)", -24.0f, 24.0f, [w]() { return w.lock() ? w.lock()->midGainDb.load() : 0.0f; },
            [w](float v) { if (auto e = w.lock()) e->midGainDb.store(v); }, stack, "Set Mid Gain");
        addFloatControl(
            paramsLayout, "Mid Freq (Hz)", 200.0f, 8000.0f,
            [w]() { return w.lock() ? w.lock()->midFreqHz.load() : 1000.0f; },
            [w](float v) { if (auto e = w.lock()) e->midFreqHz.store(v); }, stack, "Set Mid Freq");
        addFloatControl(
            paramsLayout, "High Gain (dB)", -24.0f, 24.0f,
            [w]() { return w.lock() ? w.lock()->highGainDb.load() : 0.0f; },
            [w](float v) { if (auto e = w.lock()) e->highGainDb.store(v); }, stack, "Set High Gain");
        (void)eq;
    } else if (auto* comp = dynamic_cast<CompressorEffect*>(effect.get())) {
        std::weak_ptr<CompressorEffect> w = std::static_pointer_cast<CompressorEffect>(effect);
        addFloatControl(
            paramsLayout, "Threshold (dB)", -60.0f, 0.0f,
            [w]() { return w.lock() ? w.lock()->thresholdDb.load() : -18.0f; },
            [w](float v) { if (auto e = w.lock()) e->thresholdDb.store(v); }, stack, "Set Threshold");
        addFloatControl(
            paramsLayout, "Ratio", 1.0f, 20.0f, [w]() { return w.lock() ? w.lock()->ratio.load() : 4.0f; },
            [w](float v) { if (auto e = w.lock()) e->ratio.store(v); }, stack, "Set Ratio");
        addFloatControl(
            paramsLayout, "Attack (ms)", 0.1f, 200.0f,
            [w]() { return w.lock() ? w.lock()->attackMs.load() : 10.0f; },
            [w](float v) { if (auto e = w.lock()) e->attackMs.store(v); }, stack, "Set Attack");
        addFloatControl(
            paramsLayout, "Release (ms)", 10.0f, 1000.0f,
            [w]() { return w.lock() ? w.lock()->releaseMs.load() : 100.0f; },
            [w](float v) { if (auto e = w.lock()) e->releaseMs.store(v); }, stack, "Set Release");
        (void)comp;
    } else if (auto* delay = dynamic_cast<DelayEffect*>(effect.get())) {
        std::weak_ptr<DelayEffect> w = std::static_pointer_cast<DelayEffect>(effect);
        addFloatControl(
            paramsLayout, "Delay (ms)", 1.0f, 1500.0f,
            [w]() { return w.lock() ? w.lock()->delayMs.load() : 300.0f; },
            [w](float v) { if (auto e = w.lock()) e->delayMs.store(v); }, stack, "Set Delay Time");
        addFloatControl(
            paramsLayout, "Feedback", 0.0f, 0.95f,
            [w]() { return w.lock() ? w.lock()->feedback.load() : 0.35f; },
            [w](float v) { if (auto e = w.lock()) e->feedback.store(v); }, stack, "Set Delay Feedback");
        addFloatControl(
            paramsLayout, "Mix", 0.0f, 1.0f, [w]() { return w.lock() ? w.lock()->mix.load() : 0.3f; },
            [w](float v) { if (auto e = w.lock()) e->mix.store(v); }, stack, "Set Delay Mix");
        (void)delay;
    } else if (auto* reverb = dynamic_cast<ReverbEffect*>(effect.get())) {
        std::weak_ptr<ReverbEffect> w = std::static_pointer_cast<ReverbEffect>(effect);
        addFloatControl(
            paramsLayout, "Room Size", 0.0f, 1.0f,
            [w]() { return w.lock() ? w.lock()->roomSize.load() : 0.5f; },
            [w](float v) { if (auto e = w.lock()) e->roomSize.store(v); }, stack, "Set Room Size");
        addFloatControl(
            paramsLayout, "Damping", 0.0f, 1.0f, [w]() { return w.lock() ? w.lock()->damping.load() : 0.5f; },
            [w](float v) { if (auto e = w.lock()) e->damping.store(v); }, stack, "Set Damping");
        addFloatControl(
            paramsLayout, "Mix", 0.0f, 1.0f, [w]() { return w.lock() ? w.lock()->mix.load() : 0.25f; },
            [w](float v) { if (auto e = w.lock()) e->mix.store(v); }, stack, "Set Reverb Mix");
        (void)reverb;
    } else if (auto* lim = dynamic_cast<LimiterEffect*>(effect.get())) {
        std::weak_ptr<LimiterEffect> w = std::static_pointer_cast<LimiterEffect>(effect);
        addFloatControl(
            paramsLayout, "Ceiling (dB)", -12.0f, 0.0f,
            [w]() { return w.lock() ? w.lock()->ceilingDb.load() : -0.3f; },
            [w](float v) { if (auto e = w.lock()) e->ceilingDb.store(v); }, stack, "Set Limiter Ceiling");
        addFloatControl(
            paramsLayout, "Release (ms)", 10.0f, 500.0f,
            [w]() { return w.lock() ? w.lock()->releaseMs.load() : 50.0f; },
            [w](float v) { if (auto e = w.lock()) e->releaseMs.store(v); }, stack, "Set Limiter Release");
        (void)lim;
    } else if (auto* gate = dynamic_cast<NoiseGateEffect*>(effect.get())) {
        std::weak_ptr<NoiseGateEffect> w = std::static_pointer_cast<NoiseGateEffect>(effect);
        addFloatControl(
            paramsLayout, "Threshold (dB)", -80.0f, 0.0f,
            [w]() { return w.lock() ? w.lock()->thresholdDb.load() : -40.0f; },
            [w](float v) { if (auto e = w.lock()) e->thresholdDb.store(v); }, stack, "Set Gate Threshold");
        addFloatControl(
            paramsLayout, "Attack (ms)", 0.1f, 100.0f,
            [w]() { return w.lock() ? w.lock()->attackMs.load() : 1.0f; },
            [w](float v) { if (auto e = w.lock()) e->attackMs.store(v); }, stack, "Set Gate Attack");
        addFloatControl(
            paramsLayout, "Hold (ms)", 0.0f, 500.0f,
            [w]() { return w.lock() ? w.lock()->holdMs.load() : 50.0f; },
            [w](float v) { if (auto e = w.lock()) e->holdMs.store(v); }, stack, "Set Gate Hold");
        addFloatControl(
            paramsLayout, "Release (ms)", 10.0f, 1000.0f,
            [w]() { return w.lock() ? w.lock()->releaseMs.load() : 100.0f; },
            [w](float v) { if (auto e = w.lock()) e->releaseMs.store(v); }, stack, "Set Gate Release");
        (void)gate;
    }
}

} // namespace

EffectsPopoverWidget::EffectsPopoverWidget(QWidget* parent) : QWidget(parent, Qt::Popup) {
    setAttribute(Qt::WA_DeleteOnClose, false);
    setFixedWidth(280);
    setStyleSheet(
        "EffectsPopoverWidget { background: #1e1e1e; border: 1px solid #4a4a4a; }"
        "QGroupBox { background: #2a2a2a; color: #e0e0e0; border: 1px solid #3c3c3c; "
        "border-radius: 4px; margin-top: 8px; font-weight: bold; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QLabel { color: #cccccc; }"
        "QPushButton { background: #3a3a3a; color: #e0e0e0; border: 1px solid #4a4a4a; "
        "border-radius: 3px; padding: 2px 8px; }"
        "QPushButton:hover { background: #454545; }"
        "QComboBox { background: #2a2a2a; color: #e0e0e0; border: 1px solid #4a4a4a; }");

    auto* outer = new QVBoxLayout(this);

    m_trackNameLabel = new QLabel("No track selected");
    m_trackNameLabel->setStyleSheet("font-weight: bold; color: #f0f0f0;");
    outer->addWidget(m_trackNameLabel);

    auto* addRow = new QWidget;
    auto* addRowLayout = new QHBoxLayout(addRow);
    addRowLayout->setContentsMargins(0, 0, 0, 0);
    m_addTypeCombo = new QComboBox;
    m_addTypeCombo->addItem("EQ", static_cast<int>(EffectType::EQ));
    m_addTypeCombo->addItem("Compressor", static_cast<int>(EffectType::Compressor));
    m_addTypeCombo->addItem("Delay", static_cast<int>(EffectType::Delay));
    m_addTypeCombo->addItem("Reverb", static_cast<int>(EffectType::Reverb));
    m_addTypeCombo->addItem("Limiter", static_cast<int>(EffectType::Limiter));
    m_addTypeCombo->addItem("Gate", static_cast<int>(EffectType::Gate));
    auto* addButton = new QPushButton("Add");
    connect(addButton, &QPushButton::clicked, this, [this]() {
        addEffectOfType(static_cast<EffectType>(m_addTypeCombo->currentData().toInt()));
    });
    addRowLayout->addWidget(m_addTypeCombo, 1);
    addRowLayout->addWidget(addButton);
    outer->addWidget(addRow);

    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    m_scroll->setFixedHeight(180);
    m_slotsContainer = new QWidget;
    m_slotsLayout = new QVBoxLayout(m_slotsContainer);
    m_slotsLayout->addStretch();
    m_scroll->setWidget(m_slotsContainer);
    outer->addWidget(m_scroll, 1);

    setTrack(nullptr);
}

void EffectsPopoverWidget::showAt(std::shared_ptr<Track> track, QRect globalAnchorRect) {
    m_anchorGlobalRect = globalAnchorRect;
    setTrack(std::move(track));  // triggers rebuild(), which repositions.
    show();
}

void EffectsPopoverWidget::showMasterAt(MasterBus* masterBus, QRect globalAnchorRect) {
    m_anchorGlobalRect = globalAnchorRect;
    setMasterBus(masterBus);  // triggers rebuild(), which repositions.
    show();
}

void EffectsPopoverWidget::repositionAndResize() {
    // The scroll area has a fixed height (set in the constructor), so the
    // popup's overall size is stable; this just repositions it in case the
    // anchor moved or the popup wouldn't otherwise fit on screen.
    QRect available = this->screen() ? this->screen()->availableGeometry() : QRect(0, 0, 1920, 1080);
    move(computePopoverPosition(m_anchorGlobalRect, sizeHint(), available));
}

void EffectsPopoverWidget::setTrack(std::shared_ptr<Track> track) {
    m_track = std::move(track);
    m_masterBus = nullptr;
    rebuild();
}

void EffectsPopoverWidget::setMasterBus(MasterBus* masterBus) {
    m_track.reset();
    m_masterBus = masterBus;
    rebuild();
}

std::shared_ptr<const EffectChain> EffectsPopoverWidget::currentChain() const {
    if (m_track) return m_track->effectsSnapshot();
    if (m_masterBus) return m_masterBus->effectsSnapshot();
    return nullptr;
}

void EffectsPopoverWidget::addEffectToHost(std::shared_ptr<Effect> effect) {
    if (m_track) m_track->addEffect(std::move(effect));
    else if (m_masterBus) m_masterBus->addEffect(std::move(effect));
}

void EffectsPopoverWidget::removeEffectFromHost(const QUuid& effectId) {
    if (m_track) m_track->removeEffect(effectId);
    else if (m_masterBus) m_masterBus->removeEffect(effectId);
}

void EffectsPopoverWidget::moveEffectInHost(const QUuid& effectId, int newIndex) {
    if (m_track) m_track->moveEffect(effectId, newIndex);
    else if (m_masterBus) m_masterBus->moveEffect(effectId, newIndex);
}

void EffectsPopoverWidget::pushChainCommand(std::shared_ptr<const EffectChain> before,
                                             std::shared_ptr<const EffectChain> after, const QString& text) {
    if (!m_commandStack) return;
    if (m_track) {
        m_commandStack->push(std::make_unique<EffectChainCommand>(m_track, before, after, text));
    } else if (m_masterBus) {
        MasterBus* bus = m_masterBus;
        auto restore = [bus](std::shared_ptr<const EffectChain> chain) { bus->restoreEffects(chain); };
        m_commandStack->push(std::make_unique<SetEffectChainCommand>(restore, before, after, text));
    }
}

void EffectsPopoverWidget::addEffectOfType(EffectType type) {
    if (!m_track && !m_masterBus) return;

    std::shared_ptr<Effect> effect;
    switch (type) {
        case EffectType::EQ: effect = std::make_shared<EqEffect>(); break;
        case EffectType::Compressor: effect = std::make_shared<CompressorEffect>(); break;
        case EffectType::Delay: effect = std::make_shared<DelayEffect>(); break;
        case EffectType::Reverb: effect = std::make_shared<ReverbEffect>(); break;
        case EffectType::Limiter: effect = std::make_shared<LimiterEffect>(); break;
        case EffectType::Gate: effect = std::make_shared<NoiseGateEffect>(); break;
    }
    effect->prepare(m_sampleRate);

    auto before = currentChain();
    addEffectToHost(effect);
    auto after = currentChain();
    pushChainCommand(before, after, "Add " + effectTypeName(type));
    rebuild();
    if (m_track) emit effectCountChanged(m_track);
}

void EffectsPopoverWidget::rebuild() {
    // Clear all slot widgets except the trailing stretch.
    while (m_slotsLayout->count() > 1) {
        QLayoutItem* item = m_slotsLayout->takeAt(0);
        delete item->widget();
        delete item;
    }

    if (!m_track && !m_masterBus) {
        m_trackNameLabel->setText("No track selected");
        m_addTypeCombo->setEnabled(false);
        return;
    }
    m_trackNameLabel->setText(m_masterBus       ? "Master"
                               : m_track->name.isEmpty() ? "Track"
                                                          : m_track->name);
    m_addTypeCombo->setEnabled(true);

    auto chain = currentChain();
    for (size_t i = 0; i < chain->size(); ++i) {
        auto effect = chain->at(i);
        QUuid effectId = effect->id;

        auto* box = new QGroupBox(effectTypeName(effect->type()));
        auto* boxLayout = new QVBoxLayout(box);

        auto* headerRow = new QWidget;
        auto* headerLayout = new QHBoxLayout(headerRow);
        headerLayout->setContentsMargins(0, 0, 0, 0);

        auto* bypassBox = new QCheckBox("Bypass");
        bypassBox->setChecked(effect->bypassed.load());
        std::weak_ptr<Effect> weakEffect = effect;
        connect(bypassBox, &QCheckBox::toggled, this, [this, weakEffect](bool checked) {
            auto e = weakEffect.lock();
            if (!e) return;
            bool before = !checked;
            auto setter = [weakEffect](bool v) {
                if (auto ee = weakEffect.lock()) ee->bypassed.store(v);
            };
            e->bypassed.store(checked);
            if (m_commandStack) {
                m_commandStack->push(std::make_unique<SetEffectParamCommand<bool>>(
                    setter, before, checked, "Toggle Bypass"));
            }
        });
        headerLayout->addWidget(bypassBox);
        headerLayout->addStretch();

        if (i > 0) {
            auto* upButton = new QPushButton("Up");
            connect(upButton, &QPushButton::clicked, this, [this, effectId, i]() {
                auto before = currentChain();
                moveEffectInHost(effectId, static_cast<int>(i) - 1);
                auto after = currentChain();
                pushChainCommand(before, after, "Reorder Effects");
                rebuild();
            });
            headerLayout->addWidget(upButton);
        }
        if (i + 1 < chain->size()) {
            auto* downButton = new QPushButton("Down");
            connect(downButton, &QPushButton::clicked, this, [this, effectId, i]() {
                auto before = currentChain();
                moveEffectInHost(effectId, static_cast<int>(i) + 1);
                auto after = currentChain();
                pushChainCommand(before, after, "Reorder Effects");
                rebuild();
            });
            headerLayout->addWidget(downButton);
        }

        auto* removeButton = new QPushButton("Remove");
        connect(removeButton, &QPushButton::clicked, this, [this, effectId]() {
            auto before = currentChain();
            removeEffectFromHost(effectId);
            auto after = currentChain();
            pushChainCommand(before, after, "Remove Effect");
            rebuild();
            if (m_track) emit effectCountChanged(m_track);
        });
        headerLayout->addWidget(removeButton);
        boxLayout->addWidget(headerRow);

        // Params are shown immediately (no expand/collapse step) so the
        // whole chain is editable as soon as the popover opens.
        addParamControls(boxLayout, effect, m_commandStack);

        m_slotsLayout->insertWidget(static_cast<int>(i), box);
    }

    repositionAndResize();
}

} // namespace rsd
