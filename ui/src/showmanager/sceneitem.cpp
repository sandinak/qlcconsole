/*
  Q Light Controller Plus
  sceneitem.cpp

  Copyright (C) Massimo Callegari

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
#include <QPainter>
#include <qmath.h>
#include <QDebug>
#include <QMenu>

#include "sceneitem.h"
#include "trackitem.h"

SceneItem::SceneItem(Scene *scene, ShowFunction *func)
    : ShowItem(func)
    , m_scene(scene)
{
    Q_ASSERT(scene != NULL);

    if (func->color().isValid())
        setColor(func->color());
    else
        setColor(ShowFunction::defaultColor(Function::SceneType));

    calculateWidth();
    connect(m_scene, SIGNAL(changed(quint32)), this, SLOT(slotSceneChanged(quint32)));
    // setName emits nameChanged (not changed) — repaint the block on rename.
    connect(m_scene, SIGNAL(nameChanged(quint32)), this, SLOT(slotSceneChanged(quint32)));
    setIconResource(":/scene.png");
}

void SceneItem::calculateWidth()
{
    int newWidth = 0;
    qint64 sceneDuration = getDuration();

    if (sceneDuration != 0)
        newWidth = ((50 / float(getTimeScale())) * float(sceneDuration)) / 1000;
    else
        newWidth = 100;

    if (newWidth < (50 / m_timeScale))
        newWidth = 50 / m_timeScale;
    setWidth(newWidth);
}

void SceneItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    ShowItem::paint(painter, option, widget);
    // A bare Scene is a flat clip — nothing beyond the background block + name.
    ShowItem::postPaint(painter);
}

void SceneItem::setTimeScale(int val)
{
    ShowItem::setTimeScale(val);
    calculateWidth();
}

void SceneItem::setDuration(quint32 msec, bool stretch)
{
    Q_UNUSED(stretch)

    // The timeline block length IS the ShowFunction duration for a bare Scene
    // (a Scene has no intrinsic total duration to stretch).
    if (m_function)
        m_function->setDuration(msec);
    prepareGeometryChange();
    calculateWidth();
    updateTooltip();
}

quint32 SceneItem::getDuration() const
{
    if (m_function && m_function->duration())
        return m_function->duration();
    return defaultDuration();
}

QString SceneItem::functionName() const
{
    if (m_scene)
        return m_scene->name();
    return QString();
}

Scene *SceneItem::getScene() const
{
    return m_scene;
}

void SceneItem::slotSceneChanged(quint32)
{
    prepareGeometryChange();
    calculateWidth();
    updateTooltip();
}

void SceneItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *)
{
    QMenu menu;
    QFont menuFont = qApp->font();
    menuFont.setPixelSize(14);
    menu.setFont(menuFont);

    foreach (QAction *action, getDefaultActions())
        menu.addAction(action);

    menu.exec(QCursor::pos());
}
