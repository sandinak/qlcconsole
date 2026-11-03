/*
  Q Light Controller Plus - qlcconsole
  placementcheck.cpp

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

#include <QCoreApplication>
#include <QtMath>
#include <algorithm>

#include "placementcheck.h"
#include "monitorproperties.h"
#include "stageplatform.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "righeads.h"
#include "fixture.h"
#include "truss.h"
#include "doc.h"

static QString tr(const char *text)
{
    return QCoreApplication::translate("PlacementCheck", text);
}

bool PlacementCheck::body(Doc *doc, quint32 fid, QVector3D out[8])
{
    MonitorProperties *mp = doc != nullptr ? doc->monitorProperties() : nullptr;
    Fixture *fx = doc != nullptr ? doc->fixture(fid) : nullptr;
    if (mp == nullptr || fx == nullptr || !mp->containsFixture(fid))
        return false;
    const FixtureRigProps rp = mp->fixtureRigProps(fid);
    // Placed by a structure, not by hand.
    if (rp.trussId != Truss::invalidId() || rp.onPipe() || rp.onTower() || rp.onHanger()
        || rp.onRiser() || mp->fixtureFrameGroup(fid) != 0)
        return false;

    float w = 0.0f, h = 0.0f, d = 0.0f;
    if (const QLCFixtureMode *mode = fx->fixtureMode())
    {
        const QLCPhysical &phys = mode->physical();
        if (phys.width()  > 0) w = float(phys.width())  / 1000.0f;
        if (phys.height() > 0) h = float(phys.height()) / 1000.0f;
        if (phys.depth()  > 0) d = float(phys.depth())  / 1000.0f;
    }
    // The rig view's own defaults for a definition that does not say.
    if (w <= 0.0f) w = 0.3f;
    if (h <= 0.0f) h = w * 0.2f;
    if (d <= 0.0f) d = h;

    QVector3D L, N, H;
    RigHeads::frame(rp, L, N, H);
    const float seat = (rp.placement == FixtureRigProps::Inside) ? 0.0f : 0.5f;
    const QVector3D c = RigHeads::fixtureCentre(doc, fid) + N * (d * seat);
    const QVector3D l = L * (w * 0.5f), u = H * (h * 0.5f), n = N * (d * 0.5f);
    out[0] = c - l - u - n; out[1] = c + l - u - n; out[2] = c + l - u + n; out[3] = c - l - u + n;
    out[4] = c - l + u - n; out[5] = c + l + u - n; out[6] = c + l + u + n; out[7] = c - l + u + n;

    // The rig view centres the body on its position; the fixture really stands
    // on it (a floor light) or hangs from it (a hung one), so the body's low
    // (high) point is the position's height.
    float lo = out[0].z(), hi = out[0].z();
    for (int i = 1; i < 8; ++i)
    {
        lo = qMin(lo, out[i].z());
        hi = qMax(hi, out[i].z());
    }
    const float z = RigHeads::fixtureCentre(doc, fid).z();
    const float shift = (rp.mountingType == Truss::TopHung) ? (z - hi) : (z - lo);
    for (int i = 0; i < 8; ++i)
        out[i].setZ(out[i].z() + shift);
    return true;
}

namespace
{
/* Separating-axis test of two convex point sets; returns how far they overlap
   along the axis they overlap least on (<= 0: apart). */
float overlapDepth(const QVector3D *a, int na, const QVector3D *b, int nb, const QVector<QVector3D> &axes)
{
    float least = 1.0e9f;
    for (const QVector3D &axis0 : axes)
    {
        const float len = axis0.length();
        if (len < 1.0e-5f)
            continue;
        const QVector3D axis = axis0 / len;
        float amin = 1.0e9f, amax = -1.0e9f, bmin = 1.0e9f, bmax = -1.0e9f;
        for (int i = 0; i < na; ++i)
        {
            const float p = QVector3D::dotProduct(a[i], axis);
            amin = qMin(amin, p);
            amax = qMax(amax, p);
        }
        for (int i = 0; i < nb; ++i)
        {
            const float p = QVector3D::dotProduct(b[i], axis);
            bmin = qMin(bmin, p);
            bmax = qMax(bmax, p);
        }
        const float o = qMin(amax, bmax) - qMax(amin, bmin);
        if (o <= 0.0f)
            return o;
        least = qMin(least, o);
    }
    return least;
}

/* How deep the body is in one block of a platform: the block's volume is a box
   turned about the vertical with its footing at @a base. */
