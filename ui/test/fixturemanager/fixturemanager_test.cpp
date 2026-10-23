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
#include <QMenu>
#include <memory>
#include <QTimer>
#include <QMessageBox>
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
#include <QTextBrowser>
#include <QAction>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "showembedding.h"

#include <QShortcut>
#include <QCheckBox>
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
#include "patchinventorydialog.h"
#include "addfixture.h"
#include "creategroupsdialog.h"
#include "fixturegroupsource.h"
#include "outputrulesdialog.h"
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
    QCOMPARE(targetGrp->headSubGroup(QLCPoint(1, 0)), FixtureGroup::subGroupTagFor(blockGrp->id()));
    QCOMPARE(targetGrp->headSubGroup(QLCPoint(2, 0)), FixtureGroup::subGroupTagFor(blockGrp->id()));

    // 7. Move the block as a unit. Real alternate UI path for the same
    //    result as a drag: the right-click "Move sub-group" action reaches
    //    exactly this function.
    editor->moveSubGroup(FixtureGroup::subGroupTagFor(blockGrp->id()), 0, 1);

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
    QCOMPARE(targetGrp->headSubGroup(QLCPoint(1, 1)), FixtureGroup::subGroupTagFor(blockGrp->id()));
    QCOMPARE(targetGrp->headSubGroup(QLCPoint(2, 1)), FixtureGroup::subGroupTagFor(blockGrp->id()));
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
    QCOMPARE(targetGrp->headSubGroup(QLCPoint(1, 0)), FixtureGroup::subGroupTagFor(blockGrp->id()));
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
        QCOMPARE(targetGrp->headSubGroup(QLCPoint(1, 1)), FixtureGroup::subGroupTagFor(blockGrp->id()));
        QCOMPARE(targetGrp->headSubGroup(QLCPoint(2, 1)), FixtureGroup::subGroupTagFor(blockGrp->id()));
        QCOMPARE(afterDrag.value(QLCPoint(0, 0)).fxi, targetFx->id());   // untouched by the drag
    }
}


/* Right-click @p item and return the texts of the menu it pops up. The menu's
 * exec() blocks, so a timer reads it out and closes it. */
static QStringList contextMenuTexts(FixtureManager &mgr, QTreeWidgetItem *item)
{
    QStringList texts;
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [&texts, poll]() {
        QMenu *m = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (m == nullptr)
        {
            QTimer::singleShot(10, *poll);
            return;
        }
        foreach (QAction *a, m->actions())
            if (!a->isSeparator())
                texts << a->text();
        m->close();
    };
    QTimer::singleShot(0, *poll);
    mgr.m_fixtures_tree->setCurrentItem(item);
    mgr.slotContextMenuRequested(mgr.m_fixtures_tree->visualItemRect(item).center());
    return texts;
}

static QTreeWidgetItem *rowWithRole(FixtureManager &mgr, int role)
{
    for (QTreeWidgetItemIterator it(mgr.m_fixtures_tree); *it != nullptr; ++it)
        if ((*it)->data(KColumnName, role).isValid())
            return *it;
    return nullptr;
}

void FixtureManager_Test::rightClickOnInventoryOffersInventoryActionsNotAGenericAdd()
{
    FixtureManager mgr(nullptr, m_doc);
    QTreeWidgetItem *inv = rowWithRole(mgr, PROP_INVENTORY);
    QVERIFY2(inv != nullptr, "no Inventory root in the tree");

    const QStringList texts = contextMenuTexts(mgr, inv);
    QVERIFY2(!texts.isEmpty(), "the context menu never appeared");
    QVERIFY2(texts.contains("Add to Inventory…"), qPrintable(texts.join(" | ")));
    QVERIFY2(texts.contains("Patch Inventory…"), qPrintable(texts.join(" | ")));
    QVERIFY2(!texts.contains("Add fixture..."),
             qPrintable("the generic Add fixture offer is still there: " + texts.join(" | ")));
    QVERIFY2(!texts.contains("Add RGB panel..."), qPrintable(texts.join(" | ")));
}

void FixtureManager_Test::rightClickOnUniversesStillOffersAFixtureAdd()
{
    FixtureManager mgr(nullptr, m_doc);
    // The Universes root is the top-level row that is not one of the others.
    QTreeWidgetItem *uni = nullptr;
    for (QTreeWidgetItemIterator it(mgr.m_fixtures_tree); *it != nullptr; ++it)
        if ((*it)->parent() == nullptr && (*it)->text(KColumnName) == "Universes")
            uni = *it;
    QVERIFY(uni != nullptr);

    const QStringList texts = contextMenuTexts(mgr, uni);
    QVERIFY2(texts.contains("Add fixture..."), qPrintable(texts.join(" | ")));
    QVERIFY2(!texts.contains("Add to Inventory…"), qPrintable(texts.join(" | ")));
}

void FixtureManager_Test::addFixtureWithInventorySelectedStagesIt()
{
    FixtureManager mgr(nullptr, m_doc);
    QTreeWidgetItem *inv = rowWithRole(mgr, PROP_INVENTORY);
    QVERIFY(inv != nullptr);
    mgr.m_fixtures_tree->setCurrentItem(inv);

    // Accept whatever dialog opens, and record which kind it was.
    bool sawInventoryDialog = false;
    QTimer::singleShot(0, [&sawInventoryDialog]() {
        QWidget *modal = QApplication::activeModalWidget();
        if (modal == nullptr)
            return;
        sawInventoryDialog = modal->windowTitle().contains("Inventory");
        if (QDialogButtonBox *b = modal->findChild<QDialogButtonBox *>())
            if (QPushButton *ok = b->button(QDialogButtonBox::Ok))
            {
                ok->click();
                return;
            }
        modal->close();
    });
    mgr.addFixture();

    QVERIFY2(sawInventoryDialog, "the Add dialog was not in inventory mode");
    QCOMPARE(m_doc->fixturesCount(), 1);
    QVERIFY2(!m_doc->fixtures().first()->isPatched(), "the fixture was patched instead of staged");
}

static QTreeWidgetItem *fixtureRow(FixtureManager &mgr, quint32 fid)
{
    for (QTreeWidgetItemIterator it(mgr.m_fixtures_tree); *it != nullptr; ++it)
        if ((*it)->data(KColumnName, PROP_ID).isValid()
            && (*it)->data(KColumnName, PROP_ID).toString().toUInt() == fid
            && !(*it)->data(KColumnName, PROP_HEAD).isValid())
            return *it;
    return nullptr;
}

