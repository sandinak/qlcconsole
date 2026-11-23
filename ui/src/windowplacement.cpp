/*
  Q Light Controller Plus
  windowplacement.cpp

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

#include <QApplication>
#include <QEvent>
#include <QMargins>
#include <QPointer>
#include <QScreen>
#include <QWidget>
#include <QWindow>

#include "windowplacement.h"

namespace
{
QPointer<QObject> s_watcher;

class ShowWatcher final : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter(QObject *obj, QEvent *e) override
    {
        /* Show comes before the window is put on the screen (and before
           QDialog centres one that was never moved): placed here, it never
           flashes up elsewhere. */
        if (e->type() == QEvent::Show && obj->isWidgetType())
            WindowPlacement::keepOnParentScreen(static_cast<QWidget *>(obj));
        return false;
    }
};
} // namespace

void WindowPlacement::install()
{
    if (s_watcher != nullptr || qApp == nullptr)
        return;
    s_watcher = new ShowWatcher(qApp);
    qApp->installEventFilter(s_watcher);
}

bool WindowPlacement::keepOnParentScreen(QWidget *w)
{
    if (w == nullptr || !w->isWindow() || w->windowType() != Qt::Dialog || w->parentWidget() == nullptr)
        return false;
    QWidget *anchor = w->parentWidget()->window();
    if (anchor == nullptr || !anchor->isVisible())
        return false;
    QScreen *screen = anchor->screen();
    if (screen == nullptr)
        return false;

    const QRect room = screen->availableGeometry();
    QRect frame = w->frameGeometry();
    if (w->windowHandle() != nullptr)
        frame = frame.united(w->geometry().marginsAdded(w->windowHandle()->frameMargins()));
    const bool elsewhere = w->screen() != screen || !room.intersects(frame);
    if (!elsewhere && room.contains(frame))
        return false;

    /* The frame's own margins (title bar): the window's, which the platform
       knows once it exists, even before the widget's frame geometry does. */
    QSize margins = frame.size() - w->size();
    if (w->windowHandle() != nullptr)
    {
        const QMargins m = w->windowHandle()->frameMargins();
        margins = margins.expandedTo(QSize(m.left() + m.right(), m.top() + m.bottom()));
    }
    const QSize most = room.size() - margins;
    const QSize size = w->size().boundedTo(most).expandedTo(w->minimumSize());
    if (size != w->size())
        w->resize(size);

    /* Moved only when it was put somewhere (a saved place, or a move) or is
       on another screen: one never moved is centred over its parent by
       QDialog itself, which also keeps it on that screen. */
    if (!elsewhere && !w->testAttribute(Qt::WA_Moved))
        return true;
    QRect r(frame.topLeft(), size + margins);
    if (elsewhere)
        r.moveCenter(anchor->frameGeometry().center());
    r.moveLeft(qBound(room.left(), r.left(), qMax(room.left(), room.right() + 1 - r.width())));
    r.moveTop(qBound(room.top(), r.top(), qMax(room.top(), room.bottom() + 1 - r.height())));
    if (w->windowHandle() != nullptr && w->windowHandle()->screen() != screen)
        w->windowHandle()->setScreen(screen);
    w->move(r.topLeft());
    return true;
}
