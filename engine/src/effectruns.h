/*
  Q Light Controller Plus
  effectruns.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef EFFECTRUNS_H
#define EFFECTRUNS_H

#include <QMap>
#include <QString>

/** @addtogroup engine Engine
 * @{
 */

/**
 * How many times an effect palette's effect runs, and what it leaves at the
 * end: any effect, not only a script that declares itself a one-shot.
 *
 * Kept on the palette as reserved effect values, beside the one-shot length
 * that was already there:
 *   __runs       0 = Loop, 1 = Once, N = N times. Absent: the script's own
 *                (a one-shot script runs once, everything else loops).
 *   __atEnd      0 = Hold the last frame, 1 = Release (the look's own values
 *                come back). Absent: the script's own (onFinish).
 *   __durationMs the length of one run; 0 = auto (the cue / chase step).
 *
 * A finished run stops ticking the script, so a held or released effect costs
 * nothing afterwards.
 */
namespace EffectRuns
{
inline QString runsKey() { return QStringLiteral("__runs"); }
inline QString atEndKey() { return QStringLiteral("__atEnd"); }
inline QString lengthKey() { return QStringLiteral("__durationMs"); }

/** The most runs the controls offer. */
constexpr int kMaxRuns = 99;

struct Settings
{
    int runs = 0;           ///< 0 = Loop, else how many runs before it ends
    bool release = false;   ///< at the end: release (true) or hold (false)
    int lengthMs = 0;       ///< one run's set length; 0 = auto
    bool loops() const { return runs <= 0; }
};

/** @p params' run settings (a QMap or QHash of name to value), with the script's own for anything not set:
 *  @p oneShot (it runs once unless told otherwise) and @p scriptRelease
 *  (its onFinish is "release"). */
template <typename Map>
inline Settings resolve(const Map &params, bool oneShot, bool scriptRelease)
{
    Settings s;
    auto it = params.constFind(runsKey());
    s.runs = it != params.constEnd() ? qBound(0, int(it.value() + 0.5), kMaxRuns) : (oneShot ? 1 : 0);
    it = params.constFind(atEndKey());
    s.release = it != params.constEnd() ? it.value() >= 0.5 : scriptRelease;
    it = params.constFind(lengthKey());
    s.lengthMs = it != params.constEnd() ? qMax(0, int(it.value())) : 0;
    return s;
}
}

/** @} */

#endif
