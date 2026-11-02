/*
  Q Light Controller Plus
  contextmenus_test.cpp

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

#include <QApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QPointer>
#include <QTimer>
#include <QMenu>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "fixturemanager.h"
#include "fixturegroupsource.h"
#include "programmingoutline.h"
#include "programmingmanager.h"
#include "functionmanager.h"
#include "monitor.h"
#include "monitorlayerspanel.h"
#include "showtreeview.h"
#include "functionstreewidget.h"
#undef private
#undef protected

#include "contextmenus_test.h"
#include "contextmenus.h"
#include "createfixturegroup.h"
#include "fixturetreewidget.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "paletteops.h"
#include "fixture.h"
#include "monitorproperties.h"
#include "scene.h"
#include "efx.h"
#include "doc.h"

/* The canon's own items, by object name: the ones a kind must show the same
   way in every place (ContextMenus::rig). */
static const QStringList kCanonItems = {
    "ctxEditHeadLayout", "ctxFixtureProperties", "ctxLocate", "whyOnAction", "ctxPatch",
    "outputRulesAction", "ctxSplitByHeadType", "ctxLayOutFromStage", "selectionMenu", "selectAllSimilar",
    "ctxCreateGroupFromSelection", "ctxMoveToFolder", "ctxRename", "ctxDeleteGroup"
};

static QString nameOf(const QAction *a)
{
    return a->menu() != nullptr ? a->menu()->objectName() : a->objectName();
}

/* The canon's items in @p menu, top level, in order */
static QStringList canonItems(const QMenu *menu)
{
    QStringList out;
    for (QAction *a : menu->actions())
        if (kCanonItems.contains(nameOf(a)))
            out << nameOf(a);
    return out;
}

static QString texts(const QMenu *menu)
{
    QStringList t;
    for (QAction *a : menu->actions())
        t << (a->isSeparator() ? QStringLiteral("---") : a->text());
    return t.join(" | ");
}

#define VERIFY_CANON(menu) \
    do { \
        const QStringList problems_ = ContextMenu::checkCanon(menu); \
        QVERIFY2(problems_.isEmpty(), qPrintable(problems_.join("; ") + "\n  in: " + texts(menu))); \
    } while (0)

/* A menu a widget opens with exec(): read when it pops up, then closed */
static QStringList popupCheck(const std::function<void()> &open, QStringList *canon = nullptr)
{
    QStringList problems;
    bool seen = false;
    QTimer poll;
    poll.setInterval(10);
    QObject::connect(&poll, &QTimer::timeout, [&]() {
        QMenu *m = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (m == nullptr)
            return;
        poll.stop();
        seen = true;
        problems = ContextMenu::checkCanon(m);
        if (!problems.isEmpty())
            problems << ("in: " + texts(m));
        if (canon != nullptr)
            *canon = canonItems(m);
        m->close();
    });
    poll.start();
    open();
    if (!seen)
        problems << QStringLiteral("no menu opened");
    return problems;
}

void ContextMenus_Test::init()
{
    m_doc = new Doc(this);
    m_fixtures.clear();
    for (int i = 0; i < 6; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Dim %1").arg(i + 1));
        fxi->setAddress(quint32(i));
        fxi->setUniverse(0);
        fxi->setChannels(1);
        m_doc->addFixture(fxi);
        // On the plot, so Plot Layers lists the groups.
        m_doc->monitorProperties()->setFixturePosition(fxi->id(), 0, 0, QVector3D(1000.0f * (i + 1), 2000, 0));
        m_fixtures << fxi->id();
    }
    auto group = [this](const QString &name, int from) {
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(name);
        g->setSize(QSize(3, 1));
        m_doc->addFixtureGroup(g);
        for (int i = 0; i < 3; ++i)
            g->assignFixture(m_fixtures.at(from + i), QLCPoint(i, 0));
        return g->id();
    };
    m_groupA = group("Front", 0);
    m_groupB = group("Back", 3);
}

void ContextMenus_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

/****************************************************************************
 * The builder
 ****************************************************************************/

