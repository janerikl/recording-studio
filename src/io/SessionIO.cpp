#include "SessionIO.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

#include "audio/Effects.h"
#include "io/AudioFileIO.h"

namespace rsd {

static QString effectTypeToString(EffectType t) {
    switch (t) {
        case EffectType::EQ: return "eq";
        case EffectType::Compressor: return "compressor";
        case EffectType::Delay: return "delay";
        case EffectType::Reverb: return "reverb";
    }
    return "eq";
}

static QJsonObject effectToJson(const Effect& effect) {
    QJsonObject json;
    json["id"] = effect.id.toString();
    json["type"] = effectTypeToString(effect.type());
    json["bypassed"] = effect.bypassed.load();

    switch (effect.type()) {
        case EffectType::EQ: {
            auto& eq = static_cast<const EqEffect&>(effect);
            json["lowGainDb"] = eq.lowGainDb.load();
            json["lowFreqHz"] = eq.lowFreqHz.load();
            json["midGainDb"] = eq.midGainDb.load();
            json["midFreqHz"] = eq.midFreqHz.load();
            json["midQ"] = eq.midQ.load();
            json["highGainDb"] = eq.highGainDb.load();
            json["highFreqHz"] = eq.highFreqHz.load();
            break;
        }
        case EffectType::Compressor: {
            auto& comp = static_cast<const CompressorEffect&>(effect);
            json["thresholdDb"] = comp.thresholdDb.load();
            json["ratio"] = comp.ratio.load();
            json["attackMs"] = comp.attackMs.load();
            json["releaseMs"] = comp.releaseMs.load();
            json["makeupDb"] = comp.makeupDb.load();
            break;
        }
        case EffectType::Delay: {
            auto& delay = static_cast<const DelayEffect&>(effect);
            json["delayMs"] = delay.delayMs.load();
            json["feedback"] = delay.feedback.load();
            json["mix"] = delay.mix.load();
            break;
        }
        case EffectType::Reverb: {
            auto& reverb = static_cast<const ReverbEffect&>(effect);
            json["roomSize"] = reverb.roomSize.load();
            json["damping"] = reverb.damping.load();
            json["mix"] = reverb.mix.load();
            break;
        }
    }
    return json;
}

// `sampleRate` prepares the effect's internal DSP state (filter/delay/
// reverb buffers) so it's ready to process as soon as it's attached to a
// live track, matching how a freshly-added effect is prepared in the UI.
static std::shared_ptr<Effect> effectFromJson(const QJsonObject& json, double sampleRate) {
    QString typeStr = json["type"].toString();
    std::shared_ptr<Effect> effect;

    if (typeStr == "eq") {
        auto eq = std::make_shared<EqEffect>();
        eq->lowGainDb.store(static_cast<float>(json["lowGainDb"].toDouble(0.0)));
        eq->lowFreqHz.store(static_cast<float>(json["lowFreqHz"].toDouble(120.0)));
        eq->midGainDb.store(static_cast<float>(json["midGainDb"].toDouble(0.0)));
        eq->midFreqHz.store(static_cast<float>(json["midFreqHz"].toDouble(1000.0)));
        eq->midQ.store(static_cast<float>(json["midQ"].toDouble(0.7)));
        eq->highGainDb.store(static_cast<float>(json["highGainDb"].toDouble(0.0)));
        eq->highFreqHz.store(static_cast<float>(json["highFreqHz"].toDouble(8000.0)));
        effect = eq;
    } else if (typeStr == "compressor") {
        auto comp = std::make_shared<CompressorEffect>();
        comp->thresholdDb.store(static_cast<float>(json["thresholdDb"].toDouble(-18.0)));
        comp->ratio.store(static_cast<float>(json["ratio"].toDouble(4.0)));
        comp->attackMs.store(static_cast<float>(json["attackMs"].toDouble(10.0)));
        comp->releaseMs.store(static_cast<float>(json["releaseMs"].toDouble(100.0)));
        comp->makeupDb.store(static_cast<float>(json["makeupDb"].toDouble(0.0)));
        effect = comp;
    } else if (typeStr == "delay") {
        auto delay = std::make_shared<DelayEffect>();
        delay->delayMs.store(static_cast<float>(json["delayMs"].toDouble(300.0)));
        delay->feedback.store(static_cast<float>(json["feedback"].toDouble(0.35)));
        delay->mix.store(static_cast<float>(json["mix"].toDouble(0.3)));
        effect = delay;
    } else if (typeStr == "reverb") {
        auto reverb = std::make_shared<ReverbEffect>();
        reverb->roomSize.store(static_cast<float>(json["roomSize"].toDouble(0.5)));
        reverb->damping.store(static_cast<float>(json["damping"].toDouble(0.5)));
        reverb->mix.store(static_cast<float>(json["mix"].toDouble(0.25)));
        effect = reverb;
    } else {
        return nullptr;
    }

    effect->id = QUuid(json["id"].toString());
    effect->bypassed.store(json["bypassed"].toBool(false));
    effect->prepare(sampleRate);
    return effect;
}

static QString audioFilesDirFor(const QString& projectPath) {
    QFileInfo info(projectPath);
    return info.absolutePath() + "/" + info.completeBaseName() + "_audiofiles";
}

static FadeCurve parseFadeCurve(const QString& s) {
    return s == "equalPower" ? FadeCurve::EqualPower : FadeCurve::Linear;
}

bool SessionIO::saveSession(const QString& projectPath, const Session& session,
                             const QVector<LibraryEntry>& libraryEntries) {
    QString audioDir = audioFilesDirFor(projectPath);
    QDir().mkpath(audioDir);

    // Write each unique AudioBuffer once, even if several clips share it
    // (e.g. after a split), and reuse the same relative path for all of them.
    QHash<const AudioBuffer*, QString> writtenFiles;

    QJsonObject root;
    root["sampleRate"] = session.sampleRate;
    root["channels"] = session.channels;
    root["bpm"] = session.bpm;
    root["metronomeEnabled"] = session.metronomeEnabled;

    QJsonArray tracksJson;
    for (auto& track : session.tracks) {
        QJsonObject trackJson;
        trackJson["id"] = track->id.toString();
        trackJson["name"] = track->name;
        trackJson["kind"] = track->kind == TrackKind::Bus       ? "bus"
                             : track->kind == TrackKind::Instrument ? "instrument"
                                                                     : "audio";
        trackJson["volume"] = track->volume.load();
        trackJson["pan"] = track->pan.load();
        trackJson["muted"] = track->muted.load();
        trackJson["soloed"] = track->soloed.load();
        QUuid sendBusId = track->sendBusId();
        if (!sendBusId.isNull()) trackJson["sendBusId"] = sendBusId.toString();
        trackJson["sendLevel"] = track->sendLevel.load();

        QJsonArray automationLanesJson;
        for (auto& lane : *track->automationLanesSnapshot()) {
            QJsonObject laneJson;
            laneJson["target"] = lane->target == AutomationTarget::Volume ? "volume" : "pan";
            QJsonArray pointsJson;
            for (auto& point : lane->points) {
                QJsonObject pointJson;
                pointJson["sample"] = QString::number(point.sample);
                pointJson["value"] = point.value;
                pointsJson.append(pointJson);
            }
            laneJson["points"] = pointsJson;
            automationLanesJson.append(laneJson);
        }
        trackJson["automationLanes"] = automationLanesJson;

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
            clipJson["gain"] = clip->gain;
            clipJson["fadeInSamples"] = QString::number(clip->fadeInSamples);
            clipJson["fadeOutSamples"] = QString::number(clip->fadeOutSamples);
            clipJson["fadeInCurve"] = clip->fadeInCurve == FadeCurve::EqualPower ? "equalPower" : "linear";
            clipJson["fadeOutCurve"] = clip->fadeOutCurve == FadeCurve::EqualPower ? "equalPower" : "linear";
            clipsJson.append(clipJson);
        }

        trackJson["clips"] = clipsJson;

        QJsonArray effectsJson;
        for (auto& effect : *track->effectsSnapshot()) {
            effectsJson.append(effectToJson(*effect));
        }
        trackJson["effects"] = effectsJson;

        tracksJson.append(trackJson);
    }
    root["tracks"] = tracksJson;

