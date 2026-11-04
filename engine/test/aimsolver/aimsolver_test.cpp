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
#include "qlcfixturedefcache.h"
#include "scenevalue.h"
#include <QVector2D>
#include "linecurve.h"
#include "righeads.h"
#include "areacover.h"

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

/****************************************************************************
 * Round trip: aim, DMX, and back to a beam
 ****************************************************************************/

namespace
{
enum class Def { Spot16, Spot8, TwoArm, FourArm };
enum class Mount { Hung, HungOnTruss, Floor, Deck, OnTrussTop };
enum class Aim { Point, Person, Line, Area };

/* One rig and what it aims at. Fixtures stand @c spacing metres apart along
   X from @c at; a target is absolute, a line's ends relative to its centre. */
struct AimRow
{
    const char *name;
    Def def;
    Mount mount;
    QVector3D at;               // metres (Z: height for Hung; ignored otherwise)
    int count;
    float panZero;
    float panOffset, tiltOffset;
    bool panInvert, tiltInvert;
    Aim aim;
    QVector3D target;           // the point, the person's feet, the line's centre
    QVector3D lineEnd;          // a line: its last end from the centre (first = -this); an area: its size (x, y)
    double spread;              // a line: Spread (NaN = the look's default, +5)
    int group = 1;              // a line: Grouping (lights per group)
    int cross = LineSpread::SwapSides;
    float yaw = 0.0f;           // the body turned in plan (studioYaw)
};

const double kDefault = qQNaN();

QVector3D firstEnd(const AimRow &r)
{
    return QVector3D(-r.lineEnd.x(), r.lineEnd.y(), r.lineEnd.z());
}
const float kSpacing = 1.5f;

/* Add a row here for a new fixture shape or rig: everything below runs on it. */
const AimRow kRows[] = {
    { "hung spot at a point",           Def::Spot16, Mount::Hung,       { 0, 0, 6 },     1,  0,  0,  0, false, false, Aim::Point,  { 1.5f, 4, 0 },  {}, kDefault },
    { "hung on a truss, at a person",   Def::Spot16, Mount::HungOnTruss,{ -3, 1, 5.5f }, 3,  0,  0,  0, false, false, Aim::Person, { 0, 5, 0 },     {}, kDefault },
    { "hung, turned, inverted, calibrated", Def::Spot16, Mount::Hung,   { 2, 1, 6 },     2, 90,  7, -4, true,  true,  Aim::Point,  { -2, 3, 0.5f }, {}, kDefault },
    { "floor, no height, at a person",  Def::Spot16, Mount::Floor,      { -1, 0, 0 },    2,  0,  0,  0, false, false, Aim::Person, { 0, 6, 0 },     {}, kDefault },
    { "deck, facing upstage",           Def::Spot16, Mount::Deck,       { 0, 3, 0 },     2, 180, 0,  0, false, false, Aim::Point,  { 1, -2, 2.5f }, {}, kDefault },
    { "standing on a truss top",        Def::Spot16, Mount::OnTrussTop, { 0, 0, 2 },     1,  0,  0,  0, false, false, Aim::Person, { 2, 6, 0 },     {}, kDefault },
    { "8-bit spot hung",                Def::Spot8,  Mount::Hung,       { -2, 0, 6 },    2,  0,  0,  0, false, false, Aim::Point,  { 3, 5, 0 },     {}, kDefault },
    { "hung spots on a line, Spread +5",Def::Spot16, Mount::Hung,       { -4, 0, 6 },    4,  0,  0,  0, false, false, Aim::Line,   { 0, 6, 0 },     { 4, 0, 0 }, 5 },
    { "hung spots on a line, Spread 0", Def::Spot16, Mount::Hung,       { -4, 0, 6 },    4,  0,  0,  0, false, false, Aim::Line,   { 0, 6, 0 },     { 4, 0, 0 }, 0 },
    { "hung spots on a line, crossed",  Def::Spot16, Mount::Hung,       { -4, 0, 6 },    4,  0,  0,  0, false, false, Aim::Line,   { 0, 6, 0 },     { 4, 0, 0 }, -3 },
    { "two-arm beams on a deck, line",  Def::TwoArm, Mount::Deck,       { -5, 3.6f, 0 }, 8,  0,  0,  0, false, false, Aim::Line,   { 0, 9.6f, 0 },  { 4.3f, -1.2f, 0.37f }, kDefault },
    { "two-arm beams on a deck, Spread 2", Def::TwoArm, Mount::Deck,    { -3, 3.6f, 0 }, 4,  0,  0,  0, false, false, Aim::Line,   { 0, 9.6f, 0 },  { 4.3f, -1.2f, 0.37f }, 2 },
    { "two-arm beam on the floor, person", Def::TwoArm, Mount::Floor,   { 1, 0, 0 },     1,  0,  0,  0, false, false, Aim::Person, { -2, 5, 0 },    {}, kDefault },
    { "two-arm beams hung, point",      Def::TwoArm, Mount::Hung,       { -1, 0, 5 },    2,  0,  0,  0, false, false, Aim::Point,  { 2, 4, 0 },     {}, kDefault },
    // Each arm is its own light: its own origin, angle and place.
    { "two-arm beam at a near point",   Def::TwoArm, Mount::Hung,       { 0, 0, 3 },     1,  0,  0,  0, false, false, Aim::Point,  { 0.3f, 2, 0 },  {}, kDefault },
    { "two-arm beam turned 90, point",  Def::TwoArm, Mount::Hung,       { 0, 0, 3 },     1, 90,  0,  0, false, false, Aim::Point,  { 2, 0.3f, 0 },  {}, kDefault, 1, LineSpread::SwapSides, 90 },
    { "two-arm beams, line in pairs, mirrored", Def::TwoArm, Mount::Deck, { -5, 3.6f, 0 }, 8, 0, 0, 0, false, false, Aim::Line, { 0, 9.6f, 0 }, { 4.3f, -1.2f, 0.37f }, -5, 2, LineSpread::Mirror },
    { "two-arm beams, line in pairs, swapped",  Def::TwoArm, Mount::Deck, { -5, 3.6f, 0 }, 8, 0, 0, 0, false, false, Aim::Line, { 0, 9.6f, 0 }, { 4.3f, -1.2f, 0.37f }, -5, 2, LineSpread::SwapSides },
    { "four-arm bar hung, point",       Def::FourArm, Mount::Hung,      { 0, 0, 4 },     2,  0,  0,  0, false, false, Aim::Point,  { 1, 3, 0 },     {}, kDefault },
    { "four-arm bars hung, line",       Def::FourArm, Mount::Hung,      { -3, 0, 4 },    3,  0,  0,  0, false, false, Aim::Line,   { 0, 5, 0 },     { 5, 0, 0 }, kDefault },
    // An area: each light (each arm) its own zone, at subject height (AREA_COVER.md).
    { "hung spots over an area",        Def::Spot16, Mount::Hung,       { -3, 1, 6 },    4,  0,  0,  0, false, false, Aim::Area,   { 0, 5, 0 },     { 6, 3, 0 }, kDefault },
    { "two-arm beams on a deck, area",  Def::TwoArm, Mount::Deck,       { -5, 3.6f, 0 }, 8,  0,  0,  0, false, false, Aim::Area,   { 0, 6.5f, 0 },  { 9.75f, 3.66f, 0 }, kDefault },
    { "four-arm bars hung, area",       Def::FourArm, Mount::Hung,      { -2, 0, 5 },    2,  0,  0,  0, false, false, Aim::Area,   { 0, 4, 0 },     { 4, 2, 0 }, kDefault },
};

/* A pan/tilt spot, 540/270, coarse and fine (16-bit) or coarse only. */
Fixture *addSpot(Doc *doc, const QString &name, bool fine, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel(QString("Spot %1 %2").arg(fine ? 16 : 8).arg(name));
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("ch");
    QLCFixtureHead head;
    int n = 0;
    for (QLCChannel::Group g : { QLCChannel::Pan, QLCChannel::Tilt })
        for (QLCChannel::ControlByte b : { QLCChannel::MSB, QLCChannel::LSB })
        {
            if (b == QLCChannel::LSB && !fine)
                continue;
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("%1 %2").arg(g == QLCChannel::Pan ? "Pan" : "Tilt")
                                         .arg(b == QLCChannel::MSB ? "" : "fine"));
            ch->setGroup(g);
            ch->setControlByte(b);
            def->addChannel(ch);
            mode->insertChannel(ch, quint32(n));
            head.addChannel(quint32(n));
            ++n;
        }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    mode->insertHead(-1, head);
    def->addMode(mode);

    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setAddress(address);
    return doc->addFixture(fxi) ? fxi : nullptr;
}

