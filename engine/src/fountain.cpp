/*
  Q Light Controller Plus
  fountain.cpp

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

#include "fountain.h"
#include "monitorproperties.h"
#include "stageplatform.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcphysical.h"
#include "righeads.h"
#include "scenery.h"
#include "fixture.h"
#include "truss.h"
#include "doc.h"

Fountain::Parts Fountain::ofGroup(MonitorProperties *props, quint32 groupId)
{
    Parts f;
    if (props == nullptr || groupId == 0)
        return f;
    StagePlatform *base = nullptr;
    foreach (StagePlatform *pl, props->platforms())
        if (pl != nullptr && pl->groupId() == groupId)
        {
            if (base != nullptr)
                return f;           // two decks: not a fountain
            base = pl;
        }
    if (base == nullptr)
        return f;
    const float top = props->platformBaseZ(base->id()) + base->height();
    foreach (Scenery *s, props->sceneries())
    {
        if (s == nullptr || s->groupId() != groupId || s->kind() != Scenery::Wall
            || s->outline() != Scenery::RoundedU || qAbs(s->bottom() - top) > 0.02f)
            continue;
        const float st = s->bottom() + s->height();
        if (f.inner == nullptr || st > f.inner->bottom() + f.inner->height())
            f.inner = s;
        if (f.outer == nullptr || st < f.outer->bottom() + f.outer->height())
            f.outer = s;
    }
    if (f.inner != nullptr)
    {
        f.groupId = groupId;
        f.base = base;
    }
    else
        f.outer = nullptr;
    return f;
}

Fountain::Parts Fountain::ofFixture(MonitorProperties *props, quint32 fid)
{
    return props != nullptr ? ofGroup(props, props->fixtureGroup(fid)) : Parts();
}

Fountain::Parts Fountain::ofBase(MonitorProperties *props, quint32 platformId)
{
    StagePlatform *pl = props != nullptr ? props->platform(platformId) : nullptr;
    if (pl == nullptr || pl->groupId() == 0)
        return Parts();
    const Parts f = ofGroup(props, pl->groupId());
    return f.base == pl ? f : Parts();
}

Fountain::Parts Fountain::at(MonitorProperties *props, double x, double y)
{
    if (props == nullptr)
        return Parts();
    foreach (StagePlatform *pl, props->platforms())
        if (pl != nullptr && pl->groupId() != 0 && pl->containsPoint(float(x), float(y)))
        {
            const Parts f = ofBase(props, pl->id());
            if (f.isValid())
                return f;
        }
    return Parts();
}

float Fountain::innerTop(const Parts &f)
{
    return f.inner != nullptr ? f.inner->bottom() + f.inner->height() : 0.0f;
}

float Fountain::baseTop(MonitorProperties *props, const Parts &f)
{
    return (f.base != nullptr && props != nullptr)
        ? props->platformBaseZ(f.base->id()) + f.base->height() : 0.0f;
}

bool Fountain::onInnerWall(const Parts &f, double x, double y)
{
    if (f.inner == nullptr)
        return false;
    const QList<QPointF> line = f.inner->baseLine(32);
    const QPointF p(x, y);
    const double reach = double(f.inner->thickness()) / 2.0 + 0.03;
    for (int i = 0; i + 1 < line.size(); ++i)
    {
        const QPointF a = line.at(i), ab = line.at(i + 1) - a;
        const double len2 = QPointF::dotProduct(ab, ab);
        const double t = len2 > 0.0 ? qBound(0.0, QPointF::dotProduct(p - a, ab) / len2, 1.0) : 0.0;
        const QPointF d = p - (a + ab * t);
        if (qSqrt(QPointF::dotProduct(d, d)) <= reach)
            return true;
    }
    return false;
}

static QLCPhysical physicalOf(const Fixture *fx)
{
    if (fx == nullptr)
        return QLCPhysical();
    if (fx->fixtureMode() != nullptr)
        return fx->fixtureMode()->physical();
    return fx->fixtureDef() != nullptr ? fx->fixtureDef()->physical() : QLCPhysical();
}

bool Fountain::isFlame(const Fixture *fx)
{
    return fx != nullptr && physicalOf(fx).emission() == QLCPhysical::FlameEmission;
}

QPointF Fountain::flameUnitSize(const Fixture *fx)
{
    const QLCPhysical phys = physicalOf(fx);
    return QPointF(phys.width() > 0 ? phys.width() / 1000.0 : 0.3,
                   phys.height() > 0 ? phys.height() / 1000.0 : 0.15);
}

float Fountain::flameBottom(const Doc *doc, MonitorProperties *props, quint32 fid)
{
    if (props == nullptr)
        return 0.0f;
    const Parts f = ofFixture(props, fid);
    if (f.isValid())
        return innerTop(f);
    const Fixture *fx = doc != nullptr ? doc->fixture(fid) : nullptr;
    return props->fixtureRigPosition(fid).z() + float(flameUnitSize(fx).y());
}

float Fountain::legacyFlameTop(const Doc *doc, MonitorProperties *props, quint32 fid)
{
    const Fixture *fx = doc != nullptr ? doc->fixture(fid) : nullptr;
    const QPointF sz = flameUnitSize(fx);
    return props->fixtureRigPosition(fid).z() + float(sz.y()) + float(qMax(0.45, 2.0 * sz.x()));
}

float Fountain::flameHeight(const Doc *doc, MonitorProperties *props, quint32 fid)
{
    if (props == nullptr)
        return kDefaultFireHeight;
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    if (rp.flameHeight >= 0.0f)
        return qMax(0.05f, rp.flameHeight);
    return qMax(0.1f, legacyFlameTop(doc, props, fid) - flameBottom(doc, props, fid));
}

bool Fountain::mountIn(const Doc *doc, MonitorProperties *props, quint32 fid, double x, double y)
{
    const Parts f = at(props, x, y);
    if (!f.isValid() || !props->containsFixture(fid))
        return false;
    FixtureRigProps rp = props->fixtureRigProps(fid);
    rp.clearMounts();
    rp.deckPlatformId = f.base->id();
    rp.mountingType = Truss::FloorMounted;
    rp.placement = FixtureRigProps::OnSurface;
    const bool onWall = onInnerWall(f, x, y);
    rp.deckHeightOffset = onWall ? qMax(0.0f, innerTop(f) - baseTop(props, f)) : 0.0f;

    /* A fixed light (a par) in a fountain uplights it: pointed at the
       middle of the flame, straight up when there is none. */
    const Fixture *fx = doc != nullptr ? doc->fixture(fid) : nullptr;
    if (fx != nullptr && !isFlame(fx) && RigHeads::isFixedUnit(fx)
        && rp.pixelPath == 0)
    {
        QVector3D aim(float(x), float(y), innerTop(f) + 1.0f);
        foreach (quint32 other, props->fixtureItemsID())
            if (other != fid && doc != nullptr && props->fixtureGroup(other) == f.groupId
                && isFlame(doc->fixture(other)))
            {
                const QVector3D at = props->fixtureRigPosition(other);
                aim = QVector3D(at.x(), at.y(), flameBottom(doc, props, other)
                                + flameHeight(doc, props, other) / 2.0f);
                break;
            }
        const QVector3D from(float(x), float(y), baseTop(props, f) + rp.deckHeightOffset);
        QVector3D v = aim - from;
        if (v.length() < 1e-3f)
            v = QVector3D(0, 0, 1);
        v.normalize();
        rp.fixedAim = true;
        rp.aimDown = float(qRadiansToDegrees(qAsin(double(-v.z()))));
        rp.panZeroDir = float(qRadiansToDegrees(qAtan2(double(-v.x()), double(v.y()))));
    }
    props->setFixtureRigProps(fid, rp);
    QVector3D pos = props->fixturePosition(fid, 0, 0);
    props->setFixturePosition(fid, 0, 0, QVector3D(float(x * 1000.0), float(y * 1000.0), pos.z()));
    props->setFixtureGroup(fid, f.groupId);
    props->setFixtureLayer(fid, props->group(f.groupId).layerId);
    return true;
}

