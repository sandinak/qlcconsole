/*
  Q Light Controller Plus - qlcconsole
  showreviewzonechecks.cpp

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

/* Show Review's no-light zone check (PWR-11): the plot's zones that must
   stay dark (MonitorProperties::noLightZones()) against each cue's lit
   beams. Each lit light's cone is followed from its place in the rig the
   way PWR-09 follows a laser: a mover from its resolved Position, anything
   fixed from where it throws. */

#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QtMath>
#include <algorithm>

#include "showreviewcontext.h"
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "programmercontroller.h"
#include "nolightzone.h"
#include "beamocclusion.h"
#include "stagetarget.h"
#include "scenery.h"
#include "qlcphysical.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "qlcfixturedef.h"
#include "qlcpalette.h"
#include "aimsolver.h"
#include "righeads.h"
#include "movespeed.h"
#include "function.h"
#include "collection.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

static QString tr(const char *text)
{
    return QCoreApplication::translate("ShowReview", text);
}

namespace
{
const double kRange = 30.0;

/** One light in one cue. */
struct Beam
{
    bool lit = false;
    bool hasPos = false;
    double pan = 0.5, tilt = 0.5;   //!< DMX fractions (value / 255)
    /** PWR-12: the layer whose look lit it (its cue, its id) ... */
    QPair<quint32, quint32> litBy { invalidId(), invalidId() };
    /** ... every layer of the cue on it ... */
    QVector<QPair<quint32, quint32>> layers;
    /** ... and the stage targets its position looks aim at. */
    QVector<quint32> aimTargets;
};

bool isMover(const Fixture *fx)
{
    return fx != nullptr
           && (fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB) != QLCChannel::invalid()
               || fx->channelNumber(QLCChannel::Tilt, QLCChannel::MSB) != QLCChannel::invalid());
}

bool isLaser(const Fixture *fx)
{
    const QLCPhysical &phy = fx->fixtureMode()->physical();
    return phy.emission() == QLCPhysical::LaserEmission
           || (fx->fixtureDef() != nullptr && fx->fixtureDef()->type() == QLCFixtureDef::Laser);
}

/** The lights of one cue that give a beam: lit by a dimmer above 0 (or, with
 *  no dimmer channel, any colour set), and where they are aimed. */
QHash<quint32, Beam> beamsOf(ReviewContext &ctx, const ResolvedCue &cue,
                             QHash<QPair<quint32, quint32>, QList<SceneValue>> &cache)
{
    Doc *doc = ctx.doc();
    MonitorProperties *mp = doc->monitorProperties();
    QHash<quint32, Beam> beams;
    for (const ResolvedLayer &layer : cue.layers)
    {
        if (layer.target.isOpaque() || layer.release)
            continue;
        const auto dimIt = layer.values.constFind(KeyDimmer);
        const auto colIt = layer.values.constFind(KeyColour);
        const auto posIt = layer.values.constFind(KeyPosition);
        for (quint32 fid : ctx.targetFixtures(layer.target))
        {
            Fixture *fx = doc->fixture(fid);
            if (fx == nullptr || !fx->isPatched() || fx->fixtureMode() == nullptr
                || !fx->fixtureMode()->physical().emitsLight() || !mp->containsFixture(fid))
                continue;
            Beam &b = beams[fid];
            const QPair<quint32, quint32> from(layer.fromCue, layer.layerId);
            if (!b.layers.contains(from))
                b.layers << from;
            const bool wasLit = b.lit;
            if (QLCPalette::fixtureHasDimmer(fx))
            {
                if (dimIt != layer.values.constEnd())
                {
                    const Look &look = dimIt.value().look;
                    const int v = look.none ? 0 : QLCPalette::lookDimmerValue(doc, look.palettes);
                    if (v > 0)
                        b.lit = true;
                }
            }
            else if (colIt != layer.values.constEnd() && !colIt.value().look.none
                     && !colIt.value().look.palettes.isEmpty())
                b.lit = true;
            if (b.lit && !wasLit)
                b.litBy = from;
            if (posIt != layer.values.constEnd() && !posIt.value().look.none)
            {
                for (quint32 pid : posIt.value().look.palettes)
                {
                    QLCPalette *pal = doc->palette(pid);
                    if (pal == nullptr)
                        continue;
                    if (pal->stageTargetId() != StageTarget::invalidId()
                        && !b.aimTargets.contains(pal->stageTargetId()))
                        b.aimTargets << pal->stageTargetId();
                    const QPair<quint32, quint32> key(pid, fid);
                    auto c = cache.find(key);
                    if (c == cache.end())
                        c = cache.insert(key, pal->valuesFromFixtures(doc, { fid }));
                    double pan, tilt;
                    bool hasPan, hasTilt;
                    MoveSpeed::positionOf(fx, c.value(), pan, tilt, hasPan, hasTilt);
                    if (hasPan) { b.pan = pan; b.hasPos = true; }
                    if (hasTilt) { b.tilt = tilt; b.hasPos = true; }
                }
            }
        }
    }
    return beams;
}

/** Where a DMX fraction (value / 255) on @a fx's pan or tilt lies in the head's
 *  range: across the pointing band (Fixture::positionBand()) when the channel
 *  declares one -- a UST points with 0-127 and spins above -- else unchanged. */
