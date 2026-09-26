#include "LibraryItemDelegate.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <algorithm>

#include "ui/MediaBrowserPanel.h"
#include "ui/WaveformDisplayMath.h"

namespace rsd {

namespace {
constexpr int kThumbnailWidth = 90;
constexpr int kThumbnailMargin = 6;
constexpr int kTextMargin = 4;
constexpr int kLineGap = 3; // whitespace between the name and duration lines
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

    // Draw the row's background/selection/hover state via the style, but
    // with no text of its own — the name and duration are painted below by
    // hand so the duration can use a smaller font.
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.text.clear();
    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    QRect textRect = option.rect.adjusted(kTextMargin, 0, -(kThumbnailWidth + kThumbnailMargin), 0);
    QString name = index.data(Qt::DisplayRole).toString();
    QString duration = index.data(MediaBrowserPanel::kDurationRole).toString();

    QPalette::ColorGroup colorGroup =
        (option.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
    QColor nameColor = option.palette.color(
        colorGroup, (option.state & QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text);

    QFont nameFont = option.font;
    QFontMetrics nameFm(nameFont);

    painter->save();
    if (duration.isEmpty()) {
        painter->setFont(nameFont);
        painter->setPen(nameColor);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, name);
    } else {
        QFont durationFont = nameFont;
        durationFont.setPointSizeF(std::max(nameFont.pointSizeF() - 2.0, 7.0));
        QFontMetrics durationFm(durationFont);

        int totalTextHeight = nameFm.height() + kLineGap + durationFm.height();
        int top = textRect.top() + (textRect.height() - totalTextHeight) / 2;

        painter->setFont(nameFont);
        painter->setPen(nameColor);
        QRect nameRect(textRect.left(), top, textRect.width(), nameFm.height());
        painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, name);

        QColor durationColor = nameColor;
        durationColor.setAlpha(durationColor.alpha() * 0.7);
        painter->setFont(durationFont);
        painter->setPen(durationColor);
        QRect durationRect(textRect.left(), nameRect.bottom() + kLineGap, textRect.width(),
                            durationFm.height());
        painter->drawText(durationRect, Qt::AlignLeft | Qt::AlignVCenter, duration);
    }
    painter->restore();

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

    int textHeight = QFontMetrics(option.font).height();
    QString duration = index.data(MediaBrowserPanel::kDurationRole).toString();
    if (!duration.isEmpty()) {
        QFont durationFont = option.font;
        durationFont.setPointSizeF(std::max(option.font.pointSizeF() - 2.0, 7.0));
        textHeight += kLineGap + QFontMetrics(durationFont).height();
    }
    int height = std::max({base.height(), textHeight + 6, 28});

    return QSize(base.width() + kThumbnailWidth, height);
}

} // namespace rsd
