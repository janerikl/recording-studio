#include "ClipLaneWidget.h"

#include <QContextMenuEvent>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHelpEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QToolTip>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "command/EditCommands.h"
#include "model/EditClipboard.h"
#include "ui/ClipEditMath.h"
#include "ui/ClipLaneScrollMath.h"
#include "ui/MediaBrowserPanel.h"
#include "ui/MidiNoteDisplayMath.h"
#include "ui/TimelineScaleMath.h"
#include "ui/TimelineTicks.h"
#include "ui/WaveformDisplayMath.h"
#include "waveform/WaveformCache.h"

namespace rsd {

namespace {
constexpr int kLaneHeight = 60;
} // namespace

ClipLaneWidget::ClipLaneWidget(std::shared_ptr<Track> track, QWidget* parent)
    : QWidget(parent), m_track(std::move(track)) {
    setMinimumHeight(kLaneHeight);
    setMouseTracking(true);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus); // needed to receive Ctrl+C/Ctrl+V key events
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
    return resolveDragLockSamples(m_sharedTimelineLength, timelineLengthSamples());
}

int64_t ClipLaneWidget::effectiveScrollOffset() const {
    if (m_dragMode != DragMode::None) return m_dragScrollOffsetSamples;
    return clampScrollOffset(m_scrollOffsetSamples, m_contentExtentSamples, effectiveTimelineLength());
}

void ClipLaneWidget::setSharedTimelineLength(int64_t samples) {
    m_sharedTimelineLength = samples;
    int64_t clamped = clampScrollOffset(m_scrollOffsetSamples, m_contentExtentSamples, effectiveTimelineLength());
    if (clamped != m_scrollOffsetSamples) {
        m_scrollOffsetSamples = clamped;
        emit scrollOffsetChanged(m_scrollOffsetSamples);
    }
    emit scrollRangeChanged();
    if (m_dragMode == DragMode::None) update();
}

void ClipLaneWidget::setContentExtentSamples(int64_t samples) {
    m_contentExtentSamples = samples;
    int64_t clamped = clampScrollOffset(m_scrollOffsetSamples, m_contentExtentSamples, effectiveTimelineLength());
    if (clamped != m_scrollOffsetSamples) {
        m_scrollOffsetSamples = clamped;
        emit scrollOffsetChanged(m_scrollOffsetSamples);
    }
    emit scrollRangeChanged();
    if (m_dragMode == DragMode::None) update();
}

void ClipLaneWidget::setScrollOffsetSamples(int64_t samples) {
    int64_t clamped = clampScrollOffset(samples, m_contentExtentSamples, effectiveTimelineLength());
    if (clamped == m_scrollOffsetSamples) return;
    m_scrollOffsetSamples = clamped;
    if (m_dragMode == DragMode::None) update();
    emit scrollOffsetChanged(m_scrollOffsetSamples);
}

int64_t ClipLaneWidget::maxScrollOffsetSamples() const {
    return rsd::maxScrollOffsetSamples(m_contentExtentSamples, effectiveTimelineLength());
}

void ClipLaneWidget::setPlayheadSample(int64_t sample) {
    m_playheadSample = sample;

    // Auto-scroll to follow the playhead whenever it lands outside this
    // lane's own visible window — forward (Playing/Recording ran past the
    // right edge) or backward (a seek/rewind landed before the left edge,
    // e.g. jumping back to the start after letting playback run far past
    // the session's content — previously this left the lane stuck scrolled
    // forward with no way back short of restarting the app). Each lane
    // scrolls independently (matching the existing per-lane scroll model —
    // there's no shared/global scroll, and the ruler itself has no scroll
    // concept at all).
    if (m_dragMode == DragMode::None) {
        if (sample > effectiveScrollOffset() + effectiveTimelineLength()) {
            // m_contentExtentSamples is only refreshed by MainWindow at
            // specific action points (add/remove track, zoom, etc.), never
            // continuously while recording — without this, the scroll
            // below gets silently clamped back to the pre-recording range
            // by setScrollOffsetSamples(), since it has no idea the clip
            // (and thus the timeline) has grown.
            m_contentExtentSamples = std::max(m_contentExtentSamples, sample);
        }
        int64_t revealedOffset =
            scrollOffsetToRevealPlayhead(sample, effectiveScrollOffset(), effectiveTimelineLength());
        if (revealedOffset != effectiveScrollOffset()) {
            setScrollOffsetSamples(revealedOffset);
        }
    }

    update();
}

int64_t ClipLaneWidget::xToSample(int x) const {
    int64_t total = effectiveTimelineLength();
    if (width() <= 0) return 0;
    return effectiveScrollOffset() + static_cast<int64_t>(static_cast<double>(x) / width() * total);
}

