/*
  Q Light Controller Plus - Test Unit
  effectdesigner_test.cpp

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

#include <QXmlStreamReader>
#include <QApplication>
#include <QMenu>
#include <QTreeWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QPointer>
#include <QPainter>
#include <QLineEdit>
#include <QToolButton>
#include <QTreeWidgetItemIterator>
#include <QtTest>

#define private public
#include "showtreeview.h"
#undef private
#include "effectdesigner_test.h"
#include "effectdesigner.h"
#include "effectspage.h"
#include "programmingmanager.h"
#include "workbench.h"
#include "programmingoutline.h"
#include "paletteops.h"
#include "rgbmatrix.h"
#include "effectbrowser.h"
#include "lookeditor.h"
#include "efxeditor.h"
#include "showtreecontroller.h"
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "effectpresetcache.h"
#include "functionparent.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "collection.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "efx.h"
#include "doc.h"

using namespace ShowTree;

void EffectDesigner_Test::initTestCase()
{
    // Presets are written under the user's folder: a throwaway one here.
    QVERIFY(m_home.isValid());
    qputenv("HOME", m_home.path().toUtf8());
}

void EffectDesigner_Test::init()
{
    m_doc = new Doc(this);
    m_doc->effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    m_fixtures.clear();
    for (int i = 0; i < 6; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Dim %1").arg(i + 1));
        fxi->setAddress(quint32(i));
        fxi->setUniverse(0);
        fxi->setChannels(1);
        m_doc->addFixture(fxi);
        m_fixtures << fxi->id();
    }
    m_doc->resetModified();
}

void EffectDesigner_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    delete m_doc;
    m_doc = nullptr;
}

FixtureGroup *EffectDesigner_Test::group(const QString &name, int from, int count)
{
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName(name);
    m_doc->addFixtureGroup(g);
    for (int i = from; i < from + count; ++i)
        g->assignFixture(m_fixtures[i]);
    return g;
}

static QLCPalette *effectPalette(Doc *doc, const QString &name, const QString &script)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Effect, doc);
    p->setName(name);
    p->setScriptPath(doc->effectScriptRunner()->cache()->scriptPath(script));
    doc->addPalette(p);
    return p;
}

static const Scene *layerScene(Doc *doc, quint32 cueId)
{
    const Collection *coll = qobject_cast<Collection *>(doc->function(doc->showTree()->cue(cueId)->collection));
    return coll == nullptr || coll->functions().isEmpty() ? nullptr
                                                          : qobject_cast<Scene *>(doc->function(coll->functions().first()));
}

void EffectDesigner_Test::aNewEffectOnTheBenchSavesAsAPalette()
{
    FixtureGroup *towers = group("Towers", 0, 3);
    const int palettes = m_doc->palettes().count();
    const int functions = m_doc->functions().count();
    m_doc->resetModified();             // as if just saved

    EffectDesigner ed(m_doc);
    ed.setTryOnGroups({ towers->id() });
    QVERIFY(ed.isBench());
    ed.setEffect(QLCPalette::invalidId());

    // A draft: a one-off nobody lists, played by a hidden scene on the bench.
    QLCPalette *draft = ed.draft();
    QVERIFY(draft != nullptr);
    QVERIFY(m_doc->showTree()->isInlinePalette(draft->id()));
    const Scene *preview = qobject_cast<Scene *>(m_doc->function(ed.previewSceneId()));
    QVERIFY(preview != nullptr);
    QVERIFY(!preview->isVisible());
    QCOMPARE(preview->fixtureGroups(), QList<quint32>({ towers->id() }));
    QCOMPARE(preview->palettes(), QList<quint32>({ draft->id() }));
    QVERIFY(!m_doc->isModified());
    QVERIFY(ed.effectName().isEmpty());
    QVERIFY(!ed.findChild<QPushButton *>("edSavePalette")->isEnabled());   // nothing picked yet
    QVERIFY(ed.findChild<QPushButton *>("edKeep")->isHidden());           // no layer: no one-off

    // Picked in the browser: the draft runs it.
    QVERIFY(ed.pickEffect("Dimmer Phaser"));
    QVERIFY(draft->scriptPath().endsWith("dimmer-phaser.js"));
    QCOMPARE(ed.effectName(), QString("Dimmer Phaser"));
    QVERIFY(ed.isModified());
    QVERIFY(ed.findChild<QPushButton *>("edSavePalette")->isEnabled());

    QSignalSpy chosen(&ed, &EffectDesigner::effectChosen);
    const quint32 pid = ed.saveAsPalette("Phaser Towers");
    QCOMPARE(pid, draft->id());
    QCOMPARE(chosen.count(), 1);
    QCOMPARE(chosen.first().at(2).value<EffectDesigner::Outcome>(), EffectDesigner::NewPalette);
    // Named and filed as the show's palettes are; no longer a one-off.
    QVERIFY(!m_doc->showTree()->isInlinePalette(pid));
    QCOMPARE(m_doc->palette(pid)->name(), QString("Phaser Towers"));
    QCOMPARE(m_doc->palette(pid)->path(), m_doc->showTree()->suggestPaletteFolder(QLCPalette::Effect));
    QVERIFY(m_doc->showTree()->canUndo());
    // The preview is gone; only the palette stays.
    QCOMPARE(ed.previewSceneId(), Function::invalidId());
    QCOMPARE(ed.draftId(), QLCPalette::invalidId());
    QCOMPARE(m_doc->functions().count(), functions);
    QCOMPARE(m_doc->palettes().count(), palettes + 1);
}

void EffectDesigner_Test::cancelLeavesNoTrace()
{
    group("Towers", 0, 3);
    const int palettes = m_doc->palettes().count();
    const int functions = m_doc->functions().count();
    m_doc->resetModified();             // as if just saved
    {
        EffectDesigner ed(m_doc);
        ed.setEffect(QLCPalette::invalidId());
        QVERIFY(ed.pickEffect("Dimmer Phaser"));
        QCOMPARE(m_doc->functions().count(), functions + 1);
        QSignalSpy cancelled(&ed, &EffectDesigner::cancelled);
        ed.cancel();
        QCOMPARE(cancelled.count(), 1);
        QCOMPARE(m_doc->palettes().count(), palettes);
        QCOMPARE(m_doc->functions().count(), functions);

        // And going away mid-design does the same.
        ed.setEffect(QLCPalette::invalidId());
        QVERIFY(ed.pickEffect("Dimmer Phaser"));
    }
    QCOMPARE(m_doc->palettes().count(), palettes);
    QCOMPARE(m_doc->functions().count(), functions);
    QVERIFY(!m_doc->isModified());
}

void EffectDesigner_Test::thePreviewPlaysOnlyInDesignOnScreen()
{
    FixtureGroup *towers = group("Towers", 0, 3);
    EffectDesigner ed(m_doc);
    ed.setTryOnGroups({ towers->id() });
    ed.setEffect(QLCPalette::invalidId());
    QVERIFY(ed.pickEffect("Dimmer Phaser"));
    QVERIFY(!ed.isPreviewing());                  // not on screen
    ed.resize(1100, 700);
    ed.show();
    QVERIFY(QTest::qWaitForWindowExposed(&ed));
    QVERIFY(ed.isPreviewing());
    QVERIFY2(ed.previewText().contains("On the rig"), qPrintable(ed.previewText()));
    QVERIFY(m_doc->effectScriptRunner()->failures().value(ed.draftId()).isEmpty());

    m_doc->setMode(Doc::Operate);
    QTRY_VERIFY(!ed.isPreviewing());
    QVERIFY(ed.previewText().startsWith("Live"));
    m_doc->setMode(Doc::Design);
    QTRY_VERIFY(ed.isPreviewing());
    ed.hide();
    QTRY_VERIFY(!ed.isPreviewing());
    ed.cancel();
}

void EffectDesigner_Test::keepOnALayerMakesAOneOff()
{
    FixtureGroup *towers = group("Towers", 0, 3);
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, m_doc);
    full->setName("Full");
    full->setValue(255);
    m_doc->addPalette(full);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "Announcer");
    Target t;
    t.groups << towers->id();
    const quint32 l1 = st->addLayer(c1, t);
    st->setLayerLook(c1, l1, st->lookForPalette(full->id()));
    st->clearUndo();

    EffectLayerDialog dlg(m_doc, c1, l1, t);
    EffectDesigner *ed = dlg.designer();
    QVERIFY(!ed->isBench());
    QCOMPARE(ed->targetGroups(), QList<quint32>({ towers->id() }));
    QVERIFY(!ed->findChild<QPushButton *>("edKeep")->isHidden());
    // The layer's Dimmer plays under it in the preview.
    const Scene *preview = qobject_cast<Scene *>(m_doc->function(ed->previewSceneId()));
    QCOMPARE(preview->palettes(), QList<quint32>({ full->id(), ed->draftId() }));

    QVERIFY(ed->pickEffect("Dimmer Phaser"));
    const quint32 pid = ed->keepOneOff();
    QVERIFY(pid != QLCPalette::invalidId());
    QVERIFY(st->isInlinePalette(pid));
    QVERIFY2(m_doc->palette(pid)->name().startsWith("Dimmer Phaser · 1 Announcer"),
             qPrintable(m_doc->palette(pid)->name()));
    const QString key = effectKey(st->effectKind(m_doc->palette(pid)));
    const Look *look = st->cue(c1)->layer(l1)->look(key);
    QVERIFY(look != nullptr);
    QCOMPARE(look->palettes, QList<quint32>({ pid }));
    // The layer's scene plays it, the preview is gone, one undo step.
    QVERIFY(layerScene(m_doc, c1)->palettes().contains(pid));
    QCOMPARE(m_doc->function(ed->previewSceneId()), nullptr);
    QVERIFY(st->canUndo());
    QVERIFY(st->undoText().contains("Design effect"));
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c1)->layer(l1)->look(key), nullptr);
    QVERIFY(!st->canUndo());
}

void EffectDesigner_Test::aLayersOneOffIsEditedInPlace()
{
    FixtureGroup *towers = group("Towers", 0, 3);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << towers->id();
    const quint32 l1 = st->addLayer(c1, t);
    quint32 first;
    {
        EffectLayerDialog dlg(m_doc, c1, l1, t);
        QVERIFY(dlg.designer()->pickEffect("Dimmer Phaser"));
        first = dlg.designer()->keepOneOff();
    }
    const int palettes = m_doc->palettes().count();
    const QString key = effectKey(st->effectKind(m_doc->palette(first)));
    {
        EffectLayerDialog dlg(m_doc, c1, l1, t, key);
        EffectDesigner *ed = dlg.designer();
        QCOMPARE(ed->originalId(), first);
        QCOMPARE(dlg.previousLook().palettes, QList<quint32>({ first }));
        QVERIFY(ed->draftId() != first);            // a copy, while designing
        QCOMPARE(ed->draft()->scriptPath(), m_doc->palette(first)->scriptPath());
        ed->draft()->setEffectParamValue("speed", 7);
        QCOMPARE(ed->keepOneOff(), first);
    }
    QCOMPARE(m_doc->palettes().count(), palettes);   // no second one-off
    QCOMPARE(m_doc->palette(first)->effectParamValues().value("speed"), 7.0);
    QCOMPARE(st->cue(c1)->layer(l1)->look(key)->palettes, QList<quint32>({ first }));
}

void EffectDesigner_Test::aNamedEffectIsACopyUntilUpdated()
{
    FixtureGroup *towers = group("Towers", 0, 3);
    QLCPalette *phaser = effectPalette(m_doc, "Phaser", "Dimmer Phaser");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << towers->id();
    const quint32 l1 = st->addLayer(c1, t);
    st->setLayerLook(c1, l1, st->lookForPalette(phaser->id()));

    EffectLayerDialog dlg(m_doc, c1, l1, t);
    EffectDesigner *ed = dlg.designer();
    QCOMPARE(ed->originalId(), phaser->id());
    QPushButton *update = ed->findChild<QPushButton *>("edUpdate");
    QVERIFY(!update->isHidden());
    QVERIFY(update->text().contains("Phaser"));
    QVERIFY(!update->isEnabled());                  // nothing changed yet
    QVERIFY(!ed->isModified());

    // A change is the draft's, not the palette's.
    ed->draft()->setEffectParamValue("speed", 3);
    emit ed->lookEditor()->paletteValueChanged(ed->draftId());
    QVERIFY(ed->isModified());
    QVERIFY(update->isEnabled());
    QVERIFY(!phaser->effectParamValues().contains("speed"));

    QSignalSpy chosen(ed, &EffectDesigner::effectChosen);
    QVERIFY(ed->updateOriginal());
    QCOMPARE(chosen.first().at(2).value<EffectDesigner::Outcome>(), EffectDesigner::UpdatedPalette);
    QCOMPARE(phaser->effectParamValues().value("speed"), 3.0);
    QCOMPARE(phaser->name(), QString("Phaser"));
    // The layer still plays it, as it did.
    const QString key = effectKey(st->effectKind(phaser));
    QCOMPARE(st->cue(c1)->layer(l1)->look(key)->palettes, QList<quint32>({ phaser->id() }));
}

void EffectDesigner_Test::anotherKindReplacesTheLayersEffect()
{
    FixtureGroup *towers = group("Towers", 0, 3);
    QLCPalette *chase = effectPalette(m_doc, "Chase", "Color Chase");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << towers->id();
    const quint32 l1 = st->addLayer(c1, t);
    st->setLayerLook(c1, l1, st->lookForPalette(chase->id()));
    const QString oldKey = effectKey(st->effectKind(chase));

    EffectLayerDialog dlg(m_doc, c1, l1, t);
    QVERIFY(dlg.designer()->pickEffect("Dimmer Phaser"));
    const quint32 pid = dlg.designer()->keepOneOff();
    const QString newKey = effectKey(st->effectKind(m_doc->palette(pid)));
    QVERIFY2(oldKey != newKey, qPrintable(oldKey));
    QCOMPARE(st->cue(c1)->layer(l1)->look(oldKey), nullptr);
    QCOMPARE(st->cue(c1)->layer(l1)->look(newKey)->palettes, QList<quint32>({ pid }));
    QVERIFY(m_doc->palette(chase->id()) != nullptr);      // the palette itself stays
}

void EffectDesigner_Test::theEffectsOwnColourIsKept()
{
    FixtureGroup *towers = group("Towers", 0, 3);
    QLCPalette *red = new QLCPalette(QLCPalette::Color, m_doc);
    red->setName("Red");
    red->setValue(QLCPalette::colorToString(Qt::red, QColor(Qt::black)));
    m_doc->addPalette(red);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << towers->id();
    const quint32 l1 = st->addLayer(c1, t);

    EffectLayerDialog dlg(m_doc, c1, l1, t);
    EffectDesigner *ed = dlg.designer();
    QVERIFY(ed->pickEffect("Color Chase"));
    QComboBox *own = ed->findChild<QComboBox *>("edOwnColour");
    const int at = own->findText("Red");
    QVERIFY(at > 0);
    own->setCurrentIndex(at);
    emit own->activated(at);
    QCOMPARE(ed->ownLooks(), QList<quint32>({ red->id() }));
    const Scene *preview = qobject_cast<Scene *>(m_doc->function(ed->previewSceneId()));
    QCOMPARE(preview->palettes(), QList<quint32>({ ed->draftId(), red->id() }));   // scoped under it

    const quint32 pid = ed->keepOneOff();
    const QString key = effectKey(st->effectKind(m_doc->palette(pid)));
    QCOMPARE(st->cue(c1)->layer(l1)->look(key)->own, QList<quint32>({ red->id() }));
    QCOMPARE(layerScene(m_doc, c1)->palettes(), QList<quint32>({ pid, red->id() }));
}

void EffectDesigner_Test::theShowTreeOpensItOnALayer()
{
    FixtureGroup *towers = group("Towers", 0, 3);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << towers->id();
    const quint32 l1 = st->addLayer(c1, t);

    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.selectNode(l1, c1));

    // Drive the menus and the window as a person would, never waiting on one.
    QStringList menuActions;
    QString wanted;
    bool triggered = false, designed = false;
    QTimer driver;
    connect(&driver, &QTimer::timeout, this, [&]() {
        if (EffectLayerDialog *dlg = qobject_cast<EffectLayerDialog *>(QApplication::activeModalWidget()))
        {
            if (!designed && dlg->designer()->pickEffect("Dimmer Phaser"))
            {
                designed = true;
                dlg->designer()->keepOneOff();
            }
            return;
        }
        QMenu *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (menu == nullptr)
            return;
        if (menuActions.isEmpty())
            for (QAction *a : menu->findChildren<QAction *>())
                menuActions << a->objectName();
        QAction *a = wanted.isEmpty() ? nullptr : menu->findChild<QAction *>(wanted);
        if (a != nullptr && !triggered)
        {
            triggered = true;
            // Not from in here: the window it opens waits, and this timer
            // can't tick again until this returns.
            QTimer::singleShot(0, a, &QAction::trigger);
            return;
        }
        menu->close();
    });
    driver.start(20);

    // The layer's right-click menu offers it.
    QTreeWidgetItem *item = view.m_tree->currentItem();
    QVERIFY(item != nullptr);
    view.slotTreeContextMenu(view.m_tree->visualItemRect(item).center());
    QVERIFY2(menuActions.contains("stDesignEffectLayer"), qPrintable(menuActions.join(',')));

    // The Effect row's ⋯ menu too: designing there puts it on the layer.
    menuActions.clear();
    wanted = QStringLiteral("stDesignEffect");
    view.attributeMenu(c1, l1, Target(), t, QStringLiteral("Effect."), view.mapToGlobal(QPoint(10, 10)));
    QVERIFY(triggered);
    QVERIFY(designed);
    driver.stop();
    const Layer *layer = st->cue(c1)->layer(l1);
    bool hasEffect = false;
    for (const Look &l : layer->looks)
        if (isEffectKey(l.key) && !l.palettes.isEmpty()
            && m_doc->palette(l.palettes.first())->scriptPath().endsWith("dimmer-phaser.js"))
            hasEffect = true;
    QVERIFY(hasEffect);
    QVERIFY(st->previewedCue() == c1 || st->previewedCue() == invalidId());
}

void EffectDesigner_Test::saveAsPresetListsIt()
{
    const QString dir = EffectPresetCache::userPresetsDirectory().absolutePath();
    if (!dir.startsWith(QDir(m_home.path()).canonicalPath()) && !dir.startsWith(m_home.path()))
        QSKIP("the user's preset folder isn't under the test's HOME here");
    group("Towers", 0, 3);
    EffectDesigner ed(m_doc);
    ed.setEffect(QLCPalette::invalidId());
    QVERIFY(!ed.saveAsPreset("Nothing"));           // nothing picked
    QVERIFY(ed.pickEffect("Dimmer Phaser"));
    ed.draft()->setEffectParamValue("speed", 5);
    QSignalSpy saved(&ed, &EffectDesigner::presetSaved);
    QVERIFY(ed.saveAsPreset("Slow Phaser"));
    QCOMPARE(saved.count(), 1);
    QVERIFY(m_doc->effectScriptRunner()->presetCache()->contains("Slow Phaser"));
    QCOMPARE(ed.draft()->effectPreset(), QString("Slow Phaser"));
    QCOMPARE(ed.effectName(), QString("Slow Phaser"));
    QVERIFY(ed.draftId() != QLCPalette::invalidId());       // the design goes on
    // And it is offered to pick from.
    QApplication::processEvents();
    QVERIFY(ed.pickEffect("Slow Phaser"));
    ed.cancel();
}

void EffectDesigner_Test::theEffectsPageListsEveryEffect()
{
    FixtureGroup *towers = group("Towers", 0, 3);
    QLCPalette *phaser = effectPalette(m_doc, "Phaser", "Dimmer Phaser");
    phaser->setPath("Palettes/01-Song/");
    effectPalette(m_doc, "Loose", "Color Chase");
    const quint32 oneOff = m_doc->showTree()->createInlinePalette(QLCPalette::Effect, {}, "One-off");
    EFX *efx = new EFX(m_doc);
    efx->setName("Circle");
    m_doc->addFunction(efx);

    EffectsPage page(m_doc);
    page.designer()->setTryOnGroups({ towers->id() });
    page.resize(1300, 800);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    const QStringList rows = page.navigatorText();
    QVERIFY2(rows.contains("Effect palettes"), qPrintable(rows.join('\n')));
    QVERIFY(rows.contains("  ✦ Loose"));
    QVERIFY(rows.contains("  01-Song"));
    QVERIFY(rows.contains("    ✦ Phaser"));
    QVERIFY(rows.contains("Classic effects"));
    QVERIFY(rows.contains("  Circle [EFX]"));
    for (const QString &r : rows)
        QVERIFY2(!r.contains("One-off"), qPrintable(r));
    Q_UNUSED(oneOff);

    // The bench is ready to design on.
    QVERIFY(page.designer()->draftId() != QLCPalette::invalidId());
    QVERIFY(page.designer()->isBench());

    // A palette: a copy on the bench.
    QVERIFY(page.selectPalette(phaser->id()));
    QCOMPARE(page.designer()->originalId(), phaser->id());
    QVERIFY(page.designer()->isPreviewing());

    // An EFX opens in its own editor; the designer's draft goes.
    const int palettes = m_doc->palettes().count();
    QVERIFY(page.selectFunction(efx->id()));
    QCOMPARE(page.editedFunction(), efx->id());
    QVERIFY(page.findChild<EFXEditor *>() != nullptr);
    QCOMPARE(m_doc->palettes().count(), palettes - 1);

    // + New EFX: on the Try on groups' heads, opened.
    const quint32 made = page.newEfx();
    EFX *fresh = qobject_cast<EFX *>(m_doc->function(made));
    QVERIFY(fresh != nullptr);
    QCOMPARE(fresh->fixtures().count(), 3);
    QCOMPARE(page.editedFunction(), made);
    QTRY_VERIFY(page.navigatorText().contains(QString("  %1 [EFX]").arg(fresh->name())));

    // Filter.
    page.findChild<QLineEdit *>("effectsFilter")->setText("phas");
    const QStringList filtered = page.navigatorText();
    QVERIFY(filtered.contains("    ✦ Phaser"));
    QVERIFY(!filtered.contains("  ✦ Loose"));

    // Another show (File ▸ New): the editor and the draft go first, the Doc
    // deletes what they used.
    page.newEffect();
    QVERIFY(page.designer()->draftId() != QLCPalette::invalidId());
    QVERIFY(page.selectFunction(made));
    m_doc->clearContents();
    QCOMPARE(page.editedFunction(), Function::invalidId());
    QCOMPARE(page.findChild<EFXEditor *>(), nullptr);
    QCOMPARE(page.designer()->draftId(), QLCPalette::invalidId());
    QCOMPARE(page.designer()->previewSceneId(), Function::invalidId());
    page.hide();
}

static QTreeWidgetItem *effectsRow(QTreeWidget *list, const QString &name)
{
    for (QTreeWidgetItemIterator it(list); *it; ++it)
        if ((*it)->text(0) == name)
            return *it;
    return nullptr;
}

static QAction *menuAction(QMenu *menu, const QString &objectName)
{
    for (QAction *a : menu->actions())
        if (a->objectName() == objectName)
            return a;
    return nullptr;
}

void EffectDesigner_Test::escLeavesTheBench()
{
    FixtureGroup *towers = group("Towers", 0, 3);
    Scene *scene = new Scene(m_doc);
    scene->setName("Opened From");
    m_doc->addFunction(scene);
    scene->addFixtureGroup(towers->id());

    EffectDesigner::plugIntoWorkbench();
    ProgrammingManager pm(nullptr, m_doc);
    pm.resize(1500, 900);
    pm.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pm));
    pm.showFunction(scene->id());
    Workbench *bench = pm.workbench();
    QCOMPARE(bench->loadedFunction(), scene->id());

    // The bench, opened with the scene loaded: it is where Esc goes back to.
    QVERIFY(pm.designEffect(QLCPalette::invalidId()));
    QPointer<EffectDesigner> ed = pm.effectDesigner();
    QVERIFY(ed != nullptr);
    const quint32 draft = ed->draftId();
    QVERIFY(m_doc->palette(draft) != nullptr);
    QVERIFY(!bench->findChild<QWidget *>("workbenchColumn")->isVisible());
    QTreeWidget *list = pm.findChild<QTreeWidget *>("buildEffects");
    QVERIFY(list != nullptr);

    ed->setFocus(Qt::OtherFocusReason);
    QTest::keyClick(ed, Qt::Key_Escape);
    QTRY_VERIFY(ed.isNull());
    QVERIFY(pm.effectDesigner() == nullptr);
    QTRY_VERIFY(m_doc->palette(draft) == nullptr);      // as Cancel leaves it
    QCOMPARE(bench->loadedFunction(), scene->id());
    QCOMPARE(bench->currentPage(), Workbench::ScenePage);
    QVERIFY(bench->findChild<QWidget *>("workbenchColumn")->isVisible());
    QVERIFY(list->isVisible());                         // Build ▸ Effects as it was
}

void EffectDesigner_Test::buildEffectsMakesAndDesignsEffects()
{
    FixtureGroup *towers = group("Towers", 0, 3);
    QLCPalette *phaser = effectPalette(m_doc, "Phaser", "Dimmer Phaser");
    EFX *efx = new EFX(m_doc);
    efx->setName("Circle");
    m_doc->addFunction(efx);

    EffectDesigner::plugIntoWorkbench();            // as the app does
    ProgrammingManager pm(nullptr, m_doc);
    pm.resize(1500, 900);
    pm.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pm));
    pm.setSection(ProgrammingManager::EffectsSection);
    QTreeWidget *list = pm.findChild<QTreeWidget *>("buildEffects");
    QVERIFY(list != nullptr);
    QTRY_VERIFY(effectsRow(list, "Phaser") != nullptr);
    QVERIFY(effectsRow(list, "Circle") != nullptr);

    // + New, with the classic effects in its menu.
    QToolButton *newBtn = pm.findChild<QToolButton *>("buildEffectsNew");
    QVERIFY(newBtn != nullptr && newBtn->isVisible());
    QVERIFY(newBtn->menu() != nullptr);
    QStringList newItems;
    for (QAction *a : newBtn->menu()->actions())
        if (!a->isSeparator())
            newItems << a->objectName();
    QVERIFY2(newItems.contains("buildEffectsNewPalette") && newItems.contains("buildEffectsNewEfx")
             && newItems.contains("buildEffectsNewMatrix"), qPrintable(newItems.join(',')));

    // + New (the button): a new effect in the designer, in the Workbench's
    // editor pane, the layers column out of the way.
    newBtn->click();
    // The same path a docked Show Tree cue's Effect row takes: the
    // Workbench's effect pane factory, on the bench.
    QPointer<EffectDesigner> ed = pm.effectDesigner();
    QVERIFY(ed != nullptr);
    QCOMPARE(pm.workbench()->currentPage(), Workbench::FunctionPage);
    QCOMPARE(pm.workbench()->effectPane(), static_cast<QWidget *>(ed.data()));
    QCOMPARE(pm.workbench()->loadedFunction(), Function::invalidId());
    QVERIFY(ed->isVisible());
    QVERIFY(!pm.workbench()->findChild<QWidget *>("workbenchColumn")->isVisible());
    QCOMPARE(ed->originalId(), QLCPalette::invalidId());
    QVERIFY(ed->draftId() != QLCPalette::invalidId());
    ed->setTryOnGroups({ towers->id() });

    // Designed and saved: listed in Effects, back on the bench.
    QVERIFY(ed->pickEffect("Color Chase"));
    const quint32 saved = ed->saveAsPalette("Chase Saved");
    QVERIFY(m_doc->palette(saved) != nullptr);
    QTRY_VERIFY(effectsRow(list, "Chase Saved") != nullptr);
    QTRY_VERIFY(pm.effectDesigner() != nullptr && pm.effectDesigner()->originalId() == saved);
    QTRY_VERIFY(ed.isNull());                       // a fresh designer on it

    // Clicking an effect palette designs it there.
    QTreeWidgetItem *phaserRow = effectsRow(list, "Phaser");
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualItemRect(phaserRow).center());
    ed = pm.effectDesigner();
    QVERIFY(ed != nullptr);
    QCOMPARE(ed->originalId(), phaser->id());

    // The right-click menu on an effect palette.
    QCoreApplication::processEvents();          // a queued rebuild: rows are new
    QMenu *menu = pm.effectsContextMenu(effectsRow(list, "Phaser"), &pm);
    for (const char *name : { "effectsDesign", "effectsDuplicate", "effectsRename", "effectsMove", "effectsDelete",
                              "effectsNewPalette", "effectsNewEfx", "effectsNewMatrix" })
        QVERIFY2(menuAction(menu, name) != nullptr, name);
    QVERIFY(menuAction(menu, "effectsOpen") == nullptr);
    const int before = m_doc->palettes().count();
    menuAction(menu, "effectsDuplicate")->trigger();
    delete menu;
    QTRY_VERIFY(effectsRow(list, "Phaser (copy)") != nullptr);
    QVERIFY(m_doc->palettes().count() > before);
    ed = pm.effectDesigner();
    QVERIFY(ed != nullptr);
    QCOMPARE(m_doc->palette(ed->originalId())->name(), QString("Phaser (copy)"));

    // F2 / Rename: in place.
    QCoreApplication::processEvents();
    effectsRow(list, "Phaser (copy)")->setText(0, "Phaser Two");
    QCOMPARE(m_doc->palette(ed->originalId())->name(), QString("Phaser Two"));

    // On an EFX: Open in editor loads it, the designer's draft goes.
    QCoreApplication::processEvents();
    menu = pm.effectsContextMenu(effectsRow(list, "Circle"), &pm);
    QVERIFY(menuAction(menu, "effectsOpen") != nullptr);
    QVERIFY(menuAction(menu, "effectsDesign") == nullptr);
    menuAction(menu, "effectsOpen")->trigger();
    delete menu;
    QCOMPARE(pm.workbench()->loadedFunction(), efx->id());
    QVERIFY(pm.workbench()->effectPane() == nullptr);
    QVERIFY(pm.effectDesigner() == nullptr);
    QVERIFY(pm.workbench()->findChild<QWidget *>("workbenchColumn")->isVisible());
    QTRY_VERIFY(ed.isNull());                       // and its draft with it
    for (QLCPalette *p : m_doc->palettes())
        QVERIFY2(p->name() != "Phaser Two" || !m_doc->showTree()->isInlinePalette(p->id()), "a draft left");

    // Empty space: only the New items. New EFX / RGB Matrix land on the
    // Try on groups and load.
    menu = pm.effectsContextMenu(nullptr, &pm);
    QVERIFY(menuAction(menu, "effectsDelete") == nullptr);
    QVERIFY(menuAction(menu, "effectsNewPalette") != nullptr);
    delete menu;
    const quint32 newEfx = pm.newClassicEffect(Function::EFXType);
    EFX *fresh = qobject_cast<EFX *>(m_doc->function(newEfx));
    QVERIFY(fresh != nullptr);
    QCOMPARE(fresh->fixtures().count(), 3);
    QCOMPARE(pm.workbench()->loadedFunction(), newEfx);
    QTRY_VERIFY(effectsRow(list, fresh->name()) != nullptr);
    QCOMPARE(pm.section(), ProgrammingManager::EffectsSection);
    const quint32 newMatrix = pm.newClassicEffect(Function::RGBMatrixType);
    RGBMatrix *matrix = qobject_cast<RGBMatrix *>(m_doc->function(newMatrix));
    QVERIFY(matrix != nullptr);
    QCOMPARE(matrix->fixtureGroup(), towers->id());

    // Move to folder (a function): its path, shown beside it.
    QTRY_VERIFY(effectsRow(list, matrix->name()) != nullptr);
    QCoreApplication::processEvents();
    pm.findChild<QLineEdit *>("buildEffectsFilter")->setText("Phaser");
    QVERIFY(effectsRow(list, "Circle")->isHidden());
    QVERIFY(!effectsRow(list, "Phaser")->isHidden());
    pm.findChild<QLineEdit *>("buildEffectsFilter")->clear();

    // Another show: nothing left behind.
    QVERIFY(pm.designEffect(phaser->id()));
    ed = pm.effectDesigner();
    QVERIFY(ed != nullptr && ed->draftId() != QLCPalette::invalidId());
    m_doc->clearContents();
    QVERIFY(ed.isNull() || ed->draftId() == QLCPalette::invalidId());
    pm.hide();
}

/****************************************************************************
 * Make a layer
 ****************************************************************************/

