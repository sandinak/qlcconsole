/*
  Q Light Controller Plus
  windowplacement.h

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

#ifndef WINDOWPLACEMENT_H
#define WINDOWPLACEMENT_H

#include <QRect>

#include "qlcui_export.h"

class QWidget;

/** A dialog opens where it can be seen and used, and stays where it is put.
 *
 *  A dialog that puts back where it was last time (saved geometry) opened on
 *  the screen it was last on, not where the main window is now, and one
 *  taller than a laptop ran off its bottom with OK and Cancel out of reach
 *  (windowsanity_test found both). When a parented Qt::Dialog is shown (not
 *  by the window system: an un-minimise is left be):
 *   - one never placed (QDialog centres it over its main window) is shrunk
 *     to that screen's room where its minimum allows, and centred there;
 *   - one placed (a saved place, a move, the person's drag onto another
 *     screen) stays where it is while it is wholly on any screen; on none,
 *     or across an edge, it is pulled onto the screen under its middle (else
 *     its main window's) and shrunk to fit.
 *  A saved place on another screen than the main window's is the person's
 *  own choice and is kept. Tool windows, popups and windows with no parent
 *  are left alone. */
namespace WindowPlacement
{
    /** Watches the app for such a dialog being shown (once; later calls do
     *  nothing). The App does this at start. */
    QLC_UI_EXPORT void install();

    /** Places @p w as above.
     *  Returns whether anything was changed. */
    QLC_UI_EXPORT bool keepOnParentScreen(QWidget *w);

    /** @p r moved (not resized) to lie inside @p room as far as it can:
     *  its top-left wins when it is larger. */
    QLC_UI_EXPORT QRect clampInto(QRect r, const QRect &room);
}

#endif