double headFraction(const Fixture *fx, QLCChannel::Group group, double dmxFraction)
{
    const QLCFixtureMode *mode = fx->fixtureMode();
    const quint32 n = fx->channelNumber(group, QLCChannel::MSB);
    const QLCChannel *ch = (mode != nullptr && n != QLCChannel::invalid()) ? mode->channel(n) : nullptr;
    uchar lo = 0, hi = 255;
    if (ch == nullptr || !Fixture::positionBand(ch, lo, hi))
        return dmxFraction;
    return Fixture::positionFraction(ch, dmxFraction * 255.0);
}

/** What raysOf() read, for a check that wants to suggest a cut of tape. */
struct Throw
{
    QVector3D dir;
    double h = 0.0, v = 0.0;        //!< a laser's window, full angles
    double halfRad = 0.0;           //!< any other light's cone
    bool laser = false;
};

/** The rays a lit light throws, from @a apex. False when it has no throw. */
bool raysOf(Doc *doc, Fixture *fx, const Beam &b, QVector3D &apex, QVector<QVector3D> &rays,
            bool &laser, double &coneDeg, Throw *info = nullptr)
{
    MonitorProperties *mp = doc->monitorProperties();
    const QLCPhysical &phy = fx->fixtureMode()->physical();
    laser = isLaser(fx);
    const bool stars = phy.emission() == QLCPhysical::StarEmission;
    QVector3D dir;
    if (isMover(fx))
    {
        const double panSpan = MoveSpeed::panSpanDeg(fx), tiltSpan = MoveSpeed::tiltSpanDeg(fx);
        if (!AimSolver::beamDirection(mp->fixtureRigProps(fx->id()),
                                      (headFraction(fx, QLCChannel::Pan, b.pan) - 0.5) * panSpan,
                                      (headFraction(fx, QLCChannel::Tilt, b.tilt) - 0.5) * tiltSpan,
                                      dir))
            return false;
    }
    else if (!RigHeads::fixedThrow(doc, fx->id(), dir))
        return false;
    if (dir.isNull())
        return false;
    dir.normalize();
    apex = RigHeads::fixtureCentre(doc, fx->id());
    const double declared = qMax(phy.lensDegreesMin(), phy.lensDegreesMax());
    if (info != nullptr)
    {
        info->dir = dir;
        info->laser = laser;
    }
    if (laser)
    {
        double h, v;
        LightRays::laserWindow(phy, h, v);
        if (info != nullptr)
        {
            info->h = h;
            info->v = v;
        }
        coneDeg = qMax(h, v);
        rays = LightRays::window(dir, h, v, mp->fixtureRigProps(fx->id()).mask());
        return !rays.isEmpty();
    }
    coneDeg = stars ? ((declared > 0.0 && declared < 170.0) ? declared : 100.0)
                    : (declared > 0.0 ? declared : 25.0);
    if (info != nullptr)
        info->halfRad = qDegreesToRadians(qMin(coneDeg, 160.0) / 2.0);
    rays = LightRays::cone(dir, qDegreesToRadians(qMin(coneDeg, 160.0) / 2.0),
                           mp->fixtureRigProps(fx->id()).mask());
    return !rays.isEmpty();
}

QString joined(ReviewContext &ctx, const QList<quint32> &ids, int max = 4)
{
    QStringList n;
    for (int i = 0; i < ids.count() && i < max; ++i)
        n << ctx.fixtureName(ids.at(i));
    QString s = n.join(QStringLiteral(", "));
    if (ids.count() > max)
        s += tr(" and more");
    return s;
}

/** How far each ray runs before solid scenery (not a scrim) stops it: a
 *  zone behind a cyc or a closed drape is not reached through it. */
QVector<double> sceneryStops(MonitorProperties *mp, const QVector3D &apex, const QVector<QVector3D> &rays)
{
    QVector<double> stops(rays.size(), kRange);
    QList<const Scenery *> solid;
    for (const Scenery *sc : mp->sceneries())
        if (sc != nullptr && !sc->isSeeThrough())
            solid << sc;
    if (solid.isEmpty())
        return stops;
    for (int i = 0; i < rays.size(); ++i)
        for (const Scenery *sc : solid)
        {
            double t = 0.0;
            if (sc->rayHit(apex, rays.at(i), t, stops.at(i)) && t > 0.05)
                stops[i] = t;
        }
    return stops;
}

/** One light's crossing of one zone, and the cut of tape that clears it. */
struct LeakInfo
{
    bool hit = false;
    bool laser = false;
    double lowZ = 0.0;          //!< the lowest height a ray enters the zone at
    double cutBottom = 0.0;     //!< more tape on the bottom that clears it; 0: none does
};

bool anyHit(const NoLightZone &z, const QVector3D &apex, const QVector<QVector3D> &rays,
            const QVector<double> &stops, double *lowZ = nullptr)
{
    bool hit = false;
    for (int i = 0; i < rays.size(); ++i)
    {
        double ez = 0.0;
        if (z.hitByRay(apex, rays.at(i), stops.at(i), &ez))
        {
            hit = true;
            if (lowZ == nullptr)
                return true;
            *lowZ = qMin(*lowZ, ez);
        }
    }
    return hit;
}

