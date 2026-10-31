/*
  Q Light Controller Plus - Unit test
  effectinstance_test.cpp

  Verifies that an Effect look drives ONLY its explicitly-assigned fixtures.
  Regression guard for the removed "no targets -> run on the whole rig"
  fallback, which could fire an always-on effect (e.g. Confetti) across an
  entire — possibly power-constrained — rig before any fixtures were chosen.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtTest>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QBuffer>
#include <QCryptographicHash>
#include <QImage>
#include <functional>
#if defined(Q_OS_UNIX)
#include <time.h>
#endif

#include "effectinstance_test.h"
#define private public                  // MasterTimer::timerTick(), and the instance's insides
#define protected public                // Universe::processFaders()
#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "scene.h"
#include "scenetransition.h"
#include "mastertimer.h"
#include "speedmasters.h"
#include "qlcpalette.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "qlcfixturehead.h"
#include "effectinstance.h"
#include "effectscriptrunner.h"
#include "qlccapability.h"
#include "effectscript.h"
#include "effectscriptcache.h"
#include "wledfixture.h"
#include "matriximage.h"
#include "universe.h"
#include "inputoutputmap.h"
#include "genericfader.h"
#include "fadechannel.h"
#undef protected
#undef private
#include "monitorproperties.h"
#include "righeads.h"
#include "qlcphysical.h"
#include "stagetarget.h"
#include "truss.h"
#include "scenery.h"
#include "aimsolver.h"
#include <QtMath>
#include <cmath>
#include <QVector2D>

void EffectInstance_Test::init()
{
    m_doc = new Doc(this);
    m_fixtureIds.clear();

    // Three plain fixtures, all placed in ONE group — the exact set the old
    // fallback would have blasted when a look had no targets.
    for (int i = 0; i < 3; i++)
    {
        Fixture *fx = new Fixture(m_doc);
        fx->setName(QString("Fix%1").arg(i));
        fx->setUniverse(0);
        fx->setAddress(i * 4);
        fx->setChannels(4);
        m_doc->addFixture(fx);
        m_fixtureIds << fx->id();
    }
    m_group = new FixtureGroup(m_doc);
    for (quint32 id : m_fixtureIds)
        m_group->assignFixture(id);
    m_doc->addFixtureGroup(m_group);

    // Scriptless Effect palette: with no script there is no fixtureType filter,
    // so effectiveFixtureIds() exercises the raw targeting logic directly.
    m_palette = new QLCPalette(QLCPalette::Effect, m_doc);
    m_doc->addPalette(m_palette);
}

void EffectInstance_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void EffectInstance_Test::emptyLookDrivesNothing()
{
    Scene *scene = new Scene(m_doc);
    scene->setName("EmptyLook");
    m_doc->addFunction(scene);

    EffectInstance inst(m_doc, scene->id(), m_palette->id());

    // Precondition: the fixtures really exist and are grouped, so an empty
    // result below is the fallback being gone — not an empty rig.
    QCOMPARE(m_group->fixtureList().size(), 3);

    QVERIFY2(inst.effectiveFixtureIds().isEmpty(),
             "an Effect look with no targets must drive zero fixtures");
}

void EffectInstance_Test::groupTargetDrivesItsFixtures()
{
    Scene *scene = new Scene(m_doc);
    scene->setName("GroupLook");
    m_doc->addFunction(scene);
    scene->addFixtureGroup(m_group->id());

    EffectInstance inst(m_doc, scene->id(), m_palette->id());
    QCOMPARE(inst.effectiveFixtureIds().size(), 3);
}

/* "_beat" was the time since THIS effect started, modulo a beat: two effects
   started a third of a beat apart stayed a third of a beat apart for ever,
   and a tap never brought them together. Now both read the one beat clock. */
void EffectInstance_Test::effectsStartedApartAreOnTheSameBeat()
{
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixtureGroup(m_group->id());
    MasterTimer *mt = m_doc->masterTimer();
    mt->setBpm(120);                                // half a second a beat

    EffectInstance first(m_doc, scene->id(), m_palette->id());
    QTest::qWait(170);                              // a third of a beat
    EffectInstance second(m_doc, scene->id(), m_palette->id());

    const QJSValue a = first.buildInputsObject();
    const QJSValue b = second.buildInputsObject();
    const double beatA = a.property("_beat").toNumber(), beatB = b.property("_beat").toNumber();
    double gap = qAbs(beatA - beatB);
    gap = qMin(gap, 1.0 - gap);                     // across the wrap
    QVERIFY2(gap < 0.02, qPrintable(QString("_beat %1 vs %2").arg(beatA).arg(beatB)));
    QCOMPARE(a.property("_bpm").toNumber(), 120.0);
    QVERIFY(qAbs(a.property("_beatPos").toNumber() - b.property("_beatPos").toNumber()) < 0.02);

    // A tap puts the beat now, for both.
    mt->tap();
    const double after = first.buildInputsObject().property("_beat").toNumber();
    QVERIFY2(after < 0.03 || after > 0.97, qPrintable(QString::number(after)));
}

/* On a speed master an effect's _time counts the master's beats (so a speed of
   1 is a cycle a beat), follows its tempo and stops when it's paused; the
   look's master is used unless the effect names its own. */
void EffectInstance_Test::anEffectOnASpeedMasterCountsItsBeats()
{
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixtureGroup(m_group->id());
    SpeedMasters *sm = m_doc->speedMasters();
    sm->setBpm(0, 600);
    sm->setBpm(1, 60);

    // A real (if empty) script, so the instance is running -- a scriptless
    // one never starts its clock.
    QTemporaryDir dir;
    QFile js(dir.filePath("idle.js"));
    QVERIFY(js.open(QIODevice::WriteOnly));
    js.write("(function() { var e = {}; e.apiVersion = 1; e.name = 'Idle';"
             " e.tick = function(fixtures) { return []; }; return e; })()");
    js.close();
    m_palette->setScriptPath(js.fileName());

    EffectInstance inst(m_doc, scene->id(), m_palette->id());
    QVERIFY(inst.isValid());
    auto time = [&]() { return inst.buildInputsObject().property("_time").toNumber(); };
    /* The effect's time is the master's beat position. Read the master just
       before and just after it, so it is bracketed however long the machine
       stalls in between (CI's runners do: a fixed tolerance of a few
       milliseconds flaked). */
    auto onMaster = [&](int master) {
        const double before = sm->beatPosition(master);
        const double t = time();
        const double after = sm->beatPosition(master);
        return t >= before - 1e-6 && t <= after + 1e-6;
    };

    // Free: seconds since it started, as ever.
    QVERIFY(time() < 1.0);

    // On its look's master: that master's beats.
    scene->setSpeedMaster(SpeedMasters::Master1 + 1);
    QVERIFY(onMaster(1));
    QCOMPARE(inst.buildInputsObject().property("_bpm").toNumber(), 60.0);

    // Its own choice wins over the look's.
    m_palette->setEffectParamValue("__speedMaster", SpeedMasters::Master1);
    QVERIFY(onMaster(0));
    QCOMPARE(inst.buildInputsObject().property("_bpm").toNumber(), 600.0);

    // Paused: it stands still.
    sm->setPaused(0, true);
    const double frozen = time();
    QTest::qWait(60);
    QCOMPARE(time(), frozen);
    QCOMPARE(inst.buildInputsObject().property("_bpm").toNumber(), 0.0);
    m_palette->setEffectParamValue("__speedMaster", SpeedMasters::Inherit);
}

/* Own speed (Look editor, "Own speed: ×"): off a master, the effect's
   _time runs at its "__ownRate"; changing it never jumps the time. */
void EffectInstance_Test::ownSpeedScalesTheEffectsTime()
{
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixtureGroup(m_group->id());
    QTemporaryDir dir;
    QFile js(dir.filePath("idle.js"));
    QVERIFY(js.open(QIODevice::WriteOnly));
    js.write("(function() { var e = {}; e.apiVersion = 1; e.name = 'Idle';"
             " e.tick = function(fixtures) { return []; }; return e; })()");
    js.close();
    m_palette->setScriptPath(js.fileName());
    m_palette->setEffectParamValue("__speedMaster", SpeedMasters::Free);

    EffectInstance inst(m_doc, scene->id(), m_palette->id());
    QVERIFY(inst.isValid());
    auto time = [&]() { return inst.buildInputsObject().property("_time").toNumber(); };

    // ×4: four seconds of effect a second, bracketed by a wall clock.
    m_palette->setEffectParamValue("__ownRate", 4.0);
    const double t0 = time();
    QElapsedTimer wall;
    wall.start();
    QTest::qWait(120);
    const qint64 waited = wall.elapsed();
    const double t1 = time();
    QVERIFY2(t1 - t0 >= 4.0 * waited / 1000.0 * 0.99, qPrintable(QString::number(t1 - t0)));
    QVERIFY2(t1 - t0 <= 4.0 * (waited + 2000) / 1000.0, qPrintable(QString::number(t1 - t0)));

    // Back to ×1: no jump, it carries on from where it was.
    m_palette->setEffectParamValue("__ownRate", 1.0);
    const double t2 = time();
    QVERIFY(t2 >= t1);
    QVERIFY2(t2 - t1 < 0.5, qPrintable(QString::number(t2 - t1)));

    // On a master the master sets the pace: the rate is ignored.
    m_palette->setEffectParamValue("__ownRate", 8.0);
    m_palette->setEffectParamValue("__speedMaster", SpeedMasters::Master1);
    const double before = m_doc->speedMasters()->beatPosition(0);
    const double onMaster = time();
    QVERIFY(onMaster >= before - 1e-6 && onMaster <= m_doc->speedMasters()->beatPosition(0) + 1e-6);
    m_palette->setEffectParamValue("__speedMaster", SpeedMasters::Inherit);
    m_palette->setEffectParamValue("__ownRate", 1.0);
}

/* "One-shot for ANY effect": a loop effect told to run once, or a number of
   times, stops ticking at the end and holds its last frame or lets go. The
   script alternates its dimmer every tick, so a frozen frame shows. */
void EffectInstance_Test::anyEffectRunsOnceOrNTimesThenHoldsOrReleases()
{
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixtureGroup(m_group->id());
    QTemporaryDir dir;
    QFile js(dir.filePath("blink.js"));
    QVERIFY(js.open(QIODevice::WriteOnly));
    js.write("(function() { var e = {}; e.apiVersion = 1; e.name = 'Blink';"
             " e.tick = function(fixtures, inputs, palettes, params, state) {"
             "   state.n = (state.n || 0) + 1;"
             "   return fixtures.map(function() { return { dimmer: state.n % 2 ? 1.0 : 0.2 }; }); };"
             " return e; })()");
    js.close();
    m_palette->setScriptPath(js.fileName());
    m_palette->setEffectParamValue("__durationMs", 300);   // one run
    MasterTimer timer(m_doc);                              // playing: at full
    scene->start(&timer, FunctionParent::master());
    timer.timerTick();

    auto frame = [](EffectInstance &inst) {
        QList<int> v;
        for (const EffectInstance::DmxWrite &w : inst.dmxWrites())
            v << int(w.value);
        return v;
    };
    auto running = [&](EffectInstance &inst) {
        inst.runTick();
        const QList<int> a = frame(inst);
        inst.runTick();
        return !a.isEmpty() && a != frame(inst);
    };

    // Nothing said: it loops, past any length.
    {
        EffectInstance inst(m_doc, scene->id(), m_palette->id());
        QVERIFY(inst.isValid());
        QVERIFY(running(inst));
        QTest::qWait(400);
        QVERIFY(running(inst));
    }
    // Once, then Release: it lets go.
    m_palette->setEffectParamValue("__runs", 1);
    m_palette->setEffectParamValue("__atEnd", 1);
    {
        EffectInstance inst(m_doc, scene->id(), m_palette->id());
        QVERIFY(running(inst));
        QTest::qWait(400);
        inst.runTick();
        QVERIFY2(inst.dmxWrites().isEmpty(), "a released run still drove the rig");
    }
    // Once, then Hold: the last frame stays, and nothing ticks any more.
    m_palette->setEffectParamValue("__atEnd", 0);
    {
        EffectInstance inst(m_doc, scene->id(), m_palette->id());
        QVERIFY(running(inst));
        QTest::qWait(400);
        inst.runTick();
        const QList<int> held = frame(inst);
        QVERIFY(!held.isEmpty());
        inst.runTick();
        QCOMPARE(frame(inst), held);
    }
    // Three times: still going after one run, done after three.
    m_palette->setEffectParamValue("__runs", 3);
    m_palette->setEffectParamValue("__atEnd", 1);
    {
        EffectInstance inst(m_doc, scene->id(), m_palette->id());
        QTest::qWait(350);
        QVERIFY2(running(inst), "three runs stopped after the first");
        QTest::qWait(700);
        inst.runTick();
        QVERIFY(inst.dmxWrites().isEmpty());
    }
    scene->stop(FunctionParent::master());
    timer.timerTick();
    for (const char *k : { "__runs", "__atEnd", "__durationMs" })
        m_palette->removeEffectParamValue(k);
}

/* A one-shot script (a wand sweep) is given its run as phase 0..1. Left as
   it is it runs once; told to loop, each run goes 0..1 again. */
void EffectInstance_Test::aOneShotScriptCanLoop()
{
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixtureGroup(m_group->id());
    QTemporaryDir dir;
    QFile js(dir.filePath("sweep.js"));
    QVERIFY(js.open(QIODevice::WriteOnly));
    js.write("(function() { var e = {}; e.apiVersion = 1; e.name = 'Sweep';"
             " e.lifecycle = 'oneshot'; e.onFinish = 'hold';"
             " e.tick = function(fixtures, inputs) {"
             "   return fixtures.map(function() { return { dimmer: inputs.phase }; }); };"
             " return e; })()");
    js.close();
    m_palette->setScriptPath(js.fileName());
    m_palette->setEffectParamValue("__durationMs", 300);
    MasterTimer timer(m_doc);
    scene->start(&timer, FunctionParent::master());
    timer.timerTick();

    {
        EffectInstance inst(m_doc, scene->id(), m_palette->id());
        inst.runTick();
        QVERIFY(!inst.dmxWrites().isEmpty());
        QTest::qWait(450);
        inst.runTick();
        QCOMPARE(inst.buildInputsObject().property("phase").toNumber(), 1.0);   // once: over
    }
    m_palette->setEffectParamValue("__runs", 0);                                // Loop
    {
        EffectInstance inst(m_doc, scene->id(), m_palette->id());
        QTest::qWait(450);
        inst.runTick();
        const double phase = inst.buildInputsObject().property("phase").toNumber();
        QVERIFY2(phase < 0.99, qPrintable(QString::number(phase)));             // a second run
        QVERIFY(!inst.dmxWrites().isEmpty());
        QTest::qWait(50);
        inst.runTick();
        QVERIFY(!inst.dmxWrites().isEmpty());                                     // still going
    }
    scene->stop(FunctionParent::master());
    timer.timerTick();
    m_palette->removeEffectParamValue("__runs");
    m_palette->removeEffectParamValue("__durationMs");
}

/* "when selecting an effect .. sometimes it doesn't apply on selection": a
   look with a Fade Out kept its old engine releasing through syncScene, and
   the create after took it back -- the old script played on. */
void EffectInstance_Test::anotherEffectPickedPlaysAtOnceDespiteARelease()
{
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixtureGroup(m_group->id());
    QTemporaryDir dir;
    auto script = [&](const QString &name) {
        QFile js(dir.filePath(name + ".js"));
        if (!js.open(QIODevice::WriteOnly))
            return QString();
        js.write(QString("(function() { var e = {}; e.apiVersion = 1; e.name = '%1';"
                         " e.tick = function(fixtures) { return []; }; return e; })()").arg(name).toUtf8());
        js.close();
        return js.fileName();
    };
    m_palette->setScriptPath(script("First"));
    scene->addPalette(m_palette->id());
    scene->setPaletteFade(m_palette->id(), -1, 2000);   // a release

    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    QVERIFY(runner != nullptr);
    runner->createInstancesForScene(scene->id());
    EffectInstance *first = runner->instanceForPalette(m_palette->id());
    QVERIFY(first != nullptr);
    QCOMPARE(first->m_script.name(), QString("First"));

    m_palette->setScriptPath(script("Second"));
    runner->syncScene(scene->id());
    EffectInstance *now = runner->instanceForPalette(m_palette->id());
    QVERIFY(now != nullptr);
    QCOMPARE(now->m_script.name(), QString("Second"));
    QVERIFY(!now->isFadingOut());
    int forPalette = 0;
    for (EffectInstance *i : runner->m_instances)
        if (i->effectPaletteId() == m_palette->id())
            ++forPalette;
    QCOMPARE(forPalette, 1);
    runner->destroyInstancesForScene(scene->id(), false);
}

void EffectInstance_Test::singleFixtureTargetOnly()
{
    Scene *scene = new Scene(m_doc);
    scene->setName("OneFixLook");
    m_doc->addFunction(scene);
    scene->addFixture(m_fixtureIds.first());

    EffectInstance inst(m_doc, scene->id(), m_palette->id());
    QList<quint32> got = inst.effectiveFixtureIds();
    QCOMPARE(got.size(), 1);
    QCOMPARE(got.first(), m_fixtureIds.first());
}

void EffectInstance_Test::multiHeadYieldsPerHeadCells()
{
    // A 4-head RGB fixture (12 channels) in a 4x1 group.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Pixel4");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    QLCChannel::PrimaryColour pc[3] = { QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue };
    for (int h = 0; h < 4; h++)
        for (int c = 0; c < 3; c++)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("H%1c%2").arg(h).arg(c));
            ch->setGroup(QLCChannel::Intensity);
            ch->setColour(pc[c]);
            def->addChannel(ch);
            mode->insertChannel(ch, h * 3 + c);
        }
    for (int h = 0; h < 4; h++)
    {
        QLCFixtureHead head;
        head.addChannel(h * 3 + 0); head.addChannel(h * 3 + 1); head.addChannel(h * 3 + 2);
        mode->insertHead(-1, head);
    }
    mode->cacheHeads();
    def->addMode(mode);

    Fixture *fx = new Fixture(m_doc);
    fx->setFixtureDefinition(def, mode);
    fx->setUniverse(1); fx->setAddress(0);   // clear of the init() fixtures on uni 0
    m_doc->addFixture(fx);
    QCOMPARE(fx->heads(), 4);

    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setSize(QSize(4, 1));
    grp->assignFixture(fx->id());
    m_doc->addFixtureGroup(grp);

    Scene *scene = new Scene(m_doc);
    scene->setName("PixelLook");
    m_doc->addFunction(scene);
    scene->addFixtureGroup(grp->id());

    EffectInstance inst(m_doc, scene->id(), m_palette->id());
    int cols = 0, rows = 0;
    QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);

    QCOMPARE(cells.size(), 4);   // one cell PER HEAD, not one per fixture
    QCOMPARE(cols, 4);
    QCOMPARE(rows, 1);
    QList<int> heads;
    for (const EffectInstance::EffectCell &c : cells) heads << c.head;
    std::sort(heads.begin(), heads.end());
    QCOMPARE(heads, (QList<int>() << 0 << 1 << 2 << 3));
}

/* Groups on one look are laid out as the rig stands: towers with bars hung
   between them and a step below run tower, bars, tower across, and the step
   sits under them -- not one whole group after the other, each with its own
   idea of "up". To scale: heads in one spot share a cell. */
void EffectInstance_Test::groupsAreLaidAcrossTheStageWhereTheyStand()
{
    QList<quint32> ids = m_fixtureIds;
    for (int i = 0; i < 2; ++i)
    {
        Fixture *extra = new Fixture(m_doc);
        extra->setName(QString("Fix%1").arg(3 + i));
        extra->setUniverse(0);
        extra->setAddress(12 + i * 4);
        extra->setChannels(4);
        m_doc->addFixture(extra);
        ids << extra->id();
    }
    const quint32 t1 = ids.at(0), t2 = ids.at(1), b1 = ids.at(2), b2 = ids.at(3), step = ids.at(4);

    MonitorProperties *mp = m_doc->monitorProperties();
    mp->setFixturePosition(t1, 0, 0, QVector3D(1000, 0, 3000));
    mp->setFixturePosition(t2, 0, 0, QVector3D(5000, 0, 3000));
    mp->setFixturePosition(b1, 0, 0, QVector3D(3000, 0, 3000));   // the bars share
    mp->setFixturePosition(b2, 0, 0, QVector3D(3000, 0, 3000));   // one spot
    mp->setFixturePosition(step, 0, 0, QVector3D(2000, 0, 500));  // low, downstage

    auto oneHeadEach = [this](const QList<quint32> &fids) {
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setSize(QSize(fids.size(), 1));
        for (int i = 0; i < fids.size(); ++i)
            g->assignHead(QLCPoint(i, 0), GroupHead(fids.at(i), 0));
        m_doc->addFixtureGroup(g);
        return g->id();
    };
    Scene *scene = new Scene(m_doc);
    scene->setName("TowersBarsAndAStep");
    m_doc->addFunction(scene);
    scene->addFixtureGroup(oneHeadEach({ t1, t2 }));   // towers first, as in the show
    scene->addFixtureGroup(oneHeadEach({ b1, b2 }));
    scene->addFixtureGroup(oneHeadEach({ step }));

    EffectInstance inst(m_doc, scene->id(), m_palette->id());
    int cols = 0, rows = 0;
    const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);

    QHash<quint32, QPoint> at;
    for (const EffectInstance::EffectCell &c : cells)
        at.insert(c.fixtureId, QPoint(c.col, c.row));
    QCOMPARE(cells.size(), 5);
    // Across: in stage order, to scale.
    QCOMPARE(at.value(t1).x(), 0);
    QVERIFY(at.value(t1).x() < at.value(step).x());
    QVERIFY(at.value(step).x() < at.value(b1).x());
    QCOMPARE(at.value(b1), at.value(b2));                 // one spot, one cell
    QVERIFY(at.value(b1).x() < at.value(t2).x());
    QCOMPARE(at.value(t2).x(), cols - 1);
    QCOMPARE(qAbs(at.value(b1).x() - at.value(t1).x()), qAbs(at.value(t2).x() - at.value(b1).x()));
    // Up: by height, top row first; the step is the bottom row.
    QCOMPARE(at.value(t1).y(), 0);
    QCOMPARE(at.value(b1).y(), 0);
    QCOMPARE(at.value(step).y(), rows - 1);
    QVERIFY(rows > 1);
}

/* A pixel bar's heads are where the rig view paints them: spread end to end
   along its length, in head order (RigHeads::headPositions()). */
