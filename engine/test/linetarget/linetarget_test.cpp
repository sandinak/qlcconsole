/*
  Q Light Controller Plus
  linetarget_test.cpp

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

#include "linetarget_test.h"
#include "linecurve.h"
#include "linespread.h"
#include "aimsolver.h"
#include "monitorproperties.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "scenevalue.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

static bool near3(const QVector3D &a, const QVector3D &b, float tol = 0.01f)
{
    return (a - b).length() <= tol;
}

static QString str(const QVector3D &v)
{
    return QString("(%1, %2, %3)").arg(double(v.x()), 0, 'f', 3)
                                  .arg(double(v.y()), 0, 'f', 3)
                                  .arg(double(v.z()), 0, 'f', 3);
}

void LineTarget_Test::aStraightLineIsMeasuredEvenly()
{
    LineCurve c({ QVector3D(-2, 0, 0), QVector3D(2, 0, 0) });
    QVERIFY(c.isValid());
    QVERIFY(qAbs(c.length() - 4.0) < 1e-4);
    QVERIFY2(near3(c.pointAt(0.25), QVector3D(-1, 0, 0), 1e-3f), qPrintable(str(c.pointAt(0.25))));
    QVERIFY(near3(c.pointAt(0.0), QVector3D(-2, 0, 0), 1e-4f));
    QVERIFY(near3(c.pointAt(1.0), QVector3D(2, 0, 0), 1e-4f));
    // Clamped, not extrapolated.
    QVERIFY(near3(c.pointAt(1.5), QVector3D(2, 0, 0), 1e-4f));
    QVERIFY(near3(c.pointAt(-1.0), QVector3D(-2, 0, 0), 1e-4f));

    QVERIFY(!LineCurve({ QVector3D(1, 1, 1) }).isValid());
    QVERIFY(LineCurve({ QVector3D(1, 1, 1) }).pointAt(0.5) == QVector3D(1, 1, 1));
    QVERIFY(LineCurve({}).pointAt(0.5) == QVector3D());
}

void LineTarget_Test::theCurvePassesThroughEveryPoint()
{
    const QVector3D mid(0, 1, 0.5f);
    LineCurve c({ QVector3D(-2, 0, 0), mid, QVector3D(2, 0, 0) });
    QVERIFY2(c.length() > 4.0, "bending a line makes it longer");
    // Symmetric, so the middle handle is exactly half-way along.
    QVERIFY2(near3(c.pointAt(0.5), mid, 0.01f), qPrintable(str(c.pointAt(0.5))));
    bool through = false;
    foreach (const QVector3D &s, c.samples())
        if (near3(s, mid, 1e-4f))
            through = true;
    QVERIFY2(through, "no sample sits on the middle handle");
}

void LineTarget_Test::halfWayIsHalfTheDistance()
{
    // Unevenly spaced handles on one straight line: 1 m, then 4 m.
    LineCurve c({ QVector3D(0, 0, 0), QVector3D(1, 0, 0), QVector3D(5, 0, 0) });
    QVERIFY2(qAbs(c.length() - 5.0) < 0.01, qPrintable(QString::number(c.length())));
    QVERIFY2(near3(c.pointAt(0.5), QVector3D(2.5f, 0, 0), 0.02f), qPrintable(str(c.pointAt(0.5))));
    QVERIFY2(near3(c.pointAt(0.1), QVector3D(0.5f, 0, 0), 0.02f), qPrintable(str(c.pointAt(0.1))));
}

void LineTarget_Test::spansAndNearestPoints()
{
    LineCurve c({ QVector3D(0, 0, 0), QVector3D(1, 0, 0), QVector3D(5, 0, 0) });
    QCOMPARE(c.spanAt(0.05), 0);       // in the first metre
    QCOMPARE(c.spanAt(0.5), 1);
    QCOMPARE(c.spanAt(1.0), 1);
    QVERIFY(qAbs(c.nearestFraction(QVector3D(2.5f, 1.0f, 0)) - 0.5) < 0.01);
    QVERIFY(qAbs(c.nearestFraction(QVector3D(-3, 0, 0)) - 0.0) < 1e-6);
}

void LineTarget_Test::recentringKeepsTheLineInPlace()
{
    StageTarget t(1);
    t.setPosition(QVector3D(10, 5, 0));
    t.setLinePoints({ QVector3D(0, 0, 0), QVector3D(4, 0, 0) });
    t.makeLine();
    const QVector<QVector3D> before = t.linePointsAround(t.position());
    t.recentreLine();
    const QVector<QVector3D> after = t.linePointsAround(t.position());
    QCOMPARE(after.size(), before.size());
    for (int i = 0; i < after.size(); ++i)
        QVERIFY2(near3(after.at(i), before.at(i), 1e-4f), qPrintable(str(after.at(i))));
    QVERIFY2(near3(t.position(), QVector3D(12, 5, 0), 1e-3f), qPrintable(str(t.position())));
    QVERIFY(near3(LineCurve(t.linePoints()).pointAt(0.5), QVector3D(), 1e-3f));
}

void LineTarget_Test::lightsSpreadEndToEnd()
{
    const LineSpread::Settings s;
    QCOMPARE(s.spread, 5.0);
    const QVector<double> six = LineSpread::fractions(6, s);
    QCOMPARE(six.size(), 6);
    for (int k = 0; k < 6; ++k)
        QVERIFY2(qAbs(six.at(k) - k * 0.2) < 1e-9, qPrintable(QString::number(six.at(k))));
    QCOMPARE(LineSpread::fractions(1, s), QVector<double>({ 0.5 }));
    QVERIFY(LineSpread::fractions(0, s).isEmpty());
}

void LineTarget_Test::lightsAreOrderedAlongTheLine()
{
    const QVector<QVector3D> where = { QVector3D(3, 0, 6), QVector3D(-3, 0, 6),
                                       QVector3D(0, 0, 6), QVector3D(0, 2, 6) };
    // Along +X: left, then the two level in the middle in the order given, then right.
    QCOMPARE(LineSpread::orderAlong(where, QVector3D(-2, 3, 0), QVector3D(2, 3, 0)),
             QVector<int>({ 1, 2, 3, 0 }));
    // The line drawn the other way round: the other way round.
    QCOMPARE(LineSpread::orderAlong(where, QVector3D(2, 3, 0), QVector3D(-2, 3, 0)),
             QVector<int>({ 0, 2, 3, 1 }));
    // No length: as given.
    QCOMPARE(LineSpread::orderAlong(where, QVector3D(1, 1, 0), QVector3D(1, 1, 0)),
             QVector<int>({ 0, 1, 2, 3 }));
}

void LineTarget_Test::aLineSavesAndLoads()
{
    StageTarget t(7);
    t.setName("DS edge");
    t.setPosition(QVector3D(0, 1.5f, 0));
    t.setLinePoints({ QVector3D(-2, 0, 0), QVector3D(0, 0.4f, 0.25f), QVector3D(2, 0, 0) });
    t.makeLine();

    QByteArray xml;
    {
        QXmlStreamWriter w(&xml);
        QVERIFY(t.saveXML(&w));
    }
    QVERIFY2(xml.contains("Kind=\"line\""), xml.constData());
    QCOMPARE(xml.count("<LinePoint"), 3);

    QXmlStreamReader r(xml);
    QVERIFY(r.readNextStartElement());
    StageTarget back(0);
    QVERIFY(back.loadXML(r));
    QCOMPARE(back.id(), quint32(7));
    QVERIFY(back.isLine());
    QCOMPARE(back.linePoints().size(), 3);
    QVERIFY(near3(back.linePoints().at(1), QVector3D(0, 0.4f, 0.25f), 1e-3f));
    QVERIFY(near3(back.position(), QVector3D(0, 1.5f, 0), 1e-3f));

    // A point target writes no points at all.
    StageTarget mark(8);
    xml.clear();
    {
        QXmlStreamWriter w(&xml);
        mark.saveXML(&w);
    }
    QVERIFY(!xml.contains("LinePoint"));
}

void LineTarget_Test::loadingIsForgiving()
{
    const QByteArray xml =
        "<Targets>"
        "<StageTarget ID=\"1\" Name=\"bare\" X=\"1\" Y=\"2\" Z=\"0\" Kind=\"line\"/>"
        "<StageTarget ID=\"2\" Name=\"mark\" X=\"3\" Y=\"4\" Z=\"0\">"
        "<LinePoint X=\"-1\" Y=\"0\" Z=\"0\"/><LinePoint X=\"1\" Y=\"0\" Z=\"0\"/>"
        "</StageTarget>"
        "<After/>"
        "</Targets>";
    QXmlStreamReader r(xml);
    QVERIFY(r.readNextStartElement());          // <Targets>
    QVERIFY(r.readNextStartElement());
    StageTarget a(0);
    QVERIFY(a.loadXML(r));
    QVERIFY(a.isLine());
    QCOMPARE(a.linePoints(), StageTarget::defaultLinePoints());

    QVERIFY(r.readNextStartElement());
    StageTarget b(0);
    QVERIFY(b.loadXML(r));
    QVERIFY(!b.isLine());
    QVERIFY(b.linePoints().isEmpty());
    QVERIFY(near3(b.position(), QVector3D(3, 4, 0)));

    QVERIFY(r.readNextStartElement());
    QCOMPARE(r.name().toString(), QString("After"));

    // An unknown kind (a newer file) still reads as a plain mark.
    QCOMPARE(StageTarget::stringToKind("spiral"), StageTarget::Kind::None);
    QCOMPARE(StageTarget::stringToKind("line"), StageTarget::Kind::Line);
}

static Fixture *makeSpot(Doc *doc, const QString &name)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Spot " + name);
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("2ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt };
    for (int k = 0; k < 2; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(k == 0 ? "Pan" : "Tilt");
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
    fxi->setAddress(quint32(doc->fixtures().size() * 2));
    if (doc->addFixture(fxi) == false)
        return nullptr;
    return fxi;
}

/* Three spots hung 6 m up across the stage, a line target 4 m long 3 m in
   front of them, an Aim look at it and a scene listing the spots out of
   order. */
