/*
  Q Light Controller Plus
  beamocclusion.cpp

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

#include <QCoreApplication>
#include <QtMath>
#include <cstring>

#include "beamocclusion.h"
#include "monitorproperties.h"
#include "stageplatform.h"
#include "scenery.h"
#include "hanger.h"
#include "tower.h"
#include "truss.h"
#include "stand.h"
#include "pipe.h"

namespace
{
    /** The head's own body and clamp: a beam is not tested this close to
     *  where it leaves the lens (metres). */
    const float START_SKIP = 0.25f;
    /** The end: the deck a target stands on, the floor under a mark. */
    const float END_SKIP = 0.05f;
    const float CLEARANCE_REACH = 2.0f;
    /** A stand's pole, which the model does not otherwise size. */
    const float STAND_POLE = 0.05f;

    /** FNV-1a over the numbers that make up the stage, so a change anywhere
     *  -- a drape opened, a platform moved -- is a different signature. */
    struct Signature
    {
        quint64 h = 1469598103934665603ULL;
        void bytes(const void *p, size_t n)
        {
            const uchar *b = static_cast<const uchar *>(p);
            for (size_t i = 0; i < n; ++i)
            {
                h ^= b[i];
                h *= 1099511628211ULL;
            }
        }
        void add(float f) { bytes(&f, sizeof(f)); }
        void add(double d) { add(float(d)); }
        void add(int i) { bytes(&i, sizeof(i)); }
        void add(quint32 u) { bytes(&u, sizeof(u)); }
        void add(const QVector3D &v) { add(v.x()); add(v.y()); add(v.z()); }
        void add(const QPointF &p) { add(p.x()); add(p.y()); }
        void add(const QString &s) { bytes(s.constData(), size_t(s.size()) * sizeof(QChar)); }
    };

    QString tr(const char *s)
    {
        return QCoreApplication::translate("BeamOcclusion", s);
    }

    QString nameOr(const QString &name, const QString &fallback, quint32 id)
    {
        if (!name.trimmed().isEmpty())
            return name;
        return QStringLiteral("%1 %2").arg(fallback).arg(id + 1);
    }

    int sceneryKind(const Scenery *sc)
    {
        if (sc->isSeeThrough())
            return 0;
        return sc->isBuilt() ? BeamOcclusion::Walls : BeamOcclusion::Drapes;
    }
}

QList<int> BeamOcclusion::kinds()
{
    return { Drapes, Walls, Platforms, Towers, Trusses, Pipes, Stands };
}

int BeamOcclusion::defaultKinds()
{
    return Drapes | Walls | Platforms | Towers | Trusses | Pipes | Stands;
}

float BeamOcclusion::defaultMinSize()
{
    return 0.2f;
}

float BeamOcclusion::clearanceReach()
{
    return CLEARANCE_REACH;
}

QString BeamOcclusion::kindName(int kind)
{
    switch (kind)
    {
        case Drapes:    return tr("Drapes and cyc");
        case Walls:     return tr("Walls and proscenium");
        case Platforms: return tr("Platforms and risers");
        case Towers:    return tr("Towers");
        case Trusses:   return tr("Trusses");
        case Pipes:     return tr("Pipes and booms");
        case Stands:    return tr("Stands");
        default:        return QString();
    }
}

/*****************************************************************************
 * Model: building
 *****************************************************************************/

void BeamOcclusion::Model::beginBlocker(int kind, quint32 id, const QString &name)
{
    Blocker b;
    b.kind = kind;
    b.id = id;
    b.name = name;
    b.lo = QVector3D(1e9f, 1e9f, 1e9f);
    b.hi = QVector3D(-1e9f, -1e9f, -1e9f);
    b.firstPanel = m_panels.size();
    b.panels = 0;
    b.firstRod = m_rods.size();
    b.rods = 0;
    m_blockers << b;
}

static void grow(BeamOcclusion::Model::Blocker &b, const QVector3D &p, float pad = 0.0f)
{
    b.lo = QVector3D(qMin(b.lo.x(), p.x() - pad), qMin(b.lo.y(), p.y() - pad), qMin(b.lo.z(), p.z() - pad));
    b.hi = QVector3D(qMax(b.hi.x(), p.x() + pad), qMax(b.hi.y(), p.y() + pad), qMax(b.hi.z(), p.z() + pad));
}

