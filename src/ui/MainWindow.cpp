#include "MainWindow.h"

#include <QVBoxLayout>
#include <QWidget>

namespace rsd {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), m_engine(std::make_unique<AudioEngine>()) {
    setWindowTitle("Recording Studio");
    resize(800, 600);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    m_toggleButton = new QPushButton("Start Passthrough", central);
    connect(m_toggleButton, &QPushButton::clicked, this, &MainWindow::togglePassthrough);
    layout->addWidget(m_toggleButton);

    setCentralWidget(central);
}

void MainWindow::togglePassthrough() {
    if (m_engine->isRunning()) {
        m_engine->stop();
        m_toggleButton->setText("Start Passthrough");
    } else {
        if (m_engine->start()) {
            m_toggleButton->setText("Stop Passthrough");
        }
    }
}

} // namespace rsd
