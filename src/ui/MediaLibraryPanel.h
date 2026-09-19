#pragma once

#include <QListWidget>
#include <QMimeData>
#include <QVector>
#include <memory>

#include "model/AudioBuffer.h"
#include "model/Session.h"

namespace rsd {

// Lists every unique audio file (AudioBuffer, deduped by identity) already
// used somewhere in the session, and lets the user drag one onto a track's
// clip lane to add another clip referencing that same shared buffer.
class MediaLibraryPanel : public QListWidget {
    Q_OBJECT

public:
    explicit MediaLibraryPanel(QWidget* parent = nullptr);

    void refresh(const Session& session);

    std::shared_ptr<AudioBuffer> bufferAt(int index) const;
    QString nameAt(int index) const;

    static constexpr auto kMimeType = "application/x-rsd-library-index";

protected:
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override;

private:
    QVector<std::shared_ptr<AudioBuffer>> m_buffers;
    QVector<QString> m_names;
};

} // namespace rsd
