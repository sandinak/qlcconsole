/*
  Q Light Controller Plus - Test Unit
  scenery_test.cpp

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

#include <QtTest>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "monitorproperties.h"
#include "powerdistribution.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "scenery.h"
#include "doc.h"

#include "scenery_test.h"

static bool near(double a, double b, double eps = 1e-3) { return qAbs(a - b) < eps; }

/* A 10 m cyc, 6 m tall, its bottom edge centred at (6, 0.5). */
static void cyc(Scenery &s)
{
    s.setOriginX(6.0f);
    s.setOriginY(0.5f);
    s.setWidth(10.0f);
    s.setHeight(6.0f);
}

void Scenery_Test::aFlatCycRunsAcrossTheStageFacingDownstage()
{
    Scenery s(0);
    cyc(s);
    QCOMPARE(s.kind(), int(Scenery::Cyclorama));
    QCOMPARE(Scenery::kindName(s.kind()), QString("Cyclorama"));
    QVERIFY(near(s.facing().x(), 0.0) && near(s.facing().y(), 1.0));   // toward the audience
    const QVector3D left = s.pointAt(-1.0, 0.0), right = s.pointAt(1.0, 6.0);
    QVERIFY(near(left.x(), 1.0) && near(left.y(), 0.5));
    QVERIFY(near(right.x(), 11.0) && near(right.y(), 0.5) && near(right.z(), 6.0));
    const QList<QPointF> line = s.baseLine(4);
    QCOMPARE(line.size(), 5);
    QVERIFY(near(line.at(2).x(), 6.0) && near(line.at(2).y(), 0.5));
    // Clamped past its ends.
    QVERIFY(near(s.pointAt(3.0, 0.0).x(), 11.0));
}

void Scenery_Test::theBowPullsTheMiddleUpstage()
{
    Scenery s(0);
    cyc(s);
    s.setBow(1.5f);
    QVERIFY(near(s.pointAt(0.0, 0.0).y(), -1.0));       // 1.5 m further upstage
    QVERIFY(near(s.pointAt(1.0, 0.0).y(), 0.5));        // the ends stay put
    QVERIFY(near(s.pointAt(0.5, 0.0).y(), 0.5 - 1.5 * 0.75));
    s.setBow(-2.0f);                                    // and toward the house
    QVERIFY(near(s.pointAt(0.0, 0.0).y(), 2.5));
    QVERIFY(near(s.normalAt(0.0).y(), 1.0));            // still facing it
}

void Scenery_Test::rotationTurnsTheSheet()
{
    Scenery s(0);
    cyc(s);
    s.setRotation(90.0f);                               // a sheet running up/down stage
    QVERIFY(near(s.pointAt(1.0, 0.0).x(), 6.0) && near(s.pointAt(1.0, 0.0).y(), 5.5));
    QVERIFY(near(s.facing().x(), -1.0) && near(s.facing().y(), 0.0));
}

void Scenery_Test::aBeamStopsWhereItMeetsTheSheet()
{
    Scenery s(0);
    cyc(s);
    // From a head 5 m downstage at 3 m, straight upstage.
    double t = 0.0;
    QVERIFY(s.rayHit(QVector3D(6.0f, 5.5f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), t));
    QVERIFY(near(t, 5.0));
    // Tilted down a little, still on the cloth.
    const QVector3D d = QVector3D(0.2f, -1.0f, -0.3f).normalized();
    QVERIFY(s.rayHit(QVector3D(6.0f, 5.5f, 3.0f), d, t));
    const QVector3D at = QVector3D(6.0f, 5.5f, 3.0f) + d * float(t);
    QVERIFY(near(at.y(), 0.5) && at.z() > 1.0f && at.z() < 3.0f);
}