void BeamOcclusion::Model::addPanel(const QVector3D &c, const QVector3D &e1, const QVector3D &e2)
{
    if (m_blockers.isEmpty())
        return;
    const QVector3D n = QVector3D::crossProduct(e1, e2);
    const float len = n.length();
    if (len < 1e-6f)
        return;                         // a sliver: nothing to hit
    Panel p;
    p.c = c;
    p.e1 = e1;
    p.e2 = e2;
    p.n = n / len;
    p.l1sq = e1.lengthSquared();
    p.l2sq = e2.lengthSquared();
    p.blocker = m_blockers.size() - 1;
    m_panels << p;
    Blocker &b = m_blockers.last();
    b.panels++;
    grow(b, c);
    grow(b, c + e1);
    grow(b, c + e2);
    grow(b, c + e1 + e2);

    Signature s;
    s.h = m_signature;
    s.add(c); s.add(e1); s.add(e2);
    m_signature = s.h;
}

void BeamOcclusion::Model::addRod(const QVector3D &p0, const QVector3D &p1, float r)
{
    if (m_blockers.isEmpty())
        return;
    Rod rod;
    rod.p0 = p0;
    rod.p1 = p1;
    rod.r = r;
    rod.blocker = m_blockers.size() - 1;
    m_rods << rod;
    Blocker &b = m_blockers.last();
    b.rods++;
    grow(b, p0, r);
    grow(b, p1, r);

    Signature s;
    s.h = m_signature;
    s.add(p0); s.add(p1); s.add(r);
    m_signature = s.h;
}

void BeamOcclusion::Model::endBlocker()
{
    if (m_blockers.isEmpty())
        return;
    const Blocker &b = m_blockers.last();
    if (b.panels == 0 && b.rods == 0)
    {
        m_blockers.removeLast();
        return;
    }
    Signature s;
    s.h = m_signature;
    s.add(b.kind); s.add(b.id); s.add(b.name);
    m_signature = s.h;
}

void BeamOcclusion::Model::finish()
{
    m_blockers.squeeze();
    m_panels.squeeze();
    m_rods.squeeze();
}

/*****************************************************************************
 * Model: queries
 *****************************************************************************/

/* Does the segment a + t (b - a), t in t0..t1, come within @p pad of the box? */
static bool segmentNearBox(const QVector3D &a, const QVector3D &d, float t0, float t1,
                           const QVector3D &lo, const QVector3D &hi, float pad)
{
    for (int i = 0; i < 3; ++i)
    {
        const float o = a[i], v = d[i];
        const float mn = lo[i] - pad, mx = hi[i] + pad;
        if (qAbs(v) < 1e-9f)
        {
            if (o < mn || o > mx)
                return false;
            continue;
        }
        float ta = (mn - o) / v, tb = (mx - o) / v;
        if (ta > tb)
            qSwap(ta, tb);
        t0 = qMax(t0, ta);
        t1 = qMin(t1, tb);
        if (t0 > t1)
            return false;
    }
    return true;
}

/* Closest approach of segment a + s*d (s in s0..s1) to segment p + u*e (u in
   0..1): returns the distance and the s it is at. */
static float segmentSegment(const QVector3D &a, const QVector3D &d, float s0, float s1,
                            const QVector3D &p, const QVector3D &e, float &sAt)
{
    const QVector3D r = a - p;
    const float A = QVector3D::dotProduct(d, d);
    const float E = QVector3D::dotProduct(e, e);
    const float B = QVector3D::dotProduct(d, e);
    const float C = QVector3D::dotProduct(d, r);
    const float F = QVector3D::dotProduct(e, r);
    const float den = A * E - B * B;
    float s = (den > 1e-9f) ? (B * F - C * E) / den : s0;
    s = qBound(s0, s, s1);
    float u = (E > 1e-9f) ? (B * s + F) / E : 0.0f;
    if (u < 0.0f || u > 1.0f)
    {
        u = qBound(0.0f, u, 1.0f);
        s = (A > 1e-9f) ? qBound(s0, (B * u - C) / A, s1) : s0;
    }
    sAt = s;
    return ((a + d * s) - (p + e * u)).length();
}

