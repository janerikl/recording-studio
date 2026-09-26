#include "MixerPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include "ui/LevelMeterWidget.h"
#include "ui/MixerStripWidget.h"
#include "ui/SoloExclusivityMath.h"

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
    masterStrip->setFixedWidth(110); // wider than MixerStripWidget's 90px, to read as visually distinct
    masterStrip->setAutoFillBackground(true);
    QPalette masterPalette = masterStrip->palette();
    masterPalette.setColor(QPalette::Window, QColor(52, 52, 58)); // slightly lighter than track strips
    masterStrip->setPalette(masterPalette);
    auto* masterLayout = new QVBoxLayout(masterStrip);
    masterLayout->setContentsMargins(2, 4, 2, 4);
    masterLayout->setSpacing(2); // matches MixerStripWidget's tight spacing between its controls
    // AlignTop matters here for the same reason MixerStripWidget pins its
    // layout: the master strip's content is shorter than a track strip's
    // (no name/arm/input/pan), so without it Qt vertically centers the
    // whole thing within the strip's full stretched height, pushing every
    // control down out of alignment with the track strips beside it.
    masterLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    auto* masterLabel = new QLabel("Master", masterStrip);
    masterLabel->setAlignment(Qt::AlignCenter);
    QFont masterFont = masterLabel->font();
    masterFont.setBold(true);
    masterLabel->setFont(masterFont);
    masterLayout->addWidget(masterLabel);
    masterLayout->addSpacing(10); // visual gap between the title and FX, matching track strips' breathing room

    m_masterFxButton = new QPushButton("FX", masterStrip);
    m_masterFxButton->setToolTip("Show effects for the master bus");
    connect(m_masterFxButton, &QPushButton::clicked, this, [this]() {
        QRect anchorRect(m_masterFxButton->mapToGlobal(QPoint(0, 0)), m_masterFxButton->size());
        emit masterEffectsPanelRequested(anchorRect);
    });
    masterLayout->addWidget(m_masterFxButton);

    m_masterVolumeSlider = new QSlider(Qt::Vertical, masterStrip);
    m_masterVolumeSlider->setRange(0, 200);
    m_masterVolumeSlider->setValue(100);
    m_masterVolumeSlider->setToolTip("Master Volume");
    // No fixed height here (unlike the per-track strips' 120px sliders):
    // the master strip has far fewer controls above the fader, so letting
    // the slider/meter pair expand via faderRow's stretch factor below
    // fills that extra vertical space instead of leaving it empty.
    connect(m_masterVolumeSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_masterBus) m_masterBus->volume.store(static_cast<float>(value) / 100.0f);
    });

    m_masterLevelMeter = new LevelMeterWidget(LevelMeterWidget::Orientation::Vertical, masterStrip);
    m_masterLevelMeter->setMinimumHeight(60);

    auto* faderRow = new QHBoxLayout();
    faderRow->setSpacing(4);
    faderRow->addWidget(m_masterVolumeSlider);
    faderRow->addWidget(m_masterLevelMeter);
    masterLayout->addLayout(faderRow, 1);
    masterLayout->addSpacing(8); // bottom margin so the slider/meter visibly stop short of the strip's edge

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
    connect(strip, &MixerStripWidget::soloToggled, this, &MixerPanel::handleSoloToggled);

    m_stripsLayout->insertWidget(m_stripsLayout->count() - 1, strip);
    m_strips[track->id.toString()] = strip;

    refreshSendBusOptions();
}

void MixerPanel::handleSoloToggled(std::shared_ptr<Track> track, bool checked) {
    if (!checked) return;

    QVector<QString> soloedIds;
    for (auto& [id, strip] : m_strips) {
        if (strip->track()->soloed.load()) soloedIds.push_back(id);
    }

    for (const auto& id : tracksToUnsolo(soloedIds, track->id.toString())) {
        auto it = m_strips.find(id);
        if (it != m_strips.end()) it->second->setSoloChecked(false);
    }
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

void MixerPanel::updateMeters(float masterPeakL, float masterPeakR) {
    for (auto& [id, strip] : m_strips) strip->updateMeter();
    m_masterLevelMeter->setLevels(masterPeakL, masterPeakR);
}

} // namespace rsd
