#include "SessionIO.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

#include "io/AudioFileIO.h"

namespace rsd {

static QString audioFilesDirFor(const QString& projectPath) {
    QFileInfo info(projectPath);
    return info.absolutePath() + "/" + info.completeBaseName() + "_audiofiles";
}

bool SessionIO::saveSession(const QString& projectPath, const Session& session) {
    QString audioDir = audioFilesDirFor(projectPath);
    QDir().mkpath(audioDir);

    // Write each unique AudioBuffer once, even if several clips share it
    // (e.g. after a split), and reuse the same relative path for all of them.
    QHash<const AudioBuffer*, QString> writtenFiles;

    QJsonObject root;
    root["sampleRate"] = session.sampleRate;
    root["channels"] = session.channels;

    QJsonArray tracksJson;
    for (auto& track : session.tracks) {
        QJsonObject trackJson;
        trackJson["id"] = track->id.toString();
        trackJson["name"] = track->name;
        trackJson["gainL"] = track->gainL.load();
        trackJson["gainR"] = track->gainR.load();
        trackJson["muted"] = track->muted.load();
        trackJson["soloed"] = track->soloed.load();

        QJsonArray clipsJson;
        for (auto& clip : *track->clipsSnapshot()) {
            if (!clip->buffer) continue;

            QString relPath;
            auto it = writtenFiles.find(clip->buffer.get());
            if (it != writtenFiles.end()) {
                relPath = it.value();
            } else {
                QString fileName = clip->id.toString(QUuid::WithoutBraces) + ".wav";
                QString fullPath = audioDir + "/" + fileName;
                if (!AudioFileIO::writeFile(fullPath, *clip->buffer)) {
                    std::cerr << "Failed to write audio file: " << fullPath.toStdString() << "\n";
                    continue;
                }
                relPath = QFileInfo(audioDir).fileName() + "/" + fileName;
                writtenFiles[clip->buffer.get()] = relPath;
            }

            QJsonObject clipJson;
            clipJson["id"] = clip->id.toString();
            clipJson["name"] = clip->name;
            clipJson["audioFile"] = relPath;
            clipJson["sessionStartSample"] = QString::number(clip->sessionStartSample);
            clipJson["sourceOffsetSamples"] = QString::number(clip->sourceOffsetSamples);
            clipJson["lengthSamples"] = QString::number(clip->lengthSamples);
            clipJson["muted"] = clip->muted;
            clipsJson.append(clipJson);
        }

        trackJson["clips"] = clipsJson;
        tracksJson.append(trackJson);
    }
    root["tracks"] = tracksJson;

    QFile file(projectPath);
    if (!file.open(QIODevice::WriteOnly)) {
        std::cerr << "Failed to write project file: " << projectPath.toStdString() << "\n";
        return false;
    }
    file.write(QJsonDocument(root).toJson());
    return true;
}

bool SessionIO::loadSession(const QString& projectPath, Session& outSession) {
    QFile file(projectPath);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "Failed to open project file: " << projectPath.toStdString() << "\n";
        return false;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (doc.isNull()) {
        std::cerr << "Invalid project JSON: " << err.errorString().toStdString() << "\n";
        return false;
    }

    QJsonObject root = doc.object();
    QDir baseDir = QFileInfo(projectPath).absoluteDir();

    outSession.sampleRate = root["sampleRate"].toInt(outSession.sampleRate);
    outSession.channels = root["channels"].toInt(outSession.channels);
    outSession.tracks.clear();

    // Share one AudioBuffer per unique audio file across all clips that
    // reference it, instead of reloading it from disk for each clip.
    QHash<QString, std::shared_ptr<AudioBuffer>> loadedBuffers;

    for (const auto& trackVal : root["tracks"].toArray()) {
        QJsonObject trackJson = trackVal.toObject();

        auto track = std::make_shared<Track>();
        track->id = QUuid(trackJson["id"].toString());
        track->name = trackJson["name"].toString();
        track->gainL.store(static_cast<float>(trackJson["gainL"].toDouble(1.0)));
        track->gainR.store(static_cast<float>(trackJson["gainR"].toDouble(1.0)));
        track->muted.store(trackJson["muted"].toBool(false));
        track->soloed.store(trackJson["soloed"].toBool(false));

        for (const auto& clipVal : trackJson["clips"].toArray()) {
            QJsonObject clipJson = clipVal.toObject();
            QString relPath = clipJson["audioFile"].toString();
            QString fullPath = baseDir.absoluteFilePath(relPath);

            std::shared_ptr<AudioBuffer> buffer;
            auto it = loadedBuffers.find(fullPath);
            if (it != loadedBuffers.end()) {
                buffer = it.value();
            } else {
                buffer = AudioFileIO::loadFile(fullPath);
                if (!buffer) {
                    std::cerr << "Missing audio file referenced by session: "
                              << fullPath.toStdString() << "\n";
                    continue;
                }
                loadedBuffers[fullPath] = buffer;
            }

            auto clip = std::make_shared<Clip>();
            clip->id = QUuid(clipJson["id"].toString());
            clip->name = clipJson["name"].toString();
            clip->buffer = buffer;
            clip->sessionStartSample = clipJson["sessionStartSample"].toString().toLongLong();
            clip->sourceOffsetSamples = clipJson["sourceOffsetSamples"].toString().toLongLong();
            clip->lengthSamples = clipJson["lengthSamples"].toString().toLongLong();
            clip->muted = clipJson["muted"].toBool(false);

            track->addClip(clip);
        }

        outSession.tracks.push_back(track);
    }

    return true;
}

} // namespace rsd
