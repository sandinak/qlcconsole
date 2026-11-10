/*
  Q Light Controller Plus
  aimsolver.cpp

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

#include <QtMath>
#include <QDebug>

#include "aimsolver.h"
#include "areacover.h"
#include "linecurve.h"
#include "righeads.h"
#include "monitorproperties.h"
#include "fixturegroup.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "scene.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "fixture.h"
#include "truss.h"
#include "doc.h"

bool AimSolver::floorMounted(const FixtureRigProps &rp)
{
    /* What it is BOLTED to decides, not what it is missing. This used to read
       "no truss, therefore standing on the deck", which is wrong for every
       fixture hung from something we do not model yet -- and, worse, the views
       never applied it, so a free-placed head was solved standing and drawn
       hanging. A head standing on a truss top is recorded as FloorMounted by
       the mount itself (that is what "Mounting=FloorMounted" on a truss
       means), and a head on the TOP face of a riser stands on it. */
    if (rp.mountingType == Truss::FloorMounted)
        return true;
    if (rp.riserPlatformId != UINT_MAX && rp.riserFace == 1)
        return true;
    // Deck mount means standing on the platform's top: there is nothing to
    // hang from. The rig props' TopHung default used to leak through here, so
    // of eight identical heads on one deck, the ones never explicitly set to
    // FloorMounted were solved (and drawn) upside down -- aimed "up" at a
    // target above them, past their tilt limit.
    if (rp.onDeck())
        return true;
    return false;
}

bool AimSolver::frameFor(Doc *doc, quint32 fixtureId, Frame &frame)
{
    if (doc == NULL)
        return false;
    Fixture *fixture = doc->fixture(fixtureId);
    MonitorProperties *mProps = doc->monitorProperties();
    if (fixture == NULL || fixture->fixtureMode() == NULL || mProps == NULL)
        return false;
    if (!mProps->containsFixture(fixtureId))
        return false;

    const FixtureRigProps rp = mProps->fixtureRigProps(fixtureId);
    frame.floorMounted = floorMounted(rp);

    // Fixture world position in metres; a free-placed fixture on a platform sits
    // at the deck height (mirror the target's platform handling).
    frame.position = mProps->fixtureRigPosition(fixtureId);
    /* From the middle of the body, where the views draw it and its light
       leaves: a free-placed or deck-mounted fixture's stored X/Y is the
       top-left of its plot icon (RigHeads::fixtureCentre()), and aimed from
       there every beam was drawn up to a third of a metre off its point --
       half a 510 mm two-arm bar's width, sideways. */
    {
        const QVector3D c = RigHeads::fixtureCentre(doc, fixtureId);
        frame.position.setX(c.x());
        frame.position.setY(c.y());
    }
    // Only a free-placed head gets the height of whatever platform it stands
    // over: every mount (truss, pipe, tower, deck, riser) already puts the
    // head at its real height, and adding the platform again lifted a
    // deck-mounted head to twice the deck's height.
    if (rp.primaryMount() == FixtureRigProps::NoMount)
        frame.position.setZ(frame.position.z()
                            + mProps->platformHeightAt(frame.position.x(), frame.position.y()));

    frame.panZeroDir    = rp.panZeroDir;
    frame.panOffsetDeg  = rp.panOffsetDeg;
    frame.tiltOffsetDeg = rp.tiltOffsetDeg;
    frame.panInvert     = rp.panInvert;
    frame.tiltInvert    = rp.tiltInvert;

    const Fixture::EffectiveBeam eb = fixture->effectiveBeam();
    frame.panMax  = float(eb.panMax);  if (frame.panMax  <= 0.0f) frame.panMax  = 360.0f;
    frame.tiltMax = float(eb.tiltMax); if (frame.tiltMax <= 0.0f) frame.tiltMax = 270.0f;
    return true;
}

/* A head points on its own when it has a pan or tilt channel no earlier
   head has: the two arms of a beam bar do, the pixels of a moving bar
   (one yoke, every head listing it or none) do not. */
static bool ownsAim(const QList<QLCFixtureHead> &heads, int h)
{
    const QLCFixtureHead &me = heads.at(h);
    const quint32 pan = me.channelNumber(QLCChannel::Pan, QLCChannel::MSB);
    const quint32 tilt = me.channelNumber(QLCChannel::Tilt, QLCChannel::MSB);
    if (pan == QLCChannel::invalid() && tilt == QLCChannel::invalid())
        return false;
    for (int i = 0; i < h; ++i)
    {
        const QLCFixtureHead &o = heads.at(i);
        if ((pan != QLCChannel::invalid() && o.channelNumber(QLCChannel::Pan, QLCChannel::MSB) == pan)
            || (tilt != QLCChannel::invalid() && o.channelNumber(QLCChannel::Tilt, QLCChannel::MSB) == tilt))
            return false;
    }
    return true;
}