struct Rig
{
    Doc *doc = nullptr;
    Fixture *left = nullptr, *mid = nullptr, *right = nullptr;
    StageTarget *line = nullptr;
    QLCPalette *aim = nullptr;
    Scene *scene = nullptr;
};

static Rig makeRig(QObject *parent)
{
    Rig rig;
    rig.doc = new Doc(parent);
    MonitorProperties *props = rig.doc->monitorProperties();
    rig.left  = makeSpot(rig.doc, "Left");
    rig.mid   = makeSpot(rig.doc, "Mid");
    rig.right = makeSpot(rig.doc, "Right");
    // (fid, head, linked, pos): X/Y in millimetres, Z in metres.
    props->setFixturePosition(rig.left->id(),  0, 0, QVector3D(-3000, 0, 6));
    props->setFixturePosition(rig.mid->id(),   0, 0, QVector3D(0, 0, 6));
    props->setFixturePosition(rig.right->id(), 0, 0, QVector3D(3000, 0, 6));

    rig.line = props->addStageTarget();
    rig.line->setName("Line");
    rig.line->setPosition(QVector3D(0, 3, 0));
    rig.line->makeLine();

    rig.aim = new QLCPalette(QLCPalette::Aim, rig.doc);
    rig.aim->setName("At the line");
    rig.aim->setStageTargetId(rig.line->id());
    rig.doc->addPalette(rig.aim);

    rig.scene = new Scene(rig.doc);
    rig.scene->setName("Fan");
    rig.scene->addFixture(rig.right->id());
    rig.scene->addFixture(rig.left->id());
    rig.scene->addFixture(rig.mid->id());
    rig.scene->addPalette(rig.aim->id());
    rig.doc->addFunction(rig.scene);
    return rig;
}

