/*
  Q Light Controller Plus - Test Unit
  searchkeepsselection_test.cpp

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
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include "../../../engine/test/common/stdprelude.h"
#include "fixturetreewidget.h"
#include "fixturegroupsource.h"
#include "functionstreewidget.h"
#include "programmingoutline.h"
#include "showtreeview.h"
#include <QLineEdit>
#include "fixturegroup.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

#include "searchkeepsselection_test.h"

void SearchKeepsSelection_Test::init()
{
    m_doc = new Doc(this);
}

void SearchKeepsSelection_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

static void showSmall(QTreeWidget *t)
{
    t->resize(300, 150);     // a few rows: the list has to scroll
    t->show();
    QCoreApplication::processEvents();
}

static QTreeWidgetItem *rowNamed(QTreeWidget *t, const QString &start)
{
    for (QTreeWidgetItemIterator it(t); *it; ++it)
        if ((*it)->text(0).startsWith(start))
            return *it;
    return nullptr;
}

/* Selected, current, every parent open, and its row inside the viewport. */
static bool stillThere(QTreeWidget *t, QTreeWidgetItem *it)
{
    if (it == nullptr || !it->isSelected() || t->currentItem() != it)
        return false;
    for (QTreeWidgetItem *p = it->parent(); p != nullptr; p = p->parent())
        if (!p->isExpanded())
            return false;
    return !it->isHidden() && t->visualItemRect(it).intersects(t->viewport()->rect());
}

void SearchKeepsSelection_Test::fixtureTreeStaysOnTheHitWhenCleared()
{
    for (int i = 0; i < 80; ++i)
    {
        Fixture *f = new Fixture(m_doc);
        f->setName(QString("Fix %1").arg(i, 2, 10, QChar('0')));
        f->setChannels(4);
        m_doc->addFixture(f);
    }
    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber);
    tree.updateTree();
    showSmall(&tree);

    tree.setTextFilter("Fix 61");
    QTreeWidgetItem *hit = rowNamed(&tree, "Fix 61");
    QVERIFY(hit != nullptr);
    tree.setCurrentItem(hit);
    QVERIFY(hit->isSelected());

    tree.setTextFilter(QString());
    QVERIFY(stillThere(&tree, hit));
}

void SearchKeepsSelection_Test::fixtureTreeStaysWhileTheSearchChanges()
{
    for (int i = 0; i < 80; ++i)
    {
        Fixture *f = new Fixture(m_doc);
        f->setName(QString("Fix %1").arg(i, 2, 10, QChar('0')));
        f->setChannels(4);
        m_doc->addFixture(f);
    }
    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber);
    tree.updateTree();
    showSmall(&tree);

    tree.setTextFilter("Fix 6");
    QTreeWidgetItem *hit = rowNamed(&tree, "Fix 61");
    tree.setCurrentItem(hit);
    tree.scrollToTop();
    // Still matches: stays selected, and in view.
    tree.setTextFilter("Fix 61");
    QVERIFY(stillThere(&tree, hit));
    // No longer matches: the tree does what it did (nothing is done to it).
    tree.setTextFilter("Fix 7");
    QVERIFY(hit->isHidden());
    QVERIFY(hit->isSelected());
}

void SearchKeepsSelection_Test::fixtureTreeKeepsAMultiSelection()
{
    for (int i = 0; i < 80; ++i)
    {
        Fixture *f = new Fixture(m_doc);
        f->setName(QString("Fix %1").arg(i, 2, 10, QChar('0')));
        f->setChannels(4);
        m_doc->addFixture(f);
    }
    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber);
    tree.setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree.updateTree();
    showSmall(&tree);

    tree.setTextFilter("Fix 6");
    QTreeWidgetItem *a = rowNamed(&tree, "Fix 62");
    QTreeWidgetItem *b = rowNamed(&tree, "Fix 65");
    QTreeWidgetItem *c = rowNamed(&tree, "Fix 68");
    QVERIFY(a && b && c);
    tree.setCurrentItem(a);
    b->setSelected(true);
    c->setSelected(true);
    tree.setCurrentItem(b, 0, QItemSelectionModel::NoUpdate);

    tree.setTextFilter(QString());
    QVERIFY(a->isSelected() && b->isSelected() && c->isSelected());
    QVERIFY(stillThere(&tree, b));
}

