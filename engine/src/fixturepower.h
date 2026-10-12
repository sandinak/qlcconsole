/*
  Q Light Controller Plus
  fixturepower.h

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

#ifndef FIXTUREPOWER_H
#define FIXTUREPOWER_H

#include <functional>
#include <QVector>
#include <QMap>

class QXmlStreamReader;
class QXmlStreamWriter;
class Fixture;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLFixturePower QStringLiteral("Power")

/**
 * What a fixture draws, as measured on a meter, and how much it may
 * ("some fixtures are wattage hogs .. we need a way to set maximum brightness
 * per fixture .. to prevent overload").
 *
 * Every reading is the whole fixture's current with only that on, standby
 * included: each colour alone at full, everything at half (which says how
 * the draw falls with level -- LED drivers are far from linear: a bar
 * drawing 6 A at full drew 1.7 A at half), everything off, and optionally
 * everything at full. The cap is in amps; 0 is no cap.
 */
struct PowerProfile
{
    /** Each colour alone at full (QLCChannel::PrimaryColour -> amps).
     *  NoColour is a plain dimmer's lamp. */
    QMap<int, double> colourAmps;
    double fullAmps = 0.0;          ///< everything at full (optional)
    double halfAmps = 0.0;          ///< everything at half
    double standbyAmps = 0.0;       ///< everything off
    double maxAmps = 0.0;           ///< the cap; 0: none

    /** Nothing measured: nothing to model or cap with. */
    bool isMeasured() const { return !colourAmps.isEmpty() || fullAmps > 0.0; }

    bool operator==(const PowerProfile &o) const
    {
        return colourAmps == o.colourAmps && fullAmps == o.fullAmps && halfAmps == o.halfAmps
               && standbyAmps == o.standbyAmps && maxAmps == o.maxAmps;
    }
    bool operator!=(const PowerProfile &o) const { return !(*this == o); }

    bool loadXML(QXmlStreamReader &root);
    void saveXML(QXmlStreamWriter *doc) const;
};

/**
 * A fixture's draw worked out from what it sends: each emitting channel
 * (a colour, or a plain dimmer's lamp) draws its colour's share of the
 * measured current, split among that colour's pixels, at its level times
 * the dimmers over it, bent by the curve the half reading gives.
 *
 *   amps = standby + sum over emitters of share * (level * dimmers)^gamma
 *
 * Scaling every emitter by k scales the sum by k^gamma, so the factor that
 * brings a fixture down to its cap is exact: k = ((max - s) / (amps - s))^(1/gamma).
 */
struct PowerModel
{
    struct Emitter
    {
        int offset;                 ///< channel within the fixture
        double share;               ///< amps at full, standby taken out
        QVector<int> dimmers;       ///< channels whose level multiplies it
    };
    QVector<Emitter> emitters;
    double gamma = 1.0;
    double standby = 0.0;
    double maxAmps = 0.0;

    bool isValid() const { return !emitters.isEmpty(); }

    /** The model for @p fx and @p profile (invalid when nothing to go on). */
    static PowerModel build(const Fixture *fx, const PowerProfile &profile);

    /** Amps at the values @p value(offset) gives (0..255). */
    double amps(const std::function<uchar(int offset)> &value) const;

    /** What to scale the emitters by to stay within the cap at @p amps:
     *  1 when it is within it (or there is no cap). */
    double limitFactor(double amps) const;
};

/** @} */

#endif
