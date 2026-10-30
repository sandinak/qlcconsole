/*
  Q Light Controller Plus
  righeads.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtMath>
#include <cmath>

#include "monitorproperties.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "qlcphysical.h"
#include "headlayout.h"
#include "righeads.h"
#include "fixture.h"
#include "truss.h"
#include "stageplatform.h"
#include "doc.h"

/* Its length within its face, before any turn about the vertical. */
static QVector3D faceAxis(const FixtureRigProps &rp)
{
    const double th = qDegreesToRadians(double(rp.studioAngle));
    const double c = qCos(th), s = qSin(th);
    switch (rp.studioMount)
    {
    case 0:  return QVector3D(float(c), float(s), 0.0f);   // Top/deck: X-Y plane
    case 2:  return QVector3D(0.0f, float(c), float(s));   // Side: Y-Z plane
    case 1:
    default: return QVector3D(float(c), 0.0f, float(s));   // Front/face: X-Z plane
    }
}

static QVector3D yawed(const FixtureRigProps &rp, const QVector3D &v)
{
    if (qFuzzyIsNull(rp.studioYaw))
        return v;
    const double y = qDegreesToRadians(double(rp.studioYaw));
    const float c = float(qCos(y)), sn = float(qSin(y));
    return QVector3D(v.x() * c - v.y() * sn, v.x() * sn + v.y() * c, v.z());
}

QVector3D RigHeads::axis(const FixtureRigProps &rp)
{
    return yawed(rp, faceAxis(rp));
}

bool RigHeads::fixedThrow(Doc *doc, quint32 fid, int headCount, bool mover, QVector3D &dir)
{
    if (doc == nullptr || doc->monitorProperties() == nullptr)
        return false;
    MonitorProperties *props = doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    if (rp.placement == FixtureRigProps::Inside)
        return false;
    // A fixed light aimed by pan and tilt (Q100): where it was pointed.
    if (!mover && rp.fixedAimDirection(dir))
        return true;

    /* Out of its FACE. A bar's light leaves the face its pixels are on -- the
       FART bars hang vertically off their truss facing the audience, and
       "hung means pointing down" sent their light to the floor behind the
       truss. Likewise anything whose face was chosen: a laser on the back of
       a truss, faced Back, throws upstage. Tilt and flip included. */
    if (!mover && (headCount > 1 || rp.studioFaceChosen))
    {
        QVector3D L, N, H;
        frame(rp, L, N, H);
        if (N.lengthSquared() > 1e-6f)
        {
            dir = N.normalized();
            return true;
        }
    }

    const quint32 plId = rp.onRiser() ? rp.riserPlatformId
                       : rp.onDeck()  ? rp.deckPlatformId
                                      : FixtureRigProps::invalidPlatformId();
    if (StagePlatform *host = props->platform(plId))
    {
        const float b0 = props->platformBaseZ(plId);
        const QVector3D hostCentre(host->originX() + host->width() * 0.5f,
                                   host->originY() + host->depth() * 0.5f,
                                   b0 + host->height() * 0.5f);
        dir = fixtureCentre(doc, fid) - hostCentre;
        if (dir.length() < 1e-4f)
            return false;
        dir.normalize();
        return true;
    }
    if (rp.trussId != Truss::invalidId())
    {
        /* A truss runs along its length, so "out" across it is meaningless --
           but a hung fixture points DOWN and a floor-mounted one points up,
           which is the part that matters. */
        dir = (rp.mountingType == Truss::TopHung) ? QVector3D(0.0f, 0.0f, -1.0f)
                                                  : QVector3D(0.0f, 0.0f, 1.0f);
        return true;
    }
    return false;
}

bool RigHeads::fixedThrow(Doc *doc, quint32 fid, QVector3D &dir)
{
    const Fixture *fx = doc != nullptr ? doc->fixture(fid) : nullptr;
    if (fx == nullptr)
        return false;
    const bool mover = fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB, 0) != QLCChannel::invalid()
                    || fx->channelNumber(QLCChannel::Tilt, QLCChannel::MSB, 0) != QLCChannel::invalid();
    return fixedThrow(doc, fid, qMax(1, fx->heads()), mover, dir);
}

