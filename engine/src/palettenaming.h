/*
  Q Light Controller Plus
  palettenaming.h

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

#ifndef PALETTENAMING_H
#define PALETTENAMING_H

#include <QStringList>
#include <QColor>

#include "qlcengine_export.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * A palette's name that says its value, kept saying it when the value
 * changes ("if we go from Red to Dark Red .. we should auto update the
 * title"). Only the value's part of the name is rewritten: whatever the user
 * or the show's convention put round it stays ("Act2 Red" -> "Act2 Dark
 * Red", "02-01 Red Wash" -> "02-01 Dark Red Wash"). A name with no part
 * that says the old value is the user's own and is never touched. The
 * palette's folder is never part of this.
 *
 * Every follow*() returns the new name, or an empty string when the name
 * should stay as it is.
 */
namespace PaletteNaming
{
    /************************************************************************
     * Colour
     ************************************************************************/

    /** The named colour nearest @a rgb (perceptual distance, CIE Lab), from
     *  a table with light and dark variants: "Red", "Dark Red", "Light
     *  Red", "Warm White", "Black"... With RGB black, the extra emitters
     *  @a wauv (White=red, Amber=green, UV=blue) name it ("White", "Amber",
     *  "UV"). */
    QLC_ENGINE_EXPORT QString colourName(const QColor &rgb, const QColor &wauv = QColor());

    /** Every name colourName() can give, longest first. */
    QLC_ENGINE_EXPORT QStringList colourNames();

    /** The colour a table name stands for (invalid: not one). */
    QLC_ENGINE_EXPORT QColor namedColour(const QString &name);

    /** @a name with its colour part rewritten to @a newColour, the value
     *  having been @a oldRgb / @a oldWauv (or the wheel slot @a oldMacro).
     *  The colour part is a whole-word colour name (or @a oldMacro) in the
     *  name that says the old value: the old colour's own name, or one near
     *  it (a name given by an older, coarser table). Several: the one
     *  nearest the old colour. Empty: no such part, or nothing to change. */
    QLC_ENGINE_EXPORT QString followColour(const QString &name, const QColor &oldRgb,
                                           const QColor &oldWauv, const QString &oldMacro,
                                           const QString &newColour);

    /************************************************************************
     * Levels: intensity and strobe
     ************************************************************************/

    /** The intensity name a dimmer value (0-255) gets: "Off", "Full" or
     *  "Intensity 45%". */
    QLC_ENGINE_EXPORT QString dimmerName(int value);
    /** @a name with its level part following a dimmer change ("Intensity
     *  45%" -> "Intensity 60%", "Act2 Full" -> "Act2 60%"). The part must
     *  say the old value (a percentage within one of it, or Full / Off). */
    QLC_ENGINE_EXPORT QString followDimmer(const QString &name, int oldValue, int newValue);

    /** The strobe name a rate (0-100%) gets: "Strobe Off", "Strobe Max" or
     *  "Strobe 40%". */
    QLC_ENGINE_EXPORT QString strobeName(int percent);
    QLC_ENGINE_EXPORT QString followStrobe(const QString &name, int oldPercent, int newPercent);

    /************************************************************************
     * Named states: shutter, gobo
     ************************************************************************/

    /** @a name with the whole-word part @a oldPart (a capability's name:
     *  "Open", "Strobe slow") replaced by @a newPart. */
    QLC_ENGINE_EXPORT QString followPart(const QString &name, const QString &oldPart,
                                         const QString &newPart);

    /************************************************************************
     * Factory names and the show's convention
     ************************************************************************/

    /** "New Color", "New Color 2", "" -- a name nobody gave. */
    QLC_ENGINE_EXPORT bool isFactoryName(const QString &name, const QString &typeName);

    /** What a palette with no name of its own is called for the value part
     *  @a part, learnt from its siblings (the same type, the same folder):
     *  when at least two of them name their colour (or level) with the same
     *  text round it ("Act2 Red", "Act2 Blue"), @a part gets that text too
     *  ("Act2 Amber"). Otherwise empty: no convention. @a colour: siblings are
     *  read for a colour part, else for a level (percentage / Full / Off). */
    QLC_ENGINE_EXPORT QString withConvention(const QString &part, const QStringList &siblings,
                                             bool colour);
}

/** @} */

#endif