void EffectInstance_Test::aBarsHeadsAreAlongItsLength()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Bar4");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    for (int h = 0; h < 4; h++)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("H%1").arg(h));
        ch->setGroup(QLCChannel::Intensity);
        def->addChannel(ch);
        mode->insertChannel(ch, h);
        QLCFixtureHead head;
        head.addChannel(h);
        mode->insertHead(-1, head);
    }
    mode->cacheHeads();
    QLCPhysical phys = mode->physical();
    phys.setWidth(900);
    phys.setHeight(50);
    mode->setPhysical(phys);
    def->addMode(mode);

    Fixture *fx = new Fixture(m_doc);
    fx->setFixtureDefinition(def, mode);
    fx->setUniverse(1); fx->setAddress(0);
    m_doc->addFixture(fx);
    m_doc->monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(2000, 0, 1000));

    const QVector<QVector3D> at = RigHeads::headPositions(m_doc, fx->id());
    QCOMPARE(at.size(), 4);
    const QVector3D c = RigHeads::fixtureCentre(m_doc, fx->id());
    QVERIFY(qAbs(at.at(0).x() - (c.x() - 0.45f)) < 1e-3f);
    QVERIFY(qAbs(at.at(3).x() - (c.x() + 0.45f)) < 1e-3f);
    for (int h = 1; h < 4; ++h)
    {
        QVERIFY(qAbs((at.at(h).x() - at.at(h - 1).x()) - 0.3f) < 1e-3f);   // evenly, in order
        QVERIFY(qAbs(at.at(h).z() - at.at(0).z()) < 1e-4f);
    }
}

/* A lantern of sixteen LEDs on a wire: how many there are (its own build,
   then what the device says, then the definition) and where each is. */
void EffectInstance_Test::aLanternIsBuiltOfItsLeds()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Lantern");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setGroup(QLCChannel::Intensity);
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    QLCPhysical phys = mode->physical();
    phys.setWidth(150); phys.setHeight(250); phys.setDepth(150);
    phys.setBodyShape(QLCPhysical::LanternBody);
    phys.setEmission(QLCPhysical::OmniEmission);
    phys.setLeds(16);
    mode->setPhysical(phys);
    def->addMode(mode);

    Fixture *fx = new Fixture(m_doc);
    fx->setFixtureDefinition(def, mode);
    fx->setUniverse(1); fx->setAddress(0);
    m_doc->addFixture(fx);
    const quint32 fid = fx->id();
    MonitorProperties *props = m_doc->monitorProperties();
    props->setFixturePosition(fid, 0, 0, QVector3D(2000, 0, 3000));

    int layout = -1;
    QCOMPARE(RigHeads::lightLeds(m_doc, fid, 0, &layout), 16);       // the definition
    QCOMPARE(layout, int(QLCPhysical::ClusterLeds));
    QCOMPARE(RigHeads::lightLeds(m_doc, fid, 20), 20);               // the device knows better
    FixtureRigProps rp = props->fixtureRigProps(fid);
    rp.lightLeds = 0;
    props->setFixtureRigProps(fid, rp);
    QCOMPARE(RigHeads::lightLeds(m_doc, fid, 20), 0);                // this one is a bulb
    QVERIFY(RigHeads::lightPoints(m_doc, fid).isEmpty());
    rp.lightLeds = 8;
    props->setFixtureRigProps(fid, rp);
    QCOMPARE(RigHeads::lightLeds(m_doc, fid, 20), 8);                // and this one has eight
    rp.lightLeds = -1;
    props->setFixtureRigProps(fid, rp);

    // A cluster: in the middle of the body, the first at the top.
    const QVector3D c = RigHeads::fixtureCentre(m_doc, fid);
    QVector<QVector3D> at = RigHeads::lightPoints(m_doc, fid);
    QCOMPARE(at.size(), 16);
    QVector3D mean;
    foreach (const QVector3D &p, at)
    {
        mean += p;
        QVERIFY(qAbs(p.x() - c.x()) < 0.075f);
        QVERIFY(qAbs(p.z() - c.z()) < 0.125f);
    }
    QVERIFY(at.first().z() > at.last().z());
    mean /= 16.0f;
    QVERIFY(qAbs(mean.x() - c.x()) < 0.02f);

    // LED tape, wound round a core: tighter than the hanging cluster, and
    // still from the top down.
    auto spread = [](const QVector<QVector3D> &pts) {
        QVector3D mid;
        foreach (const QVector3D &p, pts)
            mid += p;
        mid /= float(pts.size());
        double s = 0.0;
        foreach (const QVector3D &p, pts)
            s += double(QVector2D(p.x() - mid.x(), p.y() - mid.y()).length());
        return s / pts.size();
    };
    const double clusterSpread = spread(at);
    rp.lightLayout = QLCPhysical::TapeLeds;
    props->setFixtureRigProps(fid, rp);
    at = RigHeads::lightPoints(m_doc, fid);
    QCOMPARE(at.size(), 16);
    QVERIFY(spread(at) < clusterSpread * 0.5);
    for (int i = 1; i < at.size(); ++i)
        QVERIFY(at.at(i).z() < at.at(i - 1).z());

    // Glass: diffusing unless the definition or the fixture says clear.
    QVERIFY(!RigHeads::clearGlass(m_doc, fid));
    rp.lightGlass = 1;
    props->setFixtureRigProps(fid, rp);
    QVERIFY(RigHeads::clearGlass(m_doc, fid));
    phys.setClearGlass(true);
    mode->setPhysical(phys);
    rp.lightGlass = 0;
    props->setFixtureRigProps(fid, rp);
    QVERIFY(!RigHeads::clearGlass(m_doc, fid));                     // its own wins
    rp.lightGlass = -1;
    props->setFixtureRigProps(fid, rp);
    QVERIFY(RigHeads::clearGlass(m_doc, fid));                      // else the definition
}

/* The ports of WLED's effects run as WLED's own code does: its integer
   maths, its colours, its mirror and reverse -- the numbers below are worked
   from FX.cpp by hand. */
void EffectInstance_Test::wledPortsRunAsWledDoes()
{
    const QString dir = QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/";
    EffectScriptCache cache;
    cache.addDirectory(QDir(dir));
    QVERIFY(cache.scriptPathForWled("chase").endsWith("wled-chase.js"));
    QVERIFY(cache.scriptPathForWled("Breathe").endsWith("wled-breathe.js"));
    QVERIFY(cache.scriptPathForWled("Game Of Life").isEmpty());   // 2D: not ported

    auto run = [](EffectScript &es, int n, double t, int speed, int intensity, int option,
                  const QList<QColor> &cols) {
        QJSEngine *eng = es.engine();
        QJSValue fixtures = eng->newArray(uint(n));
        for (int i = 0; i < n; ++i)
        {
            QJSValue o = eng->newObject();
            o.setProperty("head", i);
            fixtures.setProperty(uint(i), o);
        }
        QJSValue inputs = eng->newObject();
        inputs.setProperty("_time", t);
        QJSValue colours = eng->newArray(uint(cols.size()));
        for (int k = 0; k < cols.size(); ++k)
        {
            QJSValue c = eng->newObject();
            c.setProperty("r", cols.at(k).red());
            c.setProperty("g", cols.at(k).green());
            c.setProperty("b", cols.at(k).blue());
            colours.setProperty(uint(k), c);
        }
        QJSValue look = eng->newObject();
        look.setProperty("colors", colours);
        QJSValue palettes = eng->newObject();
        palettes.setProperty("look", look);
        QJSValue params = eng->newObject();
        params.setProperty("speed", speed);
        params.setProperty("intensity", intensity);
        params.setProperty("palette", 0);
        params.setProperty("option", option);
        QJSValue state = es.newState();
        const QJSValue r = es.callTick(fixtures, inputs, palettes, params, state, eng->newObject());
        QList<QColor> out;
        for (int i = 0; i < n; ++i)
        {
            const QJSValue o = r.property(uint(i));
            out << QColor(o.property("r").toInt(), o.property("g").toInt(), o.property("b").toInt());
        }
        return out;
    };
    const QColor red(255, 0, 0), blue(0, 0, 255);

    EffectScript chase;
    QVERIFY(chase.load(dir + "wled-chase.js"));            // lib/wled.js came with it
    QCOMPARE(chase.wled(), QString("Chase"));
    QCOMPARE(chase.paramDefs().size(), 4);

    /* Written from what WLED's Chase does (not its code): a block of colour 3
       runs round over the palette (colour 1 with none), a block of colour 1
       behind it -- so on the strip, one block of blue in the red. */
    const QColor bg(0, 0, 40);
    auto count = [](const QList<QColor> &px, const QColor &c) {
        int n = 0;
        for (const QColor &p : px) n += (p == c) ? 1 : 0;
        return n;
    };
    QList<QColor> px = run(chase, 20, 0.3, 128, 128, 0, { red, bg, blue });
    const int block = count(px, blue);
    QVERIFY2(block >= 1, "a block of colour 3");
    QCOMPARE(count(px, red), 20 - block);                  // the rest colour 1
    // One block, all together (round the end, too).
    int starts = 0;
    for (int i = 0; i < 20; ++i)
        if (px.at(i) == blue && px.at((i + 19) % 20) != blue)
            starts++;
    QCOMPARE(starts, 1);
    // It moves.
    QVERIFY(run(chase, 20, 0.6, 128, 128, 0, { red, bg, blue }) != px);
    // Wider at a higher intensity.
    QVERIFY(count(run(chase, 20, 0.3, 128, 255, 0, { red, bg, blue }), blue) > block);
    // Mirrored: the same both sides of the middle.
    px = run(chase, 20, 0.3, 128, 128, 64, { red, bg, blue });
    for (int i = 0; i < 10; ++i)
        QCOMPARE(px.at(i), px.at(19 - i));
    // Reversed: the other way round.
    const QList<QColor> fwd = run(chase, 20, 0.3, 128, 128, 0, { red, bg, blue });
    const QList<QColor> rev = run(chase, 20, 0.3, 128, 128, 128, { red, bg, blue });
    for (int i = 0; i < 20; ++i)
        QCOMPARE(rev.at(i), fwd.at(19 - i));

    EffectScript breathe;
    QVERIFY(breathe.load(dir + "wled-breathe.js"));
    QCOMPARE(breathe.wled(), QString("Breathe"));
    // A breath: dim at its start, the full colour at the top.
    const double period = 9000.0 * std::pow(900.0 / 9000.0, 128.0 / 255.0) / 1000.0;   // speed 128
    px = run(breathe, 4, 0.0, 128, 128, 0, { red, QColor(Qt::black), QColor(Qt::black) });
    foreach (const QColor &c, px)
        QVERIFY2(c.red() < 20, qPrintable(c.name()));
    px = run(breathe, 4, period / 2, 128, 128, 0, { red, QColor(Qt::black), QColor(Qt::black) });
    QVERIFY2(px.first().red() > 240, qPrintable(px.first().name()));

    /* Percent lights a share of the strip from its start -- intensity is the
       share -- in colour 1, the rest colour 2. */
    EffectScript percent;
    QVERIFY(percent.load(dir + "wled-percent.js"));
    QCOMPARE(percent.wled(), QString("Percent"));
    QCOMPARE(count(run(percent, 20, 1.0, 128, 0, 0, { red, bg, blue }), red), 0);
    px = run(percent, 20, 1.0, 128, 128, 0, { red, bg, blue });
    QCOMPARE(count(px, red), 10);
    for (int i = 0; i < 10; ++i)
        QCOMPARE(px.at(i), red);                       // from the start
    QCOMPARE(count(px, bg), 10);
    QCOMPARE(count(run(percent, 20, 1.0, 128, 64, 0, { red, bg, blue }), red), 5);
    QCOMPARE(count(run(percent, 20, 1.0, 128, 255, 0, { red, bg, blue }), red), 20);
    // Reversed, it fills from the other end.
    px = run(percent, 20, 1.0, 128, 64, 128, { red, bg, blue });
    for (int i = 15; i < 20; ++i)
        QCOMPARE(px.at(i), red);

    /* Solid Pattern: speed is how many lit, intensity how many not, over and
       over -- speed 8 is two, intensity 24 three. */
    EffectScript pattern;
    QVERIFY(pattern.load(dir + "wled-solid-pattern.js"));
    px = run(pattern, 20, 1.0, 8, 24, 0, { red, bg, blue });
    for (int i = 0; i < 20; ++i)
        QCOMPARE(px.at(i), (i % 5) < 2 ? red : bg);
    // Solid Pattern Tri: the three colours in runs of the intensity's size.
    EffectScript tri;
    QVERIFY(tri.load(dir + "wled-solid-pattern-tri.js"));
    px = run(tri, 12, 1.0, 128, 32, 0, { red, bg, blue });
    const QList<QColor> threes = { red, red, red, bg, bg, bg, blue, blue, blue, red, red, red };
    QCOMPARE(px, threes);

    /* Found by the names a WLED device lists them under -- "Sparkle+" is what
       WLED now calls its hyper sparkle. */
    EffectScriptCache all;
    all.addDirectory(QDir(dir));
    QVERIFY(all.scriptPathForWled("Sparkle+").endsWith("wled-sparkle-plus.js"));
    QVERIFY(all.scriptPathForWled("Fireworks Starburst").endsWith("wled-fireworks-starburst.js"));
    QVERIFY(all.scriptPathForWled("PS Fire 1D").endsWith("wled-ps-fire-1d.js"));
    QVERIFY(all.scriptPathForWled("Gravimeter").endsWith("wled-gravimeter.js"));
}

/* Every WLED effect written for qlcconsole loads, paints every pixel with a
   real colour, gives the same frame for the same moment, and (all but the
   still ones) moves. The look's colours, thirty pixels, a few moments. The
   sound-reactive ones hear a made-up song -- and rest, dark, with none. */
void EffectInstance_Test::everyWledPortRuns()
{
    const QString dir = QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/";
    // Still patterns: the same frame at every moment, as on a device.
    const QStringList still = { "Solid", "Solid Pattern", "Solid Pattern Tri", "Percent" };
    int ports = 0, heard = 0;
    foreach (const QString &file, QDir(dir).entryList({ "wled-*.js" }, QDir::Files))
    {
        EffectScript es;
        QVERIFY2(es.load(dir + file), qPrintable(file));
        if (es.wled().isEmpty())
            continue;                               // the device itself, not a port
        const bool listens = es.dataChannelKeys().contains(QStringLiteral("audio"));
        QJSEngine *eng = es.engine();
        auto frame = [&](double t, bool song = true) {
            QJSValue fixtures = eng->newArray(30);
            for (int i = 0; i < 30; ++i)
            {
                QJSValue o = eng->newObject();
                o.setProperty("head", 0);
                fixtures.setProperty(uint(i), o);
            }
            QJSValue inputs = eng->newObject();
            inputs.setProperty("_time", t);
            QJSValue colours = eng->newArray(3);
            const int rgb[3][3] = { { 255, 0, 0 }, { 0, 0, 40 }, { 0, 255, 0 } };
            for (int k = 0; k < 3; ++k)
            {
                QJSValue c = eng->newObject();
                c.setProperty("r", rgb[k][0]); c.setProperty("g", rgb[k][1]); c.setProperty("b", rgb[k][2]);
                colours.setProperty(uint(k), c);
            }
            QJSValue look = eng->newObject();
            look.setProperty("colors", colours);
            QJSValue palettes = eng->newObject();
            palettes.setProperty("look", look);
            QJSValue state = es.newState();
            QJSValue data = eng->newObject();
            if (listens && song)
            {
                // A song: sixteen bands rising and falling out of step.
                QJSValue bands = eng->newArray(16);
                double peak = 0;
                for (int k = 0; k < 16; ++k)
                {
                    const double v = qBound(0.0, 0.45 + 0.45 * std::sin(t * 3.0 + k * 0.9), 1.0);
                    bands.setProperty(uint(k), v);
                    peak = qMax(peak, v);
                }
                QJSValue audio = eng->newObject();
                audio.setProperty("bands", bands);
                audio.setProperty("level", 0.3 + 0.3 * std::sin(t * 2.1));
                audio.setProperty("peak", peak);
                audio.setProperty("maxHz", 5000);
                data.setProperty("audio", audio);
            }
            const QJSValue r = es.callTick(fixtures, inputs, palettes, eng->newObject(), state, data);
            QStringList px;
            for (int i = 0; i < 30; ++i)
            {
                const QJSValue o = r.property(uint(i));
                const int cr = o.property("r").toInt(), cg = o.property("g").toInt(), cb = o.property("b").toInt();
                if (o.isUndefined() || cr < 0 || cr > 255 || cg < 0 || cg > 255 || cb < 0 || cb > 255)
                    return QStringList();
                px << QString("%1,%2,%3").arg(cr).arg(cg).arg(cb);
            }
            return px;
        };
        QSet<QString> seen;
        for (double t : { 0.0, 0.41, 1.17, 2.3, 3.9 })
        {
            const QStringList f = frame(t);
            QVERIFY2(f.size() == 30, qPrintable(file + ": not a colour on every pixel"));
            seen.insert(f.join(QLatin1Char(' ')));
        }
        QCOMPARE(frame(1.17), frame(1.17));          // the same moment, the same frame
        if (!still.contains(es.wled()))
            QVERIFY2(seen.size() > 1, qPrintable(file + ": never moves"));
        else
            QVERIFY2(seen.size() == 1, qPrintable(file + ": should hold still"));
        if (listens)
        {
            // No audio input: it rests, dark, rather than making something up.
            const QStringList quiet = frame(1.17, false);
            QVERIFY2(quiet.size() == 30, qPrintable(file));
            for (const QString &p : quiet)
                QVERIFY2(p == QLatin1String("0,0,0"), qPrintable(file + ": lit with no audio"));
            heard++;
        }
        ports++;
    }
    QVERIFY2(ports >= 160, qPrintable(QString("only %1 WLED ports").arg(ports)));
    QVERIFY2(heard >= 23, qPrintable(QString("only %1 sound-reactive WLED ports").arg(heard)));
}

/* "for WLED based fixtures we should be selecting WLED effects": a look set
   to WLED's Chase sends a WLED lantern the effect and the look's settings --
   the lantern runs it -- while an ordinary RGB fixture in the same look runs
   the port of Chase, computed here. */
void EffectInstance_Test::aWledLookRunsOnTheDeviceAndThePortElsewhere()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    QCOMPARE(def->loadXML(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/WLED/WLED-Lantern.qxf"),
             QFile::NoError);
    QLCFixtureMode *mode = def->mode("Effect (15ch)");
    QVERIFY(mode != nullptr);
    Fixture *lantern = new Fixture(m_doc);
    lantern->setFixtureDefinition(def, mode);
    lantern->setUniverse(0); lantern->setAddress(100);
    QVERIFY(m_doc->addFixture(lantern));
    QVERIFY(WledFixture::runsEffects(lantern));
    QCOMPARE(WledFixture::effectValue(lantern, "chase"), 28);
    QVERIFY(WledFixture::effectNames(lantern).size() > 100);

    // An ordinary RGB par beside it.
    QLCFixtureDef *parDef = new QLCFixtureDef();
    parDef->setManufacturer("Test"); parDef->setModel("RGB");
    QLCFixtureMode *parMode = new QLCFixtureMode(parDef);
    const QLCChannel::Preset rgb[3] = { QLCChannel::IntensityRed, QLCChannel::IntensityGreen,
                                        QLCChannel::IntensityBlue };
    for (int i = 0; i < 3; ++i)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("c%1").arg(i));
        ch->setPreset(rgb[i]);
        parDef->addChannel(ch);
        parMode->insertChannel(ch, quint32(i));
    }
    QLCFixtureHead parHead;
    for (int i = 0; i < 3; ++i)
        parHead.addChannel(quint32(i));
    parMode->insertHead(-1, parHead);
    parMode->cacheHeads();
    parDef->addMode(parMode);
    Fixture *par = new Fixture(m_doc);
    par->setFixtureDefinition(parDef, parMode);
    par->setUniverse(0); par->setAddress(300);
    QVERIFY(m_doc->addFixture(par));
    QVERIFY(!WledFixture::runsEffects(par));

    QLCPalette *chase = new QLCPalette(QLCPalette::Effect, m_doc);
    chase->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/wled-chase.js");
    chase->setWledEffect("Chase");
    chase->setEffectParamValue("speed", 200);
    chase->setEffectParamValue("intensity", 64);
    chase->setEffectParamValue("palette", 6);
    QVERIFY(m_doc->addPalette(chase));
    QLCPalette *red = new QLCPalette(QLCPalette::Color, m_doc);
    red->setValue(QColor(255, 0, 0));
    QVERIFY(m_doc->addPalette(red));
    QLCPalette *blue = new QLCPalette(QLCPalette::Color, m_doc);
    blue->setValue(QColor(0, 0, 255));
    QVERIFY(m_doc->addPalette(blue));

    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixture(lantern->id());
    scene->addFixture(par->id());
    scene->addPalette(chase->id());
    scene->addPalette(red->id());
    scene->addPalette(blue->id());

    EffectInstance inst(m_doc, scene->id(), chase->id());
    QVERIFY(inst.isValid());
    inst.runTick();
    QHash<quint32, uchar> sent;                 // lantern channel -> value
    bool parDriven = false;
    foreach (const EffectInstance::DmxWrite &w, inst.dmxWrites())
    {
        if (w.fixtureId == lantern->id())
        {
            sent.insert(w.channel, w.value);
            QCOMPARE(w.addr, 100 + int(w.channel));
        }
        if (w.fixtureId == par->id())
            parDriven = true;
    }
    QCOMPARE(int(sent.value(1)), 28);           // Effect: Chase
    QCOMPARE(int(sent.value(2)), 200);          // speed
    QCOMPARE(int(sent.value(3)), 64);           // intensity
    QCOMPARE(int(sent.value(4)), 6);            // palette
    QCOMPARE(int(sent.value(6)), 255);          // colour 1: red
    QCOMPARE(int(sent.value(8)), 0);
    QCOMPARE(int(sent.value(11)), 255);         // colour 2: blue
    QVERIFY(sent.contains(0));                  // its master dimmer, a level
    // The port runs on the par, never on the lantern. (Its colours go out
    // scaled by the scene's envelope: 0 here, with the scene not running.)
    Q_UNUSED(parDriven)
    QCOMPARE(inst.effectiveFixtureIds(), QList<quint32>() << par->id());

    // Saved and read back with its WLED effect.
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buf);
    chase->saveXML(&writer);
    writer.writeEndDocument();
    buf.close();
    QVERIFY(buf.data().contains("<WledEffect>Chase</WledEffect>"));
    buf.open(QIODevice::ReadOnly);
    QXmlStreamReader reader(&buf);
    reader.readNextStartElement();
    QLCPalette back(QLCPalette::Effect, m_doc);
    QVERIFY(back.loadXML(reader));
    QCOMPARE(back.wledEffect(), QString("Chase"));
}

/* "Do we have a fire effect that goes from the bottom up?" We did -- the RGB
   Matrix Fire -- but an effect script called Fire hid it, and both are
   fire.js, so a look saved with one could come back as the other. */
void EffectInstance_Test::bothFiresAreOfferedAndEachComesBackAsItself()
{
    // Only the shipped libraries: not whatever is in this machine's home.
    QTemporaryDir home;
    const QByteArray realHome = qgetenv("HOME");
    qputenv("HOME", home.path().toUtf8());
#ifdef Q_OS_WIN
    // Windows takes the home folder from USERPROFILE, not HOME.
    const QByteArray realProfile = qgetenv("USERPROFILE");
    qputenv("USERPROFILE", home.path().toUtf8());
#endif
    EffectScriptCache cache;
    cache.addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    cache.addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "rgbscripts"), true);
    qputenv("HOME", realHome);
#ifdef Q_OS_WIN
    qputenv("USERPROFILE", realProfile);
