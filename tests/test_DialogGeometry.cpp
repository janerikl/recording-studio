#include <QDialog>
#include <QSettings>
#include <QTest>

#include "ui/DialogGeometry.h"

using namespace rsd;

class DialogGeometryTests : public QObject {
    Q_OBJECT

private slots:
    void init() {
        QCoreApplication::setOrganizationName("RSD_DialogGeometryTests");
        QCoreApplication::setApplicationName("RSD_DialogGeometryTests");
        QSettings settings;
        settings.clear();
    }

    void restoreDoesNothingWhenNothingWasEverSaved() {
        QSettings settings;
        QDialog dialog;
        dialog.resize(400, 300);
        restoreDialogGeometry(dialog, settings, "someDialog");
        QCOMPARE(dialog.size(), QSize(400, 300));
    }

    void savedGeometryIsRestoredOnAFreshDialogInstance() {
        {
            QSettings settings;
            QDialog dialog;
            dialog.resize(777, 555);
            dialog.move(42, 24);
            saveDialogGeometry(dialog, settings, "exportDialog");
        }

        QSettings settings;
        QDialog reopened;
        reopened.resize(400, 300); // whatever the dialog's default ctor size would be
        restoreDialogGeometry(reopened, settings, "exportDialog");

        QCOMPARE(reopened.size(), QSize(777, 555));
        QCOMPARE(reopened.pos(), QPoint(42, 24));
    }

    void differentKeysDoNotCollide() {
        QSettings settings;
        QDialog settingsDlg;
        settingsDlg.resize(500, 400);
        saveDialogGeometry(settingsDlg, settings, "settingsDialog");

        QDialog exportDlg;
        exportDlg.resize(300, 200);
        saveDialogGeometry(exportDlg, settings, "exportDialog");

        QDialog reopenedSettings;
        restoreDialogGeometry(reopenedSettings, settings, "settingsDialog");
        QCOMPARE(reopenedSettings.size(), QSize(500, 400));

        QDialog reopenedExport;
        restoreDialogGeometry(reopenedExport, settings, "exportDialog");
        QCOMPARE(reopenedExport.size(), QSize(300, 200));
    }
};

QTEST_MAIN(DialogGeometryTests)
#include "test_DialogGeometry.moc"