/* The Junman two-arm LED beam (a copy of the user's definition): two heads,
   each pan pointing with DMX 0-127 ("indexed, a single 360 CCW sweep") and
   spinning above it; 360/180, coarse only. */
Fixture *addTwoArm(Doc *doc, const QString &name, quint32 address)
{
    QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef("Junman", "Two Arm LED Beam");
    if (def == nullptr)
    {
        if (!doc->fixtureDefCache()->loadQXF(QString(AIMSOLVER_TEST_DIR) + "/Junman-Two-Arm-LED-Beam.qxf"))
            return nullptr;
        def = doc->fixtureDefCache()->fixtureDef("Junman", "Two Arm LED Beam");
    }
    QLCFixtureMode *mode = def != nullptr ? def->mode("15 Channel") : nullptr;
    if (mode == nullptr)
        return nullptr;
    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setAddress(address);
    return doc->addFixture(fxi) ? fxi : nullptr;
}

/* A bar of @p arms independently aimed heads (each its own 16-bit pan and
   8-bit tilt, 540/270), 1 m wide, Layout arms x 1 -- no such fixture in the
   library yet, so made here. */
Fixture *addMultiArm(Doc *doc, const QString &name, int arms, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel(QString("Bar %1 arms %2").arg(arms).arg(name));
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("ch");
    int n = 0;
    for (int a = 0; a < arms; ++a)
    {
        QLCFixtureHead head;
        const QPair<QLCChannel::Group, QLCChannel::ControlByte> chs[] = {
            { QLCChannel::Pan, QLCChannel::MSB }, { QLCChannel::Pan, QLCChannel::LSB },
            { QLCChannel::Tilt, QLCChannel::MSB } };
        for (const auto &c : chs)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("%1 %2%3").arg(c.first == QLCChannel::Pan ? "Pan" : "Tilt")
                                          .arg(a + 1).arg(c.second == QLCChannel::LSB ? " fine" : ""));
            ch->setGroup(c.first);
            ch->setControlByte(c.second);
            def->addChannel(ch);
            mode->insertChannel(ch, quint32(n));
            head.addChannel(quint32(n));
            ++n;
        }
        mode->insertHead(-1, head);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    ph.setWidth(1000);
    ph.setHeight(200);
    ph.setDepth(200);
    ph.setLayoutSize(QSize(arms, 1));
    mode->setPhysical(ph);
    def->addMode(mode);

    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setAddress(address);
    return doc->addFixture(fxi) ? fxi : nullptr;
}

/* The pan of head @p head as its DMX reads back, degrees across its range. */
double panDegrees(const Fixture *fx, const QByteArray &own, int head)
{
    const QLCFixtureMode *mode = fx->fixtureMode();
    const QLCFixtureHead &h = mode->heads().at(head);
    const quint32 msb = h.channelNumber(QLCChannel::Pan, QLCChannel::MSB);
    const quint32 lsb = h.channelNumber(QLCChannel::Pan, QLCChannel::LSB);
    double v = uchar(own.at(int(msb)));
    if (lsb != QLCChannel::invalid())
        v += uchar(own.at(int(lsb))) / 256.0;
    return Fixture::positionFraction(mode->channel(msb), v, lsb != QLCChannel::invalid())
           * mode->physical().focusPanMax();
}

/* How far, at most, a head's DMX can leave its beam from the solved angle:
   per axis, nothing worth the name with a fine byte, half a step where the
   coarse byte is rounded (an indexed band), else a whole step (stock QLC+
   truncates). */
double dmxSlackDeg(const Fixture *fx, QLCChannel::Group group)
{
    const QLCFixtureMode *mode = fx->fixtureMode();
    const QLCFixtureHead &h = mode->heads().first();
    const quint32 msb = h.channelNumber(group, QLCChannel::MSB);
    if (msb == QLCChannel::invalid())
        return 0.0;
    const double span = group == QLCChannel::Pan ? mode->physical().focusPanMax()
                                                 : mode->physical().focusTiltMax();
    if (h.channelNumber(group, QLCChannel::LSB) != QLCChannel::invalid())
        return span / 65535.0;
    uchar lo = 0, hi = 255;
    const bool band = Fixture::positionBand(mode->channel(msb), lo, hi);
    const double step = span / double(hi - lo);
    return band ? step / 2.0 : step;
}

/* The closest the ray from @p from along @p dir comes to @p p, metres; -1
   when @p p is behind it. */
double missBy(const QVector3D &from, const QVector3D &dir, const QVector3D &p)
{
    const float along = QVector3D::dotProduct(p - from, dir);
    if (along <= 0.0f)
        return -1.0;
    return double((from + dir * along - p).length());
}
}

void AimSolver_Test::aimedHeadsLandOnTheirTarget_data()
{
    QTest::addColumn<int>("row");
    for (int i = 0; i < int(sizeof(kRows) / sizeof(kRows[0])); ++i)
        QTest::newRow(kRows[i].name) << i;
}

