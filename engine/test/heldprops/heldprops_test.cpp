/*
  Q Light Controller Plus
  heldprops_test.cpp

  Props someone holds (docs/design/BLOCKING.md, "Props someone holds"):
  a staff or a candle in a performer's hand, cued by the blocking; and the
  candles' IR remote (CandleRemote, Generic > IR Candle Remote).

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtTest>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "heldprops_test.h"
#include "../common/stdprelude.h"       // every std header, before the define
#define private public                  // MasterTimer::timerTick()
#define protected public                // Universe::processFaders()
#include "mastertimer.h"
#include "universe.h"
#undef protected
#undef private
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "fixturedefcheck.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "candleremote.h"
#include "stageplatform.h"
#include "qlcphysical.h"
#include "stagetarget.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "collection.h"
#include "heldprops.h"
#include "blocking.h"
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

StageTarget *person(MonitorProperties *mp, const QString &name, float x, float y)
{
    StageTarget *t = mp->addStageTarget();
    t->setName(name);
    t->setKind(StageTarget::Kind::Person);
    t->setPosition(QVector3D(x, y, 0.0f));
    return t;
}

StageTarget *prop(MonitorProperties *mp, StageTarget::Kind k, const QString &name, float x, float y)
{
    StageTarget *t = mp->addStageTarget();
    t->setName(name);
    t->setKind(k);
    t->setPosition(QVector3D(x, y, 0.0f));
    return t;
}

QByteArray dmx(Doc *doc)
{
    QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
    const QByteArray out = ua.first()->preGMValues();
    doc->inputOutputMap()->releaseUniverses(false);
    return out;
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

QLCFixtureDef *remoteDef()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    // Not __FILE__: Ninja compiles with a relative one (Linux CI).
    const QString path = QStringLiteral(QLC_TEST_RESOURCE_ROOT "fixtures/Generic/Generic-IR-Candle-Remote.qxf");
    if (def->loadXML(path) != QFile::NoError)
    {
        delete def;
        return nullptr;
    }
    return def;
}
}

void HeldProps_Test::init()
{
    m_doc = new Doc(this);
}

void HeldProps_Test::cleanup()
{
    BlockingScope::clearThreadDefault();
    delete m_doc;
    m_doc = nullptr;
}

/* Staff and Candle are the hand props; nothing else is, and a Person never
   is (so a hold cannot loop). They are saved by name. */
void HeldProps_Test::onlyStaffAndCandleAreHeld()
{
    QVERIFY(StageTarget::isHandPropKind(StageTarget::Kind::Staff));
    QVERIFY(StageTarget::isHandPropKind(StageTarget::Kind::Candle));
    for (StageTarget::Kind k : { StageTarget::Kind::None, StageTarget::Kind::Person, StageTarget::Kind::DrumKit,
                                 StageTarget::Kind::Structure, StageTarget::Kind::Area, StageTarget::Kind::Line,
                                 StageTarget::Kind::Keyboard, StageTarget::Kind::GrandPiano,
                                 StageTarget::Kind::UprightPiano, StageTarget::Kind::Rack,
                                 StageTarget::Kind::Speaker })
        QVERIFY2(!StageTarget::isHandPropKind(k), qPrintable(StageTarget::kindToString(k)));
    QCOMPARE(StageTarget::kindToString(StageTarget::Kind::Staff), QString("staff"));
    QCOMPARE(StageTarget::stringToKind("candle"), StageTarget::Kind::Candle);
    QCOMPARE(StageTarget::defaultPropLength(StageTarget::Kind::Staff), 1.8f);

    // A person given as a holder of another person, or a mark held: nothing.
    MonitorProperties *mp = m_doc->monitorProperties();
    StageTarget *ann = person(mp, "Ann", 2, 2);
    StageTarget *bob = person(mp, "Bob", 4, 2);
    StageTarget::Blocked b;
    b.hold = StageTarget::Hold::HeldBy;
    b.holder = bob->id();
    ann->setBlocked(0, b);
    QVERIFY(mp->targetHolder(ann) == nullptr);
    QVERIFY(near3(mp->targetPosition(ann), QVector3D(2, 2, 0)));
    StageTarget *staff = prop(mp, StageTarget::Kind::Staff, "Staff", 0, 0);
    StageTarget *mark = prop(mp, StageTarget::Kind::None, "Mark", 1, 1);
    mp->setTargetHold(staff, 0, int(StageTarget::Hold::HeldBy), mark->id());
    QVERIFY(mp->targetHolder(staff) == nullptr);       // only a Person holds
    QVERIFY(near3(mp->targetPosition(staff), QVector3D(0, 0, 0)));
}

