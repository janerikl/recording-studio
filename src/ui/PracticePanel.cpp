#include "PracticePanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "model/PracticeExercise.h"
#include "model/PracticeMath.h"

namespace rsd {

PracticePanel::PracticePanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(
        "PracticePanel { background: #1e1e1e; }"
        "QLabel { color: #cccccc; }"
        "QComboBox, QPushButton { background: #2a2a2a; color: #e0e0e0; border: 1px solid #4a4a4a; }");

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 6, 0, 6);

    auto* row = new QWidget;
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addWidget(new QLabel("Practice"));

    m_exerciseCombo = new QComboBox;
    for (const auto& ex : builtInPracticeExercises()) m_exerciseCombo->addItem(ex.name);
    rowLayout->addWidget(m_exerciseCombo, 1);

    m_startButton = new QPushButton("Start");
    connect(m_startButton, &QPushButton::clicked, this, &PracticePanel::startSelectedExercise);
    rowLayout->addWidget(m_startButton);

    outer->addWidget(row);

    m_statusLabel = new QLabel("Pick an exercise and press Start.");
    outer->addWidget(m_statusLabel);
}

void PracticePanel::startSelectedExercise() {
    m_currentIndex = 0;
    m_active = true;
    refreshStatus();
}

void PracticePanel::checkNotePlayed(int pitch) {
    if (!m_active) return;
    const auto& exercises = builtInPracticeExercises();
    int idx = m_exerciseCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(exercises.size())) return;
    const auto& pitches = exercises[static_cast<size_t>(idx)].pitches;

    size_t before = m_currentIndex;
    m_currentIndex = practiceAdvance(pitches, m_currentIndex, pitch);
    if (m_currentIndex != before) refreshStatus();
}

void PracticePanel::refreshStatus() {
    const auto& exercises = builtInPracticeExercises();
    int idx = m_exerciseCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(exercises.size())) {
        emit expectedPitchChanged(std::nullopt);
        return;
    }
    const auto& pitches = exercises[static_cast<size_t>(idx)].pitches;

    if (practiceComplete(pitches, m_currentIndex)) {
        m_active = false;
        m_statusLabel->setText(QStringLiteral("Done! Press Start to try again."));
        emit expectedPitchChanged(std::nullopt);
        return;
    }

    m_statusLabel->setText(
        QStringLiteral("Note %1 / %2").arg(m_currentIndex + 1).arg(pitches.size()));
    emit expectedPitchChanged(pitches[m_currentIndex]);
}

} // namespace rsd