void SearchKeepsSelection_Test::buildGroupsTreeStaysOnTheHitWhenCleared()
{
    for (int i = 0; i < 90; ++i)
    {
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(QString("Grp %1").arg(i, 2, 10, QChar('0')));
        g->setPath(QString("Folder %1/Sub").arg(i / 30));
        m_doc->addFixtureGroup(g);
    }
    FixtureGroupSource tree(m_doc);
    tree.reload();
    showSmall(&tree);

    tree.setFilterText("Grp 72");
    QTreeWidgetItem *hit = rowNamed(&tree, "Grp 72");
    QVERIFY(hit != nullptr);
    tree.setCurrentItem(hit);

    tree.setFilterText(QString());
    hit = rowNamed(&tree, "Grp 72");
    QVERIFY(stillThere(&tree, hit));
}

void SearchKeepsSelection_Test::functionsTreeStaysOnTheHitWhenCleared()
{
    for (int i = 0; i < 90; ++i)
    {
        Scene *s = new Scene(m_doc);
        s->setName(QString("Look %1").arg(i, 2, 10, QChar('0')));
        s->setPath(QString("Folder %1").arg(i / 30));
        m_doc->addFunction(s);
    }
    FunctionsTreeWidget tree(m_doc);
    tree.updateTree();
    showSmall(&tree);

    tree.filterByText("Look 72");
    QTreeWidgetItem *hit = rowNamed(&tree, "Look 72");
    QVERIFY(hit != nullptr);
    tree.setCurrentItem(hit);

    tree.filterByText(QString());
    QVERIFY(stillThere(&tree, hit));
}

void SearchKeepsSelection_Test::outlineStaysOnTheHitWhenCleared()
{
    for (int i = 0; i < 90; ++i)
    {
        Scene *s = new Scene(m_doc);
        s->setName(QString("Look %1").arg(i, 2, 10, QChar('0')));
        s->setPath(QString("Folder %1").arg(i / 30));
        m_doc->addFunction(s);
    }
    ProgrammingOutline tree(m_doc);
    tree.rebuild();
    showSmall(&tree);

    tree.setFilterText("Look 72");
    QTreeWidgetItem *hit = rowNamed(&tree, "Look 72");
    QVERIFY(hit != nullptr);
    tree.setCurrentItem(hit);

    tree.setFilterText(QString());
    QVERIFY(stillThere(&tree, hit));
}

void SearchKeepsSelection_Test::showTreeLibraryStaysOnTheHitWhenCleared()
{
    Fixture *f = new Fixture(m_doc);
    f->setName("Spot");
    f->setChannels(4);
    m_doc->addFixture(f);
    for (int i = 0; i < 90; ++i)
    {
        FixtureGroup *g = new FixtureGroup(m_doc);
        g->setName(QString("Rig %1").arg(i, 2, 10, QChar('0')));
        m_doc->addFixtureGroup(g);
        g->assignFixture(f->id());
    }
    ShowTreeView view(nullptr, m_doc);
    QLineEdit *filter = view.findChild<QLineEdit *>("stLibraryFilter");
    QVERIFY(filter != nullptr);
    QTreeWidget *tree = view.library();
    view.resize(500, 400);
    view.show();
    tree->resize(300, 150);
    QCoreApplication::processEvents();

    filter->setText("Rig 72");
    QTreeWidgetItem *hit = rowNamed(tree, "Rig 72");
    QVERIFY(hit != nullptr);
    tree->setCurrentItem(hit);

    filter->clear();
    QVERIFY(stillThere(tree, hit));
}

QTEST_MAIN(SearchKeepsSelection_Test)
