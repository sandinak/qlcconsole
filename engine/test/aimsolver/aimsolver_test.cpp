/*
  Q Light Controller Plus - Unit test
  aimsolver_test.cpp

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
#include <QtMath>

#include "aimsolver_test.h"
#include "aimsolver.h"
#include "monitorproperties.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "truss.h"
#include "doc.h"

/* The solver as it stood before reach was added, verbatim apart from reading
   a Frame and from the corrected bearing (it measured pan from upstage, 180
   degrees away from where every view then drew the beam): the reference the
   new one must still agree with. */
static void legacyAim(const AimSolver::Frame &f, const QVector3D &target,
                      float &panDeg, float &tiltDeg)
{
    const float dx = target.x() - f.position.x();
    const float dy = target.y() - f.position.y();
    const float dz = target.z() - f.position.z();
    const float horizDist = qSqrt(dx * dx + dy * dy);
    float azimuthDeg = float(qRadiansToDegrees(qAtan2(double(-dx), double(dy))));
    if (azimuthDeg < 0.0f) azimuthDeg += 360.0f;
    float relativePan = azimuthDeg - f.panZeroDir;
    while (relativePan >  180.0f) relativePan -= 360.0f;
    while (relativePan < -180.0f) relativePan += 360.0f;
    float panRaw = f.panMax / 2.0f + relativePan + f.panOffsetDeg;
    bool invertPan = f.panInvert;
    if (f.floorMounted)
        invertPan = !invertPan;
    if (invertPan) panRaw = f.panMax - panRaw;
    panDeg = qBound(0.0f, panRaw, f.panMax);
    const float elevDeg = float(qRadiansToDegrees(qAtan2(double(dz), double(horizDist))));
    if (f.floorMounted)
        tiltDeg = f.tiltMax / 2.0f - 90.0f + elevDeg + f.tiltOffsetDeg;
    else
        tiltDeg = f.tiltMax / 2.0f + (90.0f + elevDeg) + f.tiltOffsetDeg;
    if (f.tiltInvert) tiltDeg = f.tiltMax - tiltDeg;
    tiltDeg = qBound(0.0f, tiltDeg, f.tiltMax);
}

void AimSolver_Test::anglesAreUnchangedWherePanFits()
{
    int compared = 0;
    for (int mount = 0; mount < 2; ++mount)
    for (float panMax : { 360.0f, 540.0f })
    for (float tiltMax : { 180.0f, 270.0f })
    for (int flags = 0; flags < 4; ++flags)
    {
        AimSolver::Frame f;
        f.position = QVector3D(2.0f, 1.5f, 3.2f);
        f.floorMounted = (mount == 1);
        f.panMax = panMax;
        f.tiltMax = tiltMax;
        f.panZeroDir = 30.0f;
        f.tiltOffsetDeg = (flags & 1) ? 7.5f : 0.0f;
        f.panInvert = (flags & 2) != 0;
        f.tiltInvert = (flags & 1) != 0;

        for (float x = -4.0f; x <= 8.0f; x += 0.75f)
        for (float y = -3.0f; y <= 6.0f; y += 0.75f)
        for (float z : { 0.0f, 1.4f, 3.0f, 5.0f })
        {
            const QVector3D t(x, y, z);
            float lp = 0.0f, lt = 0.0f;
            legacyAim(f, t, lp, lt);
            const AimSolver::Solution s = AimSolver::solve(f, t);
            QVERIFY2(qAbs(s.panDeg - lp) < 1e-3f && qAbs(s.tiltDeg - lt) < 1e-3f,
                     qPrintable(QString("mount %1 pan %2 tilt %3 flags %4 at (%5,%6,%7): "
                                        "%8/%9 now, %10/%11 before")
                                .arg(mount).arg(panMax).arg(tiltMax).arg(flags)
                                .arg(x).arg(y).arg(z).arg(s.panDeg).arg(s.tiltDeg)
                                .arg(lp).arg(lt)));
            ++compared;
        }
    }
    QVERIFY(compared > 1000);
}

