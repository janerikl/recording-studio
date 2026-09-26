#include "LibraryItemDelegate.h"

#include <QPainter>
#include <algorithm>

#include "ui/MediaBrowserPanel.h"
#include "ui/WaveformDisplayMath.h"

namespace rsd {

namespace {
constexpr int kThumbnailWidth = 90;
constexpr int kThumbnailMargin = 6;
} // namespace

LibraryItemDelegate::LibraryItemDelegate(MediaBrowserPanel* owner, QObject* parent)
    : QStyledItemDelegate(parent), m_owner(owner) {}

void LibraryItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const {
    QVariant itemIdVar = index.data(MediaBrowserPanel::kItemIdRole);
    if (!itemIdVar.isValid()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem textOption = option;
    textOption.rect.adjust(0, 0, -(kThumbnailWidth + kThumbnailMargin), 0);
    QStyledItemDelegate::paint(painter, textOption, index);

    QRect thumbRect(option.rect.right() - kThumbnailWidth, option.rect.top() + 2, kThumbnailWidth - 4,
                     option.rect.height() - 4);
    if (thumbRect.width() <= 0 || thumbRect.height() <= 0) return;

    auto peaks = m_owner->peaksForItemId(itemIdVar.toString(), thumbRect.width());
    if (peaks.isEmpty()) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(option.palette.color(QPalette::Highlight));

    float scale = computeWaveformDisplayScale(peaks);
    float midY = thumbRect.top() + thumbRect.height() / 2.0f;
    float halfHeight = thumbRect.height() / 2.0f;

    for (int x = 0; x < peaks.size() && x < thumbRect.width(); ++x) {
        auto [minV, maxV] = peaks[x];
        float top = midY - std::clamp(maxV * scale, -1.0f, 1.0f) * halfHeight;
        float bottom = midY - std::clamp(minV * scale, -1.0f, 1.0f) * halfHeight;
        painter->drawLine(QPointF(thumbRect.left() + x, top), QPointF(thumbRect.left() + x, bottom));
    }

    painter->restore();
}

QSize LibraryItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QSize base = QStyledItemDelegate::sizeHint(option, index);
    return QSize(base.width() + kThumbnailWidth, std::max(base.height(), 28));
}

} // namespace rsd
