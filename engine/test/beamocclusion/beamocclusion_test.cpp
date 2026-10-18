/*
  Q Light Controller Plus - Unit test
  beamocclusion_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QtTest>
#include <QElapsedTimer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#define protected public
#define private public
#include "mastertimer.h"
#include "universe.h"
#undef private
#undef protected

#include "beamocclusion_test.h"
#include "beamocclusion.h"
#include "monitorproperties.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "stageplatform.h"
#include "mastertimer.h"
#include "qlcphysical.h"
#include "stagetarget.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "aimsolver.h"
#include "universe.h"
#include "scenery.h"
#include "fixture.h"
#include "tower.h"
#include "truss.h"
#include "scene.h"
#include "pipe.h"
#include "doc.h"

/* A moving head: pan, tilt and a dimmer. */
static Fixture *makeSpot(Doc *doc, const QString &name)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Spot " + name);
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

    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setAddress(quint32(doc->fixtures().size() * 3));
    if (doc->addFixture(fxi) == false)
        return nullptr;
    return fxi;
}

/* The main drape across the stage at Y = 0, ten metres wide, six high. */
static Scenery *mainDrape(MonitorProperties *props)
{
    Scenery *d = props->addScenery();
    d->setKind(Scenery::MainDrape);
    d->setName("Main drape");
    d->setOriginX(0.0f);
    d->setOriginY(0.0f);
    d->setWidth(10.0f);
    d->setHeight(6.0f);
    d->setOpeningWidth(8.0f);
    return d;
}

static BeamOcclusion::ModelPtr modelOf(MonitorProperties *props)
{
    return props->occlusionModel();
}

void BeamOcclusion_Test::aDrapeBlocksClosedNotOpenOrFlown()
{
    MonitorProperties props;
    Scenery *drape = mainDrape(&props);

    // From a downstage pipe to a mark upstage of the drape, through the middle.
    const QVector3D head(0.0f, 4.0f, 6.0f), mark(0.0f, -3.0f, 1.4f);
    // And one through what is the stage-left half when it opens.
    const QVector3D sideHead(4.6f, 4.0f, 6.0f), sideMark(4.6f, -3.0f, 1.4f);

    drape->setState(Scenery::Closed);
    BeamOcclusion::Hit hit = modelOf(&props)->segment(head, mark);
    QVERIFY2(hit.blocked, "a closed main drape let a beam through to the mark behind it");
    QCOMPARE(hit.name, QString("Main drape"));
    QCOMPARE(hit.kind, int(BeamOcclusion::Drapes));
    QCOMPARE(hit.id, drape->id());
    // It stops at the drape line: 4 of the 7 metres upstage.
    QVERIFY2(qAbs(hit.point.y()) < 0.01f, qPrintable(QString("stopped at y %1").arg(hit.point.y())));
    QVERIFY(qAbs(hit.fraction - 4.0f / 7.0f) < 0.01f);
    QVERIFY(qAbs(hit.distance - (head - hit.point).length()) < 0.01f);
    QVERIFY(hit.clearance <= 0.0f);

    drape->setState(Scenery::Open);
    QVERIFY2(!modelOf(&props)->segment(head, mark).blocked, "the open gap blocked the beam");
    hit = modelOf(&props)->segment(sideHead, sideMark);
    QVERIFY2(hit.blocked, "the gathered half of an open drape let the beam through");

    drape->setState(Scenery::Flown);
    QVERIFY(!modelOf(&props)->segment(head, mark).blocked);
    QVERIFY(!modelOf(&props)->segment(sideHead, sideMark).blocked);

    // A beam onto the apron, in front of a closed drape, is never in its way.
    drape->setState(Scenery::Closed);
    QVERIFY(!modelOf(&props)->segment(head, QVector3D(0.0f, 2.0f, 1.4f)).blocked);

    // A scrim is seen through.
    drape->setKind(Scenery::Scrim);
    QVERIFY(!modelOf(&props)->segment(head, mark).blocked);
}

