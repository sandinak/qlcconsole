/*
  Q Light Controller Plus
  touchpadgesture.h

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

#ifndef TOUCHPADGESTURE_H
#define TOUCHPADGESTURE_H

#include "qlcengine_export.h"
#include "gamepadrole.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * A PlayStation pad's touchpad read as a laptop's trackpad (board Q170,
 * option A): one finger dragging is a fine aim nudge -- by how far the
 * finger moved, not how long it was held -- and two fingers together zoom
 * (up and down) and focus (left and right).
 *
 * The readings arrive one channel at a time and, within a frame, in any
 * order (InputPatch hands a frame's values over as a set), so they are only
 * kept as they come (setValue()) and read once a controller tick (take()):
 * by then a finger's place and its touch belong together.
 *
 * Nothing moves:
 *  - when a finger lands, lifts, or a second one joins or leaves -- the
 *    tick starts again from where the fingers are (no jump, and one finger's
 *    move is never part of a two-finger drag);
 *  - until a new touch has gone slop() from where it landed;
 *  - for a resting finger's one-step jitter (moves are held back until they
 *    add up to jitter());
 *  - while the pad is clicked (a firm press moves the finger a little).
 */
class QLC_ENGINE_EXPORT TouchpadGesture
{
public:
    enum Kind { NoMotion, Drag, TwoFingerDrag };

    struct Motion
    {
        Kind kind = NoMotion;
        /** How far, as a fraction of the pad: right and down are positive. */
        float dx = 0.0f;
        float dy = 0.0f;
        /** The first move of this touch (it has just gone past slop()). */
        bool started = false;
    };

    /** How far a new touch goes before it moves anything (3 of the pad's
     *  256 steps). */
    static constexpr float slop() { return 3.0f / 255.0f; }
    /** Moves smaller than this are held back until they add up. */
    static constexpr float jitter() { return 1.5f / 255.0f; }

    /** A reading of a GamepadRole::isTouch() control: a place 0..1, or a
     *  touch (> 0.5 is down). Anything else is ignored. */
    void setValue(GamepadRole::Role role, float norm);
    /** The touchpad's click. */
    void setClick(bool down);

    /** What the fingers did since the last call. */
    Motion take();

    /** Fingers up, nothing pending (the tool stopped, the pad went). */
    void reset();

    /** Fingers down now. */
    int fingers() const;

private:
    struct Finger
    {
        float x = 0.5f;
        float y = 0.5f;
        bool down = false;
    };
    void restart();

    Finger m_now[2];
    Finger m_last[2];
    bool m_click = false;
    bool m_clicked = false;         ///< pressed since the last take()
    bool m_moving = false;          ///< this touch has gone past slop()
    float m_heldX = 0.0f;           ///< moved but not yet given out
    float m_heldY = 0.0f;
};

/** @} */

#endif
