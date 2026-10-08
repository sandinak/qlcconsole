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

#include "effectinstance_test.h"
#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "scene.h"
#include "qlcpalette.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "qlcfixturehead.h"
#define private public
#include "effectinstance.h"
#include "effectscript.h"
#undef private
#include "monitorproperties.h"
#include "righeads.h"
#include "qlcphysical.h"
#include "stagetarget.h"
#include "truss.h"
#include "aimsolver.h"
#include <QtMath>

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

QTEST_GUILESS_MAIN(EffectInstance_Test)