#endif

    // Both listed: the flicker by its new name, the rising flame as Fire.
    const QStringList names = cache.scriptNames();
    QVERIFY(names.contains("Firelight"));
    QVERIFY(names.contains("Fire"));
    const QString flicker = cache.scriptPath("Firelight");
    const QString rising = cache.scriptPath("Fire");
    QVERIFY2(flicker.contains("effectscripts", Qt::CaseInsensitive), qPrintable(flicker));
    QVERIFY(EffectScriptCache::isRgbScriptPath(rising));
    QVERIFY(!EffectScriptCache::isRgbScriptPath(flicker));

    // The others both libraries have are listed twice, the RGB Matrix one so named.
    QVERIFY(names.contains("Plasma") && names.contains("Plasma (RGB Matrix)"));
    QVERIFY(names.contains("Meteor (RGB Matrix)"));

    // A look saves each under a name that brings back that one.
    QCOMPARE(EffectScriptCache::portableName(flicker), QString("fire"));
    QCOMPARE(EffectScriptCache::portableName(rising), QString("rgb:fire"));
    QCOMPARE(cache.scriptPath("fire"), flicker);         // older looks: the flicker, as before
    QCOMPARE(cache.scriptPath("rgb:fire"), rising);
    QCOMPARE(cache.scriptPath("rgb:plasma"), cache.scriptPath("Plasma (RGB Matrix)"));

    // And the rising one rises: on a column of lights, the bottom is lit
    // before the top ever is.
    // On a wall of lights eight wide, and on one bar standing up a tower (one
    // column: it was black, all of it feathered away as "edge").
    for (const int cols : { 8, 1 })
    {
    EffectScript fire;
    QVERIFY(fire.load(rising));
    QJSEngine *eng = fire.engine();
    const int rows = 16;
    QJSValue fixtures = eng->newArray(rows * cols);
    for (int i = 0; i < rows * cols; ++i)
    {
        QJSValue o = eng->newObject();
        o.setProperty("id", i);
        o.setProperty("head", 0);
        o.setProperty("hasRGB", true);
        QJSValue grid = eng->newObject();
        grid.setProperty("col", i % cols);
        grid.setProperty("row", i / cols);      // 0 is the top
        grid.setProperty("cols", cols);
        grid.setProperty("rows", rows);
        o.setProperty("grid", grid);
        fixtures.setProperty(quint32(i), o);
    }
    QJSValue state = fire.newState();
    double top = 0.0, bottom = 0.0;
    for (int tick = 0; tick < 120; ++tick)
    {
        QJSValue inputs = eng->newObject();
        inputs.setProperty("_time", tick * 0.05);
        QJSValue params = eng->newObject();
        for (const EffectScript::ParamDef &d : fire.paramDefs())
            params.setProperty(d.name, double(d.defaultValue));
        const QJSValue out = fire.callTick(fixtures, inputs, eng->newObject(), params, state, eng->newObject());
        QVERIFY(out.isArray());
        for (int i = 0; i < rows * cols; ++i)
        {
            const QJSValue o = out.property(quint32(i));
            const double lum = o.property("r").toNumber() + o.property("g").toNumber() + o.property("b").toNumber();
            (i / cols < rows / 2 ? top : bottom) += std::isfinite(lum) ? lum : 0.0;
        }
    }
    QVERIFY2(bottom > 0.0 && bottom > top * 1.5,
             qPrintable(QString("%1 wide: bottom %2 top %3").arg(cols).arg(bottom).arg(top)));
    }
}

/* A look arriving by a wipe brings its effect in by the same wipe: the
   cell the wipe has reached is lit, one it has not is dark. */
void EffectInstance_Test::anEffectArrivesByItsScenesTransition()
{
    MasterTimer timer(m_doc);
    QList<Fixture *> pars;
    for (int i = 0; i < 3; ++i)
    {
        QLCFixtureDef *def = new QLCFixtureDef();
        def->setManufacturer("Test"); def->setModel(QString("RGB %1").arg(i));
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        const QLCChannel::Preset rgb[3] = { QLCChannel::IntensityRed, QLCChannel::IntensityGreen,
                                            QLCChannel::IntensityBlue };
        QLCFixtureHead head;
        for (int c = 0; c < 3; ++c)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("c%1").arg(c));
            ch->setPreset(rgb[c]);
            def->addChannel(ch);
            mode->insertChannel(ch, quint32(c));
            head.addChannel(quint32(c));
        }
        mode->insertHead(-1, head);
        mode->cacheHeads();
        def->addMode(mode);
        Fixture *fx = new Fixture(m_doc);
        fx->setFixtureDefinition(def, mode);
        fx->setUniverse(0); fx->setAddress(quint32(400 + i * 3));
        QVERIFY(m_doc->addFixture(fx));
        m_doc->monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(3000.0f * i, 0.0f, 1.0f));
        pars << fx;
    }
    QLCPalette *rainbow = new QLCPalette(QLCPalette::Effect, m_doc);
    rainbow->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/rainbow.js");
    QVERIFY(m_doc->addPalette(rainbow));
    Scene *scene = new Scene(m_doc);
    for (Fixture *fx : pars)
        scene->addFixture(fx->id());
    scene->addPalette(rainbow->id());
    SceneTransition wipe;
    wipe.shape = SceneTransition::WipeToStageLeft;
    wipe.time = 2000;
    wipe.softness = 0.2;
    scene->setTransitionIn(wipe);
    m_doc->addFunction(scene);

    scene->start(&timer, FunctionParent::master());
    for (int k = 0; k < 20; ++k)            // 400 ms of the 2 s
        timer.timerTick();
    QVERIFY(scene->isRunning());

    EffectInstance inst(m_doc, scene->id(), rainbow->id());
    QVERIFY(inst.isValid());
    inst.runTick();
    QHash<quint32, int> lit;                 // fixture -> its brightest channel
    foreach (const EffectInstance::DmxWrite &w, inst.dmxWrites())
        lit[w.fixtureId] = qMax(lit.value(w.fixtureId), int(w.value));
    // 400 ms in, a 0.4 s edge: the stage-right par well up, the stage-left one not reached.
    QVERIFY2(lit.value(pars[0]->id()) > 100,
             qPrintable(QString("first %1").arg(lit.value(pars[0]->id()))));
    QCOMPARE(lit.value(pars[2]->id()), 0);
    scene->stop(FunctionParent::master());
    timer.timerTick();
}

void EffectInstance_Test::followSpotFadesAHeadThatCannotReach()
{
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *tower = props->addTruss();
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(1.67f, 1.372f, 0.0f));
    tower->setLength(3.048f);
    tower->setWidth(0.29f);

    // A 540/270 spot: pan and tilt in its head, the dimmer fixture-wide.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Spot 540/270");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("3ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity };
    const char *names[] = { "Pan", "Tilt", "Dimmer" };
    for (int k = 0; k < 3; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
        ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    QLCFixtureHead head;
    head.addChannel(0); head.addChannel(1);
    mode->insertHead(-1, head);
    def->addMode(mode);

    Fixture *spot = new Fixture(m_doc);
    spot->setName("SR Spot");
    spot->setFixtureDefinition(def, mode);
    spot->setAddress(100);
    QVERIFY(m_doc->addFixture(spot));
    const quint32 dimmer = spot->masterIntensityChannel();
    QCOMPARE(dimmer, quint32(2));

    // Standing on top of the tower: blind within 45 degrees of straight down.
    FixtureRigProps rp;
    rp.trussId = tower->id();
    rp.trussOffset = 3.048f;
    rp.mountingType = Truss::FloorMounted;
    props->setFixtureRigProps(spot->id(), rp);

    StageTarget *who = props->addStageTarget();
    who->setName("Vocalist");
    who->setKind(StageTarget::Kind::Person);

    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, m_doc);
    aim->setStageTargetId(who->id());
    QVERIFY(m_doc->addPalette(aim));
    QLCPalette *follow = new QLCPalette(QLCPalette::Effect, m_doc);
    follow->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/followspot.js");
    follow->setEffectParamValue("fadeNearLimit", 15.0);
    QVERIFY(m_doc->addPalette(follow));

    Scene *scene = new Scene(m_doc);
    scene->setName("Solo");
    m_doc->addFunction(scene);
    scene->addFixture(spot->id());
    scene->addPalette(aim->id());
    scene->addPalette(follow->id());

    EffectInstance inst(m_doc, scene->id(), follow->id());
    QVERIFY2(inst.isValid(), "followspot.js did not load");
    inst.setParamValue("fadeNearLimit", 15.0);

    auto dimmerWrites = [&]() {
        int n = 0;
        foreach (const EffectInstance::DmxWrite &w, inst.dmxWrites())
            if (w.fixtureId == spot->id() && w.channel == dimmer)
                ++n;
        return n;
    };

    // Half a metre in front of the tower: out of reach, so it fades.
    who->setPosition(QVector3D(1.67f, 1.872f, 0.0f));
    inst.runTick();
    QVERIFY2(dimmerWrites() == 1,
             "a head aimed where it cannot reach was left lit at its limit");

    // Three metres out: 20-odd degrees to spare, past the 15 of the fade.
    who->setPosition(QVector3D(1.67f, 4.372f, 0.0f));
    inst.runTick();
    QVERIFY2(dimmerWrites() == 0,
             "a head well within reach still had its dimmer taken from the scene");

    // Fade off: never touches the dimmer, reachable or not.
    inst.setParamValue("fadeNearLimit", 0.0);
    who->setPosition(QVector3D(1.67f, 1.872f, 0.0f));
    inst.runTick();
    QCOMPARE(dimmerWrites(), 0);
}

/* A spot on a downstage pipe, the main drape across the stage, and a person
   who walks from behind it to in front of it. */
static Fixture *drapeStageSpot(Doc *doc, Scenery *&drape, StageTarget *&who)
{
    MonitorProperties *props = doc->monitorProperties();
    drape = props->addScenery();
    drape->setKind(Scenery::MainDrape);
    drape->setName("Main drape");
    drape->setWidth(10.0f);
    drape->setHeight(6.0f);
    drape->setOpeningWidth(8.0f);

    Truss *pipe = props->addTruss();
    pipe->setType(Truss::Horizontal);
    pipe->setOrigin(QVector3D(-3.0f, 4.0f, 6.0f));
    pipe->setDirection(QPointF(1.0, 0.0));
    pipe->setLength(6.0f);
    pipe->setWidth(0.29f);

    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Spot behind the drape");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("3ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity };
    const char *names[] = { "Pan", "Tilt", "Dimmer" };
    for (int k = 0; k < 3; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
        ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    QLCFixtureHead head;
    head.addChannel(0); head.addChannel(1);
    mode->insertHead(-1, head);
    def->addMode(mode);

    Fixture *spot = new Fixture(doc);
    spot->setName("FOH spot");
    spot->setFixtureDefinition(def, mode);
    spot->setAddress(300);
    if (!doc->addFixture(spot))
        return nullptr;
    FixtureRigProps rp;
    rp.trussId = pipe->id();
    rp.trussOffset = 3.0f;
    rp.mountingType = Truss::TopHung;
    props->setFixtureRigProps(spot->id(), rp);

    who = props->addStageTarget();
    who->setName("Vocalist");
    who->setKind(StageTarget::Kind::Person);
    who->setPosition(QVector3D(0.0f, -3.0f, 0.0f));     // behind the drape
    return spot;
}

void EffectInstance_Test::followSpotFadesAHeadThatCannotSeeTheSubject()
{
    Scenery *drape = nullptr;
    StageTarget *who = nullptr;
    Fixture *spot = drapeStageSpot(m_doc, drape, who);
    QVERIFY(spot != nullptr);
    const quint32 dimmer = spot->masterIntensityChannel();

    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, m_doc);
    aim->setStageTargetId(who->id());
    QVERIFY(m_doc->addPalette(aim));
    QLCPalette *follow = new QLCPalette(QLCPalette::Effect, m_doc);
    follow->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/followspot.js");
    QVERIFY(m_doc->addPalette(follow));

    Scene *scene = new Scene(m_doc);
    scene->setName("Solo");
    m_doc->addFunction(scene);
    scene->addFixture(spot->id());
    scene->addPalette(aim->id());
    scene->addPalette(follow->id());
    MasterTimer timer(m_doc);
    scene->start(&timer, FunctionParent::master());
    timer.timerTick();
    QVERIFY(scene->isRunning());

    EffectInstance inst(m_doc, scene->id(), follow->id());
    QVERIFY2(inst.isValid(), "followspot.js did not load");

    // The dimmer this tick wrote, as a fraction of full; -1 when it wrote none.
    auto level = [&]() {
        inst.runTick();
        foreach (const EffectInstance::DmxWrite &w, inst.dmxWrites())
            if (w.fixtureId == spot->id() && w.channel == dimmer)
                return int(w.value);
        return -1;
    };

    // Off (the default): behind the drape or not, it never touches the dimmer.
    QCOMPARE(level(), -1);
    QCOMPARE(level(), -1);

    // 100 ms: five ticks from full to dark.
    inst.setParamValue("fadeWhenBlocked", 100.0);
    int v = level();
    QVERIFY2(v > 0 && v < 255, qPrintable(QString("first blocked tick wrote %1").arg(v)));
    int prev = v;
    for (int i = 0; i < 6; ++i)
    {
        v = level();
        QVERIFY2(v <= prev, "went up while blocked");
        prev = v;
    }
    QCOMPARE(v, 0);

    // The drape flies out: it comes back up, and once full hands the dimmer back.
    drape->setState(Scenery::Flown);
    v = level();
    QVERIFY2(v > 0, "still dark with nothing in the way");
    for (int i = 0; i < 6; ++i)
        v = level();
    QCOMPARE(v, -1);

    // Down again, then the subject walks out in front of the drape.
    drape->setState(Scenery::Closed);
    for (int i = 0; i < 6; ++i)
        v = level();
    QCOMPARE(v, 0);
    who->setPosition(QVector3D(0.0f, 2.0f, 0.0f));
    for (int i = 0; i < 6; ++i)
        v = level();
    QCOMPARE(v, -1);

    // Turned off while dark: hands the dimmer straight back.
    who->setPosition(QVector3D(0.0f, -3.0f, 0.0f));
    for (int i = 0; i < 6; ++i)
        v = level();
    QCOMPARE(v, 0);
    inst.setParamValue("fadeWhenBlocked", 0.0);
    QCOMPARE(level(), -1);
    scene->stop(FunctionParent::master());
    timer.timerTick();
}

void EffectInstance_Test::aScriptIsToldWhatIsInTheWay()
{
    Scenery *drape = nullptr;
    StageTarget *who = nullptr;
    Fixture *spot = drapeStageSpot(m_doc, drape, who);
    QVERIFY(spot != nullptr);
    const quint32 dimmer = spot->masterIntensityChannel();

    // A script that reports what it is told through the dimmer.
    QTemporaryDir dir;
    QFile js(dir.filePath("sight.js"));
    QVERIFY(js.open(QIODevice::WriteOnly));
    js.write("(function() { var e = {}; e.apiVersion = 1; e.name = 'Sight';"
             " e.tick = function(fixtures) { return fixtures.map(function(f) {"
             "   var t = f.sceneTarget; if (!t) return { dimmer: 0.1 };"
             "   if (t.blocked === true && t.blockedBy === 'Main drape' && t.blockedKind === 'drape'"
             "       && t.blockedAt > 3 && t.blockedAt < 7 && t.clearance <= 0) return { dimmer: 0.25 };"
             "   if (t.blocked === false && t.blockedBy === '' && t.blockedKind === ''"
             "       && t.clearance > 0) return { dimmer: 0.75 };"
             "   return { dimmer: 0.5 }; }); }; return e; })()");
    js.close();

    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, m_doc);
    aim->setStageTargetId(who->id());
    QVERIFY(m_doc->addPalette(aim));
    QLCPalette *eff = new QLCPalette(QLCPalette::Effect, m_doc);
    eff->setScriptPath(js.fileName());
    QVERIFY(m_doc->addPalette(eff));
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixture(spot->id());
    scene->addPalette(aim->id());
    scene->addPalette(eff->id());
    MasterTimer timer(m_doc);
    scene->start(&timer, FunctionParent::master());
    timer.timerTick();

    EffectInstance inst(m_doc, scene->id(), eff->id());
    QVERIFY(inst.isValid());
    auto told = [&]() {
        inst.runTick();
        foreach (const EffectInstance::DmxWrite &w, inst.dmxWrites())
            if (w.fixtureId == spot->id() && w.channel == dimmer)
                return int(w.value);
        return -1;
    };
    QCOMPARE(told(), int(qRound(255 * 0.25)));
    drape->setState(Scenery::Open);
    QCOMPARE(told(), int(qRound(255 * 0.75)));
    scene->stop(FunctionParent::master());
    timer.timerTick();
}

void EffectInstance_Test::followSpotZoomsToHoldSizeOnSubject()
{
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *pipe = props->addTruss();
    pipe->setType(Truss::Horizontal);
    pipe->setOrigin(QVector3D(-3.0f, 0.0f, 6.0f));
    pipe->setDirection(QPointF(1.0, 0.0));
    pipe->setLength(6.0f);
    pipe->setWidth(0.29f);

    // Pan, Tilt in the head; Dimmer and a 10-40 degree zoom fixture-wide.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Zoom Spot");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("4ch");
    const char *names[] = { "Pan", "Tilt", "Dimmer", "Zoom" };
    for (int k = 0; k < 4; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
        ch->setControlByte(QLCChannel::MSB);
        if (k == 0) ch->setGroup(QLCChannel::Pan);
        else if (k == 1) ch->setGroup(QLCChannel::Tilt);
        else if (k == 2) ch->setGroup(QLCChannel::Intensity);
        else { ch->setGroup(QLCChannel::Beam); ch->setPreset(QLCChannel::BeamZoomSmallBig); }
        def->addChannel(ch);
        mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    ph.setLensDegreesMin(10);
    ph.setLensDegreesMax(40);
    mode->setPhysical(ph);
    QLCFixtureHead head;
    head.addChannel(0); head.addChannel(1);
    mode->insertHead(-1, head);
    def->addMode(mode);

    Fixture *spot = new Fixture(m_doc);
    spot->setName("FOH Spot");
    spot->setFixtureDefinition(def, mode);
    spot->setAddress(200);
    QVERIFY(m_doc->addFixture(spot));
    const quint32 zoomCh = 3;

    FixtureRigProps rp;
    rp.trussId = pipe->id();
    rp.trussOffset = 3.0f;
    rp.mountingType = Truss::TopHung;
    props->setFixtureRigProps(spot->id(), rp);

    StageTarget *who = props->addStageTarget();
    who->setName("Vocalist");
    who->setKind(StageTarget::Kind::Person);

    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, m_doc);
    aim->setStageTargetId(who->id());
    QVERIFY(m_doc->addPalette(aim));
    QLCPalette *follow = new QLCPalette(QLCPalette::Effect, m_doc);
    follow->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/followspot.js");
    QVERIFY(m_doc->addPalette(follow));

    Scene *scene = new Scene(m_doc);
    scene->setName("Solo");
    m_doc->addFunction(scene);
    scene->addFixture(spot->id());
    scene->addPalette(aim->id());
    scene->addPalette(follow->id());

    EffectInstance inst(m_doc, scene->id(), follow->id());
    QVERIFY2(inst.isValid(), "followspot.js did not load");
    inst.setParamValue("sizeOnSubject", 150.0);

    auto zoomWrite = [&](int &value) {
        int n = 0;
        foreach (const EffectInstance::DmxWrite &w, inst.dmxWrites())
            if (w.fixtureId == spot->id() && w.channel == zoomCh)
            {
                value = w.value;
                ++n;
            }
        return n;
    };
    // What the lens has to do for a 1.5 m beam at the subject, 10..40 degrees.
    auto expected = [&]() {
        AimSolver::Frame frame;
        AimSolver::frameFor(m_doc, spot->id(), frame);
        const double dist = double((props->targetAimPoint(who, true) - frame.position).length());
        const double want = 2.0 * qRadiansToDegrees(qAtan(0.75 / dist));
        return int(qBound(0.0, (want - 10.0) / 30.0, 1.0) * 255);
    };

    int value = -1;
    who->setPosition(QVector3D(0.0f, 1.0f, 0.0f));      // under the pipe: short throw
    inst.runTick();
    QCOMPARE(zoomWrite(value), 1);
    const int nearValue = value;
    QVERIFY2(qAbs(value - expected()) <= 1,
             qPrintable(QString("near: zoom %1, expected %2").arg(value).arg(expected())));
    QVERIFY2(nearValue > 0, "a close subject should need more than the narrowest beam");

    who->setPosition(QVector3D(0.0f, 5.0f, 0.0f));      // further out: zoom in
    inst.runTick();
    QCOMPARE(zoomWrite(value), 1);
    QVERIFY2(qAbs(value - expected()) <= 1,
             qPrintable(QString("far: zoom %1, expected %2").arg(value).arg(expected())));
    QVERIFY2(value < nearValue,
             qPrintable(QString("further away the beam must narrow: %1 near, %2 far")
                        .arg(nearValue).arg(value)));

    // Off: the zoom stays wherever the scene put it.
    inst.setParamValue("sizeOnSubject", 0.0);
    inst.runTick();
    QCOMPARE(zoomWrite(value), 0);
}

