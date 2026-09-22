/*
  Q Light Controller Plus - Test Unit
  monitor_test.cpp

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
#include <QGraphicsItem>
#include <QElapsedTimer>
#include <QTimer>
#include <QApplication>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QAbstractSpinBox>
#include <functional>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>

#define protected public
#define private public
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "structurestudioview.h"
#include "monitorlayerspanel.h"
#include "platformitem.h"
#include "studiotemplate.h"
#include <QTreeWidgetItemIterator>
#include "fixturevisualtraits.h"
#include "aimsolver.h"
#include "scene.h"
#include "zraster.h"
#include <QtMath>
#include "trussitem.h"
#include "targetitem.h"
#undef protected
#undef private

#include "monitor_test.h"
#include "monitorproperties.h"
#include "truss.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "fixturegroup.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "qlccapability.h"
#include "doc.h"
#include "qlcfile.h"
#include "qlcfixturedefcache.h"
#include <QFileInfo>

// Find a visible QPushButton by its exact text anywhere under a widget —
// QMessageBox's extra buttons (confirmFeatureDelete's "Delete"/"Detach &&
// Keep"/etc.) are added via addButton(text, role), not standard enum roles,
// so QMessageBox::button(StandardButton) can't reach them.
static QPushButton *findButtonByText(QWidget *root, const QString &text)
{
    foreach (QPushButton *b, root->findChildren<QPushButton*>())
        if (b->text().remove('&') == text)
            return b;
    return NULL;
}

/* A QDoubleSpinBox owns its own internal QLineEdit editor widget, so a
 * plain findChild<QLineEdit*>() on a dialog can grab THAT instead of the
 * dialog's actual "Name:" field -- which one it finds first depends on
 * where each lands in the widget tree, not on construction order. Skip any
 * QLineEdit whose parent is itself a spin box; return the first one left. */
static QLineEdit *findRealLineEdit(QWidget *modal)
{
    foreach (QLineEdit *le, modal->findChildren<QLineEdit *>())
        if (qobject_cast<QAbstractSpinBox *>(le->parentWidget()) == nullptr)
            return le;
    return nullptr;
}

/* Schedules a zero-delay callback that runs `fill` against whatever modal
 * QDialog is open by then (via QApplication::activeModalWidget() -- the
 * caller calls this BEFORE the slot that opens the dialog, the same as
 * every QTimer::singleShot() call this replaces), then ALWAYS clicks its Ok
 * button (or Cancel, if accept is false) -- or closes it outright if that
 * button can't be found -- BEFORE checking whether `fill` reported success.
 *
 * Why the close has to come first: a QVERIFY/QCOMPARE failure inside `fill`
 * itself returns out of `fill` early (reporting false), but if the close
 * were the LAST thing this function did, that early return would skip it
 * entirely -- and nothing else will ever close that dialog under offscreen
 * QTest (there's no user to click Cancel), so dlg.exec()'s nested event
 * loop blocks forever. A wrong widget lookup would hang the whole test
 * binary instead of just failing the one test that made it. Every
 * dialog-driving QTimer::singleShot() callback in this file used to hand-rig
 * this close-before-assert shape (or skip it and carry the risk); this is
 * that shape, written once. See WORKFLOW-SUBTLETIES.md #7. */
static void driveDialog(const std::function<bool(QWidget *)> &fill, bool accept = true)
{
    QTimer::singleShot(0, [fill, accept]() {
        QWidget *modal = QApplication::activeModalWidget();
        const bool filled = (modal != nullptr) && fill(modal);
        QDialogButtonBox *btns = modal ? modal->findChild<QDialogButtonBox *>() : nullptr;
        QPushButton *btn = btns ? btns->button(accept ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel)
                                : nullptr;
        if (btn != nullptr)
            btn->click();
        else if (modal != nullptr)
            modal->close();
        QVERIFY2(modal != nullptr && filled && btn != nullptr,
                 "the dialog's widgets were not where expected");
    });
}

/* Same close-before-assert shape as driveDialog(), for a QMessageBox-style
 * confirm whose button is found by its TEXT rather than a QDialogButtonBox
 * role (confirmFeatureDelete()'s "Delete"/"Detach & Keep"/etc. buttons are
 * added via addButton(text, role), not a standard enum role). */
static void driveMessageBox(const std::function<bool(QWidget *)> &fill, const QString &buttonText)
{
    QTimer::singleShot(0, [fill, buttonText]() {
        QWidget *modal = QApplication::activeModalWidget();
        const bool filled = (modal != nullptr) && fill(modal);
        QPushButton *btn = modal ? findButtonByText(modal, buttonText) : nullptr;
        if (btn != nullptr)
            btn->click();
        else if (modal != nullptr)
            modal->close();
        QVERIFY2(modal != nullptr && filled && btn != nullptr,
                 qPrintable(QString("the message box's \"%1\" button was not where expected")
                            .arg(buttonText)));
    });
}

// Pilot for the Lighting Studio release-gate work (SHOW_LIFECYCLE_DESIGN.md):
// can a QTest UI test drive a real, blocking QDialog::exec() call (as
// Monitor::slotAddTruss() uses) under QT_QPA_PLATFORM=offscreen? Standard
// technique: schedule the dialog interaction via QTimer::singleShot(0, ...)
// BEFORE calling the slot — exec()'s own nested event loop processes it.

void Monitor_Test::initTestCase()
{
    m_doc = new Doc(this);
}

void Monitor_Test::cleanupTestCase()
{
    delete m_doc;
    m_doc = NULL;
}

void Monitor_Test::addTrussAccepted()
{
    QWidget parent;
    Monitor mon(&parent, m_doc);

    const int before = m_doc->monitorProperties()->trusses().count();

    driveDialog([](QWidget *modal) {
        if (modal->windowTitle() != QString("Add Truss"))
            return false;
        QLineEdit *nameEdit = modal->findChild<QLineEdit *>();
        if (nameEdit == nullptr)
            return false;
        nameEdit->setText("Pilot Truss");
        return true;
    });

    mon.slotAddTruss();

    QCOMPARE(m_doc->monitorProperties()->trusses().count(), before + 1);
    Truss *t = m_doc->monitorProperties()->trusses().last();
    QCOMPARE(t->name(), QString("Pilot Truss"));

    m_doc->monitorProperties()->removeTruss(t->id());
}

void Monitor_Test::addTrussCancelled()
{
    QWidget parent;
    Monitor mon(&parent, m_doc);

    const int before = m_doc->monitorProperties()->trusses().count();

    driveDialog([](QWidget *) { return true; }, /*accept=*/false);

    mon.slotAddTruss();

    // Cancelling adds nothing.
    QCOMPARE(m_doc->monitorProperties()->trusses().count(), before);
}

void Monitor_Test::addTargetAccepted()
{
    // slotAddTarget() differs from slotAddTruss(): the StageTarget is created
    // with defaults FIRST, then an edit dialog opens — so even a cancelled
    // edit leaves the target in place (covered by addTargetEditCancelled()
    // below). Accepting here also auto-links an Aim palette to it (not
    // PanTilt -- see slotAddTarget()'s own comment on why: a PanTilt
    // palette carries fixed values and doesn't follow the target if it
    // moves, Aim resolves per fixture from geometry).
    QWidget parent;
    Monitor mon(&parent, m_doc);

    const int beforeTargets = m_doc->monitorProperties()->stageTargets().count();
    const int beforePalettes = m_doc->palettes().count();

    driveDialog([](QWidget *modal) {
        if (!modal->windowTitle().startsWith("Edit Target"))
            return false;
        QLineEdit *nameEdit = modal->findChild<QLineEdit *>();
        if (nameEdit == nullptr)
            return false;
        nameEdit->setText("Pilot Target");
        return true;
    });

    mon.slotAddTarget();

    QCOMPARE(m_doc->monitorProperties()->stageTargets().count(), beforeTargets + 1);
    StageTarget *t = m_doc->monitorProperties()->stageTargets().last();
    QCOMPARE(t->name(), QString("Pilot Target"));

    // An Aim palette auto-links to the new target.
    QCOMPARE(m_doc->palettes().count(), beforePalettes + 1);
    bool linked = false;
    foreach (QLCPalette *p, m_doc->palettes())
        if (p->stageTargetId() == t->id())
            linked = true;
    QVERIFY(linked);

    foreach (QLCPalette *p, m_doc->palettes())
        if (p->stageTargetId() == t->id())
            m_doc->deletePalette(p->id());
    m_doc->monitorProperties()->removeStageTarget(t->id());
}

void Monitor_Test::addTargetEditCancelled()
{
    QWidget parent;
    Monitor mon(&parent, m_doc);

    const int before = m_doc->monitorProperties()->stageTargets().count();

    driveDialog([](QWidget *) { return true; }, /*accept=*/false);

    mon.slotAddTarget();

    // Unlike Truss: the target was already created before the dialog opened,
    // so cancelling the EDIT still leaves it in place, with its auto-name.
    QCOMPARE(m_doc->monitorProperties()->stageTargets().count(), before + 1);
    StageTarget *t = m_doc->monitorProperties()->stageTargets().last();
    QVERIFY(t->name().startsWith("Target"));

    m_doc->monitorProperties()->removeStageTarget(t->id());
}

void Monitor_Test::addPlatformEditCancelled()
{
    // Platform's edit dialog is a heavier one than Truss/Target — it embeds a
    // full StructureStudioView (canvas + tree + inspector) via makeStudioPane(),
    // inside which a bare findChild<QLineEdit*>() is ambiguous (multiple
    // QLineEdits exist in that subtree, not just the name field). Rather than
    // guess at which one, this test only drives the unambiguous button box —
    // proving the add-then-cancel-a-heavier-editor path still works without
    // relying on a fragile widget lookup. (A same-as-Truss "accept with a
    // custom name" test would need the production dialog to tag its name
    // field with an objectName() first — not done here.)
    QWidget parent;
    Monitor mon(&parent, m_doc);

    const int before = m_doc->monitorProperties()->platforms().count();

    driveDialog([](QWidget *) { return true; }, /*accept=*/false);

    mon.slotAddPlatform();

    // Same story as Target: created before the dialog, so cancel keeps it.
    QCOMPARE(m_doc->monitorProperties()->platforms().count(), before + 1);
    StagePlatform *p = m_doc->monitorProperties()->platforms().last();
    QVERIFY(p->name().startsWith("Platform"));

    m_doc->monitorProperties()->removePlatform(p->id());
}

void Monitor_Test::removeSelectedTruss()
{
    // The other half of Phase 3's pilot: slotRemoveSelected() reads the
    // QGraphicsScene selection, dispatches to slotTrussRemoveRequested() for
    // a single selected TrussItem, which drives its OWN confirm QMessageBox
    // (confirmFeatureDelete) — a second, different kind of modal to prove.
    QWidget parent;
    Monitor mon(&parent, m_doc);

    quint32 addedId = Truss::invalidId();
    driveDialog([](QWidget *) { return true; });
    mon.slotAddTruss();
    addedId = m_doc->monitorProperties()->trusses().last()->id();

    const int before = m_doc->monitorProperties()->trusses().count();

    TrussItem *item = NULL;
    foreach (QGraphicsItem *gi, mon.m_graphicsView->scene()->items())
        if (TrussItem *ti = dynamic_cast<TrussItem*>(gi))
            if (ti->trussId() == addedId)
                item = ti;
    QVERIFY(item != NULL);
    item->setSelected(true);

    // NOTE: don't assert on windowTitle() here — QMessageBox renders
    // without a title bar on macOS (a native-alert-style quirk), so it
    // reads back empty even though confirmFeatureDelete() sets one. The
    // box's own text (via QMessageBox::text()) is the reliable check.
    driveMessageBox([](QWidget *modal) {
        QMessageBox *box = qobject_cast<QMessageBox *>(modal);
        return box != nullptr && box->text().contains("Delete");
    }, "Delete");

    mon.slotRemoveSelected();

    QCOMPARE(m_doc->monitorProperties()->trusses().count(), before - 1);
}

void Monitor_Test::removeSelectedCancelled()
{
    QWidget parent;
    Monitor mon(&parent, m_doc);

    driveDialog([](QWidget *) { return true; });
    mon.slotAddTruss();
    const quint32 addedId = m_doc->monitorProperties()->trusses().last()->id();

    const int before = m_doc->monitorProperties()->trusses().count();

    TrussItem *item = NULL;
    foreach (QGraphicsItem *gi, mon.m_graphicsView->scene()->items())
        if (TrussItem *ti = dynamic_cast<TrussItem*>(gi))
            if (ti->trussId() == addedId)
                item = ti;
    QVERIFY(item != NULL);
    item->setSelected(true);

    driveMessageBox([](QWidget *) { return true; }, "Cancel");

    mon.slotRemoveSelected();

    // Cancelling the confirm leaves the truss in place.
    QCOMPARE(m_doc->monitorProperties()->trusses().count(), before);

    m_doc->monitorProperties()->removeTruss(addedId);
}




QTEST_MAIN(Monitor_Test)

/****************************************************************************
 * Drop round-trips
 *
 * Each test simulates the END of a drag exactly the way Qt leaves it -- the
 * item is at its new scene position, nothing else has happened -- then calls
 * the same slotFixtureMoved() the mouse release calls, and asserts:
 *   1. the item does not jump at the moment of the drop, and
 *   2. after updateFixture() re-places it FROM THE MODEL, it is still there.
 * (2) is the round trip: it fails if the drop wrote the wrong thing, wrote it
 * with the wrong anchor or units, or wrote it somewhere the redraw does not
 * read. Every "snaps back" and "jumps on drop" report is one of those.
 ****************************************************************************/

namespace
{
struct DropRig
{
    QWidget parent;
    Monitor *mon = nullptr;
    MonitorGraphicsView *gv = nullptr;
    Fixture *fxi = nullptr;
    MonitorFixtureItem *item = nullptr;
    Doc *docForCleanup = nullptr;

    /* RAII, because QVERIFY returns out of the test on failure: an explicit
       teardown call is skipped exactly when the test fails, and a leaked
       Monitor poisons every test after it. */
    ~DropRig()
    {
        if (docForCleanup != nullptr && fxi != nullptr)
            docForCleanup->deleteFixture(fxi->id());
        delete mon;
    }

    // Not a QObject: assertion macros need a plain function scope.
    bool build(Doc *doc, const QVector3D &posMm)
    {
        docForCleanup = doc;
        mon = new Monitor(&parent, doc);
        gv = mon->findChild<MonitorGraphicsView *>();
        if (gv == nullptr)
            return false;

        fxi = new Fixture(doc);
        fxi->setName("DropProbe");
        fxi->setChannels(1);
        if (doc->addFixture(fxi) == false)
            return false;

        doc->monitorProperties()->setFixturePosition(fxi->id(), 0, 0, posMm);

        /* A deterministic canvas: known widget size, metric grid, no snap.
           setGridSize()/setGridMetrics() recompute cellPixels from width(),
           which resize() has already set even though nothing is shown. */
        gv->resize(1230, 760);
        gv->setSnapDivisions(0);
        gv->setGridMetrics(1000.0);            // metres
        gv->setGridSize(QSize(40, 24));        // 1230/40=30, 760/24=31 -> 30 px/cell
        if (gv->m_cellPixels <= 0)
            return false;

        gv->addFixture(fxi->id(), QPointF(posMm.x(), posMm.y()));
        item = gv->m_fixtures.value(fxi->id(), nullptr);
        return item != nullptr;
    }

};

/* Drop the item at an offset from where it currently sits and return how far
 * it ended up from the intended drop point, (a) immediately and (b) after a
 * model-driven re-place. */
static void dropAndMeasure(MonitorGraphicsView *gv, MonitorFixtureItem *item,
                           quint32 fid, const QPointF &deltaPx,
                           qreal &jumpPx, qreal &snapBackPx)
{
    const QPointF target = item->pos() + deltaPx;
    item->setPos(target);
    gv->slotFixtureMoved(item);
    jumpPx = (item->pos() - target).manhattanLength();
    gv->updateFixture(fid);
    snapBackPx = (item->pos() - target).manhattanLength();
}
} // namespace

void Monitor_Test::dropStaysPutTopView()
{
    DropRig rig;
    // Far corner: nowhere near any truss, so attach logic cannot interfere.
    QVERIFY(rig.build(m_doc, QVector3D(35000, 20000, 0)));
    rig.gv->setViewPOV(MonitorGraphicsView::PovTop);
    rig.gv->updateFixture(rig.fxi->id());

    qreal jump = 0, back = 0;
    dropAndMeasure(rig.gv, rig.item, rig.fxi->id(), QPointF(-150, -90), jump, back);
    QVERIFY2(jump < 2.0, qPrintable(QString("item jumped %1 px at the drop").arg(jump)));
    QVERIFY2(back < 2.0, qPrintable(QString("item moved %1 px when re-placed from the model").arg(back)));

}

void Monitor_Test::dropStaysPutFrontView()
{
    DropRig rig;
    QVERIFY(rig.build(m_doc, QVector3D(35000, 20000, 0)));
    rig.gv->setViewPOV(MonitorGraphicsView::PovFront);
    rig.gv->updateFixture(rig.fxi->id());

    const float yBefore = m_doc->monitorProperties()
                          ->fixturePosition(rig.fxi->id(), 0, 0).y();
    qreal jump = 0, back = 0;
    dropAndMeasure(rig.gv, rig.item, rig.fxi->id(), QPointF(-120, -60), jump, back);
    QVERIFY2(jump < 2.0, qPrintable(QString("front: jumped %1 px at the drop").arg(jump)));
    QVERIFY2(back < 2.0, qPrintable(QString("front: moved %1 px on re-place").arg(back)));

    // Front edits X and height; stage depth (Y) is not on screen and must
    // come through untouched.
    const float yAfter = m_doc->monitorProperties()
                         ->fixturePosition(rig.fxi->id(), 0, 0).y();
    QCOMPARE(yAfter, yBefore);

}

void Monitor_Test::dropStaysPutSideView()
{
    DropRig rig;
    QVERIFY(rig.build(m_doc, QVector3D(35000, 20000, 0)));
    rig.gv->setViewPOV(MonitorGraphicsView::PovSide);
    rig.gv->updateFixture(rig.fxi->id());

    const float xBefore = m_doc->monitorProperties()
                          ->fixturePosition(rig.fxi->id(), 0, 0).x();
    qreal jump = 0, back = 0;
    dropAndMeasure(rig.gv, rig.item, rig.fxi->id(), QPointF(100, -80), jump, back);
    QVERIFY2(jump < 2.0, qPrintable(QString("side: jumped %1 px at the drop").arg(jump)));
    QVERIFY2(back < 2.0, qPrintable(QString("side: moved %1 px on re-place").arg(back)));

    // Side edits Y and height; X is not on screen.
    const float xAfter = m_doc->monitorProperties()
                         ->fixturePosition(rig.fxi->id(), 0, 0).x();
    QCOMPARE(xAfter, xBefore);

}

void Monitor_Test::dropOnTrussAttachesAndStays()
{
    DropRig rig;
    QVERIFY(rig.build(m_doc, QVector3D(5000, 5000, 0)));
    rig.gv->setViewPOV(MonitorGraphicsView::PovTop);

    MonitorProperties *props = m_doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("DropBar");
    t->setOrigin(QVector3D(10.0f, 12.0f, 4.0f));   // metres
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(6.0f);
    t->setWidth(0.3f);
    rig.gv->updateTrusses();
    rig.gv->updateFixture(rig.fxi->id());

    // Drop the fixture's CENTRE onto the truss line, 2 m along the run.
    const QPointF onTrussPx = rig.gv->realPositionToPixels(12000.0, 12000.0);
    rig.item->setPos(rig.item->pos() + (onTrussPx - rig.item->iconCentreScene()));
    const QPointF target = rig.item->pos();
    rig.gv->slotFixtureMoved(rig.item);

    const FixtureRigProps rp = props->fixtureRigProps(rig.fxi->id());
    QVERIFY2(rp.trussId == t->id(), "released on the truss but did not attach");

    // The drop point WAS on the truss line, so attaching must not teleport it:
    // it stays where it was released, both immediately and re-placed.
    QVERIFY2((rig.item->pos() - target).manhattanLength() < qreal(rig.gv->m_cellPixels),
             qPrintable(QString("attach teleported the item %1 px away")
                        .arg((rig.item->pos() - target).manhattanLength())));
    rig.gv->updateFixture(rig.fxi->id());
    QVERIFY2((rig.item->pos() - target).manhattanLength() < qreal(rig.gv->m_cellPixels),
             qPrintable(QString("attach then snapped back %1 px on re-place")
                        .arg((rig.item->pos() - target).manhattanLength())));

    props->removeTruss(t->id());
}

void Monitor_Test::dropOffTrussDetachesAndStays()
{
    DropRig rig;
    QVERIFY(rig.build(m_doc, QVector3D(5000, 5000, 0)));
    rig.gv->setViewPOV(MonitorGraphicsView::PovTop);

    MonitorProperties *props = m_doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("DropBar2");
    t->setOrigin(QVector3D(10.0f, 12.0f, 4.0f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(6.0f);
    t->setWidth(0.3f);
    rig.gv->updateTrusses();

    FixtureRigProps rp = props->fixtureRigProps(rig.fxi->id());
    rp.trussId = t->id();
    rp.trussOffset = 2.0f;
    props->setFixtureRigProps(rig.fxi->id(), rp);
    rig.item->setBoundToTruss(true);
    rig.gv->updateFixture(rig.fxi->id());

    // Drag it 3 m off the truss line -- far beyond the two-widths rule.
    qreal jump = 0, back = 0;
    dropAndMeasure(rig.gv, rig.item, rig.fxi->id(), QPointF(0, 3.0 * rig.gv->m_cellPixels),
                   jump, back);

    const FixtureRigProps after = props->fixtureRigProps(rig.fxi->id());
    QVERIFY2(after.trussId == Truss::invalidId(),
             "pulled well clear of the truss but still bound");
    QVERIFY2(jump < 2.0, qPrintable(QString("detach jumped %1 px at the drop").arg(jump)));
    QVERIFY2(back < 2.0, qPrintable(QString("detach snapped back %1 px on re-place").arg(back)));

    props->removeTruss(t->id());
}

void Monitor_Test::nudgeOffTrussKeepsBindingAndOffset()
{
    DropRig rig;
    QVERIFY(rig.build(m_doc, QVector3D(5000, 5000, 0)));
    rig.gv->setViewPOV(MonitorGraphicsView::PovTop);

    MonitorProperties *props = m_doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("NudgeBar");
    t->setOrigin(QVector3D(10.0f, 12.0f, 4.0f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(6.0f);
    t->setWidth(0.3f);
    rig.gv->updateTrusses();

    FixtureRigProps rp = props->fixtureRigProps(rig.fxi->id());
    rp.trussId = t->id();
    rp.trussOffset = 2.0f;
    rp.trussCross = 0.0f;
    props->setFixtureRigProps(rig.fxi->id(), rp);
    rig.item->setBoundToTruss(true);
    rig.gv->updateFixture(rig.fxi->id());

    // Nudge 0.45 m downstage -- 1.5 truss widths, inside the two-width zone.
    const qreal nudgePx = 0.45 * rig.gv->m_cellPixels;   // 1 cell = 1 m here
    qreal jump = 0, back = 0;
    dropAndMeasure(rig.gv, rig.item, rig.fxi->id(), QPointF(0, nudgePx), jump, back);

    const FixtureRigProps after = props->fixtureRigProps(rig.fxi->id());
    QVERIFY2(after.trussId == t->id(),
             "a nudge inside the zone must stay attached");
    QVERIFY2(after.trussCross > 0.3f && after.trussCross < 0.6f,
             qPrintable(QString("trussCross should be ~0.45m, is %1").arg(after.trussCross)));
    QVERIFY2(jump < 2.0, qPrintable(QString("nudge jumped %1 px at the drop").arg(jump)));
    QVERIFY2(back < 2.0, qPrintable(QString("nudge snapped back %1 px on re-place").arg(back)));

    props->removeTruss(t->id());
}

void Monitor_Test::nearTrussStaysFreeAndLockedTrussRefuses()
{
    DropRig rig;
    QVERIFY(rig.build(m_doc, QVector3D(5000, 5000, 0)));
    rig.gv->setViewPOV(MonitorGraphicsView::PovTop);

    MonitorProperties *props = m_doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("BandBar");
    t->setOrigin(QVector3D(10.0f, 12.0f, 4.0f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(6.0f);
    t->setWidth(0.3f);
    rig.gv->updateTrusses();
    rig.gv->updateFixture(rig.fxi->id());

    // Drop 0.45 m off the line: inside the old grab-everything zone, inside
    // the detach zone, but OUTSIDE the attach zone. Near-but-free must exist.
    /* Position by the item's actual ICON centre (iconCentreScene()), not
       sceneBoundingRect()'s -- that one also reserves space for the name
       label, so its centre drifts depending on whether labels happen to be
       showing (WORKFLOW-SUBTLETIES.md #10). The product code judges by the
       icon centre, so the test must place by it too, or the two disagree
       about where the drop actually landed. */
    const QPointF nearPx = rig.gv->realPositionToPixels(12000.0, 12450.0);
    rig.item->setPos(rig.item->pos() + (nearPx - rig.item->iconCentreScene()));
    const QPointF target = rig.item->pos();
    rig.gv->slotFixtureMoved(rig.item);

    FixtureRigProps rp = props->fixtureRigProps(rig.fxi->id());
    QVERIFY2(rp.trussId == Truss::invalidId(),
             "0.45 m off the bar must stay FREE -- that band exists so a "
             "fixture can live near a truss without being grabbed");
    rig.gv->updateFixture(rig.fxi->id());
    QVERIFY2((rig.item->pos() - target).manhattanLength() < 2.0,
             "the near-band drop moved on re-place");

    // Locked truss: drop dead ON the line; must still stay free.
    t->setLocked(true);
    rig.gv->updateTrusses();
    const QPointF onPx = rig.gv->realPositionToPixels(12000.0, 12000.0);
    rig.item->setPos(rig.item->pos() + (onPx - rig.item->iconCentreScene()));
    rig.gv->slotFixtureMoved(rig.item);
    rp = props->fixtureRigProps(rig.fxi->id());
    QVERIFY2(rp.trussId == Truss::invalidId(),
             "a LOCKED truss must not acquire a dropped fixture");

    props->removeTruss(t->id());
}

/****************************************************************************
 * Lighting Studio Editor (StructureStudioView)
 *
 * The plot canvas and the studio editor are two different widgets with two
 * different drag paths; every earlier drop fix landed in MonitorGraphicsView
 * and none of it applies here. The reported case is a VERTICAL truss ("T-2",
 * origin 13.01/4.41 ft, length 10 ft) with one XL-450 bound to it, which the
 * editor refuses to move. These drive the widget's real handlers rather than
 * asserting on the model, so a refusal anywhere in press -> move -> release
 * shows up as a failure.
 ****************************************************************************/

struct StudioRig
{
    Doc *doc = nullptr;
    Truss *truss = nullptr;
    QList<quint32> spares;
    Fixture *fxi = nullptr;
    StructureStudioView *view = nullptr;

    ~StudioRig()
    {
        delete view;
        if (doc != nullptr && fxi != nullptr)
            doc->deleteFixture(fxi->id());
        if (doc != nullptr)
        {
            // Braces required: foreach expands to an if/else, so a braceless
            // if around one is a dangling else.
            foreach (quint32 sid, spares)
                doc->deleteFixture(sid);
        }
        if (doc != nullptr && truss != nullptr)
            doc->monitorProperties()->removeTruss(truss->id());
    }

    bool build(Doc *d, Truss::TrussType type = Truss::Vertical, int spareFixtures = 0)
    {
        doc = d;
        // Burn ids so the probe fixture is NOT id 0 when a test asks for it.
        for (int i = 0; i < spareFixtures; ++i)
        {
            Fixture *sp = new Fixture(d);
            sp->setName(QString("spare%1").arg(i));
            sp->setChannels(1);
            sp->setAddress(quint32(i + 1) * 16);   // distinct: addFixture() rejects overlaps
            if (d->addFixture(sp) == false)
                return false;
            spares << sp->id();
        }
        MonitorProperties *props = d->monitorProperties();

        truss = props->addTruss();
        truss->setName("T-2");
        truss->setType(type);
        truss->setOrigin(QVector3D(3.966f, 1.344f,
                                   type == Truss::Vertical ? 0.0f : 2.0f));
        truss->setDirection(QPointF(1.0, 0.0));
        truss->setLength(3.048f);                            // 10 ft
        truss->setWidth(0.29f);

        fxi = new Fixture(d);
        fxi->setName("XL-450");
        fxi->setChannels(1);
        fxi->setAddress(256);
        if (d->addFixture(fxi) == false)
            return false;

        // fixtureRigPosition() returns a null vector for a fixture with no
        // monitor item, so give it one first (the plot does this on placement).
        props->setFixturePosition(fxi->id(), 0, 0, QVector3D(0, 0, 0));

        FixtureRigProps rp = props->fixtureRigProps(fxi->id());
        rp.trussId = truss->id();
        rp.trussOffset = truss->length() / 2.0f;             // mid-run
        props->setFixtureRigProps(fxi->id(), rp);

        view = new StructureStudioView(d, StructureStudioView::TrussKind, truss->id());
        view->resize(700, 600);
        view->reload();                                      // computes m_scale/m_originPx
        return view->m_scale > 0.0;
    }

    /* The three handlers a real drag runs, in order. Returns the world
       position the fixture ended up at. */
    QVector3D dragTo(const QPointF &px)
    {
        QMouseEvent press(QEvent::MouseButtonPress, view->w2s(pos()), view->w2s(pos()),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        view->mousePressEvent(&press);
        QMouseEvent move(QEvent::MouseMove, px, px, Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        view->mouseMoveEvent(&move);
        QMouseEvent rel(QEvent::MouseButtonRelease, px, px,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        view->mouseReleaseEvent(&rel);
        return pos();
    }

    QVector3D pos() const { return doc->monitorProperties()->fixtureRigPosition(fxi->id()); }
};

void Monitor_Test::studioTrussDragMovesFixture()
{
    StudioRig rig;
    QVERIFY(rig.build(m_doc));

    // Precondition: the editor agrees the fixture is on this truss (this is
    // what fills the dialog's "Fixtures on this object" tree).
    QCOMPARE(rig.view->mountedFixtures().count(), 1);
    QCOMPARE(rig.view->mountedFixtures().first(), rig.fxi->id());

    // The reported fixture is the FIRST one in its workspace, i.e. id 0 --
    // which is a perfectly valid fixture id (Fixture::invalidId() is UINT_MAX).
    // Keep the rig on that id: a 0-means-nothing sentinel anywhere in the
    // press/drag path is exactly what made it unselectable and unmovable.
    QCOMPARE(rig.fxi->id(), quint32(0));

    // A truss opens in Front elevation, where a vertical run is a full-height
    // line -- the plane in which it can actually be slid.
    QCOMPARE(int(rig.view->plane()), int(StructureStudioView::Front));
    rig.view->setLocked(false);

    // Press must find the fixture where the view itself draws it.
    const QPointF startPx = rig.view->w2s(rig.pos());
    QCOMPARE(rig.view->hitTestFixture(startPx), rig.fxi->id());

    // Drag it a metre DOWN the truss (screen-down = -Z in an elevation).
    const QVector3D before = rig.pos();
    const QPointF targetPx = startPx + QPointF(0.0, rig.view->m_scale * 1.0);
    const QVector3D after = rig.dragTo(targetPx);

    QVERIFY2(!qFuzzyCompare(after.z(), before.z()),
             qPrintable(QString("drag did not move the fixture: z stayed %1")
                        .arg(double(before.z()))));
    QVERIFY2(qAbs(double(after.z() - before.z()) + 1.0) < 0.15,
             qPrintable(QString("dragged 1 m down but z moved %1 m")
                        .arg(double(after.z() - before.z()))));
}

void Monitor_Test::studioTrussDragBlockedWhenLocked()
{
    StudioRig rig;
    QVERIFY(rig.build(m_doc));

    // The dialog hands the view its lock state; locked means select-only.
    rig.view->setLocked(true);
    const QVector3D before = rig.pos();
    const QPointF startPx = rig.view->w2s(before);
    const QVector3D after = rig.dragTo(startPx + QPointF(0.0, rig.view->m_scale));
    QCOMPARE(after, before);
}

void Monitor_Test::studioTrussDragBlockedWhenTrussLocked()
{
    /* "if I have a fixture tied to a truss and the truss is locked in the
       editor .. I can still move it from the studio view." The dialog's own
       lock (above) is a throwaway UI toggle; a truss LOCKED (Layers panel,
       or the truss's own context menu) is saved state, and has to stop a
       drag here too, even with this view's own lock off. */
    StudioRig rig;
    QVERIFY(rig.build(m_doc));
    rig.view->setLocked(false);
    rig.truss->setLocked(true);

    const QVector3D before = rig.pos();
    const QPointF startPx = rig.view->w2s(before);
    QVERIFY2(rig.view->dragFixtureTo(rig.fxi->id(), startPx + QPointF(0.0, rig.view->m_scale)) == false,
             "dragFixtureTo() moved a fixture mounted on a locked truss");
    const QVector3D afterDirect = rig.pos();
    QCOMPARE(afterDirect, before);

    const QVector3D afterMouse = rig.dragTo(startPx + QPointF(0.0, rig.view->m_scale));
    QCOMPARE(afterMouse, before);

    // Unlocking the truss (not this view) lets the same drag through.
    rig.truss->setLocked(false);
    const QVector3D afterUnlocked = rig.dragTo(startPx + QPointF(0.0, rig.view->m_scale));
    QVERIFY2(!qFuzzyCompare(afterUnlocked.z(), before.z()),
             "unlocking the truss did not free the fixture it carries");
}

void Monitor_Test::studioTrussDragInEveryPlane()
{
    StudioRig rig;
    QVERIFY(rig.build(m_doc));
    rig.view->setLocked(false);

    // "In any view I should be able to select any unlocked fixture and move it
    // in the plane of the view." For a VERTICAL truss, Top sees the run
    // end-on -- there is no axis to slide along there -- but both elevations
    // must work.
    const StructureStudioView::Plane elevations[2] =
        { StructureStudioView::Front, StructureStudioView::Side };

    for (int i = 0; i < 2; ++i)
    {
        rig.view->setPlane(elevations[i]);
        const QVector3D before = rig.pos();
        const QPointF startPx = rig.view->w2s(before);
        QVERIFY2(rig.view->hitTestFixture(startPx) == rig.fxi->id(),
                 qPrintable(QString("plane %1: fixture not grabbable at its "
                                    "own drawn position").arg(i)));
        const QVector3D after = rig.dragTo(startPx + QPointF(0.0, rig.view->m_scale * 0.5));
        QVERIFY2(!qFuzzyCompare(after.z(), before.z()),
                 qPrintable(QString("plane %1: drag refused").arg(i)));
    }
}

void Monitor_Test::studioTrussDragAcrossTheRun()
{
    StudioRig rig;
    QVERIFY(rig.build(m_doc));
    rig.view->setLocked(false);

    // Top view of a VERTICAL truss: the run points into the screen, so there is
    // no along-axis slide -- but the across-the-truss freedom is in plane, and
    // the operator's rule is that an unlocked fixture moves in every view.
    rig.view->setPlane(StructureStudioView::Top);
    const QVector3D before = rig.pos();
    const QPointF startPx = rig.view->w2s(before);
    QCOMPARE(rig.view->hitTestFixture(startPx), rig.fxi->id());

    // 10 cm of sideways travel: inside the two-widths band, so it stays bound.
    const QVector3D after = rig.dragTo(startPx + QPointF(rig.view->m_scale * 0.1, 0.0));
    QVERIFY2(!qFuzzyCompare(after.x(), before.x()),
             "top view of a vertical truss refused to move the fixture at all");
    QCOMPARE(m_doc->monitorProperties()->fixtureRigProps(rig.fxi->id()).trussId,
             rig.truss->id());
    QVERIFY2(qAbs(double(after.z() - before.z())) < 1e-4,
             "sideways drag changed the height too");
}

void Monitor_Test::studioHorizontalTrussDragMovesFixture()
{
    StudioRig rig;
    QVERIFY(rig.build(m_doc, Truss::Horizontal, 3));
    QVERIFY2(rig.fxi->id() != 0, "rig meant to test a NON-zero fixture id");
    rig.view->setLocked(false);

    // Front elevation of a horizontal run: the axis is on screen (X), so a
    // horizontal drag slides it along the bar.
    rig.view->setPlane(StructureStudioView::Front);
    const QVector3D before = rig.pos();
    const QPointF startPx = rig.view->w2s(before);
    QCOMPARE(rig.view->hitTestFixture(startPx), rig.fxi->id());
    const QVector3D after = rig.dragTo(startPx + QPointF(rig.view->m_scale * 0.5, 0.0));
    QVERIFY2(qAbs(double(after.x() - before.x()) - 0.5) < 0.1,
             qPrintable(QString("dragged 0.5 m along the bar but x moved %1 m")
                        .arg(double(after.x() - before.x()))));

    // Top view: across the run (Y) is in plane here, so it can be nudged off
    // the chord without losing the binding.
    rig.view->setPlane(StructureStudioView::Top);
    const QVector3D b2 = rig.pos();
    const QVector3D a2 = rig.dragTo(rig.view->w2s(b2) + QPointF(0.0, rig.view->m_scale * 0.1));
    QVERIFY2(!qFuzzyCompare(a2.y(), b2.y()), "top view refused the across-the-run nudge");
    QCOMPARE(m_doc->monitorProperties()->fixtureRigProps(rig.fxi->id()).trussId,
             rig.truss->id());
}

void Monitor_Test::studioTrussDragSetsDropInElevation()
{
    StudioRig rig;
    QVERIFY(rig.build(m_doc, Truss::Horizontal));
    rig.view->setLocked(false);
    rig.view->setPlane(StructureStudioView::Front);

    // Straight down, 0.4 m: the run has no vertical freedom, so this is the
    // drop -- the fixture hangs that far under the bar and stays bound.
    const QVector3D before = rig.pos();
    const QVector3D after =
        rig.dragTo(rig.view->w2s(before) + QPointF(0.0, rig.view->m_scale * 0.4));

    QVERIFY2(qAbs(double(after.z() - before.z()) + 0.4) < 0.05,
             qPrintable(QString("dragged 0.4 m down but z moved %1 m")
                        .arg(double(after.z() - before.z()))));
    QVERIFY2(qAbs(double(after.x() - before.x())) < 1e-4,
             "a purely vertical drag slid it along the bar too");
    QCOMPARE(m_doc->monitorProperties()->fixtureRigProps(rig.fxi->id()).trussId,
             rig.truss->id());
}

void Monitor_Test::studioZoomAtKeepsCursorPointFixed()
{
    /* "pinch to zoom out and reverse to zoom out isn't working consistently."
       StructureStudioView is a plain QWidget, not a QGraphicsView, so unlike
       the main 2D plot it never got a NativeGesture/ZoomNativeGesture handler
       -- real trackpad pinch here worked only by whatever accident of
       platform wheel-event translation was happening that day. zoomAt() is
       the one anchor-math both the scroll wheel and the new pinch handler now
       call, so proving it here proves both callers at once without having to
       construct a QNativeGestureEvent (its constructor is platform/Qt-version
       shaped and not worth fighting for a test). */
    StudioRig rig;
    QVERIFY(rig.build(m_doc, Truss::Horizontal));

    const double before = rig.view->m_scale;
    const QPointF anchor(137.0, 219.0);              // arbitrary on-canvas point
    const QPointF planeBefore = rig.view->screenToPlane(anchor);

    // Zoom IN: scale grows, and the plane point under the anchor is unmoved.
    rig.view->zoomAt(anchor, before * 1.5);
    QVERIFY2(rig.view->m_scale > before,
             qPrintable(QString("zoom in did not grow scale: %1 -> %2")
                        .arg(before).arg(rig.view->m_scale)));
    const QPointF planeAfterIn = rig.view->screenToPlane(anchor);
    QVERIFY2((planeAfterIn - planeBefore).manhattanLength() < 1e-6,
             "zooming in moved the point under the cursor");

    // Zoom back OUT past the start: scale shrinks, anchor still fixed. This is
    // the exact "reverse to zoom out" direction the report named explicitly.
    const double afterIn = rig.view->m_scale;
    rig.view->zoomAt(anchor, afterIn / 3.0);
    QVERIFY2(rig.view->m_scale < afterIn,
             qPrintable(QString("zoom out did not shrink scale: %1 -> %2")
                        .arg(afterIn).arg(rig.view->m_scale)));
    const QPointF planeAfterOut = rig.view->screenToPlane(anchor);
    QVERIFY2((planeAfterOut - planeBefore).manhattanLength() < 1e-6,
             "zooming out moved the point under the cursor");

    // Clamped, not merely bounded silently wrong: asking for an absurd target
    // lands exactly on the documented [4, 4000] ends, not somewhere short of
    // them or unbounded.
    rig.view->zoomAt(anchor, 1.0);
    QCOMPARE(rig.view->m_scale, 4.0);
    rig.view->zoomAt(anchor, 1.0e9);
    QCOMPARE(rig.view->m_scale, 4000.0);
}

void Monitor_Test::updateFixtureSurvivesUnplacedFixture()
{
    QWidget parent;
    Monitor mon(&parent, m_doc);
    MonitorGraphicsView *gv = mon.findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    gv->resize(1230, 760);
    gv->setGridMetrics(1000.0);
    gv->setGridSize(QSize(40, 24));

    // A fixture that exists in the Doc but was never placed on the plot --
    // exactly what a truss-mounted fixture in the studio editor can be.
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Unplaced");
    fxi->setChannels(1);
    fxi->setAddress(400);
    QVERIFY(m_doc->addFixture(fxi));
    const quint32 fid = fxi->id();
    QVERIFY(gv->m_fixtures.contains(fid) == false);

    // Any of these reads used to be a hidden INSERT of {fid, nullptr}.
    QCOMPARE(gv->removeFixture(fid), false);
    gv->setFixtureGelColor(fid, QColor(Qt::red));
    gv->setFixtureRotation(fid, 90);
    gv->fixtureGelColor(fid);

    QVERIFY2(gv->m_fixtures.contains(fid) == false,
             "a lookup for an unplaced fixture inserted a null item into m_fixtures");

    // The crash: contains() passes, the null goes to item->setSize().
    gv->updateFixture(fid);

    m_doc->deleteFixture(fid);
}

void Monitor_Test::studioEditorDialogDragDoesNotCrash()
{
    MonitorProperties *props = m_doc->monitorProperties();

    Truss *t = props->addTruss();
    t->setName("T-2");
    t->setType(Truss::Vertical);
    t->setOrigin(QVector3D(3.966f, 1.344f, 0.0f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(3.048f);
    t->setWidth(0.29f);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("XL-450");
    fxi->setChannels(1);
    fxi->setAddress(300);
    QVERIFY(m_doc->addFixture(fxi));
    const quint32 fid = fxi->id();

    // Mounted on the truss and given a monitor item, but deliberately NOT added
    // to the 2D plot -- the state that made updateFixture() dereference null.
    props->setFixturePosition(fid, 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp = props->fixtureRigProps(fid);
    rp.trussId = t->id();
    rp.trussOffset = t->length() / 2.0f;
    props->setFixtureRigProps(fid, rp);

    QWidget parent;
    Monitor *mon = new Monitor(&parent, m_doc);
    MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);

    /* Poison the plot's item map the way a real session does. removeFixture()
       drops the fixture's plot item and its monitor-properties entry, but NOT
       the fixture itself -- it stays alive in the Doc. A SECOND call for the
       same id (easy to reach: deleting a truss runs removeFixture() over
       fixturesOnFeature(), read from the rig props, and several other paths
       call it too) then hit `m_fixtures[id]` on a missing key. That INSERTS
       {id, nullptr}, so contains() went on saying yes for a live fixture and
       updateFixture() dereferenced the null. */
    QCOMPARE(gv->removeFixture(fid), true);    // the real one
    QCOMPARE(gv->removeFixture(fid), false);   // the one that used to poison
    QVERIFY2(gv->m_fixtures.contains(fid) == false,
             "a second removeFixture() poisoned m_fixtures with a null item");
    QVERIFY2(m_doc->fixture(fid) != nullptr, "the fixture must still be alive");

    // slotEditTruss() blocks in exec(); drive the drag from inside that loop.
    QTimer::singleShot(0, mon, [mon, fid]() {
        StructureStudioView *view = nullptr;
        QDialog *found = nullptr;
        foreach (QWidget *w, QApplication::topLevelWidgets())
        {
            if (QDialog *d = qobject_cast<QDialog *>(w))
                if ((view = d->findChild<StructureStudioView *>()) != nullptr)
                { found = d; break; }
        }
        if (found != nullptr)
        {
            view->setLocked(false);
            const QPointF start =
                view->w2s(view->m_doc->monitorProperties()->fixtureRigPosition(fid));
            const QPointF end = start + QPointF(0.0, 40.0);
            QMouseEvent press(QEvent::MouseButtonPress, start, start, Qt::LeftButton,
                              Qt::LeftButton, Qt::NoModifier);
            QMouseEvent move(QEvent::MouseMove, end, end, Qt::NoButton,
                             Qt::LeftButton, Qt::NoModifier);
            QMouseEvent rel(QEvent::MouseButtonRelease, end, end, Qt::LeftButton,
                            Qt::NoButton, Qt::NoModifier);
            // The release is the one that emits fixtureMoved().
            view->mousePressEvent(&press);
            view->mouseMoveEvent(&move);
            view->mouseReleaseEvent(&rel);
        }
        // Close BEFORE the QFAIL below: if no dialog carrying a
        // StructureStudioView turned up, some OTHER modal might still be
        // open (or this one, without having been found the expected way) --
        // and nothing else will ever close it under offscreen QTest. See
        // WORKFLOW-SUBTLETIES.md #7.
        if (found != nullptr)
            found->reject();
        else if (QWidget *modal = QApplication::activeModalWidget())
            modal->close();
        QVERIFY2(found != nullptr, "no StructureStudioView in any open dialog");
    });
    mon->slotEditTruss(t->id());   // reaching here at all means it did not crash

    delete mon;
    m_doc->deleteFixture(fid);
    props->removeTruss(t->id());
}

void Monitor_Test::mountedFixtureWithoutPlotItemIsPositionedAndMovable()
{
    MonitorProperties *props = m_doc->monitorProperties();

    Truss *t = props->addTruss();
    t->setName("T-2");
    t->setType(Truss::Vertical);
    t->setOrigin(QVector3D(3.965f, 1.344f, 0.0f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(3.048f);
    t->setWidth(0.290f);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("XL-450");
    fxi->setChannels(1);
    fxi->setAddress(320);
    QVERIFY(m_doc->addFixture(fxi));
    const quint32 fid = fxi->id();

    // The workspace's actual state: rigged on the truss, no plot item at all.
    // Built from a DEFAULT FixtureRigProps, not the stored one: ids are reused
    // between tests, so reading the existing entry inherits whatever a
    // previous fixture on this id left behind (mountZOffset especially).
    FixtureRigProps rp;
    rp.trussId     = t->id();
    rp.trussOffset = 2.396f;
    rp.trussCross  = 0.093f;
    props->setFixtureRigProps(fid, rp);

    /* Setting a mount now CREATES the plot item (a mounted fixture with none is
       invisible in both the rig view and the Layers tree), so the orphan has to
       be built directly: drop the ITEM and keep the rig props. removeFixture()
       is no good here -- it deliberately clears the rig props too, precisely so
       a reused fixture id cannot inherit a stale mount.

       This is the state stage-structures-demo.qxw shipped with, and what this
       test is for: the position must still come from the mount rather than
       reading (0,0,0), and the fixture must still be draggable. */
    props->m_fixtureItems.remove(fid);
    QVERIFY2(props->fixtureItemsID().contains(fid) == false,
             "rig meant to model a fixture with NO monitor item");

    // The mount alone fixes where it is -- it must NOT read as the origin.
    const QVector3D w = props->fixtureRigPosition(fid);
    QVERIFY2(!w.isNull(), "a truss-mounted fixture with no plot item read as (0,0,0)");
    QVERIFY2(qAbs(double(w.z() - 2.396)) < 0.2,
             qPrintable(QString("expected it up the truss, got z=%1").arg(double(w.z()))));
    QVERIFY2(qAbs(double(w.x() - 3.965 - 0.093)) < 0.01,
             qPrintable(QString("expected it across at the truss, got x=%1").arg(double(w.x()))));

    // And it must be draggable in the studio editor, which is what "cannot move
    // it" was: the drag wrote rig props that the position never reflected.
    StructureStudioView view(m_doc, StructureStudioView::TrussKind, t->id());
    view.resize(700, 600);
    view.reload();
    view.setLocked(false);
    view.setPlane(StructureStudioView::Front);
    QCOMPARE(view.mountedFixtures().count(), 1);

    const QPointF startPx = view.w2s(w);
    QCOMPARE(view.hitTestFixture(startPx), fid);
    const QPointF endPx = startPx + QPointF(0.0, view.m_scale * 0.5);
    QMouseEvent press(QEvent::MouseButtonPress, startPx, startPx, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QMouseEvent move(QEvent::MouseMove, endPx, endPx, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent rel(QEvent::MouseButtonRelease, endPx, endPx, Qt::LeftButton,
                    Qt::NoButton, Qt::NoModifier);
    view.mousePressEvent(&press);
    view.mouseMoveEvent(&move);
    view.mouseReleaseEvent(&rel);

    const QVector3D after = props->fixtureRigPosition(fid);
    QVERIFY2(qAbs(double(after.z() - w.z()) + 0.5) < 0.1,
             qPrintable(QString("dragged 0.5 m down but z moved %1 m")
                        .arg(double(after.z() - w.z()))));

    m_doc->deleteFixture(fid);
    props->removeTruss(t->id());
}

void Monitor_Test::declaredHeadLayoutIsUsed()
{
    // A definition shaped like the real XL-450: 15 columns x 5 rows of heads,
    // 401 x 180 mm. Aspect alone (401/180 = 2.2) would put every head in a
    // grid of its own choosing; the declaration must win.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Warmdance");
    def->setModel("XL-450");
    def->setType(QLCFixtureDef::LEDBarPixels);

    const int cols = 15, rowsDeclared = 5;
    for (int i = 0; i < cols * rowsDeclared; ++i)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("Dimmer %1").arg(i));
        ch->setGroup(QLCChannel::Intensity);
        def->addChannel(ch);
    }

    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Pixel");
    QLCPhysical phys;
    phys.setWidth(401);
    phys.setHeight(180);
    phys.setDepth(100);
    phys.setLayoutSize(QSize(cols, rowsDeclared));
    mode->setPhysical(phys);
    foreach (QLCChannel *ch, def->channels())
    {
        mode->insertChannel(ch, mode->channels().size());
        QLCFixtureHead head;
        head.addChannel(mode->channels().size() - 1);
        mode->insertHead(-1, head);
    }
    def->addMode(mode);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("XL-450");
    fxi->setFixtureDefinition(def, mode);
    fxi->setAddress(0);
    fxi->setUniverse(3);
    QVERIFY(m_doc->addFixture(fxi));
    QCOMPARE(fxi->heads(), cols * rowsDeclared);

    QWidget parent;
    Monitor mon(&parent, m_doc);
    MonitorGraphicsView *gv = mon.findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    gv->resize(1230, 760);
    gv->setGridMetrics(1000.0);
    gv->setGridSize(QSize(40, 24));
    m_doc->monitorProperties()->setFixturePosition(fxi->id(), 0, 0, QVector3D(2000, 2000, 0));
    gv->addFixture(fxi->id(), QPointF(2000, 2000));

    MonitorFixtureItem *item = gv->m_fixtures.value(fxi->id(), nullptr);
    QVERIFY(item != nullptr);

    /* Read the layout back off the placed heads: count the distinct head
       top-edges (rows) and left-edges (columns). Comparing geometry rather
       than an internal counter keeps this honest about what is DRAWN. */
    QSet<int> tops, lefts;
    foreach (FixtureHead *h, item->m_heads)
    {
        const QRectF r = h->m_item->boundingRect();
        tops  << qRound(r.top());
        lefts << qRound(r.left());
    }
    QCOMPARE(tops.count(), rowsDeclared);
    QCOMPARE(lefts.count(), cols);

    m_doc->deleteFixture(fxi->id());
}


void Monitor_Test::verticalTrussMovesLaterallyInSideView()
{
    StudioRig rig;
    QVERIFY(rig.build(m_doc));           // vertical truss by default
    rig.view->setLocked(false);

    // SIDE: horizontal screen axis is Y (downstage <- -> upstage).
    rig.view->setPlane(StructureStudioView::Side);
    const QVector3D before = rig.pos();
    const QPointF startPx = rig.view->w2s(before);
    QCOMPARE(rig.view->hitTestFixture(startPx), rig.fxi->id());

    const QVector3D after = rig.dragTo(startPx + QPointF(rig.view->m_scale * 0.1, 0.0));
    QVERIFY2(!qFuzzyCompare(after.y(), before.y()),
             "side view refused to move a tower-mounted fixture laterally (Y)");
    QVERIFY2(qAbs(double(after.x() - before.x())) < 1e-4,
             "a lateral side-view drag also moved it in X");
    QCOMPARE(m_doc->monitorProperties()->fixtureRigProps(rig.fxi->id()).trussId,
             rig.truss->id());

    // FRONT still drives the OTHER horizontal (X) and must not disturb Y.
    rig.view->setPlane(StructureStudioView::Front);
    const QVector3D b2 = rig.pos();
    const QVector3D a2 = rig.dragTo(rig.view->w2s(b2) + QPointF(rig.view->m_scale * 0.1, 0.0));
    QVERIFY2(!qFuzzyCompare(a2.x(), b2.x()), "front view refused the lateral (X) drag");
    QVERIFY2(qAbs(double(a2.y() - b2.y())) < 1e-4,
             "a front-view drag disturbed the side view's Y offset");
}


void Monitor_Test::fixtureIsBoxedByItsRealDimensionsInEveryView()
{
    // An XL-450: 401 x 180 x 100 mm, 15 x 5 pixels, front-facing on a tower.
    const double W = 0.401, H = 0.180, D = 0.100;
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Warmdance"); def->setModel("XL-450");
    def->setType(QLCFixtureDef::LEDBarPixels);
    for (int i = 0; i < 75; ++i)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("D%1").arg(i));
        ch->setGroup(QLCChannel::Intensity);
        def->addChannel(ch);
    }
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Pixel");
    QLCPhysical ph;
    ph.setWidth(401); ph.setHeight(180); ph.setDepth(100);
    ph.setLayoutSize(QSize(15, 5));
    mode->setPhysical(ph);
    foreach (QLCChannel *ch, def->channels())
    {
        mode->insertChannel(ch, mode->channels().size());
        QLCFixtureHead h; h.addChannel(mode->channels().size() - 1);
        mode->insertHead(-1, h);
    }
    def->addMode(mode);

    MonitorProperties *props = m_doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("T-2"); t->setType(Truss::Vertical);
    t->setOrigin(QVector3D(3.965f, 1.344f, 0.0f)); t->setDirection(QPointF(1.0, 0.0));
    t->setLength(3.048f); t->setWidth(0.290f);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("XL-450");
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(3); fxi->setAddress(0);
    QVERIFY(m_doc->addFixture(fxi));
    props->setFixturePosition(fxi->id(), 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp;
    rp.trussId = t->id(); rp.trussOffset = 1.5f;
    rp.studioMount = 1;                      // front face: long axis X, depth Y
    props->setFixtureRigProps(fxi->id(), rp);

    StructureStudioView view(m_doc, StructureStudioView::TrussKind, t->id());
    view.resize(700, 600);
    view.reload();

    const FixtureVisualTraits traits = classifyFixture(fxi);
    QCOMPARE(traits.layout, QSize(15, 5));   // the declaration reached the renderer

    struct { StructureStudioView::Plane plane; double w, h; const char *name; } cases[] = {
        { StructureStudioView::Top,   W, D, "top"   },
        { StructureStudioView::Front, W, H, "front" },
        { StructureStudioView::Side,  D, H, "side"  },
    };

    for (int i = 0; i < 3; ++i)
    {
        view.setPlane(cases[i].plane);
        QPointF wPx, hPx; QRectF box;
        view.fixtureBoxPx(fxi->id(), traits, wPx, hPx, box);
        const double expW = cases[i].w * view.m_scale;
        const double expH = cases[i].h * view.m_scale;
        QVERIFY2(qAbs(box.width() - expW) < 1.0,
                 qPrintable(QString("%1 view: box is %2 px wide, expected %3")
                            .arg(cases[i].name).arg(box.width()).arg(expW)));
        QVERIFY2(qAbs(box.height() - expH) < 1.0,
                 qPrintable(QString("%1 view: box is %2 px tall, expected %3")
                            .arg(cases[i].name).arg(box.height()).arg(expH)));
    }

    m_doc->deleteFixture(fxi->id());
    props->removeTruss(t->id());
}

void Monitor_Test::structuralMountsAreExclusive()
{
    MonitorProperties *props = m_doc->monitorProperties();

    StagePlatform *pl = props->addPlatform();
    pl->setName("USP2");
    pl->setOriginX(3.661f); pl->setOriginY(1.530f);
    pl->setWidth(2.438f); pl->setDepth(1.219f); pl->setHeight(0.610f);

    Truss *t = props->addTruss();
    t->setName("T-2"); t->setType(Truss::Vertical);
    t->setOrigin(QVector3D(3.965f, 1.344f, 0.0f)); t->setDirection(QPointF(1.0, 0.0));
    t->setLength(3.048f); t->setWidth(0.290f);

    QWidget parent;
    Monitor mon(&parent, m_doc);
    MonitorGraphicsView *gv = mon.findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    gv->resize(1230, 760);
    gv->setGridMetrics(1000.0);
    gv->setGridSize(QSize(40, 24));

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("XL-450"); fxi->setChannels(1); fxi->setAddress(340);
    QVERIFY(m_doc->addFixture(fxi));
    const quint32 fid = fxi->id();
    props->setFixturePosition(fid, 0, 0, QVector3D(4000, 1500, 0));
    gv->addFixture(fid, QPointF(4000, 1500));

    // Standing on the platform, then moved onto the truss -- the sequence that
    // produced "Truss=2 ... Deck=1" in stage-structures-demo.qxw.
    FixtureRigProps rp;
    rp.deckPlatformId = pl->id();
    props->setFixtureRigProps(fid, rp);
    QCOMPARE(props->fixtureRigProps(fid).primaryMount(), FixtureRigProps::DeckMount);

    gv->attachFixtureToTruss(fid, t->id());

    const FixtureRigProps after = props->fixtureRigProps(fid);
    QCOMPARE(after.trussId, t->id());
    QVERIFY2(after.onDeck() == false,
             "attaching to a truss left the previous deck mount in place");
    QCOMPARE(after.primaryMount(), FixtureRigProps::TrussMount);

    /* And the two consumers must agree. The position comes from the truss, so
       whatever the double-click resolves to has to be the truss as well -- that
       disagreement is what opened USP2's editor for a fixture drawn on T-2. */
    const QVector3D w = props->fixtureRigPosition(fid);
    QVERIFY2(qAbs(double(w.x() - t->origin().x())) < 0.5,
             "the fixture is not positioned on the truss it is mounted on");

    m_doc->deleteFixture(fid);
    props->removeTruss(t->id());
    props->removePlatform(pl->id());
}

void Monitor_Test::mountedFixturesToggleHidesOnlyMountedOnes()
{
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("T-2"); t->setType(Truss::Vertical);
    t->setOrigin(QVector3D(3.965f, 1.344f, 0.0f)); t->setDirection(QPointF(1.0, 0.0));
    t->setLength(3.048f); t->setWidth(0.290f);

    QWidget parent;
    Monitor mon(&parent, m_doc);
    MonitorGraphicsView *gv = mon.findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    gv->resize(1230, 760);
    gv->setGridMetrics(1000.0);
    gv->setGridSize(QSize(40, 24));

    Fixture *onRig = new Fixture(m_doc);
    onRig->setName("OnTruss"); onRig->setChannels(1); onRig->setAddress(360);
    QVERIFY(m_doc->addFixture(onRig));
    Fixture *free_ = new Fixture(m_doc);
    free_->setName("FreeStanding"); free_->setChannels(1); free_->setAddress(370);
    QVERIFY(m_doc->addFixture(free_));

    foreach (Fixture *f, QList<Fixture *>() << onRig << free_)
    {
        props->setFixturePosition(f->id(), 0, 0, QVector3D(4000, 1500, 0));
        gv->addFixture(f->id(), QPointF(4000, 1500));
    }
    gv->attachFixtureToTruss(onRig->id(), t->id());

    MonitorFixtureItem *rigItem  = gv->m_fixtures.value(onRig->id(), nullptr);
    MonitorFixtureItem *freeItem = gv->m_fixtures.value(free_->id(), nullptr);
    QVERIFY(rigItem != nullptr && freeItem != nullptr);
    QVERIFY(gv->mountedFixturesVisible());

    /* This fixture may BURY its truss from above -- a wide icon on a thin 3 m
       truss -- and burial hides it regardless of the toggle, standing in a
       count badge instead. Measure rather than assume, so the test keeps
       checking what it is about (mounted hidden, free never hidden) whatever
       the geometry works out to. */
    const bool buries =
        gv->structureCoverageByItsFixtures(QStringLiteral("truss"), t->id()) > 0.40;
    QCOMPARE(rigItem->isVisible(), !buries);

    gv->setMountedFixturesVisible(false);
    QVERIFY2(rigItem->isVisible() == false, "the truss-mounted fixture stayed visible");
    QVERIFY2(freeItem->isVisible(), "a free-standing fixture must never be hidden");

    gv->setMountedFixturesVisible(true);
    QVERIFY2(rigItem->isVisible() == !buries,
             "the mounted fixture did not come back (and is not buried)");
    QVERIFY2(freeItem->isVisible(), "a free-standing fixture must never be hidden");

    m_doc->deleteFixture(onRig->id());
    m_doc->deleteFixture(free_->id());
    props->removeTruss(t->id());
}

void Monitor_Test::viewRotationIsScreenOnlyAndDragStillWorks()
{
    StudioRig rig;
    QVERIFY(rig.build(m_doc));
    rig.view->setPlane(StructureStudioView::Front);
    rig.view->setLocked(false);

    const QVector3D before = rig.pos();

    for (int turns = 0; turns < 4; ++turns)
    {
        rig.view->setRotation(turns);
        QCOMPARE(rig.view->rotation(), turns);

        // Rotating must not move anything in the model.
        QCOMPARE(rig.pos(), before);

        // w2s/screenToPlane must stay exact inverses at every turn, which is
        // what keeps dragging and hit-testing honest.
        const QPointF px = rig.view->w2s(before);
        const QPointF ab = rig.view->screenToPlane(px);
        const QPointF abDirect = rig.view->project(before);
        QVERIFY2(qAbs(ab.x() - abDirect.x()) < 1e-6 && qAbs(ab.y() - abDirect.y()) < 1e-6,
                 qPrintable(QString("turn %1: screenToPlane is not the inverse of w2s")
                            .arg(turns)));

        // And the fixture is still grabbable where it is drawn.
        QVERIFY2(rig.view->hitTestFixture(px) == rig.fxi->id(),
                 qPrintable(QString("turn %1: fixture not grabbable at its drawn position")
                            .arg(turns)));
    }

    /* Drag DOWN the screen at 180 degrees. The truss runs up the screen when
       unrotated, so a downward drag then raises it -- the point being that the
       drag follows the CURSOR, not the world axis. */
    rig.view->setRotation(2);
    const QVector3D b2 = rig.pos();
    const QVector3D a2 = rig.dragTo(rig.view->w2s(b2) + QPointF(0.0, rig.view->m_scale * 0.5));
    QVERIFY2(qAbs(double(a2.z() - b2.z()) - 0.5) < 0.1,
             qPrintable(QString("at 180 degrees a downward drag moved z by %1, expected +0.5")
                        .arg(double(a2.z() - b2.z()))));

    rig.view->setRotation(0);
}

void Monitor_Test::plotViewRotationFitsGridAndKeepsPositions()
{
    DropRig rig;
    QVERIFY(rig.build(m_doc, QVector3D(5000, 5000, 0)));
    MonitorGraphicsView *gv = rig.gv;

    // A deliberately NON-square viewport, so a turn genuinely changes which
    // extent the grid has to fit into.
    gv->resize(1200, 600);
    gv->setGridMetrics(1000.0);
    gv->setGridSize(QSize(40, 24));

    const QVector3D before =
        m_doc->monitorProperties()->fixtureItem(rig.fxi->id(), 0, 0).m_position;
    const int cellUpright = gv->m_cellPixels;
    QVERIFY(cellUpright > 0);

    for (int turns = 0; turns < 4; ++turns)
    {
        gv->setViewRotation(turns);
        QCOMPARE(gv->viewRotation(), turns);

        // The grid must still fit the view it is actually laid out in: 40x24
        // cells in 1200x600 upright, but in 600x1200 on a quarter turn.
        const int fitW = (turns & 1) ? 600 : 1200;
        const int fitH = (turns & 1) ? 1200 : 600;
        const int expect = qMin(fitW / 40, fitH / 24);
        QVERIFY2(gv->m_cellPixels == expect,
                 qPrintable(QString("turn %1: cellPixels %2, expected %3 -- the "
                                    "grid fit ignored the rotation")
                            .arg(turns).arg(gv->m_cellPixels).arg(expect)));

        // Rotating is a VIEW change; nothing in the model may move.
        QCOMPARE(m_doc->monitorProperties()->fixtureItem(rig.fxi->id(), 0, 0).m_position,
                 before);

        // The zoom must survive the turn -- reading it off m11 would give 0 here.
        QVERIFY2(qAbs(gv->viewScale() - 1.0) < 1e-6,
                 qPrintable(QString("turn %1: viewScale reads %2, expected 1.0")
                            .arg(turns).arg(gv->viewScale())));
    }

    gv->setViewRotation(0);
    QCOMPARE(gv->m_cellPixels, cellUpright);
}



void Monitor_Test::angledViewProjectsAndRefusesEdits()
{
    StudioRig rig;
    QVERIFY(rig.build(m_doc, Truss::Horizontal));
    rig.view->setLocked(false);

    const QVector3D p(2.0f, 3.0f, 4.0f);

    /* The angled projection has to DEGENERATE to the flat ones at the angles
       where they are the same camera -- an angled look that disagrees with the
       Front view at zero tilt would make it untrustworthy for judging a rig. */
    rig.view->setPlane(StructureStudioView::Angled);

    rig.view->setAngledView(0.0, 0.0);              // == Front
    QPointF ang = rig.view->project(p);
    rig.view->setPlane(StructureStudioView::Front);
    QPointF flat = rig.view->project(p);
    QVERIFY2(qAbs(ang.x() - flat.x()) < 1e-6 && qAbs(ang.y() - flat.y()) < 1e-6,
             qPrintable(QString("angled(0,0) gave %1,%2 but Front gives %3,%4")
                        .arg(ang.x()).arg(ang.y()).arg(flat.x()).arg(flat.y())));

    rig.view->setPlane(StructureStudioView::Angled);
    rig.view->setAngledView(0.0, 90.0);             // straight down == Top, Y flipped
    ang = rig.view->project(p);
    rig.view->setPlane(StructureStudioView::Top);
    flat = rig.view->project(p);
    QVERIFY2(qAbs(ang.x() - flat.x()) < 1e-6 && qAbs(ang.y() + flat.y()) < 1e-6,
             qPrintable(QString("angled(0,90) gave %1,%2, expected Top's %3,%4 "
                                "with the vertical negated").arg(ang.x()).arg(ang.y())
                        .arg(flat.x()).arg(flat.y())));

    // View-only: a drag must change nothing, at any camera angle.
    rig.view->setPlane(StructureStudioView::Angled);
    rig.view->setAngledView(45.0, 45.0);
    QVERIFY(rig.view->isViewOnly());
    const QVector3D before = rig.pos();
    const QPointF startPx = rig.view->w2s(before);
    QVERIFY2(rig.view->dragFixtureTo(rig.fxi->id(), startPx + QPointF(60, 40)) == false,
             "the angled view accepted a drag -- it has no honest inverse");
    QCOMPARE(rig.dragTo(startPx + QPointF(60, 40)), before);

    // ...but the flat views still edit normally afterwards.
    rig.view->setPlane(StructureStudioView::Front);
    QVERIFY(!rig.view->isViewOnly());
    const QVector3D after = rig.dragTo(rig.view->w2s(rig.pos()) + QPointF(rig.view->m_scale * 0.5, 0.0));
    QVERIFY2(!qFuzzyCompare(after.x(), before.x()),
             "the flat view stopped editing after visiting the angled one");
}

void Monitor_Test::stageOverviewDrawsEveryStructure()
{
    MonitorProperties *props = m_doc->monitorProperties();

    Truss *t = props->addTruss();
    t->setName("OV Truss"); t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(1.0f, 1.5f, 3.5f)); t->setDirection(QPointF(1.0, 0.0));
    t->setLength(6.0f); t->setWidth(0.3f);
    StagePlatform *pl = props->addPlatform();
    pl->setName("OV Deck"); pl->setOriginX(3.0f); pl->setOriginY(5.0f);
    pl->setWidth(2.4f); pl->setDepth(1.2f); pl->setHeight(0.6f);
    Tower *tw = props->addTower();
    tw->setName("OV Tower"); tw->setOriginX(9.0f); tw->setOriginY(2.0f);
    tw->setWidth(0.5f); tw->setDepth(0.5f); tw->setHeight(3.0f);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("OV PAR"); fxi->setChannels(1); fxi->setAddress(380);
    QVERIFY(m_doc->addFixture(fxi));
    props->setFixturePosition(fxi->id(), 0, 0, QVector3D(2000, 1500, 0));
    FixtureRigProps rp; rp.trussId = t->id(); rp.trussOffset = 2.0f;
    props->setFixtureRigProps(fxi->id(), rp);

    StructureStudioView view(m_doc, StructureStudioView::StageKind, 0);
    view.resize(900, 640);
    view.reload();

    // Every structure is enumerated, whatever kind it is.
    const QList<QPair<StructureStudioView::Kind, quint32> > all = view.everyStructure();
    QVERIFY2(all.size() >= 3, "the overview did not find all three structures");

    // The fit has to FRAME the whole rig, not one object: the truss at x=1..7
    // and the tower at x=9 must both land inside the widget.
    view.setPlane(StructureStudioView::Top);
    foreach (const QPointF &px, QList<QPointF>()
             << view.w2s(t->origin())
             << view.w2s(t->positionAt(t->length()))
             << view.w2s(QVector3D(tw->originX(), tw->originY(), 0)))
    {
        QVERIFY2(view.rect().contains(px.toPoint()),
                 qPrintable(QString("the fit left %1,%2 outside the view")
                            .arg(px.x()).arg(px.y())));
    }

    // Fixtures come from the whole plot, not one structure's mount list.
    QVERIFY(view.mountedFixtures().contains(fxi->id()));

    // Read-only: it is an overview, and the angled plane has no inverse anyway.
    view.setPlane(StructureStudioView::Angled);
    QVERIFY(view.isViewOnly());
    const QVector3D before = props->fixtureRigPosition(fxi->id());
    QVERIFY(view.dragFixtureTo(fxi->id(), view.w2s(before) + QPointF(50, 30)) == false);
    QCOMPARE(props->fixtureRigPosition(fxi->id()), before);

    m_doc->deleteFixture(fxi->id());
    props->removeTruss(t->id());
    props->removePlatform(pl->id());
    props->removeTower(tw->id());
}


void Monitor_Test::angledFixturesAreNotBillboards()
{
    StudioRig rig;
    QVERIFY(rig.build(m_doc, Truss::Horizontal));
    rig.view->setPlane(StructureStudioView::Angled);

    const FixtureVisualTraits traits = classifyFixture(m_doc->fixture(rig.fxi->id()));

    /* The screen direction of the fixture's own width axis. A billboard keeps
       this fixed however the camera swings -- that is exactly what made the
       moving heads appear to follow the viewer around the rig. */
    auto widthAngleAt = [&](double az, double el) {
        rig.view->setAngledView(az, el);
        QVector3D c[8];
        rig.view->fixtureBoxCorners(rig.fxi->id(), traits, c);
        const QPointF a = rig.view->w2s(c[0]);
        const QPointF b = rig.view->w2s(c[1]);
        return qRadiansToDegrees(qAtan2(b.y() - a.y(), b.x() - a.x()));
    };

    const double a0 = widthAngleAt(0.0, 30.0);
    const double a1 = widthAngleAt(60.0, 30.0);
    QVERIFY2(qAbs(a1 - a0) > 2.0,
             qPrintable(QString("the fixture drew at %1 deg from both cameras — "
                                "it is still a billboard").arg(a0)));

    // And the box has real depth: swinging the camera must change its extent.
    auto widthPxAt = [&](double az) {
        rig.view->setAngledView(az, 30.0);
        QVector3D c[8];
        rig.view->fixtureBoxCorners(rig.fxi->id(), traits, c);
        const QPointF a = rig.view->w2s(c[0]);
        const QPointF b = rig.view->w2s(c[1]);
        return QLineF(a, b).length();
    };
    QVERIFY2(qAbs(widthPxAt(0.0) - widthPxAt(75.0)) > 1.0,
             "the fixture kept the same on-screen width at every angle");
}


void Monitor_Test::angledOverviewDrawsNearThingsInFront()
{
    MonitorProperties *props = m_doc->monitorProperties();

    /* viewDepth's contract, which the sort depends on: LARGER is NEARER.
       +Y is downstage, so with the eye downstage looking upstage a bigger Y is
       closer. The sort once ran the other way and far things painted over near
       ones -- a step covered the tower standing in front of it. */
    StructureStudioView probe(m_doc, StructureStudioView::StageKind, 0);
    probe.setPlane(StructureStudioView::Angled);
    probe.setAngledView(0.0, 0.0);
    QVERIFY2(probe.viewDepth(QVector3D(0, 10, 0)) > probe.viewDepth(QVector3D(0, 0, 0)),
             "viewDepth: larger must mean nearer, or the painter's sort inverts");

    /* Two SOLID decks, one downstage of the other and overlapping on screen.
       Deliberately not a tower any more: towers draw as open lattice now, so
       you can legitimately see the scenery through one and sampling its middle
       says nothing about ordering. */
    StagePlatform *far_ = props->addPlatform();
    far_->setName("Far"); far_->setOriginX(0.0f); far_->setOriginY(0.0f);
    far_->setWidth(6.0f); far_->setDepth(2.0f); far_->setHeight(2.0f);
    far_->setColor(QColor(60, 110, 230));                // saturated blue

    StagePlatform *near_ = props->addPlatform();
    near_->setName("Near"); near_->setOriginX(1.0f); near_->setOriginY(3.0f);
    near_->setWidth(4.0f); near_->setDepth(1.5f); near_->setHeight(2.0f);
    near_->setColor(QColor(235, 235, 235));              // near-white, unsaturated

    StructureStudioView ov(m_doc, StructureStudioView::StageKind, 0);
    ov.resize(800, 560);
    ov.reload();
    ov.setPlane(StructureStudioView::Angled);
    ov.setAngledView(0.0, 35.0);                         // straight on, looking down
    ov.setAmbient(1.0);                                  // full work light, no dimming
    const QImage img = ov.grab().toImage();

    /* Sample a point that both decks cover on screen: the near deck's top, which
       from this camera sits in front of the far deck's body. Which one won is
       readable from saturation alone.
     *
     * w2s() is logical pixels; grab() on a HiDPI screen returns device pixels
     * (devicePixelRatio() times as many) -- scale up to match img's indexing. */
    const QPointF at = ov.w2s(QVector3D(3.0f, 3.7f, 2.0f)) * img.devicePixelRatio();
    QVERIFY2(img.rect().contains(at.toPoint()), "the sample point fell outside the view");
    const QColor px = img.pixelColor(at.toPoint());
    QVERIFY2(px.saturation() < 110,
             qPrintable(QString("the far deck painted over the near one "
                                "(sampled %1,%2,%3, saturation %4)")
                        .arg(px.red()).arg(px.green()).arg(px.blue())
                        .arg(px.saturation())));

    props->removePlatform(far_->id());
    props->removePlatform(near_->id());
}

void Monitor_Test::ambientLevelDimsTheWholeView()
{
    MonitorProperties *props = m_doc->monitorProperties();
    StagePlatform *pl = props->addPlatform();
    pl->setName("Bright Deck"); pl->setOriginX(0.0f); pl->setOriginY(0.0f);
    pl->setWidth(5.0f); pl->setDepth(3.0f); pl->setHeight(0.6f);
    pl->setColor(QColor(230, 40, 40));

    StructureStudioView v(m_doc, StructureStudioView::StageKind, 0);
    v.resize(700, 500);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(20.0, 30.0);

    const QPointF mid = v.w2s(QVector3D(2.5f, 1.5f, 0.6f));

    auto lumaAt = [&](double ambient) {
        v.setAmbient(ambient);
        const QImage img = v.grab().toImage();
        const QColor c = img.pixelColor(mid.toPoint());
        return c.red() * 0.30 + c.green() * 0.59 + c.blue() * 0.11;
    };

    const double work = lumaAt(1.0);
    const double dim  = lumaAt(0.30);
    const double dark = lumaAt(0.0);

    QVERIFY2(work > dim && dim > dark,
             qPrintable(QString("room level did not dim the view: work %1, dim %2, "
                                "blackout %3").arg(work).arg(dim).arg(dark)));
    QVERIFY2(work - dark > 30.0,
             qPrintable(QString("work light and blackout are nearly identical "
                                "(%1 vs %2)").arg(work).arg(dark)));

    props->removePlatform(pl->id());
}

void Monitor_Test::litFixturesKeepTheirBrightnessInABlackout()
{
    MonitorProperties *props = m_doc->monitorProperties();

    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Blackout Par");
    def->setType(QLCFixtureDef::ColorChanger);
    const char *nm[] = { "Dimmer", "Red", "Green", "Blue" };
    const QLCChannel::PrimaryColour pc[] = { QLCChannel::NoColour, QLCChannel::Red,
                                             QLCChannel::Green, QLCChannel::Blue };
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("4ch");
    for (int c = 0; c < 4; ++c)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(nm[c]); ch->setGroup(QLCChannel::Intensity);
        if (pc[c] != QLCChannel::NoColour) ch->setColour(pc[c]);
        def->addChannel(ch);
        mode->insertChannel(ch, c);
    }
    QLCPhysical ph;
    ph.setWidth(600); ph.setHeight(600); ph.setDepth(600);
    mode->setPhysical(ph);
    QLCFixtureHead head;
    for (int c = 0; c < 4; ++c) head.addChannel(c);
    mode->insertHead(-1, head);
    def->addMode(mode);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Blackout Par"); fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(3); fxi->setAddress(300);
    QVERIFY(m_doc->addFixture(fxi));

    /* Mounted on the front face of a plain dark deck, so the fixture is the
       only bright thing anywhere near the pixels we sample. */
    StagePlatform *pl = props->addPlatform();
    pl->setName("Blackout Deck"); pl->setOriginX(0.0f); pl->setOriginY(0.0f);
    pl->setWidth(5.0f); pl->setDepth(3.0f); pl->setHeight(1.5f);
    pl->setColor(QColor(60, 60, 60));

    props->setFixturePosition(fxi->id(), 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp;
    rp.riserPlatformId = pl->id();
    rp.riserFace = 0;               // downstage face
    rp.riserU = 2.5f;
    rp.riserV = 0.8f;
    props->setFixtureRigProps(fxi->id(), rp);

    StructureStudioView v(m_doc, StructureStudioView::StageKind, 0);
    v.resize(700, 500);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(20.0, 25.0);
    v.setLiveValues(true);
    v.setAmbient(0.0);              // blackout: the room contributes nothing

    // w2s() is logical pixels; grab() on a HiDPI screen returns device pixels
    // (devicePixelRatio() times as many). Scale the sample point (and the
    // logical-pixel search window around it) up to match.
    const double dpr = v.devicePixelRatioF();
    const QPointF at = v.w2s(props->fixtureRigPosition(fxi->id())) * dpr;

    auto meanLuma = [&]() {
        const QImage img = v.grab().toImage();
        double sum = 0.0;
        int n = 0;
        for (int dy = -8; dy <= 8; ++dy)
        {
            for (int dx = -8; dx <= 8; ++dx)
            {
                const QPoint p(at.toPoint() + QPoint(int(dx * dpr), int(dy * dpr)));
                if (img.rect().contains(p) == false) continue;
                const QColor c = img.pixelColor(p);
                sum += c.red() * 0.30 + c.green() * 0.59 + c.blue() * 0.11;
                ++n;
            }
        }
        return n > 0 ? sum / n : 0.0;
    };

    QByteArray u(512, char(0));
    u[300] = char(255); u[301] = char(255); u[302] = char(255); u[303] = char(255);
    fxi->setChannelValues(u);
    const double lit = meanLuma();

    u[300] = char(0);
    fxi->setChannelValues(u);
    const double doused = meanLuma();

    QVERIFY2(lit > 150.0,
             qPrintable(QString("a fixture at full should still read bright in a "
                                "blackout, got luma %1").arg(lit)));
    QVERIFY2(lit - doused > 60.0,
             qPrintable(QString("dousing the fixture made no difference in a "
                                "blackout (lit %1 vs out %2)").arg(lit).arg(doused)));

    m_doc->deleteFixture(fxi->id());
}


void Monitor_Test::clearTopAndInsidePlacement()
{
    MonitorProperties *props = m_doc->monitorProperties();

    StagePlatform *pl = props->addPlatform();
    pl->setName("Step"); pl->setOriginX(0.0f); pl->setOriginY(0.0f);
    pl->setWidth(4.0f); pl->setDepth(2.0f); pl->setHeight(0.8f);
    pl->setColor(QColor(220, 30, 30));

    StructureStudioView v(m_doc, StructureStudioView::StageKind, 0);
    v.resize(700, 500);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(15.0, 40.0);          // looking down onto the deck
    v.setAmbient(1.0);

    const QPointF onTop = v.w2s(QVector3D(2.0f, 1.0f, 0.8f));

    auto lumaOnTop = [&](StagePlatform::TopMaterial m) {
        pl->setTopMaterial(m);
        const QImage img = v.grab().toImage();
        const QColor c = img.pixelColor(onTop.toPoint());
        return c.red() * 0.30 + c.green() * 0.59 + c.blue() * 0.11;
    };

    const double solid = lumaOnTop(StagePlatform::SolidTop);
    const double clear = lumaOnTop(StagePlatform::ClearTop);
    const double open_ = lumaOnTop(StagePlatform::OpenTop);

    QVERIFY2(clear < solid,
             qPrintable(QString("a clear top rendered as bright as a solid one "
                                "(%1 vs %2)").arg(clear).arg(solid)));
    QVERIFY2(open_ < clear,
             qPrintable(QString("an open frame rendered no lighter than a clear "
                                "top (%1 vs %2)").arg(open_).arg(clear)));

    /* Placement: an Inside fixture stays within the volume instead of being
       seated out onto the surface, which is the whole reason the flag exists. */
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("In Step"); fxi->setChannels(1); fxi->setAddress(420);
    QVERIFY(m_doc->addFixture(fxi));
    props->setFixturePosition(fxi->id(), 0, 0, QVector3D(2000, 1000, 0));

    const FixtureVisualTraits traits = classifyFixture(fxi);
    QVector3D onSurf[8], inside[8];

    FixtureRigProps rp;
    rp.studioMount = 0;                    // lying flat: normal is up
    rp.placement = FixtureRigProps::OnSurface;
    props->setFixtureRigProps(fxi->id(), rp);
    v.fixtureBoxCorners(fxi->id(), traits, onSurf);

    rp.placement = FixtureRigProps::Inside;
    props->setFixtureRigProps(fxi->id(), rp);
    v.fixtureBoxCorners(fxi->id(), traits, inside);

    const float surfMidZ = (onSurf[0].z() + onSurf[6].z()) / 2.0f;
    const float insMidZ  = (inside[0].z() + inside[6].z()) / 2.0f;
    QVERIFY2(insMidZ < surfMidZ,
             qPrintable(QString("Inside was seated out like a surface mount "
                                "(z %1 vs %2)").arg(double(insMidZ)).arg(double(surfMidZ))));
    QVERIFY2(qAbs(double(insMidZ)) < 1e-4,
             qPrintable(QString("Inside should sit centred on its position, got z %1")
                        .arg(double(insMidZ))));

    m_doc->deleteFixture(fxi->id());
    props->removePlatform(pl->id());
}

void Monitor_Test::freePlacedFixtureIsDraggable()
{
    MonitorProperties *props = m_doc->monitorProperties();

    StagePlatform *pl = props->addPlatform();
    pl->setName("DS-3"); pl->setOriginX(0.0f); pl->setOriginY(0.0f);
    pl->setWidth(4.0f); pl->setDepth(1.0f); pl->setHeight(0.2f);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("DS3aB"); fxi->setChannels(1); fxi->setAddress(440);
    QVERIFY(m_doc->addFixture(fxi));
    // Laid on the step: no truss, pipe, tower, riser or deck mount at all.
    props->setFixturePosition(fxi->id(), 0, 0, QVector3D(1000, 500, 0));
    FixtureRigProps rp;
    props->setFixtureRigProps(fxi->id(), rp);
    QCOMPARE(rp.primaryMount(), FixtureRigProps::NoMount);
    QCOMPARE(props->fixtureFrameGroup(fxi->id()), quint32(0));

    StructureStudioView view(m_doc, StructureStudioView::PlatformKind, pl->id());
    view.resize(700, 500);
    view.reload();
    view.setPlane(StructureStudioView::Top);
    view.setLocked(false);

    const QVector3D before = props->fixtureRigPosition(fxi->id());

    /* Drag it half a metre downstage. This used to `return false` -- a fixture
       with neither a structural mount nor a frame group fell off the end of
       dragFixtureTo() and simply could not be moved, in the very editor that
       lists it and lets you select it. */
    const QPointF startPx = view.w2s(before);
    QVERIFY2(view.dragFixtureTo(fxi->id(), startPx + QPointF(0.0, view.m_scale * 0.5)),
             "the editor refused to move a free-placed fixture");

    const QVector3D after = props->fixtureRigPosition(fxi->id());
    QVERIFY2(qAbs(double(after.y() - before.y()) - 0.5) < 0.05,
             qPrintable(QString("dragged 0.5 m but y moved %1 m — check the mixed "
                                "mm/metre storage in setFixturePosition")
                        .arg(double(after.y() - before.y()))));
    QVERIFY2(qAbs(double(after.x() - before.x())) < 1e-3,
             "a downstage drag also moved it sideways");

    m_doc->deleteFixture(fxi->id());
    props->removePlatform(pl->id());
}

void Monitor_Test::draggingBackgroundPansAndSurvivesResize()
{
    StudioRig rig;
    QVERIFY(rig.build(m_doc, Truss::Horizontal));
    rig.view->setPlane(StructureStudioView::Front);
    rig.view->setLocked(false);

    // Somewhere with no fixture under it.
    const QPointF empty(30.0, 30.0);
    QCOMPARE(rig.view->hitTestFixture(empty), Fixture::invalidId());

    const QPointF originBefore = rig.view->m_originPx;
    const QVector3D posBefore = rig.pos();

    QMouseEvent press(QEvent::MouseButtonPress, empty, empty, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QMouseEvent move(QEvent::MouseMove, empty + QPointF(60, 40), empty + QPointF(60, 40), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QMouseEvent rel(QEvent::MouseButtonRelease, empty + QPointF(60, 40), empty + QPointF(60, 40),
                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    rig.view->mousePressEvent(&press);
    rig.view->mouseMoveEvent(&move);
    rig.view->mouseReleaseEvent(&rel);

    const QPointF moved = rig.view->m_originPx - originBefore;
    QVERIFY2(qAbs(moved.x() - 60.0) < 1.0 && qAbs(moved.y() - 40.0) < 1.0,
             qPrintable(QString("dragging the background moved the view by %1,%2, "
                                "expected 60,40").arg(moved.x()).arg(moved.y())));

    // Panning is a VIEW action: nothing in the model may move.
    QCOMPARE(rig.pos(), posBefore);

    /* And a resize must not undo it. refit() recentres, which would silently
       snap the view back and read as the pan not working. */
    const QPointF afterPan = rig.view->m_originPx;
    rig.view->resize(rig.view->width() + 40, rig.view->height() + 30);
    QVERIFY2((rig.view->m_originPx - afterPan).manhattanLength() < 1.0,
             "a resize recentred the view and threw the pan away");
}

void Monitor_Test::insidePlacementPutsFixtureInThePlatform()
{
    MonitorProperties *props = m_doc->monitorProperties();

    StagePlatform *pl = props->addPlatform();
    pl->setName("Clear Step"); pl->setOriginX(0.0f); pl->setOriginY(0.0f);
    pl->setWidth(3.0f); pl->setDepth(1.0f); pl->setHeight(0.5f);
    pl->setTopMaterial(StagePlatform::ClearTop);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("In Step"); fxi->setChannels(1); fxi->setAddress(460);
    QVERIFY(m_doc->addFixture(fxi));
    props->setFixturePosition(fxi->id(), 0, 0, QVector3D(1500, 500, 0));

    // Mounted the way the editor's "Add Fixtures" does it: on the riser top.
    FixtureRigProps rp;
    rp.riserPlatformId = pl->id();
    rp.riserFace = FixtureRigProps::RiserTop;
    rp.riserU = pl->width() * 0.5f;
    rp.riserV = pl->depth() * 0.5f;
    props->setFixtureRigProps(fxi->id(), rp);

    const float top = pl->height();          // base is the floor here
    QCOMPARE(props->fixtureRigPosition(fxi->id()).z(), top);

    /* Inside: it belongs IN the box. This used to change nothing but the
       drawing, so a light "inside" a clear-topped step still sat on the glass. */
    rp.placement = FixtureRigProps::Inside;
    rp.mountZOffset = 0.0f;
    props->setFixtureRigProps(fxi->id(), rp);
    const float insideZ = props->fixtureRigPosition(fxi->id()).z();
    QVERIFY2(insideZ < top,
             qPrintable(QString("Inside left the fixture on the deck (z %1, deck top %2)")
                        .arg(double(insideZ)).arg(double(top))));
    QCOMPARE(insideZ, 0.0f);                 // resting on the floor of the box

    // Raised within the box.
    rp.mountZOffset = 0.2f;
    props->setFixtureRigProps(fxi->id(), rp);
    QCOMPARE(props->fixtureRigPosition(fxi->id()).z(), 0.2f);

    // ...but never out through the top.
    rp.mountZOffset = 5.0f;
    props->setFixtureRigProps(fxi->id(), rp);
    QVERIFY2(props->fixtureRigPosition(fxi->id()).z() <= top,
             "an Inside fixture poked out through the top of the step");

    m_doc->deleteFixture(fxi->id());
    props->removePlatform(pl->id());
}

void Monitor_Test::insideFixtureHeightIsDraggableInElevation()
{
    MonitorProperties *props = m_doc->monitorProperties();

    StagePlatform *pl = props->addPlatform();
    pl->setName("Clear Step"); pl->setOriginX(0.0f); pl->setOriginY(0.0f);
    pl->setWidth(3.0f); pl->setDepth(1.0f); pl->setHeight(0.6f);
    pl->setTopMaterial(StagePlatform::ClearTop);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("In Step"); fxi->setChannels(1); fxi->setAddress(480);
    QVERIFY(m_doc->addFixture(fxi));
    props->setFixturePosition(fxi->id(), 0, 0, QVector3D(1500, 500, 0));

    FixtureRigProps rp;
    rp.riserPlatformId = pl->id();
    rp.riserFace = FixtureRigProps::RiserTop;
    rp.riserU = pl->width() * 0.5f;
    rp.riserV = pl->depth() * 0.5f;
    rp.placement = FixtureRigProps::Inside;
    props->setFixtureRigProps(fxi->id(), rp);
    QCOMPARE(props->fixtureRigPosition(fxi->id()).z(), 0.0f);   // on the floor of the box

    StructureStudioView view(m_doc, StructureStudioView::PlatformKind, pl->id());
    view.resize(700, 500);
    view.reload();
    view.setLocked(false);
    view.setPlane(StructureStudioView::Side);

    /* Drag it up 0.3 m inside the step. This did nothing before: Inside gave
       the fixture a mountZOffset but the riser drag never wrote one, so it was
       stuck on the floor of the box. */
    const QVector3D before = props->fixtureRigPosition(fxi->id());
    const QPointF startPx = view.w2s(before);
    QVERIFY(view.dragFixtureTo(fxi->id(), startPx - QPointF(0.0, view.m_scale * 0.3)));

    const float after = props->fixtureRigPosition(fxi->id()).z();
    QVERIFY2(qAbs(double(after) - 0.3) < 0.03,
             qPrintable(QString("dragged up 0.3 m inside the step but z is %1")
                        .arg(double(after))));

    // It still cannot leave the box.
    QVERIFY(view.dragFixtureTo(fxi->id(), startPx - QPointF(0.0, view.m_scale * 5.0)));
    QVERIFY2(props->fixtureRigPosition(fxi->id()).z() <= pl->height(),
             "dragging hard upward pushed the fixture out through the top");

    m_doc->deleteFixture(fxi->id());
    props->removePlatform(pl->id());
}

void Monitor_Test::deckMountedFixtureMovesVerticallyToo()
{
    MonitorProperties *props = m_doc->monitorProperties();

    StagePlatform *pl = props->addPlatform();
    pl->setName("Step"); pl->setOriginX(0.0f); pl->setOriginY(0.0f);
    pl->setWidth(3.0f); pl->setDepth(1.0f); pl->setHeight(0.6f);
    pl->setTopMaterial(StagePlatform::ClearTop);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("DeckLight"); fxi->setChannels(1); fxi->setAddress(520);
    QVERIFY(m_doc->addFixture(fxi));
    props->setFixturePosition(fxi->id(), 0, 0, QVector3D(1500, 500, 0));

    FixtureRigProps rp;
    rp.deckPlatformId = pl->id();
    rp.placement = FixtureRigProps::Inside;
    props->setFixtureRigProps(fxi->id(), rp);
    QCOMPARE(props->fixtureRigPosition(fxi->id()).z(), 0.0f);   // floor of the box

    StructureStudioView view(m_doc, StructureStudioView::PlatformKind, pl->id());
    view.resize(700, 500);
    view.reload();
    view.setLocked(false);

    // Horizontal, in plan.
    view.setPlane(StructureStudioView::Top);
    const QVector3D b0 = props->fixtureRigPosition(fxi->id());
    QVERIFY(view.dragFixtureTo(fxi->id(), view.w2s(b0) + QPointF(view.m_scale * 0.4, 0.0)));
    QVERIFY2(qAbs(double(props->fixtureRigPosition(fxi->id()).x() - b0.x()) - 0.4) < 0.05,
             "a deck fixture would not move horizontally in plan");

    /* Vertical, in an elevation. This did nothing at all before: with no deck
       branch the drag fell through to free placement, which writes the stored
       X/Y while the deck branch derives Z from the platform. */
    view.setPlane(StructureStudioView::Front);
    const QVector3D b1 = props->fixtureRigPosition(fxi->id());
    QVERIFY(view.dragFixtureTo(fxi->id(), view.w2s(b1) - QPointF(0.0, view.m_scale * 0.25)));
    const float z = props->fixtureRigPosition(fxi->id()).z();
    QVERIFY2(qAbs(double(z) - 0.25) < 0.03,
             qPrintable(QString("dragged up 0.25 m inside the step but z is %1")
                        .arg(double(z))));

    // Still cannot leave the box while it is Inside.
    QVERIFY(view.dragFixtureTo(fxi->id(), view.w2s(b1) - QPointF(0.0, view.m_scale * 6.0)));
    QVERIFY2(props->fixtureRigPosition(fxi->id()).z() <= pl->height(),
             "an Inside deck fixture was pushed out through the top");

    m_doc->deleteFixture(fxi->id());
    props->removePlatform(pl->id());
}

void Monitor_Test::insideFrameGroupFixtureIsNotPinnedToTheFace()
{
    MonitorProperties *props = m_doc->monitorProperties();

    StagePlatform *pl = props->addPlatform();
    pl->setName("DS-4"); pl->setOriginX(0.0f); pl->setOriginY(0.0f);
    pl->setWidth(2.4f); pl->setDepth(0.204f); pl->setHeight(0.204f);
    pl->setTopMaterial(StagePlatform::ClearTop);

    /* A studio FRAME group anchored to the platform -- how the step's own bars
       are held in stage-structures-demo.qxw (GLX/GLY/GLZ, no structural mount
       at all). This branch is the one that pins. */
    const quint32 gid = 77;
    props->ensureGroup(gid, 0);
    props->setGroupHasFrame(gid, true);
    props->setGroupAnchor(gid, QStringLiteral("platform"), pl->id());

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("DS4aB"); fxi->setChannels(1); fxi->setAddress(540);
    QVERIFY(m_doc->addFixture(fxi));
    props->setFixturePosition(fxi->id(), 0, 0, QVector3D(600, 75, 0));
    props->setFixtureGroup(fxi->id(), gid);
    QCOMPARE(props->fixtureFrameGroup(fxi->id()), gid);

    FixtureRigProps rp;
    rp.studioMount = 0;                     // laid flat -> facePin pins Z
    rp.placement = FixtureRigProps::Inside;
    // Sitting on the deck top, exactly as the workspace stores these
    // (GLX/GLY/GLZ with GLZ = the platform's height).
    rp.groupLocal = QVector3D(0.6f, 0.075f, pl->height());
    props->setFixtureRigProps(fxi->id(), rp);
    QCOMPARE(props->fixtureRigPosition(fxi->id()).z(), pl->height());

    StructureStudioView view(m_doc, StructureStudioView::PlatformKind, pl->id());
    view.resize(900, 500);
    view.reload();
    view.setLocked(false);
    view.setPlane(StructureStudioView::Side);

    const QVector3D before = props->fixtureRigPosition(fxi->id());
    const QPointF px = view.w2s(before);

    /* Drag DOWN the screen, into the step. This changed nothing before:
       facePin() re-pinned Z to the deck top on every drag, so an Inside
       fixture was welded to the surface it was supposed to be under. */
    QVERIFY(view.dragFixtureTo(fxi->id(), px + QPointF(0.0, view.m_scale * 0.1)));
    const float after = props->fixtureRigPosition(fxi->id()).z();
    QVERIFY2(after < before.z() - 1e-4,
             qPrintable(QString("Inside is still pinned to the face (z %1 -> %2)")
                        .arg(double(before.z())).arg(double(after))));
    QVERIFY2(after >= 0.0f && after <= pl->height(),
             qPrintable(QString("ended outside the step: z %1, height %2")
                        .arg(double(after)).arg(double(pl->height()))));

    m_doc->deleteFixture(fxi->id());
    props->removePlatform(pl->id());
}

void Monitor_Test::everyMountKindCanBeDragged()
{
    MonitorProperties *props = m_doc->monitorProperties();

    // One of everything a fixture can hang off.
    Truss *tr = props->addTruss();
    tr->setName("Sweep Truss"); tr->setType(Truss::Horizontal);
    tr->setOrigin(QVector3D(0.0f, 0.0f, 3.0f)); tr->setDirection(QPointF(1.0, 0.0));
    tr->setLength(4.0f); tr->setWidth(0.3f);

    Tower *tw = props->addTower();
    tw->setName("Sweep Tower"); tw->setOriginX(6.0f); tw->setOriginY(0.0f);
    tw->setWidth(0.5f); tw->setDepth(0.5f); tw->setHeight(3.0f);
    for (int i = 1; i <= 4; ++i)
        tw->addShelf(i * 0.6f);      // a tower mount is quantised to its shelves

    StagePlatform *pl = props->addPlatform();
    pl->setName("Sweep Deck"); pl->setOriginX(0.0f); pl->setOriginY(4.0f);
    pl->setWidth(3.0f); pl->setDepth(1.5f); pl->setHeight(0.5f);

    const quint32 gid = 88;
    props->ensureGroup(gid, 0);
    props->setGroupHasFrame(gid, true);
    props->setGroupAnchor(gid, QStringLiteral("platform"), pl->id());

    struct Case { const char *name; int kind; StructureStudioView::Kind view; quint32 viewId; };
    const Case cases[] = {
        { "truss",       0, StructureStudioView::TrussKind,    tr->id() },
        { "tower",       1, StructureStudioView::TowerKind,    tw->id() },
        { "riser",       2, StructureStudioView::PlatformKind, pl->id() },
        { "deck",        3, StructureStudioView::PlatformKind, pl->id() },
        { "frame group", 4, StructureStudioView::PlatformKind, pl->id() },
        { "free placed", 5, StructureStudioView::PlatformKind, pl->id() },
    };

    int addr = 300;
    for (const Case &c : cases)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Sweep %1").arg(c.name));
        fxi->setChannels(1);
        fxi->setAddress(addr); addr += 8;
        QVERIFY(m_doc->addFixture(fxi));
        const quint32 fid = fxi->id();
        props->setFixturePosition(fid, 0, 0, QVector3D(1000, 4500, 0));

        FixtureRigProps rp;
        rp.placement = FixtureRigProps::Inside;
        switch (c.kind)
        {
        case 0: rp.trussId = tr->id(); rp.trussOffset = 2.0f; break;
        case 1: rp.towerId = tw->id(); rp.towerU = 0.25f; rp.towerV = 0.25f; break;
        case 2: rp.riserPlatformId = pl->id(); rp.riserFace = FixtureRigProps::RiserTop;
                rp.riserU = 1.5f; rp.riserV = 0.75f; break;
        case 3: rp.deckPlatformId = pl->id(); break;
        case 4: rp.groupLocal = QVector3D(1.0f, 0.5f, pl->height()); break;
        default: break;                       // free placed: no mount at all
        }
        props->setFixtureRigProps(fid, rp);
        if (c.kind == 4)
            props->setFixtureGroup(fid, gid);

        StructureStudioView view(m_doc, c.view, c.viewId);
        view.resize(800, 560);
        view.reload();
        view.setLocked(false);

        /* Two separate questions, because answering only the first is what let
           the deck gap hide: a deck fixture with NO branch still slid sideways
           via the free-placement fallback, so "did it move at all" said yes
           while the vertical -- the thing that was broken -- was ignored.
           Drag PURELY horizontally, then PURELY vertically, and require both. */
        auto dragged = [&](int plane, const QPointF &delta) {
            view.setPlane(StructureStudioView::Plane(plane));
            const QVector3D before = props->fixtureRigPosition(fid);
            view.dragFixtureTo(fid, view.w2s(before) + delta);
            return (props->fixtureRigPosition(fid) - before).length() > 1e-4;
        };

        bool sideways = false, vertical = false;
        for (int pi = 0; pi < 3 && !sideways; ++pi)
            sideways = dragged(pi, QPointF(view.m_scale * 0.2, 0.0));
        /* Vertical only means anything in an elevation. Half a metre, not a
           nudge: a tower mount is QUANTISED to its shelves (0.6 m apart here),
           so a small drag legitimately changes nothing and would make this
           assert a fake failure. */
        for (int pi = 1; pi < 3 && !vertical; ++pi)
            vertical = dragged(pi, QPointF(0.0, -view.m_scale * 0.5));

        QVERIFY2(sideways, qPrintable(QString("a fixture mounted \"%1\" would not "
                                              "move sideways in any plane")
                                      .arg(c.name)));
        QVERIFY2(vertical, qPrintable(QString("a fixture mounted \"%1\" would not "
                                              "move VERTICALLY in any elevation — "
                                              "dragFixtureTo is ignoring the "
                                              "out-of-plane axis for it")
                                      .arg(c.name)));
        m_doc->deleteFixture(fid);
    }

    props->removeTruss(tr->id());
    props->removeTower(tw->id());
    props->removePlatform(pl->id());
}


void Monitor_Test::subPixelLedGridsAreCulledWhenZoomedOut()
{
    MonitorProperties *props = m_doc->monitorProperties();

    /* A pixel strip like the rig's Step Rows: 64 LEDs in a 2 m bar. One
       fixture, sixty-four primitives -- and this rig has dozens. */
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Strip 64");
    def->setType(QLCFixtureDef::LEDBarPixels);
    for (int i = 0; i < 64; ++i)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("D%1").arg(i));
        ch->setGroup(QLCChannel::Intensity);
        def->addChannel(ch);
    }
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("ALL");
    QLCPhysical ph; ph.setWidth(2134); ph.setHeight(25); ph.setDepth(60);
    ph.setLayoutSize(QSize(64, 1));
    mode->setPhysical(ph);
    foreach (QLCChannel *ch, def->channels())
    {
        mode->insertChannel(ch, mode->channels().size());
        QLCFixtureHead h; h.addChannel(mode->channels().size() - 1);
        mode->insertHead(-1, h);
    }
    def->addMode(mode);

    Truss *t = props->addTruss();
    t->setName("Cull Truss"); t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(0.0f, 0.0f, 2.0f)); t->setDirection(QPointF(1.0, 0.0));
    t->setLength(6.0f); t->setWidth(0.3f);

    QList<quint32> fids;
    for (int i = 0; i < 6; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Strip %1").arg(i));
        fxi->setFixtureDefinition(def, mode);
        fxi->setUniverse(3); fxi->setAddress(i * 64);
        QVERIFY(m_doc->addFixture(fxi));
        props->setFixturePosition(fxi->id(), 0, 0, QVector3D(0, 0, 0));
        FixtureRigProps rp; rp.trussId = t->id(); rp.trussOffset = 0.5f + i * 0.9f;
        props->setFixtureRigProps(fxi->id(), rp);
        fids << fxi->id();
    }

    /* Each pixel at its own level, so no two are the same colour.
     *
       Primitives are now batched by colour -- a strip of identical pixels is
       ONE op however far in you zoom, which is the point of batching but makes
       a uniform strip useless for measuring what the zoom does. */
    foreach (quint32 fid, fids)
    {
        Fixture *fxi = m_doc->fixture(fid);
        QVERIFY(fxi != nullptr);
        QByteArray u(512, char(0));
        for (int i = 0; i < 64; ++i)
            u[int(fxi->address()) + i] = char(40 + i * 3);
        fxi->setChannelValues(u);
    }

    StructureStudioView v(m_doc, StructureStudioView::StageKind, 0);
    v.resize(900, 620);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(20.0, 30.0);
    v.setLiveValues(true);

    /* Two explicit scales rather than "the default fit", which on a small test
       rig can already be close enough to draw every LED -- the first version of
       this test measured 472 primitives at both ends and proved nothing. */
    v.m_zoomed = true;                       // stop refit() re-fitting
    const double fitted = v.m_scale;

    v.m_scale = fitted * 0.15;               // whole-stage overview
    v.grab();
    const int wide = v.lastPrimitiveCount();

    v.m_scale = fitted * 4.0;                // close enough to see the LEDs
    v.grab();
    const int near_ = v.lastPrimitiveCount();

    QVERIFY2(near_ > wide * 2,
             qPrintable(QString("zoomed in should draw far more primitives "
                                "(wide %1, near %2) — the LED grids are not "
                                "coming back").arg(wide).arg(near_)));
    QVERIFY2(wide < 6 * 64,
             qPrintable(QString("the wide view drew %1 primitives for 6 strips "
                                "of 64 — sub-pixel LEDs are not being merged")
                        .arg(wide)));

    foreach (quint32 f, fids) m_doc->deleteFixture(f);
    props->removeTruss(t->id());
}

void Monitor_Test::deletingAGroupKeepsItsFixtures()
{
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("US9t");
    QVERIFY(m_doc->addFixtureGroup(grp));
    const quint32 gid = grp->id();

    QList<quint32> fids;
    for (int i = 0; i < 3; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Keeper %1").arg(i));
        fxi->setChannels(1);
        // A universe of its own: earlier tests in this run have taken addresses
        // in the low universes and addFixture() rejects an overlap.
        fxi->setUniverse(3);
        fxi->setAddress(quint32(400 + i * 4));
        QVERIFY(m_doc->addFixture(fxi));
        grp->assignFixture(fxi->id());
        fids << fxi->id();
    }
    QCOMPARE(grp->fixtureList().size(), 3);

    const int before = m_doc->fixtures().size();
    QVERIFY(m_doc->deleteFixtureGroup(gid));

    QVERIFY2(m_doc->fixtureGroup(gid) == NULL, "the group survived deletion");
    QCOMPARE(m_doc->fixtures().size(), before);
    foreach (quint32 fid, fids)
    {
        Fixture *f = m_doc->fixture(fid);
        QVERIFY2(f != NULL, "deleting the group took its fixtures with it");
        QVERIFY2(f->address() != QLCChannel::invalid(),
                 "the fixture survived but lost its patch");
    }

    foreach (quint32 fid, fids) m_doc->deleteFixture(fid);
}


void Monitor_Test::moverAimFollowsPanAndTilt()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Aim Head");
    def->setType(QLCFixtureDef::MovingHead);
    QLCChannel *pan = new QLCChannel();
    pan->setName("Pan"); pan->setGroup(QLCChannel::Pan);
    pan->setControlByte(QLCChannel::MSB);
    def->addChannel(pan);
    QLCChannel *tilt = new QLCChannel();
    tilt->setName("Tilt"); tilt->setGroup(QLCChannel::Tilt);
    tilt->setControlByte(QLCChannel::MSB);
    def->addChannel(tilt);

    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("2ch");
    QLCPhysical ph;
    ph.setWidth(300); ph.setHeight(400); ph.setDepth(300);
    ph.setFocusPanMax(360); ph.setFocusTiltMax(180);
    mode->setPhysical(ph);
    mode->insertChannel(pan, 0);
    mode->insertChannel(tilt, 1);
    QLCFixtureHead head;
    head.addChannel(0); head.addChannel(1);
    mode->insertHead(-1, head);
    def->addMode(mode);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Aim"); fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(3); fxi->setAddress(200);
    QVERIFY(m_doc->addFixture(fxi));

    FixtureRigProps rp;
    QVector3D dir;

    auto drive = [&](int panVal, int tiltVal) {
        QByteArray u(512, char(0));
        u[200] = char(panVal);
        u[201] = char(tiltVal);
        fxi->setChannelValues(u);
    };

    /* Tilt at its centre points a hung mover STRAIGHT DOWN -- its home. */
    drive(128, 128);
    QVERIFY(fixtureAimDirection(fxi, rp, dir));
    QVERIFY2(dir.z() < -0.98f,
             qPrintable(QString("centre tilt should aim straight down, got %1,%2,%3")
                        .arg(double(dir.x())).arg(double(dir.y())).arg(double(dir.z()))));

    /* Tilt to one end swings the beam up to the horizontal, along the bearing
       pan is facing. Pan centred + panZeroDir 0 means DOWNSTAGE, which is +Y. */
    drive(128, 255);
    QVERIFY(fixtureAimDirection(fxi, rp, dir));
    QVERIFY2(dir.y() > 0.9f,
             qPrintable(QString("tilted out at pan centre should face downstage "
                                "(+Y), got %1,%2,%3").arg(double(dir.x()))
                        .arg(double(dir.y())).arg(double(dir.z()))));

    /* A quarter turn clockwise from downstage is stage RIGHT, which is -X. */
    rp.panZeroDir = 90.0f;
    QVERIFY(fixtureAimDirection(fxi, rp, dir));
    QVERIFY2(dir.x() < -0.9f,
             qPrintable(QString("90 deg clockwise from downstage should face "
                                "stage right (-X), got %1,%2,%3").arg(double(dir.x()))
                        .arg(double(dir.y())).arg(double(dir.z()))));

    // A fixture with no pan or tilt has nothing to point.
    Fixture *par = new Fixture(m_doc);
    par->setName("NoAim"); par->setChannels(1); par->setUniverse(3); par->setAddress(300);
    QVERIFY(m_doc->addFixture(par));
    QVERIFY2(fixtureAimDirection(par, rp, dir) == false,
             "a fixture with no pan/tilt reported an aim direction");

    m_doc->deleteFixture(fxi->id());
    m_doc->deleteFixture(par->id());
}

void Monitor_Test::unlitFixturesAreStillObjectsInTheRoom()
{
    /* A fixture at zero is not invisible -- it is a lump of metal hanging in
       the space. Scaling the EMITTED colour by the room level cannot express
       that: an RGB fixture at zero computes as black, and black times any
       room level is still black, so every idle fixture drew as a black
       rectangle. */
    MonitorProperties *props = m_doc->monitorProperties();
    StagePlatform *pl = props->addPlatform();
    pl->setName("Dark Deck"); pl->setOriginX(0.0f); pl->setOriginY(0.0f);
    pl->setWidth(5.0f); pl->setDepth(3.0f); pl->setHeight(1.5f);
    pl->setColor(QColor(0, 0, 0));      // contributes nothing to what we sample

    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Idle Par");
    def->setType(QLCFixtureDef::ColorChanger);
    const char *nm[] = { "Red", "Green", "Blue" };
    const QLCChannel::PrimaryColour pc[] = { QLCChannel::Red, QLCChannel::Green,
                                             QLCChannel::Blue };
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("3ch");
    for (int c = 0; c < 3; ++c)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(nm[c]); ch->setGroup(QLCChannel::Intensity);
        ch->setColour(pc[c]);
        def->addChannel(ch);
        mode->insertChannel(ch, c);
    }
    QLCPhysical ph;
    ph.setWidth(600); ph.setHeight(600); ph.setDepth(600);
    mode->setPhysical(ph);
    QLCFixtureHead head;
    for (int c = 0; c < 3; ++c) head.addChannel(c);
    mode->insertHead(-1, head);
    def->addMode(mode);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Idle Par"); fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(3); fxi->setAddress(400);
    QVERIFY(m_doc->addFixture(fxi));

    props->setFixturePosition(fxi->id(), 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp;
    rp.riserPlatformId = pl->id();
    rp.riserFace = 0;
    rp.riserU = 2.5f;
    rp.riserV = 0.8f;
    props->setFixtureRigProps(fxi->id(), rp);

    fxi->setChannelValues(QByteArray(512, char(0)));    // every channel at zero

    StructureStudioView v(m_doc, StructureStudioView::StageKind, 0);
    v.resize(700, 500);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(20.0, 25.0);
    v.setLiveValues(true);              // showing output, and it is showing NONE

    const QPointF at = v.w2s(props->fixtureRigPosition(fxi->id()));

    auto peakLuma = [&](double ambient) {
        v.setAmbient(ambient);
        const QImage img = v.grab().toImage();
        double best = 0.0;
        for (int dy = -10; dy <= 10; ++dy)
        {
            for (int dx = -10; dx <= 10; ++dx)
            {
                const QPoint p(at.toPoint() + QPoint(dx, dy));
                if (img.rect().contains(p) == false) continue;
                const QColor c = img.pixelColor(p);
                best = qMax(best, c.red() * 0.30 + c.green() * 0.59 + c.blue() * 0.11);
            }
        }
        return best;
    };

    const double work = peakLuma(1.0);
    const double dark = peakLuma(0.0);

    QVERIFY2(work > 40.0,
             qPrintable(QString("an unlit fixture vanished under WORK LIGHT "
                                "(peak luma %1)").arg(work)));
    QVERIFY2(work - dark > 20.0,
             qPrintable(QString("room level made no difference to an unlit "
                                "fixture (work %1, blackout %2)")
                        .arg(work).arg(dark)));

    m_doc->deleteFixture(fxi->id());
    props->removePlatform(pl->id());
}

/* A moving head, built to order, hung at a given height. */
static Fixture *makeMover(Doc *doc, double beamDeg, quint32 addr, const char *name,
                          const QVector3D &at)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel(QString("Head %1").arg(name));
    def->setType(QLCFixtureDef::MovingHead);
    const char *nm[] = { "Pan", "Tilt", "Dimmer", "Red", "Green", "Blue" };
    const QLCChannel::Group gp[] = { QLCChannel::Pan, QLCChannel::Tilt,
        QLCChannel::Intensity, QLCChannel::Intensity, QLCChannel::Intensity,
        QLCChannel::Intensity };
    const QLCChannel::PrimaryColour cl[] = { QLCChannel::NoColour, QLCChannel::NoColour,
        QLCChannel::NoColour, QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue };
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("6ch");
    for (int k = 0; k < 6; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(nm[k]); ch->setGroup(gp[k]);
        if (cl[k] != QLCChannel::NoColour) ch->setColour(cl[k]);
        if (k < 2) ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch); mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setWidth(300); ph.setHeight(400); ph.setDepth(300);
    ph.setFocusPanMax(360); ph.setFocusTiltMax(180);
    ph.setLensDegreesMin(beamDeg); ph.setLensDegreesMax(beamDeg);
    mode->setPhysical(ph);
    QLCFixtureHead hd;
    for (int k = 0; k < 6; ++k) hd.addChannel(quint32(k));
    mode->insertHead(-1, hd);
    def->addMode(mode);

    Fixture *f = new Fixture(doc);
    f->setName(name); f->setFixtureDefinition(def, mode);
    f->setUniverse(3); f->setAddress(addr);
    if (doc->addFixture(f) == false)
        return nullptr;
    /* (fid, head, linked, pos) -- and pos carries X/Y in MILLIMETRES with Z in
       metres. Passing the coordinates as the head/linked arguments left every
       head built by this helper sitting at the origin. */
    doc->monitorProperties()->setFixturePosition(
        f->id(), 0, 0, QVector3D(at.x() * 1000.0f, at.y() * 1000.0f, at.z()));
    return f;
}

/* "The 3Z on the SR tower shows upright in the editor and inverted in the rig
   view." The elevation path asks mountingType which way up the fixture is; the
   ANGLED path never did. It built the body's up-axis as (mount normal) x
   (length axis), whose sign just falls out of the cross product -- for the
   default front-facing plane that is (0,0,-1), so every mover in the rig view
   was assembled base-up and head-down however it was actually rigged. */
void Monitor_Test::aMoverStandingOnAShelfIsNotDrawnHanging()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Tower *tw = props->addTower();
    tw->setName("SR Tower"); tw->setOriginX(7.0f); tw->setOriginY(1.0f);
    tw->setWidth(0.5f); tw->setDepth(0.5f); tw->setHeight(3.0f);
    tw->addShelf(2.4f);

    Fixture *head = makeMover(doc, 14.0, 0, "Shelf Spot", QVector3D(7.25f, 1.25f, 2.4f));
    QVERIFY(head != nullptr);

    auto rigAs = [&](int mounting) {
        FixtureRigProps rp;
        rp.towerId = tw->id();
        rp.towerMountSide = FixtureRigProps::TowerShelf;
        rp.towerShelf = 0;
        rp.towerU = 0.25f; rp.towerV = 0.25f;
        rp.mountingType = Truss::MountingType(mounting);
        props->setFixtureRigProps(head->id(), rp);
        return rp;
    };

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(760, 600);
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(15.0, 20.0);
    v.setAmbient(1.0);
    rigAs(Truss::FloorMounted);
    /* Hold the framing still. resizeEvent() refits the view to whatever is in
       the rig, and grab() on a widget that has never been shown delivers one --
       so the two frames below were rendered through DIFFERENT transforms and
       their "difference" was the whole scene shifting by 26 px. m_zoomed is
       what tells a refit to leave a deliberate view alone. */
    v.grab();                        // flush the first layout
    v.reload();                      // fit once, with the fixture present
    v.m_zoomed = true;               // ...and then stop refitting

    /* The fixture's own pixels, isolated by differencing against a render with
       it taken away -- the tower fills the window either side of the shelf, so
       a raw count there just measures lattice.
     *
       Rig props are read at paint time, so changing them and grabbing again is
       enough; no reload, which would refit and put the two frames out of
       register with each other. */
    const PreviewItem saved = props->m_fixtureItems.value(head->id()).m_baseItem;
    props->removeFixture(head->id());
    const QImage base = v.grab().toImage();
    props->m_fixtureItems[head->id()].m_baseItem = saved;

    auto fixtureRows = [&](int mounting, QVector<int> &rowCount, int &top, int &bottom) {
        rigAs(mounting);
        const QImage img = v.grab().toImage();
        rowCount.fill(0, img.height());
        top = img.height(); bottom = -1;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
            {
                const QColor a = img.pixelColor(x, y), b = base.pixelColor(x, y);
                if (qAbs(a.red() - b.red()) + qAbs(a.green() - b.green())
                    + qAbs(a.blue() - b.blue()) <= 24)
                    continue;
                ++rowCount[y];
                top = qMin(top, y); bottom = qMax(bottom, y);
            }
    };

    /* WHERE IN ITS OWN OUTLINE the widest part of the fixture falls: 0 = its
       top edge, 1 = its bottom.
     *
       The base is much the widest block on a mover -- wider than the head, far
       wider than the two yoke arms -- so this asks "is the base at the bottom
       or the top", which is the whole question and nothing else. Measured
       against the fixture's own outline rather than its mount point, so that
       SEATING the body on its shelf (a separate fix in the same function)
       cannot move it. */
    auto baseEnd = [&](int mounting) {
        QVector<int> rows; int top = 0, bottom = 0;
        fixtureRows(mounting, rows, top, bottom);
        if (bottom - top < 4)
            return -1.0;
        int widest = top, best = -1;
        for (int y = top; y <= bottom; ++y)
            if (rows[y] > best) { best = rows[y]; widest = y; }
        return double(widest - top) / double(bottom - top);
    };

    const double upright = baseEnd(Truss::FloorMounted);
    const double hung    = baseEnd(Truss::TopHung);

    QVERIFY2(upright > 0.6,
             qPrintable(QString("a mover set upright on a shelf is drawn with its "
                                "base in the air and its head slung underneath "
                                "(widest part %1 of the way down its own outline; "
                                "1 = base at the bottom)").arg(upright)));
    QVERIFY2(hung < 0.4,
             qPrintable(QString("a mover hung under a shelf is drawn standing on it "
                                "(widest part %1 of the way down its own outline; "
                                "0 = base at the top)").arg(hung)));
    delete doc;
}

/* "fixture set as underhung .. visually upright": aMoverStandingOnAShelfIsNot-
   DrawnHanging() above fixed this for a TOWER shelf mount in the Angled 3D
   view. A mover mounted on a horizontal TRUSS/bar instead -- the reported
   repro -- goes through a different branch entirely in its own flat
   (Front/Side) elevation: drawOneFixture()'s tower-shelf special case only
   fires when rp.towerId is valid, so a truss mount fell through to the
   generic Mover elevation, which hardcoded "not hung" no matter what
   mountingType said. */
void Monitor_Test::aMoverOnATrussElevationIsNotDrawnUpright()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *bar = props->addTruss();
    bar->setName("Bar 1"); bar->setType(Truss::Horizontal);
    bar->setOrigin(QVector3D(7.0f, 1.0f, 3.0f));
    bar->setDirection(QPointF(1.0, 0.0));
    bar->setLength(3.0f);
    bar->setWidth(0.29f);

    Fixture *head = makeMover(doc, 14.0, 0, "Bar Spot", QVector3D(8.5f, 1.0f, 3.0f));
    QVERIFY(head != nullptr);

    auto rigAs = [&](int mounting) {
        FixtureRigProps rp;
        rp.trussId = bar->id();
        rp.trussOffset = 1.5f;
        rp.mountingType = Truss::MountingType(mounting);
        props->setFixtureRigProps(head->id(), rp);
        return rp;
    };

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(760, 600);
    v.setPlane(StructureStudioView::Front);       // the reported view: an elevation
    v.setAmbient(1.0);
    rigAs(Truss::FloorMounted);
    v.grab();                        // flush the first layout, see the shelf test
    v.reload();
    v.m_zoomed = true;

    const PreviewItem saved = props->m_fixtureItems.value(head->id()).m_baseItem;
    props->removeFixture(head->id());
    const QImage base = v.grab().toImage();
    props->m_fixtureItems[head->id()].m_baseItem = saved;

    auto fixtureRows = [&](int mounting, QVector<int> &rowCount, int &top, int &bottom) {
        rigAs(mounting);
        const QImage img = v.grab().toImage();
        rowCount.fill(0, img.height());
        top = img.height(); bottom = -1;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
            {
                const QColor a = img.pixelColor(x, y), b = base.pixelColor(x, y);
                if (qAbs(a.red() - b.red()) + qAbs(a.green() - b.green())
                    + qAbs(a.blue() - b.blue()) <= 24)
                    continue;
                ++rowCount[y];
                top = qMin(top, y); bottom = qMax(bottom, y);
            }
    };

    /* Unlike the tower-shelf test's single-widest-row measure, this
       fixture's yoke+head OVAL happens to reach the same peak width as the
       base rect (see addMoverUnit(): the ellipse spans headW = r.width() -
       armW*2.2, close enough to the base's full r.width() at this size that
       the two tie) -- so "the first/last row at the max width" lands in
       whichever plateau the scan meets first, not reliably in the base
       band. The base is still unambiguous by a different measure: it is
       the only feature flush against ONE edge of the outline, while the
       oval/arms taper to a point at the other -- so the edge with the
       fuller band (summed over a slice near each end) IS the base end.
       0 = base at the top edge, 1 = base at the bottom edge. */
    auto baseEnd = [&](int mounting) {
        QVector<int> rows; int top = 0, bottom = 0;
        fixtureRows(mounting, rows, top, bottom);
        const int span = bottom - top + 1;
        if (span < 8)
            return -1.0;
        const int edge = qMax(2, span / 20);
        double topSum = 0.0, bottomSum = 0.0;
        for (int i = 0; i < edge; ++i)
        {
            topSum    += rows[top + i];
            bottomSum += rows[bottom - i];
        }
        return bottomSum / qMax(1.0, topSum + bottomSum);
    };

    const double upright = baseEnd(Truss::FloorMounted);
    const double hung    = baseEnd(Truss::TopHung);

    QVERIFY2(upright > 0.58,
             qPrintable(QString("a mover set On-top on a bar is drawn with its "
                                "base in the air (widest part %1 of the way down "
                                "its own outline; 1 = base at the bottom)").arg(upright)));
    QVERIFY2(hung < 0.42,
             qPrintable(QString("a mover set Underhung on a bar is drawn standing "
                                "upright instead of hanging (widest part %1 of the "
                                "way down its own outline; 0 = base at the top)")
                        .arg(hung)));
    delete doc;
}

/* "Now it's in the tower vs on top of it where I have it set in the editor."
   The body was built symmetrically about the point it is fastened to, so half
   of it was inside the structure holding it up. The elevation has always put
   the base flush with the shelf line -- the two views disagreed about seating
   in exactly the way they disagreed about which way up it goes. */
void Monitor_Test::aMoverOnAShelfSitsOnItRatherThanInIt()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Tower *tw = props->addTower();
    tw->setName("Seat Tower"); tw->setOriginX(7.0f); tw->setOriginY(1.0f);
    tw->setWidth(0.5f); tw->setDepth(0.5f); tw->setHeight(2.4f);

    // The TOP cap: clear air above the mount point, solid tower below it. That
    // is the geometry of the report, and it means everything painted above the
    // cap is the fixture and nothing else.
    Fixture *head = makeMover(doc, 14.0, 0, "Cap Spot", QVector3D(7.25f, 1.25f, 2.4f));
    QVERIFY(head != nullptr);
    FixtureRigProps rp;
    rp.towerId = tw->id();
    rp.towerMountSide = FixtureRigProps::TowerTop;
    rp.towerU = 0.25f; rp.towerV = 0.25f;
    rp.mountingType = Truss::FloorMounted;             // "on shelf (upright)"
    props->setFixtureRigProps(head->id(), rp);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(760, 600);
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(15.0, 20.0);
    v.setAmbient(1.0);
    /* Hold the framing still. resizeEvent() refits the view to whatever is in
       the rig, and grab() on a widget that has never been shown delivers one --
       so the two frames below were rendered through DIFFERENT transforms and
       their "difference" was the whole scene shifting by 26 px. m_zoomed is
       what tells a refit to leave a deliberate view alone. */
    v.grab();                        // flush the first layout
    v.reload();                      // fit once, with the fixture present
    v.m_zoomed = true;               // ...and then stop refitting
    const QImage img = v.grab().toImage();

    const PreviewItem saved = props->m_fixtureItems.value(head->id()).m_baseItem;
    props->removeFixture(head->id());                  // tower alone, same framing
    const QImage base = v.grab().toImage();
    props->m_fixtureItems[head->id()].m_baseItem = saved;

    /* How much of the fixture is drawn BELOW the surface it stands on. A few
       pixels are inevitable -- the cap has thickness on screen and the body is
       projected, not flat -- but half of it is the bug.
     *
     * w2s() is logical pixels; grab() on a HiDPI screen returns device pixels
     * (devicePixelRatio() times as many). Scale the comparison point up to
     * match the image's own indexing, or a Retina run counts almost the whole
     * frame as "below" a dividing line sitting near the top of it. */
    const double dpr = img.devicePixelRatio();
    const QPointF at = v.w2s(props->fixtureRigPosition(head->id())) * dpr;
    int above = 0, below = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
        {
            const QColor a = img.pixelColor(x, y), b = base.pixelColor(x, y);
            if (qAbs(a.red() - b.red()) + qAbs(a.green() - b.green())
                + qAbs(a.blue() - b.blue()) <= 24)
                continue;
            if (double(y) < at.y()) ++above; else ++below;
        }

    QVERIFY2(above + below > 50, "the fixture drew almost nothing");
    const double sunk = double(below) / double(above + below);
    QVERIFY2(sunk < 0.25,
             qPrintable(QString("a mover standing on the tower cap is drawn sunk "
                                "into it -- %1% of its body is below the surface "
                                "it is standing on").arg(sunk * 100.0, 0, 'f', 1)));
    delete doc;
}

/* "UST2-6 are in the platform .. and those are solid platforms?"
   They were, and the cause is a default. FixtureRigProps::mountingType starts
   life as Truss::TopHung, so any fixture dropped onto a deck and never given an
   orientation claims to be hanging -- and once the body is seated along its own
   up axis, "hanging" from the TOP of a platform means seated downward, i.e.
   inside the solid thing it is standing on. The two fixtures in that rig that
   someone HAD set to FloorMounted sat on top correctly, which is the tell. */
void Monitor_Test::aMoverOnADeckIsNotBuriedInIt()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    StagePlatform *pl = props->addPlatform();
    pl->setName("USP"); pl->setOriginX(1.0f); pl->setOriginY(1.0f);
    pl->setWidth(2.4f); pl->setDepth(1.2f); pl->setHeight(0.61f);

    // Well inside the footprint, so nothing here is about overhang.
    Fixture *head = makeMover(doc, 14.0, 0, "Deck Beam", QVector3D(2.2f, 1.6f, 0.61f));
    QVERIFY(head != nullptr);

    FixtureRigProps rp;
    rp.deckPlatformId = pl->id();
    rp.deckHeightOffset = 0.0f;
    // Deliberately NOT set: mountingType keeps its default, which is TopHung.
    QCOMPARE(rp.mountingType, Truss::TopHung);
    props->setFixtureRigProps(head->id(), rp);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(760, 600);
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(15.0, 20.0);
    v.setAmbient(1.0);
    v.grab();
    v.reload();
    v.m_zoomed = true;              // hold the framing, see the shelf test
    const QImage img = v.grab().toImage();

    const PreviewItem saved = props->m_fixtureItems.value(head->id()).m_baseItem;
    props->removeFixture(head->id());
    const QImage base = v.grab().toImage();
    props->m_fixtureItems[head->id()].m_baseItem = saved;
    props->setFixtureRigProps(head->id(), rp);

    /* A deck mount stands ON the platform: fixtureRigPosition() already returns
       the platform TOP for it, so the body belongs above that line. */
    /* fixtureCentre(), not fixtureRigPosition(): the plot anchors a free/deck
       fixture by its top-left corner and the rig view undoes that, so the raw
       stored point is half a footprint away from where the body is drawn.
     *
     * w2s() answers in LOGICAL pixels; grab() on a HiDPI screen returns an
     * image in DEVICE pixels (devicePixelRatio() times as many), and the loop
     * below indexes img by device pixel. Comparing a bare logical Y against a
     * device row index is off by that ratio -- 2x on a Retina display -- which
     * put the real dividing line in the top fifth of the frame and counted
     * everything below it, including the correctly-seated fixture, as "sunk". */
    const double dpr = img.devicePixelRatio();
    const QPointF at = v.w2s(v.fixtureCentre(head->id())) * dpr;
    int above = 0, below = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
        {
            const QColor a = img.pixelColor(x, y), b = base.pixelColor(x, y);
            if (qAbs(a.red() - b.red()) + qAbs(a.green() - b.green())
                + qAbs(a.blue() - b.blue()) <= 24)
                continue;
            if (double(y) < at.y()) ++above; else ++below;
        }

    QVERIFY2(above + below > 50, "the fixture drew almost nothing");
    qDebug() << "DIAG at=" << at << "above=" << above << "below=" << below
             << "fixtureBodyUp().z()=" << v.fixtureBodyUp(rp).z()
             << "fixtureCentre=" << v.fixtureCentre(head->id())
             << "fixtureRigPosition=" << props->fixtureRigPosition(head->id());
    const double sunk = double(below) / double(above + below);
    QVERIFY2(sunk < 0.25,
             qPrintable(QString("a mover standing on a deck is drawn sunk into it "
                                "-- %1% of its body is below the platform top it "
                                "is standing on").arg(sunk * 100.0, 0, 'f', 1)));

    /* And the rule itself: you cannot hang from a deck, so which way is up must
       not depend on a mountingType nobody set. */
    QVERIFY2(v.fixtureBodyUp(rp).z() > 0.0f,
             "a deck-mounted fixture's body points down");
    delete doc;
}

/* "when selecting a feature on the bar.truss editor .. should show the
   fixture width on the horizontal .. and the fixture height on the
   vertical like it does on the studio editor." drawDimensions() already
   existed for the STRUCTURE, but nothing called it, and it never took a
   selected fixture's own size into account at all. */
void Monitor_Test::selectingAFixtureShowsItsOwnDimensionCallouts()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *bar = props->addTruss();
    bar->setName("Bar 1"); bar->setType(Truss::Horizontal);
    bar->setOrigin(QVector3D(7.0f, 1.0f, 3.0f));
    bar->setDirection(QPointF(1.0, 0.0));
    bar->setLength(3.0f);

    Fixture *head = makeMover(doc, 14.0, 0, "Dim Test", QVector3D(8.5f, 1.0f, 3.0f));
    QVERIFY(head != nullptr);
    FixtureRigProps rp;
    rp.trussId = bar->id();
    rp.mountingType = Truss::FloorMounted;
    props->setFixtureRigProps(head->id(), rp);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(760, 600);
    v.setPlane(StructureStudioView::Front);
    v.setAmbient(1.0);
    v.grab();                        // flush the first layout, see the shelf test
    v.reload();
    v.m_zoomed = true;               // hold the framing steady between grabs

    const QImage unselected = v.grab().toImage();
    v.setHighlight({ head->id() });
    const QImage selected = v.grab().toImage();

    /* Exactly where drawDimensions() draws its horizontal callout: sMaxY+18,
       computed the same way production code does (fixtureBoxCorners() ->
       screen projection). Starting the diff band there instead of at some
       guessed offset from the fixture's CENTRE is what keeps this clear of
       the body itself.

       fixtureBoxCorners() is the ANGLED-view body, seated along a different
       axis than the flat elevation actually drawn here (the generic mover
       branch in drawOneFixture(), the exact thing that was fixed for
       "underhung .. visually upright" earlier this session) -- using it to
       predict where THIS view's fixture bottom edge falls under-measured
       it and left the band sitting inside the real silhouette, which made
       an earlier version of this assertion pass even with the feature's
       own call site reverted. Replicate the flat elevation's OWN geometry
       instead: a box of height baseR*3.2 centred on the fixture's screen
       point, the same numbers drawOneFixture() itself computes. */
    const FixtureVisualTraits traits = classifyFixture(head);
    const double totalH = v.moverBaseRadius(traits) * 3.2;
    const double sMaxY = v.w2s(v.fixtureCentre(head->id())).y() + totalH / 2.0;

    const double dpr = selected.devicePixelRatio();
    const int bandTop = int((sMaxY + 10.0) * dpr);
    const int bandBottom = qMin(selected.height(), int((sMaxY + 30.0) * dpr));
    QVERIFY2(bandBottom > bandTop, "band is empty -- test geometry is wrong");

    int changed = 0;
    for (int y = qMax(0, bandTop); y < bandBottom; ++y)
        for (int x = 0; x < selected.width(); ++x)
        {
            const QColor a = selected.pixelColor(x, y), b = unselected.pixelColor(x, y);
            if (qAbs(a.red() - b.red()) + qAbs(a.green() - b.green())
                + qAbs(a.blue() - b.blue()) > 24)
                ++changed;
        }

    QVERIFY2(changed > 10,
             qPrintable(QString("selecting the fixture drew nothing in the "
                                "dimension-callout band right below its own "
                                "box (%1 pixels changed) -- no width/height "
                                "label appeared").arg(changed)));
    delete doc;
}

/* "when selecing the bar .. should just show the bar on the rulers" / "when
   selecting the truss and bar it should show the truss on the rulers": a
   parent truss's own Lighting Studio editor draws BOTH the truss and any
   bars hung on it, and the two rulers (structureCentreA()/structureTopZ())
   always measured the editor's OWN object (the parent) no matter what was
   actually selected on the canvas -- so selecting a bar left the ruler
   describing a structure that wasn't even the one highlighted. */
void Monitor_Test::rulersFollowWhicheverOfTrussOrBarIsSelected()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *t1 = props->addTruss();
    t1->setName("T-1");
    t1->setType(Truss::Horizontal);
    t1->setOrigin(QVector3D(1.0f, 2.0f, 4.0f));
    t1->setDirection(QPointF(1.0, 0.0));
    t1->setLength(6.0f);
    t1->setWidth(0.0f);                        // zero half-thickness -> clean numbers

    Truss *bar = props->addTruss();
    bar->setParentTrussId(t1->id());
    bar->setParentOffset(1.0f);                // NOT the parent's midpoint (3.0)
    bar->setBarFace(Truss::FaceBottom);
    bar->setBarStandoff(1.0f);
    bar->setBarRun(Truss::RunAlong);
    bar->setLength(2.0f);
    bar->setWidth(0.0f);
    props->recomputeChildTrusses();
    // attach (2,2,4); Bottom standoff 1 -> base (2,2,3); Along, centred on a
    // length-2 bar -> origin (1,2,3), far end (3,2,3) -- same derivation as
    // MonitorProperties_Test::childBarFollowsParent, offset changed so the
    // bar's own span (X 1..3) is NOT centred the same as the parent's
    // (X 1..7): the two numbers below have to differ for this test to mean
    // anything.
    QCOMPARE(bar->origin(), QVector3D(1, 2, 3));

    StructureStudioView v(doc, StructureStudioView::TrussKind, t1->id());
    v.resize(800, 640);
    v.setPlane(StructureStudioView::Front);

    QCOMPARE(v.m_selectedBarId, Truss::invalidId());
    QVERIFY2(qFuzzyCompare(v.structureCentreA(), 4.0),
             qPrintable(QString("nothing selected: ruler should measure the "
                                "parent (centre 4.0) but got %1").arg(v.structureCentreA())));
    QVERIFY2(qFuzzyCompare(v.structureTopZ(), 4.0),
             qPrintable(QString("nothing selected: ruler should measure the "
                                "parent (top 4.0) but got %1").arg(v.structureTopZ())));

    v.setBarHighlight(bar->id());
    QVERIFY2(qFuzzyCompare(v.structureCentreA(), 2.0),
             qPrintable(QString("bar selected: ruler should measure the BAR "
                                "(centre 2.0) but got %1 -- still the parent")
                        .arg(v.structureCentreA())));
    QVERIFY2(qFuzzyCompare(v.structureTopZ(), 3.0),
             qPrintable(QString("bar selected: ruler should measure the BAR "
                                "(top 3.0) but got %1 -- still the parent")
                        .arg(v.structureTopZ())));

    // Selecting the truss again (clearing the bar) goes back to the parent.
    v.setBarHighlight(Truss::invalidId());
    QVERIFY(qFuzzyCompare(v.structureCentreA(), 4.0));
    QVERIFY(qFuzzyCompare(v.structureTopZ(), 4.0));

    props->removeTruss(bar->id());
    props->removeTruss(t1->id());
    delete doc;
}

/* "Why are these lights showing not in the right places in rig view relative to
   what the studio view has?" Because the two disagreed about what the stored
   number MEANS. The plot anchors a free fixture by the TOP-LEFT of its
   footprint; everything 3-D builds a body centred on the point it is handed.
   Measured, the gap was exactly half the footprint on each axis. */
void Monitor_Test::planAndRigAgreeOnAFixturesPosition()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    StagePlatform *pl = props->addPlatform();
    pl->setName("USP"); pl->setOriginX(1.0f); pl->setOriginY(1.0f);
    pl->setWidth(2.438f); pl->setDepth(1.219f); pl->setHeight(0.61f);

    // A fixture that DECLARES its size: the plot falls back to a minimum-area
    // icon for one that does not, and an invented footprint has no world-space
    // half to agree about.
    Fixture *fx = makeMover(doc, 14.0, 0, "Deck Beam", QVector3D(1.5f, 1.2f, 0.0f));
    QVERIFY(fx != nullptr);

    QWidget parent;
    Monitor *mon = new Monitor(&parent, doc);
    MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);

    gv->resize(1230, 760);
    gv->setSnapDivisions(0);
    gv->setGridMetrics(1000.0);
    // Big cells on purpose: at 30 px/m a 300 mm fixture is 9 px and the plot's
    // 16x16 minimum-AREA clamp inflates the icon, so its footprint stops being
    // the fixture's real size and there is no world-space half to agree about.
    gv->setGridSize(QSize(10, 6));
    QVERIFY(gv->m_cellPixels > 0);

    FixtureRigProps rp;
    rp.deckPlatformId = pl->id();
    props->setFixtureRigProps(fx->id(), rp);

    gv->addFixture(fx->id(), QPointF(1500.0, 1200.0));
    MonitorFixtureItem *item = gv->m_fixtures.value(fx->id(), nullptr);
    QVERIFY(item != nullptr);
    gv->updateFixture(fx->id());

    /* Where the PLAN draws the fixture's centre, in mm. The ICON size, not
       boundingRect() -- that includes the name label hanging below and would
       measure the wrong box entirely. */
    const QPointF centrePx = item->pos()
                           + QPointF(item->m_width / 2.0, item->m_height / 2.0);
    const QPointF planMm = gv->pixelsToRealPosition(centrePx.x(), centrePx.y());

    // Where the RIG view centres its body, in mm.
    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    const QVector3D w = v.fixtureCentre(fx->id());
    const QPointF rigMm(double(w.x()) * 1000.0, double(w.y()) * 1000.0);

    const double gap = QLineF(planMm, rigMm).length();
    QVERIFY2(gap < 30.0,
             qPrintable(QString("the plan and the rig disagree about where this "
                                "fixture is by %1 mm (plan %2,%3 vs rig %4,%5)")
                        .arg(gap).arg(planMm.x()).arg(planMm.y())
                        .arg(rigMm.x()).arg(rigMm.y())));
    delete mon;
    doc->deleteFixture(fx->id());
    delete doc;
}

/* Two fixtures in stage-structures-demo.qxw sit 13.8 m UPSTAGE of the origin,
   left there when they were detached from a truss. The plan's scene rect began
   at (0,0), and a QGraphicsView cannot scroll past its scene rect -- so they
   could not be seen, selected or dragged back at all. The only way to recover
   one was to edit the .qxw by hand. */
void Monitor_Test::theStudioPlanCanReachAFixturePlacedOffTheGrid()
{
    Doc *doc = new Doc(this);

    /* Scoped: DropRig's destructor talks to the Doc (it deletes its fixture and
       the Monitor), so the rig has to be gone BEFORE the Doc is. Deleting the
       Doc first left the destructor calling doc->fixture() through a dangling
       pointer -- a SIGSEGV inside QHash, nothing to do with what is under
       test. */
    {
        DropRig rig;
        QVERIFY(rig.build(doc, QVector3D(792.0f, -13778.0f, 0.0f)));   // 13.8 m upstage

        const QPointF px = rig.gv->realPositionToPixels(792.0, -13778.0);
        const QRectF scene = rig.gv->m_scene->sceneRect();

        QVERIFY2(scene.contains(px),
                 qPrintable(QString("a fixture at (0.79, -13.78) m maps to (%1,%2) px, "
                                    "outside the scene rect (%3,%4 %5x%6) -- the plan "
                                    "cannot scroll to it, so it cannot be recovered")
                            .arg(px.x()).arg(px.y())
                            .arg(scene.x()).arg(scene.y())
                            .arg(scene.width()).arg(scene.height())));
    }
    delete doc;
}

/* A stranded fixture that nothing mentions is how two of them sat 13.8 m
   upstage of stage-structures-demo for months. The tally is the mention. */
void Monitor_Test::theFooterCountsAndSelectsOffStageFixtures()
{
    Doc *doc = new Doc(this);
    {
        DropRig rig;
        QVERIFY(rig.build(doc, QVector3D(2000.0f, 2000.0f, 0.0f)));   // on the grid

        int reported = -1;
        QObject::connect(rig.gv, &MonitorGraphicsView::offGridFixturesChanged,
                         [&reported](int n) { reported = n; });

        QVERIFY2(rig.gv->fixturesOffGrid().isEmpty(),
                 "a fixture on the grid was counted as off it");

        // Send it 13.8 m upstage, the way detaching from a truss did.
        doc->monitorProperties()->setFixturePosition(rig.fxi->id(), 0, 0,
                                                     QVector3D(792.0f, -13778.0f, 0.0f));
        rig.item->setRealPosition(QPointF(792.0, -13778.0));
        rig.gv->updateFixture(rig.fxi->id());

        const QList<quint32> off = rig.gv->fixturesOffGrid();
        QCOMPARE(off.count(), 1);
        QCOMPARE(off.first(), rig.fxi->id());
        QCOMPARE(reported, 1);        // ...and the footer was told

        // Clicking the tally selects them -- the only way back to one of these.
        rig.gv->selectFixtures(off);
        QVERIFY2(rig.item->isSelected(),
                 "selecting the off-stage fixtures did not select it");

        /* Bring it home the way a DRAG does -- which is the case that was
           broken. The view emits fixtureMoved and Monitor::slotFixtureMoved()
           writes the position, so a recount done inside the view's own drag
           handling still sees the old one and the warning never clears:
           "I grabbed them and moved them onstage but the error condition
           persisted." Going through the Monitor slot is the real path. */
        rig.mon->slotFixtureMoved(rig.fxi->id(), QPointF(2000.0, 2000.0));

        QVERIFY2(rig.gv->fixturesOffGrid().isEmpty(),
                 "the fixture is back on the grid but is still counted as off it");
        QCOMPARE(reported, 0);
    }
    delete doc;
}

/* "Whyyyyy when I click on T-1 .. which has a truss glyph .. am I NOT getting
   the truss editor." Because the gate asked hasFrame, and an auto-group for a
   truss may well HAVE a frame -- T-1 and T-2 in stage-structures-demo both do.
   The row's identity is its ANCHOR; the frame is incidental. */
void Monitor_Test::doubleClickingABubbleShowsWhatItHides()
{
    /* "If I double click bubble I get nothing .. no overlay." A bubble stands
       for fixtures hidden under their structure; double-clicking it has to
       show them -- drawn, selected, and with the ground their beams land on
       shaded -- and the next selection puts them back under it. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *tower = props->addTruss();
    tower->setName("T-1");
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    tower->setLength(4.0f);
    tower->setWidth(0.29f);

    QList<quint32> fids;
    for (int i = 0; i < 3; ++i)
    {
        Fixture *f = makeMover(doc, 14.0, quint32(i * 8), qPrintable(QString("Head %1").arg(i + 1)),
                               QVector3D(3.0f, 3.0f, 4.0f));
        QVERIFY(f != nullptr);
        FixtureRigProps rp;
        rp.trussId = tower->id();
        rp.trussOffset = 4.0f;             // all at the top, so they bury it
        props->setFixtureRigProps(f->id(), rp);
        fids << f->id();
        // Pan and tilt centred: a hung head points straight down.
        QByteArray u(512, char(0));
        u[int(f->address())] = char(128);
        u[int(f->address()) + 1] = char(128);
        f->setChannelValues(u);
    }

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 7));
        view.updateTrusses();
        foreach (quint32 fid, fids)
            view.addFixture(fid, QPointF(3000.0, 3000.0));
        view.setViewPOV(MonitorGraphicsView::PovTop);
        view.refreshItemLayerState();

        QGraphicsItemGroup *badge =
            view.m_countBadges.value(QStringLiteral("truss:%1").arg(tower->id()), nullptr);
        QVERIFY2(badge != nullptr, "the fixtures should have buried the tower under a bubble");
        foreach (quint32 fid, fids)
            QVERIFY2(!view.m_fixtures.value(fid)->isVisible(), "buried fixtures start hidden");

        const QPointF sp = badge->sceneBoundingRect().center();
        const QPoint vp = view.mapFromScene(sp);
        QMouseEvent dbl(QEvent::MouseButtonDblClick, vp, view.viewport()->mapToGlobal(vp),
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        view.mouseDoubleClickEvent(&dbl);

        /* What a bubble shows is the NAMES of what it hides, one tag each --
           "we don't have to show the actual fixture(s), just the label you
           have, that works better". The icons stay under the bubble, where
           they were only ever a pile on one spot. */
        foreach (quint32 fid, fids)
        {
            QVERIFY2(!view.revealTagRect(fid).isEmpty(),
                     "a revealed fixture has no tag to click");
            QVERIFY2(!view.m_fixtures.value(fid)->isVisible(),
                     "the bubble dumped the whole stack of icons on the plot");
        }
        QCOMPARE(view.revealedFixtures().size(), fids.size());
        QCOMPARE(view.revealedFocus(), qint64(-1));
        // Footprints are the Show toggle's business, and it is off.
        QCOMPARE(view.m_projectionItems.size(), 0);
        QVERIFY2(!badge->isVisible(), "the bubble still covers what it just revealed");

        /* Straight down from 4 m with a 14 degree beam: about a metre across
           where it lands. */
        const double ppm = 1000.0 * view.m_cellPixels / view.m_unitValue;
        const double want = 2.0 * 4.0 * qTan(qDegreesToRadians(7.0)) * ppm;
        view.setProjectionOverlayVisible(true);
        view.focusRevealedFixture(fids.first());
        QCOMPARE(view.m_projectionItems.size(), 1);
        const QRectF pool = view.m_projectionItems.first()->path().boundingRect();
        QVERIFY2(qAbs(pool.width() - want) < 0.15 * want && qAbs(pool.height() - want) < 0.15 * want,
                 qPrintable(QString("the landing area is %1x%2 px, expected about %3")
                            .arg(pool.width()).arg(pool.height()).arg(want)));
        view.setProjectionOverlayVisible(false);

        // Looking somewhere else puts them back under the bubble.
        const QPoint away = view.mapFromScene(view.realPositionToPixels(9000.0, 6500.0));
        QMouseEvent elsewhere(QEvent::MouseButtonPress, away,
                              view.viewport()->mapToGlobal(away),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view.viewport(), &elsewhere);
        QCoreApplication::processEvents();
        foreach (quint32 fid, fids)
            QVERIFY2(!view.m_fixtures.value(fid)->isVisible(),
                     "the fixtures stayed out after the reveal ended");
        QCOMPARE(view.m_projectionItems.size(), 0);
        QVERIFY2(badge->isVisible(), "the bubble did not come back");
    }

    delete doc;
}

void Monitor_Test::doubleClickingAnywhereOnATowerTopOpensIt()
{
    /* "Sometimes when I double click on the top of a truss it doesn't open the
       editor .. if I move around the top and keep double clicking I eventually
       open it." The pick took whatever was highest under the cursor, and on a
       tower seen from above that was mostly NOT the tower: its own count bubble
       over the middle, the grid and anchor lines across it. Measured on a 29 px
       tower top: 24 of 49 points found the tower. Every point has to. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *tower = props->addTruss();
    tower->setName("T-1");
    tower->setType(Truss::Vertical);
    // On whole metres, so grid lines cross it the way they do in a real plot.
    tower->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    tower->setLength(3.048f);
    tower->setWidth(0.29f);
    QList<quint32> fids;
    for (int i = 0; i < 4; ++i)
    {
        Fixture *f = makeMover(doc, 14.0, quint32(i * 8), qPrintable(QString("F%1").arg(i)),
                               QVector3D(3.0f, 3.0f, 1.0f + i * 0.5f));
        QVERIFY(f != nullptr);
        FixtureRigProps rp;
        rp.trussId = tower->id();
        rp.trussOffset = 1.0f + i * 0.5f;
        props->setFixtureRigProps(f->id(), rp);
        fids << f->id();
    }

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        // Zoomed in, so the bubble covers the middle of the top rather than
        // all of it -- as on a real plot, where a tower is bigger than its badge.
        view.setGridMetrics(250.0);
        view.setGridSize(QSize(10, 7));
        view.updateTrusses();
        foreach (quint32 fid, fids)
            view.addFixture(fid, QPointF(3000.0, 3000.0));
        view.setViewPOV(MonitorGraphicsView::PovTop);
        view.refreshItemLayerState();
        QVERIFY2(view.m_countBadges.contains(QStringLiteral("truss:%1").arg(tower->id())),
                 "the fixtures should bury the tower top under a count bubble");

        int opened = 0;
        QObject::connect(&view, &MonitorGraphicsView::trussDoubleClicked,
                         [&](quint32 tid) { if (tid == tower->id()) ++opened; });

        TrussItem *ti = view.m_trussItems.value(tower->id(), nullptr);
        QVERIFY(ti != nullptr);
        QGraphicsItemGroup *badge =
            view.m_countBadges.value(QStringLiteral("truss:%1").arg(tower->id()), nullptr);
        QVERIFY(badge != nullptr);

        /* Its own bubble is the one thing on a tower top that means something
           else: double-clicking THAT shows the fixtures it stands for, rather
           than opening the tower. Everywhere else on the top opens the tower.  */
        const QRectF bubble = badge->mapRectToScene(badge->childrenBoundingRect());
        {
            const QPoint bp = view.mapFromScene(bubble.center());
            QMouseEvent dbl(QEvent::MouseButtonDblClick, bp, view.viewport()->mapToGlobal(bp),
                            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            view.mouseDoubleClickEvent(&dbl);
            QCOMPARE(opened, 0);
            QVERIFY2(!view.m_revealedFixtures.isEmpty(), "the bubble showed nothing");
            QVERIFY2(!badge->isVisible(), "the bubble stayed over what it revealed");
            // Back under the bubble for the walk below: a click off the stack
            // ends the reveal, which is the gesture a user makes next.
            const QPoint away = view.mapFromScene(view.realPositionToPixels(9000.0, 6500.0));
            QMouseEvent elsewhere(QEvent::MouseButtonPress, away,
                                  view.viewport()->mapToGlobal(away),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(view.viewport(), &elsewhere);
            QCoreApplication::processEvents();
            QVERIFY(badge->isVisible());
        }

        const QRectF top = ti->mapRectToScene(ti->footprintRect());
        const int n = 7;
        int tried = 0;
        QStringList missed;
        QVERIFY(top.width() > bubble.width() * 1.5);
        for (int yy = 0; yy < n; ++yy)
            for (int xx = 0; xx < n; ++xx)
            {
                const QPointF sp(top.left() + top.width() * (xx + 0.5) / n,
                                 top.top() + top.height() * (yy + 0.5) / n);
                if (bubble.contains(sp))
                    continue;                      // the bubble's own job, above
                const QPoint vp = view.mapFromScene(sp);
                const int before = opened;
                QMouseEvent dbl(QEvent::MouseButtonDblClick, vp, view.viewport()->mapToGlobal(vp),
                                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                view.mouseDoubleClickEvent(&dbl);
                ++tried;
                if (opened == before)
                    missed << QString("(%1,%2)").arg(xx).arg(yy);
            }
        QVERIFY2(missed.isEmpty(),
                 qPrintable(QString("double-clicking the tower top opened nothing at %1 of %2 "
                                    "points: %3").arg(missed.size()).arg(tried)
                            .arg(missed.join(" "))));
    }

    delete doc;
}

/* Every action as a path through its submenus: "Mount on > Tower > T-A > Top of
   tower". Separators are layout, not actions. */
static QStringList menuPaths(const QMenu *menu, const QString &prefix = QString())
{
    QStringList out;
    foreach (QAction *a, menu->actions())
    {
        if (a->isSeparator())
            continue;
        const QString path = prefix.isEmpty() ? a->text() : prefix + QStringLiteral(" > ") + a->text();
        if (a->menu() != nullptr)
            out << menuPaths(a->menu(), path);
        else
            out << path;
    }
    return out;
}

static QAction *menuAction(const QMenu *menu, const QString &path)
{
    const QStringList parts = path.split(QStringLiteral(" > "));
    const QMenu *m = menu;
    for (int i = 0; i < parts.size(); ++i)
    {
        QAction *hit = nullptr;
        foreach (QAction *a, m->actions())
            if (!a->isSeparator() && a->text() == parts.at(i))
                hit = a;
        if (hit == nullptr)
            return nullptr;
        if (i == parts.size() - 1)
            return hit;
        if (hit->menu() == nullptr)
            return nullptr;
        m = hit->menu();
    }
    return nullptr;
}

void Monitor_Test::theLayersListOffersEverythingThePlotDoes()
{
    /* "Any option I have when right clicking a feature in the vis window should
       work when I right click the same feature in the layers list." The plot and
       the list built their menus separately: the list had no way to delete a
       truss, add a bar, or mount a fixture, and Edit on a target did nothing.
       Now both come from one builder. Every row the list shows, of every kind,
       must offer each action the plot offers for the same thing -- and actions
       taken from the list must actually happen. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *truss = props->addTruss();
    truss->setName("T-A");
    truss->setOrigin(QVector3D(1.0f, 1.0f, 4.0f)); truss->setLength(4.0f);
    StagePlatform *deck = props->addPlatform();
    deck->setName("Deck"); deck->setOriginX(1.0f); deck->setOriginY(3.0f);
    deck->setWidth(2.0f); deck->setDepth(1.0f); deck->setHeight(0.4f);
    Pipe *boom = props->addPipe();
    boom->setName("Boom"); boom->setOrientation(Pipe::Vertical);
    Stand *stand = props->addStand();
    stand->setName("Stand");
    Tower *tower = props->addTower();
    tower->setName("Tower");
    tower->addShelf(1.0f);
    StageTarget *target = props->addStageTarget();
    target->setName("Mark");
    target->setPosition(QVector3D(3.0f, 4.0f, 0.0f));

    Fixture *spot = makeMover(doc, 14.0, 0, "Spot", QVector3D(2.0f, 1.0f, 4.0f));
    QVERIFY(spot != nullptr);
    FixtureRigProps rp;
    rp.trussId = truss->id();
    rp.trussOffset = 1.0f;
    props->setFixtureRigProps(spot->id(), rp);

    // The truss carries a fixture, so the list shows it AS its group row.
    const quint32 gid = props->nextGroupId();
    props->createGroup(gid, "T-A", 0, 0);
    props->setGroupAnchor(gid, QStringLiteral("truss"), truss->id());
    truss->setGroupId(gid);
    props->setFixtureGroup(spot->id(), gid);

    {
        MonitorGraphicsView view(doc);
        view.resize(900, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 8));
        view.updateTrusses();
        view.updatePlatforms();
        view.updateTargets();
        view.addFixture(spot->id(), QPointF(2000.0, 1000.0));
        MonitorLayersPanel panel(doc, &view);
        panel.reload();

        QSet<QString> kindsChecked;
        QTreeWidgetItemIterator it(panel.m_tree);
        for (; *it != nullptr; ++it)
        {
            QTreeWidgetItem *row = *it;
            const int type = row->data(0, Qt::UserRole).toInt();
            QString kind;
            quint32 id = 0;
            if (type == int(MonitorLayersPanel::NodeItem))
            {
                kind = row->data(0, Qt::UserRole + 2).toString();
                id = row->data(0, Qt::UserRole + 1).toUInt();
            }
            else if (type == int(MonitorLayersPanel::NodeGroup))
            {
                const MonitorProperties::MonitorGroup g = props->group(row->data(0, Qt::UserRole + 1).toUInt());
                kind = g.anchorKind;
                id = g.anchorId;
            }
            if (kind.isEmpty())
                continue;

            QMenu plot, list;
            view.buildItemMenu(&plot, kind, id, MonitorGraphicsView::MenuPlace());
            panel.buildContextMenu(&list, row);
            /* Not a subset: the SAME menu, in the same order, with the one
               action that says where you are swapped. */
            QStringList plotPaths = menuPaths(&plot);
            plotPaths.replaceInStrings(QStringLiteral("Show in Layers"), QStringLiteral("Show on Map"));
            QVERIFY2(!plotPaths.isEmpty(), qPrintable(kind + " has no menu at all"));
            QVERIFY2(plotPaths == menuPaths(&list),
                     qPrintable(QString("%1 %2: the plot offers\n  %3\nbut the Layers list offers\n  %4")
                                .arg(kind).arg(id).arg(plotPaths.join(" | "))
                                .arg(menuPaths(&list).join(" | "))));
            kindsChecked.insert(kind);
        }
        foreach (const QString &k, QStringList({ "fixture", "truss", "platform", "pipe",
                                                "stand", "tower", "target" }))
            QVERIFY2(kindsChecked.contains(k),
                     qPrintable(QString("no Layers row for a %1 was checked").arg(k)));

        auto rowFor = [&](const QString &kind, quint32 id) -> QTreeWidgetItem * {
            QTreeWidgetItemIterator r(panel.m_tree);
            for (; *r != nullptr; ++r)
            {
                const int type = (*r)->data(0, Qt::UserRole).toInt();
                const quint32 rid = (*r)->data(0, Qt::UserRole + 1).toUInt();
                if (type == int(MonitorLayersPanel::NodeItem)
                    && (*r)->data(0, Qt::UserRole + 2).toString() == kind && rid == id)
                    return *r;
                if (type == int(MonitorLayersPanel::NodeGroup)
                    && props->group(rid).anchorKind == kind && props->group(rid).anchorId == id)
                    return *r;
            }
            return nullptr;
        };

        // "Show in Layers" on the plot brings the fixture's row up in the list.
        QObject::connect(&view, &MonitorGraphicsView::revealInLayersRequested,
                         [&](const QString &k, quint32 i) { panel.revealItem(k, i); });
        {
            QMenu m;
            view.buildItemMenu(&m, QStringLiteral("fixture"), spot->id(), MonitorGraphicsView::MenuPlace());
            QAction *a = menuAction(&m, QStringLiteral("Show in Layers"));
            QVERIFY(a != nullptr);
            a->trigger();
            QVERIFY(panel.m_tree->currentItem() == rowFor(QStringLiteral("fixture"), spot->id()));
        }

        // Laid out the same way every time: the item's own action first.
        {
            QMenu m;
            view.buildItemMenu(&m, QStringLiteral("fixture"), spot->id(), MonitorGraphicsView::MenuPlace());
            const QStringList top = menuPaths(&m);
            QCOMPARE(top.first(), QStringLiteral("Edit Fixture…"));
            QCOMPARE(top.last(), QStringLiteral("Remove from View"));
        }

        // ...and removing comes last in the list too, after its own sections.
        {
            QMenu m;
            panel.buildContextMenu(&m, rowFor(QStringLiteral("truss"), truss->id()));
            QCOMPARE(menuPaths(&m).last(), QStringLiteral("Delete Truss"));
            QMenu f;
            panel.buildContextMenu(&f, rowFor(QStringLiteral("fixture"), spot->id()));
            QCOMPARE(menuPaths(&f).first(), QStringLiteral("Edit Fixture…"));
            QCOMPARE(menuPaths(&f).last(), QStringLiteral("Remove from View"));
        }

        // Edit on a target, from the list, reaches its editor.
        quint32 editedTarget = StageTarget::invalidId();
        QObject::connect(&view, &MonitorGraphicsView::targetDoubleClicked,
                         [&](quint32 tid) { editedTarget = tid; });
        {
            QMenu m;
            panel.buildContextMenu(&m, rowFor(QStringLiteral("target"), target->id()));
            QAction *a = menuAction(&m, QStringLiteral("Edit Target…"));
            QVERIFY(a != nullptr);
            a->trigger();
            QCOMPARE(editedTarget, target->id());
        }

        // Delete a truss from its row in the list.
        quint32 deleted = Truss::invalidId();
        QObject::connect(&view, &MonitorGraphicsView::trussRemoveRequested,
                         [&](quint32 tid) { deleted = tid; });
        {
            QMenu m;
            panel.buildContextMenu(&m, rowFor(QStringLiteral("truss"), truss->id()));
            QAction *a = menuAction(&m, QStringLiteral("Delete Truss"));
            QVERIFY2(a != nullptr, "the truss's row cannot delete it");
            a->trigger();
            QCOMPARE(deleted, truss->id());
        }

        // Mount the fixture on the tower from the list, then take it off.
        {
            QMenu m;
            panel.buildContextMenu(&m, rowFor(QStringLiteral("fixture"), spot->id()));
            QAction *a = menuAction(&m, QStringLiteral("Mount on > Tower > Tower > Top of tower"));
            QVERIFY2(a != nullptr, "the fixture's row cannot mount it on a tower");
            a->trigger();
            QCOMPARE(props->fixtureRigProps(spot->id()).towerId, tower->id());
            QCOMPARE(props->fixtureRigProps(spot->id()).trussId, Truss::invalidId());
        }
        {
            panel.reload();
            QMenu m;
            panel.buildContextMenu(&m, rowFor(QStringLiteral("fixture"), spot->id()));
            QAction *a = menuAction(&m, QStringLiteral("Unmount from “Tower”"));
            QVERIFY2(a != nullptr, "the fixture's row cannot unmount it");
            a->trigger();
            QCOMPARE(props->fixtureRigProps(spot->id()).towerId, Tower::invalidId());
        }
    }

    delete doc;
}

/* "if I add a bar to a truss .. that becomes it's own container for lights
   like the truss does .. it should have subentries for the fixtures
   attached to the bar" -- plus the puzzle that came with it in the real
   show file: "whats the difference between the T-1 Frame and the T-1
   Truss?" There wasn't supposed to be one. structuralMembersOf() counted a
   bar sharing its parent's group as a SECOND structural item, so
   ensureTrussGroup() saw "2 structural items" for a plain truss with one
   bar on it and refused to anchor the group -- it fell back to a bare
   "T-1 · frame" folder with the truss itself as a redundant leaf inside
   it, instead of "T-1 · on truss" the way a bar-less truss (T-2, T-3 in
   the same file) always has. */
void Monitor_Test::aBarBecomesItsOwnContainerInTheLayersList()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *t1 = props->addTruss();
    t1->setName("T-1");
    t1->setType(Truss::Vertical);
    t1->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    t1->setLength(3.0f);

    Truss *bar = props->addTruss();
    bar->setName("Bar 1");
    bar->setParentTrussId(t1->id());
    bar->setParentOffset(1.5f);
    bar->setBarFace(Truss::FaceDownstage);
    bar->setBarRun(Truss::RunAcross);
    bar->setLength(1.0f);
    props->recomputeChildTrusses();

    Fixture *onTruss = makeMover(doc, 14.0, 0, "On Truss", QVector3D(3.0f, 3.0f, 1.0f));
    QVERIFY(onTruss != nullptr);
    FixtureRigProps rpTruss;
    rpTruss.trussId = t1->id();
    rpTruss.trussOffset = 1.0f;
    props->setFixtureRigProps(onTruss->id(), rpTruss);

    Fixture *onBar = makeMover(doc, 14.0, 8, "On Bar", QVector3D(2.5f, 3.145f, 1.5f));
    QVERIFY(onBar != nullptr);
    FixtureRigProps rpBar;
    rpBar.trussId = bar->id();
    props->setFixtureRigProps(onBar->id(), rpBar);

    /* A loaded FILE (the actual repro, stage-structures-demo.qxw) doesn't
       arrive at ensureGroupRegistry() with any of this ungrouped -- the
       truss, the bar and both fixtures already carry the SAME persisted
       GroupId from when the bar was created (createBarOnTruss() syncs
       bar->groupId() to the parent's right away, and each fixture's own
       group membership is saved independently of trussId matching). Set
       the scene up the way XML load leaves it, so this exercises the
       repair ensureGroupRegistry() is actually responsible for -- fixing
       the anchor -- not the separate (and here irrelevant) fresh-mount
       auto-grouping fallback. */
    const quint32 gid = props->nextGroupId();
    props->createGroup(gid, "T-1", t1->layerId(), 0);
    t1->setGroupId(gid);
    bar->setGroupId(gid);
    props->setFixtureGroup(onTruss->id(), gid);
    props->setFixtureGroup(onBar->id(), gid);

    MonitorGraphicsView view(doc);
    view.resize(900, 700);
    view.setGridMetrics(1000.0);
    view.setGridSize(QSize(10, 8));
    view.updateTrusses();
    view.addFixture(onTruss->id(), QPointF(3000.0, 3000.0));
    view.addFixture(onBar->id(), QPointF(2500.0, 3145.0));
    view.ensureGroupRegistry();      // the exact pass that runs on file load

    // The group must still be anchored to the TRUSS -- not fallen back to
    // an unanchored "frame" folder just because a bar shares its group.
    QCOMPARE(t1->groupId(), gid);
    const MonitorProperties::MonitorGroup g = props->group(gid);
    QVERIFY2(g.anchorKind == QStringLiteral("truss") && g.anchorId == t1->id(),
             qPrintable(QString("T-1's group anchored to kind=\"%1\" id=%2 "
                                "(expected truss/%3) -- it fell back to a "
                                "plain frame folder")
                        .arg(g.anchorKind).arg(g.anchorId).arg(t1->id())));

    MonitorLayersPanel panel(doc, &view);
    panel.reload();

    QTreeWidgetItem *groupRow = nullptr, *barRow = nullptr;
    QTreeWidgetItem *onTrussRow = nullptr, *onBarRow = nullptr;
    QTreeWidgetItemIterator it(panel.m_tree);
    for (; *it != nullptr; ++it)
    {
        QTreeWidgetItem *row = *it;
        const int type = row->data(0, Qt::UserRole).toInt();
        if (type == int(MonitorLayersPanel::NodeGroup)
            && row->data(0, Qt::UserRole + 1).toUInt() == gid)
            groupRow = row;
        else if (type == int(MonitorLayersPanel::NodeItem)
                 && row->data(0, Qt::UserRole + 2).toString() == QStringLiteral("truss")
                 && row->data(0, Qt::UserRole + 1).toUInt() == bar->id())
            barRow = row;
        else if (type == int(MonitorLayersPanel::NodeItem)
                 && row->data(0, Qt::UserRole + 2).toString() == QStringLiteral("fixture")
                 && row->data(0, Qt::UserRole + 1).toUInt() == onTruss->id())
            onTrussRow = row;
        else if (type == int(MonitorLayersPanel::NodeItem)
                 && row->data(0, Qt::UserRole + 2).toString() == QStringLiteral("fixture")
                 && row->data(0, Qt::UserRole + 1).toUInt() == onBar->id())
            onBarRow = row;
    }

    QVERIFY2(groupRow != nullptr, "T-1's group has no row at all");
    QVERIFY2(groupRow->text(0).contains(QStringLiteral("on truss")),
             qPrintable(QString("T-1's row does not read as a truss: \"%1\"")
                        .arg(groupRow->text(0))));
    QVERIFY2(barRow != nullptr, "Bar 1 has no row of its own under T-1");
    QVERIFY2(onTrussRow != nullptr && onBarRow != nullptr,
             "one of the two fixtures has no row at all");

    QVERIFY2(onTrussRow->parent() == groupRow,
             "the fixture mounted directly on T-1 is not a direct child of T-1's row");
    QVERIFY2(onBarRow->parent() == barRow,
             qPrintable(QString("the fixture mounted on Bar 1 sits under \"%1\", not "
                                "under Bar 1's own row -- it's a flat sibling instead "
                                "of nested under the bar")
                        .arg(onBarRow->parent() ? onBarRow->parent()->text(0) : QStringLiteral("(no parent)"))));

    delete doc;
}

/* Chains six independent user actions in one session -- add a fixture, add a
 * truss, add a bar to it, drop the fixture on the bar, drag the bar, drag the
 * fixture back onto the truss -- through the SAME production code each one's
 * real UI runs: the "Add Truss" dialog, the bar context-menu action's slot,
 * the plot's drop-attach path (dropOnTrussAttachesAndStays' own technique),
 * and a real mousePress/Move/Release sequence on the truss's own Structure
 * Studio editor. A narrow test can pass while two features still disagree
 * about state the moment they're used back to back -- this is the class of
 * bug "T-1 Frame vs T-1 Truss" (aBarBecomesItsOwnContainerInTheLayersList,
 * above) actually was: correct in isolation, wrong once a bar and a group
 * anchor had both been touched in the same session.
 */
void Monitor_Test::buildingARigStepByStepThroughRealUiStaysConsistent()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    // 1. Add a fixture. No test anywhere drives the AddFixture dialog to
    //    completion (addfixture_test.cpp only probes it in isolation), so
    //    this step uses the same direct construction every other test in
    //    this file relies on to get a fixture into the Doc.
    Fixture *fxi = new Fixture(doc);
    fxi->setName("Workflow Mover");
    fxi->setChannels(1);
    QVERIFY(doc->addFixture(fxi));
    const quint32 fid = fxi->id();
    props->setFixturePosition(fid, 0, 0, QVector3D(0, 0, 0));

    QWidget parent;
    Monitor *mon = new Monitor(&parent, doc);
    MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    MonitorLayersPanel *layers = mon->findChild<MonitorLayersPanel *>();
    QVERIFY(layers != nullptr);

    gv->resize(1230, 760);
    gv->setSnapDivisions(0);
    gv->setGridMetrics(1000.0);
    gv->setGridSize(QSize(40, 24));
    QVERIFY(gv->m_cellPixels > 0);
    gv->setViewPOV(MonitorGraphicsView::PovTop);
    gv->addFixture(fid, QPointF(0, 0));
    MonitorFixtureItem *item = gv->m_fixtures.value(fid, nullptr);
    QVERIFY(item != nullptr);

    // 2. Add a truss -- the real, blocking "Add Truss" dialog (slotAddTruss()),
    //    filled in and accepted the way addTrussAccepted() already proved this
    //    QTimer::singleShot technique can drive under offscreen QTest.
    const int trussesBefore = props->trusses().count();
    // Vertical -- a tower, like every other bar-focused test in this file (a
    // Horizontal truss's default crossbar runs ALONG the same line as the
    // truss itself in Top view, which is a real "which one did I actually
    // hit" ambiguity, not a test artefact -- Vertical keeps the two
    // visually apart: the tower is a point, the crossbar a short line off
    // to its side).
    driveDialog([](QWidget *modal) {
        if (modal->windowTitle() != QString("Add Truss"))
            return false;
        QLineEdit *nameEdit = modal->findChild<QLineEdit *>();
        QComboBox *typeCb = modal->findChild<QComboBox *>();
        // originX, originY, originZ, dirAngle, length, width -- in the
        // order slotAddTruss() adds them to the form.
        const QList<QDoubleSpinBox *> spins = modal->findChildren<QDoubleSpinBox *>();
        if (nameEdit == nullptr || typeCb == nullptr || spins.count() != 6)
            return false;
        nameEdit->setText("Workflow Truss");
        typeCb->setCurrentIndex(1);
        spins[0]->setValue(3.0);   // origin X
        spins[1]->setValue(3.0);   // origin Y
        spins[2]->setValue(0.0);   // origin Z (base of the tower)
        spins[3]->setValue(0.0);   // direction: unused for a Vertical truss
        spins[4]->setValue(3.0);   // length (tower height)
        spins[5]->setValue(0.29);  // width
        return true;
    });
    mon->slotAddTruss();

    QCOMPARE(props->trusses().count(), trussesBefore + 1);
    Truss *t = props->trusses().last();
    QCOMPARE(t->name(), QString("Workflow Truss"));
    QCOMPARE(t->type(), Truss::Vertical);
    QVERIFY2(qAbs(double(t->length()) - 3.0) < 0.01,
             "the dialog's Length field did not reach the new truss");

    // 3. Add a bar to it -- the exact slot the truss's "Add > Bar..." context-
    //    menu action calls (Monitor::slotAddBarToTruss(), wired from
    //    MonitorGraphicsView::addBarToTrussRequested()). It opens the truss/
    //    bar editor on the new bar; accept it, the same as a user clicking OK.
    const int trussesBeforeBar = props->trusses().count();
    driveDialog([](QWidget *) { return true; });
    mon->slotAddBarToTruss(t->id(), -1.0f);   // -1 = mid-span, same as the menu action

    QCOMPARE(props->trusses().count(), trussesBeforeBar + 1);
    Truss *bar = nullptr;
    foreach (Truss *cand, props->trusses())
        if (cand->parentTrussId() == t->id())
            bar = cand;
    QVERIFY2(bar != nullptr, "slotAddBarToTruss() did not create a child bar");
    // Nothing is rigged on either object yet -- ensureTrussGroup() forms a
    // group only once something attaches (checked after step 4, below).
    QCOMPARE(t->groupId(), quint32(0));

    // Stand the crossbar well clear of the tower's own on-screen "circle" --
    // createBarOnTruss()'s default standoff (0) puts it barely past the
    // tower's own hit radius, close enough that a Top-view drop could land
    // on either object depending on rounding. A real user would see this
    // as two clearly separate things to click; give the test the same
    // separation instead of chasing a coin-flip.
    bar->setBarStandoff(0.5f);
    props->recomputeChildTrusses();
    gv->updateTrusses();   // rebuild the plot's TrussItems at the new geometry

    // 4. Put the fixture on the bar -- the plot's real drop-attach path:
    //    reposition the item over the bar's own line and call the same slot
    //    a real mouse release invokes, as dropOnTrussAttachesAndStays() does.
    gv->updateFixture(fid);
    const QVector3D barMid = bar->positionAt(bar->length() * 0.5f);
    const QPointF onBarPx = gv->realPositionToPixels(double(barMid.x()) * 1000.0,
                                                      double(barMid.y()) * 1000.0);
    item->setPos(item->pos() + (onBarPx - item->iconCentreScene()));
    gv->slotFixtureMoved(item);

    QVERIFY2(props->fixtureRigProps(fid).trussId == bar->id(),
             "dropping the fixture on the bar's line did not attach it to the bar");
    // Not just attached to the right OBJECT -- it has to have landed where it
    // was actually dropped, not snapped somewhere else on the bar.
    const QVector3D posAfterBarDrop = props->fixtureRigPosition(fid);
    QVERIFY2((posAfterBarDrop - barMid).length() < 0.1f,
             qPrintable(QString("dropped at the bar's midpoint (%1,%2,%3) but landed "
                                "at (%4,%5,%6)")
                        .arg(double(barMid.x())).arg(double(barMid.y())).arg(double(barMid.z()))
                        .arg(double(posAfterBarDrop.x())).arg(double(posAfterBarDrop.y()))
                        .arg(double(posAfterBarDrop.z()))));
    // A plain drop now forms/joins a group too, exactly as the explicit
    // "Attach to Truss" context-menu action always has -- the fix for
    // WORKFLOW-SUBTLETIES.md #2 (a plain drag-drop attach used to leave the
    // fixture rigged but ungrouped until the next file load).
    QVERIFY2(bar->groupId() != 0, "attaching a fixture to the bar formed no group");
    const MonitorProperties::MonitorGroup barGrp = props->group(bar->groupId());
    QVERIFY2(barGrp.anchorKind == QStringLiteral("truss") && barGrp.anchorId == bar->id(),
             qPrintable(QString("bar's group anchored to kind=\"%1\" id=%2 instead of "
                                "truss/%3 (the bar itself)")
                        .arg(barGrp.anchorKind).arg(barGrp.anchorId).arg(bar->id())));
    QCOMPARE(props->fixtureGroup(fid), bar->groupId());

    // 5. Move the bar -- a real mousePress/Move/Release sequence on the
    //    truss's own Structure Studio editor, the sequence
    //    draggingABarChangesItsPositionAlongTheParent() already proved drives
    //    dragBarTo() the same way a real drag does.
    StructureStudioView sv(doc, StructureStudioView::TrussKind, t->id());
    sv.resize(800, 640);
    sv.setPlane(StructureStudioView::Front);
    sv.setLocked(false);   // mousePressEvent()'s bar-drag arm is gated on this
    sv.reload();
    sv.m_zoomed = true;

    const float offsetBeforeDrag = bar->parentOffset();
    const QVector3D fixturePosBeforeDrag = props->fixtureRigPosition(fid);

    // T's Vertical, so its run is Z -- drag the crossbar half a metre up,
    // the same axis draggingABarChangesItsPositionAlongTheParent() drives.
    // Press near one END of the bar, not its midpoint: the fixture attached
    // in step 4 sits exactly at the midpoint, and mousePressEvent() hit-tests
    // a fixture BEFORE a bar -- pressing there would grab the fixture (and
    // slide it along the bar) instead of arming the bar's own drag.
    const QPointF pressPt = sv.w2s(bar->positionAt(bar->length() * 0.1f));
    const QPointF releasePt = sv.w2s(bar->origin() + QVector3D(0.0f, 0.0f, 0.5f));
    QMouseEvent press(QEvent::MouseButtonPress, pressPt, pressPt,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    sv.mousePressEvent(&press);
    QMouseEvent move(QEvent::MouseMove, releasePt, releasePt,
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    sv.mouseMoveEvent(&move);
    QMouseEvent release(QEvent::MouseButtonRelease, releasePt, releasePt,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    sv.mouseReleaseEvent(&release);

    // Not just "it moved" -- it moved to where the drag actually asked for:
    // half a metre along the parent's own run (Z, for a Vertical tower),
    // clamped to the tower's height, the same formula
    // draggingABarChangesItsPositionAlongTheParent() checks.
    const float expectedOffset = qBound(0.0f, offsetBeforeDrag + 0.5f, t->length());
    QVERIFY2(qAbs(double(bar->parentOffset()) - double(expectedOffset)) < 0.1,
             qPrintable(QString("dragging the bar 0.5m up should land parentOffset near "
                                "%1, got %2").arg(double(expectedOffset))
                        .arg(double(bar->parentOffset()))));

    // The fixture rides the bar -- it must have moved by the SAME delta the
    // bar's own offset just moved by (not merely "moved at all"), since
    // RunAcross keeps the bar's own direction fixed and only its Z shifts.
    const QVector3D fixturePosAfterDrag = props->fixtureRigPosition(fid);
    const float actualOffsetDelta = bar->parentOffset() - offsetBeforeDrag;
    const QVector3D expectedFixtureDelta(0.0f, 0.0f, actualOffsetDelta);
    const QVector3D actualFixtureDelta = fixturePosAfterDrag - fixturePosBeforeDrag;
    QVERIFY2((actualFixtureDelta - expectedFixtureDelta).length() < 0.05f,
             qPrintable(QString("bar moved by z=%1 but its fixture moved by "
                                "(%2,%3,%4) instead of matching")
                        .arg(double(actualOffsetDelta))
                        .arg(double(actualFixtureDelta.x())).arg(double(actualFixtureDelta.y()))
                        .arg(double(actualFixtureDelta.z()))));

    // 6. Relocate the fixture off the bar and directly onto the truss --
    //    another real plot drop. A Vertical truss reads as a point in Top
    //    view (MonitorGraphicsView::snapToTruss()'s Vertical branch pins
    //    ANY drop near it to the tower's own X/Y), so landing on the
    //    tower's own origin -- well clear of the crossbar's standoff --
    //    can only read as "on the truss".
    gv->updateFixture(fid);
    MonitorFixtureItem *itemNow = gv->m_fixtures.value(fid, nullptr);
    QVERIFY(itemNow != nullptr);
    const QPointF onTrussPx = gv->realPositionToPixels(double(t->origin().x()) * 1000.0,
                                                        double(t->origin().y()) * 1000.0);
    itemNow->setPos(itemNow->pos() + (onTrussPx - itemNow->iconCentreScene()));
    gv->slotFixtureMoved(itemNow);

    const FixtureRigProps rpAfter = props->fixtureRigProps(fid);
    QVERIFY2(rpAfter.trussId == t->id(),
             qPrintable(QString("relocating onto the tower left trussId=%1 "
                                "(expected the truss %2, not still the bar %3)")
                        .arg(rpAfter.trussId).arg(t->id()).arg(bar->id())));
    // A Vertical truss pins ANY attach to its own X/Y exactly (snapToTruss()'s
    // Vertical branch) -- so the fixture's world X/Y must now equal the
    // tower's origin, not just "somewhere on the truss".
    const QVector3D posAfterTrussDrop = props->fixtureRigPosition(fid);
    QVERIFY2(qAbs(double(posAfterTrussDrop.x()) - double(t->origin().x())) < 0.05
                 && qAbs(double(posAfterTrussDrop.y()) - double(t->origin().y())) < 0.05,
             qPrintable(QString("relocated fixture sits at (%1,%2), not the tower's own "
                                "origin (%3,%4)")
                        .arg(double(posAfterTrussDrop.x())).arg(double(posAfterTrussDrop.y()))
                        .arg(double(t->origin().x())).arg(double(t->origin().y()))));

    /* Settle the session the way the next file load would --
       ensureGroupRegistry() (called on load, see aBarBecomesItsOwnContainer-
       InTheLayersList's own comment on this) forms/re-anchors a group for
       any truss with something actually rigged on it. Our fixture ended
       step 6 on the TRUSS itself, so this is where "does a plain drag-drop
       leave something a reload will read correctly" actually gets checked --
       the drop path itself only ever wrote rig props, deliberately, per the
       comment after step 4. */
    gv->ensureGroupRegistry();
    QVERIFY2(t->groupId() != 0, "the truss ended the session with no group at all");
    const MonitorProperties::MonitorGroup finalGrp = props->group(t->groupId());
    QVERIFY2(finalGrp.anchorKind == QStringLiteral("truss") && finalGrp.anchorId == t->id(),
             qPrintable(QString("truss's group anchored to kind=\"%1\" id=%2 instead "
                                "of truss/%3")
                        .arg(finalGrp.anchorKind).arg(finalGrp.anchorId).arg(t->id())));

    // The Layers list agrees: the fixture sits directly under the truss's row.
    layers->reload();
    QTreeWidgetItem *groupRow = nullptr, *fixtureRow = nullptr;
    QTreeWidgetItemIterator it(layers->m_tree);
    for (; *it != nullptr; ++it)
    {
        QTreeWidgetItem *row = *it;
        const int type = row->data(0, Qt::UserRole).toInt();
        if (type == int(MonitorLayersPanel::NodeGroup)
            && row->data(0, Qt::UserRole + 1).toUInt() == t->groupId())
            groupRow = row;
        else if (type == int(MonitorLayersPanel::NodeItem)
                 && row->data(0, Qt::UserRole + 2).toString() == QStringLiteral("fixture")
                 && row->data(0, Qt::UserRole + 1).toUInt() == fid)
            fixtureRow = row;
    }
    QVERIFY2(groupRow != nullptr, "the truss's group has no row after the whole workflow");
    QVERIFY2(fixtureRow != nullptr, "the fixture has no row at all after being relocated");
    QVERIFY2(fixtureRow->parent() == groupRow,
             qPrintable(QString("after moving back onto the truss, the fixture sits under "
                                "\"%1\" instead of the truss's own row")
                        .arg(fixtureRow->parent() ? fixtureRow->parent()->text(0)
                                                   : QStringLiteral("(no parent)"))));

    delete mon;
    doc->deleteFixture(fid);
    props->removeTruss(bar->id());
    props->removeTruss(t->id());
    delete doc;
}

/* Chains the target/aim workflow: add a fixture, add a target through the
 * real "Add Target" -> "Edit Target" dialog chain, assign the fixture to aim
 * at it through a Scene (the model SceneGroupLooks' own drag-drop writes),
 * then check the RESULT, not just that references got stored -- the same
 * angle-based round trip aHeadAimedAtAMarkDrawsItsBeamThere() uses: solve the
 * aim, write it back as DMX, read the ray back, and require it actually
 * arrives at the mark. Move the target through its own real dialog and
 * confirm the beam follows; move it somewhere the fixture physically cannot
 * reach and confirm the plot's own "cannot reach" warning agrees -- and
 * clears the moment it's re-rigged within reach, without anything else
 * prodding it (mirroring aTargetSaysWhichHeadsCannotReachIt()).
 */
void Monitor_Test::aimingAFixtureAtATargetFollowsItThroughARealSession()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    // 1. Add a fixture -- a moving head, via the same direct-construction
    //    helper every aim test in this file relies on (no dialog-driven
    //    AddFixture path exists yet -- see the truss/bar workflow test).
    Fixture *fx = makeMover(doc, 14.0, 0, "Workflow Head", QVector3D(2.0f, 3.0f, 3.0f));
    QVERIFY(fx != nullptr);
    Truss *bar = props->addTruss();
    bar->setName("Rig");
    bar->setOrigin(QVector3D(0.0f, 3.0f, 3.0f));
    bar->setLength(6.0f);
    FixtureRigProps rp;
    rp.trussId = bar->id();
    rp.trussOffset = 2.0f;
    rp.mountingType = Truss::TopHung;
    props->setFixtureRigProps(fx->id(), rp);

    QWidget parent;
    Monitor *mon = new Monitor(&parent, doc);
    MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    gv->resize(1000, 700);
    gv->setGridMetrics(1000.0);
    gv->setGridSize(QSize(10, 7));
    gv->updateTrusses();
    gv->addFixture(fx->id(), QPointF(2000.0, 3000.0));

    // 2. Add a target -- the real, blocking "Add Target" -> "Edit Target"
    //    dialog chain (Monitor::slotAddTarget()), the same technique
    //    addTargetAccepted() already proved. Accepting auto-links an Aim
    //    palette to it (an Aim palette, not the stale "PanTilt" the comment
    //    on addTargetAccepted still says -- see the subtleties list).
    const int targetsBefore = props->stageTargets().count();
    const int palettesBefore = doc->palettes().count();
    driveDialog([](QWidget *modal) {
        if (!modal->windowTitle().startsWith("Edit Target"))
            return false;
        QLineEdit *nameEdit = modal->findChild<QLineEdit *>();
        // X, Y, Z, follow-spot-subject-height -- in the order
        // slotEditTarget() adds them to the form.
        const QList<QDoubleSpinBox *> spins = modal->findChildren<QDoubleSpinBox *>();
        if (nameEdit == nullptr || spins.count() < 3)
            return false;
        nameEdit->setText("Workflow Mark");
        spins[0]->setValue(4.0);   // X
        spins[1]->setValue(1.5);   // Y
        spins[2]->setValue(0.0);   // Z
        return true;
    });
    mon->slotAddTarget();
    gv->updateTargets();   // pick up the position the dialog just wrote

    QCOMPARE(props->stageTargets().count(), targetsBefore + 1);
    StageTarget *mark = props->stageTargets().last();
    QCOMPARE(mark->name(), QString("Workflow Mark"));
    QVERIFY2(qAbs(double(mark->x()) - 4.0) < 0.01 && qAbs(double(mark->y()) - 1.5) < 0.01
                 && qAbs(double(mark->z())) < 0.01,
             qPrintable(QString("the dialog's X/Y/Z fields did not reach the new target "
                                "(got %1,%2,%3)")
                        .arg(double(mark->x())).arg(double(mark->y())).arg(double(mark->z()))));

    QCOMPARE(doc->palettes().count(), palettesBefore + 1);
    QLCPalette *aim = nullptr;
    foreach (QLCPalette *p, doc->palettes())
        if (p->stageTargetId() == mark->id() && p->type() == QLCPalette::Aim)
            aim = p;
    QVERIFY2(aim != nullptr, "accepting the target dialog did not auto-link an Aim palette");

    // 3. Assign the fixture to aim at it -- through a Scene ("Look"), the
    //    data SceneGroupLooks' drag-drop itself writes (scene->addFixture()
    //    + scene->addPalette()). No test anywhere drives the real QDropEvent
    //    for that drag yet (see the subtleties list); this uses the same
    //    direct Scene-building every other aim test in this file does.
    Scene *scene = new Scene(doc);
    scene->setName("Workflow Look");
    scene->addFixture(fx->id());
    scene->addPalette(aim->id());
    QVERIFY(doc->addFunction(scene));
    gv->setActiveScene(scene->id());
    gv->updateTargets();

    QLCFixtureMode *mode = fx->fixtureMode();
    const quint32 panCh = mode->heads().first().channelNumber(QLCChannel::Pan, QLCChannel::MSB);
    const quint32 tiltCh = mode->heads().first().channelNumber(QLCChannel::Tilt, QLCChannel::MSB);
    const QLCPhysical phy = mode->physical();
    const float panMax = phy.focusPanMax() ? float(phy.focusPanMax()) : 360.0f;
    const float tiltMax = phy.focusTiltMax() ? float(phy.focusTiltMax()) : 270.0f;

    // 4. The results match the request: AimSolver resolves a reachable
    //    pan/tilt, and feeding that back as DMX draws the beam AT the mark --
    //    not just "some values got written" -- the same angle-based check
    //    aHeadAimedAtAMarkDrawsItsBeamThere() uses.
    auto checkAimsAt = [&](const QVector3D &target, const char *when) {
        AimSolver::Solution sol;
        QVERIFY2(AimSolver::reach(doc, fx->id(), target, sol),
                 qPrintable(QString("no frame to solve aim %1").arg(when)));
        QVERIFY2(sol.reachable(),
                 qPrintable(QString("cannot reach the mark %1 (margin %2)")
                            .arg(when).arg(double(sol.marginDeg))));

        QByteArray v(512, char(0));
        v[int(fx->address() + panCh)] = char(uchar(qRound(sol.panDeg * 255.0f / panMax)));
        v[int(fx->address() + tiltCh)] = char(uchar(qRound(sol.tiltDeg * 255.0f / tiltMax)));
        fx->setChannelValues(v);

        QVector3D dir;
        QVERIFY(fixtureAimDirection(fx, props->fixtureRigProps(fx->id()), dir));
        const QVector3D from = props->fixtureRigPosition(fx->id());
        QVector3D want = target - from;
        want.normalize();
        const float offBy = float(qRadiansToDegrees(qAcos(double(qBound(-1.0f,
                                    QVector3D::dotProduct(dir, want), 1.0f)))));
        QVERIFY2(offBy < 2.0f,
                 qPrintable(QString("aimed at the mark %1 but the beam points %2 degrees away")
                            .arg(when).arg(double(offBy))));
    };
    checkAimsAt(mark->position(), "at the original mark");

    // The view's own reachability read agrees: nothing unreachable yet.
    TargetItem *ti = gv->m_targetItems.value(mark->id(), nullptr);
    QVERIFY2(ti != nullptr, "the target has no plot item at all");
    QCOMPARE(ti->unreachableCount(), 0);

    // 5. Move the target -- through its own real "Edit Target" dialog again
    //    (Monitor::slotEditTarget(), the same dialog slotAddTarget() itself
    //    opened, driven the same QTimer::singleShot way).
    driveDialog([](QWidget *modal) {
        const QList<QDoubleSpinBox *> spins = modal->findChildren<QDoubleSpinBox *>();
        if (spins.count() < 3)
            return false;
        spins[0]->setValue(-1.0);   // X
        spins[1]->setValue(4.5);    // Y
        spins[2]->setValue(0.0);    // Z
        return true;
    });
    mon->slotEditTarget(mark->id());
    gv->updateTargets();

    QVERIFY2(qAbs(double(mark->x()) + 1.0) < 0.01 && qAbs(double(mark->y()) - 4.5) < 0.01,
             qPrintable(QString("moving the target through its edit dialog left it at "
                                "(%1,%2), not (-1,4.5)")
                        .arg(double(mark->x())).arg(double(mark->y()))));

    // 6. The beam follows -- resolved fresh against the NEW mark, not the old one.
    checkAimsAt(mark->position(), "after moving the target");

    // 7. Move it somewhere this fixture physically cannot reach -- the exact
    //    scenario aTargetSaysWhichHeadsCannotReachIt() proves is unreachable:
    //    a head standing upright with its tilt centred UP cannot bend down to
    //    something at its own feet half a metre off.
    rp.mountingType = Truss::FloorMounted;
    props->setFixtureRigProps(fx->id(), rp);
    const QVector3D fixtureXY = props->fixtureRigPosition(fx->id());
    mark->setX(fixtureXY.x() + 0.5f);
    mark->setY(fixtureXY.y());
    mark->setZ(0.0f);
    gv->updateTargets();
    QCoreApplication::processEvents();

    ti = gv->m_targetItems.value(mark->id(), nullptr);
    QVERIFY2(ti != nullptr, "the target lost its plot item after being moved out of reach");
    QVERIFY2(ti->unreachableCount() == 1,
             qPrintable(QString("standing upright with the mark at its own feet should be "
                                "unreachable, unreachableCount() = %1").arg(ti->unreachableCount())));

    // 8. Re-rig within reach again -- the warning clears without anything
    //    else prodding it, same as the existing regression test.
    rp.mountingType = Truss::TopHung;
    props->setFixtureRigProps(fx->id(), rp);
    QCoreApplication::processEvents();
    ti = gv->m_targetItems.value(mark->id(), nullptr);
    QVERIFY(ti != nullptr);
    QCOMPARE(ti->unreachableCount(), 0);

    delete mon;
    doc->deleteFixture(fx->id());
    props->removeTruss(bar->id());
    props->removeStageTarget(mark->id());
    delete doc;
}

/* Chains the platform/riser workflow: add a platform through its real edit
 * dialog, deck-mount one fixture through the plot's real auto-deck drop,
 * riser-mount a second fixture through the real Structure Studio drop path
 * (Monitor::mountFixtureOnStructure(), the exact function a real
 * fixturesDropped signal calls), drag the riser fixture on its own editor,
 * then move the platform itself and check BOTH fixtures land exactly where
 * the geometry says -- via two different mechanisms. A deck fixture's XY is
 * stored ABSOLUTE and has to be explicitly carried by slotPlatformMoved()'s
 * own delta loop; a riser fixture's position is DERIVED live from the
 * platform's own geometry every time it's read, so it simply comes out
 * right with no carry code involved at all. Getting the drop math right for
 * one and not the other is exactly the kind of bug that only shows up once
 * both mounts exist on the same platform in the same session.
 */
void Monitor_Test::buildingAPlatformStepByStepThroughRealUiStaysConsistent()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    // 1. Add two fixtures -- one ends up DECK-mounted, one RISER-mounted.
    //    No dialog-driven AddFixture path exists yet (see the truss/bar
    //    workflow test), so this uses the same direct construction every
    //    other test in this file relies on.
    Fixture *deckFx = new Fixture(doc);
    deckFx->setName("Deck Fixture"); deckFx->setChannels(1);
    QVERIFY(doc->addFixture(deckFx));
    Fixture *riserFx = new Fixture(doc);
    riserFx->setName("Riser Fixture"); riserFx->setChannels(1);
    riserFx->setAddress(16);
    QVERIFY(doc->addFixture(riserFx));
    props->setFixturePosition(deckFx->id(), 0, 0, QVector3D(0, 0, 0));
    props->setFixturePosition(riserFx->id(), 0, 0, QVector3D(0, 0, 0));

    QWidget parent;
    Monitor *mon = new Monitor(&parent, doc);
    MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    gv->resize(1230, 760);
    gv->setSnapDivisions(0);
    gv->setGridMetrics(1000.0);
    gv->setGridSize(QSize(40, 24));
    QVERIFY(gv->m_cellPixels > 0);
    gv->setViewPOV(MonitorGraphicsView::PovTop);
    gv->addFixture(deckFx->id(), QPointF(0, 0));
    gv->addFixture(riserFx->id(), QPointF(0, 0));

    // 2. Add a platform -- Monitor::slotAddPlatform() creates it, THEN opens
    //    its real "Lighting Studio Editor -- Riser N" dialog
    //    (slotEditPlatform()). Every field there applies LIVE as you type
    //    (each spin box is wired straight to the property setter), so
    //    filling them in and clicking OK is the same real path resizing a
    //    new step takes.
    const int platformsBefore = props->platforms().count();
    // This dialog embeds a whole Structure Studio canvas alongside the
    // geometry form (its own zoom/pan controls add MORE QDoubleSpinBoxes to
    // the dialog -- ten, not five), so any lookup here has more ways to come
    // back empty/wrong than the plainer Add Truss/Edit Target dialogs.
    driveDialog([](QWidget *modal) {
        QLineEdit *nameEdit = findRealLineEdit(modal);
        // originX, originY, width, depth, height -- in the order
        // slotEditPlatform() adds them to the form. Scope the spin-box
        // search to nameEdit's own container (the geometry form) rather than
        // the whole dialog, which would also catch the canvas's controls.
        QWidget *geomPane = nameEdit ? nameEdit->parentWidget() : nullptr;
        const QList<QDoubleSpinBox *> spins =
            geomPane ? geomPane->findChildren<QDoubleSpinBox *>() : QList<QDoubleSpinBox *>();
        if (nameEdit == nullptr || spins.count() != 5)
            return false;
        nameEdit->setText("Workflow Riser");
        spins[0]->setValue(3.0);   // origin X
        spins[1]->setValue(3.0);   // origin Y
        spins[2]->setValue(2.0);   // width
        spins[3]->setValue(1.5);   // depth
        spins[4]->setValue(0.6);   // height
        return true;
    });
    mon->slotAddPlatform();
    gv->updatePlatforms();

    QCOMPARE(props->platforms().count(), platformsBefore + 1);
    StagePlatform *plat = props->platforms().last();
    QCOMPARE(plat->name(), QString("Workflow Riser"));
    QVERIFY2(qAbs(double(plat->width()) - 2.0) < 0.01 && qAbs(double(plat->depth()) - 1.5) < 0.01
                 && qAbs(double(plat->height()) - 0.6) < 0.01,
             qPrintable(QString("the dialog's size fields did not reach the new platform "
                                "(%1 x %2 x %3)")
                        .arg(double(plat->width())).arg(double(plat->depth()))
                        .arg(double(plat->height()))));

    // 3. Put a fixture on TOP of it -- the plot's real drop path: any loose
    //    fixture dropped over a platform's footprint auto-decks, no
    //    explicit "attach" gesture needed (unlike a truss).
    MonitorFixtureItem *deckItem = gv->m_fixtures.value(deckFx->id(), nullptr);
    QVERIFY(deckItem != nullptr);
    gv->updateFixture(deckFx->id());
    /* The deck-mount check reads the item's centre as pos() + cellSize()/2
       (MonitorGraphicsView.cpp's own static halfIcon()) -- the ICON, not
       the drawn bounding rect, which also reserves space for the name
       label and so drifts depending on whether labels happen to be
       showing. MonitorFixtureItem::iconCentreScene() is the same reading
       (mapToScene(iconRect().center())), added this session to give every
       call site ONE agreed-on answer instead of each re-deriving it --
       see WORKFLOW-SUBTLETIES.md #10, whose original fix for THIS test used
       a manual cellSize()/2 computation because that canonical accessor
       didn't exist yet. */
    const QPointF onDeckPx = gv->realPositionToPixels(double(plat->originX()) * 1000.0 + 200.0,
                                                       double(plat->originY()) * 1000.0 + 200.0);
    deckItem->setPos(deckItem->pos() + (onDeckPx - deckItem->iconCentreScene()));
    gv->slotFixtureMoved(deckItem);

    QVERIFY2(props->fixtureRigProps(deckFx->id()).deckPlatformId == plat->id(),
             "dropping the fixture over the platform's footprint did not deck-mount it");
    const QVector3D deckPosBefore = props->fixtureRigPosition(deckFx->id());
    QVERIFY2(qAbs(double(deckPosBefore.z()) - double(plat->height())) < 0.01,
             qPrintable(QString("a fresh deck mount should sit at the platform's own height "
                                "(%1) but Z is %2").arg(double(plat->height()))
                        .arg(double(deckPosBefore.z()))));

    // 4. Mount the other fixture INTO it -- the real path for a riser mount
    //    is dropping on the platform's OWN Structure Studio canvas, which
    //    fires fixturesDropped() -> Monitor::mountFixtureOnStructure()
    //    (kind 3). Calling it directly is calling the exact function that
    //    signal handler calls -- the same reasoning the truss/bar test uses
    //    to call slotAddBarToTruss() directly for its context-menu action.
    mon->mountFixtureOnStructure(riserFx->id(), 3, plat->id(), 0);
    QVERIFY2(props->fixtureRigProps(riserFx->id()).riserPlatformId == plat->id(),
             "mountFixtureOnStructure(kind=3) did not riser-mount the fixture");
    const QVector3D riserPosBefore = props->fixtureRigPosition(riserFx->id());
    const QVector3D expectRiserCentre(plat->originX() + plat->width() * 0.5f,
                                      plat->originY() + plat->depth() * 0.5f,
                                      plat->height());
    QVERIFY2((riserPosBefore - expectRiserCentre).length() < 0.01f,
             qPrintable(QString("a fresh riser-top mount should sit centred on the platform "
                                "(%1,%2,%3), got (%4,%5,%6)")
                        .arg(double(expectRiserCentre.x())).arg(double(expectRiserCentre.y()))
                        .arg(double(expectRiserCentre.z()))
                        .arg(double(riserPosBefore.x())).arg(double(riserPosBefore.y()))
                        .arg(double(riserPosBefore.z()))));

    // 5. Drag the riser fixture within the platform's own Structure Studio
    //    editor -- a real mousePress/Move/Release sequence, the way
    //    studioTrussDragMovesFixture() drives the truss equivalent.
    StructureStudioView sv(doc, StructureStudioView::PlatformKind, plat->id());
    sv.resize(700, 600);
    sv.setPlane(StructureStudioView::Top);
    sv.setLocked(false);   // mousePressEvent()'s drag arm is gated on this
    sv.reload();

    const QVector3D targetOnPlatform(plat->originX() + plat->width() * 0.25f,
                                     plat->originY() + plat->depth() * 0.75f,
                                     riserPosBefore.z());
    const QPointF pressPt = sv.w2s(riserPosBefore);
    const QPointF releasePt = sv.w2s(targetOnPlatform);
    QMouseEvent press(QEvent::MouseButtonPress, pressPt, pressPt,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    sv.mousePressEvent(&press);
    QMouseEvent move(QEvent::MouseMove, releasePt, releasePt,
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    sv.mouseMoveEvent(&move);
    QMouseEvent release(QEvent::MouseButtonRelease, releasePt, releasePt,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    sv.mouseReleaseEvent(&release);

    const QVector3D riserPosAfterDrag = props->fixtureRigPosition(riserFx->id());
    QVERIFY2((riserPosAfterDrag - targetOnPlatform).length() < 0.1f,
             qPrintable(QString("dragging the riser fixture toward (%1,%2) landed it at "
                                "(%3,%4,%5)")
                        .arg(double(targetOnPlatform.x())).arg(double(targetOnPlatform.y()))
                        .arg(double(riserPosAfterDrag.x())).arg(double(riserPosAfterDrag.y()))
                        .arg(double(riserPosAfterDrag.z()))));

    // 6. Move the platform itself. No raw-QMouseEvent precedent exists for
    //    PlatformItem (it's QGraphicsSceneMouseEvent-based, not the
    //    widget-level QMouseEvent StructureStudioView tests use) -- the
    //    established main-plot convention this file already uses for a
    //    structural drag under offscreen QTest is: reposition the item,
    //    then call the real completion slot a release would invoke.
    PlatformItem *platItem = gv->m_platformItems.value(plat->id(), nullptr);
    QVERIFY(platItem != nullptr);
    const QVector3D deltaMetres(1.0f, 0.5f, 0.0f);
    const QPointF newOriginPx = gv->realPositionToPixels(
        (double(plat->originX()) + double(deltaMetres.x())) * 1000.0,
        (double(plat->originY()) + double(deltaMetres.y())) * 1000.0);
    platItem->setPos(newOriginPx);
    gv->slotPlatformMoved(platItem);

    QVERIFY2(qAbs(double(plat->originX()) - (3.0 + double(deltaMetres.x()))) < 0.05
                 && qAbs(double(plat->originY()) - (3.0 + double(deltaMetres.y()))) < 0.05,
             qPrintable(QString("moving the platform item did not update its own origin "
                                "(now %1,%2)").arg(double(plat->originX()))
                        .arg(double(plat->originY()))));

    // 7. The results match the request, for BOTH mounts, via their two
    //    different mechanisms (see the comment above the function).
    const QVector3D deckPosAfter = props->fixtureRigPosition(deckFx->id());
    QVERIFY2((deckPosAfter - (deckPosBefore + deltaMetres)).length() < 0.05f,
             qPrintable(QString("the deck fixture should have been carried by exactly "
                                "(%1,%2,0), moved by (%3,%4,%5) instead")
                        .arg(double(deltaMetres.x())).arg(double(deltaMetres.y()))
                        .arg(double(deckPosAfter.x() - deckPosBefore.x()))
                        .arg(double(deckPosAfter.y() - deckPosBefore.y()))
                        .arg(double(deckPosAfter.z() - deckPosBefore.z()))));

    const QVector3D riserPosAfterMove = props->fixtureRigPosition(riserFx->id());
    const QVector3D expectedRiserPos = targetOnPlatform + deltaMetres;
    QVERIFY2((riserPosAfterMove - expectedRiserPos).length() < 0.1f,
             qPrintable(QString("the riser fixture should follow the platform's new geometry "
                                "to (%1,%2,%3), sits at (%4,%5,%6) instead")
                        .arg(double(expectedRiserPos.x())).arg(double(expectedRiserPos.y()))
                        .arg(double(expectedRiserPos.z()))
                        .arg(double(riserPosAfterMove.x())).arg(double(riserPosAfterMove.y()))
                        .arg(double(riserPosAfterMove.z()))));

    delete mon;
    doc->deleteFixture(deckFx->id());
    doc->deleteFixture(riserFx->id());
    props->removePlatform(plat->id());
    delete doc;
}

/* Chains the stand/boom/crossbar workflow through the SAME real-UI
 * mechanisms the truss/bar test uses: a plot drop attaches the fixture to
 * the boom (MonitorGraphicsView::pipeUnderFixture()), and a real mouse
 * drag moves the crossbar (StructureStudioView::hitTestPipeBar()/
 * dragPipeBarTo()) -- both built to fix WORKFLOW-SUBTLETIES.md #11 (there
 * was no plot attach for a pipe at all, and a crossbar had none of a truss
 * bar's drag machinery; only its own "Edit Pipe" dialog could move it).
 */
void Monitor_Test::buildingAStandStepByStepThroughRealUiStaysConsistent()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    // 1. Add a fixture. No dialog-driven AddFixture path exists yet (see
    //    the truss/bar workflow test), so this uses the same direct
    //    construction every other test in this file relies on.
    Fixture *fx = new Fixture(doc);
    fx->setName("Workflow Boom Head"); fx->setChannels(1);
    QVERIFY(doc->addFixture(fx));
    props->setFixturePosition(fx->id(), 0, 0, QVector3D(0, 0, 0));

    QWidget parent;
    Monitor *mon = new Monitor(&parent, doc);
    MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    gv->resize(1230, 760);
    gv->setSnapDivisions(0);
    gv->setGridMetrics(1000.0);
    gv->setGridSize(QSize(40, 24));
    QVERIFY(gv->m_cellPixels > 0);
    gv->setViewPOV(MonitorGraphicsView::PovTop);
    gv->addFixture(fx->id(), QPointF(0, 0));

    // 2. Add a stand -- Monitor::slotAddStand() creates it bare, then opens
    //    its real "Lighting Studio Editor -- Stand N" dialog
    //    (slotEditStand()). Same embedded-canvas shape as the platform
    //    dialog (and the same findChild<QLineEdit*>() trap -- see the
    //    subtleties list -- reuse the defensive lookup).
    const int standsBefore = props->stands().count();
    driveDialog([](QWidget *modal) {
        QLineEdit *nameEdit = findRealLineEdit(modal);
        // Position X, Position Y, Top height, Base radius, Leg rotation --
        // in the order slotEditStand() adds them, scoped to nameEdit's own
        // container so the canvas's own spin boxes aren't counted too.
        QWidget *geomPane = nameEdit ? nameEdit->parentWidget() : nullptr;
        const QList<QDoubleSpinBox *> spins =
            geomPane ? geomPane->findChildren<QDoubleSpinBox *>() : QList<QDoubleSpinBox *>();
        if (nameEdit == nullptr || spins.count() != 5)
            return false;
        nameEdit->setText("Workflow Stand");
        spins[0]->setValue(3.0);   // position X
        spins[1]->setValue(3.0);   // position Y
        spins[2]->setValue(2.0);   // top height
        spins[3]->setValue(0.2);   // base radius
        spins[4]->setValue(0.0);   // leg rotation
        return true;
    });
    mon->slotAddStand();
    gv->updatePlatforms();

    QCOMPARE(props->stands().count(), standsBefore + 1);
    Stand *stand = props->stands().last();
    QCOMPARE(stand->name(), QString("Workflow Stand"));
    QVERIFY2(qAbs(double(stand->height()) - 2.0) < 0.01,
             qPrintable(QString("the dialog's Top height field did not reach the new stand "
                                "(height=%1)").arg(double(stand->height()))));

    // 3. Add a boom, then a crossbar on it -- both through
    //    Monitor::studioAddBar(kind=0, standId, ...), the exact function
    //    the Stand editor's own "Add Bar" button calls: the FIRST call (no
    //    vertical boom yet) creates the boom; the SECOND (boom now found)
    //    creates a horizontal crossbar hung on its top. In THIS order (both
    //    exist before anything attaches) on purpose: a crossbar used to be
    //    exactly coincident with its boom in Top view with no way to tell
    //    them apart (WORKFLOW-SUBTLETIES.md #14), which made this ordering
    //    a real hazard rather than an arbitrary test choice. Now that a
    //    fresh crossbar defaults to a real crossbarStandoff(), step 5 below
    //    can attach to the boom with the crossbar already in place and
    //    prove the two are actually distinguishable.
    mon->studioAddBar(0, stand->id(), nullptr);
    Pipe *boom = nullptr;
    foreach (Pipe *p, props->pipes())
        if (p->standId() == stand->id() && p->isVertical())
            boom = p;
    QVERIFY2(boom != nullptr, "studioAddBar(kind=0) did not create a boom on the stand");

    mon->studioAddBar(0, stand->id(), nullptr);
    Pipe *crossbar = nullptr;
    foreach (Pipe *p, props->pipes())
        if (p->parentPipeId() == boom->id())
            crossbar = p;
    QVERIFY2(crossbar != nullptr, "the second studioAddBar(kind=0) call did not add a crossbar");
    QVERIFY2(crossbar->crossbarStandoff() > 0.0f,
             "a fresh crossbar should default to a nonzero crossbarStandoff() now that "
             "recomputePipeAnchors() actually uses it to step clear of the boom");

    // 4. Mount the fixture on the boom -- the plot's real drop-attach path
    //    (MonitorGraphicsView::pipeUnderFixture()/slotFixtureMoved()), the
    //    pipe equivalent of the truss/bar test's own drop-attach step. The
    //    crossbar already exists (step 3) -- this is the actual proof that
    //    the standoff fix works, not just that it compiles: dropping here
    //    must land on the BOOM, not get grabbed by the crossbar sitting
    //    right next to it.
    gv->updateFixture(fx->id());
    MonitorFixtureItem *item = gv->m_fixtures.value(fx->id(), nullptr);
    QVERIFY(item != nullptr);
    const QPointF onBoomPx = gv->realPositionToPixels(double(boom->originX()) * 1000.0,
                                                       double(boom->originY()) * 1000.0);
    item->setPos(item->pos() + (onBoomPx - item->iconCentreScene()));
    gv->slotFixtureMoved(item);

    QVERIFY2(props->fixtureRigProps(fx->id()).pipeId == boom->id(),
             qPrintable(QString("dropping the fixture on the boom's plan disc did not "
                                "attach it to the boom (got pipeId=%1, boom=%2, crossbar=%3)")
                        .arg(props->fixtureRigProps(fx->id()).pipeId)
                        .arg(boom->id()).arg(crossbar->id())));
    const float offsetAfterAttach = props->fixtureRigProps(fx->id()).pipeOffset;
    QVERIFY2(qAbs(double(offsetAfterAttach) - double(boom->height()) * 0.85) < 0.01,
             qPrintable(QString("a fresh boom mount should default to 85%% up the boom "
                                "(%1), got %2 -- a Top-view drop cannot say WHERE up a "
                                "vertical boom, so this stays at attachFixtureToPipe()'s "
                                "own default")
                        .arg(double(boom->height()) * 0.85)
                        .arg(double(offsetAfterAttach))));

    // 5. Drag the fixture along the boom -- a real mousePress/Move/Release
    //    sequence on the Stand's own Structure Studio editor, the pipe
    //    equivalent of studioTrussDragMovesFixture(). No test anywhere in
    //    this file drags a pipe-mounted fixture with real mouse events yet.
    StructureStudioView sv(doc, StructureStudioView::StandKind, stand->id());
    sv.resize(700, 600);
    sv.setPlane(StructureStudioView::Front);
    sv.setLocked(false);   // mousePressEvent()'s drag arm is gated on this
    sv.reload();

    const QPointF pressPt = sv.w2s(boom->positionAt(offsetAfterAttach));
    const float targetOffset = boom->height() * 0.5f;
    const QPointF releasePt = sv.w2s(boom->positionAt(targetOffset));
    QMouseEvent press(QEvent::MouseButtonPress, pressPt, pressPt,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    sv.mousePressEvent(&press);
    QMouseEvent move(QEvent::MouseMove, releasePt, releasePt,
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    sv.mouseMoveEvent(&move);
    QMouseEvent release(QEvent::MouseButtonRelease, releasePt, releasePt,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    sv.mouseReleaseEvent(&release);

    const float offsetAfterDrag = props->fixtureRigProps(fx->id()).pipeOffset;
    QVERIFY2(qAbs(double(offsetAfterDrag) - double(targetOffset)) < 0.1,
             qPrintable(QString("dragging the fixture toward %1 up the boom landed it at %2")
                        .arg(double(targetOffset)).arg(double(offsetAfterDrag))));

    // 6. Drag the crossbar -- a real mousePress/Move/Release sequence on the
    //    Stand's own Structure Studio editor, the pipe-bar equivalent of
    //    draggingABarChangesItsPositionAlongTheParent(). Press ON THE
    //    CROSSBAR's own midpoint (not the boom, and not where the fixture
    //    now sits at 50% up -- the crossbar itself is still at the boom's
    //    default top, a different height, so there is no ambiguity between
    //    the two the way there was pressing at a bar's exact midpoint in
    //    the truss/bar test).
    const float upBoomBefore = crossbar->parentPipeOffset();
    const QVector3D crossbarMid = crossbar->positionAt(crossbar->length() * 0.5f);
    const float targetUpBoom = boom->length() * 0.5f;
    const QPointF pressPt3 = sv.w2s(crossbarMid);
    // Only the RELEASE point's height matters to dragPipeBarTo() for a
    // vertical parent boom (it projects the drag onto the boom's own Z
    // axis) -- keep the same X/Y as the press so this reads as "drag this
    // exact point straight up/down", not a sideways jump.
    const QPointF releasePt3 = sv.w2s(QVector3D(crossbarMid.x(), crossbarMid.y(),
                                                boom->positionAt(targetUpBoom).z()));
    QMouseEvent press3(QEvent::MouseButtonPress, pressPt3, pressPt3,
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    sv.mousePressEvent(&press3);
    QMouseEvent move3(QEvent::MouseMove, releasePt3, releasePt3,
                      Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    sv.mouseMoveEvent(&move3);
    QMouseEvent release3(QEvent::MouseButtonRelease, releasePt3, releasePt3,
                         Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    sv.mouseReleaseEvent(&release3);

    QVERIFY2(!qFuzzyCompare(crossbar->parentPipeOffset(), upBoomBefore),
             "dragging the crossbar on its own editor did not move it");
    QVERIFY2(qAbs(double(crossbar->parentPipeOffset()) - double(targetUpBoom)) < 0.05,
             qPrintable(QString("expected the crossbar ~%1 up the boom, got %2")
                        .arg(double(targetUpBoom))
                        .arg(double(crossbar->parentPipeOffset()))));

    // 7. Relocate the fixture -- another real drag on the boom, to a
    //    different offset again.
    sv.reload();
    const QPointF pressPt2 = sv.w2s(boom->positionAt(offsetAfterDrag));
    const float finalOffset = boom->height() * 0.2f;
    const QPointF releasePt2 = sv.w2s(boom->positionAt(finalOffset));
    QMouseEvent press2(QEvent::MouseButtonPress, pressPt2, pressPt2,
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    sv.mousePressEvent(&press2);
    QMouseEvent move2(QEvent::MouseMove, releasePt2, releasePt2,
                      Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    sv.mouseMoveEvent(&move2);
    QMouseEvent release2(QEvent::MouseButtonRelease, releasePt2, releasePt2,
                         Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    sv.mouseReleaseEvent(&release2);

    const float offsetAfterSecondDrag = props->fixtureRigProps(fx->id()).pipeOffset;
    QVERIFY2(qAbs(double(offsetAfterSecondDrag) - double(finalOffset)) < 0.1,
             qPrintable(QString("relocating the fixture toward %1 up the boom landed it at %2")
                        .arg(double(finalOffset)).arg(double(offsetAfterSecondDrag))));

    delete mon;
    doc->deleteFixture(fx->id());
    props->removePipe(crossbar->id());
    props->removePipe(boom->id());
    props->removeStand(stand->id());
    delete doc;
}

void Monitor_Test::aTrussRowInLayersOpensTheTrussNotItsGroup()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *t = props->addTruss();
    t->setName("T-1");
    t->setType(Truss::Vertical);
    t->setOrigin(QVector3D(1.6f, 1.3f, 0.0f)); t->setLength(3.0f);

    // The auto-group that stands for it -- WITH a frame, the broken case.
    const quint32 gid = props->nextGroupId();
    props->createGroup(gid, "T-1", 0, 0);
    props->setGroupAnchor(gid, QStringLiteral("truss"), t->id());
    props->setGroupHasFrame(gid, true);
    /* The truss must be a MEMBER of its own auto-group. The panel only treats a
       row as anchored when it can resolve the anchor's NAME from the group's
       items, so without this the row silently stops reading as a truss. */
    t->setGroupId(gid);
    QVERIFY(props->group(gid).hasFrame);
    QCOMPARE(props->group(gid).anchorKind, QStringLiteral("truss"));

    /* The view and the panel directly, not a whole Monitor. Monitor's teardown
       crashes on Qt5 AFTER the test body has finished -- nothing to do with
       what is under test here, which is purely the panel's routing decision,
       and the leaner harness is the honest scope for that anyway. */
        /* Scoped: the view outlives nothing. A MonitorGraphicsView on the stack
       destructs AFTER the statements below, so deleting the Doc first leaves
       its destructor talking to freed memory -- a Qt5-only SIGSEGV that Qt6
       happens to survive. */
    {
        MonitorGraphicsView view(doc);
        view.resize(800, 600);
        view.updateTrusses();     // so the map has an item to select
        MonitorLayersPanel panel(doc, &view);

        /* Watch what activating the row asks for. Both routes are observed, so the
           wrong one is a failure rather than an assumption. */
        quint32 trussAsked = Truss::invalidId();
        QObject::connect(&view, &MonitorGraphicsView::trussDoubleClicked,
                         [&trussAsked](quint32 tid) { trussAsked = tid; });
        bool studioOpened = false;
        QObject::connect(&view, &MonitorGraphicsView::studioGroupEditRequested,
                         [&studioOpened](quint32) { studioOpened = true; });

        /* Find the REAL row the panel built -- not a synthetic one -- so this
           exercises the tree the user actually clicks, badge and all. */
        panel.reload();
        QTreeWidgetItem *row = nullptr;
        QTreeWidgetItemIterator it(panel.m_tree);
        while (*it != nullptr)
        {
            if ((*it)->data(0, Qt::UserRole).toInt() == int(MonitorLayersPanel::NodeGroup)
                && (*it)->data(0, Qt::UserRole + 1).toUInt() == gid)
            { row = *it; break; }
            ++it;
        }
        QVERIFY2(row != nullptr, "the truss's group never got a row in the Layers tree");
        QVERIFY2(row->text(0).contains(QStringLiteral("on truss")),
                 qPrintable(QString("the row does not read as a truss: \"%1\"").arg(row->text(0))));

        panel.slotItemDoubleClicked(row, 0);

        QVERIFY2(!studioOpened,
                 "activating a truss row opened the group's Fixture Studio instead");
        QVERIFY2(trussAsked == t->id(),
                 qPrintable(QString("activating a row labelled \"%1\" did not open truss %2 "
                                    "(asked for %3)").arg(row->text(0)).arg(t->id())
                            .arg(trussAsked == Truss::invalidId() ? QStringLiteral("nothing")
                                                                  : QString::number(trussAsked))));

    }
    props->removeTruss(t->id());
    delete doc;
}

/* The component workflow, end to end: 12 steps x 8 strips is 96 placements by
   hand, and hand-placement is exactly how DS-3's eight ended up on a different
   plane from the other 88. Lay one out, save it, stamp the rest. */
void Monitor_Test::aSavedComponentStampsItsLayoutOntoOtherFixtures()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    auto makeBar = [&](const char *name, quint32 addr) -> Fixture * {
        Fixture *f = new Fixture(doc);
        f->setName(name);
        f->setChannels(3);
        f->setUniverse(0);
        f->setAddress(addr);
        return doc->addFixture(f) ? f : nullptr;
    };

    // --- the master: three bars in a row, laid out in a frame -------------
    QList<quint32> master;
    for (int i = 0; i < 3; i++)
    {
        Fixture *f = makeBar(qPrintable(QString("MasterBar%1").arg(i)), quint32(i * 3));
        QVERIFY(f != nullptr);
        master << f->id();
        props->setFixturePosition(f->id(), 0, 0, QVector3D(1000.0f * (i + 1), 2000.0f, 0.0f));
    }

    const quint32 gid = props->nextGroupId();
    props->createGroup(gid, "Step Master", 0, 0);
    props->setGroupHasFrame(gid, true);
    props->setGroupOrigin(gid, QVector3D(1.0f, 2.0f, 0.0f));

    // Distinct local offsets, so a stamp that loses the arrangement is visible.
    const QVector3D locals[3] = { QVector3D(0.0f, 0.0f, 0.0f),
                                  QVector3D(0.4f, 0.0f, 0.1f),
                                  QVector3D(0.8f, 0.0f, 0.2f) };
    for (int i = 0; i < 3; i++)
    {
        props->setFixtureGroup(master.at(i), gid);
        FixtureRigProps rp = props->fixtureRigProps(master.at(i));
        rp.groupLocal = locals[i];
        props->setFixtureRigProps(master.at(i), rp);
    }

    // --- save it as a component ------------------------------------------
    QString err;
    const QString path = StudioTemplate::saveToLibrary(doc, gid, "UnitTest Step", &err);
    QVERIFY2(!path.isEmpty(), qPrintable(QString("saveToLibrary failed: %1").arg(err)));

    const StudioTemplate::Info info = StudioTemplate::info(path);
    QVERIFY(info.isValid());
    QCOMPARE(info.roleCount(), 3);

    // --- stamp onto three DIFFERENT fixtures ------------------------------
    QList<quint32> target;
    for (int i = 0; i < 3; i++)
    {
        Fixture *f = makeBar(qPrintable(QString("TargetBar%1").arg(i)), quint32(100 + i * 3));
        QVERIFY(f != nullptr);
        target << f->id();
        props->setFixturePosition(f->id(), 0, 0, QVector3D(5000.0f, 6000.0f + 100.0f * i, 0.0f));
    }

    const quint32 newGid = StudioTemplate::stamp(doc, path, target, &err);
    QVERIFY2(newGid != 0, qPrintable(QString("stamp failed: %1").arg(err)));
    QVERIFY2(newGid != gid, "stamping reused the master group instead of making one");

    // The arrangement arrived: each target holds its role's local offset, and
    // they are members of the new group.
    for (int i = 0; i < 3; i++)
    {
        QCOMPARE(props->fixtureFrameGroup(target.at(i)), newGid);
        const QVector3D got = props->fixtureRigProps(target.at(i)).groupLocal;
        QVERIFY2((got - locals[i]).length() < 0.001f,
                 qPrintable(QString("role %1 landed at (%2,%3,%4), expected (%5,%6,%7)")
                            .arg(i).arg(got.x()).arg(got.y()).arg(got.z())
                            .arg(locals[i].x()).arg(locals[i].y()).arg(locals[i].z())));
    }

    // The master is untouched -- a stamp copies, it does not move the original.
    for (int i = 0; i < 3; i++)
        QCOMPARE(props->fixtureFrameGroup(master.at(i)), gid);

    QVERIFY(StudioTemplate::removeFile(path));   // don't leave it in the library
    delete doc;
}

/* "If I have fixtures on the rig unchecked .. still seeing fixtures on
   repaint." setMountedFixturesVisible() set visibility once; every other path
   that touches a fixture item recomputed it from the LAYER alone and put them
   all back. */
void Monitor_Test::hidingMountedFixturesSurvivesARefresh()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *t = props->addTruss();
    t->setName("Bar");
    t->setOrigin(QVector3D(1.0f, 2.0f, 3.0f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(4.0f);

    Fixture *fx = new Fixture(doc);
    fx->setName("Hung"); fx->setChannels(4);
    QVERIFY(doc->addFixture(fx));
    props->setFixturePosition(fx->id(), 0, 0, QVector3D(2000.0f, 2000.0f, 0.0f));
    FixtureRigProps rp;
    rp.trussId = t->id();
    rp.trussOffset = 1.0f;
    props->setFixtureRigProps(fx->id(), rp);

        /* Scoped: the view outlives nothing. A MonitorGraphicsView on the stack
       destructs AFTER the statements below, so deleting the Doc first leaves
       its destructor talking to freed memory -- a Qt5-only SIGSEGV that Qt6
       happens to survive. */
    {
        MonitorGraphicsView view(doc);
        view.resize(900, 640);
        view.addFixture(fx->id(), QPointF(2000.0, 2000.0));
        view.updateFixture(fx->id());   // sizes the icon from physical dims, as the app does
        MonitorFixtureItem *item = view.m_fixtures.value(fx->id(), nullptr);
        QVERIFY(item != nullptr);

        view.setMountedFixturesVisible(false);
        QVERIFY2(!item->isVisible(), "unchecking did not hide a mounted fixture");

        // Anything that refreshes item state must not undo it.
        view.refreshAllItems();
        QVERIFY2(!item->isVisible(),
                 "a refresh put the mounted fixture back after it was unchecked");

        view.updateFixture(fx->id());
        QVERIFY2(!item->isVisible(),
                 "updating the fixture put it back after it was unchecked");

        // ...and a fixture ADDED while unchecked must not appear either.
        Fixture *fx2 = new Fixture(doc);
        fx2->setName("Hung2"); fx2->setChannels(4); fx2->setAddress(10);
        QVERIFY(doc->addFixture(fx2));
        props->setFixturePosition(fx2->id(), 0, 0, QVector3D(2500.0f, 2000.0f, 0.0f));
        FixtureRigProps rp2;
        rp2.trussId = t->id();
        rp2.trussOffset = 2.0f;
        props->setFixtureRigProps(fx2->id(), rp2);
        view.addFixture(fx2->id(), QPointF(2500.0, 2000.0));
        view.updateFixture(fx2->id());
        MonitorFixtureItem *item2 = view.m_fixtures.value(fx2->id(), nullptr);
        QVERIFY(item2 != nullptr);
        QVERIFY2(!item2->isVisible(),
                 "a mounted fixture added while unchecked appeared anyway");

        /* Re-checking restores them -- UNLESS they bury the structure, which
           these two do (two wide icons on a thin 4 m truss cover about half of
           it from above). Burial is "not shown ever", so the count badge is
           what comes back instead. Asserted against the measurement rather than
           hard-coded, so this keeps testing the TOGGLE either way. */
        view.setMountedFixturesVisible(true);
        const bool buries =
            view.structureCoverageByItsFixtures(QStringLiteral("truss"), t->id()) > 0.40;
        if (buries)
        {
            QVERIFY2(view.m_countBadges.contains(QStringLiteral("truss:%1").arg(t->id())),
                     "buried fixtures came back with no count badge to stand for them");
        }
        else
        {
            QVERIFY(item->isVisible());
            QVERIFY(item2->isVisible());
        }

    }
    props->removeTruss(t->id());
    delete doc;
}

/* "Put bars in T1 .. and they're not visible on the rig display." */
void Monitor_Test::aBarOnATowerIsDrawnInTheRigView()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *tower = props->addTruss();
    tower->setName("T-1");
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    tower->setLength(3.0f);
    tower->setWidth(0.29f);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(800, 640);
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(20.0, 20.0);
    v.setAmbient(1.0);
    v.grab();
    v.reload();
    v.m_zoomed = true;                       // hold the framing
    const QImage before = v.grab().toImage();

    // Hang a crossbar off it, the way the truss editor's "Bar on truss" does.
    Truss *bar = props->addTruss();
    bar->setName("Bar-1");
    bar->setParentTrussId(tower->id());
    bar->setParentOffset(2.0f);              // 2 m up the tower
    bar->setBarFace(Truss::FaceDownstage);
    bar->setBarStandoff(0.0f);
    bar->setBarRun(Truss::RunAcross);
    bar->setLength(2.0f);
    bar->setWidth(0.29f);
    props->recomputeChildTrusses();          // derive its world geometry

    QVERIFY2(bar->isChildBar(), "the bar did not register as a child bar");

    v.reload();
    v.m_zoomed = true;
    const QImage after = v.grab().toImage();

    int changed = 0;
    for (int y = 0; y < after.height(); ++y)
        for (int x = 0; x < after.width(); ++x)
        {
            const QColor a = after.pixelColor(x, y), b = before.pixelColor(x, y);
            if (qAbs(a.red() - b.red()) + qAbs(a.green() - b.green())
                + qAbs(a.blue() - b.blue()) > 24)
                ++changed;
        }

    QVERIFY2(changed > 50,
             qPrintable(QString("hanging a bar on the tower changed %1 pixels -- "
                                "it is not being drawn (bar origin %2,%3,%4 len %5)")
                        .arg(changed)
                        .arg(bar->origin().x()).arg(bar->origin().y())
                        .arg(bar->origin().z()).arg(bar->length())));

    props->removeTruss(bar->id());
    props->removeTruss(tower->id());
    delete doc;
}

/* "Truss on T1 is STILL not part of T1 .. even tho if I double click it on
   the studio view it thinks it is. It should show up in the truss editor as
   something I can place." aBarOnATowerIsDrawnInTheRigView() (above) proved
   bars draw in the whole-rig StageKind overview -- but that view loops over
   every Truss in the doc independently, so a bar was ALWAYS going to be
   drawn there as just another entry in that loop, proving nothing about
   whether ITS OWN PARENT's per-object editor (StructureStudioView opened at
   Kind::TrussKind, id = the parent) draws it. That is the dialog the report
   actually points at ("the truss editor"), a completely separate code path
   (drawOneStructure()'s TrussKind branch draws only props->truss(id) itself)
   that had never been taught to recurse into its own children. */
void Monitor_Test::aBarShowsInItsParentTrusssOwnEditor()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *t1 = props->addTruss();
    t1->setName("T-1");
    t1->setType(Truss::Vertical);
    t1->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    t1->setLength(3.0f);
    t1->setWidth(0.29f);

    // Opened the same way double-clicking T-1 does: Kind::TrussKind, id=T-1.
    StructureStudioView v(doc, StructureStudioView::TrussKind, t1->id());
    v.resize(800, 640);
    v.setPlane(StructureStudioView::Front);
    v.setAmbient(1.0);
    v.grab();
    v.reload();
    v.m_zoomed = true;                       // hold the framing
    const QImage before = v.grab().toImage();

    Truss *bar = props->addTruss();
    bar->setName("Bar 1");
    bar->setParentTrussId(t1->id());
    bar->setParentOffset(1.5f);
    bar->setBarFace(Truss::FaceDownstage);
    bar->setBarStandoff(0.0f);
    bar->setBarRun(Truss::RunAcross);
    bar->setLength(1.0f);
    bar->setWidth(0.29f);
    props->recomputeChildTrusses();

    v.reload();
    v.m_zoomed = true;
    const QImage after = v.grab().toImage();

    int changed = 0;
    for (int y = 0; y < after.height(); ++y)
        for (int x = 0; x < after.width(); ++x)
        {
            const QColor a = after.pixelColor(x, y), b = before.pixelColor(x, y);
            if (qAbs(a.red() - b.red()) + qAbs(a.green() - b.green())
                + qAbs(a.blue() - b.blue()) > 24)
                ++changed;
        }

    QVERIFY2(changed > 50,
             qPrintable(QString("T-1's own editor changed only %1 pixels when a "
                                "bar was hung on it -- the bar is not drawn there, "
                                "just listed beside a blank canvas").arg(changed)));

    props->removeTruss(bar->id());
    props->removeTruss(t1->id());
    delete doc;
}

/* A Horizontal truss's default bar used to render EXACTLY on top of the
 * truss's own line in Top view: FaceBottom offsets purely along Z, which
 * Top view cannot show at all, so recomputeChildTrusses()'s `base = attach
 * + faceN * (half + barStandoff)` got zero X/Y contribution no matter how
 * large barStandoff was set (see WORKFLOW-SUBTLETIES.md #1). Fixed by
 * defaulting to FaceDownstage instead -- a real Y offset, the same face
 * the Vertical-parent case already relies on for its own separation --
 * plus a small explicit barStandoff on top of it, now that it actually
 * does something.
 */
void Monitor_Test::aBarOnAHorizontalTrussDefaultsClearOfItInTopView()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *t = props->addTruss();
    t->setName("T-1");
    t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(3.0f, 3.0f, 4.0f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(4.0f);
    t->setWidth(0.29f);

    QWidget parent;
    Monitor mon(&parent, doc);

    // The exact function the truss's own "Add > Bar..." context-menu action
    // (and slotAddBarToTruss()) calls. No dialog of its own -- that flow is
    // already covered by buildingARigStepByStepThroughRealUiStaysConsistent().
    const quint32 barId = mon.createBarOnTruss(t->id(), -1.0f);
    QVERIFY2(barId != Truss::invalidId(), "createBarOnTruss() did not create a bar");
    Truss *bar = props->truss(barId);
    QVERIFY(bar != nullptr);

    QCOMPARE(bar->barFace(), Truss::FaceDownstage);
    QCOMPARE(bar->barRun(), Truss::RunAlong);
    QVERIFY2(bar->barStandoff() > 0.0f,
             "a bar on a Horizontal truss should default to a nonzero standoff "
             "now that FaceDownstage makes one actually visible");

    // The real check: a Top-view hit-test can tell the two apart. Press
    // where the TRUSS's own centreline runs -- must NOT find the bar --
    // and press where the BAR actually draws -- must find it.
    StructureStudioView view(doc, StructureStudioView::TrussKind, t->id());
    view.resize(800, 640);
    view.setPlane(StructureStudioView::Top);
    view.reload();
    view.m_zoomed = true;

    const QPointF onTrussCentreline = view.w2s(t->positionAt(t->length() * 0.5f));
    QVERIFY2(view.hitTestBar(onTrussCentreline) == Truss::invalidId(),
             "the bar still hit-tests at the truss's own centreline -- not "
             "actually separated in Top view");

    const QPointF onBar = view.w2s(bar->positionAt(bar->length() * 0.5f));
    QCOMPARE(view.hitTestBar(onBar), barId);

    props->removeTruss(barId);
    props->removeTruss(t->id());
    delete doc;
}

/* "not selectable at all with mouse to move it in position." Half of the
   same report that got the bar drawn (aBarShowsInItsParentTrusssOwnEditor,
   above) -- visible is not the same as clickable, and nothing hit-tested a
   bar at all before this. */
void Monitor_Test::hitTestBarFindsANearbyBar()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *t1 = props->addTruss();
    t1->setName("T-1");
    t1->setType(Truss::Vertical);
    t1->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    t1->setLength(3.0f);
    t1->setWidth(0.29f);

    Truss *bar = props->addTruss();
    bar->setName("Bar 1");
    bar->setParentTrussId(t1->id());
    bar->setParentOffset(1.5f);
    bar->setBarFace(Truss::FaceDownstage);
    bar->setBarStandoff(0.0f);
    bar->setBarRun(Truss::RunAcross);
    bar->setLength(1.0f);
    bar->setWidth(0.29f);
    props->recomputeChildTrusses();

    StructureStudioView v(doc, StructureStudioView::TrussKind, t1->id());
    v.resize(800, 640);
    v.setPlane(StructureStudioView::Front);
    v.reload();
    v.m_zoomed = true;

    const QPointF onBar = v.w2s(bar->positionAt(bar->length() * 0.5f));
    QCOMPARE(v.hitTestBar(onBar), bar->id());

    /* The actual reported bug: "I canNOT move it along the truss." A fixed
       ~9 px radius around the CENTRELINE missed a click anywhere else in the
       bar's own drawn rectangle once it rendered wider than that at a normal
       zoom -- which a real truss profile does. Click near the EDGE of the
       drawn width, not the centreline, and confirm it still finds it: the
       width this asserts against is read from the same m_scale the paint
       code itself uses, so this fails honestly if the two ever disagree
       again rather than passing by coincidence. */
    const double halfWidthPx = qMax(6.0, double(bar->width()) * v.m_scale) / 2.0;
    QVERIFY2(halfWidthPx > 9.0,
             "test rig's bar isn't drawn wide enough to exercise the old bug -- "
             "widen bar->width() or the view size");
    const QPointF nearEdge = onBar + QPointF(0.0, halfWidthPx * 0.8);
    QCOMPARE(v.hitTestBar(nearEdge), bar->id());

    // ...but it does not become infinitely wide either.
    const QPointF wellPast = onBar + QPointF(0.0, halfWidthPx + 40.0);
    QCOMPARE(v.hitTestBar(wellPast), quint32(Truss::invalidId()));

    // Nowhere near anything drawn: no false hit.
    QCOMPARE(v.hitTestBar(QPointF(-500.0, -500.0)), quint32(Truss::invalidId()));

    // Only THIS truss's own bars are hit-testable from its own editor — a bar
    // hanging off some other truss must not answer here.
    Truss *elsewhere = props->addTruss();
    elsewhere->setName("T-2");
    elsewhere->setType(Truss::Vertical);
    elsewhere->setOrigin(QVector3D(9.0f, 3.0f, 0.0f));
    elsewhere->setLength(3.0f);
    Truss *otherBar = props->addTruss();
    otherBar->setParentTrussId(elsewhere->id());
    otherBar->setParentOffset(1.5f);
    otherBar->setBarFace(Truss::FaceDownstage);
    otherBar->setBarRun(Truss::RunAcross);
    otherBar->setLength(1.0f);
    props->recomputeChildTrusses();
    QCOMPARE(v.hitTestBar(v.w2s(otherBar->positionAt(otherBar->length() * 0.5f))),
             quint32(Truss::invalidId()));

    props->removeTruss(otherBar->id());
    props->removeTruss(elsewhere->id());
    props->removeTruss(bar->id());
    props->removeTruss(t1->id());
    delete doc;
}

/* "when I select it .. it does't hilight nor does the right side change to
   the bar data." The canvas half of that: a real mouse press on the bar has
   to leave the view in a state a highlight-drawing pass and an inspector
   panel can both actually read back -- m_selectedBarId/m_dragBarId (the
   drawing side, see drawOneStructure()'s amber-vs-steel check) and the
   barSelected signal (what monitor.cpp's inspector panel connects to). The
   panel wiring itself lives in Monitor and isn't reachable from here; this
   is the view-level contract that wiring depends on. */
void Monitor_Test::clickingABarSelectsAndHighlightsIt()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *t1 = props->addTruss();
    t1->setName("T-1");
    t1->setType(Truss::Vertical);
    t1->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    t1->setLength(3.0f);
    t1->setWidth(0.29f);

    Truss *bar = props->addTruss();
    bar->setName("Bar 1");
    bar->setParentTrussId(t1->id());
    bar->setParentOffset(1.5f);
    bar->setBarFace(Truss::FaceDownstage);
    bar->setBarRun(Truss::RunAcross);
    bar->setLength(1.0f);
    bar->setWidth(0.29f);
    props->recomputeChildTrusses();

    StructureStudioView v(doc, StructureStudioView::TrussKind, t1->id());
    v.resize(800, 640);
    v.setPlane(StructureStudioView::Front);
    v.setLocked(false);
    v.reload();
    v.m_zoomed = true;

    QSignalSpy spy(&v, &StructureStudioView::barSelected);
    const QPointF onBar = v.w2s(bar->positionAt(bar->length() * 0.5f));
    QMouseEvent press(QEvent::MouseButtonPress, onBar, onBar,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    v.mousePressEvent(&press);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toUInt(), bar->id());
    QCOMPARE(v.m_selectedBarId, bar->id());
    QCOMPARE(v.m_dragBarId, bar->id());
    // A bar and a fixture are never highlighted together.
    QVERIFY(v.m_highlight.isEmpty());

    QMouseEvent release(QEvent::MouseButtonRelease, onBar, onBar,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    v.mouseReleaseEvent(&release);
    // Selection (the highlight) survives release; only the DRAG state ends.
    QCOMPARE(v.m_selectedBarId, bar->id());
    QCOMPARE(v.m_dragBarId, quint32(Truss::invalidId()));

    props->removeTruss(bar->id());
    props->removeTruss(t1->id());
    delete doc;
}

void Monitor_Test::doubleClickingABarActivatesIt()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *t1 = props->addTruss();
    t1->setName("T-1");
    t1->setType(Truss::Vertical);
    t1->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    t1->setLength(3.0f);
    t1->setWidth(0.29f);

    Truss *bar = props->addTruss();
    bar->setName("Bar 1");
    bar->setParentTrussId(t1->id());
    bar->setParentOffset(1.5f);
    bar->setBarFace(Truss::FaceDownstage);
    bar->setBarRun(Truss::RunAcross);
    bar->setLength(1.0f);
    bar->setWidth(0.29f);
    props->recomputeChildTrusses();

    StructureStudioView v(doc, StructureStudioView::TrussKind, t1->id());
    v.resize(800, 640);
    v.setPlane(StructureStudioView::Front);
    v.setLocked(false);
    v.reload();
    v.m_zoomed = true;

    QSignalSpy spy(&v, &StructureStudioView::barActivated);
    const QPointF onBar = v.w2s(bar->positionAt(bar->length() * 0.5f));

    // Full real event sequence a double-click actually generates.
    QMouseEvent press1(QEvent::MouseButtonPress, onBar, onBar,
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    v.mousePressEvent(&press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, onBar, onBar,
                         Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    v.mouseReleaseEvent(&release1);
    QMouseEvent dbl(QEvent::MouseButtonDblClick, onBar, onBar,
                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    v.mouseDoubleClickEvent(&dbl);
    QMouseEvent release2(QEvent::MouseButtonRelease, onBar, onBar,
                         Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    v.mouseReleaseEvent(&release2);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toUInt(), bar->id());

    props->removeTruss(bar->id());
    props->removeTruss(t1->id());
    delete doc;
}

void Monitor_Test::draggingABarChangesItsPositionAlongTheParent()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *t1 = props->addTruss();
    t1->setName("T-1");
    t1->setType(Truss::Vertical);
    t1->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    t1->setLength(3.0f);
    t1->setWidth(0.29f);

    Truss *bar = props->addTruss();
    bar->setName("Bar 1");
    bar->setParentTrussId(t1->id());
    bar->setParentOffset(1.0f);
    bar->setBarFace(Truss::FaceDownstage);
    bar->setBarStandoff(0.0f);
    bar->setBarRun(Truss::RunAcross);
    bar->setLength(1.0f);
    bar->setWidth(0.29f);
    props->recomputeChildTrusses();

    StructureStudioView v(doc, StructureStudioView::TrussKind, t1->id());
    v.resize(800, 640);
    v.setPlane(StructureStudioView::Front);
    v.reload();
    v.m_zoomed = true;

    // T-1 is Vertical, so its run is Z: drag half a metre UP ("it must be
    // vertically movable").
    const QPointF target = v.w2s(bar->origin() + QVector3D(0.0f, 0.0f, 0.5f));
    QVERIFY(v.dragBarTo(bar->id(), target));
    QVERIFY2(qAbs(double(bar->parentOffset()) - 1.5) < 0.05,
             qPrintable(QString("expected ~1.5 m along the run after a 0.5 m drag, "
                                "got %1").arg(double(bar->parentOffset()))));

    /* "horizontally .. so it can be more on one side than the other" --
       barCrossShift, applied along the bar's OWN run direction (already
       computed and stored by recomputeChildTrusses() as bar->direction()),
       not the parent's. Bar 1 is RunAcross on a Vertical parent, so its own
       run is horizontal (X here) -- drag sideways and confirm the shift
       actually lands where recomputeChildTrusses()'s own formula
       (newOrigin = base - runVec * (length/2 - crossShift)) says it will:
       the origin moves by the SAME delta along bar->direction(). */
    const QVector3D beforeOrigin = bar->origin();
    const float beforeCross = bar->barCrossShift();
    const QPointF sideways = v.w2s(bar->origin() + QVector3D(0.3f, 0.0f, 0.0f));
    QVERIFY(v.dragBarTo(bar->id(), sideways));
    QVERIFY2(!qFuzzyCompare(bar->barCrossShift(), beforeCross),
             "a sideways drag did not change barCrossShift at all");
    const QPointF d = bar->direction();
    const QVector3D expectedOrigin = beforeOrigin
        + QVector3D(float(d.x()), float(d.y()), 0.0f) * (bar->barCrossShift() - beforeCross);
    QVERIFY2((bar->origin() - expectedOrigin).length() < 0.05f,
             qPrintable(QString("origin %1,%2,%3 doesn't match recomputeChildTrusses()'s "
                                "own cross-shift formula (expected %4,%5,%6)")
                        .arg(double(bar->origin().x())).arg(double(bar->origin().y()))
                        .arg(double(bar->origin().z()))
                        .arg(double(expectedOrigin.x())).arg(double(expectedOrigin.y()))
                        .arg(double(expectedOrigin.z()))));

    /* The PARENT truss's lock must NOT block this -- a bar is its own
       structural object, not a fixture riding one; the main 2D plot already
       evaluates a bar's own drag-lock independently of its parent
       (monitorgraphicsview.cpp's Trusses block), and inheriting the
       parent's lock here made every bar on this rig's four (deliberately
       locked) main trusses permanently unmovable regardless of the bar's
       own state. */
    t1->setLocked(true);
    QVERIFY2(v.dragBarTo(bar->id(), v.w2s(bar->origin() + QVector3D(0.0f, 0.0f, 0.3f))),
             "a bar on an unlocked bar but a locked PARENT truss should still drag");
    t1->setLocked(false);

    // The bar's OWN lock is what actually has to stop it.
    bar->setLocked(true);
    const float beforeLockedBar = bar->parentOffset();
    QVERIFY2(v.dragBarTo(bar->id(), v.w2s(bar->origin() + QVector3D(0.0f, 0.0f, 1.0f))) == false,
             "a locked bar was still draggable");
    QCOMPARE(bar->parentOffset(), beforeLockedBar);

    props->removeTruss(bar->id());
    props->removeTruss(t1->id());
    delete doc;
}

/* "Put bars in T1 .. and they're not visible on the rig display." The bars are
   FIXTURES on the tower, and the inspector shows them as Placement = Inside --
   rigged within the structure's volume rather than bolted to its face. */
void Monitor_Test::aFixtureInsideATrussIsStillDrawn()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *tower = props->addTruss();
    tower->setName("T-1");
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    tower->setLength(3.0f);
    tower->setWidth(0.29f);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(800, 640);
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(20.0, 20.0);
    v.setAmbient(1.0);
    v.grab();
    v.reload();
    v.m_zoomed = true;
    const QImage before = v.grab().toImage();

    Fixture *fx = makeMover(doc, 14.0, 0, "T1-B1", QVector3D(3.0f, 3.0f, 1.5f));
    QVERIFY(fx != nullptr);
    FixtureRigProps rp;
    rp.trussId = tower->id();
    rp.trussOffset = 1.5f;                       // half way up
    rp.placement = FixtureRigProps::Inside;      // what the inspector shows
    props->setFixtureRigProps(fx->id(), rp);

    v.reload();
    v.m_zoomed = true;
    const QImage after = v.grab().toImage();

    int changed = 0;
    for (int y = 0; y < after.height(); ++y)
        for (int x = 0; x < after.width(); ++x)
        {
            const QColor a = after.pixelColor(x, y), b = before.pixelColor(x, y);
            if (qAbs(a.red() - b.red()) + qAbs(a.green() - b.green())
                + qAbs(a.blue() - b.blue()) > 24)
                ++changed;
        }

    QVERIFY2(changed > 50,
             qPrintable(QString("a fixture rigged inside the tower changed %1 pixels "
                                "-- it is not visible in the rig view").arg(changed)));

    doc->deleteFixture(fx->id());
    props->removeTruss(tower->id());
    delete doc;
}

/* A vertical truss seen from ABOVE is a small square and its own fixtures bury
   it; the same truss from the FRONT is tall and they cover very little. So the
   rule is per structure, per view, and all-or-nothing. */
void Monitor_Test::fixturesThatBuryTheirStructureBecomeACount()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *tower = props->addTruss();
    tower->setName("T-1");
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    tower->setLength(3.0f);
    /* An ordinary 0.29 m truss -- the width a real tower is. It only failed to
       qualify while coverage was measured against boundingRect(), which pads a
       vertical truss out to cover its base plate: four times the real area, so
       a tower buried in its own fixtures reported 31%. */
    tower->setWidth(0.29f);

    QList<quint32> bars;
    for (int i = 0; i < 2; i++)
    {
        Fixture *f = makeMover(doc, 14.0, quint32(i * 8), qPrintable(QString("T1-B%1").arg(i + 1)),
                               QVector3D(3.0f, 3.0f, 1.0f + i));
        QVERIFY(f != nullptr);
        bars << f->id();
        FixtureRigProps rp;
        rp.trussId = tower->id();
        rp.trussOffset = 1.0f + i;
        props->setFixtureRigProps(f->id(), rp);
    }

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 7));        // big cells: no minimum-area clamp
        view.updateTrusses();
        foreach (quint32 fid, bars)
            view.addFixture(fid, QPointF(3000.0, 3000.0));

        // TOP view: the tower is a small square, the fixtures sit on it.
        view.setViewPOV(MonitorGraphicsView::PovTop);
        view.refreshItemLayerState();
        const double topCover =
            view.structureCoverageByItsFixtures(QStringLiteral("truss"), tower->id());
        QVERIFY2(topCover > 0.40,
                 qPrintable(QString("from above the fixtures cover only %1 of the "
                                    "tower -- the test rig is not modelling burial")
                            .arg(topCover)));

        foreach (quint32 fid, bars)
        {
            MonitorFixtureItem *it = view.m_fixtures.value(fid, nullptr);
            QVERIFY(it != nullptr);
            QVERIFY2(!it->isVisible(),
                     "a fixture burying its truss from above is still drawn");
        }
        QVERIFY2(view.m_countBadges.contains(QStringLiteral("truss:%1").arg(tower->id())),
                 "no count badge appeared on the buried truss");

        // FRONT view: the tower is tall; the same fixtures cover little of it.
        view.setViewPOV(MonitorGraphicsView::PovFront);
        view.refreshItemLayerState();
        const double frontCover =
            view.structureCoverageByItsFixtures(QStringLiteral("truss"), tower->id());
        QVERIFY2(frontCover < topCover,
                 qPrintable(QString("front coverage %1 should be less than top %2")
                            .arg(frontCover).arg(topCover)));
    }

    foreach (quint32 fid, bars)
        doc->deleteFixture(fid);
    props->removeTruss(tower->id());
    delete doc;
}

/* "t1 is now giving me the numbercircle combo on a side view when the
   fixtures are not obscuring the truss." T1 runs along X; a Side view drops
   X, so updateTrusses() clamps its near-zero projected length up to pxWid
   ("end-on trusses stay visible") -- correct for DRAWING it, but that left a
   tiny pxWid-square footprint for coverage math to measure against, and any
   fixture on the real run (spread along X, which Side also collapses)
   trivially "covered" more than 40% of a square that was never the truss's
   real on-screen size. Front, which keeps X, must not be fooled the same way. */
void Monitor_Test::endOnTrussIsNotBuriedByItsOwnFixtures()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *t1 = props->addTruss();
    t1->setName("T1");
    t1->setType(Truss::Horizontal);
    t1->setOrigin(QVector3D(3.0f, 3.0f, 3.0f));
    t1->setDirection(QPointF(1.0, 0.0));         // runs along X
    t1->setLength(3.0f);
    t1->setWidth(0.05f);

    QList<quint32> heads;
    for (int i = 0; i < 2; i++)
    {
        Fixture *f = makeMover(doc, 14.0, quint32(i * 8), qPrintable(QString("T1-B%1").arg(i + 1)),
                               QVector3D(3.0f + float(i), 3.0f, 3.0f));
        QVERIFY(f != nullptr);
        heads << f->id();
        FixtureRigProps rp;
        rp.trussId = t1->id();
        rp.trussOffset = 0.5f + float(i) * 2.0f;   // well apart along the 3 m run
        props->setFixtureRigProps(f->id(), rp);
    }

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 7));
        view.updateTrusses();
        foreach (quint32 fid, heads)
            view.addFixture(fid, QPointF(3000.0, 3000.0));

        // FRONT keeps X: the truss draws at its real length, nowhere near buried.
        view.setViewPOV(MonitorGraphicsView::PovFront);
        view.refreshItemLayerState();
        const double frontCover =
            view.structureCoverageByItsFixtures(QStringLiteral("truss"), t1->id());
        QVERIFY2(frontCover < 0.40,
                 qPrintable(QString("front (real length) coverage %1 -- test rig "
                                    "is not modelling this truss correctly").arg(frontCover)));

        // SIDE drops X: T1 draws end-on, but that is a drawing fallback, not a
        // real footprint -- its two heads (apart along the run Side just
        // collapsed) must not read as burying it.
        view.setViewPOV(MonitorGraphicsView::PovSide);
        view.refreshItemLayerState();
        const double sideCover =
            view.structureCoverageByItsFixtures(QStringLiteral("truss"), t1->id());
        QVERIFY2(sideCover < 0.40,
                 qPrintable(QString("side (end-on) coverage %1 -- an end-on truss "
                                    "reads as buried by fixtures that do not "
                                    "actually obscure it").arg(sideCover)));
        QVERIFY2(!view.m_countBadges.contains(QStringLiteral("truss:%1").arg(t1->id())),
                 "a spurious count badge covered an end-on truss's own fixtures");
        foreach (quint32 fid, heads)
        {
            MonitorFixtureItem *it = view.m_fixtures.value(fid, nullptr);
            QVERIFY(it != nullptr);
            QVERIFY2(it->isVisible(),
                     "an end-on truss's fixtures were hidden behind a false bury");
        }
    }

    foreach (quint32 fid, heads)
        doc->deleteFixture(fid);
    props->removeTruss(t1->id());
    delete doc;
}

/* Branson's steps: "added T2-B1 and 2 .. to universe 8; opened T2; added two
   fixtures and placed; closed editor; bubble not updated". The fixtures were
   patched in the Fixture Manager and never dragged onto the plot, so they had
   no icon there. Mounting them from the editor only updated an icon that did
   not exist: they were on the truss, absent from the plot, and uncounted by
   its bubble. */
void Monitor_Test::aFixtureMountedFromAnEditorLandsOnThePlot()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *tower = props->addTruss();
    tower->setName("T-2");
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    tower->setLength(3.0f);
    tower->setWidth(0.29f);

    auto mountOnTower = [&](quint32 fid, float offset) {
        FixtureRigProps rp = props->fixtureRigProps(fid);
        rp.trussId = tower->id();
        rp.trussOffset = offset;
        props->setFixtureRigProps(fid, rp);
    };

    // Two already on the plot, burying the tower from above.
    QList<quint32> placed;
    for (int i = 0; i < 2; i++)
    {
        Fixture *f = makeMover(doc, 14.0, quint32(i * 8), qPrintable(QString("T2-B%1").arg(i + 1)),
                               QVector3D(3.0f, 3.0f, 1.0f + i));
        QVERIFY(f != nullptr);
        mountOnTower(f->id(), 1.0f + i);
        placed << f->id();
    }
    // Two just patched: in the workspace, with no place on the plot at all.
    QList<quint32> fresh;
    for (int i = 0; i < 2; i++)
    {
        Fixture *f = makeMover(doc, 14.0, quint32(100 + i * 8), qPrintable(QString("New %1").arg(i + 1)),
                               QVector3D(0.0f, 0.0f, 0.0f));
        QVERIFY(f != nullptr);
        props->removeFixture(f->id());          // patched, never placed
        fresh << f->id();
    }

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 7));
        view.updateTrusses();
        foreach (quint32 fid, placed)
            view.addFixture(fid, QPointF(3000.0, 3000.0));
        view.setViewPOV(MonitorGraphicsView::PovTop);
        view.refreshItemLayerState();

        const QString key = QStringLiteral("truss:%1").arg(tower->id());
        auto badgeCount = [&]() -> int {
            QGraphicsItemGroup *g = view.m_countBadges.value(key, nullptr);
            if (g == nullptr)
                return 0;
            foreach (QGraphicsItem *c, g->childItems())
                if (QGraphicsSimpleTextItem *t = dynamic_cast<QGraphicsSimpleTextItem *>(c))
                    return t->text().toInt();
            return 0;
        };
        QCOMPARE(badgeCount(), 2);

        // The editor mounts them and tells the plot, exactly as Monitor does.
        foreach (quint32 fid, fresh)
        {
            mountOnTower(fid, 2.0f);
            view.ensureFixtureOnPlot(fid);
            view.updateFixture(fid);
        }
        view.followParentTrusses();
        QCoreApplication::processEvents();

        foreach (quint32 fid, fresh)
        {
            QVERIFY2(view.m_fixtures.contains(fid),
                     "a fixture mounted from the editor never appeared on the plot");
            const QVector3D w = props->fixtureRigPosition(fid);
            QVERIFY2(qAbs(double(w.x()) - 3.0) < 0.05 && qAbs(double(w.y()) - 3.0) < 0.05,
                     qPrintable(QString("it landed at %1,%2 rather than on the tower")
                                .arg(double(w.x())).arg(double(w.y()))));
        }
        QVERIFY2(badgeCount() == 4,
                 qPrintable(QString("the tower now carries four fixtures; its bubble says %1")
                            .arg(badgeCount())));
    }

    foreach (quint32 fid, placed + fresh)
        doc->deleteFixture(fid);
    props->removeTruss(tower->id());
    delete doc;
}

/* The truss editor's path, end to end at the plot: it mounts through
   MonitorProperties and hands the plot an updateFixture(); accepting calls
   followParentTrusses(), whose updateTrusses() ends in the layer pass that
   recounts. A tower that gains a fixture has to say so, and hide the newcomer
   with the rest. */
void Monitor_Test::anEditedStructureRecountsItsFixtures()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *tower = props->addTruss();
    tower->setName("T-1");
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    tower->setLength(3.0f);
    tower->setWidth(0.29f);

    auto mountOnTower = [&](quint32 fid, float offset) {
        FixtureRigProps rp = props->fixtureRigProps(fid);
        rp.trussId = tower->id();
        rp.trussOffset = offset;
        props->setFixtureRigProps(fid, rp);
    };

    QList<quint32> fids;
    for (int i = 0; i < 3; i++)
    {
        Fixture *f = makeMover(doc, 14.0, quint32(i * 8), qPrintable(QString("T1-S%1").arg(i + 1)),
                               QVector3D(3.0f, 3.0f, 1.0f));
        QVERIFY(f != nullptr);
        fids << f->id();
    }
    mountOnTower(fids[0], 1.0f);
    mountOnTower(fids[1], 2.0f);

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 7));
        view.updateTrusses();
        view.addFixture(fids[0], QPointF(3000.0, 3000.0));
        view.addFixture(fids[1], QPointF(3000.0, 3000.0));
        // The third is on the plot but free-standing, off the tower.
        view.addFixture(fids[2], QPointF(7000.0, 5000.0));
        view.setViewPOV(MonitorGraphicsView::PovTop);
        view.refreshItemLayerState();

        const QString key = QStringLiteral("truss:%1").arg(tower->id());
        auto badgeCount = [&]() -> int {
            QGraphicsItemGroup *g = view.m_countBadges.value(key, nullptr);
            if (g == nullptr)
                return 0;
            foreach (QGraphicsItem *c, g->childItems())
                if (QGraphicsSimpleTextItem *t = dynamic_cast<QGraphicsSimpleTextItem *>(c))
                    return t->text().toInt();
            return 0;
        };
        QCOMPARE(badgeCount(), 2);

        // The editor: mount it, tell the plot, then accept.
        mountOnTower(fids[2], 2.5f);
        view.updateFixture(fids[2]);
        view.followParentTrusses();
        QCoreApplication::processEvents();

        QVERIFY2(badgeCount() == 3,
                 qPrintable(QString("the tower now carries three fixtures and its "
                                    "badge still says %1").arg(badgeCount())));
        MonitorFixtureItem *third = view.m_fixtures.value(fids[2], nullptr);
        QVERIFY(third != nullptr);
        QVERIFY2(!third->isVisible(),
                 "the fixture just added to a buried tower is still drawn over it");
    }

    foreach (quint32 fid, fids)
        doc->deleteFixture(fid);
    props->removeTruss(tower->id());
    delete doc;
}

/* Eight strips up a step's FACE sit at eight heights and one footprint, so from
   above they land on the same spot: union 2.4% of the step, summed areas 30.6%
   -- thirteen times the union.
 *
   They must STAY VISIBLE. A "stacked" trigger was tried -- badge when the
   summed area greatly exceeds the union -- and it hid exactly this, the LED
   tape up a step's face, which is the one thing you want to see on a step. The
   rule is area only: fixtures are replaced by a count when they COVER their
   structure, not when they merely pile onto one spot. */
void Monitor_Test::stackedFixturesOnAStepBecomeACount()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    StagePlatform *pl = props->addPlatform();
    pl->setName("US1");
    pl->setOriginX(1.2f); pl->setOriginY(1.5f);
    pl->setWidth(2.438f); pl->setDepth(1.219f); pl->setHeight(0.396f);

    QList<quint32> fids;
    for (int i = 0; i < 8; i++)
    {
        Fixture *f = makeMover(doc, 14.0, quint32(i * 8),
                               qPrintable(QString("US1 #%1").arg(i + 1)),
                               QVector3D(2.4f, 2.7f, 0.05f * i));
        QVERIFY(f != nullptr);
        fids << f->id();
        FixtureRigProps rp;
        rp.riserPlatformId = pl->id();
        rp.riserFace = FixtureRigProps::RiserFront;
        rp.riserU = 1.2f;
        rp.riserV = 0.04f * i;          // up the face: same spot from above
        props->setFixtureRigProps(f->id(), rp);
    }

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.updatePlatforms();
        foreach (quint32 fid, fids)
        {
            view.addFixture(fid, QPointF(2400.0, 2700.0));
            view.updateFixture(fid);
        }
        view.setViewPOV(MonitorGraphicsView::PovTop);
        view.refreshItemLayerState();

        double stack = 0.0;
        const double cover = view.structureCoverageByItsFixtures(
            QStringLiteral("platform"), pl->id(), nullptr, &stack);

        // Piled up, but covering very little -- and that is NOT enough to hide.
        QVERIFY2(cover < 0.40,
                 qPrintable(QString("expected stacked strips to cover little by AREA, got %1")
                            .arg(cover)));
        QVERIFY2(stack >= 2.0,
                 qPrintable(QString("expected them piled up, got a stack factor of %1")
                            .arg(stack)));

        QVERIFY2(!view.m_countBadges.contains(QStringLiteral("platform:%1").arg(pl->id())),
                 "stacked-but-not-covering strips were replaced by a count");
        foreach (quint32 fid, fids)
        {
            MonitorFixtureItem *it = view.m_fixtures.value(fid, nullptr);
            QVERIFY(it != nullptr);
            QVERIFY2(it->isVisible(), "a step's face tape was hidden without covering the step");
        }
    }

    foreach (quint32 fid, fids) doc->deleteFixture(fid);
    props->removePlatform(pl->id());
    delete doc;
}

/* A bar lying in a step is built on a diagonal inside the box, firing up
   through the plexi toward the cast rather than straight up. studioAngle turns
   a fixture WITHIN its face (flat, like a bearing) and cannot say that;
   studioTilt tips the face itself. */
void Monitor_Test::studioTiltRollsAFixtureAboutItsLength()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Fixture *fx = makeMover(doc, 14.0, 0, "Step Bar", QVector3D(2.0f, 2.0f, 0.2f));
    QVERIFY(fx != nullptr);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(700, 560);

    FixtureRigProps rp;
    rp.studioMount = 0;            // lying on a top face
    rp.studioAngle = 0.0f;         // running stage left-right
    rp.studioTilt  = 0.0f;
    props->setFixtureRigProps(fx->id(), rp);

    QVector3D L0, N0, H0;
    v.fixtureFrame(props->fixtureRigProps(fx->id()), L0, N0, H0);
    QVERIFY2(qAbs(double(N0.z()) - 1.0) < 1e-3,
             "a flat top-face fixture should face straight up");

    // Tip it 20 degrees.
    rp.studioTilt = 20.0f;
    props->setFixtureRigProps(fx->id(), rp);
    QVector3D L1, N1, H1;
    v.fixtureFrame(props->fixtureRigProps(fx->id()), L1, N1, H1);

    // The length is untouched -- it is the axis we rolled about.
    QVERIFY2((L1 - L0).length() < 1e-4f, "the roll moved the fixture's long axis");

    // The face has tipped by exactly 20 degrees, and tipped UPSTAGE (-Y).
    const double tipped = qRadiansToDegrees(qAcos(qBound(-1.0, double(QVector3D::dotProduct(N0, N1)), 1.0)));
    QVERIFY2(qAbs(tipped - 20.0) < 0.5,
             qPrintable(QString("expected a 20 degree tip, got %1").arg(tipped)));
    QVERIFY2(N1.y() < -0.01f,
             qPrintable(QString("expected the face to tip UPSTAGE (-Y), got y=%1")
                        .arg(double(N1.y()))));

    // Still an orthonormal frame, or everything downstream skews.
    QVERIFY(qAbs(double(QVector3D::dotProduct(N1, L1))) < 1e-4);
    QVERIFY(qAbs(double(QVector3D::dotProduct(N1, H1))) < 1e-4);
    QVERIFY(qAbs(double(N1.length()) - 1.0) < 1e-4);

    doc->deleteFixture(fx->id());
    delete doc;
}

void Monitor_Test::theRigGridMatchesTheStudioGrid()
{
    /* Two windows onto the same stage have to agree about how big a metre is.
       The rig view used to rule its floor in a twelfth of however wide the rig
       happened to be -- an arbitrary fraction of a stage, different for every
       show, and unrelated to the grid the studio draws. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Meters);
    props->setGridSize(QVector3D(20, 3, 12));

    StagePlatform *pl = props->addPlatform();
    pl->setName("Deck"); pl->setOriginX(2.0f); pl->setOriginY(2.0f);
    pl->setWidth(4.0f); pl->setDepth(2.0f); pl->setHeight(0.4f);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(900, 620);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(28.0, 22.0);
    v.grab();

    /* One metre is one cell, so one metre of stage is one grid step of screen
       -- and the same at the front of the stage as at the back, this being an
       orthographic projection. */
    const double a = QLineF(v.w2s(QVector3D(0, 0, 0)),
                            v.w2s(QVector3D(0, 1, 0))).length();
    const double b = QLineF(v.w2s(QVector3D(0, 9, 0)),
                            v.w2s(QVector3D(0, 10, 0))).length();
    QVERIFY2(qAbs(a - b) < 0.01,
             qPrintable(QString("the floor grid is not uniform in depth: %1 px "
                                "at the front, %2 at the back").arg(a).arg(b)));

    /* And in feet, a cell is a foot: the same stage drawn in feet has its
       lines about 3.28 times closer together than in metres. */
    const double metresPerCell = 1.0;
    props->setGridUnits(MonitorProperties::Feet);
    StructureStudioView vf(doc, StructureStudioView::StageKind, 0);
    vf.resize(900, 620);
    vf.reload();
    vf.setPlane(StructureStudioView::Angled);
    vf.setAngledView(28.0, 22.0);
    vf.grab();
    QCOMPARE(int(props->gridUnits()), int(MonitorProperties::Feet));
    QVERIFY(metresPerCell > 0.3048);

    delete doc;
}

void Monitor_Test::aZoomHeadsConeFollowsItsZoomChannel()
{
    /* "Are we keying the cone of light off the info in the fixture def for min
       and max?" -- we were taking the MAXIMUM and nothing else, so every zoom
       head drew at its widest whatever the desk was telling it. The declared
       min and max are a RANGE; the zoom channel says where in it the head is
       currently sitting. Which way that channel runs is part of the definition
       and both directions are common, so it is read, not assumed. */
    Doc *doc = new Doc(this);

    auto build = [&](QLCChannel::Preset zoomPreset, double lo, double hi,
                     quint32 addr, const char *name) -> Fixture * {
        QLCFixtureDef *def = new QLCFixtureDef();
        def->setManufacturer("Test"); def->setModel(name);
        def->setType(QLCFixtureDef::MovingHead);
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName("2ch");

        QLCChannel *dim = new QLCChannel();
        dim->setName("Dimmer"); dim->setGroup(QLCChannel::Intensity);
        def->addChannel(dim); mode->insertChannel(dim, 0);

        QLCChannel *zoom = new QLCChannel();
        zoom->setName("Zoom");
        zoom->setPreset(zoomPreset);            // sets its group/behaviour
        def->addChannel(zoom); mode->insertChannel(zoom, 1);

        QLCPhysical ph;
        ph.setWidth(300); ph.setHeight(400); ph.setDepth(300);
        ph.setLensDegreesMin(lo); ph.setLensDegreesMax(hi);
        mode->setPhysical(ph);
        QLCFixtureHead hd; hd.addChannel(0); hd.addChannel(1);
        mode->insertHead(-1, hd);
        def->addMode(mode);

        Fixture *f = new Fixture(doc);
        f->setName(name); f->setFixtureDefinition(def, mode);
        f->setUniverse(3); f->setAddress(addr);
        return doc->addFixture(f) ? f : nullptr;
    };

    Fixture *up = build(QLCChannel::BeamZoomSmallBig, 8.0, 40.0, 0, "Zoom S->B");
    Fixture *down = build(QLCChannel::BeamZoomBigSmall, 8.0, 40.0, 8, "Zoom B->S");
    QVERIFY(up != nullptr);
    QVERIFY(down != nullptr);

    const FixtureVisualTraits tUp = classifyFixture(up);
    QCOMPARE(int(tUp.beamMinDeg), 8);
    QCOMPARE(int(tUp.beamMaxDeg), 40);

    auto angleAt = [&](Fixture *f, int zoomVal) {
        QByteArray u(512, char(0));
        u[int(f->address())] = char(255);              // dimmer up
        u[int(f->address()) + 1] = char(zoomVal);
        f->setChannelValues(u);
        return fixtureBeamAngle(f, f->channelValues(), classifyFixture(f));
    };

    // Small->Big: 0 is the narrow end, 255 the wide one.
    QVERIFY2(qAbs(angleAt(up, 0) - 8.0) < 0.6,
             qPrintable(QString("zoom at 0 should be the narrow end, got %1")
                        .arg(angleAt(up, 0))));
    QVERIFY2(qAbs(angleAt(up, 255) - 40.0) < 0.6,
             qPrintable(QString("zoom at full should be the wide end, got %1")
                        .arg(angleAt(up, 255))));
    const double mid = angleAt(up, 128);
    QVERIFY2(mid > 20.0 && mid < 28.0,
             qPrintable(QString("zoom halfway should be mid-range, got %1").arg(mid)));

    // Big->Small runs the other way, and that has to be read from the preset.
    QVERIFY2(qAbs(angleAt(down, 0) - 40.0) < 0.6,
             qPrintable(QString("a big->small zoom at 0 should be WIDE, got %1")
                        .arg(angleAt(down, 0))));
    QVERIFY2(qAbs(angleAt(down, 255) - 8.0) < 0.6,
             qPrintable(QString("a big->small zoom at full should be NARROW, got %1")
                        .arg(angleAt(down, 255))));

    /* A fixed lens ignores all of this, and an undeclared one still reports 0
       so callers can tell "unknown" from "zero degrees". */
    Fixture *fixed = build(QLCChannel::Custom, 15.0, 15.0, 16, "Fixed");
    QVERIFY(fixed != nullptr);
    QCOMPARE(qRound(fixtureBeamAngle(fixed, QByteArray(512, char(0)),
                                     classifyFixture(fixed))), 15);
    Fixture *none = build(QLCChannel::Custom, 0.0, 0.0, 24, "No Lens");
    QVERIFY(none != nullptr);
    QCOMPARE(fixtureBeamAngle(none, QByteArray(512, char(0)),
                              classifyFixture(none)), 0.0);

    delete doc;
}

void Monitor_Test::editingALookUpdatesTheRigView()
{
    /* "Didn't change colour when colour was changed in program from white to
       blue." What a scene SENDS is cached, because resolving palettes per
       fixture per frame is far too expensive -- but a cache of something the
       user is actively editing goes stale the moment they edit it. Exactly the
       same mistake as caching a target's position while someone drags it. */
    Doc *doc = new Doc(this);

    Fixture *head = makeMover(doc, 14.0, 0, "Edited", QVector3D(3.0f, 3.0f, 5.0f));
    QVERIFY(head != nullptr);

    QLCPalette *dim = new QLCPalette(QLCPalette::Dimmer, doc);
    dim->setName("Full"); dim->setValue(255);
    QVERIFY(doc->addPalette(dim));
    QLCPalette *col = new QLCPalette(QLCPalette::Color, doc);
    col->setName("White"); col->setValue(QColor(255, 255, 255));
    QVERIFY(doc->addPalette(col));

    Scene *scene = new Scene(doc);
    scene->setName("Edited Look");
    scene->addFixture(head->id());
    scene->addPalette(dim->id());
    scene->addPalette(col->id());
    QVERIFY(doc->addFunction(scene));

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(600, 480);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setActiveScene(scene->id());

    auto resolved = [&]() {
        QColor c(0, 0, 0);
        uchar dm = 0;
        fixtureLiveState(head, v.m_sceneValues.value(head->id()), c, dm);
        return c;
    };

    QColor before = resolved();
    QVERIFY2(before.red() > 200 && before.green() > 200 && before.blue() > 200,
             qPrintable(QString("expected the look to start white, got %1")
                        .arg(before.name())));

    /* Edit it, exactly as the Programming tab does: change the palette and let
       the scene announce it. */
    col->setValue(QColor(0, 0, 255));
    QMetaObject::invokeMethod(doc, "functionChanged", Q_ARG(quint32, scene->id()));
    QTest::qWait(30);                  // the refresh is coalesced to the event loop

    const QColor after = resolved();
    QVERIFY2(after.blue() > 200 && after.red() < 80,
             qPrintable(QString("the rig view is still showing the old colour "
                                "after the look was edited: %1").arg(after.name())));

    // And back again -- blue to white, which is the other half of the report.
    col->setValue(QColor(255, 255, 255));
    QMetaObject::invokeMethod(doc, "functionChanged", Q_ARG(quint32, scene->id()));
    QTest::qWait(30);
    const QColor back = resolved();
    QVERIFY2(back.red() > 200 && back.green() > 200,
             qPrintable(QString("editing back to white did not take: %1")
                        .arg(back.name())));

    /* And what the view actually DRAWS follows live output first.
     *
       While live preview is on, the DMX is the look being edited and it updates
       on every twitch of the colour picker. The scene resolution is cached and
       refreshes only when the scene announces a change -- which editing a
       PALETTE does not do. Cached-first therefore showed the colour the look had
       when it was selected. Live-first is what makes the picker feel connected. */
    v.setLiveValues(true);
    QByteArray drivenGreen(512, char(0));
    drivenGreen[2] = char(255);                      // dimmer
    drivenGreen[4] = char(255);                      // green
    head->setChannelValues(drivenGreen);

    QColor shown(0, 0, 0);
    uchar shownDim = 0;
    /* The VIEW's own choice, not a copy of it made here -- the first version of
       this test reimplemented the preference in the test body and so passed
       whatever the view did. */
    QVERIFY(fixtureLiveState(head, v.valuesForDrawing(head), shown, shownDim));
    QVERIFY2(shown.green() > 200 && shown.red() < 80,
             qPrintable(QString("live output did not outrank the cached look: "
                                "%1").arg(shown.name())));

    /* Nothing driving it: fall back to the look rather than reading an
       all-zero universe as a blackout. */
    head->setChannelValues(QByteArray(512, char(0)));
    QColor fallback(0, 0, 0);
    uchar fbDim = 0;
    QVERIFY(fixtureLiveState(head, v.valuesForDrawing(head), fallback, fbDim));
    QVERIFY2(fallback.red() > 200 && fallback.green() > 200,
             qPrintable(QString("an undriven fixture should fall back to the "
                                "selected look, got %1").arg(fallback.name())));

    delete doc;
}

void Monitor_Test::overlappingFixturesAreFlagged()
{
    Doc *doc = new Doc(this);

    Fixture *a = makeMover(doc, 14.0, 0, "A", QVector3D(3.0f, 3.0f, 1.0f));
    Fixture *b = makeMover(doc, 14.0, 10, "B", QVector3D(3.0f, 3.0f, 1.0f));  // same spot
    Fixture *c = makeMover(doc, 14.0, 20, "C", QVector3D(9.0f, 3.0f, 1.0f));  // well clear
    QVERIFY(a != nullptr && b != nullptr && c != nullptr);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(600, 480);
    v.reload();
    v.rebuildOverlapWarnings();

    QVERIFY2(v.m_overlappingFixtures.contains(a->id()),
             "fixture A, rigged into the same space as B, was not flagged");
    QVERIFY2(v.m_overlappingFixtures.contains(b->id()),
             "fixture B, rigged into the same space as A, was not flagged");
    QVERIFY2(v.m_overlappingFixtures.contains(c->id()) == false,
             "fixture C, rigged well clear of the others, was flagged anyway");

    delete doc;
}

void Monitor_Test::aFollowSpotAimsAtTheSubjectNotTheFloor()
{
    /* "The lights aren't pointing at the HEIGHT of the target subject, which is
       ~5ft." Right: a follow-spot aims at a PERSON. The target's XY says where
       they are standing; the height is aimSubjectHeight() above whatever they
       are standing ON, so the beam lands on a chest rather than a pair of feet.
       QLCPalette already did exactly this when resolving an Aim palette -- the
       rig view was using the target's own Z and so pointed somewhere the rig
       does not. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    props->setAimSubjectHeight(1.4f);

    StagePlatform *deck = props->addPlatform();
    deck->setName("Deck"); deck->setOriginX(2.0f); deck->setOriginY(2.0f);
    deck->setWidth(3.0f); deck->setDepth(3.0f); deck->setHeight(0.5f);

    Fixture *head = makeMover(doc, 10.0, 0, "Spot", QVector3D(3.5f, 8.0f, 6.0f));
    QVERIFY(head != nullptr);

    StageTarget *tgt = props->addStageTarget();
    tgt->setX(3.5f); tgt->setY(3.5f); tgt->setZ(0.0f);      // on the deck
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("At"); aim->setStageTargetId(tgt->id());
    QVERIFY(doc->addPalette(aim));

    Scene *scene = new Scene(doc);
    scene->setName("Plain Aim");
    scene->addFixture(head->id());
    scene->addPalette(aim->id());
    QVERIFY(doc->addFunction(scene));

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(600, 460);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setActiveScene(scene->id());

    /* No follow-spot effect: the target means exactly where it says. */
    QVector3D pt;
    QVERIFY(v.aimPointFor(head->id(), pt));
    QVERIFY2(qAbs(double(pt.z())) < 0.01,
             qPrintable(QString("a plain aim should use the target's own Z, got %1")
                        .arg(double(pt.z()))));

    /* Add a follow-spot effect and it becomes a SUBJECT: standing on a 0.5 m
       deck, chest at 0.5 + 1.4. */
    QLCPalette *fx = new QLCPalette(QLCPalette::Effect, doc);
    fx->setName("followspot");
    fx->setScriptPath("Palettes/Effect/Position/Followspot/");
    QVERIFY(doc->addPalette(fx));
    scene->addPalette(fx->id());
    v.setActiveScene(Function::invalidId());
    v.setActiveScene(scene->id());

    QVERIFY(v.aimPointFor(head->id(), pt));
    QVERIFY2(qAbs(double(pt.z()) - 1.9) < 0.05,
             qPrintable(QString("a follow-spot should aim at deck + subject "
                                "height (0.5 + 1.4 = 1.9), got %1")
                        .arg(double(pt.z()))));

    /* And the aim FOLLOWS the target: it gets dragged about with a mouse or a
       joystick, so caching where it was is caching the wrong thing. */
    tgt->setX(6.0f); tgt->setY(7.0f);
    QVERIFY(v.aimPointFor(head->id(), pt));
    QVERIFY2(qAbs(double(pt.x()) - 6.0) < 0.01 && qAbs(double(pt.y()) - 7.0) < 0.01,
             qPrintable(QString("the aim did not follow the target when it moved "
                                "(%1,%2)").arg(double(pt.x())).arg(double(pt.y()))));
    /* Moved off the deck, so the subject is now standing on the floor. */
    QVERIFY2(qAbs(double(pt.z()) - 1.4) < 0.05,
             qPrintable(QString("subject height did not re-resolve against what "
                                "the target now stands on, got %1")
                        .arg(double(pt.z()))));

    delete doc;
}

void Monitor_Test::theReachOverlayShadesWhereASpotCannotAim()
{
    /* "Can we predict the areas the spot WON'T be able to cover?" A 540/270
       spot standing on a 10 ft tower cannot tilt within 45 degrees of straight
       down, so at chest height it is blind in a circle around the tower about
       as wide as it is high above the chest. The overlay has to shade that
       circle and nothing far from it, and clear when the spot is hung. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *tower = props->addTruss();
    tower->setName("T-1");
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    tower->setLength(3.048f);
    tower->setWidth(0.29f);

    Fixture *spot = makeMover(doc, 14.0, 0, "UKing", QVector3D(3.0f, 3.0f, 3.0f));
    QVERIFY(spot != nullptr);
    {
        QLCFixtureMode *mode = spot->fixtureMode();
        QLCPhysical ph = mode->physical();
        ph.setFocusPanMax(540);
        ph.setFocusTiltMax(270);
        mode->setPhysical(ph);
    }
    FixtureRigProps rp;
    rp.trussId = tower->id();
    rp.trussOffset = 3.048f;
    rp.mountingType = Truss::FloorMounted;
    props->setFixtureRigProps(spot->id(), rp);

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 7));
        view.setViewPOV(MonitorGraphicsView::PovTop);
        view.updateTrusses();
        view.addFixture(spot->id(), QPointF(3000.0, 3000.0));
        MonitorFixtureItem *item = view.m_fixtures.value(spot->id(), nullptr);
        QVERIFY(item != nullptr);

        const double subject = double(props->aimSubjectHeight());
        const double blindRadius = 3.048 - subject;          // 45 deg: drop == distance
        auto shaded = [&](double x, double y) {
            if (view.m_reachItem == nullptr || !view.m_reachItem->isVisible())
                return false;
            const QImage img = view.m_reachItem->pixmap().toImage();
            const double cell = view.m_reachItem->scale() * view.m_unitValue
                                / (1000.0 * view.m_cellPixels);
            const QPoint px(int(x / cell), int(y / cell));
            return img.rect().contains(px) && qAlpha(img.pixel(px)) > 0;
        };

        // Nothing selected: nothing to show.
        view.setReachOverlayVisible(true);
        QVERIFY(!shaded(3.0, 3.3));

        item->setSelected(true);
        QCoreApplication::processEvents();
        QVERIFY2(shaded(3.0, 3.0 + blindRadius * 0.5),
                 "half-way out from the tower is inside the blind circle and not shaded");
        QVERIFY2(shaded(3.0 - blindRadius * 0.7, 3.0),
                 "the blind circle is not round: stage right of the tower is not shaded");
        QVERIFY2(!shaded(3.0, 3.0 + blindRadius * 1.4),
                 "well outside the blind circle is shaded");
        QVERIFY2(!shaded(8.0, 6.0), "the far corner of the stage is shaded");

        // Hang it: nothing on the stage is out of its reach any more.
        rp.mountingType = Truss::TopHung;
        rp.trussOffset = 2.8f;
        props->setFixtureRigProps(spot->id(), rp);
        QCoreApplication::processEvents();
        QVERIFY2(!shaded(3.0, 3.0 + blindRadius * 0.5),
                 "hung under the tower the spot can reach its base, but it is still shaded");

        // Deselect: gone.
        item->setSelected(false);
        QCoreApplication::processEvents();
        QVERIFY(view.m_reachItem == nullptr || !view.m_reachItem->isVisible());
    }

    delete doc;
}

void Monitor_Test::aTargetSaysWhichHeadsCannotReachIt()
{
    /* A scene aims two spots at a vocalist half a metre in front of a tower:
       one standing on top of the tower, one hung from it. The one on top
       cannot tilt that far down. The plot has to say so -- on the target, and
       on that head's aim line -- and stop saying so the moment the head is
       re-rigged to hang, which is the decision the warning is there for. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *tower = props->addTruss();
    tower->setName("T-1");
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    tower->setLength(3.048f);
    tower->setWidth(0.29f);

    Fixture *onTop = makeMover(doc, 14.0, 0, "On top", QVector3D(3.0f, 3.0f, 3.0f));
    Fixture *hung  = makeMover(doc, 14.0, 8, "Hung",   QVector3D(3.0f, 3.0f, 2.8f));
    QVERIFY(onTop != nullptr && hung != nullptr);
    FixtureRigProps rp;
    rp.trussId = tower->id();
    rp.trussOffset = 3.048f;
    rp.mountingType = Truss::FloorMounted;
    props->setFixtureRigProps(onTop->id(), rp);
    rp.trussOffset = 2.8f;
    rp.mountingType = Truss::TopHung;
    props->setFixtureRigProps(hung->id(), rp);

    StageTarget *who = props->addStageTarget();
    who->setName("Vocalist");
    who->setKind(StageTarget::Kind::Person);
    who->setPosition(QVector3D(3.0f, 3.5f, 0.0f));
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("At the vocalist");
    aim->setStageTargetId(who->id());
    QVERIFY(doc->addPalette(aim));
    Scene *scene = new Scene(doc);
    scene->setName("Solo");
    scene->addFixture(onTop->id());
    scene->addFixture(hung->id());
    scene->addPalette(aim->id());
    QVERIFY(doc->addFunction(scene));

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 7));
        view.updateTrusses();
        view.addFixture(onTop->id(), QPointF(3000.0, 3000.0));
        view.addFixture(hung->id(), QPointF(3000.0, 3000.0));
        view.setActiveScene(scene->id());

        auto redLines = [&]() {
            int n = 0;
            foreach (QGraphicsLineItem *li, view.m_aimLines)
                if (li->pen().color() == QColor(235, 70, 60))
                    ++n;
            return n;
        };

        TargetItem *ti = view.m_targetItems.value(who->id(), nullptr);
        QVERIFY(ti != nullptr);
        QVERIFY2(ti->unreachableCount() == 1,
                 qPrintable(QString("the target should report one head that cannot "
                                    "reach it, reports %1").arg(ti->unreachableCount())));
        QVERIFY(ti->toolTip().contains("On top"));
        // One line and its two arrowhead strokes, for the one head.
        QCOMPARE(redLines(), 3);
        QCOMPARE(view.m_aimLines.size(), 6);

        // Hang it instead: the warning goes, without anything else prodding it.
        rp.trussOffset = 3.0f;
        props->setFixtureRigProps(onTop->id(), rp);
        QCoreApplication::processEvents();
        ti = view.m_targetItems.value(who->id(), nullptr);
        QVERIFY(ti != nullptr);
        QCOMPARE(ti->unreachableCount(), 0);
        QCOMPARE(redLines(), 0);
    }

    delete doc;
}

void Monitor_Test::aFaceFixtureDrawsOverWhatIsInside()
{
    /* "I put this XL-450 on the front of the truss and it's showing under
       what's inside." The flat views painted a structure's fixtures in id
       order, so whichever was created first went underneath -- and a fixture
       on the face and one inside the truss at the same height sit on the same
       centre line, so nothing about their positions said which is nearer. On
       the face is nearer than inside, from any side; the one drawn on top is
       also the one a click has to pick. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *tower = props->addTruss();
    tower->setName("T-1");
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    tower->setLength(3.048f);
    tower->setWidth(0.29f);

    // Created FIRST, so the lower id: exactly the order that buried it.
    auto rigged = [&](const char *name, int placement, const QColor &gel) {
        Fixture *f = new Fixture(doc);
        f->setName(name);
        f->setChannels(1);
        f->setAddress(quint32(doc->fixtures().size()));
        doc->addFixture(f);
        FixtureRigProps rp;
        rp.trussId = tower->id();
        rp.trussOffset = 2.6f;
        rp.placement = placement;
        props->setFixtureRigProps(f->id(), rp);
        props->setFixtureGelColor(f->id(), 0, 0, gel);
        return f;
    };
    Fixture *face   = rigged("XL-450", FixtureRigProps::OnSurface, QColor(230, 0, 0));
    Fixture *inside = rigged("T1-B1",  FixtureRigProps::Inside,    QColor(0, 0, 230));
    QVERIFY(face->id() < inside->id());

    {
        StructureStudioView v(doc, StructureStudioView::TrussKind, tower->id());
        v.resize(600, 500);
        v.reload();

        const StructureStudioView::Plane planes[] = {
            StructureStudioView::Front, StructureStudioView::Side, StructureStudioView::Top };
        const char *names[] = { "Front", "Side", "Top" };
        for (int k = 0; k < 3; ++k)
        {
            v.setPlane(planes[k]);
            const QImage img = v.grab().toImage();
            // w2s() is logical pixels; grab() on a HiDPI screen returns
            // device pixels (devicePixelRatio() times as many) -- scale the
            // IMAGE-sampling point to match, but keep the LOGICAL point for
            // hitTestFixture(), which takes real widget/mouse coordinates.
            const double dpr = img.devicePixelRatio();
            const QPointF cLogical = v.w2s(v.fixtureCentre(face->id()));
            const QPoint c = (cLogical * dpr).toPoint();
            int red = 0, blue = 0;
            for (int dy = -6; dy <= 6; ++dy)
                for (int dx = -6; dx <= 6; ++dx)
                {
                    const QPoint q = c + QPoint(int(dx * dpr), int(dy * dpr));
                    if (!img.rect().contains(q))
                        continue;
                    const QColor px = img.pixelColor(q);
                    if (px.red() > 90 && px.red() > px.blue() * 2)  ++red;
                    if (px.blue() > 90 && px.blue() > px.red() * 2) ++blue;
                }
            QVERIFY2(red > 0 && red > blue,
                     qPrintable(QString("%1: the face fixture is under the one inside "
                                        "(%2 face px, %3 inside px)")
                                .arg(names[k]).arg(red).arg(blue)));
            QVERIFY2(v.hitTestFixture(cLogical) == face->id(),
                     qPrintable(QString("%1: clicking where the face fixture is drawn "
                                        "picked the one hidden under it").arg(names[k])));
        }
    }

    delete doc;
}

void Monitor_Test::aFrameFilledAcrossCoresIsTheSameFrame()
{
    /* Filling the rig on several threads splits the frame into bands of rows
       that never share a pixel, each running the same ops in the same order.
       That has to change the speed and nothing else: the picture must be the
       same bit for bit, with solids, light and translucent tops all in it,
       whatever the supersampling. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(1.0f); deck->setOriginY(1.0f);
    deck->setWidth(8.0f); deck->setDepth(3.0f); deck->setHeight(0.6f);
    StagePlatform *clear = props->addPlatform();
    clear->setOriginX(2.0f); clear->setOriginY(4.2f);
    clear->setWidth(4.0f); clear->setDepth(1.0f); clear->setHeight(0.4f);
    clear->setColor(QColor(80, 120, 200, 140));             // see-through

    StageTarget *who = props->addStageTarget();
    who->setKind(StageTarget::Kind::Person);
    who->setPosition(QVector3D(5.0f, 2.5f, 0.6f));
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setStageTargetId(who->id());
    QVERIFY(doc->addPalette(aim));
    Scene *scene = new Scene(doc);
    scene->addPalette(aim->id());
    for (int i = 0; i < 6; ++i)
    {
        Fixture *f = makeMover(doc, 12.0, quint32(i * 8), qPrintable(QString("H%1").arg(i)),
                               QVector3D(1.5f + 1.4f * i, 7.0f, 4.0f));
        QVERIFY(f != nullptr);
        scene->addFixture(f->id());
        QByteArray u(512, char(0));
        const int a = int(f->address());
        u[a + 2] = char(255); u[a + 3] = char(i * 40); u[a + 4] = char(255 - i * 30); u[a + 5] = char(200);
        f->setChannelValues(u);
    }
    QVERIFY(doc->addFunction(scene));

    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(900, 640);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(-25.0, 28.0);
        v.setBeams(true);
        v.setLiveValues(true);
        v.setActiveScene(scene->id());

        for (int ss : { 1, 2, 3 })
        {
            v.setSupersample(ss);
            v.setRenderThreads(1);
            const QImage one = v.grab().toImage();
            QVERIFY2(v.m_frameBeams.size() == 6,
                     qPrintable(QString("%1 beams in the frame, expected 6")
                                .arg(v.m_frameBeams.size())));
            for (int threads : { 2, 5, 16 })
            {
                v.setRenderThreads(threads);
                const QImage many = v.grab().toImage();
                QVERIFY2(many == one,
                         qPrintable(QString("supersample %1: %2 threads drew a different "
                                            "frame from one").arg(ss).arg(threads)));
            }
        }
        v.setRenderThreads(0);
        v.setSupersample(2);
    }

    delete doc;
}

void Monitor_Test::manyWhiteBeamsDoNotBlowOut()
{
    /* "Blue worked great .. white .. does not .. it's too overdriven." Ten
       white heads on one vocalist. Stacked translucent layers converge on
       their colour, and white is the brightest one there is: the beams
       merged into a glare and the body clipped to a flat white blob. Lit,
       the body must still be short of clipping and still show which parts
       the light is on; the air must not be a solid white mass. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    StageTarget *who = props->addStageTarget();
    who->setName("Vocalist");
    who->setKind(StageTarget::Kind::Person);
    who->setPosition(QVector3D(5.0f, 3.0f, 0.0f));

    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setStageTargetId(who->id());
    QVERIFY(doc->addPalette(aim));
    Scene *scene = new Scene(doc);
    scene->setName("Ten on one");
    scene->addPalette(aim->id());

    QList<Fixture *> heads;
    for (int i = 0; i < 10; ++i)
    {
        // An arc downstage of the subject, four metres up.
        const double a = qDegreesToRadians(-70.0 + 140.0 * i / 9.0);
        const QVector3D at(5.0f + 4.0f * float(qSin(a)), 3.0f + 4.0f * float(qCos(a)), 4.0f);
        Fixture *f = makeMover(doc, 14.0, quint32(i * 8),
                               qPrintable(QString("Front %1").arg(i + 1)), at);
        QVERIFY(f != nullptr);
        scene->addFixture(f->id());
        heads << f;
    }
    QVERIFY(doc->addFunction(scene));
    foreach (Fixture *f, heads)
    {
        QByteArray u(512, char(0));
        const int a = int(f->address());
        u[a + 2] = char(255); u[a + 3] = char(255); u[a + 4] = char(255); u[a + 5] = char(255);
        f->setChannelValues(u);
    }

    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(900, 640);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(0.0, 12.0);
        v.setAmbient(0.25);
        v.setBeams(true);
        v.setLiveValues(true);
        v.setActiveScene(scene->id());
        const QImage img = v.grab().toImage();
        QCOMPARE(v.m_frameBeams.size(), 10);

        const float foot = props->targetAimPoint(who, true).z() - props->targetAimHeight(who, true);
        const float tall = props->targetFigureHeight(who);
        // w2s() is logical pixels; grab() on a HiDPI screen returns device
        // pixels (devicePixelRatio() times as many) -- scale to match.
        const double dpr = img.devicePixelRatio();
        auto sample = [&](const QVector3D &w) {
            const QPoint c = (v.w2s(w) * dpr).toPoint();
            const int r2 = qMax(1, int(dpr));
            int r = 0, g = 0, b = 0, n = 0;
            for (int dy = -r2; dy <= r2; ++dy)
                for (int dx = -r2; dx <= r2; ++dx)
                {
                    const QColor px = img.pixelColor(c + QPoint(dx, dy));
                    r += px.red(); g += px.green(); b += px.blue(); ++n;
                }
            return QColor(r / n, g / n, b / n);
        };
        const QColor chest = sample(QVector3D(5.0f + 0.06f * tall / 1.75f,
                                              3.0f + 0.13f * tall / 1.75f, foot + 0.70f * tall));
        const QColor shin  = sample(QVector3D(5.09f * tall / 1.75f + 5.0f * (1.0f - tall / 1.75f),
                                              3.06f, foot + 0.25f * tall));

        // Pure white anywhere but on the figure itself.
        // w2s() is logical pixels; grab() on a HiDPI screen returns device
        // pixels (devicePixelRatio() times as many) -- scale the figure box,
        // the scan's start row, and the logical-pixel margins to match, or
        // the exclusion zone lands nowhere near the figure and the scan
        // starts far too early, sweeping the beam cones themselves in.
        const QPointF headPx = v.w2s(QVector3D(5.0f, 3.0f, foot + tall)) * dpr;
        const QPointF feetPx = v.w2s(QVector3D(5.0f, 3.0f, foot)) * dpr;
        const QRectF figureBox(QPointF(headPx.x() - 0.4 * v.m_scale * dpr, headPx.y()),
                               QPointF(feetPx.x() + 0.4 * v.m_scale * dpr, feetPx.y()));
        /* Below the row of heads: a lamp at full is white and should be, so
           the heads themselves are not what is being measured. */
        double belowHeads = 0.0;
        foreach (Fixture *f, heads)
            belowHeads = qMax(belowHeads, v.w2s(props->fixtureRigPosition(f->id())).y());
        belowHeads = belowHeads * dpr + 40.0 * dpr;
        int clipped = 0;
        for (int y = int(belowHeads); y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
            {
                if (figureBox.contains(QPointF(x, y)))
                    continue;
                const QColor c = img.pixelColor(x, y);
                if (c.red() >= 250 && c.green() >= 250 && c.blue() >= 250)
                    ++clipped;
            }

        const QString report = QString("chest %1, shin %2, clipped air %3 px")
                               .arg(chest.name(), shin.name()).arg(clipped);

        QVERIFY2(chest.red() < 250 || chest.green() < 250 || chest.blue() < 250,
                 qPrintable("the lit chest clipped to white: " + report));
        QVERIFY2(chest.green() > shin.green() + 60,
                 qPrintable("the lit chest does not stand out from the unlit legs: " + report));
        QVERIFY2(clipped < 150,
                 qPrintable("the beams merged into solid white: " + report));
    }

    delete doc;
}

void Monitor_Test::aPersonTakesTheColourOfTheLightOnIt()
{
    /* "The person should reflect the colour of the lights pointed at it and
       summarize the colours as they're applied." Two heads downstage of a
       vocalist, both aimed at them by the scene. With a red one lit the chest
       reads red, with a blue one blue, with both the mix; with both dark it
       is the neutral body. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    StageTarget *who = props->addStageTarget();
    who->setName("Vocalist");
    who->setKind(StageTarget::Kind::Person);
    who->setPosition(QVector3D(3.0f, 3.0f, 0.0f));
    who->setColor(QColor(255, 180, 0));

    Fixture *left  = makeMover(doc, 20.0, 0, "Front L", QVector3D(2.4f, 6.0f, 3.5f));
    Fixture *right = makeMover(doc, 20.0, 8, "Front R", QVector3D(3.6f, 6.0f, 3.5f));
    QVERIFY(left != nullptr && right != nullptr);

    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("At the vocalist");
    aim->setStageTargetId(who->id());
    QVERIFY(doc->addPalette(aim));
    Scene *scene = new Scene(doc);
    scene->setName("Solo");
    scene->addFixture(left->id());
    scene->addFixture(right->id());
    scene->addPalette(aim->id());
    QVERIFY(doc->addFunction(scene));

    // 6ch movers: Pan, Tilt, Dimmer, Red, Green, Blue.
    auto drive = [&](Fixture *f, int dim, int r, int b) {
        QByteArray u(512, char(0));
        const int a = int(f->address());
        u[a + 2] = char(dim);
        u[a + 3] = char(r);
        u[a + 5] = char(b);
        f->setChannelValues(u);
    };

    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(800, 620);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(0.0, 12.0);          // from the house, a little above
        v.setAmbient(0.25);
        v.setBeams(true);
        v.setLiveValues(true);
        v.setActiveScene(scene->id());

        /* The front of the chest, a hand's width off centre so the sample is
           on the body and not on the one line where both beams' own cones
           overlap it on screen. */
        const float foot = props->targetAimPoint(who, true).z()
                           - props->targetAimHeight(who, true);
        const float tall = props->targetFigureHeight(who);
        const QVector3D chest(3.0f, 3.0f + 0.13f * tall / 1.75f, foot + 0.72f * tall);
        auto chestColour = [&]() {
            const QImage img = v.grab().toImage();
            // w2s() is logical pixels; grab() on a HiDPI screen returns
            // device pixels (devicePixelRatio() times as many).
            const double dpr = img.devicePixelRatio();
            const QPoint c = (v.w2s(chest) * dpr).toPoint();
            const int r2 = qMax(1, int(2 * dpr));
            int r = 0, g = 0, b = 0, n = 0;
            for (int dy = -r2; dy <= r2; ++dy)
                for (int dx = -r2; dx <= r2; ++dx)
                {
                    const QColor px = img.pixelColor(c + QPoint(dx, dy));
                    r += px.red(); g += px.green(); b += px.blue(); ++n;
                }
            return QColor(r / n, g / n, b / n);
        };

        drive(left, 0, 255, 0);
        drive(right, 0, 0, 255);
        const QColor dark = chestColour();

        drive(left, 255, 255, 0);
        const QColor red = chestColour();

        drive(left, 0, 255, 0);
        drive(right, 255, 0, 255);
        const QColor blue = chestColour();

        drive(left, 255, 255, 0);
        const QColor both = chestColour();

        const QString report = QString("dark %1, red %2, blue %3, both %4")
            .arg(dark.name(), red.name(), blue.name(), both.name());
        QVERIFY2(qAbs(dark.red() - dark.blue()) < 30, qPrintable("unlit body is not neutral: " + report));
        QVERIFY2(red.red() > dark.red() + 60 && red.red() > red.blue() + 60,
                 qPrintable("a red light on the chest did not make it red: " + report));
        QVERIFY2(blue.blue() > dark.blue() + 60 && blue.blue() > blue.red() + 60,
                 qPrintable("a blue light on the chest did not make it blue: " + report));
        QVERIFY2(both.red() > dark.red() + 60 && both.blue() > dark.blue() + 60,
                 qPrintable("red and blue together did not add up: " + report));
    }

    delete doc;
}

void Monitor_Test::anAreaTargetShowsItsSize()
{
    /* "When having an area target, that should be visible on the studio." An
       Area was drawn as the same crosshair as a point, so its size -- the
       whole reason to make one -- showed nowhere. 3 m across and 1.5 m deep
       has to read as that, from above and edge on. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    StageTarget *area = props->addStageTarget();
    area->setName("Downstage centre");
    area->setKind(StageTarget::Kind::Area);
    area->setPosition(QVector3D(5.0f, 3.0f, 0.0f));
    area->setFootprint(QSizeF(3.0, 1.5));
    area->setColor(QColor(0, 255, 0));

    // yFrom/yTo: an elevation strip is measured along its lower rows only,
    // clear of the name label, whose bottom edge overlaps the strip's top.
    auto extent = [](const QImage &img, int yFrom = 0, int yTo = INT_MAX) {
        int x0 = INT_MAX, x1 = -1, y0 = INT_MAX, y1 = -1;
        for (int y = qMax(0, yFrom); y < qMin(img.height(), yTo); ++y)
            for (int x = 0; x < img.width(); ++x)
            {
                const QColor c = img.pixelColor(x, y);
                if (c.green() > c.red() + 40 && c.green() > c.blue() + 40)
                {
                    x0 = qMin(x0, x); x1 = qMax(x1, x);
                    y0 = qMin(y0, y); y1 = qMax(y1, y);
                }
            }
        return QRect(QPoint(x0, y0), QPoint(x1, y1));
    };

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 5));
        // grab() on a HiDPI screen returns device pixels, devicePixelRatio()
        // times as many as m_cellPixels/m_unitValue describe -- scale to match.
        const double ppm = 1000.0 * view.m_cellPixels / view.m_unitValue
                          * view.devicePixelRatioF();

        view.setViewPOV(MonitorGraphicsView::PovTop);
        view.updateTargets();
        QRect top = extent(view.grab().toImage());
        QVERIFY2(qAbs(top.width() - 3.0 * ppm) < 12 && qAbs(top.height() - 1.5 * ppm) < 12,
                 qPrintable(QString("from above the area is %1 x %2 px, expected %3 x %4")
                            .arg(top.width()).arg(top.height()).arg(3.0 * ppm).arg(1.5 * ppm)));

        // mapFromScene() is logical widget pixels; the strip window below
        // has to move to device pixels along with the image it indexes.
        auto stripRow = [&]() {
            TargetItem *ti = view.m_targetItems.value(area->id(), nullptr);
            const double y = ti ? view.mapFromScene(ti->pos()).y() : 0;
            return int(y * view.devicePixelRatioF());
        };
        const int rowSpan = qMax(1, int(view.devicePixelRatioF()));
        view.setViewPOV(MonitorGraphicsView::PovFront);
        view.updateTargets();
        QRect front = extent(view.grab().toImage(), stripRow() + rowSpan, stripRow() + 3 * rowSpan);
        QVERIFY2(qAbs(front.width() - 3.0 * ppm) < 12,
                 qPrintable(QString("from the front the area is %1 px wide, expected %2")
                            .arg(front.width()).arg(3.0 * ppm)));

        view.setViewPOV(MonitorGraphicsView::PovSide);
        view.updateTargets();
        QRect side = extent(view.grab().toImage(), stripRow() + rowSpan, stripRow() + 3 * rowSpan);
        QVERIFY2(qAbs(side.width() - 1.5 * ppm) < 12,
                 qPrintable(QString("from the side the area is %1 px wide, expected %2")
                            .arg(side.width()).arg(1.5 * ppm)));

        // A plain mark is still just the mark.
        area->setKind(StageTarget::Kind::None);
        view.setViewPOV(MonitorGraphicsView::PovTop);
        view.updateTargets();
        QRect mark = extent(view.grab().toImage());
        // The mark and its name label, nowhere near 3 m of ground.
        QVERIFY2(mark.width() < 1.5 * ppm,
                 qPrintable(QString("a plain target drew %1 px wide").arg(mark.width())));
    }

    delete doc;
}

void Monitor_Test::aPersonTargetIsDrawnAsAPerson()
{
    /* "Changed target to a person and it's not visualized." The kind already
       moved the aim to chest height; nothing drew it. A Person has to read as
       one in the plot and in the rig, standing on the deck under it rather
       than at the target's own Z, and tall enough that its chest is the aim
       point. Measured against the same target as a plain mark. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(2.0f); deck->setOriginY(2.0f);
    deck->setWidth(4.0f); deck->setDepth(2.0f); deck->setHeight(0.5f);

    StageTarget *who = props->addStageTarget();
    who->setName("Vocalist");
    // Z left at the floor on purpose: a person stands on the deck regardless.
    who->setPosition(QVector3D(3.0f, 3.0f, 0.0f));
    who->setColor(QColor(0, 255, 0));

    const QColor bg(0, 0, 0);
    auto isFigure = [](const QColor &c) {
        return c.green() > c.red() + 60 && c.green() > c.blue() + 60;
    };
    struct Extent { int n = 0; int top = INT_MAX; int bottom = -1; };
    auto measure = [&](const QImage &img) {
        Extent e;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (isFigure(img.pixelColor(x, y)))
                {
                    ++e.n;
                    e.top = qMin(e.top, y);
                    e.bottom = qMax(e.bottom, y);
                }
        return e;
    };

    /* ---- The plot, from the front ---- */
    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        /* Five rows, not seven: the elevation's floor line is the bottom of
           the grid, and at seven it sat under the scrollbar -- a target at
           floor height was clipped out of the grab entirely. */
        view.setGridSize(QSize(10, 5));
        view.setViewPOV(MonitorGraphicsView::PovFront);
        const double pxPerMetre = 1000.0 * view.m_cellPixels / view.m_unitValue;

        who->setKind(StageTarget::Kind::None);
        view.updatePlatforms();
        view.updateTargets();
        const Extent mark = measure(view.grab().toImage());

        who->setKind(StageTarget::Kind::Person);
        view.updateTargets();
        const Extent person = measure(view.grab().toImage());

        const int markH = mark.bottom - mark.top;
        const int personH = person.bottom - person.top;
        QVERIFY2(mark.n > 0 && person.n > 0, "the target was not drawn at all");
        QVERIFY2(personH > 1.5 * pxPerMetre && markH < 0.8 * pxPerMetre,
                 qPrintable(QString("from the front a person should stand about "
                                    "1.75 m tall (%1 px/m): got %2 px, and %3 px "
                                    "for the plain mark").arg(pxPerMetre)
                            .arg(personH).arg(markH)));

        /* Feet on the deck top, not at the target's own Z on the floor. Read
           off the figure's outline: the lowest lit pixel is the crosshair,
           which stays at the target's stored Z because that is what dragging
           it in an elevation edits. */
        TargetItem *ti = view.m_targetItems.value(who->id(), nullptr);
        QVERIFY(ti != nullptr);
        const double feetY = ti->mapToScene(ti->figurePath().boundingRect().bottomLeft()).y();
        const double deckTopY = view.projectMm(3000.0, 3000.0, 500.0).y();
        QVERIFY2(qAbs(feetY - deckTopY) <= 2.0,
                 qPrintable(QString("the person's feet are at scene y=%1, the deck "
                                    "top is at y=%2").arg(feetY).arg(deckTopY)));

        // From above it is shoulders and a head: more than the bare mark.
        view.setViewPOV(MonitorGraphicsView::PovTop);
        view.updateTargets();
        const Extent above = measure(view.grab().toImage());
        who->setKind(StageTarget::Kind::None);
        view.updateTargets();
        const Extent aboveMark = measure(view.grab().toImage());
        QVERIFY2(above.n > aboveMark.n + 200,
                 qPrintable(QString("from above a person drew %1 px against the "
                                    "plain mark's %2").arg(above.n).arg(aboveMark.n)));
    }

    /* ---- The rig ---- */
    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(800, 620);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(25.0, 20.0);

        who->setKind(StageTarget::Kind::None);
        const Extent bare = measure(v.grab().toImage());
        who->setKind(StageTarget::Kind::Person);
        v.update();
        const Extent figure = measure(v.grab().toImage());
        QVERIFY2(bare.n == 0,
                 qPrintable(QString("a plain target drew %1 px in the rig").arg(bare.n)));
        QVERIFY2(figure.n > 300,
                 qPrintable(QString("a person target drew only %1 px in the rig")
                            .arg(figure.n)));
    }
    Q_UNUSED(bg)

    delete doc;
}

void Monitor_Test::aBeamFadesAlongItsLengthWhicheverWayItPoints()
{
    /* "Some lights are right, some are solid." The cone is one polygon whose
       alpha fades from the lens to the far end, and the rasteriser fitted that
       fade as a plane through the polygon's first three corners. A cone is not
       a plane: when those three were all far-end points the fade came out flat,
       and when they straddled the lens it overshot to solid. Which happened
       depended only on which way the beam pointed on screen. This is the LED
       SPOT from the Targets scene: on the floor, thrown up and toward a camera
       in the house, where it drew as a hard disc hanging in the air. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    /* A stage-sized low deck, so the view frames the stage the way the show
       does rather than zooming in on one small fixture and throwing the beam
       off the edge. */
    StagePlatform *stage = props->addPlatform();
    stage->setOriginX(0.0f); stage->setOriginY(-4.0f);
    stage->setWidth(11.0f); stage->setDepth(14.0f); stage->setHeight(0.01f);
    // Opaque, like a real deck: a see-through one is a translucent shape of its
    // own, and ordering two of those is a different question from this one.
    stage->setColor(QColor(60, 60, 70));

    const QVector3D apexAt(5.30f, 0.93f, 0.02f);
    const QVector3D dir = QVector3D(0.22f, 0.84f, 0.50f).normalized();
    Fixture *spot = makeMover(doc, 14.0, 0, "LED SPOT", apexAt);
    QVERIFY(spot != nullptr);

    StageTarget *up = props->addStageTarget();
    up->setName("Up and out");
    up->setPosition(apexAt + dir * 4.0f);
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setStageTargetId(up->id());
    QVERIFY(doc->addPalette(aim));
    Scene *scene = new Scene(doc);
    scene->setName("Targets");
    scene->addFixture(spot->id());
    scene->addPalette(aim->id());
    QVERIFY(doc->addFunction(scene));

    // Full white: Pan, Tilt, Dimmer, R, G, B.
    QByteArray u(512, char(0));
    u[2] = char(255); u[3] = char(255); u[4] = char(255); u[5] = char(255);
    spot->setChannelValues(u);

    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(1190, 660);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setAmbient(0.0);
        v.setBeams(true);
        v.setLiveValues(true);
        v.setActiveScene(scene->id());

        // w2s() answers in logical pixels; grab() on a HiDPI screen returns
        // device pixels (devicePixelRatio() times as many). luminance()'s
        // sample point (and its own +/-1 logical-pixel window) both need
        // scaling up to match img's own indexing.
        auto luminance = [](const QImage &img, const QPointF &at) {
            const double dpr = img.devicePixelRatio();
            const QPoint atPx = (at * dpr).toPoint();
            const int r = qMax(1, int(dpr));
            double sum = 0.0;
            int n = 0;
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx)
                {
                    const QPoint q = atPx + QPoint(dx, dy);
                    if (!img.rect().contains(q)) continue;
                    const QColor c = img.pixelColor(q);
                    sum += 0.3 * c.red() + 0.59 * c.green() + 0.11 * c.blue();
                    ++n;
                }
            return n > 0 ? sum / n : 0.0;
        };

        /* From the front as in the report, and from a few other places, so
           the answer cannot depend on which corner of the cone comes first. */
        for (double az : { 0.0, 30.0, -40.0, 160.0 })
        {
            v.setAngledView(az, 4.0);
            const QImage img = v.grab().toImage();
            QVERIFY2(v.m_frameBeams.size() == 1,
                     qPrintable(QString("%1 beams in the frame, expected 1")
                                .arg(v.m_frameBeams.size())));
            const StructureStudioView::FrameBeam b = v.m_frameBeams.first();
            const QVector3D axis = b.dir.normalized();
            const QPointF nearPx = v.w2s(b.apex + axis * float(b.length * 0.12));
            const QPointF farPx  = v.w2s(b.apex + axis * float(b.length * 0.55));
            QVERIFY2(img.rect().contains(nearPx.toPoint()) && img.rect().contains(farPx.toPoint()),
                     qPrintable(QString("az %1: a sample point is off screen").arg(az)));
            const double nearL = luminance(img, nearPx);
            const double farL  = luminance(img, farPx);
            const double bg    = luminance(img, QPointF(5, 5));
            QVERIFY2(nearL > bg + 20.0,
                     qPrintable(QString("az %1: the beam is not visible near the lens "
                                        "(%2 against background %3)").arg(az).arg(nearL).arg(bg)));
            // Linear from the lens to nothing at 8 m: 0.55 of the way is well under 0.88.
            QVERIFY2(farL - bg < (nearL - bg) * 0.75,
                     qPrintable(QString("az %1: the beam is as bright at its far end (%2) "
                                        "as near the lens (%3) -- it is not fading")
                                .arg(az).arg(farL).arg(nearL)));
        }
    }

    delete doc;
}

void Monitor_Test::aBeamLandsAsAnOvalAtAnAngle()
{
    /* "Shouldn't the end of the beam be relative to the feature it's projected
       on -- an oval on the floor?" A cone cut by the floor is a circle only
       when it arrives straight down. At 30 degrees below the horizontal its
       footprint is about twice as long, along the beam, as it is wide -- and
       it lies ON the floor, not square across the beam in mid-air. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(-2.0f); deck->setOriginY(-2.0f);
    deck->setWidth(4.0f); deck->setDepth(4.0f); deck->setHeight(0.6f);

    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.reload();

        const double half = qDegreesToRadians(5.0);
        auto extents = [](const QVector<QVector3D> &pts, double &along, double &across,
                          double &zMin, double &zMax) {
            float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
            zMin = 1e9; zMax = -1e9;
            for (const QVector3D &q : pts)
            {
                x0 = qMin(x0, q.x()); x1 = qMax(x1, q.x());
                y0 = qMin(y0, q.y()); y1 = qMax(y1, q.y());
                zMin = qMin(zMin, double(q.z())); zMax = qMax(zMax, double(q.z()));
            }
            along = double(y1 - y0);
            across = double(x1 - x0);
        };

        // Off the deck, 30 degrees down toward downstage, onto the floor.
        const QVector3D apex(0.0f, 3.0f, 4.0f);
        const QVector3D down30(0.0f, float(qCos(qDegreesToRadians(30.0))),
                               -float(qSin(qDegreesToRadians(30.0))));
        bool hit = false;
        const double len = v.beamThrow(apex, down30, &hit);
        QVERIFY(hit);
        double along, across, zMin, zMax;
        extents(v.beamFootprint(apex, down30, half, len, true), along, across, zMin, zMax);
        QVERIFY2(zMin > -1e-3 && zMax < 1e-3,
                 qPrintable(QString("the pool is not on the floor: z %1..%2").arg(zMin).arg(zMax)));
        QVERIFY2(along / across > 1.7 && along / across < 2.5,
                 qPrintable(QString("at 30 degrees the pool should be about twice as long "
                                    "as wide, got %1 x %2").arg(along).arg(across)));

        // Straight down it is round.
        const QVector3D straight(0.0f, 0.0f, -1.0f);
        const double lenDown = v.beamThrow(apex, straight);
        extents(v.beamFootprint(apex, straight, half, lenDown, true), along, across, zMin, zMax);
        QVERIFY2(qAbs(along / across - 1.0) < 0.05,
                 qPrintable(QString("straight down the pool is %1 x %2").arg(along).arg(across)));

        // Landing on the deck, it lies on the deck top.
        const QVector3D overDeck(0.0f, 0.5f, 4.0f);
        const QVector3D toDeck = QVector3D(0.0f, 1.0f, -3.0f).normalized();
        const double lenDeck = v.beamThrow(overDeck, toDeck, &hit);
        QVERIFY(hit);
        extents(v.beamFootprint(overDeck, toDeck, half, lenDeck, true), along, across, zMin, zMax);
        QVERIFY2(qAbs(zMin - 0.6) < 1e-3 && qAbs(zMax - 0.6) < 1e-3,
                 qPrintable(QString("the pool on the deck is at z %1..%2").arg(zMin).arg(zMax)));
    }

    delete doc;
}

void Monitor_Test::aBeamCarriesOnPastWhatItIsAimedAt()
{
    /* "Why does the beam end on the target .. it won't in reality." A target
       is a point in the air, not a surface: light aimed at a subject carries
       on past them. This test used to assert the opposite -- the cone was cut
       at the subject so follow-spots aimed up at chest height did not sail on
       -- and it is kept, inverted, so the cut cannot quietly come back. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    // Low fixture, target above it: nothing below to stop the beam.
    Fixture *head = makeMover(doc, 10.0, 0, "Low Spot", QVector3D(1.0f, 1.0f, 0.6f));
    QVERIFY(head != nullptr);

    StageTarget *tgt = props->addStageTarget();
    tgt->setX(4.0f); tgt->setY(1.0f); tgt->setZ(1.8f);
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("Up"); aim->setStageTargetId(tgt->id());
    QVERIFY(doc->addPalette(aim));

    Scene *scene = new Scene(doc);
    scene->setName("Upward");
    scene->addFixture(head->id());
    scene->addPalette(aim->id());
    QVERIFY(doc->addFunction(scene));

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(1000, 800);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setActiveScene(scene->id());

    /* Pull back far enough that the OVERSHOOT is on screen. With one fixture
       the auto-fit frames the fixture, and the first version of this test
       sampled a point at (1620,-16) on a 600x460 canvas -- off the edge, so
       nothing was lit either way and it passed with the fix reverted. */
    v.m_zoomed = true;
    v.m_scale *= 0.25;

    QVector3D pt;
    QVERIFY(v.aimPointFor(head->id(), pt));
    const QVector3D org = props->fixtureRigPosition(head->id());
    const QVector3D dir = (pt - org).normalized();
    QVERIFY2(dir.z() > 0.05f, "this test needs an UPWARD beam to be meaningful");

    const double toTarget = double((pt - org).length());
    const double floorThrow = v.beamThrow(org, dir);
    QVERIFY2(floorThrow > toTarget + 1.0,
             qPrintable(QString("the no-hit throw (%1) must overshoot the target "
                                "(%2) or this proves nothing")
                        .arg(floorThrow).arg(toTarget)));

    /* Rendered, the beam is still there a metre and a half beyond the
       subject, on its way to wherever it really ends. */
    v.setBeams(true);
    v.setLiveValues(false);
    QByteArray u(512, char(0));
    u[2] = char(255); u[3] = char(255); u[4] = char(255); u[5] = char(255);
    head->setChannelValues(u);
    v.setLiveValues(true);

    const QImage img = v.grab().toImage();
    // w2s() is logical pixels; grab() on a HiDPI screen returns device pixels
    // (devicePixelRatio() times as many) -- scale the sample point and its
    // logical-pixel search window to match.
    const double dpr = img.devicePixelRatio();
    const QPointF beyond = v.w2s(org + dir * float(toTarget + 1.5)) * dpr;
    int lit = 0;
    for (int dy = -8; dy <= 8; ++dy)
        for (int dx = -8; dx <= 8; ++dx)
        {
            const QPoint q(beyond.toPoint() + QPoint(int(dx * dpr), int(dy * dpr)));
            if (!img.rect().contains(q)) continue;
            const QColor c = img.pixelColor(q);
            if (c.red() * 0.3 + c.green() * 0.59 + c.blue() * 0.11 > 55)
                ++lit;
        }
    QVERIFY2(lit > 60,
             qPrintable(QString("only %1 px lit 1.5 m past the subject -- the "
                                "beam is stopping at what it is aimed at")
                        .arg(lit)));

    delete doc;
}

void Monitor_Test::aSelectedLookLightsTheRigInDesign()
{
    /* "Why does it need to be live? It should work in design."
     *
       Quite right. A look is a design-time thing: you build it from a dimmer, a
       colour and an aim, and the rig view's job is to show what it does --
       whether or not a desk is currently outputting it. Beam brightness used to
       come from live DMX alone, so the whole thing was conditional on a toggle
       that is really about something else.
     *
       The selected scene's palettes are now resolved into channel values shaped
       exactly like live DMX, so colour, level, beam and visibility all read it
       without knowing the difference. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Fixture *head = makeMover(doc, 14.0, 0, "Design Head", QVector3D(3.0f, 3.0f, 5.0f));
    QVERIFY(head != nullptr);

    StageTarget *tgt = props->addStageTarget();
    tgt->setX(3.0f); tgt->setY(3.0f); tgt->setZ(0.0f);          // straight below
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("Down"); aim->setStageTargetId(tgt->id());
    QVERIFY(doc->addPalette(aim));

    QLCPalette *dim = new QLCPalette(QLCPalette::Dimmer, doc);
    dim->setName("Full"); dim->setValue(255);
    QVERIFY(doc->addPalette(dim));

    QLCPalette *col = new QLCPalette(QLCPalette::Color, doc);
    col->setName("Green"); col->setValue(QColor(0, 255, 0));
    QVERIFY(doc->addPalette(col));

    Scene *scene = new Scene(doc);
    scene->setName("Design Look");
    scene->addFixture(head->id());
    scene->addPalette(dim->id());
    scene->addPalette(col->id());
    scene->addPalette(aim->id());
    QVERIFY(doc->addFunction(scene));

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(700, 560);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(25.0, 20.0);
    v.setAmbient(0.0);
    v.setBeams(true);
    v.setLiveValues(false);            // DESIGN. No DMX anywhere.

    // Nothing driving it and no scene selected: nothing to show.
    head->setChannelValues(QByteArray(512, char(0)));
    auto greenPixels = [&]() {
        const QImage img = v.grab().toImage();
        int n = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
            {
                const QColor c = img.pixelColor(x, y);
                if (c.green() > 55 && c.green() > c.red() * 2 && c.green() > c.blue() * 2)
                    ++n;
            }
        return n;
    };
    const int unselected = greenPixels();

    v.setActiveScene(scene->id());
    QVERIFY2(v.m_sceneValues.contains(head->id()),
             "the selected scene resolved no output for a fixture it drives");

    const int selected = greenPixels();
    QVERIFY2(selected > 400,
             qPrintable(QString("selecting a look with a dimmer and a colour lit "
                                "nothing in design (%1 px)").arg(selected)));
    QVERIFY2(selected > unselected * 4,
             qPrintable(QString("selecting the look changed almost nothing "
                                "(%1 -> %2)").arg(unselected).arg(selected)));

    /* And it follows the look's actual level rather than assuming full -- the
       reason a rig can look dark is often that the look IS dark. */
    dim->setValue(14);                                   // about 5%
    v.setActiveScene(Function::invalidId());
    v.setActiveScene(scene->id());
    const int dimmed = greenPixels();
    QVERIFY2(dimmed * 3 < selected,
             qPrintable(QString("a look at 5%% drew nearly as much as one at "
                                "full (%1 against %2)").arg(dimmed).arg(selected)));

    delete doc;
}

void Monitor_Test::anUndrivenHeadPointsAtTheScenesTarget()
{
    /* "When I set a target on a position I don't see that reflected in the rig
       view, but I see the lines in the studio."
     *
       The studio draws a dashed line from every aimable fixture the scene
       touches to the StageTarget the scene's Aim palette names. The rig view
       read live pan/tilt DMX and nothing else, so with nothing driving the rig
       it had nothing to show and left the heads wherever they were last
       pointed. The target IS the intent -- and live DMX still wins over it when
       there is any, because that is what the rig is actually doing. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Fixture *head = makeMover(doc, 14.0, 0, "Aimed", QVector3D(1.0f, 1.0f, 5.0f));
    QVERIFY(head != nullptr);

    StageTarget *tgt = props->addStageTarget();
    QVERIFY(tgt != nullptr);
    tgt->setX(6.0f); tgt->setY(1.0f); tgt->setZ(0.0f);   // well off to one side

    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("At Target");
    aim->setStageTargetId(tgt->id());
    QVERIFY(doc->addPalette(aim));

    Scene *scene = new Scene(doc);
    scene->setName("Aim Scene");
    scene->addFixture(head->id());
    scene->addPalette(aim->id());
    QVERIFY(doc->addFunction(scene));

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(700, 520);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(20.0, 20.0);

    // No active scene yet: nothing to aim at.
    QVERIFY(v.m_aimTarget.isEmpty());

    v.setActiveScene(scene->id());
    QVERIFY2(v.m_aimTarget.contains(head->id()),
             "the scene's Aim palette did not reach the rig view");

    /* The direction it should be pointing: from the head, off toward +X and
       down. Pulled from the same association the studio draws. */
    QVector3D at;
    QVERIFY(v.aimPointFor(head->id(), at));
    const QVector3D want = (at - props->fixtureRigPosition(head->id())).normalized();
    QVERIFY2(want.x() > 0.5f && want.z() < -0.5f,
             qPrintable(QString("the aim direction is not toward the target: "
                                "%1,%2,%3").arg(double(want.x()))
                        .arg(double(want.y())).arg(double(want.z()))));

    /* THE CASE THAT ACTUALLY FAILED. Live output is on -- it is on by default
       in the overview -- and nothing is driving the rig, so every channel reads
       zero. fixtureAimDirection() reports that quite happily as a real
       direction (pan 0, tilt 0), because all-zero DMX and "aimed downstage" are
       indistinguishable from the channel values alone. Preferring live DMX
       therefore left every head pointing downstage the moment you merely
       SELECTED a followspot scene, while the studio drew its dashed lines to
       the target. The selected scene has to win. */
    v.setLiveValues(true);
    head->setChannelValues(QByteArray(512, char(0)));
    v.grab();

    QVector3D dmxSays;
    QVERIFY2(fixtureAimDirection(head, props->fixtureRigProps(head->id()), dmxSays),
             "expected undriven pan/tilt to still report a direction -- if this "
             "ever stops being true, the precedence below matters less");
    QVERIFY2(QVector3D::dotProduct(dmxSays, want) < 0.9f,
             "the test is not discriminating: zero DMX happens to point at the "
             "target, so it cannot show which one won");

    /* What the view actually draws. */
    QVector3D drawn;
    {
        QVector3D pt;
        QVERIFY(v.aimPointFor(head->id(), pt));
        drawn = (pt - props->fixtureRigPosition(head->id())).normalized();
    }
    QVERIFY2(QVector3D::dotProduct(drawn, want) > 0.99f,
             "with a scene selected, the head must point at that scene's target "
             "even while live output is on and nothing is driving it");

    /* And it has to be VISIBLE, which turning the head box quietly toward the
       target is not: at whole-rig zoom a mover is a few pixels across, and a
       scene that is merely selected sends no DMX, so there is no beam either.
       The studio answers this with a dashed line; so does this now -- drawn as
       an ANNOTATION over the scene rather than depth-tested into it, because on
       a real rig the run between a head and a floor target skims horizontally
       through the decks and would be almost entirely (and correctly) hidden. */
    {
        StagePlatform *wall = props->addPlatform();
        wall->setName("In The Way");
        wall->setOriginX(2.0f); wall->setOriginY(0.0f);
        wall->setWidth(2.5f); wall->setDepth(2.5f); wall->setHeight(3.0f);
        wall->setColor(QColor(30, 30, 30));

        tgt->setColor(QColor(255, 0, 255));      // unmistakable
        v.setActiveScene(Function::invalidId());
        v.setActiveScene(scene->id());
        v.reload();

        const QImage img = v.grab().toImage();

        /* Sample where the deck is actually IN THE WAY, not just anywhere along
           the run -- most of the line is in clear air, so counting the whole
           image proves nothing about occlusion. The head is 5 m up and the
           target on the floor, so the run passes INSIDE this 3 m deck around
           three-fifths of the way along. */
        const QVector3D head3 = props->fixtureRigPosition(head->id());
        QVector3D aimPt2;
        QVERIFY(v.aimPointFor(head->id(), aimPt2));
        const QVector3D buried = head3 + (aimPt2 - head3) * 0.55f;
        // w2s() is logical pixels; grab() on a HiDPI screen returns device
        // pixels (devicePixelRatio() times as many) -- scale the probe and
        // its logical-pixel search window to match.
        const double dpr = img.devicePixelRatio();
        const QPointF probe = v.w2s(buried) * dpr;

        int traced = 0;
        for (int dy = -22; dy <= 22; ++dy)
            for (int dx = -22; dx <= 22; ++dx)
            {
                const QPoint q(probe.toPoint() + QPoint(int(dx * dpr), int(dy * dpr)));
                if (!img.rect().contains(q)) continue;
                const QColor c = img.pixelColor(q);
                if (c.red() > 140 && c.blue() > 140 && c.green() < 90)
                    ++traced;
            }
        QVERIFY2(traced > 4,
                 qPrintable(QString("the aim trace is not visible where a solid "
                                    "deck stands between head and target (%1 px "
                                    "there) -- depth-tested, it vanishes into "
                                    "the scenery and the aim is unreadable again")
                            .arg(traced)));
        props->removePlatform(wall->id());
    }

    /* A fixture that CANNOT aim is left out -- a wash has nothing to point, and
       the studio makes the same check before drawing it a line. */
    Fixture *par = new Fixture(doc);
    par->setName("Wash");
    par->setChannels(3);
    QVERIFY(doc->addFixture(par));
    scene->addFixture(par->id());
    v.setActiveScene(Function::invalidId());
    v.setActiveScene(scene->id());
    QVERIFY2(v.m_aimTarget.contains(par->id()) == false,
             "a fixture with no pan or tilt was given an aim direction");

    delete doc;
}

void Monitor_Test::aBeamFadesAllTheWayOutWithTheDimmer()
{
    /* "Dimming doesn't really work .. it never fades out." Measured, and quite
       right: the beam's alpha had a constant 22 under it, so a head at 3% threw
       a beam measuring 40 against a full beam's 78 -- barely dimmer -- and then
       snapped to nothing at zero. Alpha is now strictly proportional to the
       level, with no floor. */
    Doc *doc = new Doc(this);
    Fixture *head = makeMover(doc, 14.0, 0, "Fader", QVector3D(3.0f, 3.0f, 5.0f));
    QVERIFY(head != nullptr);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(700, 560);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(25.0, 20.0);
    v.setLiveValues(true);
    v.setAmbient(0.0);

    auto beamLumaAt = [&](int level) {
        QByteArray u(512, char(0));
        u[0] = char(128); u[1] = char(128);          // straight down
        u[2] = char(level);
        u[3] = char(255); u[4] = char(255); u[5] = char(255);
        head->setChannelValues(u);
        const QImage img = v.grab().toImage();
        // w2s() is logical pixels; grab() on a HiDPI screen returns device
        // pixels (devicePixelRatio() times as many) -- scale accordingly.
        const double dpr = img.devicePixelRatio();
        const QPointF at = v.w2s(v.fixtureCentre(head->id())) * dpr;
        const QPointF probe(at.x(), at.y() + 120 * dpr);   // into the throw
        double sum = 0; int n = 0;
        for (int dy = -14; dy <= 14; ++dy)
            for (int dx = -14; dx <= 14; ++dx)
            {
                const QPoint q(probe.toPoint() + QPoint(int(dx * dpr), int(dy * dpr)));
                if (!img.rect().contains(q)) continue;
                const QColor c = img.pixelColor(q);
                sum += c.red() * 0.3 + c.green() * 0.59 + c.blue() * 0.11;
                ++n;
            }
        return n ? sum / n : 0.0;
    };

    const double out  = beamLumaAt(0);      // the empty room, for reference
    const double full = beamLumaAt(255);
    const double half = beamLumaAt(128);
    const double low  = beamLumaAt(8);

    QVERIFY2(full - out > 35.0,
             qPrintable(QString("a head at full threw almost nothing (%1 against "
                                "an empty room's %2)").arg(full).arg(out)));

    /* The real complaint: at 3% the beam has to be nearly gone. Anything above
       about a fifth of full here is the old constant floor coming back. */
    QVERIFY2((low - out) < (full - out) * 0.2,
             qPrintable(QString("a head at 3%% still threw %1 of a full beam's "
                                "%2 over an empty room's %3 -- it is not fading "
                                "out").arg(low - out).arg(full - out).arg(out)));

    // And it is monotonic in between, not a step.
    QVERIFY2(half > low && full > half,
             qPrintable(QString("beam brightness is not monotonic in the dimmer: "
                                "%1 / %2 / %3").arg(low).arg(half).arg(full)));

    delete doc;
}

void Monitor_Test::aBeamStopsAtWhatItLandsOn()
{
    /* A beam has to end somewhere. The floor, or the top of a step it is
       thrown over -- that is what catches light on a rig built out of steps.
       One thrown level or upward hits nothing and gets a sensible throw
       instead of running to the horizon. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(500, 400);
    v.reload();

    const QVector3D apex(3.0f, 3.0f, 6.0f);

    // Straight down onto a bare floor: the whole 6 m.
    QCOMPARE(qRound(v.beamThrow(apex, QVector3D(0, 0, -1)) * 100.0) / 100.0, 6.0);

    // Level, and upward: nothing to hit.
    QVERIFY(v.beamThrow(apex, QVector3D(0, 1, 0)) > 7.0);
    QVERIFY(v.beamThrow(apex, QVector3D(0, 0, 1)) > 7.0);

    // A step under it stops the beam early.
    StagePlatform *pl = props->addPlatform();
    pl->setName("Riser"); pl->setOriginX(2.0f); pl->setOriginY(2.0f);
    pl->setWidth(2.0f); pl->setDepth(2.0f); pl->setHeight(1.5f);
    QCOMPARE(qRound(v.beamThrow(apex, QVector3D(0, 0, -1)) * 100.0) / 100.0, 4.5);

    /* Aimed down but off to one side, MISSING the step's footprint: back to
       the floor. Without the footprint check a step would catch beams that
       pass nowhere near it. */
    const QVector3D away(9.0f, 9.0f, 6.0f);
    QCOMPARE(qRound(v.beamThrow(away, QVector3D(0, 0, -1)) * 100.0) / 100.0, 6.0);

    delete doc;
}

void Monitor_Test::aLitMoverThrowsAVisibleBeam()
{
    /* The beam is the point of the exercise: a head at full should put light
       between itself and the floor, in its own colour, and a head at zero
       should not. */
    Doc *doc = new Doc(this);
    Fixture *head = makeMover(doc, 14.0, 0, "Beamer", QVector3D(3.0f, 3.0f, 5.0f));
    QVERIFY(head != nullptr);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(800, 620);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(25.0, 20.0);
    v.setLiveValues(true);
    v.setAmbient(0.25);

    auto greenPixels = [&]() {
        const QImage img = v.grab().toImage();
        int n = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
            {
                const QColor c = img.pixelColor(x, y);
                if (c.green() > 60 && c.green() > c.red() * 2 && c.green() > c.blue() * 2)
                    ++n;
            }
        return n;
    };

    // Tilt centred = straight down at the floor; full dimmer, green.
    QByteArray u(512, char(0));
    u[0] = char(128); u[1] = char(128); u[2] = char(255); u[4] = char(255);
    head->setChannelValues(u);
    const int lit = greenPixels();

    u[2] = char(0);                          // dimmer out, colour still set
    head->setChannelValues(u);
    const int dark = greenPixels();

    QVERIFY2(lit > 400,
             qPrintable(QString("a head at full threw no beam (%1 px)").arg(lit)));
    QVERIFY2(lit > dark * 4,
             qPrintable(QString("dousing the head barely changed its beam "
                                "(%1 lit vs %2 out)").arg(lit).arg(dark)));

    // And the toggle turns them off without touching anything else.
    u[2] = char(255);
    head->setChannelValues(u);
    v.setBeams(false);
    const int off = greenPixels();
    QVERIFY2(off * 4 < lit,
             qPrintable(QString("turning beams off left them on screen "
                                "(%1 vs %2)").arg(off).arg(lit)));
    v.setBeams(true);

    delete doc;
}

void Monitor_Test::aTwoArmHeadDrawsEachArmOnItsOwn()
{
    /* "They are three beams on one head": a two-arm beam fixture, channelled
       the way the real one is -- X1 X2 Y1 Y2, a shared dimmer and strobe that
       belong to no head, then each arm's RGBW. Each arm is one head, and each
       head throws three beams in its own colour. Three things have to show:
         - each arm in ITS colour, not both in the mix of the two;
         - each arm pointing where ITS tilt says;
         - three beams per arm when the lens says three. */
    Doc *doc = new Doc(this);

    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Two Arm Beam");
    def->setType(QLCFixtureDef::MovingHead);
    struct Spec { const char *name; QLCChannel::Group group; QLCChannel::PrimaryColour colour; };
    const Spec spec[15] = {
        { "Pan 1",  QLCChannel::Pan,  QLCChannel::NoColour },
        { "Pan 2",  QLCChannel::Pan,  QLCChannel::NoColour },
        { "Tilt 1", QLCChannel::Tilt, QLCChannel::NoColour },
        { "Tilt 2", QLCChannel::Tilt, QLCChannel::NoColour },
        { "Intensity", QLCChannel::Intensity, QLCChannel::NoColour },
        { "Strobe", QLCChannel::Shutter, QLCChannel::NoColour },
        { "Red 1",   QLCChannel::Intensity, QLCChannel::Red },
        { "Green 1", QLCChannel::Intensity, QLCChannel::Green },
        { "Blue 1",  QLCChannel::Intensity, QLCChannel::Blue },
        { "White 1", QLCChannel::Intensity, QLCChannel::White },
        { "Red 2",   QLCChannel::Intensity, QLCChannel::Red },
        { "Green 2", QLCChannel::Intensity, QLCChannel::Green },
        { "Blue 2",  QLCChannel::Intensity, QLCChannel::Blue },
        { "White 2", QLCChannel::Intensity, QLCChannel::White },
        { "Macro",  QLCChannel::Maintenance, QLCChannel::NoColour } };
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("15 Channel");
    for (int k = 0; k < 15; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(spec[k].name); ch->setGroup(spec[k].group);
        ch->setControlByte(QLCChannel::MSB);
        if (spec[k].colour != QLCChannel::NoColour) ch->setColour(spec[k].colour);
        def->addChannel(ch); mode->insertChannel(ch, k);
    }
    QLCFixtureHead arm1, arm2;
    for (quint32 c : { 0u, 2u, 6u, 7u, 8u, 9u })    arm1.addChannel(c);
    for (quint32 c : { 1u, 3u, 10u, 11u, 12u, 13u }) arm2.addChannel(c);
    mode->insertHead(-1, arm1);
    mode->insertHead(-1, arm2);

    /* Wide and narrow, so the beams of one arm land clear of each other and
       can be counted by their area: a metre per arm, 2-degree lenses. */
    auto setBeams = [&](int beams) {
        QLCPhysical ph;
        ph.setWidth(2000); ph.setHeight(300); ph.setDepth(300);
        ph.setFocusPanMax(360); ph.setFocusTiltMax(180);
        ph.setLensDegreesMin(2); ph.setLensDegreesMax(2);
        ph.setLensBeamsPerHead(beams);
        mode->setPhysical(ph);
    };
    setBeams(3);
    def->addMode(mode);

    Fixture *fxi = new Fixture(doc);
    fxi->setName("UST"); fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(3); fxi->setAddress(0);
    QVERIFY(doc->addFixture(fxi));
    QCOMPARE(fxi->heads(), 2);
    doc->monitorProperties()->setFixturePosition(
        fxi->id(), 0, 0, QVector3D(3000.0f, 3000.0f, 4.0f));

    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(800, 620);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(25.0, 20.0);
        v.setLiveValues(true);
        v.setAmbient(0.25);

        struct Tally { int n = 0; double x = 0.0, y = 0.0; };
        auto measure = [&](Tally &red, Tally &blue) {
            const QImage img = v.grab().toImage();
            red = Tally(); blue = Tally();
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x)
                {
                    const QColor c = img.pixelColor(x, y);
                    Tally *t = nullptr;
                    if (c.red() > 60 && c.red() > c.green() * 2 && c.red() > c.blue() * 2)
                        t = &red;
                    else if (c.blue() > 60 && c.blue() > c.red() * 2 && c.blue() > c.green() * 2)
                        t = &blue;
                    if (t != nullptr) { ++t->n; t->x += x; t->y += y; }
                }
            for (Tally *t : { &red, &blue })
                if (t->n > 0) { t->x /= t->n; t->y /= t->n; }
        };
        auto drive = [&](int tilt1, int tilt2) {
            QByteArray u(512, char(0));
            u[0] = char(128); u[1] = char(128);
            u[2] = char(tilt1); u[3] = char(tilt2);
            u[4] = char(255);                 // the shared dimmer
            u[6] = char(255);                 // arm 1 red
            u[12] = char(255);                // arm 2 blue
            fxi->setChannelValues(u);
        };

        // Both arms straight down: one red, one blue -- not two magentas.
        drive(128, 128);
        Tally red, blue;
        measure(red, blue);
        QVERIFY2(red.n > 300 && blue.n > 300,
                 qPrintable(QString("each arm should throw its own colour, got "
                                    "%1 red px and %2 blue px").arg(red.n).arg(blue.n)));

        /* Tilt arm 2 about 30 degrees downstage -- far enough to move, not so
           far that its beams leave the frame, so the shift is measured on real
           beams rather than on the handful of pixels a beam thrown straight at
           the camera leaves behind. Its beams swing; arm 1's stay put. */
        drive(128, 170);
        Tally red2, blue2;
        measure(red2, blue2);
        const double redMoved  = QLineF(red.x, red.y, red2.x, red2.y).length();
        const double blueMoved = QLineF(blue.x, blue.y, blue2.x, blue2.y).length();
        QVERIFY2(blue2.n > 300 && blueMoved > 20.0 && redMoved < 3.0,
                 qPrintable(QString("tilting arm 2 moved its beams %1 px (%2 px of "
                                    "them) and arm 1's %3 px").arg(blueMoved)
                            .arg(blue2.n).arg(redMoved)));

        // Three beams per arm against one.
        drive(128, 128);
        measure(red, blue);
        const int threeBeams = red.n;
        setBeams(1);
        measure(red, blue);
        const int oneBeam = red.n;
        QVERIFY2(threeBeams > oneBeam * 2,
                 qPrintable(QString("three beams per head drew %1 px against one "
                                    "beam's %2").arg(threeBeams).arg(oneBeam)));
    }

    delete doc;
}

void Monitor_Test::aFixedConeHeadDimsWhenAimedAway()
{
    /* A head with a fixed cone pointed away from you should not blaze at you.
       Three things have to hold at once, and the last is what keeps the view
       usable:
         - aimed at the camera, full output;
         - aimed away, down to a floor (lit, but clearly not pointed here);
         - an UNDECLARED lens (0 degrees, which is what most definitions in the
           wild say) means no falloff at all. Dimming those by viewing angle
           would black out half a rig for no reason. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    auto makeHead = [&](double beamDeg, quint32 addr, const char *name) -> Fixture * {
        QLCFixtureDef *def = new QLCFixtureDef();
        def->setManufacturer("Test"); def->setModel(QString("Cone %1").arg(beamDeg));
        def->setType(QLCFixtureDef::MovingHead);
        const char *nm[] = { "Pan", "Tilt", "Dimmer" };
        const QLCChannel::Group gp[] = { QLCChannel::Pan, QLCChannel::Tilt,
                                         QLCChannel::Intensity };
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName("3ch");
        for (int k = 0; k < 3; ++k)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(nm[k]); ch->setGroup(gp[k]);
            if (k < 2) ch->setControlByte(QLCChannel::MSB);
            def->addChannel(ch); mode->insertChannel(ch, k);
        }
        QLCPhysical ph;
        ph.setWidth(300); ph.setHeight(400); ph.setDepth(300);
        // 180 puts full tilt exactly at the horizontal, so "tilt right out"
        // aims straight downstage -- at a Front camera, and nowhere else.
        ph.setFocusPanMax(360); ph.setFocusTiltMax(180);
        ph.setLensDegreesMin(beamDeg); ph.setLensDegreesMax(beamDeg);
        mode->setPhysical(ph);
        QLCFixtureHead hd;
        for (int k = 0; k < 3; ++k) hd.addChannel(quint32(k));
        mode->insertHead(-1, hd);
        def->addMode(mode);

        Fixture *f = new Fixture(doc);
        f->setName(name); f->setFixtureDefinition(def, mode);
        f->setUniverse(3); f->setAddress(addr);
        if (doc->addFixture(f) == false)
            return nullptr;
        props->setFixturePosition(f->id(), 2000, 2000, QVector3D(0, 0, 3.0f));
        return f;
    };

    Fixture *narrow = makeHead(12.0, 0, "Narrow");
    Fixture *undeclared = makeHead(0.0, 10, "Undeclared");
    QVERIFY(narrow != nullptr);
    QVERIFY(undeclared != nullptr);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(600, 400);
    v.reload();
    /* Angled at azimuth 0, elevation 0 -- which project() derives to be exactly
       a Front view, so the camera sits downstage looking up it and a head
       tilted right out at pan centre points straight at us. */
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(0.0, 0.0);
    v.setLiveValues(true);

    /* Tilt to one end throws along the bearing pan holds, and pan centred with
       panZeroDir 0 faces downstage (+Y) -- straight at a Front camera. */
    auto drive = [&](Fixture *f, int pan, int tilt) {
        QByteArray u(512, char(0));
        u[int(f->address()) + 0] = char(pan);
        u[int(f->address()) + 1] = char(tilt);
        u[int(f->address()) + 2] = char(255);
        f->setChannelValues(u);
    };

    FixtureRigProps rp;
    const FixtureVisualTraits tn = classifyFixture(narrow);
    const FixtureVisualTraits tu = classifyFixture(undeclared);
    QCOMPARE(int(tn.beamMaxDeg), 12);
    QCOMPARE(int(tu.beamMaxDeg), 0);

    // At the camera.
    drive(narrow, 128, 255);
    drive(undeclared, 128, 255);
    const double atCamera = v.beamVisibility(narrow, rp, tn, double(tn.beamMaxDeg));
    QVERIFY2(atCamera > 0.99,
             qPrintable(QString("a head aimed at the camera was dimmed to %1")
                        .arg(atCamera)));

    // Turned right around: pan half a revolution from downstage.
    rp.panZeroDir = 180.0f;
    const double away = v.beamVisibility(narrow, rp, tn, double(tn.beamMaxDeg));
    QVERIFY2(away < 0.25,
             qPrintable(QString("a 12-degree head aimed straight away still read "
                                "at %1 of full").arg(away)));

    // An undeclared lens is never dimmed, whichever way it is turned.
    QCOMPARE(v.beamVisibility(undeclared, rp, tu, double(tu.beamMaxDeg)), 1.0);
    FixtureRigProps rp2;
    QCOMPARE(v.beamVisibility(undeclared, rp2, tu, double(tu.beamMaxDeg)), 1.0);

    delete doc;
}

void Monitor_Test::aFrameIsDrawnFromOneInstant()
{
    /* A paint walks the whole rig, and the engine keeps writing values from
       the MasterTimer thread while it does. Asking each fixture for its values
       as the paint reaches it means fixtures drawn early show an older moment
       than fixtures drawn late, with the boundary moving every frame -- the rig
       flickers in and out ACROSS the stage instead of showing one picture that
       changes. The frame has to be drawn from a single instant. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Instant Par");
    def->setType(QLCFixtureDef::ColorChanger);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("3ch");
    const QLCChannel::PrimaryColour pc[] = { QLCChannel::Red, QLCChannel::Green,
                                             QLCChannel::Blue };
    for (int k = 0; k < 3; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("c%1").arg(k));
        ch->setGroup(QLCChannel::Intensity); ch->setColour(pc[k]);
        def->addChannel(ch); mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setWidth(300); ph.setHeight(300); ph.setDepth(300);
    mode->setPhysical(ph);
    QLCFixtureHead hd;
    for (int k = 0; k < 3; ++k) hd.addChannel(quint32(k));
    mode->insertHead(-1, hd);
    def->addMode(mode);

    Fixture *fxi = new Fixture(doc);
    fxi->setName("Instant"); fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(3); fxi->setAddress(0);
    QVERIFY(doc->addFixture(fxi));
    props->setFixturePosition(fxi->id(), 1000, 1000, QVector3D(0, 0, 1.5f));

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(600, 400);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(20.0, 20.0);
    v.setLiveValues(true);

    QByteArray red(512, char(0));
    red[0] = char(255);
    fxi->setChannelValues(red);
    v.grab();                                  // paints, and snapshots

    QVERIFY2(v.m_liveSnapshot.contains(fxi->id()),
             "the frame took no snapshot at all");
    QCOMPARE(uchar(v.liveValuesFor(fxi).at(0)), uchar(255));

    /* The engine moves on mid-frame. What the CURRENT frame draws from must
       not move with it. */
    QByteArray blue(512, char(0));
    blue[2] = char(255);
    fxi->setChannelValues(blue);

    QCOMPARE(uchar(fxi->channelValues().at(0)), uchar(0));      // the fixture did change
    QVERIFY2(uchar(v.liveValuesFor(fxi).at(0)) == 255,
             "the render path read through to live values instead of the frame's "
             "snapshot -- fixtures drawn at different points in a frame will "
             "show different moments");

    v.grab();                                  // next frame picks the change up
    QCOMPARE(uchar(v.liveValuesFor(fxi).at(0)), uchar(0));
    QCOMPARE(uchar(v.liveValuesFor(fxi).at(2)), uchar(255));

    delete doc;
}

/* A pixel bar on the front face of a step, built to order. */
static Fixture *makeStepTape(Doc *doc, StagePlatform *pl, int pix, quint32 addr,
                             const char *name, float riserV, float riserU = 1.219f)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel(QString("Tape %1").arg(name));
    def->setType(QLCFixtureDef::LEDBarPixels);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("tape");
    for (int h = 0; h < pix; ++h)
    {
        const QLCChannel::PrimaryColour pc[] = { QLCChannel::Red, QLCChannel::Green,
                                                 QLCChannel::Blue };
        for (int k = 0; k < 3; ++k)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("%1-%2").arg(h).arg(k));
            ch->setGroup(QLCChannel::Intensity); ch->setColour(pc[k]);
            def->addChannel(ch); mode->insertChannel(ch, h * 3 + k);
        }
        QLCFixtureHead hd;
        hd.addChannel(quint32(h * 3)); hd.addChannel(quint32(h * 3 + 1));
        hd.addChannel(quint32(h * 3 + 2));
        mode->insertHead(-1, hd);
    }
    QLCPhysical ph;
    ph.setWidth(2134); ph.setHeight(60); ph.setDepth(20);
    ph.setLayoutSize(QSize(pix, 1));
    mode->setPhysical(ph);
    def->addMode(mode);

    Fixture *f = new Fixture(doc);
    f->setName(name); f->setFixtureDefinition(def, mode);
    f->setUniverse(3); f->setAddress(addr);
    if (doc->addFixture(f) == false)
        return nullptr;
    doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp;
    rp.riserPlatformId = pl->id(); rp.riserFace = 0;
    rp.riserU = riserU; rp.riserV = riserV;
    doc->monitorProperties()->setFixtureRigProps(f->id(), rp);
    return f;
}

/* A bar with a SQUARE cross-section, so a roll about its length is visible in
   the Side view as a square turning into a diamond. */
static Fixture *makeChunkyBar(Doc *doc, quint32 addr, const char *name)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel(QString("Chunky %1").arg(name));
    def->setType(QLCFixtureDef::LEDBarPixels);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("bar");
    const QLCChannel::PrimaryColour pc[] = { QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue };
    QLCFixtureHead hd;
    for (int k = 0; k < 3; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("c%1").arg(k));
        ch->setGroup(QLCChannel::Intensity); ch->setColour(pc[k]);
        def->addChannel(ch); mode->insertChannel(ch, k);
        hd.addChannel(quint32(k));
    }
    mode->insertHead(-1, hd);
    QLCPhysical ph;
    ph.setWidth(1000); ph.setHeight(300); ph.setDepth(300);   // square section
    mode->setPhysical(ph);
    def->addMode(mode);

    Fixture *f = new Fixture(doc);
    f->setName(name); f->setFixtureDefinition(def, mode);
    f->setUniverse(3); f->setAddress(addr);
    if (doc->addFixture(f) == false)
        return nullptr;
    doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(2000, 2000, 1.0f));
    return f;
}

/* "I don't see the tilt here .. or here .. how would I know?" The flat views
   drew a fixture's housing with drawRect(boxPx) -- its AXIS-ALIGNED bounding
   box -- so a rolled bar was drawn as a slightly bigger upright rectangle and
   the roll was invisible in Top, Front and Side alike. */
void Monitor_Test::aTiltedBarIsDrawnTiltedInTheSideView()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Fixture *bar = makeChunkyBar(doc, 0, "Rolled");
    QVERIFY(bar != nullptr);

    /* Fraction of its own bounding box the drawn body fills: ~1.0 for a square
       seen square-on, 1/(cos t + sin t)^2 for one rolled by t (0.54 at 30). */
    auto fillRatio = [&](float tiltDeg) -> double {
        FixtureRigProps rp;
        rp.studioMount = 0;       // lying on a top face
        rp.studioAngle = 0.0f;    // running along X -- end-on in the Side view
        rp.studioTilt  = tiltDeg;
        props->setFixtureRigProps(bar->id(), rp);

        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(700, 560);
        v.setPlane(StructureStudioView::Side);
        v.grab();
        v.reload();
        v.m_zoomed = true;
        const QImage with = v.grab().toImage();

        const PreviewItem saved = props->m_fixtureItems.value(bar->id()).m_baseItem;
        props->m_fixtureItems.remove(bar->id());
        const QImage without = v.grab().toImage();
        props->m_fixtureItems[bar->id()].m_baseItem = saved;

        int n = 0, x0 = with.width(), y0 = with.height(), x1 = -1, y1 = -1;
        for (int y = 0; y < with.height(); ++y)
            for (int x = 0; x < with.width(); ++x)
            {
                const QColor a = with.pixelColor(x, y), b = without.pixelColor(x, y);
                if (qAbs(a.red() - b.red()) + qAbs(a.green() - b.green())
                    + qAbs(a.blue() - b.blue()) <= 24)
                    continue;
                ++n;
                x0 = qMin(x0, x); y0 = qMin(y0, y); x1 = qMax(x1, x); y1 = qMax(y1, y);
            }
        if (n < 30 || x1 <= x0 || y1 <= y0)
            return -1.0;
        return double(n) / double((x1 - x0 + 1) * (y1 - y0 + 1));
    };

    const double flat   = fillRatio(0.0f);
    const double rolled = fillRatio(30.0f);

    QVERIFY2(flat > 0.85,
             qPrintable(QString("an unrolled square section should fill its box, "
                                "got %1 -- the measurement is not sound").arg(flat)));
    QVERIFY2(rolled > 0.0 && rolled < 0.75,
             qPrintable(QString("a bar rolled 30 degrees still fills %1 of its bounding "
                                "box -- it is being drawn as an upright rectangle")
                        .arg(rolled)));

    doc->deleteFixture(bar->id());
    delete doc;
}

void Monitor_Test::aStripStaysLitFromEitherSideOfTheHouse()
{
    /* Two faults made the step fronts go dark from some angles and not others.
       The face the pixels are painted on was chosen by comparing the
       (+length,+normal) corner against (-length,-normal), which mixes the
       length axis into a question about the normal, so the answer flipped with
       the SIGN of the azimuth -- right from house right, wrong from house left.
       And the sub-pixel cull DROPPED the pixels entirely once a 64-LED strip
       fell under 2.5 screen pixels per LED, which at any whole-rig zoom is the
       normal case, not an edge case. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *pl = props->addPlatform();
    pl->setName("Step"); pl->setOriginX(0.0f); pl->setOriginY(0.0f);
    pl->setWidth(2.438f); pl->setDepth(0.204f); pl->setHeight(0.4f);
    pl->setColor(QColor(40, 40, 44));

    Fixture *tape = makeStepTape(doc, pl, 64, 0, "Tape", 0.20f);
    QVERIFY(tape != nullptr);
    QByteArray u(512, char(0));
    for (int h = 0; h < 64; ++h) u[h * 3] = char(255);      // all red
    tape->setChannelValues(u);

    auto redAcross = [&](double azimuth) {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(1000, 620);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(azimuth, 18.0);
        v.setLiveValues(true);
        v.setAmbient(0.40);
        const QImage img = v.grab().toImage();
        int red = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
            {
                const QColor c = img.pixelColor(x, y);
                if (c.red() > 110 && c.red() > c.green() * 2 && c.red() > c.blue() * 2)
                    ++red;
            }
        return red;
    };

    /* House left and house right are the same room. Whatever the strip reads
       as from +35 it has to read as from -35. */
    const double pairs[][2] = { { -20.0, 20.0 }, { -35.0, 35.0 }, { -50.0, 50.0 } };
    for (int i = 0; i < 3; ++i)
    {
        const int a = redAcross(pairs[i][0]);
        const int b = redAcross(pairs[i][1]);
        QVERIFY2(a > 150 && b > 150,
                 qPrintable(QString("the strip went dark at +/-%1: %2 vs %3 red px")
                            .arg(pairs[i][1]).arg(a).arg(b)));
        QVERIFY2(qMin(a, b) * 100 / qMax(1, qMax(a, b)) > 55,
                 qPrintable(QString("the strip reads very differently from the two "
                                    "sides of the house at +/-%1: %2 vs %3")
                            .arg(pairs[i][1]).arg(a).arg(b)));
    }

    delete doc;
}

void Monitor_Test::aStepsOwnTapeSitsInFrontOfWhatIsInsideIt()
{
    /* Lifting in-step fixtures clear of the step's faces (so they stop being
       swallowed by them) must not lift them past the LED tape on the OUTSIDE
       of that same step. From out here the tape is the nearer thing. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *pl = props->addPlatform();
    pl->setName("Step"); pl->setOriginX(0.0f); pl->setOriginY(0.0f);
    pl->setWidth(6.0f); pl->setDepth(0.6f); pl->setHeight(0.5f);
    pl->setColor(QColor(40, 40, 44));
    pl->setTopMaterial(StagePlatform::ClearTop);

    /* Near one END of a long step. Dead centre, a surface-mounted fixture is
       proud enough of the face to win on its own depth whatever the layering
       says; out at an end, viewed off-axis, its depth falls BELOW the face's
       average and the layering is the only thing deciding. */
    Fixture *tape = makeStepTape(doc, pl, 64, 0, "Tape", 0.25f, 1.2f);
    QVERIFY(tape != nullptr);
    QByteArray red(512, char(0));
    for (int h = 0; h < 64; ++h) red[h * 3] = char(255);
    tape->setChannelValues(red);

    /* A big green unit rigged INSIDE, right behind the tape. */
    Fixture *inside = makeStepTape(doc, pl, 8, 300, "Inside", 0.25f, 1.2f);
    QVERIFY(inside != nullptr);
    FixtureRigProps irp = props->fixtureRigProps(inside->id());
    irp.placement = FixtureRigProps::Inside;
    props->setFixtureRigProps(inside->id(), irp);
    QByteArray green(512, char(0));
    for (int h = 0; h < 8; ++h) green[300 + h * 3 + 1] = char(255);
    inside->setChannelValues(green);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(1000, 620);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    /* Azimuth +35 makes low X the FAR end, so this pair sits behind the
       step's near-face midpoint and both get lifted -- which is precisely when
       the layering, rather than their own geometry, decides the order. */
    v.setAngledView(35.0, 18.0);
    v.setLiveValues(true);
    v.setAmbient(0.40);
    const QImage img = v.grab().toImage();

    // w2s() is logical pixels; grab() on a HiDPI screen returns device
    // pixels (devicePixelRatio() times as many) -- scale to match.
    const double dpr = img.devicePixelRatio();
    const QPointF at = v.w2s(props->fixtureRigPosition(tape->id())) * dpr;
    int red2 = 0, green2 = 0;
    for (int dy = -6; dy <= 6; ++dy)
    {
        for (int dx = -120; dx <= 120; ++dx)
        {
            const QPoint p(at.toPoint() + QPoint(int(dx * dpr), int(dy * dpr)));
            if (img.rect().contains(p) == false) continue;
            const QColor c = img.pixelColor(p);
            if (c.red() > 110 && c.red() > c.green() * 2) ++red2;
            if (c.green() > 110 && c.green() > c.red() * 2) ++green2;
        }
    }
    /* A 60 mm tape is only a few screen pixels tall at a whole-step zoom, so
       the absolute count is small by construction -- what matters is that it is
       THERE and that the in-step unit is not on top of it. */
    QVERIFY2(red2 > 25,
             qPrintable(QString("the step's own tape is not being drawn (%1 red px)")
                        .arg(red2)));
    QVERIFY2(red2 > green2 * 2,
             qPrintable(QString("what is rigged INSIDE the step painted over the "
                                "tape on its outside: %1 red vs %2 green")
                        .arg(red2).arg(green2)));

    delete doc;
}

void Monitor_Test::fixturesInsideAStepAreAllVisible()
{
    /* Same painter's-algorithm trap as the pixels, one level up: a step is a
       solid box whose faces each carry ONE depth, so a fixture living INSIDE
       it is covered by the near face unless it happens to sit in front of that
       face's midpoint. Off-axis that hides all but the nearest -- which is
       useless, since seeing what is rigged inside a clear-topped step is the
       entire reason for putting it there. */
    /* Its own Doc. This one measures OCCLUSION across the whole stage, so
       any scenery another test left behind changes both the auto-fit scale and
       what is able to cover what. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *pl = props->addPlatform();
    pl->setName("Clear Step"); pl->setOriginX(0.0f); pl->setOriginY(0.0f);
    pl->setWidth(4.0f); pl->setDepth(1.0f); pl->setHeight(0.6f);
    pl->setColor(QColor(40, 40, 44));
    pl->setTopMaterial(StagePlatform::ClearTop);

    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Inside Par");
    def->setType(QLCFixtureDef::ColorChanger);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("3ch");
    const QLCChannel::PrimaryColour pc[] = { QLCChannel::Red, QLCChannel::Green,
                                             QLCChannel::Blue };
    for (int k = 0; k < 3; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("c%1").arg(k));
        ch->setGroup(QLCChannel::Intensity); ch->setColour(pc[k]);
        def->addChannel(ch); mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setWidth(200); ph.setHeight(200); ph.setDepth(200);
    mode->setPhysical(ph);
    QLCFixtureHead hd;
    for (int k = 0; k < 3; ++k) hd.addChannel(quint32(k));
    mode->insertHead(-1, hd);
    def->addMode(mode);

    /* Four of them spread right across the step, every one driven full red. */
    QList<quint32> fids;
    for (int i = 0; i < 4; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("Inside %1").arg(i + 1));
        f->setFixtureDefinition(def, mode);
        f->setUniverse(3); f->setAddress(quint32(i * 4));
        QVERIFY(doc->addFixture(f));
        props->setFixturePosition(f->id(), 0, 0, QVector3D(0, 0, 0));
        FixtureRigProps rp;
        rp.riserPlatformId = pl->id();
        rp.riserFace = 0;
        rp.riserU = 0.6f + i * 0.95f;
        rp.riserV = 0.30f;
        rp.placement = FixtureRigProps::Inside;
        props->setFixtureRigProps(f->id(), rp);
        QByteArray u(512, char(0));
        u[int(f->address())] = char(255);
        f->setChannelValues(u);
        fids << f->id();
    }

    auto litCount = [&](double azimuth) {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(1100, 700);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(azimuth, 18.0);
        v.setLiveValues(true);
        v.setAmbient(0.40);
        const QImage img = v.grab().toImage();
        // w2s() is logical pixels; grab() on a HiDPI screen returns device
        // pixels (devicePixelRatio() times as many) -- scale to match.
        const double dpr = img.devicePixelRatio();
        int seen = 0;
        foreach (quint32 fid, fids)
        {
            const QPointF at = v.w2s(props->fixtureRigPosition(fid)) * dpr;
            bool found = false;
            for (int dy = -9; dy <= 9 && !found; ++dy)
            {
                for (int dx = -9; dx <= 9 && !found; ++dx)
                {
                    const QPoint p(at.toPoint() + QPoint(int(dx * dpr), int(dy * dpr)));
                    if (img.rect().contains(p) == false) continue;
                    const QColor c = img.pixelColor(p);
                    if (c.red() > 120 && c.red() > c.green() * 2 && c.red() > c.blue() * 2)
                        found = true;
                }
            }
            if (found) ++seen;
        }
        return seen;
    };

    const double angles[] = { 0.0, 25.0, 40.0 };
    for (int a = 0; a < 3; ++a)
    {
        const int seen = litCount(angles[a]);
        QVERIFY2(seen == 4,
                 qPrintable(QString("at azimuth %1 only %2 of 4 fixtures inside "
                                    "the step were visible")
                            .arg(angles[a]).arg(seen)));
    }

    delete doc;
}

void Monitor_Test::pixelsSurviveOffAxisOnTheirOwnHousing()
{
    /* Half of every step went blank off a square-on view, and the halfway line
       slid across as the camera came round.
     *
       drawSolidBox() gives a face ONE depth -- the average of its four corners
       -- while each pixel carried its own. Off-axis a 2 m strip's face spans a
       real depth range, so every pixel beyond the face's midpoint sorted behind
       the housing it is painted on and was covered by it. A pixel and the metal
       it sits in are one surface and have to sort as one. */
    MonitorProperties *props = m_doc->monitorProperties();
    StagePlatform *pl = props->addPlatform();
    pl->setName("Off-axis Step"); pl->setOriginX(0.0f); pl->setOriginY(0.0f);
    pl->setWidth(2.438f); pl->setDepth(0.204f); pl->setHeight(0.204f);
    pl->setColor(QColor(40, 40, 44));       // neutral: never counted as lit

    const int PIX = 64;
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Off-axis Tape");
    def->setType(QLCFixtureDef::LEDBarPixels);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("192ch");
    for (int h = 0; h < PIX; ++h)
    {
        const QLCChannel::PrimaryColour pc[] = { QLCChannel::Red, QLCChannel::Green,
                                                 QLCChannel::Blue };
        for (int k = 0; k < 3; ++k)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("%1-%2").arg(h).arg(k));
            ch->setGroup(QLCChannel::Intensity); ch->setColour(pc[k]);
            def->addChannel(ch);
            mode->insertChannel(ch, h * 3 + k);
        }
        QLCFixtureHead hd;
        hd.addChannel(quint32(h * 3)); hd.addChannel(quint32(h * 3 + 1));
        hd.addChannel(quint32(h * 3 + 2));
        mode->insertHead(-1, hd);
    }
    QLCPhysical ph;
    ph.setWidth(2134); ph.setHeight(60); ph.setDepth(20);
    ph.setLayoutSize(QSize(PIX, 1));
    mode->setPhysical(ph);
    def->addMode(mode);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Off-axis Tape"); fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(3); fxi->setAddress(0);
    QVERIFY(m_doc->addFixture(fxi));
    props->setFixturePosition(fxi->id(), 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp;
    rp.riserPlatformId = pl->id(); rp.riserFace = 0;
    rp.riserU = 1.219f; rp.riserV = 0.1f;
    props->setFixtureRigProps(fxi->id(), rp);

    QByteArray u(512, char(0));
    for (int h = 0; h < PIX; ++h)
        u[h * 3] = char(255);               // EVERY pixel full red
    fxi->setChannelValues(u);

    /* Count lit pixels either side of the strip's own centre. Every pixel is
       driven identically, so the two halves must come out even; a housing
       eating one of them is exactly the asymmetry to catch. */
    auto halves = [&](double azimuth, int &left, int &right) {
        StructureStudioView v(m_doc, StructureStudioView::StageKind, 0);
        v.resize(1000, 640);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(azimuth, 12.0);
        v.setLiveValues(true);
        v.setAmbient(0.40);
        /* This is a housing/depth-sort test, not a beam test -- but washes
           throw light now, and a riser-mounted strip's "out from the riser"
           direction here runs nearly straight down the camera's own sightline.
           Edge-on, the wash's far-end disc balloons to cover most of the frame
           and swamps the per-pixel count this test is actually measuring. */
        v.setBeams(false);
        const QImage img = v.grab().toImage();
        // w2s() is logical pixels; grab() on a HiDPI screen returns device
        // pixels (devicePixelRatio() times as many) -- scale to match, or
        // the dividing line lands nowhere near the strip's actual centre.
        const QPointF mid = v.w2s(props->fixtureRigPosition(fxi->id())) * img.devicePixelRatio();
        left = right = 0;
        for (int y = 0; y < img.height(); ++y)
        {
            for (int x = 0; x < img.width(); ++x)
            {
                const QColor c = img.pixelColor(x, y);
                if (c.red() < 110 || c.red() < c.green() * 2 || c.red() < c.blue() * 2)
                    continue;
                if (x < mid.x()) ++left; else ++right;
            }
        }
    };

    const double angles[] = { 0.0, 20.0, 35.0 };
    for (int a = 0; a < 3; ++a)
    {
        int left = 0, right = 0;
        halves(angles[a], left, right);
        QVERIFY2(left + right > 200,
                 qPrintable(QString("nothing lit at azimuth %1 (%2 + %3)")
                            .arg(angles[a]).arg(left).arg(right)));
        const int lo = qMin(left, right), hi2 = qMax(left, right);
        QVERIFY2(lo * 100 / qMax(1, hi2) > 55,
                 qPrintable(QString("at azimuth %1 one half of the strip lost its "
                                    "pixels: %2 left vs %3 right")
                            .arg(angles[a]).arg(left).arg(right)));
    }

    m_doc->deleteFixture(fxi->id());
    props->removePlatform(pl->id());
}

void Monitor_Test::aMostlyDarkPixelBarDrawsNoBrightOutline()
{
    /* A 25 mm LED tape is about two pixels tall on screen, so its OUTLINE is
       most of the fixture. Drawing that outline in the fixture-wide colour put
       a solid pale line down the full length of a strip whose pixels were
       nearly all dark -- the "white" in the rig view. The housing is a box,
       not a lamp: the heads carry the colour, exactly as the studio does it. */
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("Tape Bar"); t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(0.0f, 0.0f, 3.0f)); t->setDirection(QPointF(1.0, 0.0));
    t->setLength(4.0f); t->setWidth(0.3f);

    const int PIX = 64;
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Tape 64");
    def->setType(QLCFixtureDef::LEDBarPixels);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("192ch");
    for (int h = 0; h < PIX; ++h)
    {
        const QLCChannel::PrimaryColour pc[] = { QLCChannel::Red, QLCChannel::Green,
                                                 QLCChannel::Blue };
        for (int k = 0; k < 3; ++k)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("%1-%2").arg(h + 1).arg(k));
            ch->setGroup(QLCChannel::Intensity); ch->setColour(pc[k]);
            def->addChannel(ch);
            mode->insertChannel(ch, h * 3 + k);
        }
        QLCFixtureHead head;
        head.addChannel(quint32(h * 3));
        head.addChannel(quint32(h * 3 + 1));
        head.addChannel(quint32(h * 3 + 2));
        mode->insertHead(-1, head);
    }
    QLCPhysical ph;
    ph.setWidth(2134); ph.setHeight(25); ph.setDepth(2);
    ph.setLayoutSize(QSize(PIX, 1));
    mode->setPhysical(ph);
    def->addMode(mode);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Tape"); fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(3); fxi->setAddress(0);
    QVERIFY(m_doc->addFixture(fxi));
    props->setFixturePosition(fxi->id(), 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp;
    rp.trussId = t->id(); rp.trussOffset = 2.0f;
    props->setFixtureRigProps(fxi->id(), rp);

    /* Only the first EIGHT pixels lit, and in MIXED colours -- which is what
       collapses the fixture-wide colour to a pale wash, and so what makes an
       outline drawn in that colour glaringly bright. The other fifty-six
       pixels are dark. */
    QByteArray u(512, char(0));
    for (int h = 0; h < 8; ++h)
        u[h * 3 + (h % 3)] = char(255);
    fxi->setChannelValues(u);

    {
        QColor whole(90, 160, 235);
        uchar wholeDim = 0;
        QVERIFY(fixtureLiveState(fxi, whole, wholeDim));
        QVERIFY2(whole.red() > 200 && whole.green() > 200 && whole.blue() > 200,
                 "the fixture-wide colour is expected to wash out to near-white "
                 "here -- that is what makes this test discriminating");
    }

    StructureStudioView v(m_doc, StructureStudioView::TrussKind, t->id());
    v.resize(900, 600);
    v.reload();
    v.setPlane(StructureStudioView::Front);
    v.setLiveValues(true);
    v.setAmbient(0.40);

    const QPointF at = v.w2s(props->fixtureRigPosition(fxi->id()));
    const QImage img = v.grab().toImage();

    /* Sweep the strip's own row band and count how much of it reads bright.
       Eight lit pixels out of sixty-four cannot legitimately light up most of
       the width; an outline drawn in the fixture colour does exactly that. */
    int bright = 0, sampled = 0;
    for (int dy = -4; dy <= 4; ++dy)
    {
        for (int dx = -180; dx <= 180; ++dx)
        {
            const QPoint p(at.toPoint() + QPoint(dx, dy));
            if (img.rect().contains(p) == false) continue;
            ++sampled;
            const QColor c = img.pixelColor(p);
            if (c.red() * 0.30 + c.green() * 0.59 + c.blue() * 0.11 > 90.0)
                ++bright;
        }
    }
    QVERIFY(sampled > 500);
    QVERIFY2(bright * 100 / sampled < 12,
             qPrintable(QString("a bar with 8 of 64 pixels lit painted %1%% of "
                                "its band bright -- the housing is being drawn "
                                "as though it were the lamp")
                        .arg(bright * 100 / sampled)));

    m_doc->deleteFixture(fxi->id());
    props->removeTruss(t->id());
}

void Monitor_Test::aLongTrussSortsAlongItsLength()
{
    /* A truss chord spans the whole rig. Described with ONE averaged depth it
       can only be wholly in front of a deck or wholly behind it, never
       crossing -- the same mistake a face with one depth was, and the last
       place it lived. It survived the depth buffer because emitLine() was still
       averaging its two ends before handing them over.
     *
       Asserted on the EMITTER, deliberately. Two pixel-level attempts at this
       both passed with the averaging put back: contriving a scene where an
       average lands on the wrong side of something is fiddly, and a test that
       cannot fail is worse than no test. What has to be true is simple -- a
       line whose ends are at different depths carries both of them into the
       rasteriser, which resolves per pixel and has its own test for that. */
    Doc *doc = new Doc(this);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(800, 600);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(20.0, 18.0);

    // Ten metres of chord, running upstage: a real depth range end to end.
    const QVector3D a(0.0f, -5.0f, 3.0f);
    const QVector3D b(0.0f,  5.0f, 3.0f);
    const double za = v.viewDepth(a), zb = v.viewDepth(b);
    QVERIFY2(qAbs(za - zb) > 1.0,
             qPrintable(QString("the test's own chord does not span depth "
                                "(%1 vs %2)").arg(za).arg(zb)));

    QImage scratch(v.size(), QImage::Format_ARGB32_Premultiplied);
    scratch.fill(Qt::transparent);
    QPainter p(&scratch);

    v.m_ops.clear();
    v.m_collecting = true;
    v.emitLine(v.w2s(a), v.w2s(b), za, zb, QColor(255, 255, 255), 1.2, p);
    v.m_collecting = false;

    QCOMPARE(v.m_ops.size(), 1);
    const StructureStudioView::DrawOp &o = v.m_ops.first();
    QCOMPARE(int(o.kind), int(StructureStudioView::DrawOp::Line));
    QVERIFY2(o.zs.size() == 2,
             "the line reached the queue with no per-end depth at all -- it can "
             "only be wholly in front of something or wholly behind it");
    QVERIFY2(qAbs(o.zs.at(0) - o.zs.at(1)) > 1.0,
             qPrintable(QString("both ends arrived at the same depth (%1, %2) -- "
                                "they are being averaged")
                        .arg(o.zs.at(0)).arg(o.zs.at(1))));

    delete doc;
}

void Monitor_Test::aSolidBoxHidesItsOwnFarEdges()
{
    /* "If that's a solid why do I see the inside lines?" -- the platform and
       truss EDITORS painted their 45-degree view with the old unsorted path
       while only the whole-rig view used the depth buffer, so a solid deck drew
       its own far edges straight through itself. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *pl = props->addPlatform();
    pl->setName("Solid"); pl->setOriginX(0.0f); pl->setOriginY(0.0f);
    pl->setWidth(3.0f); pl->setDepth(1.6f); pl->setHeight(1.0f);
    pl->setColor(QColor(190, 40, 40));
    pl->setTopMaterial(StagePlatform::SolidTop);

    StructureStudioView v(doc, StructureStudioView::PlatformKind, pl->id());
    v.resize(700, 500);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(28.0, 22.0);
    const QImage img = v.grab().toImage();

    /* Walk a horizontal line across the box and count how many times the colour
       changes materially. A solid box crossed at one height has a handful of
       genuine boundaries -- its own silhouette and the seam between two faces.
       Its hidden far edges showing through add several more. */
    int bestRow = -1, bestSpan = 0;
    for (int y = 0; y < img.height(); ++y)
    {
        int span = 0;
        for (int x = 0; x < img.width(); ++x)
        {
            const QColor c = img.pixelColor(x, y);
            if (c.red() > 60 && c.red() > c.blue() + 25) ++span;
        }
        if (span > bestSpan) { bestSpan = span; bestRow = y; }
    }
    QVERIFY2(bestSpan > 100,
             qPrintable(QString("the box barely drew (%1 px on its widest row)")
                        .arg(bestSpan)));

    int transitions = 0;
    QColor prev = img.pixelColor(0, bestRow);
    for (int x = 1; x < img.width(); ++x)
    {
        const QColor c = img.pixelColor(x, bestRow);
        const int d = qAbs(c.red() - prev.red()) + qAbs(c.green() - prev.green())
                      + qAbs(c.blue() - prev.blue());
        if (d > 45)
            ++transitions;
        prev = c;
    }
    QVERIFY2(transitions <= 8,
             qPrintable(QString("a solid box shows %1 colour transitions across "
                                "its widest row -- its own hidden edges are "
                                "drawing through it").arg(transitions)));

    delete doc;
}

void Monitor_Test::bothRenderersAgreeOnPlainOcclusion()
{
    /* The old sorted-primitive painter is kept behind m_useZBuffer as a
       fallback for the changeover. This is what stops it being dead code: on a
       scene simple enough that one depth per primitive is ENOUGH -- two
       separated boxes, no coplanar surfaces, nothing spanning depth -- the two
       renderers must agree. Where they disagree is exactly the set of cases the
       depth buffer exists for, and those are covered by their own tests. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    StagePlatform *back = props->addPlatform();
    back->setName("Back"); back->setOriginX(0.0f); back->setOriginY(4.0f);
    back->setWidth(3.0f); back->setDepth(1.0f); back->setHeight(2.0f);
    back->setColor(QColor(200, 60, 60));

    StagePlatform *front = props->addPlatform();
    front->setName("Front"); front->setOriginX(0.5f); front->setOriginY(0.0f);
    front->setWidth(2.0f); front->setDepth(1.0f); front->setHeight(1.2f);
    front->setColor(QColor(60, 200, 60));

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(600, 420);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(20.0, 25.0);

    v.m_useZBuffer = false;
    const QImage oldImg = v.grab().toImage();
    v.m_useZBuffer = true;
    const QImage newImg = v.grab().toImage();

    /* Compare which SURFACE won per pixel, not exact colours: the two paths
       antialias differently and always will. */
    auto classify = [](const QColor &c) {
        if (c.red() > c.green() + 30) return 1;      // the back (red) deck
        if (c.green() > c.red() + 30) return 2;      // the front (green) deck
        return 0;
    };
    int same = 0, differ = 0, subject = 0;
    for (int y = 0; y < oldImg.height(); y += 2)
    {
        for (int x = 0; x < oldImg.width(); x += 2)
        {
            const int a = classify(oldImg.pixelColor(x, y));
            const int b = classify(newImg.pixelColor(x, y));
            if (a == 0 && b == 0)
                continue;
            ++subject;
            if (a == b) ++same; else ++differ;
        }
    }
    QVERIFY2(subject > 500,
             qPrintable(QString("the test scene barely drew anything (%1 px)")
                        .arg(subject)));
    QVERIFY2(same * 100 / qMax(1, subject) > 92,
             qPrintable(QString("the two renderers disagree on plain occlusion: "
                                "%1 of %2 sampled pixels differ")
                        .arg(differ).arg(subject)));

    delete doc;
}

void Monitor_Test::zRasterResolvesDepthPerPixel()
{
    /* The whole point of the rasteriser: depth is decided per PIXEL, so a
       polygon whose depth varies across it can be partly in front of and partly
       behind something else -- which one depth per primitive can never express,
       and which is where every artefact in this view came from. */
    ZRaster z;
    const QSize sz(200, 100);

    auto quad = [](double x0, double y0, double x1, double y1, QPointF *out) {
        out[0] = QPointF(x0, y0); out[1] = QPointF(x1, y0);
        out[2] = QPointF(x1, y1); out[3] = QPointF(x0, y1);
    };

    QPointF far_[4], near_[4];
    quad(0, 0, 200, 100, far_);
    quad(50, 25, 150, 75, near_);
    const double farZ[4]  = { 0.0, 0.0, 0.0, 0.0 };
    const double nearZ[4] = { 1.0, 1.0, 1.0, 1.0 };

    /* Order must not matter for opaque geometry. That is the property the old
       sorted queue had to work for, and this one gets for free. */
    for (int order = 0; order < 2; ++order)
    {
        z.begin(sz, 1, QColor(0, 0, 0));
        if (order == 0)
        {
            z.poly(far_, farZ, 4, QColor(255, 0, 0));
            z.poly(near_, nearZ, 4, QColor(0, 255, 0));
        }
        else
        {
            z.poly(near_, nearZ, 4, QColor(0, 255, 0));
            z.poly(far_, farZ, 4, QColor(255, 0, 0));
        }
        const QImage img = z.resolve();
        QCOMPARE(img.pixelColor(100, 50), QColor(0, 255, 0));     // near wins
        QCOMPARE(img.pixelColor(10, 50), QColor(255, 0, 0));      // far shows around it
    }

    /* The real case. A wide face whose depth RAMPS across its width, and a
       narrow bar at constant depth lying over it. The bar must be visible along
       its whole length: in front where the face is deeper, and equally in front
       where it is shallower, because the comparison happens per pixel. Sorting
       by the face's average depth is what made half of it disappear. */
    z.begin(sz, 1, QColor(0, 0, 0));
    QPointF face[4];
    quad(0, 0, 200, 100, face);
    const double ramp[4] = { -1.0, 1.0, 1.0, -1.0 };   // far at x=0, near at x=200
    z.poly(face, ramp, 4, QColor(80, 80, 80));

    QPointF bar[4];
    quad(10, 40, 190, 60, bar);
    const double barZ[4] = { 1.5, 1.5, 1.5, 1.5 };     // in front of ALL of it
    z.poly(bar, barZ, 4, QColor(255, 0, 0));

    const QImage img = z.resolve();
    int red = 0;
    for (int x = 12; x < 188; ++x)
        if (img.pixelColor(x, 50) == QColor(255, 0, 0))
            ++red;
    QVERIFY2(red > 170,
             qPrintable(QString("a bar in front of a depth-ramped face survived "
                                "only %1 of 176 columns").arg(red)));

    // And depth is readable back, which is what hit-testing and volumetrics need.
    QVERIFY(z.depthAt(QPoint(100, 50)) > 1.4);
    QVERIFY(z.depthAt(QPoint(100, 5)) < 1.0);          // face only up there
}

void Monitor_Test::zRasterBlendsWithoutOccluding()
{
    /* A depth buffer is order-independent only for OPAQUE geometry. Translucent
       surfaces -- beams, clear tops -- must still be depth-TESTED against the
       solid world, but must not write depth, or they would hide each other. */
    ZRaster z;
    const QSize sz(100, 100);
    QPointF full[4] = { QPointF(0, 0), QPointF(100, 0), QPointF(100, 100), QPointF(0, 100) };
    const double back[4] = { 0.0, 0.0, 0.0, 0.0 };
    const double front[4] = { 1.0, 1.0, 1.0, 1.0 };

    z.begin(sz, 1, QColor(0, 0, 0));
    z.poly(full, back, 4, QColor(0, 0, 200));                       // solid ground
    z.poly(full, front, 4, QColor(255, 0, 0, 128), false);          // a beam over it

    const QColor c = z.resolve().pixelColor(50, 50);
    QVERIFY2(c.red() > 100 && c.blue() > 80,
             qPrintable(QString("a translucent surface did not blend with what is "
                                "behind it: %1").arg(c.name())));

    // No depth written, so a second translucent layer also blends.
    z.poly(full, front, 4, QColor(0, 255, 0, 128), false);
    const QColor c2 = z.resolve().pixelColor(50, 50);
    QVERIFY2(c2.green() > 100,
             qPrintable(QString("a translucent surface occluded another at the "
                                "same depth: %1").arg(c2.name())));

    // But it IS depth-tested: behind the solid ground, it must not appear.
    z.begin(sz, 1, QColor(0, 0, 0));
    z.poly(full, front, 4, QColor(0, 0, 200));                      // solid, near
    z.poly(full, back, 4, QColor(255, 0, 0, 200), false);           // beam, behind
    QCOMPARE(z.resolve().pixelColor(50, 50), QColor(0, 0, 200));
}

void Monitor_Test::aClosedShutterEmitsNothingAStrobeStillDoes()
{
    /* A shutter is a piece of metal in front of the lamp: closed, nothing gets
       out, whatever the dimmer says. This was ignored entirely -- the Shutter
       group was skipped along with everything that is not Intensity or Colour.
     *
       The opposite case matters just as much: a STROBE is emitting, just
       intermittently, and reading it as dark would blank every strobe look in
       the rig. Only an explicit ShutterClose closes it. */
    Doc *doc = new Doc(this);

    auto build = [&](QLCCapability::Preset capPreset, quint32 addr,
                     const char *name) -> Fixture * {
        QLCFixtureDef *def = new QLCFixtureDef();
        def->setManufacturer("Test"); def->setModel(name);
        def->setType(QLCFixtureDef::MovingHead);
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName("3ch");

        QLCChannel *dim = new QLCChannel();
        dim->setName("Dimmer"); dim->setGroup(QLCChannel::Intensity);
        def->addChannel(dim); mode->insertChannel(dim, 0);

        QLCChannel *red = new QLCChannel();
        red->setName("Red"); red->setGroup(QLCChannel::Intensity);
        red->setColour(QLCChannel::Red);
        def->addChannel(red); mode->insertChannel(red, 1);

        QLCChannel *sh = new QLCChannel();
        sh->setName("Shutter"); sh->setGroup(QLCChannel::Shutter);
        QLCCapability *cap = new QLCCapability(0, 255);
        cap->setName("state");
        cap->setPreset(capPreset);
        sh->addCapability(cap);
        def->addChannel(sh); mode->insertChannel(sh, 2);

        QLCPhysical ph; ph.setWidth(300); ph.setHeight(300); ph.setDepth(300);
        mode->setPhysical(ph);
        QLCFixtureHead hd; hd.addChannel(0); hd.addChannel(1); hd.addChannel(2);
        mode->insertHead(-1, hd);
        def->addMode(mode);

        Fixture *f = new Fixture(doc);
        f->setName(name); f->setFixtureDefinition(def, mode);
        f->setUniverse(3); f->setAddress(addr);
        return doc->addFixture(f) ? f : nullptr;
    };

    Fixture *shut = build(QLCCapability::ShutterClose, 0, "Shut");
    Fixture *strobing = build(QLCCapability::StrobeSlowToFast, 8, "Strobing");
    Fixture *open = build(QLCCapability::ShutterOpen, 16, "Open");
    QVERIFY(shut != nullptr);
    QVERIFY(strobing != nullptr);
    QVERIFY(open != nullptr);

    auto levelOf = [&](Fixture *f) {
        QByteArray u(512, char(0));
        u[int(f->address())] = char(255);          // dimmer full
        u[int(f->address()) + 1] = char(255);      // red full
        u[int(f->address()) + 2] = char(128);      // the shutter's one capability
        f->setChannelValues(u);
        QColor c(90, 160, 235);
        uchar d = 0;
        fixtureLiveState(f, f->channelValues(), c, d);
        return int(d);
    };

    QVERIFY2(levelOf(shut) == 0,
             qPrintable(QString("a closed shutter still emitted at level %1")
                        .arg(levelOf(shut))));
    QVERIFY2(levelOf(open) == 255,
             qPrintable(QString("an open shutter was dimmed to %1")
                        .arg(levelOf(open))));
    QVERIFY2(levelOf(strobing) == 255,
             qPrintable(QString("a STROBING fixture was read as dark (%1) -- it "
                                "is emitting, just intermittently")
                        .arg(levelOf(strobing))));

    /* And a shutter the caller never set is UNKNOWN, not closed.
     *
       A look resolved from palettes fills in the channels it sets and leaves
       the rest at zero; zero on a shutter channel is usually its "closed"
       capability. Believing that turned off every fixture whose look does not
       happen to mention a shutter -- a spot went from a beam to a bare aim
       line. Live DMX is the whole universe and does mean what it says; a
       resolved look is partial and does not. */
    QByteArray partial(512, char(0));
    partial[int(shut->address())] = char(255);        // dimmer
    partial[int(shut->address()) + 1] = char(255);    // red
    // shutter channel deliberately left at zero, i.e. never written
    QColor pc2(90, 160, 235);
    uchar pd = 0;
    QVERIFY(fixtureLiveState(shut, partial, pc2, pd, false));
    QVERIFY2(pd == 255,
             qPrintable(QString("an unwritten shutter channel was read as CLOSED "
                                "(level %1) -- a look that does not mention a "
                                "shutter must not switch the fixture off")
                        .arg(pd)));

    // The same bytes, declared complete, genuinely are a closed shutter.
    QColor pc3(90, 160, 235);
    uchar pd3 = 0;
    QVERIFY(fixtureLiveState(shut, partial, pc3, pd3, true));
    QCOMPARE(int(pd3), 0);

    delete doc;
}

void Monitor_Test::aLampWithNoColourEmitsNothingNotBlack()
{
    /* Strip the colours out of a look but leave its dimmer up, and the fixture
       reports colour #000000 at full level. Taken literally that is a
       full-strength BLACK beam, and the renderer painted one -- a dark cone
       laid over the stage, subtracting light that was never there.
     *
       A lamp whose emitters are all at zero is not emitting black. It is not
       emitting. The dimmer says how much of the mix leaves the lamp, and there
       is no mix. */
    Doc *doc = new Doc(this);

    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Dimmer+RGB");
    def->setType(QLCFixtureDef::ColorChanger);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("4ch");
    const char *nm[] = { "Dimmer", "Red", "Green", "Blue" };
    const QLCChannel::PrimaryColour pc[] = { QLCChannel::NoColour, QLCChannel::Red,
                                             QLCChannel::Green, QLCChannel::Blue };
    for (int k = 0; k < 4; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(nm[k]); ch->setGroup(QLCChannel::Intensity);
        if (pc[k] != QLCChannel::NoColour) ch->setColour(pc[k]);
        def->addChannel(ch); mode->insertChannel(ch, k);
    }
    QLCPhysical ph; ph.setWidth(300); ph.setHeight(300); ph.setDepth(300);
    mode->setPhysical(ph);
    QLCFixtureHead hd;
    for (int k = 0; k < 4; ++k) hd.addChannel(quint32(k));
    mode->insertHead(-1, hd);
    def->addMode(mode);

    Fixture *fxi = new Fixture(doc);
    fxi->setName("Stripped"); fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(3); fxi->setAddress(0);
    QVERIFY(doc->addFixture(fxi));

    /* Dimmer at FULL, every colour channel at zero -- a look with its colours
       removed while it is still running. */
    QByteArray u(512, char(0));
    u[0] = char(255);
    fxi->setChannelValues(u);

    QColor col(90, 160, 235);
    uchar dim = 200;
    QVERIFY(fixtureLiveState(fxi, fxi->channelValues(), col, dim));
    QVERIFY2(dim == 0,
             qPrintable(QString("a lamp with every emitter at zero reported level "
                                "%1 -- that is a full-strength black beam")
                        .arg(dim)));

    // Put one emitter up and it is a light again, at the dimmer's level.
    u[2] = char(255);                      // green
    fxi->setChannelValues(u);
    QVERIFY(fixtureLiveState(fxi, fxi->channelValues(), col, dim));
    QCOMPARE(int(dim), 255);
    QVERIFY(col.green() > 200 && col.red() < 60);

    // Dimmer down, colour still up: a real level, not zero.
    u[0] = char(96);
    fxi->setChannelValues(u);
    QVERIFY(fixtureLiveState(fxi, fxi->channelValues(), col, dim));
    QCOMPARE(int(dim), 96);

    /* A DIMMER-ONLY fixture must be unaffected: it has no colour to be black,
       and its level is the whole of what it says. */
    QLCFixtureDef *ddef = new QLCFixtureDef();
    ddef->setManufacturer("Test"); ddef->setModel("Plain Dimmer");
    ddef->setType(QLCFixtureDef::Dimmer);
    QLCFixtureMode *dmode = new QLCFixtureMode(ddef);
    dmode->setName("1ch");
    QLCChannel *dch = new QLCChannel();
    dch->setName("Dimmer"); dch->setGroup(QLCChannel::Intensity);
    ddef->addChannel(dch); dmode->insertChannel(dch, 0);
    QLCPhysical dph; dph.setWidth(200); dph.setHeight(200); dph.setDepth(200);
    dmode->setPhysical(dph);
    QLCFixtureHead dhd; dhd.addChannel(0);
    dmode->insertHead(-1, dhd);
    ddef->addMode(dmode);

    Fixture *plain = new Fixture(doc);
    plain->setName("Plain"); plain->setFixtureDefinition(ddef, dmode);
    plain->setUniverse(3); plain->setAddress(16);
    QVERIFY(doc->addFixture(plain));

    QByteArray pu(512, char(0));
    pu[16] = char(180);
    plain->setChannelValues(pu);
    QColor pcol(90, 160, 235);
    uchar pdim = 0;
    QVERIFY(fixtureLiveState(plain, plain->channelValues(), pcol, pdim));
    QVERIFY2(pdim == 180,
             qPrintable(QString("a dimmer-only fixture lost its level (%1) -- it "
                                "has no colour to be black").arg(pdim)));

    delete doc;
}

void Monitor_Test::everyColourModelTheEngineDefinesIsRead()
{
    /* QLCChannel::PrimaryColour defines twelve primaries. This read five --
       Red, Green, Blue, White, Amber -- and fell through `default:` for the
       rest, so a SUBTRACTIVE fixture (every mover with CMY colour-mixing
       flags) never showed its live colour at all, whatever it was doing, and
       UV/Lime/Indigo emitters contributed nothing.
     *
       Found by reading QLC+ 5's FixtureUtils::headColor(), which had all of
       this right. See RIG3D_STRATEGY_REVIEW.md. */
    Doc *doc = new Doc(this);

    auto build = [&](const QVector<QLCChannel::PrimaryColour> &cols,
                     quint32 addr, const char *name) -> Fixture * {
        QLCFixtureDef *def = new QLCFixtureDef();
        def->setManufacturer("Test"); def->setModel(name);
        def->setType(QLCFixtureDef::ColorChanger);
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName("mode");
        for (int k = 0; k < cols.size(); ++k)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("c%1").arg(k));
            ch->setGroup(QLCChannel::Intensity);
            ch->setColour(cols.at(k));
            def->addChannel(ch);
            mode->insertChannel(ch, k);
        }
        QLCPhysical ph; ph.setWidth(300); ph.setHeight(300); ph.setDepth(300);
        mode->setPhysical(ph);
        QLCFixtureHead hd;
        for (int k = 0; k < cols.size(); ++k) hd.addChannel(quint32(k));
        mode->insertHead(-1, hd);
        def->addMode(mode);

        Fixture *f = new Fixture(doc);
        f->setName(name); f->setFixtureDefinition(def, mode);
        f->setUniverse(3); f->setAddress(addr);
        return doc->addFixture(f) ? f : nullptr;
    };

    /* CMY, the one that was completely broken. Cyan flag full in, magenta and
       yellow out, passes CYAN light. Subtractive: DMX 0 everywhere is white,
       not black. */
    Fixture *cmy = build({ QLCChannel::Cyan, QLCChannel::Magenta, QLCChannel::Yellow },
                         0, "CMY Mover");
    QVERIFY(cmy != nullptr);
    QByteArray u(512, char(0));
    u[0] = char(255);
    cmy->setChannelValues(u);

    QColor col(90, 160, 235);
    uchar dim = 0;
    QVERIFY(fixtureLiveState(cmy, col, dim));
    QVERIFY2(col.green() > 150 && col.blue() > 150 && col.red() < 60,
             qPrintable(QString("a CMY fixture flagged to cyan read as %1 -- "
                                "subtractive colour is not being read")
                        .arg(col.name())));
    QVERIFY2(dim > 200,
             qPrintable(QString("a colour-mixing fixture with no dimmer channel "
                                "should read as ON, got %1").arg(dim)));

    // All flags out = white light through.
    cmy->setChannelValues(QByteArray(512, char(0)));
    QVERIFY(fixtureLiveState(cmy, col, dim));
    QVERIFY2(col.red() > 200 && col.green() > 200 && col.blue() > 200,
             qPrintable(QString("CMY with every flag out should pass white, got %1")
                        .arg(col.name())));

    /* UV, Lime and Indigo: emitters that were silently dropped. Each should
       pull the result toward its own colour. */
    struct { QLCChannel::PrimaryColour c; const char *name; } extras[] = {
        { QLCChannel::UV,     "UV" },
        { QLCChannel::Lime,   "Lime" },
        { QLCChannel::Indigo, "Indigo" },
    };
    quint32 addr = 16;
    for (int k = 0; k < 3; ++k)
    {
        Fixture *f = build({ QLCChannel::Red, extras[k].c }, addr, extras[k].name);
        QVERIFY(f != nullptr);
        addr += 8;

        QByteArray uu(512, char(0));
        uu[int(f->address())] = char(255);          // red only
        f->setChannelValues(uu);
        QColor redOnly(0, 0, 0);
        uchar d1 = 0;
        QVERIFY(fixtureLiveState(f, redOnly, d1));

        uu[int(f->address()) + 1] = char(255);      // and the extra emitter
        f->setChannelValues(uu);
        QColor withExtra(0, 0, 0);
        uchar d2 = 0;
        QVERIFY(fixtureLiveState(f, withExtra, d2));

        QVERIFY2(withExtra != redOnly,
                 qPrintable(QString("driving the %1 emitter changed nothing "
                                    "(%2 either way)")
                            .arg(extras[k].name).arg(redOnly.name())));
    }

    /* White BLENDS rather than adds. Half white over full red must not clip
       the green and blue channels up in lockstep the way adding does. */
    Fixture *rw = build({ QLCChannel::Red, QLCChannel::White }, 40, "RW");
    QVERIFY(rw != nullptr);
    QByteArray uw(512, char(0));
    uw[40] = char(255); uw[41] = char(128);
    rw->setChannelValues(uw);
    QColor rwc(0, 0, 0);
    uchar dw = 0;
    QVERIFY(fixtureLiveState(rw, rwc, dw));
    QVERIFY2(rwc.red() > rwc.green() + 40,
             qPrintable(QString("half white over full red should still read RED, "
                                "got %1").arg(rwc.name())));

    delete doc;
}

void Monitor_Test::pixelBarDrawsEachPixelInItsOwnColour()
{
    /* A pixel bar is not one colour. Reducing every head to a single
       fixture-wide colour by taking the per-primary maximum turns red
       pixels + blue pixels into a pale wash -- which is exactly what a step
       front running a multi-colour scene drew as, instead of its content. */
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Pixel Bar 8");
    def->setType(QLCFixtureDef::LEDBarPixels);

    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("24ch");
    const int PIX = 8;
    for (int h = 0; h < PIX; ++h)
    {
        QLCChannel *r = new QLCChannel();
        r->setName(QString("%1-Red").arg(h + 1));
        r->setGroup(QLCChannel::Intensity); r->setColour(QLCChannel::Red);
        QLCChannel *g = new QLCChannel();
        g->setName(QString("%1-Green").arg(h + 1));
        g->setGroup(QLCChannel::Intensity); g->setColour(QLCChannel::Green);
        QLCChannel *b = new QLCChannel();
        b->setName(QString("%1-Blue").arg(h + 1));
        b->setGroup(QLCChannel::Intensity); b->setColour(QLCChannel::Blue);
        def->addChannel(r); def->addChannel(g); def->addChannel(b);
        mode->insertChannel(r, h * 3);
        mode->insertChannel(g, h * 3 + 1);
        mode->insertChannel(b, h * 3 + 2);
        QLCFixtureHead head;
        head.addChannel(quint32(h * 3));
        head.addChannel(quint32(h * 3 + 1));
        head.addChannel(quint32(h * 3 + 2));
        mode->insertHead(-1, head);
    }
    QLCPhysical ph;
    ph.setWidth(2000); ph.setHeight(120); ph.setDepth(120);
    ph.setLayoutSize(QSize(PIX, 1));
    mode->setPhysical(ph);
    def->addMode(mode);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Pixel Bar"); fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(3); fxi->setAddress(0);
    QVERIFY(m_doc->addFixture(fxi));

    const FixtureVisualTraits traits = classifyFixture(fxi);
    QCOMPARE(traits.layout, QSize(PIX, 1));
    QCOMPARE(traits.headCount, PIX);

    /* Alternating red and blue pixels. Collapsed fixture-wide this reads as
       magenta (max-red AND max-blue); per head it is what it is. */
    QByteArray u(512, char(0));
    for (int h = 0; h < PIX; ++h)
        u[(h % 2 == 0) ? h * 3 : h * 3 + 2] = char(255);
    fxi->setChannelValues(u);

    QColor whole(90, 160, 235);
    uchar wholeDim = 0;
    QVERIFY(fixtureLiveState(fxi, whole, wholeDim));
    QVERIFY2(whole.red() > 200 && whole.blue() > 200,
             "the fixture-wide colour is expected to wash out -- that is the "
             "reason per-head colour exists");

    QColor h0(90, 160, 235), h1(90, 160, 235);
    uchar d0 = 0, d1 = 0;
    QVERIFY(fixtureHeadLiveState(fxi, 0, h0, d0));
    QVERIFY(fixtureHeadLiveState(fxi, 1, h1, d1));
    QVERIFY2(h0.red() > 200 && h0.blue() < 40,
             qPrintable(QString("head 0 should be red, got %1").arg(h0.name())));
    QVERIFY2(h1.blue() > 200 && h1.red() < 40,
             qPrintable(QString("head 1 should be blue, got %1").arg(h1.name())));

    /* And the drawing has to actually use them -- in both the elevation and
       the angled view, which have separate pixel loops. */
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("Pixel Bar Truss"); t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(0.0f, 0.0f, 3.0f)); t->setDirection(QPointF(1.0, 0.0));
    t->setLength(4.0f); t->setWidth(0.3f);
    props->setFixturePosition(fxi->id(), 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp;
    rp.trussId = t->id(); rp.trussOffset = 2.0f;
    props->setFixtureRigProps(fxi->id(), rp);

    StructureStudioView v(m_doc, StructureStudioView::TrussKind, t->id());
    v.resize(900, 600);
    v.reload();
    v.setLiveValues(true);
    v.setAmbient(0.60);

    auto countDominant = [&](int &reds, int &blues) {
        const QImage img = v.grab().toImage();
        reds = blues = 0;
        for (int y = 0; y < img.height(); ++y)
        {
            for (int x = 0; x < img.width(); ++x)
            {
                const QColor c = img.pixelColor(x, y);
                if (c.red() > 90 && c.red() > c.blue() * 2) ++reds;
                if (c.blue() > 90 && c.blue() > c.red() * 2) ++blues;
            }
        }
    };

    int reds = 0, blues = 0;
    v.setPlane(StructureStudioView::Front);
    countDominant(reds, blues);
    QVERIFY2(reds > 20 && blues > 20,
             qPrintable(QString("elevation drew no per-pixel colour: %1 red, "
                                "%2 blue pixels").arg(reds).arg(blues)));

    /* The angled view draws its pixels as 1.2 px dots, so the counts here are
       small by construction -- a handful each. What matters is that BOTH
       colours are present at all: collapsed to the fixture-wide colour they
       are all magenta and neither count can be non-zero. */
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(15.0, 20.0);
    countDominant(reds, blues);
    QVERIFY2(reds > 5 && blues > 5,
             qPrintable(QString("angled view drew no per-pixel colour: %1 red, "
                                "%2 blue pixels").arg(reds).arg(blues)));

    m_doc->deleteFixture(fxi->id());
    props->removeTruss(t->id());
}


void Monitor_Test::aRevealedFixtureCanBePickedOnItsOwn()
{
    /* "So when double clicking i get ALL the lights .. stacked .. I can't
       select which one i want to check the visibility area." Fixtures buried
       under one another are all at the same point, so the pile can only ever
       be clicked once. Each revealed fixture gets a named tag beside the
       stack, and clicking a tag picks that fixture alone. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *tower = props->addTruss();
    tower->setName("T-1");
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(3.0f, 3.0f, 0.0f));
    tower->setLength(4.0f);
    tower->setWidth(0.29f);

    QList<quint32> fids;
    for (int i = 0; i < 3; ++i)
    {
        Fixture *f = makeMover(doc, 14.0, quint32(i * 8), qPrintable(QString("Head %1").arg(i + 1)),
                               QVector3D(3.0f, 3.0f, 4.0f));
        QVERIFY(f != nullptr);
        FixtureRigProps rp;
        rp.trussId = tower->id();
        rp.trussOffset = 4.0f;
        props->setFixtureRigProps(f->id(), rp);
        fids << f->id();
        QByteArray u(512, char(0));
        u[int(f->address())] = char(128);
        u[int(f->address()) + 1] = char(128);
        f->setChannelValues(u);
    }

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 7));
        view.updateTrusses();
        foreach (quint32 fid, fids)
            view.addFixture(fid, QPointF(3000.0, 3000.0));
        view.setViewPOV(MonitorGraphicsView::PovTop);
        view.refreshItemLayerState();

        view.revealBuriedFixtures(QStringLiteral("truss"), tower->id());

        // A tag each, and no two on top of each other: that is the whole point.
        QList<QRectF> tags;
        foreach (quint32 fid, fids)
        {
            const QRectF r = view.revealTagRect(fid);
            QVERIFY2(!r.isEmpty(), "a revealed fixture has no tag");
            foreach (const QRectF &other, tags)
                QVERIFY2(!other.intersects(r), "two tags sit on top of each other");
            tags << r;
        }

        // Click the middle one, the way a user would, with footprints asked for.
        view.setProjectionOverlayVisible(true);
        const quint32 wanted = fids.at(1);
        const QPointF sp = view.revealTagRect(wanted).center();
        const QPoint vp = view.mapFromScene(sp);
        QMouseEvent press(QEvent::MouseButtonPress, vp, view.viewport()->mapToGlobal(vp),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view.viewport(), &press);

        QCOMPARE(view.revealedFocus(), qint64(wanted));
        foreach (quint32 fid, fids)
        {
            QCOMPARE(view.m_fixtures.value(fid)->isSelected(), fid == wanted);
            // Only the one picked is drawn; the rest stay tags.
            QCOMPARE(view.m_fixtures.value(fid)->isVisible(), fid == wanted);
        }
        QVERIFY2(view.m_projectionItems.size() == 1,
                 qPrintable(QString("picking one head drew %1 footprints")
                            .arg(view.m_projectionItems.size())));
    }

    delete doc;
}

void Monitor_Test::aLightFootprintSaysWhoseItIs()
{
    /* "Selecting them all .. it's not clear to me what these cones represent."
       A footprint carries the name of the head that casts it, and can be asked
       for by name from the right-click menu -- on one head, or on everything a
       truss carries -- without selecting anything first. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *bar = props->addTruss();
    bar->setName("T-A");
    bar->setOrigin(QVector3D(1.0f, 3.0f, 4.0f));
    bar->setLength(4.0f);

    Fixture *spot = makeMover(doc, 14.0, 0, "Front Spot", QVector3D(2.0f, 3.0f, 4.0f));
    QVERIFY(spot != nullptr);
    FixtureRigProps rp;
    rp.trussId = bar->id();
    rp.trussOffset = 1.0f;
    props->setFixtureRigProps(spot->id(), rp);
    QByteArray u(512, char(0));
    u[int(spot->address())] = char(128);
    u[int(spot->address()) + 1] = char(128);
    spot->setChannelValues(u);

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 7));
        view.updateTrusses();
        view.addFixture(spot->id(), QPointF(2000.0, 3000.0));
        view.setViewPOV(MonitorGraphicsView::PovTop);

        QCOMPARE(view.m_projectionItems.size(), 0);

        // The fixture's own entry, with nothing selected.
        QMenu fixMenu;
        view.populateItemMenu(&fixMenu, QStringLiteral("fixture"), spot->id(), QPointF(), false);
        QAction *act = nullptr;
        foreach (QAction *a, fixMenu.actions())
            if (a->text().contains(QStringLiteral("Light Footprint")))
                act = a;
        QVERIFY2(act != nullptr, "a fixture offers no way to show where its light lands");
        QVERIFY(act->isCheckable() && !act->isChecked());
        act->trigger();

        QCOMPARE(view.m_projectionItems.size(), 1);
        QVERIFY(view.fixtureFootprintShown(spot->id()));

        // It says whose it is, on the plot and in the tooltip.
        bool named = false;
        foreach (QGraphicsItem *child, view.m_projectionItems.first()->childItems())
        {
            foreach (QGraphicsItem *gc, child->childItems())
                if (QGraphicsSimpleTextItem *t = dynamic_cast<QGraphicsSimpleTextItem *>(gc))
                    if (t->text() == spot->name())
                        named = true;
        }
        QVERIFY2(named, "a footprint does not say which head casts it");
        QVERIFY2(view.m_projectionItems.first()->toolTip().contains(spot->name()),
                 qPrintable(view.m_projectionItems.first()->toolTip()));
        // Hatched, so overlapping ones stay tellable apart and nothing reads
        // as a solid no-light zone.
        QCOMPARE(view.m_projectionItems.first()->brush().style(), Qt::BDiagPattern);

        // And the same question asked of the truss covers what it carries.
        act->trigger();                       // off again
        QCOMPARE(view.m_projectionItems.size(), 0);
        QMenu trussMenu;
        view.populateItemMenu(&trussMenu, QStringLiteral("truss"), bar->id(), QPointF(), false);
        QAction *tact = nullptr;
        foreach (QAction *a, trussMenu.actions())
            if (a->text().contains(QStringLiteral("Light Footprint")))
                tact = a;
        QVERIFY2(tact != nullptr, "a truss offers no way to show what its heads light");
        tact->trigger();
        QCOMPARE(view.m_projectionItems.size(), 1);
        QVERIFY(view.structureFootprintsShown(QStringLiteral("truss"), bar->id()));
    }

    delete doc;
}

void Monitor_Test::addingAPersonStandsOneWhereYouClicked()
{
    /* "Did we have the 'Add person here'?" No -- only "Add Target Position",
       which makes a bare mark you then have to open and re-type as a Person.
       One step now, and the figure stands where the click was. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    QWidget parent;
    Monitor *mon = new Monitor(&parent, doc);
    MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    gv->resize(1000, 700);
    gv->setGridMetrics(1000.0);
    gv->setGridSize(QSize(10, 7));

    const QPointF where = gv->realPositionToPixels(4000.0, 2000.0);
    mon->m_pendingAddScenePos = where;
    mon->slotAddPerson();

    QCOMPARE(props->stageTargets().count(), 1);
    StageTarget *person = props->stageTargets().first();
    QCOMPARE(person->kind(), StageTarget::Kind::Person);
    QVERIFY2(qAbs(person->x() - 4.0f) < 0.05f && qAbs(person->y() - 2.0f) < 0.05f,
             qPrintable(QString("stood at %1,%2 instead of 4,2")
                        .arg(double(person->x())).arg(double(person->y()))));
    // Named as a person, and drawn as one: its height follows the subject.
    QVERIFY2(person->name().contains(QStringLiteral("Person")), qPrintable(person->name()));
    QCOMPARE(props->targetAimHeight(person, true), props->aimSubjectHeight());

    delete mon;
    delete doc;
}

void Monitor_Test::aHeadAimedAtAMarkDrawsItsBeamThere()
{
    /* "I clicked on T1-Spot to see its area and didn't get anything."

       Two halves of one model that have to agree: AimSolver turns "point at
       this" into DMX, and fixtureAimDirection turns DMX back into the ray the
       beams, the aim lines and the footprint are drawn from. Aim a head at a
       mark, feed it its own answer, and the ray must arrive at that mark --
       hung or standing, inverted or not.

       They did not. The solver measured its bearing from UPSTAGE while every
       view measured from downstage, so a head aimed at a mark drew its beam at
       the mirror image of that mark through the head: 180 degrees out, every
       time, for every fixture. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *bar = props->addTruss();
    bar->setName("T-1");
    bar->setOrigin(QVector3D(0.0f, 3.0f, 3.0f));
    bar->setLength(6.0f);

    struct Case { const char *what; Truss::MountingType mount; bool panInv; bool tiltInv; QVector3D mark; };
    /* A standing head's tilt is centred straight UP, so with this fixture's
       180 degrees of travel it works the sky, not the deck -- which is the
       whole of Branson's point about a 270 degree head on a truss top. */
    const QList<Case> cases = {
        { "hung under the bar",      Truss::TopHung,      false, false, QVector3D(4.0f, 1.5f, 0.0f) },
        { "hung, facing stage left", Truss::TopHung,      false, false, QVector3D(-1.0f, 4.5f, 0.0f) },
        { "standing on the bar",     Truss::FloorMounted, false, false, QVector3D(4.0f, 1.5f, 5.0f) },
        { "hung, pan inverted",      Truss::TopHung,      true,  false, QVector3D(4.0f, 1.5f, 0.0f) },
        { "standing, tilt inverted", Truss::FloorMounted, false, true,  QVector3D(4.0f, 1.5f, 5.0f) },
    };

    int addr = 0;
    foreach (const Case &c, cases)
    {
        Fixture *fx = makeMover(doc, 14.0, quint32(addr), c.what, QVector3D(2.0f, 3.0f, 3.0f));
        QVERIFY(fx != nullptr);
        addr += 16;
        FixtureRigProps rp;
        rp.trussId = bar->id();
        rp.trussOffset = 2.0f;
        rp.mountingType = c.mount;
        rp.panInvert = c.panInv;
        rp.tiltInvert = c.tiltInv;
        props->setFixtureRigProps(fx->id(), rp);

        AimSolver::Solution sol;
        QVERIFY2(AimSolver::reach(doc, fx->id(), c.mark, sol),
                 qPrintable(QString("no frame for the head %1").arg(c.what)));
        QVERIFY2(sol.reachable(),
                 qPrintable(QString("a head %1 says it cannot reach its mark (margin %2)")
                            .arg(c.what).arg(double(sol.marginDeg))));

        // Its own answer, written back as DMX the way a scene would.
        QLCFixtureMode *mode = fx->fixtureMode();
        const quint32 panCh = mode->heads().first().channelNumber(QLCChannel::Pan, QLCChannel::MSB);
        const quint32 tiltCh = mode->heads().first().channelNumber(QLCChannel::Tilt, QLCChannel::MSB);
        const QLCPhysical phy = mode->physical();
        const float panMax = phy.focusPanMax() ? float(phy.focusPanMax()) : 360.0f;
        const float tiltMax = phy.focusTiltMax() ? float(phy.focusTiltMax()) : 270.0f;
        QByteArray v(512, char(0));
        v[int(fx->address() + panCh)] = char(uchar(qRound(sol.panDeg * 255.0f / panMax)));
        v[int(fx->address() + tiltCh)] = char(uchar(qRound(sol.tiltDeg * 255.0f / tiltMax)));
        fx->setChannelValues(v);

        QVector3D dir;
        QVERIFY(fixtureAimDirection(fx, rp, dir));
        const QVector3D from = props->fixtureRigPosition(fx->id());
        QVector3D want = c.mark - from;
        want.normalize();
        const float offBy = float(qRadiansToDegrees(qAcos(double(qBound(-1.0f,
                                    QVector3D::dotProduct(dir, want), 1.0f)))));
        QVERIFY2(offBy < 2.0f,
                 qPrintable(QString("a head %1 aimed at (%2,%3,%4) draws its beam %5 degrees away "
                                    "(ray %6,%7,%8)")
                            .arg(c.what).arg(double(c.mark.x())).arg(double(c.mark.y()))
                            .arg(double(c.mark.z())).arg(double(offBy))
                            .arg(double(dir.x())).arg(double(dir.y())).arg(double(dir.z()))));
    }

    delete doc;
}

void Monitor_Test::anUprightHeadCannotPointBesideItself()
{
    /* Branson's physics: a head standing upright on top of a truss has its
       tilt centred straight UP, so 270 degrees of travel reaches 45 degrees
       past the horizon -- nowhere near the floor at its own feet. The model
       has to say so rather than quietly aiming it there. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *bar = props->addTruss();
    bar->setOrigin(QVector3D(0.0f, 3.0f, 3.0f));
    bar->setLength(6.0f);

    Fixture *fx = makeMover(doc, 14.0, 0, "Upright", QVector3D(2.0f, 3.0f, 3.0f));
    QVERIFY(fx != nullptr);
    FixtureRigProps rp;
    rp.trussId = bar->id();
    rp.trussOffset = 2.0f;
    rp.mountingType = Truss::FloorMounted;
    props->setFixtureRigProps(fx->id(), rp);

    const QVector3D position = props->fixtureRigPosition(fx->id());
    AimSolver::Solution atMyFeet;
    QVERIFY(AimSolver::reach(doc, fx->id(), QVector3D(position.x(), position.y(), 0.0f), atMyFeet));
    QVERIFY2(!atMyFeet.reachable(), "an upright head claims it can point straight down");

    // The same head hung from the same bar has no trouble at all.
    rp.mountingType = Truss::TopHung;
    props->setFixtureRigProps(fx->id(), rp);
    AimSolver::Solution hung;
    QVERIFY(AimSolver::reach(doc, fx->id(), QVector3D(position.x(), position.y(), 0.0f), hung));
    QVERIFY2(hung.reachable(), "a hung head cannot point straight down");

    delete doc;
}

void Monitor_Test::aWashOnATrussThrowsLightToo()
{
    /* "Mounted T1-W and set it to white wash .. don't see any light from it in
       the rig view .. did we add washes or just spots?" Just spots: the whole
       beam pass lived inside drawMoverSolid, so a fixture that cannot move
       could not shine -- which on a real stage is most of the light. A par or
       bar hung on a truss now throws a cone down the way it faces. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *bar = props->addTruss();
    bar->setName("T-1");
    bar->setOrigin(QVector3D(0.0f, 3.0f, 5.0f));
    bar->setLength(6.0f);

    Fixture *wash = makeChunkyBar(doc, 0, "T1-W");
    QVERIFY(wash != nullptr);
    FixtureRigProps rp;
    rp.trussId = bar->id();
    rp.trussOffset = 3.0f;
    rp.mountingType = Truss::TopHung;            // hanging: it points down
    props->setFixtureRigProps(wash->id(), rp);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(700, 560);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(25.0, 20.0);
    v.setLiveValues(true);
    v.setAmbient(0.0);

    auto lumaBelow = [&](int level) {
        QByteArray u(512, char(0));
        u[0] = char(level); u[1] = char(level); u[2] = char(level);   // white
        wash->setChannelValues(u);
        const QImage img = v.grab().toImage();
        // w2s() is logical pixels; grab() on a HiDPI screen returns device
        // pixels (devicePixelRatio() times as many). Scale the probe (and the
        // logical-pixel offsets that build it) up to match, or the sample
        // point lands nowhere near the beam on a Retina display.
        const double dpr = img.devicePixelRatio();
        const QPointF at = v.w2s(v.fixtureCentre(wash->id())) * dpr;
        const QPointF probe(at.x(), at.y() + 110 * dpr);      // into the throw
        double sum = 0; int n = 0;
        for (int dy = -12; dy <= 12; ++dy)
            for (int dx = -12; dx <= 12; ++dx)
            {
                const QPoint q(probe.toPoint() + QPoint(int(dx * dpr), int(dy * dpr)));
                if (!img.rect().contains(q)) continue;
                const QColor c = img.pixelColor(q);
                sum += c.red() * 0.3 + c.green() * 0.59 + c.blue() * 0.11;
                ++n;
            }
        return n ? sum / n : 0.0;
    };

    const double dark = lumaBelow(0);
    const double lit  = lumaBelow(255);
    QVERIFY2(lit - dark > 10.0,
             qPrintable(QString("a lit wash threw nothing below it (%1 against a "
                                "dark room's %2)").arg(lit).arg(dark)));

    delete doc;
}

void Monitor_Test::aPlatformOffersToPutSomethingOnIt()
{
    /* "When I right click on a platform I get most of the platform type options
       in the menu .. but no menu to add anything like a fixture or target." A
       riser is a place things stand on, so its menu offers to put them there --
       under one Add submenu, not loose entries. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    StagePlatform *deck = props->addPlatform();
    deck->setName("Riser");
    deck->setOriginX(1.0f); deck->setOriginY(1.0f);
    deck->setWidth(4.0f); deck->setDepth(3.0f); deck->setHeight(0.6f);

    QWidget parent;
    Monitor *mon = new Monitor(&parent, doc);
    MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    gv->resize(1000, 700);
    gv->setGridMetrics(1000.0);
    gv->setGridSize(QSize(10, 7));
    gv->updatePlatforms();

    // On the riser, a metre in from its downstage-left corner.
    const QPointF where = gv->realPositionToPixels(2000.0, 2000.0);
    QMenu menu;
    gv->populateItemMenu(&menu, QStringLiteral("platform"), deck->id(), where, false);

    QMenu *addMenu = nullptr;
    foreach (QAction *a, menu.actions())
        if (a->menu() != nullptr && a->text() == QStringLiteral("Add"))
            addMenu = a->menu();
    QVERIFY2(addMenu != nullptr, "a platform offers no way to add anything to it");

    QStringList offered;
    QAction *person = nullptr;
    foreach (QAction *a, addMenu->actions())
    {
        if (a->isSeparator())
            continue;
        offered << a->text();
        if (a->text().contains(QStringLiteral("Person")))
            person = a;
    }
    QVERIFY2(offered.filter(QStringLiteral("Fixture")).count() == 1,
             qPrintable(QString("Add offers: %1").arg(offered.join(", "))));
    QVERIFY2(offered.filter(QStringLiteral("Target")).count() == 1,
             qPrintable(QString("Add offers: %1").arg(offered.join(", "))));
    QVERIFY2(person != nullptr,
             qPrintable(QString("Add offers: %1").arg(offered.join(", "))));

    // And it actually puts one there, standing ON the riser, not under it.
    person->trigger();
    QCOMPARE(props->stageTargets().count(), 1);
    StageTarget *stood = props->stageTargets().first();
    QCOMPARE(stood->kind(), StageTarget::Kind::Person);
    QVERIFY2(qAbs(stood->x() - 2.0f) < 0.05f && qAbs(stood->y() - 2.0f) < 0.05f,
             qPrintable(QString("stood at %1,%2 instead of 2,2")
                        .arg(double(stood->x())).arg(double(stood->y()))));
    QVERIFY2(qAbs(stood->z() - 0.6f) < 0.01f,
             qPrintable(QString("stood at %1 m, not on the 0.6 m riser")
                        .arg(double(stood->z()))));

    delete mon;
    delete doc;
}

void Monitor_Test::theFootprintToggleTurnsThemOff()
{
    /* "Light footprint checkbox in Show isn't working .. I see it when
       unchecked." It gated only the selection, so a footprint asked for by
       name -- from a fixture's own menu, or by a bubble -- ignored it. The
       toggle is the master switch: unticked, nothing is drawn, whoever asked. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *bar = props->addTruss();
    bar->setOrigin(QVector3D(1.0f, 3.0f, 4.0f));
    bar->setLength(4.0f);

    Fixture *spot = makeMover(doc, 14.0, 0, "Front Spot", QVector3D(2.0f, 3.0f, 4.0f));
    QVERIFY(spot != nullptr);
    FixtureRigProps rp;
    rp.trussId = bar->id();
    rp.trussOffset = 1.0f;
    props->setFixtureRigProps(spot->id(), rp);
    QByteArray u(512, char(0));
    u[int(spot->address())] = char(128);
    u[int(spot->address()) + 1] = char(128);      // straight down
    spot->setChannelValues(u);

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 7));
        view.updateTrusses();
        view.addFixture(spot->id(), QPointF(2000.0, 3000.0));
        view.setViewPOV(MonitorGraphicsView::PovTop);

        // Asked for by name: it appears, and the toggle now says so.
        view.setFixtureFootprintShown(spot->id(), true);
        QCOMPARE(view.m_projectionItems.size(), 1);
        QVERIFY2(view.projectionOverlayVisible(),
                 "asking for a footprint left the Show toggle saying they are off");

        // Unticked: gone, and it stays gone when the selection changes.
        view.setProjectionOverlayVisible(false);
        QCOMPARE(view.m_projectionItems.size(), 0);
        view.selectMapItem(QStringLiteral("fixture"), spot->id());
        QCoreApplication::processEvents();
        QVERIFY2(view.m_projectionItems.isEmpty(),
                 "footprints came back while the toggle was unticked");
    }

    delete doc;
}

void Monitor_Test::aFootprintFollowsTheSceneAim()
{
    /* T1-Spot stands upright on a truss top, so at rest its tilt centre is the
       horizon exactly and nothing lands -- while the scene beside it is aiming
       that same head at a mark on the deck. The footprint asks the same
       question the rig view does: where is this head POINTED, scene first. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *tower = props->addTruss();
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(2.0f, 3.0f, 0.0f));
    tower->setLength(4.0f);
    tower->setWidth(0.29f);

    Fixture *spot = makeMover(doc, 14.0, 0, "T1-Spot", QVector3D(2.0f, 3.0f, 4.0f));
    QVERIFY(spot != nullptr);
    /* A real 270-degree head, like the one on T-1: standing upright its tilt
       is centred on the sky, so it works the deck from 45 degrees below the
       horizon outward -- near its own feet is exactly what it cannot do. */
    QLCPhysical phy = spot->fixtureMode()->physical();
    phy.setFocusTiltMax(270);
    spot->fixtureMode()->setPhysical(phy);

    FixtureRigProps rp;
    rp.trussId = tower->id();
    rp.trussOffset = 4.0f;
    rp.mountingType = Truss::FloorMounted;        // standing on the truss top
    props->setFixtureRigProps(spot->id(), rp);

    StageTarget *mark = props->addStageTarget();
    mark->setName("Mark");
    mark->setPosition(QVector3D(8.0f, 3.0f, 0.0f));
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setStageTargetId(mark->id());
    QVERIFY(doc->addPalette(aim));
    Scene *scene = new Scene(doc);
    scene->setName("Spot on the mark");
    scene->addFixture(spot->id());
    scene->addPalette(aim->id());
    QVERIFY(doc->addFunction(scene));

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 7));
        view.updateTrusses();
        view.updateTargets();
        view.addFixture(spot->id(), QPointF(2000.0, 3000.0));
        view.setViewPOV(MonitorGraphicsView::PovTop);
        view.setProjectionOverlayVisible(true);
        view.setActiveScene(scene->id());
        view.setFixtureFootprintShown(spot->id(), true);

        // Where the two ends of the aim actually are.
        const QVector3D apex = fixtureWorldCentre(doc, spot->id());
        const QVector3D at = props->targetAimPoint(mark, true);
        QVERIFY2(qAbs(apex.z() - 4.0f) < 0.2f,
                 qPrintable(QString("the head sits at z=%1, not on the 4 m tower top")
                            .arg(double(apex.z()))));
        QVERIFY2(at.z() < apex.z(),
                 qPrintable(QString("the mark (z=%1) is not below the head (z=%2)")
                            .arg(double(at.z())).arg(double(apex.z()))));

        // The scene really does aim it, and it really can get there.
        const QList<AimSolver::SceneAim> aims = AimSolver::sceneAims(doc, scene);
        QCOMPARE(aims.size(), 1);
        QVERIFY2(aims.first().solution.reachable(),
                 qPrintable(QString("the head cannot reach its own mark (margin %1)")
                            .arg(double(aims.first().solution.marginDeg))));

        QVERIFY2(view.m_projectionItems.size() == 1,
                 qPrintable(QString("%1 footprints drawn for an aimed head")
                            .arg(view.m_projectionItems.size())));
        const QPolygonF poly = view.fixtureProjection(spot->id());
        QVERIFY2(poly.size() >= 3,
                 "the head is aimed at a mark it can reach, and nothing was projected");
        const QPointF pool = poly.boundingRect().center();
        const QPointF want = view.realPositionToPixels(8000.0, 3000.0);
        const double off = QLineF(pool, want).length();
        const QPointF poolMm = view.pixelsToRealPosition(pool.x(), pool.y());
        QVERIFY2(off < view.m_cellPixels * 0.75,
                 qPrintable(QString("the pool landed at (%1, %2) m, %3 px from the "
                                    "mark at (8, 3) it is aimed at")
                            .arg(poolMm.x() / 1000.0).arg(poolMm.y() / 1000.0).arg(off)));
    }

    delete doc;
}

void Monitor_Test::aSelectedHeadShowsEverywhereItCanHit()
{
    /* "If I select T1-Spot and I have footprint enabled I should see ALL THE
       AREA THAT THE SPOT CAN HIT." The footprint drew the instantaneous cone,
       which answers "where is it pointed" -- a different and much smaller
       question, and for a head resting at its tilt centre, no shape at all.

       T1-Spot as rigged in the demo show: a 270-degree head standing UPRIGHT on
       top of a horizontal truss. Its tilt is centred on the sky, so it reaches
       45 degrees below the horizon at best -- which on a 4.5 m truss means it
       cannot light anything closer to its own feet than about 4.5 m, and can
       light the deck everywhere beyond that. Both halves are asserted: the
       hole, and the ring around it. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *bar = props->addTruss();
    bar->setName("T-1");
    bar->setOrigin(QVector3D(1.0f, 6.0f, 4.5f));
    bar->setLength(8.0f);

    Fixture *spot = makeMover(doc, 14.0, 0, "T1-Spot", QVector3D(5.0f, 6.0f, 4.5f));
    QVERIFY(spot != nullptr);
    QLCPhysical phy = spot->fixtureMode()->physical();
    phy.setFocusTiltMax(270);
    spot->fixtureMode()->setPhysical(phy);

    FixtureRigProps rp;
    rp.trussId = bar->id();
    rp.trussOffset = 4.0f;                        // 4 m along: x = 5
    rp.mountingType = Truss::FloorMounted;        // standing on the truss top
    props->setFixtureRigProps(spot->id(), rp);

    {
        MonitorGraphicsView view(doc);
        view.resize(1100, 800);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(12, 12));
        view.updateTrusses();
        view.addFixture(spot->id(), QPointF(5000.0, 6000.0));
        view.setViewPOV(MonitorGraphicsView::PovTop);

        const QVector3D at = fixtureWorldCentre(doc, spot->id());
        QVERIFY2(qAbs(at.z() - 4.5f) < 0.2f,
                 qPrintable(QString("the head is at z=%1, not on the 4.5 m truss")
                            .arg(double(at.z()))));

        double cell = 0.0;
        const QImage cov = view.fixtureCoverage(spot->id(), &cell);
        QVERIFY2(!cov.isNull(), "a moving head covers nothing at all");
        QVERIFY(cell > 0.0);

        auto covered = [&](double x, double y) {
            const int c = int(x / cell);
            const int r = int(y / cell);
            if (c < 0 || r < 0 || c >= cov.width() || r >= cov.height())
                return false;
            return qAlpha(cov.pixel(c, r)) > 0;
        };

        // Its own feet: 4.5 m up with 45 degrees of reach below the horizon
        // cannot be pointed at anything nearer than 4.5 m away.
        QVERIFY2(!covered(5.0, 6.0),
                 "the head claims it can light the deck directly under itself");
        QVERIFY2(!covered(6.5, 6.0),
                 "the head claims it can light 1.5 m from its own feet");

        // And the deck well out from it, which is what it is FOR.
        QVERIFY2(covered(5.0, 0.5),
                 "the head cannot light the deck 5.5 m downstage of it");
        QVERIFY2(covered(11.0, 6.0),
                 "the head cannot light the deck 6 m to its side");

        /* The whole point of the complaint: selecting it with footprints on
           draws that area, not nothing. */
        view.setProjectionOverlayVisible(true);
        view.selectMapItem(QStringLiteral("fixture"), spot->id());
        QCoreApplication::processEvents();
        QVERIFY2(view.m_coverageItems.size() == 1,
                 qPrintable(QString("selecting the head drew %1 coverage areas")
                            .arg(view.m_coverageItems.size())));
        const QPixmap drawn = view.m_coverageItems.first()->pixmap();
        QVERIFY(!drawn.isNull());

        // It covers a real part of the stage -- not a stray pixel, not all of it.
        int litCells = 0;
        for (int r = 0; r < cov.height(); ++r)
            for (int c = 0; c < cov.width(); ++c)
                if (qAlpha(cov.pixel(c, r)) > 0)
                    ++litCells;
        const double share = double(litCells) / double(cov.width() * cov.height());
        QVERIFY2(share > 0.3 && share < 0.95,
                 qPrintable(QString("the head covers %1%% of the stage, which is "
                                    "not a ring around a 4.5 m hole")
                            .arg(share * 100.0, 0, 'f', 1)));
    }

    delete doc;
}

void Monitor_Test::aHungHeadCoversWhatIsUnderIt()
{
    /* The same head hung from the same bar is centred on straight DOWN, so the
       deck under it is the easiest thing it can light -- the exact opposite of
       the upright case, and the check that the coverage is reading the mount
       rather than drawing a doughnut for everything. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *bar = props->addTruss();
    bar->setOrigin(QVector3D(1.0f, 6.0f, 4.5f));
    bar->setLength(8.0f);

    Fixture *spot = makeMover(doc, 14.0, 0, "Hung", QVector3D(5.0f, 6.0f, 4.5f));
    QVERIFY(spot != nullptr);
    FixtureRigProps rp;
    rp.trussId = bar->id();
    rp.trussOffset = 4.0f;
    rp.mountingType = Truss::TopHung;
    props->setFixtureRigProps(spot->id(), rp);

    {
        MonitorGraphicsView view(doc);
        view.resize(1100, 800);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(12, 12));
        view.updateTrusses();
        view.addFixture(spot->id(), QPointF(5000.0, 6000.0));
        view.setViewPOV(MonitorGraphicsView::PovTop);

        double cell = 0.0;
        const QImage cov = view.fixtureCoverage(spot->id(), &cell);
        QVERIFY2(!cov.isNull(), "a hung head covers nothing at all");
        const int c = int(5.0 / cell), r = int(6.0 / cell);
        QVERIFY2(qAlpha(cov.pixel(c, r)) > 0,
                 "a hung head cannot light the deck directly under itself");
    }

    delete doc;
}

void Monitor_Test::aHeadThatCanLightNothingSaysWhy()
{
    /* T1-Spot, as the demo show actually has it: a 190-degree head standing
       UPRIGHT on top of a truss 3.048 m up. Standing, its tilt centres on the
       sky, so it reaches 5 degrees below level -- the nearest deck it could
       light is 35 m away, well off any stage. Its coverage is therefore
       genuinely empty, and an empty overlay is indistinguishable from a broken
       one unless it says why. Measured against the real workspace before this
       was written: coverage null as rigged, the whole stage once hung. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *bar = props->addTruss();
    bar->setName("T-1");
    bar->setOrigin(QVector3D(1.0f, 1.372f, 3.048f));
    bar->setLength(8.0f);

    Fixture *spot = makeMover(doc, 14.0, 0, "T1-Spot", QVector3D(1.67f, 1.372f, 3.048f));
    QVERIFY(spot != nullptr);
    QLCPhysical phy = spot->fixtureMode()->physical();
    phy.setFocusTiltMax(190);                     // what its 12-channel mode declares
    spot->fixtureMode()->setPhysical(phy);

    FixtureRigProps rp;
    rp.trussId = bar->id();
    rp.trussOffset = 0.67f;
    rp.mountingType = Truss::FloorMounted;        // standing on the truss top
    props->setFixtureRigProps(spot->id(), rp);

    {
        MonitorGraphicsView view(doc);
        view.resize(1100, 800);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(12, 8));
        view.updateTrusses();
        view.addFixture(spot->id(), QPointF(1670.0, 1372.0));
        view.setViewPOV(MonitorGraphicsView::PovTop);
        view.setProjectionOverlayVisible(true);
        view.selectMapItem(QStringLiteral("fixture"), spot->id());
        QCoreApplication::processEvents();

        double cell = 0.0;
        QVERIFY2(view.fixtureCoverage(spot->id(), &cell).isNull(),
                 "an upright 190-degree head on a 3 m truss claims it can light the stage");
        QCOMPARE(view.m_coverageItems.size(), 0);

        // ...and it says so, with the numbers that explain it.
        QVERIFY2(view.m_projectionItems.size() == 1,
                 qPrintable(QString("%1 notes drawn for a head that can light nothing")
                            .arg(view.m_projectionItems.size())));
        QString said;
        foreach (QGraphicsItem *child, view.m_projectionItems.first()->childItems())
            if (QGraphicsSimpleTextItem *t = dynamic_cast<QGraphicsSimpleTextItem *>(child))
                said = t->text();
        QVERIFY2(said.contains("T1-Spot") && said.contains("upright")
                 && said.contains("190") && said.contains("5°"),
                 qPrintable(QString("the note says: \"%1\"").arg(said)));

        // Hung from the same bar, it lights the stage -- the mount is the whole
        // difference, which is what the note is pointing at.
        FixtureRigProps hung = rp;
        hung.mountingType = Truss::TopHung;
        props->setFixtureRigProps(spot->id(), hung);
        double hcell = 0.0;
        QVERIFY2(!view.fixtureCoverage(spot->id(), &hcell).isNull(),
                 "hung from the same bar it still covers nothing");
    }

    delete doc;
}
