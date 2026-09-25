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
    s.setBow(-2.0f);
    QCOMPARE(s.bow(), 0.0f);                            // no bowing toward the house
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

QTEST_GUILESS_MAIN(Scenery_Test)
