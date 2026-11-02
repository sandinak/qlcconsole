/*
  Q Light Controller Plus - Test Unit
  structureduplicate_test.cpp

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

#include <climits>
#include <QtTest>

#include "structureduplicate_test.h"
#include "structureduplicate.h"
#include "monitorproperties.h"
#include "hanger.h"
#include "truss.h"
#include "patchplanner.h"
#include "fixturegroup.h"
#include "scene.h"
#include "fixture.h"
#include "doc.h"

typedef StructureDuplicate SD;

void StructureDuplicate_Test::init()
{
    m_doc = new Doc(this);
}

void StructureDuplicate_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

/* A dimmer of @p channels at @p universe/@p address, hung on @p truss at
   @p offset metres, with a calibration offset so a copy can be told from a
   fresh one. */
static Fixture *hung(Doc *doc, Truss *truss, const QString &name, int channels, quint32 universe,
                     quint32 address, float offset)
{
    Fixture *fx = new Fixture(doc);
    fx->setChannels(quint32(channels));
    fx->setUniverse(universe);
    fx->setAddress(address);
    fx->setPatched(true);
    fx->setName(name);
    if (!doc->addFixture(fx))
    {
        delete fx;
        return nullptr;
    }
    FixtureRigProps rp;
    rp.trussId = truss->id();
    rp.trussOffset = offset;
    rp.trussCross = 0.25f;
    rp.panOffsetDeg = 7.0f;
    rp.tiltOffsetDeg = -3.0f;
    rp.panInvert = true;
    rp.mountZOffset = 0.1f;
    doc->monitorProperties()->setFixtureRigProps(fx->id(), rp);
    FixturePreviewItem item;
    item.m_baseItem.m_position = doc->monitorProperties()->fixtureRigPosition(fx->id()) * 1000.0f;
    doc->monitorProperties()->setFixtureProperties(fx->id(), item);
    return fx;
}

/* T-1 .. T-3 trusses; T-1 carries three lights (10 channels each) named for
   it, in universe 1 from address 1. */
struct Rig
{
    Truss *t1 = nullptr, *t2 = nullptr, *t3 = nullptr;
    QList<Fixture *> lights;
    QString k1, k2, k3;
};

static Rig makeRig(Doc *doc)
{
    Rig r;
    MonitorProperties *props = doc->monitorProperties();
    auto truss = [props](const QString &name, float x) {
        Truss *t = props->addTruss();
        t->setName(name);
        t->setOrigin(QVector3D(x, 0.0f, 4.0f));
        t->setLength(4.0f);
        return t;
    };
    r.t1 = truss("T-1", 0.0f);
    r.t2 = truss("T-2", 5.0f);
    r.t3 = truss("T-3", 10.0f);
    r.lights << hung(doc, r.t1, "T1-Spot 1", 10, 0, 0, 0.5f)
             << hung(doc, r.t1, "T1-Spot 2", 10, 0, 10, 1.5f)
             << hung(doc, r.t1, "T1-Wash", 10, 0, 20, 2.5f);
    r.k1 = QString("truss:%1").arg(r.t1->id());
    r.k2 = QString("truss:%1").arg(r.t2->id());
    r.k3 = QString("truss:%1").arg(r.t3->id());
    return r;
}

static QList<Fixture *> on(Doc *doc, const QString &key)
{
    QList<Fixture *> out;
    foreach (quint32 f, PatchPlanner::fixturesOn(doc, key))
        out << doc->fixture(f);
    return out;
}

void StructureDuplicate_Test::namesFollowTheStructure()
{
    QCOMPARE(SD::renamed("T1-Spot 3", "T-1", "T-2"), QString("T2-Spot 3"));     // the compact spelling
    QCOMPARE(SD::renamed("T-1 Wash", "T-1", "T-2"), QString("T-2 Wash"));
    QCOMPARE(SD::renamed("US1t1 Bar", "US1t1", "US1t2"), QString("US1t2 Bar"));
    QCOMPARE(SD::renamed("T1 and T10", "T1", "T2"), QString("T2 and T10"));     // T10 is not T1
    QCOMPARE(SD::renamed("AT1-3z", "T1", "T2"), QString("T2 AT1-3z"));          // AT1 is not T1
    QCOMPARE(SD::renamed("Spot 3", "T-1", "T-2"), QString("T-2 Spot 3"));       // no trace: prefixed
}

