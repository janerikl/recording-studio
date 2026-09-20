#include "InstrumentPanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include "ui/PianoKeyboardWidget.h"

namespace rsd {

namespace {
QSlider* addSlider(QVBoxLayout* layout, const QString& labelText, int minV, int maxV, int value) {
    auto* row = new QWidget;
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    auto* label = new QLabel(labelText);
    label->setMinimumWidth(90);
    auto* slider = new QSlider(Qt::Horizontal);
    slider->setRange(minV, maxV);
    slider->setValue(value);
    rowLayout->addWidget(label);
    rowLayout->addWidget(slider);
    layout->addWidget(row);
    return slider;
}
} // namespace

InstrumentPanel::InstrumentPanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(
        "InstrumentPanel { background: #1e1e1e; }"
        "QLabel { color: #cccccc; }"
        "QComboBox { background: #2a2a2a; color: #e0e0e0; border: 1px solid #4a4a4a; }");

    auto* outer = new QVBoxLayout(this);

    m_trackNameLabel = new QLabel("No instrument track selected");
    m_trackNameLabel->setStyleSheet("font-weight: bold; color: #f0f0f0;");
    outer->addWidget(m_trackNameLabel);

    m_paramsContainer = new QWidget;
    auto* paramsLayout = new QVBoxLayout(m_paramsContainer);
    paramsLayout->setContentsMargins(0, 0, 0, 0);

    auto* waveRow = new QWidget;
    auto* waveLayout = new QHBoxLayout(waveRow);
    waveLayout->setContentsMargins(0, 0, 0, 0);
    waveLayout->addWidget(new QLabel("Waveform"));
    m_waveformCombo = new QComboBox;
    m_waveformCombo->addItem("Sine", static_cast<int>(SynthWaveform::Sine));
    m_waveformCombo->addItem("Saw", static_cast<int>(SynthWaveform::Saw));
    m_waveformCombo->addItem("Square", static_cast<int>(SynthWaveform::Square));
    m_waveformCombo->addItem("Triangle", static_cast<int>(SynthWaveform::Triangle));
    waveLayout->addWidget(m_waveformCombo);
    paramsLayout->addWidget(waveRow);
    connect(m_waveformCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_track) m_track->synthParams.waveform.store(m_waveformCombo->currentData().toInt());
    });

    // Sliders store milliseconds/percent as ints; converted to the atomic's
    // native seconds/0-1 float units in each valueChanged handler.
    m_attackSlider = addSlider(paramsLayout, "Attack (ms)", 1, 2000, 10);
    connect(m_attackSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_track) m_track->synthParams.attackSeconds.store(v / 1000.0f);
    });
    m_decaySlider = addSlider(paramsLayout, "Decay (ms)", 1, 2000, 100);
    connect(m_decaySlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_track) m_track->synthParams.decaySeconds.store(v / 1000.0f);
    });
    m_sustainSlider = addSlider(paramsLayout, "Sustain (%)", 0, 100, 70);
    connect(m_sustainSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_track) m_track->synthParams.sustainLevel.store(v / 100.0f);
    });
    m_releaseSlider = addSlider(paramsLayout, "Release (ms)", 1, 3000, 200);
    connect(m_releaseSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_track) m_track->synthParams.releaseSeconds.store(v / 1000.0f);
    });
    m_filterSlider = addSlider(paramsLayout, "Filter (Hz)", 100, 15000, 8000);
    connect(m_filterSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_track) m_track->synthParams.filterCutoffHz.store(static_cast<float>(v));
    });

    outer->addWidget(m_paramsContainer);
    outer->addStretch();

    m_keyboard = new PianoKeyboardWidget(this);
    connect(m_keyboard, &PianoKeyboardWidget::noteOn, this, &InstrumentPanel::noteOn);
    connect(m_keyboard, &PianoKeyboardWidget::noteOff, this, &InstrumentPanel::noteOff);
    outer->addWidget(m_keyboard);

    setTrack(nullptr);
}

void InstrumentPanel::setTrack(std::shared_ptr<Track> track) {
    m_track = std::move(track);
    rebuild();
}

void InstrumentPanel::rebuild() {
    bool isInstrument = m_track && m_track->kind == TrackKind::Instrument;
    m_trackNameLabel->setText(isInstrument ? (m_track->name.isEmpty() ? "Instrument Track" : m_track->name)
                                            : "No instrument track selected");
    m_paramsContainer->setEnabled(isInstrument);
    m_keyboard->setEnabled(isInstrument);

    if (!isInstrument) return;

    const QSignalBlocker b1(m_waveformCombo);
    const QSignalBlocker b2(m_attackSlider);
    const QSignalBlocker b3(m_decaySlider);
    const QSignalBlocker b4(m_sustainSlider);
    const QSignalBlocker b5(m_releaseSlider);
    const QSignalBlocker b6(m_filterSlider);

    int wfIndex = m_waveformCombo->findData(m_track->synthParams.waveform.load());
    if (wfIndex >= 0) m_waveformCombo->setCurrentIndex(wfIndex);
    m_attackSlider->setValue(static_cast<int>(m_track->synthParams.attackSeconds.load() * 1000.0f));
    m_decaySlider->setValue(static_cast<int>(m_track->synthParams.decaySeconds.load() * 1000.0f));
    m_sustainSlider->setValue(static_cast<int>(m_track->synthParams.sustainLevel.load() * 100.0f));
    m_releaseSlider->setValue(static_cast<int>(m_track->synthParams.releaseSeconds.load() * 1000.0f));
    m_filterSlider->setValue(static_cast<int>(m_track->synthParams.filterCutoffHz.load()));
}

} // namespace rsd