void AimSolver_Test::aimedHeadsLandOnTheirTarget()
{
    QFETCH(int, row);
    const AimRow &r = kRows[row];

    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *truss = nullptr;
    if (r.mount == Mount::HungOnTruss || r.mount == Mount::OnTrussTop)
    {
        truss = props->addTruss();
        truss->setType(Truss::Horizontal);
        truss->setOrigin(r.at);
        truss->setLength(12.0f);
        truss->setWidth(0.29f);
    }
    StagePlatform *deck = nullptr;
    if (r.mount == Mount::Deck)
    {
        deck = props->addPlatform();
        deck->setOriginX(r.at.x() - 1.0f);
        deck->setOriginY(r.at.y() - 0.6f);
        deck->setWidth(kSpacing * float(r.count) + 2.0f);
        deck->setDepth(1.2f);
        deck->setHeight(0.61f);
    }

    QList<Fixture *> fixtures;
    quint32 address = 0;
    for (int i = 0; i < r.count; ++i)
    {
        const QString name = QString("%1 %2").arg(r.name).arg(i + 1);
        Fixture *fx = r.def == Def::TwoArm ? addTwoArm(doc, name, address)
                    : r.def == Def::FourArm ? addMultiArm(doc, name, 4, address)
                                            : addSpot(doc, name, r.def == Def::Spot16, address);
        QVERIFY2(fx != nullptr, qPrintable(name));
        address += fx->channels();
        fixtures << fx;

        const QVector3D at = r.at + QVector3D(kSpacing * float(i), 0, 0);
        FixtureRigProps rp;
        rp.panZeroDir = r.panZero;
        rp.panOffsetDeg = r.panOffset;
        rp.tiltOffsetDeg = r.tiltOffset;
        rp.panInvert = r.panInvert;
        rp.tiltInvert = r.tiltInvert;
        rp.studioYaw = r.yaw;
        switch (r.mount)
        {
            case Mount::Hung:
                props->setFixturePosition(fx->id(), 0, 0, QVector3D(at.x() * 1000, at.y() * 1000, at.z()));
                rp.mountingType = Truss::TopHung;
            break;
            case Mount::HungOnTruss:
            case Mount::OnTrussTop:
                rp.trussId = truss->id();
                rp.trussOffset = kSpacing * float(i) + 1.0f;
                rp.mountingType = r.mount == Mount::OnTrussTop ? Truss::FloorMounted : Truss::TopHung;
                if (r.mount == Mount::OnTrussTop)
                    rp.trussMountSide = FixtureRigProps::TopMounted;
            break;
            case Mount::Floor:
                // Placed on the plot with no height at all: the floor.
                props->setFixturePosition(fx->id(), 0, 0, QVector3D(at.x() * 1000, at.y() * 1000, 0));
                rp.mountingType = Truss::FloorMounted;
            break;
            case Mount::Deck:
                props->setFixturePosition(fx->id(), 0, 0, QVector3D(at.x() * 1000, at.y() * 1000, 0));
                rp.mountingType = Truss::FloorMounted;
                rp.deckPlatformId = deck->id();
            break;
        }
        props->setFixtureRigProps(fx->id(), rp);
    }

    StageTarget *target = props->addStageTarget();
    target->setName(r.name);
    target->setPosition(r.target);
    if (r.aim == Aim::Person)
        target->setKind(StageTarget::Kind::Person);
    if (r.aim == Aim::Area)
    {
        target->setKind(StageTarget::Kind::Area);
        target->setFootprint(QSizeF(r.lineEnd.x(), r.lineEnd.y()));
        QVERIFY(AreaCover::covers(target));
    }
    if (r.aim == Aim::Line)
    {
        // Bowed as a line drawn round a stage edge is: both ends the same way.
        target->setLinePoints({ firstEnd(r), QVector3D(0, 0, 0), r.lineEnd });
        target->makeLine();
        QVERIFY(target->isLine());
    }

    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("Aim");
    aim->setStageTargetId(target->id());
    if (!qIsNaN(r.spread))
        aim->setEffectParamValue(LineSpread::paramName("Spread"), r.spread);
    if (r.group != 1)
        aim->setEffectParamValue(LineSpread::paramName("Group"), r.group);
    if (r.cross != LineSpread::SwapSides)
        aim->setEffectParamValue(LineSpread::paramName("Cross"), r.cross);
    QVERIFY(doc->addPalette(aim));
    Scene *scene = new Scene(doc);
    scene->setName(r.name);
    QList<quint32> ids;
    for (Fixture *fx : fixtures)
    {
        scene->addFixture(fx->id());
        ids << fx->id();
    }
    scene->addPalette(aim->id());
    QVERIFY(doc->addFunction(scene));

    /* The members: one per fixture, or one per ARM of a multi-arm fixture
       -- each its own light, from its own origin. */
    const QList<GroupHead> members = AimSolver::aimMembers(doc, ids);
    const int expected = r.count * (r.def == Def::TwoArm ? 2 : r.def == Def::FourArm ? 4 : 1);
    QCOMPARE(members.size(), expected);

    // Where each is meant to go, as the look resolves it...
    QHash<GroupHead, QVector3D> intended;
    for (const AimSolver::SceneAim &a : AimSolver::sceneAims(doc, scene))
        intended.insert(GroupHead(a.fixtureId, a.head), a.point);
    QCOMPARE(intended.size(), members.size());

    auto label = [&](const GroupHead &m) {
        Fixture *fx = doc->fixture(m.fxi);
        return m.head < 0 ? fx->name() : QString("%1 arm %2").arg(fx->name()).arg(m.head + 1);
    };

    /* Aimed from the middle of the body, where the views draw it: a
       free-placed or deck fixture's stored X/Y is the corner of its plot
       icon, and aimed from there a beam was drawn up to 0.3 m off its point. */
    for (Fixture *fx : fixtures)
    {
        AimSolver::Frame frame;
        QVERIFY(AimSolver::frameFor(doc, fx->id(), frame));
        const QVector3D c = RigHeads::fixtureCentre(doc, fx->id());
        QVERIFY2(qAbs(frame.position.x() - c.x()) < 1e-4f && qAbs(frame.position.y() - c.y()) < 1e-4f,
                 qPrintable(QString("%1 aimed from (%2,%3), drawn at (%4,%5)").arg(fx->name())
                            .arg(frame.position.x()).arg(frame.position.y()).arg(c.x()).arg(c.y())));
    }

    // An arm's origin is its share of the body, along the body.
    for (const GroupHead &m : members)
    {
        if (m.head < 0)
            continue;
        Fixture *fx = doc->fixture(m.fxi);
        const int n = fx->heads();
        const double w = fx->fixtureMode()->physical().width() / 1000.0;
        const QVector3D off = AimSolver::armOffset(doc, m.fxi, m.head);
        const double want = (m.head - (n - 1) * 0.5) * w / n;
        const QVector3D along = r.yaw == 90.0f ? QVector3D(0, 1, 0) : QVector3D(1, 0, 0);
        QVERIFY2((off - along * float(want)).length() < 1e-3f,
                 qPrintable(QString("%1 sits (%2,%3,%4) from the body, not %5 along it")
                            .arg(label(m)).arg(off.x()).arg(off.y()).arg(off.z()).arg(want)));
    }

    // ...is on the target...
    const QList<GroupHead> hang = AimSolver::orderedLineMembers(doc, target, members, LineSpread::AlongLine);
    QList<QVector3D> along;           // a line's points, by where the lights hang
    for (const GroupHead &m : (r.aim == Aim::Line ? hang : members))
    {
        const QVector3D p = intended.value(m);
        AimSolver::Frame frame;
        QVERIFY(AimSolver::frameFor(doc, m, frame));
        if (r.aim == Aim::Point)
            QVERIFY2((p - r.target).length() < 0.01f,
                     qPrintable(QString("%1 aims at (%2,%3,%4)").arg(label(m)).arg(p.x()).arg(p.y()).arg(p.z())));
        else if (r.aim == Aim::Person)
            QVERIFY2(qAbs(p.x() - r.target.x()) < 0.01f && qAbs(p.y() - r.target.y()) < 0.01f
                     && p.z() > 0.5f && p.z() < 1.9f,
                     qPrintable(QString("%1 aims at (%2,%3,%4), not the person").arg(label(m))
                                .arg(p.x()).arg(p.y()).arg(p.z())));
        else if (r.aim == Aim::Area)
        {
            // Inside the area, at subject height above what is under it,
            // and no two lights on one zone.
            const QRectF a = AreaCover::areaRect(props, target);
            QVERIFY2(a.contains(QPointF(p.x(), p.y())),
                     qPrintable(QString("%1 aims at (%2,%3), outside the area").arg(label(m)).arg(p.x()).arg(p.y())));
            QVERIFY(qAbs(p.z() - (props->platformHeightAt(p.x(), p.y()) + 1.4f)) < 1e-3f);
            for (const QVector3D &q : along)
                QVERIFY2((q - p).length() > 0.05f, qPrintable(label(m) + " shares a zone"));
            along << p;
        }
        else
        {
            /* On the line in plan (at its height or -- a head standing above
               it that can't tilt down to it -- the people on it). */
            const QVector<QVector3D> curve = LineCurve(props->targetLinePoints(target)).samples();
            float off = 1e9f;
            for (int k = 1; k < curve.size(); ++k)
            {
                const QVector2D s(curve.at(k - 1)), d = QVector2D(curve.at(k)) - s, q = QVector2D(p) - s;
                const float t = qBound(0.0f, QVector2D::dotProduct(q, d) / qMax(1e-9f, d.lengthSquared()), 1.0f);
                off = qMin(off, (s + d * t - QVector2D(p)).length());
            }
            QVERIFY2(off < 0.02f, qPrintable(QString("%1 aims %2 m off the line").arg(label(m)).arg(off)));
            along << p;
        }
    }
    if (r.aim == Aim::Line)
    {
        // Hung along the line, in fixture order and each fixture's arms in head order.
        QCOMPARE(hang, members);
        const QVector3D first = r.target + firstEnd(r), last = r.target + r.lineEnd;
        const double spread = qIsNaN(r.spread) ? LineSpread::maxSpread() : r.spread;
        if (spread == LineSpread::maxSpread())
        {
            // End to end, in the order they stand: no crossed beams, and
            // every light its own place (16 arms of 8 beams: 16 points).
            QVERIFY2(qAbs(along.first().x() - first.x()) < 0.02f && qAbs(along.last().x() - last.x()) < 0.02f,
                     qPrintable(QString("Spread +5 spans %1..%2, the line %3..%4")
                                .arg(along.first().x()).arg(along.last().x()).arg(first.x()).arg(last.x())));
            for (int i = 1; i < along.size(); ++i)
                QVERIFY2(along.at(i).x() > along.at(i - 1).x() + 0.05f,
                         qPrintable(QString("%1 and %2 share a place").arg(label(hang.at(i - 1)), label(hang.at(i)))));
        }
        else if (spread == 0.0)
            for (const QVector3D &p : along)
                QVERIFY(qAbs(p.x() - r.target.x()) < 0.02f && qAbs(p.y() - r.target.y()) < 0.02f);
        else if (r.group == 2 && r.def == Def::TwoArm)
        {
            /* Grouping 2 pairs the two arms of each beam. Mirrored, each
               pair crosses in place: arm 2 left of arm 1, the beams still in
               order. Swapped, the pairs change sides, each keeping its arms'
               order: the first beam's arms at the far end. */
            for (int i = 0; i + 1 < along.size(); i += 2)
            {
                QCOMPARE(hang.at(i).fxi, hang.at(i + 1).fxi);
                if (r.cross == LineSpread::Mirror)
                    QVERIFY2(along.at(i).x() > along.at(i + 1).x(), qPrintable(label(hang.at(i)) + " not crossed"));
                else
                    QVERIFY2(along.at(i).x() < along.at(i + 1).x(), qPrintable(label(hang.at(i)) + " crossed"));
                if (i + 2 < along.size())
                {
                    const float here = along.at(i).x() + along.at(i + 1).x();
                    const float next = along.at(i + 2).x() + along.at(i + 3).x();
                    QVERIFY(r.cross == LineSpread::Mirror ? next > here : next < here);
                }
            }
        }
        else if (spread < 0.0)
            QVERIFY2(along.first().x() > along.last().x(), "a negative Spread crosses them");
        else
            QVERIFY(along.first().x() > first.x() + 0.1f && along.last().x() < last.x() - 0.1f);
    }

    // ...and the DMX the look sends, read back as the rig views draw it, lands there.
    const QList<SceneValue> dmx = aim->valuesFromFixtures(doc, ids, scene);
    for (Fixture *fx : fixtures)
    {
        QByteArray own(int(fx->channels()), char(0));
        for (const SceneValue &sv : dmx)
            if (sv.fxi == fx->id() && sv.channel < fx->channels())
                own[int(sv.channel)] = char(sv.value);
        // Never into a two-arm beam's spin zone.
        uchar lo = 0, hi = 255;
        for (int h = 0; h < fx->heads(); ++h)
        {
            const quint32 pan = fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB, h);
            if (pan != QLCChannel::invalid() && Fixture::positionBand(fx->channel(pan), lo, hi))
                QVERIFY2(uchar(own.at(int(pan))) <= hi, qPrintable(QString("%1 head %2 pan %3 spins")
                         .arg(fx->name()).arg(h).arg(uchar(own.at(int(pan))))));
        }

        const FixtureRigProps rp = props->fixtureRigProps(fx->id());
        const double slack = dmxSlackDeg(fx, QLCChannel::Pan) + dmxSlackDeg(fx, QLCChannel::Tilt);
        // Every head: from its own origin to its own point when it is an
        // arm, else the fixture's (a shared yoke: all of them).
        for (int h = 0; h < qMax(1, fx->heads()); ++h)
        {
            const GroupHead m(fx->id(), AimSolver::isArm(fx, h) ? h : -1);
            AimSolver::Frame frame;
            QVERIFY(AimSolver::frameFor(doc, m, frame));
            QVERIFY2(intended.contains(m), qPrintable(label(m)));
            const QVector3D p = intended.value(m);
            const float throwM = (p - frame.position).length();
            const double tol = qMax(0.10, double(throwM) * qTan(qDegreesToRadians(slack)) + 0.01);
            QVector3D dir;
            QVERIFY(AimSolver::headDirection(fx, own, h, rp, dir));
            const double miss = missBy(frame.position, dir, p);
            QVERIFY2(miss >= 0.0 && miss <= tol,
                     qPrintable(QString("%1 head %2: aimed at (%3,%4,%5) from (%6,%7,%8), the beam "
                                        "(%9,%10,%11) %12 by %13 m (allowed %14 m)")
                                .arg(fx->name()).arg(h)
                                .arg(p.x()).arg(p.y()).arg(p.z())
                                .arg(frame.position.x()).arg(frame.position.y()).arg(frame.position.z())
                                .arg(dir.x()).arg(dir.y()).arg(dir.z())
                                .arg(miss < 0.0 ? "points away" : "misses").arg(miss).arg(tol)));
        }

        /* At a point the arms converge: each pans by its own bearing to it,
           so the two arms of a beam differ by the angle the body-width
           between them makes there -- not 0, as when they shared an aim. */
        if (r.aim == Aim::Point && r.def != Def::Spot16 && r.def != Def::Spot8)
        {
            AimSolver::Frame f0, f1;
            QVERIFY(AimSolver::frameFor(doc, GroupHead(fx->id(), 0), f0));
            QVERIFY(AimSolver::frameFor(doc, GroupHead(fx->id(), 1), f1));
            auto bearing = [&](const QVector3D &from) {
                return qRadiansToDegrees(qAtan2(double(-(r.target.x() - from.x())),
                                                double(r.target.y() - from.y())));
            };
            const double want = bearing(f1.position) - bearing(f0.position);
            const double got = panDegrees(fx, own, 1) - panDegrees(fx, own, 0);
            const double step = dmxSlackDeg(fx, QLCChannel::Pan) * 2.0 + 0.01;
            QVERIFY2(qAbs(want) > 1.0, qPrintable(QString("%1: arms only %2 degrees apart").arg(fx->name()).arg(want)));
            QVERIFY2(qAbs(qAbs(got) - qAbs(want)) <= step && qAbs(got) > 0.0,
                     qPrintable(QString("%1: arms pan %2 degrees apart, the geometry says %3")
                                .arg(fx->name()).arg(got).arg(want)));
        }
    }

    delete doc;
}