void Scenery_Test::aBeamThatPassesAboveBesideOrAwayMisses()
{
    Scenery s(0);
    cyc(s);
    s.setBottom(1.0f);
    double t = -1.0;
    QVERIFY(!s.rayHit(QVector3D(6.0f, 5.5f, 8.0f), QVector3D(0.0f, -1.0f, 0.0f), t));  // over the top
    QVERIFY(!s.rayHit(QVector3D(6.0f, 5.5f, 0.5f), QVector3D(0.0f, -1.0f, 0.0f), t));  // under a flown one
    QVERIFY(!s.rayHit(QVector3D(15.0f, 5.5f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), t)); // off its end
    QVERIFY(!s.rayHit(QVector3D(6.0f, 5.5f, 3.0f), QVector3D(0.0f, 1.0f, 0.0f), t));   // into the house
    QVERIFY(!s.rayHit(QVector3D(6.0f, 5.5f, 3.0f), QVector3D(1.0f, 0.0f, 0.0f), t));   // parallel
    QVERIFY(!s.rayHit(QVector3D(6.0f, 5.5f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), t, 2.0)); // lands first
    QCOMPARE(t, -1.0);                                  // untouched on a miss
}

void Scenery_Test::aBowedCycIsHitFurtherBackInTheMiddle()
{
    Scenery s(0);
    cyc(s);
    s.setBow(1.5f);
    double mid = 0.0, end = 0.0;
    QVERIFY(s.rayHit(QVector3D(6.0f, 5.5f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), mid));
    QVERIFY(s.rayHit(QVector3D(10.9f, 5.5f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), end));
    QVERIFY2(near(mid, 6.5, 0.05), qPrintable(QString::number(mid)));   // the bow, 1.5 m back
    QVERIFY2(end < 5.2, qPrintable(QString::number(end)));
}

void Scenery_Test::aUWallRunsRoundItsFront()
{
    // A fountain's rim: 3 ft across, 4 ft deep, its open back at (5, 5).
    Scenery s(0);
    s.setKind(Scenery::Wall);
    s.setOutline(Scenery::RoundedU);
    s.setOriginX(5.0f);
    s.setOriginY(5.0f);
    s.setWidth(0.9144f);
    s.setDepth(1.2192f);
    const double r = 0.4572, side = 1.2192 - r;
    QVERIFY(near(s.pointAt(-1.0, 0.0).x(), 5.0 - r) && near(s.pointAt(-1.0, 0.0).y(), 5.0));
    QVERIFY(near(s.pointAt(1.0, 0.0).x(), 5.0 + r) && near(s.pointAt(1.0, 0.0).y(), 5.0));
    // The front of the curve is the middle, toward the audience.
    QVERIFY(near(s.pointAt(0.0, 0.0).x(), 5.0) && near(s.pointAt(0.0, 0.0).y(), 5.0 + side + r));
    QVERIFY(near(s.normalAt(0.0).y(), 1.0));
    // Outward all the way round: off the stage-right side, stage right.
    QVERIFY(near(s.normalAt(-0.95).x(), -1.0));
    QVERIFY(near(s.normalAt(0.95).x(), 1.0));
    // A face is half the thickness off the line.
    s.setThickness(0.1016f);
    QVERIFY(near(s.pointAt(0.0, 0.0, 0.0508).y(), 5.0 + side + r + 0.0508));
    QVERIFY(s.segments() > 16);
}

void Scenery_Test::aWallIsMetOnItsFaceAndOnItsTop()
{
    Scenery s(0);
    s.setKind(Scenery::Wall);
    s.setOriginX(0.0f);
    s.setOriginY(0.0f);
    s.setWidth(2.0f);
    s.setHeight(0.5f);
    s.setThickness(0.1f);
    double t = 0.0;
    QVector3D n;
    // Level, from downstage: its front face, 5 cm in front of the line.
    QVERIFY(s.rayHit(QVector3D(0.0f, 2.0f, 0.25f), QVector3D(0.0f, -1.0f, 0.0f), t, 1e9, 16, &n));
    QVERIFY2(near(t, 1.95), qPrintable(QString::number(t)));
    QVERIFY(near(n.y(), 1.0));
    // Straight down on it: its top.
    QVERIFY(s.rayHit(QVector3D(0.3f, 0.02f, 3.0f), QVector3D(0.0f, 0.0f, -1.0f), t, 1e9, 16, &n));
    QVERIFY(near(t, 2.5));
    QVERIFY(near(n.z(), 1.0));
    // Down just past its face: misses.
    QVERIFY(!s.rayHit(QVector3D(0.3f, 0.08f, 3.0f), QVector3D(0.0f, 0.0f, -1.0f), t));
}

