/*
  Q Light Controller Plus
  padnames.h

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

#ifndef PADNAMES_H
#define PADNAMES_H

#include <QString>
#include <QMap>
#include <QColor>

#include "qlcengine_export.h"
#include "gamepadrole.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * What a gamepad's controls are CALLED, by family (board Q170): the tools
 * work by role (GamepadRole), and every line that names a control -- on
 * screen, in the voice, in a warning -- is written with tokens ("{A} keeps
 * it, {B} puts it back.") and put in the words of the pad in hand: "A" and
 * "B" on an Xbox pad, "Cross" and "Circle" on a PlayStation one.
 *
 * Tokens are the role's Xbox name in braces: {A} {B} {X} {Y} {LB} {RB} {LT}
 * {RT} {View} {Menu} {LS} {RS} {Guide} {Touchpad}. Anything else in braces
 * is left as it is.
 */
namespace PadNames
{
    /** Xbox is also every other pad (the profile's names come first on the
     *  map anyway). */
    enum Family { Xbox, DualShock4, DualSense };

    /** Long: in a sentence ("Cross", "L1", "Options", "PS button").
     *  Glyph: what fits on a drawn button ("✕", "L1", "A"); empty for a
     *  control with nothing written on it (View, Menu, the guide button). */
    enum Form { Long, Glyph };

    QLC_ENGINE_EXPORT bool isPlayStation(Family family);

    /** @p role's name on a pad of @p family; empty for None. */
    QLC_ENGINE_EXPORT QString name(GamepadRole::Role role, Family family, Form form = Long);

    /** @p text with every token in @p family's words. */
    QLC_ENGINE_EXPORT QString expand(const QString &text, Family family, Form form = Long);

    /** "xbox", "dualshock4", "dualsense" (QLCIOPlugin::inputDeviceKind());
     *  anything else is Xbox. */
    QLC_ENGINE_EXPORT Family familyFrom(const QString &kindKey);
    QLC_ENGINE_EXPORT QString kindKey(Family family);

    /** For a plugin that does not say (inputDeviceKind() empty): a pad
     *  whose @p profile says PlayStation, or whose @p controls (role ->
     *  the profile's name) call A "Cross", is a PlayStation pad -- a
     *  DualSense when @p device says so, else a DualShock 4. Else Xbox. */
    QLC_ENGINE_EXPORT Family guess(const QString &profile, const QString &device,
                                   const QMap<int, QString> &controls);

    /** A face button's own colour: the Xbox pad's (A green, B red, X blue,
     *  Y yellow) or the PlayStation's (Cross blue, Circle red, Square pink,
     *  Triangle green); invalid for any other control. */
    QLC_ENGINE_EXPORT QColor faceColour(GamepadRole::Role role, Family family);
}

/** @} */

#endif
