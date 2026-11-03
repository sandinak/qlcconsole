/*
  Q Light Controller Plus - qlcconsole
  webaccessgamepad.h

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

#ifndef WEBACCESSGAMEPAD_H
#define WEBACCESSGAMEPAD_H

#include <QList>
#include <QString>

#include "gamepadrole.h"

/**
 * A game controller on the web pages (docs/features/gamepad-remote.md):
 * the browser's Gamepad API, read on the phone, tablet or Steam Deck, and
 * turned into the SAME gamepad roles the console's own controller uses
 * (GamepadRole), so a page sends "a", "rb", "lt"... and the console does
 * what its controller scheme says those do (ControllerScheme::verbFor()).
 *
 * The tables here are the one source: the pages' script is generated from
 * them (script()), and the tests read them, so a page and the console can't
 * drift apart.
 *
 * - The W3C "standard" gamepad layout, by button index (an Xbox, DualSense
 *   or a Steam Deck's pad all arrive as it in Chrome and Firefox).
 * - Keys, for what only arrives as a key: a Steam Deck's back grips
 *   (L4 / R4 / L5 / R5) reach a browser only through a Steam Input layout,
 *   which binds them to keys. The keys are the console's own Focus mode
 *   keys (FocusModePanel): Page Up / Page Down the previous / next focus
 *   call, [ / ] the previous / next fixture.
 */
namespace WebAccessGamepad
{
    /** The role of each button of the standard layout, by its index
     *  (0 A, 1 B, 2 X, 3 Y, 4 LB, 5 RB, 6 LT, 7 RT, 8 View, 9 Menu, 10 LS,
     *  11 RS, 12-15 the D-pad up, down, left, right, 16 Guide). */
    QList<GamepadRole::Role> standardButtons();
    /** The roles of the standard layout's axes (LX, LY, RX, RY). */
    QList<GamepadRole::Role> standardAxes();

    /** A key the pages read as a control: the browser's KeyboardEvent.key
     *  and the role it stands for. */
    struct Key
    {
        const char *key;
        GamepadRole::Role role;
    };
    QList<Key> keys();

    /** A button counts as down past this much of its travel (the triggers,
     *  which are axes on some pads), and up again under kReleaseAt. */
    constexpr double kPressAt = 0.5;
    constexpr double kReleaseAt = 0.3;
    /** The sticks' dead zone on the normalised reading (0..1, 0.5 centred),
     *  as FocusSession::stickVelocity() takes it: 12% of a half travel, a
     *  little wider than a desk pad's, since a handheld's sticks drift. */
    constexpr double kDeadzone = 0.06;
    /** A page sends a stick's moves at most this often (ms): 40 a second. */
    constexpr int kSendMs = 25;

    /** The pages' shared script: the tables, the reading loop and the
     *  controller test panel (window.QPad). Self-contained, no HTML. */
    QString script();
    /** The controller test panel's markup and style, put in a page's body. */
    QString testPanelHtml();
}

#endif
