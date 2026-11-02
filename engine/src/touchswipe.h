/*
  Q Light Controller Plus
  touchswipe.h

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

#ifndef TOUCHSWIPE_H
#define TOUCHSWIPE_H

#include "qlcengine_export.h"
#include "gamepadrole.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * A PlayStation pad's touchpad read as four swipes (board Q199, build B):
 * one finger carried across the pad. Used for stepping, never for a GO.
 *
 * One swipe is one step: it is given out the moment the finger has gone
 * threshold() of the pad along one axis (and clearly more than along the
 * other), and the touch is then spent until the finger lifts. A second
 * finger, a click, or a pause shorter than cooldown() after the last swipe
 * give nothing. Readings arrive in any order within a frame (as
 * TouchpadGesture), so they are only kept (setValue()) and read on the
 * controller tick (take()).
 */
class QLC_ENGINE_EXPORT TouchSwipe
{
public:
    enum Swipe { None, Left, Right, Up, Down };

    /** How far, of the pad, a finger goes to swipe. */
    static constexpr float threshold() { return 0.25f; }
    /** The other axis may be at most this share of the swipe's. */
    static constexpr float straightness() { return 0.6f; }
    /** The least time between two swipes, seconds. */
    static constexpr double cooldown() { return 0.3; }

    /** A reading of a GamepadRole::isTouch() control; anything else is ignored. */
    void setValue(GamepadRole::Role role, float norm);
    void setClick(bool down);

    /** The swipe made since the last call, @p dt seconds on. */
    Swipe take(double dt);

    /** Fingers up, nothing pending. */
    void reset();

private:
    float m_x = 0.5f, m_y = 0.5f;
    bool m_down = false, m_down2 = false, m_click = false;
    float m_fromX = 0.5f, m_fromY = 0.5f;
    bool m_tracking = false;    ///< a single finger that can still swipe
    bool m_spent = false;       ///< this touch has swiped (or was spoilt)
    double m_wait = 0.0;        ///< cooldown left
};

/** @} */

#endif