void StructureDuplicate_Test::aTowerIsDuplicatedToTwo()
{
    const Rig r = makeRig(m_doc);
    QCOMPARE(SD::targetsFor(m_doc, r.k1), QStringList({ r.k2, r.k3 }));

    SD::Options opt;
    const SD::Result res = SD::apply(m_doc, r.k1, { r.k2, r.k3 }, opt);
    QCOMPARE(res.created, 6);
    QCOMPARE(res.patched, 6);
    QCOMPARE(on(m_doc, r.k2).size(), 3);
    QCOMPARE(on(m_doc, r.k3).size(), 3);
    QCOMPARE(on(m_doc, r.k1).size(), 3);                // the original untouched
    QStringList names;
    foreach (Fixture *f, on(m_doc, r.k2))
        names << f->name();
    QCOMPARE(names, QStringList({ "T2-Spot 1", "T2-Spot 2", "T2-Wash" }));
    names.clear();
    foreach (Fixture *f, on(m_doc, r.k3))
        names << f->name();
    QCOMPARE(names, QStringList({ "T3-Spot 1", "T3-Spot 2", "T3-Wash" }));
}

void StructureDuplicate_Test::theCopiesAreMountedLikeTheOriginals()
{
    const Rig r = makeRig(m_doc);
    SD::apply(m_doc, r.k1, { r.k2 }, SD::Options());
    MonitorProperties *props = m_doc->monitorProperties();
    const QList<Fixture *> a = on(m_doc, r.k1), b = on(m_doc, r.k2);
    QCOMPARE(a.size(), b.size());
    for (int i = 0; i < a.size(); ++i)
    {
        const FixtureRigProps ra = props->fixtureRigProps(a.at(i)->id());
        const FixtureRigProps rb = props->fixtureRigProps(b.at(i)->id());
        QCOMPARE(rb.trussId, r.t2->id());
        QCOMPARE(rb.trussOffset, ra.trussOffset);
        QCOMPARE(rb.trussCross, ra.trussCross);
        QCOMPARE(rb.mountZOffset, ra.mountZOffset);
        QCOMPARE(rb.panOffsetDeg, ra.panOffsetDeg);         // calibration offsets travel
        QCOMPARE(rb.tiltOffsetDeg, ra.tiltOffsetDeg);
        QCOMPARE(rb.panInvert, ra.panInvert);
        QCOMPARE(b.at(i)->channels(), a.at(i)->channels());
        // Placed where the other truss puts it, 5 m on from the first.
        const QVector3D pa = props->fixtureRigPosition(a.at(i)->id());
        const QVector3D pb = props->fixtureRigPosition(b.at(i)->id());
        QVERIFY2(qAbs((pb.x() - pa.x()) - 5.0f) < 0.01f, qPrintable(QString::number(double(pb.x() - pa.x()))));
        // The plot's own cache follows (in millimetres).
        const QVector3D cache = props->fixtureProperties(b.at(i)->id()).m_baseItem.m_position;
        QVERIFY((cache - pb * 1000.0f).length() < 1.0f);
    }
}

void StructureDuplicate_Test::nextFreeFollowsTheOriginals()
{
    const Rig r = makeRig(m_doc);
    const SD::Preview pv = SD::preview(m_doc, r.k1, { r.k2, r.k3 }, SD::Options());
    QCOMPARE(pv.rows.size(), 6);
    // T-1 ends at address 30; the copies run on from there, in order.
    for (int i = 0; i < 6; ++i)
    {
        QVERIFY(pv.rows.at(i).patched);
        QCOMPARE(pv.rows.at(i).universe, quint32(0));
        QCOMPARE(pv.rows.at(i).address, quint32(30 + i * 10));
    }
    // Nothing changed by looking.
    QCOMPARE(m_doc->fixtures().size(), 3);

    SD::apply(m_doc, r.k1, { r.k2, r.k3 }, SD::Options());
    QCOMPARE(on(m_doc, r.k2).first()->address(), quint32(30));
    QCOMPARE(on(m_doc, r.k3).last()->address(), quint32(80));
}