LeakInfo leakOf(MonitorProperties *mp, const NoLightZone &z, const QVector3D &apex,
                const QVector<QVector3D> &rays, const QVector<double> &stops, const Throw &info,
                const LightMask &mask)
{
    LeakInfo li;
    li.laser = info.laser;
    double low = 1e9;
    if (!anyHit(z, apex, rays, stops, &low))
        return li;
    li.hit = true;
    li.lowZ = qMax(low, double(z.floorZ));
    // More tape on the bottom, in half degrees, until nothing crosses.
    const double room = info.laser ? info.v : qRadiansToDegrees(info.halfRad) * 2.0;
    for (double cut = 0.5; cut < room - double(mask.top) - double(mask.bottom) - 0.4; cut += 0.5)
    {
        LightMask m = mask;
        m.bottom += float(cut);
        const QVector<QVector3D> r2 = info.laser ? LightRays::window(info.dir, info.h, info.v, m)
                                                 : LightRays::cone(info.dir, info.halfRad, m);
        if (!anyHit(z, apex, r2, sceneryStops(mp, apex, r2)))
        {
            li.cutBottom = cut;
            break;
        }
    }
    return li;
}

void pwr11(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    // decided (can change): reported in the first cue a light's cone starts
    // to cross a zone, not again in the cues that track it on. A light whose
    // aim is a line or a target is read through the palette's pan and tilt.
    Doc *doc = ctx.doc();
    MonitorProperties *mp = doc->monitorProperties();
    if (mp == nullptr || mp->noLightZones().isEmpty())
        return;
    const QList<NoLightZone> zones = mp->noLightZones();
    QHash<QPair<quint32, quint32>, QList<SceneValue>> cache;
    QHash<QPair<quint32, quint32>, LeakInfo> leaks;     // (zone, fixture)

    for (const Production &p : doc->showTree()->productions())
    {
        for (const CueList &list : p.lists)
        {
            if (list.looks || list.isSubCues())
                continue;
            const QList<ResolvedCue> &res = ctx.resolved(list);
            if (res.count() != list.cues.count())
                continue;
            // zone id -> the lights crossing it, by cue index
            QHash<int, QHash<quint32, QSet<quint32>>> hits;
            auto hitsAt = [&](int i) -> QHash<quint32, QSet<quint32>> {
                auto it = hits.find(i);
                if (it != hits.end())
                    return it.value();
                QHash<quint32, QSet<quint32>> found;
                const QHash<quint32, Beam> beams = beamsOf(ctx, res.at(i), cache);
                for (auto b = beams.constBegin(); b != beams.constEnd(); ++b)
                {
                    if (!b.value().lit)
                        continue;
                    Fixture *fx = doc->fixture(b.key());
                    QVector3D apex;
                    QVector<QVector3D> rays;
                    bool laser;
                    double cone;
                    Throw info;
                    if (!raysOf(doc, fx, b.value(), apex, rays, laser, cone, &info))
                        continue;
                    const QVector<double> stops = sceneryStops(mp, apex, rays);
                    for (const NoLightZone &z : zones)
                    {
                        if (!z.applies(laser, cone))
                            continue;
                        const LeakInfo li = leakOf(mp, z, apex, rays, stops, info, mp->fixtureRigProps(b.key()).mask());
                        if (li.hit)
                        {
                            found[z.id].insert(b.key());
                            leaks.insert(qMakePair(z.id, b.key()), li);
                        }
                    }
                }
                hits.insert(i, found);
                return found;
            };
            for (int i = 0; i < list.cues.count(); ++i)
            {
                const Cue &cue = list.cues.at(i);
                if (cue.skip)
                    continue;
                const int before = playedBefore(list, i);
                const QHash<quint32, QSet<quint32>> now = hitsAt(i);
                if (now.isEmpty())
                    continue;
                const QHash<quint32, QSet<quint32>> none;
                const QHash<quint32, QSet<quint32>> prev = before >= 0 ? hitsAt(before) : none;
                for (const NoLightZone &z : zones)
                {
                    QList<quint32> fresh;
                    const QSet<quint32> was = prev.value(z.id);
                    for (quint32 fid : now.value(z.id))
                        if (!was.contains(fid))
                            fresh << fid;
                    if (fresh.isEmpty())
                        continue;
                    std::sort(fresh.begin(), fresh.end());
                    QStringList keys { QStringLiteral("list:%1/cue:%2").arg(list.id).arg(cue.id),
                                       QStringLiteral("zone:%1").arg(z.id) };
                    QByteArray print = QByteArray::number(z.id) + ':';
                    for (quint32 fid : fresh)
                    {
                        keys << QStringLiteral("fx:%1").arg(fid);
                        print += QByteArray::number(fid) + ',';
                    }
                    print += QByteArray::number(qRound(z.x * 100)) + ',' + QByteArray::number(qRound(z.y * 100))
                             + ',' + QByteArray::number(qRound(z.width * 100)) + ','
                             + QByteArray::number(qRound(z.depth * 100)) + ','
                             + QByteArray::number(qRound(z.floorZ * 100)) + ','
                             + QByteArray::number(qRound(z.height * 100)) + ',' + QByteArray::number(z.kind);
                    QString how;
                    QVariantList fixMask;
                    for (quint32 fid : fresh)
                    {
                        const LeakInfo li = leaks.value(qMakePair(z.id, fid));
                        if (!li.laser && li.cutBottom <= 0.0)
                            continue;
                        if (li.cutBottom > 0.0)
                        {
                            how += tr(" The bottom of %1's window passes through it at %2: tape off about "
                                      "%3° of the bottom.")
                                       .arg(ctx.fixtureName(fid),
                                            MonitorProperties::formatStageDistance(float(li.lowZ)))
                                       .arg(li.cutBottom, 0, 'f', 1);
                            fixMask << fid << li.cutBottom;
                        }
                        else
                            how += tr(" No cut of the bottom alone keeps %1's window out of it: tape another "
                                      "edge, aim it elsewhere, or move the zone.").arg(ctx.fixtureName(fid));
                    }
                    ReviewFinding f = ReviewContext::make(QStringLiteral("PWR-11"), ReviewFinding::Warning,
                        tr("In %1 a lit beam from %2 crosses the no-light zone '%3' (%4, up to %5).%6 "
                           "Aim it off, darken it in this cue, or move the zone.")
                            .arg(ctx.cueText(list, i), joined(ctx, fresh), z.name,
                                 NoLightZone::kindLabel(z.kind).toLower(),
                                 MonitorProperties::formatStageDistance(float(z.floorZ + z.height)), how),
                        keys, print, QStringLiteral("cue"), QVariantList() << cue.id);
                    f.fixMask = fixMask;
                    out << f;
                }
            }
        }
    }
}
/* PWR-12, backwash: light on scenery kept for light on purpose (a cyc, a
   scrim: Scenery::lightOnPurposeOnly()) that nobody meant. Exact and
   reproducible from the show: a lit beam's sampled rays (LightRays::cone,
   the axis and three rings of sixteen) are followed from the light until
   they meet the stage floor, anything solid the stage counts
   (BeamOcclusion), or a piece of scenery -- a scrim lets them on through,
   a curtain stops them where it hangs in that cue (curtainPoses()). Any ray
   that ends on a guarded piece is light on it. Meant comes only
   from the show: the fixture's stage look (Cyc / strip light, Uplight
   wash), a look aiming it at a target standing on that piece, or "I meant
   it" on a layer that lights it (Layer::meantOnScenery). */

