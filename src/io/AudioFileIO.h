#pragma once

#include <QString>
#include <memory>

#include "model/AudioBuffer.h"

namespace rsd {

// Thin libsndfile wrapper for WAV/FLAC/etc read+write.
class AudioFileIO {
public:
    // Returns nullptr on failure.
    static std::shared_ptr<AudioBuffer> loadFile(const QString& path);

    // Writes interleaved float samples. Returns false on failure.
    static bool writeFile(const QString& path, const AudioBuffer& buffer);
};

} // namespace rsd
