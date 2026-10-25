/*
  Q Light Controller Plus - Test Unit
  monitorproperties_test.cpp

  Copyright (c) Massimo Callegari

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
#include <functional>
#include <QSignalSpy>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QTemporaryDir>
#include <QBuffer>
#include <atomic>
#include <thread>
#define private public
#include "monitorproperties.h"
#include "righeads.h"
#include "hanger.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "qlcfixturehead.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "headlayout.h"
#undef private
#include "stageplatform.h"
#include "truss.h"
#include "pipe.h"
#include "stand.h"
#include "tower.h"
#include "stagetarget.h"
#include "scenery.h"
#include "powerdistribution.h"
#include "networktopology.h"
#include "doc.h"
#include "fixture.h"
#include "gobolibrary.h"
#include "monitorproperties_test.h"

void MonitorProperties_Test::defaults()
{
    MonitorProperties mp;

    QCOMPARE(mp.displayMode(), MonitorProperties::DMX);
    QCOMPARE(mp.channelStyle(), MonitorProperties::DMXChannels);
    QCOMPARE(mp.valueStyle(), MonitorProperties::DMXValues);
    QCOMPARE(mp.gridSize(), QVector3D(5, 3, 5));
    QCOMPARE(mp.gridUnits(), MonitorProperties::Meters);
    QCOMPARE(mp.pointOfView(), MonitorProperties::Undefined);
    QCOMPARE(mp.stageType(), MonitorProperties::StageSimple);
    QCOMPARE(mp.labelsVisible(), true);
    QVERIFY(mp.commonBackgroundImage().isEmpty());
}

void MonitorProperties_Test::fixtureItems()
{
    MonitorProperties mp;

    mp.setFixturePosition(10, 0, 0, QVector3D(1, 2, 3));
    mp.setFixtureRotation(10, 0, 0, QVector3D(0, 90, 0));
    mp.setFixtureGelColor(10, 0, 0, QColor(Qt::red));
    mp.setFixtureName(10, 0, 0, "Main");
    mp.setFixtureFlags(10, 0, 0, MonitorProperties::HiddenFlag);

    QCOMPARE(mp.fixturePosition(10,0,0), QVector3D(1,2,3));
    QCOMPARE(mp.fixtureRotation(10,0,0), QVector3D(0,90,0));
    QCOMPARE(mp.fixtureGelColor(10,0,0), QColor(Qt::red));
    QCOMPARE(mp.fixtureName(10,0,0), QString("Main"));
    QCOMPARE(mp.fixtureFlags(10,0,0), quint32(MonitorProperties::HiddenFlag));

    mp.removeFixture(10);
    QCOMPARE(mp.containsFixture(10), false);
}

void MonitorProperties_Test::genericItems()
{
    MonitorProperties mp;

    quint32 id = 100;
    mp.setItemName(id, "Item");
    mp.setItemResource(id, "path");
    mp.setItemPosition(id, QVector3D(1,1,1));
    mp.setItemRotation(id, QVector3D(0,0,90));
    mp.setItemScale(id, QVector3D(2,2,2));
    mp.setItemFlags(id, MonitorProperties::InvertedPanFlag);

    QList<quint32> ids = mp.genericItemsID();
    QCOMPARE(ids.count(), 1);
    QCOMPARE(ids.first(), id);
    QCOMPARE(mp.itemName(id), QString("Item"));
    QCOMPARE(mp.itemResource(id), QString("path"));
    QCOMPARE(mp.itemPosition(id), QVector3D(1,1,1));
    QCOMPARE(mp.itemRotation(id), QVector3D(0,0,90));
    QCOMPARE(mp.itemScale(id), QVector3D(2,2,2));
    QCOMPARE(mp.itemFlags(id), quint32(MonitorProperties::InvertedPanFlag));

    mp.removeItem(id);
    QCOMPARE(mp.containsItem(id), false);
}

void MonitorProperties_Test::layers()
{
    MonitorProperties mp;

    // A pristine object always has exactly the Default layer, active.
    QCOMPARE(mp.layers().count(), 1);
    QCOMPARE(mp.layer(MonitorProperties::defaultLayerId).name, QString("Default"));
    QCOMPARE(mp.activeLayerId(), MonitorProperties::defaultLayerId);

    quint32 a = mp.addLayer("Movers");
    quint32 b = mp.addLayer("Wash");
    QVERIFY(a != MonitorProperties::defaultLayerId);
    QVERIFY(b != a);
    QCOMPARE(mp.layers().count(), 3);

    mp.setLayerLocked(a, true);
    mp.setLayerVisible(b, false);
    QVERIFY(mp.layer(a).locked);
    QVERIFY(!mp.layer(b).visible);

    // Unknown ids resolve to Default so orphaned items still render.
    QCOMPARE(mp.layer(999).id, MonitorProperties::defaultLayerId);

    // Active-layer setter rejects unknown ids.
    mp.setActiveLayerId(a);
    QCOMPARE(mp.activeLayerId(), a);
    mp.setActiveLayerId(999);
    QCOMPARE(mp.activeLayerId(), MonitorProperties::defaultLayerId);

    // Default is permanent; removing the active layer falls back to Default.
    mp.setActiveLayerId(a);
    mp.removeLayer(MonitorProperties::defaultLayerId);
    QVERIFY(mp.hasLayer(MonitorProperties::defaultLayerId));
    mp.removeLayer(a);
    QVERIFY(!mp.hasLayer(a));
    QCOMPARE(mp.activeLayerId(), MonitorProperties::defaultLayerId);
}

void MonitorProperties_Test::layersXmlRoundTrip()
{
    MonitorProperties mp;
    quint32 a = mp.addLayer("Movers");
    quint32 b = mp.addLayer("Wash");
    mp.setLayerLocked(a, true);
    mp.setLayerVisible(b, false);
    mp.setLayerOrder(b, 5);
    mp.setActiveLayerId(a);

    // A fixture carries its layer id, group id, mounting height (Z) and the
    // vertical-strip flag through the round-trip too.
    mp.setFixturePosition(10, 0, 0, QVector3D(1, 2, 3));
    mp.setFixtureLayer(10, b);
    mp.setFixtureGroup(10, 7);
    mp.setFixtureFlags(10, 0, 0, MonitorProperties::VerticalStripFlag);

    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();

    MonitorProperties mp2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
    {
        if (reader.name() == QStringLiteral("Monitor"))
            QVERIFY(mp2.loadXML(reader, nullptr));
    }

    QCOMPARE(mp2.layers().count(), 3);
    QVERIFY(mp2.hasLayer(a));
    QVERIFY(mp2.hasLayer(b));
    QCOMPARE(mp2.layer(a).name, QString("Movers"));
    QVERIFY(mp2.layer(a).locked);
    QVERIFY(!mp2.layer(b).visible);
    QCOMPARE(mp2.layer(b).order, 5);
    QCOMPARE(mp2.activeLayerId(), a);
    QCOMPARE(mp2.fixtureLayer(10), b);
    QCOMPARE(mp2.fixtureGroup(10), quint32(7));
    QCOMPARE(mp2.fixturePosition(10, 0, 0), QVector3D(1, 2, 3));   // Z persists
    QVERIFY(mp2.fixtureFlags(10, 0, 0) & MonitorProperties::VerticalStripFlag);
}

void MonitorProperties_Test::groupRegistry()
{
    MonitorProperties mp;
    QCOMPARE(mp.groups().count(), 0);
    QCOMPARE(mp.nextGroupId(), quint32(1));

    mp.createGroup(1, "Stage Left", 0, 0);
    mp.createGroup(2, "Uplighters", 0, 1);   // nested under group 1
    QVERIFY(mp.hasGroup(1));
    QVERIFY(mp.hasGroup(2));
    QCOMPARE(mp.group(2).parentGroupId, quint32(1));
    QCOMPARE(mp.childGroups(1).count(), 1);
    QCOMPARE(mp.childGroups(1).first().id, quint32(2));
    QCOMPARE(mp.childGroups(0).count(), 1);  // group 1 is top-level
    QCOMPARE(mp.nextGroupId(), quint32(3));

    // ensureGroup is a no-op on an existing id, creates otherwise.
    mp.ensureGroup(1, 5);
    QCOMPARE(mp.group(1).layerId, quint32(0)); // unchanged
    mp.ensureGroup(9, 3);
    QVERIFY(mp.hasGroup(9));
    QCOMPARE(mp.group(9).layerId, quint32(3));

    mp.removeGroup(2);
    QVERIFY(!mp.hasGroup(2));
    QCOMPARE(mp.childGroups(1).count(), 0);
}

void MonitorProperties_Test::groupsXmlRoundTrip()
{
    MonitorProperties mp;
    mp.createGroup(1, "Stage Left", 2, 0);
    mp.createGroup(2, "Uplighters", 2, 1);   // nested under 1

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

    QCOMPARE(mp2.groups().count(), 2);
    QVERIFY(mp2.hasGroup(1));
    QVERIFY(mp2.hasGroup(2));
    QCOMPARE(mp2.group(1).name, QString("Stage Left"));
    QCOMPARE(mp2.group(1).layerId, quint32(2));
    QCOMPARE(mp2.group(2).parentGroupId, quint32(1));
}

void MonitorProperties_Test::riserMount()
{
    MonitorProperties mp;
    StagePlatform *p = mp.addPlatform();
    p->setOriginX(2); p->setOriginY(3); p->setWidth(4); p->setDepth(1); p->setHeight(0.8f);

    mp.setFixturePosition(5, 0, 0, QVector3D(0, 0, 0));   // fixture must exist
    FixtureRigProps rp;
    rp.riserPlatformId = p->id();
    rp.riserFace = FixtureRigProps::RiserFront;
    rp.riserU = 1.0f; rp.riserV = 0.5f;
    mp.setFixtureRigProps(5, rp);

    QVERIFY(mp.fixtureRigProps(5).onRiser());
    // Front (downstage) face → (originX+U, originY+depth, V) = (2+1, 3+1, 0.5)
    QCOMPARE(mp.fixtureRigPosition(5), QVector3D(3.0f, 4.0f, 0.5f));

    // Top face → (originX+U, originY+V, height)
    rp.riserFace = FixtureRigProps::RiserTop; rp.riserV = 0.4f;
    mp.setFixtureRigProps(5, rp);
    QCOMPARE(mp.fixtureRigPosition(5), QVector3D(3.0f, 3.4f, 0.8f));

    // XML round-trip of the mount.
    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();

    MonitorProperties mp2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
        if (reader.name() == QStringLiteral("Monitor"))
            mp2.loadXML(reader, nullptr);

    const FixtureRigProps rp2 = mp2.fixtureRigProps(5);
    QVERIFY(rp2.onRiser());
    QCOMPARE(rp2.riserPlatformId, p->id());
    QCOMPARE(rp2.riserFace, int(FixtureRigProps::RiserTop));
    QCOMPARE(rp2.riserV, 0.4f);

    // Removing the platform un-mounts the fixture.
    mp.removePlatform(p->id());
    QVERIFY(!mp.fixtureRigProps(5).onRiser());
}

/* DS3's eight strips in stage-structures-demo.qxw were positioned along the
   step's front nosing (RiserFace = front) while being DRAWN flat on its tread
   (studioMount = top), which is why that one step looked unlike its ten
   identical neighbours. Nothing in the renderer was wrong; the fixture was
   telling the position resolver and the geometry builder two different things. */
/* A fixture can be given a structural mount without ever having had a position
   on the plot -- mount it from a structure editor and nothing creates one. Both
   the rig view and the Layers tree enumerate fixtureItemsID(), so such a
   fixture is listed by the editor that mounted it and invisible everywhere
   else: "put bars in T1 .. they're not visible on the rig display" and "the
   added fixtures aren't showing up in the tree under the truss".

   repairOrphanedMounts() has always fixed this, but only on LOAD -- which is no
   help in the session where you are building the rig. */
void MonitorProperties_Test::mountingAFixtureGivesItAPlotItem()
{
    MonitorProperties mp;
    Truss *t = mp.addTruss();
    t->setType(Truss::Vertical);
    t->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    t->setLength(3.0f);

    // No setFixturePosition(): this fixture has never been on the plot.
    QVERIFY(mp.fixtureItemsID().contains(7) == false);

    FixtureRigProps rp;
    rp.trussId = t->id();
    rp.trussOffset = 1.5f;
    mp.setFixtureRigProps(7, rp);

    QVERIFY2(mp.fixtureItemsID().contains(7),
             "a mounted fixture has no plot item, so it is invisible in the rig "
             "view and absent from the Layers tree");

    // ...and it is placed where the mount says, not at the origin.
    const QVector3D w = mp.fixtureRigPosition(7);
    QVERIFY2(w.z() > 1.0f,
             qPrintable(QString("the rebuilt item sits at z=%1, not on the truss")
                        .arg(double(w.z()))));

    // An UNMOUNTED fixture must not gain an item -- nothing to derive from.
    FixtureRigProps none;
    mp.setFixtureRigProps(8, none);
    QVERIFY2(mp.fixtureItemsID().contains(8) == false,
             "an unmounted fixture was given a plot item out of nowhere");
}