void EffectInstance_Test::followSpotMovesEveryHeadToTheSamePoint()
{
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *pipe = props->addTruss();
    pipe->setType(Truss::Horizontal);
    pipe->setOrigin(QVector3D(-3.0f, 0.0f, 6.0f));
    pipe->setDirection(QPointF(1.0, 0.0));
    pipe->setLength(6.0f);
    pipe->setWidth(0.29f);

    // 16-bit pan and tilt in the head, so where a beam lands reads to a
    // centimetre or so; the dimmer fixture-wide.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Spot 16bit");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("5ch");
    const struct { const char *name; QLCChannel::Group group; QLCChannel::ControlByte byte; } chans[] = {
        { "Pan", QLCChannel::Pan, QLCChannel::MSB }, { "Pan fine", QLCChannel::Pan, QLCChannel::LSB },
        { "Tilt", QLCChannel::Tilt, QLCChannel::MSB }, { "Tilt fine", QLCChannel::Tilt, QLCChannel::LSB },
        { "Dimmer", QLCChannel::Intensity, QLCChannel::MSB } };
    for (int k = 0; k < 5; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(chans[k].name);
        ch->setGroup(chans[k].group);
        ch->setControlByte(chans[k].byte);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    QLCFixtureHead head;
    for (int k = 0; k < 4; ++k)
        head.addChannel(k);
    mode->insertHead(-1, head);
    def->addMode(mode);

    // Two of them, four metres apart on the pipe.
    QList<Fixture *> spots;
    for (int i = 0; i < 2; ++i)
    {
        Fixture *spot = new Fixture(m_doc);
        spot->setName(QString("Spot %1").arg(i + 1));
        spot->setFixtureDefinition(def, mode);
        spot->setUniverse(0);
        spot->setAddress(100 + 10 * i);
        QVERIFY(m_doc->addFixture(spot));
        FixtureRigProps rp;
        rp.trussId = pipe->id();
        rp.trussOffset = i == 0 ? 1.0f : 5.0f;
        rp.mountingType = Truss::TopHung;
        props->setFixtureRigProps(spot->id(), rp);
        spots << spot;
    }

    StageTarget *who = props->addStageTarget();
    who->setName("Vocalist");
    who->setKind(StageTarget::Kind::Person);
    who->setPosition(QVector3D(0.0f, 2.0f, 0.0f));

    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, m_doc);
    aim->setStageTargetId(who->id());
    QVERIFY(m_doc->addPalette(aim));
    QLCPalette *follow = new QLCPalette(QLCPalette::Effect, m_doc);
    follow->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/followspot.js");
    QVERIFY(m_doc->addPalette(follow));
    Scene *scene = new Scene(m_doc);
    scene->setName("Solo");
    m_doc->addFunction(scene);
    for (Fixture *spot : spots)
        scene->addFixture(spot->id());
    scene->addPalette(aim->id());
    scene->addPalette(follow->id());

    MasterTimer timer(m_doc);
    auto play = [&]() {
        for (int k = 0; k < 5; ++k)
        {
            timer.timerTick();
            QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
            ua.first()->processFaders();
            m_doc->inputOutputMap()->releaseUniverses(false);
        }
    };
    // Where a head's beam passes nearest @p point, by its pan and tilt as DMX
    // has them: metres off.
    auto missBy = [&](Fixture *spot, const QVector3D &point) {
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        const QByteArray dmx = ua.first()->preGMValues();
        m_doc->inputOutputMap()->releaseUniverses(false);
        const int a = int(spot->address());
        const double pan16 = (uchar(dmx.at(a)) << 8) | uchar(dmx.at(a + 1));
        const double tilt16 = (uchar(dmx.at(a + 2)) << 8) | uchar(dmx.at(a + 3));
        const double pan = 540.0 * pan16 / 65535.0 - 270.0;     // centred
        const double tilt = 270.0 * tilt16 / 65535.0 - 135.0;
        AimSolver::Frame frame;
        if (!AimSolver::frameFor(m_doc, spot->id(), frame))
            return 1.0e9;
        QVector3D dir;
        if (!AimSolver::beamDirection(props->fixtureRigProps(spot->id()), pan, tilt, dir))
            return 1.0e9;
        const QVector3D to = point - frame.position;
        const float along = QVector3D::dotProduct(to, dir);
        if (along <= 0.0f)
            return 1.0e9;                                       // pointing away
        return double((to - along * dir).length());
    };
    // The one point both heads should be on (a person: the same for both).
    auto spotPoint = [&]() {
        const QVector3D p0 = AimSolver::aimPointFor(m_doc, spots[0]->id(), who, true);
        const QVector3D p1 = AimSolver::aimPointFor(m_doc, spots[1]->id(), who, true);
        return (p0 - p1).length() < 0.001f ? p0 : QVector3D(1.0e6f, 0.0f, 0.0f);
    };
    auto show = [](double d) { return QString::number(d, 'f', 3); };

    scene->start(&timer, FunctionParent::master());
    play();
    QVERIFY(scene->isRunning());
    const QVector3D first = spotPoint();
    QVERIFY(first.x() < 1.0e5f);
    for (Fixture *spot : spots)
        QVERIFY2(missBy(spot, first) < 0.05,
                 qPrintable(QString("%1 misses the spot by %2 m").arg(spot->name(), show(missBy(spot, first)))));

    // The effect itself leaves pan and tilt to the Aim look.
    EffectInstance inst(m_doc, scene->id(), follow->id());
    QVERIFY2(inst.isValid(), "followspot.js did not load");
    inst.runTick();
    foreach (const EffectInstance::DmxWrite &w, inst.dmxWrites())
        QVERIFY2(w.channel > 3, qPrintable(QString("the effect wrote channel %1").arg(w.channel)));

    // The follow-spot point moves (what the stick does: the target moves and
    // the look re-aims): both heads move onto the same new point.
    who->setPosition(QVector3D(1.5f, 4.0f, 0.0f));
    scene->requestReaim();
    play();
    const QVector3D moved = spotPoint();
    QVERIFY(moved.x() < 1.0e5f);
    QVERIFY((moved - first).length() > 2.0f);
    for (Fixture *spot : spots)
    {
        QVERIFY2(missBy(spot, moved) < 0.05,
                 qPrintable(QString("%1 misses the moved spot by %2 m").arg(spot->name(), show(missBy(spot, moved)))));
        QVERIFY2(missBy(spot, first) > 1.0,
                 qPrintable(QString("%1 is still on the old spot").arg(spot->name())));
    }
    inst.runTick();
    foreach (const EffectInstance::DmxWrite &w, inst.dmxWrites())
        QVERIFY2(w.channel > 3, qPrintable(QString("the effect wrote channel %1").arg(w.channel)));

    scene->stop(FunctionParent::master());
    timer.timerTick();
}

/* "is it possible to visualize w/o patching?": an effect on a look whose
   targets are in the inventory still works out what they would do -- their
   channels are inert, so it reaches the rig view and not DMX. It skipped
   unpatched fixtures outright, so the FART bars (all unpatched) stayed dark
   under a spatial gradient that lit the patched step fronts. */
void EffectInstance_Test::anEffectRunsOnUnpatchedFixturesToo()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("RGB1");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    const QLCChannel::Preset pr[3] = { QLCChannel::IntensityRed, QLCChannel::IntensityGreen,
                                       QLCChannel::IntensityBlue };
    for (int c = 0; c < 3; c++)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("C%1").arg(c));
        ch->setPreset(pr[c]);
        def->addChannel(ch);
        mode->insertChannel(ch, c);
    }
    QLCFixtureHead head;
    head.addChannel(0); head.addChannel(1); head.addChannel(2);
    mode->insertHead(-1, head);
    mode->cacheHeads();
    def->addMode(mode);

    Fixture *patched = new Fixture(m_doc);
    patched->setFixtureDefinition(def, mode);
    patched->setUniverse(1); patched->setAddress(0);
    QVERIFY(m_doc->addFixture(patched));
    Fixture *staged = new Fixture(m_doc);
    staged->setFixtureDefinition(def, mode);
    staged->setPatched(false);
    QVERIFY(m_doc->addFixture(staged));

    QLCPalette *red = new QLCPalette(QLCPalette::Color, m_doc);
    red->setValue(QColor(255, 0, 0));
    QVERIFY(m_doc->addPalette(red));
    QLCPalette *blue = new QLCPalette(QLCPalette::Color, m_doc);
    blue->setValue(QColor(0, 0, 255));
    QVERIFY(m_doc->addPalette(blue));
    QLCPalette *grad = new QLCPalette(QLCPalette::Effect, m_doc);
    grad->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/spatial-gradient.js");
    QVERIFY(m_doc->addPalette(grad));

    Scene *scene = new Scene(m_doc);
    scene->setName("Gradient");
    m_doc->addFunction(scene);
    scene->addFixture(patched->id());
    scene->addFixture(staged->id());
    scene->addPalette(grad->id());
    scene->addPalette(red->id());
    scene->addPalette(blue->id());

    EffectInstance inst(m_doc, scene->id(), grad->id());
    QVERIFY2(inst.isValid(), "spatial-gradient.js did not load");
    /* What the script hands back, per cell -- each lit red -- turned into
       writes: the unpatched fixture's cell is not dropped. */
    int cols = 0, rows = 0;
    const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);
    QCOMPARE(cells.size(), 2);
    QJSEngine *js = inst.m_script.engine();
    QJSValue intents = js->newArray(quint32(cells.size()));
    for (int i = 0; i < cells.size(); ++i)
    {
        QJSValue o = js->newObject();
        o.setProperty("r", 255); o.setProperty("g", 0); o.setProperty("b", 0);
        intents.setProperty(quint32(i), o);
    }
    int forPatched = 0, forStaged = 0;
    foreach (const EffectInstance::DmxWrite &w, inst.parseIntents(intents, cells))
    {
        forPatched += (w.fixtureId == patched->id());
        forStaged += (w.fixtureId == staged->id());
    }
    QVERIFY2(forPatched > 0, "no writes at all: the intent shape is wrong");
    QVERIFY2(forStaged > 0, "the effect skipped the unpatched fixture");
}

/* "expected the rainbow chase ... only had white" -- an RGBW wash with no
   master dimmer (the Cobra Wash in 13-channel mode): Rainbow's intent brings
   a hue and dimmer 1, and the dimmer put R, G, B and W all at full. */
void EffectInstance_Test::aRainbowOnRgbwWithNoDimmerIsAHueNotWhite()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("RGBW1");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    const QLCChannel::Preset pr[4] = { QLCChannel::IntensityRed, QLCChannel::IntensityGreen,
                                       QLCChannel::IntensityBlue, QLCChannel::IntensityWhite };
    for (int c = 0; c < 4; c++)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("C%1").arg(c));
        ch->setPreset(pr[c]);
        def->addChannel(ch);
        mode->insertChannel(ch, c);
    }
    QLCFixtureHead head;
    for (int c = 0; c < 4; c++)
        head.addChannel(quint32(c));
    mode->insertHead(-1, head);
    mode->cacheHeads();
    def->addMode(mode);
    Fixture *wash = new Fixture(m_doc);
    wash->setFixtureDefinition(def, mode);
    wash->setUniverse(1); wash->setAddress(0);
    QVERIFY(m_doc->addFixture(wash));
    QCOMPARE(wash->masterIntensityChannel(), QLCChannel::invalid());

    QLCPalette *rainbow = new QLCPalette(QLCPalette::Effect, m_doc);
    rainbow->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/rainbow.js");
    QVERIFY(m_doc->addPalette(rainbow));
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixture(wash->id());
    scene->addPalette(rainbow->id());

    EffectInstance inst(m_doc, scene->id(), rainbow->id());
    QVERIFY(inst.isValid());
    int cols = 0, rows = 0;
    const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);
    QCOMPARE(cells.size(), 1);
    QJSEngine *js = inst.m_script.engine();
    QJSValue intents = js->newArray(1);
    QJSValue o = js->newObject();                    // what rainbow.js sends: a hue, and dimmer 1
    o.setProperty("r", 0); o.setProperty("g", 120); o.setProperty("b", 255);
    o.setProperty("dimmer", 1.0);
    intents.setProperty(0, o);
    QMap<quint32, int> out;                          // channel -> highest value written
    foreach (const EffectInstance::DmxWrite &w, inst.parseIntents(intents, cells))
        out[w.channel] = qMax(out.value(w.channel), int(w.value));
    QCOMPARE(out.value(0), 0);                       // red stays dark
    QCOMPARE(out.value(1), 120);
    QCOMPARE(out.value(2), 255);
    QCOMPARE(out.value(3), 0);                       // and white is not lit

    o.setProperty("dimmer", 0.5);                    // the dimmer dims the hue
    out.clear();
    foreach (const EffectInstance::DmxWrite &w, inst.parseIntents(intents, cells))
        out[w.channel] = qMax(out.value(w.channel), int(w.value));
    QCOMPARE(out.value(1), 60);
    QCOMPARE(out.value(2), 128);
    QCOMPARE(out.value(3), 0);

    // The colour is the effect's: written over the look's, dark channels too.
    bool redWritten = false;
    foreach (const EffectInstance::DmxWrite &w, inst.parseIntents(intents, cells))
    {
        if (w.channel <= 2)
            QVERIFY2(w.replace, "a colour effect's colour merged with the look's");
        redWritten = redWritten || w.channel == 0;
    }
    QVERIFY2(redWritten, "red at 0 left to the look");
}

/* "If I set a color on the entire set I see the color sweep but no rainbow" */
void EffectInstance_Test::aRainbowWithALookColourSweepsThatColour()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("RGB3");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    const QLCChannel::Preset pr[3] = { QLCChannel::IntensityRed, QLCChannel::IntensityGreen,
                                       QLCChannel::IntensityBlue };
    for (int c = 0; c < 3; c++)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("C%1").arg(c));
        ch->setPreset(pr[c]);
        def->addChannel(ch);
        mode->insertChannel(ch, c);
    }
    QLCFixtureHead head;
    head.addChannel(0); head.addChannel(1); head.addChannel(2);
    mode->insertHead(-1, head);
    mode->cacheHeads();
    def->addMode(mode);
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    for (int i = 0; i < 4; ++i)
    {
        Fixture *fx = new Fixture(m_doc);
        fx->setFixtureDefinition(def, mode);
        fx->setUniverse(2); fx->setAddress(quint32(i * 3));
        QVERIFY(m_doc->addFixture(fx));
        scene->addFixture(fx->id());
    }
    QLCPalette *rainbow = new QLCPalette(QLCPalette::Effect, m_doc);
    rainbow->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/rainbow.js");
    QVERIFY(m_doc->addPalette(rainbow));
    scene->addPalette(rainbow->id());

    auto sample = [&](QList<QColor> &heads) {
        EffectInstance inst(m_doc, scene->id(), rainbow->id());
        QVERIFY(inst.isValid());
        inst.runTick();
        QMap<int, QColor> byHead;
        foreach (const EffectInstance::DmxWrite &w, inst.dmxWrites())
        {
            if (!byHead.contains(w.addr / 3))
                byHead.insert(w.addr / 3, QColor(0, 0, 0));
            QColor &c = byHead[w.addr / 3];
            if (w.addr % 3 == 0) c.setRed(w.value);
            else if (w.addr % 3 == 1) c.setGreen(w.value);
            else c.setBlue(w.value);
        }
        heads = byHead.values();
    };

    // The look playing: an effect owns its heads' colour only while it's on.
    scene->start(m_doc->masterTimer(), FunctionParent::master());
    for (int i = 0; i < 3; ++i)
        m_doc->masterTimer()->timerTick();
    QVERIFY(scene->isRunning());

    QList<QColor> heads;
    sample(heads);                                   // no colour: the wheel
    QCOMPARE(heads.size(), 4);
    QSet<int> hues;
    foreach (const QColor &c, heads)
        hues << c.hue() / 30;
    QString seen;
    foreach (const QColor &c, heads)
        seen += QString(" (%1 %2 %3)").arg(c.red()).arg(c.green()).arg(c.blue());
    QVERIFY2(hues.size() >= 3, qPrintable("no colour in the look: a rainbow across the heads:" + seen));

    QLCPalette *cyan = new QLCPalette(QLCPalette::Color, m_doc);
    cyan->setValue(QColor(0, 220, 255));
    QVERIFY(m_doc->addPalette(cyan));
    scene->addPalette(cyan->id());
    sample(heads);                                   // cyan: a sweep of cyan
    int brightest = 0, dimmest = 255;
    foreach (const QColor &c, heads)
    {
        QCOMPARE(c.red(), 0);
        QVERIFY2(qAbs(c.green() - c.blue() * 220 / 255) <= 2,
                 qPrintable(QString("not cyan: %1 %2 %3").arg(c.red()).arg(c.green()).arg(c.blue())));
        brightest = qMax(brightest, c.blue());
        dimmest = qMin(dimmest, c.blue());
    }
    QVERIFY2(brightest - dimmest > 100, "the cyan should sweep, bright to dark across the heads");

    // Not playing: it writes nothing -- no holding the heads black. (The
    // same look, never started; stopping this one from a timerTick() on the
    // test's thread would deadlock on the universes the tick holds.)
    Scene *idle = new Scene(m_doc);
    m_doc->addFunction(idle);
    foreach (quint32 fid, scene->fixtures())
        idle->addFixture(fid);
    idle->addPalette(rainbow->id());
    idle->addPalette(cyan->id());
    QVERIFY(!idle->isRunning());
    EffectInstance after(m_doc, idle->id(), rainbow->id());
    after.runTick();
    foreach (const EffectInstance::DmxWrite &w, after.dmxWrites())
        QVERIFY2(!(w.replace && w.value == 0), "a stopped effect holds a channel at 0");
}

/* "should I expect the lights to ballyhoo over the point of target 2? cause
   it seems to ballyhoo pointing straight down" -- a movement effect moves
   round where the look's Aim or Pan/Tilt points each head. */
void EffectInstance_Test::aMovementEffectMovesRoundWhereTheLookPoints()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Mover");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    const QLCChannel::Preset pr[2] = { QLCChannel::PositionPan, QLCChannel::PositionTilt };
    for (int c = 0; c < 2; c++)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(c == 0 ? "Pan" : "Tilt");
        ch->setPreset(pr[c]);
        def->addChannel(ch);
        mode->insertChannel(ch, c);
    }
    QLCFixtureHead head;
    head.addChannel(0); head.addChannel(1);
    mode->insertHead(-1, head);
    mode->cacheHeads();
    def->addMode(mode);
    Fixture *mover = new Fixture(m_doc);
    mover->setFixtureDefinition(def, mode);
    mover->setUniverse(3); mover->setAddress(0);
    QVERIFY(m_doc->addFixture(mover));

    QLCPalette *bally = new QLCPalette(QLCPalette::Effect, m_doc);
    bally->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/ballyhoo.js");
    bally->setEffectParamValue("panSize", 0.0);      // no sweep: just its centre
    bally->setEffectParamValue("tiltSize", 0.0);
    QVERIFY(m_doc->addPalette(bally));

    auto centre = [&](Scene *scene, int &pan, int &tilt) {
        EffectInstance inst(m_doc, scene->id(), bally->id());
        QVERIFY(inst.isValid());
        inst.runTick();
        pan = tilt = -1;
        foreach (const EffectInstance::DmxWrite &w, inst.dmxWrites())
        {
            if (w.fixtureId != mover->id()) continue;
            if (w.channel == 0) pan = w.value;
            if (w.channel == 1) tilt = w.value;
        }
    };

    // Nothing pointing it: the middle of its range, as before.
    Scene *free = new Scene(m_doc);
    m_doc->addFunction(free);
    free->addFixture(mover->id());
    free->addPalette(bally->id());
    int pan = -1, tilt = -1;
    centre(free, pan, tilt);
    QVERIFY2(qAbs(pan - 128) <= 1 && qAbs(tilt - 128) <= 1,
             qPrintable(QString("pan %1 tilt %2").arg(pan).arg(tilt)));

    // The look points it a quarter of the way round, near the top of its tilt.
    Scene *aimed = new Scene(m_doc);
    m_doc->addFunction(aimed);
    aimed->addFixture(mover->id());
    aimed->setValue(mover->id(), 0, 64);
    aimed->setValue(mover->id(), 1, 40);
    aimed->addPalette(bally->id());
    centre(aimed, pan, tilt);
    QVERIFY2(qAbs(pan - 64) <= 1 && qAbs(tilt - 40) <= 1,
             qPrintable(QString("pan %1 tilt %2, the look points it at 64 / 40").arg(pan).arg(tilt)));
}

/* "breathe isn't applying to this set": a mover whose definition puts its
   Dimmer in its head (no master dimmer) and colours by a wheel. Breathe's
   dimmer goes to that Dimmer; its dimmed blue keeps the wheel on blue. */
void EffectInstance_Test::aDimmerInTheHeadBreathesAndTheWheelHoldsItsColour()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("WheelSpot");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    QLCChannel *pan = new QLCChannel(); pan->setName("Pan"); pan->setPreset(QLCChannel::PositionPan);
    QLCChannel *wheel = new QLCChannel(); wheel->setName("Color wheel"); wheel->setGroup(QLCChannel::Colour);
    const struct { int lo, hi; QColor c; const char *n; } wheelSlots[] = {
        { 0, 9, QColor(0, 0, 0), "Off" }, { 10, 19, QColor(255, 0, 0), "Red" },
        { 20, 29, QColor(0, 0, 255), "Blue" }, { 30, 39, QColor(255, 255, 255), "White" } };
    for (const auto &sl : wheelSlots)
    {
        QLCCapability *cap = new QLCCapability(uchar(sl.lo), uchar(sl.hi), sl.n);
        cap->setResource(0, sl.c);
        wheel->addCapability(cap);
    }
    QLCChannel *dim = new QLCChannel(); dim->setName("Dimmer"); dim->setPreset(QLCChannel::IntensityDimmer);
    QList<QLCChannel *> chans = { pan, wheel, dim };
    for (int c = 0; c < chans.size(); c++)
    {
        def->addChannel(chans.at(c));
        mode->insertChannel(chans.at(c), c);
    }
    QLCFixtureHead head;
    head.addChannel(0); head.addChannel(1); head.addChannel(2);   // the dimmer is the head's
    mode->insertHead(-1, head);
    mode->cacheHeads();
    def->addMode(mode);
    Fixture *spot = new Fixture(m_doc);
    spot->setFixtureDefinition(def, mode);
    spot->setUniverse(3); spot->setAddress(100);
    QVERIFY(m_doc->addFixture(spot));
    QCOMPARE(spot->masterIntensityChannel(), QLCChannel::invalid());

    QLCPalette *breathe = new QLCPalette(QLCPalette::Effect, m_doc);
    breathe->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/dimmer-phaser.js");
    QVERIFY(m_doc->addPalette(breathe));
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixture(spot->id());
    scene->addPalette(breathe->id());
    EffectInstance inst(m_doc, scene->id(), breathe->id());
    QVERIFY(inst.isValid());
    int cols = 0, rows = 0;
    const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);
    QCOMPARE(cells.size(), 1);

    QJSEngine *js = inst.m_script.engine();
    QJSValue intents = js->newArray(1);
    QJSValue o = js->newObject();                    // what Breathe sends at 30 %: the look's
    o.setProperty("dimmer", 0.3);                    // blue, dimmed, and the dimmer
    o.setProperty("r", 0); o.setProperty("g", 0); o.setProperty("b", 76);
    intents.setProperty(0, o);
    int dimOut = -1, wheelOut = -1;
    foreach (const EffectInstance::DmxWrite &w, inst.parseIntents(intents, cells))
    {
        if (w.channel == 2) dimOut = w.value;
        if (w.channel == 1) wheelOut = w.value;
    }
    QVERIFY2(qAbs(dimOut - 77) <= 1, qPrintable(QString("the head's dimmer got %1").arg(dimOut)));
    QVERIFY2(wheelOut >= 20 && wheelOut <= 29,
             qPrintable(QString("a dim blue should keep the wheel on blue, not %1").arg(wheelOut)));
}

