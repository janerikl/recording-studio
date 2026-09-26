#pragma once

#include <QString>
#include <QUuid>
#include <QVector>
#include <memory>

namespace rsd {

// A user-defined virtual grouping of Media Browser items. Folders can
// contain items from either the Project Media or Loops tab, referenced by
// LibraryItem::id() rather than owned directly, since the live items
// themselves live in MediaBrowserPanel's per-source vectors.
class LibraryFolder {
public:
    QUuid id = QUuid::createUuid();
    QString name;
    QVector<std::shared_ptr<LibraryFolder>> children;
    QVector<QString> itemRefs;
};

// Owns the root set of virtual folders for a session's Media Browser.
class LibraryFolderTree {
public:
    QVector<std::shared_ptr<LibraryFolder>> roots;

    // Creates a new folder under parent, or at the top level if parent is
    // nullptr.
    std::shared_ptr<LibraryFolder> addFolder(const QString& name, LibraryFolder* parent = nullptr);

    // Removes a folder (and everything nested inside it) wherever it is in
    // the tree. No-op if the id isn't found.
    void removeFolder(const QUuid& id);

    // Renames a folder wherever it is in the tree. No-op if the id isn't found.
    void renameFolder(const QUuid& id, const QString& name);

    // Moves (or newly files) an item into `to`, removing any prior
    // reference to it elsewhere in the tree first. Passing nullptr for `to`
    // just un-files the item.
    void moveItem(const QString& itemId, LibraryFolder* to);

    // Depth-first search for a folder by id; nullptr if not found.
    LibraryFolder* findFolder(const QUuid& id) const;
};

} // namespace rsd
