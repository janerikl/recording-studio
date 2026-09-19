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

void TimelineView::addTrack(std::shared_ptr<Track> track) {
    auto* row = new TrackRowWidget(track, m_content);
    connect(row, &TrackRowWidget::selected, this, &TimelineView::trackSelected);
    connect(row, &TrackRowWidget::clipSelectionChanged, this, &TimelineView::clipSelectionChanged);
    connect(row->clipLane(), &ClipLaneWidget::seekRequested, this, &TimelineView::seekRequested);
    connect(row->clipLane(), &ClipLaneWidget::editStarted, this, &TimelineView::editStarted);

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

} // namespace rsd
