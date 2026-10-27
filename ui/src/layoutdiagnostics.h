/*
  Q Light Controller Plus
  layoutdiagnostics.h

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

#ifndef LAYOUTDIAGNOSTICS_H
#define LAYOUTDIAGNOSTICS_H

#include <QObject>
#include <QPointer>
#include <QStringList>

#include "qlcui_export.h"

class QWidget;
class QWindow;

/** @addtogroup ui UI
 * @{
 */

/**
 * The window-size self-heal (board Q178 #11).
 *
 * On a Mac, after a tab was moved into its own window, the main window
 * sometimes grew while its contents stayed at the old size, pinned top-left
 * with blank beyond. No offscreen test reproduces it: Qt's own geometry is
 * right whenever it is told. So the platform either didn't tell Qt about the
 * resize, or didn't repaint. This watches a top-level window: on a resize or
 * an expose, once events have settled, if the platform window's size differs
 * from the widget's, it re-syncs the widget to the platform window, forces a
 * full repaint and logs it (qWarning and log()), so the next layout report
 * says whether it happened.
 */
class QLC_UI_EXPORT WindowSizeGuard final : public QObject
{
    Q_OBJECT

public:
    /** Watch @p topLevel (idempotent). The guard is the widget's child. */
    static WindowSizeGuard *watch(QWidget *topLevel);

    /** Compare now, and heal if they differ. True if it healed. */
    bool checkNow();

    /** What every guard healed, newest last (at most 50 lines). */
    static QStringList log();
    /** Forget the log (tests). */
    static void clearLog();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit WindowSizeGuard(QWidget *topLevel);
    void attachToWindow();
    void scheduleCheck();

    QPointer<QWidget> m_widget;
    QPointer<QWindow> m_window;
    bool m_pending = false;
};

namespace LayoutDiagnostics
{
    /** Help > Troubleshooting > Copy Layout Report: what each window and the
     *  levels inside it measure -- the platform window against the widget,
     *  the central area, the tabs and the page, splitters and plot
     *  viewports in it, detached windows, the screens and their pixel
     *  ratios, the Qt version -- and the self-heal log. Plain text. */
    QLC_UI_EXPORT QString report(QWidget *mainWindow);
}

/** @} */

#endif