int ClipLaneWidget::sampleToX(int64_t sample) const {
    int64_t total = effectiveTimelineLength();
    if (total <= 0) return 0;
    return static_cast<int>(static_cast<double>(sample - effectiveScrollOffset()) / total * width());
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

std::shared_ptr<MidiNote> ClipLaneWidget::findMidiNoteAt(int64_t sample, int y) const {
    auto notes = m_track->midiClipsSnapshot();
    int laneHeight = height() - 8;
    int noteH = std::max(3, noteRowHeight(laneHeight, 36, 96));
    for (auto& n : *notes) {
        if (sample < n->startSample || sample >= n->startSample + n->lengthSamples) continue;
        int noteY = pitchToY(n->pitch, laneHeight, 36, 96) + 4;
        if (std::abs(y - noteY) <= noteH / 2 + 1) return n;
    }
    return nullptr;
}

void ClipLaneWidget::paintMidiNotes(QPainter& painter) {
    auto notes = m_track->midiClipsSnapshot();
    if (notes->empty()) {
        painter.setPen(QColor(120, 120, 120));
        painter.drawText(rect(), Qt::AlignCenter, "No notes — play the on-screen keyboard while armed+recording");
        return;
    }

    int laneHeight = height() - 8;
    int noteH = std::max(3, noteRowHeight(laneHeight, 36, 96));

    for (auto& note : *notes) {
        int x0 = sampleToX(note->startSample);
        int x1 = sampleToX(note->startSample + note->lengthSamples);
        int w = std::max(2, x1 - x0);
        int y = pitchToY(note->pitch, laneHeight, 36, 96) + 4;

        int velocityGreen = 140 + static_cast<int>(std::clamp(note->velocity, 0.0f, 1.0f) * 90.0f);
        bool selected = note->id == m_selectedMidiNoteId;
        painter.setPen(selected ? QPen(QColor(255, 210, 90), 2) : Qt::NoPen);
        painter.setBrush(QColor(90, velocityGreen, 90));
        painter.drawRect(x0, y - noteH / 2, w, noteH);
    }
}

void ClipLaneWidget::paintGridLines(QPainter& painter) {
    int64_t total = effectiveTimelineLength();
    if (total <= 0 || m_sampleRate <= 0 || width() <= 0) return;

    double totalSeconds = static_cast<double>(total) / m_sampleRate;
    double tickSeconds = niceTickSeconds(totalSeconds, width());

    int64_t offset = effectiveScrollOffset();
    double offsetSeconds = static_cast<double>(offset) / m_sampleRate;
    // Start from the tick at/just before the visible window so gridlines
    // stay anchored to absolute time as this lane is scrolled, rather than
    // always starting a fresh tick at the left edge.
    double startSeconds = std::floor(offsetSeconds / tickSeconds) * tickSeconds;

    painter.setPen(QColor(55, 55, 55));
    for (double t = startSeconds; t <= offsetSeconds + totalSeconds; t += tickSeconds) {
        if (t < 0) continue;
        int x = sampleToX(static_cast<int64_t>(t * m_sampleRate));
        if (x < 0 || x > width()) continue;
        painter.drawLine(x, 0, x, height());
    }
}

static float busMeterDecay(float current, float incoming) {
    incoming = std::clamp(incoming, 0.0f, 1.0f);
    return incoming > current ? incoming : current * 0.85f;
}

void ClipLaneWidget::updateBusMeter(float peakL, float peakR) {
    m_busDisplayL = busMeterDecay(m_busDisplayL, peakL);
    m_busDisplayR = busMeterDecay(m_busDisplayR, peakR);
    update();
}

void ClipLaneWidget::setSenderLabel(const QString& label) {
    if (m_senderLabel == label) return;
    m_senderLabel = label;
    update();
}

void ClipLaneWidget::paintBusMeter(QPainter& painter) {
    // Solid amber, not the mixer's level-based green/yellow/red — this lane
    // isn't a normal audio track, so its fill deliberately reads as a
    // different kind of thing rather than "loud vs quiet".
    static const QColor kBusMeterColor(220, 150, 60);
    static const QColor kBusMeterTrack(50, 42, 30);

    int barGap = 3;
    int barHeight = (height() - 8 - barGap) / 2;
    int barY0 = 4;
    int barY1 = 4 + barHeight + barGap;
    int barWidth = std::max(0, width() - 8);

    painter.fillRect(4, barY0, barWidth, barHeight, kBusMeterTrack);
    painter.fillRect(4, barY1, barWidth, barHeight, kBusMeterTrack);

    int filledL = static_cast<int>(m_busDisplayL * barWidth);
    int filledR = static_cast<int>(m_busDisplayR * barWidth);
    if (filledL > 0) painter.fillRect(4, barY0, filledL, barHeight, kBusMeterColor);
    if (filledR > 0) painter.fillRect(4, barY1, filledR, barHeight, kBusMeterColor);

    QString label = m_senderLabel.isEmpty() ? QStringLiteral("Bus — no tracks routed to it")
                                             : m_senderLabel;
    painter.setPen(QColor(180, 150, 110));
    painter.drawText(rect().adjusted(8, 0, -8, 0), Qt::AlignHCenter | Qt::AlignVCenter, label);
}

void ClipLaneWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));
    paintGridLines(painter);

    if (m_track->kind == TrackKind::Instrument) {
        paintMidiNotes(painter);
        if (m_playheadSample >= 0) {
            int px = sampleToX(m_playheadSample);
            painter.setPen(QPen(QColor(230, 80, 80), 2));
            painter.drawLine(px, 0, px, height());
        }
        paintRangeSelection(painter);
        return;
    }

    if (m_track->kind == TrackKind::Bus) {
        paintBusMeter(painter);
        paintRangeSelection(painter);
        return;
    }

    auto clips = m_track->clipsSnapshot();
    if (clips->empty()) {
        painter.setPen(QColor(120, 120, 120));
        painter.drawText(rect(), Qt::AlignCenter, "No audio — Import or Record into this track");
        paintRangeSelection(painter);
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
            int64_t clampedLen =
                std::min(srcLen, clip->buffer->frameCount() - std::max<int64_t>(0, srcStart));
            if (clampedLen > 0) {
                auto drawChannel = [&](int channel, int centerY, float halfH) {
                    // Peaks only depend on (buffer identity, trimmed range,
                    // pixel width, channel) — while the playhead moves or
                    // the lane scrolls, none of that changes for a finished
                    // clip, so cache the result instead of rescanning
                    // potentially millions of samples on every ~33ms
                    // playhead-driven repaint (confirmed hotspot with many
                    // tracks). A still-growing live-recording clip gets a
                    // new clampedLen each tick, so it naturally bypasses the
                    // cache and stays live.
                    auto key = std::make_tuple(static_cast<const void*>(clip->buffer.get()),
                                                srcStart, clampedLen, w, channel);
                    QVector<WaveformCache::PeakPair> peaks;
                    auto cacheIt = m_peakCache.find(key);
                    if (cacheIt != m_peakCache.end()) {
                        peaks = cacheIt->second;
                    } else {
                        AudioBuffer sub;
                        sub.channels = clip->buffer->channels;
                        sub.sampleRate = clip->buffer->sampleRate;
                        sub.samples.assign(
                            clip->buffer->samples.begin() + srcStart * sub.channels,
                            clip->buffer->samples.begin() + (srcStart + clampedLen) * sub.channels);
                        peaks = WaveformCache::computePeaks(sub, w, channel);
                        if (m_peakCache.size() > 5000) m_peakCache.clear(); // bound unbounded growth
                        m_peakCache[key] = peaks;
                    }
                    // Gain scales the drawn waveform directly (can visually
                    // clip against the lane bounds above unity, same as
                    // Pro Tools/Audacity's clip-gain line) so the handle
                    // gives immediate visual feedback while dragging.
                    float displayScale = computeWaveformDisplayScale(peaks) * clip->gain;
                    // Distinct red vs the normal blue while still recording
                    // (see Clip::isLiveRecording), so it reads unambiguously
                    // as in-progress rather than a finished clip.
                    painter.setPen(clip->isLiveRecording ? QColor(230, 90, 90) : QColor(90, 170, 230));
                    for (int i = 0; i < peaks.size(); ++i) {
                        auto [minV, maxV] = peaks[i];
                        int yTop = centerY - static_cast<int>(maxV * displayScale * halfH);
                        int yBottom = centerY - static_cast<int>(minV * displayScale * halfH);
                        painter.drawLine(x0 + i, yTop, x0 + i, yBottom);
                    }
                };

                if (clip->buffer->channels >= 2) {
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

        int sr = clip->buffer ? clip->buffer->sampleRate : 0;
        QString durationText = sr > 0 ? formatDuration(clip->lengthSamples, sr) : QString();
        QString label = durationText.isEmpty() ? clip->name : clip->name + "  " + durationText;

        painter.setPen(QColor(200, 200, 200));
        painter.drawText(x0 + 4, 16, label);

        // Fade triangles: shade the faded-out region so the fade region and
        // its length are visible without needing to select the clip.
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 110));
        if (clip->fadeInSamples > 0) {
            int fadeW = std::max(1, sampleToX(clip->sessionStartSample + clip->fadeInSamples) - x0);
            QPolygon tri;
            tri << QPoint(x0, 4) << QPoint(x0 + fadeW, 4) << QPoint(x0, height() - 4);
            painter.drawPolygon(tri);
        }
        if (clip->fadeOutSamples > 0) {
            int fadeStartX =
                sampleToX(clip->sessionStartSample + clip->lengthSamples - clip->fadeOutSamples);
            QPolygon tri;
            tri << QPoint(fadeStartX, 4) << QPoint(x1, 4) << QPoint(x1, height() - 4);
            painter.drawPolygon(tri);
        }

        // Gain handle: a draggable horizontal line across the clip (4 =
        // unity at the vertical center, matching the fade triangles'
        // top/bottom insets).
        int lineY = gainLineY(clip->gain, 4, height() - 8);
        bool draggingGain = m_dragMode == DragMode::Gain && clip->id == m_dragClipId;
        painter.setPen(QPen(draggingGain ? QColor(255, 210, 100) : QColor(220, 180, 80), 2));
        painter.drawLine(x0, lineY, x1, lineY);
    }

    if (m_playheadSample >= 0) {
        int px = sampleToX(m_playheadSample);
        painter.setPen(QPen(QColor(230, 80, 80), 2));
        painter.drawLine(px, 0, px, height());
    }
    paintRangeSelection(painter);
}

