#pragma once

#include <QString>
#include <memory>

#include "model/AudioBuffer.h"

namespace rsd {

enum class ExportFormat { Wav32Float, Wav16Pcm };

// Thin libsndfile wrapper for WAV/FLAC/etc read+write.
class AudioFileIO {
public:
    // Returns nullptr on failure.
    static std::shared_ptr<AudioBuffer> loadFile(const QString& path);

    // Writes interleaved float samples. Returns false on failure. For
    // Wav16Pcm, libsndfile converts float -> 16-bit int internally; the
    // caller's buffer stays float, no separate conversion needed here.
    static bool writeFile(const QString& path, const AudioBuffer& buffer,
                           ExportFormat format = ExportFormat::Wav32Float);
};

} // namespace rsd