static const Look *effectLook(const Layer *layer)
{
    if (layer == nullptr)
        return nullptr;
    for (const Look &l : layer->looks)
        if (isEffectKey(l.key))
            return &l;
    return nullptr;
}

void EffectDesigner_Test::makeALayerIntoACue()
{
    FixtureGroup *towers = group("Towers", 0, 3);
    FixtureGroup *truss = group("Truss", 3, 3);
    QLCPalette *phaser = effectPalette(m_doc, "Phaser", "Dimmer Phaser");
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "Verse");
    st->clearUndo();

    EffectDesigner ed(m_doc);
    ed.setTryOnGroups({ towers->id(), truss->id() });
    QToolButton *btn = ed.findChild<QToolButton *>("edMakeLayer");
    QVERIFY(btn != nullptr);
    QVERIFY(!btn->isHidden());                  // the bench has it
    ed.setEffect(QLCPalette::invalidId());
    QVERIFY(!btn->isEnabled());                 // nothing picked yet
    QVERIFY(!ed.whyNoLayer().isEmpty());
    QCOMPARE(ed.makeLayerInCue(c1), invalidId());

    // A saved palette, as it is: the layer plays it by name.
    ed.setEffect(phaser->id());
    QVERIFY(btn->isEnabled());
    QCOMPARE(ed.savedPaletteForLayer(), phaser->id());
    const int palettes = m_doc->palettes().count();
    QSignalSpy made(&ed, &EffectDesigner::layerMade);
    const quint32 l1 = ed.makeLayerInCue(c1);
    QVERIFY(l1 != invalidId());
    QCOMPARE(made.count(), 1);
    QCOMPARE(made.first().at(0).toUInt(), c1);
    QCOMPARE(made.first().at(1).toUInt(), l1);
    QCOMPARE(made.first().at(3).toUInt(), phaser->id());
    const Layer *layer = st->cue(c1)->layer(l1);
    QVERIFY(layer != nullptr);
    QCOMPARE(layer->target.groups, QList<quint32>({ towers->id(), truss->id() }));
    QVERIFY(effectLook(layer) != nullptr);
    QCOMPARE(effectLook(layer)->palettes, QList<quint32>({ phaser->id() }));
    QCOMPARE(m_doc->palettes().count(), palettes);      // nothing new
    QVERIFY(layerScene(m_doc, c1)->palettes().contains(phaser->id()));
    // The bench goes on, as it was.
    QVERIFY(ed.draft() != nullptr);
    QCOMPARE(ed.originalId(), phaser->id());

    // One step: undo takes the layer and its look, redo brings them back.
    QVERIFY(st->undoText().contains("Make a layer"));
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c1)->layer(l1), nullptr);
    QVERIFY(!st->canUndo());
    QVERIFY(st->redo());
    QVERIFY(effectLook(st->cue(c1)->layer(l1)) != nullptr);
    QCOMPARE(effectLook(st->cue(c1)->layer(l1))->palettes, QList<quint32>({ phaser->id() }));

    // Changed on the bench: a one-off copy, named for the cue; the bench
    // keeps designing its own draft.
    ed.draft()->setEffectParamValue("speed", 9);
    QVERIFY(ed.isModified());
    QCOMPARE(ed.savedPaletteForLayer(), QLCPalette::invalidId());
    const quint32 l2 = ed.makeLayerInCue(c1);
    QVERIFY(l2 != invalidId() && l2 != l1);
    const Look *look = effectLook(st->cue(c1)->layer(l2));
    QVERIFY(look != nullptr);
    const quint32 oneOff = look->palettes.value(0);
    QVERIFY(oneOff != phaser->id() && oneOff != ed.draftId());
    QVERIFY(st->isInlinePalette(oneOff));
    QVERIFY2(m_doc->palette(oneOff)->name().startsWith("Dimmer Phaser · 1 Verse"),
             qPrintable(m_doc->palette(oneOff)->name()));
    QCOMPARE(m_doc->palette(oneOff)->effectParamValues().value("speed"), 9.0);
    QVERIFY(m_doc->palette(ed.draftId()) != nullptr);
    QCOMPARE(phaser->effectParamValues().value("speed", 0.0), 0.0);     // the palette untouched
    QVERIFY(st->undo());
    QCOMPARE(st->cue(c1)->layer(l2), nullptr);
    QVERIFY(st->redo());
    look = effectLook(st->cue(c1)->layer(l2));
    QVERIFY(look != nullptr);
    QCOMPARE(look->palettes, QList<quint32>({ oneOff }));
    QVERIFY(m_doc->palette(oneOff) != nullptr);
    QCOMPARE(m_doc->palette(oneOff)->effectParamValues().value("speed"), 9.0);

    // Where you came from: the button goes there, the menu says so first.
    ed.setCameFrom(c1, Function::invalidId());
    QCOMPARE(btn->popupMode(), QToolButton::MenuButtonPopup);
    QVERIFY2(btn->text().contains("1 Verse"), qPrintable(btn->text()));
    QMenu *menu = ed.makeLayerMenu();
    QAction *from = menu->findChild<QAction *>("edLayerCameFrom");
    QVERIFY(from != nullptr && from->isEnabled());
    QVERIFY(menu->findChild<QAction *>("edLayerPickScene") != nullptr);
    QVERIFY(menu->findChild<QAction *>("edLayerNewScene") != nullptr);
    QAction *treeCue = menu->findChild<QAction *>("edLayerTreeCue");
    QVERIFY(treeCue != nullptr && !treeCue->isEnabled());   // no Show Tree here
    const int layers = st->cue(c1)->layers.count();
    btn->click();
    QCOMPARE(st->cue(c1)->layers.count(), layers + 1);

    // A layer's designer has none (it is the layer).
    Target t;
    t.groups << towers->id();
    EffectLayerDialog dlg(m_doc, c1, l1, t);
    QVERIFY(dlg.designer()->findChild<QToolButton *>("edMakeLayer")->isHidden());
}

