/*
  Q Light Controller Plus
  blocking_test.cpp

  Blocking (docs/design/BLOCKING.md, board Q211): named sets of where the
  stage targets stand, chosen per cue and tracked forward.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtTest>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "blocking_test.h"
#define private public                  // MasterTimer::timerTick()
#define protected public                // Universe::processFaders()
#include "mastertimer.h"
#include "universe.h"
#undef protected
#undef private
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "mastertimer.h"
#include "qlcphysical.h"
#include "stagetarget.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "collection.h"
#include "areacover.h"
#include "aimsolver.h"
#include "blocking.h"
#include "universe.h"
#include "fixture.h"
#include "truss.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
bool near3(const QVector3D &a, const QVector3D &b, float tol = 0.01f)
{
    return (a - b).length() <= tol;
}

QString str(const QVector3D &v)
{
    return QString("(%1, %2, %3)").arg(double(v.x()), 0, 'f', 3)
                                  .arg(double(v.y()), 0, 'f', 3)
                                  .arg(double(v.z()), 0, 'f', 3);
}

/* Two 16-bit spots hung 6 m up on a pipe across the stage, a target
   "Front Rank" 2 m downstage of the pipe, an Aim look at it and a scene
   of the spots playing it (the follow-spot test's rig). */
struct Rig
{
    QList<Fixture *> spots;
    StageTarget *rank = nullptr;
    QLCPalette *aim = nullptr;
    Scene *scene = nullptr;
    FixtureGroup *group = nullptr;
};

Rig makeRig(Doc *doc, StageTarget::Kind kind = StageTarget::Kind::None)
{
    Rig rig;
    MonitorProperties *props = doc->monitorProperties();
    Truss *pipe = props->addTruss();
    pipe->setType(Truss::Horizontal);
    pipe->setOrigin(QVector3D(-3.0f, 0.0f, 6.0f));
    pipe->setDirection(QPointF(1.0, 0.0));
    pipe->setLength(6.0f);
    pipe->setWidth(0.29f);

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

    rig.group = new FixtureGroup(doc);
    rig.group->setName("Spots");
    rig.group->setSize(QSize(2, 1));
    for (int i = 0; i < 2; ++i)
    {
        Fixture *spot = new Fixture(doc);
        spot->setName(QString("Spot %1").arg(i + 1));
        spot->setFixtureDefinition(def, mode);
        spot->setUniverse(0);
        spot->setAddress(100 + 10 * i);
        doc->addFixture(spot);
        FixtureRigProps rp;
        rp.trussId = pipe->id();
        rp.trussOffset = i == 0 ? 1.0f : 5.0f;
        rp.mountingType = Truss::TopHung;
        props->setFixtureRigProps(spot->id(), rp);
        rig.spots << spot;
        rig.group->assignFixture(spot->id());
    }
    doc->addFixtureGroup(rig.group);

    rig.rank = props->addStageTarget();
    rig.rank->setName("Front Rank");
    rig.rank->setKind(kind);
    rig.rank->setPosition(QVector3D(0.0f, 2.0f, 0.0f));

    rig.aim = new QLCPalette(QLCPalette::Aim, doc);
    rig.aim->setName("Front Rank warm");
    rig.aim->setStageTargetId(rig.rank->id());
    doc->addPalette(rig.aim);

    rig.scene = new Scene(doc);
    rig.scene->setName("Front Rank warm");
    doc->addFunction(rig.scene);
    for (Fixture *spot : rig.spots)
        rig.scene->addFixture(spot->id());
    rig.scene->addPalette(rig.aim->id());
    return rig;
}

void play(Doc *doc, MasterTimer &timer, int frames = 5)
{
    for (int k = 0; k < frames; ++k)
    {
        timer.timerTick();
        QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
        ua.first()->processFaders();
        doc->inputOutputMap()->releaseUniverses(false);
    }
}

QByteArray dmx(Doc *doc)
{
    QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
    const QByteArray out = ua.first()->preGMValues();
    doc->inputOutputMap()->releaseUniverses(false);
    return out;
}

/** How far (metres) a spot's beam, by its pan and tilt as DMX has them,
 *  passes from @p point. */
