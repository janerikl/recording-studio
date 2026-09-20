#pragma once

#include <QListWidget>
#include <QString>
#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;

namespace rsd {

// Browses a user-chosen folder on disk (a sample-pack collection, say) —
// independent of the session-scoped MediaLibraryPanel, which only lists
// buffers already in the session or dropped in from outside. Dragging an
// item out sets standard QUrl mime data (same as an OS file-manager drag),
// so it lands on a track via ClipLaneWidget's existing external-file-drop
// handling with no dedicated plumbing of its own. Clicking an item asks
// AudioEngine (via the previewRequested signal) to audition it.
class LoopBrowserPanel : public QWidget {
    Q_OBJECT

public:
    explicit LoopBrowserPanel(QWidget* parent = nullptr);

signals:
    // User clicked a file to hear it (or clicked the currently-previewing
    // one again / hit Stop, in which case filePath is empty).
    void previewRequested(QString filePath);

private:
    void chooseFolder();
    void rescan();
    void applyFilter();

    QLineEdit* m_filterEdit = nullptr;
    QPushButton* m_chooseFolderButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QLabel* m_folderLabel = nullptr;
    QListWidget* m_list = nullptr;

    QString m_folderPath;
    QStringList m_allFiles; // absolute paths, unfiltered, from the last rescan
    QString m_previewingPath;
};

} // namespace rsd