float depthInBlock(const QVector3D corners[8], const StagePlatform *pl,
                   const StagePlatform::Block &blk, float base)
{
    const QList<QPointF> f = pl->corners(blk);
    QVector3D box[8];
    for (int i = 0; i < 4; ++i)
    {
        box[i] = QVector3D(float(f.at(i).x()), float(f.at(i).y()), base);
        box[i + 4] = QVector3D(float(f.at(i).x()), float(f.at(i).y()), base + blk.height);
    }
    // The block's axes (two horizontal edges and the vertical) and the body's.
    QVector<QVector3D> axes;
    axes << (box[1] - box[0]) << (box[3] - box[0]) << QVector3D(0, 0, 1);
    const QVector3D e1 = corners[1] - corners[0], e2 = corners[3] - corners[0], e3 = corners[4] - corners[0];
    axes << e1 << e2 << e3;
    const QVector3D be[3] = { box[1] - box[0], box[3] - box[0], QVector3D(0, 0, 1) };
    const QVector3D fe[3] = { e1, e2, e3 };
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            axes << QVector3D::crossProduct(be[i], fe[j]);
    return overlapDepth(corners, 8, box, 8, axes);
}
}

PlacementCheck::Buried PlacementCheck::check(Doc *doc, quint32 fid)
{
    Buried out;
    QVector3D c[8];
    if (!body(doc, fid, c))
        return out;
    MonitorProperties *mp = doc->monitorProperties();
    const FixtureRigProps rp = mp->fixtureRigProps(fid);
    const QVector3D mid = (c[0] + c[6]) * 0.5f;

    for (StagePlatform *pl : mp->platforms())
    {
        if (pl == nullptr || !pl->solid() || pl->topMaterial() != StagePlatform::SolidTop)
            continue;
        // Standing on this one (a deck mount) is what its volume is for.
        if (rp.onDeck() && rp.deckPlatformId == pl->id())
            continue;
        const float base = mp->platformBaseZ(pl->id());
        float deepest = 0.0f;
        for (const StagePlatform::Block &blk : pl->blocks())
            deepest = qMax(deepest, depthInBlock(c, pl, blk, base));
        if (deepest <= tolerance())
            continue;
        if (deepest > out.depth)
        {
            out.fixtureId = fid;
            out.platformId = pl->id();
            out.depth = deepest;
        }
    }
    if (out.depth <= 0.0f)
        return out;
    out.buried = true;
    out.fixtureId = fid;

    // What it could stand on: the highest solid top that its middle is over.
    float best = -1.0f;
    for (StagePlatform *pl : mp->platforms())
    {
        if (pl == nullptr || !pl->solid() || pl->topMaterial() != StagePlatform::SolidTop
            || !pl->containsPoint(mid.x(), mid.y()))
            continue;
        const float top = mp->platformBaseZ(pl->id()) + pl->topAt(mid.x(), mid.y());
        if (top > best)
        {
            best = top;
            out.canSit = true;
            out.sitId = pl->id();
            out.topZ = top;
        }
    }
    return out;
}

QList<PlacementCheck::Buried> PlacementCheck::buried(Doc *doc)
{
    QList<Buried> out;
    MonitorProperties *mp = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (mp == nullptr || mp->platforms().isEmpty())
        return out;
    QList<quint32> ids = mp->fixtureItemsID();
    std::sort(ids.begin(), ids.end());
    for (quint32 fid : ids)
    {
        const Buried b = check(doc, fid);
        if (b.buried)
            out << b;
    }
    return out;
}

static QString platformName(Doc *doc, quint32 id)
{
    const StagePlatform *pl = doc->monitorProperties()->platform(id);
    return pl != nullptr ? pl->name() : QString();
}

QString PlacementCheck::problemText(Doc *doc, const Buried &b)
{
    const Fixture *fx = doc->fixture(b.fixtureId);
    return tr("%1 is inside %2").arg(fx != nullptr ? fx->name() : QString::number(b.fixtureId),
                                     platformName(doc, b.platformId));
}

QString PlacementCheck::fixText(Doc *doc, const Buried &b)
{
    if (b.canSit)
        return tr("Sit it on %1 (%2 m)").arg(platformName(doc, b.sitId)).arg(double(b.topZ), 0, 'f', 2);
    return tr("Move it clear of %1").arg(platformName(doc, b.platformId));
}

bool PlacementCheck::fix(Doc *doc, const Buried &b)
{
    MonitorProperties *mp = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (mp == nullptr || !b.canSit || mp->platform(b.sitId) == nullptr || !mp->containsFixture(b.fixtureId))
        return false;
    FixtureRigProps rp = mp->fixtureRigProps(b.fixtureId);
    rp.deckPlatformId = b.sitId;
    rp.deckHeightOffset = 0.0f;
    rp.mountingType = Truss::FloorMounted;
    mp->setFixtureRigProps(b.fixtureId, rp);
    doc->setModified();
    return true;
}