void FixtureManager_Test::returnToInventoryIsOfferedForPatchedFixturesOnly()
{
    Fixture *real = new Fixture(m_doc);
    real->setName("Real");
    real->setChannels(4);
    QVERIFY(m_doc->addFixture(real));
    Fixture *staged = new Fixture(m_doc);
    staged->setName("Staged");
    staged->setChannels(4);
    staged->setPatched(false);
    QVERIFY(m_doc->addFixture(staged));

    FixtureManager mgr(nullptr, m_doc);
    QTreeWidgetItem *realRow = fixtureRow(mgr, real->id());
    QTreeWidgetItem *stagedRow = fixtureRow(mgr, staged->id());
    QVERIFY(realRow != nullptr && stagedRow != nullptr);

    QVERIFY(contextMenuTexts(mgr, realRow).contains("Return to Inventory…"));
    const QStringList st = contextMenuTexts(mgr, stagedRow);
    QVERIFY2(!st.contains("Return to Inventory…"), qPrintable(st.join(" | ")));
    QVERIFY2(st.contains("Patch…"), qPrintable(st.join(" | ")));

    /* "we should clean up that menu to organize it": the fixture, its
       address, finding it, organising it (Group and Power are submenus),
       deleting it -- in that order -- and Patch one name. */
    const QStringList rt = contextMenuTexts(mgr, realRow);
    const QStringList order = { "Fixture Properties…", "Patch…", "Return to Inventory…",
                                "Test Fixture…", "Group", "Power", "Delete…" };
    int last = -1;
    foreach (const QString &t, order)
    {
        const int at = rt.indexOf(t);
        QVERIFY2(at > last, qPrintable(QString("%1 out of place: %2").arg(t, rt.join(" | "))));
        last = at;
    }
    const QStringList gone = { "Copy into group", "Add fixture to group...", "Re-address...",
                               "Remove fixture from group", "Create group from selection…" };
    for (const QString &g : gone)
        QVERIFY2(!rt.contains(g), qPrintable(QString("%1 still at the top: %2").arg(g, rt.join(" | "))));
}