bool RigHeads::isLinear(const Fixture *fx)
{
    if (fx == nullptr)
        return false;
    if (fx->heads() > 1)
        return true;                                  // LED tape / multi-cell bar
    if (const QLCFixtureMode *m = fx->fixtureMode())  // a wide single-head bar/wash
    {
        const qreal w = m->physical().width();
        const qreal d = qMax(m->physical().height(), m->physical().depth());
        if (w > 0 && d > 0 && w > 1.8 * d)
            return true;
    }
    return false;
}

bool RigHeads::isFixedUnit(const Fixture *fx)
{
    return fx != nullptr
        && fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB, 0) == QLCChannel::invalid()
        && fx->channelNumber(QLCChannel::Tilt, QLCChannel::MSB, 0) == QLCChannel::invalid()
        && !isLinear(fx);
}

/* The face's outward normal before any roll, flip or turn. */
static QVector3D planeNormal(int mount)
{
    switch (mount)
    {
    case 0:  return QVector3D(0, 0, 1);
    case 2:  return QVector3D(1, 0, 0);
    case 1:
    default: return QVector3D(0, 1, 0);
    }
}

float RigHeads::tiltSign(const FixtureRigProps &rp)
{
    /* frame() rolls N toward -H. On an upright face N is level, so the roll
       lifts it when H points down -- true of a Front bar running stage left
       to right, not of one on the Back (flipped) or the Stage left face, where
       "+20" used to tip the light at the floor. Turn those round. A Top or
       Bottom face has a level H: nothing to lift, and it keeps its old sense
       ("positive tips it upstage"). Yaw is about the vertical: no matter. */
    QVector3D h = QVector3D::crossProduct(planeNormal(rp.studioMount), faceAxis(rp));
    if (rp.studioFlip)
        h = -h;
    return (h.z() > 1e-3f) ? -1.0f : 1.0f;
}

void RigHeads::aimAtFace(FixtureRigProps &rp)
{
    QVector3D L, N, H;
    frame(rp, L, N, H);
    const bool hadElevation = rp.fixedAim && qAbs(rp.aimDown) < 89.0f;
    const float plan = std::hypot(N.x(), N.y());
    if (plan < 0.26f)               // Top or Bottom: straight up or down
        rp.aimDown = (N.z() < 0.0f) ? 90.0f : -90.0f;
    else
    {
        // The bearing convention of fixedAimDirection(): dir = (-sin b, cos b).
        double b = qRadiansToDegrees(qAtan2(-double(N.x()), double(N.y())));
        b = std::fmod(qRound(b * 10.0) / 10.0 + 360.0, 360.0);
        rp.panZeroDir = float(b);
        if (!hadElevation)
            rp.aimDown = float(qRound(qRadiansToDegrees(qAsin(qBound(-1.0, -double(N.z()), 1.0))) * 10.0) / 10.0);
    }
    rp.fixedAim = true;
}

void RigHeads::startFixedAim(Doc *doc, quint32 fid, FixtureRigProps &rp)
{
    if (rp.fixedAim)
        return;
    QVector3D d;
    if (!fixedThrow(doc, fid, d))
        d = (rp.mountingType == Truss::TopHung) ? QVector3D(0, 0, -1) : QVector3D(0, 0, 1);
    rp.aimDown = float(qRound(qRadiansToDegrees(qAsin(qBound(-1.0, -double(d.z()), 1.0))) * 10.0) / 10.0);
    if (std::hypot(d.x(), d.y()) > 0.26f)
    {
        const double b = qRadiansToDegrees(qAtan2(-double(d.x()), double(d.y())));
        rp.panZeroDir = float(std::fmod(qRound(b * 10.0) / 10.0 + 360.0, 360.0));
    }
    rp.fixedAim = true;
}

