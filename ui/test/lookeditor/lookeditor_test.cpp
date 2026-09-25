/*
  Q Light Controller Plus - Test Unit
  lookeditor_test.cpp

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
#include <QSlider>
#include <QLabel>
#include <QComboBox>

#define private public
#include "lookeditor.h"
#undef private
#include "qlcpalette.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

#include "lookeditor_test.h"

void LookEditor_Test::aColourDarkerThanAFifthIsCalledOut()
{
    const QColor none;                                  // no white/amber/UV stored
    // The "Red" that shipped in the demo show: red 10, blue 2.
    const QString dim = LookEditor::darkColourWarning(QColor(10, 0, 2), none);
    QVERIFY2(dim.contains("4%") && dim.contains("Val"), qPrintable(dim));
    QVERIFY(dim.contains("Colour-wheel"));

    QVERIFY(LookEditor::darkColourWarning(QColor(0, 0, 0), QColor(0, 0, 0)).contains("black"));
    QVERIFY(LookEditor::darkColourWarning(QColor(1, 0, 0), none).contains("1%"));
    QVERIFY(LookEditor::darkColourWarning(QColor(50, 0, 0), none).contains("20%"));

    // Bright enough, by RGB or by an extra emitter: nothing to say.
    QVERIFY(LookEditor::darkColourWarning(QColor(51, 0, 0), none).isEmpty());
    QVERIFY(LookEditor::darkColourWarning(QColor(255, 0, 0), none).isEmpty());
    QVERIFY(LookEditor::darkColourWarning(QColor(0, 0, 0), QColor(200, 0, 0)).isEmpty());
}

void LookEditor_Test::aDarkRedLookWarnsUntilItIsBrightened()
{
    Doc doc(this);
    QLCPalette *red = new QLCPalette(QLCPalette::Color, &doc);
    red->setName("Red");
    red->setValue("#0a0002000000");
    QVERIFY(doc.addPalette(red));

    LookEditor ed(&doc);
    ed.setPalette(red->id());
    QVERIFY(!ed.m_colorWarning->isHidden());
    QVERIFY2(ed.m_colorWarning->text().contains("4%"), qPrintable(ed.m_colorWarning->text()));

    // Brightened with the R slider: the look is red now, and says nothing.
    ed.m_redSlider->setValue(255);
    QVERIFY(ed.m_colorWarning->isHidden());
    QCOMPARE(red->rgbValue().red(), 255);

    // Dragged back down to near nothing: said again.
    ed.m_redSlider->setValue(20);
    QVERIFY(!ed.m_colorWarning->isHidden());
    QVERIFY(ed.m_colorWarning->text().contains("8%"));
}

/* A one-channel fixture type whose channel is a gobo wheel with the given
   slots, 20 DMX values each (or a plain dimmer when @p gobos is empty). */
static QLCFixtureDef *wheelDef(const QString &model, const QStringList &gobos)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel(model);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("1ch");
    QLCChannel *ch = new QLCChannel();
    ch->setName(gobos.isEmpty() ? "Dimmer" : "Gobo");
    ch->setGroup(gobos.isEmpty() ? QLCChannel::Intensity : QLCChannel::Gobo);
    ch->setControlByte(QLCChannel::MSB);
    for (int i = 0; i < gobos.size(); i++)
        ch->addCapability(new QLCCapability(uchar(i * 20), uchar(i * 20 + 19), gobos.at(i)));
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    def->addMode(mode);
    return def;
}

static Fixture *addOf(Doc &doc, QLCFixtureDef *def)
{
    Fixture *f = new Fixture(&doc);
    f->setFixtureDefinition(def, def->modes().first());
    f->setAddress(quint32(doc.fixtures().size()));
    return doc.addFixture(f) ? f : nullptr;
}

static QStringList itemsOf(const QComboBox *c)
{
    QStringList out;
    for (int i = 0; i < c->count(); i++)
        out << c->itemText(i);
    return out;
}

