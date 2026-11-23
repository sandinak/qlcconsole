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

#include "qlcui_export.h"

class QWidget;

/** A dialog opens on its main window's screen, and on it.
 *
 *  A dialog that puts back where it was last time (saved geometry) opened on
 *  the screen it was last on, not where the main window is now, and one
 *  taller than a laptop ran off its bottom with OK and Cancel out of reach
 *  (windowsanity_test found both). When a parented Qt::Dialog is shown, this
 *  moves it onto its main window's screen if it is elsewhere (centred over
 *  the main window), shrinks it to the screen's room where its minimum
 *  allows, and keeps it inside that room. A dialog already wholly on the
 *  right screen is left where it is. Tool windows, popups and windows with
 *  no parent are left alone. */
namespace WindowPlacement
{
    /** Watches the app for such a dialog being shown (once; later calls do
     *  nothing). The App does this at start. */
    QLC_UI_EXPORT void install();

    /** Puts @p w on its parent window's screen, fitting it, as above.
     *  Returns whether anything was changed. */
    QLC_UI_EXPORT bool keepOnParentScreen(QWidget *w);
}

#endif
