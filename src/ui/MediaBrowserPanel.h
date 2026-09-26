#pragma once

#include <QHash>
#include <QSet>
#include <QVector>
#include <QWidget>
#include <memory>

#include "io/SessionIO.h"
#include "model/LibraryFolder.h"
#include "model/LibraryItem.h"
#include "model/Session.h"
#include "waveform/WaveformCache.h"

class QTabBar;
class QLineEdit;
class QPushButton;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QMimeData;

namespace rsd {

// Unified browser for Project Media (audio already used in, or dropped
// into, the session) and Loops (a user-chosen folder on disk), replacing
// the former separate MediaLibraryPanel/LoopBrowserPanel. Both tabs share
// one tree widget, one drag-out/preview mechanism (dispatched by
// LibraryItem::source), one waveform-thumbnail delegate, and one set of
// user-organizable virtual folders that can hold items from either tab.
class MediaBrowserPanel : public QWidget {
    Q_OBJECT

public:
    explicit MediaBrowserPanel(QWidget* parent = nullptr);

    // --- Project Media tab -------------------------------------------------

    // Merges in any session buffer not already tracked; existing entries
    // (including externally-dropped, still-unused ones) are left alone.
    void refresh(const Session& session);

    // Adds a Project Media entry directly (external drop, or restoring one
    // from a loaded session), even if its buffer isn't yet referenced by any
    // track's clip. itemId should be preserved across a session reload; if
    // empty, a new one is generated.
    void addProjectMediaEntry(const QString& name, std::shared_ptr<AudioBuffer> buffer,
                               QString itemId = QString());

    std::shared_ptr<AudioBuffer> bufferAt(int index) const;
    QString nameAt(int index) const;

    // Snapshot of every Project Media entry, suitable for SessionIO::saveSession.
    QVector<LibraryEntry> projectMediaEntries() const;

    // Discards every entry and virtual folder, e.g. when closing the session.
    void resetLibrary();

    // Restores virtual folders loaded from a session file.
    void restoreFolders(const LibraryFolderTree& folders);
    const LibraryFolderTree& folders() const { return m_folders; }

    // --- Loops tab -----------------------------------------------------

    QString loopFolderPath() const { return m_loopFolderPath; }
    // Re-scans the current Loops folder — call after writing a new file
    // into it from elsewhere (e.g. saving a rendered instrument roll).
    void refreshLoops() { rescanLoops(); }

    static constexpr auto kMimeType = "application/x-rsd-library-index";
    // Row data role holding a leaf's LibraryItem::id(); present only on leaf
    // (non-folder) tree items. Used by LibraryItemDelegate to look up the
    // waveform peaks to paint for that row.
    static constexpr int kItemIdRole = Qt::UserRole + 2;

    // Waveform peaks for the item with this id, computed and cached on
    // first request. Project Media decodes are free (buffer already in
    // memory); a Loop item is decoded from disk on first call. Returns an
    // empty vector if the item can't be found or decoded.
    QVector<WaveformCache::PeakPair> peaksForItemId(const QString& itemId, int numColumns);

    // Builds the drag-out payload for a leaf row: the custom library-index
    // MIME type for Project Media, a standard file URL for Loop. Returns
    // nullptr for a folder row (not draggable) or an unrecognized item.
    QMimeData* mimeDataForItem(QTreeWidgetItem* item) const;

    // A file was dropped on the Project Media tab from outside the app
    // (e.g. a file manager) — loads and adds it unless already present.
    void handleExternalFileDrop(const QString& path);

signals:
    // A file failed to load on external drop (e.g. not a supported audio
    // format) — MainWindow can surface this, the panel itself stays silent.
    void fileLoadFailed(QString path);

    // User clicked an item to hear it (or clicked the currently-previewing
    // one again, in which case item is default-constructed / invalid,
    // meaning "stop").
    void previewRequested(LibraryItem item);

private:
    void buildUi();
    void rebuildTree();
    void applyLoopFilter();
    void chooseLoopFolder();
    void rescanLoops();
    bool containsBuffer(const AudioBuffer* buffer) const;
    void handleItemClicked(QTreeWidgetItem* item, int column);
    void handleItemDrop(QTreeWidgetItem* folderItem, const QString& itemId);
    QTreeWidgetItem* addFolderNode(QTreeWidgetItem* parent, LibraryFolder* folder);
    QTreeWidgetItem* addItemNode(QTreeWidgetItem* parent, const LibraryItem& item);

    // Peak cache shared by the waveform delegate, keyed by LibraryItem::id().
    // Populated lazily (and synchronously) on first paint of a row; a Loop
    // item's decoded buffer is discarded immediately after its peaks are
    // computed and cached.
    QHash<QString, QVector<WaveformCache::PeakPair>> m_peakCache;

    QTabBar* m_tabBar = nullptr;
    QLineEdit* m_filterEdit = nullptr;
    QPushButton* m_chooseFolderButton = nullptr;
    QLabel* m_folderLabel = nullptr;
    QTreeWidget* m_tree = nullptr;

    QVector<LibraryItem> m_projectItems;
    QVector<LibraryItem> m_loopItems;
    // Canonical paths of Project Media files already loaded via external
    // drop, so dropping the same file again just selects the existing entry
    // instead of adding a duplicate.
    QSet<QString> m_loadedPaths;
    QStringList m_allLoopFiles; // absolute paths, unfiltered, from the last rescan
    QString m_loopFolderPath;

    LibraryFolderTree m_folders;

    LibrarySource m_activeSource = LibrarySource::ProjectMedia;
    LibraryItem m_previewingItem;
};

} // namespace rsd