void Scenery_Test::anOpenCurtainHangsOnlyAtItsEnds()
{
    Scenery s(0);
    cyc(s);
    s.setKind(Scenery::MainDrape);
    QVERIFY(s.hasState());
    double t = 0.0;
    QVERIFY(s.rayHit(QVector3D(6.0f, 5.5f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), t));    // closed
    s.setState(Scenery::Open);
    QCOMPARE(s.spans().size(), 2);
    QVERIFY(!s.rayHit(QVector3D(6.0f, 5.5f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), t));   // through the middle
    QVERIFY(s.rayHit(QVector3D(1.5f, 5.5f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), t));    // its gathered end
    // Opened by as much as asked: 2 m across the middle of the 10 m.
    s.setOpeningWidth(2.0f);
    QVERIFY(s.rayHit(QVector3D(4.5f, 5.5f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), t));    // a half, gathered
    QVERIFY(!s.rayHit(QVector3D(6.5f, 5.5f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), t));   // the opening
    s.setOpeningWidth(9.0f);
    QVERIFY(!s.rayHit(QVector3D(4.5f, 5.5f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), t));
    s.setState(Scenery::Flown);
    QVERIFY(s.spans().isEmpty());
    QVERIFY(!s.rayHit(QVector3D(1.5f, 5.5f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), t));
    // A cyc does not open.
    s.setKind(Scenery::Cyclorama);
    QCOMPARE(s.spans().size(), 1);
}

void Scenery_Test::aProsceniumIsTwoPiersAndAHeader()
{
    // 14 m wide, 7 m high; a 10 m x 5 m opening; 40 cm thick; at y = 8.
    Scenery s(0);
    s.setKind(Scenery::Proscenium);
    s.setOriginX(7.0f);
    s.setOriginY(8.0f);
    s.setWidth(14.0f);
    s.setHeight(7.0f);
    s.setThickness(0.4f);
    s.setOpeningWidth(10.0f);
    s.setOpeningHeight(5.0f);
    QVERIFY(s.isBuilt() && !s.isCloth());
    const QList<Scenery::Piece> pieces = s.pieces();
    QCOMPARE(pieces.size(), 3);
    double t = 0.0;
    QVector3D n;
    // Through the opening, from the house.
    QVERIFY(!s.rayHit(QVector3D(7.0f, 15.0f, 2.0f), QVector3D(0.0f, -1.0f, 0.0f), t));
    // On a pier: its front face, 20 cm in front of the line.
    QVERIFY(s.rayHit(QVector3D(1.0f, 15.0f, 2.0f), QVector3D(0.0f, -1.0f, 0.0f), t, 1e9, 16, &n));
    QVERIFY2(near(t, 6.8), qPrintable(QString::number(t)));
    // On the header, above the opening.
    QVERIFY(s.rayHit(QVector3D(7.0f, 15.0f, 6.0f), QVector3D(0.0f, -1.0f, 0.0f), t));
    // Up under the header, from the stage: its underside, at 5 m.
    QVERIFY(s.rayHit(QVector3D(7.0f, 8.0f, 1.0f), QVector3D(0.0f, 0.0f, 1.0f), t, 1e9, 16, &n));
    QVERIFY(near(t, 4.0) && near(n.z(), -1.0));

    QByteArray buf;
    QXmlStreamWriter w(&buf);
    QVERIFY(s.saveXML(&w));
    QXmlStreamReader r(buf);
    r.readNextStartElement();
    Scenery back(0);
    QVERIFY(back.loadXML(r));
    QVERIFY(near(back.openingWidth(), 10.0) && near(back.openingHeight(), 5.0));
}

