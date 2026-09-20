#pragma once

#include <QDialog>

#include "io/AudioFileIO.h"

class QCheckBox;
class QRadioButton;

namespace rsd {

// Format + stems choice shown before the save-file dialog in
// MainWindow::onExportClicked.
class ExportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ExportDialog(QWidget* parent = nullptr);

    ExportFormat chosenFormat() const;
    bool exportStems() const;

private:
    QRadioButton* m_float32Radio = nullptr;
    QRadioButton* m_pcm16Radio = nullptr;
    QCheckBox* m_stemsCheckBox = nullptr;
};

} // namespace rsd
