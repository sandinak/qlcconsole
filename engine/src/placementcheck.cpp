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
#include <QCollator>
#include <QHash>
#include <QSet>
#include <algorithm>

#include "placementcheck.h"
#include "monitorproperties.h"
#include "stageplatform.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
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

namespace
{
bool sitOn(Doc *doc, quint32 fid, quint32 platformId)
{
    MonitorProperties *mp = doc->monitorProperties();
    FixtureRigProps rp = mp->fixtureRigProps(fid);
    rp.deckPlatformId = platformId;
    rp.deckHeightOffset = 0.0f;
    rp.mountingType = Truss::FloorMounted;
    mp->setFixtureRigProps(fid, rp);
    doc->setModified();
    return true;
}
}

bool PlacementCheck::fix(Doc *doc, const Buried &b)
{
    MonitorProperties *mp = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (mp == nullptr || !b.canSit || mp->platform(b.sitId) == nullptr || !mp->containsFixture(b.fixtureId))
        return false;
    return sitOn(doc, b.fixtureId, b.sitId);
}


/****************************************************************************
 * FIX-16: on the floor beside the deck its row stands on
 ****************************************************************************/

namespace
{
const float kFloor = 0.05f;     // at or below this a light is "on the floor"

struct Info
{
    QString type;               // fixture type (definition + mode)
    QString stem;               // name without its trailing number; empty when it has none
    quint32 deck = UINT_MAX;    // the deck it stands on
    bool floor = false;         // placed by hand, on the floor, standing
    QVector3D pos;
};

QString typeKey(const Fixture *fx)
{
    const QLCFixtureDef *def = fx->fixtureDef();
    const QLCFixtureMode *mode = fx->fixtureMode();
    if (def == nullptr || mode == nullptr)
        return QStringLiteral("generic:%1").arg(fx->channels());
    return def->manufacturer() + QLatin1Char('|') + def->model() + QLatin1Char('|') + mode->name();
}

QString stemOf(const QString &name)
{
    int n = name.size();
    while (n > 0 && name.at(n - 1).isDigit())
        --n;
    if (n == name.size() || n == 0)
        return QString();
    return name.left(n);
}

/* Distance from a point to a deck's footprint (0 inside), metres. */
float distanceToDeck(const StagePlatform *pl, float x, float y)
{
    if (pl->containsPoint(x, y))
        return 0.0f;
    float best = 1.0e9f;
    for (const StagePlatform::Block &blk : pl->blocks())
    {
        const QList<QPointF> c = pl->corners(blk);
        for (int i = 0; i < c.size(); ++i)
        {
            const QPointF a = c.at(i), b = c.at((i + 1) % c.size());
            const QPointF ab = b - a, ap = QPointF(double(x), double(y)) - a;
            const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
            double t = len2 > 0.0 ? (ap.x() * ab.x() + ap.y() * ab.y()) / len2 : 0.0;
            t = qBound(0.0, t, 1.0);
            const QPointF q = a + ab * t;
            best = qMin(best, float(std::hypot(double(x) - q.x(), double(y) - q.y())));
        }
    }
    return best;
}

QString deckLabel(const QString &name)
{
    return name.startsWith(QLatin1String("Deck"), Qt::CaseInsensitive) ? name : QStringLiteral("Deck ") + name;
}

QString joinNames(Doc *doc, const QList<quint32> &ids)
{
    QStringList names;
    for (quint32 id : ids)
        if (const Fixture *fx = doc->fixture(id))
            names << fx->name();
    if (names.size() > 3)
        return names.at(0) + QStringLiteral(", ") + names.at(1) + QStringLiteral("…") + names.last();
    return names.join(QStringLiteral(", "));
}
}