int RigHeads::nearestFace(const QVector3D &dir)
{
    if (dir.lengthSquared() < 1e-8f)
        return -1;
    const QVector3D d = dir.normalized();
    // In FixtureRigProps::Face order.
    static const QVector3D out[] = { QVector3D(0, 0, 1), QVector3D(0, 0, -1),
                                     QVector3D(0, 1, 0), QVector3D(0, -1, 0),
                                     QVector3D(1, 0, 0), QVector3D(-1, 0, 0) };
    int best = -1;
    float bestDot = 0.86f;          // within about 30 degrees
    for (int i = 0; i < 6; ++i)
    {
        const float dot = QVector3D::dotProduct(d, out[i]);
        if (dot > bestDot)
        {
            bestDot = dot;
            best = i;
        }
    }
    return best;
}

void RigHeads::frame(const FixtureRigProps &rp, QVector3D &L, QVector3D &N, QVector3D &H)
{
    L = faceAxis(rp);
    switch (rp.studioMount)
    {
    case 0:  N = QVector3D(0, 0, 1); break;            // laid flat: depth is up
    case 2:  N = QVector3D(1, 0, 0); break;            // side face: depth across stage
    case 1:
    default: N = QVector3D(0, 1, 0); break;            // front face: depth upstage
    }
    H = QVector3D::crossProduct(N, L);
    if (H.length() < 1e-6f)
        H = QVector3D(0, 0, 1);
    H.normalize();

    /* ROLL about the fixture's own length. A bar lying in a step is built on a
       diagonal inside the box so it fires up through the plexi toward the cast,
       not straight up -- studioAngle cannot say that, because it turns the
       fixture WITHIN its face rather than tipping the face. Rotating N and H
       about L keeps the frame orthonormal, so everything downstream (body,
       pixels, beam direction) follows without knowing this exists. */
    if (!qFuzzyIsNull(rp.studioTilt))
    {
        // Signed so that + tips it UP on an upright face (tiltSign()).
        const double t = qDegreesToRadians(double(rp.studioTilt * tiltSign(rp)));
        const double c = qCos(t), sn = qSin(t);
        const QVector3D n0 = N, h0 = H;
        // Positive tips the face AWAY from H. For a bar lying on a top face
        // and running stage left-right, H is downstage, so +20 tips it upstage
        // -- which is the direction you would describe it in.
        N = (n0 * float(c) - h0 * float(sn)).normalized();
        H = (h0 * float(c) + n0 * float(sn)).normalized();
    }

    // The other side of its plane: Back, Bottom, Stage right. A roll of 180
    // degrees about the length, so the frame stays right-handed.
    if (rp.studioFlip)
    {
        N = -N;
        H = -H;
    }

    // Turned about the vertical with whatever it is part of (a group, the
    // platform or tower it is on): the whole frame, last.
    L = yawed(rp, L);
    N = yawed(rp, N);
    H = yawed(rp, H);
}

QVector3D RigHeads::fixtureCentre(Doc *doc, quint32 fid)
{
    if (doc == nullptr || doc->monitorProperties() == nullptr)
        return QVector3D();

    MonitorProperties *props = doc->monitorProperties();
    const QVector3D w = props->fixtureRigPosition(fid);
    const FixtureRigProps rp = props->fixtureRigProps(fid);

    /* A DERIVED position is already a centre: a truss, pipe, tower or riser
       mount computes the point from the structure, and the 2D plot centres its
       icon on it to match. Only the two cases that read the stored plot XY
       straight back need the anchor undone -- free placement, and a deck mount
       (which keeps the free XY and derives only Z). */
    /* On a hanger the position is the HOOK, and the fixture hangs below it:
       its centre is half its height lower. */
    if (rp.onHanger())
    {
        Fixture *hung = doc->fixture(fid);
        const float h = (hung != nullptr && hung->fixtureMode() != nullptr
                         && hung->fixtureMode()->physical().height() > 0)
                        ? float(hung->fixtureMode()->physical().height()) / 1000.0f : 0.1f;
        return w - QVector3D(0.0f, 0.0f, h * 0.5f);
    }
    if (props->fixtureFrameGroup(fid) != 0)
        return w;
    const bool derived = rp.trussId != Truss::invalidId() || rp.onPipe()
                      || rp.onTower() || rp.onRiser();
    if (derived)
        return w;

    Fixture *fx = doc->fixture(fid);
    if (fx == nullptr || fx->fixtureMode() == nullptr)
        return w;
    const QLCPhysical &phys = fx->fixtureMode()->physical();
    const float physW = phys.width() > 0 ? float(phys.width()) / 1000.0f : 0.0f;
    const float physH = phys.height() > 0 ? float(phys.height()) / 1000.0f : 0.0f;
    if (physW <= 0.0f && physH <= 0.0f)
        return w;

    /* Width and HEIGHT, not width and depth, however odd that reads for a
       top-down view: MonitorGraphicsView::updateFixture() sizes its icon
       physical().width() x physical().height() in every view, plan included,
       so the corner the plot anchored is the corner of a W x H box. */
    return QVector3D(w.x() + physW * 0.5f, w.y() + physH * 0.5f, w.z());
}