void ClipLaneWidget::paintRangeSelection(QPainter& painter) {
    if (!m_rangeSelecting && !m_hasRangeSelection) return;
    int64_t start = std::min(m_rangeSelectionStart, m_rangeSelectionEnd);
    int64_t end = std::max(m_rangeSelectionStart, m_rangeSelectionEnd);
    int x0 = sampleToX(start);
    int x1 = sampleToX(end);
    painter.fillRect(x0, 0, std::max(1, x1 - x0), height(), QColor(255, 255, 255, 40));
    painter.setPen(QPen(QColor(255, 255, 255, 120), 1));
    painter.drawLine(x0, 0, x0, height());
    painter.drawLine(x1, 0, x1, height());
}

void ClipLaneWidget::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);

    // Only the left button starts a selection/drag/range-select gesture.
    // Right-click is handled entirely by contextMenuEvent (which does its
    // own hit-testing) — letting it fall through here used to also arm a
    // move/trim drag exactly like a left-click, and since a right-click's
    // matching release doesn't reliably reach this widget before the popup
    // menu's blocking exec() takes over, that drag state could survive the
    // menu and cause the clip to jump to the mouse on the next move.
    if (event->button() != Qt::LeftButton) {
        return;
    }

    if (event->modifiers() & Qt::ShiftModifier) {
        m_rangeSelecting = true;
        m_rangeSelectionStart = m_rangeSelectionEnd = xToSample(event->pos().x());
        m_hasRangeSelection = false;
        update();
        return;
    }

    int64_t sample = xToSample(event->pos().x());

    if (m_track->kind == TrackKind::Instrument) {
        // View-only lane: notes can be selected (for copy/paste) but not
        // dragged/trimmed/split like audio Clips.
        auto note = findMidiNoteAt(sample, event->pos().y());
        if (!note) {
            clearSelection();
            m_scrubbingPlayhead = true;
            emit seekRequested(sample);
            return;
        }
        m_selectedMidiNoteId = note->id;
        emit selectionChanged(true);
        update();
        return;
    }

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
    m_dragStartY = event->pos().y();
    m_dragOrigStart = clip->sessionStartSample;
    m_dragOrigOffset = clip->sourceOffsetSamples;
    m_dragOrigLength = clip->lengthSamples;
    m_dragOrigFadeIn = clip->fadeInSamples;
    m_dragOrigFadeOut = clip->fadeOutSamples;
    m_dragOrigGain = clip->gain;
    // Lock the timeline scale for the whole gesture — recomputing it from the
    // live (already-edited) clip state on every move causes the scale to
    // shift mid-drag, snowballing tiny mouse movements into huge trims.
    // Must match whatever scale is currently painting this lane (the shared
    // cross-track/zoom-aware one, when present) — locking the local
    // per-track estimate instead made the clip visibly jump/resize the
    // instant the drag started, before the mouse even moved.
    m_dragTotalSamples = resolveDragLockSamples(m_sharedTimelineLength, timelineLengthSamples());
    m_dragScrollOffsetSamples = effectiveScrollOffset();
    m_dragInitialScrollOffsetSamples = m_dragScrollOffsetSamples;

    bool nearTopBand = event->pos().y() <= 4 + kFadeHandleBandPx;
    if (nearTopBand && std::abs(event->pos().x() - x0) <= kEdgeThresholdPx) {
        m_dragMode = DragMode::FadeIn;
        setCursor(Qt::SizeHorCursor);
    } else if (nearTopBand && std::abs(event->pos().x() - x1) <= kEdgeThresholdPx) {
        m_dragMode = DragMode::FadeOut;
        setCursor(Qt::SizeHorCursor);
    } else if (std::abs(event->pos().x() - x0) <= kEdgeThresholdPx) {
        m_dragMode = DragMode::TrimStart;
        setCursor(Qt::SizeHorCursor);
    } else if (std::abs(event->pos().x() - x1) <= kEdgeThresholdPx) {
        m_dragMode = DragMode::TrimEnd;
        setCursor(Qt::SizeHorCursor);
    } else if (std::abs(event->pos().y() - gainLineY(clip->gain, 4, height() - 8)) <=
               kGainHandleBandPx) {
        m_dragMode = DragMode::Gain;
        setCursor(Qt::SizeVerCursor);
    } else {
        m_dragMode = DragMode::Move;
        setCursor(Qt::ClosedHandCursor);
    }

    m_editBeforeSnapshot = m_track->clipsSnapshot(); // pre-edit state, for undo
    emit editStarted();
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
    } else if (std::abs(pos.y() - gainLineY(clip->gain, 4, height() - 8)) <= kGainHandleBandPx) {
        setCursor(Qt::SizeVerCursor); // near the gain line: drag to adjust gain
    } else {
        setCursor(Qt::OpenHandCursor); // over the body: move
    }
}

void ClipLaneWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_rangeSelecting) {
        m_rangeSelectionEnd = xToSample(event->pos().x());
        update();
        return;
    }

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

    // Edge-pan while moving a clip: nudge the drag's frozen scroll offset
    // forward each move event the cursor spends within kEdgePanThresholdPx
    // of the right edge, revealing more space to drop into. Pans at a fixed
    // zoom (never touches m_dragTotalSamples), so existing on-screen clips
    // never rescale — only the visible window slides right.
    if (m_dragMode == DragMode::Move) {
        constexpr int kEdgePanThresholdPx = 30;
        if (event->pos().x() > width() - kEdgePanThresholdPx) {
            m_dragScrollOffsetSamples += total / 40;
        }
    }
    int64_t pannedSoFar = m_dragScrollOffsetSamples - m_dragInitialScrollOffsetSamples;
    int64_t deltaSamples =
        static_cast<int64_t>((event->pos().x() - m_dragStartX) / static_cast<double>(width()) * total) +
        pannedSoFar;

    auto clips = m_track->clipsSnapshot();
    std::shared_ptr<Clip> original;
    for (auto& c : *clips) {
        if (c->id == m_dragClipId) { original = c; break; }
    }
    if (!original) return;

    auto edited = std::make_shared<Clip>(*original);

    if (m_dragMode == DragMode::Move) {
        // Only a floor at 0 matters now — the old upper clamp
        // (m_dragTotalSamples - length) wrongly treated the frozen visible
        // window's span as an absolute session-wide position ceiling, which
        // blocked dragging a clip further right than whatever was visible
        // when the drag started. The track's content extent naturally grows
        // to include wherever the clip ends up once dropped, same as it
        // already does for any clip placed near the existing edge.
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
    } else if (m_dragMode == DragMode::FadeIn) {
        // Dragging the top-left handle rightward lengthens the fade-in.
        edited->fadeInSamples =
            clampFadeSamples(m_dragOrigFadeIn + deltaSamples, m_dragOrigLength, m_dragOrigFadeOut);
    } else if (m_dragMode == DragMode::FadeOut) {
        // Dragging the top-right handle leftward lengthens the fade-out.
        edited->fadeOutSamples =
            clampFadeSamples(m_dragOrigFadeOut - deltaSamples, m_dragOrigLength, m_dragOrigFadeIn);
    } else if (m_dragMode == DragMode::Gain) {
        int deltaY = event->pos().y() - m_dragStartY;
        edited->gain = gainAfterVerticalDrag(m_dragOrigGain, deltaY, height());
    }

    m_track->replaceClip(m_dragClipId, edited);
    update();

    if (m_dragMode == DragMode::Move) {
        emit clipDraggedToGlobalPos(m_dragClipId, event->globalPosition().toPoint());
    }
}

void ClipLaneWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (m_rangeSelecting) {
        m_rangeSelecting = false;
        m_hasRangeSelection = m_rangeSelectionEnd != m_rangeSelectionStart;
        update();
        return;
    }

    bool wasEditing = m_dragMode != DragMode::None;

    if (m_dragMode == DragMode::Move) {
        emit clipDropped(m_dragClipId, event->globalPosition().toPoint());
        // Persist any edge-pan from this drag, otherwise effectiveScrollOffset()
        // would snap back to the pre-drag position the instant m_dragMode
        // clears below, making the just-dropped clip appear to vanish off
        // the right edge.
        if (m_dragScrollOffsetSamples != m_dragInitialScrollOffsetSamples) {
            // The moved clip may now sit past what m_contentExtentSamples
            // (only refreshed by MainWindow at specific action points, not
            // after every plain in-lane drag) currently accounts for — grow
            // it locally first so the offset below isn't clamped back down.
            m_contentExtentSamples = std::max(m_contentExtentSamples, timelineLengthSamples());
            setScrollOffsetSamples(m_dragScrollOffsetSamples);
        }
    }
    m_dragMode = DragMode::None;
    m_scrubbingPlayhead = false;
    updateHoverCursor(event->pos());

    // If clipDropped() above reassigned the clip to a different track (the
    // emit chain runs synchronously), MainWindow::onClipMovedToTrack already
    // pushed its own command covering both tracks — nothing to push here.
    auto after = m_track->clipsSnapshot();
    bool clipStillHere = std::any_of(after->begin(), after->end(),
                                      [&](const auto& c) { return c->id == m_dragClipId; });
    if (wasEditing && clipStillHere && m_editBeforeSnapshot && m_commandStack &&
        after != m_editBeforeSnapshot) {
        m_commandStack->push(std::make_unique<TrackClipsCommand>(m_track, m_editBeforeSnapshot, after));
    }
    m_editBeforeSnapshot.reset();
}

void ClipLaneWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    int64_t sample = xToSample(event->pos().x());
    auto clip = findClipAt(sample);
    if (!clip) return;

    auto before = m_track->clipsSnapshot();
    m_track->splitClip(clip->id, sample);
    if (m_commandStack) {
        m_commandStack->push(
            std::make_unique<TrackClipsCommand>(m_track, before, m_track->clipsSnapshot(), "Split Clip"));
    }
    m_selectedClipId = QUuid();
    emit selectionChanged(false);
    update();
}

void ClipLaneWidget::wheelEvent(QWheelEvent* event) {
    // Only a native horizontal delta (trackpad two-finger swipe, or a mouse
    // with a tilt wheel) pans the lane — plain vertical wheel is left alone
    // so it still scrolls the TimelineView's outer QScrollArea normally.
    int deltaPx = event->angleDelta().x();
    if (deltaPx == 0 || width() <= 0 || maxScrollOffsetSamples() <= 0) {
        QWidget::wheelEvent(event);
        return;
    }

    int64_t total = effectiveTimelineLength();
    int64_t deltaSamples = static_cast<int64_t>(deltaPx / static_cast<double>(width()) * total);
    setScrollOffsetSamples(m_scrollOffsetSamples - deltaSamples);

    if (event->modifiers() & Qt::ShiftModifier) {
        emit syncScrollToAllRequested(m_scrollOffsetSamples);
    }
    event->accept();
}

void ClipLaneWidget::contextMenuEvent(QContextMenuEvent* event) {
    int64_t sample = xToSample(event->pos().x());

    if (m_track->kind == TrackKind::Instrument) {
        auto note = findMidiNoteAt(sample, event->pos().y());
        if (!note) return;

        QMenu menu(this);
        QAction* repeatAction = menu.addAction("Repeat...");
        if (menu.exec(event->globalPos()) != repeatAction) return;

        repeatMidiNote(note);
        return;
    }

    auto clip = findClipAt(sample);
    if (!clip) return;

    QMenu menu(this);
    QAction* gainAction = menu.addAction("Set Gain...");
    QAction* repeatAction = menu.addAction("Repeat...");
    QAction* chosen = menu.exec(event->globalPos());
    if (!chosen) return;

    if (chosen == gainAction) {
        bool ok = false;
        double newGain = QInputDialog::getDouble(this, "Clip Gain", "Gain (0.0 - 2.0):", clip->gain, 0.0,
                                                  2.0, 2, &ok);
        if (!ok) return;

        if (m_commandStack) {
            m_commandStack->push(std::make_unique<SetClipGainCommand>(m_track, clip->id, clip->gain,
                                                                        static_cast<float>(newGain)));
        } else {
            auto edited = std::make_shared<Clip>(*clip);
            edited->gain = static_cast<float>(newGain);
            m_track->replaceClip(clip->id, edited);
        }
        update();
    } else if (chosen == repeatAction) {
        repeatClip(clip);
    }
}

