/*
  Q Light Controller Plus
  undogesture.h

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

#ifndef UNDOGESTURE_H
#define UNDOGESTURE_H

#include <QString>

#include "qlcui_export.h"

/** @addtogroup ui
 * @{
 */

/** Where one undo step of a run of value edits begins and ends (a colour
 *  dragged round the wheel, a fader pulled): ONE press of the mouse. Each
 *  press starts a new gesture, so a drag is one step however long it
 *  pauses while the button is down, and two clicks a moment apart are two
 *  steps. Edits with no press between them (typing a number, arrow keys, a
 *  controller's encoder) merge by time, as PaletteOps does (1.5 s). One
 *  application-wide event filter, installed by the first editor made. */
namespace UndoGesture
{
    /** Start watching the mouse (an editor that makes value edits calls it
     *  as it is made, before its first press). */
    QLC_UI_EXPORT void watch();
    /** @a base with the gesture it belongs to: the merge key of an edit. */
    QLC_UI_EXPORT QString key(const QString &base);
    /** A mouse button is down (the gesture is a drag still going on). */
    QLC_UI_EXPORT bool held();
}

/** @} */

#endif