void FixtureManager_Test::returningToInventoryReleasesTheAddress()
{
    Fixture *real = new Fixture(m_doc);
    real->setName("Real");
    real->setChannels(4);
    QVERIFY(m_doc->addFixture(real));
    QCOMPARE(m_doc->fixtureForAddress(0), real->id());

    FixtureManager mgr(nullptr, m_doc);
    QTimer::singleShot(0, []() {
        if (QMessageBox *mb = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
            mb->button(QMessageBox::Yes)->click();
    });
    mgr.returnToInventory(QList<quint32>() << real->id());

    QVERIFY(!real->isPatched());
    QCOMPARE(m_doc->fixtureForAddress(0), Fixture::invalidId());
    QCOMPARE(m_doc->fixturesCount(), 1);              // still in the show
    QCOMPARE(real->name(), QString("Real"));
}

void FixtureManager_Test::rightClickOnAnInventoryFolderOffersToPatchWhatIsInIt()
{
    for (int i = 0; i < 3; i++)
    {
        Fixture *f = new Fixture(m_doc);
        f->setName(QString("Dimmer %1").arg(i));
        f->setChannels(4);
        f->setPatched(false);
        QVERIFY(m_doc->addFixture(f));
    }

    FixtureManager mgr(nullptr, m_doc);
    QTreeWidgetItem *inv = rowWithRole(mgr, PROP_INVENTORY);
    QVERIFY(inv != nullptr && inv->childCount() == 1);
    QTreeWidgetItem *manuf = inv->child(0);                      // "Generic (3)"
    QCOMPARE(manuf->data(KColumnName, PROP_INVENTORY).toInt(), 2);

    const QStringList texts = contextMenuTexts(mgr, manuf);
    QVERIFY2(texts.contains("Patch 3 Fixtures…"), qPrintable(texts.join(" | ")));
    // A manufacturer's folder adds that manufacturer's fixtures.
    QVERIFY2(texts.contains("Add Generic Fixtures to Inventory…"), qPrintable(texts.join(" | ")));
    QVERIFY2(!texts.contains("Add fixture..."), qPrintable(texts.join(" | ")));

    // A model's folder adds more of that model; the root, anything.
    QTreeWidgetItem *model = manuf->child(0);                    // "Dimmer, 4 ch (3)"
    QCOMPARE(model->data(KColumnName, PROP_INVENTORY).toInt(), 2);
    const QStringList modelTexts = contextMenuTexts(mgr, model);
    QVERIFY2(modelTexts.contains("Add More Dimmer, 4 ch to Inventory…"), qPrintable(modelTexts.join(" | ")));
    const QStringList rootTexts = contextMenuTexts(mgr, inv);
    QVERIFY2(rootTexts.contains("Add to Inventory…"), qPrintable(rootTexts.join(" | ")));
}

namespace
{
/* Call @p act with the next modal widget of type T, polling until it is up.
 * @p act must close it. */
template <class T>
void whenModal(const std::function<void(T *)> &act)
{
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [act, poll]() {
        T *w = qobject_cast<T *>(QApplication::activeModalWidget());
        if (w == nullptr)
        {
            QTimer::singleShot(10, *poll);
            return;
        }
        act(w);
    };
    QTimer::singleShot(0, *poll);
}

/* Right-click @p item and pick the action titled @p text. False if the menu
 * never offered it. */
bool chooseFromContextMenu(FixtureManager &mgr, QTreeWidgetItem *item, const QString &text)
{
    bool found = false;
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [&found, text, poll]() {
        QMenu *m = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (m == nullptr)
        {
            QTimer::singleShot(10, *poll);
            return;
        }
        foreach (QAction *a, m->actions())
            if (a->text() == text)
            {
                found = true;
                m->setActiveAction(a);
                QTest::keyClick(m, Qt::Key_Return);   // exec() only reports a real pick
                return;
            }
        m->close();
    };
    QTimer::singleShot(0, *poll);
    mgr.m_fixtures_tree->clearSelection();
    mgr.m_fixtures_tree->setCurrentItem(item);
    item->setSelected(true);
    mgr.slotContextMenuRequested(mgr.m_fixtures_tree->visualItemRect(item).center());
    return found;
}

Fixture *stagedDimmer(Doc *doc, const QString &name)
{
    Fixture *f = new Fixture(doc);
    f->setName(name);
    f->setChannels(4);
    f->setPatched(false);
    if (!doc->addFixture(f))
        return nullptr;
    return f;
}
} // namespace

void FixtureManager_Test::choosingReturnToInventoryFromTheMenuReleasesIt()
{
    Fixture *real = new Fixture(m_doc);
    real->setName("Real");
    real->setChannels(4);
    QVERIFY(m_doc->addFixture(real));

    FixtureManager mgr(nullptr, m_doc);
    whenModal<QMessageBox>([](QMessageBox *mb) { mb->button(QMessageBox::Yes)->click(); });
    QVERIFY(chooseFromContextMenu(mgr, fixtureRow(mgr, real->id()), "Return to Inventory…"));
    QVERIFY(!real->isPatched());
    // ...and the tree now files it under the Inventory.
    QTreeWidgetItem *row = fixtureRow(mgr, real->id());
    QVERIFY(row != nullptr);
    QTreeWidgetItem *top = row;
    while (top->parent() != nullptr)
        top = top->parent();
    QVERIFY(top->data(KColumnName, PROP_INVENTORY).isValid());
}

void FixtureManager_Test::choosingAddToInventoryFromTheMenuStagesAFixture()
{
    FixtureManager mgr(nullptr, m_doc);
    QTreeWidgetItem *inv = rowWithRole(mgr, PROP_INVENTORY);
    QVERIFY(inv != nullptr);
    whenModal<AddFixture>([](AddFixture *dlg) {
        QVERIFY(dlg->inventoryMode());
        dlg->m_buttonBox->button(QDialogButtonBox::Ok)->click();
    });
    QVERIFY(chooseFromContextMenu(mgr, inv, "Add to Inventory…"));
    QCOMPARE(m_doc->fixturesCount(), 1);
    QVERIFY(!m_doc->fixtures().first()->isPatched());
}

void FixtureManager_Test::choosingPatchFromTheMenuPatchesThoseFixtures()
{
    Fixture *a = stagedDimmer(m_doc, "A");
    Fixture *b = stagedDimmer(m_doc, "B");
    QVERIFY(a && b);

    // "Patch…": just that one.
    {
        FixtureManager mgr(nullptr, m_doc);
        int rows = -1;
        whenModal<PatchInventoryDialog>([&rows](PatchInventoryDialog *dlg) {
            rows = dlg->m_table->rowCount();
            dlg->accept();
        });
        QVERIFY(chooseFromContextMenu(mgr, fixtureRow(mgr, a->id()), "Patch…"));
        QCOMPARE(rows, 1);
        QVERIFY(a->isPatched());
        QVERIFY(!b->isPatched());
    }
    // "Patch Inventory…": everything still staged.
    {
        Fixture *c = stagedDimmer(m_doc, "C");
        FixtureManager mgr(nullptr, m_doc);
        int rows = -1;
        whenModal<PatchInventoryDialog>([&rows](PatchInventoryDialog *dlg) {
            rows = dlg->m_table->rowCount();
            dlg->reject();
        });
        QVERIFY(chooseFromContextMenu(mgr, rowWithRole(mgr, PROP_INVENTORY), "Patch Inventory…"));
        QCOMPARE(rows, 2);                               // B and C
        QVERIFY(!b->isPatched() && !c->isPatched());     // cancelled
    }
}

void FixtureManager_Test::aNetworkFixturesInfoStatesItsConnection()
{
    Fixture *withIp = stagedDimmer(m_doc, "Fence 1");
    withIp->setConnection(Fixture::Network);
    withIp->setIPAddress("172.18.2.41");
    Fixture *noIp = stagedDimmer(m_doc, "Fence 2");
    noIp->setConnection(Fixture::Network);

    FixtureManager mgr(nullptr, m_doc);
    mgr.m_fixtures_tree->setCurrentItem(fixtureRow(mgr, withIp->id()));
    QVERIFY2(mgr.m_info->toPlainText().contains("Network, unicast to 172.18.2.41"),
             qPrintable(mgr.m_info->toPlainText()));
    mgr.m_fixtures_tree->setCurrentItem(fixtureRow(mgr, noIp->id()));
    QVERIFY2(mgr.m_info->toPlainText().contains("Network (no IP set)"), qPrintable(mgr.m_info->toPlainText()));
}

void FixtureManager_Test::theInventoryActionsFollowTheAddAction()
{
    FixtureManager mgr(nullptr, m_doc);
    mgr.m_addAction->setEnabled(false);
    QVERIFY(!mgr.m_addInventoryAction->isEnabled());
    QVERIFY(!mgr.m_patchInventoryAction->isEnabled());
    mgr.m_addAction->setEnabled(true);
    QVERIFY(mgr.m_addInventoryAction->isEnabled());
    QVERIFY(mgr.m_patchInventoryAction->isEnabled());
}

void FixtureManager_Test::editingAStagedFixtureKeepsItStaged()
{
    Fixture *f = stagedDimmer(m_doc, "Fence 1");
    FixtureManager mgr(nullptr, m_doc);
    mgr.m_fixtures_tree->setCurrentItem(fixtureRow(mgr, f->id()));

    QString title;
    whenModal<AddFixture>([&title](AddFixture *dlg) {
        title = dlg->windowTitle();
        dlg->m_connectionCombo->setCurrentIndex(dlg->m_connectionCombo->findData(
            AddFixture::connectionKey(Fixture::Network, Fixture::ArtNetTransport)));
        dlg->m_ipEdit->setText("172.18.2.41");
        // The universe/address fields are hidden placeholders; even if they
        // moved, an inventory fixture must not turn them into a claim.
        dlg->m_universeValue = 2;
        dlg->m_addressValue = 40;
        dlg->m_buttonBox->button(QDialogButtonBox::Ok)->click();
    });
    mgr.editFixtureProperties();

    QCOMPARE(title, QString("Change inventory fixture"));
    QVERIFY(!f->isPatched());
    QCOMPARE(f->connection(), Fixture::Network);
    QCOMPARE(f->ipAddress(), QString("172.18.2.41"));
    QCOMPARE(f->universe(), quint32(0));
    QCOMPARE(f->address(), quint32(0));
    QCOMPARE(m_doc->fixtureForAddress((2 << 9) | 40), Fixture::invalidId());
}

void FixtureManager_Test::deletingInventoryFixturesUpdatesTheFolderCounts()
{
    // Two models under one manufacturer, as in the Betopper/LM70 report.
    QList<Fixture *> lm70, other;
    for (int i = 0; i < 3; i++)
        lm70 << stagedDimmer(m_doc, QString("LM70 %1").arg(i + 1));
    Fixture *wide = new Fixture(m_doc);
    wide->setName("Wide 1");
    wide->setChannels(6);                              // "Dimmer, 6 ch": its own model folder
    wide->setPatched(false);
    QVERIFY(m_doc->addFixture(wide));
    // One of them is also in a fixture group, so it has two rows.
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Wash");
    grp->setSize(QSize(4, 1));
    QVERIFY(m_doc->addFixtureGroup(grp));
    grp->assignFixture(lm70.at(0)->id());

    FixtureManager mgr(nullptr, m_doc);
    // Looked up afresh each time: a delete that touches a group rebuilds the tree.
    auto inv = [&mgr]() { return rowWithRole(mgr, PROP_INVENTORY); };
    QCOMPARE(inv()->text(KColumnName), QString("Inventory (4)"));
    QCOMPARE(inv()->child(0)->text(KColumnName), QString("Generic (4)"));

    QVERIFY(m_doc->deleteFixture(lm70.at(0)->id()));
    QCOMPARE(fixtureRow(mgr, lm70.at(0)->id()), nullptr);   // every row of it, group one included
    QCOMPARE(inv()->text(KColumnName), QString("Inventory (3)"));
    QCOMPARE(inv()->child(0)->text(KColumnName), QString("Generic (3)"));
    QCOMPARE(inv()->child(0)->child(0)->text(KColumnName), QString("Dimmer, 4 ch (2)"));

    // Emptying a model folder drops it...
    QVERIFY(m_doc->deleteFixture(lm70.at(1)->id()));
    QVERIFY(m_doc->deleteFixture(lm70.at(2)->id()));
    QCOMPARE(inv()->child(0)->childCount(), 1);
    QCOMPARE(inv()->child(0)->child(0)->text(KColumnName), QString("Dimmer, 6 ch (1)"));
    QCOMPARE(inv()->text(KColumnName), QString("Inventory (1)"));

    // ...and the manufacturer with it; the root stays, uncounted.
    QVERIFY(m_doc->deleteFixture(wide->id()));
    QCOMPARE(inv()->childCount(), 0);
    QCOMPARE(inv()->text(KColumnName), QString("Inventory"));
}

namespace
{
/* @p count dimmers of @p channels channels, patched one after another. */
QList<quint32> addDimmers(Doc *doc, int count, int channels, const QString &name)
{
    static quint32 nextAddress = 0;
    QList<quint32> ids;
    for (int i = 0; i < count; i++)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("%1 %2").arg(name).arg(i + 1));
        f->setChannels(quint32(channels));
        f->setAddress(nextAddress % 400);
        nextAddress += quint32(channels);
        if (!doc->addFixture(f))
            return QList<quint32>();
        ids << f->id();
    }
    return ids;
}