void EffectDesigner_Test::makeALayerIntoAScene()
{
    FixtureGroup *towers = group("Towers", 0, 3);
    Scene *scene = new Scene(m_doc);
    scene->setName("Verse wash");
    m_doc->addFunction(scene);
    PaletteOps *ops = m_doc->paletteOps();
    ShowTreeController *st = m_doc->showTree();

    EffectDesigner ed(m_doc);
    ed.setTryOnGroups({ towers->id() });
    ed.setEffect(QLCPalette::invalidId());
    QVERIFY(ed.pickEffect("Dimmer Phaser"));
    QCOMPARE(ed.savedPaletteForLayer(), QLCPalette::invalidId());   // never saved

    QSignalSpy made(&ed, &EffectDesigner::layerMade);
    QVERIFY(ed.makeLayerInScene(scene->id()));
    QCOMPARE(made.count(), 1);
    QCOMPARE(made.first().at(2).toUInt(), scene->id());
    const quint32 oneOff = made.first().at(3).toUInt();
    QVERIFY(st->isInlinePalette(oneOff));
    QVERIFY(oneOff != ed.draftId());
    QCOMPARE(scene->fixtureGroups(), QList<quint32>({ towers->id() }));
    QCOMPARE(scene->palettes(), QList<quint32>({ oneOff }));
    QVERIFY2(m_doc->palette(oneOff)->name().endsWith("· Verse wash"), qPrintable(m_doc->palette(oneOff)->name()));
    QVERIFY(ops->undoText().contains("make a layer"));

    // Undo: the scene as it was; redo: back.
    QVERIFY(ops->undo());
    QVERIFY(scene->fixtureGroups().isEmpty());
    QVERIFY(scene->palettes().isEmpty());
    QVERIFY(ops->redo());
    QCOMPARE(scene->fixtureGroups(), QList<quint32>({ towers->id() }));
    QCOMPARE(scene->palettes(), QList<quint32>({ oneOff }));
    QCOMPARE(m_doc->palette(oneOff)->type(), QLCPalette::Effect);

    // The scene keeps it across a bake (an edit in the tree)...
    st->addProduction("Another");
    QVERIFY(m_doc->palette(oneOff) != nullptr);
    // ...and so does a step that took it out, while undo can put it back.
    QVERIFY(ops->sceneStep("remove look", { scene->id() }, [&]() {
        scene->removePalette(oneOff);
        return true;
    }));
    st->addProduction("Third");
    QVERIFY(m_doc->palette(oneOff) != nullptr);
    QVERIFY(ops->undo());
    QCOMPARE(scene->palettes(), QList<quint32>({ oneOff }));

    // Again into the same scene: the groups it has aren't doubled.
    QVERIFY(ed.makeLayerInScene(scene->id()));
    QCOMPARE(scene->fixtureGroups(), QList<quint32>({ towers->id() }));
    QCOMPARE(scene->palettes().count(), 2);

    // Not into the tree's own scenes, nor the preview's.
    QVERIFY(!ed.makeLayerInScene(ed.previewSceneId()));
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "One");
    Target t;
    t.groups << towers->id();
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, m_doc);
    full->setName("Full");
    full->setValue(255);
    m_doc->addPalette(full);
    st->setLayerLook(c1, st->addLayer(c1, t), st->lookForPalette(full->id()));
    const Scene *generated = layerScene(m_doc, c1);
    QVERIFY(generated != nullptr);
    QVERIFY(!ed.makeLayerInScene(generated->id()));
}

