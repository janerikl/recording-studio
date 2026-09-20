#include "MixerPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include "ui/MixerStripWidget.h"

namespace rsd {

MixerPanel::MixerPanel(QWidget* parent) : QWidget(parent) {
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* stripsContainer = new QWidget(scroll);
    m_stripsLayout = new QHBoxLayout(stripsContainer);
    m_stripsLayout->setContentsMargins(2, 2, 2, 2);
    m_stripsLayout->setSpacing(2);
    m_stripsLayout->addStretch();
    scroll->setWidget(stripsContainer);
    outer->addWidget(scroll, 1);

    // Master strip: fixed outside the scroll area so it's never scrolled
    // out of view alongside many track strips.
    auto* masterStrip = new QWidget(this);
    masterStrip->setFixedWidth(70);
    auto* masterLayout = new QVBoxLayout(masterStrip);
    masterLayout->setContentsMargins(2, 4, 2, 4);
    masterLayout->setAlignment(Qt::AlignHCenter);

    auto* masterLabel = new QLabel("Master", masterStrip);
    masterLabel->setAlignment(Qt::AlignCenter);
    masterLayout->addWidget(masterLabel);

    m_masterFxButton = new QPushButton("FX", masterStrip);
    m_masterFxButton->setToolTip("Show effects for the master bus");
    connect(m_masterFxButton, &QPushButton::clicked, this,
            [this]() { emit masterEffectsPanelRequested(); });
    masterLayout->addWidget(m_masterFxButton);

    m_masterVolumeSlider = new QSlider(Qt::Vertical, masterStrip);
    m_masterVolumeSlider->setRange(0, 200);
    m_masterVolumeSlider->setValue(100);
    m_masterVolumeSlider->setToolTip("Master Volume");
    m_masterVolumeSlider->setFixedHeight(120);
    connect(m_masterVolumeSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_masterBus) m_masterBus->volume.store(static_cast<float>(value) / 100.0f);
    });
    masterLayout->addWidget(m_masterVolumeSlider, 1, Qt::AlignHCenter);

    outer->addWidget(masterStrip);
}

void MixerPanel::setCommandStack(CommandStack* stack) {
    m_commandStack = stack;
    for (auto& [id, strip] : m_strips) strip->setCommandStack(stack);
}

void MixerPanel::setMasterBus(MasterBus* masterBus) {
    m_masterBus = masterBus;
    if (m_masterBus) {
        QSignalBlocker blocker(m_masterVolumeSlider);
        m_masterVolumeSlider->setValue(static_cast<int>(m_masterBus->volume.load() * 100.0f));
    }
}

void MixerPanel::addTrack(std::shared_ptr<Track> track) {
    auto* strip = new MixerStripWidget(track, this);
    strip->setCommandStack(m_commandStack);
    connect(strip, &MixerStripWidget::selected, this, &MixerPanel::trackSelected);
    connect(strip, &MixerStripWidget::effectsPanelRequested, this, &MixerPanel::effectsPanelRequested);

    m_stripsLayout->insertWidget(m_stripsLayout->count() - 1, strip);
    m_strips[track->id.toString()] = strip;

    refreshSendBusOptions();
}

void MixerPanel::removeTrack(const QUuid& trackId) {
    auto it = m_strips.find(trackId.toString());
    if (it == m_strips.end()) return;

    m_stripsLayout->removeWidget(it->second);
    it->second->deleteLater();
    m_strips.erase(it);

    refreshSendBusOptions();
}

void MixerPanel::clear() {
    for (auto& [id, strip] : m_strips) strip->deleteLater();
    m_strips.clear();
}

void MixerPanel::refreshTrackEffectsButton(const QUuid& trackId) {
    auto it = m_strips.find(trackId.toString());
    if (it != m_strips.end()) it->second->refreshEffectsButton();
}

void MixerPanel::refreshSendBusOptions() {
    std::vector<std::shared_ptr<Track>> busTracks;
    for (auto& [id, strip] : m_strips) {
        if (strip->track()->kind == TrackKind::Bus) busTracks.push_back(strip->track());
    }
    for (auto& [id, strip] : m_strips) strip->refreshSendBusOptions(busTracks);
}

} // namespace rsd