void AimSolver_Test::anIndexedBandRoundsToTheNearestStep()
{
    Doc *doc = new Doc(this);
    Fixture *fx = addTwoArm(doc, "UST", 0);
    QVERIFY(fx != nullptr);
    auto panAt = [&](float deg) {
        const QList<SceneValue> v = fx->positionToValues(QLCChannel::Pan, deg);
        return v.isEmpty() ? -1 : int(v.first().value);
    };
    // 0-127 over 360 degrees: the middle is 63.5, which rounds up; a step is
    // 2.83 degrees and anything within half of one lands on it.
    QCOMPARE(panAt(0.0f), 0);
    QCOMPARE(panAt(180.0f), 64);
    QCOMPARE(panAt(360.0f), 127);
    QCOMPARE(panAt(178.0f), 63);
    QCOMPARE(panAt(1.3f), 0);
    QCOMPARE(panAt(1.5f), 1);
    // Past the end it holds the end, never the spin zone.
    QCOMPARE(panAt(400.0f), 127);
    // Every head's pan, the same value (the arms are aimed together).
    const QList<SceneValue> both = fx->positionToValues(QLCChannel::Pan, 90.0f);
    QCOMPARE(both.size(), 2);
    QCOMPARE(both.at(0).value, both.at(1).value);
    // And back: the fraction is the one positionToValues() was given.
    for (int d = 0; d <= 360; d += 15)
    {
        const int dmx = panAt(float(d));
        const double back = Fixture::positionFraction(fx->channel(fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB)), dmx) * 360.0;
        QVERIFY2(qAbs(back - d) <= 360.0 / 127.0 / 2.0 + 1e-6,
                 qPrintable(QString("%1 degrees -> DMX %2 -> %3 degrees").arg(d).arg(dmx).arg(back)));
    }
    delete doc;
}