/* BLK-02: a held prop is in its holder's hand -- their place in that
   blocking, a hand to their right and in front, at hand height on whatever
   they stand on -- and moves with them in every blocking. Not on stage, it
   is not drawn; its holder removed, it is back at its own place. */
void HeldProps_Test::aHeldPropFollowsItsHolderInEveryBlocking()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    StageTarget *ann = person(mp, "Ann", 2, 2);
    StageTarget *staff = prop(mp, StageTarget::Kind::Staff, "Staff", 8, 1);
    const quint32 apron = mp->addBlockingSet("Apron");
    StageTarget::Blocked moved;
    moved.hasOffset = true;
    moved.offset = QVector3D(1.0f, 1.5f, 0.0f);
    ann->setBlocked(apron, moved);

    mp->setTargetHold(staff, 0, int(StageTarget::Hold::HeldBy), ann->id());
    QCOMPARE(mp->targetHolder(staff, 0), ann);
    QCOMPARE(mp->targetHolder(staff, apron), ann);       // the set says nothing: Default's hold
    const QVector3D hand0 = mp->targetPosition(staff, 0);
    const QVector3D handA = mp->targetPosition(staff, apron);
    QVERIFY2(near3(hand0, mp->handPoint(ann, 0)), qPrintable(str(hand0)));
    QVERIFY(hand0.x() < 2.0f && hand0.x() > 1.5f);       // her right: stage right of her
    QVERIFY(hand0.y() > 2.0f && hand0.y() < 2.3f);       // a little in front
    QVERIFY(hand0.z() > 0.8f && hand0.z() < 1.2f);       // hand height
    QVERIFY2(near3(handA - hand0, QVector3D(1.0f, 1.5f, 0.0f)), qPrintable(str(handA - hand0)));

    // She walks in Default: it goes with her, in every blocking.
    ann->setPosition(QVector3D(5, 3, 0));
    QVERIFY(near3(mp->targetPosition(staff, 0) - hand0, QVector3D(3, 1, 0)));
    QVERIFY(near3(mp->targetPosition(staff, apron) - handA, QVector3D(3, 1, 0)));

    // On a riser it rises with her feet; a taller person holds it higher.
    StagePlatform *riser = mp->addPlatform();
    riser->setOriginX(4); riser->setOriginY(2);
    riser->setWidth(2); riser->setDepth(2); riser->setHeight(0.4f);
    QVERIFY(qAbs(mp->targetPosition(staff, 0).z() - (hand0.z() + 0.4f)) < 0.01f);
    ann->setPersonHeight(2.0f);
    QVERIFY(mp->targetPosition(staff, 0).z() > hand0.z() + 0.5f);

    // The live blocking, and a scope, pick which one is read.
    QVERIFY(mp->setLiveBlocking(apron));
    QVERIFY(near3(mp->targetPosition(staff), mp->handPoint(ann, apron)));
    {
        BlockingScope scope(0);
        QVERIFY(near3(mp->targetPosition(staff), mp->handPoint(ann, 0)));
    }
    mp->setLiveBlocking(0);

    // The wand reads its holder through the same feet.
    QVERIFY(near3(mp->holderFeet(ann, 0), QVector3D(5, 3, 0.4f)));

    // Off stage in the apron number: not drawn there, still held in Default.
    mp->setTargetHold(staff, apron, int(StageTarget::Hold::OffStage));
    QVERIFY(!mp->targetOnStage(staff, apron));
    QVERIFY(mp->targetOnStage(staff, 0));
    QVERIFY(mp->targetHolder(staff, apron) == nullptr);
    QVERIFY(mp->targetOnStage(ann, apron));               // people are always on stage

    // She leaves the show: what she held is back at its own place.
    mp->removeStageTarget(ann->id());
    QVERIFY(mp->targetHolder(staff, 0) == nullptr);
    QVERIFY(near3(mp->targetPosition(staff, 0), QVector3D(8, 1, 0)));
    QCOMPARE(staff->holdIn(0), StageTarget::Hold::OwnPlace);
    QCOMPARE(staff->holdIn(apron), StageTarget::Hold::OffStage);
}

