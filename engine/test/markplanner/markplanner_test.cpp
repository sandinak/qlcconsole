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
#include "../common/stdprelude.h"
#define protected public
#define private public
#include "mastertimer.h"
#include "universe.h"
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
#include "show.h"
#include "track.h"
#include "showfunction.h"
#include "markeffect.h"
#include "markplanner.h"
#include "cuelookahead.h"
#include "functionparent.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "cueoutput.h"
#include "qlcpalette.h"
#include "wledfixture.h"
#include "chaseraction.h"
#include "inputoutputmap.h"
#include "qlccapability.h"
#include "shutterautoopen.h"
#include "outputlayer.h"
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

void MarkPlanner_Test::openingAShowWithoutMoveInBlackStartsItOff()
{
    ProgrammerController *pc = m_doc->programmer();
    QVERIFY(pc != nullptr);

    // The show's whole Engine, saved as File > Save does.
    auto saveShow = [this]() {
        QByteArray xml;
        QXmlStreamWriter w(&xml);
        w.writeStartDocument();
        m_doc->saveXML(&w);
        w.writeEndDocument();
        return xml;
    };
    // ...and opened as File > Open does: the old show cleared, the new loaded.
    auto openShow = [this](const QByteArray &xml) {
        m_doc->clearContents();
        QXmlStreamReader r(xml);
        while (!r.atEnd() && !(r.isStartElement() && r.name() == QStringLiteral("Engine")))
            r.readNext();
        return m_doc->loadXML(r, true);
    };

    // The movers' made-up definitions can't be found on a load: not this test's.
    QVERIFY(m_doc->deleteFixture(m_fixtureA));
    QVERIFY(m_doc->deleteFixture(m_fixtureB));
    pc->setAutoMoveInBlack(true);
    pc->setMoveInBlackDarkGapMs(1500);
    const QByteArray withMib = saveShow();
    QVERIFY(withMib.contains("<MoveInBlack"));

    // An older show: the same, with no <MoveInBlack> at all.
    QByteArray without = withMib;
    const int at = without.indexOf("<MoveInBlack");
    const int end = without.indexOf("/>", at);
    QVERIFY(at >= 0 && end > at);
    without.remove(at, end + 2 - at);
    QVERIFY(!without.contains("MoveInBlack"));

    // Opening the first brings its Move in black back...
    QVERIFY(openShow(withMib));
    QVERIFY(pc->isAutoMoveInBlack());
    QCOMPARE(pc->moveInBlackDarkGapMs(), 1500);

    // ...and opening the second does not keep it.
    QSignalSpy spy(pc, &ProgrammerController::autoMoveInBlackChanged);
    QVERIFY(openShow(without));
    QVERIFY(!pc->isAutoMoveInBlack());
    QCOMPARE(pc->moveInBlackDarkGapMs(), MarkPlanner::defaultDarkGapMs());
    QVERIFY(spy.count() >= 1);
    QCOMPARE(spy.last().at(0).toBool(), false);
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

void MarkPlanner_Test::aCueTooSoonIsNotMovedInBlack()
{
    startChaserLightingFixtureA(m_doc, m_fixtureA);
    // How far off the next cue is, as the planner sees it (the chaser is not
    // ticked again, so this holds still).
    const QList<CueLookahead::UpcomingCue> cues = CueLookahead::upcoming(m_doc, 60000);
    QCOMPARE(cues.count(), 1);
    const int fire = cues.first().fireInMs;
    QVERIFY2(fire > 1 && fire < INT_MAX, qPrintable(QString("fires in %1 ms").arg(fire)));

    // A lead time longer than that: too soon, nothing moves in black.
    MarkPlanner planner(m_doc, m_mark);
    auto plan = [&planner]() { QMetaObject::invokeMethod(&planner, "evaluate"); };
    planner.setDarkGapMs(fire + 1);
    planner.setEnabled(true);
    plan();
    QVERIFY(!m_mark->isMarked(m_fixtureA));
    QCOMPARE(planner.plannedCount(), 0);

    // Exactly the lead time away is enough.
    planner.setDarkGapMs(fire);
    plan();
    QVERIFY(m_mark->isMarked(m_fixtureA));
    QCOMPARE(planner.plannedCount(), 1);

    // The cue now inside the lead time: what was pre-set stays pre-set.
    planner.setDarkGapMs(fire + 1);
    plan();
    QVERIFY(m_mark->isMarked(m_fixtureA));
    QCOMPARE(planner.plannedCount(), 1);
    planner.setEnabled(false);                         // releases its pre-set
    QVERIFY(!m_mark->isMarked(m_fixtureA));

    // Well clear of the lead time: moved in black.
    MarkPlanner early(m_doc, m_mark);
    early.setDarkGapMs(fire / 2);
    early.setEnabled(true);
    QMetaObject::invokeMethod(&early, "evaluate");
    QVERIFY(m_mark->isMarked(m_fixtureA));
    early.setEnabled(false);
}

void MarkPlanner_Test::theLookaheadAsksARunningShowFirst()
{
    QVERIFY(CueLookahead::upcoming(nullptr).isEmpty());
    QVERIFY(CueLookahead::upcoming(m_doc).isEmpty());          // nothing running

    // A standalone chaser running too: the Show still answers
    startChaserLightingFixtureA(m_doc, m_fixtureA);
    QCOMPARE(CueLookahead::upcoming(m_doc).count(), 1);

    QList<Scene *> sc;
    for (int i = 0; i < 5; i++)
    {
        sc << new Scene(m_doc);
        m_doc->addFunction(sc.last());
    }
    Show *show = new Show(m_doc);
    m_doc->addFunction(show);
    Track *t1 = new Track(Function::invalidId(), show);
    QVERIFY(show->addTrack(t1));
    ShowFunction *now = t1->createShowFunction(sc[0]->id());   // already playing
    now->setStartTime(0);
    now->setDuration(600000);
    ShowFunction *later = t1->createShowFunction(sc[2]->id());
    later->setStartTime(5000);
    later->setDuration(1000);
    Track *t2 = new Track(Function::invalidId(), show);
    QVERIFY(show->addTrack(t2));
    ShowFunction *next = t2->createShowFunction(sc[1]->id());  // the nearest
    next->setStartTime(3000);
    next->setDuration(1000);
    ShowFunction *nobody = t2->createShowFunction(Function::invalidId());
    nobody->setStartTime(2000);
    Track *muted = new Track(Function::invalidId(), show);
    QVERIFY(show->addTrack(muted));
    muted->setMute(true);
    ShowFunction *silent = muted->createShowFunction(sc[3]->id());
    silent->setStartTime(1000);
    silent->setDuration(1000);

    MasterTimer timer(m_doc);
    show->start(&timer, FunctionParent::master());
    timer.timerTick();
    QVERIFY(show->isRunning());
    const quint32 pos = show->runningTime();
    QVERIFY(pos < 1000);

    QList<CueLookahead::UpcomingCue> cues = CueLookahead::upcoming(m_doc, 60000);
    QCOMPARE(cues.count(), 1);
    QCOMPARE(cues.first().functionId, sc[1]->id());
    QCOMPARE(cues.first().fireInMs, int(3000 - pos));
    QCOMPARE(cues.first().chaserId, Function::invalidId());   // a timeline item
    QCOMPARE(cues.first().step, -1);

    // Exactly at the horizon is in; one short of it is out
    QCOMPARE(CueLookahead::upcoming(m_doc, int(3000 - pos)).count(), 1);
    QVERIFY(CueLookahead::upcoming(m_doc, int(3000 - pos) - 1).isEmpty());

    // Nothing after the playhead: nothing ahead (the chaser isn't asked)
    next->setStartTime(0);
    later->setStartTime(0);
    QVERIFY(CueLookahead::upcoming(m_doc, 60000).isEmpty());

    show->stop(FunctionParent::master());
    timer.timerTick();
}

void MarkPlanner_Test::theLookaheadOfAChaser()
{
    Scene *a = new Scene(m_doc);
    Scene *b = new Scene(m_doc);
    m_doc->addFunction(a);
    m_doc->addFunction(b);
    Chaser *chaser = new Chaser(m_doc);
    chaser->setDuration(0);                    // every step holds for GO
    chaser->addStep(a->id());
    chaser->addStep(b->id());
    m_doc->addFunction(chaser);

    MasterTimer timer(m_doc);
    chaser->start(&timer, FunctionParent::master());
    timer.timerTick();
    QCOMPARE(chaser->currentStepIndex(), 0);

    // Held: ahead whatever the horizon, with its chaser and step
    QList<CueLookahead::UpcomingCue> cues = CueLookahead::upcoming(m_doc, 10);
    QCOMPARE(cues.count(), 1);
    QCOMPARE(cues.first().fireInMs, INT_MAX);
    QCOMPARE(cues.first().functionId, b->id());
    QCOMPARE(cues.first().chaserId, chaser->id());
    QCOMPARE(cues.first().step, 1);

    // Timed: the time left in the step, within the horizon only
    chaser->setDuration(MasterTimer::tick() * 100);
    cues = CueLookahead::upcoming(m_doc, 60000);
    QCOMPARE(cues.count(), 1);
    const int fire = cues.first().fireInMs;
    QVERIFY2(fire > 0 && fire <= int(MasterTimer::tick() * 100), qPrintable(QString::number(fire)));
    QVERIFY(CueLookahead::upcoming(m_doc, fire - 1).isEmpty());

    // A chaser the Show Tree's clock steps: not a standalone one
    chaser->setShowClocked(true);
    QVERIFY(CueLookahead::upcoming(m_doc, 60000).isEmpty());
    chaser->setShowClocked(false);

    chaser->stop(FunctionParent::master());
    timer.timerTick();
    QVERIFY(!chaser->isRunning());
    QVERIFY(CueLookahead::upcoming(m_doc, 60000).isEmpty());
}

QTEST_GUILESS_MAIN(MarkPlanner_Test)

void MarkPlanner_Test::aDarkWledIsMovedInBlackToItsNextLook()
{
    // A WLED lantern in Effect mode on its own API, dark now.
    QLCFixtureDef *def = new QLCFixtureDef();
    QCOMPARE(def->loadXML(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/WLED/WLED-Lantern.qxf"),
             QFile::NoError);
    Fixture *staff = new Fixture(m_doc);
    staff->setFixtureDefinition(def, def->mode("Effect (15ch)"));
    staff->setUniverse(0);
    staff->setAddress(100);
    staff->setTransport(Fixture::WledTransport);
    QVERIFY(m_doc->addFixture(staff));
    QVERIFY(WledFixture::runsEffects(staff));
    const quint32 sid = staff->id();
    const int effectCh = WledFixture::channel(staff, "Effect");
    const int speedCh = WledFixture::channel(staff, "Effect Speed");
    const int paletteCh = WledFixture::channel(staff, "Palette");
    const int redCh = WledFixture::channel(staff, "Red");
    const quint32 master = staff->masterIntensityChannel();
    QVERIFY(effectCh >= 0 && redCh >= 0 && master != QLCChannel::invalid());

    // The spell: Fireworks Starburst at speed 200, palette 6, in red.
    QLCPalette *burst = new QLCPalette(QLCPalette::Effect, m_doc);
    burst->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/wled-fireworks-starburst.js");
    burst->setWledEffect("Fireworks Starburst");
    burst->setEffectParamValue("speed", 200);
    burst->setEffectParamValue("palette", 6);
    m_doc->addPalette(burst);
    QLCPalette *red = new QLCPalette(QLCPalette::Color, m_doc);
    red->setValue(QColor(255, 0, 0));
    m_doc->addPalette(red);
    Scene *spell = new Scene(m_doc);
    spell->setName("Spell");
    spell->addFixture(sid);
    spell->addPalette(burst->id());
    spell->addPalette(red->id());
    m_doc->addFunction(spell);

    // The cue's own peek has no idea: the settings are the look's.
    QVERIFY(!CueOutput::computeCues(m_doc, spell->id()).value(sid).lit);
    // As the look plays them: lit, and its effect, speed, palette and red.
    WledLookCache looks;
    looks.beginPass();
    const CueOutput::FixtureCue fc = CueOutput::computeCues(m_doc, spell->id(), &looks).value(sid);
    looks.endPass();
    QVERIFY(fc.lit);
    QCOMPARE(int(fc.intensity), 255);
    QCOMPARE(int(fc.nonIntensity.value(quint32(effectCh))),
             WledFixture::effectValue(staff, "Fireworks Starburst"));
    QCOMPARE(int(fc.nonIntensity.value(quint32(speedCh))), 200);
    QCOMPARE(int(fc.nonIntensity.value(quint32(paletteCh))), 6);
    QCOMPARE(int(fc.nonIntensity.value(quint32(redCh))), 255);
    QVERIFY(!fc.nonIntensity.contains(master));
    QCOMPARE(looks.size(), 1);
    // A pass that does not ask for it lets it go.
    looks.beginPass();
    looks.endPass();
    QCOMPARE(looks.size(), 0);

    // A colour cue on it is Solid: its effect at rest, not the last one.
    Scene *plain = new Scene(m_doc);
    plain->addFixture(sid);
    plain->setValue(sid, master, 255);
    plain->setValue(sid, quint32(redCh), 255);
    m_doc->addFunction(plain);
    QCOMPARE(int(CueOutput::computeCues(m_doc, plain->id(), &looks).value(sid)
                     .nonIntensity.value(quint32(effectCh), 99)), 0);

    // A cue list: now (the staff dark), then the spell.
    Scene *now = new Scene(m_doc);
    m_doc->addFunction(now);
    Chaser *chaser = new Chaser(m_doc);
    chaser->setDuration(MasterTimer::tick() * 10);
    chaser->addStep(now->id());
    chaser->addStep(spell->id());
    m_doc->addFunction(chaser);
    MasterTimer timer(m_doc);
    chaser->start(&timer, FunctionParent::master());
    timer.timerTick();
    QCOMPARE(chaser->computeNextStep(chaser->currentStepIndex()), 1);

    MarkPlanner planner(m_doc, m_mark);
    planner.setDarkGapMs(0);
    // Off: nothing pre-set.
    QMetaObject::invokeMethod(&planner, "evaluate");
    QVERIFY(!m_mark->isMarked(sid));
    planner.setEnabled(true);
    QVERIFY(m_mark->isMarked(sid));
    const QHash<quint32, uchar> held = m_mark->markedValues(sid);
    QCOMPARE(int(held.value(quint32(effectCh))), WledFixture::effectValue(staff, "Fireworks Starburst"));
    QCOMPARE(int(held.value(quint32(redCh))), 255);
    QVERIFY(!held.contains(master));            // dark: the GO brings the brightness
    // Not into this cue (released when the planner is switched off).
    planner.setEnabled(false);
    QVERIFY(!m_mark->isMarked(sid));
}

/* A cue list: now (A dark, its pan left at 10 by an earlier cue), next
   (lights A, pan 128), later (lights only B). A is moved in black to pan 128
   for next -- then the operator jumps from now straight to later. Next never
   plays, so A's pan goes back to 10: a later cue that only brings A's level
   up would otherwise show next's look, which never played (UX-70). */
void MarkPlanner_Test::aPresetIsPutBackWhenItsCueIsJumpedPast()
{
    Scene *now = new Scene(m_doc);
    now->setName("Now");
    now->setValue(m_fixtureB, 1, 100);
    m_doc->addFunction(now);
    Scene *next = new Scene(m_doc);
    next->setName("Next");
    next->setValue(m_fixtureA, 1, 200);     // Dimmer
    next->setValue(m_fixtureA, 0, 128);     // Pan
    m_doc->addFunction(next);
    Scene *later = new Scene(m_doc);
    later->setName("Later");
    later->setValue(m_fixtureB, 1, 255);
    m_doc->addFunction(later);
    Chaser *chaser = new Chaser(m_doc);
    chaser->setDuration(MasterTimer::tick() * 10);
    chaser->addStep(now->id());
    chaser->addStep(next->id());
    chaser->addStep(later->id());
    m_doc->addFunction(chaser);

    Universe *uni = m_doc->inputOutputMap()->universes().first();
    uni->setChannelDefaultValue(0, 10);     // A's pan, as an earlier cue left it
    auto panA = [uni]() { uni->processFaders(); return int(uni->preGMValue(0)); };
    QCOMPARE(panA(), 10);

    MasterTimer timer(m_doc);
    chaser->start(&timer, FunctionParent::master());
    timer.timerTick();
    QCOMPARE(chaser->currentStepIndex(), 0);

    MarkPlanner planner(m_doc, m_mark);
    planner.setDarkGapMs(0);
    planner.setEnabled(true);
    QVERIFY(m_mark->isMarked(m_fixtureA));
    QCOMPARE(panA(), 128);                  // moved in black for next

    ChaserAction jump;
    jump.m_action = ChaserSetStepIndex;
    jump.m_stepIndex = 2;
    jump.m_masterIntensity = 1.0;
    jump.m_stepIntensity = 1.0;
    jump.m_fadeMode = Chaser::FromFunction;
    chaser->setAction(jump);
    for (int i = 0; i < 4; ++i)
        timer.timerTick();
    QCOMPARE(chaser->currentStepIndex(), 2);

    QMetaObject::invokeMethod(&planner, "evaluate");
    QVERIFY(!m_mark->isMarked(m_fixtureA));
    QCOMPARE(planner.plannedCount(), 0);
    QCOMPARE(panA(), 10);                   // as before the pre-set
    QCOMPARE(panA(), 10);                   // and it stays

    chaser->stop(FunctionParent::master());
    for (int i = 0; i < 4; ++i)
        timer.timerTick();
}

/*****************************************************************************
 * A shutter is a dimmer too
 *****************************************************************************/

quint32 MarkPlanner_Test::addShutterFixture(quint32 address, bool dimmer, bool shutter)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel(QString("Shutter %1%2").arg(dimmer).arg(shutter));
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    auto add = [&](const QString &name, QLCChannel::Group grp) {
        QLCChannel *ch = new QLCChannel();
        ch->setName(name);
        ch->setGroup(grp);
        def->addChannel(ch);
        mode->insertChannel(ch, def->channels().size() - 1);
        return ch;
    };
    add("Pan", QLCChannel::Pan);
    if (dimmer)
        add("Dimmer", QLCChannel::Intensity);
    QLCChannel *sh = add(shutter ? "Shutter" : "Strobe", QLCChannel::Shutter);
    if (shutter)
    {
        QLCCapability *closed = new QLCCapability(0, 7, "Closed");
        closed->setPreset(QLCCapability::ShutterClose);
        QLCCapability *open = new QLCCapability(8, 255, "Open");
        open->setPreset(QLCCapability::ShutterOpen);
        sh->addCapability(closed);
        sh->addCapability(open);
    }
    else
    {
        sh->addCapability(new QLCCapability(0, 9, "No function"));
        sh->addCapability(new QLCCapability(10, 255, "Strobe (slow to fast)"));
    }
    mode->cacheHeads();
    def->addMode(mode);

    Fixture *fx = new Fixture(m_doc);
    fx->setFixtureDefinition(def, mode);
    fx->setUniverse(0);
    fx->setAddress(address);
    m_doc->addFixture(fx);
    return fx->id();
}

namespace
{
/** A cue list on its first (empty) step, @p next its next. */
void startChaserInto(Doc *doc, Scene *next)
{
    Scene *current = new Scene(doc);
    current->setName("Current");
    doc->addFunction(current);
    doc->addFunction(next);
    Chaser *chaser = new Chaser(doc);
    chaser->setDuration(MasterTimer::tick() * 10);
    chaser->addStep(current->id());
    chaser->addStep(next->id());
    doc->addFunction(chaser);
    MasterTimer timer(doc);
    chaser->start(&timer, FunctionParent::master());
    timer.timerTick();
    QCOMPARE(chaser->computeNextStep(0), 1);
}

/** A running cue's levels (over the mark, as a cue is): a dimmer is reset
 *  every frame, so it is given one by a fader, not a resting value. */
struct Stage
{
    explicit Stage(Doc *d) : doc(d), layer(d, Universe::Auto, 0, QStringLiteral("Cue")) {}
    void cue(int address, int value)
    {
        levels[address] = value;
        QList<OutputLayer::Level> l;
        for (auto it = levels.constBegin(); it != levels.constEnd(); ++it)
            l.append({ Fixture::invalidId(), quint32(it.key()), uchar(it.value()) });
        layer.set(l);
        doc->inputOutputMap()->universes().first()->processFaders();
    }
    Doc *doc;
    OutputLayer layer;
    QMap<int, int> levels;
};

/** What is on stage now: @p values as the channels' resting values. */
void setLive(Doc *doc, const QList<QPair<int, int> > &values)
{
    Universe *uni = doc->inputOutputMap()->universes().first();
    for (const QPair<int, int> &v : values)
        uni->setChannelDefaultValue(ushort(v.first), uchar(v.second));
    uni->processFaders();
}

int live(Doc *doc, int address)
{
    Universe *uni = doc->inputOutputMap()->universes().first();
    uni->processFaders();
    return int(uni->postGMValue(address));
}

/** One DMX frame of the mark's hand-off, and its queued release. */
void handOff(Doc *doc, MarkEffect *mark)
{
    Universe *uni = doc->inputOutputMap()->universes().first();
    uni->processFaders();
    mark->writeDMX(nullptr, doc->inputOutputMap()->universes());
    QCoreApplication::processEvents();
}
}

void MarkPlanner_Test::closedIsTheDefinitionsCloseCapability()
{
    const quint32 sid = addShutterFixture(100, true, true);
    const quint32 strobeId = addShutterFixture(110, false, false);
    Fixture *spot = m_doc->fixture(sid);
    Fixture *strobe = m_doc->fixture(strobeId);
    QCOMPARE(ShutterAutoOpen::closingShutters(spot), QVector<quint32>({ 2 }));
    QVERIFY(ShutterAutoOpen::closingShutters(strobe).isEmpty());
    QVERIFY(ShutterAutoOpen::closesAt(spot->channel(2), 0));
    QVERIFY(ShutterAutoOpen::closesAt(spot->channel(2), 7));
    QVERIFY(!ShutterAutoOpen::closesAt(spot->channel(2), 8));
    QVERIFY(!ShutterAutoOpen::closesAt(strobe->channel(1), 0));   // "No function"

    // Untagged definitions: by the capability's name, not an effect's.
    QLCCapability named(0, 10, "Shutter closed");
    QVERIFY(ShutterAutoOpen::isCloseCapability(&named));
    QLCCapability blackout(0, 10, "Blackout");
    QVERIFY(ShutterAutoOpen::isCloseCapability(&blackout));
    QLCCapability effect(0, 10, "Slow open - fast close");
    QVERIFY(!ShutterAutoOpen::isCloseCapability(&effect));
    QLCCapability strobing(0, 10, "Strobe");
    QVERIFY(!ShutterAutoOpen::isCloseCapability(&strobing));
}

void MarkPlanner_Test::aDarkDimmerPresetsTheShutterToo()
{
    const quint32 sid = addShutterFixture(100, true, true);   // pan 100, dimmer 101, shutter 102
    Scene *next = new Scene(m_doc);
    next->setValue(sid, 0, 128);
    next->setValue(sid, 1, 200);
    next->setValue(sid, 2, 200);        // open
    startChaserInto(m_doc, next);
    QVERIFY(CueOutput::computeCues(m_doc, next->id()).value(sid).lit);

    // The dimmer down, the shutter open: dark by the dimmer.
    setLive(m_doc, { { 100, 10 }, { 101, 0 }, { 102, 200 } });
    MarkPlanner planner(m_doc, m_mark);
    planner.setDarkGapMs(0);
    planner.setEnabled(true);
    QVERIFY(m_mark->isMarked(sid));
    QHash<quint32, uchar> held = m_mark->markedValues(sid);
    QCOMPARE(int(held.value(0)), 128);
    QCOMPARE(int(held.value(2)), 200);   // the dimmer keeps it dark: shutter pre-set
    QVERIFY(!held.contains(1));
    planner.setEnabled(false);
    QVERIFY(!m_mark->isMarked(sid));

    // The dimmer down, the shutter closed too: the same.
    setLive(m_doc, { { 100, 10 }, { 101, 0 }, { 102, 0 } });
    planner.setEnabled(true);
    QVERIFY(m_mark->isMarked(sid));
    QCOMPARE(int(m_mark->markedValues(sid).value(2)), 200);
    QCOMPARE(live(m_doc, 102), 200);       // open behind a dark dimmer
    // Not lit while the dimmer stays down.
    handOff(m_doc, m_mark);
    QVERIFY(m_mark->isMarked(sid));
    // The cue brings the level: handed off.
    Stage stage(m_doc);
    stage.cue(101, 200);
    handOff(m_doc, m_mark);
    QVERIFY(!m_mark->isMarked(sid));
    planner.setEnabled(false);
}

void MarkPlanner_Test::aClosedShutterIsDarkAndStaysClosed()
{
    const quint32 sid = addShutterFixture(100, true, true);
    Scene *next = new Scene(m_doc);
    next->setValue(sid, 0, 128);
    next->setValue(sid, 1, 200);
    next->setValue(sid, 2, 200);
    startChaserInto(m_doc, next);

    /* The dimmer up behind a closed shutter: dark. The cue on now closes
       it (a shutter nothing sets would be auto-opened by the level). */
    setLive(m_doc, { { 100, 10 } });
    Stage stage(m_doc);
    stage.cue(102, 0);
    stage.cue(101, 255);
    QCOMPARE(live(m_doc, 101), 255);
    MarkPlanner planner(m_doc, m_mark);
    planner.setDarkGapMs(0);
    planner.setEnabled(true);
    QVERIFY(m_mark->isMarked(sid));
    QCOMPARE(planner.plannedCount(), 1);
    const QHash<quint32, uchar> held = m_mark->markedValues(sid);
    QCOMPARE(int(held.value(0)), 128);   // moved...
    QVERIFY(!held.contains(2));          // ...but the shutter is not opened
    QVERIFY(!held.contains(1));
    QCOMPARE(live(m_doc, 100), 128);
    QCOMPARE(live(m_doc, 102), 0);

    // Held on a re-plan the same way, and not handed off while closed
    // although the dimmer is up.
    QMetaObject::invokeMethod(&planner, "evaluate");
    QVERIFY(!m_mark->markedValues(sid).contains(2));
    handOff(m_doc, m_mark);
    QVERIFY(m_mark->isMarked(sid));
    QCOMPARE(live(m_doc, 102), 0);

    // The cue opens the shutter: handed off.
    stage.cue(102, 200);
    handOff(m_doc, m_mark);
    QVERIFY(!m_mark->isMarked(sid));
    planner.setEnabled(false);
}

void MarkPlanner_Test::aShutterOnlyFixtureIsMovedInBlack()
{
    const quint32 sid = addShutterFixture(100, false, true);   // pan 100, shutter 101
    QCOMPARE(m_doc->fixture(sid)->masterIntensityChannel(), QLCChannel::invalid());
    Scene *next = new Scene(m_doc);
    next->setValue(sid, 0, 128);
    next->setValue(sid, 1, 200);         // opens
    startChaserInto(m_doc, next);
    QVERIFY(CueOutput::computeCues(m_doc, next->id()).value(sid).lit);

    setLive(m_doc, { { 100, 10 }, { 101, 0 } });
    MarkPlanner planner(m_doc, m_mark);
    planner.setDarkGapMs(0);
    planner.setEnabled(true);
    QVERIFY(m_mark->isMarked(sid));
    QCOMPARE(int(m_mark->markedValues(sid).value(0)), 128);
    QVERIFY(!m_mark->markedValues(sid).contains(1));
    QCOMPARE(live(m_doc, 100), 128);
    QCOMPARE(live(m_doc, 101), 0);
    QVERIFY(planner.dangleFixtures().isEmpty());

    handOff(m_doc, m_mark);
    QVERIFY(m_mark->isMarked(sid));
    // Revealed: the shutter opens.
    setLive(m_doc, { { 101, 200 } });
    handOff(m_doc, m_mark);
    QVERIFY(!m_mark->isMarked(sid));
    // Open: not dark, so not pre-set again.
    QMetaObject::invokeMethod(&planner, "evaluate");
    QVERIFY(!m_mark->isMarked(sid));
    planner.setEnabled(false);
}

void MarkPlanner_Test::aFixtureWithNeitherIsSkipped()
{
    const quint32 sid = addShutterFixture(100, false, false);   // pan, strobe
    Scene *next = new Scene(m_doc);
    next->setValue(sid, 0, 128);
    next->setValue(sid, 1, 0);
    startChaserInto(m_doc, next);
    QVERIFY(!CueOutput::computeCues(m_doc, next->id()).value(sid).lit);

    setLive(m_doc, { { 100, 10 }, { 101, 0 } });
    MarkPlanner planner(m_doc, m_mark);
    planner.setDarkGapMs(0);
    planner.setEnabled(true);
    QVERIFY(!m_mark->isMarked(sid));
    QCOMPARE(planner.plannedCount(), 0);

    // Marked by hand, it is never handed off (nothing says lit) -- and
    // nothing upcoming lights it, so it dangles.
    m_mark->markFixture(sid, { { 0, 128 } });
    handOff(m_doc, m_mark);
    QVERIFY(m_mark->isMarked(sid));
    QCOMPARE(planner.dangleFixtures(), QList<quint32>({ sid }));
    m_mark->unmarkFixture(sid);
    planner.setEnabled(false);
}

void MarkPlanner_Test::anAutoOpenedShutterIsNotDark()
{
    const quint32 sid = addShutterFixture(100, true, true);
    Scene *next = new Scene(m_doc);
    next->setValue(sid, 0, 128);
    next->setValue(sid, 1, 200);
    next->setValue(sid, 2, 200);
    startChaserInto(m_doc, next);
    QVERIFY(m_doc->shutterAutoOpen() != nullptr && m_doc->shutterAutoOpen()->isEnabled());
    m_doc->shutterAutoOpen()->rebuild();

    // The shutter rests closed; the cue on now gives the dimmer a level.
    setLive(m_doc, { { 100, 10 }, { 102, 0 } });
    Stage stage(m_doc);
    stage.cue(101, 255);
    QVERIFY(live(m_doc, 102) >= 8);         // auto-opened: lit on stage
    MarkPlanner planner(m_doc, m_mark);
    planner.setDarkGapMs(0);
    planner.setEnabled(true);
    QVERIFY(!m_mark->isMarked(sid));
    planner.setEnabled(false);
}

void MarkPlanner_Test::aManualMarkIsLeftAlone()
{
    const quint32 sid = addShutterFixture(100, true, true);
    Scene *next = new Scene(m_doc);
    next->setValue(sid, 0, 128);
    next->setValue(sid, 1, 200);
    next->setValue(sid, 2, 200);
    startChaserInto(m_doc, next);
    setLive(m_doc, { { 100, 10 }, { 102, 0 } });
    Stage stage(m_doc);
    stage.cue(101, 255);

    const QHash<quint32, uchar> mine = { { 0, 64 } };
    m_mark->markFixture(sid, mine);
    MarkPlanner planner(m_doc, m_mark);
    planner.setDarkGapMs(0);
    planner.setEnabled(true);
    QCOMPARE(planner.plannedCount(), 0);
    QCOMPARE(m_mark->markedValues(sid), mine);
    planner.setEnabled(false);              // not the planner's to release
    QVERIFY(m_mark->isMarked(sid));
    m_mark->unmarkFixture(sid);
}

void MarkPlanner_Test::aCueThatClosesTheShutterDoesNotLightIt()
{
    const quint32 sid = addShutterFixture(100, true, true);
    const quint32 bare = addShutterFixture(110, false, true);
    Scene *next = new Scene(m_doc);
    next->setValue(sid, 0, 128);
    next->setValue(sid, 1, 200);
    next->setValue(sid, 2, 0);           // closed
    next->setValue(bare, 0, 128);        // no shutter: not known open
    startChaserInto(m_doc, next);
    const QHash<quint32, CueOutput::FixtureCue> cues = CueOutput::computeCues(m_doc, next->id());
    QVERIFY(!cues.value(sid).lit);
    QVERIFY(!cues.value(bare).lit);

    setLive(m_doc, { { 101, 0 }, { 102, 0 }, { 111, 0 } });
    MarkPlanner planner(m_doc, m_mark);
    planner.setDarkGapMs(0);
    planner.setEnabled(true);
    QVERIFY(!m_mark->isMarked(sid));
    QVERIFY(!m_mark->isMarked(bare));
    m_mark->markFixture(sid, { { 0, 128 } });
    QCOMPARE(planner.dangleFixtures(), QList<quint32>({ sid }));
    m_mark->unmarkFixture(sid);
    planner.setEnabled(false);
}