void ContextMenus_Test::sectionsLayOutInOrderWithOneSeparatorBetween()
{
    QMenu menu;
    {
        ContextMenu cm(&menu);
        // Added out of order, on purpose: the sections put them right.
        cm.add(ContextMenu::Remove, "Delete Thing…");
        cm.add(ContextMenu::Open, "Thing Properties…");
        cm.add(ContextMenu::Edit, "Rename");
        cm.add(ContextMenu::Kind, "Locate");
        cm.add(ContextMenu::Edit, "Duplicate");
        QVERIFY(cm.has(ContextMenu::Edit));
        QVERIFY(!cm.has(ContextMenu::View));
    }   // finished as it goes
    QCOMPARE(texts(&menu), QString("Thing Properties… | --- | Locate | --- | Rename | Duplicate | --- | Delete Thing…"));
    VERIFY_CANON(&menu);

    // An empty builder leaves the menu empty; finishing twice adds nothing.
    QMenu empty;
    ContextMenu e(&empty);
    QVERIFY(e.isEmpty());
    e.finish();
    e.finish();
    QVERIFY(empty.actions().isEmpty());

    // A submenu is a section's item like any other.
    QMenu withSub;
    ContextMenu s(&withSub);
    QMenu *sub = s.addMenu(ContextMenu::Organise, "Move to Folder", "subMenu");
    sub->addAction("Somewhere");
    s.add(ContextMenu::Open, "Edit Thing…");
    s.finish();
    QCOMPARE(withSub.actions().size(), 3);
    QCOMPARE(withSub.actions().last()->menu(), sub);
    VERIFY_CANON(&withSub);
}

void ContextMenus_Test::theCanonCatchesDrift()
{
    // An item added past the builder has no section.
    QMenu loose;
    {
        ContextMenu cm(&loose);
        cm.add(ContextMenu::Open, "Edit Thing…");
    }
    loose.addAction("Stray Item");
    QVERIFY(!ContextMenu::checkCanon(&loose).isEmpty());

    // Sections backwards: delete before the editor.
    QMenu backwards;
    QAction *del = backwards.addAction("Delete Thing…");
    del->setProperty(ContextMenu::sectionProperty(), int(ContextMenu::Remove));
    backwards.addSeparator();
    QAction *ed = backwards.addAction("Edit Thing…");
    ed->setProperty(ContextMenu::sectionProperty(), int(ContextMenu::Open));
    QVERIFY(!ContextMenu::checkCanon(&backwards).isEmpty());

    // Two sections with no separator; a separator inside one section.
    QMenu noSep;
    noSep.addAction("Edit Thing…")->setProperty(ContextMenu::sectionProperty(), int(ContextMenu::Open));
    noSep.addAction("Locate")->setProperty(ContextMenu::sectionProperty(), int(ContextMenu::Kind));
    QVERIFY(!ContextMenu::checkCanon(&noSep).isEmpty());
    QMenu inside;
    inside.addAction("Rename")->setProperty(ContextMenu::sectionProperty(), int(ContextMenu::Edit));
    inside.addSeparator();
    inside.addAction("Duplicate")->setProperty(ContextMenu::sectionProperty(), int(ContextMenu::Edit));
    QVERIFY(!ContextMenu::checkCanon(&inside).isEmpty());

    // Words: "...", a shortcut typed in, sentence case.
    for (const char *bad : { "Edit Thing...", "Group Selected (Ctrl+G)", "Delete group" })
    {
        QMenu words;
        ContextMenu cm(&words);
        cm.add(ContextMenu::Open, QString::fromUtf8(bad));
        cm.finish();
        QVERIFY2(!ContextMenu::checkCanon(&words).isEmpty(), bad);
    }
}

void ContextMenus_Test::titleCase()
{
    for (const char *ok : { "Select All Similar", "Move to Folder", "Create Fixture Group from Selection…",
                            "Remove from “my truss”", "Delete 3 Fixture Groups…", "Why is this on?",
                            "+ Cue", "Lay Out from the Stage…", "Same Y (straight row)", "Rename" })
    {
        // "Why is this on?" is the panel's name, a question kept as asked:
        // the one exception, and it is the panel's own text.
        if (QString::fromUtf8(ok) == "Why is this on?")
            continue;
        QVERIFY2(ContextMenu::checkTitleCase(QString::fromUtf8(ok)).isEmpty(), ok);
    }
    for (const char *bad : { "Select all similar", "Move to folder", "New scene", "Delete folder" })
        QVERIFY2(!ContextMenu::checkTitleCase(QString::fromUtf8(bad)).isEmpty(), bad);
}