void BeamOcclusion_Test::aWallBlocksButNotOverTheTop()
{
    MonitorProperties props;
    Scenery *wall = props.addScenery();
    wall->setKind(Scenery::Wall);
    wall->setName("Garden wall");
    wall->setOriginX(0.0f);
    wall->setOriginY(0.0f);
    wall->setWidth(4.0f);
    wall->setHeight(1.0f);
    wall->setThickness(0.3f);

    // A floor light behind the wall cannot see a chest in front of it...
    const QVector3D low(0.0f, -2.0f, 0.3f);
    BeamOcclusion::Hit hit = modelOf(&props)->segment(low, QVector3D(0.0f, 3.0f, 0.5f));
    QVERIFY(hit.blocked);
    QCOMPARE(hit.kind, int(BeamOcclusion::Walls));
    QCOMPARE(hit.name, QString("Garden wall"));
    // ...but one from a pipe above passes over the top.
    QVERIFY(!modelOf(&props)->segment(QVector3D(0.0f, -2.0f, 6.0f), QVector3D(0.0f, 3.0f, 1.4f)).blocked);
    // And past its end is clear.
    QVERIFY(!modelOf(&props)->segment(QVector3D(3.0f, -2.0f, 0.3f), QVector3D(3.0f, 3.0f, 0.5f)).blocked);
}

void BeamOcclusion_Test::aPlatformEdgeBlocksALowBeam()
{
    MonitorProperties props;
    StagePlatform *riser = props.addPlatform();
    riser->setName("Drum riser");
    riser->setOriginX(-1.0f);
    riser->setOriginY(-1.0f);
    riser->setWidth(2.0f);
    riser->setDepth(2.0f);
    riser->setHeight(0.6f);

    // Low across the stage, through the riser's sides.
    BeamOcclusion::Hit hit = modelOf(&props)->segment(QVector3D(-4.0f, 0.0f, 0.3f), QVector3D(4.0f, 0.0f, 0.3f));
    QVERIFY(hit.blocked);
    QCOMPARE(hit.kind, int(BeamOcclusion::Platforms));
    QCOMPARE(hit.id, riser->id());
    QVERIFY2(qAbs(hit.point.x() + 1.0f) < 0.01f, "stopped somewhere other than the riser's near side");

    // From above onto its deck (a target standing on it) is not blocked by it.
    QVERIFY(!modelOf(&props)->segment(QVector3D(0.0f, 5.0f, 6.0f), QVector3D(0.0f, 0.0f, 0.6f)).blocked);
    // From above onto the floor behind it, grazing over: clear.
    QVERIFY(!modelOf(&props)->segment(QVector3D(0.0f, 5.0f, 6.0f), QVector3D(0.0f, -3.0f, 0.0f)).blocked);
    // A side light at deck height onto the floor just behind: through the deck.
    QVERIFY(modelOf(&props)->segment(QVector3D(0.0f, 5.0f, 0.5f), QVector3D(0.0f, -1.5f, 0.0f)).blocked);
}

void BeamOcclusion_Test::aTowerBlocksAndThinPipesDoNot()
{
    MonitorProperties props;
    Tower *tower = props.addTower();
    tower->setName("SL tower");
    tower->setOriginX(-0.25f);
    tower->setOriginY(-0.25f);
    tower->setWidth(0.5f);
    tower->setDepth(0.5f);
    tower->setHeight(4.0f);

    BeamOcclusion::Hit hit = modelOf(&props)->segment(QVector3D(-3.0f, 0.0f, 1.5f), QVector3D(3.0f, 0.0f, 1.5f));
    QVERIFY(hit.blocked);
    QCOMPARE(hit.kind, int(BeamOcclusion::Towers));
    QCOMPARE(hit.name, QString("SL tower"));
    // Over the top of it: clear.
    QVERIFY(!modelOf(&props)->segment(QVector3D(-3.0f, 0.0f, 5.0f), QVector3D(3.0f, 0.0f, 5.0f)).blocked);

    // A box-truss tower is in the way too.
    Truss *box = props.addTruss();
    box->setName("Truss tower");
    box->setType(Truss::Vertical);
    box->setOrigin(QVector3D(0.0f, 3.0f, 0.0f));
    box->setLength(4.0f);
    box->setWidth(0.29f);
    hit = modelOf(&props)->segment(QVector3D(-3.0f, 3.0f, 2.0f), QVector3D(3.0f, 3.0f, 2.0f));
    QVERIFY(hit.blocked);
    QCOMPARE(hit.kind, int(BeamOcclusion::Trusses));

    // A 48 mm boom does not -- until the minimum size comes down below it.
    Pipe *boom = props.addPipe();
    boom->setName("Boom");
    boom->setOrientation(Pipe::Vertical);
    boom->setOriginX(0.0f);
    boom->setOriginY(6.0f);
    boom->setLength(3.0f);
    boom->setDiameter(0.048f);
    const QVector3D a(-3.0f, 6.0f, 1.0f), b(3.0f, 6.0f, 1.0f);
    QVERIFY(!modelOf(&props)->segment(a, b).blocked);
    props.setOcclusionMinSize(0.04f);
    hit = modelOf(&props)->segment(a, b);
    QVERIFY(hit.blocked);
    QCOMPARE(hit.kind, int(BeamOcclusion::Pipes));
}