void Fountain::repair(const Doc *doc, MonitorProperties *props)
{
    if (props == nullptr)
        return;
    QList<Parts> fountains;
    foreach (const MonitorProperties::MonitorGroup &g, props->groups())
    {
        const Parts f = ofGroup(props, g.id);
        if (f.isValid())
            fountains << f;
    }
    const QList<quint32> fids = props->fixtureItemsID();
    foreach (const Parts &f, fountains)
    {
        props->setGroupAssembly(f.groupId, true);
        const quint32 layer = props->group(f.groupId).layerId;
        /* What stands on its base is part of it: a flame built before the
           builder put it in the group or on the fountain's layer was left
           behind by a move, and held by another layer's lock. */
        foreach (quint32 fid, fids)
        {
            const FixtureRigProps rp = props->fixtureRigProps(fid);
            bool belongs = rp.deckPlatformId == f.base->id();
            if (!belongs && doc != nullptr && isFlame(doc->fixture(fid)))
            {
                const QVector3D at = props->fixtureRigPosition(fid);
                belongs = f.base->containsPoint(at.x(), at.y());
            }
            if (!belongs)
                continue;
            if (props->fixtureGroup(fid) != f.groupId)
                props->setFixtureGroup(fid, f.groupId);
            if (props->fixtureLayer(fid) != layer)
                props->setFixtureLayer(fid, layer);
        }
    }
    // Fire heights for the flames that have none: the top stays put.
    if (doc == nullptr)
        return;
    foreach (quint32 fid, fids)
    {
        if (!isFlame(doc->fixture(fid)))
            continue;
        FixtureRigProps rp = props->fixtureRigProps(fid);
        if (rp.flameHeight >= 0.0f)
            continue;
        rp.flameHeight = qMax(0.1f, legacyFlameTop(doc, props, fid) - flameBottom(doc, props, fid));
        props->setFixtureRigProps(fid, rp);
    }
}