/****************************************************************************
 * A fixture group, a fixture: one menu everywhere
 ****************************************************************************/

static QTreeWidgetItem *findRow(QTreeWidget *tree, const std::function<bool(QTreeWidgetItem *)> &match)
{
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if (match(*it))
            return *it;
    return nullptr;
}

static void selectOnly(QTreeWidget *tree, QTreeWidgetItem *row)
{
    tree->clearSelection();
    for (QTreeWidgetItem *p = row->parent(); p != nullptr; p = p->parent())
        p->setExpanded(true);
    tree->setCurrentItem(row);
    row->setSelected(true);
    tree->scrollToItem(row);
}

void ContextMenus_Test::aFixtureGroupHasOneMenuEverywhere()
{
    QWidget host;
    Monitor mon(&host, m_doc);           // the Plot: Locate is offered with it
    QVERIFY(Monitor::instance() != nullptr);

    /* The canon's order for one fixture group. Each place leaves out only
       what it says it does (and why): */
    const QStringList canon = { "ctxEditHeadLayout", "ctxLocate", "whyOnAction", "outputRulesAction",
                                "ctxLayOutFromStage", "selectionMenu", "selectAllSimilar", "ctxMoveToFolder",
                                "ctxRename", "ctxDeleteGroup" };
    auto without = [&canon](const QStringList &omit) {
        QStringList out = canon;
        for (const QString &o : omit)
            out.removeAll(o);
        return out;
    };

    // Build ▸ Groups & fixtures.
    {
        FixtureGroupSource src(m_doc);
        src.reload();
        QTreeWidgetItem *row = findRow(&src, [this](QTreeWidgetItem *it) {
            return it->data(0, FixtureGroupSource::KindRole).toInt() == FixtureGroupSource::GroupNode
                   && it->data(0, FixtureGroupSource::IdRole).toUInt() == m_groupA;
        });
        QVERIFY(row != nullptr);
        selectOnly(&src, row);
        QMenu menu;
        src.fillContextMenu(&menu, row);
        VERIFY_CANON(&menu);
        QCOMPARE(canonItems(&menu), canon);
    }

    // The Show Tree's Library: the same, beside its Pin and Make Set….
    {
        ShowTreeView view(nullptr, m_doc);
        view.resize(1200, 800);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTreeWidget *lib = view.library();
        QTreeWidgetItem *row = findRow(lib, [this](QTreeWidgetItem *it) {
            return it->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LibGroup
                   && it->data(0, ShowTreeView::IdRole).toUInt() == m_groupA;
        });
        QVERIFY(row != nullptr);
        selectOnly(lib, row);
        QMenu menu;
        view.fillLibraryMenu(&menu, row);
        VERIFY_CANON(&menu);
        QCOMPARE(canonItems(&menu), canon);
        QVERIFY(menu.findChild<QAction *>("stMakeSet") != nullptr);
    }

    // The Fixtures tab.
    {
        FixtureManager mgr(nullptr, m_doc);
        mgr.resize(900, 600);
        mgr.show();
        QTreeWidgetItem *row = findRow(mgr.m_fixtures_tree, [this](QTreeWidgetItem *it) {
            return it->data(0, PROP_GROUP).isValid() && it->data(0, PROP_GROUP).toUInt() == m_groupA;
        });
        QVERIFY(row != nullptr);
        selectOnly(mgr.m_fixtures_tree, row);
        QStringList items;
        const QStringList problems = popupCheck([&]() {
            mgr.slotContextMenuRequested(mgr.m_fixtures_tree->visualItemRect(row).center());
        }, &items);
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join("; ")));
        QCOMPARE(items, canon);
    }

    // Plot Layers: the Plot's folders are its own, so no Move to Folder;
    // Select All Similar when there are others alike.
    {
        MonitorLayersPanel *panel = mon.m_layersPanel;
        QVERIFY(panel != nullptr);
        panel->reload();
        QTreeWidgetItem *row = findRow(panel->m_tree, [this](QTreeWidgetItem *it) {
            return it->data(0, Qt::UserRole).toInt() == MonitorLayersPanel::NodeFixtureGroup
                   && it->data(0, Qt::UserRole + 1).toUInt() == m_groupA;
        });
        QVERIFY(row != nullptr);
        selectOnly(panel->m_tree, row);
        QMenu menu;
        panel->buildContextMenu(&menu, row);
        VERIFY_CANON(&menu);
        QCOMPARE(canonItems(&menu), without({ "ctxMoveToFolder" }));
    }

    // Build ▸ Scenes: a group row is the group in that scene -- taken out
    // of it (Remove from Scene), not deleted or filed from here.
    {
        Scene *s = new Scene(m_doc);
        s->setName("Warm");
        m_doc->addFunction(s);
        s->addFixtureGroup(m_groupA);
        ProgrammingOutline outline(m_doc);
        outline.resize(400, 400);
        outline.show();
        outline.rebuild();
        outline.expandAll();
        QTreeWidgetItem *row = findRow(&outline, [this](QTreeWidgetItem *it) {
            return it->data(0, ProgrammingOutline::KindRole).toInt() == ProgrammingOutline::GroupRow
                   && it->data(0, ProgrammingOutline::IdRole).toUInt() == m_groupA;
        });
        QVERIFY(row != nullptr);
        selectOnly(&outline, row);
        QMenu menu;
        outline.fillContextMenu(&menu, row);
        VERIFY_CANON(&menu);
        QCOMPARE(canonItems(&menu), without({ "selectAllSimilar", "ctxMoveToFolder", "ctxDeleteGroup" }));
        QVERIFY(menu.findChild<QAction *>("outlineRemoveFromScene") != nullptr);
        QCOMPARE(menu.actions().last()->objectName(), QString("outlineRemoveFromScene"));
    }
}

