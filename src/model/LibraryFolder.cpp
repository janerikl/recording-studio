#include "LibraryFolder.h"

namespace rsd {

namespace {

LibraryFolder* findFolderIn(const QVector<std::shared_ptr<LibraryFolder>>& folders, const QUuid& id) {
    for (auto& folder : folders) {
        if (folder->id == id) return folder.get();
        if (auto* found = findFolderIn(folder->children, id)) return found;
    }
    return nullptr;
}

bool removeFolderFrom(QVector<std::shared_ptr<LibraryFolder>>& folders, const QUuid& id) {
    for (int i = 0; i < folders.size(); ++i) {
        if (folders[i]->id == id) {
            folders.remove(i);
            return true;
        }
        if (removeFolderFrom(folders[i]->children, id)) return true;
    }
    return false;
}

bool removeItemRefFrom(QVector<std::shared_ptr<LibraryFolder>>& folders, const QString& itemId) {
    bool removed = false;
    for (auto& folder : folders) {
        folder->itemRefs.removeAll(itemId);
        removed = removeItemRefFrom(folder->children, itemId) || removed;
    }
    return removed;
}

} // namespace

std::shared_ptr<LibraryFolder> LibraryFolderTree::addFolder(const QString& name, LibraryFolder* parent) {
    auto folder = std::make_shared<LibraryFolder>();
    folder->name = name;

    if (parent) {
        parent->children.push_back(folder);
    } else {
        roots.push_back(folder);
    }
    return folder;
}

void LibraryFolderTree::removeFolder(const QUuid& id) {
    removeFolderFrom(roots, id);
}

void LibraryFolderTree::moveItem(const QString& itemId, LibraryFolder* to) {
    removeItemRefFrom(roots, itemId);
    if (to) to->itemRefs.push_back(itemId);
}

LibraryFolder* LibraryFolderTree::findFolder(const QUuid& id) const {
    return findFolderIn(roots, id);
}

} // namespace rsd