static int armCount(const QLCFixtureMode *mode)
{
    if (mode == nullptr)
        return 0;
    const QList<QLCFixtureHead> &heads = mode->heads();
    int n = 0;
    for (int h = 0; h < heads.size(); ++h)
        if (ownsAim(heads, h))
            ++n;
    return n;
}

QVector<int> AimSolver::arms(const Fixture *fx)
{
    QVector<int> out;
    const QLCFixtureMode *mode = fx != nullptr ? fx->fixtureMode() : nullptr;
    if (armCount(mode) < 2)
        return out;
    const QList<QLCFixtureHead> &heads = mode->heads();
    for (int h = 0; h < heads.size(); ++h)
        if (ownsAim(heads, h))
            out << h;
    return out;
}

bool AimSolver::isArm(const Fixture *fx, int head)
{
    const QLCFixtureMode *mode = fx != nullptr ? fx->fixtureMode() : nullptr;
    if (mode == nullptr || head < 0 || head >= mode->heads().size())
        return false;
    return ownsAim(mode->heads(), head) && armCount(mode) >= 2;
}

QVector3D AimSolver::armOffset(Doc *doc, quint32 fixtureId, int head)
{
    if (doc == nullptr || head < 0 || doc->monitorProperties() == nullptr)
        return QVector3D();
    const Fixture *fx = doc->fixture(fixtureId);
    if (fx == nullptr || !isArm(fx, head))
        return QVector3D();
    /* The definitions say nothing of where each arm is, only the body's
       size and the head count (Layout 2 x 1 on a 510 mm bar): each arm is
       in the middle of its share of the width, as the rig view draws it
       (StructureStudioView::drawMoverSolid()). */
    const QLCFixtureMode *mode = fx->fixtureMode();
    const int n = mode->heads().size();
    const int pw = mode->physical().width();
    const double w = pw > 0 ? pw / 1000.0 : 0.3;
    const double off = (head - (n - 1) * 0.5) * (w / n);
    QVector3D L, N, H;
    RigHeads::frame(doc->monitorProperties()->fixtureRigProps(fixtureId), L, N, H);
    return L * float(off);
}

bool AimSolver::frameFor(Doc *doc, const GroupHead &member, Frame &frame)
{
    if (!frameFor(doc, member.fxi, frame))
        return false;
    if (member.head >= 0)
        frame.position += armOffset(doc, member.fxi, member.head);
    return true;
}

/* Each of @a fids as its aim members, appended to @a out: the fixture, or
   each of its arms in head order (as arms(), without building its list). */
static void appendMembers(Doc *doc, const QList<quint32> &fids, QList<GroupHead> &out)
{
    for (quint32 fid : fids)
    {
        const Fixture *fx = doc->fixture(fid);
        const QLCFixtureMode *mode = fx != nullptr ? fx->fixtureMode() : nullptr;
        if (armCount(mode) < 2)
        {
            out << GroupHead(fid, -1);
            continue;
        }
        const QList<QLCFixtureHead> &heads = mode->heads();
        for (int h = 0; h < heads.size(); ++h)
            if (ownsAim(heads, h))
                out << GroupHead(fid, h);
    }
}

QList<GroupHead> AimSolver::aimMembers(Doc *doc, const QList<quint32> &fixtures)
{
    QList<GroupHead> out;
    foreach (quint32 fid, aimable(doc, fixtures))
    {
        const QVector<int> a = arms(doc->fixture(fid));
        if (a.isEmpty())
            out << GroupHead(fid, -1);
        else
            for (int h : a)
                out << GroupHead(fid, h);
    }
    return out;
}

QList<GroupHead> AimSolver::aimMembers(Doc *doc, const Scene *scene, quint32 paletteId)
{
    if (doc == nullptr || scene == nullptr)
        return QList<GroupHead>();
    const QList<quint32> fixtures = aimHeads(doc, scene, paletteId);
    // aimHeads() has already sorted out what can point.
    QList<GroupHead> out;
    foreach (quint32 fid, fixtures)
    {
        const QVector<int> a = arms(doc->fixture(fid));
        if (a.isEmpty())
            out << GroupHead(fid, -1);
        else
            for (int h : a)
                out << GroupHead(fid, h);
    }
    return out;
}