FixtureGroup *addGroup(Doc *doc, const QString &name, const QString &path,
                       const QList<quint32> &fids)
{
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName(name);
    g->setPath(path);
    g->setSize(QSize(qMax(1, int(fids.size())), 1));
    doc->addFixtureGroup(g);
    foreach (quint32 fid, fids)
        g->assignFixture(fid);
    return g;
}

QTreeWidgetItem *groupRow(FixtureManager &mgr, quint32 gid)
{
    for (QTreeWidgetItemIterator it(mgr.m_fixtures_tree); *it != nullptr; ++it)
        if ((*it)->data(KColumnName, PROP_GROUP).isValid()
            && (*it)->data(KColumnName, PROP_GROUP).toUInt() == gid)
            return *it;
    return nullptr;
}

QSet<quint32> selectedGroups(FixtureManager &mgr)
{
    QSet<quint32> out;
    foreach (QTreeWidgetItem *it, mgr.m_fixtures_tree->selectedItems())
        if (it->data(KColumnName, PROP_GROUP).isValid())
            out.insert(it->data(KColumnName, PROP_GROUP).toUInt());
    return out;
}

QList<quint32> selectedFixtureRows(FixtureManager &mgr, QList<QTreeWidgetItem *> *rows = nullptr)
{
    QList<quint32> out;
    foreach (QTreeWidgetItem *it, mgr.m_fixtures_tree->selectedItems())
        if (it->data(KColumnName, PROP_ID).isValid() && !it->data(KColumnName, PROP_HEAD).isValid())
        {
            out << it->data(KColumnName, PROP_ID).toString().toUInt();
            if (rows)
                *rows << it;
        }
    return out;
}

/* The texts of the right-click menu, submenus as "Sub ▸ Item". */
QStringList menuTree(QMenu *m, const QString &prefix = QString())
{
    QStringList out;
    foreach (QAction *a, m->actions())
    {
        if (a->isSeparator())
            continue;
        out << prefix + a->text();
        if (a->menu() != nullptr)
            out << menuTree(a->menu(), prefix + a->text() + QStringLiteral(" ▸ "));
    }
    return out;
}

QStringList contextMenuTree(FixtureManager &mgr)
{
    QStringList texts;
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [&texts, poll]() {
        QMenu *m = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (m == nullptr)
        {
            QTimer::singleShot(10, *poll);
            return;
        }
        texts = menuTree(m);
        m->close();
    };
    QTimer::singleShot(0, *poll);
    QTreeWidgetItem *cur = mgr.m_fixtures_tree->selectedItems().value(0);
    mgr.slotContextMenuRequested(cur ? mgr.m_fixtures_tree->visualItemRect(cur).center() : QPoint());
    return texts;
}

void selectOnly(QTreeWidget *tree, const QList<QTreeWidgetItem *> &rows)
{
    const bool was = tree->blockSignals(true);
    tree->clearSelection();
    foreach (QTreeWidgetItem *r, rows)
        r->setSelected(true);
    if (!rows.isEmpty())
        tree->setCurrentItem(rows.first(), 0, QItemSelectionModel::NoUpdate);
    tree->blockSignals(was);
}
} // namespace

void FixtureManager_Test::similarFixturesAndGroupsFollowTheirRules()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    const QList<quint32> four = addDimmers(m_doc, 6, 4, "Four");
    const QList<quint32> six = addDimmers(m_doc, 3, 6, "Six");
    QCOMPARE(four.size(), 6);

    QList<quint32> pool = four + six;
    QCOMPARE(FixtureManager::similarFixtures(m_doc, { four.first() }, pool), four);
    QCOMPARE(FixtureManager::similarFixtures(m_doc, { six.last() }, pool), six);
    QCOMPARE(FixtureManager::similarFixtures(m_doc, { six.first() }, four), QList<quint32>());

    // Same make-up, same folder: alike. Another folder, or another make-up: not.
    FixtureGroup *a = addGroup(m_doc, "A", "Truss", { four[0], four[1] });
    FixtureGroup *b = addGroup(m_doc, "B", "Truss", { four[2], four[3] });
    FixtureGroup *c = addGroup(m_doc, "C", "Floor", { four[4], four[5] });
    FixtureGroup *d = addGroup(m_doc, "D", "Truss", { six[0], six[1] });
    FixtureGroup *e = addGroup(m_doc, "E", "Truss", { four[0], four[1], four[2] });
    QCOMPARE(FixtureManager::similarGroups(m_doc, { a->id() }), QList<quint32>({ a->id(), b->id() }));
    QCOMPARE(FixtureManager::similarGroups(m_doc, { c->id() }), QList<quint32>({ c->id() }));
    QCOMPARE(FixtureManager::similarGroups(m_doc, { d->id() }), QList<quint32>({ d->id() }));
    QCOMPARE(FixtureManager::similarGroups(m_doc, { e->id() }), QList<quint32>({ e->id() }));

    // Folders, parents included; the common one.
    c->setPath("Floor/Left");
    QCOMPARE(FixtureManager::groupFolders(m_doc), QStringList({ "Floor", "Floor/Left", "Truss" }));
    QCOMPARE(FixtureManager::commonGroupFolder(m_doc, { a->id(), b->id() }), QString("Truss"));
    QCOMPARE(FixtureManager::commonGroupFolder(m_doc, { a->id(), c->id() }), QString());
    QCOMPARE(FixtureManager::cleanFolderPath(" Movers / Front /"), QString("Movers/Front"));
}