void EffectInstance_Test::aSharedDimmerTakesItsBrightestHead()
{
    /* A two-arm beam: one master dimmer outside its heads, an RGB set in
       each. Confetti lights one arm and leaves the other dark: the dimmer is
       the lit arm's, not the lit arm's times the dark one's (which was 0, and
       the fixture stayed black). */
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("TwoArmBeam");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    QLCChannel *dim = new QLCChannel(); dim->setName("Intensity"); dim->setPreset(QLCChannel::IntensityMasterDimmer);
    QList<QLCChannel *> chans = { dim };
    const QLCChannel::Preset rgb[] = { QLCChannel::IntensityRed, QLCChannel::IntensityGreen,
                                       QLCChannel::IntensityBlue };
    for (int h = 0; h < 2; h++)
        for (int k = 0; k < 3; k++)
        {
            QLCChannel *c = new QLCChannel();
            c->setName(QString("Colour %1.%2").arg(h).arg(k));
            c->setPreset(rgb[k]);
            chans << c;
        }
    for (int c = 0; c < chans.size(); c++)
    {
        def->addChannel(chans.at(c));
        mode->insertChannel(chans.at(c), c);
    }
    for (int h = 0; h < 2; h++)
    {
        QLCFixtureHead head;
        for (int k = 0; k < 3; k++)
            head.addChannel(quint32(1 + h * 3 + k));
        mode->insertHead(-1, head);
    }
    mode->cacheHeads();
    def->addMode(mode);
    Fixture *beam = new Fixture(m_doc);
    beam->setFixtureDefinition(def, mode);
    beam->setUniverse(0); beam->setAddress(20);
    QVERIFY(m_doc->addFixture(beam));
    QCOMPARE(beam->masterIntensityChannel(), quint32(0));
    QCOMPARE(beam->heads(), 2);

    QLCPalette *confetti = new QLCPalette(QLCPalette::Effect, m_doc);
    confetti->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/confetti.js");
    QVERIFY(m_doc->addPalette(confetti));
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixture(beam->id());
    scene->addPalette(confetti->id());
    EffectInstance inst(m_doc, scene->id(), confetti->id());
    QVERIFY(inst.isValid());
    int cols = 0, rows = 0;
    const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);
    QCOMPARE(cells.size(), 2);

    QJSEngine *js = inst.m_script.engine();
    for (int lit = 0; lit < 2; lit++)          // the lit arm first, then last
    {
        QJSValue intents = js->newArray(2);
        for (int h = 0; h < 2; h++)
        {
            const int i = (cells.at(h).head == lit) ? 200 : 0;
            QJSValue o = js->newObject();
            o.setProperty("r", i); o.setProperty("g", i); o.setProperty("b", i);
            o.setProperty("dimmer", i / 255.0);
            intents.setProperty(quint32(h), o);
        }
        const QList<EffectInstance::DmxWrite> out =
            EffectScriptRunner::composeWrites(inst.parseIntents(intents, cells));
        int n = 0, dimOut = -1;
        foreach (const EffectInstance::DmxWrite &w, out)
            if (w.fixtureId == beam->id() && w.channel == 0) { ++n; dimOut = w.value; }
        QCOMPARE(n, 1);
        QVERIFY2(qAbs(dimOut - 200) <= 1,
                 qPrintable(QString("arm %1 lit: the shared dimmer got %2").arg(lit).arg(dimOut)));
    }
}

void EffectInstance_Test::anEffectsDepthRampsInAndOut()
{
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixtureGroup(m_group->id());
    scene->addPalette(m_palette->id());
    EffectInstance inst(m_doc, scene->id(), m_palette->id());
    QVERIFY(!inst.hasDepthFade());
    QCOMPARE(inst.depth(), 1.0f);                   // no depth fade: full, as before

    inst.setDepthClock(0);
    inst.fadeDepthIn(1000, 0, true);
    QVERIFY(inst.hasDepthFade());
    const struct { qint64 at; float depth; } in[] = {
        { 0, 0.0f }, { 100, 0.1f }, { 250, 0.25f }, { 500, 0.5f }, { 900, 0.9f }, { 1000, 1.0f }, { 3000, 1.0f } };
    for (const auto &f : in)
    {
        inst.setDepthClock(f.at);
        QVERIFY2(qAbs(inst.depth() - f.depth) < 0.001f,
                 qPrintable(QString("at %1 ms: %2").arg(f.at).arg(inst.depth())));
    }
    QVERIFY(!inst.depthMoving());
    // Off the stage (the Blind view's own clock): as if its cue were up.
    inst.setDepthClock(250);
    inst.setOffStageClock(0);
    QCOMPARE(inst.depth(), 1.0f);
    inst.setOffStageClock(-1);
    QVERIFY(qAbs(inst.depth() - 0.25f) < 0.001f);

    inst.setDepthClock(3000);
    inst.startFadeOut(2000);
    QVERIFY(inst.isFadingOut());
    const struct { qint64 at; float depth; } out[] = {
        { 3000, 1.0f }, { 3500, 0.75f }, { 4000, 0.5f }, { 4600, 0.2f }, { 5000, 0.0f } };
    for (const auto &f : out)
    {
        inst.setDepthClock(f.at);
        QVERIFY2(qAbs(inst.depth() - f.depth) < 0.001f,
                 qPrintable(QString("at %1 ms: %2").arg(f.at).arg(inst.depth())));
        QCOMPARE(inst.fadeOutDone(), f.at >= 5000);
    }

    // The envelope stays full: the depth carries it.
    inst.setDepthClock(3500);
    inst.runTick();
    QCOMPARE(inst.m_envelope, 1.0f);

    // Started when its scene had been up a while: that much of the fade
    // has gone already.
    EffectInstance late(m_doc, scene->id(), m_palette->id());
    late.setDepthClock(10000);
    late.fadeDepthIn(1000, 400, true);
    QVERIFY(qAbs(late.depth() - 0.4f) < 0.001f);
    // No fade in: at full at once.
    EffectInstance snap(m_doc, scene->id(), m_palette->id());
    snap.fadeDepthIn(0, 0, true);
    QCOMPARE(snap.depth(), 1.0f);
}

void EffectInstance_Test::aReleaseMidFadeInFadesOutFromWhereItWas()
{
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixtureGroup(m_group->id());
    scene->addPalette(m_palette->id());
    EffectInstance inst(m_doc, scene->id(), m_palette->id());
    inst.setDepthClock(0);
    inst.fadeDepthIn(1000, 0, true);
    inst.setDepthClock(400);
    QVERIFY(qAbs(inst.depth() - 0.4f) < 0.001f);
    inst.startFadeOut(1000);                        // let go 40% of the way in
    for (const auto &f : { qMakePair(400, 0.4f), qMakePair(650, 0.3f), qMakePair(900, 0.2f),
                           qMakePair(1150, 0.1f), qMakePair(1400, 0.0f) })
    {
        inst.setDepthClock(f.first);
        QVERIFY2(qAbs(inst.depth() - f.second) < 0.001f,
                 qPrintable(QString("at %1 ms: %2, never back up to full").arg(f.first).arg(inst.depth())));
    }
    QVERIFY(inst.fadeOutDone());

    // Brought back while going: up again from where it had got to.
    EffectInstance back(m_doc, scene->id(), m_palette->id());
    back.setDepthClock(0);
    back.fadeDepthIn(0, 0, true);
    back.startFadeOut(1000);
    back.setDepthClock(700);
    QVERIFY(qAbs(back.depth() - 0.3f) < 0.001f);
    back.cancelFadeOut();
    back.fadeDepthIn(700, 0, false);
    QVERIFY(qAbs(back.depth() - 0.3f) < 0.001f);   // no dip to 0
    back.setDepthClock(1400);
    QCOMPARE(back.depth(), 1.0f);
}

/* A fixture with one Dimmer channel, a script that holds every fixture
   at half, and the runner that plays it: what the universe gets. */
static Fixture *oneDimmerFixture(Doc *doc, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Dimmer");
    def->setType(QLCFixtureDef::Dimmer);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    QLCChannel *dim = new QLCChannel(); dim->setName("Dimmer"); dim->setPreset(QLCChannel::IntensityDimmer);
    def->addChannel(dim);
    mode->insertChannel(dim, 0);
    QLCFixtureHead head;
    head.addChannel(0);
    mode->insertHead(-1, head);
    mode->cacheHeads();
    def->addMode(mode);
    Fixture *fx = new Fixture(doc);
    fx->setFixtureDefinition(def, mode);
    fx->setUniverse(0);
    fx->setAddress(address);
    doc->addFixture(fx);
    return fx;
}

static QString halfDimmerScript(const QTemporaryDir &dir)
{
    QFile js(dir.filePath("half.js"));
    if (!js.open(QIODevice::WriteOnly))
        return QString();
    js.write("(function() { var e = {}; e.apiVersion = 1; e.name = 'Half';"
             " e.tick = function(fixtures) { var out = [];"
             " for (var i = 0; i < fixtures.length; i++) out.push({ dimmer: 0.5 });"
             " return out; }; return e; })()");
    js.close();
    return js.fileName();
}

void EffectInstance_Test::aDimmerEffectFadesInAndOutFrameByFrame()
{
    Fixture *fx = oneDimmerFixture(m_doc, 100);
    QTemporaryDir dir;
    m_palette->setScriptPath(halfDimmerScript(dir));
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixture(fx->id());
    scene->addPalette(m_palette->id());
    scene->setPaletteFade(m_palette->id(), 1000, 1000);

    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    runner->createInstancesForScene(scene->id());
    EffectInstance *inst = runner->instanceForPalette(m_palette->id());
    QVERIFY(inst != nullptr);
    QVERIFY(inst->hasDepthFade());
    QCOMPARE(inst->m_depthMs, 1000u);
    // Its clock pinned: the fade began at 0.
    inst->setDepthClock(0);
    inst->m_depthStartMs = 0;

    /* What is under it: the look's own level, as the scene's fader would
       put it there (an Auto fader, before the effect's Override one). */
    MasterTimer timer(m_doc);
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    QSharedPointer<GenericFader> look = ua.first()->requestFader(Universe::Auto);
    m_doc->inputOutputMap()->releaseUniverses(false);
    auto setUnder = [&](int v) {
        FadeChannel fc(m_doc, fx->id(), 0);
        fc.setTarget(uint(v));
        fc.setCurrent(uint(v));
        fc.setFadeTime(0);
        look->replaceAll({ fc });
    };
    auto frame = [&](qint64 ms) {
        inst->setDepthClock(ms);
        inst->runTick();
        runner->publishWrites();
        QList<Universe *> u = m_doc->inputOutputMap()->claimUniverses();
        runner->writeDMX(&timer, u);
        u.first()->processFaders();
        const int v = uchar(u.first()->preGMValues().at(100));
        m_doc->inputOutputMap()->releaseUniverses(false);
        return v;
    };
    /* The look gives no level of its own (full, 255, the effect's base);
       at half the effect is 128. Part way in at depth d it is d of the
       way from what is under it to the look's level, times its dimmer at
       d of its depth: (u + d * (255 - u)) * (1 - d / 2). */
    auto expect = [](int under, float d) {
        return qRound((under + d * (255 - under)) * (1.0f - 0.5f * d));
    };
    setUnder(100);
    int last = -1;
    for (int ms = 0; ms <= 1000; ms += 100)
    {
        const float d = ms / 1000.0f;
        const int v = frame(ms);
        QVERIFY2(qAbs(v - expect(100, d)) <= 1,
                 qPrintable(QString("in, %1 ms: %2, wanted %3").arg(ms).arg(v).arg(expect(100, d))));
        last = v;
    }
    QCOMPARE(last, 128);                            // in: the effect alone, as without a fade
    QCOMPARE(frame(1500), 128);
    QCOMPARE(frame(0), 100);                        // depth 0: what is under it, untouched

    // Released: the look under it goes (its scene faded out), the effect
    // goes over the same time, down to nothing.
    frame(2000);
    runner->destroyInstancesForScene(scene->id(), true);
    QVERIFY(inst->isFadingOut());
    inst->m_depthStartMs = 2000;
    setUnder(0);
    for (int ms = 2000; ms <= 3000; ms += 100)
    {
        const float d = 1.0f - (ms - 2000) / 1000.0f;
        const int v = frame(ms);
        QVERIFY2(qAbs(v - expect(0, d)) <= 1,
                 qPrintable(QString("out, %1 ms: %2, wanted %3").arg(ms).arg(v).arg(expect(0, d))));
    }
    QVERIFY(inst->fadeOutDone());
    runner->slotTick();                             // reaped
    QVERIFY(runner->instanceForPalette(m_palette->id()) == nullptr);
    ua = m_doc->inputOutputMap()->claimUniverses();
    ua.first()->dismissFader(look);
    m_doc->inputOutputMap()->releaseUniverses(false);
}

void EffectInstance_Test::anEffectFollowsItsScenesFades()
{
    QTemporaryDir dir;
    m_palette->setScriptPath(halfDimmerScript(dir));
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixtureGroup(m_group->id());
    scene->addPalette(m_palette->id());
    scene->setPaletteFade(m_palette->id(), Scene::FadeFollow, Scene::FadeFollow);
    QCOMPARE(scene->paletteFadeIn(m_palette->id()), Scene::FadeFollow);
    QVERIFY(!scene->paletteFades().isEmpty());
    // As a cue starts it: its fade in and out.
    scene->m_overrideFadeInSpeed = 1500;
    scene->m_overrideFadeOutSpeed = 600;

    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    runner->createInstancesForScene(scene->id());
    EffectInstance *inst = runner->instanceForPalette(m_palette->id());
    QVERIFY(inst != nullptr);
    QVERIFY(inst->hasDepthFade());
    QCOMPARE(inst->m_depthMs, 1500u);
    QCOMPARE(inst->m_depthFrom, 0.0f);
    inst->setDepthClock(5000);                      // all the way in
    runner->destroyInstancesForScene(scene->id(), true);
    QVERIFY(inst->isFadingOut());
    QCOMPARE(inst->m_depthMs, 600u);
    QCOMPARE(inst->m_depthTo, 0.0f);

    // Back before it has gone: it comes up again from where it was.
    inst->setDepthClock(5150);
    runner->createInstancesForScene(scene->id());
    QCOMPARE(runner->instanceForPalette(m_palette->id()), inst);
    QVERIFY(!inst->isFadingOut());
    QCOMPARE(inst->m_depthTo, 1.0f);
    QVERIFY(qAbs(inst->m_depthFrom - 0.75f) < 0.001f);
    runner->destroyInstancesForScene(scene->id(), false);

    // Saved and loaded with the scene.
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    scene->saveXML(&w);
    buffer.close();
    QXmlStreamReader r(buffer.data());
    r.readNextStartElement();
    Scene back(m_doc);
    QVERIFY(back.loadXML(r));
    QCOMPARE(back.paletteFadeIn(m_palette->id()), Scene::FadeFollow);
    QCOMPARE(back.paletteFadeOut(m_palette->id()), Scene::FadeFollow);
}

void EffectInstance_Test::anEffectWithNoFadeTimesPlaysAsBefore()
{
    Fixture *fx = oneDimmerFixture(m_doc, 200);
    QTemporaryDir dir;
    m_palette->setScriptPath(halfDimmerScript(dir));
    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    MasterTimer timer(m_doc);
    auto play = [&](Scene *scene) {
        scene->m_running = true;                    // as if a cue had started it
        runner->createInstancesForScene(scene->id());
        EffectInstance *inst = runner->instanceForPalette(m_palette->id());
        if (inst == nullptr)
            return -1;
        inst->runTick();
        runner->publishWrites();
        for (const EffectInstance::DmxWrite &w : runner->m_publishedWrites)
            if (w.depth != 1.0f)
                return -2;
        QList<Universe *> u = m_doc->inputOutputMap()->claimUniverses();
        runner->writeDMX(&timer, u);
        u.first()->processFaders();
        const int v = uchar(u.first()->preGMValues().at(200));
        m_doc->inputOutputMap()->releaseUniverses(false);
        return v;
    };
    // A cue with no fade times: its effect follows them -- at once.
    Scene *follow = new Scene(m_doc);
    m_doc->addFunction(follow);
    follow->addFixture(fx->id());
    follow->addPalette(m_palette->id());
    follow->setPaletteFade(m_palette->id(), Scene::FadeFollow, Scene::FadeFollow);
    QCOMPARE(play(follow), 128);
    QCOMPARE(runner->instanceForPalette(m_palette->id())->depth(), 1.0f);
    runner->destroyInstancesForScene(follow->id(), true);
    follow->m_running = false;
    QVERIFY(runner->instanceForPalette(m_palette->id()) == nullptr);   // gone at once

    // An effect from before effect fades (none on its scene): the same.
    Scene *old = new Scene(m_doc);
    m_doc->addFunction(old);
    old->addFixture(fx->id());
    old->addPalette(m_palette->id());
    QVERIFY(old->paletteFades().isEmpty());
    QCOMPARE(play(old), 128);
    QVERIFY(!runner->instanceForPalette(m_palette->id())->hasDepthFade());
    runner->destroyInstancesForScene(old->id(), true);
    old->m_running = false;
    QVERIFY(runner->instanceForPalette(m_palette->id()) == nullptr);
}

void EffectInstance_Test::effectsPartWayInOrOutCompose()
{
    auto w = [](int value, bool replace, float depth, float scale = -1.0f, int base = 0) {
        EffectInstance::DmxWrite d;
        d.universeId = 0; d.addr = 1; d.value = uchar(value); d.fixtureId = 0;
        d.channel = 1; d.replace = replace; d.scale = scale; d.base = uchar(base); d.depth = depth;
        return d;
    };
    // At full depth: as before (depth 1, no mix).
    QList<EffectInstance::DmxWrite> out = EffectScriptRunner::composeWrites({ w(110, true, 1.0f, 0.5f, 220) });
    QCOMPARE(out.size(), 1);
    QCOMPARE(int(out.first().value), 110);
    QCOMPARE(out.first().depth, 1.0f);
    // A dimmer half in: half of its own depth, the channel half over what
    // is under it (the fader scales that by the same).
    out = EffectScriptRunner::composeWrites({ w(110, true, 0.5f, 0.5f, 220) });
    QCOMPARE(out.first().depth, 0.5f);
    QCOMPARE(out.first().scale, 0.75f);
    QCOMPARE(int(out.first().value), 165);
    // One dimmer going, another coming: each by as much of itself as is
    // in; the channel as far over what is under it as the most of them.
    out = EffectScriptRunner::composeWrites({ w(110, true, 0.25f, 0.5f, 220), w(55, true, 0.75f, 0.25f, 220) });
    QCOMPARE(out.size(), 1);
    QCOMPARE(out.first().depth, 0.75f);
    QVERIFY(qAbs(out.first().scale - 0.875f * 0.4375f) < 0.0001f);
    // One colour going, another coming: the later that far over the first.
    out = EffectScriptRunner::composeWrites({ w(200, true, 0.5f), w(40, true, 0.5f) });
    QCOMPARE(out.first().depth, 0.75f);
    QCOMPARE(int(out.first().value), qRound((200 * 0.5f * 0.5f + 40 * 0.5f) / 0.75f));
}

void EffectInstance_Test::effectsOnOneChannelLayerAsADeskDoes()
{
    auto w = [](int addr, int value, bool replace, float scale = -1.0f, int base = 0) {
        EffectInstance::DmxWrite d;
        d.universeId = 0; d.addr = addr; d.value = uchar(value); d.fixtureId = 0;
        d.channel = quint32(addr); d.replace = replace; d.scale = scale; d.base = uchar(base);
        return d;
    };
    auto valueAt = [](const QList<EffectInstance::DmxWrite> &out, int addr) {
        int n = 0, v = -1;
        foreach (const EffectInstance::DmxWrite &d, out)
            if (d.addr == addr) { ++n; v = d.value; }
        return n == 1 ? v : -100 - n;                // exactly one write per channel
    };
    // A colour, then a dimmer at half on a no-dimmer head: half the colour,
    // not half the look's (cyan's) value.
    QList<EffectInstance::DmxWrite> out = EffectScriptRunner::composeWrites(
        { w(1, 200, true), w(1, 110, true, 0.5f, 220) });
    QCOMPARE(valueAt(out, 1), 100);
    QVERIFY(out.first().replace);
    // The dimmer first, the colour after: the same.
    out = EffectScriptRunner::composeWrites({ w(1, 110, true, 0.5f, 220), w(1, 200, true) });
    QCOMPARE(valueAt(out, 1), 100);
    // A dimmer alone: the look's own value, scaled.
    out = EffectScriptRunner::composeWrites({ w(2, 110, true, 0.5f, 220) });
    QCOMPARE(valueAt(out, 2), 110);
    // Two dimmers: both.
    out = EffectScriptRunner::composeWrites({ w(2, 110, true, 0.5f, 220), w(2, 55, true, 0.25f, 220) });
    QCOMPARE(valueAt(out, 2), 28);
    // Two colours owning it: the later.
    out = EffectScriptRunner::composeWrites({ w(3, 200, true), w(3, 40, true) });
    QCOMPARE(valueAt(out, 3), 40);
    // Two that only add (a sparkle over a pulse): the higher.
    out = EffectScriptRunner::composeWrites({ w(4, 40, false), w(4, 90, false), w(5, 7, false) });
    QCOMPARE(valueAt(out, 4), 90);
    QCOMPARE(valueAt(out, 5), 7);
}

void EffectInstance_Test::aScriptsMissingTextIsEmptyNotUndefined()
{
    QTemporaryDir dir;
    QFile f(dir.filePath("bare.js"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("(function() { var e = {}; e.apiVersion = 1; e.name = 'Bare';"
            " e.tick = function(f) { return f.map(function() { return {}; }); };"
            " return e; })()");
    f.close();
    EffectScript script;
    QVERIFY(script.load(f.fileName()));
    QCOMPARE(script.name(), QString("Bare"));
    QVERIFY2(script.notes().isEmpty(), qPrintable(script.notes()));
    QVERIFY(script.description().isEmpty());
    QCOMPARE(script.syncTo(), QString("span"));
    QCOMPARE(script.lifecycle(), QString("loop"));
    QCOMPARE(script.onFinish(), QString("hold"));
}

/* A pixel fixture: @p rgbHeads heads of red, green and blue, then
   @p whiteHeads heads of white alone -- an LED strip, or a bar with both. */
static QLCFixtureDef *makePixelDef(const QString &model, int rgbHeads, int whiteHeads)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel(model);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("px");
    int n = 0;
    auto add = [&](QLCChannel::Preset preset) {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("c%1").arg(n));
        ch->setPreset(preset);
        def->addChannel(ch);
        mode->insertChannel(ch, quint32(n));
        return quint32(n++);
    };
    for (int h = 0; h < rgbHeads; ++h)
    {
        QLCFixtureHead head;
        head.addChannel(add(QLCChannel::IntensityRed));
        head.addChannel(add(QLCChannel::IntensityGreen));
        head.addChannel(add(QLCChannel::IntensityBlue));
        mode->insertHead(-1, head);
    }
    for (int h = 0; h < whiteHeads; ++h)
    {
        QLCFixtureHead head;
        head.addChannel(add(QLCChannel::IntensityWhite));
        mode->insertHead(-1, head);
    }
    mode->cacheHeads();
    def->addMode(mode);
    return def;
}

/* The step test card: each strip of a step counted and its last LED lit,
   the step's number as lines in its section's colour, its name in the
   middle; a dot running on the top bars. Worked out from the fixtures' names. */
