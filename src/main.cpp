#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

#include "ui/MainWindow.h"

namespace {

// A dark, professional DAW look applied app-wide via Qt's Fusion style +
// palette, so every stock widget (menus, buttons, docks, dialogs) matches
// the custom-styled panels without needing a per-widget stylesheet.
void applyDarkTheme(QApplication& app) {
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(37, 37, 38));
    palette.setColor(QPalette::WindowText, QColor(224, 224, 224));
    palette.setColor(QPalette::Base, QColor(30, 30, 30));
    palette.setColor(QPalette::AlternateBase, QColor(45, 45, 45));
    palette.setColor(QPalette::ToolTipBase, QColor(45, 45, 45));
    palette.setColor(QPalette::ToolTipText, QColor(224, 224, 224));
    palette.setColor(QPalette::Text, QColor(224, 224, 224));
    palette.setColor(QPalette::Button, QColor(58, 58, 58));
    palette.setColor(QPalette::ButtonText, QColor(224, 224, 224));
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(100, 170, 255));
    palette.setColor(QPalette::Highlight, QColor(70, 120, 200));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 120, 120));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 120));
    app.setPalette(palette);
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    applyDarkTheme(app);

    rsd::MainWindow window;
    window.show();

    return app.exec();
}
