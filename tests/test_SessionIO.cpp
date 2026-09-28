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

    void roundTripsMarkers() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.filePath("session.rsdproj");

        Session session;
        session.markers[1] = 48000;
        session.markers[9] = 960000;

        QVector<LibraryEntry> emptyLibrary;
        QVERIFY(SessionIO::saveSession(path, session, emptyLibrary));

        Session loaded;
        QVector<LibraryEntry> loadedLibrary;
        QVERIFY(SessionIO::loadSession(path, loaded, loadedLibrary));

        QCOMPARE(loaded.markers.size(), size_t(2));
        QCOMPARE(loaded.markers.at(1), int64_t(48000));
        QCOMPARE(loaded.markers.at(9), int64_t(960000));
    }

    void roundTripsBpmAndMetronome() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.filePath("session.rsdproj");

        Session session;
        session.bpm = 90.0;
        session.metronomeEnabled = true;

        QVector<LibraryEntry> emptyLibrary;
        QVERIFY(SessionIO::saveSession(path, session, emptyLibrary));

        Session loaded;
        QVector<LibraryEntry> loadedLibrary;
        QVERIFY(SessionIO::loadSession(path, loaded, loadedLibrary));

        QCOMPARE(loaded.bpm, 90.0);
        QCOMPARE(loaded.metronomeEnabled, true);
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

    void roundTripsLibraryEntryPersistentItemId() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.filePath("session.rsdproj");

        Session session;

        auto buffer = std::make_shared<AudioBuffer>();
        buffer->channels = 1;
        buffer->sampleRate = 48000;
        buffer->samples.assign(4800, 0.2f);

        QVector<LibraryEntry> library;
        library.append({"Snare", buffer, "fixed-item-id"});
        QVERIFY(SessionIO::saveSession(path, session, library));

        Session loaded;
        QVector<LibraryEntry> loadedLibrary;
        QVERIFY(SessionIO::loadSession(path, loaded, loadedLibrary));

        QCOMPARE(loadedLibrary.size(), 1);
        QCOMPARE(loadedLibrary[0].name, QString("Snare"));
        QCOMPARE(loadedLibrary[0].itemId, QString("fixed-item-id"));
    }

    void libraryEntryWithoutExplicitItemId_getsOneAssignedOnSave() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.filePath("session.rsdproj");

        Session session;
        auto buffer = std::make_shared<AudioBuffer>();
        buffer->channels = 1;
        buffer->sampleRate = 48000;
        buffer->samples.assign(4800, 0.2f);

        QVector<LibraryEntry> library;
        library.append({"Kick", buffer}); // itemId left blank
        QVERIFY(SessionIO::saveSession(path, session, library));

        Session loaded;
        QVector<LibraryEntry> loadedLibrary;
        QVERIFY(SessionIO::loadSession(path, loaded, loadedLibrary));

        QCOMPARE(loadedLibrary.size(), 1);
        QVERIFY(!loadedLibrary[0].itemId.isEmpty());
    }

    void roundTripsInstrumentTrackMidiNotesAndSynthParams() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.filePath("session.rsdproj");

        Session session;
        auto track = session.addTrack("Melody");
        track->kind = TrackKind::Instrument;
        track->synthParams.instrumentProgram.store(10); // Music Box
        track->synthParams.isDrumKit.store(false);

        auto note1 = std::make_shared<MidiNote>();
        note1->pitch = 60;
        note1->velocity = 0.7f;
        note1->startSample = 0;
        note1->lengthSamples = 48000;
        track->addMidiNote(note1);

        auto note2 = std::make_shared<MidiNote>();
        note2->pitch = 64;
        note2->velocity = 0.5f;
        note2->startSample = 48000;
        note2->lengthSamples = 24000;
        track->addMidiNote(note2);

        QVector<LibraryEntry> emptyLibrary;
        QVERIFY(SessionIO::saveSession(path, session, emptyLibrary));

        Session loaded;
        QVector<LibraryEntry> loadedLibrary;
        QVERIFY(SessionIO::loadSession(path, loaded, loadedLibrary));

        QCOMPARE(loaded.tracks.size(), size_t(1));
        auto loadedTrack = loaded.tracks.front();
        QCOMPARE(loadedTrack->kind, TrackKind::Instrument);
        QCOMPARE(loadedTrack->synthParams.instrumentProgram.load(), 10);
        QCOMPARE(loadedTrack->synthParams.isDrumKit.load(), false);

        auto notes = loadedTrack->midiClipsSnapshot();
        QCOMPARE(notes->size(), size_t(2));

        auto loadedNote1 = notes->at(0);
        QCOMPARE(loadedNote1->pitch, 60);
        QVERIFY(qFuzzyCompare(loadedNote1->velocity, 0.7f));
        QCOMPARE(loadedNote1->startSample, int64_t(0));
        QCOMPARE(loadedNote1->lengthSamples, int64_t(48000));

        auto loadedNote2 = notes->at(1);
        QCOMPARE(loadedNote2->pitch, 64);
        QVERIFY(qFuzzyCompare(loadedNote2->velocity, 0.5f));
        QCOMPARE(loadedNote2->startSample, int64_t(48000));
        QCOMPARE(loadedNote2->lengthSamples, int64_t(24000));
    }

    void roundTripsAllAutomationLaneTargets() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.filePath("session.rsdproj");

        Session session;
        auto track = session.addTrack("Violin");
        track->kind = TrackKind::Instrument;
        struct Expected {
            AutomationTarget target;
            std::vector<AutomationPoint> points;
        };
        const std::vector<Expected> expected = {
            {AutomationTarget::Volume, {{0, 1.0f}, {48000, 0.5f}}},
            {AutomationTarget::Pan, {{0, -0.25f}}},
            {AutomationTarget::Expression, {{0, 0.3f}, {12000, 0.8f}, {5000000000LL, 0.6f}}},
            {AutomationTarget::Vibrato, {{9600, 0.0f}, {24000, 0.55f}}},
        };
        for (const auto& e : expected) {
            track->replaceAutomationLane(std::make_shared<AutomationLane>(AutomationLane{e.target, e.points}));
        }

        QVector<LibraryEntry> emptyLibrary;
        QVERIFY(SessionIO::saveSession(path, session, emptyLibrary));

        Session loaded;
        QVector<LibraryEntry> loadedLibrary;
        QVERIFY(SessionIO::loadSession(path, loaded, loadedLibrary));

        auto lanes = loaded.tracks.front()->automationLanesSnapshot();
        QCOMPARE(lanes->size(), expected.size());
        for (const auto& e : expected) {
            auto it = std::find_if(lanes->begin(), lanes->end(),
                                   [&](const auto& lane) { return lane->target == e.target; });
            QVERIFY(it != lanes->end());
            QCOMPARE((*it)->points.size(), e.points.size());
            for (size_t i = 0; i < e.points.size(); ++i) {
                QCOMPARE((*it)->points[i].sample, e.points[i].sample);
                QVERIFY(qFuzzyCompare((*it)->points[i].value, e.points[i].value) ||
                        qFuzzyIsNull((*it)->points[i].value - e.points[i].value));
            }
        }
    }

    void loadsOldFileWithOnlyVolumeAndPanLanes() {
        // A project written before expression/vibrato existed.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.filePath("old.rsdproj");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({
            "sampleRate": 48000, "channels": 2, "bpm": 120,
            "tracks": [{
                "id": "{6f1c1f7e-0000-4000-8000-000000000001}", "name": "Old", "kind": "instrument",
                "volume": 1.0, "pan": 0.0,
                "automationLanes": [
                    {"target": "volume", "points": [{"sample": "0", "value": 0.7}]},
                    {"target": "pan", "points": [{"sample": "480", "value": -0.5}]}
                ],
                "clips": []
            }]
        })");
        file.close();

        Session loaded;
        QVector<LibraryEntry> loadedLibrary;
        QVERIFY(SessionIO::loadSession(path, loaded, loadedLibrary));
        QCOMPARE(loaded.tracks.size(), size_t(1));
        auto lanes = loaded.tracks.front()->automationLanesSnapshot();
        QCOMPARE(lanes->size(), size_t(2));
        QCOMPARE(lanes->at(0)->target, AutomationTarget::Volume);
        QCOMPARE(lanes->at(1)->target, AutomationTarget::Pan);
        QCOMPARE(lanes->at(1)->points.front().sample, int64_t(480));
    }

    void roundTripsLibraryFolderTreeAndItemMembership() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.filePath("session.rsdproj");

        Session session;
        auto parent = session.libraryFolders.addFolder("Drums");
        auto child = session.libraryFolders.addFolder("Kicks", parent.get());
        session.libraryFolders.moveItem("loop:/samples/kick.wav", child.get());

        auto buffer = std::make_shared<AudioBuffer>();
        buffer->channels = 1;
        buffer->sampleRate = 48000;
        buffer->samples.assign(4800, 0.1f);
        QVector<LibraryEntry> library;
        library.append({"Bass Hit", buffer, "bass-hit-id"});
        session.libraryFolders.moveItem("pm:bass-hit-id", parent.get());

        QVERIFY(SessionIO::saveSession(path, session, library));

        Session loaded;
        QVector<LibraryEntry> loadedLibrary;
        QVERIFY(SessionIO::loadSession(path, loaded, loadedLibrary));

        QCOMPARE(loaded.libraryFolders.roots.size(), 1);
        auto loadedParent = loaded.libraryFolders.roots[0];
        QCOMPARE(loadedParent->name, QString("Drums"));
        QCOMPARE(loadedParent->itemRefs.size(), 1);
        QCOMPARE(loadedParent->itemRefs[0], QString("pm:bass-hit-id"));

        QCOMPARE(loadedParent->children.size(), 1);
        auto loadedChild = loadedParent->children[0];
        QCOMPARE(loadedChild->name, QString("Kicks"));
        QCOMPARE(loadedChild->itemRefs.size(), 1);
        QCOMPARE(loadedChild->itemRefs[0], QString("loop:/samples/kick.wav"));
    }
};

QTEST_APPLESS_MAIN(TestSessionIO)
#include "test_SessionIO.moc"