BeamOcclusion::Hit BeamOcclusion::Model::test(const QVector3D &a, const QVector3D &b,
                                              float t0, float t1,
                                              const QVector<Exclude> &exclude) const
{
    Hit hit;
    hit.clearance = CLEARANCE_REACH;
    hit.point = b;
    const QVector3D d = b - a;
    const float len = d.length();
    hit.distance = len;
    if (len < 1e-6f || t0 >= t1)
        return hit;

    float bestT = 2.0f;
    int bestBlocker = -1;

    for (int bi = 0; bi < m_blockers.size(); ++bi)
    {
        const Blocker &bl = m_blockers.at(bi);
        bool skip = false;
        for (const Exclude &ex : exclude)
            if (ex.kind == bl.kind && ex.id == bl.id) { skip = true; break; }
        if (skip)
            continue;
        if (!segmentNearBox(a, d, t0, t1, bl.lo, bl.hi, CLEARANCE_REACH))
            continue;

        for (int pi = bl.firstPanel; pi < bl.firstPanel + bl.panels; ++pi)
        {
            const Panel &p = m_panels.at(pi);
            const float den = QVector3D::dotProduct(p.n, d);
            if (qAbs(den) < 1e-6f * len)
                continue;                       // running along it: grazing, not stopped
            const float t = QVector3D::dotProduct(p.n, p.c - a) / den;
            if (t < t0 || t > t1)
                continue;
            const QVector3D q = (a + d * t) - p.c;
            const float u = QVector3D::dotProduct(q, p.e1) / p.l1sq;
            const float v = QVector3D::dotProduct(q, p.e2) / p.l2sq;
            const float l1 = qSqrt(p.l1sq), l2 = qSqrt(p.l2sq);
            const float du = (u < 0.0f ? -u : (u > 1.0f ? u - 1.0f : 0.0f)) * l1;
            const float dv = (v < 0.0f ? -v : (v > 1.0f ? v - 1.0f : 0.0f)) * l2;
            float c;
            if (du > 0.0f || dv > 0.0f)
                c = qSqrt(du * du + dv * dv);
            else
                c = -qMin(qMin(u, 1.0f - u) * l1, qMin(v, 1.0f - v) * l2);
            hit.clearance = qMin(hit.clearance, c);
            if (c <= 0.0f && t < bestT)
            {
                bestT = t;
                bestBlocker = bi;
            }
        }
        for (int ri = bl.firstRod; ri < bl.firstRod + bl.rods; ++ri)
        {
            const Rod &rod = m_rods.at(ri);
            float s = 0.0f;
            const float dist = segmentSegment(a, d, t0, t1, rod.p0, rod.p1 - rod.p0, s);
            const float c = dist - rod.r;
            hit.clearance = qMin(hit.clearance, c);
            if (c <= 0.0f)
            {
                // Back from the closest approach to where it goes in.
                const float in = qSqrt(qMax(0.0f, rod.r * rod.r - dist * dist)) / len;
                const float t = qMax(t0, s - in);
                if (t < bestT)
                {
                    bestT = t;
                    bestBlocker = bi;
                }
            }
        }
    }

    if (bestBlocker >= 0)
    {
        const Blocker &bl = m_blockers.at(bestBlocker);
        hit.blocked = true;
        hit.kind = bl.kind;
        hit.id = bl.id;
        hit.name = bl.name;
        hit.fraction = bestT;
        hit.distance = bestT * len;
        hit.point = a + d * bestT;
        if (hit.clearance > 0.0f)
            hit.clearance = 0.0f;
    }
    return hit;
}

BeamOcclusion::Hit BeamOcclusion::Model::segment(const QVector3D &from, const QVector3D &to,
                                                 const QVector<Exclude> &exclude) const
{
    const float len = (to - from).length();
    if (len < 1e-6f)
    {
        Hit h;
        h.clearance = CLEARANCE_REACH;
        h.point = to;
        return h;
    }
    const float t0 = qMin(1.0f, START_SKIP / len);
    const float t1 = qMax(0.0f, 1.0f - END_SKIP / len);
    return test(from, to, t0, t1, exclude);
}