void EffectInstance_Test::theStepTestPatternCountsNumbersAndNamesTheSteps()
{
    MasterTimer timer(m_doc);
    // A 64-pixel RGB strip, and a top bar of 16 RGB pixels then 16 white ones.
    QLCFixtureDef *stripDef = makePixelDef("Strip", 64, 0);
    QLCFixtureDef *topDef = makePixelDef("Top", 16, 16);
    m_doc->fixtureDefCache()->addFixtureDef(stripDef);
    m_doc->fixtureDefCache()->addFixtureDef(topDef);
    QList<Fixture *> fx;
    auto add = [&](QLCFixtureDef *def, const QString &name, quint32 uni, quint32 addr, float z) {
        Fixture *f = new Fixture(m_doc);
        f->setFixtureDefinition(def, def->modes().first());
        f->setName(name);
        f->setUniverse(uni);
        f->setAddress(addr);
        m_doc->addFixture(f);
        m_doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(2000.0f, 1000.0f, z));
        fx << f;
        return f;
    };
    Fixture *s1 = add(stripDef, "CS3 #1", 1, 0, 0.6f);      // the higher strip: the top row
    Fixture *s2 = add(stripDef, "CS3 #2", 1, 192, 0.3f);
    Fixture *top = add(topDef, "CS3t1", 2, 0, 0.8f);

    QLCPalette *card = new QLCPalette(QLCPalette::Effect, m_doc);
    card->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/step-test-pattern.js");
    QVERIFY(m_doc->addPalette(card));
    Scene *scene = new Scene(m_doc);
    for (Fixture *f : fx)
        scene->addFixture(f->id());
    scene->addPalette(card->id());
    m_doc->addFunction(scene);
    scene->start(&timer, FunctionParent::master());
    timer.timerTick();

    EffectInstance inst(m_doc, scene->id(), card->id());
    QVERIFY(inst.isValid());
    inst.runTick();
    QHash<quint64, int> v;              // (fixture, channel) -> value
    foreach (const EffectInstance::DmxWrite &w, inst.dmxWrites())
        v[(quint64(w.fixtureId) << 32) | w.channel] = w.value;
    auto rgb = [&](Fixture *f, int head) {
        const quint64 k = quint64(f->id()) << 32;
        return QList<int>() << v.value(k | quint32(head * 3)) << v.value(k | quint32(head * 3 + 1))
                            << v.value(k | quint32(head * 3 + 2));
    };
    auto isWhite = [&](Fixture *f, int head) { QList<int> c = rgb(f, head); return c[0] > 0 && c[0] == c[1] && c[1] == c[2]; };
    auto isOff = [&](Fixture *f, int head) { return rgb(f, head) == QList<int>({ 0, 0, 0 }); };
    auto isGreen = [&](Fixture *f, int head) { QList<int> c = rgb(f, head); return c[0] == 0 && c[1] > 0 && c[2] == 0; };

    // Strip 1: its first LED; strip 2: its first two. Every strip's last.
    QVERIFY(isWhite(s1, 0) && isOff(s1, 1));
    QVERIFY(isWhite(s2, 0) && isWhite(s2, 1) && isOff(s2, 2));
    QVERIFY(isWhite(s1, 63) && isWhite(s2, 63));
    // CS3: three green lines from the 10th LED, every other one.
    for (Fixture *f : { s1, s2 })
    {
        QVERIFY(isGreen(f, 9) && isGreen(f, 11) && isGreen(f, 13));
        QVERIFY(isOff(f, 10) && isOff(f, 12) && !isGreen(f, 15));
    }
    // "CS3" in the middle (17 wide from LED 24): C's top row is .###. -- on the top strip.
    QVERIFY(isOff(s1, 23) && isWhite(s1, 24) && isWhite(s1, 26) && isOff(s1, 27));
    // C's second row is #...# -- the second strip.
    QVERIFY(isWhite(s2, 23) && isOff(s2, 24) && isWhite(s2, 27));
    // The top: blue on its colour pixels only, never its white ones.
    int dots = 0;
    for (int h = 0; h < 16; ++h)
    {
        QList<int> c = rgb(top, h);
        QVERIFY(c[0] == 0 && c[1] == 0);
        if (c[2] > 0)
            ++dots;
    }
    QVERIFY(dots >= 1 && dots <= 3);
    for (int h = 0; h < 16; ++h)
        QCOMPARE(v.value((quint64(top->id()) << 32) | quint32(48 + h)), 0);
    scene->stop(FunctionParent::master());
    timer.timerTick();
}

/* What the script is handed is kept between ticks -- but a fixture renamed
   or moved is seen the next tick, and a script that reorders the list it is
   given does not reorder it for the next one. */
void EffectInstance_Test::theFixturesAScriptSeesFollowTheRig()
{
    MasterTimer timer(m_doc);
    QLCFixtureDef *def = makePixelDef("Probe strip", 4, 0);
    m_doc->fixtureDefCache()->addFixtureDef(def);
    Fixture *fx = new Fixture(m_doc);
    fx->setFixtureDefinition(def, def->modes().first());
    fx->setName("Strip");
    fx->setUniverse(3);
    fx->setAddress(0);
    QVERIFY(m_doc->addFixture(fx));
    m_doc->monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(500.0f, 0, 0));

    // Each head's red is its number; green says its name, blue where it is.
    // The list is reversed in place and the answers put back in its order.
    QTemporaryDir dir;
    QFile js(dir.filePath("probe.js"));
    QVERIFY(js.open(QIODevice::WriteOnly));
    js.write("(function() { var e = {}; e.apiVersion = 1; e.name = 'Probe';"
             " e.fixtureTypes = ['rgb'];"
             " e.tick = function(fixtures) { var n = fixtures.length; fixtures.reverse();"
             "  var out = new Array(n);"
             "  for (var i = 0; i < n; i++) { var f = fixtures[i];"
             "   out[n - 1 - i] = { r: f.head * 10 + 1, g: f.name === 'Moved' ? 200 : 100,"
             "                      b: f.pos.x > 1 ? 200 : 100 }; }"
             "  return out; };"
             " return e; })()");
    js.close();

    QLCPalette *probe = new QLCPalette(QLCPalette::Effect, m_doc);
    probe->setScriptPath(js.fileName());
    QVERIFY(m_doc->addPalette(probe));
    Scene *scene = new Scene(m_doc);
    scene->addFixture(fx->id());
    scene->addPalette(probe->id());
    m_doc->addFunction(scene);
    scene->start(&timer, FunctionParent::master());
    timer.timerTick();

    EffectInstance inst(m_doc, scene->id(), probe->id());
    QVERIFY(inst.isValid());
    auto frame = [&]() {
        inst.runTick();
        QHash<quint32, int> v;
        foreach (const EffectInstance::DmxWrite &w, inst.dmxWrites())
            v[w.channel] = w.value;
        return v;
    };
    for (int tick = 0; tick < 3; ++tick)
    {
        QHash<quint32, int> v = frame();
        for (int h = 0; h < 4; ++h)
            QVERIFY2(v.value(quint32(h * 3)) == h * 10 + 1,
                     qPrintable(QString("tick %1 head %2: red %3").arg(tick).arg(h).arg(v.value(quint32(h * 3)))));
        QCOMPARE(v.value(1), 100);
        QCOMPARE(v.value(2), 100);
    }

    fx->setName("Moved");
    m_doc->monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(3000.0f, 0, 0));
    QHash<quint32, int> v = frame();
    QCOMPARE(v.value(1), 200);           // the new name
    QCOMPARE(v.value(2), 200);           // where it is now
    QCOMPARE(v.value(9), 31);            // still in its own order

    scene->stop(FunctionParent::master());
    timer.timerTick();
}

/* A rig the size of a real one -- 96 strips of 64 pixels -- run tick after
   tick: the same picture each time, and how long a tick takes (reported, not
   checked: it is the number to watch when making this faster). */
void EffectInstance_Test::aWholeRigOfPixelsTicksQuickly()
{
    MasterTimer timer(m_doc);
    QLCFixtureDef *stripDef = makePixelDef("Rig strip", 64, 0);
    m_doc->fixtureDefCache()->addFixtureDef(stripDef);
    Scene *scene = new Scene(m_doc);
    static const char *sections[] = { "US", "CS", "DS" };
    for (int s = 0; s < 3; ++s)
        for (int step = 1; step <= 4; ++step)
            for (int strip = 1; strip <= 8; ++strip)
            {
                Fixture *f = new Fixture(m_doc);
                f->setFixtureDefinition(stripDef, stripDef->modes().first());
                f->setName(QString("%1%2 #%3").arg(sections[s]).arg(step).arg(strip));
                f->setUniverse(quint32((s * 4 + step - 1) * 4 + (strip - 1) / 2));
                f->setAddress((strip - 1) % 2 ? 192 : 0);
                m_doc->addFixture(f);
                m_doc->monitorProperties()->setFixturePosition(f->id(), 0, 0,
                    QVector3D(1000.0f * step, 2000.0f * s, 1000.0f - 100.0f * strip));
                scene->addFixture(f->id());
            }

    QLCPalette *card = new QLCPalette(QLCPalette::Effect, m_doc);
    card->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/step-test-pattern.js");
    QVERIFY(m_doc->addPalette(card));
    scene->addPalette(card->id());
    m_doc->addFunction(scene);
    scene->start(&timer, FunctionParent::master());
    timer.timerTick();

    EffectInstance inst(m_doc, scene->id(), card->id());
    QVERIFY(inst.isValid());
    inst.runTick();
    const QList<EffectInstance::DmxWrite> first = inst.dmxWrites();
    QVERIFY(first.size() > 96 * 3);

    const int ticks = qMax(20, qEnvironmentVariableIntValue("QLC_RIG_TICKS"));
    QElapsedTimer clock;
    clock.start();
    for (int i = 0; i < ticks; ++i)
        inst.runTick();
    const double ms = double(clock.nsecsElapsed()) / 1e6 / ticks;
    qInfo().noquote() << QString("96 strips x 64 pixels: %1 ms a tick").arg(ms, 0, 'f', 2);

    // The same picture -- but for the spinner beside each name (LEDs 42-49,
    // all 8 strips), which goes round: on every step, its dot and its tail.
    auto still = [](const QList<EffectInstance::DmxWrite> &writes) {
        QList<QPair<quint64, int>> out;
        for (const EffectInstance::DmxWrite &w : writes)
            if (w.channel < 42 * 3 || w.channel >= 50 * 3)
                out << qMakePair((quint64(w.fixtureId) << 32) | w.channel, int(w.value));
        return out;
    };
    const QList<EffectInstance::DmxWrite> last = inst.dmxWrites();
    QCOMPARE(still(last), still(first));
    QSet<QPair<quint32, quint32>> lit;
    for (const EffectInstance::DmxWrite &w : last)
        if (w.channel >= 42 * 3 && w.channel < 50 * 3 && w.value > 0)
            lit.insert(qMakePair(w.fixtureId, w.channel / 3));
    QHash<QString, int> perStep;
    for (const auto &c : lit)
        perStep[m_doc->fixture(c.first)->name().section(' ', 0, 0)]++;
    QCOMPARE(perStep.size(), 12);
    for (auto it = perStep.cbegin(); it != perStep.cend(); ++it)
        QVERIFY2(it.value() == 4, qPrintable(QString("%1: %2 spinner cells").arg(it.key()).arg(it.value())));
    scene->stop(FunctionParent::master());
    timer.timerTick();
}

/* The Text effect on a grid of 8 strips of 24: "HI" held still in the
   middle, dot for dot; scrolling, it starts off the right edge. */
void EffectInstance_Test::theTextEffectSpellsOnAGrid()
{
    MasterTimer timer(m_doc);
    QLCFixtureDef *stripDef = makePixelDef("Text strip", 24, 0);
    m_doc->fixtureDefCache()->addFixtureDef(stripDef);
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setSize(QSize(24, 8));
    QList<Fixture *> strips;
    for (int s = 0; s < 8; ++s)
    {
        Fixture *f = new Fixture(m_doc);
        f->setFixtureDefinition(stripDef, stripDef->modes().first());
        f->setName(QString("Sign #%1").arg(s + 1));
        f->setUniverse(4 + quint32(s / 4));
        f->setAddress(quint32((s % 4) * 72));
        QVERIFY(m_doc->addFixture(f));
        strips << f;
        for (int h = 0; h < 24; ++h)
            grp->assignHead(QLCPoint(h, s), GroupHead(f->id(), h));
    }
    m_doc->addFixtureGroup(grp);

    QLCPalette *text = new QLCPalette(QLCPalette::Effect, m_doc);
    text->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/text.js");
    text->setEffectStringParam("text", "HI");
    text->setEffectParamValue("style", 1);          // Still
    QVERIFY(m_doc->addPalette(text));
    Scene *scene = new Scene(m_doc);
    for (Fixture *f : strips)
        scene->addFixture(f->id());
    scene->addFixtureGroup(grp->id());
    scene->addPalette(text->id());
    m_doc->addFunction(scene);
    scene->start(&timer, FunctionParent::master());
    timer.timerTick();

    auto lit = [&](EffectInstance &inst) {
        inst.runTick();
        QSet<QPair<int, int>> on;            // (column, row)
        foreach (const EffectInstance::DmxWrite &w, inst.dmxWrites())
            if (w.channel % 3 == 0 && w.value > 0)
                on.insert(qMakePair(int(w.channel / 3), strips.indexOf(m_doc->fixture(w.fixtureId))));
        return on;
    };

    EffectInstance still(m_doc, scene->id(), text->id());
    QVERIFY2(still.isValid(), qPrintable(still.error()));
    const QSet<QPair<int, int>> on = lit(still);
    // "HI" is 11 dots wide: from column 6. H's uprights, its bar on row 3.
    for (int row = 0; row < 7; ++row)
    {
        QVERIFY2(on.contains(qMakePair(6, row)), qPrintable(QString("H left, row %1").arg(row)));
        QVERIFY(on.contains(qMakePair(10, row)));
        QVERIFY(on.contains(qMakePair(14, row)));       // I's stem
    }
    QVERIFY(on.contains(qMakePair(8, 3)));
    QVERIFY(!on.contains(qMakePair(8, 2)));
    QVERIFY(on.contains(qMakePair(13, 0)) && on.contains(qMakePair(13, 6)) && !on.contains(qMakePair(13, 3)));
    QVERIFY(!on.contains(qMakePair(5, 3)) && !on.contains(qMakePair(17, 3)));
    for (int col = 0; col < 24; ++col)
        QVERIFY(!on.contains(qMakePair(col, 7)));        // 7 rows tall: the 8th is clear
    QCOMPARE(on.size(), (7 + 3 + 7) + (2 + 7 + 2));      // H, I

    // Scrolling, slowly: it comes in from the right, so nothing shows yet.
    text->setEffectParamValue("style", 0);
    text->setEffectParamValue("speed", 1);
    EffectInstance scroll(m_doc, scene->id(), text->id());
    QVERIFY(lit(scroll).isEmpty());

    scene->stop(FunctionParent::master());
    timer.timerTick();
}

/*****************************************************************************
 * Matrix effects
 *****************************************************************************/

namespace
{
const QStringList kMatrixScripts = {
    "matrix-bars.js", "matrix-box-chase.js", "matrix-checker.js", "matrix-chevrons.js",
    "matrix-eq.js", "matrix-iris.js", "matrix-noise.js", "matrix-radar.js", "matrix-rain.js",
    "matrix-ripple.js", "matrix-snake.js", "matrix-sparkle.js", "matrix-strobe.js", "matrix-wipe.js",
    // The second set
    "matrix-balls.js", "matrix-dissolve.js", "matrix-fire.js", "matrix-fireworks.js",
    "matrix-gradient.js", "matrix-image.js", "matrix-kaleidoscope.js", "matrix-lissajous.js",
    "matrix-scanner.js", "matrix-spiral.js", "matrix-starfield.js", "matrix-vu.js"
};
/* The matrix effects with a scope: Whole canvas, Each box, Each box staggered. */
const QStringList kScopedMatrixScripts = {
    "matrix-bars.js", "matrix-checker.js", "matrix-chevrons.js", "matrix-eq.js", "matrix-iris.js",
    "matrix-radar.js", "matrix-ripple.js", "matrix-snake.js", "matrix-wipe.js",
    "matrix-balls.js", "matrix-dissolve.js", "matrix-fire.js", "matrix-fireworks.js",
    "matrix-gradient.js", "matrix-image.js", "matrix-kaleidoscope.js", "matrix-lissajous.js",
    "matrix-scanner.js", "matrix-spiral.js", "matrix-starfield.js", "matrix-vu.js"
};

/* The Warmdance XL-450 in its 165ch mode: 45 RGB heads, then 30 white, on a
   bar built in rows of fifteen, RGB / W / RGB / W / RGB. */
QPoint xl165Cell(int head)
{
    if (head < 45)
        return QPoint(head % 15, (head / 15) * 2);
    const int w = head - 45;
    return QPoint(w % 15, 1 + (w / 15) * 2);
}

/* One head as a script sees it, for the frame-by-frame checks. */
struct MatrixHead
{
    int id = 0, head = 0, col = 0, row = 0;
    bool rgb = true, white = false, dimmer = false;
};
struct MatrixLayout
{
    QString name;
    int cols = 0, rows = 0;
    QList<MatrixHead> heads;
};

MatrixLayout matrixLayout(const QString &name)
{
    MatrixLayout l;
    l.name = name;
    auto box = [&l](int id, int x0, int mode) {
        const int n = mode == 165 ? 75 : mode == 135 ? 45 : 30;
        for (int h = 0; h < n; ++h)
        {
            MatrixHead m;
            m.id = id;
            m.head = h;
            QPoint at = mode == 165 ? xl165Cell(h) : QPoint(h % 15, h / 15);
            m.col = x0 + at.x();
            m.row = at.y();
            m.rgb = mode == 135 || (mode == 165 && h < 45);
            m.white = mode == 30 || (mode == 165 && h >= 45);
            m.dimmer = mode != 135;         // a white channel reads as a dimmer
            l.heads << m;
        }
    };
    if (name == "box")        { box(1, 0, 165); l.cols = 15; l.rows = 5; }
    else if (name == "rig")   { for (int b = 0; b < 8; ++b) box(10 + b, b * 15, 165); l.cols = 120; l.rows = 5; }
    else if (name == "rgb")   { box(1, 0, 135); l.cols = 15; l.rows = 3; }
    else if (name == "white") { box(1, 0, 30); l.cols = 15; l.rows = 2; }
    else if (name == "matrix")
    {
        // A plain RGB matrix: eight strips of sixteen.
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 16; ++c)
            {
                MatrixHead m;
                m.id = 100 + r; m.head = c; m.col = c; m.row = r;
                l.heads << m;
            }
        l.cols = 16; l.rows = 8;
    }
    else if (name == "ragged")
    {
        // Cells past the grid the heads claim: a 15 x 2 box on a 10 x 1
        // grid (a layout edited under a running effect, or a host's slip).
        for (int h = 0; h < 30; ++h)
        {
            MatrixHead m;
            m.id = 1; m.head = h; m.col = h % 15; m.row = h / 15;
            l.heads << m;
        }
        l.cols = 10; l.rows = 1;
    }
    else
    {
        // Sixteen single-head lights in a row: the effect browser's preview.
        for (int c = 0; c < 16; ++c)
        {
            MatrixHead m;
            m.id = c; m.col = c; m.dimmer = true;
            l.heads << m;
        }
        l.cols = 16; l.rows = 1;
    }
    return l;
}

/* One frame of @p es on @p l at @p t: each head's intent as "r,g,b,w,d"
   (a dash for a key it leaves out), or an empty list if any is out of range
   or the script failed. */
/* Changes to one frame's inputs, settings and palettes before the tick. */
using FrameTweak = std::function<void(QJSEngine *, QJSValue &inputs, QJSValue &params, QJSValue &palettes)>;

QStringList matrixFrame(EffectScript &es, const MatrixLayout &l, double t, int white,
                        const QMap<QString, double> &extra = QMap<QString, double>(),
                        const FrameTweak &tweak = FrameTweak())
{
    QJSEngine *eng = es.engine();
    QJSValue fixtures = eng->newArray(uint(l.heads.size()));
    for (int i = 0; i < l.heads.size(); ++i)
    {
        const MatrixHead &m = l.heads.at(i);
        QJSValue o = eng->newObject();
        o.setProperty("id", m.id);
        o.setProperty("head", m.head);
        o.setProperty("hasRGB", m.rgb || m.white);      // the fixture's, not the head's
        o.setProperty("hasDimmer", m.dimmer);
        o.setProperty("headRGB", m.rgb);
        o.setProperty("headWhite", m.white);
        o.setProperty("phase", 0);
        QJSValue g = eng->newObject();
        g.setProperty("col", m.col); g.setProperty("row", m.row);
        g.setProperty("cols", l.cols); g.setProperty("rows", l.rows);
        o.setProperty("grid", g);
        fixtures.setProperty(uint(i), o);
    }
    QJSValue inputs = eng->newObject();
    inputs.setProperty("_time", t);
    inputs.setProperty("_bpm", 120);
    inputs.setProperty("_beatPos", t * 2);
    inputs.setProperty("_beat", t * 2 - std::floor(t * 2));
    inputs.setProperty("_beatCount", int(std::floor(t * 2)));
    QJSValue params = eng->newObject();
    for (const EffectScript::ParamDef &d : es.paramDefs())
        params.setProperty(d.name, double(d.defaultValue));
    params.setProperty("white", white);
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
        params.setProperty(it.key(), it.value());
    QJSValue palettes = eng->newObject();
    QJSValue look = eng->newObject();
    look.setProperty("colors", eng->newArray(0));
    palettes.setProperty("look", look);
    if (tweak)
        tweak(eng, inputs, params, palettes);
    QJSValue state = es.newState();
    const QJSValue r = es.callTick(fixtures, inputs, palettes, params, state, eng->newObject());
    if (!r.isArray() || r.property("length").toInt() != l.heads.size())
        return QStringList();
    QStringList out;
    for (int i = 0; i < l.heads.size(); ++i)
    {
        const QJSValue o = r.property(uint(i));
        QStringList parts;
        for (const char *k : { "r", "g", "b", "w" })
        {
            const QJSValue v = o.property(k);
            if (v.isUndefined()) { parts << "-"; continue; }
            const double n = v.toNumber();
            if (!(n >= 0 && n <= 255) || n != std::floor(n))
                return QStringList();
            parts << QString::number(int(n));
        }
        const QJSValue d = o.property("dimmer");
        if (!d.isUndefined() && !(d.toNumber() >= 0 && d.toNumber() <= 1))
            return QStringList();
        parts << (d.isUndefined() ? QString("-") : QString::number(d.toNumber(), 'f', 3));
        out << parts.join(',');
    }
    return out;
}

bool headLit(const QString &intent)
{
    const QStringList p = intent.split(',');
    for (int k = 0; k < 4; ++k)
        if (p.at(k) != "-" && p.at(k).toInt() > 0)
            return true;
    return p.at(4) != "-" && p.at(4).toDouble() > 0;
}
}

/* Heads past the grid they claim: every matrix effect, in every scope,
   still answers each head (Lissajous threw on them: "Cannot read property
   'r' of undefined"), and Lissajous lights the ones its figure crosses. */
void EffectInstance_Test::matrixEffectsTakeCellsOffTheGrid()
{
    const QString dir = QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/";
    const MatrixLayout l = matrixLayout("ragged");
    const QList<double> times = { 0.0, 0.37, 0.71, 1.1, 1.9, 2.6, 3.3, 4.05 };
    for (const QString &file : kMatrixScripts)
    {
        EffectScript es;
        QVERIFY2(es.load(dir + file), qPrintable(file + ": " + es.lastError()));
        bool offGridLit = false;
        for (int scope = 0; scope < 3; ++scope)
            for (double t : times)
            {
                const QStringList f = matrixFrame(es, l, t, 0, { { "scope", scope } });
                QVERIFY2(f.size() == l.heads.size(),
                         qPrintable(QString("%1 scope %2 t %3: no frame").arg(file).arg(scope).arg(t)));
                for (int i = 0; i < l.heads.size(); ++i)
                    if (l.heads.at(i).col >= l.cols || l.heads.at(i).row >= l.rows)
                        offGridLit = offGridLit || headLit(f.at(i));
            }
        if (file == "matrix-lissajous.js")
            QVERIFY2(offGridLit, "Lissajous: no head off the grid ever lit");
    }
}

