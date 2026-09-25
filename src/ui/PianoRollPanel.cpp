#include "PianoRollPanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "PianoRollGridWidget.h"

namespace rsd {

PianoRollPanel::PianoRollPanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(
        "PianoRollPanel { background: #1e1e1e; }"
        "QLabel { color: #cccccc; }"
        "QComboBox { background: #2a2a2a; color: #e0e0e0; border: 1px solid #4a4a4a; }");

    auto* outer = new QVBoxLayout(this);

    m_trackNameLabel = new QLabel("No instrument track selected");
    m_trackNameLabel->setStyleSheet("font-weight: bold; color: #f0f0f0;");
    outer->addWidget(m_trackNameLabel);

    auto* toolbar = new QWidget;
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->addWidget(new QLabel("Snap"));
    m_snapCombo = new QComboBox;
    m_snapCombo->addItem("Off", 0);
    m_snapCombo->addItem("1/4", 4);
    m_snapCombo->addItem("1/8", 8);
    m_snapCombo->addItem("1/16", 16);
    m_snapCombo->addItem("1/32", 32);
    m_snapCombo->setCurrentIndex(3); // 1/16 default
    toolbarLayout->addWidget(m_snapCombo);
    toolbarLayout->addStretch();
    m_saveToLoopBrowserButton = new QPushButton("Save to Loop Browser");
    connect(m_saveToLoopBrowserButton, &QPushButton::clicked, this,
            [this]() { emit saveToLoopBrowserRequested(m_track); });
    toolbarLayout->addWidget(m_saveToLoopBrowserButton);
    outer->addWidget(toolbar);

    m_grid = new PianoRollGridWidget;
    connect(m_snapCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { m_grid->setSnapDenominator(m_snapCombo->currentData().toInt()); });
    m_grid->setSnapDenominator(m_snapCombo->currentData().toInt());

    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidget(m_grid);
    m_scrollArea->setWidgetResizable(false);
    outer->addWidget(m_scrollArea);

    setTrack(nullptr);
}

void PianoRollPanel::setTrack(std::shared_ptr<Track> track) {
    m_track = std::move(track);
    bool isInstrument = m_track && m_track->kind == TrackKind::Instrument;
    m_trackNameLabel->setText(isInstrument ? (m_track->name.isEmpty() ? "Instrument Track" : m_track->name)
                                            : "No instrument track selected");
    m_snapCombo->setEnabled(isInstrument);
    m_saveToLoopBrowserButton->setEnabled(isInstrument);
    m_grid->setEnabled(isInstrument);
    m_grid->setTrack(isInstrument ? m_track : nullptr);
}

void PianoRollPanel::setCommandStack(CommandStack* stack) { m_grid->setCommandStack(stack); }

void PianoRollPanel::setBpm(double bpm) { m_grid->setBpm(bpm); }

void PianoRollPanel::setSampleRate(int sampleRate) { m_grid->setSampleRate(sampleRate); }

} // namespace rsd
