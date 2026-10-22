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
#include <QtTest>

#define private public
#include "showtreeview.h"
#undef private
#include "effectdesigner_test.h"
#include "effectdesigner.h"
#include "effectspage.h"
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
}

QTEST_MAIN(EffectDesigner_Test)
