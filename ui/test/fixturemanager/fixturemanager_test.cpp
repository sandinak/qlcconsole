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
#include <QInputDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLineEdit>
#include <QMouseEvent>
#include <functional>

#define KColumnName 0

#define protected public
#define private public
#include "fixturemanager.h"
#include "fixturetreewidget.h"
#include "fixture.h"
#include "inputoutputmap.h"
#include "fixturegroup.h"
#include "fixturegroupeditor.h"
#include "createfixturegroup.h"
#include "doc.h"
#undef private
#undef protected

#include "fixturemanager_test.h"

/* Schedules a zero-delay callback that runs `fill` against whatever modal
 * QDialog is open by then (via QApplication::activeModalWidget() -- called
 * BEFORE the action that opens the dialog, same as the QTimer::singleShot()
 * calls this replaces), then ALWAYS clicks its Ok button -- or closes it
 * outright if that button can't be found -- BEFORE checking whether `fill`
 * reported success.
 *
 * Why the close has to come first: a QVERIFY/QCOMPARE failure inside `fill`
 * itself returns out of `fill` early (reporting false), but if the close
 * were the LAST thing this function did, that early return would skip it
 * entirely -- and nothing else will ever close that dialog under offscreen
 * QTest, so its nested event loop blocks forever: a wrong widget lookup
 * would hang the whole test binary instead of just failing the one test
 * that made it. See qlcconsole's WORKFLOW-SUBTLETIES.md #7 (this shape was
 * hand-rigged at each call site here before being pulled out once). */
static void driveDialog(const std::function<bool(QWidget *)> &fill)
{
    QTimer::singleShot(0, [fill]() {
        QWidget *modal = QApplication::activeModalWidget();
        const bool filled = (modal != nullptr) && fill(modal);
        QDialogButtonBox *btns = modal ? modal->findChild<QDialogButtonBox *>() : nullptr;
        QPushButton *btn = btns ? btns->button(QDialogButtonBox::Ok) : nullptr;
        if (btn != nullptr)
            btn->click();
        else if (modal != nullptr)
            modal->close();
        QVERIFY2(modal != nullptr && filled && btn != nullptr,
                 "the dialog's widgets were not where expected");
    });
}

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

/* Chains the fixture-group workflow: create two groups through the real,
 * shared "Create Fixture Group" dialog, open one's real layout editor by
 * selecting it in the tree, click a real cell to anchor the next paste,
 * paste the other group in as a block through the real "Add group as
 * block..." handler and its own picker dialog, then move that block as a
 * unit and check every head lands exactly where the geometry says.
 *
 * Two scope choices, both disclosed in WORKFLOW-SUBTLETIES.md: creating a
 * group here uses the shared CreateFixtureGroup::createFromFixtures()
 * factory rather than simulating a right-click "New Group..." on the
 * tree's folder row (menu.exec() plus the QAction dispatch it would need
 * is a lot of additional simulated-UI surface for the same real dialog
 * underneath); and "move as a unit" calls FixtureGroupEditor::
 * moveSubGroup() directly -- the exact function the right-click "Move
 * sub-group" context-menu action reaches -- rather than synthesizing the
 * internal QDrag/QDropEvent cycle a mouse drag actually goes through,
 * which risks needing a real OS-level drag session under offscreen QTest.
 */
