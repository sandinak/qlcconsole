/*
  Q Light Controller Plus
  windowspace.h

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

#ifndef WINDOWSPACE_H
#define WINDOWSPACE_H

#include "qlcui_export.h"

class QWidget;

/** A dialog stays on the desktop (macOS Space) of the window it belongs to.
 *
 *  Qt's Cocoa plugin makes every Qt::Dialog an NSPanel with
 *  NSWindowCollectionBehaviorMoveToActiveSpace ("popups show on the parent's
 *  desktop"). On a Cmd-Tab back from an app on another Space that pulls the
 *  key dialog -- Fixture Properties, Show Review, a venue prompt -- onto the
 *  Space you came from, away from the main window. Cleared, macOS switches
 *  to the Space the dialog and its main window are on, as for any window.
 *  Elsewhere (and off screen) nothing is done. */
namespace WindowSpace
{
    /** Whether @p w is a window this keeps with its parent: a top-level
     *  Qt::Dialog that has a parent widget. Popups, menus, tool windows and
     *  a dialog with no parent are left as Qt makes them. */
    QLC_UI_EXPORT bool staysWithParent(const QWidget *w);

    /** Watches the app for such a window being shown (once; later calls do
     *  nothing). The App does this at start. */
    QLC_UI_EXPORT void install();
    QLC_UI_EXPORT bool isInstalled();

    /** How many shows of such a window it has kept with their parent (for
     *  the tests: counted on every platform, native or not). */
    QLC_UI_EXPORT int keptCount();

    /** The native part: @p w's NSWindow no longer moves to the active
     *  Space. False when there is none (not macOS, or not Cocoa). */
    QLC_UI_EXPORT bool keepOnParentSpace(QWidget *w);
}

#endif