/* BLK-03: "cued as to when they get the prop and when it's taken from
   them". Five cues: 1 Default (the staff on its stand), 2 "Ann has it"
   (she gets it), 3 sets nothing (tracks: still hers), 4 "Bob has it"
   (handed on), 5 "Bare stage" (taken: not on stage). */
void HeldProps_Test::cuesGiveTakeAndHandOnWithTracking()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    StageTarget *ann = person(mp, "Ann", 2, 2);
    StageTarget *bob = person(mp, "Bob", 6, 2);
    StageTarget *staff = prop(mp, StageTarget::Kind::Staff, "Staff", 9, 0.5f);
    const quint32 annHas = mp->addBlockingSet("Ann has it");
    const quint32 bobHas = mp->addBlockingSet("Bob has it");
    const quint32 bare = mp->addBlockingSet("Bare stage");
    QCOMPARE(HeldProps::setHold(mp, { staff->id() }, annHas, StageTarget::Hold::HeldBy, ann->id()), 1);
    QCOMPARE(HeldProps::setHold(mp, { staff->id() }, annHas, StageTarget::Hold::HeldBy, ann->id()), 0);
    QCOMPARE(HeldProps::setHold(mp, { staff->id() }, bobHas, StageTarget::Hold::HeldBy, bob->id()), 1);
    QCOMPARE(HeldProps::setHold(mp, { staff->id() }, bare, StageTarget::Hold::OffStage), 1);
    QCOMPARE(HeldProps::setHold(mp, { ann->id() }, bare, StageTarget::Hold::OffStage), 0);   // not a prop

    ShowTreeController *st = m_doc->showTree();
    const quint32 listId = st->addCueList(st->addProduction("P"), "Main", ListMode::Tracking);
    QList<quint32> c;
    for (int i = 0; i < 5; ++i)
        c << st->addCue(listId, i - 1, QString("Cue %1").arg(i + 1));
    QVERIFY(st->setCuesBlocking({ c[1] }, annHas));
    QVERIFY(st->setCuesBlocking({ c[3] }, bobHas));
    QVERIFY(st->setCuesBlocking({ c[4] }, bare));

    auto holderIn = [&](int i) { return mp->targetHolder(staff, st->cueBlocking(c[i])); };
    QVERIFY(holderIn(0) == nullptr);
    QVERIFY(near3(mp->targetPosition(staff, st->cueBlocking(c[0])), QVector3D(9, 0.5f, 0)));
    QCOMPARE(holderIn(1), ann);                                      // she gets it
    QCOMPARE(holderIn(2), ann);                                      // a cue that says nothing keeps it
    QVERIFY(near3(mp->targetPosition(staff, st->cueBlocking(c[2])), mp->handPoint(ann, annHas)));
    QCOMPARE(holderIn(3), bob);                                      // handed on
    QVERIFY(near3(mp->targetPosition(staff, st->cueBlocking(c[3])), mp->handPoint(bob, bobHas)));
    QVERIFY(holderIn(4) == nullptr);                                 // taken
    QVERIFY(!mp->targetOnStage(staff, st->cueBlocking(c[4])));
    // A skipped cue passes nothing on: with cue 2 skipped, cue 3 is Default's.
    QVERIFY(st->setCuesSkipped({ c[1] }, true));
    QVERIFY(holderIn(2) == nullptr);
}

/* BLK-04: a show with no holds reads exactly as before: every target where
   positionIn() says, a staff with no hold at its own place in every set. */
void HeldProps_Test::defaultIsUnchanged()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    StageTarget *ann = person(mp, "Ann", 2, 2);
    StageTarget *mark = prop(mp, StageTarget::Kind::None, "DSC", 5, 4);
    StageTarget *staff = prop(mp, StageTarget::Kind::Staff, "Staff", 7, 1);
    const quint32 apron = mp->addBlockingSet("Apron");
    StageTarget::Blocked b;
    b.hasOffset = true;
    b.offset = QVector3D(0, 1.8f, 0);
    mark->setBlocked(apron, b);
    for (quint32 set : { quint32(0), apron })
    {
        for (StageTarget *t : { ann, mark, staff })
        {
            QVERIFY(near3(mp->targetPosition(t, set), t->positionIn(set)));
            QVERIFY(mp->targetOnStage(t, set));
            QVERIFY(mp->targetHolder(t, set) == nullptr);
        }
        QCOMPARE(staff->holdIn(set), StageTarget::Hold::OwnPlace);
    }
    // Nothing about holds is written for them.
    QByteArray buf;
    QXmlStreamWriter w(&buf);
    for (StageTarget *t : { ann, mark, staff })
        t->saveXML(&w);
    QVERIFY(!buf.contains("Hold="));
    QVERIFY(!buf.contains("Set=\"0\""));
    QVERIFY(!buf.contains("LitBy"));
    // Its own place in Default is the plain state: nothing kept.
    mp->setTargetHold(staff, 0, int(StageTarget::Hold::OwnPlace));
    QVERIFY(!staff->isBlockedIn(0));
}

