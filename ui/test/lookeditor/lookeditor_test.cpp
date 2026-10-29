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

#define private public
#include "lookeditor.h"
#include "matriximage.h"
#include "effectbrowser.h"
#undef private
#include <QListWidget>
#include <QLineEdit>
#include <QTabBar>
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
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
#include "qlcfixturedefcache.h"
#include "qlcfile.h"
#include "paletteops.h"

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
    QCOMPARE(ed.m_newScriptButton->text(), QString::fromUtf8("New script…"));
    QCOMPARE(ed.m_editScriptButton->text(), QString::fromUtf8("Edit script…"));
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
    QCOMPARE(ed.m_effectChangeButton->text(), QString("Change effect…"));

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
    // Only what runs here too: the ported ones.
    const QStringList without = inWledTab();
    QVERIFY2(!without.isEmpty(), "the ported WLED effects still run on the par");
    // ...each one a port: an effect script that says it is WLED's.
    EffectScriptCache *cache = doc.effectScriptRunner()->cache();
    for (const QString &n : without)
        QVERIFY2(!cache->scriptPathForWled(n).isEmpty(), qPrintable(n + " has no port"));
    ed.closeEffectPicker(true);

    // A WLED device among the targets: all of WLED's own.
    scene->addFixture(lantern->id());
    ed.openEffectPicker();
    QVERIFY(browser->wledTargets());
    const QStringList with = inWledTab();
    QVERIFY2(with.size() > without.size() + 10,
             qPrintable(QString("%1 with a WLED target, %2 without").arg(with.size()).arg(without.size())));
    ed.closeEffectPicker(true);
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
