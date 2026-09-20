#include <QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "audio/Effects.h"
#include "io/SessionIO.h"

using namespace rsd;

class TestSessionIO : public QObject {
    Q_OBJECT

private slots:
    void roundTripsClipGainAndFades() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.filePath("session.rsdproj");

        Session session;
        auto track = session.addTrack("T0");

        auto clip = std::make_shared<Clip>();
        clip->buffer = std::make_shared<AudioBuffer>();
        clip->buffer->channels = 1;
        clip->buffer->sampleRate = 48000;
        clip->buffer->samples.assign(48000, 0.1f);
        clip->lengthSamples = 48000;
        clip->gain = 0.6f;
        clip->fadeInSamples = 100;
        clip->fadeOutSamples = 200;
        clip->fadeInCurve = FadeCurve::EqualPower;
        clip->fadeOutCurve = FadeCurve::Linear;
        track->addClip(clip);

        QVector<LibraryEntry> emptyLibrary;
        QVERIFY(SessionIO::saveSession(path, session, emptyLibrary));

        Session loaded;
        QVector<LibraryEntry> loadedLibrary;
        QVERIFY(SessionIO::loadSession(path, loaded, loadedLibrary));

        QCOMPARE(loaded.tracks.size(), size_t(1));
        auto loadedTrack = loaded.tracks.front();

