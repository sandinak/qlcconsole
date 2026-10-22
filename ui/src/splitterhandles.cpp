/*
  Q Light Controller Plus
  splitterhandles.cpp

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

#include <QSplitterHandle>
#include <QSizePolicy>
#include <QSplitter>
#include <QSettings>
#include <QEvent>

#include "splitterhandles.h"

namespace
{
/** Catches a double-click on a splitter's handles. One per splitter, its
 *  child, so it goes when the splitter does. */
class HandleResetFilter final : public QObject
{
public:
    HandleResetFilter(QSplitter *splitter, const QString &key)
        : QObject(splitter), m_splitter(splitter), m_key(key) {}

    void watch()
    {
        // Handle 0 is never shown; a pane added later brings a new handle.
        for (int i = 1; i < m_splitter->count(); ++i)
            m_splitter->handle(i)->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonDblClick)
        {
            SplitterHandles::reset(m_splitter, m_key);
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QSplitter *m_splitter;
    QString m_key;
};

int stretchOf(const QWidget *w, Qt::Orientation o)
{
    const QSizePolicy sp = w->sizePolicy();
    return o == Qt::Horizontal ? sp.horizontalStretch() : sp.verticalStretch();
}

int hintOf(const QWidget *w, Qt::Orientation o)
{
    const QSize hint = w->sizeHint().expandedTo(w->minimumSizeHint());
    return o == Qt::Horizontal ? hint.width() : hint.height();
}
}

QList<int> SplitterHandles::defaultSizes(QSplitter *splitter)
{
    QList<int> sizes;
    if (splitter == nullptr)
        return sizes;
    const Qt::Orientation o = splitter->orientation();
    int total = 0;
    for (int s : splitter->sizes())
        total += s;
    if (total <= 0)
        total = o == Qt::Horizontal ? splitter->width() : splitter->height();

    // Panes with no stretch take their size hint; the rest share what is left.
    int fixed = 0, stretchSum = 0;
    for (int i = 0; i < splitter->count(); ++i)
    {
        const QWidget *w = splitter->widget(i);
        if (w->isHidden())
            continue;
        const int st = stretchOf(w, o);
        if (st > 0)
            stretchSum += st;
        else
            fixed += hintOf(w, o);
    }
    const int left = qMax(0, total - fixed);
    for (int i = 0; i < splitter->count(); ++i)
    {
        const QWidget *w = splitter->widget(i);
        if (w->isHidden())
        {
            sizes << 0;
            continue;
        }
        const int st = stretchOf(w, o);
        if (stretchSum == 0)
            sizes << hintOf(w, o);
        else if (st > 0)
            sizes << left * st / stretchSum;
        else
            sizes << qMin(hintOf(w, o), total);
    }
    return sizes;
}

void SplitterHandles::reset(QSplitter *splitter, const QString &key)
{
    if (splitter == nullptr)
        return;
    splitter->setSizes(defaultSizes(splitter));
    if (!key.isEmpty())
        QSettings().remove(key);
}

void SplitterHandles::resetOnDoubleClick(QSplitter *splitter, const QString &key)
{
    if (splitter == nullptr)
        return;
    HandleResetFilter *f = new HandleResetFilter(splitter, key);
    f->watch();
}

void SplitterHandles::remember(QSplitter *splitter, const QString &key)
{
    if (splitter == nullptr)
        return;
    const QVariant state = QSettings().value(key);
    if (state.isValid())
        splitter->restoreState(state.toByteArray());
    QObject::connect(splitter, &QSplitter::splitterMoved, splitter, [splitter, key]() {
        QSettings().setValue(key, splitter->saveState());
    });
    resetOnDoubleClick(splitter, key);
}