void StructureDuplicate_Test::anOffsetPatternIsApplied()
{
    const Rig r = makeRig(m_doc);
    SD::Options opt;
    opt.patch = SD::Offset;
    opt.addressStep = 100;
    opt.universeStep = 1;
    const SD::Result res = SD::apply(m_doc, r.k1, { r.k2, r.k3 }, opt);
    QCOMPARE(res.patched, 6);
    for (int i = 0; i < 3; ++i)
    {
        Fixture *o = on(m_doc, r.k1).at(i);
        Fixture *c2 = on(m_doc, r.k2).at(i);
        Fixture *c3 = on(m_doc, r.k3).at(i);
        QCOMPARE(c2->universe(), o->universe() + 1);
        QCOMPARE(c2->address(), o->address() + 100);
        QCOMPARE(c3->universe(), o->universe() + 2);
        QCOMPARE(c3->address(), o->address() + 200);
    }
}

void StructureDuplicate_Test::anOffsetThatClashesLeavesTheCopyUnpatched()
{
    const Rig r = makeRig(m_doc);
    SD::Options opt;
    opt.patch = SD::Offset;                  // no step: the same addresses
    const SD::Preview pv = SD::preview(m_doc, r.k1, { r.k2 }, opt);
    QCOMPARE(pv.rows.size(), 3);
    foreach (const SD::Row &row, pv.rows)
    {
        QVERIFY(row.clash);
        QVERIFY(!row.patched);
        QVERIFY2(row.note.contains("taken"), qPrintable(row.note));
    }
    QVERIFY(!pv.problems.isEmpty());
    const SD::Result res = SD::apply(m_doc, r.k1, { r.k2 }, opt);
    QCOMPARE(res.created, 3);
    QCOMPARE(res.patched, 0);
    foreach (Fixture *f, on(m_doc, r.k2))
        QVERIFY(!f->isPatched());
    // The originals keep their channels.
    QVERIFY(on(m_doc, r.k1).first()->isPatched());
}

void StructureDuplicate_Test::unpatchedLeavesThemInTheInventory()
{
    const Rig r = makeRig(m_doc);
    SD::Options opt;
    opt.patch = SD::Unpatched;
    const SD::Result res = SD::apply(m_doc, r.k1, { r.k2 }, opt);
    QCOMPARE(res.created, 3);
    foreach (Fixture *f, on(m_doc, r.k2))
        QVERIFY(!f->isPatched());
}

void StructureDuplicate_Test::groupsTakeTheCopiesOrGoParallel()
{
    const Rig r = makeRig(m_doc);
    FixtureGroup *all = new FixtureGroup(m_doc);
    all->setName("All Spots");
    all->setSize(QSize(6, 1));
    all->assignFixture(r.lights.at(0)->id());
    all->assignFixture(r.lights.at(1)->id());
    m_doc->addFixtureGroup(all);
    // Undo deletes the group and loads it back under the same id, so the
    // test looks it up by id afterwards and never through the old pointer.
    const quint32 allId = all->id();

    // Added to: the copies of the two spots join it.
    SD::Options opt;
    opt.fixtureGroups = SD::AddToGroups;
    SD::Preview pv = SD::preview(m_doc, r.k1, { r.k2 }, opt);
    QCOMPARE(pv.groups.size(), 1);
    QVERIFY(!pv.groups.first().parallel);
    QCOMPARE(pv.groups.first().members, 2);
    SD::Result res = SD::apply(m_doc, r.k1, { r.k2 }, opt);
    QCOMPARE(m_doc->fixtureGroups().size(), 1);
    QCOMPARE(all->fixtureList().size(), 4);
    foreach (Fixture *f, on(m_doc, r.k2))
        QCOMPARE(all->fixtureList().contains(f->id()), f->name() != "T2-Wash");
    SD::undo(m_doc, res.undo);
    QCOMPARE(m_doc->fixtures().size(), 3);
    QVERIFY(m_doc->fixtureGroup(allId) != nullptr);
    QCOMPARE(m_doc->fixtureGroup(allId)->fixtureList().size(), 2);

    // Parallel: a group of their own, named by the structure.
    opt.fixtureGroups = SD::ParallelGroups;
    pv = SD::preview(m_doc, r.k1, { r.k2, r.k3 }, opt);
    QCOMPARE(pv.groups.size(), 2);
    QVERIFY(pv.groups.first().parallel);
    QCOMPARE(pv.groups.first().name, QString("T-2 All Spots"));
    res = SD::apply(m_doc, r.k1, { r.k2, r.k3 }, opt);
    QCOMPARE(m_doc->fixtureGroups().size(), 3);
    QCOMPARE(m_doc->fixtureGroup(allId)->fixtureList().size(), 2);      // T-1's own is as it was
    QStringList names;
    foreach (FixtureGroup *g, m_doc->fixtureGroups())
        names << g->name();
    names.sort();
    QCOMPARE(names, QStringList({ "All Spots", "T-2 All Spots", "T-3 All Spots" }));
    foreach (FixtureGroup *g, m_doc->fixtureGroups())
        if (g->name() == "T-2 All Spots")
        {
            QCOMPARE(g->fixtureList().size(), 2);
            foreach (quint32 f, g->fixtureList())
                QVERIFY(PatchPlanner::mountKey(m_doc, f) == r.k2);
        }
}

