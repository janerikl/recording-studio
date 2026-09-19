#pragma once

#include <QListWidget>
#include <QMimeData>
#include <QSet>
#include <QVector>
#include <memory>

#include "model/AudioBuffer.h"
#include "model/Session.h"

namespace rsd {

// Lists unique audio files (AudioBuffers), and lets the user drag one onto a
// track's clip lane to add a clip referencing that shared buffer. Entries
// come from two sources: every buffer already used somewhere in the session
// (merged in via refresh(), never removed once added even if later unused —
// so a file you dropped in but haven't placed yet doesn't vanish), and files
// dragged in directly from outside the application (a file manager), loaded
// on drop and added even before they're used on any track.
class MediaLibraryPanel : public QListWidget {
    Q_OBJECT

public:
    explicit MediaLibraryPanel(QWidget* parent = nullptr);

    // Merges in any session buffer not already tracked; existing entries
    // (including externally-dropped, still-unused ones) are left alone.
    void refresh(const Session& session);

    // Discards every entry, e.g. when closing the session.
    void resetLibrary();

    std::shared_ptr<AudioBuffer> bufferAt(int index) const;
    QString nameAt(int index) const;

    static constexpr auto kMimeType = "application/x-rsd-library-index";

signals:
    // A file failed to load on external drop (e.g. not a supported audio
    // format) — MainWindow can surface this, the panel itself stays silent.
    void fileLoadFailed(QString path);

protected:
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    bool containsBuffer(const AudioBuffer* buffer) const;
    void addEntry(const QString& name, std::shared_ptr<AudioBuffer> buffer);

    QVector<std::shared_ptr<AudioBuffer>> m_buffers;
    QVector<QString> m_names;
    // Canonical paths of files already loaded via external drop, so dropping
    // the same file again just selects the existing entry instead of adding
    // a duplicate.
    QSet<QString> m_loadedPaths;
};

} // namespace rsd
