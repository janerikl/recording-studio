#include "MediaBrowserPanel.h"

#include <QByteArray>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QTabBar>
#include <QTreeWidget>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

#include "io/AudioFileIO.h"
#include "ui/LibraryFilterMath.h"
#include "ui/LibraryItemDelegate.h"
#include "ui/MediaPreviewToggleMath.h"

namespace rsd {

namespace {

constexpr auto kSettingsKey = "loopBrowserFolder";
constexpr int kFolderRole = Qt::UserRole + 1; // QUuid string, present on folder rows only

// Outgoing drags carry either the custom library-index MIME type (Project
// Media) or a standard file URL (Loop), matching what ClipLaneWidget (and
// any other file-drop target) already understands.
class BrowserTreeWidget : public QTreeWidget {
public:
    explicit BrowserTreeWidget(MediaBrowserPanel* owner)
        : QTreeWidget(owner), m_owner(owner) {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setAcceptDrops(true); // for external file drops onto the Project Media tab
    }

protected:
    QMimeData* mimeData(const QList<QTreeWidgetItem*>& items) const override {
        if (items.isEmpty()) return nullptr;
        return m_owner->mimeDataForItem(items.first());
    }

    void dragEnterEvent(QDragEnterEvent* event) override {
        if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    }

    void dragMoveEvent(QDragMoveEvent* event) override {
        if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    }

    void dropEvent(QDropEvent* event) override {
        if (!event->mimeData()->hasUrls()) return;
        for (const QUrl& url : event->mimeData()->urls()) {
            if (!url.isLocalFile()) continue;
            m_owner->handleExternalFileDrop(url.toLocalFile());
        }
        event->acceptProposedAction();
    }

private:
    MediaBrowserPanel* m_owner;
};

} // namespace

static QString formatDuration(int64_t samples, int sampleRate) {
    if (sampleRate <= 0) return QString();
    double totalSeconds = static_cast<double>(samples) / sampleRate;
    int mins = static_cast<int>(totalSeconds) / 60;
    double secs = totalSeconds - mins * 60;
    return QString("%1:%2").arg(mins).arg(secs, 4, 'f', 1, QChar('0'));
}

MediaBrowserPanel::MediaBrowserPanel(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void MediaBrowserPanel::buildUi() {
    auto* outer = new QVBoxLayout(this);

    m_tabBar = new QTabBar(this);
    m_tabBar->addTab("Project Media");
    m_tabBar->addTab("Loops");
    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        m_activeSource = index == 0 ? LibrarySource::ProjectMedia : LibrarySource::Loop;
        m_chooseFolderButton->setVisible(m_activeSource == LibrarySource::Loop);
        m_folderLabel->setVisible(m_activeSource == LibrarySource::Loop);
        rebuildTree();
    });
    outer->addWidget(m_tabBar);

    auto* folderRow = new QHBoxLayout();
    m_folderLabel = new QLabel("No folder chosen", this);
    m_folderLabel->setWordWrap(true);
    m_folderLabel->setVisible(false);
    m_chooseFolderButton = new QPushButton("Choose Folder...", this);
    m_chooseFolderButton->setVisible(false);
    connect(m_chooseFolderButton, &QPushButton::clicked, this, &MediaBrowserPanel::chooseLoopFolder);
    folderRow->addWidget(m_folderLabel, 1);
    folderRow->addWidget(m_chooseFolderButton);
    outer->addLayout(folderRow);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Filter by name...");
    connect(m_filterEdit, &QLineEdit::textChanged, this, &MediaBrowserPanel::rebuildTree);
    outer->addWidget(m_filterEdit);

    m_tree = new BrowserTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setItemDelegate(new LibraryItemDelegate(this, m_tree));
    connect(m_tree, &QTreeWidget::itemClicked, this, &MediaBrowserPanel::handleItemClicked);
    outer->addWidget(m_tree, 1);

    QSettings settings("RecordingStudio", "RecordingStudio");
    QString savedFolder = settings.value(kSettingsKey).toString();
    if (!savedFolder.isEmpty() && QFileInfo::exists(savedFolder)) {
        m_loopFolderPath = savedFolder;
        m_folderLabel->setText(m_loopFolderPath);
        rescanLoops();
    }

    rebuildTree();
}

bool MediaBrowserPanel::containsBuffer(const AudioBuffer* buffer) const {
    for (auto& item : m_projectItems) {
        if (item.buffer.get() == buffer) return true;
    }
    return false;
}