/****************************************************************************
 * Area cover: an Aim at an Area covers it (docs/design/AREA_COVER.md, Q209)
 ****************************************************************************/

namespace
{
/* A spot hung (or standing, @p standing) at @p at metres, its lens @p lensDeg
   wide (0: none declared). */
Fixture *placeSpot(Doc *doc, const QString &name, const QVector3D &at, double lensDeg,
                   bool standing = false)
{
    static quint32 address = 0;
    Fixture *fx = addSpot(doc, name, true, address % 500);
    if (fx == nullptr)
        return nullptr;
    address += fx->channels();
    if (lensDeg > 0.0)
    {
        QLCPhysical ph = fx->fixtureMode()->physical();
        ph.setLensDegreesMin(lensDeg);
        ph.setLensDegreesMax(lensDeg);
        fx->fixtureMode()->setPhysical(ph);
    }
    MonitorProperties *props = doc->monitorProperties();
    props->setFixturePosition(fx->id(), 0, 0, QVector3D(at.x() * 1000, at.y() * 1000, at.z()));
    FixtureRigProps rp;
    rp.mountingType = standing ? Truss::FloorMounted : Truss::TopHung;
    props->setFixtureRigProps(fx->id(), rp);
    return fx;
}

StageTarget *addArea(Doc *doc, const QString &name, const QVector3D &centre, const QSizeF &size)
{
    StageTarget *t = doc->monitorProperties()->addStageTarget();
    t->setName(name);
    t->setKind(StageTarget::Kind::Area);
    t->setPosition(centre);
    t->setFootprint(size);
    return t;
}

/* Whether plan segments a-b and c-d cross (touching ends do not count). */
bool crosses(const QPointF &a, const QPointF &b, const QPointF &c, const QPointF &d)
{
    auto side = [](const QPointF &p, const QPointF &q, const QPointF &r) {
        return (q.x() - p.x()) * (r.y() - p.y()) - (q.y() - p.y()) * (r.x() - p.x());
    };
    const double d1 = side(c, d, a), d2 = side(c, d, b), d3 = side(a, b, c), d4 = side(a, b, d);
    return ((d1 > 1e-9 && d2 < -1e-9) || (d1 < -1e-9 && d2 > 1e-9))
           && ((d3 > 1e-9 && d4 < -1e-9) || (d3 < -1e-9 && d4 > 1e-9));
}

QPointF planOf(const QVector3D &v)
{
    return QPointF(v.x(), v.y());
}
}

