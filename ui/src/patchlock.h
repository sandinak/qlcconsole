/*
  Q Light Controller Plus
  patchlock.h

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

#ifndef PATCHLOCK_H
#define PATCHLOCK_H

#include <QObject>
#include <QPointer>
#include <QIcon>
#include <QString>

#include "qlcui_export.h"
#include "doc.h"

class QWidget;
class QPushButton;
class QLabel;

/** "The patch is locked" in Operate (board Q358): Fixtures and Connections
 *  say so and cannot be edited until Design. One helper does all of it, so no
 *  control on either tab carries its own Operate rule for this:
 *   - a slim bar across the top of the page, with a Design button;
 *   - every editing control under the page greyed, the sentence as its
 *     tooltip (buttons, spin boxes, combos, the toolbars' actions, the
 *     tree's in-place editing and drag and drop; its context menus' editing
 *     items too);
 *   - the tab's name carries a lock (lockedIcon(), App).
 *  All of it is gone in Design. */
class QLC_UI_EXPORT PatchLock : public QObject
{
    Q_OBJECT

public:
    /** Puts the bar at the top of @p page's box layout and starts following
     *  @p doc's mode. Call once the page's own controls exist; a control made
     *  later is caught by the next refresh(). */
    static PatchLock *attach(QWidget *page, Doc *doc);
    static PatchLock *of(const QWidget *page);

    /** "Operate: the patch is locked. Design to change it" */
    static QString sentence();
    static QIcon lockedIcon(const QIcon &icon);

    bool isLocked() const;
    QWidget *bar() const { return m_bar; }
    QPushButton *designButton() const { return m_design; }

    /** Applies the doc's mode again: call from the page's own mode handler,
     *  after it has set what Design needs. */
    void refresh();

    /** Greys @p menu's editing items (the application-wide filter calls this
     *  as a menu of a locked page shows; a test may too). */
    void lockMenu(QObject *menu);
    /** A control shown after the page was locked (an editor opened in Operate). */
    void lockLate(QWidget *w);

signals:
    /** The Design button: App goes to Design through its usual path (the
     *  show's PIN included). */
    void designRequested();

private:
    PatchLock(QWidget *page, Doc *doc);
    void applyTo(bool locked);
    void lockOne(QWidget *w, bool locked);

    QPointer<QWidget> m_page;
    QPointer<Doc> m_doc;
    QWidget *m_bar;
    QPushButton *m_design;
    QLabel *m_text;
    bool m_locked;
};

#endif