AimSolver::Solution AimSolver::solve(const Frame &frame, const QVector3D &targetWorld)
{
    const float dx = targetWorld.x() - frame.position.x();
    const float dy = targetWorld.y() - frame.position.y();
    const float dz = targetWorld.z() - frame.position.z();
    const float horizDist = qSqrt(dx * dx + dy * dy);

    /* Horizontal bearing of the target, in the SAME terms as a head's pan zero:
       0 = downstage (+Y), 90 = stage right (-X), turning the way the "Facing
       (pan zero)" menu names them. It used to measure from upstage, which put
       every solved pan exactly 180 degrees out from the direction the plot and
       the rig view then drew the beam -- aim a head at a mark and its beam
       went to the mirror image of that mark through the head. Nothing else in
       the app used the old sense; MonitorFixtureItem, fixtureAimDirection and
       the facing menu all agree on this one. */
    float azimuthDeg = float(qRadiansToDegrees(qAtan2(double(-dx), double(dy))));
    if (azimuthDeg < 0.0f) azimuthDeg += 360.0f;

    float relativePan = azimuthDeg - frame.panZeroDir;
    while (relativePan >  180.0f) relativePan -= 360.0f;
    while (relativePan < -180.0f) relativePan += 360.0f;

    float panRaw = frame.panMax / 2.0f + relativePan + frame.panOffsetDeg;
    // Floor mount = hung fixture flipped 180° about horizontal → pan sense
    // reverses (and tilt negates). The panInvert flag toggles from there.
    bool invertPan = frame.panInvert;
    if (frame.floorMounted)
        invertPan = !invertPan;
    if (invertPan) panRaw = frame.panMax - panRaw;

    const float elevDeg = float(qRadiansToDegrees(qAtan2(double(dz), double(horizDist))));

    float tiltRaw;
    if (frame.floorMounted)
        // Floor: tilt centre = straight up; tilt down toward the target =
        // centre - (90 - elev).
        tiltRaw = frame.tiltMax / 2.0f - 90.0f + elevDeg + frame.tiltOffsetDeg;
    else
        // TopHung/SideArm: tilt centre = straight down; deviation = 90 + elev.
        tiltRaw = frame.tiltMax / 2.0f + (90.0f + elevDeg) + frame.tiltOffsetDeg;
    if (frame.tiltInvert) tiltRaw = frame.tiltMax - tiltRaw;

    /* Every way of pointing at the same place: the same pan a whole turn
       either side, or half a turn round with the tilt mirrored through home.
       Invert maps x to max - x on both axes, which commutes with all of these,
       so they can be taken after it. */
    auto marginOf = [&](float pan, float tilt) {
        return qMin(qMin(pan, frame.panMax - pan), qMin(tilt, frame.tiltMax - tilt));
    };
    float bestPan = panRaw, bestTilt = tiltRaw;
    float best = marginOf(panRaw, tiltRaw);
    if (best < 0.0f)
    {
        for (int k = -2; k <= 2; ++k)
        {
            if (k == 0)
                continue;
            const float pan = panRaw + 180.0f * float(k);
            const float tilt = (k % 2 != 0) ? frame.tiltMax - tiltRaw : tiltRaw;
            /* A real improvement only. Mirroring leaves the tilt shortfall
               exactly as it was, so when tilt is what is out of range the two
               tie -- and a tie decided by rounding swung an unreachable head
               half a turn for nothing. */
            const float m = marginOf(pan, tilt);
            if (m > best + 0.01f)
            {
                best = m;
                bestPan = pan;
                bestTilt = tilt;
            }
        }
    }

    Solution out;
    out.panDeg = qBound(0.0f, bestPan, frame.panMax);
    out.tiltDeg = qBound(0.0f, bestTilt, frame.tiltMax);
    out.marginDeg = best;
    return out;
}

void AimSolver::intendedAngles(const FixtureRigProps &rp, double panCentredDeg,
                               double tiltCentredDeg, double &panRelDeg, double &tiltRelDeg)
{
    /* solve() works in fixture degrees from 0; centred, the middle of the
       range is 0 and mirroring about it is negation, which is what invert does
       there. Undo it first, as solve() applied it last. */
    bool invertPan = rp.panInvert;
    if (floorMounted(rp))
        invertPan = !invertPan;
    double pan = invertPan ? -panCentredDeg : panCentredDeg;
    double tilt = rp.tiltInvert ? -tiltCentredDeg : tiltCentredDeg;

    // Then the calibration: solve() added it to correct the physical head.
    panRelDeg = pan - double(rp.panOffsetDeg);
    tiltRelDeg = tilt - double(rp.tiltOffsetDeg);
}

bool AimSolver::beamDirection(const FixtureRigProps &rp, double panCentredDeg,
                              double tiltCentredDeg, QVector3D &dir)
{
    double panRel = 0.0, tiltRel = 0.0;
    intendedAngles(rp, panCentredDeg, tiltCentredDeg, panRel, tiltRel);

    const double yaw = qDegreesToRadians(double(rp.panZeroDir) + panRel);
    /* Elevation of the beam above the horizon. A hanging head is centred on
       straight down, a standing one on straight up -- which is why a head
       standing on a truss top cannot point at its own feet however much tilt
       it has. */
    const double elevDeg = floorMounted(rp) ? (tiltRel + 90.0) : (tiltRel - 90.0);
    const double elev = qDegreesToRadians(elevDeg);

    // Horizontal bearing: 0 = downstage (+Y), turning clockwise from above.
    dir = QVector3D(float(-qSin(yaw) * qCos(elev)),
                    float(qCos(yaw) * qCos(elev)),
                    float(qSin(elev)));
    if (dir.length() < 1e-6f)
        return false;
    dir.normalize();
    return true;
}

