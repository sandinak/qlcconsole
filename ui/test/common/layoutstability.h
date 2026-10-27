/*
  Q Light Controller Plus
  layoutstability.h

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

#ifndef LAYOUTSTABILITY_H
#define LAYOUTSTABILITY_H

#include <QCoreApplication>
#include <QStringList>
#include <QWidget>
#include <QLayout>
#include <QRect>
#include <QHash>
#include <QSize>

/* A window's layout, to check that changing what it says never moves it:
   the window's size and the geometry of the named children (objectName),
   plus any extra widgets given by a name of the test's own. */
struct LayoutSnapshot
{
    QSize size;
    QHash<QString, QRect> rects;

    static void settle(QWidget *window)
    {
        QCoreApplication::processEvents();
        if (window->layout() != nullptr)
            window->layout()->activate();
        QCoreApplication::processEvents();
    }

    static LayoutSnapshot take(QWidget *window, const QStringList &names,
                               const QHash<QString, QWidget *> &extra = QHash<QString, QWidget *>())
    {
        settle(window);
        LayoutSnapshot s;
        s.size = window->size();
        for (const QString &name : names)
        {
            QWidget *w = window->findChild<QWidget *>(name);
            s.rects.insert(name, w != nullptr ? w->geometry() : QRect(-1, -1, -1, -1));
        }
        for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
            s.rects.insert(it.key(), it.value() != nullptr ? it.value()->geometry() : QRect(-1, -1, -1, -1));
        return s;
    }

    /* What moved since @p before, at @p step; empty when nothing did. */
    QString movedSince(const LayoutSnapshot &before, const QString &step) const
    {
        QStringList moved;
        if (size != before.size)
            moved << QStringLiteral("the window %1x%2 -> %3x%4")
                     .arg(before.size.width()).arg(before.size.height())
                     .arg(size.width()).arg(size.height());
        for (auto it = before.rects.constBegin(); it != before.rects.constEnd(); ++it)
        {
            const QRect now = rects.value(it.key());
            if (now != it.value())
                moved << QStringLiteral("%1 (%2,%3 %4x%5) -> (%6,%7 %8x%9)").arg(it.key())
                         .arg(it.value().x()).arg(it.value().y())
                         .arg(it.value().width()).arg(it.value().height())
                         .arg(now.x()).arg(now.y()).arg(now.width()).arg(now.height());
        }
        if (moved.isEmpty())
            return QString();
        return QStringLiteral("after %1: %2").arg(step, moved.join(QStringLiteral("; ")));
    }
};

#endif
