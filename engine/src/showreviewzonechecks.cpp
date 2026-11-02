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
#include <QHash>
#include <QSet>
#include <QtMath>
#include <algorithm>

#include "showreviewcontext.h"
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "programmercontroller.h"
#include "nolightzone.h"
#include "qlcphysical.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "qlcfixturedef.h"
#include "qlcpalette.h"
#include "aimsolver.h"
#include "righeads.h"
#include "movespeed.h"
#include "function.h"
#include "fixture.h"
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
            if (posIt != layer.values.constEnd() && !posIt.value().look.none)
            {
                for (quint32 pid : posIt.value().look.palettes)
                {
                    QLCPalette *pal = doc->palette(pid);
                    if (pal == nullptr)
                        continue;
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

/** The rays a lit light throws, from @a apex. False when it has no throw. */
bool raysOf(Doc *doc, Fixture *fx, const Beam &b, QVector3D &apex, QVector<QVector3D> &rays,
            bool &laser, double &coneDeg)
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
    if (laser)
    {
        double h, v;
        LightRays::laserWindow(phy, h, v);
        coneDeg = qMax(h, v);
        rays = LightRays::window(dir, h, v);
        return true;
    }
    coneDeg = stars ? ((declared > 0.0 && declared < 170.0) ? declared : 100.0)
                    : (declared > 0.0 ? declared : 25.0);
    rays = LightRays::cone(dir, qDegreesToRadians(qMin(coneDeg, 160.0) / 2.0));
    return true;
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
                    if (!raysOf(doc, fx, b.value(), apex, rays, laser, cone))
                        continue;
                    for (const NoLightZone &z : zones)
                        if (z.applies(laser, cone) && z.hitByRays(apex, rays, kRange))
                            found[z.id].insert(b.key());
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
                    out << ReviewContext::make(QStringLiteral("PWR-11"), ReviewFinding::Warning,
                        tr("In %1 a lit beam from %2 crosses the no-light zone '%3' (%4). Aim it off, "
                           "darken it in this cue, or move the zone.")
                            .arg(ctx.cueText(list, i), joined(ctx, fresh), z.name,
                                 NoLightZone::kindLabel(z.kind).toLower()),
                        keys, print, QStringLiteral("cue"), QVariantList() << cue.id);
                }
            }
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
    };
}