QVector<QVector3D> RigHeads::headPositions(Doc *doc, quint32 fid)
{
    QVector<QVector3D> out;
    Fixture *fx = (doc != nullptr) ? doc->fixture(fid) : nullptr;
    if (fx == nullptr || doc->monitorProperties() == nullptr)
        return out;
    MonitorProperties *props = doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const QVector3D c = fixtureCentre(doc, fid);
    const int heads = qMax(1, fx->heads());

    // The body the rig view builds (StructureStudioView::fixtureBoxCorners()).
    float physW = 0.0f, physH = 0.0f, physD = 0.0f;
    QSize grid;
    if (const QLCFixtureMode *mode = fx->fixtureMode())
    {
        const QLCPhysical &phys = mode->physical();
        if (phys.width()  > 0) physW = float(phys.width())  / 1000.0f;
        if (phys.height() > 0) physH = float(phys.height()) / 1000.0f;
        if (phys.depth()  > 0) physD = float(phys.depth())  / 1000.0f;
        grid = HeadLayout::declaredGrid(mode);
    }
    const double w = physW > 0.0f ? double(physW) : 0.3;
    const double h = physH > 0.0f ? double(physH) : w * 0.2;
    const double d = physD > 0.0f ? double(physD) : h;
    QVector3D L, N, H;
    frame(rp, L, N, H);
    const double seat = (rp.placement == FixtureRigProps::Inside) ? 0.0 : 0.5;
    const QVector3D c2 = c + N * float(d * seat);

    /* Its pixel grid, and which head is in which cell
       (StructureStudioView::headGrid() and headsByCell()): the LED map's
       when there is one, else the declared grid, else one row. */
    int cols = grid.isValid() ? grid.width() : heads;
    int rows = grid.isValid() ? grid.height() : 1;
    QVector<QPoint> cellOf(heads, QPoint(-1, -1));
    bool mapped = false;
    if (heads > 1 && props->hasHeadLayout(fx))
    {
        const HeadLayout map = props->headLayoutFor(fx);
        if (map.isValid() && map.cells.size() >= heads)
        {
            cols = map.size.width();
            rows = map.size.height();
            for (int hd = 0; hd < heads; ++hd)
                cellOf[hd] = map.cellOf(hd);
            mapped = true;
        }
    }
    if (!mapped)
        for (int hd = 0; hd < heads; ++hd)
            cellOf[hd] = QPoint(hd % cols, hd / cols);

    out.resize(heads);
    for (int hd = 0; hd < heads; ++hd)
    {
        const QPoint cell = cellOf.at(hd);
        if (cell.x() < 0 || cell.y() < 0)
        {
            out[hd] = c2;
            continue;
        }
        // Across the face as the rig view paints it: the end cells at the ends.
        const double fc = cols > 1 ? double(cell.x()) / (cols - 1) : 0.5;
        const double fr = rows > 1 ? double(cell.y()) / (rows - 1) : 0.5;
        out[hd] = c2 + L * float(w * (fc - 0.5)) + H * float(h * (fr - 0.5));
    }
    return out;
}

