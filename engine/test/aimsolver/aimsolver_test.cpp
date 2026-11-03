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
enum class Def { Spot16, Spot8, TwoArm };
enum class Mount { Hung, HungOnTruss, Floor, Deck, OnTrussTop };
enum class Aim { Point, Person, Line };

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
    QVector3D lineEnd;          // a line: its last end from the centre (first = -this)
    double spread;              // a line: Spread (NaN = the look's default, +5)
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

    // Where each head is meant to go, as the look resolves it...
    QHash<quint32, QVector3D> intended;
    for (const AimSolver::SceneAim &a : AimSolver::sceneAims(doc, scene))
        intended.insert(a.fixtureId, a.point);
    QCOMPARE(intended.size(), fixtures.size());

    // ...is on the target...
    QList<QVector3D> along;           // a line's points, by where the heads stand
    for (Fixture *fx : fixtures)
    {
        const QVector3D p = intended.value(fx->id());
        AimSolver::Frame frame;
        QVERIFY(AimSolver::frameFor(doc, fx->id(), frame));
        if (r.aim == Aim::Point)
            QVERIFY2((p - r.target).length() < 0.01f,
                     qPrintable(QString("%1 aims at (%2,%3,%4)").arg(fx->name()).arg(p.x()).arg(p.y()).arg(p.z())));
        else if (r.aim == Aim::Person)
            QVERIFY2(qAbs(p.x() - r.target.x()) < 0.01f && qAbs(p.y() - r.target.y()) < 0.01f
                     && p.z() > 0.5f && p.z() < 1.9f,
                     qPrintable(QString("%1 aims at (%2,%3,%4), not the person").arg(fx->name())
                                .arg(p.x()).arg(p.y()).arg(p.z())));
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
            QVERIFY2(off < 0.02f, qPrintable(QString("%1 aims %2 m off the line").arg(fx->name()).arg(off)));
            along << p;
        }
    }
    if (r.aim == Aim::Line)
    {
        const QVector3D first = r.target + firstEnd(r), last = r.target + r.lineEnd;
        const double spread = qIsNaN(r.spread) ? LineSpread::maxSpread() : r.spread;
        if (spread == LineSpread::maxSpread())
        {
            // End to end, in the order they stand: no crossed beams.
            QVERIFY2(qAbs(along.first().x() - first.x()) < 0.02f && qAbs(along.last().x() - last.x()) < 0.02f,
                     qPrintable(QString("Spread +5 spans %1..%2, the line %3..%4")
                                .arg(along.first().x()).arg(along.last().x()).arg(first.x()).arg(last.x())));
            for (int i = 1; i < along.size(); ++i)
                QVERIFY(along.at(i).x() > along.at(i - 1).x());
        }
        else if (spread == 0.0)
            for (const QVector3D &p : along)
                QVERIFY(qAbs(p.x() - r.target.x()) < 0.02f && qAbs(p.y() - r.target.y()) < 0.02f);
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

        AimSolver::Frame frame;
        QVERIFY(AimSolver::frameFor(doc, fx->id(), frame));
        const QVector3D p = intended.value(fx->id());
        const float throwM = (p - frame.position).length();
        const double slack = dmxSlackDeg(fx, QLCChannel::Pan) + dmxSlackDeg(fx, QLCChannel::Tilt);
        const double tol = qMax(0.10, double(throwM) * qTan(qDegreesToRadians(slack)) + 0.01);
        const FixtureRigProps rp = props->fixtureRigProps(fx->id());
        // Every head: a two-arm beam's arms are aimed together.
        for (int h = 0; h < qMax(1, fx->heads()); ++h)
        {
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