double missBy(Doc *doc, Fixture *spot, const QVector3D &point)
{
    const QByteArray d = dmx(doc);
    const int a = int(spot->address());
    const double pan16 = (uchar(d.at(a)) << 8) | uchar(d.at(a + 1));
    const double tilt16 = (uchar(d.at(a + 2)) << 8) | uchar(d.at(a + 3));
    const double pan = 540.0 * pan16 / 65535.0 - 270.0;
    const double tilt = 270.0 * tilt16 / 65535.0 - 135.0;
    AimSolver::Frame frame;
    if (!AimSolver::frameFor(doc, spot->id(), frame))
        return 1.0e9;
    QVector3D dir;
    if (!AimSolver::beamDirection(doc->monitorProperties()->fixtureRigProps(spot->id()), pan, tilt, dir))
        return 1.0e9;
    const QVector3D to = point - frame.position;
    const float along = QVector3D::dotProduct(to, dir);
    if (along <= 0.0f)
        return 1.0e9;
    return double((to - along * dir).length());
}

QString show(double d)
{
    return QString::number(d, 'f', 3);
}

/** A list of @p n cues, the first with a layer on the spots playing the
 *  Aim look (the rest track it). */
QList<quint32> makeCues(Doc *doc, const Rig &rig, int n, quint32 *listOut = nullptr)
{
    ShowTreeController *st = doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "Main", ListMode::Tracking);
    QList<quint32> cues;
    for (int i = 0; i < n; ++i)
        cues << st->addCue(listId, i - 1, QString("Cue %1").arg(i + 1));
    Target t;
    t.groups << rig.group->id();
    st->setLayerLook(cues.first(), st->addLayer(cues.first(), t), st->lookForPalette(rig.aim->id()));
    if (listOut != nullptr)
        *listOut = listId;
    return cues;
}
}

void Blocking_Test::init()
{
    m_doc = new Doc(this);
}

void Blocking_Test::cleanup()
{
    BlockingScope::clearThreadDefault();
    delete m_doc;
    m_doc = nullptr;
}

/* One reader: a target's place in a blocking is its Default place moved by
   the blocking's offset; a target the blocking leaves alone stays put; the
   live blocking, a scope and a thread's default pick which one is read. */
void Blocking_Test::aTargetStandsWhereItsBlockingSays()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    StageTarget *rank = mp->addStageTarget();
    rank->setPosition(QVector3D(0, 2, 0));
    StageTarget *drums = mp->addStageTarget();
    drums->setPosition(QVector3D(-3, -1, 0.5f));

    QVERIFY(mp->blockingSets().isEmpty());
    QCOMPARE(mp->blockingName(0), QString("Default"));
    const quint32 apron = mp->addBlockingSet("Apron number");
    QVERIFY(apron != 0);
    QCOMPARE(mp->blockingName(apron), QString("Apron number"));
    // Names stay unique, and never "Default".
    const quint32 apron2 = mp->addBlockingSet("Apron number");
    QCOMPARE(mp->blockingName(apron2), QString("Apron number 2"));
    QVERIFY(!mp->renameBlockingSet(apron2, "Default"));
    QVERIFY(mp->renameBlockingSet(apron2, "Finale"));

    StageTarget::Blocked b;
    b.hasOffset = true;
    b.offset = QVector3D(0, 1.8f, 0);
    rank->setBlocked(apron, b);

    QVERIFY(near3(mp->targetPosition(rank, 0), QVector3D(0, 2, 0)));
    QVERIFY2(near3(mp->targetPosition(rank, apron), QVector3D(0, 3.8f, 0)),
             qPrintable(str(mp->targetPosition(rank, apron))));
    QVERIFY(near3(mp->targetPosition(drums, apron), QVector3D(-3, -1, 0.5f)));     // not in it: Default
    QVERIFY(near3(mp->targetAimPoint(rank, false, apron), QVector3D(0, 3.8f, 0)));

    // The live blocking is what an unscoped read gets.
    QCOMPARE(mp->liveBlocking(), quint32(0));
    QVERIFY(near3(mp->targetPosition(rank), QVector3D(0, 2, 0)));
    QSignalSpy live(mp, &MonitorProperties::liveBlockingChanged);
    QVERIFY(mp->setLiveBlocking(apron));
    QVERIFY(!mp->setLiveBlocking(apron));
    QCOMPARE(live.count(), 1);
    QVERIFY(near3(mp->targetPosition(rank), QVector3D(0, 3.8f, 0)));
    {
        // Blind, the Studio, Show Review: a scope on this thread only.
        BlockingScope scope(0);
        QVERIFY(near3(mp->targetPosition(rank), QVector3D(0, 2, 0)));
        {
            BlockingScope inner(apron);
            QVERIFY(near3(mp->targetPosition(rank), QVector3D(0, 3.8f, 0)));
        }
        QVERIFY(near3(mp->targetPosition(rank), QVector3D(0, 2, 0)));
    }
    QVERIFY(near3(mp->targetPosition(rank), QVector3D(0, 3.8f, 0)));
    // The UI's default for its thread (the blocking its views show).
    BlockingScope::setThreadDefault(0);
    QVERIFY(near3(mp->targetPosition(rank), QVector3D(0, 2, 0)));
    BlockingScope::clearThreadDefault();
    QVERIFY(near3(mp->targetPosition(rank), QVector3D(0, 3.8f, 0)));

    // Bound to a riser, a blocking moves it off the riser by its offset.
    StagePlatform *riser = mp->addPlatform();
    riser->setOriginX(4);
    riser->setOriginY(4);
    riser->setWidth(2);
    riser->setDepth(2);
    riser->setHeight(0.4f);
    rank->setBinding(QStringLiteral("platform"), riser->id());
    QVERIFY2(near3(mp->targetPosition(rank, 0), QVector3D(5, 5, 0.4f)), qPrintable(str(mp->targetPosition(rank, 0))));
    QVERIFY2(near3(mp->targetPosition(rank, apron), QVector3D(5, 6.8f, 0.4f)),
             qPrintable(str(mp->targetPosition(rank, apron))));
    rank->clearBinding();

    // A set removed takes its overrides, and the live blocking, with it.
    mp->removeBlockingSet(apron);
    QVERIFY(!rank->isBlockedIn(apron));
    QCOMPARE(mp->liveBlocking(), quint32(0));
    QVERIFY(near3(mp->targetPosition(rank), QVector3D(0, 2, 0)));
}