void StructureDuplicate_Test::aGroupNamedForTheStructureIsAlwaysParallel()
{
    const Rig r = makeRig(m_doc);
    FixtureGroup *own = new FixtureGroup(m_doc);
    own->setName("T1 Spots");
    own->setSize(QSize(4, 1));
    own->assignFixture(r.lights.at(0)->id());
    own->assignFixture(r.lights.at(1)->id());
    m_doc->addFixtureGroup(own);

    SD::Options opt;
    opt.fixtureGroups = SD::AddToGroups;                    // asked to add, but it is T1's
    const SD::Preview pv = SD::preview(m_doc, r.k1, { r.k2 }, opt);
    QCOMPARE(pv.groups.size(), 1);
    QVERIFY(pv.groups.first().parallel);
    QCOMPARE(pv.groups.first().name, QString("T2 Spots"));
    SD::apply(m_doc, r.k1, { r.k2 }, opt);
    QCOMPARE(own->fixtureList().size(), 2);
    QCOMPARE(m_doc->fixtureGroups().size(), 2);
}

void StructureDuplicate_Test::mapGroupsFollowTheStructure()
{
    const Rig r = makeRig(m_doc);
    MonitorProperties *props = m_doc->monitorProperties();
    // T-1's own plot group, and a folder two of its lights are in.
    props->createGroup(1, "T-1 truss", 0);
    r.t1->setGroupId(1);
    props->createGroup(2, "Front wash", 0);
    r.t2->setGroupId(5);
    props->createGroup(5, "T-2 truss", 0);
    foreach (Fixture *f, r.lights)
        props->setFixtureGroup(f->id(), 1);
    props->setFixtureGroup(r.lights.at(2)->id(), 2);

    SD::Options opt;                                        // map groups: parallel
    const SD::Result res = SD::apply(m_doc, r.k1, { r.k2 }, opt);
    const QList<Fixture *> b = on(m_doc, r.k2);
    QCOMPARE(props->fixtureGroup(b.at(0)->id()), quint32(5));       // the target's own group
    const quint32 folder = props->fixtureGroup(b.at(2)->id());
    QVERIFY(folder != 2 && folder != 0);
    QCOMPARE(props->group(folder).name, QString("T-2 Front wash"));
    QCOMPARE(res.undo.createdMapGroups.size(), 1);
    SD::undo(m_doc, res.undo);
    QVERIFY(!props->hasGroup(folder));
}

