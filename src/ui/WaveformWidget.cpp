#include "WaveformWidget.h"

#include <QPainter>
#include <QPen>
#include <algorithm>

namespace rsd {

WaveformWidget::WaveformWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(120);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(30, 30, 30));
    setAutoFillBackground(true);
    setPalette(pal);
}

void WaveformWidget::setBuffer(std::shared_ptr<AudioBuffer> buffer) {
    m_buffer = std::move(buffer);
    rebuildCache();
    update();
}

void WaveformWidget::setTimelineLength(int64_t samples) {
    m_timelineLength = samples;
    rebuildCache();
    update();
}

void WaveformWidget::setPlayheadSample(int64_t sample) {
    m_playheadSample = sample;
    update();
}

void WaveformWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    rebuildCache();
}

void WaveformWidget::rebuildCache() {
    if (!m_buffer || width() <= 0) {
        m_peaks.clear();
        m_audioColumns = 0;
        return;
    }

    if (m_timelineLength > 0) {
        // Only the portion of the widget proportional to how much of the
        // shared timeline the buffer's own audio actually spans.
        int64_t frames = m_buffer->frameCount();
        m_audioColumns =
            std::clamp(static_cast<int>(static_cast<double>(frames) / m_timelineLength * width()),
                       0, width());
    } else {
        m_audioColumns = width();
    }

    m_peaks = WaveformCache::computePeaks(*m_buffer, std::max(0, m_audioColumns));
}

void WaveformWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));

    if (m_peaks.isEmpty()) {
        painter.setPen(QColor(120, 120, 120));
        painter.drawText(rect(), Qt::AlignCenter, "No audio loaded");
        return;
    }

    const int midY = height() / 2;
    const float halfHeight = static_cast<float>(height()) / 2.0f - 2.0f;

    painter.setPen(QColor(90, 170, 230));
    for (int x = 0; x < m_peaks.size(); ++x) {
        auto [minV, maxV] = m_peaks[x];
        int yTop = midY - static_cast<int>(maxV * halfHeight);
        int yBottom = midY - static_cast<int>(minV * halfHeight);
        painter.drawLine(x, yTop, x, yBottom);
    }

    painter.setPen(QColor(60, 60, 60));
    painter.drawLine(0, midY, width(), midY);

    if (m_playheadSample >= 0 && m_timelineLength > 0) {
        int px = static_cast<int>(static_cast<double>(m_playheadSample) / m_timelineLength * width());
        painter.setPen(QPen(QColor(230, 80, 80), 2));
        painter.drawLine(px, 0, px, height());
    }
}

} // namespace rsd