void FixtureManager_Test::populatingAndMovingAFixtureGroupBlockThroughRealUi()
{
    // 1. Fixtures for two groups -- left unplaced so
    //    CreateFixtureGroup::suggestGrid()'s "near-square of the head
    //    count" fallback gives an exact, predictable grid: 1 fixture -> 1x1,
    //    2 fixtures -> 2x1.
    Fixture *targetFx = new Fixture(m_doc);
    targetFx->setName("Target Member"); targetFx->setChannels(1);
    QVERIFY(m_doc->addFixture(targetFx));

    Fixture *blockFxA = new Fixture(m_doc);
    blockFxA->setName("Block Member A"); blockFxA->setChannels(1);
    blockFxA->setAddress(16);
    QVERIFY(m_doc->addFixture(blockFxA));
    Fixture *blockFxB = new Fixture(m_doc);
    blockFxB->setName("Block Member B"); blockFxB->setChannels(1);
    blockFxB->setAddress(32);
    QVERIFY(m_doc->addFixture(blockFxB));

    // 2. Create the TARGET group -- the real "Create Fixture Group" dialog,
    //    the same shared factory the Fixture Manager, the Programming tab
    //    and the Studio all use to create a group from a selection.
    driveDialog([](QWidget *modal) {
        CreateFixtureGroup *cfg = qobject_cast<CreateFixtureGroup *>(modal);
        if (cfg == nullptr)
            return false;
        cfg->m_nameEdit->setText("Workflow Target Group");
        return true;
    });
    FixtureGroup *targetGrp = CreateFixtureGroup::createFromFixtures(
        m_doc, QList<quint32>() << targetFx->id(), nullptr);
    QVERIFY2(targetGrp != nullptr, "createFromFixtures() returned no group");
    QCOMPARE(targetGrp->name(), QString("Workflow Target Group"));
    QCOMPARE(targetGrp->size(), QSize(1, 1));
    QCOMPARE(targetGrp->headsMap().value(QLCPoint(0, 0)).fxi, targetFx->id());
    // Grow it past its one occupied cell -- a real user would do this with
    // the editor's own width/height spin boxes; this is the same kind of
    // direct-API simplification selectingAGroupOpensItsLayoutEditor() above
    // already uses for its own group's size, done here so the paste in
    // step 6 has an uncontested empty cell to land on rather than fighting
    // over (0,0) with the fixture already there.
    targetGrp->setSize(QSize(3, 2));

    // 3. Create the SOURCE (block) group the same real way.
    driveDialog([](QWidget *modal) {
        CreateFixtureGroup *cfg = qobject_cast<CreateFixtureGroup *>(modal);
        if (cfg == nullptr)
            return false;
        cfg->m_nameEdit->setText("Workflow Block Group");
        return true;
    });
    FixtureGroup *blockGrp = CreateFixtureGroup::createFromFixtures(
        m_doc, QList<quint32>() << blockFxA->id() << blockFxB->id(), nullptr);
    QVERIFY2(blockGrp != nullptr, "createFromFixtures() returned no group");
    QCOMPARE(blockGrp->size(), QSize(2, 1));
    QCOMPARE(blockGrp->headsMap().value(QLCPoint(0, 0)).fxi, blockFxA->id());
    QCOMPARE(blockGrp->headsMap().value(QLCPoint(1, 0)).fxi, blockFxB->id());

    // 4. Open the target group's real layout editor by selecting it in the
    //    tree -- the exact path selectingAGroupOpensItsLayoutEditor() above
    //    already proves opens FixtureManager::m_groupEditor.
    FixtureManager mgr(nullptr, m_doc);
    mgr.resize(1100, 700);
    mgr.m_splitter->setSizes(QList<int>() << 700 << 400);

    QTreeWidgetItem *targetRow = nullptr;
    QTreeWidgetItemIterator it(mgr.m_fixtures_tree);
    while (*it != nullptr)
    {
        const QVariant g = (*it)->data(KColumnName, PROP_GROUP);
        if (g.isValid() && g.toUInt() == targetGrp->id()) { targetRow = *it; break; }
        ++it;
    }
    QVERIFY2(targetRow != nullptr, "the target group never got a row carrying its id");
    mgr.m_fixtures_tree->clearSelection();
    targetRow->setSelected(true);
    targetRow = nullptr;   // dies on the deferred rebuild -- see the test above
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY2(mgr.m_groupEditor != nullptr,
             "selecting the target group did not open its layout editor");

    FixtureGroupEditor *editor = mgr.m_groupEditor;
    QCoreApplication::processEvents();   // let the splitter/table settle to real geometry

    // 5. Click a real cell -- a plain press+release (never crossing the
    //    drag-distance threshold, so this can never arm startCellDrag())
    //    through the SAME eventFilter() a real click on the grid goes
    //    through, to set where the next paste anchors.
    QVERIFY2(editor->m_table != nullptr, "the layout editor has no table");
    // Column 1, not 0: column 0 already holds the target group's own
    // fixture (from creation, above) -- clicking there would still set the
    // anchor correctly, but the block would then have to paste ON TOP of
    // an already-occupied cell, which is its own separate (and untested
    // here) question. Anchoring on an empty cell keeps this test about the
    // paste-and-move mechanics, not the collision rule.
    const QPoint cellPx(editor->m_table->columnViewportPosition(1) + editor->m_table->columnWidth(1) / 2,
                        editor->m_table->rowViewportPosition(0) + editor->m_table->rowHeight(0) / 2);
    QMouseEvent press(QEvent::MouseButtonPress, cellPx, cellPx,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    editor->eventFilter(editor->m_table->viewport(), &press);
    QMouseEvent release(QEvent::MouseButtonRelease, cellPx, cellPx,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    editor->eventFilter(editor->m_table->viewport(), &release);
    QCOMPARE(editor->m_column, 1);
    QCOMPARE(editor->m_row, 0);

    // 6. Paste the block group in -- FixtureGroupEditor::slotAddGroupBlock(),
    //    the exact handler the real "Add group as block..." button calls,
    //    driving the real QInputDialog it opens to pick which group.
    driveDialog([](QWidget *modal) {
        QComboBox *combo = modal->findChild<QComboBox *>();
        if (combo == nullptr)
            return false;
        combo->setCurrentText("Workflow Block Group");
        return true;
    });
    editor->slotAddGroupBlock();

    // Anchored at (1,0): block A/B keep their own relative layout (A at the
    // block's own origin, B one cell along), tagged with the source group's
    // id, and the target group's own original member at (0,0) is untouched.
    const QMap<QLCPoint, GroupHead> afterPaste = targetGrp->headsMap();
    QCOMPARE(afterPaste.value(QLCPoint(0, 0)).fxi, targetFx->id());
    QCOMPARE(afterPaste.value(QLCPoint(1, 0)).fxi, blockFxA->id());
    QCOMPARE(afterPaste.value(QLCPoint(2, 0)).fxi, blockFxB->id());
    QCOMPARE(targetGrp->headSubGroup(QLCPoint(1, 0)), blockGrp->id());
    QCOMPARE(targetGrp->headSubGroup(QLCPoint(2, 0)), blockGrp->id());

    // 7. Move the block as a unit. Real alternate UI path for the same
    //    result as a drag: the right-click "Move sub-group" action reaches
    //    exactly this function.
    editor->moveSubGroup(blockGrp->id(), 0, 1);

    const QMap<QLCPoint, GroupHead> afterMove = targetGrp->headsMap();
    QVERIFY2(afterMove.value(QLCPoint(1, 1)).fxi == blockFxA->id(),
             "moving the block by (0,1) did not carry member A to (1,1)");
    QVERIFY2(afterMove.value(QLCPoint(2, 1)).fxi == blockFxB->id(),
             "moving the block by (0,1) did not carry member B to (2,1)");
    QVERIFY2(afterMove.contains(QLCPoint(1, 0)) == false,
             "the block's old (1,0) cell is still occupied after moving away from it");
    QVERIFY2(afterMove.contains(QLCPoint(2, 0)) == false,
             "the block's old (2,0) cell is still occupied after moving away from it");
    QCOMPARE(afterMove.value(QLCPoint(0, 0)).fxi, targetFx->id());   // untouched by the move
    QCOMPARE(targetGrp->headSubGroup(QLCPoint(1, 1)), blockGrp->id());
    QCOMPARE(targetGrp->headSubGroup(QLCPoint(2, 1)), blockGrp->id());
}

QTEST_MAIN(FixtureManager_Test)
