#pragma once

#include <QStyledItemDelegate>

namespace rsd {

class MediaBrowserPanel;

// Paints a compact waveform-peaks thumbnail in the right-hand strip of each
// Media Browser leaf row (folder rows get no thumbnail), backed by
// MediaBrowserPanel::peaksForItemId — cached in memory, decoded on demand
// for Loop items not yet in the session.
class LibraryItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    LibraryItemDelegate(MediaBrowserPanel* owner, QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    MediaBrowserPanel* m_owner;
};

} // namespace rsd