void MediaBrowserPanel::addProjectMediaEntry(const QString& name, std::shared_ptr<AudioBuffer> buffer,
                                              QString itemId) {
    if (itemId.isEmpty()) itemId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    LibraryItem item;
    item.source = LibrarySource::ProjectMedia;
    item.name = name;
    item.buffer = buffer;
    item.itemId = itemId;
    m_projectItems.push_back(item);

    if (m_activeSource == LibrarySource::ProjectMedia) rebuildTree();
}

void MediaBrowserPanel::refresh(const Session& session) {
    for (auto& track : session.tracks) {
        for (auto& clip : *track->clipsSnapshot()) {
            if (!clip->buffer || containsBuffer(clip->buffer.get())) continue;
            addProjectMediaEntry(clip->name, clip->buffer);
        }
    }
}

std::shared_ptr<AudioBuffer> MediaBrowserPanel::bufferAt(int index) const {
    if (index < 0 || index >= m_projectItems.size()) return nullptr;
    return m_projectItems[index].buffer;
}

QString MediaBrowserPanel::nameAt(int index) const {
    if (index < 0 || index >= m_projectItems.size()) return QString();
    return m_projectItems[index].name;
}

QVector<LibraryEntry> MediaBrowserPanel::projectMediaEntries() const {
    QVector<LibraryEntry> entries;
    for (auto& item : m_projectItems) entries.append({item.name, item.buffer, item.itemId});
    return entries;
}

void MediaBrowserPanel::resetLibrary() {
    m_projectItems.clear();
    m_loadedPaths.clear();
    m_folders = LibraryFolderTree();
    m_peakCache.clear();
    m_previewingItem = LibraryItem();
    rebuildTree();
}

void MediaBrowserPanel::restoreFolders(const LibraryFolderTree& folders) {
    m_folders = folders;
    rebuildTree();
}

void MediaBrowserPanel::chooseLoopFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "Choose Sample Folder", m_loopFolderPath);
    if (dir.isEmpty()) return;

    m_loopFolderPath = dir;
    m_folderLabel->setText(m_loopFolderPath);

    QSettings settings("RecordingStudio", "RecordingStudio");
    settings.setValue(kSettingsKey, m_loopFolderPath);

    rescanLoops();
}

void MediaBrowserPanel::rescanLoops() {
    m_allLoopFiles.clear();
    if (!m_loopFolderPath.isEmpty()) {
        QStringList nameFilters = {"*.wav", "*.aiff", "*.aif", "*.flac", "*.ogg"};
        QDirIterator it(m_loopFolderPath, nameFilters, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) m_allLoopFiles.append(it.next());
        m_allLoopFiles.sort(Qt::CaseInsensitive);
    }
    rebuildLoopItems();
}

void MediaBrowserPanel::rebuildLoopItems() {
    // Despite the name, this builds the full (unfiltered) Loop item list from
    // the last rescan; the search-box text is applied uniformly to both tabs
    // in rebuildTree() via filterLibraryItems().
    m_loopItems.clear();

    for (const QString& path : m_allLoopFiles) {
        LibraryItem item;
        item.source = LibrarySource::Loop;
        item.name = QFileInfo(path).fileName();
        item.path = path;
        m_loopItems.push_back(item);
    }

    if (m_activeSource == LibrarySource::Loop) rebuildTree();
}

QTreeWidgetItem* MediaBrowserPanel::addFolderNode(QTreeWidgetItem* parent, LibraryFolder* folder) {
    auto* node = new QTreeWidgetItem(QStringList{folder->name});
    node->setData(0, kFolderRole, folder->id.toString());
    node->setFlags(node->flags() | Qt::ItemIsDropEnabled);
    if (parent) {
        parent->addChild(node);
    } else {
        m_tree->addTopLevelItem(node);
    }

    for (auto& child : folder->children) addFolderNode(node, child.get());
    return node;
}

QTreeWidgetItem* MediaBrowserPanel::addItemNode(QTreeWidgetItem* parent, const LibraryItem& item) {
    auto* node = new QTreeWidgetItem(QStringList{item.name});
    node->setData(0, MediaBrowserPanel::kItemIdRole, item.id());
    node->setFlags((node->flags() | Qt::ItemIsDragEnabled) & ~Qt::ItemIsDropEnabled);
    if (item.source == LibrarySource::ProjectMedia && item.buffer) {
        QString duration = formatDuration(item.buffer->frameCount(), item.buffer->sampleRate);
        node->setToolTip(0, QString("%1\nDuration: %2\nSample rate: %3 Hz\nChannels: %4")
                                 .arg(item.name)
                                 .arg(duration)
                                 .arg(item.buffer->sampleRate)
                                 .arg(item.buffer->channels));
    } else {
        node->setToolTip(0, item.path);
    }

    if (parent) {
        parent->addChild(node);
    } else {
        m_tree->addTopLevelItem(node);
    }
    return node;
}

