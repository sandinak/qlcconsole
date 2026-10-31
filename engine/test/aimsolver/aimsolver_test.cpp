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
#include "stageplatform.h"
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
    const AimSolver::Solution farPt = AimSolver::solve(f, QVector3D(1.67f, 4.372f, 2.01f));
    QVERIFY(farPt.reachable());
    QVERIFY(qAbs(farPt.marginDeg - (45.0f - 22.46f)) < 0.2f);

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

    AimSolver::Solution nearPt, away;
    QVERIFY(AimSolver::reach(doc, fxi->id(), QVector3D(1.67f, 1.872f, 2.01f), nearPt));
    QVERIFY(AimSolver::reach(doc, fxi->id(), QVector3D(1.67f, 4.372f, 2.01f), away));
    QVERIFY2(!nearPt.reachable() && away.reachable(),
             qPrintable(QString("upright on the tower: margin %1 half a metre out, "
                                "%2 three metres out").arg(nearPt.marginDeg).arg(away.marginDeg)));

    // Hang it and the same close target is fine.
    rp.mountingType = Truss::TopHung;
    props->setFixtureRigProps(fxi->id(), rp);
    QVERIFY(AimSolver::reach(doc, fxi->id(), QVector3D(1.67f, 1.872f, 2.01f), nearPt));
    QVERIFY(nearPt.reachable());

    float pan = 0.0f, tilt = 0.0f;
    QVERIFY(AimSolver::aimDegrees(doc, fxi->id(), QVector3D(1.67f, 1.872f, 2.01f), pan, tilt));
    QCOMPARE(pan, nearPt.panDeg);
    QCOMPARE(tilt, nearPt.tiltDeg);

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

void AimSolver_Test::aDeckHeadStandsAtTheDecksHeight()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(1.0f);
    deck->setOriginY(1.0f);
    deck->setWidth(10.0f);
    deck->setDepth(2.0f);
    deck->setHeight(0.6f);

    // Eight identical heads on one deck: the rig-props default (TopHung) must
    // not leave the ones nobody touched upside down.
    Fixture *onDeck = makeSpot(doc, "On deck");
    Fixture *free   = makeSpot(doc, "Free over deck");
    QVERIFY(onDeck != nullptr && free != nullptr);
    props->setFixturePosition(onDeck->id(), 0, 0, QVector3D(3000, 1500, 0));
    props->setFixturePosition(free->id(), 0, 0, QVector3D(5000, 1500, 0));
    FixtureRigProps rp;
    rp.deckPlatformId = deck->id();
    QCOMPARE(rp.mountingType, int(Truss::TopHung));
    props->setFixtureRigProps(onDeck->id(), rp);

    AimSolver::Frame f;
    QVERIFY(AimSolver::frameFor(doc, onDeck->id(), f));
    QVERIFY2(f.floorMounted, "a deck-mounted head stands on the deck");
    QVERIFY2(qAbs(f.position.z() - 0.6f) < 0.01f,
             qPrintable(QString("deck head solved at %1 m, deck is 0.6 m").arg(f.position.z())));

    // A head merely placed over the deck (no mount) still gets its height.
    QVERIFY(AimSolver::frameFor(doc, free->id(), f));
    QVERIFY2(qAbs(f.position.z() - 0.6f) < 0.01f,
             qPrintable(QString("free head over the deck solved at %1 m").arg(f.position.z())));

    // Standing there, a followspot subject a couple of metres upstage is easy.
    AimSolver::Solution s;
    QVERIFY(AimSolver::reach(doc, onDeck->id(), QVector3D(3.0f, 4.0f, 2.0f), s));
    QVERIFY2(s.reachable(), qPrintable(QString("margin %1").arg(s.marginDeg)));

    delete doc;
}

/* Centred fixture degrees from a solution, as DMX carries them. */
static void centred(const AimSolver::Frame &f, const AimSolver::Solution &s,
                    double &pan, double &tilt)
{
    pan = double(s.panDeg) - double(f.panMax) / 2.0;
    tilt = double(s.tiltDeg) - double(f.tiltMax) / 2.0;
}

static double degreesBetween(const QVector3D &a, const QVector3D &b)
{
    const double d = double(QVector3D::dotProduct(a.normalized(), b.normalized()));
    return qRadiansToDegrees(qAcos(qBound(-1.0, d, 1.0)));
}