void BeamOcclusion_Test::clearanceSaysHowCloseItPasses()
{
    MonitorProperties props;
    Scenery *drape = mainDrape(&props);
    drape->setState(Scenery::Open);     // the gap runs from x = -4 to 4

    // Through the gap 0.5 m in from its stage-left edge.
    BeamOcclusion::Hit hit = modelOf(&props)->segment(QVector3D(3.5f, 4.0f, 3.0f), QVector3D(3.5f, -3.0f, 1.4f));
    QVERIFY(!hit.blocked);
    QVERIFY2(qAbs(hit.clearance - 0.5f) < 0.02f, qPrintable(QString("clearance %1").arg(hit.clearance)));
    // Nothing near: as far as it looks.
    drape->setState(Scenery::Flown);
    hit = modelOf(&props)->segment(QVector3D(3.5f, 4.0f, 3.0f), QVector3D(3.5f, -3.0f, 1.4f));
    QCOMPARE(hit.clearance, BeamOcclusion::clearanceReach());
    // Blocked 0.3 m in from the edge.
    drape->setState(Scenery::Open);
    hit = modelOf(&props)->segment(QVector3D(4.3f, 4.0f, 3.0f), QVector3D(4.3f, -3.0f, 1.4f));
    QVERIFY(hit.blocked);
    QVERIFY2(qAbs(hit.clearance + 0.3f) < 0.02f, qPrintable(QString("clearance %1").arg(hit.clearance)));
}

void BeamOcclusion_Test::aHeadIsNotBlockedByItsOwnMount()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    // A tower with a head on its top shelf, and the target on the far side.
    Tower *tower = props->addTower();
    tower->setName("SR tower");
    tower->setOriginX(-0.25f);
    tower->setOriginY(-0.25f);
    tower->setWidth(0.5f);
    tower->setDepth(0.5f);
    tower->setHeight(3.0f);
    tower->addShelf(2.0f);

    Fixture *onTower = makeSpot(doc, "On the tower");
    Fixture *beside = makeSpot(doc, "Beside it");
    QVERIFY(onTower != nullptr && beside != nullptr);

    FixtureRigProps rp;
    rp.towerId = tower->id();
    rp.towerShelf = 0;
    rp.towerU = 0.25f;
    rp.towerV = 0.25f;
    rp.mountingType = Truss::TopHung;
    props->setFixtureRigProps(onTower->id(), rp);

    // The other one on a truss over to the stage-right side.
    Truss *pipe = props->addTruss();
    pipe->setName("SR pipe");
    pipe->setType(Truss::Horizontal);
    pipe->setOrigin(QVector3D(-4.0f, -2.0f, 2.0f));
    pipe->setDirection(QPointF(0.0, 1.0));
    pipe->setLength(4.0f);
    pipe->setWidth(0.29f);
    FixtureRigProps rp2;
    rp2.trussId = pipe->id();
    rp2.trussOffset = 2.0f;           // at (-4, 0, ~2)
    props->setFixtureRigProps(beside->id(), rp2);

    // A point stage left: through the tower for the one beside it, out of
    // its own tower for the one on it.
    const QVector3D point(4.0f, 0.0f, 1.0f);
    const BeamOcclusion::Hit own = AimSolver::lineOfSight(doc, onTower->id(), point);
    QVERIFY2(!own.blocked, qPrintable("blocked by its own tower: " + own.name));
    const BeamOcclusion::Hit other = AimSolver::lineOfSight(doc, beside->id(), point);
    QVERIFY2(other.blocked, "the tower did not block a head beside it");
    QCOMPARE(other.name, QString("SR tower"));
    // ...and the truss the second hangs on does not block it either, though
    // the beam starts inside it.
    QVERIFY(BeamOcclusion::ownMounts(props, beside->id()).size() >= 1);

    delete doc;
}