bool AimSolver::headDirection(const Fixture *fx, const QByteArray &values, int head,
                              const FixtureRigProps &rp, QVector3D &dir,
                              const double *panFrac, const double *tiltFrac)
{
    const QLCFixtureMode *mode = fx != nullptr ? fx->fixtureMode() : nullptr;
    if (mode == nullptr || mode->heads().isEmpty())
        return false;

    const QLCFixtureHead *h = &mode->heads().at(
        (head >= 0 && head < mode->heads().size()) ? head : 0);
    if (h->channelNumber(QLCChannel::Pan,  QLCChannel::MSB) == QLCChannel::invalid()
        && h->channelNumber(QLCChannel::Tilt, QLCChannel::MSB) == QLCChannel::invalid())
        h = &mode->heads().first();
    const quint32 panCh  = h->channelNumber(QLCChannel::Pan,  QLCChannel::MSB);
    const quint32 tiltCh = h->channelNumber(QLCChannel::Tilt, QLCChannel::MSB);
    if (panCh == QLCChannel::invalid() && tiltCh == QLCChannel::invalid())
        return false;                       // not a mover: nothing to point

    /* The centre of the pointing band is 0 degrees and the range is centred
       on it. The band is the channel's indexed-position range when it has one
       (Fixture::positionFraction()): read across the whole channel, a two-arm
       beam pointing straight downstage (DMX 64 of 0-127) was drawn a quarter
       turn off, at the wings. The fine byte counts: read coarse only, a
       16-bit 540 degree head drew up to 2 degrees off its mark. */
    auto degrees = [&](quint32 msb, quint32 lsb, double maxDeg, const double *frac) {
        if (msb == QLCChannel::invalid() || msb >= quint32(values.size()))
            return 0.0;
        const bool fine = lsb != QLCChannel::invalid() && lsb < quint32(values.size());
        double val = 0.0;
        if (frac != nullptr)
            val = *frac * 255.0;
        else
        {
            val = double(uchar(values.at(int(msb))));
            if (fine)
                val += double(uchar(values.at(int(lsb)))) / 256.0;
        }
        return Fixture::positionFraction(mode->channel(msb), val, fine) * maxDeg - (maxDeg / 2.0);
    };
    /* The definition's range; only a 0 pays for the default lookup. */
    double panMax  = mode->physical().focusPanMax();
    double tiltMax = mode->physical().focusTiltMax();
    if (panMax <= 0 || tiltMax <= 0)
    {
        const Fixture::EffectiveBeam eb = fx->effectiveBeam();
        if (panMax <= 0)  panMax  = eb.panMax  > 0 ? eb.panMax  : 360.0;
        if (tiltMax <= 0) tiltMax = eb.tiltMax > 0 ? eb.tiltMax : 270.0;
    }
    return beamDirection(rp,
                         degrees(panCh, h->channelNumber(QLCChannel::Pan, QLCChannel::LSB), panMax, panFrac),
                         degrees(tiltCh, h->channelNumber(QLCChannel::Tilt, QLCChannel::LSB), tiltMax, tiltFrac),
                         dir);
}

bool AimSolver::reach(Doc *doc, quint32 fixtureId, const QVector3D &targetWorld,
                      Solution &solution)
{
    Frame frame;
    if (frameFor(doc, fixtureId, frame) == false)
        return false;
    solution = solve(frame, targetWorld);
    return true;
}

bool AimSolver::aimDegrees(Doc *doc, quint32 fixtureId, const QVector3D &targetWorld,
                           float &panDeg, float &tiltDeg)
{
    Solution s;
    if (reach(doc, fixtureId, targetWorld, s) == false)
        return false;
    panDeg = s.panDeg;
    tiltDeg = s.tiltDeg;
    return true;
}

bool AimSolver::reach(Doc *doc, const GroupHead &member, const QVector3D &targetWorld,
                      Solution &solution)
{
    Frame frame;
    if (frameFor(doc, member, frame) == false)
        return false;
    solution = solve(frame, targetWorld);
    return true;
}

bool AimSolver::aimDegrees(Doc *doc, const GroupHead &member, const QVector3D &targetWorld,
                           float &panDeg, float &tiltDeg)
{
    Solution s;
    if (reach(doc, member, targetWorld, s) == false)
        return false;
    panDeg = s.panDeg;
    tiltDeg = s.tiltDeg;
    return true;
}

QVector3D AimSolver::aimPointFor(Doc *doc, const GroupHead &member, const StageTarget *target,
                                 bool subjectMode, bool framed)
{
    MonitorProperties *mProps = (doc != nullptr) ? doc->monitorProperties() : nullptr;
    if (mProps == nullptr || target == nullptr)
        return QVector3D();
    Frame frame;
    Fixture *fx = doc->fixture(member.fxi);
    if (fx == nullptr || !frameFor(doc, member, frame))
        return mProps->targetAimPoint(target, subjectMode);
    return mProps->targetAimPointFor(target, subjectMode, frame.position,
                                     RigHeads::beamAngle(fx, fx->channelValues()), framed);
}

