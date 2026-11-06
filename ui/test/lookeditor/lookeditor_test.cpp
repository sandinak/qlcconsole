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
#include <QBuffer>
#include <QXmlStreamWriter>
#include <QScrollArea>
#include <QToolButton>
#include <QMenu>
#include <QTextBrowser>
#include <QDialog>
#include <QPushButton>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QCollator>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTemporaryDir>
#include <QImage>
#include <QColorDialog>
#include <QSignalSpy>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#include "lookeditor.h"
#include "programmercontroller.h"
#include "matriximage.h"
#include "effectbrowser.h"
#undef private
#include <QListWidget>
#include <QLineEdit>
#include <QTabBar>
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "effectpresetcache.h"
#include "effectorigin.h"
#include "effectrunsmenu.h"
#include "qlcpalette.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"
#include "gobolibrary.h"
#include "monitorproperties.h"

#include "speedmasters.h"
#include "spreadpreview.h"
#include "linespreadpreview.h"
#include "linespread.h"
#include "stagetarget.h"
#include "lookeditor_test.h"
#include "stagecolourpicker.h"
#include "palettenaming.h"
#include <QHelpEvent>
#include <QLayout>
#include <QScrollBar>
#include <QToolTip>
#include "qlcfixturedefcache.h"
#include "qlcfile.h"
#include "paletteops.h"
#include "formationeditor.h"
#include "lineplanview.h"
#include "lineformation.h"
#include "beamlanding.h"
#include "truss.h"
#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "fixturegroup.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "aimsolver.h"
#include "lengthspinbox.h"
#include "measurementunits.h"
#include "monitor/feetinchesspinbox.h"

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

void LookEditor_Test::colourPickerLayoutHoldsAtEveryWidth()
{
    Doc doc(this);
    QLCPalette *p = new QLCPalette(QLCPalette::Color, &doc);
    p->setName("Flesh");
    p->setValue("#ffcdaa000000");
    QVERIFY(doc.addPalette(p));

    LookEditor ed(&doc);
    ed.setPalette(p->id());
    ed.m_macroRow->setVisible(true);        // the row that collided with the button
    ed.m_whiteRow->setVisible(true);
    ed.m_amberRow->setVisible(true);
    ed.m_uvRow->setVisible(true);
    ed.show();

    // The editor's minimum, a laptop, full HD and very wide (Q207).
    for (int w : { 200, 400, 500, 600, 700, 800, 900, 1000, 1100, 1280, 1600, 1920, 3000 })
    {
        for (int h : { 420, 500, 600, 800, 900, 1080 })
        {
            ed.resize(w, h);
            QCoreApplication::processEvents();
            ed.layout()->activate();
            const QString at = QStringLiteral(" at %1x%2").arg(ed.width()).arg(ed.height());

            // Everything is compared in the editor's coordinates.
            auto rectOf = [&](QWidget *x) {
                return QRect(x->mapTo(&ed, QPoint(0, 0)), x->size());
            };
            struct Item { QString name; QWidget *w; };
            QList<Item> items;
            auto add = [&](const QString &n, QWidget *x) {
                QVERIFY2(x != nullptr && x->isVisibleTo(&ed), qPrintable(n + " visible" + at));
                items << Item{ n, x };
            };
            add("dialog", ed.m_colorDialog);
            for (QSlider *s : { ed.m_redSlider, ed.m_greenSlider, ed.m_blueSlider,
                                ed.m_whiteSlider, ed.m_amberSlider, ed.m_uvSlider })
            {
                add("slider", s);
                // A slider you can work: tall and not squeezed flat.
                QVERIFY2(s->height() >= 60, qPrintable("slider height" + at));
                QVERIFY2(s->width() >= 10, qPrintable("slider width" + at));
                // The numbered label above it is part of its column.
                add("column", s->parentWidget());
            }
            add("macro row", ed.m_macroRow);
            add("name", ed.m_colourName);

            // Too small for the page: it scrolls, rather than overlapping.
            QScrollArea *sa = ed.m_colourScroll;
            if (ed.m_colourBody->width() > sa->viewport()->width())
                QVERIFY2(!sa->horizontalScrollBar()->isHidden(), qPrintable("h-scroll" + at));
            if (ed.m_colourBody->height() > sa->viewport()->height())
                QVERIFY2(!sa->verticalScrollBar()->isHidden(), qPrintable("v-scroll" + at));
            // Narrow: the sliders are under the picker, wide: beside it.
            if (sa->viewport()->width() >= 900)
                QVERIFY2(ed.m_redSlider->mapTo(&ed, QPoint(0, 0)).x() >= rectOf(ed.m_colorDialog).right(),
                         qPrintable("sliders beside the picker" + at));

            // The sliders' columns sit beside the dialog, never over it.
            for (int i = 0; i < items.size(); ++i)
            {
                QVERIFY2(rectOf(ed.m_colourBody).contains(rectOf(items[i].w)),
                         qPrintable(items[i].name + " inside the page" + at));
                for (int j = i + 1; j < items.size(); ++j)
                {
                    if (items[i].w->isAncestorOf(items[j].w) || items[j].w->isAncestorOf(items[i].w))
                        continue;
                    QVERIFY2(!rectOf(items[i].w).intersects(rectOf(items[j].w)),
                             qPrintable(items[i].name + " overlaps " + items[j].name + at));
                }
            }

            // Inside the dialog: grid, button, field and brightness bar each
            // have their own room, all within the dialog.
            QList<QWidget *> inner;
            for (QWidget *k : ed.m_colorDialog->findChildren<QWidget *>())
            {
                const QByteArray cls = k->metaObject()->className();
                if (cls.endsWith("QColorWell") || cls.endsWith("QWellArray") || cls.endsWith("QColorPicker")
                    || cls.endsWith("QColorLuminancePicker")
                    || (cls == "QPushButton" && k->isVisibleTo(&ed)))
                    inner << k;
            }
            QVERIFY2(inner.size() >= 4, qPrintable("dialog parts found" + at));
            for (int i = 0; i < inner.size(); ++i)
            {
                QVERIFY2(rectOf(ed.m_colorDialog).contains(rectOf(inner[i])),
                         qPrintable(QString(inner[i]->metaObject()->className())
                                    + " inside the dialog" + at));
                for (int j = i + 1; j < inner.size(); ++j)
                    QVERIFY2(!rectOf(inner[i]).intersects(rectOf(inner[j])),
                             qPrintable(QString(inner[i]->metaObject()->className()) + " overlaps "
                                        + inner[j]->metaObject()->className() + at));
            }
        }
    }
}

void LookEditor_Test::colourSwatchesAreTheStageColours()
{
    const QList<PaletteNaming::StageColour> &all = PaletteNaming::stageColours();
    QCOMPARE(all.size(), PaletteNaming::StageColourCount);
    QSet<QString> names;
    for (const PaletteNaming::StageColour &c : all)
    {
        QVERIFY(c.rgb.isValid());
        QVERIFY2(!names.contains(c.name), qPrintable(c.name));
        names.insert(c.name);
    }
    for (const char *want : { "Flesh", "Pale Skin", "Warm Skin", "Deep Skin", "Blacklight",
                              "Congo Blue", "Cool Fluorescent", "Warm Fluorescent",
                              "Daylight 5600K", "Tungsten 3200K", "Candle", "Sodium Vapour",
                              "Mercury Vapour", "Moonlight", "Lavender", "Amber", "Straw",
                              "Bastard Amber", "Surprise Pink", "Red", "Green", "Blue" })
        QVERIFY2(names.contains(QString::fromLatin1(want)), want);

    // Course lesson 2.2 clicks the violet at row 3, column 2.
    QCOMPARE(all.at(17).rgb, QColor(0xaa, 0x00, 0xff));

    // The dialog's grid is this list, and hovering swatch N says its name.
    QColorDialog dlg;
    dlg.setOptions(QColorDialog::NoButtons | QColorDialog::DontUseNativeDialog);
    StageColourPicker *pick = StageColourPicker::install(&dlg);
    dlg.show();
    QVERIFY(pick->swatchGrid() != nullptr);
    for (int i = 0; i < all.size(); ++i)
        QCOMPARE(QColorDialog::standardColor((i / 8) + (i % 8) * 6), all.at(i).rgb);

    QWidget *grid = pick->swatchGrid();
    // A click on a swatch picks it (lesson 2.2's click: column 2, row 3).
    QTest::mouseClick(grid, Qt::LeftButton, Qt::NoModifier,
                      QPoint(grid->width() * 3 / 16, grid->height() * 5 / 12));
    QCOMPARE(dlg.currentColor(), QColor(0xaa, 0x00, 0xff));
    for (int i = 0; i < all.size(); ++i)
    {
        const int col = i % 8, row = i / 8;
        const QPoint at((col * 2 + 1) * grid->width() / 16, (row * 2 + 1) * grid->height() / 12);
        QCOMPARE(pick->swatchAt(at), i);
        QHelpEvent he(QEvent::ToolTip, at, grid->mapToGlobal(at));
        QVERIFY(QCoreApplication::sendEvent(grid, &he));
        QVERIFY2(QToolTip::isVisible(), qPrintable(all.at(i).name));
        QVERIFY2(QToolTip::text().startsWith(all.at(i).name)
                     && QToolTip::text().contains(all.at(i).rgb.name()),
                 qPrintable(QToolTip::text()));
    }
}

void LookEditor_Test::hoveringTheFieldNamesTheNearestColour()
{
    bool exact = false;
    QCOMPARE(PaletteNaming::stageColourName(QColor(255, 205, 170), &exact), QString("Flesh"));
    QVERIFY(exact);
    QCOMPARE(PaletteNaming::stageColourName(QColor(252, 200, 168), &exact), QString("Flesh"));
    QVERIFY(!exact);
    QCOMPARE(PaletteNaming::stageColourName(QColor(0, 0, 4)), QString("Black"));
    QCOMPARE(PaletteNaming::stageColourName(QColor(250, 10, 5)), QString("Red"));
    QVERIFY(StageColourPicker::describe(QColor(252, 200, 168)).contains("Flesh"));
    QVERIFY(StageColourPicker::describe(QColor(252, 200, 168)).contains("#fcc8a8"));

    QColorDialog dlg;
    dlg.setOptions(QColorDialog::NoButtons | QColorDialog::DontUseNativeDialog);
    StageColourPicker *pick = StageColourPicker::install(&dlg);
    dlg.show();
    QWidget *field = pick->colourField();
    QVERIFY(field != nullptr);
    const QPoint at(field->width() / 2, field->height() / 2);
    QHelpEvent he(QEvent::ToolTip, at, field->mapToGlobal(at));
    QVERIFY(QCoreApplication::sendEvent(field, &he));
    QVERIFY2(QToolTip::isVisible(), "the field names the colour under the pointer");
    QVERIFY2(QToolTip::text().contains("#"), qPrintable(QToolTip::text()));
}

