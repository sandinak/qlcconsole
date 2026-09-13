/*
  Q Light Controller Plus
  looksdrop_test.cpp

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

#include <QtTest>

#define protected public
#define private public
#include "scenegrouplooks.h"
#undef private
#undef protected

#include "qlcpalette.h"
#include "scene.h"
#include "doc.h"

#include "looksdrop_test.h"

void LooksDrop_Test::init()
{
    m_doc = new Doc(this);
}

void LooksDrop_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void LooksDrop_Test::aDroppedLookLandsOnTheSceneNotInsideTheEffect()
{
    /* In this model the palette ORDER decides nesting: a Colour or Dimmer
       listed after an Effect belongs to that effect and feeds it, rather than
       painting the scene. Scene::addPalette() appends — so every dropped look
       landed at the end, which is after the effect, which means inside it.
       Dropping a colour ABOVE the effect in the list put it under the effect
       anyway, which is what Branson hit.

       Appending is not a neutral default when position carries meaning. */
    QLCPalette *dim = new QLCPalette(QLCPalette::Dimmer, m_doc);
    dim->setName("Full");
    QVERIFY(m_doc->addPalette(dim));

    QLCPalette *fx = new QLCPalette(QLCPalette::Effect, m_doc);
    fx->setName("followspot");
    fx->setScriptPath("Palettes/Effect/Position/Followspot/");
    QVERIFY(m_doc->addPalette(fx));

    QLCPalette *col = new QLCPalette(QLCPalette::Color, m_doc);
    col->setName("Blue");
    col->setValue(QColor(0, 0, 255));
    QVERIFY(m_doc->addPalette(col));

    Scene *scene = new Scene(m_doc);
    scene->setName("Drop Target");
    scene->addPalette(dim->id());
    scene->addPalette(fx->id());
    QVERIFY(m_doc->addFunction(scene));

    SceneGroupLooks looks(scene, m_doc);

    // What a drop does: add (which appends), then place.
    scene->addPalette(col->id());
    QCOMPARE(scene->palettes().last(), col->id());     // appended, i.e. inside the effect

    /* Dropped on no particular row -- onto the panel, as you do when you just
       want the colour in the scene. */
    looks.placeDroppedLooks(QList<quint32>() << col->id(), QPoint(-500, -500));

    const QList<quint32> order = scene->palettes();
    const int colIx = order.indexOf(col->id());
    const int fxIx  = order.indexOf(fx->id());
    QVERIFY(colIx >= 0);
    QVERIFY(fxIx >= 0);
    QVERIFY2(colIx < fxIx,
             qPrintable(QString("a dropped colour landed at %1, after the effect "
                                "at %2 -- it has been swallowed by the effect "
                                "instead of colouring the scene")
                        .arg(colIx).arg(fxIx)));

    /* And it goes AFTER the existing base looks, not in front of them: the
       scene's own dimmer was there first and the drop should not jump the
       queue. */
    QCOMPARE(order.indexOf(dim->id()), 0);
    QCOMPARE(colIx, 1);

    /* A scene with no effect at all: the drop simply goes on the end, which is
       the same thing -- everything is a base look. */
    Scene *plain = new Scene(m_doc);
    plain->setName("No Effect");
    plain->addPalette(dim->id());
    QVERIFY(m_doc->addFunction(plain));

    SceneGroupLooks looks2(plain, m_doc);
    plain->addPalette(col->id());
    looks2.placeDroppedLooks(QList<quint32>() << col->id(), QPoint(-500, -500));
    QCOMPARE(plain->palettes().size(), 2);
    QCOMPARE(plain->palettes().last(), col->id());
}

QTEST_MAIN(LooksDrop_Test)