void ClipLaneWidget::repeatClip(const std::shared_ptr<Clip>& clip) {
    bool ok = false;
    int count = QInputDialog::getInt(this, "Repeat Clip", "Number of repeats:", 1, 1, 999, 1, &ok);
    if (!ok) return;

    auto before = m_track->clipsSnapshot();
    int64_t insertAt = clip->sessionStartSample + clip->lengthSamples;
    auto after = repeatItemAfter<Clip>(*before, *clip, insertAt, count);
    m_track->restoreClips(std::make_shared<const Track::ClipList>(std::move(after)));
    if (m_commandStack) {
        m_commandStack->push(
            std::make_unique<TrackClipsCommand>(m_track, before, m_track->clipsSnapshot(), "Repeat Clip"));
    }
    update();
}

void ClipLaneWidget::repeatMidiNote(const std::shared_ptr<MidiNote>& note) {
    bool ok = false;
    int count = QInputDialog::getInt(this, "Repeat Note", "Number of repeats:", 1, 1, 999, 1, &ok);
    if (!ok) return;

    auto before = m_track->midiClipsSnapshot();
    int64_t insertAt = note->startSample + note->lengthSamples;
    auto after = repeatItemAfter<MidiNote>(*before, *note, insertAt, count);
    m_track->restoreMidiClips(std::make_shared<const Track::MidiNoteList>(std::move(after)));
    if (m_commandStack) {
        m_commandStack->push(std::make_unique<TrackMidiCommand>(m_track, before, m_track->midiClipsSnapshot(),
                                                                  "Repeat Note"));
    }
    update();
}

void ClipLaneWidget::deleteSelected() {
    if (m_selectedClipId.isNull()) return;
    auto before = m_track->clipsSnapshot();
    m_track->removeClip(m_selectedClipId);
    if (m_commandStack) {
        m_commandStack->push(
            std::make_unique<TrackClipsCommand>(m_track, before, m_track->clipsSnapshot(), "Delete Clip"));
    }
    m_selectedClipId = QUuid();
    emit selectionChanged(false);
    update();
}

void ClipLaneWidget::clearSelection() {
    m_selectedClipId = QUuid();
    m_selectedMidiNoteId = QUuid();
    emit selectionChanged(false);
    update();
}

void ClipLaneWidget::copyRangeSelection() {
    if (!m_hasRangeSelection) return;
    int64_t start = std::min(m_rangeSelectionStart, m_rangeSelectionEnd);
    int64_t end = std::max(m_rangeSelectionStart, m_rangeSelectionEnd);

    if (m_track->kind == TrackKind::Instrument) {
        EditClipboardStore::instance().setMidi(extractRange<MidiNote>(*m_track->midiClipsSnapshot(), start, end));
    } else {
        EditClipboardStore::instance().setClips(extractRange<Clip>(*m_track->clipsSnapshot(), start, end));
    }
}

void ClipLaneWidget::pasteAtPlayhead() {
    if (m_playheadSample < 0) return;
    auto& store = EditClipboardStore::instance();

    if (m_track->kind == TrackKind::Instrument && store.hasMidi()) {
        auto before = m_track->midiClipsSnapshot();
        auto after = pasteRange<MidiNote>(*before, store.midi(), m_playheadSample);
        m_track->restoreMidiClips(std::make_shared<const Track::MidiNoteList>(std::move(after)));
        if (m_commandStack) {
            m_commandStack->push(std::make_unique<TrackMidiCommand>(m_track, before,
                                                                      m_track->midiClipsSnapshot(), "Paste"));
        }
        update();
    } else if (m_track->kind != TrackKind::Instrument && store.hasClips()) {
        auto before = m_track->clipsSnapshot();
        auto after = pasteRange<Clip>(*before, store.clips(), m_playheadSample);
        m_track->restoreClips(std::make_shared<const Track::ClipList>(std::move(after)));
        if (m_commandStack) {
            m_commandStack->push(
                std::make_unique<TrackClipsCommand>(m_track, before, m_track->clipsSnapshot(), "Paste"));
        }
        update();
    }
}

void ClipLaneWidget::copySelectedItem() {
    auto& store = EditClipboardStore::instance();

    if (m_track->kind == TrackKind::Instrument) {
        if (m_selectedMidiNoteId.isNull()) return;
        for (auto& n : *m_track->midiClipsSnapshot()) {
            if (n && n->id == m_selectedMidiNoteId) {
                store.setSingleMidi(
                    {std::make_shared<MidiNote>(*n), n->startSample + n->lengthSamples, m_track->id});
                return;
            }
        }
    } else {
        if (m_selectedClipId.isNull()) return;
        for (auto& c : *m_track->clipsSnapshot()) {
            if (c && c->id == m_selectedClipId) {
                store.setSingleClip(
                    {std::make_shared<Clip>(*c), c->sessionStartSample + c->lengthSamples, m_track->id});
                return;
            }
        }
    }
}