/* A venue moves a target's Default place (Q128). A blocking is an offset
   from it, so the apron mark stays "1.8 m downstage of where they stand"
   in every room. */
void Blocking_Test::aBlockingIsAnOffsetSoAVenueMovesIt()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    StageTarget *rank = mp->addStageTarget();
    rank->setPosition(QVector3D(0, 2, 0));
    const quint32 apron = mp->addBlockingSet("Apron number");
    StageTarget::Blocked b;
    b.hasOffset = true;
    b.offset = QVector3D(0, 1.8f, 0);
    rank->setBlocked(apron, b);

    // What VenueCorrections does: the show's own held, the venue's on.
    rank->setBasePosition(rank->position());
    rank->setPosition(QVector3D(0.5f, 1.0f, 0));
    QVERIFY2(near3(mp->targetPosition(rank, apron), QVector3D(0.5f, 2.8f, 0)),
             qPrintable(str(mp->targetPosition(rank, apron))));

    // The show file keeps the show's place and the offset, not the venue's.
    QByteArray xml;
    {
        QXmlStreamWriter w(&xml);
        rank->saveXML(&w);
    }
    QVERIFY2(xml.contains("Y=\"2.000\""), xml.constData());
    QVERIFY2(xml.contains("<Blocked Set=\"1\" DX=\"0.000\" DY=\"1.800\" DZ=\"0.000\"/>"), xml.constData());
}

/* Unset, a cue keeps the cue before's blocking; the first starts from
   Default; a skipped cue passes nothing on; Default can be set on purpose. */
