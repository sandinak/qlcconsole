/*
  Q Light Controller Plus
  blockingwindowbanner.h

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

#ifndef BLOCKINGWINDOWBANNER_H
#define BLOCKINGWINDOWBANNER_H

#include <QFrame>
#include <QPointer>

#include "qlcui_export.h"

class QLabel;
class QPushButton;

/** A window that is open and holds part of the app still (Fixture Properties
 *  locks the rig view while it is open) can end up out of sight: behind
 *  another window, on another macOS Space, minimised. The part it holds then
 *  just stops answering. This banner sits across the top of what is held:
 *  "Fixture Properties is open. [Show it]", and Show it brings that window
 *  back, in front, on the held area's screen.
 *
 *  It is shown for as long as the window is open, and takes itself away (and
 *  its event filter with it) when the window finishes or is deleted. Its
 *  button works while the lock drops every other click in the held area: it
 *  answers its own clicks ahead of the lock (an application event filter
 *  installed after the lock's runs before it), at the window as a real
 *  screen delivers them. */
class QLC_UI_EXPORT BlockingWindowBanner final : public QFrame
{
    Q_OBJECT

public:
    /** Shows the banner over @p heldArea while @p blocker is open. Call it
     *  after the lock is in place. Returns the banner (owned by @p heldArea;
     *  it deletes itself when @p blocker closes). Null when either is null. */
    static BlockingWindowBanner *whileOpen(QWidget *heldArea, QWidget *blocker);

    /** Brings @p window back where @p near is: un-minimised, onto @p near's
     *  screen if it is on another, shown, raised and given the keyboard. */
    static void bringBack(QWidget *window, QWidget *near);

    /** The banner over @p heldArea, if one is up. */
    static BlockingWindowBanner *bannerOn(const QWidget *heldArea);

    QWidget *blocker() const { return m_blocker; }
    QPushButton *showButton() const { return m_show; }
    QString text() const;

    /** "Show it": bringBack(blocker, the held area). */
    void showBlocker();

protected:
    bool eventFilter(QObject *obj, QEvent *e) override;

private:
    BlockingWindowBanner(QWidget *heldArea, QWidget *blocker);
    ~BlockingWindowBanner() override;

    void place();
    void finish();
    bool onShowButton(QObject *obj) const;

    QPointer<QWidget> m_area;
    QPointer<QWidget> m_blocker;
    QLabel *m_label = nullptr;
    QPushButton *m_show = nullptr;
    bool m_filtering = false;
    bool m_pressed = false;
};

#endif
