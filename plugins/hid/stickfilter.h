/*
  Q Light Controller Plus
  stickfilter.h

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

#ifndef STICKFILTER_H
#define STICKFILTER_H

#include <QList>
#include <QMap>
#include <QtGlobal>

/**
 * Where a controller's sticks rest, and their readings corrected for it.
 *
 * A worn or uncalibrated stick does not come back to the middle: a DualShock
 * 4 left alone can read +0.12, which every tool behind it (Focus mode, the
 * follow spot, Venue focus, a slider) took for a held push, and the lights
 * moved by themselves. The HID plugin puts every stick reading of every
 * controller through one of these, before anything else sees it:
 *
 *  - the rest is learnt: for a moment after the controller arrives, a stick
 *    holding still near the middle (kAutoLimit) is taken as resting there;
 *    "Centre Sticks" (requestCapture()) learns it again on purpose, further
 *    out (kCaptureLimit). A stick that is moving, or further out (held, a
 *    trigger, a throttle), is never taken;
 *  - each reading is moved by the rest and stretched so both ends still
 *    reach full travel (correct()): rest 0.10, full right is still 1.0;
 *  - a stick sitting still past the deadzone for driftTime() with nothing
 *    else on the controller touched is reported once (drifted()) -- never
 *    re-centred by itself: a held slow pan looks just the same.
 *
 * The deadzone itself stays with the tools (FocusSession::stickVelocity(),
 * followspot.js): this only moves the middle back to the middle.
 *
 * Readings are the plugin's channel values, 0..255 with 128 the middle.
 * Plain Qt Core, times passed in (ms): tested without a controller or a
 * clock (stickfilter_test).
 */
class StickFilter
{
public:
    /** Further out than this a resting stick is not learnt by itself. */
    static constexpr double kAutoLimit = 0.30;
    /** ...and not even by Centre Sticks: that is a stick being held. */
    static constexpr double kCaptureLimit = 0.50;
    /** How long a stick holds still (within kSteadyCounts) to be resting. */
    static constexpr qint64 kSteadyMs = 300;
    static constexpr int kSteadyCounts = 3;
    /** After it arrives, how long a controller's rest is learnt by itself. */
    static constexpr qint64 kAutoWindowMs = 2000;
    /** How long Centre Sticks waits for the sticks to be let go. */
    static constexpr qint64 kCaptureTimeoutMs = 3000;

    StickFilter() = default;

    /** The stick axes (their channels); any other channel passes untouched. */
    void setAxes(const QList<int> &channels);
    QList<int> axes() const { return m_axes.keys(); }
    bool isAxis(int channel) const { return m_axes.contains(channel); }

    /** The deadzone the tools apply (the profile's), for drifted(): as
     *  they apply it, a fraction of the 0..1 reading either side of the
     *  middle -- so 0.05 is +-0.10 of a stick's -1..1 travel. */
    void setDeadzone(double deadzone) { m_deadzone = qBound(0.0, deadzone, 0.5); }
    double deadzone() const { return m_deadzone; }

    void setDriftTime(qint64 ms) { m_driftMs = ms; }
    qint64 driftTime() const { return m_driftMs; }

    /** The controller arrived (or was opened) at @p now: learn its rest
     *  from what it reads for the next kAutoWindowMs. */
    void begin(qint64 now);

    /** Centre Sticks: learn every stick's rest again, from now. */
    void requestCapture(qint64 now);
    bool isCapturing() const { return m_captureUntil >= 0; }

    /** A reading @p raw from @p channel at @p now: what it says once the
     *  rest is taken out (for any other channel, @p raw). */
    uchar feed(int channel, uchar raw, qint64 now);

    /** What happened since the last tick. */
    struct Tick
    {
        /** Rests learnt (their corrected reading changed: send it again). */
        QList<int> learnt;
        /** Centre Sticks finished: true once, with how many sticks were
         *  held off the middle (and so left as they were). */
        bool captureDone = false;
        int held = 0;
        /** A stick sitting off the middle (once, until Centre Sticks). */
        int drifted = -1;
        double driftOffset = 0.0;
    };
    Tick tick(qint64 now);
    /** True while tick() has anything to do. */
    bool needsTick() const;

    /** Where @p channel rests (-1..1), and whether it was learnt (else 0). */
    double rest(int channel) const { return m_axes.value(channel).rest; }
    bool isLearnt(int channel) const { return m_axes.value(channel).learnt; }
    /** Set a rest (one remembered from before). */
    void setRest(int channel, double rest);
    /** Every learnt rest, by channel (to remember). */
    QMap<int, double> rests() const;

    /** The last reading of @p channel (-1: none yet), and its output. */
    int raw(int channel) const { return m_axes.value(channel).raw; }
    uchar output(int channel) const;

    /** A reading 0..255 as -1..1, 128 the middle -- and back. */
    static double toNorm(double value);
    static uchar fromNorm(double norm);
    /** @p norm with the rest @p rest taken out, both ends still at +-1. */
    static double correct(double norm, double rest);

private:
    struct Axis
    {
        int raw = -1;
        double rest = 0.0;
        bool learnt = false;
        // The reading's range since it last moved, and since when.
        int lo = -1, hi = -1;
        qint64 still = 0;
        bool captured = false;    ///< by the capture under way
    };
    bool steady(const Axis &a, qint64 now) const;

    QMap<int, Axis> m_axes;
    double m_deadzone = 0.05;
    qint64 m_driftMs = 3000;
    qint64 m_autoUntil = -1;      ///< learning by itself until (-1: not)
    qint64 m_captureUntil = -1;   ///< Centre Sticks until (-1: not)
    qint64 m_lastOther = 0;       ///< anything not a stick last changed
    bool m_driftSaid = false;
};

#endif