const double kOnSurface = 0.5;      //!< metres: a target this close stands on it

/** How much of one beam lands on each guarded piece (scenery id -> rays). */
struct Landing
{
    QVector<QPair<quint32, int>> on;
    int rays = 0;
};

/** The horizontal distance from @a p to piece @a sc's line, and whether @a p
 *  is within its height; for "a target standing on it". */
bool standsOn(const Scenery *sc, const QVector3D &p)
{
    const double tol = kOnSurface + double(sc->thickness()) / 2.0;
    if (double(p.z()) < double(sc->bottom()) - tol || double(p.z()) > double(sc->bottom() + sc->height()) + tol)
        return false;
    const QList<QPointF> line = sc->baseLine(qMax(16, sc->segments()));
    for (int i = 0; i + 1 < line.count(); ++i)
    {
        const QPointF a = line.at(i), b = line.at(i + 1);
        const double ex = b.x() - a.x(), ey = b.y() - a.y();
        const double len2 = ex * ex + ey * ey;
        double t = len2 > 1e-12 ? ((double(p.x()) - a.x()) * ex + (double(p.y()) - a.y()) * ey) / len2 : 0.0;
        t = qBound(0.0, t, 1.0);
        const double dx = double(p.x()) - (a.x() + ex * t), dy = double(p.y()) - (a.y() + ey * t);
        if (dx * dx + dy * dy <= tol * tol)
            return true;
    }
    return false;
}

/** Whether target @a tid stands on @a sc: a mark, an area, a structure or a
 *  line (every point of it) -- never a person, a kit or a band instrument, who stand in front. */
bool targetOn(MonitorProperties *mp, quint32 tid, const Scenery *sc)
{
    const StageTarget *t = mp->stageTarget(tid);
    if (t == nullptr || t->kind() == StageTarget::Kind::Person || t->kind() == StageTarget::Kind::DrumKit
        || StageTarget::isBandKind(t->kind()))
        return false;
    if (t->isLine())
    {
        const QVector<QVector3D> pts = mp->targetLinePoints(t);
        if (pts.isEmpty())
            return false;
        for (const QVector3D &p : pts)
            if (!standsOn(sc, p))
                return false;
        return true;
    }
    return standsOn(sc, mp->targetAimPoint(t));
}

/* A curtain moved by a fixture (Scenery::isDriven(): the main drape on a
   Stage Curtain fixture) is where the cue puts it: the functions the cue
   plays (its function layers, carried in or its own) set the curtain
   fixture's channels, and Scenery::poseFromValues() reads them the way the
   rig views do. A list that moves the curtain somewhere leaves it at rest
   (closed: every channel 0) in a cue that plays nothing on it; a list that
   never moves it leaves it as plotted (its state()). Decided, can change. */

/** What function @a fid (a scene, or a collection of them) sets on fixture
 *  @a fxId, into @a values. False when it sets nothing there. */
bool curtainValues(Doc *doc, quint32 fid, quint32 fxId, QByteArray &values, int depth = 0)
{
    Function *f = doc->function(fid);
    if (f == nullptr || depth > 4)
        return false;
    bool any = false;
    if (Scene *scene = qobject_cast<Scene *>(f))
    {
        for (const SceneValue &sv : scene->values())
            if (sv.fxi == fxId && int(sv.channel) < values.size())
            {
                values[int(sv.channel)] = char(sv.value);
                any = true;
            }
    }
    else if (Collection *coll = qobject_cast<Collection *>(f))
    {
        for (quint32 child : coll->functions())
            any = curtainValues(doc, child, fxId, values, depth + 1) || any;
    }
    return any;
}

