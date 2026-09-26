#include "DialogGeometry.h"

#include <QDialog>
#include <QSettings>

namespace rsd {

void restoreDialogGeometry(QDialog& dialog, QSettings& settings, const QString& key) {
    QString settingsKey = "dialogs/" + key + "/geometry";
    if (settings.contains(settingsKey)) {
        dialog.restoreGeometry(settings.value(settingsKey).toByteArray());
    }
}

void saveDialogGeometry(QDialog& dialog, QSettings& settings, const QString& key) {
    settings.setValue("dialogs/" + key + "/geometry", dialog.saveGeometry());
}

} // namespace rsd
