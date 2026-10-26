/*
  Q Light Controller Plus - Test Unit
  stagedscreens_test.cpp

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
#include <QTableWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#define private public
#define protected public
#include "channelsselection.h"
#include "addchannelsgroup.h"
#include "connectionstree.h"
#include "fixturegroupsource.h"
#include "scenegrouplooks.h"
#include "fixturegroupeditor.h"
#undef protected
#undef private
#include "channelsgroup.h"
#include "fixturegroup.h"
#include "scene.h"
#include "fixture.h"
#include "doc.h"

#include "stagedscreens_test.h"

static Fixture *dimmer(Doc *doc, const QString &name, bool patched)
{
    Fixture *f = new Fixture(doc);
    f->setName(name);
    f->setChannels(2);
    f->setPatched(patched);
    if (!doc->addFixture(f))
    {
        delete f;
        return nullptr;
    }
    return f;
}

/* Every row of @p tree whose text contains @p needle. */
static QList<QTreeWidgetItem *> rowsWith(QTreeWidget *tree, const QString &needle)
{
    QList<QTreeWidgetItem *> out;
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it)
        for (int c = 0; c < tree->columnCount(); c++)
            if ((*it)->text(c).contains(needle))
            {
                out << *it;
                break;
            }
    return out;
}

void StagedScreens_Test::init()
{
    m_doc = new Doc(this);
    m_real = dimmer(m_doc, "Real Dimmer", true);
    m_staged = dimmer(m_doc, "Staged Dimmer", false);
    QVERIFY(m_real != nullptr && m_staged != nullptr);
}

void StagedScreens_Test::cleanup()
{
    delete m_doc;
}

void StagedScreens_Test::channelPickersListOnlyPatchedFixtures()
{
    ChannelsSelection sel(m_doc);
    QVERIFY(!rowsWith(sel.m_channelsTree, "Real Dimmer").isEmpty());
    QVERIFY(rowsWith(sel.m_channelsTree, "Staged Dimmer").isEmpty());

    ChannelsGroup *grp = new ChannelsGroup(m_doc);
    grp->setName("Faders");
    QVERIFY(m_doc->addChannelsGroup(grp));
    AddChannelsGroup add(nullptr, m_doc, grp);
    QVERIFY(!rowsWith(add.m_tree, "Real Dimmer").isEmpty());
    QVERIFY(rowsWith(add.m_tree, "Staged Dimmer").isEmpty());
}

void StagedScreens_Test::theConnectionsListCountsOnlyPatchedFixtures()
{
    /* By universe's Carries (the Overview grid's Usage, folded in): the
       staged fixture is not in universe 1. */
    ConnectionsTree tree(m_doc);
    tree.m_showUnused->setChecked(true);
    tree.m_byUniverse->setChecked(true);
    QTreeWidgetItem *first = tree.m_tree->topLevelItem(0);
    QVERIFY(first != nullptr);
    const QString usage = first->text(2);
    QVERIFY2(usage.startsWith("1 fx") && usage.contains("2/512"), qPrintable(usage));
    tree.m_byUniverse->setChecked(false);
}

void StagedScreens_Test::theProgrammingSourceTreeFilesStagedFixturesUnderInventory()
{
    Fixture *b = dimmer(m_doc, "another staged", false);    // sorts before "Staged"
    QVERIFY(b != nullptr);
    FixtureGroupSource src(m_doc);
    src.reload();

    QTreeWidgetItem *universes = nullptr, *inventory = nullptr;
    for (int i = 0; i < src.topLevelItemCount(); i++)
    {
        if (src.topLevelItem(i)->text(0) == "Universes")
            universes = src.topLevelItem(i);
        else if (src.topLevelItem(i)->text(0) == "Inventory")
            inventory = src.topLevelItem(i);
    }
    QVERIFY(universes != nullptr && inventory != nullptr);
    QCOMPARE(inventory->data(0, FixtureGroupSource::KindRole).toInt(), int(FixtureGroupSource::CategoryNode));
    QCOMPARE(inventory->childCount(), 2);
    QCOMPARE(inventory->child(0)->data(0, FixtureGroupSource::IdRole).toUInt(), b->id());
    QCOMPARE(inventory->child(1)->data(0, FixtureGroupSource::IdRole).toUInt(), m_staged->id());
    // Under Universes, only the real one.
    QCOMPARE(universes->childCount(), 1);
    QCOMPARE(universes->child(0)->childCount(), 1);
    QCOMPARE(universes->child(0)->child(0)->data(0, FixtureGroupSource::IdRole).toUInt(), m_real->id());

    // Nothing staged: no empty Inventory folder.
    m_doc->deleteFixture(b->id());
    m_doc->deleteFixture(m_staged->id());
    src.reload();
    for (int i = 0; i < src.topLevelItemCount(); i++)
        QVERIFY(src.topLevelItem(i)->text(0) != "Inventory");
}

void StagedScreens_Test::aLookTargetSaysItIsUnpatched()
{
    Scene *scene = new Scene(m_doc);
    scene->setName("Look");
    scene->setValue(m_real->id(), 0, 255);
    scene->setValue(m_staged->id(), 0, 255);
    QVERIFY(m_doc->addFunction(scene));

    SceneGroupLooks looks(scene, m_doc, nullptr, true);
    QTreeWidgetItem *real = nullptr, *staged = nullptr;
    for (QTreeWidgetItemIterator it(looks.m_targetList); *it != nullptr; ++it)
    {
        if ((*it)->data(0, Qt::UserRole).toUInt() == m_real->id() && (*it)->text(0).startsWith("Real"))
            real = *it;
        if ((*it)->data(0, Qt::UserRole).toUInt() == m_staged->id() && (*it)->text(0).startsWith("Staged"))
            staged = *it;
    }
    QVERIFY(real != nullptr && staged != nullptr);
    QVERIFY(!real->text(0).contains("unpatched"));
    QVERIFY2(staged->text(0).endsWith("(unpatched)"), qPrintable(staged->text(0)));
    QVERIFY(staged->toolTip(0).contains("inventory"));
}

void StagedScreens_Test::aGroupLayoutSaysAHeadIsUnpatched()
{
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Pair");
    grp->setSize(QSize(4, 1));                          // two heads each
    QVERIFY(m_doc->addFixtureGroup(grp));
    grp->assignFixture(m_real->id());
    grp->assignFixture(m_staged->id());

    FixtureGroupEditor ed(grp, m_doc, nullptr);
    QStringList cells;
    for (int r = 0; r < ed.m_table->rowCount(); r++)
        for (int c = 0; c < ed.m_table->columnCount(); c++)
            if (QTableWidgetItem *it = ed.m_table->item(r, c))
                cells << it->text();
    const QString all = cells.join(" | ");
    QVERIFY2(all.contains("Real Dimmer H:1\nA:1 U:1"), qPrintable(all));
    QVERIFY2(all.contains("Staged Dimmer H:1\nunpatched"), qPrintable(all));
}

QTEST_MAIN(StagedScreens_Test)