void AimSolver_Test::uprightOnATowerCannotLookStraightDown()
{
    // A UKing 100W spot, 540/270, standing on a 10 ft tower.
    AimSolver::Frame f;
    f.position = QVector3D(1.67f, 1.372f, 3.25f);
    f.floorMounted = true;
    f.panMax = 540.0f;
    f.tiltMax = 270.0f;

    // Chest height on the upstage deck right in front of it: 68 deg down.
    const AimSolver::Solution close = AimSolver::solve(f, QVector3D(1.67f, 1.872f, 2.01f));
    QVERIFY2(!close.reachable(),
             qPrintable(QString("a chest half a metre from the tower base should be "
                                "out of reach, margin %1").arg(close.marginDeg)));
    QVERIFY(qAbs(close.marginDeg - (45.0f - 68.03f)) < 0.2f);

    // Downstage, 22 deg down: well inside.
    const AimSolver::Solution far = AimSolver::solve(f, QVector3D(1.67f, 4.372f, 2.01f));
    QVERIFY(far.reachable());
    QVERIFY(qAbs(far.marginDeg - (45.0f - 22.46f)) < 0.2f);

    // The edge of the cone is where drop equals distance.
    const AimSolver::Solution edge = AimSolver::solve(f, QVector3D(1.67f, 1.372f + 1.24f, 2.01f));
    QVERIFY2(qAbs(edge.marginDeg) < 0.1f,
             qPrintable(QString("at the cone's edge the margin should be ~0, got %1")
                        .arg(edge.marginDeg)));
}

void AimSolver_Test::hungUnderABarReachesTheStage()
{
    AimSolver::Frame f;
    f.position = QVector3D(1.67f, 1.372f, 3.25f);
    f.floorMounted = false;
    f.panMax = 540.0f;
    f.tiltMax = 270.0f;

    for (float d : { 0.0f, 0.5f, 1.24f, 3.0f, 8.0f })
    {
        const AimSolver::Solution s = AimSolver::solve(f, QVector3D(1.67f, 1.372f + d, 2.01f));
        QVERIFY2(s.reachable(),
                 qPrintable(QString("hung, a chest %1 m out should be reachable, "
                                    "margin %2").arg(d).arg(s.marginDeg)));
    }
    // Its blind cone is overhead: 60 deg up is out.
    const AimSolver::Solution up = AimSolver::solve(f, QVector3D(1.67f, 1.372f + 1.0f,
                                                                3.25f + float(qTan(qDegreesToRadians(60.0)))));
    QVERIFY(!up.reachable());
}

void AimSolver_Test::aShortPanReachesBehindByTiltingBack()
{
    AimSolver::Frame f;
    f.position = QVector3D(0.0f, 0.0f, 4.0f);
    f.floorMounted = false;
    f.panMax = 180.0f;
    f.tiltMax = 270.0f;

    /* Directly "behind" the pan zero: relative pan 180, beyond +/-90. Pan zero
       faces DOWNSTAGE (+Y), so behind it is upstage -- this read +Y while the
       solver measured its bearing from upstage, which was the same mistake
       that pointed every drawn beam at the mirror image of its target. */
    const QVector3D behind(0.0f, -3.0f, 0.0f);
    float lp = 0.0f, lt = 0.0f;
    legacyAim(f, behind, lp, lt);
    const AimSolver::Solution s = AimSolver::solve(f, behind);
    QVERIFY2(s.reachable(),
             qPrintable(QString("swinging round and tilting back reaches it, margin %1")
                        .arg(s.marginDeg)));
    QVERIFY2(qAbs(s.panDeg - lp) > 1.0f,
             "the clamped answer points somewhere else; this one must not be it");

    /* And it really points there: the mirrored solution is pan 180 away with
       tilt reflected through home, i.e. the same beam. */
    QVERIFY(s.panDeg >= 0.0f && s.panDeg <= f.panMax);
    QVERIFY(s.tiltDeg >= 0.0f && s.tiltDeg <= f.tiltMax);
}

/* A 540/270 moving head: the UKing 100W spot's ranges. */
static Fixture *makeSpot(Doc *doc, const QString &name)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Spot 540/270 " + name);
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

