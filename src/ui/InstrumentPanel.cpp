#include "InstrumentPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "audio/GMInstruments.h"
#include "ui/DrumPadWidget.h"
#include "ui/PianoKeyboardWidget.h"
#include "ui/PracticePanel.h"

namespace rsd {

InstrumentPanel::InstrumentPanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(
        "InstrumentPanel { background: #1e1e1e; }"
        "QLabel { color: #cccccc; }"
        "QComboBox { background: #2a2a2a; color: #e0e0e0; border: 1px solid #4a4a4a; }"
        "QCheckBox { color: #cccccc; }");

    auto* outer = new QVBoxLayout(this);

    m_trackNameLabel = new QLabel("No instrument track selected");
    m_trackNameLabel->setStyleSheet("font-weight: bold; color: #f0f0f0;");
    outer->addWidget(m_trackNameLabel);

    m_paramsContainer = new QWidget;
    auto* paramsLayout = new QVBoxLayout(m_paramsContainer);
    paramsLayout->setContentsMargins(0, 0, 0, 0);

    m_drumKitCheck = new QCheckBox("Drum Kit");
    paramsLayout->addWidget(m_drumKitCheck);
    connect(m_drumKitCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_track) m_track->synthParams.isDrumKit.store(checked);
        m_instrumentCombo->setEnabled(!checked);
        m_playerStack->setCurrentWidget(checked ? static_cast<QWidget*>(m_drumPads)
                                                 : static_cast<QWidget*>(m_keyboard));
        // Practice exercises are melodic (scales/songs) — not meaningful
        // for a drum kit, so hide it and clear any pending highlight.
        m_practicePanel->setVisible(!checked);
        if (checked) m_keyboard->setExpectedPitch(std::nullopt);
    });

    auto* instrumentRow = new QWidget;
    auto* instrumentLayout = new QHBoxLayout(instrumentRow);
    instrumentLayout->setContentsMargins(0, 0, 0, 0);
    instrumentLayout->addWidget(new QLabel("Instrument"));
    m_instrumentCombo = new QComboBox;
    for (int i = 0; i < static_cast<int>(gmInstrumentNames().size()); ++i) {
        m_instrumentCombo->addItem(gmInstrumentNames()[i], i);
    }
    instrumentLayout->addWidget(m_instrumentCombo, 1);
    paramsLayout->addWidget(instrumentRow);
    connect(m_instrumentCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_track) m_track->synthParams.instrumentProgram.store(m_instrumentCombo->currentData().toInt());
    });

    outer->addWidget(m_paramsContainer);
    outer->addStretch();

    m_practicePanel = new PracticePanel(this);
    outer->addWidget(m_practicePanel);

    m_playerStack = new QStackedWidget(this);
    m_keyboard = new PianoKeyboardWidget(this);
    connect(m_keyboard, &PianoKeyboardWidget::noteOn, this, &InstrumentPanel::noteOn);
    connect(m_keyboard, &PianoKeyboardWidget::noteOff, this, &InstrumentPanel::noteOff);
    connect(m_keyboard, &PianoKeyboardWidget::noteOn, m_practicePanel,
            [this](int pitch, float) { m_practicePanel->checkNotePlayed(pitch); });
    connect(m_practicePanel, &PracticePanel::expectedPitchChanged, m_keyboard,
            &PianoKeyboardWidget::setExpectedPitch);
    m_playerStack->addWidget(m_keyboard);

    m_drumPads = new DrumPadWidget(this);
    connect(m_drumPads, &DrumPadWidget::noteOn, this, &InstrumentPanel::noteOn);
    connect(m_drumPads, &DrumPadWidget::noteOff, this, &InstrumentPanel::noteOff);
    m_playerStack->addWidget(m_drumPads);

    outer->addWidget(m_playerStack);

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
    m_playerStack->setEnabled(isInstrument);

    if (!isInstrument) return;

    const QSignalBlocker b1(m_drumKitCheck);
    const QSignalBlocker b2(m_instrumentCombo);

    bool isDrumKit = m_track->synthParams.isDrumKit.load();
    m_drumKitCheck->setChecked(isDrumKit);
    m_instrumentCombo->setEnabled(!isDrumKit);

    int programIndex = m_instrumentCombo->findData(m_track->synthParams.instrumentProgram.load());
    if (programIndex >= 0) m_instrumentCombo->setCurrentIndex(programIndex);

    m_playerStack->setCurrentWidget(isDrumKit ? static_cast<QWidget*>(m_drumPads)
                                               : static_cast<QWidget*>(m_keyboard));
    m_practicePanel->setVisible(!isDrumKit);
}

} // namespace rsd