void ContextMenus_Test::aFixtureHasOneMenuEverywhere()
{
    QWidget host;
    Monitor mon(&host, m_doc);
    const quint32 fid = m_fixtures.first();
    /* Create Fixture Group from Selection… is inside Selection ▸ (Q201),
       where the place offers it. */
    const QStringList canon = { "ctxFixtureProperties", "ctxLocate", "whyOnAction", "ctxPatch",
                                "outputRulesAction", "selectionMenu", "selectAllSimilar" };
    auto without = [&canon](const QStringList &omit) {
        QStringList out = canon;
        for (const QString &o : omit)
            out.removeAll(o);
        return out;
    };

    // Build ▸ Groups & fixtures.
    {
        FixtureGroupSource src(m_doc);
        src.reload();
        QTreeWidgetItem *row = findRow(&src, [fid](QTreeWidgetItem *it) {
            return it->data(0, FixtureGroupSource::KindRole).toInt() == FixtureGroupSource::FixtureNode
                   && it->data(0, FixtureGroupSource::IdRole).toUInt() == fid;
        });
        QVERIFY(row != nullptr);
        selectOnly(&src, row);
        QMenu menu;
        src.fillContextMenu(&menu, row);
        VERIFY_CANON(&menu);
        QCOMPARE(canonItems(&menu), canon);
        QMenu *sel = menu.findChild<QMenu *>("selectionMenu");
        QVERIFY(sel != nullptr);
        QVERIFY(sel->actions().contains(menu.findChild<QAction *>("ctxCreateGroupFromSelection")));
    }

    // The Fixtures tab: its own Patch (it also places staged fixtures) has
    // the canon's name and place.
    {
        FixtureManager mgr(nullptr, m_doc);
        mgr.resize(900, 600);
        mgr.show();
        QTreeWidgetItem *row = findRow(mgr.m_fixtures_tree, [fid](QTreeWidgetItem *it) {
            return it->data(0, PROP_ID).isValid() && it->data(0, PROP_ID).toUInt() == fid
                   && !it->data(0, PROP_GROUP).isValid() && !it->data(0, PROP_HEAD).isValid();
        });
        QVERIFY(row != nullptr);
        selectOnly(mgr.m_fixtures_tree, row);
        QStringList items;
        const QStringList problems = popupCheck([&]() {
            mgr.slotContextMenuRequested(mgr.m_fixtures_tree->visualItemRect(row).center());
        }, &items);
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join("; ")));
        QCOMPARE(items, canon);
    }

    // The Library: no Select All Similar for a fixture there, and Make Set…
    // is its grouping.
    {
        ShowTreeView view(nullptr, m_doc);
        view.resize(1200, 800);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTreeWidget *lib = view.library();
        QTreeWidgetItem *row = findRow(lib, [fid](QTreeWidgetItem *it) {
            return it->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::LibFixture
                   && it->data(0, ShowTreeView::IdRole).toUInt() == fid;
        });
        if (row != nullptr)   // the Library lists loose fixtures only when there are some
        {
            selectOnly(lib, row);
            QMenu menu;
            view.fillLibraryMenu(&menu, row);
            VERIFY_CANON(&menu);
            QCOMPARE(canonItems(&menu), without({ "selectAllSimilar" }));
        }
    }

    // Build ▸ Scenes: a fixed fixture of a scene.
    {
        Scene *s = new Scene(m_doc);
        s->setName("Cold");
        m_doc->addFunction(s);
        s->addFixture(fid);
        ProgrammingOutline outline(m_doc);
        outline.resize(400, 400);
        outline.show();
        outline.rebuild();
        outline.expandAll();
        QTreeWidgetItem *row = findRow(&outline, [fid](QTreeWidgetItem *it) {
            return it->data(0, ProgrammingOutline::KindRole).toInt() == ProgrammingOutline::FixtureRow
                   && it->data(0, ProgrammingOutline::IdRole).toUInt() == fid;
        });
        QVERIFY(row != nullptr);
        selectOnly(&outline, row);
        QMenu menu;
        outline.fillContextMenu(&menu, row);
        VERIFY_CANON(&menu);
        QCOMPARE(canonItems(&menu), without({ "selectAllSimilar" }));
    }
}