/* BLK-05: the holds are saved with the blocking (each set's, and
   Default's as Set="0"), with the kinds and a candle's remote. */
void HeldProps_Test::holdsSurviveSaveAndLoad()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    StageTarget *ann = person(mp, "Ann", 2, 2);
    StageTarget *bob = person(mp, "Bob", 6, 2);
    StageTarget *staff = prop(mp, StageTarget::Kind::Staff, "Staff", 8, 1);
    StageTarget *candle = prop(mp, StageTarget::Kind::Candle, "Candle", 9, 1);
    staff->setPersonHeight(1.5f);
    candle->setLitBy(42);
    const quint32 a = mp->addBlockingSet("A");
    const quint32 b = mp->addBlockingSet("B");
    mp->setTargetHold(staff, 0, int(StageTarget::Hold::HeldBy), ann->id());
    mp->setTargetHold(staff, a, int(StageTarget::Hold::HeldBy), bob->id());
    mp->setTargetHold(staff, b, int(StageTarget::Hold::OwnPlace));
    mp->setTargetHold(candle, 0, int(StageTarget::Hold::OffStage));
    mp->setTargetHold(candle, a, int(StageTarget::Hold::HeldBy), ann->id());
    StageTarget::Blocked moved = candle->blocked(b);
    moved.hasOffset = true;
    moved.offset = QVector3D(-1, 0, 0);
    candle->setBlocked(b, moved);

    const QByteArray snap = mp->snapshot(m_doc);
    QVERIFY(snap.contains("Hold=\"held\""));
    QVERIFY(snap.contains("Kind=\"staff\""));

    Doc other(this);
    MonitorProperties *mp2 = other.monitorProperties();
    QVERIFY(mp2->restoreSnapshot(snap, &other));
    StageTarget *s2 = mp2->stageTarget(staff->id());
    StageTarget *c2 = mp2->stageTarget(candle->id());
    QVERIFY(s2 != nullptr && c2 != nullptr);
    QCOMPARE(s2->kind(), StageTarget::Kind::Staff);
    QCOMPARE(c2->kind(), StageTarget::Kind::Candle);
    QCOMPARE(s2->propLength(), 1.5f);
    QCOMPARE(c2->litBy(), quint32(42));
    QCOMPARE(mp2->targetHolder(s2, 0)->name(), QString("Ann"));
    QCOMPARE(mp2->targetHolder(s2, a)->name(), QString("Bob"));
    QVERIFY(mp2->targetHolder(s2, b) == nullptr);
    QCOMPARE(s2->holdIn(b), StageTarget::Hold::OwnPlace);
    QVERIFY(!mp2->targetOnStage(c2, 0));
    QCOMPARE(mp2->targetHolder(c2, a)->name(), QString("Ann"));
    QVERIFY(!mp2->targetOnStage(c2, b));                   // B says nothing of its hold: Default's
    QVERIFY(near3(c2->positionIn(b), QVector3D(8, 1, 0))); // its place there kept
}

/* BLK-06: a show saved with held props, opened by a build before them. That
   build's loader (as of train/045a) reads an unknown Kind as a plain mark,
   ignores unknown attributes, and keeps a Blocked only when its set is not
   0 and it has a place, a line or a size. Run here as that loader would:
   the same file with what it does not know taken out loads as plain marks
   at their own Default places, and this build reading a hold it does not
   know (a newer one) keeps what Default says. */
