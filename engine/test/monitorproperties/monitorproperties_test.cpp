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
#include <QSignalSpy>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QTemporaryDir>
#include <QBuffer>
#define private public
#include "monitorproperties.h"
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

void MonitorProperties_Test::itemLocksFoldIntoALockedLayer()
{
    Doc doc(this);
    MonitorProperties *mp = doc.monitorProperties();
    PowerDistribution *pd = doc.powerDistribution();
    NetworkTopology *nt = doc.networkTopology();

    // Nothing pinned: nothing moves, and no "Locked" layer appears.
    Truss *free = mp->addTruss();
    QCOMPARE(mp->foldItemLocksIntoLayers(pd, nt), 0);
    QCOMPARE(mp->layers().size(), 1);

    // One of each kind, pinned on its own on the (unlocked) Default layer.
    Truss *t = mp->addTruss();
    t->setLocked(true);
    StagePlatform *deck = mp->addPlatform();
    deck->setLocked(true);
    Pipe *pipe = mp->addPipe();
    pipe->setLocked(true);
    Stand *stand = mp->addStand();
    stand->setLocked(true);
    Tower *tower = mp->addTower();
    tower->setLocked(true);
    StageTarget *mark = mp->addStageTarget();
    mark->setLocked(true);
    Scenery *cyc = mp->addScenery();
    cyc->setLocked(true);
    const quint32 imgId = mp->addImage("plan.png");
    MonitorProperties::MonitorImage img = mp->image(imgId);
    img.locked = true;
    mp->setImage(img);
    PowerSource ps;
    ps.locked = true;
    pd->sources() << ps;
    NetworkNode node;
    node.locked = true;
    nt->nodes() << node;

    // Already on a locked layer: that lock already says it, so it stays.
    const quint32 held = mp->addLayer("Held");
    mp->setLayerLocked(held, true);
    Stand *heldStand = mp->addStand();
    heldStand->setLayerId(held);
    heldStand->setLocked(true);

    QCOMPARE(mp->foldItemLocksIntoLayers(pd, nt), 10);
    quint32 locked = 0;
    int named = 0;
    foreach (const MonitorProperties::MonitorLayer &l, mp->layers())
        if (l.name == QStringLiteral("Locked"))
        {
            locked = l.id;
            named++;
        }
    QCOMPARE(named, 1);
    QVERIFY(mp->layer(locked).locked);
    QCOMPARE(t->layerId(), locked);
    QCOMPARE(deck->layerId(), locked);
    QCOMPARE(pipe->layerId(), locked);
    QCOMPARE(stand->layerId(), locked);
    QCOMPARE(tower->layerId(), locked);
    QCOMPARE(mark->layerId(), locked);
    QCOMPARE(cyc->layerId(), locked);
    QCOMPARE(mp->image(imgId).layerId, locked);
    QCOMPARE(pd->sources().first().layerId, locked);
    QCOMPARE(nt->nodes().first().layerId, locked);
    QCOMPARE(free->layerId(), quint32(MonitorProperties::defaultLayerId));
    QCOMPARE(heldStand->layerId(), held);
    QVERIFY(!mp->layer(MonitorProperties::defaultLayerId).locked);

    // Run again (the next load): already folded, and the layer is reused.
    Truss *later = mp->addTruss();
    later->setLocked(true);
    QCOMPARE(mp->foldItemLocksIntoLayers(pd, nt), 1);
    QCOMPARE(later->layerId(), locked);
    QCOMPARE(mp->layers().size(), 3);

    // Without power or network, the rest still folds.
    Pipe *p2 = mp->addPipe();
    p2->setLocked(true);
    QCOMPARE(mp->foldItemLocksIntoLayers(nullptr, nullptr), 1);
    QCOMPARE(p2->layerId(), locked);

    // Loading a workspace does the fold: an old show's pinned truss lands
    // on the locked layer, and the unpinned one does not.
    Doc old(this);
    Truss *pinned = old.monitorProperties()->addTruss();
    pinned->setName("T-1");
    pinned->setLocked(true);
    Truss *loose = old.monitorProperties()->addTruss();
    loose->setName("T-2");
    QByteArray ws;
    QXmlStreamWriter ww(&ws);
    ww.writeStartDocument();
    QVERIFY(old.saveXML(&ww));
    ww.writeEndDocument();
    Doc reloaded(this);
    QXmlStreamReader rr(ws);
    while (rr.readNextStartElement() && rr.name() != QStringLiteral("Engine"))
        ;
    QVERIFY(reloaded.loadXML(rr));
    MonitorProperties *rp = reloaded.monitorProperties();
    const Truss *rt1 = rp->truss(pinned->id());
    const Truss *rt2 = rp->truss(loose->id());
    QVERIFY(rt1 != nullptr && rt2 != nullptr);
    QVERIFY(rt1->layerId() != MonitorProperties::defaultLayerId);
    QCOMPARE(rp->layer(rt1->layerId()).name, QString("Locked"));
    QVERIFY(rp->layer(rt1->layerId()).locked);
    QVERIFY(rt1->locked());
    QCOMPARE(rt2->layerId(), quint32(MonitorProperties::defaultLayerId));
    QVERIFY(!rt2->locked());
}

void MonitorProperties_Test::itemLockFlagsMirrorTheirLayer()
{
    Doc doc(this);
    MonitorProperties *mp = doc.monitorProperties();
    PowerDistribution *pd = doc.powerDistribution();
    NetworkTopology *nt = doc.networkTopology();
    const quint32 rig = mp->addLayer("Rig");

    Truss *onRig = mp->addTruss();
    onRig->setLayerId(rig);
    Truss *elsewhere = mp->addTruss();
    elsewhere->setLocked(true);                       // stale: its layer is not
    Scenery *cyc = mp->addScenery();
    cyc->setLayerId(rig);
    const quint32 imgId = mp->addImage("plan.png");
    MonitorProperties::MonitorImage img = mp->image(imgId);
    img.layerId = rig;
    mp->setImage(img);
    PowerSource ps;
    ps.layerId = rig;
    pd->sources() << ps;
    NetworkNode node;
    node.locked = false;
    node.layerId = rig;
    nt->nodes() << node;

    mp->setLayerLocked(rig, true);
    mp->syncItemLocksToLayers(pd, nt);
    QVERIFY(onRig->locked());
    QVERIFY(!elsewhere->locked());
    QVERIFY(cyc->locked());
    QVERIFY(mp->image(imgId).locked);
    QVERIFY(pd->sources().first().locked);
    QVERIFY(nt->nodes().first().locked);

    mp->setLayerLocked(rig, false);
    mp->syncItemLocksToLayers(nullptr, nullptr);      // no power/network: skipped
    QVERIFY(!onRig->locked());
    QVERIFY(!cyc->locked());
    QVERIFY(!mp->image(imgId).locked);
    QVERIFY(pd->sources().first().locked);
    mp->syncItemLocksToLayers(pd, nt);
    QVERIFY(!pd->sources().first().locked);
    QVERIFY(!nt->nodes().first().locked);
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
