/*
  Q Light Controller Plus - Unit test
  markplanner_test.cpp

  Verifies MarkPlanner::dangleFixtures(): a marked (positioned-but-dark)
  fixture that the upcoming cue is about to light is fine; one the upcoming
  cue never touches is "dangling" -- a pre-set nothing is going to reveal.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtTest>
#include <QSignalSpy>

// MasterTimer::timerTick() is private (only MasterTimerPrivate is a friend) --
// the same trick chaser_test.cpp uses to drive it synchronously in tests
// instead of waiting on the real ticking thread.
#define protected public
#define private public
#include "mastertimer.h"
#undef protected
#undef private

#include "markplanner_test.h"
#include "doc.h"
#include "fixture.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "scene.h"
#include "chaser.h"
#include "markeffect.h"
#include "markplanner.h"
#include "functionparent.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "showtreemodel.h"
#include <QBuffer>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>

// A minimal mover: Pan + Dimmer (master intensity). Mirrors cueoutput_test's
// fixture -- MarkPlanner's lit/dark check goes through the same CueOutput
// path, which needs a real fixture mode to resolve masterIntensityChannel().
static QLCFixtureDef *makeMoverDef()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("MarkPlannerMover");
    QLCFixtureMode *mode = new QLCFixtureMode(def);

    auto add = [&](const QString &name, QLCChannel::Group grp) {
        QLCChannel *ch = new QLCChannel();
        ch->setName(name); ch->setGroup(grp);
        def->addChannel(ch); mode->insertChannel(ch, def->channels().size() - 1);
    };
    add("Pan",    QLCChannel::Pan);
    add("Dimmer", QLCChannel::Intensity);   // → master

    mode->cacheHeads();
    def->addMode(mode);
    return def;
}

static Fixture *addMover(Doc *doc, quint32 address)
{
    QLCFixtureDef *def = makeMoverDef();
    QLCFixtureMode *mode = def->modes().first();

    Fixture *fx = new Fixture(doc);
    fx->setFixtureDefinition(def, mode);
    fx->setUniverse(0);
    fx->setAddress(address);
    doc->addFixture(fx);
    return fx;
}

void MarkPlanner_Test::init()
{
    m_doc = new Doc(this);

    Fixture *a = addMover(m_doc, 0);
    Fixture *b = addMover(m_doc, 10);
    m_fixtureA = a->id();
    m_fixtureB = b->id();
    QCOMPARE(a->masterIntensityChannel(), quint32(1));   // Dimmer is master

    m_mark = new MarkEffect(m_doc);
}

void MarkPlanner_Test::cleanup()
{
    delete m_doc;   // MarkEffect is doc-parented; goes with it
    m_doc = nullptr;
    m_mark = nullptr;
}

// Starts a 2-step Chaser (step 0 = filler, step 1 = lights m_fixtureA's
// Dimmer+Pan) and advances it one tick so it reports as currently on step 0
// with step 1 next -- exactly what CueLookahead::upcoming() looks for.
static void startChaserLightingFixtureA(Doc *doc, quint32 fixtureA)
{
    Scene *current = new Scene(doc);
    current->setName("Current");
    doc->addFunction(current);

    Scene *next = new Scene(doc);
    next->setName("Next");
    next->setValue(fixtureA, 1, 200);   // Dimmer (intensity)
    next->setValue(fixtureA, 0, 128);   // Pan
    doc->addFunction(next);

    Chaser *chaser = new Chaser(doc);
    chaser->setDuration(MasterTimer::tick() * 10);
    chaser->addStep(current->id());
    chaser->addStep(next->id());
    doc->addFunction(chaser);

    // A local, never-started MasterTimer: preRun()/write() run synchronously
    // via timerTick(), no background thread involved (mirrors chaser_test's
    // writeHTP pattern). Its destructor is a no-op here since start() was
    // never called, so it's safe to let it go out of scope -- the Chaser's
    // own isRunning()/step state persists independently of the driving timer.
    MasterTimer timer(doc);
    chaser->start(&timer, FunctionParent::master());
    timer.timerTick();

    QCOMPARE(chaser->currentStepIndex(), 0);
    QCOMPARE(chaser->computeNextStep(0), 1);
}

void MarkPlanner_Test::markedFixtureInUpcomingCueIsNotDangling()
{
    startChaserLightingFixtureA(m_doc, m_fixtureA);

    m_mark->markFixture(m_fixtureA, { {0, 128} });   // Pan held

    MarkPlanner planner(m_doc, m_mark);
    QCOMPARE(planner.dangleFixtures(), QList<quint32>());
}

void MarkPlanner_Test::markedFixtureNotInUpcomingCueIsDangling()
{
    startChaserLightingFixtureA(m_doc, m_fixtureA);

    m_mark->markFixture(m_fixtureB, { {0, 128} });   // B is never lit by "Next"

    MarkPlanner planner(m_doc, m_mark);
    QCOMPARE(planner.dangleFixtures(), QList<quint32>({ m_fixtureB }));
}

void MarkPlanner_Test::noMarksIsNeverDangling()
{
    startChaserLightingFixtureA(m_doc, m_fixtureA);

    MarkPlanner planner(m_doc, m_mark);
    QVERIFY(planner.dangleFixtures().isEmpty());
}

void MarkPlanner_Test::noUpcomingCueMeansMarkedIsDangling()
{
    // No Chaser/Show running at all -> CueLookahead::upcoming() is empty ->
    // nothing is ever going to reveal a mark, so it dangles.
    m_mark->markFixture(m_fixtureA, { {0, 128} });

    MarkPlanner planner(m_doc, m_mark);
    QCOMPARE(planner.dangleFixtures(), QList<quint32>({ m_fixtureA }));
}

void MarkPlanner_Test::signalFiresOnlyOnChange()
{
    startChaserLightingFixtureA(m_doc, m_fixtureA);

    MarkPlanner planner(m_doc, m_mark);
    QSignalSpy spy(&planner, &MarkPlanner::dangleFixturesChanged);

    // evaluate() is the QTimer-connected slot; invoke it directly (bypassing
    // the 300ms timer) via the meta-object system, the same way real ticks
    // drive it -- private slots are still meta-callable.
    auto tick = [&planner]() { QMetaObject::invokeMethod(&planner, "evaluate"); };

    // Nothing marked yet: still empty -> no change -> no emission.
    tick();
    QCOMPARE(spy.count(), 0);

    // Mark B (dangling, since only A is in the upcoming cue) -> one emission.
    m_mark->markFixture(m_fixtureB, { {0, 128} });
    tick();
    QCOMPARE(spy.count(), 1);
    spy.clear();

    // Re-evaluate with nothing changed -> no new emission.
    tick();
    QCOMPARE(spy.count(), 0);

    // Unmark B -> back to empty -> one more emission.
    m_mark->unmarkFixture(m_fixtureB);
    tick();
    QCOMPARE(spy.count(), 1);
}

void MarkPlanner_Test::aDarkFixtureIsPresetButAStagedOneIsNot()
{
    startChaserLightingFixtureA(m_doc, m_fixtureA);

    MarkPlanner planner(m_doc, m_mark);
    planner.setDarkGapMs(0);                           // any lead time will do
    planner.setEnabled(true);
    QMetaObject::invokeMethod(&planner, "evaluate");
    QVERIFY(m_mark->isMarked(m_fixtureA));             // dark now, lit next: pre-set
    planner.setEnabled(false);                         // releases its own pre-set
    QVERIFY(!m_mark->isMarked(m_fixtureA));

    // Same cue, but A goes back to the inventory: it has no address, so
    // there is nothing to pre-set.
    QVERIFY(m_doc->unpatchFixture(m_fixtureA));
    MarkPlanner staged(m_doc, m_mark);
    staged.setDarkGapMs(0);
    staged.setEnabled(true);
    QMetaObject::invokeMethod(&staged, "evaluate");
    QVERIFY(!m_mark->isMarked(m_fixtureA));
}

void MarkPlanner_Test::theShowsMoveInBlackSavesLoadsAndResets()
{
    ProgrammerController *pc = m_doc->programmer();
    QVERIFY(pc != nullptr);
    QVERIFY(!pc->isAutoMoveInBlack());
    QCOMPARE(pc->moveInBlackDarkGapMs(), MarkPlanner::defaultDarkGapMs());

    QSignalSpy spy(pc, &ProgrammerController::autoMoveInBlackChanged);
    m_doc->resetModified();
    pc->setAutoMoveInBlack(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(m_doc->isModified());                      // it's the show's
    pc->setAutoMoveInBlack(true);                      // no change, no noise
    QCOMPARE(spy.count(), 1);

    m_doc->resetModified();
    pc->setMoveInBlackDarkGapMs(1500);
    QCOMPARE(spy.count(), 2);                          // the lead time is told too
    QCOMPARE(spy.last().at(0).toBool(), true);
    QVERIFY(m_doc->isModified());
    pc->setMoveInBlackDarkGapMs(1500);
    QCOMPARE(spy.count(), 2);

    QByteArray xml;
    {
        QBuffer buf(&xml);
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        QVERIFY(pc->saveMoveInBlackXML(&w));
    }
    QVERIFY(xml.contains("Auto=\"1\""));
    QVERIFY(xml.contains("DarkGapMs=\"1500\""));

    // A new show: off, the default lead time, and the views are told.
    m_doc->clearContents();
    QVERIFY(!pc->isAutoMoveInBlack());
    QCOMPARE(pc->moveInBlackDarkGapMs(), MarkPlanner::defaultDarkGapMs());
    QCOMPARE(spy.last().at(0).toBool(), false);

    // Loading the show brings its own back.
    QXmlStreamReader r(xml);
    while (!r.atEnd() && !r.isStartElement())
        r.readNext();
    QVERIFY(pc->loadMoveInBlackXML(r));
    QVERIFY(pc->isAutoMoveInBlack());
    QCOMPARE(pc->moveInBlackDarkGapMs(), 1500);
    QCOMPARE(spy.last().at(0).toBool(), true);
    pc->setAutoMoveInBlack(false);
}

void MarkPlanner_Test::thePresetCountFollowsThePlanner()
{
    ProgrammerController *pc = m_doc->programmer();
    startChaserLightingFixtureA(m_doc, m_fixtureA);
    QSignalSpy spy(pc, &ProgrammerController::moveInBlackPresetsChanged);
    pc->setMoveInBlackDarkGapMs(0);                    // any lead time will do
    QCOMPARE(pc->moveInBlackPresetCount(), 0);
    pc->setAutoMoveInBlack(true);                      // evaluates at once
    QCOMPARE(pc->moveInBlackPresetCount(), 1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(0).toInt(), 1);
    pc->setAutoMoveInBlack(false);                     // releases its pre-set
    QCOMPARE(pc->moveInBlackPresetCount(), 0);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.last().at(0).toInt(), 0);
}

void MarkPlanner_Test::perListAndPerCueMoveInBlack()
{
    using namespace ShowTree;
    // A tree list: cue One (nothing), cue Two plays a scene lighting A.
    Scene *next = new Scene(m_doc);
    next->setName("Next");
    next->setValue(m_fixtureA, 1, 200);     // Dimmer
    next->setValue(m_fixtureA, 0, 128);     // Pan
    m_doc->addFunction(next);

    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 one = st->addCue(listId, -1, "One");
    const quint32 two = st->addCue(listId, 0, "Two");
    Target fn;
    fn.function = next->id();
    QVERIFY(st->addLayer(two, fn) != invalidId());
    Q_UNUSED(one);

    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    QVERIFY(chaser != nullptr);
    MasterTimer timer(m_doc);
    chaser->start(&timer, FunctionParent::master());
    timer.timerTick();
    QCOMPARE(chaser->currentStepIndex(), 0);

    MarkPlanner planner(m_doc, m_mark);
    planner.setDarkGapMs(0);
    auto plan = [&]() { QMetaObject::invokeMethod(&planner, "evaluate"); };

    // The show's off, the list follows it: nothing.
    plan();
    QVERIFY(!m_mark->isMarked(m_fixtureA));

    // The list On: pre-positioned although the show's is off.
    QVERIFY(st->setListMoveInBlack(listId, MoveInBlack::On));
    QVERIFY(chaser->isRunning());
    plan();
    QVERIFY(m_mark->isMarked(m_fixtureA));
    QCOMPARE(planner.plannedCount(), 1);

    // Two says don't: released, and nothing more.
    QVERIFY(st->setCuesNoMoveInBlack({ two }, true));
    plan();
    QVERIFY(!m_mark->isMarked(m_fixtureA));
    QCOMPARE(planner.plannedCount(), 0);
    QVERIFY(st->setCuesNoMoveInBlack({ two }, false));

    // The show's on, the list Off: nothing.
    planner.setEnabled(true);
    QVERIFY(st->setListMoveInBlack(listId, MoveInBlack::Off));
    plan();
    QVERIFY(!m_mark->isMarked(m_fixtureA));

    // ... and back to the show's: pre-positioned.
    QVERIFY(st->setListMoveInBlack(listId, MoveInBlack::Show));
    plan();
    QVERIFY(m_mark->isMarked(m_fixtureA));

    // The show's off again, the list follows: released.
    planner.setEnabled(false);
    plan();
    QVERIFY(!m_mark->isMarked(m_fixtureA));
    chaser->stop(FunctionParent::master());
}

QTEST_GUILESS_MAIN(MarkPlanner_Test)