void ContextMenus_Test::theFixtureGroupItemsWork()
{
    QWidget parent;
    int changed = 0;
    ContextMenus::RigPlace place;
    place.parent = &parent;
    place.changed = [&changed]() { ++changed; };

    // Rename… (a dialog where the list cannot edit in place).
    {
        QMenu menu;
        ContextMenu cm(&menu);
        ContextMenus::fixtureGroup(cm, m_doc, { m_groupA }, place);
        cm.finish();
        QAction *rename = menu.findChild<QAction *>("ctxRename");
        QVERIFY(rename != nullptr);
        QCOMPARE(rename->text(), QString("Rename…"));
        QTimer::singleShot(0, []() {
            if (QInputDialog *d = qobject_cast<QInputDialog *>(QApplication::activeModalWidget()))
            {
                d->setTextValue("Front Wash");
                d->accept();
            }
        });
        rename->trigger();
        QCOMPARE(m_doc->fixtureGroup(m_groupA)->name(), QString("Front Wash"));
        QCOMPARE(changed, 1);
    }

    // Inline where the list can: "Rename", no dialog.
    {
        quint32 renamed = 0;
        ContextMenus::RigPlace inPlace = place;
        inPlace.renameInPlace = [&renamed](quint32 gid) { renamed = gid; };
        QMenu menu;
        ContextMenu cm(&menu);
        ContextMenus::fixtureGroup(cm, m_doc, { m_groupA }, inPlace);
        cm.finish();
        QAction *rename = menu.findChild<QAction *>("ctxRename");
        QCOMPARE(rename->text(), QString("Rename"));
        rename->trigger();
        QCOMPARE(renamed, m_groupA);
    }

    // Two groups: one Create Fixture Group from them, Delete both (asked),
    // and no Edit Head Layout or Rename for two.
    {
        QMenu menu;
        ContextMenu cm(&menu);
        ContextMenus::fixtureGroup(cm, m_doc, { m_groupA, m_groupB }, place);
        cm.finish();
        QVERIFY(menu.findChild<QAction *>("ctxEditHeadLayout") == nullptr);
        QVERIFY(menu.findChild<QAction *>("ctxRename") == nullptr);
        QVERIFY(menu.findChild<QAction *>("ctxCreateGroupFromSelection") != nullptr);
        QAction *del = menu.findChild<QAction *>("ctxDeleteGroup");
        QVERIFY(del != nullptr);
        QCOMPARE(del->text(), QString("Delete 2 Fixture Groups…"));
        QTimer::singleShot(0, []() {
            if (QMessageBox *mb = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                mb->button(QMessageBox::Yes)->click();
        });
        del->trigger();
        QVERIFY(m_doc->fixtureGroup(m_groupA) == nullptr);
        QVERIFY(m_doc->fixtureGroup(m_groupB) == nullptr);
        for (quint32 fid : m_fixtures)
            QVERIFY(m_doc->fixture(fid) != nullptr);    // the fixtures stay
    }

    // Not editable here (Operate, a locked plot): nothing that changes the
    // show, only what finds and asks.
    {
        Fixture *f = m_doc->fixture(m_fixtures.first());
        QVERIFY(f != nullptr);
        ContextMenus::RigPlace locked = place;
        locked.editable = false;
        QMenu menu;
        ContextMenu cm(&menu);
        ContextMenus::fixture(cm, m_doc, { f->id() }, locked);
        cm.finish();
        QVERIFY(menu.findChild<QAction *>("ctxCreateGroupFromSelection") == nullptr);
        QAction *patch = menu.findChild<QAction *>("ctxPatch");
        QVERIFY(patch != nullptr && !patch->isEnabled());
        VERIFY_CANON(&menu);
    }
}

/****************************************************************************
 * The other migrated menus: they follow the canon
 ****************************************************************************/

void ContextMenus_Test::buildMenusFollowTheCanon()
{
    Scene *s = new Scene(m_doc);
    s->setName("Warm");
    s->setPath("Act 1");
    m_doc->addFunction(s);
    EFX *efx = new EFX(m_doc);
    efx->setName("Circle");
    m_doc->addFunction(efx);

    ProgrammingManager pm(nullptr, m_doc);
    pm.resize(1500, 900);
    pm.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pm));

    // Build ▸ Scenes: a scene, a folder, empty space.
    ProgrammingOutline *outline = pm.m_outline;
    outline->rebuild();
    outline->expandAll();
    QTreeWidgetItem *scene = outline->sceneItem(s->id());
    QVERIFY(scene != nullptr);
    selectOnly(outline, scene);
    {
        QMenu menu;
        outline->fillContextMenu(&menu, scene);
        VERIFY_CANON(&menu);
        QVERIFY2(!texts(&menu).contains("New Scene"), qPrintable(texts(&menu)));   // New… is the folder's
    }
    QTreeWidgetItem *folder = findRow(outline, [](QTreeWidgetItem *it) {
        return it->data(0, ProgrammingOutline::KindRole).toInt() == ProgrammingOutline::FolderRow
               && !it->data(0, ProgrammingOutline::PathRole).toString().isEmpty();
    });
    QVERIFY(folder != nullptr);
    selectOnly(outline, folder);
    {
        QMenu menu;
        outline->fillContextMenu(&menu, folder);
        VERIFY_CANON(&menu);
        const QString t = texts(&menu);
        QVERIFY2(t.contains("New Scene in Act 1") && t.contains("New Function")
                 && t.contains("Rename Folder") && t.endsWith("Delete Folder"), qPrintable(t));
    }
    outline->clearSelection();
    {
        QMenu menu;
        outline->fillContextMenu(&menu, nullptr);
        VERIFY_CANON(&menu);
    }

    // Build ▸ Effects: an EFX row (Open in Editor first, Delete… last), and
    // empty space (the New… items).
    pm.setSection(ProgrammingManager::EffectsSection);
    QTreeWidget *list = pm.findChild<QTreeWidget *>("buildEffects");
    QVERIFY(list != nullptr);
    QTreeWidgetItem *circle = nullptr;
    QTRY_VERIFY((circle = findRow(list, [](QTreeWidgetItem *it) { return it->text(0) == "Circle"; })) != nullptr);
    {
        QMenu *menu = pm.effectsContextMenu(circle, &pm);
        VERIFY_CANON(menu);
        QCOMPARE(menu->actions().first()->objectName(), QString("effectsOpen"));
        QCOMPARE(menu->actions().last()->objectName(), QString("effectsDelete"));
        QVERIFY(menu->findChild<QAction *>("effectsNewEfx") == nullptr);
        delete menu;
    }
    {
        QMenu *menu = pm.effectsContextMenu(nullptr, &pm);
        VERIFY_CANON(menu);
        QVERIFY(menu->findChild<QAction *>("effectsNewEfx") != nullptr);
        delete menu;
    }

    // Build ▸ Palettes: a palette row and empty space, opened as the user does.
    QLCPalette *pal = new QLCPalette(QLCPalette::Dimmer);
    pal->setName("Half");
    pal->setValue(128);
    m_doc->addPalette(pal);
    pm.setSection(ProgrammingManager::PalettesSection);
    pm.m_paletteTree->updateTree();
    QTreeWidgetItem *palRow = nullptr;
    QTRY_VERIFY((palRow = findRow(pm.m_paletteTree, [&pm, pal](QTreeWidgetItem *it) {
        return pm.m_paletteTree->itemPaletteId(it) == pal->id();
    })) != nullptr);
    selectOnly(pm.m_paletteTree, palRow);
    QStringList problems = popupCheck([&]() {
        pm.slotPaletteTreeMenu(pm.m_paletteTree->visualItemRect(palRow).center());
    });
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join("; ")));
    pm.m_paletteTree->clearSelection();
    problems = popupCheck([&]() {
        pm.slotPaletteTreeMenu(QPoint(5, pm.m_paletteTree->viewport()->height() - 5));
    });
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join("; ")));
}