void LookEditor_Test::aNewGoboOffersTheWorkspacesWheels()
{
    QLCFixtureDef *spot = wheelDef("Spot", { "Open", "Star", "Ring" });
    QLCFixtureDef *beam = wheelDef("Beam", { "Open", "Dots" });
    QLCFixtureDef *par  = wheelDef("Par", {});
    {
        Doc doc(this);
        QLCPalette *gobo = new QLCPalette(QLCPalette::Gobo, &doc);
        gobo->setValue(0);
        QVERIFY(doc.addPalette(gobo));

        // No fixture with a wheel at all: no picker, only the raw value.
        QVERIFY(addOf(doc, par) != nullptr);
        LookEditor ed(&doc);
        ed.setPalette(gobo->id());
        QVERIFY(ed.m_singleCombo->isHidden());
        QVERIFY(ed.m_singleSourceCombo->isHidden());

        // One type with a wheel: its gobos, and nothing to choose between.
        Fixture *b1 = addOf(doc, beam);
        QVERIFY(b1 != nullptr);
        ed.setPalette(gobo->id());
        QVERIFY(!ed.m_singleCombo->isHidden());
        QCOMPARE(itemsOf(ed.m_singleCombo), QStringList({ "Open", "Dots" }));
        QVERIFY(ed.m_singleSourceCombo->isHidden());

        // Two types: the more numerous first, and a "Wheel of" chooser.
        QVERIFY(addOf(doc, spot) != nullptr);
        QVERIFY(addOf(doc, spot) != nullptr);
        ed.setPalette(gobo->id());
        QVERIFY(!ed.m_singleSourceCombo->isHidden());
        QCOMPARE(ed.m_singleSourceCombo->count(), 2);
        QVERIFY2(ed.m_singleSourceCombo->itemText(0).contains("Spot")
                 && ed.m_singleSourceCombo->itemText(0).contains("2 fixtures"),
                 qPrintable(ed.m_singleSourceCombo->itemText(0)));
        QCOMPARE(itemsOf(ed.m_singleCombo), QStringList({ "Open", "Star", "Ring" }));

        // Picking a gobo stores its DMX value.
        ed.m_singleCombo->setCurrentIndex(2);
        ed.slotSingleCapabilityPicked(2);
        QCOMPARE(gobo->intValue1(), 49);                // Ring: 40-59

        // Switch wheels: the list follows, the stored value does not change.
        ed.m_singleSourceCombo->setCurrentIndex(1);
        ed.slotSingleSourcePicked(1);
        QCOMPARE(itemsOf(ed.m_singleCombo), QStringList({ "Open", "Dots" }));
        QCOMPARE(gobo->intValue1(), 49);
        ed.slotSingleCapabilityPicked(1);
        QCOMPARE(gobo->intValue1(), 29);                // Dots: 20-39

        // The chosen wheel is remembered for the next gobo opened.
        ed.setPalette(gobo->id());
        QCOMPARE(ed.m_singleSourceCombo->currentIndex(), 1);
        QCOMPARE(ed.m_singleCombo->currentText(), QString("Dots"));

        // In a look, the look's own fixtures decide: only the Beam here.
        Scene *look = new Scene(&doc);
        QVERIFY(doc.addFunction(look));
        look->addFixture(b1->id());
        ed.setContextScene(look);
        ed.setPalette(gobo->id());
        QVERIFY(ed.m_singleSourceCombo->isHidden());
        QCOMPARE(itemsOf(ed.m_singleCombo), QStringList({ "Open", "Dots" }));

        // A look with no wheel among its fixtures falls back to the workspace.
        look->removeFixture(b1->id());
        look->addFixture(doc.fixtures().first()->id());   // the Par
        ed.setPalette(gobo->id());
        QCOMPARE(ed.m_singleSourceCombo->count(), 2);
        ed.setContextScene(nullptr);
    }
    delete spot;
    delete beam;
    delete par;
}

QTEST_MAIN(LookEditor_Test)
