/*
  Q Light Controller Plus
  undogesture.cpp

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

#include <QCoreApplication>
#include <QMouseEvent>
#include <QPointer>

#include "undogesture.h"

namespace
{

/* Counts presses and keeps whether a button is down, from the events
   themselves (QGuiApplication::mouseButtons() is the window system's, which
   synthesized and test events never reach). A press seen twice (passed up
   to a parent) only counts twice: the number is a key, not a tally. */
class Watcher final : public QObject
{
public:
    explicit Watcher(QObject *parent) : QObject(parent) {}

    quint64 gesture = 0;
    bool down = false;

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        switch (ev->type())
        {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
            ++gesture;
            down = true;
            break;
        case QEvent::MouseButtonRelease:
            down = static_cast<QMouseEvent *>(ev)->buttons() != Qt::NoButton;
            break;
        default:
            break;
        }
        return QObject::eventFilter(obj, ev);
    }
};

QPointer<Watcher> s_watcher;

Watcher *watcher()
{
    if (s_watcher.isNull() && QCoreApplication::instance() != nullptr)
    {
        s_watcher = new Watcher(QCoreApplication::instance());
        QCoreApplication::instance()->installEventFilter(s_watcher);
    }
    return s_watcher.data();
}

}

void UndoGesture::watch()
{
    watcher();
}

QString UndoGesture::key(const QString &base)
{
    const Watcher *w = watcher();
    return w == nullptr ? base : base + QStringLiteral("#") + QString::number(w->gesture);
}

bool UndoGesture::held()
{
    const Watcher *w = watcher();
    return w != nullptr && w->down;
}
