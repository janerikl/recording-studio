#include "MediaLibraryPanel.h"

#include <QByteArray>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>

#include "io/AudioFileIO.h"

namespace rsd {

MediaLibraryPanel::MediaLibraryPanel(QWidget* parent) : QListWidget(parent) {
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setAcceptDrops(true); // for external file drops; outgoing drag uses DragOnly above
}

bool MediaLibraryPanel::containsBuffer(const AudioBuffer* buffer) const {
    for (auto& b : m_buffers) {
        if (b.get() == buffer) return true;
    }
    return false;
}

void MediaLibraryPanel::addEntry(const QString& name, std::shared_ptr<AudioBuffer> buffer) {
    m_buffers.push_back(buffer);
    m_names.push_back(name);
    addItem(name);
}

void MediaLibraryPanel::refresh(const Session& session) {
    for (auto& track : session.tracks) {
        for (auto& clip : *track->clipsSnapshot()) {
            if (!clip->buffer || containsBuffer(clip->buffer.get())) continue;
            addEntry(clip->name, clip->buffer);
        }
    }
}

std::shared_ptr<AudioBuffer> MediaLibraryPanel::bufferAt(int index) const {
    if (index < 0 || index >= m_buffers.size()) return nullptr;
    return m_buffers[index];
}

QString MediaLibraryPanel::nameAt(int index) const {
    if (index < 0 || index >= m_names.size()) return QString();
    return m_names[index];
}

QMimeData* MediaLibraryPanel::mimeData(const QList<QListWidgetItem*>& items) const {
    if (items.isEmpty()) return nullptr;
    int row = this->row(items.first());

    auto* mime = new QMimeData();
    mime->setData(kMimeType, QByteArray::number(row));
    return mime;
}

void MediaLibraryPanel::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MediaLibraryPanel::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MediaLibraryPanel::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasUrls()) return;

    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) continue;
        QString path = url.toLocalFile();

        auto buffer = AudioFileIO::loadFile(path);
        if (!buffer) {
            emit fileLoadFailed(path);
            continue;
        }
        addEntry(QFileInfo(path).fileName(), buffer);
    }
    event->acceptProposedAction();
}

} // namespace rsd