void HeldProps_Test::anOlderBuildSeesThePropAtItsPlace()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    StageTarget *ann = person(mp, "Ann", 2, 2);
    StageTarget *staff = prop(mp, StageTarget::Kind::Staff, "Staff", 8, 1);
    const quint32 a = mp->addBlockingSet("A");
    mp->setTargetHold(staff, 0, int(StageTarget::Hold::HeldBy), ann->id());
    mp->setTargetHold(staff, a, int(StageTarget::Hold::OffStage));
    QByteArray buf;
    {
        QXmlStreamWriter w(&buf);
        staff->saveXML(&w);
    }
    // Every hold is a Blocked with no place of its own.
    QVERIFY(buf.contains("<Blocked Set=\"0\" Hold=\"held\""));
    QVERIFY(buf.contains("<Blocked Set=\"" + QByteArray::number(a) + "\" Hold=\"off\"/>"));
    QVERIFY(!buf.contains("DX="));

    // What the older loader sees: the kind unknown, the holds dropped.
    QByteArray old = buf;
    old.replace("Kind=\"staff\"", "Kind=\"wobble\"");
    old.replace("Hold=\"held\"", "Hold=\"levitating\"");
    old.replace("Hold=\"off\"", "Hold=\"levitating\"");
    StageTarget seen(99);
    QXmlStreamReader r(old);
    r.readNextStartElement();
    QVERIFY(seen.loadXML(r));
    QCOMPARE(seen.kind(), StageTarget::Kind::None);
    QVERIFY(seen.blockedSets().isEmpty());
    QVERIFY(near3(seen.positionIn(a), QVector3D(8, 1, 0)));
    QCOMPARE(seen.holdIn(a), StageTarget::Hold::OwnPlace);
}

/* BLK-07: "give one to each of the selected people": N people, N candles,
   each held by one, in the blocking shown; not on stage in Default; their
   own places beside where each one stands. (One undo step: heldprops UI
   test.) */
void HeldProps_Test::giveEachMakesOneEach()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    QList<quint32> people;
    for (int i = 0; i < 5; ++i)
        people << person(mp, QString("Choir %1").arg(i + 1), 1.0f + i, 3.0f)->id();
    StageTarget *mark = prop(mp, StageTarget::Kind::None, "Mark", 0, 0);
    const quint32 candles = mp->addBlockingSet("Candles");
    const int before = mp->stageTargets().size();
    const QList<quint32> made = HeldProps::giveEach(mp, people + QList<quint32>{ mark->id() },
                                                    StageTarget::Kind::Candle, candles);
    QCOMPARE(made.size(), 5);                                  // the mark gets none
    QCOMPARE(mp->stageTargets().size(), before + 5);
    for (int i = 0; i < 5; ++i)
    {
        StageTarget *c = mp->stageTarget(made.at(i));
        QCOMPARE(c->kind(), StageTarget::Kind::Candle);
        QCOMPARE(mp->targetHolder(c, candles)->id(), people.at(i));
        QVERIFY(!mp->targetOnStage(c, 0));
        QVERIFY(c->name().contains(QString("Choir %1").arg(i + 1)));
        QVERIFY(qAbs(c->position().x() - (1.0f + i)) < 0.5f);
    }
    // In Default, they are held in Default.
    const QList<quint32> staffs = HeldProps::giveEach(mp, { people.first() }, StageTarget::Kind::Staff, 0);
    QCOMPARE(staffs.size(), 1);
    QCOMPARE(mp->targetHolder(mp->stageTarget(staffs.first()), 0)->id(), people.first());
    QVERIFY(HeldProps::giveEach(mp, people, StageTarget::Kind::Person, 0).isEmpty());
}

/* BLK-08, answer 2 ("given it's lit it won't matter"): an Aim at a person
   covers the same heads, to the same DMX, with and without a staff in
   their hand -- in Default and in a blocking. */
