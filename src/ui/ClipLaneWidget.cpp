#include "ClipLaneWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <cstdlib>

#include "waveform/WaveformCache.h"

namespace rsd {

ClipLaneWidget::ClipLaneWidget(std::shared_ptr<Track> track, QWidget* parent)
    : QWidget(parent), m_track(std::move(track)) {
    setMinimumHeight(120);
    setMouseTracking(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(30, 30, 30));
    setAutoFillBackground(true);
    setPalette(pal);
}

void ClipLaneWidget::refresh() { update(); }

void ClipLaneWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

int64_t ClipLaneWidget::timelineLengthSamples() const {
    auto clips = m_track->clipsSnapshot();
    int64_t maxEnd = 0;
    for (auto& c : *clips) maxEnd = std::max(maxEnd, c->sessionStartSample + c->lengthSamples);
    // A little headroom so clips at the very end aren't flush against the widget edge.
    int64_t minLength = m_track->clipsSnapshot()->empty() ? 44100 * 5 : maxEnd;
    return std::max(maxEnd, minLength);
}

int64_t ClipLaneWidget::xToSample(int x) const {
    int64_t total = timelineLengthSamples();
    if (width() <= 0) return 0;
    return static_cast<int64_t>(static_cast<double>(x) / width() * total);
}

int ClipLaneWidget::sampleToX(int64_t sample) const {
    int64_t total = timelineLengthSamples();
    if (total <= 0) return 0;
    return static_cast<int>(static_cast<double>(sample) / total * width());
}

std::shared_ptr<Clip> ClipLaneWidget::findClipAt(int64_t sample) const {
    auto clips = m_track->clipsSnapshot();
    for (auto& c : *clips) {
        if (sample >= c->sessionStartSample && sample < c->sessionStartSample + c->lengthSamples) {
            return c;
        }
    }
    return nullptr;
}

void ClipLaneWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));

    auto clips = m_track->clipsSnapshot();
    if (clips->empty()) {
        painter.setPen(QColor(120, 120, 120));
        painter.drawText(rect(), Qt::AlignCenter, "No audio — Import or Record into this track");
        return;
    }

    const int midY = height() / 2;
    const float halfHeight = static_cast<float>(height()) / 2.0f - 8.0f;

    for (auto& clip : *clips) {
        if (!clip->buffer) continue;

        int x0 = sampleToX(clip->sessionStartSample);
        int x1 = sampleToX(clip->sessionStartSample + clip->lengthSamples);
        int w = std::max(1, x1 - x0);

        bool selected = clip->id == m_selectedClipId;
        QColor bg = selected ? QColor(60, 70, 90) : QColor(45, 45, 45);
        painter.fillRect(x0, 4, w, height() - 8, bg);
        painter.setPen(selected ? QColor(120, 180, 255) : QColor(80, 80, 80));
        painter.drawRect(x0, 4, w - 1, height() - 9);

        // Peaks for just this clip's visible/trimmed sample range.
        int64_t srcStart = clip->sourceOffsetSamples;
        int64_t srcLen = clip->lengthSamples;
        if (srcLen > 0 && clip->buffer->frameCount() > 0) {
            AudioBuffer sub;
            sub.channels = clip->buffer->channels;
            sub.sampleRate = clip->buffer->sampleRate;
            int64_t clampedLen =
                std::min(srcLen, clip->buffer->frameCount() - std::max<int64_t>(0, srcStart));
            if (clampedLen > 0) {
                sub.samples.assign(
                    clip->buffer->samples.begin() + srcStart * sub.channels,
                    clip->buffer->samples.begin() + (srcStart + clampedLen) * sub.channels);
                auto peaks = WaveformCache::computePeaks(sub, w);
                painter.setPen(QColor(90, 170, 230));
                for (int i = 0; i < peaks.size(); ++i) {
                    auto [minV, maxV] = peaks[i];
                    int yTop = midY - static_cast<int>(maxV * halfHeight);
                    int yBottom = midY - static_cast<int>(minV * halfHeight);
                    painter.drawLine(x0 + i, yTop, x0 + i, yBottom);
                }
            }
        }

        painter.setPen(QColor(200, 200, 200));
        painter.drawText(x0 + 4, 16, clip->name);
    }
}

