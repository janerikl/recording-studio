#pragma once

namespace rsd {

// Click-to-preview toggle logic shared by MediaLibraryPanel (and mirroring
// LoopBrowserPanel's inline equivalent): clicking the row already previewing
// stops it (-1); clicking any other row previews that one instead.
inline int nextPreviewIndex(int clickedIndex, int currentlyPreviewingIndex) {
    return clickedIndex == currentlyPreviewingIndex ? -1 : clickedIndex;
}

} // namespace rsd
