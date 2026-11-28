/*
  Q Light Controller Plus
  cuetabs.h

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

#ifndef CUETABS_H
#define CUETABS_H

#include <QTabBar>
#include <QTest>

#include "showtreeview.h"

/* The cue page is tabbed (Edit | Tracking | Timing | Notes) wherever it
 * shows: a test that uses a field the way a person does clicks the tab that
 * holds it first, with the mouse. True: @a w's tab is the current one (or
 * the page has no tabs up, so every row is there). */
inline bool clickCueTabOf(ShowTreeView *view, const QWidget *w)
{
    if (view == nullptr || w == nullptr)
        return false;
    const int tab = view->cueTabOf(w);
    QTabBar *bar = view->findChild<QTabBar *>(QStringLiteral("stCueTabs"));
    if (tab < 0 || bar == nullptr || !view->cueTabsShown())
        return true;
    if (!bar->isTabVisible(tab))
        return false;
    if (bar->currentIndex() != tab && bar->isVisible())
        QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, bar->tabRect(tab).center());
    if (bar->currentIndex() != tab && !bar->isVisible())
        view->showCueTab(ShowTreeView::CueTab(tab));     // a page not on screen: no mouse to use
    return bar->currentIndex() == tab;
}

#endif