void EffectDesigner_Test::makeALayerIntoANewScene()
{
    FixtureGroup *towers = group("Towers", 0, 3);
    QLCPalette *phaser = effectPalette(m_doc, "Phaser", "Dimmer Phaser");
    PaletteOps *ops = m_doc->paletteOps();

    EffectDesigner ed(m_doc);
    ed.setTryOnGroups({ towers->id() });
    ed.setEffect(phaser->id());
    const int functions = m_doc->functions().count();
    const quint32 sid = ed.makeLayerInNewScene("Phaser on Towers");
    Scene *scene = qobject_cast<Scene *>(m_doc->function(sid));
    QVERIFY(scene != nullptr);
    QVERIFY(scene->isVisible());
    QCOMPARE(scene->name(), QString("Phaser on Towers"));
    QCOMPARE(scene->fixtureGroups(), QList<quint32>({ towers->id() }));
    QCOMPARE(scene->palettes(), QList<quint32>({ phaser->id() }));
    QCOMPARE(m_doc->functions().count(), functions + 1);

    QVERIFY(ops->undo());
    QCOMPARE(m_doc->function(sid), nullptr);
    QVERIFY(ops->redo());
    scene = qobject_cast<Scene *>(m_doc->function(sid));
    QVERIFY(scene != nullptr);
    QCOMPARE(scene->name(), QString("Phaser on Towers"));
    QCOMPARE(scene->palettes(), QList<quint32>({ phaser->id() }));

    // No Try on groups: nothing to make it on.
    ed.setTryOnGroups({});
    QVERIFY(!ed.whyNoLayer().isEmpty());
    QCOMPARE(ed.makeLayerInNewScene("Nothing"), Function::invalidId());
}

