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
#include <QMimeData>
#include <QDropEvent>
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
    {
        // Scoped so the iterator is destroyed (and unregistered from the
        // tree model) the moment the lookup is done, rather than sitting
        // idle-but-still-registered through the group-changed rebuild
        // later in this test -- see
        // pastingABlockOntoAnOccupiedCellOverwritesTheOccupant()'s own
        // fuller comment on why a lingering QTreeWidgetItemIterator (not
        // just a lingering raw QTreeWidgetItem* like targetRow itself)
        // is a real hazard here, not a theoretical one.
        QTreeWidgetItemIterator it(mgr.m_fixtures_tree);
        while (*it != nullptr)
        {
            const QVariant g = (*it)->data(KColumnName, PROP_GROUP);
            if (g.isValid() && g.toUInt() == targetGrp->id()) { targetRow = *it; break; }
            ++it;
        }
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

void FixtureManager_Test::pastingABlockOntoAnOccupiedCellOverwritesTheOccupant()
{
    // 1. An occupant fixture for the paste's destination cell, a member the
    //    paste must leave untouched, and the source block's two fixtures.
    Fixture *targetFx = new Fixture(m_doc);
    targetFx->setName("Target Member"); targetFx->setChannels(1);
    QVERIFY(m_doc->addFixture(targetFx));

    Fixture *occupantFx = new Fixture(m_doc);
    occupantFx->setName("Occupant"); occupantFx->setChannels(1);
    occupantFx->setAddress(8);
    QVERIFY(m_doc->addFixture(occupantFx));

    Fixture *blockFxA = new Fixture(m_doc);
    blockFxA->setName("Block Member A"); blockFxA->setChannels(1);
    blockFxA->setAddress(16);
    QVERIFY(m_doc->addFixture(blockFxA));
    Fixture *blockFxB = new Fixture(m_doc);
    blockFxB->setName("Block Member B"); blockFxB->setChannels(1);
    blockFxB->setAddress(32);
    QVERIFY(m_doc->addFixture(blockFxB));

    // 2. Target group via the real "Create Fixture Group" dialog, same as
    //    the sibling workflow test.
    driveDialog([](QWidget *modal) {
        CreateFixtureGroup *cfg = qobject_cast<CreateFixtureGroup *>(modal);
        if (cfg == nullptr)
            return false;
        cfg->m_nameEdit->setText("Collision Target Group");
        return true;
    });
    FixtureGroup *targetGrp = CreateFixtureGroup::createFromFixtures(
        m_doc, QList<quint32>() << targetFx->id(), nullptr);
    QVERIFY2(targetGrp != nullptr, "createFromFixtures() returned no group");
    targetGrp->setSize(QSize(3, 2));

    // Plant an occupant at (1,0) -- the cell the paste below will land on --
    // directly via the model, the same kind of direct-API simplification
    // the setSize() call above already uses for setup that isn't the part
    // under test.
    QVERIFY2(targetGrp->assignHead(QLCPoint(1, 0), GroupHead(occupantFx->id(), 0)),
             "could not plant the occupant fixture ahead of the paste");
    QCOMPARE(targetGrp->headsMap().value(QLCPoint(1, 0)).fxi, occupantFx->id());

    // 3. Source (block) group, same as the sibling workflow test.
    driveDialog([](QWidget *modal) {
        CreateFixtureGroup *cfg = qobject_cast<CreateFixtureGroup *>(modal);
        if (cfg == nullptr)
            return false;
        cfg->m_nameEdit->setText("Collision Block Group");
        return true;
    });
    FixtureGroup *blockGrp = CreateFixtureGroup::createFromFixtures(
        m_doc, QList<quint32>() << blockFxA->id() << blockFxB->id(), nullptr);
    QVERIFY2(blockGrp != nullptr, "createFromFixtures() returned no group");
    QCOMPARE(blockGrp->headsMap().value(QLCPoint(0, 0)).fxi, blockFxA->id());
    QCOMPARE(blockGrp->headsMap().value(QLCPoint(1, 0)).fxi, blockFxB->id());

    // 4. Open the target group's real layout editor by selecting it, same
    //    path selectingAGroupOpensItsLayoutEditor() proves opens it.
    FixtureManager mgr(nullptr, m_doc);
    mgr.resize(1100, 700);
    mgr.m_splitter->setSizes(QList<int>() << 700 << 400);

    QTreeWidgetItem *targetRow = nullptr;
    {
        // Scoped: a QTreeWidgetItemIterator registers itself with the tree
        // model for the whole of its lifetime, not just while it's actively
        // stepping -- an idle one left alive past this point stays
        // registered through the group-changed tree rebuild below (this
        // test's whole point: pasting onto the occupant's cell removes an
        // existing tree item, not just adds new ones). An unscoped version
        // of exactly this loop segfaulted inside Qt's own
        // QTreeWidgetItemIteratorPrivate::ensureValidIterator() -- but only
        // under a Release build (undefined behaviour Debug happened not to
        // manifest; see check-all.sh's own comment on exactly this class of
        // Release-only bug, and CLAUDE.md's note on why the Release leg
        // isn't redundant). Block-scoping it so it's destroyed the moment
        // the lookup is done avoids the whole class of hazard -- the same
        // rule targetRow's own "dies on the deferred rebuild" comment below
        // already applies to a raw pointer into the tree, just for the
        // iterator instead.
        QTreeWidgetItemIterator it(mgr.m_fixtures_tree);
        while (*it != nullptr)
        {
            const QVariant g = (*it)->data(KColumnName, PROP_GROUP);
            if (g.isValid() && g.toUInt() == targetGrp->id()) { targetRow = *it; break; }
            ++it;
        }
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

    // 5. Click the OCCUPIED cell (1,0) -- unlike the sibling workflow test,
    //    which deliberately anchors on an empty cell to sidestep exactly
    //    this question, this test anchors ON the occupant on purpose.
    QVERIFY2(editor->m_table != nullptr, "the layout editor has no table");
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

    // 6. Paste the block in, anchored right on top of the occupant --
    //    FixtureGroupEditor::slotAddGroupBlock(), the exact handler the
    //    real "Add group as block..." button calls.
    driveDialog([](QWidget *modal) {
        QComboBox *combo = modal->findChild<QComboBox *>();
        if (combo == nullptr)
            return false;
        combo->setCurrentText("Collision Block Group");
        return true;
    });
    editor->slotAddGroupBlock();

    // slotAddGroupBlock()'s paste loop only skips a source head that's
    // ALREADY a member of the target group somewhere
    // (`existing.contains(gh)`) -- it never checks whether the DESTINATION
    // cell is already occupied by something else. FixtureGroup::assignHead()
    // then writes `m_heads[pt] = head` unconditionally for an explicit
    // point, a plain QMap assignment that silently overwrites whatever was
    // there. So: pasting a block onto an occupied cell overwrites the
    // occupant with no warning and no refusal (WORKFLOW-SUBTLETIES.md #13).
    // If a future change makes assignHead() refuse or relocate on collision
    // instead, this test -- and that entry -- should be updated to match.
    const QMap<QLCPoint, GroupHead> afterPaste = targetGrp->headsMap();
    QVERIFY2(afterPaste.value(QLCPoint(1, 0)).fxi == blockFxA->id(),
             "the paste did not land block member A on the (occupied) anchor cell");
    QCOMPARE(targetGrp->headSubGroup(QLCPoint(1, 0)), blockGrp->id());
    QCOMPARE(afterPaste.value(QLCPoint(2, 0)).fxi, blockFxB->id());
    QCOMPARE(afterPaste.value(QLCPoint(0, 0)).fxi, targetFx->id());   // untouched

    bool occupantStillPresent = false;
    foreach (const GroupHead &gh, targetGrp->headsMap())
        if (gh.fxi == occupantFx->id())
            occupantStillPresent = true;
    QVERIFY2(occupantStillPresent == false,
             "the occupant that was at the paste's anchor cell survived the "
             "paste somewhere else in the group -- assignHead() should have "
             "overwritten it in place with no trace left behind");
}

void FixtureManager_Test::draggingASubGroupBlockMovesItThroughTheRealDropPath()
{
    // 1. Fixtures + target/source groups, paste the block in -- identical
    //    setup to populatingAndMovingAFixtureGroupBlockThroughRealUi()'s own
    //    steps 1-6 (see that test for the reasoning behind each one); what's
    //    under test here is what happens AFTER the paste: moving the pasted
    //    block by dragging it, through the real internal QDrag/QDropEvent
    //    code path, instead of that test's own moveSubGroup() call.
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

    driveDialog([](QWidget *modal) {
        CreateFixtureGroup *cfg = qobject_cast<CreateFixtureGroup *>(modal);
        if (cfg == nullptr)
            return false;
        cfg->m_nameEdit->setText("Drag Target Group");
        return true;
    });
    FixtureGroup *targetGrp = CreateFixtureGroup::createFromFixtures(
        m_doc, QList<quint32>() << targetFx->id(), nullptr);
    QVERIFY2(targetGrp != nullptr, "createFromFixtures() returned no group");
    targetGrp->setSize(QSize(3, 2));

    driveDialog([](QWidget *modal) {
        CreateFixtureGroup *cfg = qobject_cast<CreateFixtureGroup *>(modal);
        if (cfg == nullptr)
            return false;
        cfg->m_nameEdit->setText("Drag Block Group");
        return true;
    });
    FixtureGroup *blockGrp = CreateFixtureGroup::createFromFixtures(
        m_doc, QList<quint32>() << blockFxA->id() << blockFxB->id(), nullptr);
    QVERIFY2(blockGrp != nullptr, "createFromFixtures() returned no group");

    FixtureManager mgr(nullptr, m_doc);
    mgr.resize(1100, 700);
    mgr.m_splitter->setSizes(QList<int>() << 700 << 400);

    QTreeWidgetItem *targetRow = nullptr;
    {
        // Scoped so the iterator is destroyed (and unregistered from the
        // tree model) the moment the lookup is done, rather than sitting
        // idle-but-still-registered through the group-changed rebuild
        // later in this test -- see
        // pastingABlockOntoAnOccupiedCellOverwritesTheOccupant()'s own
        // fuller comment on why a lingering QTreeWidgetItemIterator (not
        // just a lingering raw QTreeWidgetItem* like targetRow itself)
        // is a real hazard here, not a theoretical one.
        QTreeWidgetItemIterator it(mgr.m_fixtures_tree);
        while (*it != nullptr)
        {
            const QVariant g = (*it)->data(KColumnName, PROP_GROUP);
            if (g.isValid() && g.toUInt() == targetGrp->id()) { targetRow = *it; break; }
            ++it;
        }
    }
    QVERIFY2(targetRow != nullptr, "the target group never got a row carrying its id");
    mgr.m_fixtures_tree->clearSelection();
    targetRow->setSelected(true);
    targetRow = nullptr;
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY2(mgr.m_groupEditor != nullptr,
             "selecting the target group did not open its layout editor");

    FixtureGroupEditor *editor = mgr.m_groupEditor;
    QCoreApplication::processEvents();

    QVERIFY2(editor->m_table != nullptr, "the layout editor has no table");
    const QPoint anchorCellPx(editor->m_table->columnViewportPosition(1) + editor->m_table->columnWidth(1) / 2,
                              editor->m_table->rowViewportPosition(0) + editor->m_table->rowHeight(0) / 2);
    QMouseEvent press(QEvent::MouseButtonPress, anchorCellPx, anchorCellPx,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    editor->eventFilter(editor->m_table->viewport(), &press);
    QMouseEvent release(QEvent::MouseButtonRelease, anchorCellPx, anchorCellPx,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    editor->eventFilter(editor->m_table->viewport(), &release);

    driveDialog([](QWidget *modal) {
        QComboBox *combo = modal->findChild<QComboBox *>();
        if (combo == nullptr)
            return false;
        combo->setCurrentText("Drag Block Group");
        return true;
    });
    editor->slotAddGroupBlock();

    QCOMPARE(targetGrp->headsMap().value(QLCPoint(1, 0)).fxi, blockFxA->id());
    QCOMPARE(targetGrp->headsMap().value(QLCPoint(2, 0)).fxi, blockFxB->id());

    // 2. Reproduce what a real press-and-drag-past-threshold on the block
    //    would have captured in startCellDrag() (m_dragCells: the occupied
    //    selected cells; m_dragAnchor: the cell the drag started on) --
    //    setting them directly here, rather than actually crossing the drag
    //    threshold and calling QDrag::exec(), is the whole point: it lets
    //    the REST of the path -- the real eventFilter() Drop branch below --
    //    run unfaked.
    editor->m_dragCells = QList<QLCPoint>() << QLCPoint(1, 0) << QLCPoint(2, 0);
    editor->m_dragAnchor = QLCPoint(1, 0);

    // The mime type is FixtureGroupEditor's own private CELLS_DRAG_MIME
    // constant (not exposed in the header); startCellDrag() sets no payload
    // bytes on it either -- the Drop handler below reads move geometry from
    // the drop position and m_dragCells/m_dragAnchor, never from the
    // QMimeData's own bytes, so an empty payload under the right format
    // string is exactly what a real internal drag would carry too.
    static const char *kCellsDragMime = "application/x-qlc-groupcells";

    // 3. Drop somewhere that COLLIDES with the target's own untouched member
    //    at (0,0): moving the block by (-1,0) would land member A exactly on
    //    top of it. canMoveHeads()'s guard must refuse this, and nothing
    //    should move -- proving the guard is actually wired into this path,
    //    not just moveHeads()'s direct callers.
    {
        const QPoint dropPx(editor->m_table->columnViewportPosition(0) + editor->m_table->columnWidth(0) / 2,
                            editor->m_table->rowViewportPosition(0) + editor->m_table->rowHeight(0) / 2);
        QMimeData mime;
        mime.setData(kCellsDragMime, QByteArray());
        QDropEvent drop(QPointF(dropPx), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QVERIFY2(editor->eventFilter(editor->m_table->viewport(), &drop),
                 "the Drop branch did not claim a CELLS_DRAG_MIME drop");
        QTest::qWait(30);   // moveHeads() runs off a deferred QTimer::singleShot(0, ...)

        QCOMPARE(targetGrp->headsMap().value(QLCPoint(1, 0)).fxi, blockFxA->id());
        QCOMPARE(targetGrp->headsMap().value(QLCPoint(2, 0)).fxi, blockFxB->id());
        QCOMPARE(targetGrp->headsMap().value(QLCPoint(0, 0)).fxi, targetFx->id());
    }

    // 4. Drop somewhere that fits: one row down, (0,+1). This is the actual
    //    proof the path works end to end, not just that it refuses.
    {
        const QPoint dropPx(editor->m_table->columnViewportPosition(1) + editor->m_table->columnWidth(1) / 2,
                            editor->m_table->rowViewportPosition(1) + editor->m_table->rowHeight(1) / 2);
        QMimeData mime;
        mime.setData(kCellsDragMime, QByteArray());
        QDropEvent drop(QPointF(dropPx), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QVERIFY2(editor->eventFilter(editor->m_table->viewport(), &drop),
                 "the Drop branch did not claim a CELLS_DRAG_MIME drop");
        QTest::qWait(30);

        const QMap<QLCPoint, GroupHead> afterDrag = targetGrp->headsMap();
        QVERIFY2(afterDrag.value(QLCPoint(1, 1)).fxi == blockFxA->id(),
                 "dragging the block by (0,+1) did not carry member A to (1,1)");
        QVERIFY2(afterDrag.value(QLCPoint(2, 1)).fxi == blockFxB->id(),
                 "dragging the block by (0,+1) did not carry member B to (2,1)");
        QVERIFY2(afterDrag.contains(QLCPoint(1, 0)) == false,
                 "the block's old (1,0) cell is still occupied after dragging away from it");
        QVERIFY2(afterDrag.contains(QLCPoint(2, 0)) == false,
                 "the block's old (2,0) cell is still occupied after dragging away from it");
        QCOMPARE(targetGrp->headSubGroup(QLCPoint(1, 1)), blockGrp->id());
        QCOMPARE(targetGrp->headSubGroup(QLCPoint(2, 1)), blockGrp->id());
        QCOMPARE(afterDrag.value(QLCPoint(0, 0)).fxi, targetFx->id());   // untouched by the drag
    }
}

QTEST_MAIN(FixtureManager_Test)
