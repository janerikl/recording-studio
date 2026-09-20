#include "AutomationLaneWidget.h"

#include <QColor>
#include <QComboBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <algorithm>

#include "audio/AutomationMath.h"
#include "audio/PanLawMath.h"
#include "command/EditCommands.h"

namespace rsd {

namespace {
constexpr int kHitToleranceMarginPx = 8;
} // namespace

AutomationLaneWidget::AutomationLaneWidget(std::shared_ptr<Track> track, QWidget* parent)
    : QWidget(parent), m_track(std::move(track)) {
    setFocusPolicy(Qt::StrongFocus);
    setFixedHeight(kHeight);
    setStyleSheet("background: #161616;");

    m_targetCombo = new QComboBox(this);
    m_targetCombo->addItem("Volume", static_cast<int>(AutomationTarget::Volume));
    m_targetCombo->addItem("Pan", static_cast<int>(AutomationTarget::Pan));
    m_targetCombo->setGeometry(2, 2, 70, kComboHeight);
    connect(m_targetCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        m_selectedPointIndex = -1;
        update();
    });
}

AutomationTarget AutomationLaneWidget::currentTarget() const {
    return static_cast<AutomationTarget>(m_targetCombo->currentData().toInt());
}

void AutomationLaneWidget::setScale(int64_t visibleLengthSamples, int64_t scrollOffsetSamples) {
    m_visibleLengthSamples = visibleLengthSamples;
    m_scrollOffsetSamples = scrollOffsetSamples;
    update();
}

int64_t AutomationLaneWidget::xToSample(int x) const {
    if (width() <= 0 || m_visibleLengthSamples <= 0) return m_scrollOffsetSamples;
    return m_scrollOffsetSamples +
           static_cast<int64_t>(static_cast<double>(x) / width() * m_visibleLengthSamples);
}

int AutomationLaneWidget::sampleToX(int64_t sample) const {
    if (m_visibleLengthSamples <= 0) return 0;
    return static_cast<int>(static_cast<double>(sample - m_scrollOffsetSamples) / m_visibleLengthSamples *
                             width());
}

float AutomationLaneWidget::yToValue(int y) const {
    // Volume [0,2] and Pan [-1,1] both map top-to-bottom as high-to-low,
    // matching the gain-line/velocity-lane convention used elsewhere.
    float t = 1.0f - std::clamp(static_cast<float>(y) / static_cast<float>(height()), 0.0f, 1.0f);
    return currentTarget() == AutomationTarget::Volume ? t * 2.0f : (t * 2.0f - 1.0f);
}

int AutomationLaneWidget::valueToY(float value) const {
    float t = currentTarget() == AutomationTarget::Volume ? value / 2.0f : (value + 1.0f) / 2.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    return static_cast<int>((1.0f - t) * height());
}

std::shared_ptr<AutomationLane> AutomationLaneWidget::currentLane() const {
    auto lanes = m_track->automationLanesSnapshot();
    for (auto& lane : *lanes) {
        if (lane->target == currentTarget()) return lane;
    }
    return nullptr;
}

void AutomationLaneWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#161616"));

    float staticValue = currentTarget() == AutomationTarget::Volume ? m_track->volume.load() : m_track->pan.load();
    auto lane = currentLane();

    painter.setPen(QColor("#66aaff"));
    if (!lane || lane->points.empty()) {
        int y = valueToY(staticValue);
        painter.drawLine(0, y, width(), y);
    } else {
        QPoint prev(sampleToX(lane->points.front().sample), valueToY(lane->points.front().value));
        // Extend flat to the left edge from the first point, and flat to the
        // right edge from the last, matching evaluateAutomation's clamping.
        painter.drawLine(0, prev.y(), prev.x(), prev.y());
        for (size_t i = 1; i < lane->points.size(); ++i) {
            QPoint cur(sampleToX(lane->points[i].sample), valueToY(lane->points[i].value));
            painter.drawLine(prev, cur);
            prev = cur;
        }
        painter.drawLine(prev.x(), prev.y(), width(), prev.y());

        for (size_t i = 0; i < lane->points.size(); ++i) {
            int x = sampleToX(lane->points[i].sample);
            int y = valueToY(lane->points[i].value);
            bool selected = static_cast<int>(i) == m_selectedPointIndex;
            painter.setBrush(selected ? QColor("#ffcc66") : QColor("#66aaff"));
            painter.drawEllipse(QPoint(x, y), 4, 4);
        }
    }
}

