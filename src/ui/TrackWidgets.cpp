#include "TrackWidgets.h"

#include <QApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QVBoxLayout>
#include <algorithm>
#include <climits>

#include "ui/TrackKindColor.h"

namespace rsd {

namespace {
// The header used to need 360px to fit a full channel-strip's worth of
// controls in one row; now that those live only on the mixer strip (see
// MixerStripWidget), it's just Name/Active/Takes/Auto stacked two rows
// tall over two columns — narrower, freeing width for the waveform lane.
constexpr int kHeaderWidth = 160;
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

    // Fixed color accent per TrackKind (Audio/Instrument/Bus), so a track's
    // type is visible at a glance without opening its controls.
    auto* kindStripe = new QFrame(this);
    kindStripe->setFixedWidth(4);
    kindStripe->setFrameShape(QFrame::NoFrame);
    kindStripe->setAutoFillBackground(true);
    QPalette stripePalette = kindStripe->palette();
    stripePalette.setColor(QPalette::Window, trackKindColor(m_track->kind));
    kindStripe->setPalette(stripePalette);
    rowLayout->addWidget(kindStripe);

    auto* header = new QWidget(this);
    auto* headerLayout = new QGridLayout(header);
    headerLayout->setContentsMargins(4, 2, 4, 2);
    headerLayout->setSpacing(2);
    header->setFixedWidth(kHeaderWidth);

    auto* nameLabel = new QLabel(m_track->name, header);
    headerLayout->addWidget(nameLabel, 0, 0);

    m_selectButton = new QRadioButton("Active", header);
    connect(m_selectButton, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) emit selected(m_track);
    });
    headerLayout->addWidget(m_selectButton, 0, 1);

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
    headerLayout->addWidget(m_takesToggleButton, 1, 0);

    // Shows/hides this track's automation curve lane (volume/pan), always
    // available (unlike Takes, not gated on any prior recording).
    m_automationToggleButton = new QPushButton("Auto", header);
    m_automationToggleButton->setToolTip("Show/hide the volume/pan automation lane");
    m_automationToggleButton->setCheckable(true);
    connect(m_automationToggleButton, &QPushButton::toggled, this,
            [this](bool checked) { m_automationLane->setVisible(checked); });
    headerLayout->addWidget(m_automationToggleButton, 1, 1);

    // Mute/Solo/Pan/Volume/Send/Arm/input-source/FX all now live only on
    // the Mixer panel's strip for this track (see MixerStripWidget) —
    // kept here would just be a duplicate control fighting the same
    // TrackState. The row keeps only what the mixer doesn't have: Active
    // selection and Takes/Auto (which toggle lanes, not just mirror a
    // value).

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
