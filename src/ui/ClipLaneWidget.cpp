#include "ClipLaneWidget.h"

#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <cstdlib>

#include "ui/MediaLibraryPanel.h"
#include "waveform/WaveformCache.h"

namespace rsd {

ClipLaneWidget::ClipLaneWidget(std::shared_ptr<Track> track, QWidget* parent)
    : QWidget(parent), m_track(std::move(track)) {
    setMinimumHeight(120);
    setMouseTracking(true);
    setAcceptDrops(true);
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
    // Generous fixed headroom beyond the content, and a floor, so there's
    // always comfortable room to drag a clip around without the timeline's
    // scale visibly changing underneath the user as a side effect.
    constexpr int64_t kFloor = 44100 * 30;
    constexpr int64_t kHeadroom = 44100 * 10;
    return std::max(kFloor, maxEnd + kHeadroom);
}

int64_t ClipLaneWidget::effectiveTimelineLength() const {
    // While actively dragging, keep using the scale captured at drag-start
    // for BOTH hit-testing and painting — otherwise the scale shifts as the
    // clip's own extents change mid-drag, which visually cancels out the
    // very edit the user is making (e.g. moving a clip right also grows the
    // total length, so the clip appears to stay in the same place).
    if (m_dragMode != DragMode::None) return m_dragTotalSamples;
    if (m_sharedTimelineLength > 0) return m_sharedTimelineLength;
    return timelineLengthSamples();
}

void ClipLaneWidget::setSharedTimelineLength(int64_t samples) {
    m_sharedTimelineLength = samples;
    if (m_dragMode == DragMode::None) update();
}

void ClipLaneWidget::setPlayheadSample(int64_t sample) {
    m_playheadSample = sample;
    update();
}

int64_t ClipLaneWidget::xToSample(int x) const {
    int64_t total = effectiveTimelineLength();
    if (width() <= 0) return 0;
    return static_cast<int64_t>(static_cast<double>(x) / width() * total);
}

int ClipLaneWidget::sampleToX(int64_t sample) const {
    int64_t total = effectiveTimelineLength();
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

                auto drawChannel = [&](int channel, int centerY, float halfH) {
                    auto peaks = WaveformCache::computePeaks(sub, w, channel);
                    painter.setPen(QColor(90, 170, 230));
                    for (int i = 0; i < peaks.size(); ++i) {
                        auto [minV, maxV] = peaks[i];
                        int yTop = centerY - static_cast<int>(maxV * halfH);
                        int yBottom = centerY - static_cast<int>(minV * halfH);
                        painter.drawLine(x0 + i, yTop, x0 + i, yBottom);
                    }
                };

                if (sub.channels >= 2) {
                    // Stereo: split the lane into a top (L) and bottom (R)
                    // half instead of averaging channels into one trace.
                    int quarterH = height() / 4;
                    int topCenter = height() / 4;
                    int bottomCenter = (3 * height()) / 4;
                    drawChannel(0, topCenter, quarterH - 4.0f);
                    painter.setPen(QColor(70, 70, 70));
                    painter.drawLine(x0, midY, x0 + w, midY);
                    drawChannel(1, bottomCenter, quarterH - 4.0f);
                } else {
                    drawChannel(-1, midY, halfHeight);
                }
            }
        }

        painter.setPen(QColor(200, 200, 200));
        painter.drawText(x0 + 4, 16, clip->name);
    }

    if (m_playheadSample >= 0) {
        int px = sampleToX(m_playheadSample);
        painter.setPen(QPen(QColor(230, 80, 80), 2));
        painter.drawLine(px, 0, px, height());
    }
}

void ClipLaneWidget::mousePressEvent(QMouseEvent* event) {
    int64_t sample = xToSample(event->pos().x());
    auto clip = findClipAt(sample);

    if (!clip) {
        clearSelection();
        m_scrubbingPlayhead = true;
        emit seekRequested(sample);
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
    // Lock the timeline scale for the whole gesture — recomputing it from the
    // live (already-edited) clip state on every move causes the scale to
    // shift mid-drag, snowballing tiny mouse movements into huge trims.
    m_dragTotalSamples = timelineLengthSamples();

    if (std::abs(event->pos().x() - x0) <= kEdgeThresholdPx) {
        m_dragMode = DragMode::TrimStart;
        setCursor(Qt::SizeHorCursor);
    } else if (std::abs(event->pos().x() - x1) <= kEdgeThresholdPx) {
        m_dragMode = DragMode::TrimEnd;
        setCursor(Qt::SizeHorCursor);
    } else {
        m_dragMode = DragMode::Move;
        setCursor(Qt::ClosedHandCursor);
    }

    emit editStarted(); // snapshot the pre-edit state for undo, before any mutation below
    update();
}

void ClipLaneWidget::updateHoverCursor(const QPoint& pos) {
    int64_t sample = xToSample(pos.x());
    auto clip = findClipAt(sample);
    if (!clip) {
        unsetCursor();
        return;
    }

    int x0 = sampleToX(clip->sessionStartSample);
    int x1 = sampleToX(clip->sessionStartSample + clip->lengthSamples);

    if (std::abs(pos.x() - x0) <= kEdgeThresholdPx || std::abs(pos.x() - x1) <= kEdgeThresholdPx) {
        setCursor(Qt::SizeHorCursor); // near an edge: trim
    } else {
        setCursor(Qt::OpenHandCursor); // over the body: move
    }
}

void ClipLaneWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_scrubbingPlayhead) {
        emit seekRequested(xToSample(event->pos().x()));
        return;
    }

    if (m_dragMode == DragMode::None) {
        updateHoverCursor(event->pos());
        return;
    }

    int64_t total = m_dragTotalSamples;
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
        // Clamp within [0, total - length] so the clip can never be dragged
        // past the scale that was frozen for this gesture — otherwise
        // releasing the mouse would force a rescale (the timeline "widening")
        // right as the clip settles.
        int64_t maxStart = std::max<int64_t>(0, m_dragTotalSamples - m_dragOrigLength);
        int64_t newStart = std::clamp<int64_t>(m_dragOrigStart + deltaSamples, 0, maxStart);
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

    if (m_dragMode == DragMode::Move) {
        emit clipDraggedToGlobalPos(m_dragClipId, event->globalPosition().toPoint());
    }
}

void ClipLaneWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (m_dragMode == DragMode::Move) {
        emit clipDropped(m_dragClipId, event->globalPosition().toPoint());
    }
    m_dragMode = DragMode::None;
    m_scrubbingPlayhead = false;
    updateHoverCursor(event->pos());
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

void ClipLaneWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat(MediaLibraryPanel::kMimeType)) event->acceptProposedAction();
}

void ClipLaneWidget::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasFormat(MediaLibraryPanel::kMimeType)) return;

    bool ok = false;
    int libraryIndex = event->mimeData()->data(MediaLibraryPanel::kMimeType).toInt(&ok);
    if (!ok) return;

    int64_t sample = xToSample(event->position().toPoint().x());
    emit mediaDropped(libraryIndex, sample);
    event->acceptProposedAction();
}

} // namespace rsd