void AimSolver_Test::areaZonesFollowWhereLightsHang()
{
    Doc *doc = new Doc(this);
    // Three across on a front truss (downstage, y 9) and three on a back one
    // (upstage, y 1), over a 6 x 3 m area from x 0..6, y 3..6.
    QList<Fixture *> front, back;
    QList<quint32> ids;
    for (int i = 0; i < 3; ++i)
    {
        front << placeSpot(doc, QString("Front %1").arg(i + 1), QVector3D(1.0f + 2.0f * i, 9.0f, 5.0f), 20);
        back << placeSpot(doc, QString("Back %1").arg(i + 1), QVector3D(1.0f + 2.0f * i, 1.0f, 6.0f), 20);
    }
    // In a mixed order: who takes what must not depend on it.
    ids << back.at(2)->id() << front.at(0)->id() << back.at(0)->id()
        << front.at(2)->id() << back.at(1)->id() << front.at(1)->id();
    StageTarget *area = addArea(doc, "Chorus", QVector3D(3, 4.5f, 0), QSizeF(6, 3));
    const QList<GroupHead> members = AimSolver::aimMembers(doc, ids);
    QCOMPARE(members.size(), 6);

    int rows = 0;
    QVector<QVector3D> where;
    for (const GroupHead &m : members)
    {
        AimSolver::Frame f;
        QVERIFY(AimSolver::frameFor(doc, m, f));
        where << f.position;
    }
    AreaCover::cells(QRectF(0, 3, 6, 3), where, &rows);
    QCOMPARE(rows, 2);                                  // a 3 x 2 rig, a 3 x 2 grid

    const QList<AreaCover::Zone> zones = AreaCover::zones(doc, area, members);
    QCOMPARE(zones.size(), 6);
    QHash<quint32, QVector3D> at;
    for (const AreaCover::Zone &z : zones)
    {
        for (const QVector3D &other : at)
            QVERIFY2((other - z.aim).length() > 0.5f, "two zones share a centre");
        at.insert(z.member.fxi, z.aim);
        QVERIFY(QRectF(0, 3, 6, 3).contains(planOf(z.aim)));
        QVERIFY(qAbs(z.cell.width() - 2.0) < 1e-6 && qAbs(z.cell.height() - 1.5) < 1e-6);
    }
    for (int i = 0; i < 3; ++i)
    {
        // Front lights on the downstage row, back lights on the upstage one...
        QVERIFY2(at.value(front.at(i)->id()).y() > 4.5f, qPrintable(front.at(i)->name()));
        QVERIFY2(at.value(back.at(i)->id()).y() < 4.5f, qPrintable(back.at(i)->name()));
        // ...each in its own column: stage right light, stage right zone.
        QVERIFY(qAbs(at.value(front.at(i)->id()).x() - (1.0f + 2.0f * i)) < 1e-3f);
        QVERIFY(qAbs(at.value(back.at(i)->id()).x() - (1.0f + 2.0f * i)) < 1e-3f);
    }
    // No two beams cross in plan.
    for (int i = 0; i < zones.size(); ++i)
        for (int j = i + 1; j < zones.size(); ++j)
        {
            AimSolver::Frame fi, fj;
            QVERIFY(AimSolver::frameFor(doc, zones.at(i).member, fi));
            QVERIFY(AimSolver::frameFor(doc, zones.at(j).member, fj));
            QVERIFY(!crosses(planOf(fi.position), planOf(zones.at(i).aim),
                             planOf(fj.position), planOf(zones.at(j).aim)));
        }

    /* Two towers of three shelves (his AT-W-ALL): low shelves take the front
       rank, top shelves the back one, each tower its own side. */
    QVector<QVector3D> towers;
    for (float z : { 0.2f, 1.2f, 2.2f })
        towers << QVector3D(0.25f, 8.4f, z) << QVector3D(11.65f, 8.4f, z);
    const QRectF chorus(2.68, 3.9, 9.754, 3.658);
    const QVector<QRectF> c = AreaCover::cells(chorus, towers, &rows);
    QCOMPARE(rows, 3);
    for (int i = 0; i < towers.size(); ++i)
    {
        const int shelf = i / 2;                        // 0 low .. 2 high
        const bool sr = (i % 2) == 0;
        QVERIFY2(sr ? c.at(i).center().x() < chorus.center().x() : c.at(i).center().x() > chorus.center().x(),
                 qPrintable(QString("tower %1 shelf %2 on the wrong side").arg(i % 2).arg(shelf)));
        const double rank = (chorus.bottom() - c.at(i).center().y()) / chorus.height();   // 0 front .. 1 back
        QVERIFY2(qAbs(rank - (shelf + 0.5) / 3.0) < 1e-6,
                 qPrintable(QString("shelf %1 at rank %2").arg(shelf).arg(rank)));
    }
    delete doc;
}

void AimSolver_Test::areaAimsAtSubjectHeightAboveTheRiser()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    // A 0.61 m riser under the upstage half of the area (y 3..4.5).
    StagePlatform *riser = props->addPlatform();
    riser->setOriginX(-1.0f);
    riser->setOriginY(3.0f);
    riser->setWidth(8.0f);
    riser->setDepth(1.5f);
    riser->setHeight(0.61f);
    QList<quint32> ids;
    for (int i = 0; i < 3; ++i)
    {
        ids << placeSpot(doc, QString("F%1").arg(i), QVector3D(1.0f + 2.0f * i, 9.0f, 5.0f), 20)->id();
        ids << placeSpot(doc, QString("B%1").arg(i), QVector3D(1.0f + 2.0f * i, 1.0f, 6.0f), 20)->id();
    }
    // The area's own Z says nothing: what is under each zone does.
    StageTarget *area = addArea(doc, "Chorus", QVector3D(3, 4.5f, 0.396f), QSizeF(6, 3));
    const QList<AreaCover::Zone> zones = AreaCover::zones(doc, area, AimSolver::aimMembers(doc, ids));
    QCOMPARE(zones.size(), 6);
    for (const AreaCover::Zone &z : zones)
    {
        const float want = z.aim.y() < 4.5f ? 0.61f + 1.4f : 1.4f;
        QVERIFY2(qAbs(z.aim.z() - want) < 1e-4f,
                 qPrintable(QString("zone at y %1 aims at z %2, not %3").arg(z.aim.y()).arg(z.aim.z()).arg(want)));
    }
    // An aim-height override is the subject height.
    area->setAimHeightOverride(1.0f);
    for (const AreaCover::Zone &z : AreaCover::zones(doc, area, AimSolver::aimMembers(doc, ids)))
        QVERIFY(qAbs(z.aim.z() - (z.aim.y() < 4.5f ? 1.61f : 1.0f)) < 1e-4f);
    delete doc;
}