void EffectInstance_Test::matrixEffectsAreRepeatableAndKeepTheirWhites()
{
    const QString dir = QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/";
    const QStringList layouts = { "box", "rig", "rgb", "white", "matrix", "strip" };
    const QList<double> times = { 0.0, 0.13, 0.37, 0.71, 1.1, 1.9, 2.6, 4.05 };
    QHash<QString, MatrixLayout> built;
    for (const QString &n : layouts)
        built.insert(n, matrixLayout(n));

    for (const QString &file : kMatrixScripts)
    {
        EffectScript es;
        QVERIFY2(es.load(dir + file), qPrintable(file + ": " + es.lastError()));
        QVERIFY2(es.fixtureTypes().contains("matrix"), qPrintable(file));
        QVERIFY2(EffectScriptCache::categoryForTypes(es.fixtureTypes()) == "Matrix", qPrintable(file));
        bool hasWhite = false;
        for (const EffectScript::ParamDef &d : es.paramDefs())
            hasWhite = hasWhite || (d.name == "white" && d.enumValues.size() == 5);
        QVERIFY2(hasWhite, qPrintable(file + ": no white setting"));

        for (const QString &ln : layouts)
        {
            const MatrixLayout &l = built.value(ln);
            for (int white = 0; white < 5; ++white)
            {
                QSet<QString> seen;
                bool whiteLit = false, anyLit = false;
                for (double t : times)
                {
                    const QStringList f = matrixFrame(es, l, t, white);
                    const QString where = QString("%1 on %2, white %3, t %4").arg(file, ln).arg(white).arg(t);
                    QVERIFY2(f.size() == l.heads.size(), qPrintable(where + ": failed or out of range"));
                    seen.insert(f.join(' '));
                    for (int i = 0; i < f.size(); ++i)
                    {
                        const MatrixHead &m = l.heads.at(i);
                        const QStringList p = f.at(i).split(',');
                        anyLit = anyLit || headLit(f.at(i));
                        if (!m.rgb && m.white)
                        {
                            // A white pixel is only ever told white.
                            QVERIFY2(p.at(0) == "-" && p.at(1) == "-" && p.at(2) == "-", qPrintable(where));
                            if (p.at(3) != "-" && p.at(3).toInt() > 0)
                                whiteLit = true;
                            if (white == 3 && ln == "box")
                                QVERIFY2(p.at(3) == "-", qPrintable(where + ": RGB only, white lit"));
                        }
                        if (m.rgb && !m.white)
                        {
                            // An RGB pixel never gets white (it would black its colour out).
                            QVERIFY2(p.at(3) == "-", qPrintable(where + ": w on an RGB pixel"));
                            if (white == 4 && (ln == "box" || ln == "rig"))
                                QVERIFY2(f.at(i) == "-,-,-,-,-", qPrintable(where + ": White only, RGB lit"));
                            if (white == 4 && ln == "rgb" && headLit(f.at(i)))
                                QVERIFY2(p.at(0) == p.at(1) && p.at(1) == p.at(2),
                                         qPrintable(where + ": White only with no white pixels is grey"));
                        }
                    }
                    // The same moment, the same frame.
                    QCOMPARE(matrixFrame(es, l, t, white), f);
                }
                const QString where = QString("%1 on %2, white %3").arg(file, ln).arg(white);
                QVERIFY2(seen.size() > 1, qPrintable(where + ": never moves"));
                if (white != 4 || ln == "rgb" || ln == "white")
                    QVERIFY2(anyLit, qPrintable(where + ": dark"));
                // A box in its white-only mode still shows the picture.
                if (ln == "white" && white != 2)
                    QVERIFY2(whiteLit, qPrintable(where + ": white-only box stays dark"));
                if ((ln == "box" || ln == "rig") && (white == 1 || white == 4))
                    QVERIFY2(whiteLit, qPrintable(where + ": white pixels never lit"));
            }
        }
    }

    /* Still frames of the effects with no randomness, at their defaults on
       one box: a change to one of these is a change in what it draws. */
    const QList<QPair<QString, QString>> stills = {
        { "matrix-bars.js",     "837b8c514b9c" },
        { "matrix-chevrons.js", "e004b4669ea3" },
        { "matrix-checker.js",  "1bf4d3b3de4d" },
        { "matrix-snake.js",    "5e9f31b83655" },
        { "matrix-wipe.js",     "4f878342656b" }
    };
    const MatrixLayout box = built.value("box");
    QStringList drifted;
    for (const auto &st : stills)
    {
        EffectScript es;
        QVERIFY(es.load(dir + st.first));
        QStringList all;
        for (double t : { 0.37, 1.1, 2.6 })
            all << matrixFrame(es, box, t, 0);
        const QString sum = QString::fromLatin1(
            QCryptographicHash::hash(all.join('|').toUtf8(), QCryptographicHash::Md5).toHex().left(12));
        if (sum != st.second)
            drifted << QString("%1 is %2, was %3").arg(st.first, sum, st.second);
    }
    QVERIFY2(drifted.isEmpty(), qPrintable(drifted.join("; ")));
}

void EffectInstance_Test::matrixEffectsRunOnBoxesAndTheirModes()
{
    MasterTimer timer(m_doc);
    QLCFixtureDef *def165 = makePixelDef("XL 165", 45, 30);
    QLCFixtureDef *def135 = makePixelDef("XL 135", 45, 0);
    QLCFixtureDef *def30 = makePixelDef("XL 30", 0, 30);
    QLCFixtureDef *strip = makePixelDef("Strip 16", 16, 0);
    for (QLCFixtureDef *d : { def165, def135, def30, strip })
        m_doc->fixtureDefCache()->addFixtureDef(d);

    struct Rig
    {
        QString name;
        QLCFixtureDef *def;
        int count;
        QSize size;
    };
    const QList<Rig> rigs = {
        { "box", def165, 1, QSize(15, 5) },
        { "rig", def165, 8, QSize(120, 5) },
        { "rgb", def135, 1, QSize(15, 3) },
        { "white", def30, 1, QSize(15, 2) },
        { "matrix", strip, 8, QSize(16, 8) }
    };
    quint32 uni = 1;
    for (const Rig &rig : rigs)
    {
        FixtureGroup *grp = new FixtureGroup(m_doc);
        grp->setSize(rig.size);
        QList<Fixture *> fx;
        const int chans = rig.def->modes().first()->channels().size();
        int addr = 0;
        for (int b = 0; b < rig.count; ++b)
        {
            if (addr + chans > 512) { ++uni; addr = 0; }
            Fixture *f = new Fixture(m_doc);
            f->setFixtureDefinition(rig.def, rig.def->modes().first());
            f->setName(QString("%1 #%2").arg(rig.name).arg(b + 1));
            f->setUniverse(uni);
            f->setAddress(quint32(addr));
            addr += chans;
            QVERIFY(m_doc->addFixture(f));
            fx << f;
            for (int h = 0; h < f->heads(); ++h)
            {
                QPoint at;
                if (rig.name == "matrix") at = QPoint(h, b);
                else if (rig.def == def165) at = xl165Cell(h) + QPoint(b * 15, 0);
                else at = QPoint(h % 15, h / 15);
                grp->assignHead(QLCPoint(at.x(), at.y()), GroupHead(f->id(), h));
            }
        }
        ++uni;
        m_doc->addFixtureGroup(grp);
        Scene *scene = new Scene(m_doc);
        scene->setName(rig.name);
        scene->addFixtureGroup(grp->id());
        for (Fixture *f : fx)
            scene->addFixture(f->id());
        m_doc->addFunction(scene);

        for (const QString &file : kMatrixScripts)
        {
            QLCPalette *pal = new QLCPalette(QLCPalette::Effect, m_doc);
            pal->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/" + file);
            QVERIFY(m_doc->addPalette(pal));
            scene->addPalette(pal->id());
            const QString where = rig.name + ": " + file;

            EffectInstance inst(m_doc, scene->id(), pal->id());
            QVERIFY2(inst.isValid(), qPrintable(where + ": " + inst.error()));
            int cols = 0, rows = 0;
            const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);
            QCOMPARE(cells.size(), rig.count * fx.first()->heads());
            QCOMPARE(QSize(cols, rows), rig.size);
            QJSValue fixtures = inst.buildFixturesArray(cells, cols, rows);
            // Each head says what it is.
            for (int i = 0; i < cells.size(); ++i)
            {
                const QJSValue o = fixtures.property(uint(i));
                const bool white = rig.def == def30 || (rig.def == def165 && cells.at(i).head >= 45);
                QCOMPARE(o.property("headWhite").toBool(), white);
                QCOMPARE(o.property("headRGB").toBool(), !white);
            }
            int lit = 0;
            for (double t : { 0.02, 0.4, 0.9, 1.6, 3.3 })
            {
                QJSValue inputs = inst.buildInputsObject();
                inputs.setProperty("_time", t);
                inputs.setProperty("_bpm", 0);
                QJSValue r = inst.m_script.callTick(fixtures, inputs, inst.buildPalettesObject(),
                                                    inst.buildParamsObject(), inst.m_state,
                                                    inst.buildDataChannelsObject());
                QVERIFY2(r.isArray(), qPrintable(where + ": its tick failed"));
                QCOMPARE(r.property("length").toInt(), cells.size());
                for (int i = 0; i < cells.size(); ++i)
                {
                    const QJSValue o = r.property(uint(i));
                    for (const char *k : { "r", "g", "b", "w" })
                        if (!o.property(k).isUndefined())
                        {
                            const double v = o.property(k).toNumber();
                            QVERIFY2(v >= 0 && v <= 255, qPrintable(where));
                            if (v > 0) ++lit;
                        }
                }
            }
            QVERIFY2(lit > 0, qPrintable(where + ": dark"));
            scene->removePalette(pal->id());
        }

        /* Through to DMX: a strobe hit, free running and just started, is
           full white on every emitter -- the RGB pixels' three and the white
           pixels' one -- and on nothing else. */
        QLCPalette *strobe = new QLCPalette(QLCPalette::Effect, m_doc);
        strobe->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/matrix-strobe.js");
        strobe->setEffectParamValue("timing", 1);
        strobe->setEffectParamValue("speed", 0.5);
        strobe->setEffectParamValue("length", 0.9);
        strobe->setEffectParamValue("decay", 0);
        QVERIFY(m_doc->addPalette(strobe));
        scene->addPalette(strobe->id());
        scene->start(&timer, FunctionParent::master());
        timer.timerTick();
        EffectInstance inst(m_doc, scene->id(), strobe->id());
        inst.runTick();
        QSet<QPair<quint32, quint32>> full;
        foreach (const EffectInstance::DmxWrite &w, inst.dmxWrites())
            if (w.value == 255)
                full.insert(qMakePair(w.fixtureId, w.channel));
        QCOMPARE(full.size(), rig.count * fx.first()->channels());
        scene->stop(FunctionParent::master());
        timer.timerTick();
    }
}

namespace
{
/* The new matrix effects. */
const QStringList kSecondMatrixScripts = {
    "matrix-balls.js", "matrix-dissolve.js", "matrix-fire.js", "matrix-fireworks.js",
    "matrix-gradient.js", "matrix-image.js", "matrix-kaleidoscope.js", "matrix-lissajous.js",
    "matrix-scanner.js", "matrix-spiral.js", "matrix-starfield.js", "matrix-vu.js"
};

/* A tiny animated GIF: three 4 x 2 frames, red for 100ms, green for 200ms,
   blue for 300ms. */
const char *kTinyGif =
    "R0lGODlhBAACAIEAAP8AAAAAAAAAAAAAACH/C05FVFNDQVBFMi4wAwEAAAAh+QQACgAAACwAAAAABAACAAAIBwABCBwoMCAAIfkEARQAAQAs"
    "AAAAAAQAAgCBAP8AAAAAAAAAAAAACAcAAQgcKDAgACH5BAEeAAEALAAAAAAEAAIAgQAA/wAAAAAAAAAAAAgHAAEIHCgwIAA7";

QString tinyGifSetting()
{
    QString why;
    const QString v = MatrixImage::fromBytes(QByteArray::fromBase64(kTinyGif), "tiny.gif", &why);
    return v;
}

/* Box @p b's heads' intents (in head order) from a frame on the rig. */
QStringList boxOf(const MatrixLayout &l, const QStringList &frame, int b)
{
    QStringList out;
    for (int i = 0; i < l.heads.size(); ++i)
        if (l.heads.at(i).id == 10 + b)
            out << frame.at(i);
    return out;
}

FrameTweak lookColours(const QList<QColor> &cols)
{
    return [cols](QJSEngine *eng, QJSValue &, QJSValue &, QJSValue &palettes) {
        QJSValue arr = eng->newArray(uint(cols.size()));
        for (int i = 0; i < cols.size(); ++i)
        {
            QJSValue c = eng->newObject();
            c.setProperty("r", cols.at(i).red());
            c.setProperty("g", cols.at(i).green());
            c.setProperty("b", cols.at(i).blue());
            arr.setProperty(uint(i), c);
        }
        QJSValue look = eng->newObject();
        look.setProperty("colors", arr);
        palettes.setProperty("look", look);
    };
}

FrameTweak withImage(const QString &setting)
{
    return [setting](QJSEngine *eng, QJSValue &, QJSValue &params, QJSValue &) {
        QSharedPointer<const MatrixImage::Frames> f = MatrixImage::decode(setting);
        params.setProperty("image", f.isNull() ? QJSValue(QJSValue::NullValue) : MatrixImage::toScript(eng, *f));
    };
}

/* An RGB head's "r,g,b" as numbers. */
QColor rgbOf(const QString &intent)
{
    const QStringList p = intent.split(',');
    auto v = [&p](int k) { return p.at(k) == "-" ? 0 : p.at(k).toInt(); };
    return QColor(v(0), v(1), v(2));
}
}

void EffectInstance_Test::matrixEffectsTakeTheLooksColoursAndKeepTime()
{
    const QString dir = QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/";
    const MatrixLayout box = matrixLayout("box");
    const QList<double> times = { 0.1, 0.45, 0.8, 1.3, 2.2, 3.7 };

    for (const QString &file : kSecondMatrixScripts)
    {
        EffectScript es;
        QVERIFY2(es.load(dir + file), qPrintable(file + ": " + es.lastError()));
        bool scoped = false;
        for (const EffectScript::ParamDef &d : es.paramDefs())
            if (d.name == "scope")
                scoped = d.enumValues.size() == 3;
        QVERIFY2(scoped, qPrintable(file + ": no Each box, staggered"));

        /* The look's red and green: every RGB pixel some mix of the two
           (the accents go to the white pixels); both of them seen. */
        QMap<QString, double> extra;
        if (file == "matrix-vu.js") { extra["colours"] = 1; extra["gain"] = 2; }   // the look's colours up a full meter
        if (file == "matrix-scanner.js") extra["count"] = 2;        // a bar a colour
        bool red = false, green = false;               // a pixel more red than green, and one more green
        for (double t : times)
        {
            const QStringList f = matrixFrame(es, box, t, 0, extra,
                                              lookColours({ QColor(255, 0, 0), QColor(0, 255, 0) }));
            QVERIFY2(f.size() == box.heads.size(), qPrintable(file));
            for (int i = 0; i < f.size(); ++i)
            {
                if (!box.heads.at(i).rgb)
                    continue;
                const QColor c = rgbOf(f.at(i));
                QVERIFY2(c.blue() == 0, qPrintable(QString("%1 at %2: blue in a red and green look (%3)")
                                                   .arg(file).arg(t).arg(f.at(i))));
                red = red || c.red() > c.green() + 20;
                green = green || c.green() > c.red() + 20;
            }
        }
        if (file != "matrix-image.js")                 // the heart is the first colour only
            QVERIFY2(red && green, qPrintable(file + ": not both of the look's colours"));
        else
            QVERIFY2(red, qPrintable(file + ": not the look's colour"));
    }

    /* Speed: twice the speed, half the time -- the same frame. */
    const QMap<QString, QMap<QString, double>> scaled = {
        { "matrix-balls.js", {} }, { "matrix-fire.js", {} }, { "matrix-starfield.js", {} },
        { "matrix-spiral.js", {} }, { "matrix-lissajous.js", {} }, { "matrix-kaleidoscope.js", { { "spin", 0 } } },
        { "matrix-scanner.js", {} }, { "matrix-dissolve.js", { { "timing", 1 } } }, { "matrix-gradient.js", {} },
        { "matrix-fireworks.js", { { "timing", 1 }, { "rise", 0 }, { "fade", 1 } } }
    };
    for (auto it = scaled.constBegin(); it != scaled.constEnd(); ++it)
    {
        EffectScript es;
        QVERIFY(es.load(dir + it.key()));
        double sp = 0.5;
        for (const EffectScript::ParamDef &d : es.paramDefs())
            if (d.name == "speed")
                sp = qMax(double(d.min), 0.4);
        for (double t : { 0.7, 1.9 })
        {
            QMap<QString, double> slow = it.value(), fast = it.value();
            slow["speed"] = sp;
            fast["speed"] = sp * 2;
            if (it.key() == "matrix-fireworks.js")
            {
                // Its rise and fade are seconds: halve them with the time.
                fast["fade"] = 0.5;
            }
            const QStringList a = matrixFrame(es, box, t * 2, 0, slow);
            const QStringList b = matrixFrame(es, box, t, 0, fast);
            QVERIFY2(!a.isEmpty() && a == b, qPrintable(it.key() + ": speed doesn't scale its time"));
        }
    }

    /* On the beat: the beat clock, not the time, says where it is; and a
       firework bursts on the beat -- its flash lights the white pixels. */
    for (const QString &file : { QString("matrix-dissolve.js"), QString("matrix-fireworks.js") })
    {
        EffectScript es;
        QVERIFY(es.load(dir + file));
        auto atBeat = [](double beatPos, double time) {
            return [beatPos, time](QJSEngine *, QJSValue &inputs, QJSValue &, QJSValue &) {
                inputs.setProperty("_time", time);
                inputs.setProperty("_beatPos", beatPos);
                inputs.setProperty("_beat", beatPos - std::floor(beatPos));
                inputs.setProperty("_beatCount", int(std::floor(beatPos)));
            };
        };
        const QMap<QString, double> beat = { { "timing", 0 } };
        QCOMPARE(matrixFrame(es, box, 0, 0, beat, atBeat(3.3, 1.0)),
                 matrixFrame(es, box, 0, 0, beat, atBeat(3.3, 7.0)));
        QVERIFY(matrixFrame(es, box, 0, 0, beat, atBeat(3.3, 1.0)) != matrixFrame(es, box, 0, 0, beat, atBeat(3.6, 1.0)));
        if (file == "matrix-fireworks.js")
        {
            auto whites = [&](double bp) {
                int n = 0;
                const QStringList f = matrixFrame(es, box, 0, 2, beat, atBeat(bp, 0));   // accents only
                for (int i = 0; i < f.size(); ++i)
                    if (box.heads.at(i).white && f.at(i).split(',').at(3).toInt() > 0)
                        ++n;
                return n;
            };
            // Every 2 beats: a burst on beat 4, nothing on 3.
            QVERIFY2(whites(4.03) > whites(3.0), qPrintable(QString("%1 vs %2").arg(whites(4.03)).arg(whites(3.0))));
        }
    }
}

void EffectInstance_Test::matrixEffectsStaggerBoxToBox()
{
    const QString dir = QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/";
    const MatrixLayout rig = matrixLayout("rig");
    const QString gif = tinyGifSetting();
    QVERIFY(!gif.isEmpty());

    for (const QString &file : kScopedMatrixScripts)
    {
        EffectScript es;
        QVERIFY2(es.load(dir + file), qPrintable(file));
        const FrameTweak tw = file == "matrix-image.js" ? withImage(gif) : FrameTweak();
        for (double t : { 0.9, 2.35 })
        {
            const QString where = QString("%1 at %2").arg(file).arg(t);
            QMap<QString, double> each = { { "scope", 1 } };
            const QStringList eachF = matrixFrame(es, rig, t, 0, each, tw);
            // No stagger is the same as Each box.
            QMap<QString, double> none = { { "scope", 2 }, { "stagger", 0 } };
            QCOMPARE(matrixFrame(es, rig, t, 0, none, tw), eachF);

            auto staggered = [&](int order, double off) {
                QMap<QString, double> m = { { "scope", 2 }, { "staggerOrder", order }, { "stagger", off } };
                return matrixFrame(es, rig, t, 0, m, tw);
            };
            const QStringList l2r = staggered(0, 0.3), r2l = staggered(1, 0.3);
            const QStringList mid = staggered(2, 0.3), rnd = staggered(3, 0.3);
            QVERIFY2(l2r.size() == rig.heads.size(), qPrintable(where));
            // The first box goes as it would on its own; the last, right to left.
            QVERIFY2(boxOf(rig, l2r, 0) == boxOf(rig, eachF, 0), qPrintable(where + ": first box lags"));
            QVERIFY2(boxOf(rig, r2l, 7) == boxOf(rig, eachF, 7), qPrintable(where + ": right to left"));
            // Out from the middle: the middle two lead, and mirror boxes go together.
            QVERIFY2(boxOf(rig, mid, 3) == boxOf(rig, eachF, 3), qPrintable(where + ": middle"));
            QVERIFY2(boxOf(rig, mid, 4) == boxOf(rig, eachF, 4), qPrintable(where + ": middle"));

            bool alike = true;                         // Each box: every box the same?
            for (int b = 1; b < 8; ++b)
                alike = alike && boxOf(rig, eachF, b) == boxOf(rig, eachF, 0);
            if (!alike)
                continue;                              // boxes of their own (a meter a band)
            QVERIFY2(boxOf(rig, mid, 2) == boxOf(rig, mid, 5), qPrintable(where + ": mirror boxes"));
            QVERIFY2(boxOf(rig, l2r, 1) == boxOf(rig, r2l, 6), qPrintable(where + ": second from each end"));
            // Random: the same pictures, shuffled.
            QStringList a, b;
            for (int k = 0; k < 8; ++k)
            {
                a << boxOf(rig, l2r, k).join(' ');
                b << boxOf(rig, rnd, k).join(' ');
            }
            QSet<QString> sa(a.begin(), a.end()), sb(b.begin(), b.end());
            QVERIFY2(sa == sb, qPrintable(where + ": random isn't the same pictures"));
            if (sa.size() > 2)
                QVERIFY2(a != b, qPrintable(where + ": random is left to right"));
        }
    }

    /* The lag itself: a free ripple, a ring a second, a quarter of a ring
       a box -- box 3 shows now what box 0 showed three quarters of a
       second ago. A radar at half a turn a second: an eighth of a turn,
       a quarter second, a box. */
    struct Lag { QString file; QMap<QString, double> set; double secondsPerBox; };
    const QList<Lag> lags = {
        { "matrix-ripple.js", { { "timing", 1 }, { "speed", 1 }, { "stagger", 0.25 } }, 0.25 },
        { "matrix-radar.js", { { "speed", 0.5 }, { "stagger", 0.125 } }, 0.25 },
        { "matrix-fire.js", { { "speed", 2 }, { "stagger", 0.5 } }, 0.25 },
        { "matrix-spiral.js", { { "speed", 0.25 }, { "stagger", 0.25 } }, 1.0 }
    };
    for (const Lag &lag : lags)
    {
        EffectScript es;
        QVERIFY(es.load(dir + lag.file));
        QMap<QString, double> st = lag.set, each = lag.set;
        st["scope"] = 2;
        st["staggerOrder"] = 0;
        each["scope"] = 1;
        const double t = 5.0;
        const QStringList f = matrixFrame(es, rig, t, 0, st);
        for (int b = 0; b < 8; ++b)
            QVERIFY2(boxOf(rig, f, b) == boxOf(rig, matrixFrame(es, rig, t - b * lag.secondsPerBox, 0, each), b),
                     qPrintable(QString("%1: box %2").arg(lag.file).arg(b)));
    }
}

