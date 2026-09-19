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

    m_selectGroup->addButton(row->selectButton());
    // Insert before the trailing stretch.
    m_layout->insertWidget(m_layout->count() - 1, row);
    m_rows[track->id.toString()] = row;

    if (m_rows.size() == 1) {
        row->selectButton()->setChecked(true);
    }
}

void TimelineView::removeTrack(const QUuid& trackId) {
    auto it = m_rows.find(trackId.toString());
    if (it == m_rows.end()) return;

    m_selectGroup->removeButton(it->second->selectButton());
    m_layout->removeWidget(it->second);
    it->second->deleteLater();
    m_rows.erase(it);
}

void TimelineView::refreshTrackWaveform(const QUuid& trackId, std::shared_ptr<AudioBuffer> buffer) {
    auto it = m_rows.find(trackId.toString());
    if (it == m_rows.end()) return;
    it->second->setWaveformBuffer(std::move(buffer));
}

} // namespace rsd
