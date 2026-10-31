/*
  Q Light Controller Plus
  groupmasters_test.cpp

  Copyright (C) Branson Matheson

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QBuffer>
#include <QtTest>

#define private public
#define protected public
#include "universe.h"
#include "mastertimer.h"
#undef private
#undef protected

#include "groupmasters_test.h"
#include "inputoutputmap.h"
#include "groupmasters.h"
#include "fixturegroup.h"
#include "mastertimer.h"
#include "parkeffect.h"
#include "collection.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfile.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

#include "../common/resource_paths.h"

namespace
{
const quint32 kDimmer = 1;      // MAC250+ Mode 1
const quint32 kPan = 7;
const int kMoverStride = 20;    // movers at 0, 20, 40
const int kPackAt = 100;        // the dimmer pack's first channel

int out(Universe *u, int address)
{
    return uchar(u->postGMValues()->at(address));
}
}

void GroupMasters_Test::init()
{
    m_doc = new Doc(this);
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir));
    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef("Martin", "MAC250+");
    QVERIFY(def != nullptr);
    QLCFixtureMode *mode = def->mode("Mode 1");
    QVERIFY(mode != nullptr);

    m_movers.clear();
    for (int i = 0; i < 3; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Mover %1").arg(i + 1));
        fxi->setFixtureDefinition(def, mode);
        fxi->setUniverse(0);
        fxi->setAddress(quint32(i * kMoverStride));
        QVERIFY(m_doc->addFixture(fxi));
        m_movers << fxi->id();
    }
    Fixture *pack = new Fixture(m_doc);
    pack->setName("Pack");
    pack->setUniverse(0);
    pack->setAddress(kPackAt);
    pack->setChannels(4);
    QVERIFY(m_doc->addFixture(pack));
    QCOMPARE(pack->heads(), 4);
    m_pack = pack->id();
}

void GroupMasters_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    frame(3);
    delete m_doc;
    m_doc = nullptr;
}

void GroupMasters_Test::frame(int frames)
{
    for (int i = 0; i < frames; ++i)
    {
        m_doc->masterTimer()->timerTick();
        for (Universe *u : m_doc->inputOutputMap()->universes())
            u->processFaders();
    }
}

Universe *GroupMasters_Test::uni() const
{
    return m_doc->inputOutputMap()->universes().first();
}

FixtureGroup *GroupMasters_Test::group(const QString &name, const QList<quint32> &fixtures, int level)
{
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName(name);
    grp->setSize(QSize(8, 1));
    m_doc->addFixtureGroup(grp);
    for (quint32 id : fixtures)
        grp->assignFixture(id);
    if (level >= 0)
    {
        grp->setMaster(true);
        grp->setMasterLevel(uchar(level));
    }
    m_doc->groupMasters()->rebuild();
    return grp;
}

Scene *GroupMasters_Test::look(const QList<quint32> &fixtures, uchar dimmer, uchar pan)
{
    Scene *s = new Scene(m_doc);
    for (quint32 id : fixtures)
    {
        if (id == m_pack)
        {
            for (quint32 ch = 0; ch < 4; ++ch)
                s->setValue(id, ch, dimmer);
            continue;
        }
        s->setValue(id, kDimmer, dimmer);
        s->setValue(id, kPan, pan);
    }
    s->setFadeInSpeed(0);
    s->setFadeOutSpeed(0);
    m_doc->addFunction(s);
    return s;
}

/* The brief: a cue at full with its group's master at 50% gives half the
   intensity; its position (LTP) is untouched; a fixture outside the group
   is untouched; and what the faders hold (pre-GM) is the cue's, so
   nothing that reads under the master sees it. */
void GroupMasters_Test::aCueAtFullUnderAHalfMasterIsHalf()
{
    FixtureGroup *front = group("Front", { m_movers[0], m_movers[1] }, 128);
    QCOMPARE(m_doc->groupMasters()->groups(), QList<FixtureGroup *>() << front);
    Scene *s = look(m_movers, 255, 200);
    s->start(m_doc->masterTimer(), FunctionParent::master());
    frame(3);

    QCOMPARE(out(uni(), kDimmer), 128);
    QCOMPARE(out(uni(), kMoverStride + kDimmer), 128);
    QCOMPARE(out(uni(), kPan), 200);                          // LTP: as the cue says
    QCOMPARE(out(uni(), 2 * kMoverStride + kDimmer), 255);    // not in the group
    QCOMPARE(int(uchar(uni()->preGMValues().at(kDimmer))), 255);

    // At full the master changes nothing; at 0 the group is dark, still aimed.
    front->setMasterLevel(255);
    frame();
    QCOMPARE(out(uni(), kDimmer), 255);
    front->setMasterLevel(0);
    frame();
    QCOMPARE(out(uni(), kDimmer), 0);
    QCOMPARE(out(uni(), kPan), 200);

    // A group without a master: its level does nothing.
    front->setMaster(false);
    frame();
    QCOMPARE(out(uni(), kDimmer), 255);
    QVERIFY(m_doc->groupMasters()->groups().isEmpty());
}

