#include "TrackWidgets.h"

#include <QApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QVBoxLayout>
#include <algorithm>
#include <climits>

#include "ui/TrackEffectsLabel.h"

namespace rsd {

namespace {
// Header is laid out as a compact 3-row grid so the whole row can be as
// short as the waveform lane (kLaneHeight in ClipLaneWidget.cpp) instead of
// the tall single-column stack this used to be.
constexpr int kHeaderWidth = 360;
} // namespace

TrackRowWidget::TrackRowWidget(std::shared_ptr<Track> track, QWidget* parent)
    : QWidget(parent), m_track(std::move(track)) {
    setAttribute(Qt::WA_StyledBackground, true); // so setDropHighlight's stylesheet actually paints
    // Cap the row at its own content height so TimelineView's trailing
    // stretch (not this row) absorbs any extra vertical space in the scroll
    // area — otherwise, once the lane gained its own QVBoxLayout (for the
    // new per-track scrollbar), rows would balloon to fill the window.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* rowLayout = new QHBoxLayout(this);

    auto* header = new QWidget(this);
    auto* headerLayout = new QGridLayout(header);
    headerLayout->setContentsMargins(4, 2, 4, 2);
    headerLayout->setSpacing(2);
    header->setFixedWidth(kHeaderWidth);

    auto* nameLabel = new QLabel(m_track->name, header);
    headerLayout->addWidget(nameLabel, 0, 0, 1, 2);

    m_selectButton = new QRadioButton("Active", header);
    connect(m_selectButton, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) emit selected(m_track);
    });
    headerLayout->addWidget(m_selectButton, 0, 2, 1, 2);

    // Jumps straight to this track's effects: selects the row (so the side
    // panel shows its chain) and asks the panel to be brought to the front,
    // without needing to click the row first and then hunt for the dock.
    m_effectsButton = new QPushButton(QString::fromStdString(
                                           formatEffectsButtonLabel(m_track->effectsSnapshot()->size())),
                                       header);
    m_effectsButton->setToolTip("Show effects for this track");
    connect(m_effectsButton, &QPushButton::clicked, this, [this]() {
        m_selectButton->setChecked(true);
        emit effectsPanelRequested(m_track);
    });
    headerLayout->addWidget(m_effectsButton, 0, 4);

    // Shows/hides the stacked take lanes captured by the most recent
    // punch/loop recording (see rebuildTakeLanes()). Disabled when the
    // track has no takes.
    m_takesToggleButton = new QPushButton("Takes", header);
    m_takesToggleButton->setToolTip("Show/hide takes from the last punch/loop recording");
    m_takesToggleButton->setCheckable(true);
    m_takesToggleButton->setEnabled(false);
    connect(m_takesToggleButton, &QPushButton::toggled, this, [this](bool checked) {
        if (m_takeLanesContainer) m_takeLanesContainer->setVisible(checked);
    });
    headerLayout->addWidget(m_takesToggleButton, 0, 5);

    // Shows/hides this track's automation curve lane (volume/pan), always
    // available (unlike Takes, not gated on any prior recording).
    m_automationToggleButton = new QPushButton("Auto", header);
    m_automationToggleButton->setToolTip("Show/hide the volume/pan automation lane");
    m_automationToggleButton->setCheckable(true);
    connect(m_automationToggleButton, &QPushButton::toggled, this,
            [this](bool checked) { m_automationLane->setVisible(checked); });
    headerLayout->addWidget(m_automationToggleButton, 0, 6);

    m_muteBox = new QCheckBox("Mute", header);
    connect(m_muteBox, &QCheckBox::toggled, this, [this](bool checked) {
        TrackState before = TrackState::capture(*m_track);
        m_track->muted.store(checked, std::memory_order_relaxed);
        if (m_commandStack) {
            m_commandStack->push(std::make_unique<TrackStateCommand>(
                m_track, before, TrackState::capture(*m_track), "Mute Track"));
        }
    });
    headerLayout->addWidget(m_muteBox, 1, 0);

    m_soloBox = new QCheckBox("Solo", header);
    connect(m_soloBox, &QCheckBox::toggled, this, [this](bool checked) {
        TrackState before = TrackState::capture(*m_track);
        m_track->soloed.store(checked, std::memory_order_relaxed);
        if (m_commandStack) {
            m_commandStack->push(std::make_unique<TrackStateCommand>(
                m_track, before, TrackState::capture(*m_track), "Solo Track"));
        }
    });
    headerLayout->addWidget(m_soloBox, 1, 1);

    m_armBox = new QCheckBox("Arm", header);
    connect(m_armBox, &QCheckBox::toggled, this, [this](bool checked) {
        TrackState before = TrackState::capture(*m_track);
        m_track->recordArmed.store(checked, std::memory_order_relaxed);
        if (m_commandStack) {
            m_commandStack->push(std::make_unique<TrackStateCommand>(
                m_track, before, TrackState::capture(*m_track), "Arm Track"));
        }
    });
    headerLayout->addWidget(m_armBox, 1, 2);

    m_sourceCombo = new QComboBox(header);
    m_sourceCombo->addItem("Mic", QVariant::fromValue(static_cast<int>(AudioSource::Mic)));
    m_sourceCombo->addItem("System Audio",
                            QVariant::fromValue(static_cast<int>(AudioSource::SystemAudio)));
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
    headerLayout->addWidget(m_sourceCombo, 1, 3);

    // Pan and volume are both stored directly on Track now (source of truth
    // for the mixer and for automation curves to target); no more deriving
    // gainL/gainR from a pan-only gesture.
    m_panDial = new QDial(header);
    m_panDial->setRange(-100, 100);
    m_panDial->setValue(static_cast<int>(m_track->pan.load() * 100));
    m_panDial->setToolTip("Pan");
    m_panDial->setFixedSize(28, 28);
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
    headerLayout->addWidget(m_panDial, 2, 0);

    m_volumeSlider = new QSlider(Qt::Horizontal, header);
    m_volumeSlider->setRange(0, 200);
    m_volumeSlider->setValue(static_cast<int>(m_track->volume.load() * 100));
    m_volumeSlider->setToolTip("Volume");
    m_volumeSlider->setFixedHeight(16);
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
    headerLayout->addWidget(m_volumeSlider, 2, 1, 1, 3);

    // Aux send: post-fader tap to a Bus track. Buses don't send to other
    // buses, so a Bus track's own row skips this control entirely.
    if (m_track->kind != TrackKind::Bus) {
        m_sendBusCombo = new QComboBox(header);
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
        headerLayout->addWidget(m_sendBusCombo, 3, 0, 1, 2);

        m_sendLevelSlider = new QSlider(Qt::Horizontal, header);
        m_sendLevelSlider->setRange(0, 100);
        m_sendLevelSlider->setValue(static_cast<int>(m_track->sendLevel.load() * 100));
        m_sendLevelSlider->setToolTip("Send Level");
        m_sendLevelSlider->setFixedHeight(16);
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
        headerLayout->addWidget(m_sendLevelSlider, 3, 2, 1, 2);
    }

    rowLayout->addWidget(header);

    // Lane + its own horizontal scrollbar, stacked vertically, so scrolling
    // one track doesn't move the others (default; Shift syncs them — see
    // syncScrollToAllRequested).
    auto* laneContainer = new QWidget(this);
    auto* laneLayout = new QVBoxLayout(laneContainer);
    laneLayout->setContentsMargins(0, 0, 0, 0);
    laneLayout->setSpacing(0);

    m_clipLane = new ClipLaneWidget(m_track, laneContainer);
    connect(m_clipLane, &ClipLaneWidget::selectionChanged, this,
            [this](bool hasSelection) { emit clipSelectionChanged(m_track, hasSelection); });
    laneLayout->addWidget(m_clipLane, 1);

    m_laneScrollBar = new QScrollBar(Qt::Horizontal, laneContainer);
    m_laneScrollBar->setFixedHeight(12);
    m_laneScrollBar->setEnabled(false);
    laneLayout->addWidget(m_laneScrollBar);

    connect(m_laneScrollBar, &QScrollBar::valueChanged, this, [this](int v) {
        m_clipLane->setScrollOffsetSamples(static_cast<int64_t>(v));
        if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
            emit syncScrollToAllRequested(static_cast<int64_t>(v));
        }
    });
    connect(m_clipLane, &ClipLaneWidget::scrollOffsetChanged, this, [this](int64_t samples) {
        int clamped = static_cast<int>(std::clamp<int64_t>(samples, 0, INT_MAX));
        m_laneScrollBar->blockSignals(true);
        m_laneScrollBar->setValue(clamped);
        m_laneScrollBar->blockSignals(false);
    });
    connect(m_clipLane, &ClipLaneWidget::scrollRangeChanged, this, [this]() {
        int64_t maxOffset = m_clipLane->maxScrollOffsetSamples();
        int64_t visible = m_clipLane->visibleLengthSamples();
        m_laneScrollBar->blockSignals(true);
        m_laneScrollBar->setRange(0, static_cast<int>(std::clamp<int64_t>(maxOffset, 0, INT_MAX)));
        m_laneScrollBar->setPageStep(static_cast<int>(std::clamp<int64_t>(visible, 0, INT_MAX)));
        m_laneScrollBar->setEnabled(maxOffset > 0);
        m_laneScrollBar->blockSignals(false);
    });
    connect(m_clipLane, &ClipLaneWidget::syncScrollToAllRequested, this,
            [this](int64_t samples) { emit syncScrollToAllRequested(samples); });

    // Stacked take lanes, hidden until toggled on. Kept in sync with the
    // main lane's scale/scroll via the same signals used for the scrollbar.
    m_takeLanesContainer = new QWidget(laneContainer);
    m_takeLanesLayout = new QVBoxLayout(m_takeLanesContainer);
    m_takeLanesLayout->setContentsMargins(0, 1, 0, 0);
    m_takeLanesLayout->setSpacing(1);
    m_takeLanesContainer->setVisible(false);
    laneLayout->addWidget(m_takeLanesContainer);

    auto syncTakeLaneScale = [this]() {
        int64_t visible = m_clipLane->visibleLengthSamples();
        int64_t offset = m_clipLane->currentScrollOffsetSamples();
        for (auto* w : m_takeLaneWidgets) w->setScale(visible, offset);
    };
    connect(m_clipLane, &ClipLaneWidget::scrollOffsetChanged, this, syncTakeLaneScale);
    connect(m_clipLane, &ClipLaneWidget::scrollRangeChanged, this, syncTakeLaneScale);

    // Automation lane, hidden until toggled on. Kept in sync with the main
    // lane's scale/scroll the same way the take lanes are.
    m_automationLane = new AutomationLaneWidget(m_track, laneContainer);
    m_automationLane->setVisible(false);
    laneLayout->addWidget(m_automationLane);
    auto syncAutomationLaneScale = [this]() {
        m_automationLane->setScale(m_clipLane->visibleLengthSamples(),
                                    m_clipLane->currentScrollOffsetSamples());
    };
    connect(m_clipLane, &ClipLaneWidget::scrollOffsetChanged, this, syncAutomationLaneScale);
    connect(m_clipLane, &ClipLaneWidget::scrollRangeChanged, this, syncAutomationLaneScale);

    rowLayout->addWidget(laneContainer, 1);

    rebuildTakeLanes();
}

