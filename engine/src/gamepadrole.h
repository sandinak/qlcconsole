/*
  Q Light Controller Plus
  gamepadrole.h

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

#ifndef GAMEPADROLE_H
#define GAMEPADROLE_H

#include <QString>

#include "qlcengine_export.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * The standard controls of a gamepad, by what they ARE rather than by the
 * index a particular controller reports them on. A tool that uses a gamepad as
 * a gamepad (Focus mode) asks for "the right bumper", and any controller whose
 * input profile says which of its channels that is works the same way.
 */
namespace GamepadRole
{
    enum Role
    {
        None = 0,
        // Buttons
        A, B, X, Y,
        LB, RB,
        Back,       ///< "View" on an Xbox One / Series pad
        Start,      ///< "Menu" on an Xbox One / Series pad
        LS, RS,     ///< stick clicks
        Guide,
        DpadUp, DpadDown, DpadLeft, DpadRight,
        // Axes
        LeftX, LeftY, RightX, RightY,
        LeftTrigger, RightTrigger
    };

    /** The role of a control, from what its input profile says: the
     *  explicit @p role key ("a", "lb", "start", "dpad_up", "rx"... as
     *  QLCIOPlugin::inputChannelRole() returns them) when there is one, else
     *  the control's @p name read the usual ways ("A", "LB", "Left Bumper",
     *  "View", "Menu", "D-pad Up", "Right Stick X"...), so a profile written
     *  before roles existed still works. None when neither says. */
    QLC_ENGINE_EXPORT Role fromProfile(const QString &role, const QString &name);

    /** The role key for @p role, as a profile writes it ("a", "dpad_up"...);
     *  empty for None. */
    QLC_ENGINE_EXPORT QString key(Role role);

    /** True for the stick and trigger roles. */
    QLC_ENGINE_EXPORT bool isAxis(Role role);
}

/** @} */

#endif
