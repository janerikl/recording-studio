#pragma once

#include <QString>
#include <QVector>

#include "model/LibraryItem.h"

namespace rsd {

// Returns `items` with the entry matching `id` (LibraryItem::id()) renamed.
// No-op (returns `items` unchanged) if no entry matches.
QVector<LibraryItem> renameLibraryItemById(const QVector<LibraryItem>& items, const QString& id,
                                            const QString& newName);

// Returns `items` with the entry matching `id` (LibraryItem::id()) dropped.
// No-op (returns `items` unchanged) if no entry matches.
QVector<LibraryItem> removeLibraryItemById(const QVector<LibraryItem>& items, const QString& id);

} // namespace rsd
