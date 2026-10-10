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
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QBuffer>

#include "effectinstance_test.h"
#define private public                  // MasterTimer::timerTick(), and the instance's insides
#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "scene.h"
#include "scenetransition.h"
#include "mastertimer.h"
#include "qlcpalette.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "qlcfixturehead.h"
#include "effectinstance.h"
#include "effectscript.h"
#include "effectscriptcache.h"
#include "wledfixture.h"
#undef private
#include "monitorproperties.h"
#include "righeads.h"
#include "qlcphysical.h"
#include "stagetarget.h"
#include "truss.h"
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
    QVERIFY(cache.scriptPathForWled("Fireworks").isEmpty());

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
    const QColor red(255, 0, 0), green(0, 255, 0), blue(0, 0, 255);

    EffectScript chase;
    QVERIFY(chase.load(dir + "wled-chase.js"));            // lib/wled.js came with it
    QCOMPARE(chase.wled(), QString("Chase"));
    QCOMPARE(chase.paramDefs().size(), 4);

    // Sixteen LEDs, intensity 128: blocks 1 + (128 * 16 >> 10) = 3 wide. At
    // the start the colour-3 block (blue) is at 0..2, colour 1 (red) behind
    // it at 3..5, the palette (colour 1, red) the rest.
    QList<QColor> px = run(chase, 16, 0.0, 0, 128, 0, { red, green, blue });
    for (int i = 0; i < 16; ++i)
        QCOMPARE(px.at(i), i < 3 ? blue : red);

    // Speed 0 counts a millisecond a step: 8.192 s is an eighth of the way
    // round, two LEDs on.
    px = run(chase, 16, 8.192, 0, 128, 0, { red, green, blue });
    for (int i = 0; i < 16; ++i)
        QCOMPARE(px.at(i), (i >= 2 && i < 5) ? blue : red);

    // Mirrored, it runs on half the string (8, blocks of 2) both ways out.
    px = run(chase, 16, 0.0, 0, 128, 64, { red, green, blue });
    for (int i = 0; i < 16; ++i)
        QCOMPARE(px.at(i), (i < 2 || i > 13) ? blue : red);

    // Reversed, from the other end.
    px = run(chase, 16, 0.0, 0, 128, 128, { red, green, blue });
    for (int i = 0; i < 16; ++i)
        QCOMPARE(px.at(i), i > 12 ? blue : red);

    EffectScript breathe;
    QVERIFY(breathe.load(dir + "wled-breathe.js"));
    QCOMPARE(breathe.wled(), QString("Breathe"));

    // At rest: 30/255 of colour 1 over colour 2 (black) -- (255 * 30) >> 8.
    px = run(breathe, 4, 0.0, 128, 128, 0, { red, QColor(Qt::black), QColor(Qt::black) });
    foreach (const QColor &c, px)
        QCOMPARE(c, QColor(29, 0, 0));
    // At the top of the breath, speed 128 (x26): 1.008 s is counter 26208,
    // 8190 after WLED's folding -- sin16 / 103 = 224, so 254 of 255.
    px = run(breathe, 4, 1.008, 128, 128, 0, { red, QColor(Qt::black), QColor(Qt::black) });
    QCOMPARE(px.first(), QColor(253, 0, 0));
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
    EffectScriptCache cache;
    cache.addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    cache.addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "rgbscripts"), true);
    qputenv("HOME", realHome);

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

QTEST_GUILESS_MAIN(EffectInstance_Test)