/** Where each driven curtain is in @a cue; @a moved: the curtains the cue set. */
BeamOcclusion::Poses curtainPoses(Doc *doc, const QList<const Scenery *> &driven, const ResolvedCue &cue,
                                  QSet<quint32> *moved = nullptr)
{
    BeamOcclusion::Poses poses;
    for (const Scenery *sc : driven)
    {
        const Fixture *fx = doc->fixture(sc->driverFixture());
        QByteArray values(int(fx->channels()), char(0));
        bool set = false;
        for (const ResolvedLayer &layer : cue.layers)
            if (layer.target.isOpaque() && !layer.release)
                set = curtainValues(doc, layer.target.function, fx->id(), values) || set;
        if (set && moved != nullptr)
            moved->insert(sc->id());
        poses.insert(sc->id(), Scenery::poseFromValues(fx->fixtureMode(), values));
    }
    return poses;
}

QString posesKey(const BeamOcclusion::Poses &poses)
{
    QList<quint32> ids = poses.keys();
    std::sort(ids.begin(), ids.end());
    QString key;
    for (quint32 id : ids)
        key += QStringLiteral("%1:%2:%3;").arg(id).arg(poses.value(id).open, 0, 'g', 6)
                                          .arg(poses.value(id).fly, 0, 'g', 6);
    return key;
}

Landing landingOf(Doc *doc, Fixture *fx, const Beam &b, const QList<Scenery *> &all,
                  const BeamOcclusion::ModelPtr &sight, const BeamOcclusion::Poses &poses)
{
    Landing out;
    QVector3D apex;
    QVector<QVector3D> rays;
    bool laser;
    double cone;
    if (!raysOf(doc, fx, b, apex, rays, laser, cone))
        return out;
    out.rays = rays.count();
    const QVector<BeamOcclusion::Exclude> own = BeamOcclusion::ownMounts(doc->monitorProperties(), fx->id());
    QMap<quint32, int> on;
    for (QVector3D d : rays)
    {
        d.normalize();
        // The floor, or as far as Show Review follows a beam.
        double best = kRange;
        if (d.z() < -1e-6f && apex.z() > 0.0f)
            best = qMin(best, double(-apex.z() / d.z()));
        // Anything solid the stage counts but scenery (taken below, as it is).
        if (sight && !sight->isEmpty())
        {
            const BeamOcclusion::Hit h = sight->ray(apex, d, float(best), own);
            if (h.blocked && h.kind != BeamOcclusion::Drapes && h.kind != BeamOcclusion::Walls
                && double(h.distance) > 0.05 && double(h.distance) < best)
                best = double(h.distance);
        }
        // Scenery, nearest first: a scrim lets the ray on through.
        QVector<QPair<double, const Scenery *>> hits;
        for (const Scenery *sc : all)
        {
            double t = 0.0;
            const auto pose = poses.constFind(sc->id());
            if (sc->rayHit(apex, d, t, best, 16, nullptr, pose != poses.constEnd() ? &pose.value() : nullptr)
                && t > 0.05)
                hits << qMakePair(t, sc);
        }
        std::sort(hits.begin(), hits.end(), [](const QPair<double, const Scenery *> &x,
                                               const QPair<double, const Scenery *> &y) {
            return x.first != y.first ? x.first < y.first : x.second->id() < y.second->id();
        });
        for (const auto &h : hits)
        {
            if (h.second->lightOnPurposeOnly())
                on[h.second->id()]++;
            if (!h.second->isSeeThrough())
                break;
        }
    }
    for (auto it = on.constBegin(); it != on.constEnd(); ++it)
        out.on << qMakePair(it.key(), it.value());
    return out;
}

bool meantLook(const Fixture *fx)
{
    return fx->stageLook() == Fixture::CycStripLook || fx->stageLook() == Fixture::UplightWashLook;
}

/** The "I meant it" marks of the show: (cue, layer) -> fixtures. */
QHash<QPair<quint32, quint32>, QList<quint32>> meantMarks(Doc *doc)
{
    QHash<QPair<quint32, quint32>, QList<quint32>> marks;
    for (const Production &p : doc->showTree()->productions())
        for (const CueList &list : p.lists)
            for (const Cue &cue : list.cues)
                for (const Layer &layer : cue.layers)
                    if (!layer.meantOnScenery.isEmpty())
                        marks.insert(qMakePair(cue.id, layer.id), layer.meantOnScenery);
    return marks;
}

/** Who marked a layer meant, when, and the note with it. */
struct MeantWho
{
    QString by, when, note;
};

/** (cue, layer) -> who said I meant it. */
QHash<QPair<quint32, quint32>, MeantWho> meantPeople(Doc *doc)
{
    QHash<QPair<quint32, quint32>, MeantWho> out;
    for (const Production &p : doc->showTree()->productions())
        for (const CueList &list : p.lists)
            for (const Cue &cue : list.cues)
                for (const Layer &layer : cue.layers)
                    if (!layer.meantOnScenery.isEmpty())
                        out.insert(qMakePair(cue.id, layer.id), { layer.meantBy, layer.meantWhen, layer.meantNote });
    return out;
}

/** What one cue puts on the guarded scenery: (scenery, fixture) -> rays on
 *  it, split by how it is meant. */
