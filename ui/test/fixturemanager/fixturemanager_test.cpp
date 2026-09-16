/*
  Q Light Controller Plus - Test Unit
  fixturemanager_test.cpp

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
#include <QSplitter>

#define KColumnName 0

#define protected public
#define private public
#include "fixturemanager.h"
#include "fixturetreewidget.h"
#include "fixture.h"
#include "inputoutputmap.h"
#include "fixturegroup.h"
#include "fixturegroupeditor.h"
#include "doc.h"
#undef private
#undef protected

#include "fixturemanager_test.h"

void FixtureManager_Test::init()
{
    m_doc = new Doc(this);
}

void FixtureManager_Test::cleanup()
{
    delete m_doc;
}

void FixtureManager_Test::deletingAFixtureRemovesItsRowUnderItsUniverse()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Probe");
    fxi->setChannels(4);
    QVERIFY(m_doc->addFixture(fxi));
    const quint32 fid = fxi->id();

    FixtureManager mgr(nullptr, m_doc);

    // The row must exist right after construction, nested under its
    // universe folder (Universes -> Universe N -> fixture) -- the shape
    // FixtureManager's tree always uses (ShowGroups is unconditional).
    auto findRow = [&]() -> QTreeWidgetItem * {
        QTreeWidgetItemIterator it(mgr.m_fixtures_tree);
        while (*it != nullptr)
        {
            if ((*it)->data(KColumnName, PROP_ID).isValid()
                && (*it)->data(KColumnName, PROP_ID).toString().toUInt() == fid)
                return *it;
            ++it;
        }
        return nullptr;
    };

    QVERIFY2(findRow() != nullptr, "the fixture never appeared in the tree");

    // What a real delete does: Doc::deleteFixture() emits fixtureRemoved(),
    // which FixtureManager is connected to (slotFixtureRemoved()). This
    // used to only look one level below each top-level item, which finds a
    // fixture living directly under a group folder but never one nested a
    // level further down under a universe folder.
    QVERIFY(m_doc->deleteFixture(fid));

    QVERIFY2(findRow() == nullptr,
             "the fixture's row is still in the tree after Doc::deleteFixture() "
             "-- the delete looked like it silently did nothing");
}

/* "When I click on a fixture group it's not displaying the layout of the group
   .. just jumping around in the tree." */
void FixtureManager_Test::selectingAGroupOpensItsLayoutEditor()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Member");
    fxi->setChannels(4);
    QVERIFY(m_doc->addFixture(fxi));

    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Block A");
    grp->setSize(QSize(4, 2));
    m_doc->addFixtureGroup(grp);
    grp->assignFixture(fxi->id());
    const quint32 gid = grp->id();

    FixtureManager mgr(nullptr, m_doc);

    // Find the group's row the way the manager tags it.
    QTreeWidgetItem *row = nullptr;
    QTreeWidgetItemIterator it(mgr.m_fixtures_tree);
    while (*it != nullptr)
    {
        const QVariant g = (*it)->data(KColumnName, PROP_GROUP);
        if (g.isValid() && g.toUInt() == gid) { row = *it; break; }
        ++it;
    }
    QVERIFY2(row != nullptr, "the group never got a row carrying its id");

    // Select it exactly as a click does, then let the manager react.
    /* Select it exactly as a click does. NOTE: nothing may hold `row` across
       this -- selecting a group rebuilds the tree, so the pointer dies. That
       is the bug itself: before the rebuild was deferred, this crashed. */
    mgr.m_fixtures_tree->clearSelection();
    row->setSelected(true);
    row = nullptr;

    // Let the deferred rebuild run, the way the event loop does in the app.
    QCoreApplication::processEvents();

    QVERIFY2(mgr.m_groupEditor != nullptr,
             "selecting a fixture group did not open its layout editor");

    /* And it must SURVIVE the deferred tree rebuild. fixtureGroupSelected()
       calls setShowHeads(true), whose updateTree() is deferred to the next turn
       of the event loop (rebuilding mid-click deletes the row Qt is still
       handling). Nothing in this test ran that timer before, so the window
       after it fires was never covered -- which is where the app lives. */
    QCoreApplication::processEvents();
    QVERIFY2(mgr.m_groupEditor != nullptr,
             "the group's layout editor vanished once the deferred tree rebuild ran");

    /* Created is not the same as VISIBLE. fixtureGroupSelected() restores a
       splitter state captured BEFORE the editor existed, so the pane can come
       back with zero width -- the editor is there and you cannot see it, which
       reads exactly like clicking the group doing nothing. */
    /* Measured on the SPLITTER's allotment, not the editor's width(): a widget
       only has a width once something has laid it out, which depends on the
       platform and the window manager -- it held locally and was 0 under the
       gate on all four configurations. The splitter's sizes are what the guard
       actually controls, and they are deterministic. */
    mgr.resize(1100, 700);
    mgr.m_splitter->setSizes(QList<int>() << 700 << 400);
    QCoreApplication::processEvents();
    QVERIFY2(mgr.m_splitter->sizes().value(1) > 20,
             qPrintable(QString("the layout editor's pane is %1 px wide -- invisible")
                        .arg(mgr.m_splitter->sizes().value(1))));

    /* And it must survive the pane having been COLLAPSED beforehand -- by a
       drag, or by a splitter state persisted from an earlier session. That is
       the state a fresh test never reproduces on its own, and the one where
       "clicking the group does nothing" comes from. */
    QList<int> collapsed = mgr.m_splitter->sizes();
    QVERIFY(collapsed.size() >= 2);
    collapsed[0] = collapsed.at(0) + collapsed.at(1);
    collapsed[1] = 0;
    mgr.m_splitter->setSizes(collapsed);
    QCoreApplication::processEvents();

    mgr.m_fixtures_tree->clearSelection();
    mgr.slotSelectionChanged();          // away from the group

    // Re-find the row: the rebuild replaced it, and the old pointer is dead.
    QTreeWidgetItem *again = nullptr;
    QTreeWidgetItemIterator it2(mgr.m_fixtures_tree);
    while (*it2 != nullptr)
    {
        const QVariant g = (*it2)->data(KColumnName, PROP_GROUP);
        if (g.isValid() && g.toUInt() == gid) { again = *it2; break; }
        ++it2;
    }
    QVERIFY(again != nullptr);
    again->setSelected(true);
    mgr.slotSelectionChanged();          // and back to it
    QCoreApplication::processEvents();

    QVERIFY(mgr.m_groupEditor != nullptr);
    QVERIFY2(mgr.m_splitter->sizes().value(1) > 20,
             qPrintable(QString("with the pane collapsed the editor came back in a "
                                "%1 px pane -- indistinguishable from doing nothing")
                        .arg(mgr.m_splitter->sizes().value(1))));

    // ...and the selection is still on the group afterwards, rather than the
    // tree having rebuilt underneath the click and moved it somewhere else.
    const QList<QTreeWidgetItem *> after = mgr.m_fixtures_tree->selectedItems();
    QCOMPARE(after.count(), 1);
    const QVariant still = after.first()->data(KColumnName, PROP_GROUP);
    QVERIFY2(still.isValid() && still.toUInt() == gid,
             "the selection moved off the group after clicking it");
}

QTEST_MAIN(FixtureManager_Test)