void EffectDesigner_Test::makeALayerFromTheWorkbench()
{
    FixtureGroup *towers = group("Towers", 0, 3);
    QLCPalette *phaser = effectPalette(m_doc, "Phaser", "Dimmer Phaser");
    Scene *scene = new Scene(m_doc);
    scene->setName("Verse wash");
    m_doc->addFunction(scene);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "Chorus");

    EffectDesigner::plugIntoWorkbench();
    ShowTreeView view(nullptr, m_doc);
    view.resize(1000, 700);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.selectNode(c1));

    ProgrammingManager pm(nullptr, m_doc);
    pm.resize(1500, 900);
    pm.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pm));

    // The scene loaded, then an effect designed: the bench remembers it.
    pm.showFunction(scene->id());
    QCOMPARE(pm.workbench()->loadedFunction(), scene->id());
    QVERIFY(pm.designEffect(phaser->id()));
    QPointer<EffectDesigner> ed = pm.effectDesigner();
    QVERIFY(ed != nullptr);
    QCOMPARE(ed->cameFromScene(), scene->id());
    ed->setTryOnGroups({ towers->id() });
    QToolButton *btn = ed->findChild<QToolButton *>("edMakeLayer");
    QVERIFY(btn->isVisible() && btn->isEnabled());
    QVERIFY2(btn->text().contains("Verse wash"), qPrintable(btn->text()));

    // Another effect on the same bench: still where it came from.
    QVERIFY(pm.designEffect(QLCPalette::invalidId()));
    ed = pm.effectDesigner();
    QCOMPARE(ed->cameFromScene(), scene->id());
    QVERIFY(pm.designEffect(phaser->id()));
    ed = pm.effectDesigner();
    ed->setTryOnGroups({ towers->id() });

    // The Show Tree's selected cue is offered too.
    QMenu *menu = ed->makeLayerMenu();
    QAction *treeCue = menu->findChild<QAction *>("edLayerTreeCue");
    QVERIFY(treeCue != nullptr && treeCue->isEnabled());
    QVERIFY2(treeCue->text().contains("1 Chorus"), qPrintable(treeCue->text()));
    treeCue->trigger();
    QCOMPARE(st->cue(c1)->layers.count(), 1);
    const quint32 l1 = st->cue(c1)->layers.first().id;
    // ...and its new layer is selected there.
    QTRY_COMPARE(view.currentId(), l1);
    QCOMPARE(view.currentCueId(), c1);

    // The button: into the scene it came from, loaded with the look selected.
    ed = pm.effectDesigner();
    QVERIFY(ed != nullptr);
    btn = ed->findChild<QToolButton *>("edMakeLayer");
    btn->click();
    QCOMPARE(scene->palettes(), QList<quint32>({ phaser->id() }));
    QCOMPARE(scene->fixtureGroups(), QList<quint32>({ towers->id() }));
    QTRY_COMPARE(pm.workbench()->loadedFunction(), scene->id());
    QTRY_VERIFY(ed.isNull());                       // the bench made way for it
    const ProgrammingOutline::Row row = ProgrammingOutline::rowOf(pm.workbench()->rows()->currentItem());
    QCOMPARE(row.kind, int(ProgrammingOutline::PaletteRow));
    QCOMPARE(row.id, phaser->id());
    QCOMPARE(pm.workbench()->currentPaletteId(), phaser->id());

    // Undo (Build's history) takes it out again.
    QVERIFY(m_doc->paletteOps()->undo());
    QVERIFY(scene->palettes().isEmpty());

    const QString shots = qEnvironmentVariable("QLC_EFFECTDESIGNER_SHOTS");
    if (!shots.isEmpty())
    {
        QDir().mkpath(shots);
        QVERIFY(pm.designEffect(phaser->id()));
        pm.effectDesigner()->setTryOnGroups({ towers->id() });
        QMenu *m = pm.effectDesigner()->makeLayerMenu();
        QToolButton *b = pm.effectDesigner()->findChild<QToolButton *>("edMakeLayer");
        m->popup(b->mapToGlobal(QPoint(0, b->height())));
        QTest::qWait(300);
        QPixmap shot = pm.grab();
        QPainter paint(&shot);
        paint.drawPixmap(pm.mapFromGlobal(m->pos()), m->grab());
        paint.end();
        QVERIFY(shot.save(QDir(shots).filePath("make-a-layer.png")));
        m->hide();
    }
    pm.hide();
    view.hide();
}