void ClipLaneWidget::mousePressEvent(QMouseEvent* event) {
    int64_t sample = xToSample(event->pos().x());
    auto clip = findClipAt(sample);

    if (!clip) {
        clearSelection();
        return;
    }

    m_selectedClipId = clip->id;
    emit selectionChanged(true);

    int x0 = sampleToX(clip->sessionStartSample);
    int x1 = sampleToX(clip->sessionStartSample + clip->lengthSamples);

    m_dragClipId = clip->id;
    m_dragStartX = event->pos().x();
    m_dragOrigStart = clip->sessionStartSample;
    m_dragOrigOffset = clip->sourceOffsetSamples;
    m_dragOrigLength = clip->lengthSamples;

    if (std::abs(event->pos().x() - x0) <= kEdgeThresholdPx) {
        m_dragMode = DragMode::TrimStart;
    } else if (std::abs(event->pos().x() - x1) <= kEdgeThresholdPx) {
        m_dragMode = DragMode::TrimEnd;
    } else {
        m_dragMode = DragMode::Move;
    }

    update();
}

void ClipLaneWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragMode == DragMode::None) return;

    int64_t total = timelineLengthSamples();
    if (total <= 0 || width() <= 0) return;
    int64_t deltaSamples =
        static_cast<int64_t>((event->pos().x() - m_dragStartX) / static_cast<double>(width()) * total);

    auto clips = m_track->clipsSnapshot();
    std::shared_ptr<Clip> original;
    for (auto& c : *clips) {
        if (c->id == m_dragClipId) { original = c; break; }
    }
    if (!original) return;

    auto edited = std::make_shared<Clip>(*original);

    if (m_dragMode == DragMode::Move) {
        int64_t newStart = std::max<int64_t>(0, m_dragOrigStart + deltaSamples);
        edited->sessionStartSample = newStart;
    } else if (m_dragMode == DragMode::TrimStart) {
        int64_t maxTrim = m_dragOrigLength - 1; // keep at least 1 sample
        int64_t trim = std::clamp<int64_t>(deltaSamples, -m_dragOrigOffset, maxTrim);
        edited->sourceOffsetSamples = m_dragOrigOffset + trim;
        edited->sessionStartSample = m_dragOrigStart + trim;
        edited->lengthSamples = m_dragOrigLength - trim;
    } else if (m_dragMode == DragMode::TrimEnd) {
        int64_t available = original->buffer ? original->buffer->frameCount() - m_dragOrigOffset
                                              : m_dragOrigLength;
        int64_t newLength =
            std::clamp<int64_t>(m_dragOrigLength + deltaSamples, 1, available);
        edited->lengthSamples = newLength;
    }

    m_track->replaceClip(m_dragClipId, edited);
    update();
}

void ClipLaneWidget::mouseReleaseEvent(QMouseEvent*) {
    m_dragMode = DragMode::None;
}

void ClipLaneWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    int64_t sample = xToSample(event->pos().x());
    auto clip = findClipAt(sample);
    if (!clip) return;

    m_track->splitClip(clip->id, sample);
    m_selectedClipId = QUuid();
    emit selectionChanged(false);
    update();
}

void ClipLaneWidget::deleteSelected() {
    if (m_selectedClipId.isNull()) return;
    m_track->removeClip(m_selectedClipId);
    m_selectedClipId = QUuid();
    emit selectionChanged(false);
    update();
}

void ClipLaneWidget::clearSelection() {
    m_selectedClipId = QUuid();
    emit selectionChanged(false);
    update();
}

} // namespace rsd