int RigHeads::lightLeds(Doc *doc, quint32 fid, int deviceLeds, int *layout)
{
    Fixture *fx = (doc != nullptr) ? doc->fixture(fid) : nullptr;
    if (fx == nullptr || doc->monitorProperties() == nullptr)
        return 0;
    const FixtureRigProps rp = doc->monitorProperties()->fixtureRigProps(fid);
    const QLCFixtureMode *mode = fx->fixtureMode();
    const int defLeds = mode != nullptr ? mode->physical().leds() : 0;
    const int defLayout = mode != nullptr ? int(mode->physical().ledLayout())
                                          : int(QLCPhysical::ClusterLeds);
    if (layout != nullptr)
        *layout = rp.lightLayout >= 0 ? rp.lightLayout : defLayout;

    if (rp.lightLeds >= 0)
        return rp.lightLeds;                // this one's real build
    if (defLeds <= 0)
        return 0;                           // a point, as the definition says
    return deviceLeds > 0 ? deviceLeds : defLeds;
}

QVector<QVector3D> RigHeads::lightPoints(Doc *doc, quint32 fid, int deviceLeds)
{
    QVector<QVector3D> out;
    int layout = QLCPhysical::ClusterLeds;
    const int n = qMin(lightLeds(doc, fid, deviceLeds, &layout), 1024);
    if (n <= 0)
        return out;

    Fixture *fx = doc->fixture(fid);
    const FixtureRigProps rp = doc->monitorProperties()->fixtureRigProps(fid);
    float physW = 0.0f, physH = 0.0f, physD = 0.0f;
    if (const QLCFixtureMode *mode = fx->fixtureMode())
    {
        const QLCPhysical &phys = mode->physical();
        if (phys.width()  > 0) physW = float(phys.width())  / 1000.0f;
        if (phys.height() > 0) physH = float(phys.height()) / 1000.0f;
        if (phys.depth()  > 0) physD = float(phys.depth())  / 1000.0f;
    }
    // The body the rig view builds (headPositions() above).
    const double w = physW > 0.0f ? double(physW) : 0.3;
    const double h = physH > 0.0f ? double(physH) : w * 0.2;
    const double d = physD > 0.0f ? double(physD) : h;
    QVector3D L, N, H;
    frame(rp, L, N, H);
    const double seat = (rp.placement == FixtureRigProps::Inside) ? 0.0 : 0.5;
    const QVector3D c = fixtureCentre(doc, fid) + N * float(d * seat);
    // The first LED at the TOP, whichever way the frame's H points: a string
    // is wired from where it hangs.
    const QVector3D up = (H.z() < 0.0f) ? -H : H;

    out.resize(n);
    for (int i = 0; i < n; ++i)
    {
        const double t = (i + 0.5) / n;
        switch (layout)
        {
        case QLCPhysical::TapeLeds:
        {
            /* LED tape wound tight round a core in the middle: a narrow
               helix, eight to a turn, top to bottom. Less spread than a
               cluster -- the light comes from one bright column. */
            const double a = 2.0 * M_PI * i / 8.0;
            const double rx = qMax(0.008, 0.07 * w), ry = qMax(0.008, 0.07 * d);
            out[i] = c + L * float(rx * qCos(a)) + N * float(ry * qSin(a))
                       + up * float(h * (0.18 - 0.36 * t));
            break;
        }
        case QLCPhysical::ClusterLeds:
        default:
        {
            /* A wire string hung from the top and let fall in a loose bunch:
               through most of the cage, top to bottom in order (a golden-
               angle spiral), each a different way in or out -- the same
               every time, so a frame never shimmers. */
            const double zz = 1.0 - 2.0 * t;
            const double rr = qSqrt(qMax(0.0, 1.0 - zz * zz));
            const double phi = i * 2.399963;
            double jitter = qSin(i * 12.9898) * 43758.5453;
            jitter = 0.40 + 0.60 * (jitter - qFloor(jitter));
            out[i] = c + L * float(0.36 * w * jitter * rr * qCos(phi))
                       + N * float(0.36 * d * jitter * rr * qSin(phi))
                       + up * float(0.28 * h * zz);
            break;
        }
        }
    }
    return out;
}

