#pragma once

#include <QComboBox>
#include <QDialog>

#include "audio/AudioEngine.h"

namespace rsd {

// Lets the user pick output/input device and sample rate, then restarts
// AudioEngine's stream with the new settings on accept.
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(AudioEngine& engine, QWidget* parent = nullptr);

    // Set after accept() if the sample rate changed, so MainWindow can
    // update Session/ruler/timeline scale to match.
    unsigned int chosenSampleRate() const { return m_chosenSampleRate; }

private slots:
    void onAccept();

private:
    AudioEngine& m_engine;
    QComboBox* m_outputCombo = nullptr;
    QComboBox* m_inputCombo = nullptr;
    QComboBox* m_systemAudioCombo = nullptr;
    QComboBox* m_sampleRateCombo = nullptr;
    unsigned int m_chosenSampleRate = 0;
};

} // namespace rsd