void FixtureManager_Test::selectAllSimilarSelectsTheRowsAlike()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    const QList<quint32> four = addDimmers(m_doc, 6, 4, "Four");
    const QList<quint32> six = addDimmers(m_doc, 2, 6, "Six");
    QVERIFY(four.size() == 6 && six.size() == 2);
    FixtureGroup *a = addGroup(m_doc, "A", "Truss", { four[0], four[1], six[0] });
    FixtureGroup *b = addGroup(m_doc, "B", "Truss", { four[2], four[3], six[1] });
    FixtureGroup *c = addGroup(m_doc, "C", "", { four[4], four[5] });
    Q_UNUSED(c)

    FixtureManager mgr(nullptr, m_doc);

    // Groups.
    QTreeWidgetItem *ra = groupRow(mgr, a->id());
    QVERIFY(ra != nullptr);
    selectOnly(mgr.m_fixtures_tree, { ra });
    const QStringList texts = contextMenuTree(mgr);
    QVERIFY2(texts.contains("Select All Similar"), qPrintable(texts.join(" | ")));
    QCOMPARE(mgr.selectAllSimilar(), 2);
    QCOMPARE(selectedGroups(mgr), QSet<quint32>({ a->id(), b->id() }));

    // A fixture in the rig: every one of its kind, once each, under its universe.
    QTreeWidgetItem *rigRow = nullptr;
    for (QTreeWidgetItemIterator it(mgr.m_fixtures_tree); *it != nullptr; ++it)
        if ((*it)->data(KColumnName, PROP_ID).toString().toUInt() == four[0]
            && (*it)->data(KColumnName, PROP_ID).isValid()
            && (*it)->parent() != nullptr && (*it)->parent()->data(KColumnName, PROP_UNIVERSE).isValid())
            rigRow = *it;
    QVERIFY(rigRow != nullptr);
    selectOnly(mgr.m_fixtures_tree, { rigRow });
    QCOMPARE(mgr.selectAllSimilar(), 6);
    QList<quint32> got = selectedFixtureRows(mgr);
    std::sort(got.begin(), got.end());
    QCOMPARE(got, four);

    // A fixture inside group A: A's own of its kind.
    QTreeWidgetItem *inA = nullptr;
    ra = groupRow(mgr, a->id());
    for (int i = 0; ra != nullptr && i < ra->childCount(); i++)
        if (ra->child(i)->data(KColumnName, PROP_ID).toString().toUInt() == four[1])
            inA = ra->child(i);
    QVERIFY(inA != nullptr);
    selectOnly(mgr.m_fixtures_tree, { inA });
    QCOMPARE(mgr.selectAllSimilar(), 2);
    QList<QTreeWidgetItem *> rows;
    got = selectedFixtureRows(mgr, &rows);
    std::sort(got.begin(), got.end());
    QCOMPARE(got, QList<quint32>({ four[0], four[1] }));
    foreach (QTreeWidgetItem *r, rows)
        QCOMPARE(r->parent()->data(KColumnName, PROP_GROUP).toUInt(), a->id());
}

void FixtureManager_Test::moveToFolderMenuFilesGroups()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    const QList<quint32> four = addDimmers(m_doc, 4, 4, "Four");
    QCOMPARE(four.size(), 4);
    FixtureGroup *a = addGroup(m_doc, "A", "Truss", { four[0] });
    FixtureGroup *b = addGroup(m_doc, "B", "Truss", { four[1] });
    FixtureGroup *c = addGroup(m_doc, "C", "Floor", { four[2] });

    {
        FixtureManager mgr(nullptr, m_doc);
        selectOnly(mgr.m_fixtures_tree, { groupRow(mgr, a->id()), groupRow(mgr, b->id()) });
        const QStringList texts = contextMenuTree(mgr);
        QVERIFY2(texts.contains("Group ▸ Move 2 Groups to Folder ▸ Fixture Groups (no folder)"),
                 qPrintable(texts.join(" | ")));
        QVERIFY2(texts.contains("Group ▸ Move 2 Groups to Folder ▸ Floor"), qPrintable(texts.join(" | ")));
        QVERIFY2(texts.contains("Group ▸ Move 2 Groups to Folder ▸ New Folder…"),
                 qPrintable(texts.join(" | ")));
        QVERIFY2(texts.contains("Group ▸ Create Group(s) from Selection…"), qPrintable(texts.join(" | ")));
    }

    QMenu sub;
    FixtureManager::fillMoveToFolderMenu(&sub, m_doc, { a->id(), b->id() });
    QAction *truss = nullptr, *floor = nullptr, *root = nullptr, *newFolder = nullptr;
    foreach (QAction *act, sub.actions())
    {
        if (act->data().toString() == "Truss") truss = act;
        if (act->data().toString() == "Floor") floor = act;
        if (act->data().typeId() == QMetaType::QString && act->data().toString().isEmpty()) root = act;
        if (act->objectName() == "newGroupFolder") newFolder = act;
    }
    QVERIFY(truss && floor && root && newFolder);
    QVERIFY2(!truss->isEnabled(), "where they all are already is offered");

    QVERIFY(FixtureManager::applyMoveToFolder(nullptr, m_doc, { a->id(), b->id() }, floor));
    QCOMPARE(a->path(), QString("Floor"));
    QCOMPARE(b->path(), QString("Floor"));
    QCOMPARE(c->path(), QString("Floor"));

    // A new folder goes inside the one they are in.
    driveDialog([](QWidget *w) {
        QLineEdit *le = w->findChild<QLineEdit *>();
        if (le == nullptr)
            return false;
        le->setText(" Front ");
        return true;
    });
    QVERIFY(FixtureManager::applyMoveToFolder(nullptr, m_doc, { a->id(), b->id() }, newFolder));
    QCOMPARE(a->path(), QString("Floor/Front"));
    QCOMPARE(b->path(), QString("Floor/Front"));

    QVERIFY(FixtureManager::applyMoveToFolder(nullptr, m_doc, { a->id() }, root));
    QCOMPARE(a->path(), QString());
    QVERIFY(m_doc->isModified());
}