void Scenery_Test::xmlRoundTripAndStorage()
{
    Scenery s(7);
    cyc(s);
    s.setName("Upstage Cyc");
    s.setBottom(0.3f);
    s.setRotation(5.0f);
    s.setBow(1.2f);
    s.setColor(QColor("#ddeeff"));
    s.setLocked(true);
    s.setLayerId(3);
    s.setGroupId(4);
    s.setKind(Scenery::Wall);
    s.setOutline(Scenery::RoundedU);
    s.setDepth(1.25f);
    s.setThickness(0.1f);
    s.setState(Scenery::Open);

    QByteArray buf;
    QXmlStreamWriter w(&buf);
    QVERIFY(s.saveXML(&w));
    QXmlStreamReader r(buf);
    r.readNextStartElement();
    Scenery back(0);
    QVERIFY(back.loadXML(r));
    QCOMPARE(back.id(), quint32(7));
    QCOMPARE(back.name(), QString("Upstage Cyc"));
    QVERIFY(near(back.originX(), 6.0) && near(back.originY(), 0.5));
    QVERIFY(near(back.width(), 10.0) && near(back.height(), 6.0) && near(back.bottom(), 0.3));
    QVERIFY(near(back.rotation(), 5.0) && near(back.bow(), 1.2));
    QCOMPARE(back.color(), QColor("#ddeeff"));
    QVERIFY(back.locked());
    QCOMPARE(back.layerId(), quint32(3));
    QCOMPARE(back.groupId(), quint32(4));
    QCOMPARE(back.kind(), int(Scenery::Wall));
    QCOMPARE(back.outline(), int(Scenery::RoundedU));
    QVERIFY(near(back.depth(), 1.25) && near(back.thickness(), 0.1));
    QCOMPARE(back.state(), int(Scenery::Open));

    QXmlStreamReader wrong(QByteArray("<Stand ID=\"1\"/>"));
    wrong.readNextStartElement();
    QVERIFY(!back.loadXML(wrong));

    // Kept by the monitor properties, and saved with the workspace.
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    Scenery *a = props->addScenery();
    Scenery *b = props->addScenery();
    QCOMPARE(props->sceneries().size(), 2);
    QVERIFY(a->id() != b->id());
    a->setName("Cyc");
    QByteArray ws;
    QXmlStreamWriter ww(&ws);
    ww.writeStartDocument();
    QVERIFY(doc.saveXML(&ww));
    ww.writeEndDocument();
    QVERIFY(ws.contains("<Scenery"));
    Doc other(this);
    QXmlStreamReader rr(ws);
    while (rr.readNextStartElement() && rr.name() != QStringLiteral("Engine"))
        ;
    QVERIFY(other.loadXML(rr));
    QCOMPARE(other.monitorProperties()->sceneries().size(), 2);
    QVERIFY(other.monitorProperties()->scenery(a->id()) != nullptr);
    QCOMPARE(other.monitorProperties()->scenery(a->id())->name(), QString("Cyc"));

    props->removeScenery(a->id());
    QCOMPARE(props->sceneries().size(), 1);
    QVERIFY(props->scenery(a->id()) == nullptr);
}

/* A 10 m main drape across the stage at y = 0.5, opening 8 m. */
static void drape(Scenery &s)
{
    cyc(s);
    s.setKind(Scenery::MainDrape);
    s.setOpeningWidth(8.0f);
}

void Scenery_Test::aDrivenCurtainOpensAsItsChannelMoves()
{
    Scenery s(0);
    drape(s);
    Scenery::Pose pose;
    // Closed: one sheet, all the way across -- as the fixed Closed state is.
    QCOMPARE(s.spans(pose), s.spans());
    QCOMPARE(s.spans(pose).size(), 1);
    // Fully open: exactly as the fixed Open state draws it.
    pose.open = 1.0;
    s.setState(Scenery::Open);
    QCOMPARE(s.spans(pose), s.spans());
    QCOMPARE(s.spans(pose).size(), 2);
    QVERIFY(near(s.spans(pose).at(1).first, 0.8));          // 8 m of 10 m: the halves at +-0.8
    s.setState(Scenery::Closed);
    // On the way: the halves part from the middle, as far as the channel says.
    double last = -1.0;
    for (int v = 1; v <= 255; v += 26)
    {
        pose.open = v / 255.0;
        const QList<QPair<double, double> > sp = s.spans(pose);
        QCOMPARE(sp.size(), 2);
        QVERIFY(near(sp.at(0).second, -sp.at(1).first));     // centred
        QVERIFY(near(sp.at(1).first, 0.8 * pose.open));
        QVERIFY(sp.at(1).first > last);                       // opening as it rises
        last = sp.at(1).first;
    }
    // Half open: a beam through the middle passes, one 3 m off centre is stopped.
    pose.open = 0.5;
    double t = 0.0;
    QVERIFY(!s.rayHit(QVector3D(6.0f, 5.5f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), t, 1e9, 16, nullptr, &pose));
    QVERIFY(s.rayHit(QVector3D(9.0f, 5.5f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), t, 1e9, 16, nullptr, &pose));
    pose.open = 1.0;
    QVERIFY(!s.rayHit(QVector3D(9.0f, 5.5f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), t, 1e9, 16, nullptr, &pose));
    // Not a curtain: a pose changes nothing.
    s.setKind(Scenery::Cyclorama);
    QCOMPARE(s.spans(pose).size(), 1);
}

