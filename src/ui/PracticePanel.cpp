#include "PracticePanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "audio/RhythmClickTrack.h"
#include "audio/RhythmMath.h"
#include "model/PracticeExercise.h"
#include "model/PracticeMath.h"
#include "model/RhythmPattern.h"
#include "ui/RhythmStaffWidget.h"

namespace rsd {

namespace {
constexpr double kTapToleranceSeconds = 0.15;
}

PracticePanel::PracticePanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(
        "PracticePanel { background: #1e1e1e; }"
        "QLabel { color: #cccccc; }"
        "QComboBox, QPushButton { background: #2a2a2a; color: #e0e0e0; border: 1px solid #4a4a4a; }");
    setFocusPolicy(Qt::StrongFocus);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 6, 0, 6);

    auto* modeRow = new QWidget;
    auto* modeLayout = new QHBoxLayout(modeRow);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->addWidget(new QLabel("Mode"));
    m_modeCombo = new QComboBox;
    m_modeCombo->addItem("Pitch");
    m_modeCombo->addItem("Rhythm");
    modeLayout->addWidget(m_modeCombo, 1);
    outer->addWidget(modeRow);
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, &PracticePanel::rebuildModeUi);

    // --- Pitch mode ---
    m_pitchModeContainer = new QWidget;
    auto* pitchOuter = new QVBoxLayout(m_pitchModeContainer);
    pitchOuter->setContentsMargins(0, 0, 0, 0);

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

    pitchOuter->addWidget(row);

    m_statusLabel = new QLabel("Pick an exercise and press Start.");
    pitchOuter->addWidget(m_statusLabel);
    outer->addWidget(m_pitchModeContainer);

    // --- Rhythm mode ---
    m_rhythmModeContainer = new QWidget;
    auto* rhythmOuter = new QVBoxLayout(m_rhythmModeContainer);
    rhythmOuter->setContentsMargins(0, 0, 0, 0);

    auto* rhythmRow = new QWidget;
    auto* rhythmRowLayout = new QHBoxLayout(rhythmRow);
    rhythmRowLayout->setContentsMargins(0, 0, 0, 0);
    rhythmRowLayout->addWidget(new QLabel("Word"));
    m_rhythmCombo = new QComboBox;
    for (const auto& p : builtInRhythmPatterns()) m_rhythmCombo->addItem(p.word);
    rhythmRowLayout->addWidget(m_rhythmCombo, 1);
    rhythmOuter->addWidget(rhythmRow);

    auto* rhythmIntroLabel = new QLabel(
        "Say the word out loud in rhythm — its syllables match the notes shown below. "
        "Press Play to hear it as a click, then Tap Back and tap the same rhythm yourself "
        "(Tap button or Spacebar).");
    rhythmIntroLabel->setWordWrap(true);
    rhythmOuter->addWidget(rhythmIntroLabel);

    connect(m_rhythmCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        const auto& patterns = builtInRhythmPatterns();
        if (idx < 0 || idx >= static_cast<int>(patterns.size())) return;
        m_rhythmStaff->setPattern(patterns[static_cast<size_t>(idx)]);
        m_rhythmStatusLabel->setText(
            QStringLiteral("Say \"%1\" in rhythm, then press Play to hear it.")
                .arg(patterns[static_cast<size_t>(idx)].word));
    });

    m_rhythmStaff = new RhythmStaffWidget;
    rhythmOuter->addWidget(m_rhythmStaff);
    if (!builtInRhythmPatterns().empty()) m_rhythmStaff->setPattern(builtInRhythmPatterns()[0]);

    auto* rhythmButtonRow = new QWidget;
    auto* rhythmButtonLayout = new QHBoxLayout(rhythmButtonRow);
    rhythmButtonLayout->setContentsMargins(0, 0, 0, 0);
    m_playRhythmButton = new QPushButton("Play");
    connect(m_playRhythmButton, &QPushButton::clicked, this, &PracticePanel::onPlayRhythmClicked);
    rhythmButtonLayout->addWidget(m_playRhythmButton);

    m_tapBackButton = new QPushButton("Tap Back");
    connect(m_tapBackButton, &QPushButton::clicked, this, &PracticePanel::onTapBackClicked);
    rhythmButtonLayout->addWidget(m_tapBackButton);

    m_tapButton = new QPushButton("Tap");
    connect(m_tapButton, &QPushButton::clicked, this, &PracticePanel::registerTap);
    rhythmButtonLayout->addWidget(m_tapButton);
    rhythmOuter->addWidget(rhythmButtonRow);

    m_rhythmStatusLabel = new QLabel;
    m_rhythmStatusLabel->setWordWrap(true);
    if (!builtInRhythmPatterns().empty()) {
        m_rhythmStatusLabel->setText(
            QStringLiteral("Say \"%1\" in rhythm, then press Play to hear it.")
                .arg(builtInRhythmPatterns()[0].word));
    }
    rhythmOuter->addWidget(m_rhythmStatusLabel);

    outer->addWidget(m_rhythmModeContainer);

    rebuildModeUi();
}