void Blocking_Test::aCueTracksItsBlockingForward()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    const quint32 apron = mp->addBlockingSet("Apron number");
    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "Main", ListMode::Tracking);
    QList<quint32> c;
    for (int i = 0; i < 5; ++i)
        c << st->addCue(listId, i - 1, QString("Cue %1").arg(i + 1));

    for (quint32 id : c)
        QCOMPARE(st->cueBlocking(id), quint32(0));
    QVERIFY(st->setCuesBlocking({ c[1] }, apron));
    QVERIFY(!st->setCuesBlocking({ c[1] }, apron));     // no change, no undo step
    QCOMPARE(st->cueOwnBlocking(c[1]), apron);
    QCOMPARE(st->cueOwnBlocking(c[2]), ShowTree::invalidId());
    QCOMPARE(st->cueBlocking(c[0]), quint32(0));
    QCOMPARE(st->cueBlocking(c[1]), apron);
    QCOMPARE(st->cueBlocking(c[2]), apron);             // tracks
    QCOMPARE(st->cueBlocking(c[4]), apron);
    QVERIFY(st->setCuesBlocking({ c[3] }, 0));          // back to Default on purpose
    QCOMPARE(st->cueBlocking(c[3]), quint32(0));
    QCOMPARE(st->cueBlocking(c[4]), quint32(0));

    // The tree's flag: where it changes, not where it tracks.
    QVERIFY(!st->cueChangesBlocking(c[0]));
    QVERIFY(st->cueChangesBlocking(c[1]));
    QVERIFY(!st->cueChangesBlocking(c[2]));
    QVERIFY(st->cueChangesBlocking(c[3]));

    // A skipped cue passes nothing on: cue 3 starts from cue 1.
    QVERIFY(st->setCuesSkipped({ c[1] }, true));
    QCOMPARE(st->cueBlocking(c[2]), quint32(0));
    QCOMPARE(st->cueBlocking(c[1]), apron);             // jumped to, it plays its own
    QVERIFY(st->setCuesSkipped({ c[1] }, false));

    // One undo step, and back.
    QVERIFY(st->undo());
    QVERIFY(st->undo());
    QVERIFY(st->undo());
    QCOMPARE(st->cueBlocking(c[4]), apron);
    QVERIFY(st->undo());
    QCOMPARE(st->cueBlocking(c[4]), quint32(0));
    QCOMPARE(st->cueOwnBlocking(c[1]), ShowTree::invalidId());
}

/* The look is aimed at a target; the cue moves the target. Cue 1 plays
   "Front Rank warm" where they usually stand, cue 2 tracks the same look
   with the Apron blocking: the heads land on the apron mark, and the next
   cue in Default brings them back. Nothing but the cue changed. */
void Blocking_Test::aimDmxFollowsTheCuesBlocking()
{
    Rig rig = makeRig(m_doc);
    MonitorProperties *mp = m_doc->monitorProperties();
    const quint32 apron = mp->addBlockingSet("Apron number");
    StageTarget::Blocked b;
    b.hasOffset = true;
    b.offset = QVector3D(0, 2.0f, 0);
    rig.rank->setBlocked(apron, b);

    QList<quint32> cues = makeCues(m_doc, rig, 3);
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setCuesBlocking({ cues[1] }, apron));
    QVERIFY(st->setCuesBlocking({ cues[2] }, 0));
    QList<Collection *> colls;
    for (quint32 id : cues)
    {
        Collection *coll = qobject_cast<Collection *>(m_doc->function(st->cue(id)->collection));
        QVERIFY2(coll != nullptr, "the cue did not bake");
        colls << coll;
    }
    // The bake caches each cue's blocking on its Collection.
    QCOMPARE(colls[0]->blocking(), quint32(0));
    QCOMPARE(colls[1]->blocking(), apron);
    QCOMPARE(colls[2]->blocking(), quint32(0));

    const QVector3D home = mp->targetAimPoint(rig.rank, false, 0);
    const QVector3D onApron = mp->targetAimPoint(rig.rank, false, apron);
    QVERIFY(near3(onApron, home + QVector3D(0, 2, 0)));

    MasterTimer timer(m_doc);
    auto go = [&](int i) {
        if (i > 0)
            colls[i - 1]->stop(FunctionParent::master());
        colls[i]->start(&timer, FunctionParent::master());
        play(m_doc, timer);
        QCoreApplication::processEvents();      // what stays up re-aims (Doc, queued)
        play(m_doc, timer);
    };

    go(0);
    QVERIFY(colls[0]->isRunning());     // the tree's own scenes, baked from the layer
    QCOMPARE(mp->liveBlocking(), quint32(0));
    for (Fixture *spot : rig.spots)
        QVERIFY2(missBy(m_doc, spot, home) < 0.05,
                 qPrintable(QString("cue 1: %1 misses home by %2 m").arg(spot->name(), show(missBy(m_doc, spot, home)))));

    go(1);
    QCOMPARE(mp->liveBlocking(), apron);
    for (Fixture *spot : rig.spots)
        QVERIFY2(missBy(m_doc, spot, onApron) < 0.05,
                 qPrintable(QString("cue 2: %1 misses the apron mark by %2 m")
                            .arg(spot->name(), show(missBy(m_doc, spot, onApron)))));

    go(2);
    QCOMPARE(mp->liveBlocking(), quint32(0));
    for (Fixture *spot : rig.spots)
        QVERIFY2(missBy(m_doc, spot, home) < 0.05,
                 qPrintable(QString("cue 3: %1 misses home by %2 m").arg(spot->name(), show(missBy(m_doc, spot, home)))));

    // A look that stays up through a blocking change re-aims over the fade
    // it is given, from where the heads are.
    colls[2]->stop(FunctionParent::master());
    play(m_doc, timer);
    rig.scene->start(&timer, FunctionParent::master());
    play(m_doc, timer);
    QVERIFY(mp->setLiveBlocking(apron, 2000));
    QCoreApplication::processEvents();
    play(m_doc, timer, 3);
    double first = 0.0;
    for (Fixture *spot : rig.spots)
        first = qMax(first, missBy(m_doc, spot, onApron));
    QVERIFY2(first > 0.3, qPrintable(QString("snapped at once: %1 m off").arg(show(first))));
    play(m_doc, timer, 120);
    for (Fixture *spot : rig.spots)
        QVERIFY2(missBy(m_doc, spot, onApron) < 0.05,
                 qPrintable(QString("after the fade: %1 m off").arg(show(missBy(m_doc, spot, onApron)))));
    rig.scene->stop(FunctionParent::master());
    play(m_doc, timer);
}