bool RigHeads::clearGlass(Doc *doc, quint32 fid)
{
    Fixture *fx = (doc != nullptr) ? doc->fixture(fid) : nullptr;
    if (fx == nullptr || doc->monitorProperties() == nullptr)
        return false;
    const int own = doc->monitorProperties()->fixtureRigProps(fid).lightGlass;
    if (own >= 0)
        return own == 1;
    return fx->fixtureMode() != nullptr && fx->fixtureMode()->physical().clearGlass();
}

bool RigHeads::hasZoomChannel(const Fixture *fx)
{
    const QLCFixtureMode *mode = (fx != nullptr) ? fx->fixtureMode() : nullptr;
    if (mode == nullptr)
        return false;
    for (const QLCChannel *ch : mode->channels())
        if (ch != nullptr && (ch->preset() == QLCChannel::BeamZoomSmallBig
                              || ch->preset() == QLCChannel::BeamZoomBigSmall))
            return true;
    return false;
}

bool RigHeads::lensRange(const Fixture *fx, double &lo, double &hi)
{
    const QLCFixtureMode *mode = (fx != nullptr) ? fx->fixtureMode() : nullptr;
    if (mode == nullptr)
        return false;
    const QLCPhysical &phys = mode->physical();
    lo = qMin(phys.lensDegreesMin(), phys.lensDegreesMax());
    hi = qMax(phys.lensDegreesMin(), phys.lensDegreesMax());
    return lo > 0.0 && hi - lo >= 0.51;
}

double RigHeads::setLensAngle(const Fixture *fx)
{
    const Doc *doc = (fx != nullptr) ? qobject_cast<const Doc *>(fx->parent()) : nullptr;
    MonitorProperties *props = (doc != nullptr) ? const_cast<Doc *>(doc)->monitorProperties() : nullptr;
    double lo = 0.0, hi = 0.0;
    if (props == nullptr || !lensRange(fx, lo, hi))
        return 0.0;
    const float set = props->fixtureRigProps(fx->id()).lensDeg;
    return set > 0.0f ? qBound(lo, double(set), hi) : 0.0;
}

double RigHeads::beamAngle(const Fixture *fx, const QByteArray &values)
{
    const QLCFixtureMode *mode = (fx != nullptr) ? fx->fixtureMode() : nullptr;
    if (mode == nullptr)
        return 0.0;
    const QLCPhysical &phys = mode->physical();
    const double lo = qMin(phys.lensDegreesMin(), phys.lensDegreesMax());
    const double hi = qMax(phys.lensDegreesMin(), phys.lensDegreesMax());
    if (hi <= 0.0)
        return 0.0;                       // no lens declared at all

    double angle = hi;                    // a fixed cone, or no zoom to read
    // Zoomed by hand (no zoom channel): what its lens was set to (Q103).
    const double setTo = setLensAngle(fx);
    if (setTo > 0.0 && !hasZoomChannel(fx))
        angle = setTo;
    double open = 1.0;                    // the iris, fully open
    const int count = qMin(mode->channels().size(), values.size());
    for (int c = 0; c < count; ++c)
    {
        const QLCChannel *ch = mode->channel(quint32(c));
        if (ch == nullptr)
            continue;
        const double v = uchar(values.at(c)) / 255.0;
        switch (ch->preset())
        {
        case QLCChannel::BeamZoomSmallBig:
            if (lo > 0.0 && hi - lo >= 0.51)
                angle = lo + (hi - lo) * v;
            break;
        case QLCChannel::BeamZoomBigSmall:
            if (lo > 0.0 && hi - lo >= 0.51)
                angle = lo + (hi - lo) * (1.0 - v);
            break;
        case QLCChannel::ShutterIrisMinToMax:
            open = v;
            break;
        case QLCChannel::ShutterIrisMaxToMin:
            open = 1.0 - v;
            break;
        default:
            break;
        }
    }
    /* An iris cuts the cone down from its edge: the beam it leaves is the
       same lens with a smaller hole, its half-angle's tangent in proportion.
       Never quite shut -- a shut iris is the dimmer's business, not a
       zero-width beam. */
    if (open < 1.0)
    {
        const double t = qMax(0.1, open) * qTan(qDegreesToRadians(angle * 0.5));
        angle = 2.0 * qRadiansToDegrees(qAtan(t));
    }
    return angle;
}