void Scenery_Test::flyingTakesItUpAndOut()
{
    Scenery s(0);
    drape(s);
    Scenery::Pose pose;
    pose.fly = 0.5;
    QList<Scenery::Piece> pc = s.pieces(pose);
    QCOMPARE(pc.size(), 1);
    QVERIFY(near(pc.first().z0, 3.0) && near(pc.first().z1, 9.0));   // half its 6 m up
    double t = 0.0;
    // Under it now: a level beam at 2 m goes through where it hung.
    QVERIFY(!s.rayHit(QVector3D(6.0f, 5.5f, 2.0f), QVector3D(0.0f, -1.0f, 0.0f), t, 1e9, 16, nullptr, &pose));
    pose.fly = 1.0;
    QVERIFY(s.pieces(pose).isEmpty());
    s.setState(Scenery::Flown);
    QCOMPARE(s.staticPose().fly, 1.0);
    QVERIFY(s.pieces().isEmpty());
}

static QLCFixtureDef *curtainDef()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    const QString path = QFileInfo(QStringLiteral(__FILE__)).dir().absoluteFilePath(
        QStringLiteral("../../../resources/fixtures/Generic/Generic-Stage-Curtain.qxf"));
    if (def->loadXML(path) != QFile::NoError)
    {
        delete def;
        return nullptr;
    }
    return def;
}

void Scenery_Test::theChannelsAreFoundByName()
{
    QLCFixtureDef *def = curtainDef();
    QVERIFY(def != nullptr);
    QLCFixtureMode *open = def->mode("Open");
    QLCFixtureMode *fine = def->mode("Open 16-bit + Fly");
    QLCFixtureMode *fly = def->mode("Open + Fly");
    QVERIFY(open && fine && fly);
    QCOMPARE(Scenery::curtainChannel(open, Scenery::OpenChannel), 0);
    QCOMPARE(Scenery::curtainChannel(open, Scenery::FlyChannel), -1);
    QCOMPARE(Scenery::curtainChannel(fine, Scenery::OpenFineChannel), 1);
    QCOMPARE(Scenery::curtainChannel(fine, Scenery::FlyChannel), 2);
    QCOMPARE(Scenery::curtainChannel(fly, Scenery::FlyChannel), 1);

    QByteArray v(1, char(0));
    QCOMPARE(Scenery::poseFromValues(open, v).open, 0.0);
    v[0] = char(255);
    QCOMPARE(Scenery::poseFromValues(open, v).open, 1.0);
    v[0] = char(51);
    QVERIFY(near(Scenery::poseFromValues(open, v).open, 0.2));
    QByteArray w(3, char(0));
    w[0] = char(128); w[1] = char(0); w[2] = char(255);
    const Scenery::Pose p = Scenery::poseFromValues(fine, w);
    QVERIFY(near(p.open, 32768.0 / 65535.0, 1e-6));
    QCOMPARE(p.fly, 1.0);
    // Values that are not there (a fixture not yet written) are closed and in.
    QCOMPARE(Scenery::poseFromValues(fine, QByteArray()).open, 0.0);
    delete def;
}

