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

    // Pan is a convenience gesture, not a stored value: moving it derives and
    // writes both gainL/gainR via a simple linear pan law. The two gain
    // sliders are the actual source of truth read by the mixer, so adjusting
    // one directly doesn't move the dial back (not every L/R pair has a
    // matching symmetric pan angle).
    m_panDial = new QDial(header);
    m_panDial->setRange(-100, 100);
    m_panDial->setValue(0);
    m_panDial->setToolTip("Pan");
    m_panDial->setFixedSize(28, 28);
    connect(m_panDial, &QDial::sliderPressed, this,
            [this]() { m_dragBeforeState = TrackState::capture(*m_track); });
    connect(m_panDial, &QDial::valueChanged, this, [this](int v) {
        float p = v / 100.0f;
        float gl = p <= 0 ? 1.0f : 1.0f - p;
        float gr = p >= 0 ? 1.0f : 1.0f + p;
        m_track->gainL.store(gl, std::memory_order_relaxed);
        m_track->gainR.store(gr, std::memory_order_relaxed);
        if (m_gainLSlider) m_gainLSlider->setValue(static_cast<int>(gl * 100));
        if (m_gainRSlider) m_gainRSlider->setValue(static_cast<int>(gr * 100));
    });
    connect(m_panDial, &QDial::sliderReleased, this, [this]() {
        if (m_commandStack && m_dragBeforeState) {
            m_commandStack->push(std::make_unique<TrackStateCommand>(
                m_track, *m_dragBeforeState, TrackState::capture(*m_track), "Pan Track"));
        }
        m_dragBeforeState.reset();
    });
    headerLayout->addWidget(m_panDial, 2, 0);

    m_gainLSlider = new QSlider(Qt::Horizontal, header);
    m_gainLSlider->setRange(0, 200);
    m_gainLSlider->setValue(static_cast<int>(m_track->gainL.load() * 100));
    m_gainLSlider->setToolTip("Gain L");
    m_gainLSlider->setFixedHeight(16);
    connect(m_gainLSlider, &QSlider::sliderPressed, this,
            [this]() { m_dragBeforeState = TrackState::capture(*m_track); });
    connect(m_gainLSlider, &QSlider::valueChanged, this,
            [this](int v) { m_track->gainL.store(v / 100.0f, std::memory_order_relaxed); });
    connect(m_gainLSlider, &QSlider::sliderReleased, this, [this]() {
        if (m_commandStack && m_dragBeforeState) {
            m_commandStack->push(std::make_unique<TrackStateCommand>(
                m_track, *m_dragBeforeState, TrackState::capture(*m_track), "Gain L"));
        }
        m_dragBeforeState.reset();
    });
    headerLayout->addWidget(m_gainLSlider, 2, 1, 1, 3);

    m_gainRSlider = new QSlider(Qt::Horizontal, header);
    m_gainRSlider->setRange(0, 200);
    m_gainRSlider->setValue(static_cast<int>(m_track->gainR.load() * 100));
    m_gainRSlider->setToolTip("Gain R");
    m_gainRSlider->setFixedHeight(16);
    connect(m_gainRSlider, &QSlider::sliderPressed, this,
            [this]() { m_dragBeforeState = TrackState::capture(*m_track); });
    connect(m_gainRSlider, &QSlider::valueChanged, this,
            [this](int v) { m_track->gainR.store(v / 100.0f, std::memory_order_relaxed); });
    connect(m_gainRSlider, &QSlider::sliderReleased, this, [this]() {
        if (m_commandStack && m_dragBeforeState) {
            m_commandStack->push(std::make_unique<TrackStateCommand>(
                m_track, *m_dragBeforeState, TrackState::capture(*m_track), "Gain R"));
        }
        m_dragBeforeState.reset();
    });
    headerLayout->addWidget(m_gainRSlider, 3, 1, 1, 3);

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

    rowLayout->addWidget(laneContainer, 1);

    rebuildTakeLanes();
}

void TrackRowWidget::setDropHighlight(bool on) {
    setStyleSheet(on ? "background: rgba(120, 180, 255, 40);" : "");
}

void TrackRowWidget::setCommandStack(CommandStack* stack) {
    m_commandStack = stack;
    m_clipLane->setCommandStack(stack);
}

void TrackRowWidget::refreshEffectsButton() {
    m_effectsButton->setText(
        QString::fromStdString(formatEffectsButtonLabel(m_track->effectsSnapshot()->size())));
}

void TrackRowWidget::setLaneScrollOffset(int64_t sampleOffset) {
    m_clipLane->setScrollOffsetSamples(sampleOffset);
}

void TrackRowWidget::refreshTakeLanes() { rebuildTakeLanes(); }

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
