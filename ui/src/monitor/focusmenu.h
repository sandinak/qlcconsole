/*
  Q Light Controller Plus
  focusmenu.h

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

#ifndef FOCUSMENU_H
#define FOCUSMENU_H

#include <QCoreApplication>
#include <QString>
#include <functional>

class QAbstractButton;
class QWidget;
class QAction;
class QMenu;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The focus tools' entries, wherever they appear (board Q146, Q148): the
 * Lighting Studio's Focus ▾ menu, and why each entry is greyed when there
 * is nothing it can focus. Each *Blocked() is empty when the entry can be
 * used, else the reason, said in its tooltip.
 */
class FocusMenu final
{
    Q_DECLARE_TR_FUNCTIONS(FocusMenu)

public:
    /** Quick focus: no light (moving head or fixed) placed on the plot. */
    static QString quickFocusBlocked(Doc *doc);
    /** Venue focus: no focus calls yet, or none with a light on the plot. */
    static QString venueFocusBlocked(Doc *doc);
    /** Practice: no calls, no moving head in them, or no saved show. */
    static QString practiceBlocked(Doc *doc);
    /** The focus report: no venue yet. */
    static QString reportBlocked(Doc *doc);
    /** A new venue, or applying one: the show has no file to keep it beside. */
    static QString venuesBlocked(Doc *doc);
    /** Rig and universes: no venue applied. */
    static QString venueRigBlocked(Doc *doc);
    /** Focus this cue / look: no lights in the show at all. */
    static QString focusCueBlocked(Doc *doc);
    /** Make this a venue focus cue: no focus calls for it to run. */
    static QString venueFocusCueBlocked(Doc *doc);

    /** Enable @p action when @p blocked is empty; its tooltip the reason,
     *  else @p tip. */
    static void setAvailable(QAction *action, const QString &blocked, const QString &tip);
    static void setAvailable(QAbstractButton *button, const QString &blocked, const QString &tip);

    /** Fill the Lighting Studio's Focus ▾ menu (cleared first), its state
     *  as it is now: Quick focus (@p quickFocus), Venue focus, Edit focus
     *  calls, Venues ▸ (the venue menu, VenueMenu::fill()), Practice
     *  (scramble), Focus report. Dialogs open over @p parent. */
    static void fill(QMenu *menu, Doc *doc, QWidget *parent, const std::function<void()> &quickFocus);
};

/** @} */

#endif