/* A fader moved while nothing new is written still reaches the output:
   the universe works the channels it changed out again at once, as it does
   for a Grand Master move. */
void GroupMasters_Test::aMasterMovesTheStageWithoutAFrame()
{
    FixtureGroup *front = group("Front", { m_movers[0] }, 255);
    Scene *s = look({ m_movers[0] }, 200, 10);
    s->start(m_doc->masterTimer(), FunctionParent::master());
    frame(3);
    QCOMPARE(out(uni(), kDimmer), 200);
    front->setMasterLevel(64);
    QCOMPARE(out(uni(), kDimmer), 50);
    QCOMPARE(int(uni()->groupMaster(kDimmer)), 64);
    QCOMPARE(int(uni()->groupMaster(kPan)), 64);       // marked; the universe leaves LTP be
    QCOMPARE(out(uni(), kPan), 10);
}

void GroupMasters_Test::theGrandMasterMultipliesWithIt()
{
    group("Front", { m_movers[0] }, 128);
    Scene *s = look({ m_movers[0] }, 255, 10);
    s->start(m_doc->masterTimer(), FunctionParent::master());
    m_doc->inputOutputMap()->setGrandMasterValue(128);
    frame(3);
    QCOMPARE(out(uni(), kDimmer), 64);
    m_doc->inputOutputMap()->setGrandMasterValue(255);
    frame();
    QCOMPARE(out(uni(), kDimmer), 128);
}

/* An override hit wins over the show on its channels, and a master scales
   what it sends too: "house to full" under a half master is half. */
void GroupMasters_Test::anOverrideHitIsScaledToo()
{
    FixtureGroup *front = group("Front", { m_movers[0] }, 128);
    Scene *base = look({ m_movers[0] }, 100, 10);
    Scene *flood = look({ m_movers[0] }, 255, 90);
    Collection *hit = new Collection(m_doc);
    hit->addFunction(flood->id());
    hit->setHitOverride(true);
    hit->setHitFades(0, 0);
    m_doc->addFunction(hit);

    base->start(m_doc->masterTimer(), FunctionParent::master());
    frame(3);
    QCOMPARE(out(uni(), kDimmer), 50);
    hit->start(m_doc->masterTimer(), FunctionParent::master());
    frame(4);
    QCOMPARE(int(uchar(uni()->preGMValues().at(kDimmer))), 255);
    QCOMPARE(out(uni(), kDimmer), 128);
    QCOMPARE(out(uni(), kPan), 90);

    front->setMasterLevel(255);
    QCOMPARE(out(uni(), kDimmer), 255);
    front->setMasterLevel(128);
    hit->stop(FunctionParent::master());
    frame(4);
    QCOMPARE(out(uni(), kDimmer), 50);                 // back to the show, still mastered
}

/* decided (can change): a head in two mastered groups is scaled by both,
   multiplied, as a master and the Grand Master are; a group that holds the
   same head twice still scales it once. */
void GroupMasters_Test::overlappingGroupsMultiply()
{
    group("All", { m_movers[0], m_movers[1] }, 128);
    FixtureGroup *sl = group("Stage left", { m_movers[1] }, 128);
    Scene *s = look({ m_movers[0], m_movers[1] }, 255, 0);
    s->start(m_doc->masterTimer(), FunctionParent::master());
    frame(3);
    QCOMPARE(out(uni(), kDimmer), 128);
    QCOMPARE(out(uni(), kMoverStride + kDimmer), 64);

    sl->setMasterLevel(0);
    QCOMPARE(out(uni(), kDimmer), 128);
    QCOMPARE(out(uni(), kMoverStride + kDimmer), 0);
    sl->setMasterLevel(255);
    QCOMPARE(out(uni(), kMoverStride + kDimmer), 128);
}

/* A group of some heads of a fixture: those heads, not the others. */
void GroupMasters_Test::partOfAFixtureLeavesItsOtherHeads()
{
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Pack 1-2");
    grp->setSize(QSize(4, 1));
    m_doc->addFixtureGroup(grp);
    QVERIFY(grp->assignHead(QLCPoint(0, 0), GroupHead(m_pack, 0)));
    QVERIFY(grp->assignHead(QLCPoint(1, 0), GroupHead(m_pack, 1)));
    grp->setMaster(true);
    grp->setMasterLevel(128);

    Scene *s = look({ m_pack }, 255, 0);
    s->start(m_doc->masterTimer(), FunctionParent::master());
    frame(3);
    QCOMPARE(out(uni(), kPackAt), 128);
    QCOMPARE(out(uni(), kPackAt + 1), 128);
    QCOMPARE(out(uni(), kPackAt + 2), 255);
    QCOMPARE(out(uni(), kPackAt + 3), 255);
}

