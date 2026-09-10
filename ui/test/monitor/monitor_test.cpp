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
#include <QElapsedTimer>
#include <QTimer>
#include <QApplication>
#include <QLineEdit>
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
#include "fixturevisualtraits.h"
#include <QtMath>
#include "trussitem.h"
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
#include "doc.h"
#include "qlcfile.h"
#include "qlcfixturedefcache.h"
#include <QXmlStreamReader>

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
    rig.item->setPos(rig.item->pos() + (onTrussPx - rig.item->sceneBoundingRect().center()));
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
    /* Position by the item's actual scene CENTRE: boundingRect's origin is
       not (0,0) (it carries a halo/label margin), so "pos minus half the rect"
       misses by that margin -- the product code judges by the centre, so the
       test must place by it too. */
    const QPointF nearPx = rig.gv->realPositionToPixels(12000.0, 12450.0);
    rig.item->setPos(rig.item->pos() + (nearPx - rig.item->sceneBoundingRect().center()));
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
    rig.item->setPos(rig.item->pos() + (onPx - rig.item->sceneBoundingRect().center()));
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
            foreach (quint32 sid, spares)
                doc->deleteFixture(sid);
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
        QMouseEvent press(QEvent::MouseButtonPress, view->w2s(pos()),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        view->mousePressEvent(&press);
        QMouseEvent move(QEvent::MouseMove, px, Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        view->mouseMoveEvent(&move);
        QMouseEvent rel(QEvent::MouseButtonRelease, px,
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
        foreach (QWidget *w, QApplication::topLevelWidgets())
        {
            if (QDialog *d = qobject_cast<QDialog *>(w))
                if ((view = d->findChild<StructureStudioView *>()) != nullptr)
                {
                    view->setLocked(false);
                    const QPointF start =
                        view->w2s(view->m_doc->monitorProperties()->fixtureRigPosition(fid));
                    const QPointF end = start + QPointF(0.0, 40.0);
                    QMouseEvent press(QEvent::MouseButtonPress, start, Qt::LeftButton,
                                      Qt::LeftButton, Qt::NoModifier);
                    QMouseEvent move(QEvent::MouseMove, end, Qt::NoButton,
                                     Qt::LeftButton, Qt::NoModifier);
                    QMouseEvent rel(QEvent::MouseButtonRelease, end, Qt::LeftButton,
                                    Qt::NoButton, Qt::NoModifier);
                    // The release is the one that emits fixtureMoved().
                    view->mousePressEvent(&press);
                    view->mouseMoveEvent(&move);
                    view->mouseReleaseEvent(&rel);
                    d->reject();
                    return;
                }
        }
        QFAIL("no StructureStudioView in any open dialog");
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
    QMouseEvent press(QEvent::MouseButtonPress, startPx, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QMouseEvent move(QEvent::MouseMove, endPx, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent rel(QEvent::MouseButtonRelease, endPx, Qt::LeftButton,
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
    QVERIFY(rigItem->isVisible());

    gv->setMountedFixturesVisible(false);
    QVERIFY2(rigItem->isVisible() == false, "the truss-mounted fixture stayed visible");
    QVERIFY2(freeItem->isVisible(), "a free-standing fixture must never be hidden");

    gv->setMountedFixturesVisible(true);
    QVERIFY2(rigItem->isVisible(), "the mounted fixture did not come back");

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
       readable from saturation alone. */
    const QPointF at = ov.w2s(QVector3D(3.0f, 3.7f, 2.0f));
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

    const QPointF at = v.w2s(props->fixtureRigPosition(fxi->id()));

    auto meanLuma = [&]() {
        const QImage img = v.grab().toImage();
        double sum = 0.0;
        int n = 0;
        for (int dy = -8; dy <= 8; ++dy)
        {
            for (int dx = -8; dx <= 8; ++dx)
            {
                const QPoint p(at.toPoint() + QPoint(dx, dy));
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

    QMouseEvent press(QEvent::MouseButtonPress, empty, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QMouseEvent move(QEvent::MouseMove, empty + QPointF(60, 40), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QMouseEvent rel(QEvent::MouseButtonRelease, empty + QPointF(60, 40),
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

    StructureStudioView v(m_doc, StructureStudioView::StageKind, 0);
    v.resize(900, 620);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(20.0, 30.0);

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
                                "of 64 — sub-pixel LEDs are not being culled")
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
