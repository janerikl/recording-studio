#pragma once

#include <QString>

#include "model/Session.h"

namespace rsd {

// Serializes a Session to a .rsdproj JSON file plus a companion
// "<name>_audiofiles/" directory holding one WAV per unique AudioBuffer
// referenced by any clip (Ardour-style: project file + external audio).
class SessionIO {
public:
    static bool saveSession(const QString& projectPath, const Session& session);

    // Replaces outSession's tracks in place (keeps the same Session address,
    // since AudioEngine holds a raw pointer to it).
    static bool loadSession(const QString& projectPath, Session& outSession);
};

} // namespace rsd
