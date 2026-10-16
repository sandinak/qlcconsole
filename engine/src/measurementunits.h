/*
  Q Light Controller Plus
  measurementunits.h

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

#ifndef MEASUREMENTUNITS_H
#define MEASUREMENTUNITS_H

#include <QCoreApplication>
#include <QString>

/** @addtogroup engine Engine
 * @{
 */

/**
 * The unit a person works in, and turning what they type into the units a
 * file keeps (millimetres, kilograms) and back.
 *
 * One app-wide preference (Settings -> Units), shared by qlcconsole and the
 * fixture definition editor. Lengths show in the chosen unit; weight follows
 * from it -- kilograms with mm/cm/m, pounds with in/ft.
 *
 * Typed values may carry their own unit and are converted: 7.5", 7.5 in,
 * 0.625', 6' 3", 6 ft 3 in, 190.5mm, 19.05 cm, 0.19 m; 12 kg, 26.5 lb(s),
 * 400 g, 5 lb 4 oz. A bare number is in the preferred unit. Nothing here
 * changes what is stored: callers keep mm / kg as the file format does.
 */
class MeasurementUnits final
{
    Q_DECLARE_TR_FUNCTIONS(MeasurementUnits)

public:
    enum LengthUnit { Millimetres = 0, Centimetres, Metres, Inches, Feet };
    enum WeightUnit { Kilograms = 0, Pounds };

    /** The preferred length unit (Millimetres when never set). Read from
     *  qlcconsole's own settings so the fixture editor follows it too. */
    static LengthUnit preferredLength();
    static void setPreferredLength(LengthUnit unit);
    /** True once a unit has been chosen (or seeded) */
    static bool isPreferenceSet();
    /** The first show opened decides the unit, once: when no unit has been
     *  chosen yet and that show's grid was in feet (how the unit was picked
     *  before Settings -> Units), the preference becomes feet. Any later
     *  show changes nothing. True when it set the preference. */
    static bool seedFromFirstShow(bool showInFeet);

    /** Kilograms for the metric lengths, pounds for inches and feet */
    static WeightUnit weightUnitFor(LengthUnit unit);

    /** Short symbol / human name / settings key of a unit */
    static QString symbol(LengthUnit unit);
    static QString symbol(WeightUnit unit);
    static QString name(LengthUnit unit);
    static QString key(LengthUnit unit);
    static LengthUnit fromKey(const QString &key, LengthUnit fallback = Millimetres);

    /** Millimetres in one @a unit, kilograms in one @a unit */
    static double mmPer(LengthUnit unit);
    static double kgPer(WeightUnit unit);

    /** Parse a typed length into millimetres. A bare number is in
     *  @a bareUnit. Empty text is 0. Negative, malformed or unit-less
     *  mixtures set @a ok false and return 0. */
    static double parseLength(const QString &text, LengthUnit bareUnit, bool *ok);
    /** Parse a typed weight into kilograms (see parseLength) */
    static double parseWeight(const QString &text, WeightUnit bareUnit, bool *ok);

    /** @a mm shown in @a unit, e.g. "190.5 mm", "7.5 in", "1' 3.5\"".
     *  A whole number of millimetres -- what a file holds -- is shown with
     *  the fewest decimals that still mean that same millimetre (191 mm ->
     *  7.5 in, not 7.52); anything else is rounded to the unit's precision
     *  (mm 0.1, cm 0.01, m 0.0001, in 0.01). Trailing zeros are dropped. */
    static QString formatLength(double mm, LengthUnit unit);
    /** @a kg shown in @a unit: kg to 0.001, lb to 0.01, no trailing zeros */
    static QString formatWeight(double kg, WeightUnit unit);

    /** The whole millimetres a file keeps for @a mm: halves round up, so
     *  7.5" (190.5 mm) is saved as 191 -- and 191 shows as 7.5 in again. */
    static int toStoredMm(double mm);

    /** What the fields accept, for a tooltip */
    static QString lengthHelp(LengthUnit unit);
    static QString weightHelp(WeightUnit unit);

    /** The settings key the preference lives under */
    static QString settingsKey();
};

/** @} */

#endif
