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
#include <QGuiApplication>
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
        // A spontaneous show (un-minimised, the window system's own) is
        // where the person left it.
        if (e->type() == QEvent::Show && !e->spontaneous() && obj->isWidgetType())
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

QRect WindowPlacement::clampInto(QRect r, const QRect &room)
{
    r.moveLeft(qBound(room.left(), r.left(), qMax(room.left(), room.right() + 1 - r.width())));
    r.moveTop(qBound(room.top(), r.top(), qMax(room.top(), room.bottom() + 1 - r.height())));
    return r;
}

bool WindowPlacement::keepOnParentScreen(QWidget *w)
{
    if (w == nullptr || !w->isWindow() || w->windowType() != Qt::Dialog || w->parentWidget() == nullptr)
        return false;
    QWidget *anchor = w->parentWidget()->window();
    if (anchor == nullptr || !anchor->isVisible() || anchor->screen() == nullptr)
        return false;

    QRect frame = w->frameGeometry();
    if (w->windowHandle() != nullptr)
        frame = frame.united(w->geometry().marginsAdded(w->windowHandle()->frameMargins()));
    const bool neverPlaced = !w->testAttribute(Qt::WA_Moved);

    /* Where it goes. One never placed: QDialog centres it over its main
       window, on that window's screen; only its size is ours to check. One
       placed (a saved place, a move, the person's own drag onto another
       screen): it stays wherever it is wholly on a screen, and is only
       pulled onto one when it is on none or across an edge. */
    QScreen *screen = anchor->screen();
    if (!neverPlaced)
    {
        for (QScreen *s : QGuiApplication::screens())
            if (s->availableGeometry().contains(frame))
                return false;
        if (QScreen *under = QGuiApplication::screenAt(frame.center()))
            screen = under;
    }
    const QRect room = screen->availableGeometry();
    if (neverPlaced && frame.width() <= room.width() && frame.height() <= room.height())
        return false;

    /* The frame's own margins (title bar): the window's, which the platform
       knows once it exists, even before the widget's frame geometry does. */
    QSize margins = frame.size() - w->size();
    if (w->windowHandle() != nullptr)
    {
        const QMargins m = w->windowHandle()->frameMargins();
        margins = margins.expandedTo(QSize(m.left() + m.right(), m.top() + m.bottom()));
    }
    const QSize size = w->size().boundedTo(room.size() - margins).expandedTo(w->minimumSize());
    if (size != w->size())
        w->resize(size);

    /* Placed here, which also keeps QDialog from centring it again by its
       own sums (they left a dialog as tall as the screen 2 px past its
       bottom): one never placed is centred over its main window, as QDialog
       would; one placed keeps its place as far as the room allows. */
    QRect r(frame.topLeft(), size + margins);
    if (neverPlaced)
        r.moveCenter(anchor->frameGeometry().center());
    r = clampInto(r, room);
    if (w->windowHandle() != nullptr && w->windowHandle()->screen() != screen)
        w->windowHandle()->setScreen(screen);
    w->move(r.topLeft());
    return true;
}
