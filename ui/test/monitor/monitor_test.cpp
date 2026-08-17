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

    QTimer::singleShot(0, [&mon]() {
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

    QTimer::singleShot(0, [&mon]() {
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

    QTimer::singleShot(0, [&mon]() {
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

    QTimer::singleShot(0, [&mon]() {
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

    QTimer::singleShot(0, [&mon]() {
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
    QTimer::singleShot(0, [&mon]() {
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

    QTimer::singleShot(0, [&mon]() {
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

    QTimer::singleShot(0, [&mon]() {
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

    QTimer::singleShot(0, [&mon]() {
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
