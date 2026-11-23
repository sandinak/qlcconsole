/*
  Q Light Controller Plus
  windowspace.cpp

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
#include <QPointer>
#include <QWidget>
#include <QEvent>

#include "windowspace.h"

namespace
{
int s_kept = 0;
QPointer<QObject> s_watcher;

class ShowWatcher final : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter(QObject *obj, QEvent *e) override
    {
        /* Show comes after the native window exists and before it is
           ordered in, every time it is shown -- also after Qt made a new
           NSWindow for it (a flag change). */
        if (e->type() == QEvent::Show && obj->isWidgetType())
        {
            QWidget *w = static_cast<QWidget *>(obj);
            if (WindowSpace::staysWithParent(w))
            {
                ++s_kept;
                WindowSpace::keepOnParentSpace(w);
            }
        }
        return false;
    }
};
} // namespace

bool WindowSpace::staysWithParent(const QWidget *w)
{
    return w != nullptr && w->isWindow() && w->windowType() == Qt::Dialog
           && w->parentWidget() != nullptr;
}

void WindowSpace::install()
{
    if (s_watcher != nullptr || qApp == nullptr)
        return;
    s_watcher = new ShowWatcher(qApp);
    qApp->installEventFilter(s_watcher);
}

bool WindowSpace::isInstalled()
{
    return s_watcher != nullptr;
}

int WindowSpace::keptCount()
{
    return s_kept;
}

#ifndef Q_OS_MACOS
bool WindowSpace::keepOnParentSpace(QWidget *)
{
    return false;
}
#endif