BeamOcclusion::Hit BeamOcclusion::Model::ray(const QVector3D &origin, const QVector3D &dir, float maxT,
                                             const QVector<Exclude> &exclude) const
{
    if (maxT <= 0.0f)
        return Hit();
    const QVector3D to = origin + dir.normalized() * maxT;
    return test(origin, to, qMin(1.0f, START_SKIP / maxT), 1.0f, exclude);
}

/*****************************************************************************
 * Building from a stage
 *****************************************************************************/

/* A box standing on @p z0 up to @p z1, its floor corners @p cs (in order round
   it): its four sides, and its top when @p top. */
static void addBox(BeamOcclusion::Model &m, const QList<QPointF> &cs, float z0, float z1, bool top)
{
    if (cs.size() < 4 || z1 <= z0)
        return;
    const QVector3D up(0.0f, 0.0f, z1 - z0);
    for (int i = 0; i < 4; ++i)
    {
        const QPointF p = cs.at(i), q = cs.at((i + 1) % 4);
        const QVector3D c(float(p.x()), float(p.y()), z0);
        m.addPanel(c, QVector3D(float(q.x() - p.x()), float(q.y() - p.y()), 0.0f), up);
    }
    if (top)
    {
        const QVector3D c(float(cs.at(0).x()), float(cs.at(0).y()), z1);
        m.addPanel(c, QVector3D(float(cs.at(1).x() - cs.at(0).x()), float(cs.at(1).y() - cs.at(0).y()), 0.0f),
                      QVector3D(float(cs.at(3).x() - cs.at(0).x()), float(cs.at(3).y() - cs.at(0).y()), 0.0f));
    }
}

BeamOcclusion::ModelPtr BeamOcclusion::build(const MonitorProperties *mp, int kinds, float minSize)
{
    QSharedPointer<Model> m(new Model);
    if (mp == nullptr)
        return m;

    if (kinds & (Drapes | Walls))
    {
        foreach (Scenery *sc, mp->sceneries())
        {
            if (sc == nullptr)
                continue;
            const int kind = sceneryKind(sc);
            if (kind == 0 || (kinds & kind) == 0)
                continue;
            m->beginBlocker(kind, sc->id(), nameOr(sc->name(), Scenery::kindName(sc->kind()), sc->id()));
            const int whole = qMax(1, sc->segments());
            // The middle of a built piece: anything going through it crosses it.
            foreach (const Scenery::Piece &pc, sc->pieces())
            {
                if (pc.z1 <= pc.z0 || pc.u1 <= pc.u0)
                    continue;
                const int n = qMax(1, int(qCeil(whole * (pc.u1 - pc.u0) / 2.0)));
                const QVector3D up(0.0f, 0.0f, pc.z1 - pc.z0);
                for (int i = 0; i < n; ++i)
                {
                    const double ua = pc.u0 + (pc.u1 - pc.u0) * i / n;
                    const double ub = pc.u0 + (pc.u1 - pc.u0) * (i + 1) / n;
                    const QVector3D p0 = sc->pointAt(ua, pc.z0);
                    const QVector3D p1 = sc->pointAt(ub, pc.z0);
                    QVector3D along = p1 - p0;
                    along.setZ(0.0f);
                    m->addPanel(QVector3D(p0.x(), p0.y(), pc.z0), along, up);
                }
            }
            m->endBlocker();
        }
    }

    if (kinds & Platforms)
    {
        foreach (StagePlatform *pl, mp->platforms())
        {
            if (pl == nullptr || pl->topMaterial() == StagePlatform::OpenTop)
                continue;
            m->beginBlocker(Platforms, pl->id(), nameOr(pl->name(), tr("Platform"), pl->id()));
            const float base = mp->platformBaseZ(pl->id());
            const bool solidTop = pl->topMaterial() == StagePlatform::SolidTop;
            foreach (const StagePlatform::Block &bk, pl->blocks())
                addBox(*m, pl->corners(bk), base, base + bk.height, solidTop);
            m->endBlocker();
        }
    }

    if (kinds & Towers)
    {
        foreach (Tower *tw, mp->towers())
        {
            if (tw == nullptr)
                continue;
            m->beginBlocker(Towers, tw->id(), nameOr(tw->name(), tr("Tower"), tw->id()));
            addBox(*m, tw->corners(), 0.0f, tw->height(), true);
            m->endBlocker();
        }
    }

    if (kinds & Trusses)
    {
        foreach (Truss *t, mp->trusses())
        {
            if (t == nullptr || t->width() < minSize || t->length() <= 0.0f)
                continue;
            m->beginBlocker(Trusses, t->id(), nameOr(t->name(), tr("Truss"), t->id()));
            m->addRod(t->positionAt(0.0f), t->positionAt(t->length()), t->width() * 0.5f);
            m->endBlocker();
        }
    }

    if (kinds & Pipes)
    {
        foreach (Pipe *p, mp->pipes())
        {
            if (p == nullptr || p->diameter() < minSize)
                continue;
            m->beginBlocker(Pipes, p->id(), nameOr(p->name(), tr("Pipe"), p->id()));
            m->addRod(p->positionAt(0.0f), p->positionAt(p->length()), p->diameter() * 0.5f);
            m->endBlocker();
        }
    }

    if (kinds & Stands)
    {
        foreach (Stand *s, mp->stands())
        {
            if (s == nullptr || STAND_POLE < minSize)
                continue;
            m->beginBlocker(Stands, s->id(), nameOr(s->name(), tr("Stand"), s->id()));
            const QVector3D top = s->topPos();
            m->addRod(QVector3D(top.x(), top.y(), 0.0f), top, STAND_POLE * 0.5f);
            m->endBlocker();
        }
    }

    m->finish();
    return m;
}

