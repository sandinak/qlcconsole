/*
  Q Light Controller Plus
  venuemenu.h

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

#ifndef VENUEMENU_H
#define VENUEMENU_H

#include <QCoreApplication>
#include <QToolButton>
#include <QMenu>
#include <QPointer>
#include <QString>

class QInputDialog;
class QTimer;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The venue menu (board Q150): one menu, wherever it opens -- the venue
 * chip in the app's footer, View ▸ Venue…, and the Lighting Studio's
 * Focus ▸ Venues ▸. Switch venue ▸ (the venues and None), New venue…,
 * Rename…, Rig and universes…, Venue focus…, Focus report…, and Clashes (n)
 * when the venue applied has any; "Still at <venue>?" first while that is
 * unanswered.
 */
class VenueMenu final
{
    Q_DECLARE_TR_FUNCTIONS(VenueMenu)

public:
    /** "Venue: Lincoln HS", "Venue: Lincoln HS?" (not confirmed yet: Still
     *  at…?), "Venue: none"; with clashes, " · 3 clashes" after it.
     *  @p clashes: clashCount(), when known already (-1: worked out). */
    static QString chipText(Doc *doc, int clashes = -1);
    /** What the chip says when pointed at: what is applied, whether it was
     *  confirmed, its clashes. */
    static QString chipToolTip(Doc *doc, int clashes = -1);
    /** The clashes of the venue applied (VenueRig::clashes()); 0 with none. */
    static int clashCount(Doc *doc);

    /** Rename: no venue applied. */
    static QString renameBlocked(Doc *doc);

    /** Fill @p menu (cleared first) as things are now. Dialogs open over
     *  @p parent. */
    static void fill(QMenu *menu, Doc *doc, QWidget *parent);

    /** Apply @p name ("" for None), as Switch venue ▸ does: not while a
     *  focus session holds the heads, and the clashes asked about first.
     *  True when it is applied. */
    static bool switchTo(Doc *doc, QWidget *parent, const QString &name);
    /** New venue… : its name asked ("venueNewName", not modal); made from
     *  the last venue's corrections and applied. */
    static QInputDialog *askNewVenue(Doc *doc, QWidget *parent);
    /** Rename… the venue applied ("venueRenameName", not modal). */
    static QInputDialog *askRename(Doc *doc, QWidget *parent);
};

/**
 * The venue chip in the app's footer, beside Ready and the Grand Master:
 * "Venue: Lincoln HS", amber with a count when it clashes with the show.
 * A click opens the venue menu under it. Kept up to date as venues are
 * applied, renamed, removed and confirmed, and as the rig changes.
 */
class VenueChip final : public QToolButton
{
    Q_OBJECT

public:
    VenueChip(Doc *doc, QWidget *parent);

    /** Shown whatever the show has (the qlcconsole and Everything views);
     *  else (Classic) only when the show has venues. */
    void setAlwaysShown(bool on);
    bool alwaysShown() const { return m_alwaysShown; }
    /** Whether it would be shown now (its parent shown). */
    bool wanted() const;

    int clashes() const { return m_clashes; }
    bool isAmber() const { return m_clashes > 0; }

    /** The venue menu, filled now, opened under the chip (or at the mouse
     *  when the chip is hidden). Not modal. */
    QMenu *openMenu();
    QMenu *menu() const { return m_menu; }

public slots:
    /** Read everything again now. */
    void refresh();

private:
    /** refresh() a moment from now, once for many changes. */
    void refreshSoon();

private:
    Doc *m_doc;
    bool m_alwaysShown = true;
    int m_clashes = 0;
    QPointer<QMenu> m_menu;
    QTimer *m_soon;
};

/** @} */

#endif
