/*
  Q Light Controller Plus
  followspotdrive.h

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

#ifndef FOLLOWSPOTDRIVE_H
#define FOLLOWSPOTDRIVE_H

#include <QElapsedTimer>
#include <QPointF>
#include <QVector>

/** @addtogroup engine Engine
 * @{
 */

/**
 * How the follow spot's sticks turn into movement on the stage: one place
 * for the feel, the same for a moved subject (target mode) and a flown
 * head (free-fly).
 *
 * - Deflection: a stick's -1..1 travel, centred as the controller sends it
 *   (the HID plugin's rest point, where it learns one, is already taken
 *   out). Within the deadzone nothing; past it the travel is rescaled to
 *   start from zero, so the beam does not jump into motion at the edge.
 * - Response curve: expo, the same both ways -- fine near the centre, the
 *   whole Top Speed at full throw.
 * - Coarse + fine: the left stick at full Top Speed, the right at a share
 *   of it (Fine Stick), summed.
 * - Speed step: a multiplier the D-pad steps through (x0.25 .. x2).
 * - Smoothing: the velocity follows the sticks critically damped -- no
 *   step at a start or a stop, and no overshoot -- integrated exactly over
 *   whatever time the tick took (frame-rate independent).
 *
 * Speeds are metres a second on the stage; what a person reads is in
 * Settings > Units (MeasurementUnits).
 */
class FollowSpotDrive
{
public:
    struct Settings
    {
        double topSpeed = kDefaultTopSpeed;     ///< m/s, left stick at full throw, x1
        double fineShare = kDefaultFineShare;   ///< the right stick's full throw, a share of topSpeed
        double expo = kDefaultExpo;             ///< 0 linear .. 1 cubic
        double deadzone = 0.10;                 ///< of the -1..1 travel
    };

    /** A brisk walk: 6 ft/s, 1.83 m/s. */
    static constexpr double kDefaultTopSpeed = 1.8288;
    static constexpr double kDefaultFineShare = 0.2;
    static constexpr double kDefaultExpo = 0.4;
    /** Critically damped at this rate (1/s): 95% of a change in 0.19 s. */
    static constexpr double kResponse = 25.0;
    /** Below this (m/s) with the sticks centred, it has stopped. */
    static constexpr double kStopped = 1.0e-4;

    FollowSpotDrive() = default;

    void setSettings(const Settings &s);
    const Settings &settings() const { return m_settings; }

    /** The sticks' deflection, -1..1 each axis (x right, y up). */
    void setCoarse(double x, double y);
    void setFine(double x, double y);

    /** The D-pad's speed step. */
    static const QVector<double> &multipliers();
    double multiplier() const { return m_multiplier; }
    /** Step to the next multiplier up (+1) or down (-1); true if it changed. */
    bool stepMultiplier(int dir);
    void setMultiplier(double m);

    /** A deflection through the deadzone and the response curve: -1..1,
     *  odd (shape(-d) == -shape(d)), monotonic, 0 inside the deadzone. */
    static double shape(double deflection, double deadzone, double expo);

    /** The velocity the sticks ask for, m/s (before smoothing). */
    QPointF wanted() const;

    /** Advance @p dt seconds; returns how far it moved (metres). */
    QPointF step(double dt);

    /** The smoothed velocity now, m/s. */
    QPointF velocity() const { return QPointF(m_x.v, m_y.v); }

    /** Moving, or about to (a stick off centre). */
    bool isActive() const;

    /** Stop dead (the subject was put somewhere else). */
    void stop();

private:
    struct Axis
    {
        double v = 0;   ///< velocity
        double a = 0;   ///< its rate of change
        /** Advance toward @p w over @p dt; returns the distance covered. */
        double step(double w, double dt);
    };

    Settings m_settings;
    double m_coarseX = 0, m_coarseY = 0, m_fineX = 0, m_fineY = 0;
    double m_multiplier = 1.0;
    Axis m_x, m_y;
};

/** The time between ticks, as the tick that asks for it sees it: real
 *  elapsed time, the first tick (and a stall's) a nominal one, capped so a
 *  stall of seconds is not one big jump. A test sets the time it wants. */
class TickClock
{
public:
    explicit TickClock(double nominal = 0.02) : m_nominal(nominal) {}
    double next();
    void reset() { m_clock.invalidate(); }
    /** Tests: every tick is @p dt seconds (0 = real time again). */
    void setFixed(double dt) { m_fixed = dt; }

private:
    QElapsedTimer m_clock;
    double m_nominal;
    double m_fixed = 0;
};

/** @} */

#endif