void StructureDuplicate_Test::anOccupiedTargetIsSkippedByDefault()
{
    const Rig r = makeRig(m_doc);
    hung(m_doc, r.t2, "Old", 4, 1, 0, 1.0f);
    const SD::Preview pv = SD::preview(m_doc, r.k1, { r.k2, r.k3 }, SD::Options());
    QCOMPARE(pv.occupied.value(r.k2), 1);
    QCOMPARE(pv.skipped, QStringList({ r.k2 }));
    QCOMPARE(pv.rows.size(), 3);                            // T-3 only
    const SD::Result res = SD::apply(m_doc, r.k1, { r.k2, r.k3 }, SD::Options());
    QCOMPARE(res.created, 3);
    QCOMPARE(on(m_doc, r.k2).size(), 1);
    QCOMPARE(on(m_doc, r.k3).size(), 3);
}

void StructureDuplicate_Test::anOccupiedTargetCanTakeMore()
{
    const Rig r = makeRig(m_doc);
    hung(m_doc, r.t2, "Old", 4, 1, 0, 1.0f);
    SD::Options opt;
    opt.existing.insert(r.k2, SD::Add);
    const SD::Result res = SD::apply(m_doc, r.k1, { r.k2 }, opt);
    QCOMPARE(res.created, 3);
    QCOMPARE(on(m_doc, r.k2).size(), 4);
}

void StructureDuplicate_Test::anOccupiedTargetCanBeReplaced()
{
    const Rig r = makeRig(m_doc);
    Fixture *old = hung(m_doc, r.t2, "T2-Spot 1", 4, 1, 0, 1.0f);   // the name a copy will want
    const quint32 oldId = old->id();
    SD::Options opt;
    opt.existing.insert(r.k2, SD::Replace);
    const SD::Preview pv = SD::preview(m_doc, r.k1, { r.k2 }, opt);
    QCOMPARE(pv.replacedFixtures, 1);
    QCOMPARE(pv.rows.first().name, QString("T2-Spot 1"));           // freed by the replace
    const SD::Result res = SD::apply(m_doc, r.k1, { r.k2 }, opt);
    QCOMPARE(res.replaced, 1);
    QCOMPARE(on(m_doc, r.k2).size(), 3);
    QVERIFY(m_doc->fixture(oldId) == nullptr || m_doc->fixture(oldId)->name() == "T2-Spot 1");
    foreach (Fixture *f, on(m_doc, r.k2))
        QVERIFY(f->channels() == 10);

    SD::undo(m_doc, res.undo);
    QCOMPARE(on(m_doc, r.k2).size(), 1);
    Fixture *back = m_doc->fixture(oldId);
    QVERIFY(back != nullptr);
    QCOMPARE(back->name(), QString("T2-Spot 1"));
    QCOMPARE(back->channels(), quint32(4));
    QVERIFY(back->isPatched());
    QCOMPARE(back->universe(), quint32(1));
    QCOMPARE(m_doc->monitorProperties()->fixtureRigProps(oldId).trussId, r.t2->id());
}

void StructureDuplicate_Test::replaceCountsTheScenesThatLoseLights()
{
    const Rig r = makeRig(m_doc);
    Fixture *old = hung(m_doc, r.t2, "Old", 4, 1, 0, 1.0f);
    Scene *sc = new Scene(m_doc);
    sc->setName("S");
    sc->setValue(old->id(), 0, 255);
    m_doc->addFunction(sc);
    SD::Options opt;
    opt.existing.insert(r.k2, SD::Replace);
    const SD::Preview pv = SD::preview(m_doc, r.k1, { r.k2 }, opt);
    QCOMPARE(pv.replacedInScenes, 1);
}

void StructureDuplicate_Test::undoPutsEverythingBack()
{
    const Rig r = makeRig(m_doc);
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("T1 Spots");
    g->setSize(QSize(4, 1));
    g->assignFixture(r.lights.at(0)->id());
    m_doc->addFixtureGroup(g);

    const int fixtures = m_doc->fixtures().size();
    const SD::Result res = SD::apply(m_doc, r.k1, { r.k2, r.k3 }, SD::Options());
    QCOMPARE(m_doc->fixtures().size(), fixtures + 6);
    QCOMPARE(m_doc->fixtureGroups().size(), 3);
    QVERIFY(!res.undo.isEmpty());

    SD::undo(m_doc, res.undo);
    QCOMPARE(m_doc->fixtures().size(), fixtures);
    QCOMPARE(m_doc->fixtureGroups().size(), 1);
    QVERIFY(on(m_doc, r.k2).isEmpty());
    QVERIFY(on(m_doc, r.k3).isEmpty());
    // No mount left behind for a reused id.
    foreach (quint32 id, res.createdIds)
        QCOMPARE(m_doc->monitorProperties()->fixtureRigProps(id).trussId, Truss::invalidId());
    // The originals are as they were.
    QCOMPARE(on(m_doc, r.k1).size(), 3);
    QCOMPARE(r.lights.at(2)->address(), quint32(20));
    // Their addresses are free again: a second go lands where the first did.
    const SD::Result again = SD::apply(m_doc, r.k1, { r.k2 }, SD::Options());
    QCOMPARE(on(m_doc, r.k2).first()->address(), quint32(30));
    QCOMPARE(again.created, 3);
}