void AutomationLaneWidget::mousePressEvent(QMouseEvent* event) {
    if (event->pos().y() < kComboHeight && event->pos().x() < 74) return; // over the combo box

    auto before = m_track->automationLanesSnapshot();
    int64_t sample = xToSample(event->pos().x());
    int64_t tolerance = xToSample(kHitToleranceMarginPx) - xToSample(0);

    auto lane = currentLane();
    int hitIndex = lane ? findPointNear(lane->points, sample, tolerance) : -1;

    if (hitIndex >= 0) {
        m_selectedPointIndex = hitIndex;
        m_dragging = true;
        m_editBeforeSnapshot = before;
        update();
        return;
    }

    // Empty space: add a new point at the clicked position/value.
    float value = yToValue(event->pos().y());
    auto newLane = lane ? std::make_shared<AutomationLane>(*lane)
                         : std::make_shared<AutomationLane>(AutomationLane{currentTarget(), {}});
    insertPointSorted(newLane->points, {sample, value});
    m_track->replaceAutomationLane(newLane);

    auto it = std::find_if(newLane->points.begin(), newLane->points.end(),
                            [&](const AutomationPoint& p) { return p.sample == sample; });
    m_selectedPointIndex = it != newLane->points.end() ? static_cast<int>(it - newLane->points.begin()) : -1;

    commitEdit(before, "Add automation point");
    update();
}

void AutomationLaneWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!m_dragging || m_selectedPointIndex < 0) return;
    auto lane = currentLane();
    if (!lane || m_selectedPointIndex >= static_cast<int>(lane->points.size())) return;

    auto newLane = std::make_shared<AutomationLane>(*lane);
    int64_t sample = std::max<int64_t>(0, xToSample(event->pos().x()));
    float value = yToValue(event->pos().y());
    newLane->points[m_selectedPointIndex] = {sample, value};
    std::sort(newLane->points.begin(), newLane->points.end(),
              [](const AutomationPoint& a, const AutomationPoint& b) { return a.sample < b.sample; });
    // Track the point through the re-sort so drags across neighbors keep it selected.
    auto it = std::find_if(newLane->points.begin(), newLane->points.end(),
                            [&](const AutomationPoint& p) { return p.sample == sample && p.value == value; });
    m_selectedPointIndex = it != newLane->points.end() ? static_cast<int>(it - newLane->points.begin()) : -1;

    m_track->replaceAutomationLane(newLane);
    update();
}

void AutomationLaneWidget::mouseReleaseEvent(QMouseEvent*) {
    if (!m_dragging) return;
    m_dragging = false;
    commitEdit(m_editBeforeSnapshot, "Move automation point");
    m_editBeforeSnapshot.reset();
}

void AutomationLaneWidget::keyPressEvent(QKeyEvent* event) {
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
        m_selectedPointIndex >= 0) {
        auto before = m_track->automationLanesSnapshot();
        auto lane = currentLane();
        if (lane && m_selectedPointIndex < static_cast<int>(lane->points.size())) {
            auto newLane = std::make_shared<AutomationLane>(*lane);
            newLane->points.erase(newLane->points.begin() + m_selectedPointIndex);
            m_track->replaceAutomationLane(newLane);
            m_selectedPointIndex = -1;
            commitEdit(before, "Delete automation point");
            update();
        }
        return;
    }
    QWidget::keyPressEvent(event);
}

void AutomationLaneWidget::commitEdit(std::shared_ptr<const Track::AutomationLaneList> before,
                                       const QString& text) {
    auto after = m_track->automationLanesSnapshot();
    if (m_commandStack && before && after != before) {
        m_commandStack->push(std::make_unique<TrackAutomationCommand>(m_track, before, after, text));
    }
}

} // namespace rsd