struct CueLanding
{
    QMap<QPair<quint32, quint32>, int> unmeant;     //!< not meant
    QMap<QPair<quint32, quint32>, int> marked;      //!< meant only by "I meant it"
    QSet<QPair<quint32, quint32>> meant;            //!< meant some other way
    QHash<quint32, int> rays;                       //!< fixture -> rays sampled
    QHash<quint32, QPair<quint32, quint32>> litBy;
    QHash<quint32, QVector<QPair<quint32, quint32>>> markedOn;  //!< fixture -> layers marking it
};

void pwr12(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    Doc *doc = ctx.doc();
    MonitorProperties *mp = doc->monitorProperties();
    if (mp == nullptr)
        return;
    QList<Scenery *> all;
    bool anyGuarded = false;
    for (Scenery *sc : mp->sceneries())
    {
        if (sc == nullptr)
            continue;
        all << sc;
        anyGuarded = anyGuarded || sc->lightOnPurposeOnly();
    }
    if (!anyGuarded)
        return;
    std::sort(all.begin(), all.end(), [](const Scenery *a, const Scenery *b) { return a->id() < b->id(); });
    // Curtains at their fixed state: the show's, never where one is now.
    const BeamOcclusion::ModelPtr sight = mp->occlusionModel(BeamOcclusion::Poses());
    const QHash<QPair<quint32, quint32>, QList<quint32>> marks = meantMarks(doc);
    const QHash<QPair<quint32, quint32>, MeantWho> meantWho = meantPeople(doc);
    QHash<QPair<quint32, quint32>, QList<SceneValue>> cache;
    // (fixture, pan, tilt, curtains) -> where it lands: a look reused across cues is followed once.
    QHash<QString, Landing> landings;
    QList<const Scenery *> driven;
    for (const Scenery *sc : all)
    {
        const Fixture *dfx = sc->isDriven() ? doc->fixture(sc->driverFixture()) : nullptr;
        if (dfx != nullptr && dfx->fixtureMode() != nullptr)
            driven << sc;
    }

    for (const Production &p : doc->showTree()->productions())
    {
        for (const CueList &list : p.lists)
        {
            if (list.looks || list.isSubCues())
                continue;
            const QList<ResolvedCue> &res = ctx.resolved(list);
            if (res.count() != list.cues.count())
                continue;
            // The curtains this list moves somewhere; the others stay as plotted.
            QSet<quint32> listMoves;
            if (!driven.isEmpty())
                for (const ResolvedCue &rc : res)
                    curtainPoses(doc, driven, rc, &listMoves);
            QHash<int, CueLanding> done;
            auto landingAt = [&](int i) -> CueLanding {
                auto it = done.find(i);
                if (it != done.end())
                    return it.value();
                CueLanding cl;
                BeamOcclusion::Poses poses;
                if (!listMoves.isEmpty())
                {
                    const BeamOcclusion::Poses at = curtainPoses(doc, driven, res.at(i));
                    for (auto p = at.constBegin(); p != at.constEnd(); ++p)
                        if (listMoves.contains(p.key()))
                            poses.insert(p.key(), p.value());
                }
                const QString curtains = posesKey(poses);
                const QHash<quint32, Beam> beams = beamsOf(ctx, res.at(i), cache);
                QList<quint32> ids = beams.keys();
                std::sort(ids.begin(), ids.end());
                for (quint32 fid : ids)
                {
                    const Beam &b = beams[fid];
                    if (!b.lit)
                        continue;
                    Fixture *fx = doc->fixture(fid);
                    const QString key = QStringLiteral("%1/%2/%3/%4/%5").arg(fid).arg(b.hasPos)
                                        .arg(b.pan, 0, 'g', 17).arg(b.tilt, 0, 'g', 17).arg(curtains);
                    auto lit = landings.find(key);
                    if (lit == landings.end())
                        lit = landings.insert(key, landingOf(doc, fx, b, all, sight, poses));
                    const Landing &land = lit.value();
                    if (land.on.isEmpty())
                        continue;
                    cl.rays.insert(fid, land.rays);
                    cl.litBy.insert(fid, b.litBy);
                    QVector<QPair<quint32, quint32>> markedBy;
                    for (const QPair<quint32, quint32> &l : b.layers)
                        if (marks.value(l).contains(fid))
                            markedBy << l;
                    cl.markedOn.insert(fid, markedBy);
                    for (const QPair<quint32, int> &on : land.on)
                    {
                        const QPair<quint32, quint32> pair(on.first, fid);
                        bool aimed = false;
                        for (quint32 tid : b.aimTargets)
                            aimed = aimed || targetOn(mp, tid, mp->scenery(on.first));
                        if (meantLook(fx) || aimed)
                            cl.meant.insert(pair);
                        else if (!markedBy.isEmpty())
                            cl.marked.insert(pair, on.second);
                        else
                            cl.unmeant.insert(pair, on.second);
                    }
                }
                done.insert(i, cl);
                return cl;
            };

            /* Play order. A light marked meant stays meant while it keeps
               landing in the cues that follow (its layer set again, say);
               a light not meant is one run from the cue it starts landing
               in until it stops: one finding, every cue of it marked. */
            QHash<int, QSet<QPair<quint32, quint32>>> meantNow;       // cue -> pairs meant (any way)
            QHash<int, QHash<QPair<quint32, quint32>, int>> runOf;   // cue -> pair -> run start cue
            QMap<QPair<int, quint32>, QList<quint32>> fresh;          // (cue, scenery) -> fixtures
            QMap<QPair<int, quint32>, QList<quint32>> freshMarked;
            QMap<QPair<int, quint32>, QList<quint32>> runCues;        // (start, scenery) -> cue indexes
            for (int i = 0; i < list.cues.count(); ++i)
            {
                if (list.cues.at(i).skip)
                    continue;
                const int before = playedBefore(list, i);
                CueLanding cl = landingAt(i);
                const QSet<QPair<quint32, quint32>> prevMeant = before >= 0 ? meantNow.value(before)
                                                                            : QSet<QPair<quint32, quint32>>();
                const QHash<QPair<quint32, quint32>, int> prevRun = before >= 0 ? runOf.value(before)
                                                                                : QHash<QPair<quint32, quint32>, int>();
                QSet<QPair<quint32, quint32>> nowMeant = cl.meant;
                for (auto it = cl.marked.constBegin(); it != cl.marked.constEnd(); ++it)
                {
                    nowMeant.insert(it.key());
                    if (!prevMeant.contains(it.key()))
                        freshMarked[qMakePair(i, it.key().first)] << it.key().second;
                }
                QHash<QPair<quint32, quint32>, int> nowRun;
                for (auto it = cl.unmeant.constBegin(); it != cl.unmeant.constEnd(); ++it)
                {
                    if (prevMeant.contains(it.key()))
                    {
                        nowMeant.insert(it.key());      // carried on from a cue it was meant in
                        continue;
                    }
                    const auto pr = prevRun.constFind(it.key());
                    const int start = pr != prevRun.constEnd() ? pr.value() : i;
                    nowRun.insert(it.key(), start);
                    if (start == i)
                        fresh[qMakePair(i, it.key().first)] << it.key().second;
                    QList<quint32> &rc = runCues[qMakePair(start, it.key().first)];
                    if (!rc.contains(quint32(i)))
                        rc << quint32(i);
                }
                meantNow.insert(i, nowMeant);
                runOf.insert(i, nowRun);
                done.insert(i, cl);
            }

            auto fxText = [&](const CueLanding &cl, quint32 sid, const QList<quint32> &fids) {
                QStringList parts;
                for (int k = 0; k < fids.count() && k < 4; ++k)
                {
                    const quint32 fid = fids.at(k);
                    const QPair<quint32, quint32> pair(sid, fid);
                    const int on = cl.unmeant.value(pair, cl.marked.value(pair));
                    const int rays = qMax(1, cl.rays.value(fid));
                    parts << tr("%1 (about %2%)").arg(ctx.fixtureName(fid))
                                 .arg(qMax(1, qRound(100.0 * on / rays)));
                }
                QString s = parts.join(QStringLiteral(", "));
                if (fids.count() > 4)
                    s += tr(" and %1 more").arg(fids.count() - 4);
                return s;
            };
            auto emitFinding = [&](int i, quint32 sid, QList<quint32> fids, bool intended) {
                std::sort(fids.begin(), fids.end());
                const Cue &cue = list.cues.at(i);
                const Scenery *sc = mp->scenery(sid);
                const CueLanding cl = landingAt(i);
                QStringList keys { QStringLiteral("list:%1/cue:%2").arg(list.id).arg(cue.id),
                                   QStringLiteral("scenery:%1").arg(sid) };
                QByteArray print = QByteArray::number(sid) + ':';
                QVariantList markList;
                quint32 jumpTo = cue.id;
                for (quint32 fid : fids)
                {
                    keys << QStringLiteral("fx:%1").arg(fid);
                    print += QByteArray::number(fid) + ',';
                    if (intended)
                    {
                        for (const QPair<quint32, quint32> &l : cl.markedOn.value(fid))
                            markList << QVariant(QVariantList() << l.first << l.second << fid);
                    }
                    else
                    {
                        const QPair<quint32, quint32> l = cl.litBy.value(fid);
                        if (l.first != invalidId() && l.second != invalidId())
                        {
                            markList << QVariant(QVariantList() << l.first << l.second << fid);
                            if (jumpTo == cue.id && l.first == cue.id)
                                jumpTo = l.second;      // "Show me": its layer in this cue
                        }
                    }
                }
                QVariantList cues;
                for (quint32 idx : runCues.value(qMakePair(i, sid), QList<quint32>() << quint32(i)))
                    cues << list.cues.at(int(idx)).id;
                const QString what = sc != nullptr ? QStringLiteral("%1 '%2'")
                                     .arg(Scenery::kindName(sc->kind()).toLower(), sc->name()) : QString();
                ReviewFinding f = ReviewContext::make(QStringLiteral("PWR-12"), ReviewFinding::Warning,
                    intended
                        ? tr("In %1 light from %2 lands on the %3: meant (I meant it).")
                              .arg(ctx.cueText(list, i), fxText(cl, sid, fids), what)
                        : tr("In %1 light from %2 lands on the %3, not marked as meant. If you meant it, "
                             "say so (I meant it); if not, aim it off, shutter or zoom it, or darken it "
                             "in this cue.")
                              .arg(ctx.cueText(list, i), fxText(cl, sid, fids), what),
                    keys, print, QStringLiteral("cue"),
                    QVariantList() << jumpTo << QVariant(cues) << QVariant(markList));
                if (intended)
                {
                    f.state = ReviewFinding::Accepted;
                    f.intended = true;
                    // Who, when and the note: the latest mark on the layers that say it.
                    for (quint32 fid : fids)
                        for (const QPair<quint32, quint32> &l : cl.markedOn.value(fid))
                        {
                            const MeantWho who = meantWho.value(l);
                            if (f.acceptedWhen.isEmpty() || who.when > f.acceptedWhen)
                            {
                                f.acceptedBy = who.by;
                                f.acceptedWhen = who.when;
                                f.acceptedNote = who.note;
                            }
                        }
                }
                out << f;
            };
            for (auto it = fresh.constBegin(); it != fresh.constEnd(); ++it)
                emitFinding(it.key().first, it.key().second, it.value(), false);
            for (auto it = freshMarked.constBegin(); it != freshMarked.constEnd(); ++it)
                emitFinding(it.key().first, it.key().second, it.value(), true);
        }
    }
}

}

