#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>
#include <algorithm>
#include <set>

namespace rsd {

SettingsDialog::SettingsDialog(AudioEngine& engine, QWidget* parent)
    : QDialog(parent), m_engine(engine) {
    setWindowTitle("Audio Settings");

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    m_outputCombo = new QComboBox(this);
    m_inputCombo = new QComboBox(this);
    m_systemAudioCombo = new QComboBox(this);
    // Sentinels: -1 = "no input at all", -2 = "system default". Kept
    // distinct so they don't collide when read back in onAccept().
    m_inputCombo->addItem("None (playback only)", QVariant(-1));
    // System audio has no "system default" concept (no such device exists);
    // -1 here just means "disabled", the only sentinel it needs.
    m_systemAudioCombo->addItem("None (disabled)", QVariant(-1));
    m_sampleRateCombo = new QComboBox(this);

    auto devices = engine.listDevices();
    std::set<unsigned int> commonSampleRates;

    m_outputCombo->addItem("System Default", QVariant(-2));
    m_inputCombo->addItem("System Default", QVariant(-2));

    for (auto& dev : devices) {
        if (dev.maxOutputChannels > 0) {
            m_outputCombo->addItem(dev.name, QVariant(static_cast<int>(dev.id)));
        }
        if (dev.maxInputChannels > 0) {
            m_inputCombo->addItem(dev.name, QVariant(static_cast<int>(dev.id)));
            // System audio is picked from the same capture-capable device
            // list — on Linux/PulseAudio a ".monitor" loopback source shows
            // up here as a regular input device, so no special detection
            // is needed; the user just selects it in this second dropdown.
            m_systemAudioCombo->addItem(dev.name, QVariant(static_cast<int>(dev.id)));
        }
        for (unsigned int sr : dev.sampleRates) commonSampleRates.insert(sr);
    }

    if (commonSampleRates.empty()) {
        commonSampleRates = {44100, 48000, 96000};
    }
    for (unsigned int sr : commonSampleRates) {
        m_sampleRateCombo->addItem(QString::number(sr) + " Hz", QVariant(static_cast<int>(sr)));
    }

    // Pre-select whatever the engine is currently using.
    int outIdx = m_outputCombo->findData(
        engine.preferredOutputDevice() == AudioEngine::kUseSystemDefault
            ? -2
            : static_cast<int>(engine.preferredOutputDevice()));
    if (outIdx >= 0) m_outputCombo->setCurrentIndex(outIdx);

    int inSentinel = engine.preferredInputDevice() == AudioEngine::kNoInputDevice ? -1
                      : engine.preferredInputDevice() == AudioEngine::kUseSystemDefault
                          ? -2
                          : static_cast<int>(engine.preferredInputDevice());
    int inIdx = m_inputCombo->findData(inSentinel);
    if (inIdx >= 0) m_inputCombo->setCurrentIndex(inIdx);

    int sysSentinel = engine.preferredSystemAudioDevice() == AudioEngine::kNoInputDevice
                           ? -1
                           : static_cast<int>(engine.preferredSystemAudioDevice());
    int sysIdx = m_systemAudioCombo->findData(sysSentinel);
    if (sysIdx >= 0) m_systemAudioCombo->setCurrentIndex(sysIdx);

    int srIdx = m_sampleRateCombo->findData(static_cast<int>(engine.sampleRate()));
    if (srIdx >= 0) m_sampleRateCombo->setCurrentIndex(srIdx);

    form->addRow("Output Device:", m_outputCombo);
    form->addRow("Microphone Device:", m_inputCombo);
    form->addRow("System Audio Device (loopback):", m_systemAudioCombo);
    form->addRow("Sample Rate:", m_sampleRateCombo);
    layout->addLayout(form);

    layout->addWidget(new QLabel("Applying changes restarts the audio stream.", this));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void SettingsDialog::onAccept() {
    int outId = m_outputCombo->currentData().toInt();
    int inId = m_inputCombo->currentData().toInt();
    int sysId = m_systemAudioCombo->currentData().toInt();
    unsigned int sr = static_cast<unsigned int>(m_sampleRateCombo->currentData().toInt());

    m_engine.setPreferredOutputDevice(outId < 0 ? AudioEngine::kUseSystemDefault
                                                 : static_cast<unsigned int>(outId));
    m_engine.setPreferredInputDevice(inId == -1   ? AudioEngine::kNoInputDevice
                                      : inId == -2 ? AudioEngine::kUseSystemDefault
                                                    : static_cast<unsigned int>(inId));
    m_engine.setPreferredSystemAudioDevice(
        sysId == -1 ? AudioEngine::kNoInputDevice : static_cast<unsigned int>(sysId));
    m_engine.setPreferredSampleRate(sr);

    if (!m_engine.restart()) {
        QMessageBox::warning(this, "Settings Failed",
                              "Could not open the audio stream with these settings. "
                              "Reverting is recommended.");
        return;
    }

    m_chosenSampleRate = sr;
    accept();
}

} // namespace rsd