void AimSolver_Test::areaCoverageFollowsBeamWidth()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StageTarget *area = addArea(doc, "Chorus", QVector3D(3, 4.5f, 0), QSizeF(6, 3));
    // Six lights, each straight above its 2 x 1.5 m zone, 4.6 m over subject height.
    auto beams = [&](double deg, int leaveOut) {
        QList<AreaCover::Beam> out;
        for (int r = 0; r < 2; ++r)
            for (int c = 0; c < 3; ++c)
            {
                if (r * 3 + c == leaveOut)
                    continue;
                AreaCover::Beam b;
                b.aim = QVector3D(1.0f + 2.0f * c, 3.75f + 1.5f * r, 1.4f);
                b.origin = b.aim + QVector3D(0, 0, 4.6f);
                b.degrees = deg;
                out << b;
            }
        return out;
    };
    // Narrow: six circles of radius 4.6 tan 5 deg on 18 square metres.
    const AreaCover::Coverage narrow = AreaCover::coverage(props, area, beams(10.0, -1), 96);
    const double radius = 4.6 * qTan(qDegreesToRadians(5.0));
    const double want = 6.0 * M_PI * radius * radius / 18.0;
    QVERIFY2(qAbs(narrow.fraction - want) < 0.02,
             qPrintable(QString("narrow covers %1, should be %2").arg(narrow.fraction).arg(want)));
    QVERIFY(!narrow.gap.isEmpty());
    QVERIFY(!narrow.gapRects().isEmpty());
    QVERIFY(narrow.caption().startsWith(QString("Covers %1% of Chorus").arg(qRound(narrow.fraction * 100))));
    // Wide enough to reach each zone's corners (half-diagonal 1.25 m: 30.4 deg).
    const AreaCover::Coverage wide = AreaCover::coverage(props, area, beams(32.0, -1));
    QCOMPARE(wide.fraction, 1.0);
    QVERIFY(wide.gap.isEmpty());
    QVERIFY(wide.gapRects().isEmpty());
    QCOMPARE(wide.caption(), QString("Covers all of Chorus"));
    // Tape over the bottom of every cone: they light less of the area.
    {
        QList<AreaCover::Beam> taped = beams(32.0, -1);
        for (AreaCover::Beam &b : taped)
            b.mask.bottom = 20.0f;
        const AreaCover::Coverage cut = AreaCover::coverage(props, area, taped);
        QVERIFY2(cut.fraction < wide.fraction - 0.1,
                 qPrintable(QString("taped covers %1, untaped %2").arg(cut.fraction).arg(wide.fraction)));
    }
    // Without the upstage, stage-left light (the back row, the largest x):
    // its zone is the gap, and it is named so.
    const AreaCover::Coverage missing = AreaCover::coverage(props, area, beams(32.0, 2));
    QVERIFY(missing.fraction < 0.9 && missing.fraction > 0.75);
    QCOMPARE(missing.gap, QString("upstage left"));
    QVERIFY(missing.caption().contains(QString("upstage left")));

    // A footprint straight down is a circle of the cone's radius at that depth.
    const QVector<QPointF> fp = AreaCover::footprint(QVector3D(0, 0, 5), QVector3D(0, 0, 1), 20.0);
    QCOMPARE(fp.size(), 48);
    for (const QPointF &pt : fp)
        QVERIFY(qAbs(std::hypot(pt.x(), pt.y()) - 4.0 * qTan(qDegreesToRadians(10.0))) < 1e-3);

    // From fixtures: the lens is read from the definition, else 20 degrees.
    QList<quint32> ids;
    for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 3; ++c)
            ids << placeSpot(doc, QString("S%1%2").arg(r).arg(c),
                             QVector3D(1.0f + 2.0f * c, r == 0 ? 9.0f : 1.0f, 6.0f), 50)->id();
    const AreaCover::Coverage fromRig = AreaCover::coverage(doc, area, AimSolver::aimMembers(doc, ids));
    QCOMPARE(fromRig.beams.size(), 6);
    for (const AreaCover::Beam &b : fromRig.beams)
        QCOMPARE(b.degrees, 50.0);
    QCOMPARE(fromRig.fraction, 1.0);
    delete doc;
}

void AimSolver_Test::areaOfNoSizeIsAPoint()
{
    Doc *doc = new Doc(this);
    QList<quint32> ids;
    ids << placeSpot(doc, "A", QVector3D(0, 0, 6), 20)->id() << placeSpot(doc, "B", QVector3D(4, 0, 6), 20)->id();
    StageTarget *area = addArea(doc, "Spot", QVector3D(2, 4, 0.5f), QSizeF());
    QVERIFY(!AreaCover::covers(area));
    QVERIFY(AreaCover::zones(doc, area, AimSolver::aimMembers(doc, ids)).isEmpty());
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setStageTargetId(area->id());
    QVERIFY(doc->addPalette(aim));
    Scene *scene = new Scene(doc);
    for (quint32 id : ids)
        scene->addFixture(id);
    scene->addPalette(aim->id());
    QVERIFY(doc->addFunction(scene));
    // Both at the point, at the area's own Z, as before.
    const QList<AimSolver::SceneAim> aims = AimSolver::sceneAims(doc, scene);
    QCOMPARE(aims.size(), 2);
    for (const AimSolver::SceneAim &a : aims)
        QVERIFY((a.point - QVector3D(2, 4, 0.5f)).length() < 1e-4f);
    // Given a size, they spread over it.
    area->setFootprint(QSizeF(4, 2));
    const QList<AimSolver::SceneAim> spread = AimSolver::sceneAims(doc, scene);
    QCOMPARE(spread.size(), 2);
    QVERIFY((spread.at(0).point - spread.at(1).point).length() > 1.0f);
    delete doc;
}

void AimSolver_Test::areaGivesEachArmItsOwnZone()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Fixture *bar = addTwoArm(doc, "UST", 0);
    QVERIFY(bar != nullptr);
    props->setFixturePosition(bar->id(), 0, 0, QVector3D(2000, 1000, 5));
    FixtureRigProps rp;
    rp.mountingType = Truss::TopHung;
    props->setFixtureRigProps(bar->id(), rp);
    StageTarget *area = addArea(doc, "Pair", QVector3D(2.25f, 4, 0), QSizeF(4, 2));
    const QList<GroupHead> members = AimSolver::aimMembers(doc, QList<quint32>() << bar->id());
    QCOMPARE(members.size(), 2);
    const QList<AreaCover::Zone> zones = AreaCover::zones(doc, area, members);
    QCOMPARE(zones.size(), 2);
    QVERIFY(zones.at(0).member.head != zones.at(1).member.head);
    // Side by side across the stage: each arm the half on its own side.
    AimSolver::Frame f0, f1;
    QVERIFY(AimSolver::frameFor(doc, zones.at(0).member, f0));
    QVERIFY(AimSolver::frameFor(doc, zones.at(1).member, f1));
    QVERIFY((f0.position.x() < f1.position.x()) == (zones.at(0).aim.x() < zones.at(1).aim.x()));
    QVERIFY(qAbs(zones.at(0).aim.x() - zones.at(1).aim.x()) > 1.5f);
    delete doc;
}