void HeldProps_Test::anAimAtThePersonIgnoresTheProp()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    Truss *pipe = mp->addTruss();
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

    StageTarget *ann = person(mp, "Ann", 0.5f, 2.0f);
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, m_doc);
    aim->setName("Ann");
    aim->setStageTargetId(ann->id());
    m_doc->addPalette(aim);
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    for (int i = 0; i < 2; ++i)
    {
        Fixture *spot = new Fixture(m_doc);
        spot->setName(QString("Spot %1").arg(i + 1));
        spot->setFixtureDefinition(def, mode);
        spot->setUniverse(0);
        spot->setAddress(100 + 10 * i);
        m_doc->addFixture(spot);
        FixtureRigProps rp;
        rp.trussId = pipe->id();
        rp.trussOffset = i == 0 ? 1.0f : 5.0f;
        rp.mountingType = Truss::TopHung;
        mp->setFixtureRigProps(spot->id(), rp);
        scene->addFixture(spot->id());
    }
    scene->addPalette(aim->id());
    const quint32 apron = mp->addBlockingSet("Apron");
    StageTarget::Blocked b;
    b.hasOffset = true;
    b.offset = QVector3D(0, 1.5f, 0);
    ann->setBlocked(apron, b);

    MasterTimer timer(m_doc);
    auto output = [&](quint32 set) {
        mp->setLiveBlocking(set);
        scene->start(&timer, FunctionParent::master());
        play(m_doc, timer);
        const QByteArray out = dmx(m_doc);
        scene->stop(FunctionParent::master());
        play(m_doc, timer, 2);
        return out;
    };
    const QVector3D chest = mp->targetAimPoint(ann, true, 0);
    const QByteArray bare0 = output(0);
    const QByteArray bareA = output(apron);
    QVERIFY(bare0 != bareA);                   // the blocking does move the beams

    const QList<quint32> staffs = HeldProps::giveEach(mp, { ann->id() }, StageTarget::Kind::Staff, 0);
    QCOMPARE(staffs.size(), 1);
    HeldProps::giveEach(mp, { ann->id() }, StageTarget::Kind::Candle, apron);
    QVERIFY(mp->targetHolder(mp->stageTarget(staffs.first()), apron) != nullptr);
    QVERIFY(near3(mp->targetAimPoint(ann, true, 0), chest));
    QCOMPARE(output(0), bare0);
    QCOMPARE(output(apron), bareA);
}

/* BLK-09: what a candle is drawn as, from what the show last sent the IR
   remote -- unknown before anything; each button; a sequence On, Dim,
   Brighten, Off; SELECT held still sends nothing, and WINDOW changing
   sends the button again. */
void HeldProps_Test::candleStateFollowsSelect()
{
    using CR = CandleRemote;
    // Each value, from not known.
    CR::State s;
    QVERIFY(!s.known && !s.lit);
    QCOMPARE(CR::command(0), CR::Nothing);
    QCOMPARE(CR::command(7), CR::Nothing);
    QCOMPARE(CR::command(255), CR::Nothing);
    QVERIFY(CR::apply(s, CR::Nothing) == s);
    QVERIFY(!CR::apply(s, CR::Dim).known);              // a level change lights nothing
    QVERIFY(!CR::apply(s, CR::Brighten).known);
    CR::State on = CR::apply(s, CR::On);
    QVERIFY(on.known && on.lit && !on.dimmed && !on.flicker);
    CR::State off = CR::apply(s, CR::Off);
    QVERIFY(off.known && !off.lit);
    CR::State flick = CR::apply(s, CR::Candle);
    QVERIFY(flick.known && flick.lit && flick.flicker);
    CR::State steady = CR::apply(flick, CR::Light);
    QVERIFY(steady.lit && !steady.flicker);
    // Off keeps the mode; On brings it back at full.
    CR::State dimFlick = CR::apply(CR::apply(flick, CR::Dim), CR::Off);
    QVERIFY(!dimFlick.lit && dimFlick.flicker);
    CR::State back = CR::apply(dimFlick, CR::On);
    QVERIFY(back.lit && back.flicker && !back.dimmed);  // "starts full on every start"

    // Through the fixture, as the plot reads it.
    QLCFixtureDef *def = remoteDef();
    QVERIFY(def != nullptr);
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(def));
    Fixture *remote = new Fixture(m_doc);
    remote->setName("Candles");
    remote->setFixtureDefinition(def, def->modes().first());
    remote->setUniverse(0);
    remote->setAddress(0);
    QVERIFY(m_doc->addFixture(remote));
    QVERIFY(CR::isRemote(remote));
    MonitorProperties *mp = m_doc->monitorProperties();
    StageTarget *candle = prop(mp, StageTarget::Kind::Candle, "Candle", 1, 1);
    CR *cr = CR::of(m_doc);
    QCOMPARE(CR::of(m_doc), cr);
    QCOMPARE(cr->remotes(), QList<quint32>{ remote->id() });
    QCOMPARE(cr->remoteFor(candle), remote->id());       // the only one
    QVERIFY(!cr->stateFor(candle).known);                // before any command

    QSignalSpy changed(cr, &CR::changed);
    auto send = [&](int select, int window) {
        QByteArray v(2, 0);
        v[0] = char(select);
        v[1] = char(window);
        remote->setOwnChannelValues(v);
        return cr->stateFor(candle);
    };
    CR::State st = send(1, 30);                          // On
    QVERIFY(st.known && st.lit && !st.dimmed);
    QCOMPARE(changed.count(), 1);
    st = send(5, 30);                                    // Dim
    QVERIFY(st.lit && st.dimmed);
    st = send(6, 30);                                    // Brighten
    QVERIFY(st.lit && !st.dimmed);
    st = send(2, 30);                                    // Off
    QVERIFY(st.known && !st.lit);
    st = send(0, 30);                                    // nothing
    QVERIFY(st.known && !st.lit);
    const int n = changed.count();
    st = send(3, 30);                                    // Candle
    QVERIFY(st.lit && st.flicker);
    st = send(4, 30);                                    // Light
    QVERIFY(st.lit && !st.flicker);
    st = send(5, 30);
    QVERIFY(st.dimmed);
    // WINDOW changed with SELECT the same: the button again (no change here).
    st = send(5, 31);
    QVERIFY(st.dimmed);
    QVERIFY(changed.count() > n);

    // A second remote: a candle with none named has none; named, its own.
    Fixture *second = new Fixture(m_doc);
    second->setName("Candles 2");
    second->setFixtureDefinition(def, def->modes().first());
    second->setUniverse(0);
    second->setAddress(10);
    QVERIFY(m_doc->addFixture(second));
    QCOMPARE(cr->remotes().size(), 2);
    QCOMPARE(cr->remoteFor(candle), UINT_MAX);
    QVERIFY(!cr->stateFor(candle).known);
    candle->setLitBy(remote->id());
    QVERIFY(cr->stateFor(candle).dimmed);

    // A new show: unknown again.
    cr->reset();
    QVERIFY(!cr->stateFor(candle).known);
}

