#pragma once

#include <QWidget>
#include <optional>

class QComboBox;
class QLabel;
class QPushButton;

namespace rsd {

// Learn-to-play aid: pick a built-in exercise (scale/simple song), then
// play along — the panel tracks progress against InstrumentPanel's
// noteOn events (fed via checkNotePlayed()) and reports which pitch
// should be highlighted next on the keyboard.
class PracticePanel : public QWidget {
    Q_OBJECT

public:
    explicit PracticePanel(QWidget* parent = nullptr);

public slots:
    // Call with every noteOn the player widget emits; advances the
    // exercise if it matches the expected next note.
    void checkNotePlayed(int pitch);

signals:
    // std::nullopt when no exercise is selected/active, or it's complete.
    void expectedPitchChanged(std::optional<int> pitch);

private:
    void startSelectedExercise();
    void refreshStatus();

    QComboBox* m_exerciseCombo = nullptr;
    QPushButton* m_startButton = nullptr;
    QLabel* m_statusLabel = nullptr;

    size_t m_currentIndex = 0;
    bool m_active = false;
};

} // namespace rsd
