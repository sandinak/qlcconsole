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

#define KColumnName 0

#define protected public
#define private public
#include "fixturemanager.h"
#include "fixturetreewidget.h"
#include "fixture.h"
#include "inputoutputmap.h"
#include "fixturegroup.h"
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

    // ...and the selection is still on the group afterwards, rather than the
    // tree having rebuilt underneath the click and moved it somewhere else.
    const QList<QTreeWidgetItem *> after = mgr.m_fixtures_tree->selectedItems();
    QCOMPARE(after.count(), 1);
    const QVariant still = after.first()->data(KColumnName, PROP_GROUP);
    QVERIFY2(still.isValid() && still.toUInt() == gid,
             "the selection moved off the group after clicking it");
}

QTEST_MAIN(FixtureManager_Test)