/*****************************************************************************
 * A head's own mount
 *****************************************************************************/

static void addTrussChain(const MonitorProperties *mp, quint32 trussId,
                          QVector<BeamOcclusion::Exclude> &out)
{
    // A bar and the truss it hangs on, up the chain; the stands it rests on.
    for (int guard = 0; guard < 16 && trussId != Truss::invalidId(); ++guard)
    {
        Truss *t = mp->truss(trussId);
        if (t == nullptr)
            break;
        out << BeamOcclusion::Exclude{ BeamOcclusion::Trusses, t->id() };
        if (t->isStandMounted())
            out << BeamOcclusion::Exclude{ BeamOcclusion::Stands, t->standId() };
        if (t->endStandId() != Truss::invalidId())
            out << BeamOcclusion::Exclude{ BeamOcclusion::Stands, t->endStandId() };
        trussId = t->parentTrussId();
    }
}

QVector<BeamOcclusion::Exclude> BeamOcclusion::ownMounts(const MonitorProperties *mp, quint32 fixtureId)
{
    QVector<Exclude> out;
    if (mp == nullptr)
        return out;
    const FixtureRigProps rp = mp->fixtureRigProps(fixtureId);
    switch (rp.primaryMount())
    {
        case FixtureRigProps::HangerMount:
            if (Hanger *h = mp->hanger(rp.hangerId))
            {
                if (h->parentKind() == Hanger::OnTower)
                    out << Exclude{ Towers, h->parentId() };
                else
                    addTrussChain(mp, h->parentId(), out);
            }
        break;
        case FixtureRigProps::TrussMount:
            addTrussChain(mp, rp.trussId, out);
        break;
        case FixtureRigProps::PipeMount:
        {
            quint32 pid = rp.pipeId;
            for (int guard = 0; guard < 16 && pid != Pipe::invalidId(); ++guard)
            {
                Pipe *p = mp->pipe(pid);
                if (p == nullptr)
                    break;
                out << Exclude{ Pipes, p->id() };
                if (p->isStandMounted())
                    out << Exclude{ Stands, p->standId() };
                if (p->isTrussHung())
                    addTrussChain(mp, p->parentTrussId(), out);
                pid = p->parentPipeId();
            }
        }
        break;
        case FixtureRigProps::TowerMount:
            out << Exclude{ Towers, rp.towerId };
        break;
        case FixtureRigProps::DeckMount:
            out << Exclude{ Platforms, rp.deckPlatformId };
        break;
        case FixtureRigProps::RiserMount:
            out << Exclude{ Platforms, rp.riserPlatformId };
        break;
        case FixtureRigProps::NoMount:
        default:
        {
            // A free head standing on a platform stands on its deck.
            const QVector3D at = mp->fixtureRigPosition(fixtureId);
            foreach (StagePlatform *pl, mp->platforms())
                if (pl != nullptr && pl->containsPoint(at.x(), at.y()))
                    out << Exclude{ Platforms, pl->id() };
        }
        break;
    }
    return out;
}