void StructureDuplicate_Test::aDeckCarriesItsLightsToTheOtherDeck()
{
    MonitorProperties *props = m_doc->monitorProperties();
    StagePlatform *d1 = props->addPlatform();
    d1->setName("Deck 1");
    d1->setOriginX(1.0f);
    d1->setOriginY(2.0f);
    d1->setWidth(2.0f);
    d1->setDepth(2.0f);
    StagePlatform *d2 = props->addPlatform();
    d2->setName("Deck 2");
    d2->setOriginX(6.0f);
    d2->setOriginY(2.0f);
    d2->setWidth(2.0f);
    d2->setDepth(2.0f);

    Fixture *fx = new Fixture(m_doc);
    fx->setChannels(4);
    fx->setName("Deck 1 Par");
    fx->setUniverse(0);
    fx->setAddress(0);
    fx->setPatched(true);
    QVERIFY(m_doc->addFixture(fx));
    FixtureRigProps rp;
    rp.deckPlatformId = d1->id();
    props->setFixtureRigProps(fx->id(), rp);
    FixturePreviewItem item;
    item.m_baseItem.m_position = QVector3D(1500.0f, 2500.0f, 0.0f);
    props->setFixtureProperties(fx->id(), item);

    const QString k1 = QString("platform:%1").arg(d1->id());
    const QString k2 = QString("platform:%1").arg(d2->id());
    QCOMPARE(SD::targetsFor(m_doc, k1), QStringList({ k2 }));
    const SD::Result res = SD::apply(m_doc, k1, { k2 }, SD::Options());
    QCOMPARE(res.created, 1);
    Fixture *copy = on(m_doc, k2).first();
    QCOMPARE(copy->name(), QString("Deck 2 Par"));
    const QVector3D p = props->fixtureProperties(copy->id()).m_baseItem.m_position;
    QVERIFY(qAbs(p.x() - 6500.0f) < 0.5f && qAbs(p.y() - 2500.0f) < 0.5f);   // 5 m on
    QCOMPARE(props->fixtureRigProps(copy->id()).deckPlatformId, d2->id());
}

void StructureDuplicate_Test::nothingToCopyIsSaid()
{
    const Rig r = makeRig(m_doc);
    // T-2 has nothing on it: nothing to copy from, and nowhere offered.
    QVERIFY(SD::targetsFor(m_doc, r.k2).isEmpty());
    const SD::Preview pv = SD::preview(m_doc, r.k2, { r.k1 }, SD::Options());
    QVERIFY(pv.rows.isEmpty());
    QVERIFY(!pv.problems.isEmpty());
    const SD::Result res = SD::apply(m_doc, r.k2, { r.k1 }, SD::Options());
    QCOMPARE(res.created, 0);
    QVERIFY(res.undo.isEmpty());
}

QTEST_MAIN(StructureDuplicate_Test)

/* A hanger with a light on it (rp.hangerId), and a child bar of @p parent with
   a light on that and a hanger on the bar. */
static Fixture *lightOn(Doc *doc, const QString &name, quint32 address, FixtureRigProps rp)
{
    Fixture *fx = new Fixture(doc);
    fx->setChannels(5);
    fx->setUniverse(0);
    fx->setAddress(address);
    fx->setPatched(true);
    fx->setName(name);
    if (!doc->addFixture(fx))
    {
        delete fx;
        return nullptr;
    }
    doc->monitorProperties()->setFixtureRigProps(fx->id(), rp);
    return fx;
}