void TrackRowWidget::setDropHighlight(bool on) {
    setStyleSheet(on ? "background: rgba(120, 180, 255, 40);" : "");
}

void TrackRowWidget::setCommandStack(CommandStack* stack) {
    m_commandStack = stack;
    m_clipLane->setCommandStack(stack);
    m_automationLane->setCommandStack(stack);
}

void TrackRowWidget::refreshEffectsButton() {
    m_effectsButton->setText(
        QString::fromStdString(formatEffectsButtonLabel(m_track->effectsSnapshot()->size())));
}

void TrackRowWidget::setLaneScrollOffset(int64_t sampleOffset) {
    m_clipLane->setScrollOffsetSamples(sampleOffset);
}

void TrackRowWidget::refreshTakeLanes() { rebuildTakeLanes(); }

void TrackRowWidget::refreshSendBusOptions(const std::vector<std::shared_ptr<Track>>& busTracks) {
    if (!m_sendBusCombo) return; // this row's own track is a Bus; no send control

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
    // The bus the track was sending to may no longer exist (removed):
    // reflect that back onto the track itself rather than leaving it
    // silently pointing at a stale id.
    if (selectIndex == 0 && !currentId.isEmpty()) m_track->setSendBusId(QUuid());
}

void TrackRowWidget::rebuildTakeLanes() {
    for (auto* w : m_takeLaneWidgets) {
        m_takeLanesLayout->removeWidget(w);
        w->deleteLater();
    }
    m_takeLaneWidgets.clear();

    auto takes = m_track->takesSnapshot();
    m_takesToggleButton->setEnabled(!takes->empty());
    if (takes->empty()) {
        m_takesToggleButton->setChecked(false);
        return;
    }

    auto activeClips = m_track->clipsSnapshot();
    int64_t visible = m_clipLane->visibleLengthSamples();
    int64_t offset = m_clipLane->currentScrollOffsetSamples();

    int takeNumber = 1;
    for (auto& take : *takes) {
        bool active = std::any_of(activeClips->begin(), activeClips->end(),
                                   [&](auto& c) { return c->buffer == take->buffer; });
        auto* w = new TakeLaneWidget(take, takeNumber, m_takeLanesContainer);
        w->setActive(active);
        w->setScale(visible, offset);
        connect(w, &TakeLaneWidget::takeClicked, this,
                [this](std::shared_ptr<Clip> t) { emit takeSelected(m_track, t); });
        m_takeLanesLayout->addWidget(w);
        m_takeLaneWidgets.push_back(w);
        ++takeNumber;
    }
}

} // namespace rsd
