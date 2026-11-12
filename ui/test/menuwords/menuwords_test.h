/*
  Q Light Controller Plus
  menuwords_test.h

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

#ifndef MENUWORDS_TEST_H
#define MENUWORDS_TEST_H

#include <QObject>

/* The canon's words (docs/design/CONTEXT_MENUS.md §3.2) for EVERY menu item
   in the UI, read from the sources: each label given to addAction(),
   addMenu() or ContextMenu::add() in ui/src is Title Case, ends in "…"
   (never "..."), and has no shortcut typed into it. contextmenus_test checks
   the menus built by the shared builders at run time; this one catches the
   rest -- a toolbar's drop-down, a submenu, a menu a new view adds -- so a
   new label in sentence case fails the gate wherever it is.

   It also checks that no label or message shows a length in metres only
   (Settings > Units: a show in feet reads feet), and that the UI never
   names the Build tab by its old name. */
class MenuWords_Test final : public QObject
{
    Q_OBJECT

private slots:
    void theReaderFindsLabels();
    void everyMenuLabelIsTitleCase();
    void everyButtonLabelIsTitleCase();
    void noLengthIsMetresOnly();
    void theTabsHaveTheirNames();
    void noTextSendsYouToAHiddenBuild();
};

#endif
