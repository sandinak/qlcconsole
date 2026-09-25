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

#include <QTreeWidget>
#include <QLabel>
#include "monitorproperties.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "truss.h"
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


/* A 540/270 moving head: the UKing 100W spot's ranges. */
static Fixture *makeSpot(Doc *doc, const QString &name)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Spot " + name);
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("2ch");
    for (int k = 0; k < 2; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(k == 0 ? "Pan" : "Tilt");
        ch->setGroup(k == 0 ? QLCChannel::Pan : QLCChannel::Tilt);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    QLCFixtureHead head;
    head.addChannel(0); head.addChannel(1);
    mode->insertHead(-1, head);
    def->addMode(mode);

    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setAddress(quint32(doc->fixtures().size() * 2));
    return doc->addFixture(fxi) ? fxi : nullptr;
}

void LooksDrop_Test::aHeadAimedOutOfReachIsFlagged()
{
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *tower = props->addTruss();
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(1.67f, 1.372f, 0.0f));
    tower->setLength(3.048f);
    tower->setWidth(0.29f);

    Fixture *onTop = makeSpot(m_doc, "On top");
    Fixture *hung  = makeSpot(m_doc, "Hung");
    QVERIFY(onTop != nullptr && hung != nullptr);
    FixtureRigProps rp;
    rp.trussId = tower->id();
    rp.trussOffset = 3.048f;
    rp.mountingType = Truss::FloorMounted;
    props->setFixtureRigProps(onTop->id(), rp);
    rp.trussOffset = 2.8f;
    rp.mountingType = Truss::TopHung;
    props->setFixtureRigProps(hung->id(), rp);

    StageTarget *who = props->addStageTarget();
    who->setName("Vocalist");
    who->setKind(StageTarget::Kind::Person);
    who->setPosition(QVector3D(1.67f, 1.872f, 0.0f));
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, m_doc);
    aim->setName("At the vocalist");
    aim->setStageTargetId(who->id());
    QVERIFY(m_doc->addPalette(aim));

    Scene *scene = new Scene(m_doc);
    scene->setName("Solo");
    scene->addFixture(onTop->id());
    scene->addFixture(hung->id());
    scene->addPalette(aim->id());
    QVERIFY(m_doc->addFunction(scene));

    SceneGroupLooks *looks = new SceneGroupLooks(scene, m_doc, nullptr, true);
    looks->reload();

    auto rowText = [&](const QString &name) {
        QTreeWidgetItemIterator it(looks->m_targetList);
        for (; *it != nullptr; ++it)
            if ((*it)->text(0).startsWith(name))
                return (*it)->text(0);
        return QString();
    };

    QVERIFY2(rowText("On top").contains("can't reach: Vocalist"),
             qPrintable("the head standing on the tower is not flagged: " + rowText("On top")));
    QVERIFY2(!rowText("Hung").contains("can't reach"),
             qPrintable("the hung head is flagged: " + rowText("Hung")));
    QVERIFY(looks->m_targetsLabel->text().contains("1 can't reach"));

    // Moving the target somewhere it can reach clears it too -- the target
    // moving is as much a change as the head being re-rigged.
    who->setPosition(QVector3D(1.67f, 4.372f, 0.0f));
    props->notifyStageTargetsChanged();
    QCoreApplication::processEvents();
    QVERIFY2(!rowText("On top").contains("can't reach"),
             qPrintable("still flagged after the target moved away: " + rowText("On top")));
    // ...and back in close, it is flagged again.
    who->setPosition(QVector3D(1.67f, 1.872f, 0.0f));
    props->notifyStageTargetsChanged();
    QCoreApplication::processEvents();
    QVERIFY(rowText("On top").contains("can't reach: Vocalist"));

    // Re-rig the one on top to hang: the warning clears on its own.
    props->setFixtureRigProps(onTop->id(), rp);
    QCoreApplication::processEvents();
    QVERIFY2(!rowText("On top").contains("can't reach"),
             qPrintable("still flagged after being hung: " + rowText("On top")));
    QVERIFY(!looks->m_targetsLabel->text().contains("can't reach"));

    delete looks;
}

QTEST_MAIN(LooksDrop_Test)
