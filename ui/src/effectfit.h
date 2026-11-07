/*
  Q Light Controller Plus
  effectfit.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef EFFECTFIT_H
#define EFFECTFIT_H

#include <QStringList>
#include <QList>

class Fixture;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Does this effect fit these fixtures? ("When searching for effects it should
 * know which fixtures are being worked with, so WLED effects don't show up
 * for non-WLED groups.") One answer for every place that offers effects.
 *
 * The rules, from what the fixtures can do:
 *  - a WLED effect (with a port or without) needs a WLED device;
 *  - a gobo effect needs a gobo wheel, a beam effect zoom, iris or focus;
 *  - a matrix effect needs a grid of heads: a pixel bar, or several fixtures
 *    together (and light to drive: colour or a dimmer);
 *  - any other script takes the fixture types it declares, the way the engine
 *    picks the fixtures it runs on: position needs pan/tilt, colour needs
 *    colour mixing, a dimmer-only fixture gets the intensity effects.
 * A mixed set of fixtures fits if it fits ANY of them (it runs on those);
 * runsOn/of say how many. No targets known yet: everything fits.
 */
class EffectFit
{
public:
    /** What one fixture can do. */
    struct Caps
    {
        bool wled = false;
        bool panTilt = false;
        bool gobo = false;
        bool beam = false;          ///< zoom, iris or focus
        bool rgb = false;
        bool colorWheel = false;
        bool dimmer = false;
        bool shutter = false;
        bool uv = false;
        bool freeRotate = false;    ///< a head turns endlessly on pan or tilt (FreeRotation)
        int heads = 1;
    };
    static Caps capsOf(const Fixture *fixture);

    /** The fixtures an effect is being worked on. */
    struct Targets
    {
        QList<Caps> fixtures;
        bool known() const { return !fixtures.isEmpty(); }
        bool anyWled() const;
        int totalHeads() const;
    };
    /** Distinct fixture ids -> targets (ids that are no fixture are skipped). */
    static Targets targetsOf(Doc *doc, const QList<quint32> &fixtureIds);

    /** What an effect needs. */
    struct Need
    {
        bool wled = false;
        bool gobo = false;
        bool beam = false;
        bool matrix = false;
        QStringList types;          ///< a script's declared fixtureTypes (lowercase)
    };
    /** A WLED device effect (listed under WLED). */
    static Need wledNeed();
    /** A script's need, from its declared types and the intents it emits
     *  (read from the file at @p scriptPath; empty: types only). A script that
     *  ports a WLED effect (@p wledPort, its effect.wled) is WLED's: it needs a
     *  WLED device like the device effects. */
    static Need scriptNeed(const QStringList &fixtureTypes, const QString &scriptPath,
                           const QString &wledPort = QString());

    struct Result
    {
        bool fits = true;
        int runsOn = 0;
        int of = 0;
        QString why;                ///< fits: "Runs on 6 of 10 fixtures" when partial; else why not
    };
    static Result fit(const Need &need, const Targets &targets);

private:
    static bool typesMatch(const QStringList &types, const Caps &c);
};

/** @} */

#endif