/* decided (can change): a master leaves what the Grand Master leaves: a
   parked channel sends its park, an Ignore-GM channel its level. */
void GroupMasters_Test::parkAndIgnoreGmAreNotScaled()
{
    group("Front", { m_movers[0], m_movers[1] }, 128);
    Scene *s = look({ m_movers[0], m_movers[1] }, 255, 0);
    s->start(m_doc->masterTimer(), FunctionParent::master());

    QByteArray rules(UNIVERSE_SIZE, char(0));
    rules[kMoverStride + kDimmer] = char(Universe::GMExempt);
    uni()->setOutputRules(rules);

    ParkEffect park(m_doc);
    park.parkFixture(m_movers[0], { { kDimmer, 200 } });
    frame(3);
    QCOMPARE(out(uni(), kDimmer), 200);
    QCOMPARE(out(uni(), kMoverStride + kDimmer), 255);
    park.unparkAll();
    frame(2);
    QCOMPARE(out(uni(), kDimmer), 128);
}

/* Saved with the group: on, with its level. Absent (a group saved before,
   or by QLC+ 4): no master. */
void GroupMasters_Test::saveAndLoad()
{
    FixtureGroup *front = group("Front", { m_movers[0] }, 90);
    FixtureGroup *plain = group("Plain", { m_movers[1] });

    auto roundTrip = [this](FixtureGroup *grp) {
        QBuffer buffer;
        buffer.open(QIODevice::WriteOnly | QIODevice::Text);
        QXmlStreamWriter xmlWriter(&buffer);
        grp->saveXML(&xmlWriter);
        xmlWriter.setDevice(nullptr);
        buffer.close();
        const QByteArray xml = buffer.data();
        buffer.open(QIODevice::ReadOnly | QIODevice::Text);
        QXmlStreamReader xmlReader(&buffer);
        xmlReader.readNextStartElement();
        FixtureGroup *back = new FixtureGroup(m_doc);
        back->loadXML(xmlReader);
        return qMakePair(back, xml);
    };

    auto a = roundTrip(front);
    QVERIFY(a.second.contains("<Master Level=\"90\"/>"));
    QVERIFY(a.first->hasMaster());
    QCOMPARE(int(a.first->masterLevel()), 90);
    delete a.first;

    auto b = roundTrip(plain);
    QVERIFY(!b.second.contains("Master"));
    QVERIFY(!b.first->hasMaster());
    QCOMPARE(int(b.first->masterLevel()), 255);
    delete b.first;

    // A master with no level said is at full.
    QByteArray xml("<FixtureGroup ID=\"7\"><Name>X</Name><Master/></FixtureGroup>");
    QBuffer in(&xml);
    in.open(QIODevice::ReadOnly);
    QXmlStreamReader r(&in);
    r.readNextStartElement();
    FixtureGroup g(m_doc);
    QVERIFY(g.loadXML(r));
    QVERIFY(g.hasMaster());
    QCOMPARE(int(g.masterLevel()), 255);

    // A copy keeps it.
    FixtureGroup copy(m_doc);
    copy.copyFrom(front);
    QVERIFY(copy.hasMaster());
    QCOMPARE(int(copy.masterLevel()), 90);
}

/* A group deleted, or emptied, takes its master off the stage. */
void GroupMasters_Test::aDeletedGroupLetsGo()
{
    FixtureGroup *front = group("Front", { m_movers[0] }, 128);
    Scene *s = look({ m_movers[0] }, 255, 0);
    s->start(m_doc->masterTimer(), FunctionParent::master());
    frame(3);
    QCOMPARE(out(uni(), kDimmer), 128);
    front->resignFixture(m_movers[0]);
    QCoreApplication::processEvents();      // an edit: coalesced
    frame();
    QCOMPARE(out(uni(), kDimmer), 255);

    front->assignFixture(m_movers[0]);
    QCoreApplication::processEvents();
    frame();
    QCOMPARE(out(uni(), kDimmer), 128);
    m_doc->deleteFixtureGroup(front->id());
    QCoreApplication::processEvents();
    frame();
    QCOMPARE(out(uni(), kDimmer), 255);
    QCOMPARE(int(uni()->groupMaster(kDimmer)), 255);
}

QTEST_GUILESS_MAIN(GroupMasters_Test)