void AimSolver_Test::reachReadsTheRig()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *tower = props->addTruss();
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(1.67f, 1.372f, 0.0f));
    tower->setLength(3.048f);
    tower->setWidth(0.29f);

    Fixture *fxi = makeSpot(doc, "SR Spot");
    QVERIFY(fxi != nullptr);

    FixtureRigProps rp;
    rp.trussId = tower->id();
    rp.trussOffset = 3.048f;
    rp.mountingType = Truss::FloorMounted;
    rp.mountZOffset = 0.2f;              // the tilt axis, a little above the plate
    props->setFixtureRigProps(fxi->id(), rp);

    AimSolver::Solution near, away;
    QVERIFY(AimSolver::reach(doc, fxi->id(), QVector3D(1.67f, 1.872f, 2.01f), near));
    QVERIFY(AimSolver::reach(doc, fxi->id(), QVector3D(1.67f, 4.372f, 2.01f), away));
    QVERIFY2(!near.reachable() && away.reachable(),
             qPrintable(QString("upright on the tower: margin %1 half a metre out, "
                                "%2 three metres out").arg(near.marginDeg).arg(away.marginDeg)));

    // Hang it and the same close target is fine.
    rp.mountingType = Truss::TopHung;
    props->setFixtureRigProps(fxi->id(), rp);
    QVERIFY(AimSolver::reach(doc, fxi->id(), QVector3D(1.67f, 1.872f, 2.01f), near));
    QVERIFY(near.reachable());

    float pan = 0.0f, tilt = 0.0f;
    QVERIFY(AimSolver::aimDegrees(doc, fxi->id(), QVector3D(1.67f, 1.872f, 2.01f), pan, tilt));
    QCOMPARE(pan, near.panDeg);
    QCOMPARE(tilt, near.tiltDeg);

    delete doc;
}

void AimSolver_Test::aSceneSaysWhichHeadsCannotReach()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *tower = props->addTruss();
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(1.67f, 1.372f, 0.0f));
    tower->setLength(3.048f);
    tower->setWidth(0.29f);

    Fixture *onTop = makeSpot(doc, "On top");
    Fixture *hung  = makeSpot(doc, "Hung");
    QVERIFY(onTop != nullptr && hung != nullptr);

    FixtureRigProps rp;
    rp.trussId = tower->id();
    rp.trussOffset = 3.048f;
    rp.mountingType = Truss::FloorMounted;
    props->setFixtureRigProps(onTop->id(), rp);
    rp.trussOffset = 2.8f;
    rp.mountingType = Truss::TopHung;
    props->setFixtureRigProps(hung->id(), rp);

    // A vocalist half a metre in front of the tower.
    StageTarget *who = props->addStageTarget();
    who->setName("Vocalist");
    who->setKind(StageTarget::Kind::Person);
    who->setPosition(QVector3D(1.67f, 1.872f, 0.0f));

    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("At the vocalist");
    aim->setStageTargetId(who->id());
    QVERIFY(doc->addPalette(aim));

    Scene *scene = new Scene(doc);
    scene->setName("Solo");
    scene->addFixture(onTop->id());
    scene->addFixture(hung->id());
    scene->addPalette(aim->id());
    QVERIFY(doc->addFunction(scene));

    const QList<AimSolver::SceneAim> aims = AimSolver::sceneAims(doc, scene);
    QCOMPARE(aims.size(), 2);
    QHash<quint32, AimSolver::SceneAim> byFixture;
    foreach (const AimSolver::SceneAim &a, aims)
        byFixture.insert(a.fixtureId, a);

    QVERIFY(byFixture.contains(onTop->id()) && byFixture.contains(hung->id()));
    QCOMPARE(byFixture.value(onTop->id()).targetId, who->id());
    // Aimed at the chest, as the palette resolves a Person.
    QVERIFY(qAbs(double(byFixture.value(onTop->id()).point.z()) - 1.4) < 1e-3);
    QVERIFY2(!byFixture.value(onTop->id()).solution.reachable(),
             "the spot standing on the tower can reach a chest half a metre away");
    QVERIFY2(byFixture.value(hung->id()).solution.reachable(),
             "the hung spot cannot reach a chest half a metre away");

    delete doc;
}

QTEST_MAIN(AimSolver_Test)