void BeamOcclusion_Test::theStageSaysWhichKindsCount()
{
    MonitorProperties props;
    mainDrape(&props);
    const QVector3D head(0.0f, 4.0f, 6.0f), mark(0.0f, -3.0f, 1.4f);
    QVERIFY(modelOf(&props)->segment(head, mark).blocked);
    props.setOcclusionKinds(BeamOcclusion::defaultKinds() & ~BeamOcclusion::Drapes);
    QVERIFY(!modelOf(&props)->segment(head, mark).blocked);
    props.setOcclusionKinds(BeamOcclusion::defaultKinds());
    QVERIFY(modelOf(&props)->segment(head, mark).blocked);
    QCOMPARE(BeamOcclusion::kinds().size(), 7);
    foreach (int k, BeamOcclusion::kinds())
        QVERIFY(!BeamOcclusion::kindName(k).isEmpty());
}

void BeamOcclusion_Test::theModelIsRebuiltOnlyWhenTheStageChanges()
{
    MonitorProperties props;
    Scenery *drape = mainDrape(&props);
    BeamOcclusion::ModelPtr a = modelOf(&props);
    BeamOcclusion::ModelPtr b = modelOf(&props);
    QVERIFY2(a.data() == b.data(), "rebuilt with nothing changed");
    QCOMPARE(a->blockerCount(), 1);

    drape->setState(Scenery::Open);
    BeamOcclusion::ModelPtr c = modelOf(&props);
    QVERIFY2(c.data() != a.data(), "a drape opening did not rebuild the model");
    // The old one is still whole for whoever holds it (the DMX thread).
    QVERIFY(a->segment(QVector3D(0, 4, 6), QVector3D(0, -3, 1.4f)).blocked);
    QVERIFY(!c->segment(QVector3D(0, 4, 6), QVector3D(0, -3, 1.4f)).blocked);

    drape->setOriginY(-1.0f);
    QVERIFY(modelOf(&props).data() != c.data());
    StagePlatform *p = props.addPlatform();
    BeamOcclusion::ModelPtr d = modelOf(&props);
    QCOMPARE(d->blockerCount(), 2);
    p->setHeight(1.0f);
    QVERIFY(modelOf(&props).data() != d.data());
}

void BeamOcclusion_Test::theSettingsAndTheOptionAreSaved()
{
    MonitorProperties mp;
    // Defaults write nothing new.
    {
        QByteArray buf;
        QXmlStreamWriter writer(&buf);
        mp.saveXML(&writer, nullptr);
        QVERIFY(!buf.contains("Occlusion"));
    }
    mp.setOcclusionKinds(BeamOcclusion::Drapes | BeamOcclusion::Walls);
    mp.setOcclusionMinSize(0.5f);
    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();
    MonitorProperties mp2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
        if (reader.name() == QStringLiteral("Monitor"))
            QVERIFY(mp2.loadXML(reader, nullptr));
    QCOMPARE(mp2.occlusionKinds(), int(BeamOcclusion::Drapes | BeamOcclusion::Walls));
    QCOMPARE(mp2.occlusionMinSize(), 0.5f);

    Doc doc(this);
    QLCPalette aim(QLCPalette::Aim, &doc);
    aim.setName("Aim");
    aim.setStageTargetId(3);
    QVERIFY(!aim.dimWhenBlocked());
    auto roundTrip = [&doc](QLCPalette &p) {
        QByteArray b;
        QXmlStreamWriter w(&b);
        w.writeStartDocument();
        p.saveXML(&w);
        w.writeEndDocument();
        QLCPalette *back = new QLCPalette(QLCPalette::Undefined, &doc);
        QXmlStreamReader r(b);
        r.readNextStartElement();
        back->loadXML(r);
        return back;
    };
    QLCPalette *off = roundTrip(aim);
    QVERIFY(!off->dimWhenBlocked());
    aim.setDimWhenBlocked(true);
    QLCPalette *on = roundTrip(aim);
    QVERIFY(on->dimWhenBlocked());
    QLCPalette *copy = aim.createCopy();
    QVERIFY(copy->dimWhenBlocked());
    delete off; delete on; delete copy;
}