struct Hung
{
    Hanger *h1 = nullptr, *h2 = nullptr;
    Truss *bar = nullptr;
    Fixture *onH1 = nullptr, *onBar = nullptr, *onH2 = nullptr;
};

static Hung hangThings(Doc *doc, const Rig &r)
{
    Hung g;
    MonitorProperties *props = doc->monitorProperties();
    g.h1 = props->addHanger(Hanger::OnTruss, r.t1->id());
    g.h1->setName("Hook A");
    g.h1->setAlong(1.0f);
    g.h1->setArm(0.4f);
    g.h1->setDrop(0.6f);
    g.h1->setAngle(90.0f);
    g.bar = props->addTruss();
    g.bar->setName("T1 Bar");
    g.bar->setLength(2.0f);
    g.bar->setParentTrussId(r.t1->id());
    g.bar->setParentOffset(2.0f);
    g.bar->setBarStandoff(0.5f);
    g.bar->setBarCrossShift(0.2f);
    props->recomputeChildTrusses();
    g.h2 = props->addHanger(Hanger::OnTruss, g.bar->id());
    g.h2->setName("Bar Hook");
    g.h2->setAlong(0.5f);
    props->recomputeHangers();

    FixtureRigProps rp;
    rp.hangerId = g.h1->id();
    rp.panOffsetDeg = 5.0f;
    g.onH1 = lightOn(doc, "T1-Hung 1", 30, rp);
    FixtureRigProps rb;
    rb.trussId = g.bar->id();
    rb.trussOffset = 1.0f;
    g.onBar = lightOn(doc, "T1-Bar Spot", 35, rb);
    FixtureRigProps rh;
    rh.hangerId = g.h2->id();
    g.onH2 = lightOn(doc, "T1-Bar Hung", 40, rh);
    return g;
}