QList<ShowReview::Check> ShowReviewChecks::zoneChecks()
{
    typedef ShowReview S;
    return {
        { QStringLiteral("PWR-11"), tr("Light into a no-light zone"), S::Power,
          tr("A zone marked on the plot as dark (the orchestra pit, the conductor, a camera, the "
             "audience's eyes) that a lit beam's cone crosses in a cue. It is named once, in the "
             "first cue the beam starts to cross it. Aim the light off, darken it, or move the "
             "zone."), pwr11 },
        { QStringLiteral("PWR-12"), tr("Light on the cyc nobody meant (backwash)"), S::Power,
          tr("Light that lands on a cyc or a scrim (or any scenery set to take light only on "
             "purpose) by accident: a front light or a special at a performer that carries on and "
             "splashes the cyc behind them. Any part of a lit beam's cone that reaches it counts, "
             "followed from the plot; a closed curtain in front of it (as that cue leaves it) "
             "stops the light. It is meant when the fixture is a Cyc / strip light or an "
             "Uplight wash, when its look aims it at a target on the cyc, or when you said so: "
             "I meant it, kept on the layer that lights it. Named once, in the cue it starts to "
             "land; the Show Tree marks every cue it lands in."), pwr12 },
    };
}

/* PWR-12's answers, from the panel and the Show Tree. */