/* The drape stage with one spot on a downstage pipe, and a person. */
struct DrapeStage
{
    Scenery *drape;
    Fixture *spot;
    StageTarget *who;
    QLCPalette *aim;
    Scene *scene;
};

static DrapeStage drapeStage(Doc *doc)
{
    DrapeStage st;
    MonitorProperties *props = doc->monitorProperties();
    st.drape = mainDrape(props);
    Truss *pipe = props->addTruss();
    pipe->setName("FOH");
    pipe->setType(Truss::Horizontal);
    pipe->setOrigin(QVector3D(-3.0f, 4.0f, 6.0f));
    pipe->setDirection(QPointF(1.0, 0.0));
    pipe->setLength(6.0f);
    pipe->setWidth(0.29f);
    st.spot = makeSpot(doc, "FOH spot");
    FixtureRigProps rp;
    rp.trussId = pipe->id();
    rp.trussOffset = 3.0f;
    rp.mountingType = Truss::TopHung;
    props->setFixtureRigProps(st.spot->id(), rp);

    st.who = props->addStageTarget();
    st.who->setName("Vocalist");
    st.who->setKind(StageTarget::Kind::Person);
    st.who->setPosition(QVector3D(0.0f, -3.0f, 0.0f));   // upstage of the drape

    st.aim = new QLCPalette(QLCPalette::Aim, doc);
    st.aim->setName("At the vocalist");
    st.aim->setStageTargetId(st.who->id());
    doc->addPalette(st.aim);

    st.scene = new Scene(doc);
    st.scene->setName("Solo");
    st.scene->setFadeInSpeed(0);
    st.scene->setFadeOutSpeed(0);
    st.scene->addFixture(st.spot->id());
    st.scene->addPalette(st.aim->id());
    st.scene->setValue(st.spot->id(), 2, 255);           // full on its dimmer
    doc->addFunction(st.scene);
    return st;
}

void BeamOcclusion_Test::aSceneAimSaysWhatIsInTheWay()
{
    Doc *doc = new Doc(this);
    DrapeStage st = drapeStage(doc);

    QList<AimSolver::SceneAim> aims = AimSolver::sceneAims(doc, st.scene);
    QCOMPARE(aims.size(), 1);
    QVERIFY(aims.first().sight.blocked);
    QCOMPARE(aims.first().sight.name, QString("Main drape"));
    QCOMPARE(aims.first().paletteId, st.aim->id());
    QVERIFY(!aims.first().dimsWhenBlocked);
    // Off: nothing asks to be dimmed.
    QVERIFY(AimSolver::blockedDimmedHeads(doc, st.scene).isEmpty());
    st.aim->setDimWhenBlocked(true);
    QCOMPARE(AimSolver::blockedDimmedHeads(doc, st.scene).size(), 1);

    st.drape->setState(Scenery::Open);
    aims = AimSolver::sceneAims(doc, st.scene);
    QVERIFY(!aims.first().sight.blocked);
    QVERIFY(AimSolver::blockedDimmedHeads(doc, st.scene).isEmpty());
    delete doc;
}