quint32 PlacementCheck::deckOf(Doc *doc, quint32 fid)
{
    MonitorProperties *mp = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (mp == nullptr || !mp->containsFixture(fid))
        return UINT_MAX;
    const FixtureRigProps rp = mp->fixtureRigProps(fid);
    if (rp.trussId != Truss::invalidId() || rp.onPipe() || rp.onTower() || rp.onHanger()
        || rp.onRiser() || mp->fixtureFrameGroup(fid) != 0)
        return UINT_MAX;
    const QVector3D p = mp->fixtureRigPosition(fid);
    if (rp.onDeck())
    {
        const StagePlatform *pl = mp->platform(rp.deckPlatformId);
        return (pl != nullptr && p.z() > kFloor) ? rp.deckPlatformId : UINT_MAX;
    }
    if (p.z() <= kFloor)
        return UINT_MAX;
    // A free light whose height is a solid top under it.
    for (StagePlatform *pl : mp->platforms())
    {
        if (pl == nullptr || !pl->solid() || pl->topMaterial() != StagePlatform::SolidTop
            || !pl->containsPoint(p.x(), p.y()))
            continue;
        if (qAbs(mp->platformBaseZ(pl->id()) + pl->topAt(p.x(), p.y()) - p.z()) <= tolerance())
            return pl->id();
    }
    return UINT_MAX;
}

namespace
{
struct Rows
{
    Doc *doc;
    MonitorProperties *mp;
    QHash<quint32, Info> info;
    QList<quint32> ids;                         // every fixture on the plot, id order
    QHash<QString, QList<quint32>> byType;
    QHash<QString, QList<quint32>> byStem;      // type + stem
    QHash<quint32, QList<const FixtureGroup *>> groups;
};

void build(Rows &r)
{
    r.ids = r.mp->fixtureItemsID();
    std::sort(r.ids.begin(), r.ids.end());
    for (quint32 fid : r.ids)
    {
        const Fixture *fx = r.doc->fixture(fid);
        if (fx == nullptr)
            continue;
        Info i;
        i.type = typeKey(fx);
        i.stem = stemOf(fx->name());
        i.deck = PlacementCheck::deckOf(r.doc, fid);
        i.pos = r.mp->fixtureRigPosition(fid);
        QVector3D c[8];
        const FixtureRigProps rp = r.mp->fixtureRigProps(fid);
        i.floor = i.deck == UINT_MAX && !rp.onDeck() && rp.mountingType != Truss::TopHung
                  && i.pos.z() <= kFloor && PlacementCheck::body(r.doc, fid, c);
        r.info.insert(fid, i);
        r.byType[i.type] << fid;
        if (!i.stem.isEmpty())
            r.byStem[i.type + QLatin1Char('\n') + i.stem] << fid;
    }
    for (const FixtureGroup *g : r.doc->fixtureGroups())
        for (quint32 fid : g->fixtureList())
            if (r.info.contains(fid))
                r.groups[fid] << g;
}

/* The row it is judged against: the first of the candidate sets in which more
   than half the mates (two at least) stand on decks and one of those decks is
   close. */
PlacementCheck::OffRow judge(Rows &r, quint32 fid)
{
    PlacementCheck::OffRow out;
    const Info &me = r.info.value(fid);
    if (!me.floor)
        return out;
    QVector3D body[8];
    PlacementCheck::body(r.doc, fid, body);
    // "A body length": the longest edge of its box, 0.5 m at least.
    float edge = 0.0f;
    edge = qMax(edge, (body[1] - body[0]).length());
    edge = qMax(edge, (body[3] - body[0]).length());
    edge = qMax(edge, (body[4] - body[0]).length());
    const float reach = qMax(0.5f, edge);

    struct Cand { QString how; QList<quint32> mates; };
    QList<Cand> cands;
    QList<const FixtureGroup *> gs = r.groups.value(fid);
    for (const FixtureGroup *g : gs)
    {
        Cand c;
        c.how = QStringLiteral("group ") + g->name();
        for (quint32 m : g->fixtureList())
            if (m != fid && r.info.contains(m) && r.info.value(m).type == me.type && !c.mates.contains(m))
                c.mates << m;
        if (c.mates.size() >= 2)
            cands << c;
    }
    std::stable_sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) {
        return a.mates.size() < b.mates.size();
    });
    if (!me.stem.isEmpty())
    {
        Cand c;
        c.how = QStringLiteral("name ") + me.stem;
        for (quint32 m : r.byStem.value(me.type + QLatin1Char('\n') + me.stem))
            if (m != fid)
                c.mates << m;
        if (c.mates.size() >= 2)
            cands << c;
    }
    {
        Cand c;
        c.how = QStringLiteral("line");
        for (quint32 m : r.byType.value(me.type))
            if (m != fid && qAbs(r.info.value(m).pos.y() - me.pos.y()) <= 0.3f)
                c.mates << m;
        if (c.mates.size() >= 2)
            cands << c;
    }

    for (const Cand &c : cands)
    {
        QList<quint32> on;
        QSet<quint32> decks;
        for (quint32 m : c.mates)
        {
            const quint32 d = r.info.value(m).deck;
            if (d == UINT_MAX)
                continue;
            on << m;
            decks << d;
        }
        if (on.size() < 2 || on.size() * 2 <= c.mates.size())
            continue;
        // The nearest of those decks, if it is close enough to be "the same run".
        quint32 best = UINT_MAX;
        float bestD = 1.0e9f;
        QList<quint32> sorted = decks.values();
        std::sort(sorted.begin(), sorted.end());
        for (quint32 d : sorted)
        {
            const StagePlatform *pl = r.mp->platform(d);
            if (pl == nullptr)
                continue;
            const float dist = distanceToDeck(pl, me.pos.x(), me.pos.y());
            if (dist < bestD)
            {
                bestD = dist;
                best = d;
            }
        }
        if (best == UINT_MAX || bestD > reach)
            continue;
        const StagePlatform *pl = r.mp->platform(best);
        QCollator collator;
        collator.setNumericMode(true);
        std::sort(on.begin(), on.end(), [&](quint32 a, quint32 b) {
            return collator.compare(r.doc->fixture(a)->name(), r.doc->fixture(b)->name()) < 0;
        });
        out.off = true;
        out.fixtureId = fid;
        out.sitId = best;
        out.topZ = r.mp->platformBaseZ(best) + pl->topAt(me.pos.x(), me.pos.y());
        out.distance = bestD;
        out.mates = on;
        out.rowSize = c.mates.size();
        out.decks = sorted;
        out.row = c.how;
        return out;
    }
    return out;
}
}