void AimSolver_Test::areaCoverNeedsHeadAndDeck()
{
    /* Q213: a spot counts as lit only if it is lit at head height AND on the
       deck; the three fractions say which is missing. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StageTarget *area = addArea(doc, "Chorus", QVector3D(3, 4.5f, 0), QSizeF(6, 3));

    // A near-horizontal, wide beam from downstage at chest height
    // (6 lights across): heads are lit all through, but the deck close to the
    // lights is steeper than the beam reaches, so the deck near them is dark,
    // and coverage drops from what head height alone would say.
    QList<AreaCover::Beam> flat;
    for (int c = 0; c < 6; ++c)
    {
        AreaCover::Beam b;
        b.origin = QVector3D(0.5f + float(c), 8.0f, 1.6f);
        b.aim = QVector3D(0.5f + float(c), 4.5f, 1.4f);
        b.degrees = 60.0;
        flat << b;
    }
    const AreaCover::Coverage low = AreaCover::coverage(props, area, flat, 96);
    QVERIFY2(low.headOnly > 0.1, qPrintable(QString("head-only %1").arg(low.headOnly)));
    QCOMPARE(low.deckOnly, 0.0);
    // Both heights are what counts: less than the head-height reading alone.
        QVERIFY(low.fraction < 0.9);
    QVERIFY2(low.deckGap.contains("downstage"), qPrintable(low.deckGap));
    QVERIFY(low.headGap.isEmpty() || !low.headGap.contains("downstage"));
    QVERIFY(!low.deckGapRects().isEmpty());
    QVERIFY(low.caption().contains("deck dark downstage"));
    QVERIFY(!low.caption().contains("heads dark downstage"));

    // A steep narrow beam is the other way round: its circle is wider at the
    // deck than at head height, so a ring of deck is lit with heads dark.
    QList<AreaCover::Beam> steep;
    for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 3; ++c)
        {
            AreaCover::Beam b;
            b.aim = QVector3D(1.0f + 2.0f * c, 3.75f + 1.5f * r, 1.4f);
            b.origin = b.aim + QVector3D(0, 0, 4.6f);
            b.degrees = 12.0;
            steep << b;
        }
    const AreaCover::Coverage down = AreaCover::coverage(props, area, steep, 96);
    QVERIFY2(down.deckOnly > 0.02, qPrintable(QString("deck-only %1").arg(down.deckOnly)));
    QCOMPARE(down.headOnly, 0.0);
    QVERIFY(!down.headGapRects().isEmpty());
    QVERIFY(down.deckLit.count(true) > down.headLit.count(true));
    // Where both are dark in the same places it is one gap, not two.
    const AreaCover::Coverage dark = AreaCover::coverage(props, area, QList<AreaCover::Beam>(), 48);
    QCOMPARE(dark.fraction, 0.0);
    QVERIFY(!dark.caption().contains("deck dark"));

    delete doc;
}

void AimSolver_Test::areaCoversHisChorus()
{
    /* His show, modelled: three ranks of risers (0.183, 0.396, 0.61 m) under
       the Chorus area (9.754 x 3.658 m), eight two-arm USTs standing on the
       top riser, and six front washes on two towers either side, three
       shelves each (AT-W-ALL). */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    const float ranks[3][2] = { { 3.760f, 0.610f }, { 4.979f, 0.396f }, { 6.198f, 0.183f } };
    QList<StagePlatform *> risers;
    for (const auto &rk : ranks)
    {
        StagePlatform *p = props->addPlatform();
        p->setOriginX(2.744f);
        p->setOriginY(rk[0]);
        p->setWidth(9.752f);
        p->setDepth(1.219f);
        p->setHeight(rk[1]);
        risers << p;
    }
    QList<quint32> ids;
    for (int i = 0; i < 8; ++i)
    {
        Fixture *ust = addTwoArm(doc, QString("UST %1").arg(i + 1), quint32(340 + 20 * i));
        QVERIFY(ust != nullptr);
        props->setFixturePosition(ust->id(), 0, 0, QVector3D((3.2f + 1.2f * i) * 1000, 4.2f * 1000, 0));
        FixtureRigProps rp;
        rp.mountingType = Truss::FloorMounted;
        rp.deckPlatformId = risers.first()->id();
        props->setFixtureRigProps(ust->id(), rp);
        ids << ust->id();
    }
    QList<quint32> washes;
    for (float z : { 0.2f, 1.2f, 2.2f })
        for (float x : { 0.25f, 11.65f })
            washes << placeSpot(doc, QString("Wash %1 %2").arg(x < 5 ? "SR" : "SL").arg(z), QVector3D(x, 8.43f, z), 25.0,
                                true)->id();
    ids << washes;
    StageTarget *chorus = addArea(doc, "Chorus", QVector3D(7.556f, 5.724f, 0.396f), QSizeF(9.754, 3.658));
    const QList<GroupHead> members = AimSolver::aimMembers(doc, ids);
    QCOMPARE(members.size(), 22);

    const QList<AreaCover::Zone> zones = AreaCover::zones(doc, chorus, members);
    QCOMPARE(zones.size(), 22);
    const QRectF a = AreaCover::areaRect(props, chorus);
    QHash<GroupHead, AreaCover::Zone> of;
    for (const AreaCover::Zone &z : zones)
    {
        for (const AreaCover::Zone &o : of)
            QVERIFY2(!o.cell.intersects(z.cell.adjusted(0.01, 0.01, -0.01, -0.01)), "two zones overlap");
        of.insert(z.member, z);
        // Subject height above the riser actually under the zone's centre.
        QVERIFY(qAbs(z.aim.z() - (props->platformHeightAt(z.aim.x(), z.aim.y()) + 1.4f)) < 1e-4f);
    }
    // The washes take the front, the USTs on the top riser the back.
    double washY = 0.0, ustY = 0.0;
    for (const AreaCover::Zone &z : zones)
        (washes.contains(z.member.fxi) ? washY : ustY) += z.aim.y();
    washY /= 6.0;
    ustY /= 16.0;
    QVERIFY2(washY > a.center().y() && ustY < washY,
             qPrintable(QString("washes at y %1, USTs at y %2").arg(washY).arg(ustY)));
    // Each tower lights its own side.
    for (quint32 w : washes)
    {
        AimSolver::Frame f;
        QVERIFY(AimSolver::frameFor(doc, GroupHead(w, -1), f));
        const float zx = of.value(GroupHead(w, -1)).aim.x();
        QVERIFY(f.position.x() < 5.0f ? zx < a.center().x() : zx > a.center().x());
    }

    /* Coverage, before and after: every light at the centre (as it was) and
       spread over the zones. */
    QList<AreaCover::Beam> before;
    const QVector3D centre = props->targetAimPoint(chorus);
    for (const GroupHead &m : members)
    {
        AimSolver::Frame f;
        QVERIFY(AimSolver::frameFor(doc, m, f));
        AreaCover::Beam b;
        b.member = m;
        b.origin = f.position;
        b.aim = centre;
        Fixture *fx = doc->fixture(m.fxi);
        b.degrees = RigHeads::beamAngle(fx, QByteArray(int(fx->channels()), char(0)));
        if (b.degrees <= 0.0)
            b.degrees = AreaCover::defaultBeamDegrees();
        before << b;
    }
    const AreaCover::Coverage was = AreaCover::coverage(props, chorus, before);
    const AreaCover::Coverage now = AreaCover::coverage(doc, chorus, members);
    qDebug().noquote() << QString("Chorus: %1% at the centre, %2% covered (%3)")
                          .arg(qRound(was.fraction * 100)).arg(qRound(now.fraction * 100)).arg(now.caption());
    qDebug().noquote() << QString("Chorus both %1% head-only %2% deck-only %3% (head height alone %4%, deck alone %5%): %6")
                          .arg(now.fraction * 100, 0, 'f', 1).arg(now.headOnly * 100, 0, 'f', 1)
                          .arg(now.deckOnly * 100, 0, 'f', 1).arg((now.fraction + now.headOnly) * 100, 0, 'f', 1)
                          .arg((now.fraction + now.deckOnly) * 100, 0, 'f', 1).arg(now.caption());
    // Q213: at head height alone it still reads as before (spread over the
    // zones beats everything at the centre, and well over 85%); with the deck
    // counted too it is lower, and the deck corners are what is dark.
    const double headAlone = now.fraction + now.headOnly;
    QVERIFY2(headAlone > was.fraction + was.headOnly + 0.1 && headAlone > 0.85,
             qPrintable(QString("head height covers %1 now, %2 before")
                        .arg(headAlone).arg(was.fraction + was.headOnly)));
    // (Which of several exactly tied matches the solver picks moves the
    // reading by a few points -- 55 to 74% depending on the pick -- so the
    // bound is the floor that holds for any of them, and the pick itself is
    // made platform-independent in AreaCover::cells.)
    QVERIFY2(now.fraction > 0.5 && now.fraction < headAlone - 0.05 && now.headOnly > 0.05,
             qPrintable(QString("both %1, head only %2").arg(now.fraction).arg(now.headOnly)));
    QVERIFY(!now.deckGap.isEmpty());
    QVERIFY(now.caption().contains("deck dark"));
    delete doc;
}