/* A line's points and an area's footprint are part of a blocking: the
   chorus line bends round the apron, the dance area is wider there. */
void Blocking_Test::areaAndLineShapesChangePerBlocking()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    const quint32 apron = mp->addBlockingSet("Apron number");

    StageTarget *area = mp->addStageTarget();
    area->setKind(StageTarget::Kind::Area);
    area->setPosition(QVector3D(0, 3, 0));
    area->setFootprint(QSizeF(2, 1));
    StageTarget::Blocked wide;
    wide.hasOffset = true;
    wide.offset = QVector3D(0, 2, 0);
    wide.hasFootprint = true;
    wide.footprint = QSizeF(6, 1.5);
    area->setBlocked(apron, wide);
    QCOMPARE(mp->targetFootprint(area, 0), QSizeF(2, 1));
    QCOMPARE(mp->targetFootprint(area, apron), QSizeF(6, 1.5));
    QCOMPARE(area->footprint(), QSizeF(2, 1));          // Default's own is untouched
    {
        BlockingScope scope(0);
        QCOMPARE(AreaCover::areaRect(mp, area), QRectF(-1, 2.5, 2, 1));
    }
    {
        BlockingScope scope(apron);
        QCOMPARE(AreaCover::areaRect(mp, area), QRectF(-3, 4.25, 6, 1.5));
    }

    StageTarget *line = mp->addStageTarget();
    line->setPosition(QVector3D(0, 2, 0));
    line->makeLine();       // -2..2 across
    StageTarget::Blocked arc;
    arc.hasLine = true;
    arc.linePoints = { QVector3D(-3, 0, 0), QVector3D(0, 1.5f, 0), QVector3D(3, 0, 0) };
    line->setBlocked(apron, arc);
    const QVector<QVector3D> home = mp->targetLinePoints(line, 0);
    const QVector<QVector3D> there = mp->targetLinePoints(line, apron);
    QCOMPARE(home.size(), 2);
    QCOMPARE(there.size(), 3);
    QVERIFY(near3(home.first(), QVector3D(-2, 2, 0)));
    QVERIFY(near3(there.at(1), QVector3D(0, 3.5f, 0)));
    QCOMPARE(line->linePoints().size(), 2);              // Default's own is untouched
    {
        BlockingScope scope(apron);
        QVERIFY(near3(AimSolver::linePoint(m_doc, line, 0.0), QVector3D(-3, 2, 0)));
    }
}

/* The follow spot aims where the person stands in the cue's blocking: its
   own aim point (a chest, AimSolver::aimPointFor in subject mode) and the
   heads on stage both follow the live blocking. */
