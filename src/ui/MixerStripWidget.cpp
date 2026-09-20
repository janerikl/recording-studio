#include "MixerStripWidget.h"

#include <QVBoxLayout>

#include "ui/TrackEffectsLabel.h"

namespace rsd {

MixerStripWidget::MixerStripWidget(std::shared_ptr<Track> track, QWidget* parent)
    : QWidget(parent), m_track(std::move(track)) {
    setFixedWidth(70);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 4, 2, 4);
    layout->setSpacing(2);
    layout->setAlignment(Qt::AlignHCenter);

    m_nameLabel = new QLabel(m_track->name, this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(true);
    layout->addWidget(m_nameLabel);

    m_effectsButton = new QPushButton(
        QString::fromStdString(formatEffectsButtonLabel(m_track->effectsSnapshot()->size())), this);
    m_effectsButton->setToolTip("Show effects for this track");
    connect(m_effectsButton, &QPushButton::clicked, this, [this]() {
        emit selected(m_track);
        emit effectsPanelRequested(m_track);
    });
    layout->addWidget(m_effectsButton);

    m_armBox = new QCheckBox("Arm", this);
    m_armBox->setChecked(m_track->recordArmed.load());
    connect(m_armBox, &QCheckBox::toggled, this, [this](bool checked) {
        TrackState before = TrackState::capture(*m_track);
        m_track->recordArmed.store(checked, std::memory_order_relaxed);
        if (m_commandStack) {
            m_commandStack->push(std::make_unique<TrackStateCommand>(
                m_track, before, TrackState::capture(*m_track), "Arm Track"));
        }
    });
    layout->addWidget(m_armBox);

    m_sourceCombo = new QComboBox(this);
    m_sourceCombo->addItem("Mic", QVariant::fromValue(static_cast<int>(AudioSource::Mic)));
    m_sourceCombo->addItem("Sys", QVariant::fromValue(static_cast<int>(AudioSource::SystemAudio)));
    m_sourceCombo->setToolTip("Input source");
    m_sourceCombo->setCurrentIndex(m_track->inputSource.load() == AudioSource::SystemAudio ? 1 : 0);
    connect(m_sourceCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        TrackState before = TrackState::capture(*m_track);
        m_track->inputSource.store(index == 1 ? AudioSource::SystemAudio : AudioSource::Mic);
        if (m_commandStack) {
            m_commandStack->push(std::make_unique<TrackStateCommand>(
                m_track, before, TrackState::capture(*m_track), "Change Track Input Source"));
        }
    });
    layout->addWidget(m_sourceCombo);

    m_panDial = new QDial(this);
    m_panDial->setRange(-100, 100);
    m_panDial->setValue(static_cast<int>(m_track->pan.load() * 100));
    m_panDial->setToolTip("Pan");
    m_panDial->setFixedSize(40, 40);
    connect(m_panDial, &QDial::sliderPressed, this,
            [this]() { m_dragBeforeState = TrackState::capture(*m_track); });
    connect(m_panDial, &QDial::valueChanged, this,
            [this](int v) { m_track->pan.store(v / 100.0f, std::memory_order_relaxed); });
    connect(m_panDial, &QDial::sliderReleased, this, [this]() {
        if (m_commandStack && m_dragBeforeState) {
            m_commandStack->push(std::make_unique<TrackStateCommand>(
                m_track, *m_dragBeforeState, TrackState::capture(*m_track), "Pan Track"));
        }
        m_dragBeforeState.reset();
    });
    layout->addWidget(m_panDial, 0, Qt::AlignHCenter);

    m_volumeSlider = new QSlider(Qt::Vertical, this);
    m_volumeSlider->setRange(0, 200);
    m_volumeSlider->setValue(static_cast<int>(m_track->volume.load() * 100));
    m_volumeSlider->setToolTip("Volume");
    m_volumeSlider->setFixedHeight(120);
    connect(m_volumeSlider, &QSlider::sliderPressed, this,
            [this]() { m_dragBeforeState = TrackState::capture(*m_track); });
    connect(m_volumeSlider, &QSlider::valueChanged, this,
            [this](int v) { m_track->volume.store(v / 100.0f, std::memory_order_relaxed); });
    connect(m_volumeSlider, &QSlider::sliderReleased, this, [this]() {
        if (m_commandStack && m_dragBeforeState) {
            m_commandStack->push(std::make_unique<TrackStateCommand>(
                m_track, *m_dragBeforeState, TrackState::capture(*m_track), "Volume"));
        }
        m_dragBeforeState.reset();
    });
    layout->addWidget(m_volumeSlider, 1, Qt::AlignHCenter);