/* BLK-10: the shipped definition loads, is clean by the definition check,
   is not a light, and its capabilities sit on the blaster's chart: SELECT
   0 nothing, 1..6 the buttons as single values, the rest nothing; WINDOW
   tenths of a second, 255 constantly. */
void HeldProps_Test::candleRemoteDefinitionMatchesTheChart()
{
    QLCFixtureDef *def = remoteDef();
    QVERIFY(def != nullptr);
    QCOMPARE(def->manufacturer(), CandleRemote::manufacturer());
    QCOMPARE(def->model(), CandleRemote::model());
    QCOMPARE(def->modes().size(), 1);
    QLCFixtureMode *mode = def->modes().first();
    QCOMPARE(mode->channels().size(), 2);
    QVERIFY(!FixtureDefCheck::expectsLight(mode));
    for (const FixtureDefCheck::Issue &i : FixtureDefCheck::check(def))
        QVERIFY2(i.level != FixtureDefCheck::Issue::Error, qPrintable(i.message));

    const QLCChannel *sel = mode->channel(0);
    QCOMPARE(sel->name(), QString("Select"));
    const struct { int v; const char *name; } chart[] = {
        { 0, "Nothing" }, { 1, "On" }, { 2, "Off" }, { 3, "Candle (flicker)" }, { 4, "Light (steady)" },
        { 5, "Dim" }, { 6, "Brighten" } };
    for (const auto &c : chart)
    {
        const QLCCapability *cap = sel->searchCapability(uchar(c.v));
        QVERIFY2(cap != nullptr, qPrintable(QString::number(c.v)));
        QCOMPARE(int(cap->min()), c.v);
        QCOMPARE(int(cap->max()), c.v);                 // a single value, not a range
        QCOMPARE(cap->name(), QString(c.name));
        if (c.v >= 1)
            QCOMPARE(int(CandleRemote::command(c.v)), c.v);
    }
    QCOMPARE(sel->searchCapability(uchar(7))->name(), QString("Nothing"));
    QCOMPARE(sel->searchCapability(uchar(255))->name(), QString("Nothing"));
    const QLCChannel *win = mode->channel(1);
    QCOMPARE(win->name(), QString("Window"));
    QCOMPARE(int(win->searchCapability(uchar(255))->min()), 255);
    QVERIFY(win->searchCapability(uchar(255))->name().contains("constantly"));
    QVERIFY(win->searchCapability(uchar(20))->name().contains("tenths"));
    delete def;
}

QTEST_MAIN(HeldProps_Test)