void LineTarget_Test::anAimLookSpreadsItsHeadsAlongTheLine()
{
    Rig rig = makeRig(this);
    QVERIFY(rig.left && rig.mid && rig.right);

    const QList<AimSolver::SceneAim> aims = AimSolver::sceneAims(rig.doc, rig.scene);
    QCOMPARE(aims.size(), 3);
    QHash<quint32, QVector3D> at;
    foreach (const AimSolver::SceneAim &a, aims)
    {
        QCOMPARE(a.targetId, rig.line->id());
        at.insert(a.fixtureId, a.point);
    }
    QVERIFY2(near3(at.value(rig.left->id()),  QVector3D(-2, 3, 0)), qPrintable(str(at.value(rig.left->id()))));
    QVERIFY2(near3(at.value(rig.mid->id()),   QVector3D(0, 3, 0)),  qPrintable(str(at.value(rig.mid->id()))));
    QVERIFY2(near3(at.value(rig.right->id()), QVector3D(2, 3, 0)),  qPrintable(str(at.value(rig.right->id()))));

    // Any ordered list of heads works, not only a scene's.
    const QHash<quint32, double> fr = AimSolver::lineFractions(
        rig.doc, rig.line, { rig.mid->id(), rig.right->id() }, LineSpread::Settings());
    QCOMPARE(fr.value(rig.mid->id()), 0.0);
    QCOMPARE(fr.value(rig.right->id()), 1.0);

    delete rig.doc;
}