        auto clips = loadedTrack->clipsSnapshot();
        QCOMPARE(clips->size(), size_t(1));
        auto loadedClip = clips->front();
        QVERIFY(qFuzzyCompare(loadedClip->gain, 0.6f));
        QCOMPARE(loadedClip->fadeInSamples, int64_t(100));
        QCOMPARE(loadedClip->fadeOutSamples, int64_t(200));
        QCOMPARE(loadedClip->fadeInCurve, FadeCurve::EqualPower);
        QCOMPARE(loadedClip->fadeOutCurve, FadeCurve::Linear);
    }

    void loadingSessionWithoutNewFieldsUsesDefaults() {
        // Simulates an old .rsdproj saved before gain/fade/volume/pan
        // existed: those keys are simply absent from the JSON.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.filePath("session.rsdproj");

        Session session;
        auto track = session.addTrack("T0");
        auto clip = std::make_shared<Clip>();
        clip->buffer = std::make_shared<AudioBuffer>();
        clip->buffer->channels = 1;
        clip->buffer->sampleRate = 48000;
        clip->buffer->samples.assign(100, 0.0f);
        clip->lengthSamples = 100;
        track->addClip(clip);

        QVector<LibraryEntry> emptyLibrary;
        QVERIFY(SessionIO::saveSession(path, session, emptyLibrary));

        // Strip the new keys out of the saved JSON to emulate a pre-upgrade file.
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        QJsonObject root = doc.object();
        QJsonArray tracks = root["tracks"].toArray();
        QJsonObject trackObj = tracks[0].toObject();
        QJsonArray clips = trackObj["clips"].toArray();
        QJsonObject clipObj = clips[0].toObject();
        clipObj.remove("gain");
        clipObj.remove("fadeInSamples");
        clipObj.remove("fadeOutSamples");
        clipObj.remove("fadeInCurve");
        clipObj.remove("fadeOutCurve");
        clips[0] = clipObj;
        trackObj["clips"] = clips;
        tracks[0] = trackObj;
        root["tracks"] = tracks;
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(root).toJson());
        file.close();

        Session loaded;
        QVector<LibraryEntry> loadedLibrary;
        QVERIFY(SessionIO::loadSession(path, loaded, loadedLibrary));

        auto loadedTrack = loaded.tracks.front();
        auto loadedClip = loadedTrack->clipsSnapshot()->front();
        QCOMPARE(loadedClip->gain, 1.0f);
        QCOMPARE(loadedClip->fadeInSamples, int64_t(0));
        QCOMPARE(loadedClip->fadeOutSamples, int64_t(0));
        QCOMPARE(loadedClip->fadeInCurve, FadeCurve::Linear);
    }

    void roundTripsTrackEffectChain() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.filePath("session.rsdproj");

        Session session;
        auto track = session.addTrack("T0");

        auto eq = std::make_shared<EqEffect>();
        eq->prepare(48000.0);
        eq->midGainDb.store(5.0f);
        eq->midFreqHz.store(2500.0f);
        eq->bypassed.store(true);
        track->addEffect(eq);

        auto comp = std::make_shared<CompressorEffect>();
        comp->prepare(48000.0);
        comp->thresholdDb.store(-12.0f);
        comp->ratio.store(3.5f);
        track->addEffect(comp);

        QVector<LibraryEntry> emptyLibrary;
        QVERIFY(SessionIO::saveSession(path, session, emptyLibrary));

        Session loaded;
        QVector<LibraryEntry> loadedLibrary;
        QVERIFY(SessionIO::loadSession(path, loaded, loadedLibrary));

        auto effects = loaded.tracks.front()->effectsSnapshot();
        QCOMPARE(effects->size(), size_t(2));

        auto loadedEq = std::dynamic_pointer_cast<EqEffect>(effects->at(0));
        QVERIFY(loadedEq);
        QVERIFY(qFuzzyCompare(loadedEq->midGainDb.load(), 5.0f));
        QVERIFY(qFuzzyCompare(loadedEq->midFreqHz.load(), 2500.0f));
        QVERIFY(loadedEq->bypassed.load());
        QCOMPARE(loadedEq->id, eq->id);

        auto loadedComp = std::dynamic_pointer_cast<CompressorEffect>(effects->at(1));
        QVERIFY(loadedComp);
        QVERIFY(qFuzzyCompare(loadedComp->thresholdDb.load(), -12.0f));
        QVERIFY(qFuzzyCompare(loadedComp->ratio.load(), 3.5f));
    }

    void roundTripsBusRoutingAndMasterBus() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.filePath("session.rsdproj");

        Session session;
        auto bus = session.addTrack("Reverb Bus");
        bus->kind = TrackKind::Bus;
        bus->volume.store(0.8f);

        auto track = session.addTrack("T0");
        track->setSendBusId(bus->id);
        track->sendLevel.store(0.4f);

        session.masterBus.volume.store(0.9f);
        auto masterEq = std::make_shared<EqEffect>();
        masterEq->prepare(48000.0);
        masterEq->midGainDb.store(2.0f);
        session.masterBus.addEffect(masterEq);

        QVector<LibraryEntry> emptyLibrary;
        QVERIFY(SessionIO::saveSession(path, session, emptyLibrary));

        Session loaded;
        QVector<LibraryEntry> loadedLibrary;
        QVERIFY(SessionIO::loadSession(path, loaded, loadedLibrary));

        QCOMPARE(loaded.tracks.size(), size_t(2));
        auto loadedBus = loaded.tracks.at(0);
        auto loadedTrack = loaded.tracks.at(1);

        QCOMPARE(loadedBus->kind, TrackKind::Bus);
        QVERIFY(qFuzzyCompare(loadedBus->volume.load(), 0.8f));

        QCOMPARE(loadedTrack->kind, TrackKind::Audio);
        QCOMPARE(loadedTrack->sendBusId(), bus->id);
        QVERIFY(qFuzzyCompare(loadedTrack->sendLevel.load(), 0.4f));

        QVERIFY(qFuzzyCompare(loaded.masterBus.volume.load(), 0.9f));
        auto masterEffects = loaded.masterBus.effectsSnapshot();
        QCOMPARE(masterEffects->size(), size_t(1));
        auto loadedMasterEq = std::dynamic_pointer_cast<EqEffect>(masterEffects->at(0));
        QVERIFY(loadedMasterEq);
        QVERIFY(qFuzzyCompare(loadedMasterEq->midGainDb.load(), 2.0f));
    }
};

QTEST_APPLESS_MAIN(TestSessionIO)
#include "test_SessionIO.moc"
