#include "TimeRulerWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QString>
#include <algorithm>
#include <cmath>
#include <iterator>

#include "ui/TimelineTicks.h"

namespace rsd {

TimeRulerWidget::TimeRulerWidget(QWidget* parent) : QWidget(parent) {
    setFixedHeight(28);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(24, 24, 24));
    setAutoFillBackground(true);
    setPalette(pal);
}

int TimeRulerWidget::sampleToX(int64_t sample) const {
    if (m_timelineLength <= 0) return m_leftMargin;
    return m_leftMargin +
           static_cast<int>(static_cast<double>(sample) / m_timelineLength * laneWidth());
}

int64_t TimeRulerWidget::xToSample(int x) const {
    int lw = laneWidth();
    if (lw <= 0) return 0;
    double frac = static_cast<double>(x - m_leftMargin) / lw;
    frac = std::clamp(frac, 0.0, 1.0);
    return static_cast<int64_t>(frac * m_timelineLength);
}

void TimeRulerWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(24, 24, 24));

    double totalSeconds = static_cast<double>(m_timelineLength) / std::max(1, m_sampleRate);
    if (totalSeconds <= 0) return;

    int lw = laneWidth();
    double tickSeconds = niceTickSeconds(totalSeconds, lw);

    painter.setPen(QColor(150, 150, 150));
    for (double t = 0; t <= totalSeconds; t += tickSeconds) {
        int64_t sample = static_cast<int64_t>(t * m_sampleRate);
        int x = sampleToX(sample);
        painter.drawLine(x, height() - 8, x, height());

        int totalSecs = static_cast<int>(t);
        int mins = totalSecs / 60;
        int secs = totalSecs % 60;
        QString label = QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0'));
        painter.drawText(x + 2, height() - 10, label);
    }

    if (m_punchRegion.isValid()) {
        int xStart = sampleToX(m_punchRegion.startSample);
        int xEnd = sampleToX(m_punchRegion.endSample);
        painter.fillRect(xStart, 0, xEnd - xStart, height(), QColor(230, 160, 40, 70));
        painter.setPen(QPen(QColor(230, 160, 40), 2));
        painter.drawLine(xStart, 0, xStart, height());
        painter.drawLine(xEnd, 0, xEnd, height());
    }

    painter.setPen(QPen(QColor(230, 80, 80), 2));
    int px = sampleToX(m_playheadSample);
    painter.drawLine(px, 0, px, height());

    // Bookmarks: a small numbered flag at each marker's position.
    painter.setPen(QPen(QColor(90, 190, 230), 2));
    for (auto& [slot, sample] : m_markers) {
        int mx = sampleToX(sample);
        painter.drawLine(mx, 0, mx, 10);
        painter.drawText(mx + 2, 10, QString::number(slot));
    }
}

void TimeRulerWidget::mousePressEvent(QMouseEvent* event) {
    if (event->pos().x() < m_leftMargin) return;

    if (event->button() == Qt::RightButton) {
        m_definingPunchRegion = true;
        m_punchDragAnchor = xToSample(event->pos().x());
        m_punchRegion = {m_punchDragAnchor, m_punchDragAnchor};
        update();
        return;
    }

    m_scrubbing = true;
    emit seekRequested(xToSample(event->pos().x()));
}

void TimeRulerWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_definingPunchRegion) {
        int64_t sample = xToSample(event->pos().x());
        m_punchRegion = {std::min(m_punchDragAnchor, sample), std::max(m_punchDragAnchor, sample)};
        emit punchRegionEdited(m_punchRegion);
        update();
        return;
    }
    if (!m_scrubbing) return;
    emit seekRequested(xToSample(event->pos().x()));
}

void TimeRulerWidget::mouseReleaseEvent(QMouseEvent*) {
    if (m_definingPunchRegion) {
        m_definingPunchRegion = false;
        emit punchRegionEdited(m_punchRegion);
        return;
    }
    m_scrubbing = false;
}

} // namespace rsd
