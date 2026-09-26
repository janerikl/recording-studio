#include "ExportDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QSettings>
#include <QVBoxLayout>

#include "ui/DialogGeometry.h"

namespace rsd {

ExportDialog::ExportDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Export Session");

    QSettings windowSettings("RecordingStudio", "RecordingStudio");
    restoreDialogGeometry(*this, windowSettings, "exportDialog");
    connect(this, &QDialog::finished, this, [this]() {
        QSettings settings("RecordingStudio", "RecordingStudio");
        saveDialogGeometry(*this, settings, "exportDialog");
    });

    auto* layout = new QVBoxLayout(this);

    auto* formatGroup = new QGroupBox("Format", this);
    auto* formatLayout = new QVBoxLayout(formatGroup);
    m_float32Radio = new QRadioButton("32-bit float WAV (best for further editing)", formatGroup);
    m_pcm16Radio = new QRadioButton("16-bit WAV (smaller, standard for sharing)", formatGroup);
    m_float32Radio->setChecked(true);
    formatLayout->addWidget(m_float32Radio);
    formatLayout->addWidget(m_pcm16Radio);
    layout->addWidget(formatGroup);

    m_stemsCheckBox = new QCheckBox("Also export stems (one file per track)", this);
    layout->addWidget(m_stemsCheckBox);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

ExportFormat ExportDialog::chosenFormat() const {
    return m_pcm16Radio->isChecked() ? ExportFormat::Wav16Pcm : ExportFormat::Wav32Float;
}

bool ExportDialog::exportStems() const {
    return m_stemsCheckBox->isChecked();
}

} // namespace rsd
