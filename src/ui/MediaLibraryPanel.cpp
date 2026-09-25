#include "MediaLibraryPanel.h"

#include <QByteArray>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>

#include "io/AudioFileIO.h"
#include "ui/MediaPreviewToggleMath.h"

namespace rsd {

MediaLibraryPanel::MediaLibraryPanel(QWidget* parent) : QListWidget(parent) {
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setAcceptDrops(true); // for external file drops; outgoing drag uses DragOnly above
    setStyleSheet("QListWidget::item { padding: 8px; }");

    connect(this, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        m_previewingRow = nextPreviewIndex(row(item), m_previewingRow);
        emit previewRequested(m_previewingRow);
    });
}

static QString formatDuration(int64_t samples, int sampleRate) {
    if (sampleRate <= 0) return QString();
    double totalSeconds = static_cast<double>(samples) / sampleRate;
    int mins = static_cast<int>(totalSeconds) / 60;
    double secs = totalSeconds - mins * 60;
    return QString("%1:%2").arg(mins).arg(secs, 4, 'f', 1, QChar('0'));
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

    QString duration = formatDuration(buffer->frameCount(), buffer->sampleRate);
    auto* item = new QListWidgetItem(name + "\n" + duration);
    item->setToolTip(QString("%1\nDuration: %2\nSample rate: %3 Hz\nChannels: %4")
                          .arg(name)
                          .arg(duration)
                          .arg(buffer->sampleRate)
                          .arg(buffer->channels));
    addItem(item);
}

void MediaLibraryPanel::resetLibrary() {
    clear();
    m_buffers.clear();
    m_names.clear();
    m_loadedPaths.clear();
    m_previewingRow = -1;
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
        QString canonical = QFileInfo(path).canonicalFilePath();
        if (canonical.isEmpty()) canonical = path; // file vanished between drop and stat; fall back

        if (m_loadedPaths.contains(canonical)) continue; // already in the library

        auto buffer = AudioFileIO::loadFile(path);
        if (!buffer) {
            emit fileLoadFailed(path);
            continue;
        }
        m_loadedPaths.insert(canonical);
        addEntry(QFileInfo(path).fileName(), buffer);
    }
    event->acceptProposedAction();
}

} // namespace rsd