void Blocking_Test::theFollowSpotFollowsTheBlockedPosition()
{
    Rig rig = makeRig(m_doc, StageTarget::Kind::Person);
    MonitorProperties *mp = m_doc->monitorProperties();
    const quint32 apron = mp->addBlockingSet("Apron number");
    StageTarget::Blocked b;
    b.hasOffset = true;
    b.offset = QVector3D(1.5f, 2.0f, 0);
    rig.rank->setBlocked(apron, b);

    const QVector3D chestHome = AimSolver::aimPointFor(m_doc, rig.spots[0]->id(), rig.rank, true);
    QVERIFY(mp->setLiveBlocking(apron));
    const QVector3D chestApron = AimSolver::aimPointFor(m_doc, rig.spots[0]->id(), rig.rank, true);
    QVERIFY2(near3(chestApron - chestHome, QVector3D(1.5f, 2.0f, 0), 0.02f),
             qPrintable(str(chestApron - chestHome)));

    MasterTimer timer(m_doc);
    rig.scene->start(&timer, FunctionParent::master());
    play(m_doc, timer);
    const QVector3D at = mp->targetAimPoint(rig.rank, false);
    QVERIFY(near3(at, QVector3D(1.5f, 4.0f, 0) + QVector3D(0, 0, at.z())));
    for (Fixture *spot : rig.spots)
    {
        const QVector3D p = AimSolver::aimPointFor(m_doc, spot->id(), rig.rank, false);
        QVERIFY2(missBy(m_doc, spot, p) < 0.05,
                 qPrintable(QString("%1 misses the blocked subject by %2 m").arg(spot->name(),
                                                                                 show(missBy(m_doc, spot, p)))));
    }
    rig.scene->stop(FunctionParent::master());
    play(m_doc, timer);
}

/* The sets in the show, each target's place in them, a cue's blocking on
   the cue: all back as they were. */
void Blocking_Test::blockingSurvivesSaveAndLoad()
{
    Rig rig = makeRig(m_doc);
    MonitorProperties *mp = m_doc->monitorProperties();
    const quint32 apron = mp->addBlockingSet("Apron number");
    const quint32 finale = mp->addBlockingSet("Finale");
    StageTarget::Blocked b;
    b.hasOffset = true;
    b.offset = QVector3D(0.25f, 1.8f, 0);
    b.hasFootprint = true;
    b.footprint = QSizeF(3, 2);
    rig.rank->setBlocked(apron, b);
    StageTarget::Blocked l;
    l.hasLine = true;
    l.linePoints = { QVector3D(-1, 0, 0), QVector3D(1, 0, 0.5f) };
    rig.rank->setBlocked(finale, l);
    const QList<quint32> cues = makeCues(m_doc, rig, 3);
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setCuesBlocking({ cues[1] }, apron));
    QVERIFY(st->setCuesBlocking({ cues[2] }, 0));

    QByteArray props, tree;
    {
        QXmlStreamWriter w(&props);
        w.writeStartDocument();
        QVERIFY(mp->saveXML(&w, m_doc));
        w.writeEndDocument();
    }
    {
        QXmlStreamWriter w(&tree);
        w.writeStartDocument();
        st->saveXML(&w);
        w.writeEndDocument();
    }
    QVERIFY(props.contains("<Blocking ID=\"1\" Name=\"Apron number\"/>"));
    QVERIFY(tree.contains("Blocking=\"1\""));
    QVERIFY(tree.contains("Blocking=\"0\""));

    Doc other(this);
    {
        QXmlStreamReader r(props);
        QVERIFY(r.readNextStartElement());
        QVERIFY(other.monitorProperties()->loadXML(r, &other));
    }
    {
        QXmlStreamReader r(tree);
        QVERIFY(r.readNextStartElement());
        QVERIFY(other.showTree()->loadXML(r));
    }
    MonitorProperties *mp2 = other.monitorProperties();
    QCOMPARE(mp2->blockingSets().size(), 2);
    QCOMPARE(mp2->blockingName(apron), QString("Apron number"));
    QCOMPARE(mp2->blockingName(finale), QString("Finale"));
    StageTarget *rank = mp2->stageTarget(rig.rank->id());
    QVERIFY(rank != nullptr);
    const StageTarget::Blocked back = rank->blocked(apron);
    QVERIFY(back.hasOffset && back.hasFootprint && !back.hasLine);
    QVERIFY(near3(back.offset, b.offset, 1e-3f));
    QCOMPARE(back.footprint, QSizeF(3, 2));
    const StageTarget::Blocked lineBack = rank->blocked(finale);
    QVERIFY(lineBack.hasLine && !lineBack.hasOffset);
    QCOMPARE(lineBack.linePoints.size(), 2);
    QVERIFY(near3(lineBack.linePoints.at(1), QVector3D(1, 0, 0.5f), 1e-3f));
    QCOMPARE(other.showTree()->cueOwnBlocking(cues[0]), ShowTree::invalidId());
    QCOMPARE(other.showTree()->cueBlocking(cues[1]), apron);
    QCOMPARE(other.showTree()->cueBlocking(cues[2]), quint32(0));
}