void FixtureManager_Test::createGroupFromSelectionStartsInTheCurrentFolder()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    const QList<quint32> four = addDimmers(m_doc, 4, 4, "Four");
    QCOMPARE(four.size(), 4);
    FixtureGroup *a = addGroup(m_doc, "A", "Truss/Top", { four[0], four[1] });
    FixtureGroup *b = addGroup(m_doc, "B", "Truss/Top", { four[2], four[3] });
    const int before = m_doc->fixtureGroups().size();

    FixtureManager mgr(nullptr, m_doc);
    QString seen;
    whenModal<CreateGroupsDialog>([&seen](CreateGroupsDialog *dlg) {
        seen = dlg->m_folderEdit->text();
        dlg->accept();
    });
    const QList<FixtureGroup *> made = mgr.createGroupsFromSelection({}, { a->id(), b->id() }, "Truss/Top");
    QCOMPARE(seen, QString("Truss/Top"));
    QCOMPARE(m_doc->fixtureGroups().size(), before + 1);
    QCOMPARE(made.size(), 1);
    QCOMPARE(made.first()->path(), QString("Truss/Top"));
}

void FixtureManager_Test::groupToolsOnARealShow()
{
    QString show = qEnvironmentVariable("FM_DH_SHOW");
    if (show.isEmpty())
        show = QString::fromUtf8(FM_DH_SHOW);
    if (!QFileInfo::exists(show))
        QSKIP("DanceHysteria-0.6 is not here (set FM_DH_SHOW)");

    // Read a copy: the show itself is never touched.
    QTemporaryDir dir;
    const QString copy = dir.filePath(QStringLiteral("show.qlcc"));
    QVERIFY(QFile::copy(show, copy));
    m_doc->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures"));
    m_doc->setWorkspacePath(dir.path());
    if (m_doc->showEmbedding()->readWorkspace(copy))
        m_doc->showEmbedding()->apply(dir.path());
    QFile file(copy);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QXmlStreamReader r(&file);
    bool loaded = false;
    while (!r.atEnd() && !loaded)
        if (r.readNextStartElement() && r.name() == QLatin1String("Engine"))
            loaded = m_doc->loadXML(r, false);
    QVERIFY(loaded);
    QVERIFY2(m_doc->fixtureGroups().size() >= 40, "not the show with the groups");

    auto byName = [this](const QString &n) -> FixtureGroup * {
        foreach (FixtureGroup *g, m_doc->fixtureGroups())
            if (g->name() == n)
                return g;
        return nullptr;
    };
    FixtureGroup *ds1t = byName("DS1t");
    FixtureGroup *ds1f = byName("DS1f");
    QVERIFY(ds1t && ds1f);

    // The truss tops alike: all of them, none of the faces.
    const QList<quint32> tops = FixtureManager::similarGroups(m_doc, { ds1t->id() });
    QStringList topNames;
    foreach (quint32 gid, tops)
        topNames << m_doc->fixtureGroup(gid)->name();
    QVERIFY2(tops.size() >= 4, qPrintable(topNames.join(", ")));
    QVERIFY2(!topNames.contains("DS1f") && !topNames.contains("DS1t RGB"), qPrintable(topNames.join(", ")));

    {
        FixtureManager mgr(nullptr, m_doc);
        selectOnly(mgr.m_fixtures_tree, { groupRow(mgr, ds1t->id()) });
        QCOMPARE(mgr.selectAllSimilar(), tops.size());
        QCOMPARE(selectedGroups(mgr), QSet<quint32>(tops.begin(), tops.end()));

        // ...into a new folder, through the menu's own handler.
        QMenu sub;
        FixtureManager::fillMoveToFolderMenu(&sub, m_doc, tops);
        QAction *nf = sub.findChild<QAction *>("newGroupFolder");
        QVERIFY(nf != nullptr);
        driveDialog([](QWidget *w) {
            QLineEdit *le = w->findChild<QLineEdit *>();
            if (le == nullptr)
                return false;
            le->setText("Truss tops");
            return true;
        });
        QVERIFY(FixtureManager::applyMoveToFolder(&mgr, m_doc, tops, nf));
        mgr.updateView();
        foreach (quint32 gid, tops)
            QCOMPARE(m_doc->fixtureGroup(gid)->path(), QString("Truss tops"));
        QVERIFY(groupRow(mgr, ds1t->id())->parent()->data(KColumnName, PROP_FOLDER).toString()
                == QString("Truss tops"));
        // Alike still: they moved together.
        QCOMPARE(FixtureManager::similarGroups(m_doc, { ds1t->id() }).size(), tops.size());
    }

    // Build's Groups & fixtures: the same rule.
    FixtureGroupSource src(m_doc);
    src.reload();
    QTreeWidgetItem *srcRow = src.groupItemById(ds1t->id());
    QVERIFY(srcRow != nullptr);
    src.clearSelection();
    QCOMPARE(src.selectAllSimilar(srcRow), tops.size());
    QSet<quint32> srcSel;
    foreach (QTreeWidgetItem *it, src.selectedItems())
        if (it->data(0, FixtureGroupSource::KindRole).toInt() == FixtureGroupSource::GroupNode)
            srcSel.insert(it->data(0, FixtureGroupSource::IdRole).toUInt());
    QCOMPARE(srcSel, QSet<quint32>(tops.begin(), tops.end()));

    // A fixture leaf inside a group: that group's of its kind.
    QTreeWidgetItem *leaf = srcRow->child(0);
    QVERIFY(leaf != nullptr);
    src.clearSelection();
    const int n = src.selectAllSimilar(leaf);
    QVERIFY(n >= 1);
    foreach (QTreeWidgetItem *it, src.selectedItems())
        QCOMPARE(it->parent(), srcRow);
}