void MediaBrowserPanel::rebuildTree() {
    if (!m_tree) return;
    m_tree->clear();

    const QVector<LibraryItem>& allItems =
        m_activeSource == LibrarySource::ProjectMedia ? m_projectItems : m_loopItems;
    QString filter = m_filterEdit ? m_filterEdit->text() : QString();
    QVector<LibraryItem> items = filterLibraryItems(allItems, filter);

    // Build a lookup so filed items don't also show up unfiled.
    QSet<QString> filedIds;
    for (auto& folder : m_folders.roots) {
        // (Folders can hold items from either source; only mark the ones
        // relevant to whichever source is active as "filed" so the other
        // source's items aren't hidden by an id collision that can't
        // actually happen — ids are namespaced by source already.)
        for (auto& ref : folder->itemRefs) filedIds.insert(ref);
    }

    for (auto& folder : m_folders.roots) {
        auto* folderNode = addFolderNode(nullptr, folder.get());
        for (auto& item : items) {
            if (folder->itemRefs.contains(item.id())) addItemNode(folderNode, item);
        }
    }

    for (auto& item : items) {
        if (filedIds.contains(item.id())) continue;
        addItemNode(nullptr, item);
    }

    m_tree->expandAll();
}

void MediaBrowserPanel::handleItemClicked(QTreeWidgetItem* item, int /*column*/) {
    if (!item) return;
    QVariant itemIdVar = item->data(0, MediaBrowserPanel::kItemIdRole);
    if (!itemIdVar.isValid()) return; // a folder row, not a leaf

    QString itemId = itemIdVar.toString();
    const QVector<LibraryItem>& items =
        m_activeSource == LibrarySource::ProjectMedia ? m_projectItems : m_loopItems;

    for (auto& candidate : items) {
        if (candidate.id() != itemId) continue;

        if (m_previewingItem.isValid() && m_previewingItem.id() == candidate.id()) {
            m_previewingItem = LibraryItem();
            emit previewRequested(LibraryItem());
        } else {
            m_previewingItem = candidate;
            emit previewRequested(candidate);
        }
        return;
    }
}

QVector<WaveformCache::PeakPair> MediaBrowserPanel::peaksForItemId(const QString& itemId,
                                                                    int numColumns) {
    auto cached = m_peakCache.find(itemId);
    if (cached != m_peakCache.end()) return cached.value();

    for (auto& item : m_projectItems) {
        if (item.id() != itemId || !item.buffer) continue;
        auto peaks = WaveformCache::computePeaks(*item.buffer, numColumns);
        m_peakCache.insert(itemId, peaks);
        return peaks;
    }

    for (auto& item : m_loopItems) {
        if (item.id() != itemId) continue;
        // Decoded synchronously on first paint of this row; the buffer is
        // discarded once peaks are computed so memory stays bounded even
        // for a large sample-pack folder.
        auto buffer = AudioFileIO::loadFile(item.path);
        QVector<WaveformCache::PeakPair> peaks;
        if (buffer) peaks = WaveformCache::computePeaks(*buffer, numColumns);
        m_peakCache.insert(itemId, peaks);
        return peaks;
    }

    return {};
}

void MediaBrowserPanel::handleExternalFileDrop(const QString& path) {
    QString canonical = QFileInfo(path).canonicalFilePath();
    if (canonical.isEmpty()) canonical = path; // file vanished between drop and stat; fall back

    if (m_loadedPaths.contains(canonical)) return; // already in the library

    auto buffer = AudioFileIO::loadFile(path);
    if (!buffer) {
        emit fileLoadFailed(path);
        return;
    }
    m_loadedPaths.insert(canonical);
    addProjectMediaEntry(QFileInfo(path).fileName(), buffer);
}

QMimeData* MediaBrowserPanel::mimeDataForItem(QTreeWidgetItem* item) const {
    QVariant itemIdVar = item ? item->data(0, MediaBrowserPanel::kItemIdRole) : QVariant();
    if (!itemIdVar.isValid()) return nullptr; // a folder row can't be dragged out

    if (m_activeSource == LibrarySource::ProjectMedia) {
        for (int i = 0; i < m_projectItems.size(); ++i) {
            if (m_projectItems[i].id() != itemIdVar.toString()) continue;
            auto* mime = new QMimeData();
            mime->setData(kMimeType, QByteArray::number(i));
            return mime;
        }
        return nullptr;
    }

    for (auto& loopItem : m_loopItems) {
        if (loopItem.id() != itemIdVar.toString()) continue;
        auto* mime = new QMimeData();
        mime->setUrls({QUrl::fromLocalFile(loopItem.path)});
        return mime;
    }
    return nullptr;
}

} // namespace rsd