    QJsonArray markersJson;
    for (auto& [slot, position] : session.markers) {
        QJsonObject markerJson;
        markerJson["slot"] = slot;
        markerJson["position"] = QString::number(position);
        markersJson.append(markerJson);
    }
    root["markers"] = markersJson;

    root["masterVolume"] = session.masterBus.volume.load();
    QJsonArray masterEffectsJson;
    for (auto& effect : *session.masterBus.effectsSnapshot()) {
        masterEffectsJson.append(effectToJson(*effect));
    }
    root["masterEffects"] = masterEffectsJson;

    QJsonArray libraryJson;
    for (auto& entry : libraryEntries) {
        const QString& name = entry.first;
        auto& buffer = entry.second;
        if (!buffer) continue;

        QString relPath;
        auto it = writtenFiles.find(buffer.get());
        if (it != writtenFiles.end()) {
            relPath = it.value();
        } else {
            QString fileName = QUuid::createUuid().toString(QUuid::WithoutBraces) + ".wav";
            QString fullPath = audioDir + "/" + fileName;
            if (!AudioFileIO::writeFile(fullPath, *buffer)) {
                std::cerr << "Failed to write audio file: " << fullPath.toStdString() << "\n";
                continue;
            }
            relPath = QFileInfo(audioDir).fileName() + "/" + fileName;
            writtenFiles[buffer.get()] = relPath;
        }

        QJsonObject entryJson;
        entryJson["name"] = name;
        entryJson["audioFile"] = relPath;
        libraryJson.append(entryJson);
    }
    root["mediaLibrary"] = libraryJson;

