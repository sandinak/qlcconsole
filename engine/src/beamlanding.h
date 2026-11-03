/*
  Q Light Controller Plus
  beamlanding.h

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

#ifndef BEAMLANDING_H
#define BEAMLANDING_H

#include <QVector3D>
#include <QString>
#include <QList>
#include <climits>

#include "qlcengine_export.h"
#include "scenevalue.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Where a head's beam lands for raw pan / tilt DMX -- the forward direction
 * of AimSolver::solve(), for a position that is stored values rather than an
 * aim (a PanTilt palette, a scene's own values).
 *
 * The math is the rig view's (MonitorFixtureItem / fixturevisualtraits):
 * MSB plus fine, read across the channel's pointing band
 * (Fixture::positionBand(), so an indexed-pan head's 0-127 is its whole
 * range; read at 16 bits, as positionToValues() writes it), centred on the
 * range, turned into a world direction by
 * AimSolver::beamDirection() with the rig props (mounting, pan zero, invert,
 * calibration). The beam starts where AimSolver::frameFor() puts the head,
 * the same origin solve() aims from, and is followed to the deck (the floor,
 * or the top of a platform under it).
 *
 * Then it is named: the nearest stage target within a tolerance ("Line 1",
 * "Announcer"), else the stage area it lands in ("DS centre"), else off
 * stage, into the audience, or up into the air.
 */
namespace BeamLanding
{
    enum Kind
    {
        NoGeometry = 0,     //!< not on the plot, or no pan / tilt: nothing to say
        OnStage,            //!< lands on the deck
        IntoAudience,       //!< downstage of the stage's front edge
        OffStage,           //!< outside the stage to the sides or upstage
        IntoAir             //!< level or upwards: never meets the floor
    };

    /** Metres: a landing this close to a target is "at" it. */
    constexpr double nearTarget() { return 1.0; }

    struct Result
    {
        quint32 fixture = UINT_MAX;
        Kind kind = NoGeometry;
        QVector3D origin;           ///< the beam's origin, metres
        QVector3D direction;        ///< unit vector, +X stage left, +Y downstage, +Z up
        QVector3D point;            ///< where it lands (IntoAir: 10 m along the beam)
        quint32 target = UINT_MAX;  ///< the stage target it is at, if any
        double lineFraction = -1.0; ///< 0..1 along it, when that target is a line
        QString area;               ///< "DS centre", "US left" ...; empty off the stage

        bool valid() const { return kind != NoGeometry; }
    };

    /** The world direction of a head at pan / tilt @p panDmx / @p tiltDmx
     *  (MSB 0..255, fine as the fraction). False without rig geometry. */
    QLC_ENGINE_EXPORT bool direction(Doc *doc, quint32 fixtureId, double panDmx, double tiltDmx,
                                     QVector3D &dir);

    /** The pan and tilt DMX @p values set on @p fixtureId (MSB + LSB / 256),
     *  either may be missing. */
    QLC_ENGINE_EXPORT void dmxOf(Doc *doc, quint32 fixtureId, const QList<SceneValue> &values,
                                 double &panDmx, double &tiltDmx, bool &hasPan, bool &hasTilt);

    /** Where @p fixtureId's beam lands for @p values, named. A missing axis is
     *  read at the centre of its range. */
    QLC_ENGINE_EXPORT Result land(Doc *doc, quint32 fixtureId, const QList<SceneValue> &values);

    /** The stage area a floor point is in: up / centre / down stage by
     *  thirds of the depth to the front edge, left / centre / right by thirds
     *  of the width (stage left is +X). Empty when it is off the stage. */
    QLC_ENGINE_EXPORT QString areaAt(Doc *doc, const QVector3D &point);

    /** One phrase for a landing: "≈ Line 1", "≈ Announcer", "DS centre",
     *  "off stage", "into the audience", "into the air". */
    QLC_ENGINE_EXPORT QString describe(Doc *doc, const Result &result);

    /** A line for several heads: where most land, then what misses ("lands
     *  DS centre, 2 heads off stage"). Empty when none could be solved. */
    QLC_ENGINE_EXPORT QString summary(Doc *doc, const QList<Result> &results);
}

/** @} */

#endif