/* A show from before blocking: Default only, every cue in it, and what
   it plays byte for byte what it played -- a set it does not use changes
   nothing, and saving it again writes nothing new. */
void Blocking_Test::anOldShowLoadsWithDefaultAndPlaysTheSame()
{
    Rig rig = makeRig(m_doc);
    const QList<quint32> cues = makeCues(m_doc, rig, 2);
    MonitorProperties *mp = m_doc->monitorProperties();
    ShowTreeController *st = m_doc->showTree();

    // Save as it is: nothing about blocking in either file.
    QByteArray props, tree;
    {
        QXmlStreamWriter w(&props);
        QVERIFY(mp->saveXML(&w, m_doc));
    }
    {
        QXmlStreamWriter w(&tree);
        st->saveXML(&w);
    }
    QVERIFY(!props.contains("Blocking") && !props.contains("Blocked"));
    QVERIFY(!tree.contains("Blocking"));

    // Loaded: Default only, every cue in it.
    Doc other(this);
    {
        QXmlStreamReader r(props);
        QVERIFY(r.readNextStartElement());
        QVERIFY(other.monitorProperties()->loadXML(r, &other));
    }
    QVERIFY(other.monitorProperties()->blockingSets().isEmpty());
    for (quint32 c : cues)
        QCOMPARE(st->cueBlocking(c), quint32(0));

    // What it plays, before and after a set exists that no cue uses.
    QSignalSpy live(mp, &MonitorProperties::liveBlockingChanged);
    MasterTimer timer(m_doc);
    auto output = [&]() {
        Collection *coll = qobject_cast<Collection *>(m_doc->function(st->cue(cues[1])->collection));
        coll->start(&timer, FunctionParent::master());
        play(m_doc, timer);
        QCoreApplication::processEvents();
        play(m_doc, timer);
        const QByteArray out = dmx(m_doc);
        coll->stop(FunctionParent::master());
        play(m_doc, timer, 3);
        return out;
    };
    const QByteArray before = output();
    QVERIFY(before.mid(100, 4) != QByteArray(4, '\0'));    // the heads were aimed
    QCOMPARE(live.count(), 0);                              // and nothing changed blocking

    const quint32 apron = mp->addBlockingSet("Apron number");
    StageTarget::Blocked b;
    b.hasOffset = true;
    b.offset = QVector3D(0, 2, 0);
    rig.rank->setBlocked(apron, b);
    const QByteArray after = output();
    QCOMPARE(after, before);
    QCOMPARE(live.count(), 0);
}

/* BLK-01 lives in showreview_test (every check is fired and shown quiet
   there); here: the reach it reads changes with the blocking. */
void Blocking_Test::showReviewSaysWhenABlockingIsOutOfReach()
{
    Rig rig = makeRig(m_doc);
    MonitorProperties *mp = m_doc->monitorProperties();
    const quint32 up = mp->addBlockingSet("Flying");
    StageTarget::Blocked b;
    b.hasOffset = true;
    b.offset = QVector3D(0, -2, 14);        // straight over the pipe: no hung head tilts there
    rig.rank->setBlocked(up, b);
    auto reachable = [&]() {
        bool all = true;
        for (const AimSolver::SceneAim &a : AimSolver::sceneAims(m_doc, rig.scene))
            all = all && a.solution.reachable();
        return all;
    };
    {
        BlockingScope scope(0);
        QVERIFY(reachable());
    }
    {
        BlockingScope scope(up);
        QVERIFY(!reachable());
    }
}

QTEST_MAIN(Blocking_Test)
