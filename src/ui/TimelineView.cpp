#include "TimelineView.h"

namespace rsd {

TimelineView::TimelineView(QWidget* parent) : QScrollArea(parent) {
    m_content = new QWidget(this);
    m_layout = new QVBoxLayout(m_content);
    m_layout->addStretch();

    setWidget(m_content);
    setWidgetResizable(true);

    m_selectGroup = new QButtonGroup(this);
}

void TimelineView::setCommandStack(CommandStack* stack) {
    m_commandStack = stack;
    for (auto& [id, row] : m_rows) row->setCommandStack(stack);
}

void TimelineView::addTrack(std::shared_ptr<Track> track) {
    auto* row = new TrackRowWidget(track, m_content);
    row->setCommandStack(m_commandStack);
    connect(row, &TrackRowWidget::selected, this, &TimelineView::trackSelected);
    connect(row, &TrackRowWidget::clipSelectionChanged, this, &TimelineView::clipSelectionChanged);
    connect(row->clipLane(), &ClipLaneWidget::seekRequested, this, &TimelineView::seekRequested);
    connect(row->clipLane(), &ClipLaneWidget::editStarted, this, &TimelineView::editStarted);
    connect(row->clipLane(), &ClipLaneWidget::clipDraggedToGlobalPos, this,
            &TimelineView::onClipDraggedToGlobalPos);
    connect(row->clipLane(), &ClipLaneWidget::clipDropped, this, &TimelineView::onClipDropped);
    connect(row->clipLane(), &ClipLaneWidget::mediaDropped, this, &TimelineView::onMediaDropped);

    m_selectGroup->addButton(row->selectButton());
    // Insert before the trailing stretch.
    m_layout->insertWidget(m_layout->count() - 1, row);
    m_rows[track->id.toString()] = row;

    if (m_rows.size() == 1) {
        row->selectButton()->setChecked(true);
    }

    row->clipLane()->setSharedTimelineLength(m_lastTimelineLength);
    row->clipLane()->setPlayheadSample(m_lastPlayheadSample);
}

void TimelineView::removeTrack(const QUuid& trackId) {
    auto it = m_rows.find(trackId.toString());
    if (it == m_rows.end()) return;

    m_selectGroup->removeButton(it->second->selectButton());
    m_layout->removeWidget(it->second);
    it->second->deleteLater();
    m_rows.erase(it);
}

void TimelineView::clear() {
    for (auto& [id, row] : m_rows) {
        m_selectGroup->removeButton(row->selectButton());
        m_layout->removeWidget(row);
        row->deleteLater();
    }
    m_rows.clear();
}

void TimelineView::refreshTrackWaveform(const QUuid& trackId) {
    auto it = m_rows.find(trackId.toString());
    if (it == m_rows.end()) return;
    it->second->refreshWaveform();
}

void TimelineView::deleteSelectedClipOn(const QUuid& trackId) {
    auto it = m_rows.find(trackId.toString());
    if (it == m_rows.end()) return;
    it->second->clipLane()->deleteSelected();
}

void TimelineView::setSharedTimelineLength(int64_t samples) {
    m_lastTimelineLength = samples;
    for (auto& [id, row] : m_rows) row->clipLane()->setSharedTimelineLength(samples);
}

void TimelineView::setPlayheadSample(int64_t sample) {
    m_lastPlayheadSample = sample;
    for (auto& [id, row] : m_rows) row->clipLane()->setPlayheadSample(sample);
}

void TimelineView::clearSelectionOn(const QUuid& trackId) {
    auto it = m_rows.find(trackId.toString());
    if (it == m_rows.end()) return;
    it->second->clipLane()->clearSelection();
}

TrackRowWidget* TimelineView::rowForClipLane(QObject* clipLaneSender) const {
    for (auto& [id, row] : m_rows) {
        if (row->clipLane() == clipLaneSender) return row;
    }
    return nullptr;
}

TrackRowWidget* TimelineView::rowAtGlobalPos(const QPoint& globalPos) const {
    for (auto& [id, row] : m_rows) {
        QRect rowRect(row->mapToGlobal(QPoint(0, 0)), row->size());
        if (rowRect.contains(globalPos)) return row;
    }
    return nullptr;
}

void TimelineView::onClipDraggedToGlobalPos(QUuid /*clipId*/, QPoint globalPos) {
    auto* target = rowAtGlobalPos(globalPos);
    if (target == m_highlightedRow) return;

    if (m_highlightedRow) m_highlightedRow->setDropHighlight(false);
    m_highlightedRow = target;
    if (m_highlightedRow) m_highlightedRow->setDropHighlight(true);
}

void TimelineView::onClipDropped(QUuid clipId, QPoint globalPos) {
    if (m_highlightedRow) {
        m_highlightedRow->setDropHighlight(false);
        m_highlightedRow = nullptr;
    }

    auto* sourceRow = rowForClipLane(sender());
    auto* targetRow = rowAtGlobalPos(globalPos);
    if (!sourceRow || !targetRow || sourceRow == targetRow) return;

    emit clipMovedToTrack(clipId, sourceRow->track()->id, targetRow->track()->id);
}

void TimelineView::onMediaDropped(int libraryIndex, int64_t sessionStartSample) {
    auto* row = rowForClipLane(sender());
    if (!row) return;
    emit mediaDroppedOnTrack(row->track()->id, libraryIndex, sessionStartSample);
}

} // namespace rsd
