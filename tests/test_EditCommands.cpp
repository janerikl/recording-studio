#include <QtTest>

#include "command/CommandStack.h"
#include "command/EditCommands.h"

using namespace rsd;

class TestEditCommands : public QObject {
    Q_OBJECT

private slots:
    void trackClipsCommandUndoRedoRestoresList() {
        auto track = std::make_shared<Track>();
        auto clip = std::make_shared<Clip>();
        clip->name = "A";

        auto before = track->clipsSnapshot();
        track->addClip(clip);
        auto after = track->clipsSnapshot();

        CommandStack stack;
        stack.push(std::make_unique<TrackClipsCommand>(track, before, after));
        QCOMPARE(track->clipsSnapshot()->size(), size_t(1));

        stack.undo();
        QCOMPARE(track->clipsSnapshot()->size(), size_t(0));

        stack.redo();
        QCOMPARE(track->clipsSnapshot()->size(), size_t(1));
        QCOMPARE(track->clipsSnapshot()->front()->name, QString("A"));
    }

    void trackClipsCommandUndoesSplitWithoutArithmeticInversion() {
        auto track = std::make_shared<Track>();
        auto clip = std::make_shared<Clip>();
        clip->lengthSamples = 100;
        track->addClip(clip);

        auto before = track->clipsSnapshot();
        track->splitClip(clip->id, 40);
        auto after = track->clipsSnapshot();
        QCOMPARE(after->size(), size_t(2));

        CommandStack stack;
        stack.push(std::make_unique<TrackClipsCommand>(track, before, after));

        stack.undo();
        QCOMPARE(track->clipsSnapshot()->size(), size_t(1));
        QCOMPARE(track->clipsSnapshot()->front()->id, clip->id);
        QCOMPARE(track->clipsSnapshot()->front()->lengthSamples, int64_t(100));
    }

    void trackStateCommandUndoRedoRestoresScalars() {
        auto track = std::make_shared<Track>();
        TrackState before = TrackState::capture(*track);

        track->volume.store(0.5f);
        track->muted.store(true);
        TrackState after = TrackState::capture(*track);

        CommandStack stack;
        stack.push(std::make_unique<TrackStateCommand>(track, before, after));
        QCOMPARE(track->volume.load(), 0.5f);
        QVERIFY(track->muted.load());

        stack.undo();
        QCOMPARE(track->volume.load(), 1.0f);
        QVERIFY(!track->muted.load());

        stack.redo();
        QCOMPARE(track->volume.load(), 0.5f);
        QVERIFY(track->muted.load());
    }

    void addTrackCommandUndoRedo() {
        Session session;
        auto track = std::make_shared<Track>();
        track->name = "New Track";

        CommandStack stack;
        stack.push(std::make_unique<AddTrackCommand>(&session, track));
        QCOMPARE(session.tracks.size(), size_t(1));

        stack.undo();
        QCOMPARE(session.tracks.size(), size_t(0));

        stack.redo();
        QCOMPARE(session.tracks.size(), size_t(1));
        QCOMPARE(session.tracks.front()->name, QString("New Track"));
    }

    void removeTrackCommandUndoRestoresOriginalIndex() {
        Session session;
        auto t0 = session.addTrack("T0");
        auto t1 = session.addTrack("T1");
        auto t2 = session.addTrack("T2");

        CommandStack stack;
        stack.push(std::make_unique<RemoveTrackCommand>(&session, t1, 1));
        QCOMPARE(session.tracks.size(), size_t(2));
        QCOMPARE(session.tracks[0]->id, t0->id);
        QCOMPARE(session.tracks[1]->id, t2->id);

        stack.undo();
        QCOMPARE(session.tracks.size(), size_t(3));
        QCOMPARE(session.tracks[0]->id, t0->id);
        QCOMPARE(session.tracks[1]->id, t1->id);
        QCOMPARE(session.tracks[2]->id, t2->id);
    }

    void setClipGainCommandUndoRedo() {
        auto track = std::make_shared<Track>();
        auto clip = std::make_shared<Clip>();
        track->addClip(clip);

        CommandStack stack;
        stack.push(std::make_unique<SetClipGainCommand>(track, clip->id, 1.0f, 0.5f));
        QCOMPARE(track->clipsSnapshot()->front()->gain, 0.5f);

        stack.undo();
        QCOMPARE(track->clipsSnapshot()->front()->gain, 1.0f);

        stack.redo();
        QCOMPARE(track->clipsSnapshot()->front()->gain, 0.5f);
    }

    void setClipFadeCommandUndoRedo() {
        auto track = std::make_shared<Track>();
        auto clip = std::make_shared<Clip>();
        clip->lengthSamples = 100;
        track->addClip(clip);

        ClipFadeState before{0, 0, FadeCurve::Linear, FadeCurve::Linear};
        ClipFadeState after{10, 20, FadeCurve::EqualPower, FadeCurve::Linear};

        CommandStack stack;
        stack.push(std::make_unique<SetClipFadeCommand>(track, clip->id, before, after));
        auto edited = track->clipsSnapshot()->front();
        QCOMPARE(edited->fadeInSamples, int64_t(10));
        QCOMPARE(edited->fadeOutSamples, int64_t(20));
        QCOMPARE(edited->fadeInCurve, FadeCurve::EqualPower);

        stack.undo();
        auto reverted = track->clipsSnapshot()->front();
        QCOMPARE(reverted->fadeInSamples, int64_t(0));
        QCOMPARE(reverted->fadeOutSamples, int64_t(0));
    }

    void compositeCommandUndoesInReverseOrder() {
        auto track = std::make_shared<Track>();
        std::vector<QString> log;

        struct LoggingCommand : Command {
            LoggingCommand(std::vector<QString>& log, QString tag) : log(log), tag(std::move(tag)) {}
            void redo() override { log.push_back("redo:" + tag); }
            void undo() override { log.push_back("undo:" + tag); }
            QString text() const override { return tag; }
            std::vector<QString>& log;
            QString tag;
        };

        std::vector<std::unique_ptr<Command>> subCommands;
        subCommands.push_back(std::make_unique<LoggingCommand>(log, "first"));
        subCommands.push_back(std::make_unique<LoggingCommand>(log, "second"));

        CommandStack stack;
        stack.push(std::make_unique<CompositeCommand>(std::move(subCommands)));
        QCOMPARE(log, (std::vector<QString>{"redo:first", "redo:second"}));

        log.clear();
        stack.undo();
        // Undo must run in reverse so composite mutations (e.g. a cross-track
        // clip move) unwind symmetrically with how they were applied.
        QCOMPARE(log, (std::vector<QString>{"undo:second", "undo:first"}));
    }
};

QTEST_APPLESS_MAIN(TestEditCommands)
#include "test_EditCommands.moc"