void StructureDuplicate_Test::hangersAndChildBarsAreCopied()
{
    const Rig r = makeRig(m_doc);
    const Hung g = hangThings(m_doc, r);
    MonitorProperties *props = m_doc->monitorProperties();
    const int trussesBefore = props->trusses().size();
    const int hangersBefore = props->hangers().size();

    SD::Options opt;
    const SD::Preview pv = SD::preview(m_doc, r.k1, { r.k2, r.k3 }, opt);
    QCOMPARE(pv.rows.size(), 12);                       // 6 on the truss, 6 hung
    QCOMPARE(pv.newHangers, 4);
    QCOMPARE(pv.newBars, 2);
    QCOMPARE(m_doc->fixtures().size(), 6);              // preview changes nothing
    QCOMPARE(props->hangers().size(), hangersBefore);

    const SD::Result res = SD::apply(m_doc, r.k1, { r.k2, r.k3 }, opt);
    QCOMPARE(res.created, 12);
    QCOMPARE(res.patched, 12);
    QCOMPARE(res.createdHangers, 4);
    QCOMPARE(res.createdBars, 2);
    QCOMPARE(props->trusses().size(), trussesBefore + 2);
    QCOMPARE(props->hangers().size(), hangersBefore + 4);

    // T-2's bar and hangers: named for it, hung from it, shaped like the originals.
    Truss *bar2 = nullptr;
    foreach (Truss *t, props->trusses())
        if (t->isChildBar() && t->parentTrussId() == r.t2->id())
            bar2 = t;
    QVERIFY(bar2 != nullptr);
    QCOMPARE(bar2->name(), QString("T2 Bar"));
    QCOMPARE(bar2->length(), 2.0f);
    QCOMPARE(bar2->parentOffset(), 2.0f);
    QCOMPARE(bar2->barStandoff(), 0.5f);
    QCOMPARE(bar2->barCrossShift(), 0.2f);
    QVERIFY(bar2->id() != g.bar->id());
    QVERIFY(qAbs(bar2->origin().x() - g.bar->origin().x() - 5.0f) < 0.01f);   // moved with T-2

    Hanger *hook2 = nullptr, *barHook2 = nullptr;
    foreach (Hanger *h, props->hangers())
    {
        if (h->parentKind() == Hanger::OnTruss && h->parentId() == r.t2->id())
            hook2 = h;
        if (h->parentKind() == Hanger::OnTruss && h->parentId() == bar2->id())
            barHook2 = h;
    }
    QVERIFY(hook2 != nullptr && barHook2 != nullptr);
    QCOMPARE(hook2->name(), QString("T-2 Hook A"));
    QCOMPARE(hook2->along(), 1.0f);
    QCOMPARE(hook2->arm(), 0.4f);
    QCOMPARE(hook2->drop(), 0.6f);
    QCOMPARE(hook2->angle(), 90.0f);
    QCOMPARE(barHook2->along(), 0.5f);

    // The lights on them: named, mounted on the copies, calibration kept, patched.
    const quint32 f1 = props->fixtureOnHanger(hook2->id());
    QVERIFY(f1 != UINT_MAX);
    QCOMPARE(m_doc->fixture(f1)->name(), QString("T2-Hung 1"));
    QCOMPARE(props->fixtureRigProps(f1).panOffsetDeg, 5.0f);
    QVERIFY(m_doc->fixture(f1)->isPatched());
    const quint32 f2 = props->fixtureOnHanger(barHook2->id());
    QCOMPARE(m_doc->fixture(f2)->name(), QString("T2-Bar Hung"));
    const QList<Fixture *> onBar = on(m_doc, QString("truss:%1").arg(bar2->id()));
    QCOMPARE(onBar.size(), 1);
    QCOMPARE(onBar.first()->name(), QString("T2-Bar Spot"));
    QCOMPARE(props->fixtureRigProps(onBar.first()->id()).trussOffset, 1.0f);
    // The originals are untouched, and no address is used twice.
    QCOMPARE(props->fixtureOnHanger(g.h1->id()), g.onH1->id());
    QCOMPARE(props->fixtureRigProps(g.onBar->id()).trussId, g.bar->id());
    QSet<quint32> seen;
    foreach (Fixture *fx, m_doc->fixtures())
        for (quint32 a = fx->address(); a < fx->address() + fx->channels(); ++a)
        {
            QVERIFY2(!seen.contains(a), "an address is used twice");
            seen.insert(a);
        }

    // One step back.
    SD::undo(m_doc, res.undo);
    QCOMPARE(props->trusses().size(), trussesBefore);
    QCOMPARE(props->hangers().size(), hangersBefore);
    QCOMPARE(m_doc->fixtures().size(), 6);
    QCOMPARE(props->fixtureOnHanger(g.h1->id()), g.onH1->id());
}

void StructureDuplicate_Test::replaceTakesTheTargetsHangersAndBarsAndUndoPutsThemBack()
{
    const Rig r = makeRig(m_doc);
    const Hung g = hangThings(m_doc, r);
    MonitorProperties *props = m_doc->monitorProperties();

    // T-2 gets a copy first, then a second copy replaces it.
    SD::Options opt;
    QVERIFY(SD::apply(m_doc, r.k1, { r.k2 }, opt).created > 0);
    const int hangers = props->hangers().size();
    const int trusses = props->trusses().size();
    const int fixtures = m_doc->fixtures().size();

    opt.defaultExisting = SD::Replace;
    const SD::Result res = SD::apply(m_doc, r.k1, { r.k2 }, opt);
    QCOMPARE(props->hangers().size(), hangers);         // replaced, not doubled
    QCOMPARE(props->trusses().size(), trusses);
    QCOMPARE(m_doc->fixtures().size(), fixtures);

    SD::undo(m_doc, res.undo);
    QCOMPARE(props->hangers().size(), hangers);
    QCOMPARE(props->trusses().size(), trusses);
    QCOMPARE(m_doc->fixtures().size(), fixtures);
    // The first copy is whole again: a bar on T-2 with its light.
    int bars = 0;
    foreach (Truss *t, props->trusses())
        if (t->parentTrussId() == r.t2->id())
        {
            ++bars;
            QCOMPARE(on(m_doc, QString("truss:%1").arg(t->id())).size(), 1);
        }
    QCOMPARE(bars, 1);
    Q_UNUSED(g);
}
