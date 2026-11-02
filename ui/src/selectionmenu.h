/*
  Q Light Controller Plus
  selectionmenu.h

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

#ifndef SELECTIONMENU_H
#define SELECTIONMENU_H

#include <QCoreApplication>
#include <QList>

#include "qlcui_export.h"

class QAction;
class QMenu;
class Workbench;
class Doc;

/** @addtogroup ui
 * @{
 */

/**
 * The "Selection ▸" submenu (Q201): one entry on every context menu where
 * fixtures are right-clicked -- the Lighting Studio, the 2D plot, the
 * Fixture Manager, the Workbench's targets, Build's Fixtures & Groups --
 * so what is picked can become a layer wherever it was picked:
 *
 * - **New Layer from Selection**: a new scene layer on them in the current
 *   Workbench (a Show Tree cue: a layer of the cue targeting them),
 *   selected and ready to edit. One undo step.
 * - **Add to Current Layer**: them added to the Workbench's selected layer.
 * - then the menu's own selection-shaped items (Create Group from
 *   Selection…), handed in by the caller.
 *
 * A disabled item says why in its tooltip. Built in one place so every
 * menu reads the same.
 */
class QLC_UI_EXPORT SelectionMenu
{
    Q_DECLARE_TR_FUNCTIONS(SelectionMenu)

public:
    /** The Workbench the actions go to: the Show Tree's docked one while it
     *  shows, else the Build tab's (null: none). */
    static Workbench *currentWorkbench();

    /** Add "Selection ▸" to @a menu for @a fixtures (empty: the programmer
     *  selection), with @a extra (the menu's own selection items, moved in
     *  here) after a separator. @a bench: where the layer actions go (null:
     *  currentWorkbench()). Returns the submenu. */
    static QMenu *addTo(QMenu *menu, Doc *doc, const QList<quint32> &fixtures,
                        const QList<QAction *> &extra = QList<QAction *>(),
                        Workbench *bench = nullptr);

    /** The submenu's title, and its actions' (tests, docs). */
    static QString title();
    static QString newLayerText();
    static QString addToLayerText();
};

/** @} */

#endif