void FixtureManager_Test::newHereMakesAGroupInTheFolder()
{
    const QList<quint32> fids = addDimmers(m_doc, 3, 1, "Dim");
    FixtureGroup *front = addGroup(m_doc, "Front", "Movers", fids.mid(0, 2));

    FixtureManager mgr(nullptr, m_doc);
    mgr.resize(1000, 700);
    mgr.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mgr));
    mgr.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&mgr));
    QShortcut *sc = mgr.findChild<QShortcut *>("fmNewHere");
    QVERIFY(sc != nullptr);
    QCOMPARE(sc->key(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));

    // A group selected: a new one beside it, in its folder.
    QTreeWidgetItem *row = groupRow(mgr, front->id());
    QVERIFY(row != nullptr);
    mgr.m_fixtures_tree->setCurrentItem(row);
    QCOMPARE(mgr.newHereLabel(), QString("New Group in Movers"));
    const quint32 gid = mgr.newHere();
    FixtureGroup *made = m_doc->fixtureGroup(gid);
    QVERIFY(made != nullptr);
    QCOMPARE(made->name(), QString("New Group"));
    QCOMPARE(made->path(), QString("Movers"));
    QCOMPARE(made->size(), QSize(4, 4));
    QCOMPARE(selectedGroups(mgr), QSet<quint32>({ gid }));
    QVERIFY(mgr.m_groupEditor != nullptr);
    QLineEdit *name = mgr.m_groupEditor->findChild<QLineEdit *>("m_nameEdit");
    QVERIFY(name != nullptr);
    QTRY_VERIFY(name->hasFocus());
    QCOMPARE(name->selectedText(), QString("New Group"));

    // Again, from the new one: the next free name.
    QCOMPARE(m_doc->fixtureGroup(mgr.newHere())->name(), QString("New Group 2"));

    // A fixture in a group: that group's folder. A loose one: nothing.
    QTreeWidgetItem *inGroup = groupRow(mgr, front->id())->child(0);
    QVERIFY(inGroup != nullptr);
    mgr.m_fixtures_tree->setCurrentItem(inGroup);
    QCOMPARE(mgr.newHereLabel(), QString("New Group in Movers"));
    QTreeWidgetItem *loose = nullptr;
    for (QTreeWidgetItemIterator it(mgr.m_fixtures_tree); *it != nullptr; ++it)
        if ((*it)->data(KColumnName, PROP_ID).isValid()
            && (*it)->data(KColumnName, PROP_ID).toString().toUInt() == fids.at(2)
            && ((*it)->parent() == nullptr || !(*it)->parent()->data(KColumnName, PROP_GROUP).isValid()))
            loose = *it;
    QVERIFY(loose != nullptr);
    mgr.m_fixtures_tree->setCurrentItem(loose);
    QCOMPARE(mgr.newHereLabel(), QString());
    QCOMPARE(mgr.newHere(), FixtureGroup::invalidId());
}

void FixtureManager_Test::outputRulesForFixturesAndGroups()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    const QList<quint32> three = addDimmers(m_doc, 3, 1, "Circuit");
    QCOMPARE(three.size(), 3);
    FixtureGroup *house = addGroup(m_doc, "House", QString(), { three[0], three[1] });
    m_doc->fixture(three[0])->setOutputRules(Fixture::Switch);

    // A group's members, mixed: Switch partly checked and left as each has
    // it; Blackout ticked for all.
    {
        OutputRulesDialog dlg(m_doc, OutputRulesDialog::collect(m_doc, { house->id() }, {}));
        QCheckBox *sw = dlg.box(Fixture::Switch);
        QCheckBox *bo = dlg.box(Fixture::ExemptBlackout);
        QVERIFY(sw != nullptr && bo != nullptr);
        QCOMPARE(sw->checkState(), Qt::PartiallyChecked);
        QCOMPARE(bo->checkState(), Qt::Unchecked);
        bo->setChecked(true);
        QVERIFY(dlg.apply());
        QCOMPARE(m_doc->fixture(three[0])->outputRules(), Fixture::Switch | Fixture::ExemptBlackout);
        QCOMPARE(m_doc->fixture(three[1])->outputRules(), Fixture::OutputRules(Fixture::ExemptBlackout));
        QCOMPARE(m_doc->fixture(three[2])->outputRules(), Fixture::OutputRules());
    }
    {
        OutputRulesDialog dlg(m_doc, OutputRulesDialog::collect(m_doc, { house->id() }, {}));
        QCOMPARE(dlg.box(Fixture::ExemptBlackout)->checkState(), Qt::Checked);
        dlg.box(Fixture::Switch)->setCheckState(Qt::Checked);
        QVERIFY(dlg.apply());
        QVERIFY(m_doc->fixture(three[1])->outputRules() & Fixture::Switch);
        QVERIFY(dlg.apply() == false);      // nothing more to change
    }

    // The Fixture Manager offers it for a fixture and for a group, and the
    // fixture's row says what it has.
    FixtureManager mgr(nullptr, m_doc);
    selectOnly(mgr.m_fixtures_tree, { fixtureRow(mgr, three[2]) });
    QVERIFY2(contextMenuTree(mgr).contains("Output Rules…"), "offered for a fixture");
    selectOnly(mgr.m_fixtures_tree, { groupRow(mgr, house->id()) });
    QVERIFY2(contextMenuTree(mgr).contains("Output Rules…"), "offered for a group");

    driveDialog([](QWidget *w) {
        QCheckBox *gm = w->findChild<QCheckBox *>("outputRuleGrandMaster");
        if (gm == nullptr)
            return false;
        gm->setChecked(true);
        return true;
    });
    QVERIFY(OutputRulesDialog::edit(nullptr, m_doc, {}, { three[2] }));
    QCOMPARE(m_doc->fixture(three[2])->outputRules(), Fixture::OutputRules(Fixture::ExemptGrandMaster));
    mgr.updateView();
    QTreeWidgetItem *row = fixtureRow(mgr, three[2]);
    QVERIFY(row != nullptr);
    QVERIFY2(row->toolTip(KColumnName).contains("Grand Master"), qPrintable(row->toolTip(KColumnName)));
    QVERIFY(fixtureRow(mgr, three[0])->toolTip(KColumnName).contains("Blackout"));

    // Build's Groups & fixtures offers it too.
    FixtureGroupSource src(m_doc);
    src.reload();
    QTreeWidgetItem *grp = src.groupItemById(house->id());
    QVERIFY(grp != nullptr);
    src.clearSelection();
    grp->setSelected(true);
    QStringList texts;
    QTimer::singleShot(0, [&texts]() {
        if (QMenu *m = qobject_cast<QMenu *>(QApplication::activePopupWidget()))
        {
            foreach (QAction *a, m->actions())
                texts << a->text();
            m->close();
        }
    });
    src.slotContextMenu(src.visualItemRect(grp).center());
    QVERIFY2(texts.contains("Output Rules…"), qPrintable(texts.join(" | ")));
}

