/*
  Q Light Controller Plus
  colourmix.h

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

#ifndef COLOURMIX_H
#define COLOURMIX_H

#include <QVector>
#include <QtGlobal>
#include "qlcengine_export.h"

class QLCFixtureMode;

/** @addtogroup engine Engine
 * @{
 */

/**
 * How a head with no red, green and blue emitters still shows an RGB
 * colour: by CMY mixing (cyan = 255 - red, and so on), or by the colour
 * wheel slot nearest it. Read off the fixture's MODE, once: the slot
 * table is what an effect looks a colour up in every frame.
 */
class QLC_ENGINE_EXPORT ColourMix
{
public:
    /** A wheel slot: the colour its capability names, and the DMX of its
     *  middle. */
    struct Slot
    {
        int r = 0, g = 0, b = 0;
        uchar dmx = 0;
    };
    /** A Colour-group channel and the slots on it that name a colour (a
     *  capability resource colour, else a name QColor reads). */
    struct Wheel
    {
        quint32 channel = 0;
        QVector<Slot> slots;
    };

    /** Every Colour-group channel of @p mode with at least one coloured
     *  slot, in channel order. */
    static QVector<Wheel> wheels(const QLCFixtureMode *mode);

    /** The slot of @p wheel nearest (@p r, @p g, @p b) by RGB distance (the
     *  first of equals); -1 when it has none. */
    static int nearest(const Wheel &wheel, int r, int g, int b);

    /** It has cyan, magenta and yellow mixing channels (any group). */
    static bool hasCMY(const QLCFixtureMode *mode);

    /** An RGB colour can be shown on it without RGB emitters: it mixes
     *  CMY, or has a colour wheel of two or more coloured slots. */
    static bool showsRgbWithoutRgb(const QLCFixtureMode *mode);
};

/** @} */

#endif