static FixtureGroup *groupNamed(Doc *doc, const QString &name)
{
    for (FixtureGroup *g : doc->fixtureGroups())
        if (g->name() == name)
            return g;
    return nullptr;
}

void EffectDesigner_Test::picturesOfARealShow()
{
    const QString path = qEnvironmentVariable("QLC_EFFECTDESIGNER_SHOW");
    const QString shots = qEnvironmentVariable("QLC_EFFECTDESIGNER_SHOTS");
    if (path.isEmpty() || shots.isEmpty())
        QSKIP("set QLC_EFFECTDESIGNER_SHOW and QLC_EFFECTDESIGNER_SHOTS to picture a real show");

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QXmlStreamReader r(&file);
    bool loaded = false;
    while (!r.atEnd() && !loaded)
        if (r.readNextStartElement() && r.name() == QLatin1String("Engine"))
            loaded = m_doc->loadXML(r, false);
    QVERIFY(loaded);
    QDir().mkpath(shots);
    ShowTreeController *st = m_doc->showTree();

    // A layer on AT-W-ALL (else the first group layer there is).
    const FixtureGroup *atw = groupNamed(m_doc, qEnvironmentVariable("QLC_EFFECTDESIGNER_GROUP", "AT-W-ALL"));
    quint32 cueId = invalidId(), layerId = invalidId();
    Target target;
    for (const Production &p : st->productions())
        for (const CueList &l : p.lists)
            for (const Cue &c : l.cues)
                for (const Layer &layer : c.layers)
                    if (cueId == invalidId() && !layer.target.isOpaque()
                        && (atw == nullptr || layer.target.groups.contains(atw->id())))
                    {
                        cueId = c.id;
                        layerId = layer.id;
                        target = layer.target;
                    }
    QVERIFY2(cueId != invalidId(), "the show has no Show Tree layer on that group");
    {
        EffectLayerDialog dlg(m_doc, cueId, layerId, target);
        dlg.resize(1400, 860);
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        QVERIFY(dlg.designer()->pickEffect(qEnvironmentVariable("QLC_EFFECTDESIGNER_EFFECT", "Color Chase")));
        for (int i = 0; i < 20; ++i)
        {
            QTest::qWait(50);
            dlg.designer()->browser()->update();
        }
        QVERIFY(dlg.grab().save(QDir(shots).filePath("effect-designer-layer.png")));
        dlg.designer()->cancel();
    }

    // The bench: Window ▸ Effects, trying a matrix effect on the XL-450s.
    const FixtureGroup *xl = groupNamed(m_doc, qEnvironmentVariable("QLC_EFFECTDESIGNER_BENCH", "TrussC-RGB"));
    for (FixtureGroup *g : xl != nullptr ? QList<FixtureGroup *>() : m_doc->fixtureGroups())
    {
        bool all = !g->fixtureList().isEmpty();
        for (quint32 f : g->fixtureList())
        {
            const Fixture *fx = m_doc->fixture(f);
            all = all && fx != nullptr && fx->fixtureDef() != nullptr && fx->fixtureDef()->model() == "XL-450";
        }
        if (all && (xl == nullptr || g->fixtureList().count() > xl->fixtureList().count()))
            xl = g;
    }
    EffectsPage page(m_doc);
    if (xl != nullptr)
        page.designer()->setTryOnGroups({ xl->id() });
    page.resize(1500, 880);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    QVERIFY(page.designer()->pickEffect(qEnvironmentVariable("QLC_EFFECTDESIGNER_MATRIX", "Matrix Fire")));
    for (int i = 0; i < 20; ++i)
        QTest::qWait(50);
    QVERIFY(page.grab().save(QDir(shots).filePath("effects-page-bench.png")));
    page.designer()->cancel();
    page.hide();

    // Build ▸ Effects: the designer in the Workbench, and the right-click menu.
    EffectDesigner::plugIntoWorkbench();
    ProgrammingManager pm(nullptr, m_doc);
    pm.resize(1600, 940);
    pm.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pm));
    pm.designEffect(QLCPalette::invalidId());
    if (xl != nullptr)
        pm.effectDesigner()->setTryOnGroups({ xl->id() });
    QVERIFY(pm.effectDesigner()->pickEffect(qEnvironmentVariable("QLC_EFFECTDESIGNER_MATRIX", "Matrix Fire")));
    for (int i = 0; i < 20; ++i)
        QTest::qWait(50);
    QVERIFY(pm.grab().save(QDir(shots).filePath("build-effects-designer.png")));
    {
        QTreeWidget *list = pm.findChild<QTreeWidget *>("buildEffects");
        QTreeWidgetItem *row = nullptr;
        for (QTreeWidgetItemIterator it(list); *it && row == nullptr; ++it)
            if ((*it)->flags() & Qt::ItemIsSelectable)
                row = *it;
        QMenu *menu = pm.effectsContextMenu(row, &pm);
        const QRect r = list->visualItemRect(row);
        menu->popup(list->viewport()->mapToGlobal(r.center()));
        QTest::qWait(200);
        // The window and the menu, one picture.
        QPixmap shot = pm.grab();
        QPainter paint(&shot);
        paint.drawPixmap(pm.mapFromGlobal(menu->pos()), menu->grab());
        paint.end();
        QVERIFY(shot.save(QDir(shots).filePath("build-effects-menu.png")));
        menu->hide();
        delete menu;
    }

    // Make a layer, on the real show: its menu, then into a cue and into a
    // new scene, each taken back by its undo.
    {
        EffectDesigner *ed = pm.effectDesigner();
        QToolButton *b = ed->findChild<QToolButton *>("edMakeLayer");
        QVERIFY(b != nullptr && b->isVisible());
        QMenu *m = ed->makeLayerMenu();
        m->popup(b->mapToGlobal(QPoint(0, b->height())));
        QTest::qWait(200);
        QPixmap shot = pm.grab();
        QPainter paint(&shot);
        paint.drawPixmap(pm.mapFromGlobal(m->pos()), m->grab());
        paint.end();
        QVERIFY(shot.save(QDir(shots).filePath("build-make-a-layer.png")));
        m->hide();

        quint32 firstCue = invalidId();
        for (const Production &p : st->productions())
            for (const CueList &l : p.lists)
                if (firstCue == invalidId() && !l.cues.isEmpty())
                    firstCue = l.cues.first().id;
        if (firstCue != invalidId())
        {
            const int layers = st->cue(firstCue)->layers.count();
            const quint32 lid = ed->makeLayerInCue(firstCue);
            QVERIFY(lid != invalidId());
            QCOMPARE(st->cue(firstCue)->layers.count(), layers + 1);
            QVERIFY(st->undo());
            QCOMPARE(st->cue(firstCue)->layers.count(), layers);
        }
        const quint32 sid = ed->makeLayerInNewScene("Make a layer (test)");
        QVERIFY(qobject_cast<Scene *>(m_doc->function(sid)) != nullptr);
        QVERIFY(m_doc->paletteOps()->undo());
        QCOMPARE(m_doc->function(sid), nullptr);
    }
    QTest::qWait(50);
    if (pm.effectDesigner() != nullptr)
        pm.effectDesigner()->cancel();
    pm.hide();
}

QTEST_MAIN(EffectDesigner_Test)