bool ShowReview::canMarkMeant(const ReviewFinding &finding, bool meant)
{
    if (finding.check != QLatin1String("PWR-12") || finding.jumpData.count() < 3)
        return false;
    return meant ? (!finding.intended && finding.isOpen()) : finding.intended;
}

QList<quint32> ShowReview::findingCues(const ReviewFinding &finding)
{
    QList<quint32> cues;
    if (finding.check != QLatin1String("PWR-12") || finding.jumpData.count() < 2)
        return cues;
    for (const QVariant &v : finding.jumpData.at(1).toList())
        cues << v.toUInt();
    return cues;
}

namespace
{
Layer *findLayer(QList<Production> &tree, quint32 cueId, quint32 layerId)
{
    for (Production &p : tree)
        for (CueList &list : p.lists)
            for (Cue &cue : list.cues)
                if (cue.id == cueId)
                    return cue.layer(layerId);
    return nullptr;
}
}

bool ShowReview::setLightMeant(Doc *doc, const ReviewFinding &finding, bool meant, const QString &note,
                               const QString &by)
{
    if (doc == nullptr || !canMarkMeant(finding, meant))
        return false;
    const QVariantList marks = finding.jumpData.at(2).toList();
    if (marks.isEmpty())
        return false;
    const QString who = by.trimmed().isEmpty() ? defaultName() : by.trimmed();
    const QString when = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString why = note.trimmed();
    return doc->showTree()->edit(meant ? tr("I meant it") : tr("Not meant"),
                                 [&marks, meant, who, when, why](QList<Production> &tree) {
        bool any = false;
        for (const QVariant &v : marks)
        {
            const QVariantList m = v.toList();
            if (m.count() < 3)
                continue;
            Layer *layer = findLayer(tree, m.at(0).toUInt(), m.at(1).toUInt());
            const quint32 fid = m.at(2).toUInt();
            if (layer == nullptr)
                continue;
            if (meant && !layer->meantOnScenery.contains(fid))
            {
                layer->meantOnScenery << fid;
                std::sort(layer->meantOnScenery.begin(), layer->meantOnScenery.end());
                layer->meantBy = who;
                layer->meantWhen = when;
                layer->meantNote = why;
                any = true;
            }
            else if (!meant && layer->meantOnScenery.removeAll(fid) > 0)
            {
                if (layer->meantOnScenery.isEmpty())
                    layer->meantBy = layer->meantWhen = layer->meantNote = QString();
                any = true;
            }
        }
        return any;
    });
}

bool ShowReview::clearLayerMeant(Doc *doc, quint32 cueId, quint32 layerId)
{
    if (doc == nullptr)
        return false;
    return doc->showTree()->edit(tr("Not meant"), [cueId, layerId](QList<Production> &tree) {
        Layer *layer = findLayer(tree, cueId, layerId);
        if (layer == nullptr || layer->meantOnScenery.isEmpty())
            return false;
        layer->meantOnScenery.clear();
        layer->meantBy = layer->meantWhen = layer->meantNote = QString();
        return true;
    });
}