void BeamOcclusion_Test::anAimLookDimsABlockedHeadOnlyWhenAsked()
{
    Doc *doc = new Doc(this);
    DrapeStage st = drapeStage(doc);
    const int dimAddr = int(st.spot->address()) + 2;
    MasterTimer timer(doc);

    auto tick = [&]() {
        timer.timerTick();
        QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
        ua[0]->processFaders();
        const uchar v = uchar(ua[0]->preGMValues()[dimAddr]);
        doc->inputOutputMap()->releaseUniverses(false);
        return v;
    };

    // Off (the default): the existing show is untouched -- full, blocked or not.
    st.scene->start(&timer, FunctionParent::master());
    QCOMPARE(int(tick()), 255);
    QCOMPARE(int(tick()), 255);
    st.scene->stop(FunctionParent::master());
    tick();
    tick();

    // On: blocked from the start, it starts out.
    st.aim->setDimWhenBlocked(true);
    st.scene->start(&timer, FunctionParent::master());
    QCOMPARE(int(tick()), 0);
    QCOMPARE(int(tick()), 0);

    // The drape opens: it fades back up to the look's level.
    st.drape->setState(Scenery::Open);
    uchar v = tick();
    QVERIFY2(v < 255, "came straight back rather than fading");
    for (int i = 0; i < 40; ++i)
        v = tick();
    QCOMPARE(int(v), 255);

    // Closed again: it goes back out, over a fade.
    st.drape->setState(Scenery::Closed);
    v = tick();
    QVERIFY(v > 0);
    for (int i = 0; i < 40; ++i)
        v = tick();
    QCOMPARE(int(v), 0);

    // The target walks downstage of the drape, and is seen again.
    st.who->setPosition(QVector3D(0.0f, 2.0f, 0.0f));
    st.scene->requestReaim();
    for (int i = 0; i < 40; ++i)
        v = tick();
    QCOMPARE(int(v), 255);

    st.scene->stop(FunctionParent::master());
    tick();
    // The doc is the test's (deleted after the timer, which still knows it).
}

void BeamOcclusion_Test::aHundredHeadsAndFiftyFeaturesFitATick()
{
    MonitorProperties props;
    // 50 features: 10 drapes and legs, 10 walls, 10 platforms, 10 towers, 10 trusses.
    for (int i = 0; i < 10; ++i)
    {
        Scenery *leg = props.addScenery();
        leg->setKind(Scenery::Leg);
        leg->setOriginX(-6.0f + i * 1.3f);
        leg->setOriginY(-1.0f - (i % 3));
        leg->setWidth(1.2f);
        leg->setHeight(6.0f);
        Scenery *wall = props.addScenery();
        wall->setKind(Scenery::Wall);
        wall->setOriginX(-6.0f + i * 1.3f);
        wall->setOriginY(3.0f + (i % 2));
        wall->setWidth(1.0f);
        wall->setHeight(0.8f);
        wall->setThickness(0.2f);
        StagePlatform *pl = props.addPlatform();
        pl->setOriginX(-6.0f + i * 1.3f);
        pl->setOriginY(-4.0f);
        pl->setWidth(1.0f);
        pl->setDepth(1.0f);
        pl->setHeight(0.4f + 0.1f * i);
        Tower *tw = props.addTower();
        tw->setOriginX(-7.0f + i * 1.5f);
        tw->setOriginY(5.0f);
        tw->setHeight(3.0f);
        Truss *tr = props.addTruss();
        tr->setType(Truss::Horizontal);
        tr->setOrigin(QVector3D(-6.0f, -5.0f + i, 6.0f + 0.2f * i));
        tr->setDirection(QPointF(1.0, 0.0));
        tr->setLength(12.0f);
        tr->setWidth(0.3f);
    }
    BeamOcclusion::ModelPtr model = modelOf(&props);
    QCOMPARE(model->blockerCount(), 50);

    // 100 heads, each to its own point, as a tick would ask -- and a few
    // ticks of it, to time the steady state rather than the first.
    QVector<QVector3D> heads, points;
    for (int i = 0; i < 100; ++i)
    {
        heads << QVector3D(-6.0f + (i % 20) * 0.6f, 6.0f - (i / 20), 5.5f);
        points << QVector3D(-5.0f + (i % 10), -4.0f + (i % 7), 1.4f);
    }
    int blocked = 0;
    QElapsedTimer t;
    t.start();
    const int TICKS = 20;
    for (int k = 0; k < TICKS; ++k)
    {
        BeamOcclusion::ModelPtr m = props.occlusionModel();   // once a tick
        for (int i = 0; i < heads.size(); ++i)
            if (m->segment(heads.at(i), points.at(i)).blocked)
                ++blocked;
    }
    const double perTickMs = double(t.nsecsElapsed()) / 1e6 / TICKS;
    qDebug() << "100 heads x 50 features:" << perTickMs << "ms a tick," << blocked / TICKS << "blocked";
    QVERIFY(blocked > 0);
    // A tick is 20 ms; line of sight must be a small part of it, even in a
    // debug build.
    QVERIFY2(perTickMs < 4.0, qPrintable(QString("%1 ms a tick").arg(perTickMs)));
}

QTEST_GUILESS_MAIN(BeamOcclusion_Test)
