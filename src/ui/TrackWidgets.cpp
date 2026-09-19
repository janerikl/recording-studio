#include "TrackWidgets.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace rsd {

TrackRowWidget::TrackRowWidget(std::shared_ptr<Track> track, QWidget* parent)
    : QWidget(parent), m_track(std::move(track)) {
    auto* rowLayout = new QHBoxLayout(this);

    auto* header = new QWidget(this);
    auto* headerLayout = new QVBoxLayout(header);
    header->setFixedWidth(200);

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

    // Pan is a convenience gesture, not a stored value: moving it derives and
    // writes both gainL/gainR via a simple linear pan law. The two gain
    // sliders are the actual source of truth read by the mixer, so adjusting
    // one directly doesn't move the dial back (not every L/R pair has a
    // matching symmetric pan angle).
    m_panDial = new QDial(header);
    m_panDial->setRange(-100, 100);
    m_panDial->setValue(0);
    m_panDial->setToolTip("Pan");
    m_panDial->setFixedSize(48, 48);
    connect(m_panDial, &QDial::valueChanged, this, [this](int v) {
        float p = v / 100.0f;
        float gl = p <= 0 ? 1.0f : 1.0f - p;
        float gr = p >= 0 ? 1.0f : 1.0f + p;
        m_track->gainL.store(gl, std::memory_order_relaxed);
        m_track->gainR.store(gr, std::memory_order_relaxed);
        if (m_gainLSlider) m_gainLSlider->setValue(static_cast<int>(gl * 100));
        if (m_gainRSlider) m_gainRSlider->setValue(static_cast<int>(gr * 100));
    });
    headerLayout->addWidget(m_panDial);

    m_gainLSlider = new QSlider(Qt::Horizontal, header);
    m_gainLSlider->setRange(0, 200);
    m_gainLSlider->setValue(static_cast<int>(m_track->gainL.load() * 100));
    m_gainLSlider->setToolTip("Gain L");
    connect(m_gainLSlider, &QSlider::valueChanged, this,
            [this](int v) { m_track->gainL.store(v / 100.0f, std::memory_order_relaxed); });
    headerLayout->addWidget(m_gainLSlider);

    m_gainRSlider = new QSlider(Qt::Horizontal, header);
    m_gainRSlider->setRange(0, 200);
    m_gainRSlider->setValue(static_cast<int>(m_track->gainR.load() * 100));
    m_gainRSlider->setToolTip("Gain R");
    connect(m_gainRSlider, &QSlider::valueChanged, this,
            [this](int v) { m_track->gainR.store(v / 100.0f, std::memory_order_relaxed); });
    headerLayout->addWidget(m_gainRSlider);

    rowLayout->addWidget(header);

    m_clipLane = new ClipLaneWidget(m_track, this);
    connect(m_clipLane, &ClipLaneWidget::selectionChanged, this,
            [this](bool hasSelection) { emit clipSelectionChanged(m_track, hasSelection); });
    rowLayout->addWidget(m_clipLane, 1);
}

} // namespace rsd