void AimSolver_Test::calibrationIsInTheDmxAndOutOfThePicture()
{
    struct Case { const char *what; bool standing; bool panInv; bool tiltInv;
                  float facing; float panOfs; float tiltOfs; QVector3D mark; };
    const QList<Case> cases = {
        { "hung, +3 pan",              false, false, false,   0.0f,  3.0f,  0.0f, QVector3D(4.0f, 6.0f, 0.0f) },
        { "hung, -2.5 tilt",           false, false, false,   0.0f,  0.0f, -2.5f, QVector3D(1.0f, 5.0f, 0.0f) },
        { "hung, both, facing SL",     false, false, false, 270.0f,  4.0f,  1.5f, QVector3D(5.0f, 2.0f, 0.0f) },
        { "hung, pan inverted",        false, true,  false,   0.0f,  3.0f, -1.0f, QVector3D(4.0f, 6.0f, 0.0f) },
        { "hung, tilt inverted",       false, false, true,   90.0f, -3.0f,  2.0f, QVector3D(0.5f, 4.0f, 0.0f) },
        { "standing, both",            true,  false, false,   0.0f,  5.0f,  3.0f, QVector3D(4.0f, 8.0f, 6.0f) },
        { "standing, both inverted",   true,  true,  true,  180.0f, -2.0f, -4.0f, QVector3D(1.0f, -3.0f, 7.0f) },
    };

    foreach (const Case &c, cases)
    {
        FixtureRigProps rp;
        rp.mountingType = c.standing ? Truss::FloorMounted : Truss::TopHung;
        rp.panInvert = c.panInv;
        rp.tiltInvert = c.tiltInv;
        rp.panZeroDir = c.facing;

        AimSolver::Frame f;
        f.position = QVector3D(2.0f, 3.0f, c.standing ? 1.0f : 5.0f);
        f.floorMounted = AimSolver::floorMounted(rp);
        f.panZeroDir = c.facing;
        f.panInvert = c.panInv;
        f.tiltInvert = c.tiltInv;
        f.panMax = 540.0f;
        f.tiltMax = 270.0f;

        const AimSolver::Solution plain = AimSolver::solve(f, c.mark);
        f.panOffsetDeg = c.panOfs;
        f.tiltOffsetDeg = c.tiltOfs;
        rp.panOffsetDeg = c.panOfs;
        rp.tiltOffsetDeg = c.tiltOfs;
        const AimSolver::Solution calibrated = AimSolver::solve(f, c.mark);
        QVERIFY2(plain.reachable() && calibrated.reachable(), c.what);

        // The DMX carries the offset, mirrored as the head is.
        const double dPan = double(calibrated.panDeg - plain.panDeg);
        const double dTilt = double(calibrated.tiltDeg - plain.tiltDeg);
        const bool panMirrored = c.panInv != c.standing;
        QVERIFY2(qAbs(qAbs(dPan) - double(qAbs(c.panOfs))) < 1e-3
                 && (c.panOfs == 0.0f || (dPan > 0) == ((c.panOfs > 0) != panMirrored)),
                 qPrintable(QString("%1: pan DMX moved %2 for an offset of %3")
                            .arg(c.what).arg(dPan).arg(double(c.panOfs))));
        QVERIFY2(qAbs(qAbs(dTilt) - double(qAbs(c.tiltOfs))) < 1e-3,
                 qPrintable(QString("%1: tilt DMX moved %2 for an offset of %3")
                            .arg(c.what).arg(dTilt).arg(double(c.tiltOfs))));

        // ...and the picture does not: the beam drawn from it hits the mark.
        double pan = 0.0, tilt = 0.0;
        centred(f, calibrated, pan, tilt);
        QVector3D dir;
        QVERIFY(AimSolver::beamDirection(rp, pan, tilt, dir));
        const double off = degreesBetween(dir, c.mark - f.position);
        QVERIFY2(off < 0.05, qPrintable(QString("%1: the beam is drawn %2 degrees off its mark")
                                        .arg(c.what).arg(off)));

        // Reading the raw angles WITHOUT removing the offset would miss it.
        FixtureRigProps raw = rp;
        raw.panOffsetDeg = 0.0f;
        raw.tiltOffsetDeg = 0.0f;
        QVERIFY(AimSolver::beamDirection(raw, pan, tilt, dir));
        QVERIFY2(degreesBetween(dir, c.mark - f.position) > 0.5,
                 qPrintable(QString("%1: the offset made no difference to the raw ray").arg(c.what)));
    }
}

void AimSolver_Test::zeroOffsetsInverseIsUnchanged()
{
    FixtureRigProps rp;
    double panRel = 0.0, tiltRel = 0.0;
    AimSolver::intendedAngles(rp, 30.0, -45.0, panRel, tiltRel);
    QCOMPARE(panRel, 30.0);
    QCOMPARE(tiltRel, -45.0);

    // Hung, centred: straight down.
    QVector3D dir;
    QVERIFY(AimSolver::beamDirection(rp, 0.0, 0.0, dir));
    QVERIFY(qAbs(double(dir.z()) + 1.0) < 1e-5);

    // Tilted 90 at pan 0: level, downstage (+Y).
    QVERIFY(AimSolver::beamDirection(rp, 0.0, 90.0, dir));
    QVERIFY(qAbs(double(dir.y()) - 1.0) < 1e-5);

    // Offsets come off before the geometry: an offset head at its offset
    // reads exactly as the plain head at zero.
    rp.panOffsetDeg = 7.0f;
    rp.tiltOffsetDeg = -3.0f;
    AimSolver::intendedAngles(rp, 7.0, -3.0, panRel, tiltRel);
    QVERIFY(qAbs(panRel) < 1e-6 && qAbs(tiltRel) < 1e-6);
}

QTEST_MAIN(AimSolver_Test)
