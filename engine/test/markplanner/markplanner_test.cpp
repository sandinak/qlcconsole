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

QTEST_GUILESS_MAIN(MarkPlanner_Test)
