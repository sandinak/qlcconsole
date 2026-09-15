/*
  Q Light Controller Plus - Test Unit
  fixturetreewidget_test.cpp

  Copyright (c) Massimo Callegari

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
#include <QTreeWidgetItemIterator>
#include <QKeyEvent>

#define protected public
#define private public
#include "fixturetreewidget.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "inputoutputmap.h"
#include "doc.h"
#undef private
#undef protected

#include "fixturetreewidget_test.h"

void FixtureTreeWidget_Test::init()
{
    m_doc = new Doc(this);
}

void FixtureTreeWidget_Test::cleanup()
{
    delete m_doc;
}

void FixtureTreeWidget_Test::treeCounts()
{
    Fixture* fxi = new Fixture(m_doc);
    fxi->setChannels(4);
    m_doc->addFixture(fxi);

    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber);
    tree.updateTree();
    QVERIFY(tree.universeCount() >= 1);
    QVERIFY(tree.fixturesCount() >= 1);
    QVERIFY(tree.channelsCount() >= 4);
}

QTEST_MAIN(FixtureTreeWidget_Test)

void FixtureTreeWidget_Test::selectionSurvivesARebuild()
{
    /* updateTree() throws the whole tree away and builds it again. Anything
       that rebuilds for its own reasons -- setShowHeads(), which the Fixture
       Manager calls when it opens a group's LAYOUT EDITOR -- used to come back
       with nothing selected. The manager then sat on a tree whose selection
       contradicted the pane it had just opened, and the next selection signal
       from any source fell through to "nothing selected" and tore the editor
       down. That is the "open a group for layout and it jumps straight back to
       the tree" report, and it was intermittent precisely because it needed a
       second, unrelated event to land.

       Blocking signals during the rebuild (which the widget also does) only
       hides the immediate reentry. The state still has to be right afterwards. */
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Probe");
    fxi->setChannels(4);
    QVERIFY(m_doc->addFixture(fxi));

    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Layout Group");
    grp->assignFixture(fxi->id());
    m_doc->addFixtureGroup(grp);

    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber
                                  | FixtureTreeWidget::ShowGroups);
    tree.updateTree();

    // Find and select the group's row.
    QTreeWidgetItem *groupItem = nullptr;
    QTreeWidgetItemIterator it(&tree);
    while (*it != nullptr)
    {
        if ((*it)->data(0, PROP_GROUP).isValid()
            && (*it)->data(0, PROP_GROUP).toUInt() == grp->id())
        {
            groupItem = *it;
            break;
        }
        ++it;
    }
    QVERIFY2(groupItem != nullptr, "the group never appeared in the tree");
    groupItem->setSelected(true);
    QCOMPARE(tree.selectedItems().size(), 1);

    /* Exactly what opening a layout editor does. */
    tree.setShowHeads(true);

    QCOMPARE(tree.selectedItems().size(), 1);
    const QVariant still = tree.selectedItems().first()->data(0, PROP_GROUP);
    QVERIFY2(still.isValid() && still.toUInt() == grp->id(),
             "the group stopped being selected when the tree rebuilt -- the "
             "Fixture Manager will tear its layout editor down on the next "
             "selection signal from anywhere");

    // And back again, which is what closing the editor does.
    tree.setShowHeads(false);
    QCOMPARE(tree.selectedItems().size(), 1);
    QCOMPARE(tree.selectedItems().first()->data(0, PROP_GROUP).toUInt(), grp->id());
}

/** Find the "Universe N" row for the given universe id, or nullptr. */
static QTreeWidgetItem *findUniverseItem(FixtureTreeWidget &tree, quint32 uni)
{
    QTreeWidgetItemIterator it(&tree);
    while (*it != nullptr)
    {
        if ((*it)->data(0, PROP_UNIVERSE).isValid()
            && (*it)->data(0, PROP_UNIVERSE).toUInt() == uni)
            return *it;
        ++it;
    }
    return nullptr;
}

void FixtureTreeWidget_Test::enterOnAUniverseRowRenamesIt()
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    QVERIFY(iomap->addUniverse());
    const quint32 uni = 0;

    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber
                                  | FixtureTreeWidget::ShowGroups);
    tree.updateTree();

    QTreeWidgetItem *uniItem = findUniverseItem(tree, uni);
    QVERIFY2(uniItem != nullptr, "the universe never appeared in the tree");

    tree.setCurrentItem(uniItem);
    QKeyEvent enterEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    tree.keyPressEvent(&enterEvent);

    QVERIFY2((uniItem->flags() & Qt::ItemIsEditable) != 0,
             "Enter did not arm the universe row for editing");
    QCOMPARE(uniItem->text(0), iomap->getUniverseNameByID(uni));

    // What the real inline editor does on commit: change the text, which
    // fires itemChanged(), wired to slotItemChanged() in the constructor.
    uniItem->setText(0, "Renamed Universe");

    QCOMPARE(iomap->getUniverseNameByID(uni), QString("Renamed Universe"));
    QVERIFY2((uniItem->flags() & Qt::ItemIsEditable) == 0,
             "the row stayed armed after the edit committed");
}

void FixtureTreeWidget_Test::emptyUniverseEditIsRejected()
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    QVERIFY(iomap->addUniverse());
    const quint32 uni = 0;
    iomap->setUniverseName(int(uni), "Original Name");

    FixtureTreeWidget tree(m_doc, FixtureTreeWidget::UniverseNumber
                                  | FixtureTreeWidget::ShowGroups);
    tree.updateTree();

    QTreeWidgetItem *uniItem = findUniverseItem(tree, uni);
    QVERIFY2(uniItem != nullptr, "the universe never appeared in the tree");

    tree.setCurrentItem(uniItem);
    QKeyEvent enterEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    tree.keyPressEvent(&enterEvent);

    uniItem->setText(0, "");   // cleared the field, then committed

    QCOMPARE(iomap->getUniverseNameByID(uni), QString("Original Name"));
    QCOMPARE(uniItem->text(0), QString("Original Name"));
}
