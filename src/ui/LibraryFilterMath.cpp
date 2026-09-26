#include "ui/LibraryFilterMath.h"

namespace rsd {

QVector<LibraryItem> filterLibraryItems(const QVector<LibraryItem>& items, const QString& filter) {
    if (filter.isEmpty()) return items;

    QVector<LibraryItem> result;
    for (const auto& item : items) {
        if (item.name.contains(filter, Qt::CaseInsensitive)) result.push_back(item);
    }
    return result;
}

} // namespace rsd