void EffectInstance_Test::matrixImagePlaysAGifKeptInTheShow()
{
    MatrixImage::clearCache();
    const QString dir = QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/";
    const QString gif = tinyGifSetting();
    QVERIFY(MatrixImage::isImage(gif));
    QVERIFY(gif.startsWith("data:image/gif;name=tiny.gif;base64,"));

    // Its frames: three, 4 x 2, at their own times.
    QSharedPointer<const MatrixImage::Frames> f = MatrixImage::decode(gif);
    QVERIFY(!f.isNull());
    QCOMPARE(f->rgba.size(), 3);
    QCOMPARE(QSize(f->width, f->height), QSize(4, 2));
    QCOMPARE(f->delaysMs, QVector<int>({ 100, 200, 300 }));
    QCOMPARE(f->totalMs, 600);
    QCOMPARE(f->rgba.at(1).size(), 4 * 2 * 4);
    QCOMPARE(quint8(f->rgba.at(1).at(1)), quint8(255));            // frame 2 is green
    QVERIFY(MatrixImage::describe(gif).contains("3 frames"));
    QCOMPARE(MatrixImage::decode(gif).data(), f.data());           // decoded once

    // What it won't take.
    QString why;
    QVERIFY(MatrixImage::fromBytes(QByteArray(MatrixImage::kMaxFileBytes + 1, 'x'), "big.gif", &why).isEmpty());
    QVERIFY(why.contains("KB"));
    QVERIFY(MatrixImage::fromBytes("not a picture", "x.png", &why).isEmpty());
    QVERIFY(MatrixImage::decode("hello").isNull());

    // A big picture comes down to kMaxSide.
    QImage big(800, 400, QImage::Format_RGB32);
    big.fill(Qt::blue);
    QByteArray png;
    QBuffer pb(&png);
    pb.open(QIODevice::WriteOnly);
    big.save(&pb, "PNG");
    QSharedPointer<const MatrixImage::Frames> bf = MatrixImage::decode(MatrixImage::fromBytes(png, "big.png"));
    QVERIFY(!bf.isNull());
    QCOMPARE(QSize(bf->width, bf->height), QSize(MatrixImage::kMaxSide, MatrixImage::kMaxSide / 2));
    QCOMPARE(bf->rgba.size(), 1);

    /* Played on a box: red, green, blue at their own times, round again;
       twice as fast, twice as soon; a frame a beat on the beat. */
    EffectScript es;
    QVERIFY(es.load(dir + "matrix-image.js"));
    const MatrixLayout box = matrixLayout("box");
    auto colourAt = [&](double t, const QMap<QString, double> &set) {
        const QStringList fr = matrixFrame(es, box, t, 0, set, withImage(gif));
        // The middle RGB head: row 2, column 7.
        for (int i = 0; i < box.heads.size(); ++i)
            if (box.heads.at(i).rgb && box.heads.at(i).col == 7 && box.heads.at(i).row == 2)
                return rgbOf(fr.at(i));
        return QColor();
    };
    const QMap<QString, double> own = { { "fit", 2 } };
    QCOMPARE(colourAt(0.05, own), QColor(255, 0, 0));
    QCOMPARE(colourAt(0.15, own), QColor(0, 255, 0));
    QCOMPARE(colourAt(0.35, own), QColor(0, 0, 255));
    QCOMPARE(colourAt(0.65, own), QColor(255, 0, 0));
    QCOMPARE(colourAt(0.075, { { "fit", 2 }, { "speed", 2 } }), QColor(0, 255, 0));
    // On the beat (120bpm, a frame a beat): beat 4 is the second frame of the round.
    QCOMPARE(colourAt(2.1, { { "fit", 2 }, { "timing", 1 }, { "division", 2 } }), QColor(0, 255, 0));

    // Fit letterboxes (4 x 2 into 15 x 5 is 10 x 5, in the middle); fill and stretch fill.
    auto edgeLit = [&](int fit) {
        const QStringList fr = matrixFrame(es, box, 0.05, 0, { { "fit", fit } }, withImage(gif));
        for (int i = 0; i < box.heads.size(); ++i)
            if (box.heads.at(i).rgb && box.heads.at(i).col == 0 && box.heads.at(i).row == 0)
                return headLit(fr.at(i));
        return false;
    };
    QVERIFY(!edgeLit(0));
    QVERIFY(edgeLit(1));
    QVERIFY(edgeLit(2));

    /* Kept in the show: through a look's save and load, and out on DMX. */
    MasterTimer timer(m_doc);
    QLCFixtureDef *def = makePixelDef("XL image", 45, 30);
    m_doc->fixtureDefCache()->addFixtureDef(def);
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setSize(QSize(15, 5));
    Fixture *fx = new Fixture(m_doc);
    fx->setFixtureDefinition(def, def->modes().first());
    fx->setUniverse(1);
    fx->setAddress(0);
    QVERIFY(m_doc->addFixture(fx));
    for (int h = 0; h < fx->heads(); ++h)
    {
        const QPoint at = xl165Cell(h);
        grp->assignHead(QLCPoint(at.x(), at.y()), GroupHead(fx->id(), h));
    }
    m_doc->addFixtureGroup(grp);

    QLCPalette *pal = new QLCPalette(QLCPalette::Effect, m_doc);
    pal->setScriptPath(dir + "matrix-image.js");
    pal->setEffectStringParam("image", gif);
    pal->setEffectParamValue("fit", 2);
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    QVERIFY(pal->saveXML(&w));
    w.writeEndDocument();
    buf.close();
    QXmlStreamReader r(buf.data());
    r.readNextStartElement();
    m_doc->effectScriptRunner()->cache()->addDirectory(QDir(dir));
    delete pal;
    QVERIFY(QLCPalette::loader(r, m_doc));            // as a show loads it: the script by name
    QLCPalette *back = nullptr;
    for (QLCPalette *pp : m_doc->palettes())
        if (pp->type() == QLCPalette::Effect && pp->scriptPath().endsWith("matrix-image.js"))
            back = pp;
    QVERIFY(back != nullptr);
    QCOMPARE(back->effectStringParam("image"), gif);

    Scene *scene = new Scene(m_doc);
    scene->addFixtureGroup(grp->id());
    scene->addFixture(fx->id());
    scene->addPalette(back->id());
    m_doc->addFunction(scene);
    scene->start(&timer, FunctionParent::master());
    timer.timerTick();
    EffectInstance inst(m_doc, scene->id(), back->id());
    QVERIFY(inst.isValid());
    inst.runTick();
    // Just started: the red frame -- every RGB head's red full, its green none.
    int reds = 0, greens = 0;
    for (const EffectInstance::DmxWrite &dw : inst.dmxWrites())
    {
        if (dw.channel >= 45 * 3)
            continue;
        if (dw.channel % 3 == 0 && dw.value == 255) ++reds;
        if (dw.channel % 3 == 1 && dw.value > 0) ++greens;
    }
    QCOMPARE(reds, 45);
    QCOMPARE(greens, 0);
    scene->stop(FunctionParent::master());
    timer.timerTick();
}

/* How long each matrix effect takes a tick on a rig of eight XL-450s
   (600 pixels): each scope it has, through the host. The engine ticks
   at 50 a second, every effect on the one thread: an effect taking a
   good part of the 20ms would starve the rest. */
/* This thread's CPU time, in ns: what a tick of an effect costs itself,
   whatever else the machine is doing (a gate's other builds and tests made
   wall-clock time fail the budget at a high load). Where there is no
   per-thread clock: wall-clock time, as before. */
static qint64 threadCpuNs(const QElapsedTimer &wall)
{
#if defined(Q_OS_UNIX) && defined(CLOCK_THREAD_CPUTIME_ID)
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0)
        return qint64(ts.tv_sec) * 1000000000LL + qint64(ts.tv_nsec);
#endif
    return wall.nsecsElapsed();
}

void EffectInstance_Test::matrixEffectsKeepTimeOnEightBoxes()
{
    MasterTimer timer(m_doc);
    QLCFixtureDef *def = makePixelDef("XL timing", 45, 30);
    m_doc->fixtureDefCache()->addFixtureDef(def);
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setSize(QSize(120, 5));
    QList<Fixture *> fx;
    for (int b = 0; b < 8; ++b)
    {
        Fixture *f = new Fixture(m_doc);
        f->setFixtureDefinition(def, def->modes().first());
        f->setUniverse(quint32(b + 1));
        f->setAddress(0);
        QVERIFY(m_doc->addFixture(f));
        fx << f;
        for (int h = 0; h < f->heads(); ++h)
        {
            const QPoint at = xl165Cell(h) + QPoint(b * 15, 0);
            grp->assignHead(QLCPoint(at.x(), at.y()), GroupHead(f->id(), h));
        }
    }
    m_doc->addFixtureGroup(grp);
    Scene *scene = new Scene(m_doc);
    scene->addFixtureGroup(grp->id());
    for (Fixture *f : fx)
        scene->addFixture(f->id());
    m_doc->addFunction(scene);
    scene->start(&timer, FunctionParent::master());
    timer.timerTick();

    // A big picture for the image: the most it has to average.
    QImage pic(MatrixImage::kMaxSide, MatrixImage::kMaxSide, QImage::Format_RGB32);
    for (int y = 0; y < pic.height(); ++y)
        for (int x = 0; x < pic.width(); ++x)
            pic.setPixel(x, y, qRgb(x * 255 / pic.width(), y * 255 / pic.height(), (x ^ y) & 255));
    QByteArray png;
    QBuffer pb(&png);
    pb.open(QIODevice::WriteOnly);
    pic.save(&pb, "PNG");
    const QString picture = MatrixImage::fromBytes(png, "timing.png");
    QVERIFY(!picture.isEmpty());

    const int ticks = qMax(30, qEnvironmentVariableIntValue("QLC_MATRIX_TICKS"));
    // 12 ms in an optimised build, what plays a show. Unoptimised (Debug,
    // and CI's coverage build, which also counts every line) gets twice
    // that: there matrix-lissajous measured 12.4 ms on a GitHub runner.
#ifdef __OPTIMIZE__
    const double defaultBudget = 12.0;
#else
    const double defaultBudget = 24.0;
#endif
    const double budget = qEnvironmentVariableIsSet("QLC_MATRIX_BUDGET_MS")
        ? qEnvironmentVariable("QLC_MATRIX_BUDGET_MS").toDouble() : defaultBudget;
    QStringList slow, table;
    double worst = 0;
    for (const QString &file : kMatrixScripts)
    {
        const int scopes = kScopedMatrixScripts.contains(file) ? 3 : 1;
        QStringList row;
        for (int scope = 0; scope < scopes; ++scope)
        {
            QLCPalette *pal = new QLCPalette(QLCPalette::Effect, m_doc);
            pal->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/" + file);
            if (scopes > 1)
                pal->setEffectParamValue("scope", scope);
            if (file == "matrix-image.js")
                pal->setEffectStringParam("image", picture);
            QVERIFY(m_doc->addPalette(pal));
            scene->addPalette(pal->id());
            EffectInstance inst(m_doc, scene->id(), pal->id());
            QVERIFY2(inst.isValid(), qPrintable(file));
            inst.runTick();                            // the first: the canvas, the picture
            if (file != "matrix-strobe.js")             // between hits, rightly dark
                QVERIFY2(!inst.dmxWrites().isEmpty(), qPrintable(file + ": nothing out"));
            QElapsedTimer clock;
            clock.start();
            const qint64 from = threadCpuNs(clock);
            for (int i = 0; i < ticks; ++i)
                inst.runTick();
            const double ms = double(threadCpuNs(clock) - from) / 1e6 / ticks;
            worst = qMax(worst, ms);
            row << QString::number(ms, 'f', 2);
            if (budget > 0 && ms > budget)
                slow << QString("%1 (scope %2): %3 ms").arg(file).arg(scope).arg(ms, 0, 'f', 2);
            scene->removePalette(pal->id());
        }
        table << QString("%1 %2").arg(file, -26).arg(row.join(" / "));
    }
    qInfo().noquote() << "8 x XL-450, CPU ms a tick (whole canvas / each box / staggered):\n  "
                         + table.join("\n  ");
    QVERIFY2(slow.isEmpty(), qPrintable(slow.join("; ")));
    scene->stop(FunctionParent::master());
    timer.timerTick();
}

/* Not a check: with QLC_MATRIX_PREVIEW_DIR set, a few seconds of each new
   matrix effect on eight XL-450s, frame by frame, as PNGs to look at. */
void EffectInstance_Test::matrixEffectPreviews()
{
    const QString out = qEnvironmentVariable("QLC_MATRIX_PREVIEW_DIR");
    if (out.isEmpty())
        QSKIP("QLC_MATRIX_PREVIEW_DIR not set");
    const QString dir = QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/";
    const MatrixLayout rig = matrixLayout("rig");
    QString gif = tinyGifSetting();
    const QString wheel = qEnvironmentVariable("QLC_MATRIX_PREVIEW_GIF");
    if (!wheel.isEmpty())
        gif = MatrixImage::fromFile(wheel);

    struct Show { QString name, file; QMap<QString, double> set; QList<QColor> look; };
    const QList<QColor> pinkBlue = { QColor(255, 0, 140), QColor(0, 120, 255) };
    const QList<Show> shows = {
        { "fire", "matrix-fire.js", {}, {} },
        { "fire-blue-each-box", "matrix-fire.js", { { "palette", 2 }, { "scope", 1 } }, {} },
        { "starfield", "matrix-starfield.js", {}, {} },
        { "starfield-warp-whole", "matrix-starfield.js", { { "scope", 0 }, { "trails", 1 }, { "speed", 1.2 } }, {} },
        { "spiral", "matrix-spiral.js", {}, pinkBlue },
        { "lissajous", "matrix-lissajous.js", {}, {} },
        { "kaleidoscope", "matrix-kaleidoscope.js", {}, {} },
        { "kaleidoscope-bars", "matrix-kaleidoscope.js", { { "pattern", 1 }, { "folds", 3 } }, pinkBlue },
        { "balls", "matrix-balls.js", {}, {} },
        { "fireworks", "matrix-fireworks.js", {}, {} },
        { "scanner", "matrix-scanner.js", {}, {} },
        { "scanner-whole-3", "matrix-scanner.js", { { "scope", 0 }, { "count", 3 } }, pinkBlue },
        { "dissolve", "matrix-dissolve.js", { { "speed", 0.7 } }, {} },
        { "image-gif-fill", "matrix-image.js", { { "fit", 1 } }, {} },
        { "image-gif-each-box-fit", "matrix-image.js", { { "fit", 0 }, { "scope", 1 } }, {} },
        { "image-no-picture", "matrix-image.js", { { "scope", 1 } }, {} },
        { "gradient", "matrix-gradient.js", { { "angle", 20 } }, {} },
        { "vu", "matrix-vu.js", {}, {} },
        { "vu-staggered", "matrix-vu.js", { { "scope", 2 }, { "band", 0 } }, {} },
        { "ripple-staggered", "matrix-ripple.js", { { "scope", 2 }, { "timing", 1 }, { "speed", 0.6 } }, {} },
        { "ripple-staggered-middle", "matrix-ripple.js", { { "scope", 2 }, { "timing", 1 }, { "speed", 0.6 }, { "staggerOrder", 2 } }, {} },
        { "radar-staggered", "matrix-radar.js", { { "scope", 2 } }, {} },
        { "spiral-staggered-random", "matrix-spiral.js", { { "scope", 2 }, { "staggerOrder", 3 } }, pinkBlue },
        { "scanner-staggered", "matrix-scanner.js", { { "scope", 2 } }, {} },
        { "iris-staggered", "matrix-iris.js", { { "scope", 2 } }, {} }
    };
    const int cell = 9, gap = 1, boxGap = 6;
    const int W = rig.cols * (cell + gap) + 7 * boxGap + gap, H = rig.rows * (cell + gap) + gap;
    for (const Show &sh : shows)
    {
        EffectScript es;
        QVERIFY(es.load(dir + sh.file));
        QDir().mkpath(out + "/" + sh.name);
        FrameTweak tw = [&](QJSEngine *eng, QJSValue &in, QJSValue &pa, QJSValue &pal) {
            if (!sh.look.isEmpty())
                lookColours(sh.look)(eng, in, pa, pal);
            if (sh.file == "matrix-image.js" && sh.name != "image-no-picture")
                withImage(gif)(eng, in, pa, pal);
        };
        // State carried frame to frame, as the host does.
        QJSEngine *eng = es.engine();
        QJSValue state = es.newState();
        for (int i = 0; i < 100; ++i)
        {
            const double t = 1.0 + i * 0.04;
            QJSValue fixtures = eng->newArray(uint(rig.heads.size()));
            for (int h = 0; h < rig.heads.size(); ++h)
            {
                const MatrixHead &m = rig.heads.at(h);
                QJSValue o = eng->newObject();
                o.setProperty("id", m.id); o.setProperty("head", m.head);
                o.setProperty("hasRGB", true); o.setProperty("hasDimmer", m.dimmer);
                o.setProperty("headRGB", m.rgb); o.setProperty("headWhite", m.white);
                QJSValue g = eng->newObject();
                g.setProperty("col", m.col); g.setProperty("row", m.row);
                g.setProperty("cols", rig.cols); g.setProperty("rows", rig.rows);
                o.setProperty("grid", g);
                fixtures.setProperty(uint(h), o);
            }
            QJSValue inputs = eng->newObject();
            inputs.setProperty("_time", t);
            inputs.setProperty("_bpm", 120);
            inputs.setProperty("_beatPos", t * 2);
            inputs.setProperty("_beat", t * 2 - std::floor(t * 2));
            inputs.setProperty("_beatCount", int(std::floor(t * 2)));
            QJSValue params = eng->newObject();
            for (const EffectScript::ParamDef &d : es.paramDefs())
                if (d.type.isEmpty() || d.type == "direction")
                    params.setProperty(d.name, double(d.defaultValue));
            for (auto it = sh.set.constBegin(); it != sh.set.constEnd(); ++it)
                params.setProperty(it.key(), it.value());
            QJSValue palettes = eng->newObject();
            QJSValue look = eng->newObject();
            look.setProperty("colors", eng->newArray(0));
            palettes.setProperty("look", look);
            tw(eng, inputs, params, palettes);
            const QJSValue r = es.callTick(fixtures, inputs, palettes, params, state, eng->newObject());
            QVERIFY2(r.isArray(), qPrintable(sh.name));
            QImage img(W, H, QImage::Format_RGB32);
            img.fill(QColor(18, 18, 18));
            for (int h = 0; h < rig.heads.size(); ++h)
            {
                const MatrixHead &m = rig.heads.at(h);
                const QJSValue o = r.property(uint(h));
                auto v = [&o](const char *k) { return o.property(k).isUndefined() ? 0 : o.property(k).toInt(); };
                QColor c = m.rgb ? QColor(v("r"), v("g"), v("b"))
                                 : QColor(v("w"), v("w"), qMin(255, int(v("w") * 0.92)));
                const int x0 = gap + m.col * (cell + gap) + (m.col / 15) * boxGap;
                const int y0 = gap + m.row * (cell + gap);
                for (int yy = 0; yy < cell; ++yy)
                    for (int xx = 0; xx < cell; ++xx)
                        img.setPixel(x0 + xx, y0 + yy, c.rgb());
            }
            img.save(QString("%1/%2/%3.png").arg(out, sh.name).arg(i, 3, 10, QChar('0')));
        }
    }
}

void EffectInstance_Test::aLooseBoxIsItsOwnGrid()
{
    QLCFixtureDef *def = makePixelDef("XL loose", 45, 30);
    QLCFixtureMode *mode = def->modes().first();
    QLCPhysical phys = mode->physical();
    phys.setLayoutSize(QSize(15, 5));
    mode->setPhysical(phys);
    m_doc->fixtureDefCache()->addFixtureDef(def);
    QList<Fixture *> fx;
    for (int b = 0; b < 2; ++b)
    {
        Fixture *f = new Fixture(m_doc);
        f->setFixtureDefinition(def, mode);
        f->setUniverse(1 + quint32(b));
        f->setAddress(0);
        QVERIFY(m_doc->addFixture(f));
        fx << f;
    }
    // Its rows as the bar is built: RGB, W, RGB, W, RGB.
    m_doc->monitorProperties()->setHeadLayout(
        HeadLayout::fromRowPattern(fx.first(), { "RGB", "W", "RGB", "W", "RGB" }));

    Scene *scene = new Scene(m_doc);
    for (Fixture *f : fx)
        scene->addFixture(f->id());
    m_doc->addFunction(scene);
    QLCPalette *pal = new QLCPalette(QLCPalette::Effect, m_doc);
    pal->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/matrix-bars.js");
    QVERIFY(m_doc->addPalette(pal));
    scene->addPalette(pal->id());

    EffectInstance inst(m_doc, scene->id(), pal->id());
    int cols = 0, rows = 0;
    const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);
    QCOMPARE(cells.size(), 150);
    QCOMPARE(QSize(cols, rows), QSize(30, 5));          // side by side, each its grid
    for (const EffectInstance::EffectCell &c : cells)
    {
        const QPoint want = xl165Cell(c.head) + QPoint(c.fixtureId == fx.at(1)->id() ? 15 : 0, 0);
        QCOMPARE(QPoint(c.col, c.row), want);
    }
}

void EffectInstance_Test::midiDataIsPublishedOncePerTick()
{
    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    QVERIFY(runner != nullptr);
    QVERIFY(runner->dataChannel(QStringLiteral("midi")).isEmpty());

    // A burst of values on the note range (channels 128-255), as one frame
    // of a busy input brings: note 2 struck, many others moving.
    for (int round = 0; round < 20; round++)
        for (quint32 ch = 128; ch < 256; ch++)
            runner->slotInputValueChanged(0, ch, uchar((ch + round) % 200), QString());
    runner->slotInputValueChanged(0, 130, 200, QString());

    // Nothing is rebuilt until the tick that reads it.
    QVERIFY(runner->dataChannel(QStringLiteral("midi")).isEmpty());

    QVERIFY(QMetaObject::invokeMethod(runner, "slotTick", Qt::DirectConnection));
    const QVariantMap midi = runner->dataChannel(QStringLiteral("midi"));
    QVERIFY(!midi.isEmpty());
    QCOMPARE(midi.value(QStringLiteral("held")).toList().at(2).toBool(), true);
    QCOMPARE(midi.value(QStringLiteral("velocity")).toList().at(2).toInt(), 100);
    QVERIFY(midi.value(QStringLiteral("universes")).toMap().contains(QStringLiteral("1")));
}

QTEST_GUILESS_MAIN(EffectInstance_Test)
