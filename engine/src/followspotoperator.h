/*
  Q Light Controller Plus
  followspotoperator.h

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

#ifndef FOLLOWSPOTOPERATOR_H
#define FOLLOWSPOTOPERATOR_H

#include <QCoreApplication>
#include <QPointF>
#include <QString>
#include <QList>

class MonitorProperties;

/** The show's operators (ProgrammerController), in <Engine>. */
#define KXMLQLCFollowSpotOperators QStringLiteral("FollowSpotOperators")

/** @addtogroup engine Engine
 * @{
 */

/**
 * Where the follow-spot operator stands and which way they face: front of
 * house (the default, as it always was), a side tower, upstage, the back
 * of the house. The stick is turned by the facing before it moves the
 * subject, so the stick pushed up always sends the subject the way the
 * operator looks -- away from where they stand -- and right is their right.
 *
 * Facing is degrees on the plan, clockwise seen from above, in 15-degree
 * steps: 0 looks upstage (-Y), 90 toward stage left (+X), 180 downstage,
 * 270 toward stage right. Positions are world metres (the plan's: X stage
 * left +, Y downstage +); eye height is millimetres, shown in Settings >
 * Units like every length.
 *
 * Kept by ProgrammerController per controller (the stick) and per venue,
 * saved with the show.
 */
struct FollowSpotOperator
{
    Q_DECLARE_TR_FUNCTIONS(FollowSpotOperator)

public:
    enum Preset
    {
        FrontOfHouse = 0,   ///< the default: as it always was
        StageLeftTower,
        StageRightTower,
        Upstage,
        BackOfHouse
    };

    Preset preset = FrontOfHouse;
    int facing = 0;                 ///< degrees, 0..345 in 15-degree steps
    int eyeMm = kDefaultEyeMm;      ///< eye height above the stage floor

    static constexpr int kStep = 15;
    static constexpr int kDefaultEyeMm = 1600;

    /** The preset with its own facing and eye height. */
    static FollowSpotOperator fromPreset(Preset p);
    static int presetFacing(Preset p);
    static int presetEyeMm(Preset p);
    /** "Front of house", "SL tower"... */
    static QString presetName(Preset p);
    /** The XML key: "FOH", "SL", "SR", "US", "BOH". */
    static QString presetKey(Preset p);
    /** From the key; FrontOfHouse for an unknown one. */
    static Preset presetFromKey(const QString &key);
    static QList<Preset> presets();

    /** The nearest 15-degree step, 0..345. */
    static int snapFacing(double degrees);
    /** "0° (upstage)", "90° (toward stage left)", "105°"... */
    static QString facingName(int facing);

    /** Nothing changed from the way it always was: front of house, facing
     *  upstage (the eye height is only for the rig view). */
    bool isFrontOfHouse() const { return preset == FrontOfHouse && facing == 0; }
    bool operator==(const FollowSpotOperator &o) const
    { return preset == o.preset && facing == o.facing && eyeMm == o.eyeMm; }
    bool operator!=(const FollowSpotOperator &o) const { return !(*this == o); }

    /** For the footer: "from SL tower", "from SL tower, facing 255°";
     *  empty from front of house facing upstage. */
    QString viewpointText() const;

    /** Where the preset stands on @p props's stage, world metres: a little
     *  off the stage's edge (front of house and back of house out in the
     *  house). */
    static QPointF position(Preset p, const MonitorProperties *props);
    QPointF position(const MonitorProperties *props) const { return position(preset, props); }

    /** The facing from @p from that puts @p centre straight ahead, turned by
     *  a stick pushed (@p sx right, @p sy up, -1..1) toward it as the
     *  operator stands -- pushed straight up, they face it; pushed right,
     *  it is on their right. Snapped to 15 degrees. -1 when the stick is
     *  not pushed far enough (half way) to say. */
    static int facingFromPush(const QPointF &from, const QPointF &centre, double sx, double sy);

    /** The rig view from where the operator stands (its Operator view):
     *  the angled view's azimuth (0 from the audience, + toward stage left)
     *  looking the way they face, and its elevation, looking down from their
     *  eye to centre stage (degrees, at least 5). */
    static double viewAzimuth(int facing);
    double viewElevation(const MonitorProperties *props) const;
    /** The plan turned in quarter turns (clockwise) so the operator is at
     *  its bottom edge and the stick pushed up goes up the screen. */
    static int planTurns(int facing);

    /** The stick (@p sx right, @p sy up) on the floor for an operator
     *  facing @p facing degrees: metres along X and Y of the plan. */
    static QPointF onFloor(double sx, double sy, int facing);
    /** The same with the facing's cosine and sine already worked out (the
     *  MasterTimer thread's per-frame form). */
    static inline QPointF onFloor(double sx, double sy, double c, double s)
    {
        // right = (cos, sin), forward = (sin, -cos) on the plan.
        return QPointF(sx * c + sy * s, sx * s - sy * c);
    }
};

/** @} */

#endif
