/*
  Q Light Controller Plus
  sceneitem.h

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

#ifndef SCENEITEM_H
#define SCENEITEM_H

#include <QGraphicsItem>
#include <QObject>
#include <QAction>
#include <QFont>

#include "showitem.h"
#include "scene.h"

/** @addtogroup ui_functions
 * @{
 */

/**
 *
 * Scene Item. A bare Scene placed directly on a Show timeline track as a simple
 * timed "clip": it runs the Scene for the item's duration (the ShowRunner starts
 * the Scene at the item's start time and stops it at start + duration, so the
 * Scene fades out over its own fade-out time). No hidden Sequence/chase wrapper.
 *
 */
class SceneItem final : public ShowItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    SceneItem(Scene *scene, ShowFunction *func);

    /** @reimp */
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    /** @reimp — double-click jumps to the Programming tab with this scene
     *  loaded, so there's an assisted path from a timeline clip to the look
     *  it plays (previously: only "Align to cursor"/"Lock" in the context
     *  menu, no edit action at all). */
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

    /** @reimp */
    void setTimeScale(int val) override;

    /** @reimp */
    void setDuration(quint32 msec, bool stretch) override;

    /** @reimp */
    quint32 getDuration() const override;

    /** @reimp */
    QString functionName() const override;

    /** Return a pointer to the Scene Function associated to this item */
    Scene *getScene() const;

    /** Default clip length (ms) for a freshly-dropped Scene. */
    static quint32 defaultDuration() { return 5000; }

protected:
    /** @reimp */
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

protected slots:
    void slotSceneChanged(quint32);

private:
    /** Calculate item width for paint() and boundingRect() */
    void calculateWidth();

private:
    /** Reference to the actual Scene Function */
    Scene *m_scene;
};

/** @} */

#endif
