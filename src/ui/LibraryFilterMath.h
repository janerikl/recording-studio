#pragma once

#include <QString>
#include <QVector>

#include "model/LibraryItem.h"

namespace rsd {

// Items whose name contains `filter` (case-insensitive). An empty filter
// keeps every item. Shared by both Media Browser tabs so Project Media and
// Loops filter identically.
QVector<LibraryItem> filterLibraryItems(const QVector<LibraryItem>& items, const QString& filter);

} // namespace rsd