void ContextMenus_Test::theLibraryMenusFollowTheCanon()
{
    QLCPalette *pal = new QLCPalette(QLCPalette::Dimmer);
    pal->setName("Half");
    pal->setValue(128);
    m_doc->addPalette(pal);
    ShowTreeView view(nullptr, m_doc);
    view.resize(1200, 800);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTreeWidget *lib = view.library();
    int checked = 0;
    for (QTreeWidgetItemIterator it(lib); *it; ++it)
    {
        QMenu menu;
        selectOnly(lib, *it);
        view.fillLibraryMenu(&menu, *it);
        if (menu.isEmpty())
            continue;
        const QStringList p = ContextMenu::checkCanon(&menu);
        QVERIFY2(p.isEmpty(), qPrintable((*it)->text(0) + ": " + p.join("; ") + "\n  in: " + texts(&menu)));
        ++checked;
    }
    QVERIFY(checked >= 3);     // groups, fixtures or palettes, the palette heading
}

void ContextMenus_Test::thePlotMenusFollowTheCanon()
{
    QWidget host;
    Monitor mon(&host, m_doc);
    QMenu menu;
    mon.fillCanvasMenu(&menu, QPointF(10, 10));
    VERIFY_CANON(&menu);
    const QString t = texts(&menu);
    QVERIFY2(t.startsWith("Add Here"), qPrintable(t));
    QVERIFY2(!t.contains("(Ctrl"), qPrintable(t));
}

void ContextMenus_Test::theFunctionsTabFollowsTheCanon()
{
    FunctionManager fm(nullptr, m_doc);
    fm.resize(800, 600);
    fm.show();
    const QStringList problems = popupCheck([&]() { fm.slotTreeContextMenuRequested(); });
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join("; ")));
    // Delete last, New Sequence offered as in the toolbar.
    QMenu probe;
    QVERIFY(fm.m_deleteAction->property(ContextMenu::sectionProperty()).toInt() == ContextMenu::Remove);
    QVERIFY(fm.m_addSequenceAction->property(ContextMenu::sectionProperty()).isValid());
}

QTEST_MAIN(ContextMenus_Test)