QVector3D AimSolver::aimPointFor(Doc *doc, quint32 fixtureId, const StageTarget *target,
                                 bool subjectMode, bool framed)
{
    MonitorProperties *mProps = (doc != nullptr) ? doc->monitorProperties() : nullptr;
    if (mProps == nullptr || target == nullptr)
        return QVector3D();
    Frame frame;
    Fixture *fx = doc->fixture(fixtureId);
    if (fx == nullptr || !frameFor(doc, fixtureId, frame))
        return mProps->targetAimPoint(target, subjectMode);
    return mProps->targetAimPointFor(target, subjectMode, frame.position,
                                     RigHeads::beamAngle(fx, fx->channelValues()), framed);
}

bool AimSolver::sceneFollowsSubject(Doc *doc, const Scene *scene)
{
    if (doc == NULL || scene == NULL)
        return false;

    foreach (quint32 pid, scene->palettes())
    {
        QLCPalette *p = doc->palette(pid);
        if (p != NULL && p->type() == QLCPalette::Effect &&
            p->scriptPath().contains(QLatin1String("followspot"), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

bool AimSolver::sceneFramesSubject(Doc *doc, const Scene *scene)
{
    if (doc == NULL || scene == NULL)
        return false;
    for (quint32 pid : scene->palettes())
    {
        const QLCPalette *p = doc->palette(pid);
        if (p != NULL && p->type() == QLCPalette::Effect && p->followSizesBeam()
            && p->scriptPath().contains(QLatin1String("followspot"), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

/* aimable() into @a heads, cleared first. */
static void aimableInto(Doc *doc, const QList<quint32> &fixtures, QList<quint32> &heads)
{
    heads.clear();
    if (doc == NULL)
        return;
    MonitorProperties *mProps = doc->monitorProperties();
    for (quint32 fid : fixtures)
    {
        if (heads.contains(fid))
            continue;
        Fixture *fxi = doc->fixture(fid);
        if (fxi == NULL || (mProps != NULL && !mProps->containsFixture(fid)))
            continue;
        if (fxi->channelNumber(QLCChannel::Pan,  QLCChannel::MSB) == QLCChannel::invalid()
            && fxi->channelNumber(QLCChannel::Tilt, QLCChannel::MSB) == QLCChannel::invalid())
            continue;               // nothing to point
        heads << fid;
    }
}

QList<quint32> AimSolver::aimable(Doc *doc, const QList<quint32> &fixtures)
{
    QList<quint32> heads;
    aimableInto(doc, fixtures, heads);
    return heads;
}

QList<quint32> AimSolver::aimHeads(Doc *doc, const Scene *scene, quint32 paletteId)
{
    if (doc == NULL || scene == NULL)
        return QList<quint32>();
    if (paletteId == UINT_MAX)
        return aimHeads(doc, scene);
    /* What the look lays out over: a formation's span when it is one with
       other scenes, less the heads a lower layer aims itself. */
    return aimable(doc, scene->layoutFixtures(paletteId));
}

QList<quint32> AimSolver::aimHeads(Doc *doc, const Scene *scene)
{
    if (doc == NULL || scene == NULL)
        return QList<quint32>();

    QList<quint32> all;
    foreach (quint32 gid, scene->fixtureGroups())
    {
        FixtureGroup *fg = doc->fixtureGroup(gid);
        if (fg == NULL)
            continue;
        foreach (quint32 fid, fg->fixtureList())
            all << fid;
    }
    all << scene->fixtures();
    return aimable(doc, all);
}

QList<quint32> AimSolver::orderedLineHeads(Doc *doc, const StageTarget *target,
                                          const QList<quint32> &heads, LineSpread::Order order)
{
    if (doc == NULL || target == NULL || order != LineSpread::AlongLine)
        return heads;
    /* By where the heads hang, measured from the line's first end towards
       its last: the stage-left head takes the stage-left end, so a full
       spread never crosses beams by accident. */
    const QVector<QVector3D> pts = target->linePointsIn(doc->monitorProperties()->currentBlocking());
    if (pts.size() < 2)
        return heads;
    QVector<QVector3D> where;
    foreach (quint32 fid, heads)
    {
        Frame f;
        where << (frameFor(doc, fid, f) ? f.position : QVector3D());
    }
    QList<quint32> ordered;
    foreach (int i, LineSpread::orderAlong(where, pts.first(), pts.last()))
        ordered << heads.at(i);
    return ordered;
}

QList<GroupHead> AimSolver::orderedLineMembers(Doc *doc, const StageTarget *target,
                                            const QList<GroupHead> &members,
                                            LineSpread::Order order)
{
    if (doc == NULL || target == NULL || order != LineSpread::AlongLine)
        return members;
    const QVector<QVector3D> pts = target->linePointsIn(doc->monitorProperties()->currentBlocking());
    if (pts.size() < 2)
        return members;
    // Each arm by where it hangs: the two arms of a bar take neighbouring
    // places, the one nearer the line's start first.
    QVector<QVector3D> where;
    where.reserve(members.size());
    foreach (const GroupHead &m, members)
    {
        Frame f;
        where << (frameFor(doc, m, f) ? f.position : QVector3D());
    }
    QList<GroupHead> ordered;
    foreach (int i, LineSpread::orderAlong(where, pts.first(), pts.last()))
        ordered << members.at(i);
    return ordered;
}

QHash<GroupHead, double> AimSolver::lineMemberFractions(Doc *doc, const StageTarget *target,
                                                  const QList<GroupHead> &members,
                                                  const LineSpread::Settings &settings)
{
    QHash<GroupHead, double> out;
    if (doc == NULL || target == NULL || !target->isLine() || members.isEmpty())
        return out;
    const QList<GroupHead> ordered = orderedLineMembers(doc, target, members, settings.order);
    const QVector<double> fr = LineSpread::fractions(ordered.size(), settings);
    for (int i = 0; i < ordered.size(); ++i)
        out.insert(ordered.at(i), fr.at(i));
    return out;
}

QHash<quint32, double> AimSolver::lineFractions(Doc *doc, const StageTarget *target,
                                                const QList<quint32> &heads,
                                                const LineSpread::Settings &settings)
{
    QHash<quint32, double> out;
    if (doc == NULL || target == NULL || !target->isLine() || heads.isEmpty())
        return out;
    const QList<quint32> ordered = orderedLineHeads(doc, target, heads, settings.order);
    const QVector<double> fr = LineSpread::fractions(ordered.size(), settings);
    for (int i = 0; i < ordered.size(); ++i)
        out.insert(ordered.at(i), fr.at(i));
    return out;
}

QVector3D AimSolver::linePoint(Doc *doc, const StageTarget *target, double fraction)
{
    MonitorProperties *mProps = (doc != NULL) ? doc->monitorProperties() : NULL;
    if (mProps == NULL || target == NULL)
        return QVector3D();
    return LineCurve(mProps->targetLinePoints(target)).pointAt(fraction);
}

/* A head standing on the deck can't tilt below its own horizon: a point on
   the line lower than it (the riser's top, from a beam on the floor beside
   it) was out of reach, and the nearest it got was level, out over the house.
   It lights the people standing on the line instead -- the subject height
   above it. */
static QVector3D standingLift(Doc *doc, MonitorProperties *mProps, const GroupHead &member,
                              QVector3D at)
{
    AimSolver::Frame frame;
    if (AimSolver::frameFor(doc, member, frame) && frame.floorMounted
        && at.z() < frame.position.z() && !AimSolver::solve(frame, at).reachable())
        at.setZ(at.z() + mProps->aimSubjectHeight());
    return at;
}

QVector3D AimSolver::linePointFor(Doc *doc, quint32 fixtureId, const StageTarget *target,
                                  double fraction)
{
    MonitorProperties *mProps = (doc != NULL) ? doc->monitorProperties() : NULL;
    if (mProps == NULL || target == NULL)
        return QVector3D();
    return standingLift(doc, mProps, GroupHead(fixtureId, -1),
                        LineCurve(mProps->targetLinePoints(target)).pointAt(fraction));
}

QVector3D AimSolver::linePointFor(Doc *doc, const GroupHead &member, const StageTarget *target,
                                  double fraction)
{
    MonitorProperties *mProps = (doc != NULL) ? doc->monitorProperties() : NULL;
    if (mProps == NULL || target == NULL)
        return QVector3D();
    return standingLift(doc, mProps, member,
                        LineCurve(mProps->targetLinePoints(target)).pointAt(fraction));
}

QHash<quint32, QVector3D> AimSolver::linePoints(Doc *doc, const QLCPalette *look,
                                                const StageTarget *target,
                                                const QList<quint32> &heads)
{
    QHash<quint32, QVector3D> out;
    MonitorProperties *mProps = (doc != NULL) ? doc->monitorProperties() : NULL;
    if (mProps == NULL || look == NULL || target == NULL || !target->isLine())
        return out;
    const LineSpread::Settings settings = LineSpread::fromParams(look->effectParamValues());
    const QHash<quint32, double> fr = lineFractions(doc, target, heads, settings);
    const LineCurve curve(mProps->targetLinePoints(target));
    for (auto it = fr.constBegin(); it != fr.constEnd(); ++it)
        out.insert(it.key(), standingLift(doc, mProps, GroupHead(it.key(), -1),
                                          curve.pointAt(it.value())));
    return out;
}

QHash<GroupHead, QVector3D> AimSolver::lineMemberPoints(Doc *doc, const QLCPalette *look,
                                                  const StageTarget *target,
                                                  const QList<GroupHead> &members)
{
    QHash<GroupHead, QVector3D> out;
    MonitorProperties *mProps = (doc != NULL) ? doc->monitorProperties() : NULL;
    if (mProps == NULL || look == NULL || target == NULL || !target->isLine())
        return out;
    const LineSpread::Settings settings = LineSpread::fromParams(look->effectParamValues());
    const QHash<GroupHead, double> fr = lineMemberFractions(doc, target, members, settings);
    const LineCurve curve(mProps->targetLinePoints(target));
    for (auto it = fr.constBegin(); it != fr.constEnd(); ++it)
        out.insert(it.key(), standingLift(doc, mProps, it.key(), curve.pointAt(it.value())));
    return out;
}

BeamOcclusion::Hit AimSolver::lineOfSight(Doc *doc, const BeamOcclusion::Model &model,
                                          quint32 fixtureId, const QVector3D &from,
                                          const QVector3D &point)
{
    MonitorProperties *mProps = (doc != nullptr) ? doc->monitorProperties() : nullptr;
    if (mProps == nullptr || model.isEmpty())
    {
        BeamOcclusion::Hit clear;
        clear.clearance = BeamOcclusion::clearanceReach();
        clear.distance = (point - from).length();
        clear.point = point;
        return clear;
    }
    return model.segment(from, point, BeamOcclusion::ownMounts(mProps, fixtureId));
}

BeamOcclusion::Hit AimSolver::lineOfSight(Doc *doc, quint32 fixtureId, const QVector3D &point,
                                          const BeamOcclusion::Model *model)
{
    Frame frame;
    MonitorProperties *mProps = (doc != nullptr) ? doc->monitorProperties() : nullptr;
    if (mProps == nullptr || !frameFor(doc, fixtureId, frame))
    {
        BeamOcclusion::Hit clear;
        clear.clearance = BeamOcclusion::clearanceReach();
        clear.point = point;
        return clear;
    }
    if (model != nullptr)
        return lineOfSight(doc, *model, fixtureId, frame.position, point);
    const BeamOcclusion::ModelPtr current = mProps->occlusionModel(doc);
    return lineOfSight(doc, *current, fixtureId, frame.position, point);
}

static QList<AimSolver::SceneAim> collectAims(Doc *doc, const Scene *scene, bool dimmingOnly)
{
    QList<AimSolver::SceneAim> out;
    if (doc == NULL || scene == NULL)
        return out;
    MonitorProperties *mProps = doc->monitorProperties();
    if (mProps == NULL)
        return out;

    const bool subject = AimSolver::sceneFollowsSubject(doc, scene);
    BeamOcclusion::ModelPtr sightModel;
    foreach (quint32 pid, scene->runtimePalettes())   // a tried-on Aim draws too
    {
        QLCPalette *pal = doc->palette(pid);
        if (pal == NULL || pal->type() != QLCPalette::Aim)
            continue;
        if (dimmingOnly && !pal->dimWhenBlocked())
            continue;
        if (sightModel.isNull())
            sightModel = mProps->occlusionModel(doc);
        StageTarget *tgt = mProps->stageTarget(pal->stageTargetId());
        if (tgt == NULL)
            continue;
        // The heads it lays out over (a formation across several scenes:
        // all of them) and the ones this scene writes (the scene's groups'
        // members and its fixtures, less any it leaves out), once each.
        // Each arm of a multi-arm fixture is its own member.
        const QList<GroupHead> heads = AimSolver::aimMembers(doc, scene, pid);
        const QList<GroupHead> written = AimSolver::aimMembers(doc, scene->writtenFixtures(pid));
        // A line gives each head its own point on it.
        const QHash<GroupHead, QVector3D> onLine = AimSolver::lineMemberPoints(doc, pal, tgt, heads);
        // An area gives each head its own zone of it.
        const QHash<GroupHead, QVector3D> onArea = AreaCover::memberPoints(doc, tgt, heads);
        foreach (const GroupHead &m, written)
        {
            const quint32 fid = m.fxi;
            AimSolver::Frame frame;
            if (!AimSolver::frameFor(doc, m, frame))
                continue;
            // A line target: the head's own point on the line is what it
            // aims at, so that is the point whose sight is checked.
            const QVector3D point = tgt->isLine()
                    ? onLine.value(m, mProps->targetAimPoint(tgt))
                    : onArea.contains(m) ? onArea.value(m)
                                         : AimSolver::aimPointFor(doc, m, tgt, subject,
                                                                          subject && AimSolver::sceneFramesSubject(doc, scene));
            AimSolver::SceneAim aim;
            aim.fixtureId = fid;
            aim.head = m.head;
            aim.targetId = tgt->id();
            aim.paletteId = pal->id();
            aim.point = point;
            aim.solution = AimSolver::solve(frame, point);
            aim.sight = AimSolver::lineOfSight(doc, *sightModel, fid, frame.position, point);
            aim.dimsWhenBlocked = pal->dimWhenBlocked();
            out << aim;
        }
    }
    return out;
}

QList<AimSolver::SceneAim> AimSolver::sceneAims(Doc *doc, const Scene *scene)
{
    return collectAims(doc, scene, false);
}

/* What the sight check of every tick works in, one per thread (MasterTimer's,
   and the UI's when it asks): cleared and refilled each call, never freed,
   so once grown to the rig's size it allocates nothing. */
namespace
{
struct SightScratch
{
    QList<quint32> fixtures;
    QList<quint32> aimable;
    QList<GroupHead> heads;
    QList<GroupHead> written;
    QVector<BeamOcclusion::Exclude> mounts;
};
}

/* Every head an Aim look in @a scene that dims when blocked points at
   something it cannot see: @a blocked(fixtureId, hit) for each, in the order
   sceneAims() lists them. As collectAims(dimmingOnly) without the lists it
   builds: the members, written heads and mounts go into this thread's
   scratch. A head's point and sight are worked out afresh every call (a
   target that walks, a curtain driven by a fixture, a head that zooms). */
template <typename F>
static void forEachBlockedDimmed(Doc *doc, const Scene *scene, F blocked)
{
    if (doc == NULL || scene == NULL)
        return;
    MonitorProperties *mProps = doc->monitorProperties();
    if (mProps == NULL)
        return;
    BeamOcclusion::ModelPtr sightModel;
    bool subjectKnown = false, subject = false;
    const QList<quint32> pals = scene->runtimePalettes();   // a tried-on Aim draws too
    for (quint32 pid : pals)
    {
        QLCPalette *pal = doc->palette(pid);
        if (pal == NULL || pal->type() != QLCPalette::Aim || !pal->dimWhenBlocked())
            continue;
        if (sightModel.isNull())
            sightModel = mProps->occlusionModel(doc);
        StageTarget *tgt = mProps->stageTarget(pal->stageTargetId());
        if (tgt == NULL)
            continue;
        // Reached only when a look asks: a scene without one costs what
        // it did, not a thread-local lookup more.
        static thread_local SightScratch sc;
        if (!subjectKnown)
        {
            subject = AimSolver::sceneFollowsSubject(doc, scene);
            subjectKnown = true;
        }
        // As aimMembers(doc, scene, pid) and aimMembers(doc, writtenFixtures).
        sc.heads.clear();
        if (pid == UINT_MAX)
            sc.heads = AimSolver::aimMembers(doc, scene, pid);
        else
        {
            scene->layoutFixtures(pid, sc.fixtures);
            aimableInto(doc, sc.fixtures, sc.aimable);
            appendMembers(doc, sc.aimable, sc.heads);
        }
        scene->writtenFixtures(pid, sc.fixtures);
        aimableInto(doc, sc.fixtures, sc.aimable);
        sc.written.clear();
        appendMembers(doc, sc.aimable, sc.written);

        // Both empty, without a single allocation, unless the target is a
        // line (lineMemberPoints) or an area (AreaCover's kept zones).
        const QHash<GroupHead, QVector3D> onLine = AimSolver::lineMemberPoints(doc, pal, tgt, sc.heads);
        const QHash<GroupHead, QVector3D> onArea = AreaCover::memberPoints(doc, tgt, sc.heads);
        for (const GroupHead &m : sc.written)
        {
            AimSolver::Frame frame;
            if (!AimSolver::frameFor(doc, m, frame))
                continue;
            const QVector3D point = tgt->isLine()
                    ? onLine.value(m, mProps->targetAimPoint(tgt))
                    : onArea.contains(m) ? onArea.value(m)
                                         : AimSolver::aimPointFor(doc, m, tgt, subject,
                                                                          subject && AimSolver::sceneFramesSubject(doc, scene));
            // AimSolver::lineOfSight(), with the mounts in the scratch.
            if (sightModel->isEmpty())
                continue;                       // nothing to block it: clear
            BeamOcclusion::ownMounts(mProps, m.fxi, sc.mounts);
            const BeamOcclusion::Hit hit = sightModel->segment(frame.position, point, sc.mounts);
            if (hit.blocked)
                blocked(m.fxi, hit);
        }
    }
}

void AimSolver::blockedDimmedFixtures(Doc *doc, const Scene *scene, QVector<quint32> &into)
{
    into.clear();
    forEachBlockedDimmed(doc, scene, [&into](quint32 fid, const BeamOcclusion::Hit &) {
        if (!into.contains(fid))
            into << fid;
    });
}

QHash<quint32, BeamOcclusion::Hit> AimSolver::blockedDimmedHeads(Doc *doc, const Scene *scene)
{
    QHash<quint32, BeamOcclusion::Hit> out;
    if (doc == NULL || scene == NULL)
        return out;
    // The cheap question first: does any Aim look here ask for it at all?
    bool asked = false;
    foreach (quint32 pid, scene->runtimePalettes())
    {
        QLCPalette *pal = doc->palette(pid);
        if (pal != NULL && pal->type() == QLCPalette::Aim && pal->dimWhenBlocked())
        {
            asked = true;
            break;
        }
    }
    if (!asked)
        return out;
    foreach (const SceneAim &a, collectAims(doc, scene, true))
        if (a.sight.blocked && !out.contains(a.fixtureId))
            out.insert(a.fixtureId, a.sight);
    return out;
}