void ClipLaneWidget::pasteChainedSingleItem() {
    auto& store = EditClipboardStore::instance();

    if (m_track->kind == TrackKind::Instrument) {
        if (!store.hasSingleMidi() || store.singleMidi().sourceTrackId != m_track->id) return;
        const auto& pending = store.singleMidi();
        int64_t insertAt = pending.nextPasteSample;
        int64_t length = pending.item->lengthSamples;

        auto before = m_track->midiClipsSnapshot();
        auto after = insertItemAfter<MidiNote>(*before, *pending.item, insertAt);
        m_track->restoreMidiClips(std::make_shared<const Track::MidiNoteList>(std::move(after)));
        if (m_commandStack) {
            m_commandStack->push(std::make_unique<TrackMidiCommand>(m_track, before,
                                                                      m_track->midiClipsSnapshot(), "Paste"));
        }
        store.advanceSingleMidi(insertAt + length);
        update();
    } else {
        if (!store.hasSingleClip() || store.singleClip().sourceTrackId != m_track->id) return;
        const auto& pending = store.singleClip();
        int64_t insertAt = pending.nextPasteSample;
        int64_t length = pending.item->lengthSamples;

        auto before = m_track->clipsSnapshot();
        auto after = insertItemAfter<Clip>(*before, *pending.item, insertAt);
        m_track->restoreClips(std::make_shared<const Track::ClipList>(std::move(after)));
        if (m_commandStack) {
            m_commandStack->push(
                std::make_unique<TrackClipsCommand>(m_track, before, m_track->clipsSnapshot(), "Paste"));
        }
        store.advanceSingleClip(insertAt + length);
        update();
    }
}

void ClipLaneWidget::keyPressEvent(QKeyEvent* event) {
    if (event->matches(QKeySequence::Copy)) {
        if (m_hasRangeSelection) {
            copyRangeSelection();
        } else {
            copySelectedItem();
        }
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        auto& store = EditClipboardStore::instance();
        if (store.hasMidi() || store.hasClips()) {
            pasteAtPlayhead();
        } else {
            pasteChainedSingleItem();
        }
        return;
    }
    QWidget::keyPressEvent(event);
}

void ClipLaneWidget::dragEnterEvent(QDragEnterEvent* event) {
    // Two sources: an existing MediaBrowserPanel entry (by index), or a
    // plain file drop — either straight from a file manager, or from the
    // loop browser (which sets standard QUrl mime data, indistinguishable
    // from an OS drag, so it needs no special-cased MIME type of its own).
    if (event->mimeData()->hasFormat(MediaBrowserPanel::kMimeType) || event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void ClipLaneWidget::dropEvent(QDropEvent* event) {
    int64_t sample = xToSample(event->position().toPoint().x());

    if (event->mimeData()->hasFormat(MediaBrowserPanel::kMimeType)) {
        bool ok = false;
        int libraryIndex = event->mimeData()->data(MediaBrowserPanel::kMimeType).toInt(&ok);
        if (!ok) return;
        emit mediaDropped(libraryIndex, sample);
        event->acceptProposedAction();
        return;
    }

    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            if (!url.isLocalFile()) continue;
            emit externalFileDropped(url.toLocalFile(), sample);
            break; // one clip per drop, even if several files were dragged together
        }
        event->acceptProposedAction();
    }
}

QString ClipLaneWidget::formatDuration(int64_t samples, int sampleRate) {
    if (sampleRate <= 0) return QString();
    double totalSeconds = static_cast<double>(samples) / sampleRate;
    int mins = static_cast<int>(totalSeconds) / 60;
    double secs = totalSeconds - mins * 60;
    return QString("%1:%2").arg(mins).arg(secs, 4, 'f', 1, QChar('0'));
}

bool ClipLaneWidget::event(QEvent* ev) {
    if (ev->type() == QEvent::ToolTip) {
        auto* helpEvent = static_cast<QHelpEvent*>(ev);
        int64_t sample = xToSample(helpEvent->pos().x());
        auto clip = findClipAt(sample);

        if (clip && clip->buffer) {
            QString text = QString("%1\n"
                                    "Duration: %2\n"
                                    "Trim start: %3\n"
                                    "Sample rate: %4 Hz\n"
                                    "Channels: %5\n"
                                    "Source length: %6")
                                .arg(clip->name)
                                .arg(formatDuration(clip->lengthSamples, clip->buffer->sampleRate))
                                .arg(formatDuration(clip->sourceOffsetSamples, clip->buffer->sampleRate))
                                .arg(clip->buffer->sampleRate)
                                .arg(clip->buffer->channels)
                                .arg(formatDuration(clip->buffer->frameCount(), clip->buffer->sampleRate));
            QToolTip::showText(helpEvent->globalPos(), text, this);
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(ev);
}

} // namespace rsd
