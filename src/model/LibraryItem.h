#pragma once

#include <QString>
#include <memory>

#include "model/AudioBuffer.h"

namespace rsd {

enum class LibrarySource { ProjectMedia, Loop };

// Uniform view of a Media Browser row, whichever tab it came from. A
// ProjectMedia entry always has a buffer already in memory; a Loop entry is
// just a file on disk until something (preview, drag-drop, waveform
// thumbnail) needs it decoded.
struct LibraryItem {
    LibrarySource source = LibrarySource::ProjectMedia;
    QString name;
    std::shared_ptr<AudioBuffer> buffer; // set for ProjectMedia; null for Loop until decoded
    QString path;                        // canonical path; always set for Loop
    // Persistent identity for a ProjectMedia entry, stable across session
    // save/reload (a raw buffer pointer isn't). Assigned once when the
    // entry is first added to the library.
    QString itemId;

    // Stable key for this item usable as a map key (waveform cache, virtual
    // folder membership) and safe to persist across a session reload.
    QString id() const {
        switch (source) {
            case LibrarySource::ProjectMedia:
                return "pm:" + itemId;
            case LibrarySource::Loop:
                return "loop:" + path;
        }
        return QString();
    }

    bool isValid() const {
        return source == LibrarySource::ProjectMedia ? !itemId.isEmpty() : !path.isEmpty();
    }
};

} // namespace rsd
