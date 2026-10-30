/*
  Q Light Controller Plus
  effectrunsmenu.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef EFFECTRUNSMENU_H
#define EFFECTRUNSMENU_H

#include <QCoreApplication>
#include <QList>

#include "effectruns.h"

class QWidget;
class QMenu;
class Function;
class QLCPalette;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * How an effect runs (Loop | Once | N times, At the end Hold | Release, one
 * run's Length) as a menu, for where there is no room for the look editor's
 * row: the Show Tree effect row's ⋯, Build ▸ Effects' right-click. Writes the
 * same values the look editor does (EffectRuns), and the playing effect
 * takes them at once.
 *
 * A classic EFX or RGB Matrix has its own Run order: Loop is Loop, Once is
 * Single shot (it stops at the end; it has no Hold or N times).
 */
class EffectRunsMenu
{
    Q_DECLARE_TR_FUNCTIONS(EffectRunsMenu)

public:
    /** Effect palette @p p's run settings, with its script's own defaults. */
    static EffectRuns::Settings settings(const QLCPalette *p);

    /** Set run settings on @p palettes (effect palettes; others skipped):
     *  @p runs (0 Loop, 1 Once, N), and At the end and Length unless < 0.
     *  Marks the show changed and updates the playing effects. */
    static void apply(Doc *doc, const QList<quint32> &palettes, int runs,
                      int release = -1, int lengthMs = -1);

    /** A "Runs" submenu on @p menu for @p palettes; @p dialogParent for the
     *  N times / Length boxes. Null when none of them is an effect. */
    static QMenu *add(QMenu *menu, Doc *doc, const QList<quint32> &palettes, QWidget *dialogParent);

    /** A "Runs" submenu on @p menu for a classic effect: Loop or Once (its
     *  Run order). Null when @p f is neither an EFX nor an RGB Matrix. */
    static QMenu *addForFunction(QMenu *menu, Doc *doc, Function *f);
};

/** @} */

#endif
