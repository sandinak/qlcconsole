/*
  Q Light Controller Plus
  spreadlayer.h

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

#ifndef SPREADLAYER_H
#define SPREADLAYER_H

#include <QVector3D>
#include <QVector>
#include <QString>
#include <QMap>

/** @addtogroup engine Engine
 * @{
 */

/**
 * The spread layer (SPEED_SPREAD_DESIGN.md, S3): how far along an effect's
 * cycle each head is, worked out by the host so every effect spreads the same
 * way -- the phasers' degrees, the orbits' fractions, the gobo chase's steps
 * all become one "fixture.phase" in cycles.
 *
 * Heads are put in order along an AXIS (their order in the look, the head
 * layout's columns or rows, or where they stand on stage), heads at the same
 * place on that axis sharing a phase; BLOCKS of neighbours share one; the
 * order is split into mirrored WINGS; and runs in a DIRECTION (forwards,
 * backwards, from the centre out, from the ends in). Then a fraction 0..1
 * along it, times the AMOUNT in cycles, is the head's phase.
 */
class SpreadLayer
{
public:
    enum Axis { Order = 0, Columns, Rows, StageX, StageY, StageZ,
                FromTarget,     //!< distance from the target, on the floor: a ripple
                AroundTarget    //!< angle around it, from downstage: a sweep
              };
    /** The axis needs a centre (phases()' @p centre). */
    static bool needsCentre(Axis a) { return a == FromTarget || a == AroundTarget; }
    enum Direction { Forward = 0, Reverse, CentreOut, EndsIn };

    struct Settings
    {
        double amount = 0;          //!< cycles across the selection; 0 = off
        Axis axis = Order;
        Direction direction = Forward;
        int wings = 1;              //!< 1 = none
        int blocks = 1;             //!< heads that share a phase, in a row

        bool isActive() const { return amount != 0; }
        bool operator==(const Settings &o) const
        {
            return amount == o.amount && axis == o.axis && direction == o.direction
                   && wings == o.wings && blocks == o.blocks;
        }
        bool operator!=(const Settings &o) const { return !(*this == o); }
    };

    /** A head, as the layer sees it. */
    struct Point
    {
        int order = 0;              //!< its place in the look's list
        int col = 0, row = 0;       //!< its head-layout cell
        QVector3D pos;              //!< where it stands (metres)
    };

    /** The settings an effect palette carries ("__spreadAmount",
     *  "__spreadAxis", "__spreadDirection", "__spreadWings",
     *  "__spreadBlocks"). Absent: off. */
    static Settings fromParams(const QMap<QString, double> &params);
    static QString paramName(const char *what) { return QStringLiteral("__spread") + QLatin1String(what); }

    /** Each point's phase, in cycles (0 .. amount), in the points' order.
     *  @p centre is the target for FromTarget / AroundTarget; without one,
     *  centre stage (0,0). */
    static QVector<double> phases(const QVector<Point> &points, const Settings &settings,
                                  const QVector3D *centre = nullptr);
};

/** @} */

#endif
