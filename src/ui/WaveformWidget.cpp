#include "WaveformWidget.h"

#include <QPainter>
#include <QPen>
#include <algorithm>
#include <cmath>

#include "ui/TimelineTicks.h"

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

void WaveformWidget::setLeftMargin(int px) {
    m_leftMargin = px;
    rebuildCache();
    update();
}

int WaveformWidget::laneWidth() const { return std::max(0, width() - m_leftMargin); }

void WaveformWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    rebuildCache();
}

void WaveformWidget::rebuildCache() {
    if (!m_buffer || laneWidth() <= 0) {
        m_peaks.clear();
        m_audioColumns = 0;
        return;
    }

    if (m_timelineLength > 0) {
        // Only the portion of the widget proportional to how much of the
        // shared timeline the buffer's own audio actually spans.
        int64_t frames = m_buffer->frameCount();
        m_audioColumns = std::clamp(
            static_cast<int>(static_cast<double>(frames) / m_timelineLength * laneWidth()), 0,
            laneWidth());
    } else {
        m_audioColumns = laneWidth();
    }

    m_peaks = WaveformCache::computePeaks(*m_buffer, std::max(0, m_audioColumns));
}

void WaveformWidget::paintGridLines(QPainter& painter) {
    if (m_timelineLength <= 0 || m_sampleRate <= 0 || laneWidth() <= 0) return;

    double totalSeconds = static_cast<double>(m_timelineLength) / m_sampleRate;
    double tickSeconds = niceTickSeconds(totalSeconds, laneWidth());

    painter.setPen(QColor(55, 55, 55));
    for (double t = 0; t <= totalSeconds; t += tickSeconds) {
        int64_t sample = static_cast<int64_t>(t * m_sampleRate);
        int x = m_leftMargin +
                static_cast<int>(static_cast<double>(sample) / m_timelineLength * laneWidth());
        painter.drawLine(x, 0, x, height());
    }
}

void WaveformWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));
    paintGridLines(painter);

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
        painter.drawLine(m_leftMargin + x, yTop, m_leftMargin + x, yBottom);
    }

    painter.setPen(QColor(60, 60, 60));
    painter.drawLine(m_leftMargin, midY, width(), midY);

    if (m_playheadSample >= 0 && m_timelineLength > 0) {
        int px = m_leftMargin + static_cast<int>(static_cast<double>(m_playheadSample) /
                                                   m_timelineLength * laneWidth());
        painter.setPen(QPen(QColor(230, 80, 80), 2));
        painter.drawLine(px, 0, px, height());
    }
}

} // namespace rsd