void LookEditor_Test::blacklightSwatchDrivesUv()
{
    Doc doc(this);
    QLCPalette *p = new QLCPalette(QLCPalette::Color, &doc);
    p->setName("Look");
    p->setValue("#ffffff000000");
    QVERIFY(doc.addPalette(p));
    LookEditor ed(&doc);
    ed.setPalette(p->id());

    QColor bl;
    for (const PaletteNaming::StageColour &c : PaletteNaming::stageColours())
        if (c.name == "Blacklight")
            bl = c.rgb;
    QVERIFY(bl.isValid());
    ed.m_colorDialog->setCurrentColor(bl);
    QCOMPARE(ed.m_uvSlider->value(), 255);
    QCOMPARE(p->wauvValue().blue(), 255);
    QVERIFY(ed.m_colourName->text().startsWith("Blacklight"));

    // Any other colour puts UV back to 0.
    ed.m_colorDialog->setCurrentColor(QColor(255, 205, 170));
    QCOMPARE(ed.m_uvSlider->value(), 0);
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

void LookEditor_Test::aColourLookCanPickARingSlot()
{
    Doc doc(this);
    QDir dir(TEST_FIXTURE_DIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(doc.fixtureDefCache()->load(dir));
    QLCFixtureDef *def = doc.fixtureDefCache()->fixtureDef("Branson", "LED Movinghead+Circle (2 heads)");
    QVERIFY(def != nullptr);

    QLCPalette *look = new QLCPalette(QLCPalette::Color, &doc);
    look->setName("New Color");
    look->setValue(QLCPalette::colorToString(QColor(Qt::blue), QColor()));
    QVERIFY(doc.addPalette(look));

    // No ring anywhere: no chooser.
    LookEditor ed(&doc);
    ed.setPalette(look->id());
    QVERIFY(ed.m_macroRow->isHidden());

    Fixture *f = new Fixture(&doc);
    f->setFixtureDefinition(def, def->mode("12 Channel"));
    QVERIFY(doc.addFixture(f));
    ed.setPalette(QLCPalette::invalidId());
    ed.setPalette(look->id());
    QVERIFY(!ed.m_macroRow->isHidden());
    QCOMPARE(ed.m_macroCombo->currentIndex(), 0);           // nearest to the colour
    QList<QColor> colours;
    const QStringList names = ed.colourSlotNames(&colours);
    QVERIFY(names.contains("off"));
    QVERIFY(names.contains("Red/Yellow-Chase"));
    QVERIFY(names.contains("Blue/Purple"));                 // the wheel's split
    QCOMPARE(names.count(QStringLiteral("Blue")), 1);       // wheel and ring: once
    QVERIFY(!colours.at(names.indexOf("Red/Yellow-Chase")).isValid());

    // A chase: the look names it, and is named after it.
    const int chase = ed.m_macroCombo->findData("Red/Yellow-Chase");
    QVERIFY(chase > 0);
    ed.m_macroCombo->setCurrentIndex(chase);
    emit ed.m_macroCombo->activated(chase);
    QCOMPARE(look->colourMacro(), QString("Red/Yellow-Chase"));
    QCOMPARE(look->name(), QString("Red/Yellow-Chase"));
    QCOMPARE(look->rgbValue(), QColor(Qt::blue));           // the colour stays

    // A colour slot sets the picker too.
    const int yellow = ed.m_macroCombo->findData("Yellow");
    ed.m_macroCombo->setCurrentIndex(yellow);
    emit ed.m_macroCombo->activated(yellow);
    QCOMPARE(look->colourMacro(), QString("Yellow"));
    QCOMPARE(look->rgbValue(), QColor(Qt::yellow));

    // Back to the colour alone.
    ed.m_macroCombo->setCurrentIndex(0);
    emit ed.m_macroCombo->activated(0);
    QVERIFY(look->colourMacro().isEmpty());

    // Shown as it is when opened again.
    look->setColourMacro("RGB/Random-Chase");
    ed.setPalette(QLCPalette::invalidId());
    ed.setPalette(look->id());
    QCOMPARE(ed.m_macroCombo->currentData().toString(), QString("RGB/Random-Chase"));
}

void LookEditor_Test::anAimLookCanDimLightsThatCannotSeeItsTarget()
{
    Doc doc(this);
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, &doc);
    aim->setName("At the vocalist");
    QVERIFY(doc.addPalette(aim));

    LookEditor ed(&doc);
    ed.setPalette(aim->id());
    QCheckBox *dim = ed.findChild<QCheckBox *>("aimDimWhenBlocked");
    QVERIFY(dim != nullptr);
    // Off unless asked for: an existing show never changes.
    QVERIFY(!dim->isChecked());
    QVERIFY(!aim->dimWhenBlocked());
    dim->setChecked(true);
    QVERIFY(aim->dimWhenBlocked());

    // Opened again, it shows what the look says.
    LookEditor again(&doc);
    again.setPalette(aim->id());
    QVERIFY(again.findChild<QCheckBox *>("aimDimWhenBlocked")->isChecked());
    again.findChild<QCheckBox *>("aimDimWhenBlocked")->setChecked(false);
    QVERIFY(!aim->dimWhenBlocked());
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

/* A shutter like the AT-Spots': Closed 0-7, Open 8-15, strobe 16-255 --
   or, with @p strobeOnly, a plain strobe channel (0 open, the rest strobe). */
static QLCFixtureDef *shutterDef(const QString &model, bool strobeOnly)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel(model);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("1ch");
    QLCChannel *ch = new QLCChannel();
    ch->setName("Shutter");
    if (strobeOnly)
    {
        ch->setPreset(QLCChannel::ShutterStrobeSlowFast);
    }
    else
    {
        ch->setGroup(QLCChannel::Shutter);
        QLCCapability *closed = new QLCCapability(0, 7, "Closed");
        closed->setPreset(QLCCapability::ShutterClose);
        QLCCapability *open = new QLCCapability(8, 15, "Open");
        open->setPreset(QLCCapability::ShutterOpen);
        QLCCapability *strobe = new QLCCapability(16, 255, "Strobe slow - fast");
        strobe->setPreset(QLCCapability::StrobeSlowToFast);
        ch->addCapability(closed);
        ch->addCapability(open);
        ch->addCapability(strobe);
    }
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    def->addMode(mode);
    return def;
}

void LookEditor_Test::aShutterLookOffersClosed()
{
    QLCFixtureDef *spot = shutterDef("Spot", false);
    QLCFixtureDef *mover = shutterDef("Mover", true);
    {
        Doc doc(this);
        QVERIFY(addOf(doc, mover) != nullptr);
        QVERIFY(addOf(doc, mover) != nullptr);
        Fixture *s = addOf(doc, spot);
        QVERIFY(s != nullptr);
        QLCPalette *sh = new QLCPalette(QLCPalette::Shutter, &doc);
        QVERIFY(doc.addPalette(sh));

        // The strobe-only movers are not a source, though more numerous.
        LookEditor ed(&doc);
        ed.setPalette(sh->id());
        QVERIFY(ed.m_singleSourceCombo->isHidden());
        QCOMPARE(itemsOf(ed.m_singleCombo), QStringList({ "Closed", "Open", "Strobe slow - fast" }));
        // A new look opens, and says so -- not "Closed", what sits at 0.
        QCOMPARE(ed.m_singleCombo->currentText(), QString("Open"));
        QCOMPARE(sh->shutterIntent(&doc, { s->id() }), QLCPalette::ShutterIntentOpen);

        // Closed is stored as a value that closes, and is sent as such.
        ed.slotSingleCapabilityPicked(0);
        QCOMPARE(sh->intValue1(), 3);
        QCOMPARE(sh->shutterIntent(&doc, { s->id() }), QLCPalette::ShutterIntentClosed);
        QCOMPARE(sh->valuesFromFixtures(&doc, { s->id() }).first().value, uchar(3));

        // Dragged to 0, still Closed: 0 is an open look's.
        ed.m_singleSlider->setValue(0);
        QCOMPARE(sh->intValue1(), 1);
        QCOMPARE(sh->shutterIntent(&doc, { s->id() }), QLCPalette::ShutterIntentClosed);
    }
    delete spot;
    delete mover;
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

void LookEditor_Test::aSwappedSlotShowsItsOwnGlass()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    GoboLibrary::setUserDirectoryOverride(dir.filePath("Gobos"));
    QImage logo(40, 40, QImage::Format_ARGB32);
    logo.fill(Qt::black);
    QVERIFY(logo.save(GoboLibrary::userDirectory().absoluteFilePath("our-logo.png")));

    QLCFixtureDef *spot = wheelDef("Spot", { "Open", "Star", "Ring" });
    {
        Doc doc(this);
        Fixture *f = addOf(doc, spot);
        QVERIFY(f != nullptr);
        doc.monitorProperties()->setGoboSwap(f->id(), 0, 20, "our-logo.png");   // Star's slot
        QLCPalette *gobo = new QLCPalette(QLCPalette::Gobo, &doc);
        gobo->setValue(29);                                                     // Star
        QVERIFY(doc.addPalette(gobo));

        LookEditor ed(&doc);
        ed.setPalette(gobo->id());
        QCOMPARE(itemsOf(ed.m_singleCombo), QStringList({ "Open", "Star — our-logo", "Ring" }));
        QVERIFY(!ed.m_singleCombo->itemIcon(1).isNull());
        QCOMPARE(ed.m_singleCombo->currentIndex(), 1);
        // The big preview shows the fitted glass too.
        QVERIFY(!ed.m_singlePreview->isHidden());
        QVERIFY(!ed.m_singlePreview->pixmap().isNull());

        // A missing picture falls back to the definition's slot.
        doc.monitorProperties()->setGoboSwap(f->id(), 0, 20, "gone.png");
        ed.setPalette(gobo->id());
        QCOMPARE(ed.m_singleCombo->itemText(1), QString("Star"));
    }
    delete spot;
    GoboLibrary::setUserDirectoryOverride(QString());
}

void LookEditor_Test::effectsArePickedFromABrowserWithWledsOwn()
{
    Doc doc(this);
    doc.effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    // A WLED lantern in the show: its firmware's effects are offered.
    QLCFixtureDef *def = new QLCFixtureDef();
    QCOMPARE(def->loadXML(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/WLED/WLED-Lantern.qxf"),
             QFile::NoError);
    Fixture *lantern = new Fixture(&doc);
    lantern->setFixtureDefinition(def, def->mode("Effect (15ch)"));
    QVERIFY(doc.addFixture(lantern));

    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("New Effect");
    QVERIFY(doc.addPalette(look));
    LookEditor ed(&doc);
    ed.setPalette(look->id());
    EffectBrowser *browser = ed.m_effectBrowser;
    QVERIFY(browser != nullptr);
    QVERIFY(!ed.m_effectScriptCombo->isVisible());      // the browser, not a dropdown

    const QStringList tabs = browser->tabNames();
    QCOMPARE(tabs.first(), QString("Recent"));
    QVERIFY(tabs.contains("Color"));
    QCOMPARE(tabs.last(), QString("WLED"));

    // A row by its name -- and, where names repeat (an RGB Matrix "Fireworks"
    // beside WLED's), by its category.
    auto rowNamed = [browser](const QString &label, const QString &category = QString()) -> QListWidgetItem * {
        for (int i = 0; i < browser->list()->count(); ++i)
        {
            QListWidgetItem *it = browser->list()->item(i);
            const int e = it->data(Qt::UserRole + 10).toInt();
            if (it->text() == label
                && (category.isEmpty() || browser->m_entries.at(e).category == category))
                return it;
        }
        return nullptr;
    };

    // Search finds it wherever it lives: WLED's Chase and the port's generator.
    browser->search()->setText("chase");
    QVERIFY(rowNamed("Chase", "WLED") != nullptr);
    QVERIFY(rowNamed("WLED Chase") != nullptr);
    QVERIFY(rowNamed("Breathe") == nullptr);

    // Picking WLED's Chase: the lantern runs it, everything else its port.
    QListWidgetItem *chase = rowNamed("Chase", "WLED");
    browser->list()->setCurrentItem(chase);
    emit browser->list()->itemClicked(chase);
    QCOMPARE(look->wledEffect(), QString("Chase"));
    QVERIFY(look->scriptPath().endsWith("wled-chase.js"));
    QCOMPARE(look->name(), QString("WLED Chase"));
    QCOMPARE(EffectBrowser::recent().first(), QString("3:Chase"));

    // One with no port (a 2D one): the device runs it; the look gets WLED's controls.
    browser->search()->setText("game of life");
    QListWidgetItem *life = rowNamed("Game Of Life", "WLED");
    QVERIFY(life != nullptr);
    emit browser->list()->itemClicked(life);
    QCOMPARE(look->wledEffect(), QString("Game Of Life"));
    QVERIFY(look->scriptPath().endsWith("wled-device.js"));

    // An ordinary effect is not a WLED one.
    browser->search()->setText("breathe");
    QListWidgetItem *breathe = rowNamed("WLED Breathe");
    QVERIFY(breathe != nullptr);
    emit browser->list()->itemClicked(breathe);
    QVERIFY(look->wledEffect().isEmpty());
    QVERIFY(look->scriptPath().endsWith("wled-breathe.js"));

    // The stand-in is never listed.
    browser->search()->setText("device effect");
    QVERIFY(rowNamed("WLED device effect") == nullptr);

    // Previews run: a visible row has a frame of sixteen lights.
    browser->search()->setText("WLED Chase");
    browser->resize(500, 400);
    browser->show();
    QCoreApplication::processEvents();
    browser->tickPreviews();
    QCOMPARE(browser->m_frames.value(rowNamed("WLED Chase")->data(Qt::UserRole + 11).toString()).lights.size(), 16);
    if (!qEnvironmentVariableIsEmpty("BROWSER_PNG"))
    {
        browser->search()->clear();
        const QString tab = qEnvironmentVariableIsEmpty("BROWSER_TAB")
                                ? QStringLiteral("Color") : qEnvironmentVariable("BROWSER_TAB");
        browser->tabs()->setCurrentIndex(browser->tabNames().indexOf(tab));
        browser->setParent(nullptr);
        browser->resize(560, 760);
        browser->show();
        QCoreApplication::processEvents();
        for (int k = 0; k < 32; ++k)            // a position effect's trail fills in
        {
            QTest::qWait(80);
            browser->tickPreviews();
        }
        browser->grab().save(qEnvironmentVariable("BROWSER_PNG"));
        browser->setParent(&ed);
    }
}

/* "position based effects shouldn't show matrix lines but show what the
   effect does": a position effect's preview is where the heads point and the
   shape they trace; a gobo, iris or zoom one is a row of beams. */
void LookEditor_Test::previewsShowWhatEachEffectDoes()
{
    Doc doc(this);
    doc.effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("Look");
    QVERIFY(doc.addPalette(look));
    LookEditor ed(&doc);
    ed.setPalette(look->id());
    EffectBrowser *browser = ed.m_effectBrowser;
    QVERIFY(browser != nullptr);
    EffectScriptCache *cache = doc.effectScriptRunner()->cache();

    const EffectBrowser::Preview diamond = browser->previewFrame(cache->scriptPath("Diamond"));
    QCOMPARE(diamond.kind, EffectBrowser::Preview::Position);
    QCOMPARE(diamond.spots.size(), 16);
    const QVector<QVector<QPointF> > shape = browser->samplePaths(cache->scriptPath("Diamond"));
    QVERIFY(!shape.isEmpty());
    QRectF seen;
    foreach (const QPointF &q, shape.at(0))
        seen = seen.isNull() ? QRectF(q, QSizeF(1e-9, 1e-9)) : seen.united(QRectF(q, QSizeF(1e-9, 1e-9)));
    QVERIFY2(seen.width() > 0.01 && seen.height() > 0.01, "a diamond moves both ways");

    QCOMPARE(browser->previewFrame(cache->scriptPath("Stage Shape")).kind,
             EffectBrowser::Preview::Position);       // aims at the look's target
    QCOMPARE(browser->previewFrame(cache->scriptPath("Gobo Chase")).kind, EffectBrowser::Preview::Gobo);
    QCOMPARE(browser->previewFrame(cache->scriptPath("Iris Pulse")).kind, EffectBrowser::Preview::Beam);
    QCOMPARE(browser->previewFrame(cache->scriptPath("Zoom Pulse")).kind, EffectBrowser::Preview::Beam);

    // Free Spin: the heads go round -- a position preview whose pan keeps
    // moving one way (a spin, not a sweep).
    const QString spin = cache->scriptPath("Free Spin");
    QVERIFY(!spin.isEmpty());
    QCOMPARE(browser->previewFrame(spin).kind, EffectBrowser::Preview::Position);
    const QVector<QVector<QPointF> > turns = browser->samplePaths(spin);
    QVERIFY(!turns.isEmpty() && turns.at(0).size() > 2);
    QVERIFY2(turns.at(0).last().x() != turns.at(0).first().x(), "a spinning head turns");

    // A follow-spot does nothing held still: it is previewed steered.
    EffectBrowser::Preview follow;
    for (int k = 0; k < 3 && follow.kind != EffectBrowser::Preview::Position; ++k)
        follow = browser->previewFrame(cache->scriptPath("Followspot"));
    QCOMPARE(follow.kind, EffectBrowser::Preview::Position);

    // Amber lights in amber, not dark.
    const EffectBrowser::Preview amber = browser->previewFrame(cache->scriptPath("Amber Pulse"));
    QCOMPARE(amber.kind, EffectBrowser::Preview::Lights);
    int lit = 0;
    foreach (const QColor &c, amber.lights)
        lit += (c.red() > 60 && c.blue() < c.red()) ? 1 : 0;
    QVERIFY2(lit > 0, "amber pulse should light amber");

    // A matrix effect is shown on a pixel box -- RGB rows and white rows --
    // and a strobe hit lights the white rows too.
    const EffectBrowser::Preview bars = browser->previewFrame(cache->scriptPath("Matrix Bars"));
    QCOMPARE(bars.kind, EffectBrowser::Preview::Matrix);
    QCOMPARE(bars.cols * bars.rows, bars.lights.size());
    QVERIFY(bars.rows > 1);
    const QString strobePath = cache->scriptPath("Matrix Strobe");
    QCOMPARE(browser->previewFrame(strobePath).kind, EffectBrowser::Preview::Matrix);
    QVERIFY(!browser->m_runs.value(strobePath).isNull());
    bool whiteRowLit = false;
    for (int k = 0; k < 40 && !whiteRowLit; ++k)
    {
        // Row 1 is a white row; a hit is a few hundredths of a second in.
        const EffectBrowser::Preview strobe = browser->previewWith(browser->m_runs.value(strobePath).data(),
                                                                   k * 0.05);
        QCOMPARE(strobe.lights.size(), strobe.cols * strobe.rows);
        for (int x = 0; x < strobe.cols; ++x)
            whiteRowLit = whiteRowLit || strobe.lights.at(strobe.cols + x).red() > 200;
    }
    QVERIFY2(whiteRowLit, "a strobe hit lights the box's white rows");
}

/* An effect with words to show (the Text effect) gets a box to type them in,
   showing the script's own words to start with; what is typed is the look's. */
void LookEditor_Test::anEffectsTextIsTypedIn()
{
    Doc doc(this);
    doc.effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("Sign");
    QVERIFY(doc.addPalette(look));
    LookEditor ed(&doc);
    ed.resize(1000, 640);
    ed.show();
    ed.setPalette(look->id());
    EffectBrowser *browser = ed.m_effectBrowser;
    browser->search()->setText("Text");             // whichever tab it is under
    QApplication::processEvents();
    QListWidgetItem *textItem = nullptr;
    for (int i = 0; i < browser->list()->count() && textItem == nullptr; ++i)
        if (browser->list()->item(i)->text() == QLatin1String("Text"))
            textItem = browser->list()->item(i);
    QVERIFY2(textItem != nullptr, "the Text effect should be in the list");
    browser->pick(textItem);
    QApplication::processEvents();
    QVERIFY(look->scriptPath().endsWith("text.js"));

    QLineEdit *box = ed.m_effectDynWidget->findChild<QLineEdit *>("effectText_text");
    QVERIFY2(box != nullptr, "a text setting should be a box to type in");
    QCOMPARE(box->text(), QString("HELLO"));
    box->setText("SHOW TIME");
    QCOMPARE(look->effectStringParam("text"), QString("SHOW TIME"));
}

/* Q95: New script… and Edit script… are one Script ▾ on the effect page,
   not two buttons beside the effect. */
void LookEditor_Test::scriptsAreUnderOneScriptButton()
{
    Doc doc(this);
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("Sign");
    QVERIFY(doc.addPalette(look));
    LookEditor ed(&doc);
    ed.resize(1000, 640);
    ed.show();
    ed.setPalette(look->id());

    QToolButton *script = ed.findChild<QToolButton *>("lookEditorScript");
    QVERIFY(script != nullptr);
    QCOMPARE(script->text(), QString("Script"));
    QCOMPARE(script->popupMode(), QToolButton::InstantPopup);
    QVERIFY(script->menu() != nullptr);
    QCOMPARE(script->menu()->actions(), QList<QAction *>({ ed.m_newScriptButton, ed.m_editScriptButton }));
    QCOMPARE(ed.m_newScriptButton->text(), QString::fromUtf8("New Script…"));
    QCOMPARE(ed.m_editScriptButton->text(), QString::fromUtf8("Edit Script…"));
    for (QAbstractButton *b : ed.findChildren<QAbstractButton *>())
        QVERIFY2(!b->text().contains("script…"), qPrintable(b->text()));
}

/* Matrix Image's picture: what it is, shown beside the buttons to choose
   one or clear it; kept in the look itself (MatrixImage). */
void LookEditor_Test::anEffectsPictureIsShownAndCleared()
{
    Doc doc(this);
    doc.effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("Logo");
    QVERIFY(doc.addPalette(look));
    LookEditor ed(&doc);
    ed.resize(1000, 640);
    ed.show();
    ed.setPalette(look->id());
    EffectBrowser *browser = ed.m_effectBrowser;
    browser->search()->setText("Matrix Image");
    QApplication::processEvents();
    QListWidgetItem *item = nullptr;
    for (int i = 0; i < browser->list()->count() && item == nullptr; ++i)
        if (browser->list()->item(i)->text() == QLatin1String("Matrix Image"))
            item = browser->list()->item(i);
    QVERIFY2(item != nullptr, "Matrix Image should be in the list");
    browser->pick(item);
    QApplication::processEvents();
    QVERIFY(look->scriptPath().endsWith("matrix-image.js"));

    QLabel *what = ed.m_effectDynWidget->findChild<QLabel *>("effectImageLabel_image");
    QPushButton *clear = ed.m_effectDynWidget->findChild<QPushButton *>("effectImageClear_image");
    QVERIFY2(what != nullptr && clear != nullptr, "a picture setting should say what it is, with buttons");
    QVERIFY(ed.m_effectDynWidget->findChild<QPushButton *>("effectImageChoose_image") != nullptr);
    QVERIFY(what->text().contains("No picture"));
    QVERIFY(!clear->isEnabled());

    // A picture in the look: its name, size and frames.
    const QByteArray gif = QByteArray::fromBase64(
        "R0lGODlhBAACAIEAAP8AAAAAAAAAAAAAACH/C05FVFNDQVBFMi4wAwEAAAAh+QQACgAAACwAAAAABAACAAAIBwABCBwoMCAAIfkEARQAAQAs"
        "AAAAAAQAAgCBAP8AAAAAAAAAAAAACAcAAQgcKDAgACH5BAEeAAEALAAAAAAEAAIAgQAA/wAAAAAAAAAAAAgHAAEIHCgwIAA7");
    QLCPalette *logo = new QLCPalette(QLCPalette::Effect, &doc);
    logo->setName("Logo 2");
    logo->setScriptPath(look->scriptPath());
    logo->setEffectStringParam("image", MatrixImage::fromBytes(gif, "tiny.gif"));
    QVERIFY(doc.addPalette(logo));
    ed.setPalette(logo->id());
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    look = logo;
    what = ed.m_effectDynWidget->findChild<QLabel *>("effectImageLabel_image");
    clear = ed.m_effectDynWidget->findChild<QPushButton *>("effectImageClear_image");
    QVERIFY(what != nullptr && clear != nullptr);
    QVERIFY2(what->text().contains("tiny.gif") && what->text().contains("3 frames"), qPrintable(what->text()));
    QVERIFY(clear->isEnabled());

    clear->click();
    QVERIFY(look->effectStringParam("image").isEmpty());
    QVERIFY(what->text().contains("No picture"));
}

/* "Speed:" puts the effect on the main clock or a speed master; its rate then
   reads per beat. */
void LookEditor_Test::anEffectIsPutOnASpeedMaster()
{
    Doc doc(this);
    doc.effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("Phase");
    QVERIFY(doc.addPalette(look));
    LookEditor ed(&doc);
    ed.resize(1000, 640);
    ed.show();
    ed.setPalette(look->id());
    EffectBrowser *browser = ed.m_effectBrowser;
    browser->search()->setText("Dimmer Phaser");
    QApplication::processEvents();
    QListWidgetItem *item = nullptr;
    for (int i = 0; i < browser->list()->count() && item == nullptr; ++i)
        if (browser->list()->item(i)->text() == QLatin1String("Dimmer Phaser"))
            item = browser->list()->item(i);
    QVERIFY(item != nullptr);
    browser->pick(item);
    QApplication::processEvents();

    auto perBeatLabel = [&]() {
        foreach (QLabel *l, ed.m_effectDynWidget->findChildren<QLabel *>())
            if (l->text().contains("(per beat)"))
                return true;
        return false;
    };
    QComboBox *speed = ed.m_effectDynWidget->findChild<QComboBox *>("effectSpeedMaster");
    QVERIFY(speed != nullptr);
    QCOMPARE(speed->currentData().toInt(), int(SpeedMasters::Inherit));
    QVERIFY(!perBeatLabel());

    speed->setCurrentIndex(speed->findData(SpeedMasters::Master1 + 1));
    QCOMPARE(int(look->effectParamValues().value("__speedMaster")), int(SpeedMasters::Master1 + 1));
    QTRY_VERIFY(perBeatLabel());                    // rebuilt: rate per beat

    speed = ed.m_effectDynWidget->findChild<QComboBox *>("effectSpeedMaster");
    QVERIFY(speed != nullptr);
    QCOMPARE(speed->currentData().toInt(), int(SpeedMasters::Master1 + 1));
    speed->setCurrentIndex(speed->findData(SpeedMasters::Free));
    QTRY_VERIFY(!perBeatLabel());
}

/* "when selecting own speed on an effect there should be a setting for it":
   Confetti has no speed of its own; Own speed shows a rate (×) that is kept
   in the look, goes on a master and comes back, and survives trying
   another effect. */
void LookEditor_Test::ownSpeedHasASetting()
{
    Doc doc(this);
    doc.effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("Sparkle");
    QVERIFY(doc.addPalette(look));
    LookEditor ed(&doc);
    ed.resize(1000, 640);
    ed.show();
    ed.setPalette(look->id());
    EffectBrowser *browser = ed.m_effectBrowser;
    auto itemNamed = [browser](const QString &name) -> QListWidgetItem * {
        browser->search()->setText(name);
        QApplication::processEvents();
        for (int i = 0; i < browser->list()->count(); ++i)
            if (browser->list()->item(i)->text() == name)
                return browser->list()->item(i);
        return nullptr;
    };
    QListWidgetItem *item = itemNamed("Confetti");
    QVERIFY(item != nullptr);
    browser->pick(item);
    QApplication::processEvents();

    QComboBox *speed = ed.m_effectDynWidget->findChild<QComboBox *>("effectSpeedMaster");
    QVERIFY(speed != nullptr);
    speed->setCurrentIndex(speed->findData(SpeedMasters::Free));
    QDoubleSpinBox *own = nullptr;
    QTRY_VERIFY((own = ed.m_effectDynWidget->findChild<QDoubleSpinBox *>("effectOwnRate")) != nullptr);
    QVERIFY(own->isVisibleTo(&ed));
    QCOMPARE(own->value(), 1.0);
    own->setValue(2.5);
    QCOMPARE(look->effectParamValues().value("__ownRate"), 2.5);

    // On a master the master sets the pace: no own rate.
    speed->setCurrentIndex(speed->findData(SpeedMasters::MainClock));
    QTRY_VERIFY(ed.m_effectDynWidget->findChild<QDoubleSpinBox *>("effectOwnRate") == nullptr);
    speed = ed.m_effectDynWidget->findChild<QComboBox *>("effectSpeedMaster");
    speed->setCurrentIndex(speed->findData(SpeedMasters::Free));
    QTRY_VERIFY((own = ed.m_effectDynWidget->findChild<QDoubleSpinBox *>("effectOwnRate")) != nullptr);
    QCOMPARE(own->value(), 2.5);

    // Trying another effect keeps the look's own speed.
    item = itemNamed("Dimmer Phaser");
    QVERIFY(item != nullptr);
    browser->pick(item);
    QApplication::processEvents();
    QCOMPARE(look->effectParamValues().value("__ownRate"), 2.5);
}

/* "when I set something on the speed masters .. and then double click on
   the icon it resets those": the look's Speed and Spread are the look's,
   not the effect's -- double-clicking the effect's icon in the picker, or
   the look itself, or trying another effect, must keep them. */
void LookEditor_Test::aLooksSpeedMasterAndSpreadSurviveADoubleClick()
{
    Doc doc(this);
    doc.effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("Phase");
    QVERIFY(doc.addPalette(look));
    LookEditor ed(&doc);
    ed.resize(1000, 640);
    ed.show();
    ed.setPalette(look->id());
    EffectBrowser *browser = ed.m_effectBrowser;
    auto itemNamed = [browser](const QString &name) -> QListWidgetItem * {
        browser->search()->setText(name);
        QApplication::processEvents();
        for (int i = 0; i < browser->list()->count(); ++i)
            if (browser->list()->item(i)->text() == name)
                return browser->list()->item(i);
        return nullptr;
    };
    QListWidgetItem *item = itemNamed("Dimmer Phaser");
    QVERIFY(item != nullptr);
    browser->pick(item);
    QApplication::processEvents();

    QComboBox *speed = ed.m_effectDynWidget->findChild<QComboBox *>("effectSpeedMaster");
    QVERIFY(speed != nullptr);
    speed->setCurrentIndex(speed->findData(SpeedMasters::Master1 + 1));
    QApplication::processEvents();
    look->setEffectParamValue(QStringLiteral("__spreadAmount"), 50);
    look->setEffectParamValue(QStringLiteral("__spreadWings"), 2);
    auto kept = [look]() {
        return int(look->effectParamValues().value("__speedMaster")) == int(SpeedMasters::Master1 + 1)
            && look->effectParamValues().value("__spreadAmount") == 50.0
            && look->effectParamValues().value("__spreadWings") == 2.0;
    };
    QVERIFY(kept());

    // The look double-clicked (it opens in the editor again).
    ed.setPalette(look->id());
    QApplication::processEvents();
    QVERIFY(kept());

    // Its effect's icon double-clicked in the picker.
    ed.openEffectPicker();
    item = itemNamed("Dimmer Phaser");
    QVERIFY(item != nullptr);
    const QRect r = browser->list()->visualItemRect(item);
    QTest::mouseClick(browser->list()->viewport(), Qt::LeftButton, Qt::NoModifier, r.center());
    QTest::mouseDClick(browser->list()->viewport(), Qt::LeftButton, Qt::NoModifier, r.center());
    QTest::mouseRelease(browser->list()->viewport(), Qt::LeftButton, Qt::NoModifier, r.center());
    QApplication::processEvents();
    QVERIFY(kept());

    // Another effect tried, then back to it: the look's Speed and Spread
    // go with it (until now they were wiped as the effect's own settings).
    browser->search()->setText("Dimmer");
    QApplication::processEvents();
    QListWidgetItem *other = nullptr;
    for (int i = 0; i < browser->list()->count() && other == nullptr; ++i)
        if (browser->list()->item(i)->text() != QLatin1String("Dimmer Phaser"))
            other = browser->list()->item(i);
    QVERIFY(other != nullptr);
    browser->pick(other);
    QApplication::processEvents();
    QVERIFY(kept());
    item = itemNamed("Dimmer Phaser");
    browser->pick(item);
    QApplication::processEvents();
    QVERIFY(kept());
    ed.m_effectPicker->findChild<QPushButton *>("effectPickerDone")->click();
    QVERIFY(kept());
    speed = ed.m_effectDynWidget->findChild<QComboBox *>("effectSpeedMaster");
    QCOMPARE(speed->currentData().toInt(), int(SpeedMasters::Master1 + 1));

    // And they are saved with the look.
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    {
        QXmlStreamWriter w(&buf);
        QVERIFY(look->saveXML(&w));
    }
    buf.close();
    buf.open(QIODevice::ReadOnly);
    QXmlStreamReader xml(&buf);
    xml.readNextStartElement();
    QLCPalette back(QLCPalette::Effect);
    QVERIFY(back.loadXML(xml));
    QCOMPARE(int(back.effectParamValues().value("__speedMaster")), int(SpeedMasters::Master1 + 1));
    QCOMPARE(back.effectParamValues().value("__spreadAmount"), 50.0);
}

/* An effect that takes the host spread gets a Spread row; turned on, it hides
   the effect's own spread settings (the phaser's spread and reverse). */
void LookEditor_Test::theHostSpreadStandsInForTheEffectsOwn()
{
    Doc doc(this);
    doc.effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("Chase");
    QVERIFY(doc.addPalette(look));
    LookEditor ed(&doc);
    ed.resize(1000, 640);
    ed.show();
    ed.setPalette(look->id());
    EffectBrowser *browser = ed.m_effectBrowser;
    browser->search()->setText("Dimmer Phaser");
    QApplication::processEvents();
    QListWidgetItem *item = nullptr;
    for (int i = 0; i < browser->list()->count() && item == nullptr; ++i)
        if (browser->list()->item(i)->text() == QLatin1String("Dimmer Phaser"))
            item = browser->list()->item(i);
    QVERIFY(item != nullptr);
    browser->pick(item);
    QApplication::processEvents();

    auto ownRows = [&]() {
        QStringList names;
        foreach (QLabel *l, ed.m_effectDynWidget->findChildren<QLabel *>())
            if (l->text() == QLatin1String("spread") || l->text() == QLatin1String("reverse"))
                names << l->text();
        return names.size();
    };
    QCOMPARE(ownRows(), 2);
    QDoubleSpinBox *amount = ed.m_effectDynWidget->findChild<QDoubleSpinBox *>("spreadAmount");
    QVERIFY2(amount != nullptr, "the phaser takes the host spread");
    QComboBox *dir = ed.m_effectDynWidget->findChild<QComboBox *>("spreadDirection");
    QVERIFY(!dir->isEnabled());

    amount->setValue(1.0);
    QCOMPARE(look->effectParamValues().value("__spreadAmount"), 1.0);
    QTRY_COMPARE(ownRows(), 0);                     // rebuilt without them
    dir = ed.m_effectDynWidget->findChild<QComboBox *>("spreadDirection");
    QVERIFY(dir->isEnabled());
    dir->setCurrentIndex(2);                        // centre out
    QCOMPARE(int(look->effectParamValues().value("__spreadDirection")), 2);
    QSpinBox *wings = ed.m_effectDynWidget->findChild<QSpinBox *>("spreadWings");
    wings->setValue(2);
    QCOMPARE(int(look->effectParamValues().value("__spreadWings")), 2);

    amount = ed.m_effectDynWidget->findChild<QDoubleSpinBox *>("spreadAmount");
    amount->setValue(0.0);
    QTRY_COMPARE(ownRows(), 2);                     // its own again
}

/* wfaudit 2026-11-03: a Pan/Tilt look on a scene of pars used to edit
   happily and move nothing. It says so, as a gobo look on pars does. */
void LookEditor_Test::aLookTheTargetsCantDoIsSaid()
{
    Doc doc(this);
    Scene *scene = new Scene(&doc);
    doc.addFunction(scene);
    QLCPalette *pos = new QLCPalette(QLCPalette::PanTilt, &doc);
    pos->setName("Centre");
    QVERIFY(doc.addPalette(pos));
    scene->addPalette(pos->id());

    LookEditor ed(&doc);
    ed.resize(1000, 700);
    ed.show();
    // No fixtures yet: nothing to warn about.
    ed.setContextScene(scene);
    ed.setPalette(pos->id());
    QVERIFY(!ed.m_warning->isVisible());

    for (int i = 0; i < 2; ++i)
    {
        Fixture *fx = new Fixture(&doc);
        fx->setName(QString("Par %1").arg(i + 1));
        fx->setChannels(1);
        fx->setAddress(quint32(i));
        QVERIFY(doc.addFixture(fx));
        scene->addFixture(fx->id());
    }
    ed.setContextScene(scene);
    ed.setPalette(pos->id());
    QTRY_VERIFY(ed.m_warning->isVisible());
    QVERIFY(ed.m_warning->text().contains("can pan or tilt"));
    ed.setContextScene(nullptr);
}

/* "need S3 .. resolved": the Spread row shows the look's heads coloured by
   phase, and follows the settings as they change. */
void LookEditor_Test::theSpreadIsPreviewed()
{
    Doc doc(this);
    doc.effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    Scene *scene = new Scene(&doc);
    for (int i = 0; i < 3; ++i)
    {
        Fixture *fx = new Fixture(&doc);
        fx->setName(QString("Par %1").arg(i + 1));
        fx->setChannels(1);
        fx->setAddress(quint32(i));
        QVERIFY(doc.addFixture(fx));
        doc.monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(1000.0f * i, 0, 0));
        scene->addFixture(fx->id());
    }
    doc.addFunction(scene);
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("Wave");
    QVERIFY(doc.addPalette(look));
    scene->addPalette(look->id());

    LookEditor ed(&doc);
    ed.resize(1000, 700);
    ed.show();
    ed.setContextScene(scene);
    ed.setPalette(look->id());
    EffectBrowser *browser = ed.m_effectBrowser;
    browser->search()->setText("Dimmer Phaser");
    QApplication::processEvents();
    QListWidgetItem *item = nullptr;
    for (int i = 0; i < browser->list()->count() && item == nullptr; ++i)
        if (browser->list()->item(i)->text() == QLatin1String("Dimmer Phaser"))
            item = browser->list()->item(i);
    QVERIFY(item != nullptr);
    browser->pick(item);
    QApplication::processEvents();

    SpreadPreview *preview = ed.m_effectDynWidget->findChild<SpreadPreview *>();
    QVERIFY2(preview != nullptr, "a look with heads gets a preview");
    QVERIFY(!preview->isVisible());                     // off: nothing to show

    ed.m_effectDynWidget->findChild<QDoubleSpinBox *>("spreadAmount")->setValue(1.0);
    QTRY_VERIFY(ed.m_effectDynWidget->findChild<SpreadPreview *>() != nullptr
                && ed.m_effectDynWidget->findChild<SpreadPreview *>()->isVisible());
    preview = ed.m_effectDynWidget->findChild<SpreadPreview *>();
    QCOMPARE(preview->dots().size(), 3);
    QCOMPARE(preview->dots().at(0).phase, 0.0);
    QVERIFY(qAbs(preview->dots().at(2).phase - 2.0 / 3) < 1e-9);

    ed.m_effectDynWidget->findChild<QComboBox *>("spreadDirection")->setCurrentIndex(1);   // reverse
    QVERIFY(qAbs(preview->dots().at(0).phase - 2.0 / 3) < 1e-9);
    QCOMPARE(preview->dots().at(2).phase, 0.0);
    preview->grab();                                     // it paints
}

/* "the effect search and listing with tabs is taking up too much space when
   working WITH an effect .. we still need to be able to access it .. a
   selection should apply immediately": the browser is a window of its own,
   the settings have the width, and every pick plays at once. */
/* A new effect palette made while the editor shows another effect starts
   as "New Effect" with no effect, and is named after the one picked
   (COURSE_PLAN §8 #18: it took the effect shown before's name). */
void LookEditor_Test::aNewEffectLookStartsAsANewEffect()
{
    Doc doc(this);
    doc.effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    LookEditor ed(&doc);
    ed.resize(1000, 640);
    ed.show();
    auto pickNamed = [&](const QString &name) {
        EffectBrowser *browser = ed.m_effectBrowser;
        browser->search()->setText(name);
        QApplication::processEvents();
        for (int i = 0; i < browser->list()->count(); ++i)
            if (browser->list()->item(i)->text() == name)
            {
                browser->pick(browser->list()->item(i));
                QApplication::processEvents();
                return true;
            }
        return false;
    };

    // The first: picked, named after it, picker closed.
    const quint32 first = doc.paletteOps()->create(QLCPalette::Effect, QString());
    ed.setPalette(first);
    QTRY_VERIFY(ed.effectPickerOpen());
    QVERIFY(pickNamed("Color Chase"));
    ed.m_effectPicker->findChild<QPushButton *>("effectPickerDone")->click();
    QCOMPARE(doc.palette(first)->name(), QString("Color Chase"));

    // A second, made while the first is shown.
    const quint32 second = doc.paletteOps()->create(QLCPalette::Effect, QString());
    QLCPalette *p = doc.palette(second);
    const QString factory = p->name();
    ed.setPalette(second);
    QTRY_VERIFY(ed.effectPickerOpen());
    QApplication::processEvents();
    QCOMPARE(p->name(), factory);
    QVERIFY2(p->scriptPath().isEmpty(), qPrintable(p->scriptPath()));
    QCOMPARE(doc.palette(first)->name(), QString("Color Chase"));
    QVERIFY(doc.palette(first)->scriptPath().endsWith("color-chase.js"));
    // Typed into the search, as a person does: what the list shows on the
    // way is not picked.
    EffectBrowser *browser = ed.m_effectBrowser;
    browser->search()->setFocus();
    browser->search()->selectAll();
    QTest::keyClicks(browser->search(), "Sparkle");
    QApplication::processEvents();
    QCOMPARE(p->name(), factory);
    QVERIFY2(p->scriptPath().isEmpty(), qPrintable(p->scriptPath()));
    QVERIFY(pickNamed("Sparkle"));
    ed.m_effectPicker->findChild<QPushButton *>("effectPickerDone")->click();
    QCOMPARE(p->name(), QString("Sparkle"));
    QVERIFY(p->scriptPath().endsWith("sparkle.js"));
}

/* Made with the editor closed (the Show Tree opens it after): the picker
   opens by itself as the editor appears, as it does with it open. */
void LookEditor_Test::thePickerOpensWhenTheEditorIsShown()
{
    Doc doc(this);
    doc.effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    LookEditor ed(&doc);
    ed.resize(1000, 640);
    const quint32 id = doc.paletteOps()->create(QLCPalette::Effect, QString());
    ed.setPalette(id);
    QApplication::processEvents();
    QVERIFY(!ed.effectPickerOpen());
    ed.show();
    QTRY_VERIFY(ed.effectPickerOpen());
    ed.closeEffectPicker(false);

    // An effect already picked: no picker on show.
    QLCPalette *p = doc.palette(id);
    p->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/sparkle.js");
    ed.hide();
    ed.setPalette(id);
    ed.show();
    QTest::qWait(50);
    QVERIFY(!ed.effectPickerOpen());
}

void LookEditor_Test::theEffectPickerIsAWindowOfItsOwn()
{
    Doc doc(this);
    doc.effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("Side");
    QVERIFY(doc.addPalette(look));
    LookEditor ed(&doc);
    ed.resize(1000, 640);
    ed.show();
    auto pickNamed = [&](const QString &name) {
        EffectBrowser *browser = ed.m_effectBrowser;
        browser->search()->setText(name);
        QApplication::processEvents();
        for (int i = 0; i < browser->list()->count(); ++i)
            if (browser->list()->item(i)->text() == name)
            {
                browser->pick(browser->list()->item(i));
                QApplication::processEvents();
                return true;
            }
        return false;
    };

    // A new effect look: the picker opens by itself -- choosing is the first thing.
    ed.setPalette(look->id());
    QTRY_VERIFY(ed.effectPickerOpen());
    QVERIFY(!ed.isAncestorOf(ed.m_effectBrowser));      // its own window
    QVERIFY(pickNamed("Dimmer Phaser"));
    QVERIFY(look->scriptPath().endsWith("dimmer-phaser.js"));          // applied at once
    QVERIFY(ed.m_effectDynWidget->findChildren<QSlider *>().size() > 0); // its settings shown
    ed.m_effectPicker->findChild<QPushButton *>("effectPickerDone")->click();
    QVERIFY(!ed.effectPickerOpen());

    // The settings have the width; the header names the effect.
    QScrollArea *details = ed.findChild<QScrollArea *>("effectDetails");
    QVERIFY(details != nullptr);
    QVERIFY(details->isAncestorOf(ed.m_effectPersistentCb));
    QVERIFY(details->isAncestorOf(ed.m_saveAsEffectButton));
    QVERIFY2(details->width() > ed.width() * 0.8,
             qPrintable(QString("settings %1 wide of %2").arg(details->width()).arg(ed.width())));
    QCOMPARE(ed.m_effectPickedLabel->text(), QString("Dimmer Phaser"));
    QCOMPARE(ed.m_effectChangeButton->text(), QString("Change Effect…"));

    // What it is lives behind "?" and in the picker, not over the settings.
    QVERIFY(!details->isAncestorOf(ed.m_effectDescLabel) || !ed.m_effectDescLabel->isVisible());
    QVERIFY(ed.m_effectInfoButton->isVisible());
    QVERIFY(ed.m_effectInfoButton->toolTip().contains("Dimmer Phaser"));
    QVERIFY(ed.m_effectInfoButton->toolTip().contains("Works with"));
    ed.m_effectInfoButton->click();
    QVERIFY(ed.m_effectDetails != nullptr && ed.m_effectDetails->isVisible());
    const QString about = ed.m_effectDetails->findChild<QTextBrowser *>("effectDetailsText")->toPlainText();
    QVERIFY2(about.contains("rate") && about.contains("Cycles per second"), qPrintable(about));
    ed.m_effectDetails->hide();
    QVERIFY(ed.m_effectPickerInfo->toPlainText().contains("Cycles per second"));

    // The picker is wide enough for its tabs, whole.
    ed.openEffectPicker();
    QApplication::processEvents();
    QTabBar *tabs = ed.m_effectBrowser->tabs();
    QVERIFY(tabs->tabRect(tabs->count() - 1).right() <= tabs->width());
    for (int i = 0; i < tabs->count(); ++i)
        QVERIFY(!tabs->tabText(i).contains(QChar(0x2026)));
    if (!qEnvironmentVariableIsEmpty("PICKER_SHOT"))
        ed.m_effectPicker->grab().save(qEnvironmentVariable("PICKER_SHOT"));
    ed.closeEffectPicker(true);

    // Change, try two, Cancel: the phaser is back with its settings and name.
    look->setEffectParamValue("rate", 3.5);
    const QString name = look->name();
    ed.m_effectChangeButton->click();
    QVERIFY(ed.effectPickerOpen());
    QVERIFY(pickNamed("Rainbow"));
    QVERIFY(look->scriptPath().endsWith("rainbow.js"));
    QCOMPARE(ed.m_effectPickedLabel->text(), QString("Rainbow"));
    QVERIFY(pickNamed("Orbit"));
    ed.m_effectPicker->findChild<QPushButton *>("effectPickerCancel")->click();
    QVERIFY(!ed.effectPickerOpen());
    QVERIFY(look->scriptPath().endsWith("dimmer-phaser.js"));
    QCOMPARE(look->effectParamValues().value("rate"), 3.5);
    QCOMPARE(look->name(), name);
    QCOMPARE(ed.m_effectPickedLabel->text(), QString("Dimmer Phaser"));

    // Esc is Cancel too.
    ed.openEffectPicker();
    QVERIFY(pickNamed("Rainbow"));
    QTest::keyClick(ed.m_effectBrowser->search(), Qt::Key_Escape);
    QVERIFY(!ed.effectPickerOpen());
    QVERIFY(look->scriptPath().endsWith("dimmer-phaser.js"));

    // Arrow keys pick as they go.
    ed.openEffectPicker();
    EffectBrowser *browser = ed.m_effectBrowser;
    browser->search()->clear();
    QApplication::processEvents();
    QVERIFY(browser->list()->count() > 2);
    browser->list()->setFocus();
    browser->list()->setCurrentRow(0);
    const QString before = look->scriptPath() + look->wledEffect() + look->effectPreset();
    QTest::keyClick(browser->list(), Qt::Key_Down);
    QApplication::processEvents();
    QVERIFY2(look->scriptPath() + look->wledEffect() + look->effectPreset() != before,
             "an arrow key should pick the next one");

    // Closing the window keeps what's picked.
    QTest::keyClick(browser->list(), Qt::Key_Down);
    const QString kept = look->scriptPath() + look->wledEffect() + look->effectPreset();
    ed.m_effectPicker->close();
    QVERIFY(!ed.effectPickerOpen());
    QCOMPARE(look->scriptPath() + look->wledEffect() + look->effectPreset(), kept);
}

/* "If they're not cross compatible I'd like to not show the effects that
   won't work if a wled instance isn't in the targets .. and make sure it
   does show it": a WLED effect with no port runs on a WLED device only. */
void LookEditor_Test::wledOnlyEffectsNeedAWledTarget()
{
    Doc doc(this);
    doc.effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    // The WLED list comes from a WLED fixture in the show.
    QLCFixtureDef *def = new QLCFixtureDef();
    QCOMPARE(def->loadXML(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/WLED/WLED-Lantern.qxf"),
             QFile::NoError);
    Fixture *lantern = new Fixture(&doc);
    lantern->setFixtureDefinition(def, def->mode("Effect (15ch)"));
    lantern->setConnection(Fixture::Network);
    lantern->setIPAddress("172.18.2.60");
    QVERIFY(doc.addFixture(lantern));
    Fixture *par = new Fixture(&doc);
    par->setName("Par");
    par->setChannels(3);
    par->setAddress(100);
    QVERIFY(doc.addFixture(par));

    Scene *scene = new Scene(&doc);
    scene->addFixture(par->id());                  // a plain fixture only
    doc.addFunction(scene);
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("Look");
    QVERIFY(doc.addPalette(look));
    scene->addPalette(look->id());

    LookEditor ed(&doc);
    ed.resize(1000, 640);
    ed.show();
    ed.setContextScene(scene);
    ed.setPalette(look->id());
    QTRY_VERIFY(ed.effectPickerOpen());
    EffectBrowser *browser = ed.m_effectBrowser;
    auto inWledTab = [browser]() {
        QStringList names;
        const int t = browser->tabNames().indexOf("WLED");
        if (t < 0)
            return names;
        browser->tabs()->setCurrentIndex(t);
        QApplication::processEvents();
        for (int i = 0; i < browser->list()->count(); ++i)
            names << browser->list()->item(i)->text();
        return names;
    };
    QVERIFY(!browser->wledTargets());
    // None of WLED's own on a par: the tab is gone, and its ported generators
    // too.
    QVERIFY2(inWledTab().isEmpty(), "no WLED effects for a plain fixture");
    QVERIFY(browser->tabNames().indexOf("WLED") < 0);
    // Show All Effects brings them back, greyed.
    browser->showAllButton()->setChecked(true);
    QVERIFY(browser->tabNames().indexOf("WLED") >= 0);
    browser->showAllButton()->setChecked(false);
    ed.closeEffectPicker(true);

    // A WLED device among the targets: all of WLED's own.
    scene->addFixture(lantern->id());
    ed.openEffectPicker();
    QVERIFY(browser->wledTargets());
    const QStringList with = inWledTab();
    QVERIFY2(with.size() > 10, qPrintable(QString("%1 with a WLED target").arg(with.size())));
    ed.closeEffectPicker(true);
}

void LookEditor_Test::anEffectSaysWhereItCameFrom()
{
    // My effects live in the user's folder: a fresh one for this test.
    QTemporaryDir home;
    const QByteArray wasHome = qgetenv("HOME");
    qputenv("HOME", home.path().toUtf8());
    struct Restore { QByteArray h; ~Restore() { qputenv("HOME", h); } } restore{ wasHome };

    Doc doc(this);
    EffectScriptCache *cache = doc.effectScriptRunner()->cache();
    cache->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    const QString chasePath = cache->scriptPath("Color Chase");
    QVERIFY(!chasePath.isEmpty());

    // Mine: saved on this computer, a faster Color Chase.
    EffectPresetCache::Preset mine;
    mine.name = "Quick Chase";
    mine.script = EffectScriptCache::portableName(chasePath);
    mine.category = "Color";
    mine.params.insert("speed", 9.0);
    QVERIFY(doc.effectScriptRunner()->presetCache()->savePreset(mine));
    QVERIFY(EffectOrigin::isMinePath(doc.effectScriptRunner()->presetCache()->preset("Quick Chase").path));

    // This show: an effect palette with a wider band.
    QLCPalette *verse = new QLCPalette(QLCPalette::Effect, &doc);
    verse->setName("Verse chase");
    verse->setScriptPath(chasePath);
    verse->setEffectParamValue("width", 0.6);
    QVERIFY(doc.addPalette(verse));

    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("New Effect");
    QVERIFY(doc.addPalette(look));
    LookEditor ed(&doc);
    ed.setPalette(look->id());
    EffectBrowser *browser = ed.m_effectBrowser;
    QVERIFY(browser != nullptr);
    auto entryOf = [browser](const QString &label) -> const EffectBrowser::Entry * {
        for (int i = 0; i < browser->list()->count(); ++i)
        {
            const int e = browser->list()->item(i)->data(Qt::UserRole + 10).toInt();
            if (browser->m_entries.at(e).label == label)
                return &browser->m_entries.at(e);
        }
        return nullptr;
    };
    auto itemOf = [browser](const QString &label) -> QListWidgetItem * {
        for (int i = 0; i < browser->list()->count(); ++i)
            if (browser->list()->item(i)->text() == label)
                return browser->list()->item(i);
        return nullptr;
    };

    browser->search()->setText("chase");
    const EffectBrowser::Entry *builtIn = entryOf("Color Chase");
    const EffectBrowser::Entry *quick = entryOf("Quick Chase");
    const EffectBrowser::Entry *show = entryOf("Verse chase");
    QVERIFY(builtIn != nullptr && quick != nullptr && show != nullptr);
    QCOMPARE(builtIn->origin, int(EffectOrigin::BuiltIn));
    QVERIFY(builtIn->basedOn.isEmpty());                         // no mark, no line
    QCOMPARE(quick->origin, int(EffectOrigin::Mine));
    QVERIFY2(quick->basedOn.startsWith("based on Color Chase"), qPrintable(quick->basedOn));
    QVERIFY2(quick->diffTip.contains("speed", Qt::CaseInsensitive), qPrintable(quick->diffTip));
    QCOMPARE(show->origin, int(EffectOrigin::ThisShow));
    QVERIFY2(show->basedOn.startsWith("based on Color Chase"), qPrintable(show->basedOn));
    QVERIFY2(show->diffTip.contains("width", Qt::CaseInsensitive), qPrintable(show->diffTip));
    QVERIFY(itemOf("Verse chase")->toolTip().contains("width", Qt::CaseInsensitive));

    // The filters: none is everything; each one lists only its own, from every tab.
    browser->search()->clear();
    QToolButton *fMine = browser->originFilter(EffectOrigin::Mine);
    QToolButton *fShow = browser->originFilter(EffectOrigin::ThisShow);
    QToolButton *fBuiltIn = browser->originFilter(EffectOrigin::BuiltIn);
    QCOMPARE(fBuiltIn->text(), QString("Built-in"));
    QCOMPARE(fMine->text(), QString("Mine"));
    QCOMPARE(fShow->text(), QString("This show"));
    fMine->setChecked(true);
    QCOMPARE(browser->list()->count(), 1);
    QCOMPARE(browser->list()->item(0)->text(), QString("Quick Chase"));
    fMine->setChecked(false);
    fShow->setChecked(true);
    QCOMPARE(browser->list()->count(), 1);
    QCOMPARE(browser->list()->item(0)->text(), QString("Verse chase"));
    fShow->setChecked(false);
    fBuiltIn->setChecked(true);
    QVERIFY(browser->list()->count() > 10);
    QVERIFY(itemOf("Quick Chase") == nullptr && itemOf("Verse chase") == nullptr);
    fBuiltIn->setChecked(false);

    // Picking the show's one copies it in: its effect, its settings.
    browser->search()->setText("verse");
    QListWidgetItem *pick = itemOf("Verse chase");
    QVERIFY(pick != nullptr);
    browser->list()->setCurrentItem(pick);
    emit browser->list()->itemClicked(pick);
    QCOMPARE(look->scriptPath(), chasePath);
    QCOMPARE(look->effectParamValues().value("width"), 0.6);
    // ...and says what it is based on, and how it differs.
    QVERIFY(!ed.basedOnLabel()->isHidden());
    QVERIFY2(ed.basedOnLabel()->text().startsWith("based on Color Chase"), qPrintable(ed.basedOnLabel()->text()));
    QVERIFY(ed.basedOnLabel()->toolTip().contains("width", Qt::CaseInsensitive));

    // The save buttons say where it goes.
    QPushButton *saveMine = ed.findChild<QPushButton *>("effectSaveAs");
    QVERIFY(saveMine != nullptr);
    QCOMPARE(saveMine->text(), QString("Save to My Effects…"));
}

void LookEditor_Test::anyEffectRunsOnceOrNTimes()
{
    Doc doc(this);
    EffectScriptCache *cache = doc.effectScriptRunner()->cache();
    cache->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("Chase");
    look->setScriptPath(cache->scriptPath("Color Chase"));      // a loop effect
    QVERIFY(doc.addPalette(look));
    LookEditor ed(&doc);
    ed.setPalette(look->id());

    QComboBox *runs = ed.findChild<QComboBox *>("effectRuns");
    QSpinBox *count = ed.findChild<QSpinBox *>("effectRunsCount");
    QComboBox *atEnd = ed.findChild<QComboBox *>("effectAtEnd");
    QDoubleSpinBox *len = ed.findChild<QDoubleSpinBox *>("effectRunLength");
    QVERIFY(runs != nullptr && count != nullptr && atEnd != nullptr && len != nullptr);
    QCOMPARE(runs->currentText(), QString("Loop"));               // a loop effect loops
    QVERIFY(atEnd->isHidden() && count->isHidden());

    runs->setCurrentIndex(1);                                      // Once
    QCOMPARE(look->effectParamValues().value("__runs"), 1.0);
    QVERIFY(!atEnd->isHidden() && !len->isHidden() && count->isHidden());
    atEnd->setCurrentIndex(1);                                     // Release
    QCOMPARE(look->effectParamValues().value("__atEnd"), 1.0);
    len->setValue(1.5);
    QCOMPARE(look->effectParamValues().value("__durationMs"), 1500.0);
    runs->setCurrentIndex(2);                                      // N times
    QVERIFY(!count->isHidden());
    count->setValue(4);
    QCOMPARE(look->effectParamValues().value("__runs"), 4.0);
    runs->setCurrentIndex(0);                                      // Loop again
    QCOMPARE(look->effectParamValues().value("__runs"), 0.0);
    QVERIFY(atEnd->isHidden());

    // A one-shot script starts on Once, with its own end (the wand lets go).
    QLCPalette *wand = new QLCPalette(QLCPalette::Effect, &doc);
    wand->setName("Wand");
    wand->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/wand.js");
    QVERIFY(doc.addPalette(wand));
    ed.setPalette(wand->id());
    runs = ed.findChild<QComboBox *>("effectRuns");
    atEnd = ed.findChild<QComboBox *>("effectAtEnd");
    QCOMPARE(runs->currentText(), QString("Once"));
    QCOMPARE(atEnd->currentText(), QString("Release"));
}

/* A Breathe on the Dimmer Phaser, as resources/effectpresets/Breathe.json
   has it. */
static QLCPalette *aBreathe(Doc &doc)
{
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("Breathe");
    look->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/dimmer-phaser.js");
    look->setEffectParamValue("rate", 0.2);
    look->setEffectParamValue("minDim", 0.05);
    look->setEffectParamValue("maxDim", 1.0);
    return doc.addPalette(look) ? look : nullptr;
}

/* The number beside a setting's slider is a field: typed, the setting
   takes that exact value (in its own units and range) and the slider
   follows; dragged, the number follows the slider. */
void LookEditor_Test::anEffectSettingTakesATypedValue()
{
    Doc doc(this);
    QLCPalette *look = aBreathe(doc);
    QVERIFY(look != nullptr);
    LookEditor ed(&doc);
    ed.setPalette(look->id());

    QDoubleSpinBox *minDim = ed.findChild<QDoubleSpinBox *>("effectParam_minDim");
    QVERIFY(minDim != nullptr);
    QCOMPARE(minDim->value(), 0.05);
    QCOMPARE(minDim->minimum(), 0.0);
    QCOMPARE(minDim->maximum(), 1.0);
    QSlider *slider = nullptr;
    for (QSlider *sl : ed.findChildren<QSlider *>())
        if (sl->property("paramName").toString() == "minDim")
            slider = sl;
    QVERIFY(slider != nullptr);

    minDim->setValue(0.123);
    QCOMPARE(look->effectParamValues().value("minDim"), 0.123);   // exact, not a slider step
    QCOMPARE(slider->value(), 123);
    minDim->setValue(5.0);                                          // out of range: its top
    QCOMPARE(look->effectParamValues().value("minDim"), 1.0);

    slider->setValue(250);
    QCOMPARE(look->effectParamValues().value("minDim"), 0.25);
    QCOMPARE(minDim->value(), 0.25);
}

/* Run Once, a wave's Length is one cycle: a Breathe at 0.2 a second is a
   5 s breath. Typed, it sets the rate (no run length of its own); the
   rate's slider and number follow, and the rate moves the Length. */
void LookEditor_Test::aWavesLengthIsOneCycle()
{
    Doc doc(this);
    QLCPalette *look = aBreathe(doc);
    QVERIFY(look != nullptr);
    LookEditor ed(&doc);
    ed.setPalette(look->id());

    QComboBox *runs = ed.findChild<QComboBox *>("effectRuns");
    QDoubleSpinBox *len = ed.findChild<QDoubleSpinBox *>("effectRunLength");
    QDoubleSpinBox *rate = ed.findChild<QDoubleSpinBox *>("effectParam_rate");
    QVERIFY(runs != nullptr && len != nullptr && rate != nullptr);
    QVERIFY(len->isHidden());                                       // Loop: no Length
    runs->setCurrentIndex(1);                                       // Once
    QVERIFY(!len->isHidden());
    QCOMPARE(len->value(), 5.0);
    QCOMPARE(len->specialValueText(), QString());                   // no "auto": it is the cycle's
    QVERIFY(len->toolTip().contains("rate"));

    len->setValue(5.5);
    QVERIFY(qAbs(look->effectParamValues().value("rate") - 1.0 / 5.5) < 1e-9);
    QVERIFY(!look->effectParamValues().contains("__durationMs"));
    QCOMPARE(rate->value(), 0.18);

    rate->setValue(0.25);
    QCOMPARE(len->value(), 4.0);
    QVERIFY(!look->effectParamValues().contains("__durationMs"));

    // The context menu's Length says and sets the same
    EffectScript::ParamDef def;
    QCOMPARE(EffectOrigin::scriptCycleParam(look->scriptPath(), &def), QString("rate"));
    EffectRunsMenu::setCycleLength(&doc, { look->id() }, "rate", def, 6.0);
    QVERIFY(qAbs(look->effectParamValues().value("rate") - 1.0 / 6.0) < 1e-9);

    // A loop effect with no wave keeps its run Length (auto = the cue's)
    QLCPalette *chase = new QLCPalette(QLCPalette::Effect, &doc);
    chase->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/color-chase.js");
    QVERIFY(doc.addPalette(chase));
    QVERIFY(EffectOrigin::scriptCycleParam(chase->scriptPath()).isEmpty());
}

void LookEditor_Test::theBrowsersListsAreAToZ()
{
    Doc doc(this);
    doc.effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    QLCFixtureDef *def = new QLCFixtureDef();
    QCOMPARE(def->loadXML(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/WLED/WLED-Lantern.qxf"),
             QFile::NoError);
    Fixture *lantern = new Fixture(&doc);
    lantern->setFixtureDefinition(def, def->mode("Effect (15ch)"));
    QVERIFY(doc.addFixture(lantern));
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    QVERIFY(doc.addPalette(look));
    LookEditor ed(&doc);
    ed.setPalette(look->id());
    EffectBrowser *browser = ed.m_effectBrowser;
    QVERIFY(browser != nullptr);

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    const QStringList tabs = browser->tabNames();
    for (int t = 1; t < tabs.size(); ++t)                // Recent keeps its order
    {
        browser->m_tabs->setCurrentIndex(t);
        QStringList keys;
        for (int i = 0; i < browser->list()->count(); ++i)
            keys << browser->m_entries.at(browser->list()->item(i)->data(Qt::UserRole + 10).toInt()).sortName;
        QVERIFY2(keys.size() > 0 || tabs.at(t) != "WLED", "the WLED tab is empty");
        for (int i = 1; i < keys.size(); ++i)
            QVERIFY2(collator.compare(keys.at(i - 1), keys.at(i)) <= 0,
                     qPrintable(QString("%1: \"%2\" before \"%3\"").arg(tabs.at(t), keys.at(i - 1), keys.at(i))));
    }
}

/* Aim at: lists the Studio's targets as they come and go, with the look
   open (a target added, renamed or deleted while the editor is up). */
void LookEditor_Test::theAimPickerFollowsTargetsAddedInTheStudio()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    QLCPalette *look = new QLCPalette(QLCPalette::Aim, &doc);
    look->setName("Aim");
    QVERIFY(doc.addPalette(look));
    LookEditor ed(&doc);
    ed.show();
    ed.setPalette(look->id());
    QCOMPARE(ed.m_aimTargetCombo->count(), 1);

    StageTarget *mark = props->addStageTarget();   // as Monitor::addTarget()
    mark->setName("Centre");
    props->notifyStageTargetsChanged();
    QCOMPARE(ed.m_aimTargetCombo->count(), 2);
    QVERIFY(ed.m_aimTargetCombo->findData(mark->id()) >= 0);

    StageTarget *edge = props->addStageTarget();
    edge->makeLine();
    edge->setName("Edge");
    props->notifyStageTargetsChanged();
    QVERIFY(ed.m_aimTargetCombo->itemText(ed.m_aimTargetCombo->findData(edge->id())).startsWith("Line:"));

    mark->setName("Downstage");
    props->notifyStageTargetsChanged();
    QCOMPARE(ed.m_aimTargetCombo->itemText(ed.m_aimTargetCombo->findData(mark->id())), QString("Downstage"));

    // The look's own pick survives a rebuild; deleting the picked target falls back to (none).
    ed.m_aimTargetCombo->setCurrentIndex(ed.m_aimTargetCombo->findData(edge->id()));
    QCOMPARE(look->stageTargetId(), edge->id());
    props->removeStageTarget(mark->id());
    QCOMPARE(ed.m_aimTargetCombo->count(), 2);
    QCOMPARE(ed.m_aimTargetCombo->currentData().toUInt(), edge->id());
    props->removeStageTarget(edge->id());
    QCOMPARE(ed.m_aimTargetCombo->count(), 1);
}

void LookEditor_Test::aLineLooksSpreadIsSetOnTheAimPage()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StageTarget *line = props->addStageTarget();
    line->setName("Edge");
    line->makeLine();
    StageTarget *mark = props->addStageTarget();
    mark->setName("Mark");

    QLCPalette *look = new QLCPalette(QLCPalette::Aim, &doc);
    look->setName("Along the edge");
    look->setStageTargetId(line->id());
    QVERIFY(doc.addPalette(look));

    LookEditor ed(&doc);
    ed.resize(800, 500);
    ed.show();
    ed.setPalette(look->id());
    QApplication::processEvents();

    QVERIFY2(ed.m_lineBox->isVisible(), "a look on a line shows no Spread");
    QCOMPARE(ed.m_lineSpreadSpin->value(), 5.0);
    QCOMPARE(ed.m_lineSpreadSlider->value(), 50);
    // No scene to say which lights: six stand in, evenly end to end.
    QCOMPARE(ed.m_linePreview->dots().size(), 6);
    QCOMPARE(ed.m_linePreview->dots().first().fraction, 0.0);

    ed.m_lineSpreadSlider->setValue(-25);
    QCOMPARE(ed.m_lineSpreadSpin->value(), -2.5);
    QCOMPARE(look->effectParamValues().value("__lineSpread"), -2.5);
    QVERIFY(qAbs(ed.m_linePreview->dots().first().fraction - 0.75) < 1e-9);
    ed.m_linePreview->grab();                           // it paints

    ed.m_lineSpreadSpin->setValue(0.0);
    QCOMPARE(ed.m_lineSpreadSlider->value(), 0);
    QCOMPARE(look->effectParamValues().value("__lineSpread"), 0.0);

    // Back to the default: the look no longer carries it.
    ed.m_lineSpreadSpin->setValue(5.0);
    QVERIFY(!look->effectParamValues().contains("__lineSpread"));

    // Re-opened, it shows what the look has.
    look->setEffectParamValue("__lineSpread", -1.0);
    ed.setPalette(look->id());
    QCOMPARE(ed.m_lineSpreadSpin->value(), -1.0);

    // A point target has no spread.
    ed.m_aimTargetCombo->setCurrentIndex(ed.m_aimTargetCombo->findData(mark->id()));
    QCOMPARE(look->stageTargetId(), mark->id());
    QVERIFY(!ed.m_lineBox->isVisible());
    // And the picker says which targets are lines.
    QVERIFY(ed.m_aimTargetCombo->itemText(ed.m_aimTargetCombo->findData(line->id())).startsWith("Line:"));
}

void LookEditor_Test::aLineLooksGroupingIsSetOnTheAimPage()
{
    Doc doc(this);
    StageTarget *line = doc.monitorProperties()->addStageTarget();
    line->makeLine();
    QLCPalette *look = new QLCPalette(QLCPalette::Aim, &doc);
    look->setStageTargetId(line->id());
    QVERIFY(doc.addPalette(look));

    LookEditor ed(&doc);
    ed.resize(800, 500);
    ed.show();
    ed.setPalette(look->id());
    QApplication::processEvents();
    QVERIFY(!ed.m_lineCrossCombo->isEnabled());         // each light: nothing to choose

    ed.m_lineGroupCombo->setCurrentIndex(ed.m_lineGroupCombo->findData(LineSpread::Halves));
    QVERIFY(ed.m_lineCrossCombo->isEnabled());
    QCOMPARE(look->effectParamValues().value("__lineGroup"), double(LineSpread::Halves));
    ed.m_lineSpreadSpin->setValue(-5.0);

    // "1 2 3 cross 4 5 6": the halves swap sides.
    auto at = [&ed](int number) {
        foreach (const LineSpreadPreview::Dot &d, ed.m_linePreview->dots())
            if (d.number == number)
                return d.fraction;
        return -1.0;
    };
    QVERIFY(qAbs(at(1) - 0.6) < 1e-9 && qAbs(at(4) - 0.0) < 1e-9);

    ed.m_lineCrossCombo->setCurrentIndex(ed.m_lineCrossCombo->findData(int(LineSpread::Mirror)));
    QCOMPARE(look->effectParamValues().value("__lineCross"), double(LineSpread::Mirror));
    QVERIFY(qAbs(at(1) - 0.4) < 1e-9 && qAbs(at(3) - 0.0) < 1e-9);

    ed.m_lineShareCheck->setChecked(true);
    QCOMPARE(look->effectParamValues().value("__lineShare"), 1.0);
    QVERIFY(!ed.m_lineCrossCombo->isEnabled());

    ed.m_lineOrderCombo->setCurrentIndex(ed.m_lineOrderCombo->findData(int(LineSpread::LookOrder)));
    QCOMPARE(look->effectParamValues().value("__lineOrder"), double(LineSpread::LookOrder));

    // Re-opened, every control shows the look.
    ed.setPalette(look->id());
    QCOMPARE(ed.m_lineGroupCombo->currentData().toInt(), int(LineSpread::Halves));
    QCOMPARE(ed.m_lineCrossCombo->currentData().toInt(), int(LineSpread::Mirror));
    QVERIFY(ed.m_lineShareCheck->isChecked());
    QCOMPARE(ed.m_lineOrderCombo->currentData().toInt(), int(LineSpread::LookOrder));
}

void LookEditor_Test::aLooksNameFollowsItsColour()
{
    Doc doc(this);
    PaletteOps *ops = doc.paletteOps();
    const quint32 id = ops->create(QLCPalette::Color, PaletteOps::pathForFolder("Act 2"), "02-01 Red Wash",
                                   { QLCPalette::colorToString(QColor(255, 0, 0), QColor()) });
    QLCPalette *p = doc.palette(id);
    QVERIFY(p != nullptr);
    ops->clearUndo();
    const QString path = p->path();

    LookEditor ed(&doc);
    QSignalSpy renamed(&ed, &LookEditor::paletteRenamed);
    ed.setPalette(id);
    QCOMPARE(ed.m_nameEdit->text(), QString("02-01 Red Wash"));

    // Red to dark red, on the R slider: only the colour part changes, live.
    ed.m_redSlider->setValue(139);
    QCOMPARE(p->name(), QString("02-01 Dark Red Wash"));
    QCOMPARE(ed.m_nameEdit->text(), QString("02-01 Dark Red Wash"));
    QCOMPARE(p->path(), path);                              // the folder stays
    QCOMPARE(renamed.count(), 1);
    // The same drag goes on: still one step.
    ed.m_redSlider->setValue(120);
    QCOMPARE(p->name(), QString("02-01 Dark Red Wash"));
    QVERIFY(ops->canUndo());

    // One Undo: the colour and the name together, and the editor shows it.
    QVERIFY(ops->undo());
    QCOMPARE(p->rgbValue(), QColor(255, 0, 0));
    QCOMPARE(p->name(), QString("02-01 Red Wash"));
    QVERIFY(!ops->canUndo());
    QCOMPARE(ed.m_nameEdit->text(), QString("02-01 Red Wash"));
    QCOMPARE(ed.m_redSlider->value(), 255);

    QVERIFY(ops->redo());
    QCOMPARE(p->name(), QString("02-01 Dark Red Wash"));
    QCOMPARE(ed.m_nameEdit->text(), QString("02-01 Dark Red Wash"));

    // Blue from the picker: the colour part again.
    ed.m_colorDialog->setCurrentColor(QColor(0, 0, 255));
    QCOMPARE(p->name(), QString("02-01 Blue Wash"));
}

void LookEditor_Test::aCustomNameStays()
{
    Doc doc(this);
    const quint32 id = doc.paletteOps()->create(QLCPalette::Color, QString(), "Crimson Tide",
                                                { QLCPalette::colorToString(QColor(255, 0, 0), QColor()) });
    LookEditor ed(&doc);
    ed.setPalette(id);
    ed.m_redSlider->setValue(139);
    QCOMPARE(doc.palette(id)->name(), QString("Crimson Tide"));
    ed.m_blueSlider->setValue(255);
    QCOMPARE(doc.palette(id)->name(), QString("Crimson Tide"));
    QCOMPARE(ed.m_nameEdit->text(), QString("Crimson Tide"));
}

void LookEditor_Test::aLevelInTheNameFollowsTheLevel()
{
    Doc doc(this);
    PaletteOps *ops = doc.paletteOps();
    const quint32 dim = ops->create(QLCPalette::Dimmer, QString(), "Verse Intensity 45%", { 115 });
    LookEditor ed(&doc);
    ed.setPalette(dim);
    ed.slotDimmerChanged(153);
    QCOMPARE(doc.palette(dim)->name(), QString("Verse Intensity 60%"));
    QCOMPARE(ed.m_nameEdit->text(), QString("Verse Intensity 60%"));
    ed.slotDimmerChanged(255);
    QCOMPARE(doc.palette(dim)->name(), QString("Verse Full"));

    const quint32 strobe = ops->create(QLCPalette::Strobe, QString(), "Strobe 40%", { 0.4 });
    ed.setPalette(strobe);
    ed.m_strobeSlider->setValue(70);
    QCOMPARE(doc.palette(strobe)->name(), QString("Strobe 70%"));
}

void LookEditor_Test::aNewColourIsNamedLikeItsSiblings()
{
    Doc doc(this);
    PaletteOps *ops = doc.paletteOps();
    const QString act2 = PaletteOps::pathForFolder("Act2");
    ops->create(QLCPalette::Color, act2, "Act2 Red", { QLCPalette::colorToString(Qt::red, QColor()) });
    ops->create(QLCPalette::Color, act2, "Act2 Blue", { QLCPalette::colorToString(Qt::blue, QColor()) });
    const quint32 id = ops->create(QLCPalette::Color, act2, QString());
    QVERIFY(doc.palette(id)->name().startsWith("New Color"));

    LookEditor ed(&doc);
    ed.setPalette(id);
    ed.m_colorDialog->setCurrentColor(QColor(255, 176, 0));
    QCOMPARE(doc.palette(id)->name(), QString("Act2 Amber"));
    // Then it follows like any other.
    ed.m_colorDialog->setCurrentColor(QColor(0, 255, 0));
    QCOMPARE(doc.palette(id)->name(), QString("Act2 Green"));
    QCOMPARE(PaletteOps::folderOf(doc.palette(id)), QString("Act2"));
}

QTEST_MAIN(LookEditor_Test)

/****************************************************************************
 * Formation palettes (Q197)
 ****************************************************************************/

namespace
{
Fixture *formationSpot(Doc *doc, const QString &name, double x)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Spot " + name);
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("2ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt };
    for (int k = 0; k < 2; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(k == 0 ? "Pan" : "Tilt");
        ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    QLCFixtureHead head;
    head.addChannel(0);
    head.addChannel(1);
    mode->insertHead(-1, head);
    def->addMode(mode);
    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(quint32(100 + doc->fixtures().size() * 2));
    if (!doc->addFixture(fxi))
        return nullptr;
    doc->monitorProperties()->setFixturePosition(fxi->id(), 0, 0, QVector3D(float(x * 1000.0), 0, 6));
    return fxi;
}

FixtureGroup *formationGroup(Doc *doc, const QString &name, const QList<Fixture *> &fixtures)
{
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName(name);
    doc->addFixtureGroup(g);
    for (Fixture *f : fixtures)
        g->assignFixture(f->id());
    return g;
}

StageTarget *formationLine(Doc *doc)
{
    StageTarget *edge = doc->monitorProperties()->addStageTarget();
    edge->setName("Edge");
    edge->setPosition(QVector3D(0, 3, 0));
    edge->setLinePoints({ QVector3D(-5, 0, 0), QVector3D(5, 0, 0) });
    edge->makeLine();
    return edge;
}

QByteArray showXml(Doc *doc)
{
    QByteArray xml;
    QBuffer buf(&xml);
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    doc->saveXML(&w);
    w.writeEndDocument();
    return xml;
}
}

void LookEditor_Test::aFormationIsEditedOnItsPage()
{
    Doc doc(this);
    formationLine(&doc);
    const quint32 pid = doc.paletteOps()->create(QLCPalette::Formation, QString(), "Pairs crossed");
    QVERIFY(pid != QLCPalette::invalidId());
    LookEditor ed(&doc);
    ed.resize(800, 700);
    ed.show();
    ed.setPalette(pid);
    QApplication::processEvents();
    FormationEditor *fe = ed.formationEditor();
    QVERIFY(fe != nullptr && fe->isVisible());
    // New: sets nothing; every control off.
    QVERIFY(!fe->spreadSet()->isChecked() && !fe->groupSet()->isChecked() && !fe->crossSet()->isChecked());
    QVERIFY(!fe->spreadSpin()->isEnabled());

    QSignalSpy changed(&ed, &LookEditor::paletteChanged);
    fe->spreadSet()->setChecked(true);              // at +5: set, and +5
    QLCPalette *p = doc.palette(pid);
    QVERIFY(p->formationHas(QLCPalette::FormationSpread));
    QCOMPARE(p->formationValue(QLCPalette::FormationSpread), 5.0);
    fe->spreadSpin()->setValue(0.0);
    QCOMPARE(p->formationValue(QLCPalette::FormationSpread), 0.0);
    fe->groupSet()->setChecked(true);
    fe->groupCombo()->setCurrentIndex(fe->groupCombo()->findData(QString("pairs")));
    fe->crossSet()->setChecked(true);
    fe->crossCombo()->setCurrentIndex(fe->crossCombo()->findData(QString("mirror")));
    QCOMPARE(ShowTreeController::formationSummary(p), QString("spread 0, pairs, mirror"));
    QVERIFY(changed.count() >= 4);
    // Undo the last: Cross back to unset; the page follows when re-shown.
    QVERIFY(doc.paletteOps()->undo());
    QCOMPARE(p->formationValue(QLCPalette::FormationCross), 0.0);
    QVERIFY(doc.paletteOps()->undo());
    QVERIFY(!p->formationHas(QLCPalette::FormationCross));
    ed.setPalette(pid);
    QVERIFY(!fe->crossSet()->isChecked());
    QVERIFY(fe->groupSet()->isChecked());
    // Unticked: unset again (not the default).
    fe->groupSet()->setChecked(false);
    QVERIFY(!p->formationHas(QLCPalette::FormationGroup));
    fe->grab();
}

void LookEditor_Test::aFormationsPreviewPickNeverSticks()
{
    Doc doc(this);
    StageTarget *edge = formationLine(&doc);
    StageTarget *other = doc.monitorProperties()->addStageTarget();
    other->setName("Back");
    other->setLinePoints({ QVector3D(-3, 0, -3), QVector3D(3, 0, -3) });
    other->makeLine();
    Fixture *a1 = formationSpot(&doc, "A1", -3), *a2 = formationSpot(&doc, "A2", 3);
    FixtureGroup *ga = formationGroup(&doc, "A", { a1, a2 });
    const quint32 pid = doc.paletteOps()->create(QLCPalette::Formation, QString(), "Centre", QVariantList(),
                                                 [](QLCPalette *p) {
                                                     p->setFormationValue(QLCPalette::FormationSpread, 0);
                                                 });
    LookEditor ed(&doc);
    ed.resize(800, 700);
    ed.show();
    ed.setPalette(pid);
    QApplication::processEvents();
    FormationEditor *fe = ed.formationEditor();

    doc.resetModified();
    doc.paletteOps()->clearUndo();
    const QByteArray before = showXml(&doc);
    QVERIFY(!doc.isModified());

    // The line, a group, a fixture: picked and unpicked through the page.
    fe->lineCombo()->setCurrentIndex(fe->lineCombo()->findData(QVariant(other->id())));
    QCOMPARE(fe->pick().line, other->id());
    for (int i = 0; i < fe->pickList()->count(); ++i)
        fe->pickList()->item(i)->setCheckState(Qt::Checked);
    QCOMPARE(fe->pick().groups, QList<quint32>({ ga->id() }));
    QCOMPARE(fe->pick().fixtures.count(), 2);
    QVERIFY(!fe->plan()->beams().isEmpty());
    fe->lineCombo()->setCurrentIndex(fe->lineCombo()->findData(QVariant(edge->id())));

    QVERIFY(!doc.isModified());
    QVERIFY(!doc.paletteOps()->canUndo());
    QCOMPARE(showXml(&doc), before);

    // Kept for the session, per palette: another palette, then back.
    const FormationEditor::Pick kept = fe->pick();
    const quint32 pid2 = doc.paletteOps()->create(QLCPalette::Formation, QString(), "Other");
    ed.setPalette(pid2);
    QVERIFY(fe->pick().fixtures.isEmpty());
    ed.setPalette(pid);
    QCOMPARE(fe->pick().line, kept.line);
    QCOMPARE(fe->pick().groups, kept.groups);
    QCOMPARE(fe->pick().fixtures, kept.fixtures);
}

void LookEditor_Test::aFormationsPreviewMatchesTheCue()
{
    Doc doc(this);
    StageTarget *edge = formationLine(&doc);
    Fixture *a1 = formationSpot(&doc, "A1", -4), *a2 = formationSpot(&doc, "A2", -1);
    Fixture *b1 = formationSpot(&doc, "B1", 2), *b2 = formationSpot(&doc, "B2", 4);
    FixtureGroup *ga = formationGroup(&doc, "A", { a1, a2 });
    FixtureGroup *gb = formationGroup(&doc, "B", { b1, b2 });
    formationGroup(&doc, "Unused", { a1 });
    ShowTreeController *st = doc.showTree();
    const quint32 aim = st->aimPaletteFor(edge->id());
    const quint32 pid = doc.paletteOps()->create(QLCPalette::Formation, QString(), "Pairs crossed", QVariantList(),
                                                 [](QLCPalette *p) {
                                                     p->setFormationValue(QLCPalette::FormationSpread, -2.5);
                                                     p->setFormationValue(QLCPalette::FormationGroup, 2);
                                                     p->setFormationValue(QLCPalette::FormationCross, 1);
                                                 });
    // A cue with the two layers, aimed at the line as one, the Formation on it.
    const quint32 listId = st->addCueList(st->addProduction("P"), "L");
    const quint32 c1 = st->addCue(listId, -1, "One");
    ShowTree::Target ta, tb;
    ta.groups << ga->id();
    tb.groups << gb->id();
    st->addLayer(c1, ta);
    st->addLayer(c1, tb);
    QVERIFY(st->setCueLook(c1, st->lookForPalette(aim)));
    QVERIFY(st->setCueLook(c1, st->lookForPalette(pid)));
    QHash<quint32, QVector3D> onCue;
    for (const ShowTree::Target &t : { ta, tb })
    {
        const Scene *sc = qobject_cast<const Scene *>(doc.function(st->layerScene(c1, t)));
        QVERIFY(sc != nullptr);
        for (const AimSolver::SceneAim &a : AimSolver::sceneAims(&doc, sc))
            onCue.insert(a.fixtureId, a.point);
    }
    QCOMPARE(onCue.count(), 4);

    // The default pick: the first line, and the groups aimed at it most.
    const FormationEditor::Pick def = FormationEditor::defaultPick(&doc);
    QCOMPARE(def.line, edge->id());
    QList<quint32> both { ga->id(), gb->id() };
    std::sort(both.begin(), both.end());
    QCOMPARE(def.groups, both);

    LookEditor ed(&doc);
    ed.resize(800, 700);
    ed.show();
    ed.setPalette(pid);
    QApplication::processEvents();
    FormationEditor *fe = ed.formationEditor();
    QCOMPARE(fe->pick().groups, both);
    // decided: the editor's own pick is shown, labelled with where it came from.
    QVERIFY(def.note.contains("Showing A + B"));
    QVERIFY(def.note.contains("Edge"));
    QVERIFY2(def.note.contains("One"), qPrintable(def.note));
    QVERIFY(!fe->hint()->isHidden());
    QVERIFY(fe->hint()->text().contains("Showing A + B"));
    QVERIFY(fe->hint()->text().contains("Show none"));
    const QList<LinePlanView::Beam> &beams = fe->plan()->beams();
    QCOMPARE(beams.count(), 4);
    for (const LinePlanView::Beam &b : beams)
    {
        QVERIFY(onCue.contains(b.fixture));
        const QVector3D want = onCue.value(b.fixture);
        QVERIFY2(qAbs(b.to.x() - want.x()) < 0.05 && qAbs(b.to.y() - want.y()) < 0.05,
                 qPrintable(QString("%1: preview %2,%3 cue %4,%5").arg(b.name).arg(b.to.x()).arg(b.to.y())
                            .arg(want.x()).arg(want.y())));
    }
    QCOMPARE(fe->strip()->dots().count(), 4);
    // The pop-out works from here too, fed by the same preview.
    emit fe->plan()->popOutRequested();
    QVERIFY(fe->planWindow() != nullptr && fe->planWindow()->isVisible());
    QCOMPARE(fe->planWindow()->plan()->beams().count(), 4);
    fe->planWindow()->close();

    // "Show none": no lights, the hint says how to pick, nothing is an edit.
    fe->clearPick();
    QVERIFY(fe->pick().isEmpty());
    QCOMPARE(fe->pick().line, edge->id());
    QVERIFY(fe->pick().note.isEmpty());
    QVERIFY(fe->hint()->text().contains("Tick"));
    QVERIFY(fe->plan()->beams().isEmpty());
    // A pick of the user's own carries no note.
    FormationEditor::Pick mine;
    mine.line = edge->id();
    mine.groups << ga->id();
    fe->setPick(mine);
    QVERIFY(fe->hint()->isHidden());

    // A show with no aims yet: the line, no groups, and a hint.
    Doc empty(this);
    formationLine(&empty);
    const FormationEditor::Pick none = FormationEditor::defaultPick(&empty);
    QVERIFY(none.groups.isEmpty());
    const quint32 pe = empty.paletteOps()->create(QLCPalette::Formation, QString(), "F");
    LookEditor ed2(&empty);
    ed2.resize(800, 700);
    ed2.show();
    ed2.setPalette(pe);
    QApplication::processEvents();
    QVERIFY(!ed2.formationEditor()->hint()->isHidden());
    QVERIFY(ed2.formationEditor()->hint()->text().contains("Tick"));
}


namespace
{
QLCPalette *lineAim(Doc *doc, const StageTarget *line, double spread, int group, LineSpread::Cross cross)
{
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim);
    aim->setStageTargetId(line->id());
    LineSpread::Settings s;
    s.spread = spread;
    s.group = group;
    s.cross = cross;
    QMap<QString, double> params;
    LineSpread::toParams(s, params);
    aim->setEffectParamValues(params);
    Q_UNUSED(doc);
    return aim;
}
}

void LookEditor_Test::aLineFormationShowsGroupsAndCrossings()
{
    Doc doc(this);
    StageTarget *edge = formationLine(&doc);
    Scene scene(&doc);
    for (int i = 0; i < 8; ++i)
    {
        Fixture *f = formationSpot(&doc, QString("S%1").arg(i + 1), -4.5 + i * 1.25);
        QVERIFY(f != nullptr);
        scene.addFixture(f->id());
    }

    // Pairs swapping sides, fully crossed.
    QScopedPointer<QLCPalette> swap(lineAim(&doc, edge, -5.0, 2, LineSpread::SwapSides));
    const LineFormation fs = LineFormation::build(&doc, &scene, swap.data());
    QCOMPARE(fs.lights().count(), 8);
    QCOMPARE(fs.groupCount(), 4);
    QVERIFY(fs.grouped());
    const QList<quint32> heads = AimSolver::aimHeads(&doc, &scene, swap->id());
    const QHash<quint32, double> engine = AimSolver::lineFractions(&doc, edge, heads,
                                                                   LineSpread::fromParams(swap->effectParamValues()));
    const QHash<quint32, QVector3D> points = AimSolver::linePoints(&doc, swap.data(), edge, heads);
    for (const LineFormation::Light &l : fs.lights())
    {
        QCOMPARE(l.group, (l.number - 1) / 2);
        QCOMPARE(fs.colourFor(l), LineFormation::groupColour(l.group));
        // The preview is the engine's assignment.
        QVERIFY(engine.contains(l.fixture));
        QCOMPARE(l.fraction, engine.value(l.fixture));
        QVERIFY(l.placed);
        QCOMPARE(l.to, QPointF(points.value(l.fixture).x(), points.value(l.fixture).y()));
    }
    // Groups swap sides: every pair from different groups lands the other way round.
    QCOMPARE(fs.crossings(), 24);
    QVERIFY(fs.caption().contains("Groups swap sides"));
    QVERIFY(fs.caption().contains("Pairs: 4 groups"));
    // Neighbouring groups never share a colour.
    for (int g = 0; g < 7; ++g)
        QVERIFY(LineFormation::groupColour(g) != LineFormation::groupColour(g + 1));
    QVERIFY(LineFormation::groupColour(6) != LineFormation::groupColour(7));

    // Each pair flips in place: one crossing per pair.
    QScopedPointer<QLCPalette> mirror(lineAim(&doc, edge, -5.0, 2, LineSpread::Mirror));
    const LineFormation fm = LineFormation::build(&doc, &scene, mirror.data());
    QCOMPARE(fm.crossings(), 4);
    QVERIFY(fm.caption().contains("Each group flips"));

    // A positive spread crosses nothing, whatever Cross says -- and says so.
    QScopedPointer<QLCPalette> open(lineAim(&doc, edge, 5.0, 2, LineSpread::SwapSides));
    const LineFormation fo = LineFormation::build(&doc, &scene, open.data());
    QCOMPARE(fo.crossings(), 0);
    QVERIFY(fo.crossIdle());
    QVERIFY(fo.caption().contains("Cross acts below 0"));

    // Each light on its own is not grouped: no group colours.
    QScopedPointer<QLCPalette> each(lineAim(&doc, edge, -5.0, 1, LineSpread::SwapSides));
    const LineFormation fe = LineFormation::build(&doc, &scene, each.data());
    QVERIFY(!fe.grouped());
    QCOMPARE(fe.colourFor(fe.lights().first()), LineFormation::pointColour());
    QCOMPARE(fe.crossings(), 28);

    // The strip and the plan draw that model.
    LineSpreadPreview strip(&doc);
    strip.setLook(&scene, swap.data());
    QCOMPARE(strip.dots().count(), 8);
    for (int i = 0; i < 8; ++i)
    {
        QCOMPARE(strip.dots().at(i).group, fs.lights().at(i).group);
        QCOMPARE(strip.dots().at(i).fraction, fs.lights().at(i).fraction);
    }
    LinePlanView plan(&doc);
    plan.setFormation(&scene, swap.data());
    QCOMPARE(plan.beams().count(), 8);
    for (const LinePlanView::Beam &b : plan.beams())
        QCOMPARE(b.colour, LineFormation::groupColour(b.group));
    QCOMPARE(plan.caption(), fs.caption());
    // Painting works offscreen, small and big.
    strip.resize(300, 84);
    QVERIFY(!strip.grab().isNull());
    plan.resize(260, 170);
    QVERIFY(!plan.grab().isNull());

    // Every beam and connector ends in an arrowhead, so source and
    // destination read: a filled triangle whose point is at the landing end.
    QVERIFY(!strip.arrows().isEmpty());
    QVERIFY(!plan.arrows().isEmpty());
    QVERIFY(plan.arrows().count() <= plan.beams().count());
    for (const QPolygonF &a : plan.arrows())
    {
        QCOMPARE(a.count(), 3);
        // Its base is behind the point, along the beam: the point is the apex.
        const QPointF mid = (a.at(1) + a.at(2)) / 2.0;
        QVERIFY(QLineF(mid, a.at(0)).length() > 3.0);
    }
    // The helper: the point at the tip, the base back along the line.
    const QPolygonF head = LineFormation::arrowhead(QPointF(0, 0), QPointF(0, 20), 6.0);
    QCOMPARE(head.count(), 3);
    QCOMPARE(head.at(0), QPointF(0, 20));
    QCOMPARE(head.at(1).y(), 14.0);
    QCOMPARE(head.at(2).y(), 14.0);
    QVERIFY(LineFormation::arrowhead(QPointF(0, 0), QPointF(0, 2), 6.0).isEmpty());

    // No scene: six stand-ins, grouped as the look says.
    const LineFormation stand = LineFormation::build(&doc, nullptr, swap.data());
    QCOMPARE(stand.lights().count(), 6);
    QCOMPARE(stand.groupCount(), 3);
}

void LookEditor_Test::planNamesNeverOverlap()
{
    Doc doc(this);
    StageTarget *edge = formationLine(&doc);
    Scene scene(&doc);
    // Four trusses, two spots on each, hung close together.
    for (int t = 0; t < 4; ++t)
        for (int k = 0; k < 2; ++k)
        {
            Fixture *f = formationSpot(&doc, QString("T%1-S%2").arg(t + 1).arg(k + 1),
                                       -4.0 + t * 2.6 + k * 0.4);
            QVERIFY(f != nullptr);
            scene.addFixture(f->id());
        }
    QScopedPointer<QLCPalette> aim(lineAim(&doc, edge, -5.0, 2, LineSpread::SwapSides));
    LinePlanView plan(&doc);
    plan.resize(700, 500);
    plan.setFormation(&scene, aim.data());
    QVERIFY(plan.drawScale() >= 1.6);
    QVERIFY(!plan.grab().isNull());
    const QList<QRectF> rects = plan.labelRects();
    QCOMPARE(rects.count(), 8);
    for (int i = 0; i < rects.count(); ++i)
    {
        QVERIFY(QRectF(plan.rect()).contains(rects.at(i)));
        for (int j = i + 1; j < rects.count(); ++j)
            QVERIFY2(!rects.at(i).intersects(rects.at(j)),
                     qPrintable(QString("names %1 and %2 overlap").arg(i).arg(j)));
    }
}

namespace
{
/* A 16-bit mover hung (or standing) at @p at (metres); @p indexed: its pan
   points with DMX 0-127 and spins above, as a two-arm beam's does. */
Fixture *landingSpot(Doc *doc, const QString &name, const QVector3D &at, bool standing, bool indexed)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Landing " + name);
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("4ch");
    struct { const char *name; QLCChannel::Group group; QLCChannel::ControlByte byte; } chans[] = {
        { "Pan", QLCChannel::Pan, QLCChannel::MSB }, { "Pan fine", QLCChannel::Pan, QLCChannel::LSB },
        { "Tilt", QLCChannel::Tilt, QLCChannel::MSB }, { "Tilt fine", QLCChannel::Tilt, QLCChannel::LSB } };
    QLCFixtureHead head;
    for (int k = 0; k < 4; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(chans[k].name);
        ch->setGroup(chans[k].group);
        ch->setControlByte(chans[k].byte);
        if (indexed && k == 0)
        {
            QLCCapability *point = new QLCCapability(0, 127, "Pan");
            point->setPreset(QLCCapability::RotationIndexed);
            ch->addCapability(point);
            ch->addCapability(new QLCCapability(128, 255, "Spin"));
        }
        def->addChannel(ch);
        mode->insertChannel(ch, k);
        head.addChannel(k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    mode->insertHead(-1, head);
    def->addMode(mode);
    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(quint32(10 + doc->fixtures().size() * 4));
    if (!doc->addFixture(fxi))
        return nullptr;
    MonitorProperties *mp = doc->monitorProperties();
    mp->setFixturePosition(fxi->id(), 0, 0, QVector3D(at.x() * 1000.0f, at.y() * 1000.0f, at.z()));
    if (standing)
    {
        FixtureRigProps rp = mp->fixtureRigProps(fxi->id());
        rp.mountingType = Truss::FloorMounted;
        mp->setFixtureRigProps(fxi->id(), rp);
    }
    return fxi;
}

StageTarget *pointTarget(Doc *doc, const QString &name, const QVector3D &at)
{
    StageTarget *t = doc->monitorProperties()->addStageTarget();
    t->setName(name);
    t->setPosition(at);
    return t;
}
}

void LookEditor_Test::aStoredPositionLandsWhereItAims()
{
    Doc doc(this);
    MonitorProperties *mp = doc.monitorProperties();
    mp->setGridUnits(MonitorProperties::Meters);
    mp->setGridSize(QVector3D(10, 3, 8));       // 10 m wide, 8 m deep
    Fixture *hung = landingSpot(&doc, "Hung", QVector3D(5, 1, 6), false, false);
    Fixture *stand = landingSpot(&doc, "Stand", QVector3D(2, 6, 0.4f), true, false);
    Fixture *index = landingSpot(&doc, "Index", QVector3D(8, 1, 5), false, true);
    QVERIFY(hung != nullptr && stand != nullptr && index != nullptr);
    StageTarget *mark = pointTarget(&doc, "Announcer", QVector3D(3.5f, 4.0f, 0.0f));

    // What an Aim at the mark writes, read back the other way: on the mark.
    QLCPalette aim(QLCPalette::Aim);
    aim.setStageTargetId(mark->id());
    for (Fixture *f : { hung, stand, index })
    {
        const QList<SceneValue> values = aim.valuesFromFixtures(&doc, { f->id() });
        QVERIFY2(!values.isEmpty(), qPrintable(f->name()));
        const BeamLanding::Result r = BeamLanding::land(&doc, f->id(), values);
        QVERIFY2(r.kind == BeamLanding::OnStage, qPrintable(f->name()));
        const QVector3D want = mp->targetAimPoint(mark);
        const double miss = double((r.point - want).toVector2D().length());
        QVERIFY2(miss < 0.10, qPrintable(QString("%1 lands %2 m off").arg(f->name()).arg(miss)));
        QCOMPARE(r.target, mark->id());
        QCOMPARE(BeamLanding::describe(&doc, r), QString::fromUtf8("\u2248 Announcer"));
    }

    // Move the mark away: the same values land on the stage, named by area.
    const QList<SceneValue> onMark = aim.valuesFromFixtures(&doc, { hung->id() });
    mark->setPosition(QVector3D(9.5f, 0.5f, 0.0f));
    BeamLanding::Result r = BeamLanding::land(&doc, hung->id(), onMark);
    QCOMPARE(r.target, StageTarget::invalidId());
    QCOMPARE(BeamLanding::describe(&doc, r), QString("centre stage"));
    QCOMPARE(BeamLanding::areaAt(&doc, QVector3D(9, 7.5f, 0)), QString("DS left"));
    QCOMPARE(BeamLanding::areaAt(&doc, QVector3D(0.5f, 0.5f, 0)), QString("US right"));

    // Aimed outside the stage: off stage, or into the audience.
    StageTarget *wing = pointTarget(&doc, "Wing", QVector3D(-3.0f, 4.0f, 0.0f));
    QLCPalette toWing(QLCPalette::Aim);
    toWing.setStageTargetId(wing->id());
    const QList<SceneValue> wingValues = toWing.valuesFromFixtures(&doc, { hung->id() });
    StageTarget *house = pointTarget(&doc, "House", QVector3D(5.0f, 11.0f, 0.0f));
    QLCPalette toHouse(QLCPalette::Aim);
    toHouse.setStageTargetId(house->id());
    const QList<SceneValue> houseValues = toHouse.valuesFromFixtures(&doc, { stand->id() });
    wing->setPosition(QVector3D(50, 50, 0));
    house->setPosition(QVector3D(50, 50, 0));
    QCOMPARE(BeamLanding::land(&doc, hung->id(), wingValues).kind, BeamLanding::OffStage);
    QCOMPARE(BeamLanding::land(&doc, stand->id(), houseValues).kind, BeamLanding::IntoAudience);

    // Hung, tilted fully back: up into the air.
    const quint32 tiltCh = hung->channelNumber(QLCChannel::Tilt, QLCChannel::MSB);
    QList<SceneValue> up { SceneValue(hung->id(), tiltCh, 0) };
    QCOMPARE(BeamLanding::land(&doc, hung->id(), up).kind, BeamLanding::IntoAir);

    QList<BeamLanding::Result> all { BeamLanding::land(&doc, hung->id(), onMark),
                                     BeamLanding::land(&doc, hung->id(), wingValues),
                                     BeamLanding::land(&doc, stand->id(), houseValues),
                                     BeamLanding::land(&doc, hung->id(), up) };
    QCOMPARE(BeamLanding::summary(&doc, all),
             QString("lands centre stage, 1 head off stage, 1 head into the audience, 1 head into the air"));

    // The PanTilt page: with lights in context, the plan and the line.
    const quint32 pid = doc.paletteOps()->create(QLCPalette::PanTilt, QString(), "UST-0", QVariantList(),
                                                 [](QLCPalette *p) { p->setValue(270, 200); });
    // A twin hung on the same spot lands on the same point as Hung.
    Fixture *twin = landingSpot(&doc, "Twin", QVector3D(5, 1, 6), false, false);
    Scene scene(&doc);
    scene.addFixture(hung->id());
    scene.addFixture(index->id());
    scene.addFixture(twin->id());
    LookEditor ed(&doc);
    ed.resize(800, 400);
    ed.show();
    ed.setContextScene(&scene);
    ed.setPalette(pid);
    QApplication::processEvents();
    LinePlanView *plan = ed.findChild<LinePlanView *>("lookPanTiltPlan");
    QVERIFY(plan != nullptr && plan->isVisible());
    QVERIFY(plan->formation().isRawPosition());
    QCOMPARE(plan->beams().count(), 3);
    QLabel *line = ed.findChild<QLabel *>("lookPanTiltLanding");
    QVERIFY(line != nullptr && !line->text().isEmpty());
    plan->resize(600, 300);
    QVERIFY(!plan->grab().isNull());
    // Hung and Twin's points are drawn once ("x2"), Index's on its own.
    QList<int> marks = plan->pointMarks();
    std::sort(marks.begin(), marks.end());
    QCOMPARE(marks, QList<int>({ 1, 2 }));
    // A stored position's beams point at where they land too.
    QVERIFY(!plan->arrows().isEmpty());
    // No context: nothing to draw, so nothing shown.
    ed.setContextScene(nullptr);
    QVERIFY(!plan->isVisible());
}

/* "person height needs to be in feet/inches as set in settings": a length
   shows in the unit Settings -> Units picks and takes a length typed in any
   unit, kept as the caller keeps it (an effect's cm, a target's metres). */
void LookEditor_Test::aLengthShowsAndTakesTheChosenUnit()
{
    // An effect's "cm" setting, in feet: 167.64 cm is 5' 6".
    LengthSpinBox feet(10.0, nullptr, MeasurementUnits::Feet);
    feet.setRange(50, 250);
    feet.setValue(167.64);
    QCOMPARE(feet.text(), QString("5' 6\""));
    QCOMPARE(feet.valueFromText("5' 6\""), 167.64);
    QVERIFY(qAbs(feet.valueFromText("5.5 ft") - 167.64) < 1e-6);
    QVERIFY(qAbs(feet.valueFromText("66 in") - 167.64) < 1e-6);
    QVERIFY(qAbs(feet.valueFromText("1.676 m") - 167.6) < 1e-6);
    QVERIFY(qAbs(feet.valueFromText("5.5") - 167.64) < 1e-6);        // bare: the chosen unit
    QString typed = "5' 6\"";
    int pos = 0;
    QCOMPARE(int(feet.validate(typed, pos)), int(QValidator::Acceptable));
    typed = "12'";                                                    // past its 250 cm
    QCOMPARE(int(feet.validate(typed, pos)), int(QValidator::Intermediate));
    typed = "tall";
    QCOMPARE(int(feet.validate(typed, pos)), int(QValidator::Intermediate));

    // In metres and in centimetres.
    LengthSpinBox metres(10.0, nullptr, MeasurementUnits::Metres);
    metres.setRange(50, 250);
    metres.setValue(167.64);
    QCOMPARE(metres.text(), QString("1.6764 m"));
    QVERIFY(qAbs(metres.valueFromText("5' 6\"") - 167.64) < 1e-6);
    LengthSpinBox cm(10.0, nullptr, MeasurementUnits::Centimetres);
    cm.setRange(50, 250);
    cm.setValue(170);
    QCOMPARE(cm.text(), QString("170 cm"));

    /* The rig's metre fields show Settings > Units and take a length typed
       in any unit: a bare number is in the chosen unit. */
    const MeasurementUnits::LengthUnit before = MeasurementUnits::preferredLength();
    MeasurementUnits::setPreferredLength(MeasurementUnits::Metres);
    FeetInchesSpinBox m(false);
    m.setRange(0.5, 3.0);
    QVERIFY(qAbs(m.valueFromText("5' 6\"") - 1.6764) < 1e-6);
    QVERIFY(qAbs(m.valueFromText("66 in") - 1.6764) < 1e-6);
    QVERIFY(qAbs(m.valueFromText("5.5 ft") - 1.6764) < 1e-6);
    QVERIFY(qAbs(m.valueFromText("1.676") - 1.676) < 1e-6);
    QVERIFY(qAbs(m.valueFromText("1.676 m") - 1.676) < 1e-6);
    typed = "5' 6\"";
    QCOMPARE(int(m.validate(typed, pos)), int(QValidator::Acceptable));
    m.setValue(1.6764);
    QCOMPARE(m.text(), QString("1.6764 m"));
    MeasurementUnits::setPreferredLength(MeasurementUnits::Feet);
    FeetInchesSpinBox f(true);
    f.setRange(0.5 * 3.28084, 3.0 * 3.28084);
    f.setValue(1.6764 * 3.28084);
    QCOMPARE(f.text(), QString("5' 6\""));
    FeetInchesSpinBox fm(false);                 // metres inside, feet shown
    fm.setRange(0.5, 3.0);
    fm.setValue(1.6764);
    QCOMPARE(fm.text(), QString("5' 6\""));
    MeasurementUnits::setPreferredLength(MeasurementUnits::Millimetres);
    QCOMPARE(fm.text(), QString("5' 6\""));      // a field keeps the unit it was made in
    FeetInchesSpinBox mm(false);
    mm.setRange(0.5, 3.0);
    mm.setValue(1.6764);
    QCOMPARE(mm.text(), QString("1676.4 mm"));
    MeasurementUnits::setPreferredLength(before);

    // The follow spot's Person Height in the effect editor: in feet when
    // Settings -> Units says feet, kept in cm.
    const MeasurementUnits::LengthUnit was = MeasurementUnits::preferredLength();
    MeasurementUnits::setPreferredLength(MeasurementUnits::Feet);
    Doc doc(this);
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("Follow");
    look->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/followspot.js");
    look->setEffectParamValue("personHeight", 167.64);
    QVERIFY(doc.addPalette(look));
    {
        LookEditor ed(&doc);
        ed.setPalette(look->id());
        QDoubleSpinBox *height = ed.findChild<QDoubleSpinBox *>("effectParam_personHeight");
        QVERIFY(height != nullptr);
        QVERIFY(dynamic_cast<LengthSpinBox *>(height) != nullptr);
        QCOMPARE(height->text(), QString("5' 6\""));
        height->setValue(height->valueFromText("6'"));
        QVERIFY(qAbs(look->effectParamValues().value("personHeight") - 182.88) < 1e-6);
        QVERIFY2(height->toolTip().contains("from 1' 7 5/8\" to 8' 2 3/8\""), qPrintable(height->toolTip()));
    }
    MeasurementUnits::setPreferredLength(was);
}

/* "visual indication ... of followspot enable and activity": the follow-spot
   effect's editor says whether it is on and what it is doing, with a light. */
void LookEditor_Test::theFollowSpotEditorSaysWhatItIsDoing()
{
    Doc doc(this);
    StageTarget *who = doc.monitorProperties()->addStageTarget();
    who->setName("ES-Witch");
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, &doc);
    look->setName("Follow");
    look->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/followspot.js");
    QVERIFY(doc.addPalette(look));
    LookEditor ed(&doc);
    ed.setPalette(look->id());
    QLabel *say = ed.findChild<QLabel *>("followSpotText");
    QLabel *light = ed.findChild<QLabel *>("followSpotLight");
    QVERIFY(say != nullptr && light != nullptr);
    QVERIFY2(say->text().contains("Not playing"), qPrintable(say->text()));

    using Status = ProgrammerController::FollowSpotStatus;
    ProgrammerController *pc = doc.programmer();
    auto set = [&](Status::State state) {
        Status st;
        st.state = state;
        st.paletteId = look->id();
        st.targetId = who->id();
        st.sceneId = 1;
        st.live = true;
        pc->m_followStatus = st;
        emit pc->followSpotStatusChanged();
    };
    set(Status::NoStick);
    QVERIFY2(say->text().contains("ES-Witch") && say->text().contains("doesn't move"), qPrintable(say->text()));
    QCOMPARE(light->property("state").toInt(), int(Status::NoStick));
    set(Status::Holding);
    QVERIFY2(say->text().contains("holding"), qPrintable(say->text()));
    QCOMPARE(light->property("state").toInt(), int(Status::Holding));
    set(Status::Moving);
    QVERIFY2(say->text().contains("Following ES-Witch · moving"), qPrintable(say->text()));
    QCOMPARE(light->property("state").toInt(), int(Status::Moving));
    QVERIFY(light->styleSheet().contains("#3ec46d"));
    set(Status::Off);
    QVERIFY(say->text().contains("Not playing"));
}

