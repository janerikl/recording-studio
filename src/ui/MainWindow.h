#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <memory>

#include "audio/AudioEngine.h"

namespace rsd {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void togglePassthrough();

private:
    std::unique_ptr<AudioEngine> m_engine;
    QPushButton* m_toggleButton = nullptr;
};

} // namespace rsd