void PracticePanel::setBpm(double bpm) { m_bpm = bpm; }

void PracticePanel::rebuildModeUi() {
    bool rhythmMode = m_modeCombo->currentIndex() == 1;
    m_pitchModeContainer->setVisible(!rhythmMode);
    m_rhythmModeContainer->setVisible(rhythmMode);
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

void PracticePanel::onPlayRhythmClicked() {
    const auto& patterns = builtInRhythmPatterns();
    int idx = m_rhythmCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(patterns.size())) return;

    auto buffer = renderRhythmPianoBuffer(patterns[static_cast<size_t>(idx)], m_bpm, 48000, 2);
    emit rhythmPlaybackRequested(buffer);
}

void PracticePanel::onTapBackClicked() {
    m_tapping = true;
    m_tapTimesSeconds.clear();
    m_tapTimer.start();
    m_rhythmStatusLabel->setText("Tapping... (press Tap or Space on each beat)");
    setFocus();

    const auto& patterns = builtInRhythmPatterns();
    int idx = m_rhythmCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(patterns.size())) return;
    double totalSeconds = beatsToSeconds(patternTotalBeats(patterns[static_cast<size_t>(idx)]), m_bpm);
    int durationMs = static_cast<int>((totalSeconds + 0.75) * 1000.0);
    QTimer::singleShot(durationMs, this, &PracticePanel::finishTapping);
}

void PracticePanel::registerTap() {
    if (!m_tapping) return;
    m_tapTimesSeconds.push_back(static_cast<double>(m_tapTimer.elapsed()) / 1000.0);
}

void PracticePanel::finishTapping() {
    if (!m_tapping) return;
    m_tapping = false;

    const auto& patterns = builtInRhythmPatterns();
    int idx = m_rhythmCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(patterns.size())) return;
    const auto& pattern = patterns[static_cast<size_t>(idx)];

    std::vector<double> expectedSeconds;
    for (double beat : onsetBeats(pattern)) expectedSeconds.push_back(beatsToSeconds(beat, m_bpm));

    auto result = scoreTaps(expectedSeconds, m_tapTimesSeconds, kTapToleranceSeconds);

    QString verdictLine;
    for (auto v : result.verdicts) {
        switch (v) {
            case TapVerdict::Hit: verdictLine += "✓ "; break;
            case TapVerdict::Early: verdictLine += "early "; break;
            case TapVerdict::Late: verdictLine += "late "; break;
            case TapVerdict::Miss: verdictLine += "miss "; break;
        }
    }

    m_rhythmStatusLabel->setText(
        QStringLiteral("%1\nAccuracy: %2%  (missed: %3, extra taps: %4)")
            .arg(verdictLine.trimmed())
            .arg(result.accuracyPercent, 0, 'f', 0)
            .arg(result.missedCount)
            .arg(result.extraTapCount));
}

void PracticePanel::keyPressEvent(QKeyEvent* event) {
    if (m_tapping && event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        registerTap();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace rsd