QTEST_MAIN(FixtureManager_Test)

namespace
{
/* A definition of its own with a .qxf, in @p doc's cache (which owns it), and
 * @p count fixtures on it. */
QList<quint32> addFixturesOfDef(Doc *doc, const QString &model, int count)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(QStringLiteral("Test"));
    def->setModel(model);
    def->setType(QLCFixtureDef::MovingHead);
    def->setDefinitionSourceFile(QStringLiteral("/nonexistent/Test-%1.qxf").arg(model));
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(QStringLiteral("1ch"));
    QLCChannel *ch = new QLCChannel();
    ch->setName(QStringLiteral("Dimmer"));
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    def->addMode(mode);
    doc->fixtureDefCache()->addFixtureDef(def);

    QList<quint32> ids;
    for (int i = 0; i < count; i++)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QStringLiteral("%1 %2").arg(model).arg(i + 1));
        f->setFixtureDefinition(def, mode);
        f->setAddress(quint32(400 + doc->fixtures().size()));
        if (!doc->addFixture(f))
            return QList<quint32>();
        ids << f->id();
    }
    return ids;
}
} // namespace

void FixtureManager_Test::editDefinitionForFixturesSharingOne()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    const QList<quint32> spots = addFixturesOfDef(m_doc, "Spot", 2);
    const QList<quint32> washes = addFixturesOfDef(m_doc, "Wash", 1);
    QCOMPARE(spots.size(), 2);
    QCOMPARE(washes.size(), 1);
    FixtureGroup *spotGroup = addGroup(m_doc, "Spots", QString(), spots);
    FixtureGroup *mixedGroup = addGroup(m_doc, "Mixed", QString(), { spots[0], washes[0] });

    QCOMPARE(FixtureManager::sharedDefinitionSourceFile(m_doc, spots),
             QStringLiteral("/nonexistent/Test-Spot.qxf"));
    int defs = 0;
    QVERIFY(FixtureManager::sharedDefinitionSourceFile(m_doc, { spots[0], washes[0] }, &defs).isEmpty());
    QCOMPARE(defs, 2);

    FixtureManager mgr(nullptr, m_doc);
    const QString edit = QStringLiteral("Edit Fixture Definition…");
    const QString several = QStringLiteral("Edit Fixture Definition… (several definitions)");

    // One fixture, as before.
    selectOnly(mgr.m_fixtures_tree, { fixtureRow(mgr, spots[0]) });
    QVERIFY(contextMenuTree(mgr).contains(edit));

    // Two of the same definition: offered.
    selectOnly(mgr.m_fixtures_tree, { fixtureRow(mgr, spots[0]), fixtureRow(mgr, spots[1]) });
    QStringList texts = contextMenuTree(mgr);
    QVERIFY2(texts.contains(edit), qPrintable(texts.join(" | ")));

    // Two definitions: only the disabled note.
    selectOnly(mgr.m_fixtures_tree, { fixtureRow(mgr, spots[0]), fixtureRow(mgr, washes[0]) });
    texts = contextMenuTree(mgr);
    QVERIFY2(!texts.contains(edit), qPrintable(texts.join(" | ")));
    QVERIFY2(texts.contains(several), qPrintable(texts.join(" | ")));

    // A group whose members share one: offered; a mixed group: not.
    selectOnly(mgr.m_fixtures_tree, { groupRow(mgr, spotGroup->id()) });
    texts = contextMenuTree(mgr);
    QVERIFY2(texts.contains(edit), qPrintable(texts.join(" | ")));
    selectOnly(mgr.m_fixtures_tree, { groupRow(mgr, mixedGroup->id()) });
    texts = contextMenuTree(mgr);
    QVERIFY2(!texts.contains(edit), qPrintable(texts.join(" | ")));
}

void FixtureManager_Test::stageLookIsOfferedInOperate()
{
    QVERIFY(m_doc->inputOutputMap()->addUniverse());
    const QList<quint32> two = addDimmers(m_doc, 2, 1, "Look");
    QCOMPARE(two.size(), 2);
    FixtureManager mgr(nullptr, m_doc);
    m_doc->setMode(Doc::Operate);
    selectOnly(mgr.m_fixtures_tree, { fixtureRow(mgr, two[0]), fixtureRow(mgr, two[1]) });
    const QStringList texts = contextMenuTree(mgr);
    m_doc->setMode(Doc::Design);
    QVERIFY2(texts.contains("Stage Look"), qPrintable(texts.join(" | ")));
    QVERIFY2(!texts.filter(QStringLiteral("Stage Look ▸ ")).isEmpty(), qPrintable(texts.join(" | ")));
}

void FixtureManager_Test::stageLookMenuHasAScenerySectionWithCurtain()
{
    const QList<quint32> one = addDimmers(m_doc, 1, 1, "Relay");
    QCOMPARE(one.size(), 1);
    Fixture *fx = m_doc->fixture(one.first());
    QVERIFY(fx != nullptr);

    QMenu menu;
    FixtureManager::fillStageLookMenu(&menu, m_doc, one);
    // Its own section, last, after the practicals.
    const QList<QAction *> acts = menu.actions();
    int scenery = -1, curtain = -1, practicals = -1;
    for (int i = 0; i < acts.size(); ++i)
    {
        if (acts.at(i)->isSeparator() && acts.at(i)->text() == "Scenery")
            scenery = i;
        if (acts.at(i)->isSeparator() && acts.at(i)->text() == "Practicals")
            practicals = i;
        if (acts.at(i)->objectName() == "stageLook_Curtain")
            curtain = i;
    }
    QVERIFY(practicals >= 0);
    QVERIFY(scenery > practicals);
    QCOMPARE(curtain, scenery + 1);
    QCOMPARE(curtain, int(acts.size()) - 1);
    QCOMPARE(acts.at(curtain)->text(), QString("Curtain"));
    QVERIFY(!acts.at(curtain)->icon().isNull());
    // A dimmer's Auto is not a curtain.
    QVERIFY2(!acts.first()->text().contains("Curtain"), qPrintable(acts.first()->text()));

    acts.at(curtain)->trigger();
    QCOMPARE(fx->stageLook(), Fixture::CurtainLook);
    QMenu again;
    FixtureManager::fillStageLookMenu(&again, m_doc, one);
    QAction *checked = again.findChild<QAction *>("stageLook_Curtain");
    QVERIFY(checked != nullptr && checked->isChecked());
    // Still "Auto (...)" what it would be unmarked.
    QVERIFY2(!again.actions().first()->text().contains("Curtain"),
             qPrintable(again.actions().first()->text()));
    fx->setStageLook(Fixture::AutoLook);
}