/*****************************************************************************
 * Cache
 *****************************************************************************/

static quint64 stageSignature(const MonitorProperties *mp, int kinds, float minSize)
{
    using namespace BeamOcclusion;
    Signature s;
    s.add(kinds);
    s.add(minSize);
    if (mp == nullptr)
        return s.h;
    if (kinds & (Drapes | Walls))
        for (Scenery *sc : mp->sceneries())
        {
            if (sc == nullptr) continue;
            s.add(sc->id()); s.add(sc->kind()); s.add(sc->name());
            s.add(sc->originX()); s.add(sc->originY()); s.add(sc->width()); s.add(sc->height());
            s.add(sc->bottom()); s.add(sc->rotation()); s.add(sc->bow()); s.add(sc->outline());
            s.add(sc->depth()); s.add(sc->state());
            s.add(sc->openingWidth()); s.add(sc->openingHeight());
            /* What actually hangs: whatever opens or flies the curtain --
               its state now, a live pose later -- shows here. */
            for (const Scenery::Piece &pc : sc->pieces())
            {
                s.add(pc.u0); s.add(pc.u1); s.add(pc.z0); s.add(pc.z1);
            }
        }
    if (kinds & Platforms)
        for (StagePlatform *pl : mp->platforms())
        {
            if (pl == nullptr) continue;
            s.add(pl->id()); s.add(pl->name());
            s.add(pl->originX()); s.add(pl->originY()); s.add(pl->width()); s.add(pl->depth());
            s.add(pl->height()); s.add(pl->rotation()); s.add(int(pl->topMaterial()));
            s.add(int(pl->shape())); s.add(int(pl->stepsClimb())); s.add(pl->stepCount());
            s.add(pl->stepRise()); s.add(pl->stepRun());
            s.add(mp->platformBaseZ(pl->id()));
        }
    if (kinds & Towers)
        for (Tower *tw : mp->towers())
        {
            if (tw == nullptr) continue;
            s.add(tw->id()); s.add(tw->name());
            s.add(tw->originX()); s.add(tw->originY()); s.add(tw->width()); s.add(tw->depth());
            s.add(tw->height()); s.add(tw->rotation());
        }
    if (kinds & Trusses)
        for (Truss *t : mp->trusses())
        {
            if (t == nullptr) continue;
            s.add(t->id()); s.add(t->name()); s.add(int(t->type())); s.add(t->width());
            s.add(t->length()); s.add(t->origin()); s.add(t->direction());
            s.add(t->parentTrussId());
        }
    if (kinds & Pipes)
        for (Pipe *p : mp->pipes())
        {
            if (p == nullptr) continue;
            s.add(p->id()); s.add(p->name()); s.add(p->diameter());
            s.add(p->positionAt(0.0f)); s.add(p->positionAt(p->length()));
        }
    if (kinds & Stands)
        for (Stand *st : mp->stands())
        {
            if (st == nullptr) continue;
            s.add(st->id()); s.add(st->name()); s.add(st->topPos());
        }
    return s.h;
}

BeamOcclusion::ModelPtr BeamOcclusion::Cache::get(const MonitorProperties *mp, int kinds, float minSize)
{
    const quint64 sig = stageSignature(mp, kinds, minSize);
    QMutexLocker locker(&m_mutex);
    if (m_model.isNull() || m_signature != sig)
    {
        m_model = build(mp, kinds, minSize);
        m_signature = sig;
    }
    return m_model;
}

void BeamOcclusion::Cache::clear()
{
    QMutexLocker locker(&m_mutex);
    m_model.clear();
}
