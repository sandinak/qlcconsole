/*
  Q Light Controller Plus
  plotplacement.h

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

#ifndef PLOTPLACEMENT_H
#define PLOTPLACEMENT_H

/* The bench shows' fixtures on the plot (rainbow-movers, hazard-arming),
   with the engine's own calls, as the Lighting Studio's drop and Attach to
   Pipe store them: a plot item (MonitorProperties::setFixturePosition, X/Y
   in millimetres, Z in metres) on the placement layer, and its rig props
   (FixtureRigProps: metres). Lengths a person reads elsewhere go through
   MeasurementUnits; these are storage. */

#include <QVector3D>
#include <QtMath>

#include "monitorproperties.h"
#include "stagetarget.h"
#include "fixture.h"
#include "truss.h"
#include "pipe.h"

namespace PlotPlacement
{

constexpr float kFoot = 0.3048f;        // metres

/** A horizontal pipe across the stage, centred on the centre line,
 *  @p upstage metres upstage of the downstage edge, @p trim metres up. */
inline Pipe *addFrontPipe(MonitorProperties *mp, const QString &name, float length, float trim, float upstage)
{
    Pipe *p = mp->addPipe();
    p->setName(name);
    p->setOrientation(Pipe::Horizontal);
    p->setRunAngle(0.0f);                           // along X
    p->setLength(length);
    p->setBaseZ(trim);
    p->setBaseRadius(0.0f);                         // hung: no stand under it
    p->setOriginX(mp->stageCentre().x() - length / 2.0f);
    p->setOriginY(mp->downstageEdgeY() - upstage);
    p->setLayerId(mp->placementLayerId());
    return p;
}

/** A target on the deck, @p across metres from the centre line (+: stage
 *  left), @p upstage metres upstage of the downstage edge. */
inline StageTarget *addTarget(MonitorProperties *mp, const QString &name, float across, float upstage)
{
    StageTarget *t = mp->addStageTarget();
    t->setName(name);
    t->setPosition(QVector3D(mp->stageCentre().x() + across, mp->downstageEdgeY() - upstage, 0.0f));
    mp->notifyStageTargetsChanged();
    return t;
}

/** A plot item for @p fid at @p world (metres), on the placement layer. */
inline void place(MonitorProperties *mp, quint32 fid, const QVector3D &world)
{
    mp->setFixturePosition(fid, 0, 0, QVector3D(world.x() * 1000.0f, world.y() * 1000.0f, world.z()));
    mp->setFixtureLayer(fid, mp->placementLayerId());
}

/** Hung under @p pipe, @p offset metres along it. */
inline void hang(MonitorProperties *mp, Fixture *f, Pipe *pipe, float offset)
{
    place(mp, f->id(), pipe->positionAt(offset));
    FixtureRigProps rp = mp->fixtureRigProps(f->id());
    rp.clearMounts();
    rp.pipeId = pipe->id();
    rp.pipeOffset = offset;
    rp.mountingType = Truss::TopHung;
    mp->setFixtureRigProps(f->id(), rp);
}

/** Standing on the deck at @p across / @p upstage (as addTarget()), facing downstage. */
inline void standOnDeck(MonitorProperties *mp, Fixture *f, float across, float upstage)
{
    place(mp, f->id(), QVector3D(mp->stageCentre().x() + across, mp->downstageEdgeY() - upstage, 0.0f));
    FixtureRigProps rp = mp->fixtureRigProps(f->id());
    rp.clearMounts();
    rp.mountingType = Truss::FloorMounted;
    rp.panZeroDir = 0.0f;
    mp->setFixtureRigProps(f->id(), rp);
}

/** A fixed light (no pan or tilt) pointed at @p t from where it hangs
 *  (FixtureRigProps::fixedAim: bearing clockwise from downstage, down from level). */
inline void pointFixedAt(MonitorProperties *mp, Fixture *f, const StageTarget *t)
{
    const QVector3D from = mp->fixtureRigPosition(f->id());
    const QVector3D d = t->position() - from;
    FixtureRigProps rp = mp->fixtureRigProps(f->id());
    rp.fixedAim = true;
    rp.panZeroDir = float(qRadiansToDegrees(qAtan2(double(-d.x()), double(d.y()))));
    rp.aimDown = float(qRadiansToDegrees(qAtan2(double(-d.z()), qSqrt(double(d.x() * d.x() + d.y() * d.y())))));
    mp->setFixtureRigProps(f->id(), rp);
}

}

#endif
