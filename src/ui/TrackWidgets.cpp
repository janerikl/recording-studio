#include "TrackWidgets.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace rsd {

TrackRowWidget::TrackRowWidget(std::shared_ptr<Track> track, QWidget* parent)
    : QWidget(parent), m_track(std::move(track)) {
    auto* rowLayout = new QHBoxLayout(this);

    auto* header = new QWidget(this);
    auto* headerLayout = new QVBoxLayout(header);
    header->setFixedWidth(160);

    auto* nameLabel = new QLabel(m_track->name, header);
    headerLayout->addWidget(nameLabel);

    m_selectButton = new QRadioButton("Active", header);
    connect(m_selectButton, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) emit selected(m_track);
    });
    headerLayout->addWidget(m_selectButton);

    m_muteBox = new QCheckBox("Mute", header);
    connect(m_muteBox, &QCheckBox::toggled, this,
            [this](bool checked) { m_track->muted.store(checked, std::memory_order_relaxed); });
    headerLayout->addWidget(m_muteBox);

    m_soloBox = new QCheckBox("Solo", header);
    connect(m_soloBox, &QCheckBox::toggled, this,
            [this](bool checked) { m_track->soloed.store(checked, std::memory_order_relaxed); });
    headerLayout->addWidget(m_soloBox);

    m_armBox = new QCheckBox("Rec Arm", header);
    connect(m_armBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_track->recordArmed.store(checked, std::memory_order_relaxed);
    });
    headerLayout->addWidget(m_armBox);

    rowLayout->addWidget(header);

    m_clipLane = new ClipLaneWidget(m_track, this);
    connect(m_clipLane, &ClipLaneWidget::selectionChanged, this,
            [this](bool hasSelection) { emit clipSelectionChanged(m_track, hasSelection); });
    rowLayout->addWidget(m_clipLane, 1);
}

} // namespace rsd