    QFile file(projectPath);
    if (!file.open(QIODevice::WriteOnly)) {
        std::cerr << "Failed to write project file: " << projectPath.toStdString() << "\n";
        return false;
    }
    file.write(QJsonDocument(root).toJson());
    return true;
}

bool SessionIO::loadSession(const QString& projectPath, Session& outSession,
                             QVector<LibraryEntry>& outLibraryEntries) {
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
    outSession.bpm = root["bpm"].toDouble(outSession.bpm);
    outSession.metronomeEnabled = root["metronomeEnabled"].toBool(outSession.metronomeEnabled);
    outSession.tracks.clear();
    outSession.markers.clear();
    for (const auto& markerVal : root["markers"].toArray()) {
        QJsonObject markerJson = markerVal.toObject();
        int slot = markerJson["slot"].toInt();
        int64_t position = markerJson["position"].toString().toLongLong();
        outSession.markers[slot] = position;
    }

    // Share one AudioBuffer per unique audio file across all clips that
    // reference it, instead of reloading it from disk for each clip.
    QHash<QString, std::shared_ptr<AudioBuffer>> loadedBuffers;

    for (const auto& trackVal : root["tracks"].toArray()) {
        QJsonObject trackJson = trackVal.toObject();

        auto track = std::make_shared<Track>();
        track->id = QUuid(trackJson["id"].toString());
        track->name = trackJson["name"].toString();
        QString kindStr = trackJson["kind"].toString("audio");
        track->kind = kindStr == "bus" ? TrackKind::Bus
                      : kindStr == "instrument" ? TrackKind::Instrument
                                                 : TrackKind::Audio;
        track->volume.store(static_cast<float>(trackJson["volume"].toDouble(1.0)));
        track->pan.store(static_cast<float>(trackJson["pan"].toDouble(0.0)));
        track->muted.store(trackJson["muted"].toBool(false));
        track->soloed.store(trackJson["soloed"].toBool(false));
        track->setSendBusId(QUuid(trackJson["sendBusId"].toString()));
        track->sendLevel.store(static_cast<float>(trackJson["sendLevel"].toDouble(0.0)));

        Track::AutomationLaneList automationLanes;
        for (const auto& laneVal : trackJson["automationLanes"].toArray()) {
            QJsonObject laneJson = laneVal.toObject();
            auto lane = std::make_shared<AutomationLane>();
            lane->target =
                laneJson["target"].toString() == "pan" ? AutomationTarget::Pan : AutomationTarget::Volume;
            for (const auto& pointVal : laneJson["points"].toArray()) {
                QJsonObject pointJson = pointVal.toObject();
                lane->points.push_back(
                    {pointJson["sample"].toString().toLongLong(),
                     static_cast<float>(pointJson["value"].toDouble())});
            }
            automationLanes.push_back(std::move(lane));
        }
        track->restoreAutomationLanes(
            std::make_shared<const Track::AutomationLaneList>(std::move(automationLanes)));

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
            clip->gain = static_cast<float>(clipJson["gain"].toDouble(1.0));
            clip->fadeInSamples = clipJson["fadeInSamples"].toString().toLongLong();
            clip->fadeOutSamples = clipJson["fadeOutSamples"].toString().toLongLong();
            clip->fadeInCurve = parseFadeCurve(clipJson["fadeInCurve"].toString());
            clip->fadeOutCurve = parseFadeCurve(clipJson["fadeOutCurve"].toString());

            track->addClip(clip);
        }

        for (const auto& effectVal : trackJson["effects"].toArray()) {
            auto effect = effectFromJson(effectVal.toObject(), outSession.sampleRate);
            if (effect) track->addEffect(effect);
        }

        outSession.tracks.push_back(track);
    }

    outSession.masterBus.volume.store(static_cast<float>(root["masterVolume"].toDouble(1.0)));
    for (const auto& effectVal : root["masterEffects"].toArray()) {
        auto effect = effectFromJson(effectVal.toObject(), outSession.sampleRate);
        if (effect) outSession.masterBus.addEffect(effect);
    }

    for (const auto& entryVal : root["mediaLibrary"].toArray()) {
        QJsonObject entryJson = entryVal.toObject();
        QString relPath = entryJson["audioFile"].toString();
        QString fullPath = baseDir.absoluteFilePath(relPath);

        std::shared_ptr<AudioBuffer> buffer;
        auto it = loadedBuffers.find(fullPath);
        if (it != loadedBuffers.end()) {
            buffer = it.value();
        } else {
            buffer = AudioFileIO::loadFile(fullPath);
            if (!buffer) {
                std::cerr << "Missing audio file referenced by session: " << fullPath.toStdString()
                          << "\n";
                continue;
            }
            loadedBuffers[fullPath] = buffer;
        }

        outLibraryEntries.append({entryJson["name"].toString(), buffer});
    }

    return true;
}

} // namespace rsd