void LineTarget_Test::thePaletteAimsEachHeadAtItsOwnPoint()
{
    Rig rig = makeRig(this);
    auto pans = [&rig]() {
        QHash<quint32, int> pan;
        foreach (const SceneValue &v, rig.aim->valuesFromFixtures(rig.doc, rig.scene->fixtures(), rig.scene))
            if (v.channel == 0)
                pan.insert(v.fxi, v.value);
        return pan;
    };

    QHash<quint32, int> pan = pans();
    QCOMPARE(pan.size(), 3);
    /* Each head straight at the point in front of it would be the same pan;
       the outer heads are 1 m in from where they hang, so they turn inwards,
       opposite ways. */
    QVERIFY2(pan.value(rig.left->id()) != pan.value(rig.right->id()),
             "the outer heads are aimed the same way");

    // The same heads at a point target all converge.
    rig.line->setKind(StageTarget::Kind::None);
    QHash<quint32, int> conv = pans();
    QVERIFY(conv.value(rig.left->id()) != pan.value(rig.left->id()));

    // Without the owning scene, the list given is the set.
    rig.line->makeLine();
    QHash<quint32, int> alone;
    foreach (const SceneValue &v, rig.aim->valuesFromFixtures(rig.doc, { rig.left->id() }, nullptr))
        if (v.channel == 0)
            alone.insert(v.fxi, v.value);
    QCOMPARE(alone.size(), 1);
    // One head on its own aims at the centre, as the point target did.
    QCOMPARE(alone.value(rig.left->id()), conv.value(rig.left->id()));

    delete rig.doc;
}

void LineTarget_Test::movingTheTargetMovesTheLine()
{
    Rig rig = makeRig(this);
    MonitorProperties *props = rig.doc->monitorProperties();
    rig.line->setPosition(QVector3D(1, 4, 0.5f));
    const QVector<QVector3D> world = props->targetLinePoints(rig.line);
    QCOMPARE(world.size(), 2);
    QVERIFY(near3(world.at(0), QVector3D(-1, 4, 0.5f)));
    QVERIFY(near3(world.at(1), QVector3D(3, 4, 0.5f)));
    // Its single point -- what a follow spot or an old build uses -- is the centre.
    QVERIFY(near3(props->targetAimPoint(rig.line), QVector3D(1, 4, 0.5f)));
    // An aim height does not lift a line off the heights it was drawn at.
    rig.line->setAimHeightOverride(1.4f);
    QVERIFY(near3(props->targetAimPoint(rig.line), QVector3D(1, 4, 0.5f)));
    delete rig.doc;
}

QTEST_MAIN(LineTarget_Test)
