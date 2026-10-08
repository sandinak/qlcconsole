/*
  Q Light Controller Plus
  righeads.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtMath>

#include "monitorproperties.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "headlayout.h"
#include "righeads.h"
#include "fixture.h"
#include "truss.h"
#include "doc.h"

QVector3D RigHeads::axis(const FixtureRigProps &rp)
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

void RigHeads::frame(const FixtureRigProps &rp, QVector3D &L, QVector3D &N, QVector3D &H)
{
    L = axis(rp);
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
        const double t = qDegreesToRadians(double(rp.studioTilt));
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

    // Its pixel grid, and which head is in which cell (StructureStudioView::headsByCell()).
    const int cols = grid.isValid() ? grid.width() : heads;
    const int rows = grid.isValid() ? grid.height() : 1;
    QVector<QPoint> cellOf(heads, QPoint(-1, -1));
    bool mapped = false;
    if (props->hasHeadLayout(fx))
    {
        const HeadLayout map = props->headLayoutFor(fx);
        if (map.size == QSize(cols, rows))
        {
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
