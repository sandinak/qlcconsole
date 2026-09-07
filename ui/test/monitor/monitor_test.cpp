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
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>

#define protected public
#define private public
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "trussitem.h"
#undef protected
#undef private

#include "monitor_test.h"
#include "monitorproperties.h"
#include "truss.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "doc.h"

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

    QTimer::singleShot(0, []() {
        QWidget *modal = QApplication::activeModalWidget();
        QVERIFY(modal != NULL);
        QCOMPARE(modal->windowTitle(), QString("Add Truss"));

        QLineEdit *nameEdit = modal->findChild<QLineEdit*>();
        QVERIFY(nameEdit != NULL);
        nameEdit->setText("Pilot Truss");

        QDialogButtonBox *btns = modal->findChild<QDialogButtonBox*>();
        QVERIFY(btns != NULL);
        QPushButton *okBtn = btns->button(QDialogButtonBox::Ok);
        QVERIFY(okBtn != NULL);
        okBtn->click();
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

    QTimer::singleShot(0, []() {
        QWidget *modal = QApplication::activeModalWidget();
        QVERIFY(modal != NULL);

        QDialogButtonBox *btns = modal->findChild<QDialogButtonBox*>();
        QVERIFY(btns != NULL);
        QPushButton *cancelBtn = btns->button(QDialogButtonBox::Cancel);
        QVERIFY(cancelBtn != NULL);
        cancelBtn->click();
    });

    mon.slotAddTruss();

    // Cancelling adds nothing.
    QCOMPARE(m_doc->monitorProperties()->trusses().count(), before);
}

void Monitor_Test::addTargetAccepted()
{
    // slotAddTarget() differs from slotAddTruss(): the StageTarget is created
    // with defaults FIRST, then an edit dialog opens — so even a cancelled
    // edit leaves the target in place (covered by addTargetEditCancelled()
    // below). Accepting here also auto-links a PanTilt palette to it.
    QWidget parent;
    Monitor mon(&parent, m_doc);

    const int beforeTargets = m_doc->monitorProperties()->stageTargets().count();
    const int beforePalettes = m_doc->palettes().count();

    QTimer::singleShot(0, []() {
        QWidget *modal = QApplication::activeModalWidget();
        QVERIFY(modal != NULL);
        QVERIFY(modal->windowTitle().startsWith("Edit Target"));

        QLineEdit *nameEdit = modal->findChild<QLineEdit*>();
        QVERIFY(nameEdit != NULL);
        nameEdit->setText("Pilot Target");

        QDialogButtonBox *btns = modal->findChild<QDialogButtonBox*>();
        QVERIFY(btns != NULL);
        QPushButton *okBtn = btns->button(QDialogButtonBox::Ok);
        QVERIFY(okBtn != NULL);
        okBtn->click();
    });

    mon.slotAddTarget();

    QCOMPARE(m_doc->monitorProperties()->stageTargets().count(), beforeTargets + 1);
    StageTarget *t = m_doc->monitorProperties()->stageTargets().last();
    QCOMPARE(t->name(), QString("Pilot Target"));

    // A PanTilt palette auto-links to the new target.
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

    QTimer::singleShot(0, []() {
        QWidget *modal = QApplication::activeModalWidget();
        QVERIFY(modal != NULL);

        QDialogButtonBox *btns = modal->findChild<QDialogButtonBox*>();
        QVERIFY(btns != NULL);
        QPushButton *cancelBtn = btns->button(QDialogButtonBox::Cancel);
        QVERIFY(cancelBtn != NULL);
        cancelBtn->click();
    });

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

    QTimer::singleShot(0, []() {
        QWidget *modal = QApplication::activeModalWidget();
        QVERIFY(modal != NULL);

        QDialogButtonBox *btns = modal->findChild<QDialogButtonBox*>();
        QVERIFY(btns != NULL);
        QPushButton *cancelBtn = btns->button(QDialogButtonBox::Cancel);
        QVERIFY(cancelBtn != NULL);
        cancelBtn->click();
    });

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
    QTimer::singleShot(0, []() {
        QWidget *modal = QApplication::activeModalWidget();
        QDialogButtonBox *btns = modal->findChild<QDialogButtonBox*>();
        btns->button(QDialogButtonBox::Ok)->click();
    });
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

    QTimer::singleShot(0, []() {
        // NOTE: don't assert on windowTitle() here — QMessageBox renders
        // without a title bar on macOS (a native-alert-style quirk), so it
        // reads back empty even though confirmFeatureDelete() sets one. The
        // box's own text (via QMessageBox::text()) is the reliable check.
        QWidget *modal = QApplication::activeModalWidget();
        QVERIFY(modal != NULL);
        QMessageBox *box = qobject_cast<QMessageBox*>(modal);
        QVERIFY(box != NULL);
        QVERIFY(box->text().contains("Delete"));

        QPushButton *deleteBtn = findButtonByText(modal, "Delete");
        QVERIFY(deleteBtn != NULL);
        deleteBtn->click();
    });

    mon.slotRemoveSelected();

    QCOMPARE(m_doc->monitorProperties()->trusses().count(), before - 1);
}

void Monitor_Test::removeSelectedCancelled()
{
    QWidget parent;
    Monitor mon(&parent, m_doc);

    QTimer::singleShot(0, []() {
        QWidget *modal = QApplication::activeModalWidget();
        QDialogButtonBox *btns = modal->findChild<QDialogButtonBox*>();
        btns->button(QDialogButtonBox::Ok)->click();
    });
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

    QTimer::singleShot(0, []() {
        QWidget *modal = QApplication::activeModalWidget();
        QVERIFY(modal != NULL);
        QPushButton *cancelBtn = findButtonByText(modal, "Cancel");
        QVERIFY(cancelBtn != NULL);
        cancelBtn->click();
    });

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
    const QPointF half(rig.item->boundingRect().width() / 2.0,
                       rig.item->boundingRect().height() / 2.0);
    rig.item->setPos(onTrussPx - half);
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
