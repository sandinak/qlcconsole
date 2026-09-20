/*
  Q Light Controller Plus - Test Unit
  programmingmanager_test.cpp

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
#include <QSignalSpy>

#define protected public
#define private public
#include "programmingmanager.h"
#include "lookeditor.h"
#include "scene.h"
#include "fixture.h"
#include "qlcpalette.h"
#include "doc.h"
#undef private
#undef protected

#include "programmingmanager_test.h"

void ProgrammingManager_Test::init()
{
    m_doc = new Doc(this);
}

void ProgrammingManager_Test::cleanup()
{
    delete m_doc;
}

void ProgrammingManager_Test::editingALookColourNotifiesItsScene()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Probe");
    fxi->setChannels(4);
    QVERIFY(m_doc->addFixture(fxi));

    QLCPalette *dim = new QLCPalette(QLCPalette::Dimmer, m_doc);
    dim->setName("Full");
    dim->setValue(255);
    QVERIFY(m_doc->addPalette(dim));

    QLCPalette *col = new QLCPalette(QLCPalette::Color, m_doc);
    col->setName("White");
    col->setValue(QColor(255, 255, 255));
    QVERIFY(m_doc->addPalette(col));

    Scene *scene = new Scene(m_doc);
    scene->setName("Edited Look");
    scene->addFixture(fxi->id());
    scene->addPalette(dim->id());
    scene->addPalette(col->id());
    QVERIFY(m_doc->addFunction(scene));

    ProgrammingManager mgr(nullptr, m_doc);
    QVERIFY(mgr.m_lookEditor != nullptr);
    mgr.m_lookEditor->setPalette(col->id());
    QCOMPARE(mgr.m_lookEditor->paletteId(), col->id());

    QSignalSpy spy(m_doc, &Doc::functionChanged);

    // What the real colour picker in the look editor does when dragged --
    // not a manually-injected Doc::functionChanged like a shallower test
    // would use, which proves nothing about whether the real UI action
    // actually reaches the scene.
    mgr.m_lookEditor->slotColorChanged(QColor(0, 0, 255));

    QVERIFY2(spy.count() > 0,
             "Doc::functionChanged never fired -- editing the look's colour "
             "through the real editor left the scene looking unchanged to "
             "anything listening for it (e.g. the rig view)");
    bool sawScene = false;
    for (const QList<QVariant> &call : spy)
        if (call.at(0).toUInt() == scene->id())
            sawScene = true;
    QVERIFY2(sawScene, "functionChanged fired, but not for the scene that "
                        "actually uses the edited palette");
}

QTEST_MAIN(ProgrammingManager_Test)
