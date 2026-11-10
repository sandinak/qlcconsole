/*
  Q Light Controller Plus
  followspotframing.h

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

#ifndef FOLLOWSPOTFRAMING_H
#define FOLLOWSPOTFRAMING_H

#include <QString>

/** @addtogroup engine Engine
 * @{
 */

/**
 * How big a follow spot's beam is on its subject: the one formula the
 * effect (each head, EffectInstance -> followspot.js's sceneTarget.beamWidth),
 * the pad's size step and the footer all use.
 *
 * - Framing (Head & Shoulders, Waist Up, Full Body): a share of the
 *   person's height from just over the head down, as the beam lands on an
 *   upright body. A head that comes in steeply stretches its beam down the
 *   body by 1 / level, so it needs less width for the same picture -- never
 *   under the shoulders' width. Times the Framing Size (the look's %) and
 *   the pad's scale for the run.
 * - Custom: Width on Subject (the look's "sizeOnSubject", centimetres), or
 *   the pad's width for the run.
 * - Neither: no width; the Beam Angle (the look's, or the pad's) if any.
 *
 * Pure arithmetic, no allocation: called per head per effect tick.
 */
namespace FollowSpotFraming
{
    enum Framing { Custom = 0, HeadShoulders, WaistUp, FullBody };
    /** What the pad's Left / Right step: the Framing Size, the Width on
     *  Subject, or the beam angle. */
    enum Mode { ScaleMode, WidthMode, BeamMode };

    /** The look's settings (its effect parameters). */
    struct Look
    {
        int framing = Custom;
        double scale = 1.0;     ///< Framing Size, 1 = 100%
        double widthCm = 0.0;   ///< Width on Subject (Custom); 0 = none
        double beamDeg = 0.0;   ///< Beam Angle; 0 = the scene's own zoom
    };

    /** The pad's change for this run (D-pad Left / Right); 0 / 1 = none. */
    struct Pad
    {
        double scale = 1.0;     ///< on a Framing
        double widthCm = 0.0;   ///< on a Custom width
        double beamDeg = 0.0;   ///< with no size

        bool isSet() const { return scale != 1.0 || widthCm > 0.0 || beamDeg > 0.0; }
    };

    /** The parameter names, as followspot.js declares them. */
    QString framingKey();
    QString scaleKey();
    QString widthKey();
    QString beamKey();

    /** A look from its parameter values (QMap or QHash of name -> value). */
    template <typename Map>
    Look fromParams(const Map &values)
    {
        Look l;
        l.framing = int(values.value(framingKey(), 0.0) + 0.5);
        if (l.framing < Custom || l.framing > FullBody)
            l.framing = Custom;
        const double pct = values.value(scaleKey(), 100.0);
        l.scale = pct > 0.0 ? pct / 100.0 : 1.0;
        l.widthCm = values.value(widthKey(), 0.0);
        l.beamDeg = values.value(beamKey(), 0.0);
        return l;
    }

    /** It sizes the beam on the subject (a Framing, or a Custom width): its
     *  aim then keeps the top of the beam just over the head at any size
     *  (MonitorProperties::targetAimPointFor(), framed). */
    inline bool sizesBeam(const Look &l) { return l.framing != Custom || l.widthCm > 0.0; }

    /** The share of the height a Framing covers, level; 0 for Custom. */
    double span(int framing);
    /** The least width, as a share of the height (the shoulders). */
    constexpr double kLeast = 0.30;
    /** The level a beam is read at: no flatter than straight on, and no
     *  steeper than 0.25 (the aim rule's own limit). */
    double level(double level);

    /** The width wanted on the subject (metres) for a head whose throw
     *  comes in at @p level, on a person @p heightM tall; 0 = no size. */
    double widthM(const Look &l, const Pad &p, double heightM, double level);
    /** The beam angle (degrees) that is @p widthM wide at @p distM. */
    double angleFor(double widthM, double distM);
    /** The beam angle wanted with no width (degrees); 0 = none. */
    inline double beamDeg(const Look &l, const Pad &p) { return p.beamDeg > 0.0 ? p.beamDeg : l.beamDeg; }

    /** "Waist Up", "Head & Shoulders"... (the editor's names); empty for Custom. */
    QString framingName(int framing);
}

/** @} */

#endif
