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
#define private public
#include "monitorproperties.h"
#undef private
#include "stageplatform.h"
#include "truss.h"
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

QTEST_APPLESS_MAIN(MonitorProperties_Test)
