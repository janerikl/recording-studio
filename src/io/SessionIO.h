#pragma once

#include <QString>
#include <QVector>
#include <utility>

#include "model/Session.h"

namespace rsd {

// One Media Library entry: display name + its audio data.
using LibraryEntry = std::pair<QString, std::shared_ptr<AudioBuffer>>;

// Serializes a Session to a .rsdproj JSON file plus a companion
// "<name>_audiofiles/" directory holding one WAV per unique AudioBuffer
// referenced by any clip or Media Library entry (Ardour-style: project
// file + external audio). Media Library entries are saved/restored
// alongside tracks even if not placed on any track's clip lane, since
// they aren't otherwise represented anywhere in the Session model.
class SessionIO {
public:
    static bool saveSession(const QString& projectPath, const Session& session,
                             const QVector<LibraryEntry>& libraryEntries);

    // Replaces outSession's tracks in place (keeps the same Session address,
    // since AudioEngine holds a raw pointer to it). outLibraryEntries is
    // appended with every saved Media Library entry.
    static bool loadSession(const QString& projectPath, Session& outSession,
                             QVector<LibraryEntry>& outLibraryEntries);
};

} // namespace rsd
