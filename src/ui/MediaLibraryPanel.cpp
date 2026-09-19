#include "MediaLibraryPanel.h"

#include <QByteArray>

namespace rsd {

MediaLibraryPanel::MediaLibraryPanel(QWidget* parent) : QListWidget(parent) {
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSelectionMode(QAbstractItemView::SingleSelection);
}

void MediaLibraryPanel::refresh(const Session& session) {
    // Preserve selection isn't worth the complexity here — this rebuilds
    // from scratch whenever the session's clips change structurally.
    clear();
    m_buffers.clear();
    m_names.clear();

    for (auto& track : session.tracks) {
        for (auto& clip : *track->clipsSnapshot()) {
            if (!clip->buffer) continue;
            bool alreadyListed = false;
            for (auto& b : m_buffers) {
                if (b.get() == clip->buffer.get()) { alreadyListed = true; break; }
            }
            if (alreadyListed) continue;

            m_buffers.push_back(clip->buffer);
            m_names.push_back(clip->name);
            addItem(clip->name);
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

} // namespace rsd