    m_muteBox = new QCheckBox("Mute", this);
    m_muteBox->setChecked(m_track->muted.load());
    connect(m_muteBox, &QCheckBox::toggled, this, [this](bool checked) {
        TrackState before = TrackState::capture(*m_track);
        m_track->muted.store(checked, std::memory_order_relaxed);
        if (m_commandStack) {
            m_commandStack->push(std::make_unique<TrackStateCommand>(
                m_track, before, TrackState::capture(*m_track), "Mute Track"));
        }
    });
    layout->addWidget(m_muteBox);

    m_soloBox = new QCheckBox("Solo", this);
    m_soloBox->setChecked(m_track->soloed.load());
    connect(m_soloBox, &QCheckBox::toggled, this, [this](bool checked) {
        TrackState before = TrackState::capture(*m_track);
        m_track->soloed.store(checked, std::memory_order_relaxed);
        if (m_commandStack) {
            m_commandStack->push(std::make_unique<TrackStateCommand>(
                m_track, before, TrackState::capture(*m_track), "Solo Track"));
        }
    });
    layout->addWidget(m_soloBox);

    // Aux send: post-fader tap to a Bus track. Buses don't send to other
    // buses, so a Bus track's own strip skips this control entirely.
    if (m_track->kind != TrackKind::Bus) {
        m_sendBusCombo = new QComboBox(this);
        m_sendBusCombo->addItem("No Send", QString());
        m_sendBusCombo->setToolTip("Send to Bus");
        connect(m_sendBusCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
            TrackState before = TrackState::capture(*m_track);
            m_track->setSendBusId(QUuid(m_sendBusCombo->itemData(index).toString()));
            if (m_commandStack) {
                m_commandStack->push(std::make_unique<TrackStateCommand>(
                    m_track, before, TrackState::capture(*m_track), "Change Track Send"));
            }
        });
        layout->addWidget(m_sendBusCombo);

        m_sendLevelSlider = new QSlider(Qt::Horizontal, this);
        m_sendLevelSlider->setRange(0, 100);
        m_sendLevelSlider->setValue(static_cast<int>(m_track->sendLevel.load() * 100));
        m_sendLevelSlider->setToolTip("Send Level");
        connect(m_sendLevelSlider, &QSlider::sliderPressed, this,
                [this]() { m_dragBeforeState = TrackState::capture(*m_track); });
        connect(m_sendLevelSlider, &QSlider::valueChanged, this, [this](int v) {
            m_track->sendLevel.store(v / 100.0f, std::memory_order_relaxed);
        });
        connect(m_sendLevelSlider, &QSlider::sliderReleased, this, [this]() {
            if (m_commandStack && m_dragBeforeState) {
                m_commandStack->push(std::make_unique<TrackStateCommand>(
                    m_track, *m_dragBeforeState, TrackState::capture(*m_track), "Send Level"));
            }
            m_dragBeforeState.reset();
        });
        layout->addWidget(m_sendLevelSlider);
    }
}

void MixerStripWidget::refreshEffectsButton() {
    m_effectsButton->setText(
        QString::fromStdString(formatEffectsButtonLabel(m_track->effectsSnapshot()->size())));
}

void MixerStripWidget::refreshSendBusOptions(const std::vector<std::shared_ptr<Track>>& busTracks) {
    if (!m_sendBusCombo) return; // this strip's own track is a Bus; no send control

    QString currentId = m_track->sendBusId().toString();
    m_sendBusCombo->blockSignals(true);
    m_sendBusCombo->clear();
    m_sendBusCombo->addItem("No Send", QString());
    int selectIndex = 0;
    for (auto& bus : busTracks) {
        QString id = bus->id.toString();
        m_sendBusCombo->addItem(bus->name, id);
        if (id == currentId) selectIndex = m_sendBusCombo->count() - 1;
    }
    m_sendBusCombo->setCurrentIndex(selectIndex);
    m_sendBusCombo->blockSignals(false);
    if (selectIndex == 0 && !currentId.isEmpty()) m_track->setSendBusId(QUuid());
}

} // namespace rsd
