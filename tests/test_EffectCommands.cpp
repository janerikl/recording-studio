#include <QtTest>

#include "audio/Effects.h"
#include "command/CommandStack.h"
#include "command/EditCommands.h"
#include "model/Track.h"

using namespace rsd;

class TestEffectCommands : public QObject {
    Q_OBJECT

private slots:
    void addEffectCommandIsUndoableAndRedoable() {
        auto track = std::make_shared<Track>();
        CommandStack stack;

        auto before = track->effectsSnapshot();
        auto eq = std::make_shared<EqEffect>();
        eq->prepare(48000.0);
        track->addEffect(eq);
        auto after = track->effectsSnapshot();

        stack.push(std::make_unique<EffectChainCommand>(track, before, after, "Add EQ"));
        QCOMPARE(track->effectsSnapshot()->size(), size_t(1));

        stack.undo();
        QCOMPARE(track->effectsSnapshot()->size(), size_t(0));

        stack.redo();
        QCOMPARE(track->effectsSnapshot()->size(), size_t(1));
        QCOMPARE(track->effectsSnapshot()->front()->id, eq->id);
    }

    void removeEffectCommandIsUndoableAndRedoable() {
        auto track = std::make_shared<Track>();
        auto eq = std::make_shared<EqEffect>();
        eq->prepare(48000.0);
        track->addEffect(eq);

        CommandStack stack;
        auto before = track->effectsSnapshot();
        track->removeEffect(eq->id);
        auto after = track->effectsSnapshot();

        stack.push(std::make_unique<EffectChainCommand>(track, before, after, "Remove EQ"));
        QCOMPARE(track->effectsSnapshot()->size(), size_t(0));

        stack.undo();
        QCOMPARE(track->effectsSnapshot()->size(), size_t(1));

        stack.redo();
        QCOMPARE(track->effectsSnapshot()->size(), size_t(0));
    }

    void reorderEffectCommandIsUndoableAndRedoable() {
        auto track = std::make_shared<Track>();
        auto eq = std::make_shared<EqEffect>();
        eq->prepare(48000.0);
        auto comp = std::make_shared<CompressorEffect>();
        comp->prepare(48000.0);
        track->addEffect(eq);
        track->addEffect(comp);

        CommandStack stack;
        auto before = track->effectsSnapshot();
        track->moveEffect(comp->id, 0); // move compressor to the front
        auto after = track->effectsSnapshot();

        stack.push(std::make_unique<EffectChainCommand>(track, before, after, "Reorder"));
        QCOMPARE(track->effectsSnapshot()->at(0)->id, comp->id);
        QCOMPARE(track->effectsSnapshot()->at(1)->id, eq->id);

        stack.undo();
        QCOMPARE(track->effectsSnapshot()->at(0)->id, eq->id);
        QCOMPARE(track->effectsSnapshot()->at(1)->id, comp->id);

        stack.redo();
        QCOMPARE(track->effectsSnapshot()->at(0)->id, comp->id);
    }

    void setEffectFloatParamCommandIsUndoableAndRedoable() {
        auto eq = std::make_shared<EqEffect>();
        eq->prepare(48000.0);
        eq->midGainDb.store(0.0f);

        CommandStack stack;
        std::weak_ptr<EqEffect> weakEq = eq;
        auto setter = [weakEq](float v) {
            if (auto e = weakEq.lock()) e->midGainDb.store(v);
        };
        stack.push(std::make_unique<SetEffectParamCommand<float>>(setter, 0.0f, 6.0f, "Set Mid Gain"));
        QCOMPARE(eq->midGainDb.load(), 6.0f);

        stack.undo();
        QCOMPARE(eq->midGainDb.load(), 0.0f);

        stack.redo();
        QCOMPARE(eq->midGainDb.load(), 6.0f);
    }

    void setEffectBypassCommandIsUndoableAndRedoable() {
        auto eq = std::make_shared<EqEffect>();
        eq->prepare(48000.0);

        CommandStack stack;
        std::weak_ptr<EqEffect> weakEq = eq;
        auto setter = [weakEq](bool v) {
            if (auto e = weakEq.lock()) e->bypassed.store(v);
        };
        stack.push(std::make_unique<SetEffectParamCommand<bool>>(setter, false, true, "Bypass EQ"));
        QVERIFY(eq->bypassed.load());

        stack.undo();
        QVERIFY(!eq->bypassed.load());
    }
};

QTEST_APPLESS_MAIN(TestEffectCommands)
#include "test_EffectCommands.moc"