QList<PlacementCheck::OffRow> PlacementCheck::offRow(Doc *doc)
{
    QList<OffRow> out;
    MonitorProperties *mp = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (mp == nullptr || mp->platforms().isEmpty())
        return out;
    Rows r{doc, mp, {}, {}, {}, {}, {}};
    build(r);
    for (quint32 fid : r.ids)
    {
        if (!r.info.contains(fid) || !r.info.value(fid).floor)
            continue;
        // A light inside the deck is FIX-15's, and sitting it on top fixes both.
        if (check(doc, fid).buried)
            continue;
        const OffRow o = judge(r, fid);
        if (o.off)
            out << o;
    }
    return out;
}

PlacementCheck::OffRow PlacementCheck::checkRow(Doc *doc, quint32 fid)
{
    MonitorProperties *mp = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (mp == nullptr || mp->platforms().isEmpty() || check(doc, fid).buried)
        return OffRow();
    Rows r{doc, mp, {}, {}, {}, {}, {}};
    build(r);
    return judge(r, fid);
}

QString PlacementCheck::problemText(Doc *doc, const OffRow &o)
{
    const Fixture *fx = doc->fixture(o.fixtureId);
    const QString name = fx != nullptr ? fx->name() : QString::number(o.fixtureId);
    const StagePlatform *pl = doc->monitorProperties()->platform(o.sitId);
    QString deck = deckLabel(pl != nullptr ? pl->name() : QString());
    deck += QStringLiteral(" (%1 m)").arg(double(o.topZ), 0, 'f', 2);
    if (o.decks.size() > 1)
        deck += o.decks.size() == 2 ? tr(" and 1 more deck")
                                    : tr(" and %1 more decks").arg(o.decks.size() - 1);
    return tr("%1 is on the floor; %2 are on %3").arg(name, joinNames(doc, o.mates), deck);
}

QString PlacementCheck::fixText(Doc *doc, const OffRow &o)
{
    const StagePlatform *pl = doc->monitorProperties()->platform(o.sitId);
    return tr("Sit it on %1 (%2 m)").arg(deckLabel(pl != nullptr ? pl->name() : QString()))
                                    .arg(double(o.topZ), 0, 'f', 2);
}

bool PlacementCheck::fix(Doc *doc, const OffRow &o)
{
    MonitorProperties *mp = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (mp == nullptr || !o.off || mp->platform(o.sitId) == nullptr || !mp->containsFixture(o.fixtureId))
        return false;
    return sitOn(doc, o.fixtureId, o.sitId);
}