void MonitorProperties_Test::riserFaceAndDrawingPlaneCannotDisagree()
{
    MonitorProperties mp;
    StagePlatform *p = mp.addPlatform();
    p->setOriginX(2); p->setOriginY(3); p->setWidth(4); p->setDepth(1); p->setHeight(0.8f);

    // 5: the DS3 shape -- bolted to the front face, drawn lying on the top.
    mp.setFixturePosition(5, 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps bad;
    bad.riserPlatformId = p->id();
    bad.riserFace = FixtureRigProps::RiserFront;
    bad.studioMount = 0;                       // Top -- contradicts the face
    mp.setFixtureRigProps(5, bad);

    // 6: on a riser's top, drawn on its SIDE. Not a contradiction the riser
    // model can resolve (it has no side), so it must be left exactly as it is.
    mp.setFixturePosition(6, 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps side;
    side.riserPlatformId = p->id();
    side.riserFace = FixtureRigProps::RiserTop;
    side.studioMount = 2;
    mp.setFixtureRigProps(6, side);

    // 7: NOT riser-mounted. A bar standing on a deck may legitimately lie in
    // any plane -- riserFace means nothing here and must not drive anything.
    mp.setFixturePosition(7, 0, 0, QVector3D(1000, 1000, 0));
    FixtureRigProps deck;
    deck.deckPlatformId = p->id();
    deck.riserFace = FixtureRigProps::RiserFront;  // leftover, meaningless
    deck.studioMount = 0;                          // deliberately Top
    mp.setFixtureRigProps(7, deck);

    // 8: on nothing at all, running vertically. The face and run angle are
    // properties of the fixture; they used to be written only for fixtures in
    // a studio frame, so for everything else both controls silently did
    // nothing -- set them, save, reopen, back to Front at 0 degrees.
    mp.setFixturePosition(8, 0, 0, QVector3D(2000, 2000, 0));
    FixtureRigProps loose;
    loose.studioMount = 2;
    loose.studioAngle = 90.0f;
    mp.setFixtureRigProps(8, loose);

    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();

    MonitorProperties mp2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
        if (reader.name() == QStringLiteral("Monitor"))
            mp2.loadXML(reader, nullptr);

    // The contradiction is gone, and the face -- the half the position is
    // derived from, and the half that was observably right -- is what won.
    QCOMPARE(mp2.fixtureRigProps(5).studioMount, 1);
    QCOMPARE(mp2.fixtureRigProps(5).riserFace, int(FixtureRigProps::RiserFront));
    // Front face → (originX+U, originY+depth, V), i.e. it did not move.
    QCOMPARE(mp2.fixtureRigPosition(5), mp.fixtureRigPosition(5));

    QCOMPARE(mp2.fixtureRigProps(6).studioMount, 2);        // Side: untouched
    QCOMPARE(mp2.fixtureRigProps(7).studioMount, 0);        // not on a riser: untouched
    QCOMPARE(mp2.fixtureRigProps(8).studioMount, 2);        // survives the file
    QCOMPARE(mp2.fixtureRigProps(8).studioAngle, 90.0f);
}

void MonitorProperties_Test::deckMountAlwaysDrawsOnTop()
{
    /* "front view of UST1 is STILL putting it off the platform .. when the
       top view shows it's clearly entirely on the platform." Deck mounting
       (deckPlatformId, not the riser's face+U/V) never gets a repair pass the
       way riser mounts do, so a fixture dropped onto a deck through any path
       that does not touch the Face combo keeps studioMount at its general
       default of 1 (Front/face). fixtureBoxCorners() then builds its box on
       the front-face axes -- seated along world Y, not lifted along Z -- so
       Top (X-Y) still looks right while Front (X-Z) shows it straddling the
       deck instead of standing on it. */
    MonitorProperties mp;
    StagePlatform *p = mp.addPlatform();
    p->setOriginX(2); p->setOriginY(3); p->setWidth(4); p->setDepth(1); p->setHeight(0.8f);

    // UST1: dropped on the deck, never walked through the Face combo -- left
    // at FixtureRigProps' general default (1, Front/face).
    mp.setFixturePosition(10, 0, 0, QVector3D(3000, 3000, 0));
    FixtureRigProps ust1;
    ust1.deckPlatformId = p->id();
    QCOMPARE(ust1.studioMount, 1);              // the default this bug relies on
    mp.setFixtureRigProps(10, ust1);

    // A second deck fixture, deliberately drawn on its Side (2) -- a genuine
    // choice, not the stray default, and must survive unlike Top vs Front.
    mp.setFixturePosition(11, 0, 0, QVector3D(3200, 3000, 0));
    FixtureRigProps sideDeck;
    sideDeck.deckPlatformId = p->id();
    sideDeck.studioMount = 2;
    mp.setFixtureRigProps(11, sideDeck);

    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();

    MonitorProperties mp2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
        if (reader.name() == QStringLiteral("Monitor"))
            mp2.loadXML(reader, nullptr);

    QCOMPARE(mp2.fixtureRigProps(10).studioMount, 0);   // corrected to Top
    QCOMPARE(mp2.fixtureRigProps(11).studioMount, 2);   // a real choice: untouched
}

void MonitorProperties_Test::trussMountPlaneMatchesWhichWayItRuns()
{
    /* "fixture set as underhung .. visually upright": a mover/PAR mounted on
       a horizontal bar has no Face control anywhere in the UI to ever move
       studioMount off its stray default of 1 (Front/face), so
       fixtureFrame() keeps building it on the front-face axes no matter
       what Orientation (mountingType) says -- the sign-flip that
       Underhung/On-top apply on top of that frame can't fix an already
       wrong base frame. */
    MonitorProperties mp;
    Truss *bar = mp.addTruss();
    bar->setType(Truss::Horizontal);

    Truss *tower = mp.addTruss();
    tower->setType(Truss::Vertical);

    // Underhung on the bar, never walked through a Face combo -- left at the
    // general default.
    mp.setFixturePosition(20, 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps hung;
    hung.trussId = bar->id();
    hung.mountingType = Truss::TopHung;
    QCOMPARE(hung.studioMount, 1);              // the default this bug relies on
    mp.setFixtureRigProps(20, hung);

    // A second bar fixture, deliberately drawn on its Side (2) -- a genuine
    // choice, must survive.
    mp.setFixturePosition(21, 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps sideBar;
    sideBar.trussId = bar->id();
    sideBar.studioMount = 2;
    mp.setFixtureRigProps(21, sideBar);

    // Mounted straight on a VERTICAL tower/truss instead: the default is
    // already the right plane (this is why T1-B1/T1-B2-style direct tower
    // mounts render correctly with no repair at all) -- must be untouched.
    mp.setFixturePosition(22, 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps onTower;
    onTower.trussId = tower->id();
    onTower.mountingType = Truss::SideArm;
    QCOMPARE(onTower.studioMount, 1);
    mp.setFixtureRigProps(22, onTower);

    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();

    MonitorProperties mp2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
        if (reader.name() == QStringLiteral("Monitor"))
            mp2.loadXML(reader, nullptr);

    QCOMPARE(mp2.fixtureRigProps(20).studioMount, 0);   // corrected to Top
    QCOMPARE(mp2.fixtureRigProps(21).studioMount, 2);   // a real choice: untouched
    QCOMPARE(mp2.fixtureRigProps(22).studioMount, 1);   // vertical: already right

    // The live, no-reload path: a dialog committing a fresh mount onto the
    // horizontal bar must get the same correction applied immediately
    // (repairMountPlane(), the single-fixture half of the load-time pass),
    // not only after the next save/load round-trip.
    FixtureRigProps live;
    live.trussId = bar->id();
    live.mountingType = Truss::TopHung;
    QCOMPARE(live.studioMount, 1);
    mp.repairMountPlane(live);
    QCOMPARE(live.studioMount, 0);
}

/* "why do the lanterns not all look the same": three of six lanterns on
   hangers kept the Top face of what they hung from before and were drawn on
   their side. A fixture on a hanger hangs straight -- on load, and when it is
   hung. */
void MonitorProperties_Test::aFixtureOnAHangerHangsStraight()
{
    MonitorProperties mp;
    FixtureRigProps rp;
    rp.hangerId = 2;
    rp.mountingType = Truss::TopHung;
    rp.studioMount = 0;            // left from a truss mount
    rp.studioAngle = 90.0f;
    rp.trussOffset = 0.111f;
    mp.repairMountPlane(rp);
    QCOMPARE(rp.studioMount, 1);
    QCOMPARE(rp.studioAngle, 0.0f);

    // And when a show loads with it so.
    rp.studioMount = 0;
    mp.setFixtureRigProps(7, rp);
    mp.repairMountPlanes();
    QCOMPARE(mp.fixtureRigProps(7).studioMount, 1);
}

/* "a truss can be horizontal on 2 stands": its ends on the two stands'
   tops, its run and length following them; a stand removed leaves it where
   it was; it saves and loads; a vertical truss doesn't span. */
void MonitorProperties_Test::aTrussSpansTwoStands()
{
    MonitorProperties mp;
    Stand *a = mp.addStand();
    a->setOriginX(1.0f); a->setOriginY(2.0f); a->setHeight(3.0f);
    Stand *b = mp.addStand();
    b->setOriginX(5.0f); b->setOriginY(2.0f); b->setHeight(3.0f);
    Truss *t = mp.addTruss();
    t->setType(Truss::Horizontal);
    t->setLength(1.0f);
    t->setStandId(a->id());
    t->setEndStandId(b->id());
    QVERIFY(t->spansStands());
    mp.recomputeStandMounts();
    QCOMPARE(t->origin(), QVector3D(1.0f, 2.0f, 3.0f));
    QVERIFY(qAbs(t->length() - 4.0f) < 0.001f);
    QVERIFY((t->positionAt(t->length()) - QVector3D(5.0f, 2.0f, 3.0f)).length() < 0.001f);

    // The other stand moves: the truss follows.
    b->setOriginX(1.0f); b->setOriginY(8.0f);
    mp.recomputeStandMounts();
    QVERIFY(qAbs(t->length() - 6.0f) < 0.001f);
    QVERIFY((t->positionAt(t->length()) - QVector3D(1.0f, 8.0f, 3.0f)).length() < 0.001f);

    // Saved and loaded.
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    t->saveXML(&w);
    buf.close();
    QVERIFY(buf.data().contains("EndStand="));

    // Vertical: no span.
    t->setType(Truss::Vertical);
    QVERIFY(!t->spansStands());
    t->setType(Truss::Horizontal);

    // The other stand removed: the truss keeps where it was, on the first.
    mp.removeStand(b->id());
    QCOMPARE(t->endStandId(), Stand::invalidId());
    QCOMPARE(t->standId(), a->id());
    QVERIFY(qAbs(t->length() - 6.0f) < 0.001f);
}

void MonitorProperties_Test::settingAFaceTakesTheRiserFaceWithIt()
{
    MonitorProperties mp;
    StagePlatform *p = mp.addPlatform();
    p->setOriginX(2); p->setOriginY(3); p->setWidth(4); p->setDepth(1); p->setHeight(0.8f);

    mp.setFixturePosition(5, 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp;
    rp.riserPlatformId = p->id();
    rp.riserFace = FixtureRigProps::RiserFront;
    rp.riserU = 1.0f; rp.riserV = 0.5f;
    mp.setFixtureRigProps(5, rp);
    QCOMPARE(mp.fixtureRigPosition(5), QVector3D(3.0f, 4.0f, 0.5f));   // front face

    /* Laying it on the top plane must MOVE it onto the top, not just redraw it
       there -- otherwise the load-time repair would spend its life undoing a
       perfectly deliberate edit. */
    rp = mp.fixtureRigProps(5);
    rp.setStudioMount(0);
    QCOMPARE(rp.riserFace, int(FixtureRigProps::RiserTop));
    mp.setFixtureRigProps(5, rp);
    QCOMPARE(mp.fixtureRigPosition(5), QVector3D(3.0f, 3.5f, 0.8f));   // top surface

    // Side has no riser equivalent: leave the face as it was rather than
    // inventing one and teleporting the fixture.
    rp.setStudioMount(2);
    QCOMPARE(rp.studioMount, 2);
    QCOMPARE(rp.riserFace, int(FixtureRigProps::RiserTop));

    // Off a riser the face is inert, so the plane is free.
    FixtureRigProps free_;
    free_.riserFace = FixtureRigProps::RiserFront;
    free_.setStudioMount(0);
    QCOMPARE(free_.studioMount, 0);
    QCOMPARE(free_.riserFace, int(FixtureRigProps::RiserFront));
}

void MonitorProperties_Test::stageOrigin()
{
    MonitorProperties mp;
    // Default is 0,0.
    QCOMPARE(mp.stageOrigin(), QPointF(0, 0));

    mp.setStageOrigin(QPointF(2.5, -1.25));
    QCOMPARE(mp.stageOrigin(), QPointF(2.5, -1.25));

    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();

    MonitorProperties mp2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
    {
        if (reader.name() == QStringLiteral("Monitor"))
            QVERIFY(mp2.loadXML(reader, nullptr));
    }
    QCOMPARE(mp2.stageOrigin(), QPointF(2.5, -1.25));

    // A zero origin is omitted from XML but still loads as 0,0.
    MonitorProperties mp3;
    QByteArray buf2;
    QXmlStreamWriter w2(&buf2);
    w2.writeStartDocument();
    mp3.saveXML(&w2, nullptr);
    w2.writeEndDocument();
    QVERIFY(!buf2.contains("OriginX"));
}

void MonitorProperties_Test::imagesXmlRoundTrip()
{
    MonitorProperties mp;
    const quint32 id = mp.addImage("/tmp/backdrop.png");
    MonitorProperties::MonitorImage img = mp.image(id);
    img.name    = "Cyc";
    img.plane   = MonitorProperties::MonitorImage::FrontBackdrop;
    img.originX = 1.5f;
    img.originY = 0.25f;
    img.originZ = 0.75f;
    img.width   = 6.0f;
    img.height  = 3.0f;
    img.rotation = 37.5f;
    img.layerId = 4;
    img.groupId = 9;
    img.locked  = true;
    mp.setImage(img);

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

    QCOMPARE(mp2.images().count(), 1);
    QVERIFY(mp2.hasImage(id));
    const MonitorProperties::MonitorImage r = mp2.image(id);
    QCOMPARE(r.name, QString("Cyc"));
    QCOMPARE(r.source, QString("/tmp/backdrop.png"));
    QCOMPARE(r.plane, int(MonitorProperties::MonitorImage::FrontBackdrop));
    QCOMPARE(r.originX, 1.5f);
    QCOMPARE(r.originZ, 0.75f);
    QCOMPARE(r.width, 6.0f);
    QCOMPARE(r.height, 3.0f);
    QCOMPARE(r.rotation, 37.5f);
    QCOMPARE(r.layerId, quint32(4));
    QCOMPARE(r.groupId, quint32(9));
    QVERIFY(r.locked);
}

void MonitorProperties_Test::childBarFollowsParent()
{
    MonitorProperties mp;
    Truss *parent = mp.addTruss();
    parent->setType(Truss::Horizontal);
    parent->setOrigin(QVector3D(1, 2, 4));    // 4 m high
    parent->setDirection(QPointF(1, 0));      // runs +X
    parent->setLength(6);
    parent->setWidth(0.0f);                   // zero half-thickness → clean math

    Truss *bar = mp.addTruss();
    bar->setLength(2);
    bar->setParentTrussId(parent->id());
    bar->setParentOffset(3.0f);               // Along = 3 m
    bar->setBarFace(Truss::FaceBottom);
    bar->setBarStandoff(1.0f);                // 1 m below the bottom face
    bar->setBarRun(Truss::RunAlong);

    mp.recomputeChildTrusses();
    // attach (4,2,4); Bottom standoff 1 → base (4,2,3); Along dir (1,0), centred
    // on a length-2 bar → origin (3,2,3).
    QCOMPARE(bar->type(), Truss::Horizontal);
    QCOMPARE(bar->origin(), QVector3D(3, 2, 3));

    // Downstage face pushes the base +Y.
    bar->setBarFace(Truss::FaceDownstage);
    mp.recomputeChildTrusses();
    QCOMPARE(bar->origin(), QVector3D(3, 3, 4));   // base (4,3,4) - dir*1

    // Drop run → a Vertical hanging bar, base kept (no centring).
    bar->setBarFace(Truss::FaceBottom);
    bar->setBarRun(Truss::RunDrop);
    mp.recomputeChildTrusses();
    QCOMPARE(bar->type(), Truss::Vertical);
    QCOMPARE(bar->origin(), QVector3D(4, 2, 3));

    // Move the parent → the bar follows.
    parent->setOrigin(QVector3D(0, 0, 5));
    mp.recomputeChildTrusses();
    QCOMPARE(bar->origin(), QVector3D(3, 0, 4));

    // XML round-trip keeps the mount params + re-derives on load.
    bar->setBarFace(Truss::FaceStageRight);
    bar->setBarStandoff(0.5f);
    bar->setBarRun(Truss::RunAcross);
    QByteArray buf;
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    mp.saveXML(&w, nullptr);
    w.writeEndDocument();
    MonitorProperties mp2;
    QXmlStreamReader r(buf);
    while (r.readNextStartElement())
        if (r.name() == QStringLiteral("Monitor"))
            QVERIFY(mp2.loadXML(r, nullptr));
    Truss *bar2 = mp2.truss(bar->id());
    QVERIFY(bar2 != nullptr);
    QCOMPARE(bar2->parentTrussId(), parent->id());
    QCOMPARE(bar2->parentOffset(), 3.0f);
    QCOMPARE(bar2->barFace(), int(Truss::FaceStageRight));
    QCOMPARE(bar2->barStandoff(), 0.5f);
    QCOMPARE(bar2->barRun(), int(Truss::RunAcross));
}

void MonitorProperties_Test::studioFrameDerivation()
{
    MonitorProperties mp;

    // A studio group with a local frame, and a fixture placed in it.
    mp.createGroup(1, "Pod", MonitorProperties::defaultLayerId, 0);
    mp.setGroupHasFrame(1, true);
    mp.setGroupFrame(1, QVector3D(5, 5, 0), 0.0f);

    mp.setFixturePosition(7, 0, 0, QVector3D(0, 0, 0));   // fixture must exist
    mp.setFixtureGroup(7, 1);
    QCOMPARE(mp.fixtureFrameGroup(7), quint32(1));

    FixtureRigProps rp;
    rp.groupLocal = QVector3D(1, 2, 0.5f);
    mp.setFixtureRigProps(7, rp);

    // rotation 0 → world = origin + local, exactly.
    QCOMPARE(mp.fixtureRigPosition(7), QVector3D(6, 7, 0.5f));

    // rotate the frame 90°: local (1,0,0) about Z → world (origin.x, origin.y+1).
    mp.setFixtureRigProps(7, [] { FixtureRigProps r; r.groupLocal = QVector3D(2, 0, 0); return r; }());
    mp.setGroupRotation(1, 90.0f);
    const QVector3D w = mp.fixtureRigPosition(7);
    QVERIFY(qAbs(w.x() - 5.0f) < 1e-4f);
    QVERIFY(qAbs(w.y() - 7.0f) < 1e-4f);

    // world<->local inverse round-trips.
    const QVector3D local = mp.worldToGroupLocal(1, w);
    QVERIFY(qAbs(local.x() - 2.0f) < 1e-4f);
    QVERIFY(qAbs(local.y() - 0.0f) < 1e-4f);

    // Nested: a frameless child under a frame parent resolves to the parent.
    mp.createGroup(2, "Sub", MonitorProperties::defaultLayerId, 1);   // parent = 1
    mp.setFixturePosition(8, 0, 0, QVector3D(0, 0, 0));
    mp.setFixtureGroup(8, 2);
    QCOMPARE(mp.fixtureFrameGroup(8), quint32(1));   // walks up to the framed group

    // A frameless group derives nothing (fixtureFrameGroup == 0).
    mp.setGroupHasFrame(1, false);
    QCOMPARE(mp.fixtureFrameGroup(7), quint32(0));
}

void MonitorProperties_Test::studioFrameXmlRoundTrip()
{
    MonitorProperties mp;
    mp.createGroup(3, "Step", MonitorProperties::defaultLayerId, 0);
    mp.setGroupHasFrame(3, true);
    mp.setGroupFrame(3, QVector3D(1.5f, 2.5f, 0.25f), 30.0f);
    mp.setGroupBinding(3, 42, 0.2f, 0.3f);

    mp.setFixturePosition(9, 0, 0, QVector3D(0, 0, 0));
    mp.setFixtureGroup(9, 3);
    FixtureRigProps rp; rp.groupLocal = QVector3D(0.4f, -0.6f, 1.2f);
    mp.setFixtureRigProps(9, rp);

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

    const MonitorProperties::MonitorGroup g = mp2.group(3);
    QVERIFY(g.hasFrame);
    QCOMPARE(g.origin, QVector3D(1.5f, 2.5f, 0.25f));
    QCOMPARE(g.rotation, 30.0f);
    QCOMPARE(g.boundFxGroup, quint32(42));
    QCOMPARE(g.pitchX, 0.2f);
    QCOMPARE(g.pitchY, 0.3f);
    QCOMPARE(mp2.fixtureRigProps(9).groupLocal, QVector3D(0.4f, -0.6f, 1.2f));
}

void MonitorProperties_Test::studioFrameSlavedToPlatform()
{
    MonitorProperties mp;
    StagePlatform *pl = mp.addPlatform();
    pl->setOriginX(2); pl->setOriginY(3); pl->setWidth(4); pl->setDepth(1); pl->setHeight(0.8f);

    // A studio group anchored to the platform: its frame is slaved to it.
    const quint32 gid = 5;
    mp.createGroup(gid, "US-1", MonitorProperties::defaultLayerId, 0);
    mp.setGroupAnchor(gid, "platform", pl->id());
    mp.setGroupHasFrame(gid, true);
    mp.recomputeAnchoredFrames();
    QCOMPARE(mp.group(gid).origin, QVector3D(2, 3, 0));

    // A member placed in the frame derives origin + local.
    mp.setFixturePosition(9, 0, 0, QVector3D(0, 0, 0));
    mp.setFixtureGroup(9, gid);
    FixtureRigProps rp; rp.groupLocal = QVector3D(1.0f, 1.0f, 0.5f);
    mp.setFixtureRigProps(9, rp);
    QCOMPARE(mp.fixtureRigPosition(9), QVector3D(3, 4, 0.5f));

    // Move the platform: the slaved frame AND the member follow.
    pl->setOriginX(5); pl->setOriginY(6);
    mp.recomputeAnchoredFrames();
    QCOMPARE(mp.group(gid).origin, QVector3D(5, 6, 0));
    QCOMPARE(mp.fixtureRigPosition(9), QVector3D(6, 7, 0.5f));
}

void MonitorProperties_Test::gridUnitsConvertExactly()
{
    QCOMPARE(MonitorProperties::gridUnitsFor(MeasurementUnits::Millimetres), MonitorProperties::Meters);
    QCOMPARE(MonitorProperties::gridUnitsFor(MeasurementUnits::Centimetres), MonitorProperties::Meters);
    QCOMPARE(MonitorProperties::gridUnitsFor(MeasurementUnits::Metres), MonitorProperties::Meters);
    QCOMPARE(MonitorProperties::gridUnitsFor(MeasurementUnits::Inches), MonitorProperties::Feet);
    QCOMPARE(MonitorProperties::gridUnitsFor(MeasurementUnits::Feet), MonitorProperties::Feet);

    MonitorProperties mp;
    mp.setGridSize(QVector3D(10, 4, 8));
    mp.setFixturePosition(1, 0, 0, QVector3D(1000, 2000, 3));
    mp.convertGridUnits(MonitorProperties::Feet);
    QCOMPARE(mp.gridUnits(), MonitorProperties::Feet);
    QVERIFY(qAbs(mp.gridSize().x() - 32.8084f) < 1e-3f);
    QVERIFY(qAbs(mp.gridSize().z() - 26.2467f) < 1e-3f);
    QCOMPARE(mp.stageCentre(), QVector3D(5.0f, 4.0f, 0.0f));          // same stage
    QCOMPARE(mp.fixturePosition(1, 0, 0), QVector3D(1000, 2000, 3));   // mm, untouched

    mp.convertGridUnits(MonitorProperties::Feet);   // already: nothing
    QVERIFY(qAbs(mp.gridSize().x() - 32.8084f) < 1e-3f);
    mp.convertGridUnits(MonitorProperties::Meters);  // and back, not grown
    QVERIFY(qAbs(mp.gridSize().x() - 10.0f) < 1e-4f);
    QVERIFY(qAbs(mp.gridSize().y() - 4.0f) < 1e-4f);
}

void MonitorProperties_Test::gridSizeSavedWholeAndExact()
{
    MonitorProperties mp;
    mp.setGridSize(QVector3D(10, 4, 8));
    mp.convertGridUnits(MonitorProperties::Feet);

    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();

    // Whole feet where an older reader (toInt) looks, the exact extent
    // beside them, the unit as the person has it.
    QVERIFY2(buf.contains("Width=\"33\""), buf.constData());
    QVERIFY(buf.contains("Depth=\"26\""));
    QVERIFY(buf.contains("ExactWidth=\"32.8084"));
    QVERIFY(buf.contains("Units=\"1\""));

    MonitorProperties mp2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
        if (reader.name() == QStringLiteral("Monitor"))
            QVERIFY(mp2.loadXML(reader, nullptr));
    QCOMPARE(mp2.gridUnits(), MonitorProperties::Feet);
    QVERIFY(qAbs(mp2.gridSize().x() - 32.8084f) < 1e-3f);
    QVERIFY(qAbs(mp2.gridSize().z() - 26.2467f) < 1e-3f);

    // A whole stage writes no Exact* at all, as before.
    MonitorProperties whole;
    whole.setGridSize(QVector3D(12, 3, 9));
    QByteArray buf2;
    QXmlStreamWriter w2(&buf2);
    w2.writeStartDocument();
    whole.saveXML(&w2, nullptr);
    w2.writeEndDocument();
    QVERIFY(!buf2.contains("Exact"));
    QVERIFY(buf2.contains("Width=\"12\""));
    QVERIFY(buf2.contains("Units=\"0\""));

    // A show this fork wrote before Exact* ("32.8084" in Width) is read as
    // the number, not as 0.
    QByteArray old = buf2;
    old.replace("Width=\"12\"", "Width=\"32.8084\"");
    MonitorProperties mp3;
    QXmlStreamReader r3(old);
    while (r3.readNextStartElement())
        if (r3.name() == QStringLiteral("Monitor"))
            QVERIFY(mp3.loadXML(r3, nullptr));
    QVERIFY(qAbs(mp3.gridSize().x() - 32.8084f) < 1e-3f);
}

void MonitorProperties_Test::reset()
{
    MonitorProperties mp;
    mp.setGridSize(QVector3D(10,10,10));
    mp.setGridUnits(MonitorProperties::Feet);
    mp.setPointOfView(MonitorProperties::FrontView);
    mp.setStageType(MonitorProperties::StageBox);
    mp.setLabelsVisible(true);
    mp.setFixturePosition(1,0,0,QVector3D(1,2,3));
    mp.setItemName(2,"foo");
    mp.setCommonBackgroundImage("img.png");
    mp.setStageOrigin(QPointF(3, 4));
    quint32 lyr = mp.addLayer("Extra");
    mp.setActiveLayerId(lyr);
    mp.createGroup(1, "G", lyr, 0);

    mp.reset();

    QCOMPARE(mp.gridSize(), QVector3D(5,3,5));
    QCOMPARE(mp.gridUnits(), MonitorProperties::Meters);
    QCOMPARE(mp.pointOfView(), MonitorProperties::Undefined);
    QCOMPARE(mp.stageType(), MonitorProperties::StageSimple);
    QCOMPARE(mp.labelsVisible(), true);
    QCOMPARE(mp.fixtureItemsID().count(), 0);
    QCOMPARE(mp.genericItemsID().count(), 0);
    QVERIFY(mp.commonBackgroundImage().isEmpty());
    // reset() clears custom layers back to just the Default (active).
    QCOMPARE(mp.layers().count(), 1);
    QVERIFY(mp.hasLayer(MonitorProperties::defaultLayerId));
    QCOMPARE(mp.activeLayerId(), MonitorProperties::defaultLayerId);
    QCOMPARE(mp.groups().count(), 0);
    QCOMPARE(mp.stageOrigin(), QPointF(0, 0));
}

void MonitorProperties_Test::stageStructureAddRemove()
{
    // Truss add already covered elsewhere (riserMount/childBarFollowsParent);
    // remove was never exercised.
    {
        MonitorProperties mp;
        Truss *a = mp.addTruss();
        Truss *b = mp.addTruss();
        QVERIFY(a->id() != b->id());
        QCOMPARE(mp.trusses().count(), 2);
        mp.removeTruss(a->id());
        QCOMPARE(mp.trusses().count(), 1);
        QCOMPARE(mp.trusses().first()->id(), b->id());
        // Removing an id that's already gone is a safe no-op.
        mp.removeTruss(a->id());
        QCOMPARE(mp.trusses().count(), 1);
    }

    // Pipe (Boom/Bar/House-electric — all the same object).
    {
        MonitorProperties mp;
        QCOMPARE(mp.pipes().count(), 0);
        Pipe *a = mp.addPipe();
        a->setName("Boom 1");
        Pipe *b = mp.addPipe();
        QVERIFY(a->id() != b->id());
        QCOMPARE(mp.pipes().count(), 2);
        mp.removePipe(a->id());
        QCOMPARE(mp.pipes().count(), 1);
        QCOMPARE(mp.pipes().first()->id(), b->id());
        mp.removePipe(a->id());
        QCOMPARE(mp.pipes().count(), 1);
    }

    // Stand.
    {
        MonitorProperties mp;
        QCOMPARE(mp.stands().count(), 0);
        Stand *a = mp.addStand();
        a->setName("Stand 1");
        Stand *b = mp.addStand();
        QVERIFY(a->id() != b->id());
        QCOMPARE(mp.stands().count(), 2);
        mp.removeStand(a->id());
        QCOMPARE(mp.stands().count(), 1);
        QCOMPARE(mp.stands().first()->id(), b->id());
        mp.removeStand(a->id());
        QCOMPARE(mp.stands().count(), 1);
    }

    // Tower.
    {
        MonitorProperties mp;
        QCOMPARE(mp.towers().count(), 0);
        Tower *a = mp.addTower();
        a->setName("Tower 1");
        Tower *b = mp.addTower();
        QVERIFY(a->id() != b->id());
        QCOMPARE(mp.towers().count(), 2);
        mp.removeTower(a->id());
        QCOMPARE(mp.towers().count(), 1);
        QCOMPARE(mp.towers().first()->id(), b->id());
        mp.removeTower(a->id());
        QCOMPARE(mp.towers().count(), 1);
    }

    // StageTarget.
    {
        MonitorProperties mp;
        QCOMPARE(mp.stageTargets().count(), 0);
        StageTarget *a = mp.addStageTarget();
        a->setName("Target 1");
        StageTarget *b = mp.addStageTarget();
        QVERIFY(a->id() != b->id());
        QCOMPARE(mp.stageTargets().count(), 2);
        mp.removeStageTarget(a->id());
        QCOMPARE(mp.stageTargets().count(), 1);
        QCOMPARE(mp.stageTargets().first()->id(), b->id());
        mp.removeStageTarget(a->id());
        QCOMPARE(mp.stageTargets().count(), 1);
    }
}

void MonitorProperties_Test::stageStructuresXmlRoundTrip()
{
    // One of each — Platform already covered by riserMount — survives a
    // save/load cycle: the actual guarantee that matters (a Look built in
    // Construction still has its rig on disk after a reload).
    MonitorProperties mp;
    Truss *truss = mp.addTruss();
    truss->setName("US Truss");
    Pipe *pipe = mp.addPipe();
    pipe->setName("Boom SL");
    Stand *stand = mp.addStand();
    stand->setName("Stand 1");
    Tower *tower = mp.addTower();
    tower->setName("Tower 1");
    StageTarget *target = mp.addStageTarget();
    target->setName("Centre Stage");
    target->setPosition(QVector3D(1.0f, 2.0f, 0.0f));

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

    QCOMPARE(mp2.trusses().count(), 1);
    QCOMPARE(mp2.trusses().first()->name(), QString("US Truss"));
    QCOMPARE(mp2.pipes().count(), 1);
    QCOMPARE(mp2.pipes().first()->name(), QString("Boom SL"));
    QCOMPARE(mp2.stands().count(), 1);
    QCOMPARE(mp2.stands().first()->name(), QString("Stand 1"));
    QCOMPARE(mp2.towers().count(), 1);
    QCOMPARE(mp2.towers().first()->name(), QString("Tower 1"));
    QCOMPARE(mp2.stageTargets().count(), 1);
    QCOMPARE(mp2.stageTargets().first()->name(), QString("Centre Stage"));
    QCOMPARE(mp2.stageTargets().first()->position(), QVector3D(1.0f, 2.0f, 0.0f));
}

void MonitorProperties_Test::aNewlyPlacedFixtureSavesNoJunk()
{
    MonitorProperties mp;

    // Exactly what placing a fixture on the plot does: a position, nothing else.
    mp.setFixturePosition(42, 0, 0, QVector3D(1, 2, 0));

    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();

    const QString xml = QString::fromUtf8(buf);
    QVERIFY2(xml.contains("FixedZoom") == false, qPrintable(xml));
    QVERIFY2(xml.contains("Hidden") == false, qPrintable(xml));
}

/* Save @p doc and load it back into @p into; @p edit may change the XML
   between (to make it look like an older show). */
static bool roundTrip(Doc &doc, Doc &into, const std::function<void(QByteArray &)> &edit = nullptr)
{
    QByteArray ws;
    QXmlStreamWriter ww(&ws);
    ww.writeStartDocument();
    if (!doc.saveXML(&ww))
        return false;
    ww.writeEndDocument();
    if (edit)
        edit(ws);
    QXmlStreamReader rr(ws);
    while (rr.readNextStartElement() && rr.name() != QStringLiteral("Engine"))
        ;
    return into.loadXML(rr);
}

void MonitorProperties_Test::anObjectsLockIsItsOwn()
{
    Doc doc(this);
    MonitorProperties *mp = doc.monitorProperties();
    const quint32 rig = mp->addLayer("Rig");
    Truss *pinned = mp->addTruss();
    pinned->setName("Pinned");
    pinned->setLayerId(rig);
    pinned->setLocked(true);
    Truss *loose = mp->addTruss();
    loose->setName("Loose");
    loose->setLayerId(rig);
    Stand *onDefault = mp->addStand();
    onDefault->setLocked(true);
    mp->setLayerLocked(rig, true);

    // Saved and loaded: the layer's lock and each object's own, apart.
    Doc reloaded(this);
    QVERIFY(roundTrip(doc, reloaded));
    MonitorProperties *rp = reloaded.monitorProperties();
    QVERIFY(rp->itemLocksAreOwn());
    QVERIFY(rp->layer(rig).locked);
    QVERIFY(rp->truss(pinned->id())->locked());
    QVERIFY(!rp->truss(loose->id())->locked());
    QVERIFY(rp->stand(onDefault->id())->locked());
    QCOMPARE(rp->stand(onDefault->id())->layerId(), quint32(MonitorProperties::defaultLayerId));
    // No layer is made for a lock: an object stays where it is.
    QCOMPARE(rp->layers().size(), mp->layers().size());

    // Unlocking the layer leaves the pinned one pinned.
    rp->setLayerLocked(rig, false);
    QVERIFY(rp->truss(pinned->id())->locked());
    QCOMPARE(rp->migrateLegacyLocks(nullptr, nullptr), 0);   // nothing to do
    QVERIFY(rp->truss(pinned->id())->locked());
}

void MonitorProperties_Test::legacyLockedLayerIsTakenApart()
{
    Doc doc(this);
    MonitorProperties *mp = doc.monitorProperties();
    PowerDistribution *pd = doc.powerDistribution();
    const quint32 rig = mp->addLayer("Trusses");
    const quint32 held = mp->addLayer("Stage Lights");
    mp->setLayerLocked(held, true);
    const quint32 locked = mp->addLayer("Locked");
    mp->setLayerLocked(locked, true);

    // T-1's folder stayed on Trusses when the old load moved T-1 alone.
    Truss *t1 = mp->addTruss();
    t1->setName("T-1");
    const quint32 gid = mp->nextGroupId();
    mp->createGroup(gid, "T-1", rig, 0);
    t1->setGroupId(gid);
    t1->setLayerId(locked);
    t1->setLocked(true);
    // Its bar has no folder: it goes where its truss goes.
    Truss *bar = mp->addTruss();
    bar->setName("Bar 1");
    bar->setParentTrussId(t1->id());
    bar->setLayerId(locked);
    bar->setLocked(true);
    // A hanger on T-1 follows it.
    Hanger *hanger = mp->addHanger(Hanger::OnTruss, t1->id());
    hanger->setLayerId(locked);
    // A boom whose fixture is on Trusses.
    Pipe *boom = mp->addPipe();
    boom->setLayerId(locked);
    boom->setLocked(true);
    Fixture *onBoom = new Fixture(&doc);
    onBoom->setChannels(4);
    QVERIFY(doc.addFixture(onBoom));
    mp->setFixturePosition(onBoom->id(), 0, 0, QVector3D(1, 1, 0));
    mp->setFixtureLayer(onBoom->id(), rig);
    FixtureRigProps rpBoom;
    rpBoom.pipeId = boom->id();
    mp->setFixtureRigProps(onBoom->id(), rpBoom);
    // Nothing says where this stand was: it stays, still held.
    Stand *stand = mp->addStand();
    stand->setLayerId(locked);
    stand->setLocked(true);
    // Mirrored locks on a locked layer, from when an object's lock was a copy.
    StagePlatform *deck = mp->addPlatform();
    deck->setLayerId(held);
    deck->setLocked(true);
    PowerSource ps;
    ps.layerId = held;
    ps.locked = true;
    pd->sources() << ps;
    // Locked on its own on an unlocked layer (an old show): stays as it is.
    Tower *tower = mp->addTower();
    tower->setLocked(true);

    // As a show saved before 0.3: no ItemLocks mark.
    auto older = [](QByteArray &xml) {
        xml.replace(" ItemLocks=\"1\"", "");
        QVERIFY(!xml.contains("ItemLocks"));
    };
    Doc reloaded(this);
    QVERIFY(roundTrip(doc, reloaded, older));
    MonitorProperties *r = reloaded.monitorProperties();
    QVERIFY(r->itemLocksAreOwn());

    QCOMPARE(r->truss(t1->id())->layerId(), rig);
    QVERIFY(r->truss(t1->id())->locked());
    QCOMPARE(r->truss(bar->id())->layerId(), rig);
    QVERIFY(r->truss(bar->id())->locked());
    QCOMPARE(r->hanger(hanger->id())->layerId(), rig);
    QCOMPARE(r->pipe(boom->id())->layerId(), rig);
    QVERIFY(r->pipe(boom->id())->locked());
    QCOMPARE(r->stand(stand->id())->layerId(), locked);
    QVERIFY(r->stand(stand->id())->locked());
    // The layer stays for the stand, unlocked: the stand's lock is its own.
    QVERIFY(r->hasLayer(locked));
    QVERIFY(!r->layer(locked).locked);
    // The mirrored locks are gone; the layer still holds them.
    QVERIFY(!r->platform(deck->id())->locked());
    QVERIFY(!reloaded.powerDistribution()->sources().first().locked);
    QVERIFY(r->layer(held).locked);
    QVERIFY(r->tower(tower->id())->locked());
    QCOMPARE(r->tower(tower->id())->layerId(), quint32(MonitorProperties::defaultLayerId));

    // Once put right and saved, it is not done again.
    Doc again(this);
    QVERIFY(roundTrip(reloaded, again));
    QVERIFY(again.monitorProperties()->hasLayer(locked));
    QCOMPARE(again.monitorProperties()->stand(stand->id())->layerId(), locked);

    // With the stand placed too, nothing is left on it and it goes.
    stand->setLayerId(rig);
    Doc emptied(this);
    QVERIFY(roundTrip(doc, emptied, older));
    QVERIFY(!emptied.monitorProperties()->hasLayer(locked));
    QCOMPARE(emptied.monitorProperties()->truss(t1->id())->layerId(), rig);

    // A show that already has own locks keeps a layer named "Locked" as is.
    Doc current(this);
    QVERIFY(roundTrip(doc, current));
    QVERIFY(current.monitorProperties()->hasLayer(locked));
    QVERIFY(current.monitorProperties()->layer(locked).locked);
    QCOMPARE(current.monitorProperties()->truss(t1->id())->layerId(), locked);
    QVERIFY(current.monitorProperties()->platform(deck->id())->locked());
}

void MonitorProperties_Test::goboSwapsAreKeptPerFixtureAndSaved()
{
    Doc doc(this);
    Fixture *a = new Fixture(&doc);
    a->setChannels(4);
    QVERIFY(doc.addFixture(a));
    Fixture *b = new Fixture(&doc);
    b->setChannels(4);
    b->setAddress(10);
    QVERIFY(doc.addFixture(b));
    MonitorProperties *mp = doc.monitorProperties();

    QVERIFY(mp->goboSwap(a->id(), 3, 16).isEmpty());
    mp->setGoboSwap(a->id(), 3, 16, "logo.png");
    mp->setGoboSwap(a->id(), 3, 73, "logo.png");      // its shake
    mp->setGoboSwap(a->id(), 4, 28, "/abs/breakup.svg");
    QCOMPARE(mp->goboSwap(a->id(), 3, 16), QString("logo.png"));
    QCOMPARE(mp->goboSwaps(a->id()).size(), 3);
    QVERIFY(mp->goboSwaps(b->id()).isEmpty());
    QVERIFY(mp->goboSwap(a->id(), 3, 17).isEmpty());   // another slot
    mp->setGoboSwap(a->id(), 3, 73, QString());         // restored
    QCOMPARE(mp->goboSwaps(a->id()).size(), 2);
    mp->setGoboSwap(b->id(), 9, 9, QString());          // nothing to restore
    QVERIFY(mp->goboSwaps(b->id()).isEmpty());

    // Taking it off the plot keeps the glass; it is still the same unit.
    mp->removeFixture(a->id());
    QCOMPARE(mp->goboSwaps(a->id()).size(), 2);

    // Saved and loaded with the workspace.
    QByteArray ws;
    QXmlStreamWriter ww(&ws);
    ww.writeStartDocument();
    QVERIFY(doc.saveXML(&ww));
    ww.writeEndDocument();
    QVERIFY(ws.contains("<GoboSwap"));
    Doc back(this);
    QXmlStreamReader rr(ws);
    while (rr.readNextStartElement() && rr.name() != QStringLiteral("Engine"))
        ;
    QVERIFY(back.loadXML(rr));
    QCOMPARE(back.monitorProperties()->goboSwap(a->id(), 3, 16), QString("logo.png"));
    QCOMPARE(back.monitorProperties()->goboSwap(a->id(), 4, 28), QString("/abs/breakup.svg"));
    QCOMPARE(back.monitorProperties()->goboSwaps(a->id()).size(), 2);

    // Deleting the fixture takes its glass: a reused id starts with none.
    QVERIFY(doc.deleteFixture(a->id()));
    QVERIFY(mp->goboSwaps(a->id()).isEmpty());

    // A cleared workspace has none.
    back.monitorProperties()->reset();
    QVERIFY(back.monitorProperties()->goboSwaps(a->id()).isEmpty());
}

void MonitorProperties_Test::theGoboLibraryImportsAndResolves()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString userDir = home.filePath("Gobos");
    GoboLibrary::setUserDirectoryOverride(userDir);

    QVERIFY(GoboLibrary::userDirectory().exists());
    QVERIFY(GoboLibrary::userGobos().isEmpty());
    QVERIFY(GoboLibrary::imageFilters().contains("*.svg"));
    QVERIFY(GoboLibrary::imageFilters().contains("*.png"));

    auto write = [&](const QString &path, const QByteArray &data) {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    };
    QDir(home.path()).mkpath("elsewhere");
    const QString logo = home.filePath("elsewhere/logo.png");
    write(logo, "picture one");

    // Imported: copied in under its own name.
    QCOMPARE(GoboLibrary::import(logo), QString("logo.png"));
    QVERIFY(QFile::exists(userDir + "/logo.png"));
    QCOMPARE(GoboLibrary::userGobos(), QStringList() << "logo.png");
    // The same picture again: the same name, no second copy.
    QCOMPARE(GoboLibrary::import(logo), QString("logo.png"));
    QCOMPARE(GoboLibrary::userGobos().size(), 1);
    // A DIFFERENT picture with that name: a free name, the first kept.
    QDir(home.path()).mkpath("other");
    const QString other = home.filePath("other/logo.png");
    write(other, "picture two");
    QCOMPARE(GoboLibrary::import(other), QString("logo-2.png"));
    QFile first(userDir + "/logo.png");
    QVERIFY(first.open(QIODevice::ReadOnly));
    QCOMPARE(first.readAll(), QByteArray("picture one"));
    // Already in the folder: nothing copied.
    QCOMPARE(GoboLibrary::import(userDir + "/logo-2.png"), QString("logo-2.png"));
    QVERIFY(GoboLibrary::import(home.filePath("nope.png")).isEmpty());
    QVERIFY(GoboLibrary::import(home.path()).isEmpty());    // a folder

    // Resolved: a bare name from the folder, an absolute path as it is.
    QCOMPARE(GoboLibrary::resolve("logo.png"), QDir(userDir).absoluteFilePath("logo.png"));
    QCOMPARE(GoboLibrary::resolve(logo), logo);
    QVERIFY(GoboLibrary::resolve("gone.png").isEmpty());
    QVERIFY(GoboLibrary::resolve(home.filePath("gone.png")).isEmpty());
    QVERIFY(GoboLibrary::resolve(QString()).isEmpty());

    GoboLibrary::setUserDirectoryOverride(QString());
}

void MonitorProperties_Test::patternPicturesAreKeptWithTheShow()
{
    QTemporaryDir show, downloads;
    QVERIFY(show.isValid() && downloads.isValid());
    auto writeFile = [](const QString &path, const QByteArray &bytes) {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(bytes);
    };
    const QString picked = downloads.filePath("stones.jpg");
    writeFile(picked, "stones");

    Doc doc(this);
    MonitorProperties *mp = doc.monitorProperties();
    Truss *t = mp->addTruss();
    t->setSock(true);
    t->setSockPattern(picked);
    Tower *tw = mp->addTower();
    tw->setFaces(true);
    tw->setFacesPattern(picked);

    auto save = [&doc]() {
        QByteArray out;
        QXmlStreamWriter w(&out);
        doc.monitorProperties()->keepPatternsWithShow(&doc);
        doc.monitorProperties()->saveXML(&w, &doc);
        return out;
    };

    // An unsaved show has no folder yet: the picture stays where it is.
    doc.setWorkspacePath(QString());
    QVERIFY(save().contains(picked.toUtf8()));
    QCOMPARE(t->sockPattern(), picked);

    // ...and stays absolute whatever the current directory is (it used to
    // come back relative to it when the picture lay under it).
    const QString cwd = QDir::currentPath();
    QDir::setCurrent(downloads.path());
    const QByteArray unsaved = save();
    QDir::setCurrent(cwd);
    QVERIFY(unsaved.contains(QFileInfo(picked).absoluteFilePath().toUtf8()));
    QCOMPARE(doc.normalizeComponentPath(picked), QFileInfo(picked).absoluteFilePath());

    // Saved into a folder: copied into images/, written relative, used from there.
    doc.setWorkspacePath(show.path());
    QByteArray xml = save();
    const QString copy = QDir(show.path()).absoluteFilePath("images/stones.jpg");
    QVERIFY(QFile::exists(copy));
    QVERIFY(xml.contains("SockPattern=\"images/stones.jpg\""));
    QVERIFY(xml.contains("FacesPattern=\"images/stones.jpg\""));
    QVERIFY(!xml.contains(downloads.path().toUtf8()));
    QCOMPARE(QFileInfo(t->sockPattern()).canonicalFilePath(), QFileInfo(copy).canonicalFilePath());

    // Saving again copies nothing more.
    save();
    QCOMPARE(QDir(show.filePath("images")).entryList(QDir::Files).count(), 1);

    // A different picture of the same name doesn't overwrite it.
    QTemporaryDir other;
    writeFile(other.filePath("stones.jpg"), "other stones");
    t->setSockPattern(other.filePath("stones.jpg"));
    xml = save();
    QVERIFY(xml.contains("SockPattern=\"images/stones-2.jpg\""));
    QVERIFY(xml.contains("FacesPattern=\"images/stones.jpg\""));

    // Loading resolves them against the show's folder.
    MonitorProperties back;
    QXmlStreamReader reader(xml);
    while (reader.readNextStartElement())
        if (reader.name() == QStringLiteral("Monitor"))
            QVERIFY(back.loadXML(reader, &doc));
    QCOMPARE(back.trusses().count(), 1);
    QCOMPARE(QFileInfo(back.trusses().first()->sockPattern()).canonicalFilePath(),
             QFileInfo(show.filePath("images/stones-2.jpg")).canonicalFilePath());
}

void MonitorProperties_Test::savingTheShowKeepsItsPatternPictures()
{
    QTemporaryDir show, downloads;
    QVERIFY(show.isValid() && downloads.isValid());
    const QString picked = downloads.filePath("velour.jpg");
    {
        QFile f(picked);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("velour");
    }

    Doc doc(this);
    doc.setWorkspacePath(show.path());
    MonitorProperties *mp = doc.monitorProperties();
    Truss *t = mp->addTruss();
    t->setSock(true);
    t->setSockPattern(picked);
    Tower *tw = mp->addTower();
    tw->setFaces(true);
    tw->setFacesPattern(picked);

    const QString copy = QDir(show.path()).absoluteFilePath("images/velour.jpg");

    // An undo snapshot is not a save: nothing is copied, the path is kept.
    const QByteArray snap = mp->snapshot(&doc);
    QVERIFY(!QFile::exists(copy));
    QVERIFY(!QDir(show.path()).exists("images"));
    QCOMPARE(t->sockPattern(), picked);
    QCOMPARE(tw->facesPattern(), picked);
    QVERIFY(!snap.contains("images/velour.jpg"));

    // Saving the show copies the picture beside it and writes it relative.
    QByteArray xml;
    QXmlStreamWriter w(&xml);
    w.writeStartDocument();
    QVERIFY(doc.saveXML(&w));
    w.writeEndDocument();
    QVERIFY(QFile::exists(copy));
    QVERIFY2(xml.contains("SockPattern=\"images/velour.jpg\""), xml.constData());
    QVERIFY2(xml.contains("FacesPattern=\"images/velour.jpg\""), xml.constData());
    QVERIFY(!xml.contains(downloads.path().toUtf8()));
    QCOMPARE(QFileInfo(t->sockPattern()).canonicalFilePath(), QFileInfo(copy).canonicalFilePath());
    QCOMPARE(QFileInfo(tw->facesPattern()).canonicalFilePath(), QFileInfo(copy).canonicalFilePath());
}

void MonitorProperties_Test::aLensSetByHandIsSavedWithTheLight()
{
    MonitorProperties mp;
    mp.setFixturePosition(3, 0, 0, QVector3D(1, 1, 0));
    mp.setFixturePosition(4, 0, 0, QVector3D(2, 1, 0));
    FixtureRigProps zoomed = mp.fixtureRigProps(3);
    zoomed.lensDeg = 26.0f;
    mp.setFixtureRigProps(3, zoomed);
    FixtureRigProps plain = mp.fixtureRigProps(4);
    plain.panZeroDir = 90.0f;                  // a rig entry, but no lens
    mp.setFixtureRigProps(4, plain);

    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();

    // Exactly one Lens, on the light set by hand.
    const QString xml = QString::fromUtf8(buf);
    QCOMPARE(xml.count(QStringLiteral("Lens=")), 1);
    QRegularExpression rig(QStringLiteral("<FixtureRig [^>]*FID=\"3\"[^>]*>"));
    const QRegularExpressionMatch m = rig.match(xml);
    QVERIFY2(m.hasMatch(), qPrintable(xml));
    QVERIFY2(m.captured(0).contains(QStringLiteral("Lens=\"26.0\"")), qPrintable(m.captured(0)));

    MonitorProperties back;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
        if (reader.name() == QStringLiteral("Monitor"))
            QVERIFY(back.loadXML(reader, nullptr));
    QCOMPARE(back.fixtureRigProps(3).lensDeg, 26.0f);
    QCOMPARE(back.fixtureRigProps(4).lensDeg, 0.0f);
    QCOMPARE(back.fixtureRigProps(4).panZeroDir, 90.0f);
}

void MonitorProperties_Test::aFixedLightIsAimedByPanAndTilt()
{
    FixtureRigProps rp;
    QVector3D dir;
    QVERIFY(!rp.fixedAimDirection(dir));            // not aimed: as its mount says

    rp.fixedAim = true;
    rp.panZeroDir = 0.0f;                            // downstage
    rp.aimDown = 40.0f;
    QVERIFY(rp.fixedAimDirection(dir));
    QVERIFY(qAbs(dir.x()) < 1e-4f);
    QVERIFY(qAbs(dir.y() - float(qCos(qDegreesToRadians(40.0)))) < 1e-4f);   // toward the house
    QVERIFY(qAbs(dir.z() + float(qSin(qDegreesToRadians(40.0)))) < 1e-4f);   // and down
    rp.panZeroDir = 90.0f;                           // stage right is -X
    rp.aimDown = 0.0f;
    QVERIFY(rp.fixedAimDirection(dir));
    QVERIFY(qAbs(dir.x() + 1.0f) < 1e-4f && qAbs(dir.z()) < 1e-4f);
    rp.aimDown = 90.0f;                              // straight down
    QVERIFY(rp.fixedAimDirection(dir));
    QVERIFY(qAbs(dir.z() + 1.0f) < 1e-4f);

    // Saved and loaded; an unaimed fixture writes nothing new.
    MonitorProperties mp;
    FixtureRigProps aimed;
    aimed.fixedAim = true;
    aimed.panZeroDir = 30.0f;
    aimed.aimDown = 45.0f;
    mp.setFixtureRigProps(1, aimed);
    mp.setFixtureRigProps(2, FixtureRigProps());
    QByteArray out;
    {
        QXmlStreamWriter w(&out);
        QVERIFY(mp.saveXML(&w, nullptr));
    }
    QVERIFY(out.contains("AimDown=\"45.0\""));
    QCOMPARE(out.count("AimDown"), 1);
    MonitorProperties back;
    QXmlStreamReader r(out);
    while (r.readNextStartElement())
        if (r.name() == QStringLiteral("Monitor"))
            QVERIFY(back.loadXML(r, nullptr));
    QVERIFY(back.fixtureRigProps(1).fixedAim);
    QCOMPARE(back.fixtureRigProps(1).aimDown, 45.0f);
    QCOMPARE(back.fixtureRigProps(1).panZeroDir, 30.0f);
    QVERIFY(!back.fixtureRigProps(2).fixedAim);
}

QTEST_GUILESS_MAIN(MonitorProperties_Test)

/* A hanger is rigging, like a bar: put on a tower truss part way up, its
   bracket is clamped to the tower's face, its arm reaches out, its chain
   drops to a hook -- and a lantern mounted on it hangs from that hook. It
   follows its tower; it is saved; removing the tower takes its hangers with
   it and leaves the lantern where it was, unmounted. On a Tower structure it
   clamps to the tower's side. */
void MonitorProperties_Test::aTrussSockIsSavedWithItsColourAndStretch()
{
    Truss t(4);
    t.setLength(6.0f);
    QVERIFY(!t.hasSock());
    QCOMPARE(t.sockColour(), QColor(Qt::white));
    QCOMPARE(t.sockFrom(), 0.0f);
    QCOMPARE(t.sockTo(), 6.0f);                         // the whole truss
    QVERIFY(!t.sockCovers(1.0f));                       // none: covers nothing

    auto roundTrip = [](const Truss &in, QByteArray *xml) {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        in.saveXML(&w);
        w.writeEndDocument();
        buf.close();
        *xml = buf.data();
        buf.open(QIODevice::ReadOnly);
        QXmlStreamReader r(&buf);
        r.readNextStartElement();
        Truss *out = new Truss(in.id());
        out->loadXML(r);
        return out;
    };
    QByteArray xml;
    delete roundTrip(t, &xml);
    QVERIFY(!xml.contains("Sock"));                     // no sock, nothing said

    t.setSock(true);
    t.setSockColour(QColor("#ff8000"));
    t.setSockRange(1.5f, 4.0f);
    QVERIFY(t.sockCovers(2.0f));
    QVERIFY(!t.sockCovers(1.0f) && !t.sockCovers(5.0f));
    Truss *back = roundTrip(t, &xml);
    QVERIFY(back->hasSock());
    QCOMPARE(back->sockColour(), QColor("#ff8000"));
    QCOMPARE(back->sockFrom(), 1.5f);
    QCOMPARE(back->sockTo(), 4.0f);
    delete back;

    // To its end: it follows the truss when the truss grows.
    t.setSockRange(0.0f, -1.0f);
    back = roundTrip(t, &xml);
    back->setLength(8.0f);
    QCOMPARE(back->sockTo(), 8.0f);
    delete back;

    // Out of range is held to the truss.
    t.setSockRange(2.0f, 20.0f);
    QCOMPARE(t.sockTo(), 6.0f);
}

void MonitorProperties_Test::aTowersFacesAreSavedWithTheirSidesAndPrint()
{
    Tower t(2);
    t.setHeight(2.4f);
    QVERIFY(!t.hasFaces());
    QCOMPARE(t.facesSides(), int(Tower::AllSides));
    QCOMPARE(t.facesTo(), 2.4f);                        // the whole tower

    auto roundTrip = [](const Tower &in, QByteArray *xml) {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        in.saveXML(&w);
        w.writeEndDocument();
        buf.close();
        *xml = buf.data();
        buf.open(QIODevice::ReadOnly);
        QXmlStreamReader r(&buf);
        r.readNextStartElement();
        Tower *out = new Tower(in.id());
        out->loadXML(r);
        return out;
    };
    QByteArray xml;
    delete roundTrip(t, &xml);
    QVERIFY(!xml.contains("Faces"));

    t.setFaces(true);
    t.setFacesSides(Tower::FrontSide | Tower::LeftSide);
    t.setFacesColour(QColor("#101010"));
    t.setFacesRange(0.3f, 2.0f);
    t.setFacesPattern("/shows/art/tower-wrap.png");
    QCOMPARE(t.facesPatternOpacity(), 1.0f);            // the print as it is
    delete roundTrip(t, &xml);
    QVERIFY(!xml.contains("FacesPatternOpacity"));
    t.setFacesPatternOpacity(0.4f);
    Tower *back = roundTrip(t, &xml);
    QCOMPARE(back->facesPatternOpacity(), 0.4f);
    QVERIFY(back->hasFaces());
    QCOMPARE(back->facesSides(), int(Tower::FrontSide | Tower::LeftSide));
    QCOMPARE(back->facesColour(), QColor("#101010"));
    QCOMPARE(back->facesFrom(), 0.3f);
    QCOMPARE(back->facesTo(), 2.0f);
    QCOMPARE(back->facesPattern(), QString("/shows/art/tower-wrap.png"));
    delete back;

    // The sock takes a print too.
    Truss truss(5);
    truss.setLength(3.0f);
    truss.setSock(true);
    truss.setSockPattern("/shows/art/sock.png");
    truss.setSockPatternOpacity(0.25f);
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    truss.saveXML(&w);
    w.writeEndDocument();
    buf.close();
    buf.open(QIODevice::ReadOnly);
    QXmlStreamReader r(&buf);
    r.readNextStartElement();
    Truss in(5);
    in.loadXML(r);
    QCOMPARE(in.sockPattern(), QString("/shows/art/sock.png"));
    QCOMPARE(in.sockPatternOpacity(), 0.25f);
    truss.setSockPatternOpacity(3.0f);
    QCOMPARE(truss.sockPatternOpacity(), 1.0f);         // held to 0..1
}

void MonitorProperties_Test::aLanternHangsOnItsHanger()
{
    MonitorProperties mp;
    Truss *tower = mp.addTruss();
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(2.0f, 1.0f, 0.0f));
    tower->setLength(3.0f);
    tower->setWidth(0.29f);

    Hanger *h = mp.addHanger(Hanger::OnTruss, tower->id());
    h->setAlong(2.5f);
    h->setAngle(90.0f);                                        // stage left, +X
    h->setArm(0.4f);
    h->setDrop(0.6f);
    mp.recomputeHangers();
    QVERIFY(qAbs(h->fastened().x() - (2.0f + 0.145f)) < 1e-4f); // the face, not the axis
    QVERIFY(qAbs(h->fastened().z() - 2.5f) < 1e-4f);
    QVERIFY(qAbs(h->hook().x() - (2.0f + 0.145f + 0.4f)) < 1e-4f);
    QVERIFY(qAbs(h->hook().z() - (2.5f - 0.6f)) < 1e-4f);

    FixtureRigProps rp;
    rp.hangerId = h->id();
    mp.setFixtureRigProps(7, rp);
    QCOMPARE(mp.fixtureRigProps(7).primaryMount(), FixtureRigProps::HangerMount);
    QCOMPARE(mp.fixtureRigPosition(7), h->hook());
    QCOMPARE(mp.fixtureOnHanger(h->id()), quint32(7));

    // Downstage is +Y.
    h->setAngle(0.0f);
    mp.recomputeHangers();
    QVERIFY(qAbs(mp.fixtureRigPosition(7).y() - (1.0f + 0.145f + 0.4f)) < 1e-4f);

    // It follows its tower.
    tower->setOrigin(QVector3D(5.0f, 1.0f, 0.0f));
    mp.recomputeChildTrusses();
    QVERIFY(qAbs(mp.fixtureRigPosition(7).x() - 5.0f) < 1e-4f);

    // Saved and read back, with what hangs on it.
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();
    buf.close();
    MonitorProperties mp2;
    buf.open(QIODevice::ReadOnly);
    QXmlStreamReader reader(&buf);
    while (reader.readNextStartElement())
        if (reader.name() == QStringLiteral("Monitor"))
            QVERIFY(mp2.loadXML(reader, nullptr));
    QCOMPARE(mp2.hangers().size(), 1);
    const Hanger *back = mp2.hangers().first();
    QCOMPARE(back->parentKind(), Hanger::OnTruss);
    QCOMPARE(back->arm(), 0.4f);
    QCOMPARE(back->drop(), 0.6f);
    QCOMPARE(mp2.fixtureRigProps(7).hangerId, back->id());
    QVERIFY((mp2.fixtureRigPosition(7) - mp.fixtureRigPosition(7)).length() < 1e-3f);

    // The tower goes: so do its hangers; the lantern stays, unmounted.
    mp.removeTruss(tower->id());
    QVERIFY(mp.hangers().isEmpty());
    QVERIFY(!mp.fixtureRigProps(7).onHanger());

    // On a Tower structure: clamped to its side.
    Tower *tw = mp.addTower();
    tw->setOriginX(1.0f);
    tw->setOriginY(1.0f);
    tw->setWidth(0.4f);
    tw->setDepth(0.4f);
    Hanger *th = mp.addHanger(Hanger::OnTower, tw->id());
    th->setAlong(2.0f);
    th->setAngle(270.0f);                                      // stage right, -X
    mp.recomputeHangers();
    QVERIFY(qAbs(th->fastened().x() - 1.0f) < 1e-4f);          // the tower's stage-right side
    QVERIFY(qAbs(th->fastened().y() - 1.2f) < 1e-4f);
}

void MonitorProperties_Test::aTowerOffsetIsTheHeight()
{
    MonitorProperties mp;

    Truss *tower = mp.addTruss();
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(1.67f, 1.372f, 0.0f));
    tower->setLength(3.048f);
    tower->setWidth(0.29f);

    Truss *pipe = mp.addTruss();
    pipe->setType(Truss::Horizontal);
    pipe->setOrigin(QVector3D(0.0f, 2.0f, 5.0f));
    pipe->setDirection(QPointF(1.0, 0.0));
    pipe->setLength(6.0f);
    pipe->setWidth(0.29f);

    /* T1-Spot from the show: on top of T-1, attached top-mounted. */
    FixtureRigProps rp;
    rp.trussId = tower->id();
    rp.trussOffset = 3.048f;
    rp.trussMountSide = FixtureRigProps::TopMounted;
    mp.setFixtureRigProps(0, rp);
    QVERIFY2(qAbs(double(mp.fixtureRigPosition(0).z()) - 3.048) < 1e-4,
             qPrintable(QString("a fixture on top of a 3.048 m tower sits at %1 m")
                        .arg(double(mp.fixtureRigPosition(0).z()))));

    // The default side, part way up: the height its properties show.
    rp.trussOffset = 2.654f;
    rp.trussMountSide = FixtureRigProps::UnderHung;
    mp.setFixtureRigProps(1, rp);
    QVERIFY(qAbs(double(mp.fixtureRigPosition(1).z()) - 2.654) < 1e-4);

    // A horizontal run still takes the nudge: top chord above, bottom below.
    rp.trussId = pipe->id();
    rp.trussOffset = 3.0f;
    rp.trussMountSide = FixtureRigProps::TopMounted;
    mp.setFixtureRigProps(2, rp);
    QVERIFY(qAbs(double(mp.fixtureRigPosition(2).z()) - 5.145) < 1e-4);
    rp.trussMountSide = FixtureRigProps::UnderHung;
    mp.setFixtureRigProps(3, rp);
    QVERIFY(qAbs(double(mp.fixtureRigPosition(3).z()) - 4.855) < 1e-4);
}

void MonitorProperties_Test::targetKindsAndAimPoint()
{
    /* A target kind only earns its place if it changes where light is aimed or
       what is drawn. These test the first. */
    QCOMPARE(StageTarget::defaultAimHeight(StageTarget::Kind::None), 0.0f);
    QVERIFY(StageTarget::defaultAimHeight(StageTarget::Kind::Person) > 1.2f);
    QVERIFY(StageTarget::defaultAimHeight(StageTarget::Kind::DrumKit)
            < StageTarget::defaultAimHeight(StageTarget::Kind::Person));

    MonitorProperties mp;
    mp.setAimSubjectHeight(1.4f);

    StagePlatform *deck = mp.addPlatform();
    deck->setOriginX(2.0f); deck->setOriginY(2.0f);
    deck->setWidth(4.0f); deck->setDepth(2.0f); deck->setHeight(0.5f);

    StageTarget *t = mp.addStageTarget();
    t->setPosition(QVector3D(3.0f, 3.0f, 0.0f));        // standing on the deck

    /* A plain target is exactly where it says it is... */
    QCOMPARE(double(mp.targetAimPoint(t).z()), 0.0);

    /* ...unless the caller says this is a follow-spot aim, which is how every
       show built before kinds existed behaves. */
    QVERIFY(qAbs(double(mp.targetAimPoint(t, true).z()) - 1.9) < 0.01);

    /* A target that knows what it is does not need telling. */
    t->setKind(StageTarget::Kind::Person);
    QVERIFY2(qAbs(double(mp.targetAimPoint(t).z()) - 1.9) < 0.01,
             "a Person should be aimed at chest height above what it stands on "
             "without the caller having to ask");

    /* ...and that chest is 80% of the person's own height: the follow-spot
       aim height does not size them ("the target is 5'4 .. I STILL see Z for
       the top of that head at 8.7 feet"). */
    mp.setAimSubjectHeight(1.6f);
    QVERIFY2(qAbs(double(mp.targetAimPoint(t).z()) - 1.9) < 0.01,
             "the follow-spot aim height moved a Person");
    QVERIFY(qAbs(double(mp.targetFigureHeight(t)) - 1.75) < 0.01);
    t->setPersonHeight(1.626f);                                    // 5'4"
    QVERIFY(qAbs(double(mp.targetFigureHeight(t)) - 1.626) < 0.001);
    QVERIFY(qAbs(double(mp.targetAimPoint(t).z()) - (0.5 + 1.626 * 0.8)) < 0.01);
    t->setPersonHeight(-1.0f);
    mp.setAimSubjectHeight(1.4f);

    /* A drum kit is lower than a vocalist -- the whole point of having kinds
       rather than one global height for everything on the stage. */
    t->setKind(StageTarget::Kind::DrumKit);
    const double kit = double(mp.targetAimPoint(t).z());
    t->setKind(StageTarget::Kind::Person);
    QVERIFY2(kit < double(mp.targetAimPoint(t).z()),
             "a drum kit is being aimed at the same height as a person");

    /* And one override beats any kind, which is what stops the kind list
       growing a new entry for every one-off. */
    t->setAimHeightOverride(0.25f);
    QVERIFY(qAbs(double(mp.targetAimPoint(t).z()) - 0.75) < 0.01);
}

void MonitorProperties_Test::targetFollowsWhatItIsBoundTo()
{
    /* A bound target is a RELATIONSHIP, not a coordinate. Steps get moved,
       restacked and re-heighted between load-ins, and a target that remembers
       where the step used to be is worse than no target at all. */
    MonitorProperties mp;

    StagePlatform *deck = mp.addPlatform();
    deck->setOriginX(1.0f); deck->setOriginY(1.0f);
    deck->setWidth(2.0f); deck->setDepth(2.0f); deck->setHeight(0.4f);

    StageTarget *t = mp.addStageTarget();
    t->setPosition(QVector3D(99.0f, 99.0f, 99.0f));     // deliberately nonsense
    t->setKind(StageTarget::Kind::Structure);
    t->setBinding(QStringLiteral("platform"), deck->id());
    QVERIFY(t->isBound());

    QVector3D p = mp.targetAimPoint(t);
    QVERIFY2(qAbs(double(p.x()) - 2.0) < 0.01 && qAbs(double(p.y()) - 2.0) < 0.01,
             "a bound target should sit on the structure, not at its own stored "
             "position");
    QVERIFY(qAbs(double(p.z()) - 0.4) < 0.01);          // the deck's top

    // Move the step: the target goes with it, with no re-binding.
    deck->setOriginX(6.0f);
    deck->setHeight(1.2f);
    p = mp.targetAimPoint(t);
    QVERIFY2(qAbs(double(p.x()) - 7.0) < 0.01,
             "the target did not follow the step when it moved");
    QVERIFY2(qAbs(double(p.z()) - 1.2) < 0.01,
             "the target did not follow the step when its height changed");

    // A binding to something that no longer exists falls back to its own position.
    t->setBinding(QStringLiteral("platform"), 4242);
    p = mp.targetAimPoint(t);
    QVERIFY2(qAbs(double(p.x()) - 99.0) < 0.01,
             "a dangling binding should fall back to the stored position, not "
             "collapse to the origin");
}

void MonitorProperties_Test::targetXmlRoundTrip()
{
    /* Kinds are persisted by NAME. An ordinal can never be reordered or removed
       without silently re-meaning every saved file -- and an unknown name from
       a newer build has to degrade to None rather than becoming whatever now
       sits at that index. */
    QByteArray buf;
    quint32 savedId = 0;
    {
        MonitorProperties mp;
        StageTarget *t = mp.addStageTarget();
        savedId = t->id();
        t->setName("Lead Vocal");
        t->setPosition(QVector3D(1.5f, 2.5f, 0.0f));
        t->setKind(StageTarget::Kind::DrumKit);
        t->setAimHeightOverride(0.85f);
        t->setBinding(QStringLiteral("truss"), 7);
        t->setFootprint(QSizeF(1.6, 1.2));

        QXmlStreamWriter w(&buf);
        w.writeStartDocument();
        t->saveXML(&w);
        w.writeEndDocument();
    }

    MonitorProperties mp2;
    StageTarget *t2 = mp2.addStageTarget();
    QXmlStreamReader r(buf);
    QVERIFY(r.readNextStartElement());
    QVERIFY(t2->loadXML(r));

    QCOMPARE(t2->id(), savedId);
    QCOMPARE(t2->name(), QString("Lead Vocal"));
    QCOMPARE(int(t2->kind()), int(StageTarget::Kind::DrumKit));
    QVERIFY(qAbs(double(t2->aimHeightOverride()) - 0.85) < 0.001);
    QCOMPARE(t2->boundType(), QString("truss"));
    QCOMPARE(t2->boundId(), quint32(7));
    QVERIFY(qAbs(t2->footprint().width() - 1.6) < 0.001);
    QVERIFY(qAbs(t2->footprint().height() - 1.2) < 0.001);

    /* A kind this build has never heard of must not become one it has. */
    QCOMPARE(int(StageTarget::stringToKind("holographic-unicorn")),
             int(StageTarget::Kind::None));
    QCOMPARE(int(StageTarget::stringToKind("PERSON")),
             int(StageTarget::Kind::Person));

    /* And a plain target writes none of this, so an older build sees exactly
       the target it always did. */
    QByteArray plainBuf;
    {
        MonitorProperties mp3;
        StageTarget *p = mp3.addStageTarget();
        p->setPosition(QVector3D(1, 2, 0));
        QXmlStreamWriter w(&plainBuf);
        p->saveXML(&w);
    }
    QVERIFY2(plainBuf.contains("Kind=") == false,
             "a target with no kind is writing Kind= into every show file");
    QVERIFY2(plainBuf.contains("BoundType=") == false,
             "an unbound target is writing a binding into every show file");
}

void MonitorProperties_Test::stepsAreAFlightOfTreads()
{
    // The default: two steps, 8" rise and run, 3' wide, climbing upstage.
    MonitorProperties mp;
    StagePlatform *st = mp.addPlatform();
    st->setOriginX(1.0f);
    st->setOriginY(2.0f);
    st->setSteps(2, 0.2032f, 0.2032f, 0.9144f);
    QVERIFY(st->isSteps());
    QCOMPARE(st->width(), 0.9144f);                 // across stage
    QCOMPARE(st->depth(), 0.4064f);                 // 16"
    QCOMPARE(st->height(), 0.4064f);                // 16"
    QCOMPARE(st->stepWidth(), 0.9144f);

    // +Y is downstage, so the bottom step is the high-Y strip.
    const float x = 1.4f;
    QCOMPARE(st->topAt(x, 2.0f + 0.35f), 0.2032f);  // bottom tread
    QCOMPARE(st->topAt(x, 2.0f + 0.05f), 0.4064f);  // top tread
    QCOMPARE(st->topAt(x, 9.0f), 0.2032f);          // off the downstage edge: the bottom one
    QCOMPARE(mp.platformHeightAt(x, 2.35f), 0.2032f);
    QCOMPARE(mp.platformHeightAt(x, 2.05f), 0.4064f);

    const QList<StagePlatform::Block> b = st->blocks();
    QCOMPARE(b.size(), 2);
    QCOMPARE(b[0].height, 0.2032f);
    QCOMPARE(b[0].y1, 2.4064f);                     // the downstage strip
    QCOMPARE(b[1].height, 0.4064f);
    QCOMPARE(b[1].y0, 2.0f);

    // A plain platform is one block at its own height, everywhere.
    StagePlatform *box = mp.addPlatform();
    box->setHeight(0.5f);
    QCOMPARE(box->blocks().size(), 1);
    QCOMPARE(box->topAt(0.3f, 0.3f), 0.5f);
}

void MonitorProperties_Test::stepsClimbEveryWay()
{
    MonitorProperties mp;
    StagePlatform *st = mp.addPlatform();
    st->setOriginX(0.0f);
    st->setOriginY(0.0f);

    st->setSteps(3, 0.2f, 0.3f, 1.0f, StagePlatform::ClimbDownstage);
    QCOMPARE(st->depth(), 0.9f);
    QCOMPARE(st->topAt(0.5f, 0.1f), 0.2f);         // upstage edge is the bottom
    QCOMPARE(st->topAt(0.5f, 0.8f), 0.6f);

    st->setSteps(3, 0.2f, 0.3f, 1.0f, StagePlatform::ClimbStageLeft);   // toward +X
    QCOMPARE(st->width(), 0.9f);
    QCOMPARE(st->depth(), 1.0f);
    QCOMPARE(st->stepWidth(), 1.0f);
    QCOMPARE(st->topAt(0.1f, 0.5f), 0.2f);
    QCOMPARE(st->topAt(0.8f, 0.5f), 0.6f);

    st->setSteps(3, 0.2f, 0.3f, 1.0f, StagePlatform::ClimbStageRight);
    QCOMPARE(st->topAt(0.8f, 0.5f), 0.2f);
    QCOMPARE(st->topAt(0.1f, 0.5f), 0.6f);
}

void MonitorProperties_Test::stepsRoundTripInXml()
{
    StagePlatform st(4);
    st.setName("House steps");
    st.setSteps(4, 0.18f, 0.28f, 1.2f, StagePlatform::ClimbStageRight);
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    st.saveXML(&w);
    buf.close();
    QVERIFY(buf.data().contains("Steps=\"4\""));

    QXmlStreamReader r(buf.data());
    r.readNextStartElement();
    StagePlatform back(0);
    QVERIFY(back.loadXML(r));
    QVERIFY(back.isSteps());
    QCOMPARE(back.stepCount(), 4);
    QCOMPARE(back.stepRise(), 0.18f);
    QCOMPARE(back.stepRun(), 0.28f);
    QCOMPARE(back.stepsClimb(), StagePlatform::ClimbStageRight);
    QCOMPARE(back.width(), st.width());
    QCOMPARE(back.depth(), 1.2f);
    QCOMPARE(back.height(), st.height());

    // A plain platform writes nothing new.
    StagePlatform box(5);
    QBuffer b2;
    b2.open(QIODevice::WriteOnly);
    QXmlStreamWriter w2(&b2);
    box.saveXML(&w2);
    QVERIFY(!b2.data().contains("Step"));
}

void MonitorProperties_Test::aRoundedFrontCutsTheCorners()
{
    // A 10 m stage, 6 m deep at the middle, its front 1 m further out there.
    StagePlatform st(1);
    st.setOriginX(0.0f);
    st.setOriginY(0.0f);
    st.setWidth(10.0f);
    st.setDepth(6.0f);
    QVERIFY(!st.hasRoundedFront());
    QCOMPARE(st.frontEdgeAt(0.0f), 6.0f);
    st.setFrontCurve(1.0f);
    QVERIFY(st.hasRoundedFront());
    QVERIFY(qAbs(st.frontEdgeAt(5.0f) - 6.0f) < 1e-4f);    // the middle: the full depth
    QVERIFY(qAbs(st.frontEdgeAt(0.0f) - 5.0f) < 1e-4f);    // the corners, cut back
    QVERIFY(qAbs(st.frontEdgeAt(10.0f) - 5.0f) < 1e-4f);
    QVERIFY(st.frontEdgeAt(2.5f) > 5.0f && st.frontEdgeAt(2.5f) < 6.0f);
    QVERIFY(st.containsPoint(5.0f, 5.9f));
    QVERIFY(!st.containsPoint(0.2f, 5.9f));                 // off the cut corner
    QVERIFY(st.containsPoint(0.2f, 4.9f));
    // The outline runs corner to corner round the front.
    const QList<QPointF> fp = st.footprint(10);
    QCOMPARE(fp.size(), 2 + 11);
    QVERIFY(qAbs(fp.at(7).y() - 6.0) < 1e-3);               // the middle of the arc
    // No more than a half circle; steps are square.
    st.setFrontCurve(9.0f);
    QCOMPARE(st.frontCurve(), 5.0f);
    QVERIFY(qAbs(st.frontEdgeAt(0.0f) - 1.0f) < 1e-3f);
    st.setSteps(3, 0.2f, 0.3f, 2.0f);
    QVERIFY(!st.hasRoundedFront());

    // Saved, and only when it has one.
    StagePlatform box(2);
    box.setFrontCurve(0.75f);
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    box.saveXML(&w);
    buf.close();
    QXmlStreamReader r(buf.data());
    r.readNextStartElement();
    StagePlatform back(0);
    QVERIFY(back.loadXML(r));
    QCOMPARE(back.frontCurve(), 0.75f);
    StagePlatform square(3);
    QBuffer b2;
    b2.open(QIODevice::WriteOnly);
    QXmlStreamWriter w2(&b2);
    square.saveXML(&w2);
    QVERIFY(!b2.data().contains("FrontCurve"));
}

void MonitorProperties_Test::aPlatformTurnsAboutItsMiddle()
{
    // 4 m wide, 2 m deep, its middle at (5, 3); turned a quarter.
    StagePlatform st(1);
    st.setOriginX(3.0f);
    st.setOriginY(2.0f);
    st.setWidth(4.0f);
    st.setDepth(2.0f);
    QVERIFY(st.containsPoint(6.5f, 3.0f));
    QVERIFY(!st.containsPoint(5.0f, 4.5f));
    st.setRotation(90.0f);
    QVERIFY(st.isRotated());
    QCOMPARE(st.centre(), QPointF(5.0, 3.0));
    // Now 2 m across and 4 m up and down stage.
    QVERIFY(!st.containsPoint(6.5f, 3.0f));
    QVERIFY(st.containsPoint(5.0f, 4.5f));
    const QList<QPointF> fp = st.footprint();
    QCOMPARE(fp.size(), 4);
    // Its upstage-left corner (3, 2) goes round to (6, 1).
    QVERIFY(qAbs(fp.at(0).x() - 6.0) < 1e-4 && qAbs(fp.at(0).y() - 1.0) < 1e-4);
    // Local and world go back and forth.
    const QPointF w(6.2, 4.1);
    QVERIFY(QLineF(st.toWorld(st.toLocal(w)), w).length() < 1e-9);
    // Kept between -180 and 180; saved, and only when turned.
    st.setRotation(270.0f);
    QCOMPARE(st.rotation(), -90.0f);
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter wr(&buf);
    st.saveXML(&wr);
    buf.close();
    QXmlStreamReader r(buf.data());
    r.readNextStartElement();
    StagePlatform back(0);
    QVERIFY(back.loadXML(r));
    QCOMPARE(back.rotation(), -90.0f);

    // Steps turned: the tread under a point is found in the flight's own axes.
    StagePlatform steps(2);
    steps.setOriginX(0.0f);
    steps.setOriginY(0.0f);
    steps.setSteps(2, 0.2f, 0.5f, 1.0f, StagePlatform::ClimbUpstage);   // 1 m x 1 m
    const float lowAt = steps.topAt(0.5f, 0.9f);                      // downstage: bottom step
    QCOMPARE(lowAt, 0.2f);
    steps.setRotation(180.0f);                                         // now it climbs downstage
    QCOMPARE(steps.topAt(0.5f, 0.9f), 0.4f);
}

void MonitorProperties_Test::whatIsOnAPlatformOrTowerTurnsWithIt()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *pl = props->addPlatform();
    pl->setOriginX(0.0f);
    pl->setOriginY(0.0f);
    pl->setWidth(2.0f);
    pl->setDepth(2.0f);
    pl->setHeight(0.5f);

    // On its front face, at its middle; and standing on its deck, off-centre.
    Fixture *face = new Fixture(&doc);
    face->setChannels(1);
    QVERIFY(doc.addFixture(face));
    FixtureRigProps rf;
    rf.riserPlatformId = pl->id();
    rf.riserFace = FixtureRigProps::RiserFront;
    rf.riserU = 1.0f;
    rf.riserV = 0.25f;
    props->setFixtureRigProps(face->id(), rf);
    QVERIFY(qAbs(props->fixtureRigPosition(face->id()).y() - 2.0f) < 1e-4f);

    Fixture *deck = new Fixture(&doc);
    deck->setChannels(1);
    deck->setAddress(10);
    QVERIFY(doc.addFixture(deck));
    props->setFixturePosition(deck->id(), 0, 0, QVector3D(1500.0f, 1000.0f, 0.0f));
    FixtureRigProps rd;
    rd.deckPlatformId = pl->id();
    props->setFixtureRigProps(deck->id(), rd);

    props->turnPlatform(pl->id(), 90.0f);
    // The front face now looks stage right: (1, 2) went round the middle (1, 1) to (0, 1).
    const QVector3D f = props->fixtureRigPosition(face->id());
    QVERIFY2(qAbs(f.x() - 0.0f) < 1e-4f && qAbs(f.y() - 1.0f) < 1e-4f,
             qPrintable(QString("%1, %2").arg(f.x()).arg(f.y())));
    QCOMPARE(props->fixtureRigProps(face->id()).studioYaw, 90.0f);
    QCOMPARE(props->fixtureRigProps(face->id()).panZeroDir, 90.0f);
    // The deck one: (1.5, 1) round to (1, 1.5), still on top.
    const QVector3D d = props->fixtureRigPosition(deck->id());
    QVERIFY2(qAbs(d.x() - 1.0f) < 1e-3f && qAbs(d.y() - 1.5f) < 1e-3f && qAbs(d.z() - 0.5f) < 1e-3f,
             qPrintable(QString("%1, %2, %3").arg(d.x()).arg(d.y()).arg(d.z())));

    // Its facing: a front-face fixture's depth looks upstage (+Y); turned a
    // quarter, toward stage right (-X).
    QVector3D L, N, H;
    RigHeads::frame(props->fixtureRigProps(face->id()), L, N, H);
    QVERIFY2(qAbs(N.x() + 1.0f) < 1e-4f && qAbs(N.y()) < 1e-4f,
             qPrintable(QString("%1, %2, %3").arg(N.x()).arg(N.y()).arg(N.z())));
    QVERIFY(qAbs(RigHeads::axis(props->fixtureRigProps(face->id())).y() - 1.0f) < 1e-4f);

    // A tower: its shelf fixture goes round, and a hanger on its side with it.
    Tower *tw = props->addTower();
    tw->setOriginX(10.0f);
    tw->setOriginY(10.0f);
    tw->setWidth(0.4f);
    tw->setDepth(0.4f);
    tw->addShelf(1.0f);
    Fixture *shelf = new Fixture(&doc);
    shelf->setChannels(1);
    shelf->setAddress(20);
    QVERIFY(doc.addFixture(shelf));
    FixtureRigProps rs;
    rs.towerId = tw->id();
    rs.towerShelf = 0;
    rs.towerU = 0.4f;           // its stage-left edge, middle depth
    rs.towerV = 0.2f;
    props->setFixtureRigProps(shelf->id(), rs);
    Hanger *h = props->addHanger(Hanger::OnTower, tw->id());
    h->setAngle(90.0f);         // out of its stage-left side
    props->turnTower(tw->id(), 90.0f);
    const QVector3D sp = props->fixtureRigPosition(shelf->id());
    QVERIFY2(qAbs(sp.x() - 10.2f) < 1e-4f && qAbs(sp.y() - 10.4f) < 1e-4f,
             qPrintable(QString("%1, %2").arg(sp.x()).arg(sp.y())));
    QCOMPARE(props->fixtureRigProps(shelf->id()).studioYaw, 90.0f);
    QCOMPARE(h->angle(), 0.0f);                       // now out of its downstage side

    // Saved with the fixture.
    QByteArray ws;
    QXmlStreamWriter ww(&ws);
    ww.writeStartDocument();
    QVERIFY(doc.saveXML(&ww));
    ww.writeEndDocument();
    Doc other(this);
    QXmlStreamReader rr(ws);
    while (rr.readNextStartElement() && rr.name() != QStringLiteral("Engine"))
        ;
    QVERIFY(other.loadXML(rr));
    QCOMPARE(other.monitorProperties()->fixtureRigProps(face->id()).studioYaw, 90.0f);
    QCOMPARE(other.monitorProperties()->platform(pl->id())->rotation(), 90.0f);
    QCOMPARE(other.monitorProperties()->tower(tw->id())->rotation(), 90.0f);
}

void MonitorProperties_Test::aFixtureOnStepsStandsOnItsTread()
{
    MonitorProperties mp;
    StagePlatform *st = mp.addPlatform();
    st->setSteps(2, 0.2032f, 0.2032f, 0.9144f);     // y 0..0.4064, bottom tread downstage

    // Standing on the bottom tread (y = 0.3 m, in mm on the plot).
    mp.setFixturePosition(7, 0, 0, QVector3D(400.0f, 300.0f, 0.0f));
    FixtureRigProps rp;
    rp.deckPlatformId = st->id();
    mp.setFixtureRigProps(7, rp);
    QCOMPARE(mp.fixtureRigPosition(7).z(), 0.2032f);

    // On the top one.
    mp.setFixturePosition(7, 0, 0, QVector3D(400.0f, 100.0f, 0.0f));
    QCOMPARE(mp.fixtureRigPosition(7).z(), 0.4064f);
}

void MonitorProperties_Test::aFixtureFacesEachOfSixWays()
{
    MonitorProperties mp;
    const FixtureRigProps::Face all[6] = {
        FixtureRigProps::FaceTop, FixtureRigProps::FaceBottom, FixtureRigProps::FaceFront,
        FixtureRigProps::FaceBack, FixtureRigProps::FaceStageLeft, FixtureRigProps::FaceStageRight };
    const int plane[6] = { 0, 0, 1, 1, 2, 2 };
    for (int i = 0; i < 6; ++i)
    {
        FixtureRigProps rp;
        mp.setFixtureFace(rp, all[i]);
        QCOMPARE(rp.studioMount, plane[i]);
        QCOMPARE(rp.studioFlip, (i % 2) == 1);
        QCOMPARE(rp.face(), all[i]);
        QVERIFY(rp.studioFaceChosen);
    }
    // The default is Front, facing downstage, and not a choice.
    FixtureRigProps def;
    QCOMPARE(def.face(), FixtureRigProps::FaceFront);
    QVERIFY(!def.studioFaceChosen);
}

void MonitorProperties_Test::aFaceOnATrussIsThatSideOfIt()
{
    MonitorProperties mp;
    Truss *tower = mp.addTruss();
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(2.0f, 3.0f, 0.0f));
    tower->setLength(3.0f);
    tower->setWidth(0.3f);
    mp.setFixturePosition(1, 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp;
    rp.trussId = tower->id();
    rp.trussOffset = 1.5f;

    // On the back of a tower: its upstage side (+Y is downstage).
    mp.setFixtureFace(rp, FixtureRigProps::FaceBack);
    QCOMPARE(rp.trussCrossY, -0.15f);
    mp.setFixtureRigProps(1, rp);
    QVERIFY(mp.fixtureRigPosition(1).y() < 3.0f);
    mp.setFixtureFace(rp, FixtureRigProps::FaceStageRight);
    QCOMPARE(rp.trussCross, -0.15f);                // stage right is -X
    QCOMPARE(rp.trussCrossY, -0.15f);               // the other axis is left alone

    // A horizontal run across stage: Front/Back are its downstage/upstage
    // chords, Top/Bottom on and under it; it has no stage-left chord.
    Truss *pipe = mp.addTruss();
    pipe->setType(Truss::Horizontal);
    pipe->setOrigin(QVector3D(0.0f, 5.0f, 4.0f));
    pipe->setDirection(QPointF(1.0, 0.0));
    pipe->setLength(6.0f);
    pipe->setWidth(0.3f);
    FixtureRigProps h;
    h.trussId = pipe->id();
    mp.setFixtureFace(h, FixtureRigProps::FaceFront);
    QCOMPARE(h.trussCross, 0.15f);
    QCOMPARE(h.trussMountSide, int(FixtureRigProps::Centered));
    mp.setFixtureFace(h, FixtureRigProps::FaceBack);
    QCOMPARE(h.trussCross, -0.15f);
    mp.setFixtureFace(h, FixtureRigProps::FaceTop);
    QCOMPARE(h.trussMountSide, int(FixtureRigProps::TopMounted));
    QCOMPARE(h.trussCross, 0.0f);
    mp.setFixtureFace(h, FixtureRigProps::FaceStageLeft);
    QCOMPARE(h.trussCross, 0.0f);                   // unchanged: no such chord
    QCOMPARE(h.face(), FixtureRigProps::FaceStageLeft);

    // Inside a truss it only turns.
    FixtureRigProps in;
    in.trussId = tower->id();
    in.placement = FixtureRigProps::Inside;
    mp.setFixtureFace(in, FixtureRigProps::FaceBack);
    QCOMPARE(in.trussCrossY, 0.0f);
    QCOMPARE(in.face(), FixtureRigProps::FaceBack);
}

void MonitorProperties_Test::aChosenFaceSurvivesSaveAndRepair()
{
    MonitorProperties mp;
    Truss *pipe = mp.addTruss();
    pipe->setType(Truss::Horizontal);
    pipe->setDirection(QPointF(1.0, 0.0));
    pipe->setLength(6.0f);
    mp.setFixturePosition(1, 0, 0, QVector3D(0, 0, 0));
    mp.setFixturePosition(2, 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps back;
    back.trussId = pipe->id();
    mp.setFixtureFace(back, FixtureRigProps::FaceBack);
    mp.setFixtureRigProps(1, back);
    FixtureRigProps front;
    front.trussId = pipe->id();
    mp.setFixtureFace(front, FixtureRigProps::FaceFront);
    mp.setFixtureRigProps(2, front);

    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();
    QVERIFY(buf.contains("SF=\"1\""));

    MonitorProperties mp2;
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
        if (reader.name() == QStringLiteral("Monitor"))
            QVERIFY(mp2.loadXML(reader, nullptr));
    // The load-time repair moves a STRAY default Front on a horizontal truss
    // to Top. A chosen Front or Back stays.
    mp2.repairMountPlanes();
    QCOMPARE(mp2.fixtureRigProps(1).face(), FixtureRigProps::FaceBack);
    QCOMPARE(mp2.fixtureRigProps(2).face(), FixtureRigProps::FaceFront);

    FixtureRigProps stray;
    stray.trussId = pipe->id();
    mp2.repairMountPlane(stray);
    QCOMPARE(stray.studioMount, 0);
}

/* An XL-450 as its 165-channel mode is defined: 45 RGB heads, then 30 white
   ones, on a declared 15 x 5 grid. */
static QLCFixtureDef *xl450(QLCFixtureMode **modeOut)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Warmdance");
    def->setModel("XL-450");
    def->setType(QLCFixtureDef::LEDBarPixels);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("165ch");
    auto add = [&](const QString &name, QLCChannel::Preset preset) {
        QLCChannel *ch = new QLCChannel();
        ch->setName(name);
        ch->setPreset(preset);
        def->addChannel(ch);
        mode->insertChannel(ch, mode->channels().size());
        return quint32(mode->channels().size() - 1);
    };
    for (int i = 0; i < 45; ++i)
    {
        QLCFixtureHead h;
        h.addChannel(add(QString("Red %1").arg(i + 1), QLCChannel::IntensityRed));
        h.addChannel(add(QString("Green %1").arg(i + 1), QLCChannel::IntensityGreen));
        h.addChannel(add(QString("Blue %1").arg(i + 1), QLCChannel::IntensityBlue));
        mode->insertHead(-1, h);
    }
    for (int i = 0; i < 30; ++i)
    {
        QLCFixtureHead h;
        h.addChannel(add(QString("White %1").arg(i + 1), QLCChannel::IntensityWhite));
        mode->insertHead(-1, h);
    }
    QLCPhysical ph;
    ph.setLayoutSize(QSize(15, 5));
    ph.setWidth(401); ph.setHeight(180); ph.setDepth(100);
    mode->setPhysical(ph);
    def->addMode(mode);
    *modeOut = mode;
    return def;
}

void MonitorProperties_Test::headKindsAreReadFromTheChannels()
{
    QLCFixtureMode *mode = nullptr;
    QLCFixtureDef *def = xl450(&mode);
    Fixture fx(this);
    fx.setFixtureDefinition(def, mode);
    QCOMPARE(HeadKinds::kindOf(mode, 0), QString("RGB"));
    QCOMPARE(HeadKinds::kindOf(mode, 44), QString("RGB"));
    QCOMPARE(HeadKinds::kindOf(mode, 45), QString("W"));
    QCOMPARE(HeadKinds::kindsOf(&fx), QStringList({ "RGB", "W" }));
    QVERIFY(HeadKinds::isMixed(&fx));
    QCOMPARE(HeadLayout::keyFor(&fx), QString("Warmdance|XL-450|165ch"));
    fx.setFixtureDefinition(nullptr, nullptr);
    delete def;
}

void MonitorProperties_Test::aRowPatternMapsEachKindToItsRows()
{
    QLCFixtureMode *mode = nullptr;
    QLCFixtureDef *def = xl450(&mode);
    Fixture fx(this);
    fx.setFixtureDefinition(def, mode);

    // What everything assumed: head order, row by row -- RGB in rows 0-2.
    const HeadLayout order = HeadLayout::headOrder(&fx);
    QCOMPARE(order.size, QSize(15, 5));
    QCOMPARE(order.cellOf(45), QPoint(0, 3));
    QCOMPARE(order.rowKinds(&fx), QStringList({ "RGB", "RGB", "RGB", "W", "W" }));

    // As built.
    const HeadLayout built = HeadLayout::fromRowPattern(&fx, { "RGB", "W", "RGB", "W", "RGB" });
    QVERIFY(built.isValid());
    QCOMPARE(built.cellOf(0), QPoint(0, 0));        // RGB 1
    QCOMPARE(built.cellOf(14), QPoint(14, 0));
    QCOMPARE(built.cellOf(15), QPoint(0, 2));       // RGB 16 starts the 2nd RGB row
    QCOMPARE(built.cellOf(30), QPoint(0, 4));
    QCOMPARE(built.cellOf(45), QPoint(0, 1));       // W 1
    QCOMPARE(built.cellOf(60), QPoint(0, 3));       // W 16
    QCOMPARE(built.rowKinds(&fx), QStringList({ "RGB", "W", "RGB", "W", "RGB" }));

    // Rows that cannot hold the heads there are: refused, not guessed.
    QVERIFY(!HeadLayout::fromRowPattern(&fx, { "RGB", "RGB", "RGB", "RGB", "W" }).isValid());
    fx.setFixtureDefinition(nullptr, nullptr);
    delete def;
}

void MonitorProperties_Test::anLedMapLivesInTheLibraryAndTheShow()
{
    QTemporaryDir lib;
    HeadLayoutLibrary::setUserDirectoryOverride(lib.path());
    QLCFixtureMode *mode = nullptr;
    QLCFixtureDef *def = xl450(&mode);
    Fixture fx(this);
    fx.setFixtureDefinition(def, mode);
    const HeadLayout built = HeadLayout::fromRowPattern(&fx, { "RGB", "W", "RGB", "W", "RGB" });

    MonitorProperties mp;
    QVERIFY(!mp.hasHeadLayout(&fx));
    QCOMPARE(mp.headLayoutFor(&fx), HeadLayout::headOrder(&fx));   // nothing known: head order

    // In the library: every show sees it.
    QVERIFY(HeadLayoutLibrary::save(built));
    QCOMPARE(HeadLayoutLibrary::load(built.key), built);
    QVERIFY(mp.hasHeadLayout(&fx));
    QCOMPARE(mp.headLayoutFor(&fx), built);

    // A show's own copy wins, and travels with the file.
    HeadLayout mine = built;
    mine.cells[0] = QPoint(1, 0);
    mine.cells[1] = QPoint(0, 0);
    mp.setHeadLayout(mine);
    QCOMPARE(mp.headLayoutFor(&fx), mine);
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
    QCOMPARE(mp2.headLayoutFor(&fx), mine);

    HeadLayoutLibrary::setUserDirectoryOverride(QString());
    fx.setFixtureDefinition(nullptr, nullptr);
    delete def;
}

void MonitorProperties_Test::anLedMapReadWhileItIsSetIsWhole()
{
    QTemporaryDir lib;
    HeadLayoutLibrary::setUserDirectoryOverride(lib.path());   // no library copy
    QLCFixtureMode *mode = nullptr;
    QLCFixtureDef *def = xl450(&mode);
    Fixture fx(this);
    fx.setFixtureDefinition(def, mode);

    const HeadLayout rows = HeadLayout::fromRowPattern(&fx, { "RGB", "W", "RGB", "W", "RGB" });
    QVERIFY(rows.isValid());
    HeadLayout swapped = rows;
    std::swap(swapped.cells[0], swapped.cells[1]);
    const HeadLayout order = HeadLayout::headOrder(&fx);

    MonitorProperties mp;
    std::atomic<bool> stop(false);
    std::atomic<int> reads(0), torn(0);
    std::thread dmx([&]() {                  // the matrix effect's tick
        while (!stop.load())
        {
            const HeadLayout got = mp.headLayoutFor(&fx);
            if (!(got == rows || got == swapped || got == order))
                torn.fetch_add(1);
            reads.fetch_add(1);
        }
    });
    QElapsedTimer clock;
    clock.start();
    int sets = 0;
    // The editor, and File > New, for as long as the reader is reading.
    while ((reads.load() < 20000 || sets < 3000) && clock.elapsed() < 10000)
    {
        mp.setHeadLayout(sets % 2 ? rows : swapped);
        if (sets % 7 == 0)
            mp.reset();
        ++sets;
    }
    stop.store(true);
    dmx.join();
    QVERIFY2(reads.load() >= 20000, qPrintable(QString::number(reads.load())));
    QCOMPARE(torn.load(), 0);

    HeadLayoutLibrary::setUserDirectoryOverride(QString());
    fx.setFixtureDefinition(nullptr, nullptr);
    delete def;
}

void MonitorProperties_Test::aSnapshotPutsTheRigBack()
{
    MonitorProperties mp;
    mp.setGridUnits(MonitorProperties::Feet);
    Truss *pipe = mp.addTruss();
    pipe->setLength(6.0f);
    pipe->setOrigin(QVector3D(1.0f, 2.0f, 4.0f));
    StageTarget *target = mp.addStageTarget();
    target->setName("DSC");
    const quint32 targetId = target->id();
    mp.setFixturePosition(1, 0, 0, QVector3D(1000, 2000, 3));
    FixtureRigProps rp;
    rp.trussId = pipe->id();
    mp.setFixtureRigProps(1, rp);

    const QByteArray before = mp.snapshot(nullptr);
    QVERIFY(!before.isEmpty());

    // A run of edits: the pipe moved, the target deleted, a tower added,
    // the fixture moved and taken off the pipe.
    pipe->setOrigin(QVector3D(5.0f, 5.0f, 5.0f));
    mp.removeStageTarget(targetId);
    mp.addTower();
    mp.setFixturePosition(1, 0, 0, QVector3D(0, 0, 0));
    mp.setFixtureRigProps(1, FixtureRigProps());
    QVERIFY(mp.snapshot(nullptr) != before);

    QSignalSpy targets(&mp, &MonitorProperties::stageTargetsChanged);
    QSignalSpy rig(&mp, &MonitorProperties::rigPropsChanged);
    QVERIFY(mp.restoreSnapshot(before, nullptr));
    // As it was -- as loading a saved show has it (a load writes a
    // default mount out in full), and a second restore changes nothing.
    const QByteArray restored = mp.snapshot(nullptr);
    QVERIFY(mp.restoreSnapshot(restored, nullptr));
    QCOMPARE(mp.snapshot(nullptr), restored);
    QCOMPARE(mp.truss(rp.trussId)->origin(), QVector3D(1.0f, 2.0f, 4.0f));
    QCOMPARE(mp.fixtureItem(1, 0, 0).m_position, QVector3D(1000, 2000, 3));
    QCOMPARE(mp.towers().count(), 0);
    QVERIFY(mp.stageTarget(targetId) != nullptr);
    QCOMPARE(mp.stageTarget(targetId)->name(), QString("DSC"));
    QCOMPARE(mp.fixtureRigProps(1).trussId, rp.trussId);
    QCOMPARE(mp.gridUnits(), MonitorProperties::Feet);
    QVERIFY(targets.count() >= 1);
    QVERIFY(rig.count() >= 1);

    // The pipe object the edits held is retired, not freed: still readable.
    QCOMPARE(pipe->length(), 6.0f);
    QVERIFY(mp.truss(rp.trussId) != pipe);               // a fresh one stands in

    // What the snapshot has just as it is stays the same object -- an
    // editor still open around a cancelled one keeps editing the real thing.
    Truss *kept = mp.truss(rp.trussId);
    StageTarget *changed = mp.stageTarget(targetId);
    const QByteArray now = mp.snapshot(nullptr);
    changed->setName("Moved");
    QVERIFY(mp.restoreSnapshot(now, nullptr));
    QCOMPARE(mp.truss(rp.trussId), kept);
    QVERIFY(mp.stageTarget(targetId) != changed);
    QCOMPARE(mp.stageTarget(targetId)->name(), QString("DSC"));
}

void MonitorProperties_Test::aPersonIsAsTallAsTheirHeight()
{
    MonitorProperties mp;
    mp.setAimSubjectHeight(1.524f);                     // 5'0"
    StagePlatform *riser = mp.addPlatform();
    riser->setOriginX(0.0f); riser->setOriginY(0.0f);
    riser->setWidth(2.438f); riser->setDepth(1.219f); riser->setHeight(0.610f);   // 2'
    StageTarget *t = mp.addStageTarget();
    t->setKind(StageTarget::Kind::Person);
    t->setX(1.0f); t->setY(0.5f); t->setZ(0.610f);

    // No height given: a usual one (5'9"), aimed at its chest -- the follow-
    // spot aim height (5'0" here) never sizes a person.
    QCOMPARE(mp.targetFigureHeight(t), 1.75f);
    QVERIFY(qAbs(mp.targetAimHeight(t, true) - 1.4f) < 0.001f);

    // 5'4": drawn 5'4", aimed at the chest, the deck under them.
    t->setPersonHeight(1.626f);
    QCOMPARE(mp.targetFigureHeight(t), 1.626f);
    QVERIFY(qAbs(mp.targetAimHeight(t, true) - 1.626f * 0.8f) < 0.001f);
    const QVector3D aim = mp.targetAimPoint(t, true);
    QVERIFY(qAbs(aim.z() - (0.610f + 1.626f * 0.8f)) < 0.001f);
    // Head 2' + 5'4" = 7'4"; a 10' truss tops out 2'8" above it.
    QVERIFY(qAbs(3.048f - (0.610f + mp.targetFigureHeight(t)) - 0.812f) < 0.01f);

    // An aim override still wins the aim, not the height.
    t->setAimHeightOverride(1.0f);
    QCOMPARE(mp.targetAimHeight(t, true), 1.0f);
    QCOMPARE(mp.targetFigureHeight(t), 1.626f);

    // Saved and read back.
    QByteArray buf;
    QXmlStreamWriter w(&buf);
    t->saveXML(&w);
    QVERIFY(buf.contains("PersonHeight=\"1.626\""));
    StageTarget back(t->id());
    QXmlStreamReader r(buf);
    r.readNextStartElement();
    QVERIFY(back.loadXML(r));
    QCOMPARE(back.personHeight(), 1.626f);
}

static QByteArray autoLooksSaved(const MonitorProperties &mp)
{
    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();
    return buf;
}

void MonitorProperties_Test::autoStageLooksIsSavedWithTheShow()
{
    MonitorProperties mp;
    QVERIFY(!mp.autoStageLooks());
    // Off: nothing written, a show saves as it always has.
    QVERIFY(!autoLooksSaved(mp).contains("AutoStageLooks"));

    QSignalSpy spy(&mp, &MonitorProperties::autoStageLooksChanged);
    mp.setAutoStageLooks(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
    mp.setAutoStageLooks(true);                 // the same again: quiet
    QCOMPARE(spy.count(), 1);

    const QByteArray on = autoLooksSaved(mp);
    QVERIFY(on.contains("AutoStageLooks=\"1\""));
    {
        MonitorProperties back;
        QXmlStreamReader reader(on);
        while (reader.readNextStartElement())
            if (reader.name() == QStringLiteral("Monitor"))
                QVERIFY(back.loadXML(reader, nullptr));
        QVERIFY(back.autoStageLooks());
        QCOMPARE(autoLooksSaved(back), on);
    }

    // A snapshot from before it was turned on turns it off, and says so.
    MonitorProperties other;
    const QByteArray offSnap = other.snapshot(nullptr);
    QVERIFY(mp.restoreSnapshot(offSnap, nullptr));
    QVERIFY(!mp.autoStageLooks());
    QCOMPARE(spy.last().at(0).toBool(), false);
    // ...and one with it on turns it back on.
    QVERIFY(mp.restoreSnapshot(on, nullptr));
    QVERIFY(mp.autoStageLooks());
    QCOMPARE(spy.last().at(0).toBool(), true);

    // A new show starts with it off.
    mp.reset();
    QVERIFY(!mp.autoStageLooks());
}


static QByteArray stageMarksXml(const MonitorProperties &mp)
{
    QByteArray buf;
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    mp.saveXML(&writer, nullptr);
    writer.writeEndDocument();
    return buf;
}

static void loadStageMarksXml(MonitorProperties &mp, const QByteArray &buf)
{
    QXmlStreamReader reader(buf);
    while (reader.readNextStartElement())
    {
        if (reader.name() == QStringLiteral("Monitor"))
            QVERIFY(mp.loadXML(reader, nullptr));
    }
}

void MonitorProperties_Test::stageMarksAreSavedWithTheShow()
{
    MonitorProperties mp;
    QCOMPARE(mp.stageMarks(), int(MonitorProperties::NoStageMarks));
    QVERIFY(mp.curtainLineIsAuto());
    QVERIFY(mp.numberLineIsAuto());

    // Unused, nothing is written: an older reader sees what it always did.
    const QByteArray plain = stageMarksXml(mp);
    QVERIFY(!plain.contains("StageMarks"));
    QVERIFY(!plain.contains("CurtainLineY"));
    QVERIFY(!plain.contains("NumberLineY"));

    mp.setStageMarkVisible(MonitorProperties::CentreMarkFlag, true);
    mp.setStageMarkVisible(MonitorProperties::NumberLineFlag, true);
    mp.setStageMarkVisible(MonitorProperties::CurtainLineFlag, true);
    mp.setStageMarkVisible(MonitorProperties::CurtainLineFlag, false);
    QVERIFY(mp.stageMarkVisible(MonitorProperties::CentreMarkFlag));
    QVERIFY(!mp.stageMarkVisible(MonitorProperties::CentreLineFlag));
    QVERIFY(!mp.stageMarkVisible(MonitorProperties::CurtainLineFlag));
    mp.setCurtainLineY(3.25f);
    QVERIFY(!mp.curtainLineIsAuto());
    QCOMPARE(mp.curtainLineY(), 3.25f);

    MonitorProperties back;
    loadStageMarksXml(back, stageMarksXml(mp));
    QCOMPARE(back.stageMarks(), int(MonitorProperties::CentreMarkFlag | MonitorProperties::NumberLineFlag));
    QVERIFY(!back.curtainLineIsAuto());
    QCOMPARE(back.curtainLineY(), 3.25f);
    QVERIFY(back.numberLineIsAuto());

    // A show from before: off and automatic, whatever was there before it.
    loadStageMarksXml(back, plain);
    back.reset();
    loadStageMarksXml(back, plain);
    QCOMPARE(back.stageMarks(), int(MonitorProperties::NoStageMarks));
    QVERIFY(back.curtainLineIsAuto());

    // Back to automatic is saved as automatic; a stray bit is not kept.
    mp.setCurtainLineAuto();
    mp.setNumberLineY(-1.5f);
    mp.setStageMarks(0xFF);
    QCOMPARE(mp.stageMarks(), 0x1F);
    MonitorProperties again;
    loadStageMarksXml(again, stageMarksXml(mp));
    QVERIFY(again.curtainLineIsAuto());
    QCOMPARE(again.numberLineY(), -1.5f);
    QCOMPARE(again.stageMarks(), 0x1F);
}

void MonitorProperties_Test::stageMarksFollowTheStage()
{
    MonitorProperties mp;
    mp.setGridUnits(MonitorProperties::Meters);
    mp.setGridSize(QVector3D(10, 6, 8));

    // A bare floor: the grid's front edge; the curtain a typical apron behind.
    QCOMPARE(mp.stageCentre().x(), 5.0f);
    QCOMPARE(mp.downstageEdgeY(), 8.0f);
    QCOMPARE(mp.numberLineY(), 8.0f);
    QVERIFY(qAbs(mp.curtainLineY() - 6.8f) < 1e-4f);
    QCOMPARE(mp.numberLineReach(), 5);

    // The deck across centre stage...
    StagePlatform *deck = mp.addPlatform();
    deck->setOriginX(1.0f); deck->setOriginY(1.0f);
    deck->setWidth(8.0f); deck->setDepth(5.0f);
    QCOMPARE(mp.downstageEdgeY(), 6.0f);
    // ...not a riser on it, nor a platform off to one side.
    StagePlatform *riser = mp.addPlatform();
    riser->setOriginX(4.0f); riser->setOriginY(5.5f);
    riser->setWidth(2.0f); riser->setDepth(1.5f);
    riser->setStackable(true);
    StagePlatform *side = mp.addPlatform();
    side->setOriginX(0.0f); side->setOriginY(6.0f);
    side->setWidth(2.0f); side->setDepth(1.8f);
    QCOMPARE(mp.downstageEdgeY(), 6.0f);
    // An apron built as its own piece, butted against the front, rounded.
    StagePlatform *apron = mp.addPlatform();
    apron->setOriginX(2.0f); apron->setOriginY(6.0f);
    apron->setWidth(6.0f); apron->setDepth(1.5f);
    apron->setFrontCurve(0.5f);
    QCOMPARE(mp.downstageEdgeY(), 7.5f);
    QCOMPARE(mp.numberLineY(), 7.5f);
    QVERIFY(qAbs(mp.autoCurtainLineY() - 6.3f) < 1e-4f);
    QVERIFY(mp.curtainLineScenery() == nullptr);

    // The proscenium gives the curtain line, and a main drape before it.
    Scenery *pros = mp.addScenery();
    pros->setKind(Scenery::Proscenium);
    pros->setOriginX(5.0f); pros->setOriginY(5.0f); pros->setWidth(10.0f);
    QCOMPARE(mp.curtainLineScenery(), pros);
    QCOMPARE(mp.curtainLineY(), 5.0f);
    Scenery *offCentre = mp.addScenery();      // a drape not across centre
    offCentre->setKind(Scenery::MainDrape);
    offCentre->setOriginX(1.0f); offCentre->setOriginY(5.5f); offCentre->setWidth(1.0f);
    QCOMPARE(mp.curtainLineScenery(), pros);
    Scenery *main = mp.addScenery();
    main->setKind(Scenery::MainDrape);
    main->setOriginX(5.0f); main->setOriginY(4.8f); main->setWidth(9.0f);
    main->setRotation(180.0f);                  // a sheet turned round is still across
    QCOMPARE(mp.curtainLineScenery(), main);
    QCOMPARE(mp.curtainLineY(), 4.8f);

    // Moved, a line stays put; back to automatic, it follows again.
    mp.setCurtainLineY(4.0f);
    mp.setNumberLineY(7.0f);
    main->setOriginY(4.6f);
    QCOMPARE(mp.curtainLineY(), 4.0f);
    QCOMPARE(mp.numberLineY(), 7.0f);
    mp.setCurtainLineAuto();
    mp.setNumberLineAuto();
    QCOMPARE(mp.curtainLineY(), 4.6f);
    QCOMPARE(mp.numberLineY(), 7.5f);

    // In feet the number line counts feet, and the fallback apron is 4 ft.
    MonitorProperties ft;
    ft.setGridUnits(MonitorProperties::Feet);
    ft.setGridSize(QVector3D(41, 20, 30));
    QCOMPARE(ft.gridUnitMetres(), 0.3048f);
    QCOMPARE(ft.numberLineReach(), 20);
    QVERIFY(qAbs(ft.downstageEdgeY() - 30.0f * 0.3048f) < 1e-4f);
    QVERIFY(qAbs(ft.curtainLineY() - 26.0f * 0.3048f) < 1e-4f);
}

void MonitorProperties_Test::aPositionIsWrittenAgainstTheStageMarks()
{
    typedef MonitorProperties MP;
    MP::StageOffset o;
    o.across = 2.44f;
    o.upstage = 3.06f;
    QCOMPARE(MP::formatStageOffset(o, MP::Meters), QString::fromUtf8("SL 2.4 · US 3.1 m"));
    QCOMPARE(MP::formatStageOffset(o, MP::Meters, MP::SpokenOffset), QStringLiteral("SL 2, US 3"));

    // Stage right and downstage; a hair off centre is centre.
    o.across = -1.0f;
    o.upstage = -0.5f;
    QCOMPARE(MP::formatStageOffset(o, MP::Meters), QString::fromUtf8("SR 1.0 · DS 0.5 m"));
    o.across = 0.04f;
    o.upstage = 0.0f;
    QCOMPARE(MP::formatStageOffset(o, MP::Meters), QString::fromUtf8("CL · on line"));
    QCOMPARE(MP::formatStageOffset(o, MP::Meters, MP::SpokenOffset),
             QStringLiteral("CL, on the number line"));
    o.fromCurtainLine = true;
    QCOMPARE(MP::formatStageOffset(o, MP::Meters, MP::SpokenOffset),
             QStringLiteral("CL, on the curtain line"));
    o.fromCurtainLine = false;

    // Rounding is to what is written: 0.05 m rounds up, spoken to the unit.
    o.across = 0.06f;
    o.upstage = 1.49f;
    QCOMPARE(MP::formatStageOffset(o, MP::Meters), QString::fromUtf8("SL 0.1 · US 1.5 m"));
    QCOMPARE(MP::formatStageOffset(o, MP::Meters, MP::SpokenOffset), QStringLiteral("CL, US 1"));

    // Off the floor: its height, with the unit after the last number.
    o.across = 2.0f;
    o.upstage = 0.0f;
    o.height = 0.6f;
    QCOMPARE(MP::formatStageOffset(o, MP::Meters),
             QString::fromUtf8("SL 2.0 · on line · ↑ 0.6 m"));
    QCOMPARE(MP::formatStageOffset(o, MP::Meters, MP::SpokenOffset), QStringLiteral("SL 2, on the number line, 1 up"));

    // Feet: to the inch, in feet and inches; spoken in whole feet.
    o.across = -7.0f * 0.3048f - 0.0254f * 11.0f;    // 7' 11"
    o.upstage = 10.0f * 0.3048f;
    o.height = 0.0f;
    QCOMPARE(MP::formatStageOffset(o, MP::Feet), QString::fromUtf8("SR 7' 11\" · US 10'"));
    QCOMPARE(MP::formatStageOffset(o, MP::Feet, MP::SpokenOffset), QStringLiteral("SR 8, US 10"));

    // Against the show: from centre and the number line, else the curtain line.
    MonitorProperties mp;
    mp.setGridUnits(MP::Meters);
    mp.setGridSize(QVector3D(10, 6, 8));         // centre x 5, front edge y 8
    const QVector3D spot(7.4f, 4.9f, 0.0f);
    QCOMPARE(mp.stagePositionText(spot), QString::fromUtf8("SL 2.4 · US 3.1 m"));
    mp.setCurtainLineY(6.0f);
    QCOMPARE(mp.stagePositionText(spot), QString::fromUtf8("SL 2.4 · US 3.1 m"));  // marks off: number line
    mp.setStageMarkVisible(MP::CurtainLineFlag, true);
    QVERIFY(mp.stageOffset(spot).fromCurtainLine);
    QCOMPARE(mp.stagePositionText(spot), QString::fromUtf8("SL 2.4 · US 1.1 m"));
    mp.setStageMarkVisible(MP::NumberLineFlag, true);
    QCOMPARE(mp.stagePositionText(spot), QString::fromUtf8("SL 2.4 · US 3.1 m"));
    mp.setGridUnits(MP::Feet);
    mp.setGridSize(QVector3D(10 / 0.3048f, 6, 8 / 0.3048f));
    QCOMPARE(mp.stagePositionText(spot, MP::SpokenOffset), QStringLiteral("SL 8, US 10"));
}

void MonitorProperties_Test::stageLinesAreSetFromTheFrontAndTheCurtainLine()
{
    MonitorProperties mp;
    mp.setGridUnits(MonitorProperties::Meters);
    mp.setGridSize(QVector3D(10, 6, 8));
    StagePlatform *deck = mp.addPlatform();
    deck->setOriginX(1.0f); deck->setOriginY(1.0f);
    deck->setWidth(8.0f); deck->setDepth(5.0f);
    QCOMPARE(mp.downstageEdgeY(), 6.0f);

    // Automatic, they read as distances too: the curtain a typical apron
    // upstage of the front, the number line on the front -- downstage of
    // the curtain line by as much.
    QVERIFY(qAbs(mp.curtainLineUpstage() - 1.2f) < 1e-4f);
    QVERIFY(qAbs(mp.numberLineDownstageOfCurtain() - 1.2f) < 1e-4f);

    // The curtain line upstage of the front; the number line downstage of
    // the curtain line (negative: upstage of it).
    mp.setCurtainLineUpstage(2.5f);
    mp.setNumberLineDownstageOfCurtain(1.5f);
    QVERIFY(!mp.curtainLineIsAuto());
    QVERIFY(!mp.numberLineIsAuto());
    QCOMPARE(mp.curtainLineY(), 3.5f);
    QCOMPARE(mp.numberLineY(), 5.0f);
    QCOMPARE(mp.curtainLineUpstage(), 2.5f);
    QCOMPARE(mp.numberLineDownstageOfCurtain(), 1.5f);

    // Saved as the plan's Y, as before; back as the same distances.
    const QByteArray xml = stageMarksXml(mp);
    QVERIFY(xml.contains("Kind=\"Curtain\" Visible=\"0\" Y=\"3.5\""));
    QVERIFY(xml.contains("Kind=\"Number\" Visible=\"0\" Y=\"5\""));
    MonitorProperties back;
    loadStageMarksXml(back, xml);
    StagePlatform *deck2 = back.addPlatform();
    deck2->setOriginX(1.0f); deck2->setOriginY(1.0f);
    deck2->setWidth(8.0f); deck2->setDepth(5.0f);
    QCOMPARE(back.curtainLineUpstage(), 2.5f);
    QCOMPARE(back.numberLineDownstageOfCurtain(), 1.5f);

    // Each set line stays where it is: the curtain line moved, the number
    // line doesn't follow (its distance changes); an apron added, the
    // curtain line stays and is further from the new front.
    mp.setCurtainLineUpstage(2.0f);
    QCOMPARE(mp.numberLineY(), 5.0f);
    QCOMPARE(mp.numberLineDownstageOfCurtain(), 1.0f);
    StagePlatform *apron = mp.addPlatform();
    apron->setOriginX(2.0f); apron->setOriginY(6.0f);
    apron->setWidth(6.0f); apron->setDepth(1.0f);
    QCOMPARE(mp.downstageEdgeY(), 7.0f);
    QCOMPARE(mp.curtainLineY(), 4.0f);
    QCOMPARE(mp.curtainLineUpstage(), 3.0f);

    // A show saved before this (a plan Y, written the same way) reads as it
    // did: the same lines, shown as their distances.
    const QByteArray old("<?xml version=\"1.0\"?><Monitor DisplayMode=\"0\" ShowLabels=\"1\" "
                         "StageMarks=\"12\" CurtainLineY=\"4.2\" NumberLineY=\"6\"/>");
    MonitorProperties older;
    loadStageMarksXml(older, old);
    older.setGridUnits(MonitorProperties::Meters);
    older.setGridSize(QVector3D(10, 6, 8));      // no deck: the grid's front, 8
    QCOMPARE(older.curtainLineY(), 4.2f);
    QCOMPARE(older.numberLineY(), 6.0f);
    QVERIFY(qAbs(older.curtainLineUpstage() - 3.8f) < 1e-4f);
    QVERIFY(qAbs(older.numberLineDownstageOfCurtain() - 1.8f) < 1e-4f);
    QVERIFY(older.targetPositionsVisible());

    // In feet the distance is still metres in the model; the menu shows feet.
    MonitorProperties ft;
    ft.setGridUnits(MonitorProperties::Feet);
    ft.setGridSize(QVector3D(40, 20, 30));
    ft.setCurtainLineUpstage(4.0f * 0.3048f);
    QVERIFY(qAbs(ft.curtainLineY() - 26.0f * 0.3048f) < 1e-4f);
}

void MonitorProperties_Test::theApronIsMeasuredFromTheCurtainLine()
{
    typedef MonitorProperties MP;
    MonitorProperties mp;
    mp.setGridUnits(MP::Meters);
    mp.setGridSize(QVector3D(10, 6, 8));
    QVERIFY(mp.mainDeck() == nullptr);

    StagePlatform *deck = mp.addPlatform();
    deck->setOriginX(1.0f); deck->setOriginY(1.0f);
    deck->setWidth(8.0f); deck->setDepth(5.0f);
    deck->setHeight(1.0f);
    deck->setColor(QColor(120, 40, 40));
    QCOMPARE(mp.mainDeck(), deck);
    QVERIFY(mp.apronAuto());
    QVERIFY(qAbs(mp.apronDepth() - 1.2f) < 1e-4f);  // the automatic curtain line to the front
    QVERIFY(mp.apronPlatform() == nullptr);         // hidden: no deck of its own

    // Shown: its own deck, from the curtain line to the deck's front, like it.
    const int count = mp.platforms().size();
    mp.setStagePieceVisible(MP::ApronPiece, true);
    StagePlatform *apron = mp.apronPlatform();
    QVERIFY(apron != nullptr);
    QVERIFY(apron != deck);
    QCOMPARE(mp.platforms().size(), count + 1);
    QVERIFY(mp.isApronPlatform(apron->id()));
    QCOMPARE(apron->originX(), 1.0f);
    QVERIFY(qAbs(apron->originY() - 4.8f) < 1e-4f);
    QCOMPARE(apron->width(), 8.0f);
    QCOMPARE(apron->height(), 1.0f);
    QCOMPARE(apron->color(), QColor(120, 40, 40));
    QCOMPARE(apron->layerId(), mp.stageLayerId());
    QVERIFY(mp.hasStageLayer());
    QCOMPARE(mp.platformBaseZ(apron->id()), 0.0f);  // deck, not a riser on it
    QCOMPARE(mp.mainDeck(), deck);                  // never the stage's own deck
    QCOMPARE(mp.downstageEdgeY(), 6.0f);

    // Deeper: its front moves; the curtain line, the deck, the count don't.
    QVERIFY(mp.setApronDepth(2.0f));
    QVERIFY(!mp.apronAuto());
    QVERIFY(mp.curtainLineIsAuto());
    QCOMPARE(mp.platforms().size(), count + 1);
    QVERIFY(qAbs(mp.downstageEdgeY() - 6.8f) < 1e-4f);
    QVERIFY(qAbs(mp.apronDepth() - 2.0f) < 1e-4f);
    QCOMPARE(deck->depth(), 5.0f);
    QCOMPARE(mp.deckFrontY(), 6.0f);
    QVERIFY(qAbs(mp.curtainLineUpstage() - 1.2f) < 1e-4f);  // from the deck's front
    QVERIFY(!mp.setApronDepth(-1.0f));

    // Its front curve, right at its front.
    mp.setApronFrontCurve(0.5f);
    QCOMPARE(apron->frontCurve(), 0.5f);
    QVERIFY(qAbs(apron->frontEdgeAt(5.0f) - 6.8f) < 1e-4f);

    // The curtain line moved: the apron starts there, its depth kept.
    mp.setCurtainLineY(4.0f);
    QCOMPARE(apron->originY(), 4.0f);
    QVERIFY(qAbs(mp.downstageEdgeY() - 6.0f) < 1e-4f);

    // Back to Auto: to the deck's front again, the deck's front curve.
    mp.setApronAuto(true);
    QVERIFY(qAbs(mp.apronDepth() - 2.0f) < 1e-4f);
    QCOMPARE(apron->frontCurve(), 0.0f);

    // Saved with the apron, not as a platform of the show's: back the same.
    const QByteArray xml = stageMarksXml(mp);
    QCOMPARE(xml.count("<StagePlatform "), count);
    QVERIFY(xml.contains("Kind=\"Apron\" Visible=\"1\" Auto=\"1\""));
    MonitorProperties back;
    loadStageMarksXml(back, xml);
    QCOMPARE(back.platforms().size(), count + 1);
    QVERIFY(back.apronPlatform() != nullptr);
    QCOMPARE(back.apronPlatform()->id(), apron->id());
    QCOMPARE(back.apronPlatform()->originY(), 4.0f);
    QVERIFY(qAbs(back.apronDepth() - 2.0f) < 1e-4f);

    // Hidden: no deck, the id kept for it; a new platform does not take it.
    const quint32 apronId = apron->id();
    mp.setStagePieceVisible(MP::ApronPiece, false);
    QVERIFY(mp.apronPlatform() == nullptr);
    QCOMPARE(mp.platforms().size(), count);
    StagePlatform *other = mp.addPlatform();
    QVERIFY(other->id() != apronId);
    mp.removePlatform(other->id());
    mp.setStagePieceVisible(MP::ApronPiece, true);
    QVERIFY(mp.apronPlatform() != nullptr);
    QCOMPARE(mp.apronPlatform()->id(), apronId);

    // A curtain line found from the main drape: the apron starts there.
    Scenery *main = mp.addScenery();
    main->setKind(Scenery::MainDrape);
    main->setOriginX(5.0f); main->setOriginY(5.0f); main->setWidth(9.0f);
    mp.setCurtainLineAuto();
    QCOMPARE(mp.curtainLineY(), 5.0f);
    QVERIFY(mp.setApronDepth(3.0f));
    QVERIFY(mp.curtainLineIsAuto());
    QVERIFY(qAbs(mp.downstageEdgeY() - 8.0f) < 1e-4f);
    QCOMPARE(mp.apronPlatform()->originY(), 5.0f);

    // Over the audience: how far from the curtain line, in crew terms.
    QCOMPARE(mp.curtainLineOffsetText(9.2f), QStringLiteral("4.2 m DS of curtain line"));
    QCOMPARE(mp.curtainLineOffsetText(4.0f), QStringLiteral("1.0 m US of curtain line"));
    QCOMPARE(mp.curtainLineOffsetText(5.02f), QStringLiteral("on the curtain line"));
    QCOMPARE(MP::formatCurtainLineOffset(-0.4572f, MP::Feet), QStringLiteral("1' 6\" US of curtain line"));
    QCOMPARE(MP::formatStageDistance(1.8f, MP::Meters), QStringLiteral("1.8 m"));
    QCOMPARE(MP::formatStageDistance(1.8f, MP::Feet), QStringLiteral("5' 11\""));
    QCOMPARE(MP::formatStageDistance(1.2192f, MP::Feet), QStringLiteral("4'"));
}

void MonitorProperties_Test::targetPositionsAreSavedOnlyWhenOff()
{
    MonitorProperties mp;
    QVERIFY(mp.targetPositionsVisible());
    QVERIFY(!stageMarksXml(mp).contains("TargetPositions"));

    mp.setTargetPositionsVisible(false);
    const QByteArray off = stageMarksXml(mp);
    QVERIFY(off.contains("TargetPositions=\"0\""));
    MonitorProperties back;
    loadStageMarksXml(back, off);
    QVERIFY(!back.targetPositionsVisible());

    // A new show, or one that never turned them off, has them on.
    back.reset();
    QVERIFY(back.targetPositionsVisible());
    mp.setTargetPositionsVisible(true);
    QVERIFY(!stageMarksXml(mp).contains("TargetPositions"));
}

void MonitorProperties_Test::theNumberLineReadsFromCentre()
{
    typedef MonitorProperties MP;
    QCOMPARE(MP::numberLineLabel(0), QStringLiteral("CL"));
    QCOMPARE(MP::numberLineLabel(4), QStringLiteral("4"));
    QCOMPARE(MP::numberLineLabel(-4), QStringLiteral("4"));

    QCOMPARE(MP::numberLineStride(40.0), 1);
    QCOMPARE(MP::numberLineStride(15.0), 2);
    QCOMPARE(MP::numberLineStride(5.0), 5);
    QCOMPARE(MP::numberLineStride(2.0), 20);
    QCOMPARE(MP::numberLineStride(0.01), 50);

    foreach (int stride, QList<int>() << 1 << 2 << 5 << 10 << 20 << 50)
    {
        for (int k = -60; k <= 60; ++k)
        {
            // Mirrored, and long only where numbered.
            QCOMPARE(MP::numberLineMajorTick(k, stride), MP::numberLineMajorTick(-k, stride));
            if (MP::numberLineMajorTick(k, stride))
                QCOMPARE(k % stride, 0);
        }
        QVERIFY(MP::numberLineMajorTick(0, stride));
    }
    // Every 2 numbered: no long tick at 5 between 4 and 6.
    QVERIFY(!MP::numberLineMajorTick(5, 2));
    QVERIFY(MP::numberLineMajorTick(4, 2));
    QVERIFY(MP::numberLineMajorTick(5, 1));
}

void MonitorProperties_Test::theApronNeverMovesARiserAtTheFront()
{
    /* Branson's stage: a deck, and a centre riser butted against its front.
       Setting the apron's depth moved the riser (it was taken for the apron
       piece). The apron is its own piece now. */
    typedef MonitorProperties MP;
    MonitorProperties mp;
    mp.setGridUnits(MP::Meters);
    mp.setGridSize(QVector3D(12, 6, 10));
    StagePlatform *deck = mp.addPlatform();
    deck->setName(QStringLiteral("Deck"));
    deck->setOriginX(1.0f); deck->setOriginY(1.0f);
    deck->setWidth(10.0f); deck->setDepth(5.0f);
    deck->setHeight(0.9f);
    StagePlatform *riser = mp.addPlatform();
    riser->setName(QStringLiteral("Centre riser"));
    riser->setOriginX(5.0f); riser->setOriginY(6.0f);     // butted at the front, at centre
    riser->setWidth(2.0f); riser->setDepth(1.0f);
    riser->setHeight(0.4f);
    const QList<QPair<QString, float> > before = QList<QPair<QString, float> >()
        << qMakePair(QStringLiteral("x"), riser->originX()) << qMakePair(QStringLiteral("y"), riser->originY())
        << qMakePair(QStringLiteral("w"), riser->width()) << qMakePair(QStringLiteral("d"), riser->depth())
        << qMakePair(QStringLiteral("h"), riser->height());
    auto unchanged = [&]() {
        const QList<float> now = QList<float>() << riser->originX() << riser->originY()
                                                << riser->width() << riser->depth() << riser->height();
        for (int i = 0; i < before.size(); ++i)
            if (qAbs(now.at(i) - before.at(i).second) > 1e-5f)
                return false;
        return mp.platform(riser->id()) == riser && deck->depth() == 5.0f && deck->originY() == 1.0f;
    };

    mp.setStagePieceVisible(MP::CurtainPiece, true);
    mp.setStagePieceVisible(MP::ApronPiece, true);
    mp.setCurtainLineY(4.5f);
    foreach (float depth, QList<float>() << 0.5f << 1.5f << 3.0f << 4.0f << 0.0f << 2.0f)
    {
        QVERIFY(mp.setApronDepth(depth));
        QVERIFY2(unchanged(), qPrintable(QString::number(double(depth))));
    }
    // The apron is its own platform, from the curtain line, as wide as the deck.
    StagePlatform *apron = mp.apronPlatform();
    QVERIFY(apron != nullptr);
    QVERIFY(apron != riser && apron != deck);
    QCOMPARE(apron->originY(), 4.5f);
    QCOMPARE(apron->depth(), 2.0f);
    QCOMPARE(apron->originX(), 1.0f);
    QCOMPARE(apron->width(), 10.0f);
    // Its front curve and Auto don't touch the riser either.
    mp.setApronFrontCurve(0.6f);
    mp.setApronAuto(true);
    QVERIFY(unchanged());
    // Nor does a save and load.
    MonitorProperties back;
    loadStageMarksXml(back, stageMarksXml(mp));
    const StagePlatform *riser2 = back.platform(riser->id());
    QVERIFY(riser2 != nullptr);
    QCOMPARE(riser2->originY(), 6.0f);
    QCOMPARE(riser2->depth(), 1.0f);
    QVERIFY(back.apronPlatform() != nullptr);
    QVERIFY(back.apronPlatform()->id() != riser->id());
}

void MonitorProperties_Test::stageMarksMigrateToStagePieces()
{
    typedef MonitorProperties MP;
    /* A show saved with Show ▸ Stage marks: the centre mark and line, the
       curtain line and number line on and set, target positions off. */
    const QByteArray old("<?xml version=\"1.0\"?><Monitor DisplayMode=\"0\" ShowLabels=\"1\" "
                         "StageMarks=\"15\" CurtainLineY=\"4.2\" NumberLineY=\"6\" TargetPositions=\"0\">"
                         "<Grid Width=\"10\" Height=\"6\" Depth=\"8\" Units=\"0\"/>"
                         "<StagePlatform ID=\"0\" Name=\"Deck\" OriginX=\"1\" OriginY=\"1\" Width=\"8\" Depth=\"5\" Height=\"1\"/>"
                         "</Monitor>");
    MonitorProperties mp;
    loadStageMarksXml(mp, old);
    QVERIFY(mp.stagePieceVisible(MP::CentrePiece));
    QVERIFY(mp.stageMarkVisible(MP::CentreMarkFlag));
    QVERIFY(mp.stageMarkVisible(MP::CentreLineFlag));
    QVERIFY(mp.stagePieceVisible(MP::CurtainPiece));
    QVERIFY(mp.stagePieceVisible(MP::NumberPiece));
    QCOMPARE(mp.curtainLineY(), 4.2f);
    QCOMPARE(mp.numberLineY(), 6.0f);
    QVERIFY(!mp.targetPositionsVisible());
    // The tinted apron is the apron piece now: Auto, the deck past the curtain line.
    QVERIFY(mp.stagePieceVisible(MP::ApronPiece));
    QVERIFY(mp.apronAuto());
    QVERIFY(mp.hasStageLayer());
    QCOMPARE(mp.layer(mp.stageLayerId()).name, QStringLiteral("Stage"));
    const int platforms = mp.platforms().size();
    QVERIFY(mp.mainDeck() != nullptr);
    QCOMPARE(mp.deckFrontY(), 6.0f);
    QVERIFY(qAbs(mp.apronDepth() - 1.8f) < 1e-4f);
    QVERIFY(mp.apronPlatform() != nullptr);
    QCOMPARE(mp.apronPlatform()->originY(), 4.2f);
    QCOMPARE(platforms, 2);
    QCOMPARE(mp.downstageEdgeY(), 6.0f);         // the same front as before

    // Saved the new way only; back exactly as it was.
    const QByteArray now = stageMarksXml(mp);
    QVERIFY(now.contains("<StagePieces"));
    QVERIFY(!now.contains("StageMarks="));
    QVERIFY(!now.contains("CurtainLineY="));
    QVERIFY(!now.contains("NumberLineY="));
    QVERIFY(now.contains("TargetPositions=\"0\""));
    MonitorProperties back;
    loadStageMarksXml(back, now);
    QCOMPARE(back.stageMarks(), mp.stageMarks());
    QCOMPARE(back.curtainLineY(), 4.2f);
    QCOMPARE(back.numberLineY(), 6.0f);
    QCOMPARE(back.stageLayerId(), mp.stageLayerId());
    QCOMPARE(back.layers().size(), mp.layers().size());
    QCOMPARE(back.platforms().size(), platforms);
    QCOMPARE(stageMarksXml(back), now);

    // Only the number line shown before: just that, and no apron.
    MonitorProperties some;
    loadStageMarksXml(some, QByteArray("<?xml version=\"1.0\"?><Monitor DisplayMode=\"0\" StageMarks=\"8\"/>"));
    QCOMPARE(some.stageMarks(), int(MP::NumberLineFlag));
    QVERIFY(!some.stagePieceVisible(MP::ApronPiece));
    // And a show from before the marks: none, no Stage layer, saved as before.
    MonitorProperties none;
    loadStageMarksXml(none, QByteArray("<?xml version=\"1.0\"?><Monitor DisplayMode=\"0\"/>"));
    QCOMPARE(none.stageMarks(), int(MP::NoStageMarks));
    QVERIFY(!none.hasStageLayer());
    QCOMPARE(none.layers().size(), 1);
    QVERIFY(!stageMarksXml(none).contains("StagePieces"));
}

void MonitorProperties_Test::stagePiecesAreShownAndLockedOnTheirOwnAndByLayer()
{
    typedef MonitorProperties MP;
    MonitorProperties mp;
    QVERIFY(!mp.hasStageLayer());
    QCOMPARE(mp.stagePieces().size(), 4);
    foreach (MP::StagePiece piece, MP::stagePieces())
    {
        QVERIFY(!mp.stagePieceVisible(piece));
        QVERIFY(!MP::stagePieceName(piece).isEmpty());
    }

    // Shown: the Stage layer is made, and it stays.
    mp.setStagePieceVisible(MP::CurtainPiece, true);
    QVERIFY(mp.hasStageLayer());
    const quint32 stage = mp.stageLayerId();
    QVERIFY(stage != MP::defaultLayerId);
    mp.removeLayer(stage);
    QVERIFY(mp.hasLayer(stage));
    QCOMPARE(mp.ensureStageLayer(), stage);
    QVERIFY(mp.stagePieceShown(MP::CurtainPiece));
    QVERIFY(mp.stagePieceMovable(MP::CurtainPiece));

    // Its layer hidden: not drawn, though it is still on.
    mp.setLayerVisible(stage, false);
    QVERIFY(mp.stagePieceVisible(MP::CurtainPiece));
    QVERIFY(!mp.stagePieceShown(MP::CurtainPiece));
    QVERIFY(!mp.stageMarkShown(MP::CurtainLineFlag));
    QVERIFY(!mp.stagePieceMovable(MP::CurtainPiece));
    mp.setLayerVisible(stage, true);

    // Locked on its own, or with its layer.
    mp.setStagePieceLocked(MP::CurtainPiece, true);
    QVERIFY(!mp.stagePieceMovable(MP::CurtainPiece));
    QVERIFY(mp.stagePieceShown(MP::CurtainPiece));
    mp.setStagePieceLocked(MP::CurtainPiece, false);
    mp.setLayerLocked(stage, true);
    QVERIFY(!mp.stagePieceMovable(MP::CurtainPiece));
    mp.setLayerLocked(stage, false);

    // The centre piece comes back as it was hidden.
    mp.setStageMarks(mp.stageMarks() | MP::CentreLineFlag);
    QVERIFY(mp.stagePieceVisible(MP::CentrePiece));
    mp.setStagePieceVisible(MP::CentrePiece, false);
    QVERIFY(!mp.stageMarkVisible(MP::CentreLineFlag));
    mp.setStagePieceVisible(MP::CentrePiece, true);
    QVERIFY(mp.stageMarkVisible(MP::CentreLineFlag));
    QVERIFY(!mp.stageMarkVisible(MP::CentreMarkFlag));

    // Locks and a hidden centre piece's parts are saved with the show.
    mp.setStagePieceLocked(MP::NumberPiece, true);
    mp.setStagePieceVisible(MP::CentrePiece, false);
    MonitorProperties back;
    loadStageMarksXml(back, stageMarksXml(mp));
    QVERIFY(back.stagePieceLocked(MP::NumberPiece));
    QVERIFY(!back.stagePieceLocked(MP::CurtainPiece));
    QVERIFY(!back.stagePieceVisible(MP::CentrePiece));
    back.setStagePieceVisible(MP::CentrePiece, true);
    QVERIFY(back.stageMarkVisible(MP::CentreLineFlag));
    QCOMPARE(back.stageLayerId(), stage);
}
