#pragma once

#include <QElapsedTimer>
#include <QWidget>
#include <memory>
#include <optional>
#include <vector>

#include "model/AudioBuffer.h"

class QComboBox;
class QLabel;
class QPushButton;

namespace rsd {

class RhythmStaffWidget;

// Learn-to-play aid: pick a built-in exercise (scale/simple song), then
// play along — the panel tracks progress against InstrumentPanel's
// noteOn events (fed via checkNotePlayed()) and reports which pitch
// should be highlighted next on the keyboard.
//
// Also hosts a second "Rhythm" mode: a word-mnemonic rhythm-dictation
// trainer (see model/RhythmPattern.h) — play a click pattern, then tap it
// back and get scored, independent of pitch.
class PracticePanel : public QWidget {
    Q_OBJECT

public:
    explicit PracticePanel(QWidget* parent = nullptr);

    void setBpm(double bpm);

public slots:
    // Call with every noteOn the player widget emits; advances the
    // exercise if it matches the expected next note (Pitch mode only).
    void checkNotePlayed(int pitch);

signals:
    // std::nullopt when no exercise is selected/active, or it's complete.
    void expectedPitchChanged(std::optional<int> pitch);

    // Rhythm mode: ask the host to play this click-track buffer (routed
    // to AudioEngine::previewSample() via InstrumentPanel -> MainWindow).
    void rhythmPlaybackRequested(std::shared_ptr<AudioBuffer> buffer);

protected:
    void keyPressEvent(class QKeyEvent* event) override;

private:
    void startSelectedExercise();
    void refreshStatus();
    void rebuildModeUi();

    void onPlayRhythmClicked();
    void onTapBackClicked();
    void registerTap();
    void finishTapping();

    QComboBox* m_modeCombo = nullptr;

    // Pitch mode.
    QWidget* m_pitchModeContainer = nullptr;
    QComboBox* m_exerciseCombo = nullptr;
    QPushButton* m_startButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    size_t m_currentIndex = 0;
    bool m_active = false;

    // Rhythm mode.
    QWidget* m_rhythmModeContainer = nullptr;
    QComboBox* m_rhythmCombo = nullptr;
    RhythmStaffWidget* m_rhythmStaff = nullptr;
    QPushButton* m_playRhythmButton = nullptr;
    QPushButton* m_tapBackButton = nullptr;
    QPushButton* m_tapButton = nullptr;
    QLabel* m_rhythmStatusLabel = nullptr;
    double m_bpm = 120.0;
    bool m_tapping = false;
    QElapsedTimer m_tapTimer;
    std::vector<double> m_tapTimesSeconds;
};

} // namespace rsd
