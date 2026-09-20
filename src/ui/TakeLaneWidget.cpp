#include "TakeLaneWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

#include "waveform/WaveformCache.h"

namespace rsd {

TakeLaneWidget::TakeLaneWidget(std::shared_ptr<Clip> take, int takeNumber, QWidget* parent)
    : QWidget(parent), m_take(std::move(take)), m_takeNumber(takeNumber) {
    setFixedHeight(kHeight);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QString("Take %1 — click to make this the active comp").arg(m_takeNumber));
}

void TakeLaneWidget::setActive(bool active) {
    if (m_active == active) return;
    m_active = active;
    update();
}

void TakeLaneWidget::setScale(int64_t visibleLengthSamples, int64_t scrollOffsetSamples) {
    m_visibleLengthSamples = visibleLengthSamples;
    m_scrollOffsetSamples = scrollOffsetSamples;
    update();
}

int TakeLaneWidget::sampleToX(int64_t sample) const {
    if (m_visibleLengthSamples <= 0) return 0;
    return static_cast<int>(static_cast<double>(sample - m_scrollOffsetSamples) / m_visibleLengthSamples *
                             width());
}

void TakeLaneWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    QColor bg = m_active ? QColor(45, 65, 45) : QColor(35, 35, 35);
    painter.fillRect(rect(), bg);

    if (!m_take || !m_take->buffer) return;

    int x0 = sampleToX(m_take->sessionStartSample);
    int x1 = sampleToX(m_take->sessionStartSample + m_take->lengthSamples);
    int w = std::max(1, x1 - x0);

    QColor border = m_active ? QColor(120, 220, 140) : QColor(70, 70, 70);
    painter.setPen(border);
    painter.drawRect(x0, 1, w - 1, height() - 3);

    if (m_take->buffer->frameCount() > 0) {
        auto peaks = WaveformCache::computePeaks(*m_take->buffer, w, -1);
        painter.setPen(m_active ? QColor(140, 230, 160) : QColor(120, 120, 120));
        int centerY = height() / 2;
        float halfHeight = static_cast<float>(height()) / 2.0f - 4.0f;
        for (int i = 0; i < peaks.size(); ++i) {
            auto [minV, maxV] = peaks[i];
            int yTop = centerY - static_cast<int>(maxV * halfHeight);
            int yBottom = centerY - static_cast<int>(minV * halfHeight);
            painter.drawLine(x0 + i, yTop, x0 + i, yBottom);
        }
    }

    painter.setPen(m_active ? QColor(200, 255, 210) : QColor(170, 170, 170));
    QString label = QString("Take %1%2").arg(m_takeNumber).arg(m_active ? "  (active)" : "");
    painter.drawText(x0 + 4, 13, label);
}

void TakeLaneWidget::mousePressEvent(QMouseEvent*) { emit takeClicked(m_take); }

} // namespace rsd
