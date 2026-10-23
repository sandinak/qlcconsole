/*
  Q Light Controller Plus
  controllerscheme.h

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

#ifndef CONTROLLERSCHEME_H
#define CONTROLLERSCHEME_H

#include <QString>

#include "qlcengine_export.h"
#include "gamepadrole.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * The one controller layout every on-stage tool uses (FOCUS_REFINE_DESIGN.md
 * section 2): Focus mode, Focus calls and Refine. A control does the same
 * KIND of thing in each -- the triggers step the big unit (a focus call, a
 * layer), the bumpers the small one (a fixture), Y changes the scope, A keeps,
 * B reverts -- and only the words for it change. Learn it once.
 *
 * verbFor() is the whole map, by gamepad role; label() is what the panels
 * and the help say a verb does in each tool.
 */
namespace ControllerScheme
{
    /** The tool a label is for. */
    enum Tool { FocusTool, FocusCallsTool, RefineTool };

    /** What a control does, the same in every tool. */
    enum Verb
    {
        NoVerb = 0,
        PreviousGroup, NextGroup,     ///< LT / RT: a focus call, a layer
        PreviousItem, NextItem,       ///< LB / RB: a fixture (or a group)
        CycleScope,                   ///< Y: group <-> fixture, layer -> group -> fixture
        Keep,                         ///< A: save the correction, keep the edit
        Revert,                       ///< B: back to what was kept
        Reset,                        ///< X: zero the correction, drop the exception
        Undo,                         ///< View/Back: undo the last keep
        Exit,                         ///< Menu/Start
        ZoomWider, ZoomNarrower,      ///< D-pad up / down
        FocusNearer, FocusFarther,    ///< D-pad left / right
        Alternate,                    ///< LS click: second reference / D-pad page
        WhereAmI                      ///< RS click: say and flash where you are
    };

    /** What @p role does (NoVerb for the sticks and anything unused). The
     *  triggers are verbs too: see TriggerButton for reading them. */
    QLC_ENGINE_EXPORT Verb verbFor(GamepadRole::Role role);

    /** The role that does @p verb (None for NoVerb). */
    QLC_ENGINE_EXPORT GamepadRole::Role roleFor(Verb verb);

    /** What @p verb is called in @p tool, for the on-screen map ("Next focus
     *  call", "Next layer"...); empty when the tool does not use it. */
    QLC_ENGINE_EXPORT QString label(Tool tool, Verb verb);

    /** What a stick does in @p tool ("Nudge aim", "Move"...). */
    QLC_ENGINE_EXPORT QString stickLabel(Tool tool, bool fine);

    /** Every verb in map order, for a table. */
    QLC_ENGINE_EXPORT QList<Verb> verbs();

    /**
     * A trigger read as a button: an axis 0 (released) .. 1, pressed past
     * pressAt() and released again below releaseAt(), so a trigger resting
     * a little in, or a noisy one, does not repeat.
     */
    class QLC_ENGINE_EXPORT TriggerButton
    {
    public:
        static constexpr float pressAt() { return 0.6f; }
        static constexpr float releaseAt() { return 0.35f; }

        /** A new reading; true on the press only. */
        bool update(float norm);
        bool isDown() const { return m_down; }
        void reset() { m_down = false; }

    private:
        bool m_down = false;
    };
}

/** @} */

#endif