void Scenery_Test::theBundledCurtainIsNotALight()
{
    Doc doc(this);
    QLCFixtureDef *def = curtainDef();
    QVERIFY(def != nullptr);
    QVERIFY(doc.fixtureDefCache()->addFixtureDef(def));
    QVERIFY(!def->modes().first()->physical().emitsLight());
    QCOMPARE(def->modes().first()->physical().powerConsumption(), 0);

    Fixture *curtain = new Fixture(&doc);
    curtain->setFixtureDefinition(def, def->mode("Open + Fly"));
    curtain->setPatched(false);
    QVERIFY(doc.addFixture(curtain));
    QVERIFY(Scenery::isCurtainFixture(curtain));

    // No load, and not a light whose power is unknown either.
    int fuzzy = -1;
    QCOMPARE(doc.totalPowerConsumption(fuzzy), 0);
    QCOMPARE(fuzzy, 0);
    QCOMPARE(PowerEstimator::fixtureFullWatts(curtain), 0.0);
    QCOMPARE(PowerEstimator::basis(curtain), PowerEstimator::NoLoad);
    QVERIFY(PowerEstimator::byType(&doc, QHash<quint32, double>(), QHash<quint32, double>()).isEmpty());

    // A show with a drape linked to it: saved and opened again, still linked.
    Scenery *sc = doc.monitorProperties()->addScenery();
    drape(*sc);
    sc->setDriverFixture(curtain->id());
    QByteArray ws;
    {
        QXmlStreamWriter ww(&ws);
        ww.writeStartDocument();
        QVERIFY(doc.saveXML(&ww));
        ww.writeEndDocument();
    }
    Doc other(this);
    QLCFixtureDef *def2 = curtainDef();
    QVERIFY(def2 != nullptr && other.fixtureDefCache()->addFixtureDef(def2));
    QXmlStreamReader rr(ws);
    while (rr.readNextStartElement() && rr.name() != QStringLiteral("Engine"))
        ;
    QVERIFY(other.loadXML(rr));
    Scenery *back = other.monitorProperties()->scenery(sc->id());
    QVERIFY(back != nullptr && back->isDriven());
    QCOMPARE(back->driverFixture(), curtain->id());
    Fixture *again = other.fixture(back->driverFixture());
    QVERIFY(again != nullptr && Scenery::isCurtainFixture(again));
    QVERIFY(!again->isPatched());
    QCOMPARE(again->fixtureMode()->name(), QString("Open + Fly"));

    // A dimmer is no curtain.
    Fixture *dim = new Fixture(&doc);
    dim->setChannels(1);
    QVERIFY(doc.addFixture(dim));
    QVERIFY(!Scenery::isCurtainFixture(dim));
}

void Scenery_Test::theDriverIsSavedAndOldShowsAreUndriven()
{
    // A drape from a show saved before curtains could be driven.
    const QByteArray old("<Scenery ID=\"1\" Name=\"Main drape 2\" Kind=\"2\" OriginX=\"0.000\" "
                         "OriginY=\"0.000\" Width=\"14.000\" Height=\"6.000\" State=\"Open\" "
                         "OpeningWidth=\"12.192\" Color=\"#761622\"/>");
    QXmlStreamReader r(old);
    r.readNextStartElement();
    Scenery s(0);
    QVERIFY(s.loadXML(r));
    QVERIFY(!s.isDriven());
    QCOMPARE(s.driverFixture(), Scenery::invalidId());
    QCOMPARE(s.staticPose().open, 1.0);
    QCOMPARE(s.spans().size(), 2);

    // Saved again unchanged: still no driver.
    QByteArray buf;
    {
        QXmlStreamWriter w(&buf);
        QVERIFY(s.saveXML(&w));
    }
    QVERIFY(!buf.contains("Fixture="));

    // Driven: saved, and read back.
    s.setDriverFixture(12);
    QVERIFY(s.isDriven());
    buf.clear();
    {
        QXmlStreamWriter w(&buf);
        QVERIFY(s.saveXML(&w));
    }
    QVERIFY(buf.contains("Fixture=\"12\""));
    QXmlStreamReader r2(buf);
    r2.readNextStartElement();
    Scenery back(0);
    QVERIFY(back.loadXML(r2));
    QCOMPARE(back.driverFixture(), quint32(12));
    QVERIFY(back.isDriven());
    QCOMPARE(back.state(), int(Scenery::Open));      // its fixed state is kept too

    // Only a curtain is driven.
    back.setKind(Scenery::Cyclorama);
    QVERIFY(!back.isDriven());
}

QTEST_GUILESS_MAIN(Scenery_Test)
