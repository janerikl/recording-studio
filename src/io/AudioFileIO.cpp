#include "AudioFileIO.h"

#include <sndfile.h>

#include <QByteArray>
#include <iostream>

namespace rsd {

std::shared_ptr<AudioBuffer> AudioFileIO::loadFile(const QString& path) {
    SF_INFO info{};
    SNDFILE* file = sf_open(path.toUtf8().constData(), SFM_READ, &info);
    if (!file) {
        std::cerr << "Failed to open " << path.toStdString() << ": " << sf_strerror(nullptr) << "\n";
        return nullptr;
    }

    auto buffer = std::make_shared<AudioBuffer>();
    buffer->channels = info.channels;
    buffer->sampleRate = static_cast<int>(info.samplerate);
    buffer->samples.resize(static_cast<size_t>(info.frames) * info.channels);

    sf_count_t readCount = sf_readf_float(file, buffer->samples.data(), info.frames);
    sf_close(file);

    if (readCount != info.frames) {
        buffer->samples.resize(static_cast<size_t>(readCount) * info.channels);
    }

    return buffer;
}

bool AudioFileIO::writeFile(const QString& path, const AudioBuffer& buffer, ExportFormat format) {
    SF_INFO info{};
    info.samplerate = buffer.sampleRate;
    info.channels = buffer.channels;
    info.format = SF_FORMAT_WAV | (format == ExportFormat::Wav16Pcm ? SF_FORMAT_PCM_16 : SF_FORMAT_FLOAT);

    SNDFILE* file = sf_open(path.toUtf8().constData(), SFM_WRITE, &info);
    if (!file) {
        std::cerr << "Failed to write " << path.toStdString() << ": " << sf_strerror(nullptr) << "\n";
        return false;
    }

    sf_count_t frames = buffer.frameCount();
    sf_count_t written = sf_writef_float(file, buffer.samples.data(), frames);
    sf_close(file);

    return written == frames;
}

} // namespace rsd
