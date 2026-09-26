#include "ui/LibraryItemOps.h"

namespace rsd {

QVector<LibraryItem> renameLibraryItemById(const QVector<LibraryItem>& items, const QString& id,
                                            const QString& newName) {
    QVector<LibraryItem> result = items;
    for (auto& item : result) {
        if (item.id() == id) {
            item.name = newName;
            break;
        }
    }
    return result;
}

QVector<LibraryItem> removeLibraryItemById(const QVector<LibraryItem>& items, const QString& id) {
    QVector<LibraryItem> result;
    result.reserve(items.size());
    for (const auto& item : items) {
        if (item.id() != id) result.push_back(item);
    }
    return result;
}

} // namespace rsd
