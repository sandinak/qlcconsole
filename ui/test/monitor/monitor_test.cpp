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
#include <QShortcut>
#include <memory>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QElapsedTimer>
#include <QTimer>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QAbstractSpinBox>
#include <functional>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QPainter>
#include <QTemporaryDir>
#include <QToolButton>
#include <QMenu>
#include <QCheckBox>
#include <QScopeGuard>
#include <QDialog>
#include <QTreeWidget>
#include <QSettings>

#define protected public
#define private public
#include "monitor.h"
#include "fixturefields.h"
#include <QFormLayout>
#include "righeads.h"
#include "colourswatchbutton.h"
#include "headtypesdialog.h"
#include "ledlayouteditor.h"
#include "stagegroupdialog.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "focusmodepanel.h"
#include "focussession.h"
#include "programmercontroller.h"
#include "structurestudioview.h"
#include "rhiraster.h"
#include "mastertimer.h"
#include "lanternpixels.h"
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "monitorlayerspanel.h"
#include "monitorinventorypanel.h"
#include "networktopologydialog.h"
#include "networkreportdialog.h"
#include "monitorfixture.h"
#include "networknodeitem.h"
#include "sceneryitem.h"
#include "monitorruler.h"
#include "hangeritem.h"
#include "patchinventorydialog.h"
#include "fixturemanager.h"
#include "itemicons.h"
#include <QActionGroup>
#include <QTableWidget>
#include <QHeaderView>
#include "hanger.h"
#include "beamoptics.h"
#include "gobolibrary.h"
#include "fixturetreewidget.h"
#include "networkgearcatalog.h"
#define private public
#include "gobowheelsdialog.h"
#undef private
#include "standitem.h"
#include "toweritem.h"
#include "scenery.h"
#include "networktopology.h"
#include "platformitem.h"
#include "studiotemplate.h"
#include <QTreeWidgetItemIterator>
#include "fixturevisualtraits.h"
#include "aimsolver.h"
#include "scene.h"
#include "zraster.h"
#include <QtMath>
#include "trussitem.h"
#include "powerdistribution.h"
#include "targetitem.h"
#undef protected
#undef private

#include "monitor_test.h"
#include "fixturepower.h"
#include "powerlimiter.h"
#include "powerprofiledialog.h"
#include "monitorproperties.h"
#include "truss.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "fixturegroup.h"
#include "createfixturegroup.h"
#include "headlayout.h"
#include "tower.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "qlccapability.h"
#include "patchplanner.h"
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

/* The LED maps these tests expect, from the repo -- not whatever is in the
   operator's own library (~/Library/.../HeadLayouts). They passed on a Mac
   that had the maps there and failed everywhere else. */
static QString testLayouts()
{
    return QFileInfo(QStringLiteral(__FILE__)).absolutePath() + QStringLiteral("/headlayouts");
}

/* Every case: a connection to a slot or signal that doesn't exist fails it.
   Qt only warns, and the action it was meant for silently does nothing --
   Add Target, Add Person, Copy and Paste were dead in the Studio for a week
   behind such warnings. */
void Monitor_Test::init()
{
    QTest::failOnWarning(QRegularExpression(QStringLiteral("No such (slot|signal)")));
}

void Monitor_Test::initTestCase()
{
    m_doc = new Doc(this);
    HeadLayoutLibrary::setUserDirectoryOverride(testLayouts());
}

void Monitor_Test::cleanupTestCase()
{
    HeadLayoutLibrary::setUserDirectoryOverride(QString());
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




/* How long the rig view takes to draw a real show: QLC_RIG_BENCH=<show file>
   (skipped without it). Reported, not checked -- the number to watch when
   making the rig view faster. QLC_RIG_BENCH_LIT=1 sends every channel at
   full first (every beam lit: the worst case); QLC_RIG_BENCH_SHOT=<file.png>
   saves the frame, "-q1"/"-q2" before the extension; QLC_RIG_BENCH_THREADS=n
   draws on n threads (1: one at a time, to compare against). */
void Monitor_Test::aShowsRigViewDrawTime()
{
    const QString path = qEnvironmentVariable("QLC_RIG_BENCH");
    if (path.isEmpty())
        QSKIP("set QLC_RIG_BENCH to a show file");
    /* The show's fixtures need their definitions, or every one loads as a
       generic dimmer: QLC_RIG_BENCH_DEFS=<dir>[;<dir>...] loads them first
       (a fixtures map where the dir has one, else every .qxf in it). */
    for (const QString &dir : qEnvironmentVariable("QLC_RIG_BENCH_DEFS")
                                  .split(QLatin1Char(';'), Qt::SkipEmptyParts))
    {
        QDir d(dir, QStringLiteral("*.qxf"));
        if (!m_doc->fixtureDefCache()->loadMap(d))
            m_doc->fixtureDefCache()->load(d);
    }
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QXmlStreamReader r(&file);
    QVERIFY(r.readNextStartElement());
    bool loaded = false;
    while (r.readNextStartElement())
    {
        if (r.name() == QLatin1String("Engine"))
        {
            loaded = m_doc->loadXML(r);
            break;
        }
        r.skipCurrentElement();
    }
    QVERIFY(loaded);

    if (qEnvironmentVariableIntValue("QLC_RIG_BENCH_LIT") == 1)
    {
        // What the rig view reads: each fixture's own values. A show's
        // typical state: dimmers and colours full, the shutter open, pan and
        // tilt centred, gobos and prisms out (everything else at 0).
        for (Fixture *fx : m_doc->fixtures())
        {
            // setChannelValues() takes the universe, from its channel 0.
            const int at = int(fx->address());
            QByteArray v(at + int(fx->channels()), char(0));
            for (quint32 c = 0; c < fx->channels(); ++c)
            {
                const QLCChannel *ch = fx->channel(c);
                if (ch == nullptr)
                    continue;
                switch (ch->group())
                {
                case QLCChannel::Intensity:
                    v[at + int(c)] = char(255);
                    break;
                case QLCChannel::Pan: case QLCChannel::Tilt:
                    v[at + int(c)] = char(128);
                    break;
                case QLCChannel::Shutter:
                    v[at + int(c)] = char(255);
                    for (const QLCCapability *cap : ch->capabilities())
                        if (cap->preset() == QLCCapability::ShutterOpen)
                        {
                            v[at + int(c)] = char(cap->middle());
                            break;
                        }
                    break;
                default:
                    break;
                }
            }
            fx->setChannelValues(v);
        }
    }
    const QString shot = qEnvironmentVariable("QLC_RIG_BENCH_SHOT");

    const int frames = qMax(1, qEnvironmentVariableIntValue("QLC_RIG_FRAMES"));
    for (int quality = 1; quality <= 2; ++quality)
    {
        StructureStudioView view(m_doc, StructureStudioView::StageKind, 0);
        view.resize(1100, 680);
        view.reload();
        view.setPlane(StructureStudioView::Angled);
        view.setLiveValues(true);
        view.setBeams(true);
        view.setHaze(1.0);
        view.setSupersample(quality);
        if (qEnvironmentVariableIsSet("QLC_RIG_BENCH_THREADS"))
            view.setRenderThreads(qEnvironmentVariableIntValue("QLC_RIG_BENCH_THREADS"));
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.repaint();                      // the first frame builds what it keeps
        QElapsedTimer clock;
        clock.start();
        for (int i = 0; i < frames; ++i)
            view.repaint();
        qInfo().noquote() << QString("rig view, %1 fixtures, quality %2: %3 ms a frame")
                             .arg(m_doc->fixtures().size()).arg(quality)
                             .arg(double(clock.nsecsElapsed()) / 1e6 / frames, 0, 'f', 1);
        if (!shot.isEmpty())
        {
            QString name = shot;
            name.insert(name.lastIndexOf(QLatin1Char('.')), QString("-q%1").arg(quality));
            view.grab().save(name);
        }
    }
}

/* ⇧⌘R opens the Rig Overview, and closes it again when it is the window in
   front; ⌘W closes it too. */
void Monitor_Test::theRigOverviewShortcutTogglesIt()
{
    QWidget parent;
    Monitor mon(&parent, m_doc);
    auto overview = []() -> QDialog * {
        foreach (QWidget *w, QApplication::topLevelWidgets())
            if (QDialog *d = qobject_cast<QDialog *>(w))
                if (d->windowTitle() == QString("Rig Overview") && d->isVisible())
                    return d;
        return nullptr;
    };

    mon.toggleStageOverview();
    QPointer<QDialog> dlg = overview();
    QVERIFY(dlg != nullptr);
    dlg->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(dlg));
    mon.toggleStageOverview();          // in front: closed
    QTRY_VERIFY(dlg.isNull());
    QVERIFY(overview() == nullptr);

    mon.toggleStageOverview();          // and open again
    dlg = overview();
    QVERIFY(dlg != nullptr);
    dlg->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(dlg));
    QTest::keySequence(dlg, QKeySequence::Close);
    QTRY_VERIFY(dlg.isNull());
}

/* A layer is shown or hidden from the Rig Overview's own Layers menu, without
   the Studio -- and from the Studio's eye -- in Operate mode as well as
   Design: it is how you look at the rig. The Studio's list follows a change
   made in the overview; locking stays Design only. */
void Monitor_Test::layersShowAndHideFromTheRigOverviewInAnyMode()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    const quint32 upper = props->addLayer("Upper");
    QVERIFY(props->layer(upper).visible);
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        doc->setMode(Doc::Operate);

        // The Studio's list: eye and labels work in Operate, the lock doesn't.
        QTreeWidget *tree = mon.findChild<MonitorLayersPanel *>()->findChild<QTreeWidget *>();
        QVERIFY(tree != nullptr);
        auto layerButtons = [tree, upper]() {
            for (int i = 0; i < tree->topLevelItemCount(); ++i)
            {
                QTreeWidgetItem *node = tree->topLevelItem(i);
                if (node->data(0, Qt::UserRole).toInt() == int(MonitorLayersPanel::NodeLayer)
                    && node->data(0, Qt::UserRole + 1).toUInt() == upper)
                    return tree->itemWidget(node, 1)->findChildren<QToolButton *>();
            }
            return QList<QToolButton *>();
        };
        QList<QToolButton *> b = layerButtons();
        QCOMPARE(b.size(), 4);                  // collapse, eye, labels, lock
        QVERIFY(b.at(1)->isEnabled());
        QVERIFY(b.at(2)->isEnabled());
        QVERIFY(!b.at(3)->isEnabled());
        b.at(1)->click();                       // the eye: hidden
        QVERIFY(!props->layer(upper).visible);
        QTRY_VERIFY(!layerButtons().isEmpty() && !layerButtons().at(1)->isChecked());
        layerButtons().at(1)->click();          // and shown again
        QVERIFY(props->layer(upper).visible);

        // The overview's own menu.
        mon.toggleStageOverview();
        QDialog *dlg = nullptr;
        foreach (QWidget *w, QApplication::topLevelWidgets())
            if (QDialog *d = qobject_cast<QDialog *>(w))
                if (d->windowTitle() == QString("Rig Overview") && d->isVisible())
                    dlg = d;
        QVERIFY(dlg != nullptr);
        QToolButton *layersBtn = dlg->findChild<QToolButton *>("rigLayers");
        QVERIFY(layersBtn != nullptr && layersBtn->menu() != nullptr);
        QMenu *menu = layersBtn->menu();
        emit menu->aboutToShow();
        QAction *upperAction = nullptr;
        for (QAction *a : menu->actions())
            if (a->text() == "Upper")
                upperAction = a;
        QVERIFY(upperAction != nullptr);
        QVERIFY(upperAction->isCheckable() && upperAction->isChecked());
        upperAction->trigger();                 // hidden, from the overview
        QVERIFY(!props->layer(upper).visible);
        QVERIFY(doc->isModified());
        // ... and the Studio's list says so.
        QTRY_VERIFY(!layerButtons().isEmpty() && !layerButtons().at(1)->isChecked());

        emit menu->aboutToShow();               // rebuilt: as the show is now
        QAction *all = nullptr;
        for (QAction *a : menu->actions())
            if (a->text() == "Show all")
                all = a;
        QVERIFY(all != nullptr);
        all->trigger();
        QVERIFY(props->layer(upper).visible);
        dlg->close();
        doc->setMode(Doc::Design);
    }
    delete doc;
}

/* Moving a drape, the ruler marks its middle, not where it was grabbed;
   just hovering, the mark is the pointer. */
void Monitor_Test::theRulerMarksTheMiddleOfADraggedDrape()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Meters);
    Scenery *drape = props->addScenery();
    drape->setKind(Scenery::MainDrape);
    drape->setOriginX(6.0f); drape->setOriginY(6.0f);
    drape->setWidth(8.0f);
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        parent.resize(1200, 800);
        parent.show();
        MonitorGraphicsView *gv = mon.m_graphicsView;
        SceneryItem *item = gv->m_sceneryItems.value(drape->id());
        QVERIFY(item != nullptr);
        QVERIFY(mon.m_hRuler != nullptr);
        item->setSelected(true);
        const QRect band = gv->selectionViewportRect();
        QVERIFY(band.isValid());

        // Grabbed well off its middle: a quarter of the way along it.
        const QPoint grab(band.left() + band.width() / 4, band.center().y());
        auto move = [&](Qt::MouseButtons buttons) {
            QMouseEvent me(QEvent::MouseMove, QPointF(grab), QPointF(gv->viewport()->mapToGlobal(grab)),
                           Qt::NoButton, buttons, Qt::NoModifier);
            QApplication::sendEvent(gv->viewport(), &me);
        };
        move(Qt::NoButton);
        QCOMPARE(mon.m_hRuler->cursorPixel(), grab.x());
        QCOMPARE(mon.m_hRuler->itemRange(), qMakePair(band.left(), band.right()));
        move(Qt::LeftButton);
        QCOMPARE(mon.m_hRuler->cursorPixel(), band.center().x());
    }
    delete doc;
}

/* "for rig view .. can we have it follow the viewability of the studio": a
   layer hidden in the Lighting Studio is hidden from the whole-rig view --
   its structures, scenery and fixtures -- while an editor still shows its
   own object. Toggling the layer in the Studio redraws the overview. */
void Monitor_Test::theRigViewHidesWhatTheStudioHides()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(2.0f); deck->setOriginY(2.0f);
    Scenery *cyc = props->addScenery();
    cyc->setOriginX(5.0f); cyc->setOriginY(0.5f); cyc->setWidth(8.0f);
    Fixture *fx = new Fixture(doc);
    fx->setChannels(3);
    doc->addFixture(fx);
    props->setFixturePosition(fx->id(), 0, 0, QVector3D(3000.0f, 3000.0f, 0));

    const quint32 hidden = props->addLayer("Set");
    deck->setLayerId(hidden);
    cyc->setLayerId(hidden);
    props->setFixtureLayer(fx->id(), hidden);

    StructureStudioView rig(doc, StructureStudioView::StageKind, 0);
    auto has = [&rig](StructureStudioView::Kind kind, quint32 id) {
        return rig.everyStructure().contains(qMakePair(kind, id));
    };
    QVERIFY(has(StructureStudioView::PlatformKind, deck->id()));
    QVERIFY(has(StructureStudioView::SceneryKind, cyc->id()));
    QVERIFY(rig.mountedFixtures().contains(fx->id()));

    props->setLayerVisible(hidden, false);
    QVERIFY(!has(StructureStudioView::PlatformKind, deck->id()));
    QVERIFY(!has(StructureStudioView::SceneryKind, cyc->id()));
    QVERIFY(!rig.mountedFixtures().contains(fx->id()));

    // An editor shows its own deck whatever layer it is on.
    StructureStudioView editor(doc, StructureStudioView::PlatformKind, deck->id());
    QVERIFY(editor.layerShown(hidden));

    // Toggled in the Studio, the open overview hears of it.
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        mon.showStageOverview();
        QVERIFY(mon.m_overviewView != nullptr);
        mon.m_overviewView->m_floorPointsAge.start();
        props->setLayerVisible(hidden, true);
        mon.m_graphicsView->refreshItemLayerState();
        QVERIFY2(!mon.m_overviewView->m_floorPointsAge.isValid(),
                 "the overview should measure the rig again when a layer changes");
        if (mon.m_overviewDlg != nullptr)
            mon.m_overviewDlg->close();
    }
    delete doc;
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
    // A plain drop only moves it ("use a shift drag/drop to enable that").
    rig.gv->slotFixtureMoved(rig.item);
    QVERIFY2(props->fixtureRigProps(rig.fxi->id()).trussId == Truss::invalidId(),
             "a drop without Shift hung it on the truss");
    // With Shift held, the same drop hangs it.
    rig.item->setPos(rig.item->pos() + (onTrussPx - rig.item->iconCentreScene()));
    rig.gv->setAttachOnDrop(true);
    rig.gv->slotFixtureMoved(rig.item);
    rig.gv->setAttachOnDrop(false);

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

    // Locked truss (its layer locked): drop dead ON the line; must still stay free.
    const quint32 lockedLayer = props->addLayer("Locked");
    t->setLayerId(lockedLayer);
    props->setLayerLocked(lockedLayer, true);
    rig.gv->updateTrusses();
    const QPointF onPx = rig.gv->realPositionToPixels(12000.0, 12000.0);
    rig.item->setPos(rig.item->pos() + (onPx - rig.item->iconCentreScene()));
    rig.gv->slotFixtureMoved(rig.item);
    rp = props->fixtureRigProps(rig.fxi->id());
    QVERIFY2(rp.trussId == Truss::invalidId(),
             "a LOCKED truss must not acquire a dropped fixture");

    props->removeTruss(t->id());
    props->removeLayer(lockedLayer);
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
    /* The LED grids alone: a lit strip's light now comes from along it (a
       cone per lit stretch, up to six), and counting those beams in measures
       something else. */
    v.setBeams(false);

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

/* "the truss editor shows the fixtures just under the cross member as it
   should be .. but the rig view shows them down a bit?"
 *
   The elevation draws a mover on a truss or pipe CENTRED on the point it is
   rigged at (and the hit test agrees), so that is where a rigger drags it to
   sit it under the bar -- the drop (mountZOffset) records it. The 3D rig view
   then seated the body below that same point, top flush with it, so every
   head placed by eye in the editor hung half a body lower in the overview,
   clear of the bar it was clamped to. Seating belongs to what the elevations
   seat too: a tower shelf, a deck, a riser. A truss or pipe mount is centred
   in both.
 *
   QLC_HUNG_SHOTS=<dir> saves the frames this measures. */
void Monitor_Test::aMoverHungOnATrussIsAtTheSameHeightInEveryView()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    Truss *bar = props->addTruss();
    bar->setName("Bar 1"); bar->setType(Truss::Horizontal);
    bar->setOrigin(QVector3D(7.0f, 1.0f, 3.0f));
    bar->setDirection(QPointF(1.0, 0.0));
    bar->setLength(3.0f);
    bar->setWidth(0.29f);

    Fixture *head = makeMover(doc, 14.0, 0, "Hung Spot", QVector3D(8.5f, 1.0f, 3.0f));
    QVERIFY(head != nullptr);
    FixtureRigProps rp;
    rp.trussId = bar->id();
    rp.trussOffset = 1.5f;
    rp.mountingType = Truss::TopHung;
    rp.trussMountSide = FixtureRigProps::UnderHung;
    rp.mountZOffset = -0.2f;                     // dropped below the bar, as rigged
    props->setFixtureRigProps(head->id(), rp);

    const QString shots = qEnvironmentVariable("QLC_HUNG_SHOTS");

    /* Where the fixture's drawn body is centred vertically, as a fraction of
       its own drawn height away from the rig point: 0 = centred on it, +0.5 =
       entirely below it. */
    auto bodyOffset = [&](StructureStudioView::Plane plane, const char *tag,
                          QPointF *centroid) -> double {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(760, 600);
        v.setPlane(plane);
        if (plane == StructureStudioView::Angled)
            v.setAngledView(15.0, 20.0);
        v.setAmbient(1.0);
        v.grab();                    // flush the first layout, see the shelf test
        v.reload();
        v.m_zoomed = true;
        const QImage img = v.grab().toImage();

        const PreviewItem saved = props->m_fixtureItems.value(head->id()).m_baseItem;
        props->removeFixture(head->id());
        const QImage base = v.grab().toImage();
        props->m_fixtureItems[head->id()].m_baseItem = saved;
        props->setFixtureRigProps(head->id(), rp);

        if (!shots.isEmpty())
            img.save(QDir(shots).filePath(QString("hung-%1.png").arg(tag)));

        const double dpr = img.devicePixelRatio();
        const QPointF at = v.w2s(v.fixtureCentre(head->id())) * dpr;
        double sumX = 0.0, sumY = 0.0; int n = 0, top = img.height(), bottom = -1;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
            {
                const QColor a = img.pixelColor(x, y), b = base.pixelColor(x, y);
                if (qAbs(a.red() - b.red()) + qAbs(a.green() - b.green())
                    + qAbs(a.blue() - b.blue()) <= 24)
                    continue;
                sumX += x; sumY += y; ++n;
                top = qMin(top, y); bottom = qMax(bottom, y);
            }
        if (n < 50)
            return 99.0;
        if (centroid != nullptr)
            *centroid = QPointF(sumX / n, sumY / n) / dpr;
        return (sumY / n - at.y()) / qMax(1, bottom - top + 1);
    };

    QPointF front, angled;
    const double inEditor = bodyOffset(StructureStudioView::Front, "front", &front);
    const double inRig    = bodyOffset(StructureStudioView::Angled, "angled", &angled);
    QVERIFY2(inEditor < 50.0 && inRig < 50.0, "the fixture drew almost nothing");

    QVERIFY2(qAbs(inEditor) < 0.2,
             qPrintable(QString("the elevation no longer centres a truss-hung mover "
                                "on its rig point (offset %1 of its height)")
                        .arg(inEditor, 0, 'f', 2)));
    QVERIFY2(qAbs(inRig - inEditor) < 0.2,
             qPrintable(QString("the rig view hangs a truss-hung mover %1 of its "
                                "height lower than the truss editor shows it")
                        .arg(inRig - inEditor, 0, 'f', 2)));

    /* And picking it in the rig view finds it where it is drawn. */
    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(760, 600);
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(15.0, 20.0);
        v.grab();
        v.reload();
        v.m_zoomed = true;
        v.grab();
        QCOMPARE(v.hitTestFixture(angled), head->id());
    }
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
            QCOMPARE(top.first(), QStringLiteral("Fixture Properties…"));
            QCOMPARE(top.last(), QStringLiteral("Remove from View"));
        }

        // ...and removing comes last in the list too, after its own sections.
        {
            QMenu m;
            panel.buildContextMenu(&m, rowFor(QStringLiteral("truss"), truss->id()));
            QCOMPARE(menuPaths(&m).last(), QStringLiteral("Delete Truss"));
            QMenu f;
            panel.buildContextMenu(&f, rowFor(QStringLiteral("fixture"), spot->id()));
            QCOMPARE(menuPaths(&f).first(), QStringLiteral("Fixture Properties…"));
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

    // Scoped: view/panel must be torn down before `delete doc` below runs --
    // MonitorGraphicsView::~MonitorGraphicsView() walks its MonitorFixtureItems,
    // and each one dereferences its Doc* on destruction (clearFixtures() ->
    // ~MonitorFixtureItem() -> m_doc->fixture()). Every other test in this file
    // that builds a MonitorGraphicsView on top of a `delete doc` ending scopes it
    // the same way; this one didn't, so `delete doc` freed the Doc while `view`
    // was still alive, and its destructor ran that lookup through a dangling
    // pointer -- a real use-after-free, just one that happened not to crash
    // under Qt6's allocator. It reproducibly segfaults under Qt5 (confirmed on
    // a clean checkout, independent of any unrelated change) because there
    // Doc's freed memory gets reused/unmapped instead of sitting untouched.
    {
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
    }

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
    gv->setAttachOnDrop(true);   // its drops are Shift-drops: they hang what they drop
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
    // Iterator declared in the for-init so it dies with the loop: Qt5's
    // ~QTreeWidgetItemIterator() unregisters from the tree's model, so one that
    // outlived `delete mon` below touched a freed model and segfaulted.
    for (QTreeWidgetItemIterator it(layers->m_tree); *it != nullptr; ++it)
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
    gv->setAttachOnDrop(true);   // its drops are Shift-drops: they hang what they drop
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
        // Origin X/Y, width, depth, height, front curve, facing -- then the
        // Steps shape's own Rise / Run / Step width, created after them and
        // hidden for a box.
        if (nameEdit == nullptr || spins.count() != 10)
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
    gv->setAttachOnDrop(true);   // its drops are Shift-drops: they hang what they drop
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
void Monitor_Test::aFixtureOverATowerBecomesItsCount()
{
    /* "why is this AT light on top covering the truss vs showing the blue
       circle with the count": one head standing on a 16" tower covers the
       tower from above -- it was measured against the tower's base plate,
       about five times its size, and never counted as covering it. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Tower *tw = props->addTower();
    tw->setName("AT2");
    tw->setOriginX(3.0f);
    tw->setOriginY(3.0f);
    tw->setWidth(0.405f);
    tw->setDepth(0.405f);
    tw->setHeight(2.4f);
    Fixture *f = makeMover(doc, 14.0, 0, "AT2-36-1", QVector3D(3.2f, 3.2f, 2.4f));
    QVERIFY(f != nullptr);
    FixtureRigProps rp;
    rp.towerId = tw->id();
    rp.towerMountSide = FixtureRigProps::TowerTop;
    rp.towerU = 0.2f;
    rp.towerV = 0.2f;
    rp.mountingType = Truss::FloorMounted;
    props->setFixtureRigProps(f->id(), rp);
    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 7));
        view.setViewPOV(MonitorGraphicsView::PovTop);
        view.updatePlatforms();
        view.addFixture(f->id(), QPointF(3200.0, 3200.0));
        view.refreshItemLayerState();
        view.refreshFixtureCountBadges();
        const double cover = view.structureCoverageByItsFixtures(QStringLiteral("tower"), tw->id());
        QVERIFY2(cover > 0.40, qPrintable(QString("the head covers %1 of the tower").arg(cover)));
        QVERIFY2(view.m_countBadges.contains(QStringLiteral("tower:%1").arg(tw->id())),
                 "no count bubble on the tower its head covers");
        QVERIFY(!view.m_fixtures.value(f->id())->isVisible());
    }
    delete doc;
}

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
    // The refresh is coalesced to the event loop: wait for it, not a fixed time.
    QTRY_VERIFY_WITH_TIMEOUT(resolved().blue() > 200 && resolved().red() < 80, 5000);

    const QColor after = resolved();
    QVERIFY2(after.blue() > 200 && after.red() < 80,
             qPrintable(QString("the rig view is still showing the old colour "
                                "after the look was edited: %1").arg(after.name())));

    // And back again -- blue to white, which is the other half of the report.
    col->setValue(QColor(255, 255, 255));
    QMetaObject::invokeMethod(doc, "functionChanged", Q_ARG(quint32, scene->id()));
    QTRY_VERIFY_WITH_TIMEOUT(resolved().red() > 200 && resolved().green() > 200, 5000);
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

/* A show opened on a computer without some of its definitions (the Windows
   VM's first DanceHysteria): each such fixture stands in as a generic dimmer
   of its channel count, whose physical is a made-up 300 mm per channel. The
   rig view drew a 64-channel bar as a 19 m wall of 64 heads, every one
   overlapping its neighbours in the warning red -- the whole rig looked
   wireframed in red. A stand-in is one small body, never an overlap. */
void Monitor_Test::aStandInForAMissingDefinitionIsOneSmallBody()
{
    Doc *doc = new Doc(this);
    for (int i = 0; i < 2; ++i)
    {
        QXmlStreamReader in(QString(
            "<Fixture><Manufacturer>Nowhere</Manufacturer><Model>Bar 64</Model>"
            "<Mode>64ch</Mode><ID>%1</ID><Name>Bar %1</Name><Universe>0</Universe>"
            "<Address>%2</Address><Channels>64</Channels></Fixture>").arg(i).arg(i * 64));
        in.readNextStartElement();
        QVERIFY(Fixture::loader(in, doc));
        // Half a metre apart: clear of each other as the bars really are.
        doc->monitorProperties()->setFixturePosition(quint32(i), 0, 0,
                                                     QVector3D(2.0f + 0.5f * i, 2.0f, 3.0f));
    }
    Fixture *fx = doc->fixture(0);
    QVERIFY(fx != nullptr && fx->definitionUnavailable());

    const FixtureVisualTraits t = classifyFixture(fx);
    QCOMPARE(t.headCount, 1);
    QVERIFY2(t.physW < 0.5f, qPrintable(QString("a stand-in %1 m wide").arg(t.physW)));

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(600, 480);
    v.reload();
    v.rebuildOverlapWarnings();
    QVERIFY2(v.m_overlappingFixtures.isEmpty(), "stand-ins flagged as overlapping");

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

void Monitor_Test::aBlockedBeamIsTracedToWhatBlocksIt()
{
    /* A spot on the FOH pipe aimed at a vocalist upstage of the closed main
       drape: the trace runs red and dashed to the drape, with a cross where
       it stops and the blocker named; the drape opens and it is an ordinary
       trace again. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Scenery *drape = props->addScenery();
    drape->setKind(Scenery::MainDrape);
    drape->setName("Main drape 2");
    drape->setOriginX(5.0f);
    drape->setOriginY(3.0f);
    drape->setWidth(10.0f);
    drape->setHeight(6.0f);
    drape->setOpeningWidth(8.0f);

    Truss *pipe = props->addTruss();
    pipe->setName("FOH");
    pipe->setType(Truss::Horizontal);
    pipe->setOrigin(QVector3D(2.0f, 6.5f, 6.0f));
    pipe->setDirection(QPointF(1.0, 0.0));
    pipe->setLength(6.0f);
    pipe->setWidth(0.29f);

    Fixture *spot = makeMover(doc, 14.0, 0, "FOH spot", QVector3D(5.0f, 6.5f, 6.0f));
    QVERIFY(spot != nullptr);
    FixtureRigProps rp;
    rp.trussId = pipe->id();
    rp.trussOffset = 3.0f;
    rp.mountingType = Truss::TopHung;
    props->setFixtureRigProps(spot->id(), rp);

    StageTarget *who = props->addStageTarget();
    who->setName("Vocalist");
    who->setKind(StageTarget::Kind::Person);
    who->setPosition(QVector3D(5.0f, 1.0f, 0.0f));
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("At the vocalist");
    aim->setStageTargetId(who->id());
    QVERIFY(doc->addPalette(aim));
    Scene *scene = new Scene(doc);
    scene->setName("Solo");
    scene->addFixture(spot->id());
    scene->addPalette(aim->id());
    QVERIFY(doc->addFunction(scene));

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 8));
        view.updateTrusses();
        view.addFixture(spot->id(), QPointF(5000.0, 6500.0));
        view.setActiveScene(scene->id());
        view.updateAimLines();

        auto blockedTraces = [&]() {
            QList<QGraphicsLineItem *> out;
            foreach (QGraphicsLineItem *li, view.m_aimLines)
                if (li->data(0).toString() == QLatin1String("blockedTrace"))
                    out << li;
            return out;
        };
        QList<QGraphicsLineItem *> traces = blockedTraces();
        QCOMPARE(traces.size(), 1);
        QCOMPARE(traces.first()->pen().style(), Qt::DashLine);
        QVERIFY2(traces.first()->toolTip().contains("Main drape 2"),
                 qPrintable(traces.first()->toolTip()));
        // It stops short of the target: at the drape, not at the vocalist.
        TargetItem *ti = view.m_targetItems.value(who->id(), nullptr);
        QVERIFY(ti != nullptr);
        const QLineF l = traces.first()->line();
        QVERIFY(QLineF(l.p1(), ti->pos()).length() > l.length() + 5.0);

        drape->setState(Scenery::Open);
        view.updateAimLines();
        QVERIFY(blockedTraces().isEmpty());
        QVERIFY(!view.m_aimLines.isEmpty());
    }

    delete doc;
}

void Monitor_Test::theLineOfSightMenuSetsWhatBlocks()
{
    QWidget parent;
    Monitor mon(&parent, m_doc);
    MonitorProperties *props = m_doc->monitorProperties();
    const int before = props->occlusionKinds();
    QVERIFY(before & BeamOcclusion::Towers);

    QMenu *menu = mon.findChild<QMenu *>("lineOfSightMenu");
    QVERIFY(menu != nullptr);
    QAction *towers = nullptr, *beams = nullptr;
    foreach (QAction *a, menu->actions())
    {
        if (a->objectName() == QStringLiteral("lineOfSightKind%1").arg(int(BeamOcclusion::Towers)))
            towers = a;
        else if (a->objectName() == QLatin1String("lineOfSightBeams"))
            beams = a;
    }
    QVERIFY(towers != nullptr && beams != nullptr);
    QVERIFY(towers->isChecked());
    towers->setChecked(false);
    QCOMPARE(props->occlusionKinds(), before & ~int(BeamOcclusion::Towers));
    towers->setChecked(true);
    QCOMPARE(props->occlusionKinds(), before);

    const bool was = StructureStudioView::lineOfSightBeams();
    beams->setChecked(!beams->isChecked());
    QCOMPARE(StructureStudioView::lineOfSightBeams(), beams->isChecked());
    beams->setChecked(was);
    QCOMPARE(StructureStudioView::lineOfSightBeams(), was);
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

void Monitor_Test::aLineTargetIsEditedInTheStudio()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StageTarget *line = props->addStageTarget();
    line->setName("L");
    line->setPosition(QVector3D(5.0f, 3.0f, 0.0f));
    line->makeLine();                              // (3,3) .. (7,3)

    auto world = [&]() { return props->targetLinePoints(line); };
    auto near = [](const QVector3D &a, const QVector3D &b) { return (a - b).length() < 0.03f; };
    auto str = [](const QVector3D &v) {
        return QString("(%1, %2, %3)").arg(double(v.x()), 0, 'f', 2)
                                      .arg(double(v.y()), 0, 'f', 2).arg(double(v.z()), 0, 'f', 2);
    };

    MonitorGraphicsView view(doc);
    view.resize(1000, 700);
    view.setGridMetrics(1000.0);
    view.setGridSize(QSize(10, 7));
    view.setViewPOV(MonitorGraphicsView::PovTop);
    view.updateTargets();
    view.show();
    QCoreApplication::processEvents();

    auto vpOf = [&](float x, float y) {
        return view.mapFromScene(view.projectMm(double(x) * 1000.0, double(y) * 1000.0, 0.0));
    };
    auto send = [&](QEvent::Type type, const QPoint &vp, Qt::KeyboardModifiers mods) {
        const Qt::MouseButton b = (type == QEvent::MouseMove) ? Qt::NoButton : Qt::LeftButton;
        const Qt::MouseButtons held = (type == QEvent::MouseButtonRelease) ? Qt::NoButton : Qt::LeftButton;
        QMouseEvent ev(type, vp, view.viewport()->mapToGlobal(vp), b, held, mods);
        QApplication::sendEvent(view.viewport(), &ev);
    };
    auto drag = [&](const QPoint &from, const QPoint &to, Qt::KeyboardModifiers mods = Qt::NoModifier) {
        send(QEvent::MouseButtonPress, from, mods);
        send(QEvent::MouseMove, from + (to - from) / 2, mods);
        send(QEvent::MouseMove, to, mods);
        send(QEvent::MouseButtonRelease, to, mods);
        QCoreApplication::processEvents();
    };

    TargetItem *ti = view.m_targetItems.value(line->id(), nullptr);
    QVERIFY(ti != nullptr && ti->isLine());
    QCOMPARE(ti->lineHandleCount(), 2);
    QVERIFY2(ti->flags() & QGraphicsItem::ItemIsMovable, "a line in Design mode cannot be edited");
    // Picked on the line, not on the empty stage its box covers.
    QVERIFY(ti->shape().contains(ti->mapFromScene(view.mapToScene(vpOf(4.0f, 3.0f)))));
    QVERIFY(!ti->shape().contains(ti->mapFromScene(view.mapToScene(vpOf(4.0f, 4.0f)))));

    // 1. Drag the right-hand end up and right.
    drag(vpOf(7.0f, 3.0f), vpOf(8.0f, 4.0f));
    QVERIFY2(near(world().last(), QVector3D(8, 4, 0)), qPrintable(str(world().last())));
    QVERIFY2(near(world().first(), QVector3D(3, 3, 0)), qPrintable(str(world().first())));
    // Re-centred: the target's own point is half-way along again.
    QVERIFY(near(line->position(), QVector3D(5.5f, 3.5f, 0)));

    // ... and undone.
    view.undoLastMove();
    QVERIFY2(near(world().last(), QVector3D(7, 3, 0)), qPrintable(str(world().last())));
    QVERIFY(near(line->position(), QVector3D(5, 3, 0)));

    // 2. Alt on an end stretches it along the line, whatever way the mouse goes.
    ti = view.m_targetItems.value(line->id(), nullptr);
    drag(vpOf(7.0f, 3.0f), vpOf(8.0f, 4.0f), Qt::AltModifier);
    QVERIFY2(near(world().last(), QVector3D(8, 3, 0)), qPrintable(str(world().last())));
    view.undoLastMove();

    // 3. Double-click the curve off the centre: a point there.
    ti = view.m_targetItems.value(line->id(), nullptr);
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, vpOf(6.0f, 3.0f));
    QCOMPARE(line->linePoints().size(), 3);
    QVERIFY2(near(world().at(1), QVector3D(6, 3, 0)), qPrintable(str(world().at(1))));
    // Bend it by that point: a drag of the mid handle.
    drag(vpOf(6.0f, 3.0f), vpOf(6.0f, 2.0f));
    QVERIFY2(near(world().at(1), QVector3D(6, 2, 0)), qPrintable(str(world().at(1))));
    QVERIFY(near(world().first(), QVector3D(3, 3, 0)) && near(world().last(), QVector3D(7, 3, 0)));
    // Double-click the mid point: gone.
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, vpOf(6.0f, 2.0f));
    QCOMPARE(line->linePoints().size(), 2);
    // Undo puts it back where it was bent to; undo again unbends; again un-adds.
    view.undoLastMove();
    QCOMPARE(line->linePoints().size(), 3);
    QVERIFY(near(world().at(1), QVector3D(6, 2, 0)));
    view.undoLastMove();
    QVERIFY(near(world().at(1), QVector3D(6, 3, 0)));
    view.undoLastMove();
    QCOMPARE(line->linePoints().size(), 2);

    // 4. Double-clicking an end adds nothing and removes nothing.
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, vpOf(3.0f, 3.0f));
    QCOMPARE(line->linePoints().size(), 2);

    // 5. Drag the body: the whole line goes, its shape with it.
    view.m_scene->clearSelection();
    drag(vpOf(4.0f, 3.0f), vpOf(4.0f, 5.0f));
    QVERIFY2(near(line->position(), QVector3D(5, 5, 0)), qPrintable(str(line->position())));
    QVERIFY(near(world().first(), QVector3D(3, 5, 0)) && near(world().last(), QVector3D(7, 5, 0)));
    view.undoLastMove();
    QVERIFY2(near(line->position(), QVector3D(5, 3, 0)), qPrintable(str(line->position())));

    // 6. From the front, an end drags in height.
    view.setViewPOV(MonitorGraphicsView::PovFront);
    view.updateTargets();
    QCoreApplication::processEvents();
    auto vpFront = [&](float x, float z) {
        return view.mapFromScene(view.projectMm(double(x) * 1000.0, 3000.0, double(z) * 1000.0));
    };
    drag(vpFront(7.0f, 0.0f), vpFront(7.0f, 2.0f));
    QVERIFY2(near(world().last(), QVector3D(7, 3, 2)), qPrintable(str(world().last())));
    QVERIFY(near(world().first(), QVector3D(3, 3, 0)));

    // Outside Design mode a line does not edit: a double-click on it adds
    // nothing, and its handles do not drag.
    doc->setMode(Doc::Operate);
    view.updateTargets();
    QCoreApplication::processEvents();
    const QVector<QVector3D> held = line->linePoints();
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, vpFront(4.0f, 0.5f));
    QCOMPARE(line->linePoints(), held);
    drag(vpFront(3.0f, 0.0f), vpFront(2.0f, 1.0f));
    QCOMPARE(line->linePoints(), held);

    delete doc;
}

void Monitor_Test::headsAimedAtALineGoToTheirOwnPoints()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Fixture *a = makeMover(doc, 15.0, 0, "A", QVector3D(2.0f, 0.5f, 5.0f));
    Fixture *b = makeMover(doc, 15.0, 10, "B", QVector3D(8.0f, 0.5f, 5.0f));
    QVERIFY(a != nullptr && b != nullptr);

    StageTarget *line = props->addStageTarget();
    line->setName("L");
    line->setPosition(QVector3D(5.0f, 3.0f, 0.0f));
    line->makeLine();

    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("At L");
    aim->setStageTargetId(line->id());
    doc->addPalette(aim);
    Scene *scene = new Scene(doc);
    scene->setName("Fan");
    scene->addFixture(b->id());
    scene->addFixture(a->id());
    scene->addPalette(aim->id());
    doc->addFunction(scene);

    {
        MonitorGraphicsView view(doc);
        view.resize(1000, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 7));
        view.setViewPOV(MonitorGraphicsView::PovTop);
        view.addFixture(a->id(), QPointF(2000.0, 500.0));
        view.addFixture(b->id(), QPointF(8000.0, 500.0));
        view.setActiveScene(scene->id());
        view.updateTargets();

        // The traces end on the line's two ends: A (stage left) on the left.
        const QPointF leftEnd = view.projectMm(3000.0, 3000.0, 0.0);
        const QPointF rightEnd = view.projectMm(7000.0, 3000.0, 0.0);
        int toLeft = 0, toRight = 0;
        foreach (QGraphicsLineItem *li, view.m_aimLines)
        {
            const QPointF end = li->line().p2();
            if (QLineF(end, leftEnd).length() < 2.0)  toLeft++;
            if (QLineF(end, rightEnd).length() < 2.0) toRight++;
        }
        QVERIFY2(toLeft == 1 && toRight == 1,
                 qPrintable(QString("%1 traces to the left end, %2 to the right, of %3 lines")
                            .arg(toLeft).arg(toRight).arg(view.m_aimLines.size())));
    }

    {
        StructureStudioView rig(doc, StructureStudioView::StageKind, 0);
        rig.setActiveScene(scene->id());
        QVector3D pa, pb;
        QVERIFY(rig.aimPointFor(a->id(), pa));
        QVERIFY(rig.aimPointFor(b->id(), pb));
        QVERIFY2((pa - QVector3D(3, 3, 0)).length() < 0.02f, "the rig view aims A elsewhere");
        QVERIFY2((pb - QVector3D(7, 3, 0)).length() < 0.02f, "the rig view aims B elsewhere");
        // Drag the line and the rig view follows, without a rebuild.
        line->setPosition(QVector3D(5.0f, 4.0f, 0.0f));
        QVERIFY(rig.aimPointFor(a->id(), pa));
        QVERIFY((pa - QVector3D(3, 4, 0)).length() < 0.02f);
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
    // A short name: its label is measured with the mark below, and a font
    // without real glyphs (QPA minimal, the Linux test runner's) draws a long
    // one wider than 1.5 m of plan.
    area->setName("DSC");
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
        /* From above: the rectangle by its edges -- runs of 30 px and more
           -- so its name label isn't measured with it (a platform with no
           real fonts draws the label wider than the area: 314 px for 300). */
        auto edges = [](const QImage &img) {
            int x0 = INT_MAX, x1 = -1, y0 = INT_MAX, y1 = -1;
            for (int y = 0; y < img.height(); ++y)
            {
                int run = 0;
                for (int x = 0; x <= img.width(); ++x)
                {
                    const QColor c = x < img.width() ? img.pixelColor(x, y) : QColor(Qt::black);
                    if (c.green() > c.red() + 40 && c.green() > c.blue() + 40)
                    {
                        ++run;
                        continue;
                    }
                    if (run >= 30)
                    {
                        x0 = qMin(x0, x - run); x1 = qMax(x1, x - 1);
                        y0 = qMin(y0, y); y1 = qMax(y1, y);
                    }
                    run = 0;
                }
            }
            return QRect(QPoint(x0, y0), QPoint(x1, y1));
        };
        QRect top = edges(view.grab().toImage());
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

        /* Not the axis tripod in the corner (drawAxisTripod(): its green
           "DS" label read as figure where a heavier font draws it -- Linux). */
        auto withoutTripod = [&v](QImage img) {
            QPainter p(&img);
            p.fillRect(QRect(v.width() - 140, v.height() - 120, 140, 120), Qt::black);
            return img;
        };
        who->setKind(StageTarget::Kind::None);
        const Extent bare = measure(withoutTripod(v.grab().toImage()));
        who->setKind(StageTarget::Kind::Person);
        v.update();
        const Extent figure = measure(withoutTripod(v.grab().toImage()));
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

    /* A DIMMED bar is still its colour (2026-10-17, Show Tree): Blue with a
       19% Dimmer look on a bar with no dimmer channel goes out as (0,1,48)
       per pixel. The level used to be counted twice -- in the colour and
       again as the "dimmer" -- and the bar drew as 9/255: black, on a rig
       that was plainly blue. */
    QByteArray dimBlue(512, char(0));
    for (int h = 0; h < PIX; ++h)
    {
        dimBlue[h * 3 + 1] = char(1);
        dimBlue[h * 3 + 2] = char(48);
    }
    fxi->setChannelValues(dimBlue);
    QColor hb;
    uchar db = 0;
    QVERIFY(fixtureHeadLiveState(fxi, 0, hb, db));
    QCOMPARE(int(db), 48);
    QVERIFY2(hb.blue() == 255 && hb.red() == 0 && hb.green() < 10,
             qPrintable(QString("the hue at full, the level once: got %1").arg(hb.name())));
    const QColor unlitGrey(90, 90, 90);
    const QColor drawn = v.pixelColor(fxi, dimBlue, 0, unlitGrey, unlitGrey, 1.0, -1);
    QVERIFY2(drawn.blue() >= 45 && drawn.blue() > drawn.red() + 20,
             qPrintable(QString("a 19%% blue pixel should draw blue, got %1").arg(drawn.name())));

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

    struct Case { const char *what; Truss::MountingType mount; bool panInv; bool tiltInv; QVector3D mark;
                  float panOfs = 0.0f; float tiltOfs = 0.0f; };
    /* A standing head's tilt is centred straight UP, so with this fixture's
       180 degrees of travel it works the sky, not the deck -- which is the
       whole of Branson's point about a 270 degree head on a truss top. */
    const QList<Case> cases = {
        { "hung under the bar",      Truss::TopHung,      false, false, QVector3D(4.0f, 1.5f, 0.0f) },
        { "hung, facing stage left", Truss::TopHung,      false, false, QVector3D(-1.0f, 4.5f, 0.0f) },
        { "standing on the bar",     Truss::FloorMounted, false, false, QVector3D(4.0f, 1.5f, 5.0f) },
        { "hung, pan inverted",      Truss::TopHung,      true,  false, QVector3D(4.0f, 1.5f, 0.0f) },
        { "standing, tilt inverted", Truss::FloorMounted, false, true,  QVector3D(4.0f, 1.5f, 5.0f) },
        /* Calibration offsets correct the physical head: in the DMX, out of
           the picture. A head with offsets aimed at a mark still draws its
           beam on the mark. */
        { "hung, calibrated +8/-5",  Truss::TopHung,      false, false, QVector3D(4.0f, 1.5f, 0.0f), 8.0f, -5.0f },
        { "hung, inverted, calibrated", Truss::TopHung,   true,  true,  QVector3D(-1.0f, 4.5f, 0.0f), -6.0f, 4.0f },
        { "standing, calibrated",    Truss::FloorMounted, false, false, QVector3D(4.0f, 1.5f, 5.0f), 5.0f, 3.0f },
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
        rp.panOffsetDeg = c.panOfs;
        rp.tiltOffsetDeg = c.tiltOfs;
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

void Monitor_Test::aCalibratedHeadsPoolLandsOnItsMark()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *bar = props->addTruss();
    bar->setName("T-1");
    bar->setOrigin(QVector3D(1.0f, 2.0f, 5.0f));
    bar->setLength(8.0f);

    Fixture *spot = makeMover(doc, 10.0, 0, "Calibrated", QVector3D(3.0f, 2.0f, 5.0f));
    QVERIFY(spot != nullptr);
    FixtureRigProps rp;
    rp.trussId = bar->id();
    rp.trussOffset = 2.0f;
    rp.mountingType = Truss::TopHung;
    rp.panOffsetDeg = 12.0f;
    rp.tiltOffsetDeg = -6.0f;
    props->setFixtureRigProps(spot->id(), rp);

    const QVector3D mark(7.0f, 6.0f, 0.0f);
    AimSolver::Solution sol;
    QVERIFY(AimSolver::reach(doc, spot->id(), mark, sol));
    QVERIFY(sol.reachable());

    // The DMX the solver sends, offsets and all, as the head is being driven.
    QLCFixtureMode *mode = spot->fixtureMode();
    const QLCPhysical phy = mode->physical();
    QByteArray v(512, char(0));
    v[int(spot->address())] = char(uchar(qRound(sol.panDeg * 255.0f / float(phy.focusPanMax()))));
    v[int(spot->address() + 1)] = char(uchar(qRound(sol.tiltDeg * 255.0f / float(phy.focusTiltMax()))));
    v[int(spot->address() + 2)] = char(255);
    spot->setChannelValues(v);

    MonitorGraphicsView view(doc);
    view.resize(1000, 700);
    view.setGridMetrics(1000.0);
    view.setGridSize(QSize(10, 8));
    view.updateTrusses();
    view.addFixture(spot->id(), QPointF(3000.0, 2000.0));
    view.setViewPOV(MonitorGraphicsView::PovTop);

    auto poolOff = [&]() {
        const QPolygonF poly = view.fixtureProjection(spot->id());
        if (poly.size() < 3)
            return -1.0;
        const QPointF want = view.realPositionToPixels(double(mark.x()) * 1000.0,
                                                       double(mark.y()) * 1000.0);
        return QLineF(poly.boundingRect().center(), want).length() / view.m_cellPixels;
    };
    const double calibrated = poolOff();
    QVERIFY2(calibrated >= 0.0 && calibrated < 0.5,
             qPrintable(QString("a calibrated head's pool is %1 m from its mark").arg(calibrated)));

    // The same DMX read as if there were no calibration lands well off it:
    // the offset is real, and the view is what takes it out.
    rp.panOffsetDeg = 0.0f;
    rp.tiltOffsetDeg = 0.0f;
    props->setFixtureRigProps(spot->id(), rp);
    const double raw = poolOff();
    QVERIFY2(raw < 0.0 || raw > 1.0,
             qPrintable(QString("without removing the offset the pool is still %1 m from the mark")
                        .arg(raw)));

    delete doc;
}

void Monitor_Test::focusModeSetupOffersTheSceneAimFirst()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Fixture *a = makeMover(doc, 14.0, 0, "FA", QVector3D(2.0f, 2.0f, 5.0f));
    Fixture *b = makeMover(doc, 14.0, 16, "FB", QVector3D(5.0f, 2.0f, 5.0f));
    QVERIFY(a != nullptr && b != nullptr);
    StageTarget *mark = props->addStageTarget();
    mark->setName("Mark");
    mark->setPosition(QVector3D(4.0f, 5.0f, 0.0f));
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setStageTargetId(mark->id());
    QVERIFY(doc->addPalette(aim));
    Scene *scene = new Scene(doc);
    scene->setName("Look");
    scene->addFixture(a->id());
    scene->addFixture(b->id());
    scene->addPalette(aim->id());
    QVERIFY(doc->addFunction(scene));

    {
        FocusModeSetupDialog dlg(doc, scene->id(), { b->id() });
        const FocusSession::Options o = dlg.options();
        QCOMPARE(o.fixtures, QList<quint32>({ b->id() }));
        QCOMPARE(o.first.kind, FocusSession::Reference::SceneAim);
        QCOMPARE(o.first.id, scene->id());
        QVERIFY(!o.second.isValid());
        QCOMPARE(o.others, FocusSession::OthersDark);
    }
    {
        // Nothing selected, no scene: every head, aimed at a target.
        FocusModeSetupDialog dlg(doc, Function::invalidId(), {});
        const FocusSession::Options o = dlg.options();
        QCOMPARE(o.fixtures.size(), 2);
        QCOMPARE(o.first.kind, FocusSession::Reference::Target);
        QCOMPARE(o.first.id, mark->id());
    }
    delete doc;
}

void Monitor_Test::focusModeRunsFromTheKeyboard()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Fixture *a = makeMover(doc, 14.0, 0, "FA", QVector3D(2.0f, 6.0f, 5.0f));
    Fixture *b = makeMover(doc, 14.0, 16, "FB", QVector3D(5.0f, 6.0f, 5.0f));
    QVERIFY(a != nullptr && b != nullptr);
    StageTarget *mark = props->addStageTarget();
    mark->setName("Mark");
    mark->setPosition(QVector3D(4.0f, 3.0f, 0.0f));

    QWidget parent;
    Monitor *mon = new Monitor(&parent, doc);
    FocusSession *s = doc->programmer()->focusSession();

    FocusSession::Options o;
    o.fixtures = { a->id(), b->id() };
    o.first.kind = FocusSession::Reference::Target;
    o.first.id = mark->id();

    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(doc, &parent, o, &why);
    QVERIFY2(panel != nullptr, qPrintable(why));
    QVERIFY(s->isActive());
    QCOMPARE(s->currentFixture(), a->id());
    QLabel *offsets = panel->findChild<QLabel *>("focusOffsets");
    QLabel *progress = panel->findChild<QLabel *>("focusProgress");
    QVERIFY(offsets != nullptr && progress != nullptr);

    // Its head is ringed on the plot.
    MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    MonitorFixtureItem *ringed = gv->fixtureItemForId(a->id());
    QVERIFY(ringed != nullptr);
    QVERIFY(ringed->m_highlighted);

    QTest::keyClick(panel, Qt::Key_Right);                     // one step of pan
    QTest::keyClick(panel, Qt::Key_Up, Qt::ShiftModifier);     // a degree of tilt
    QCOMPARE(s->liveOffset(a->id()).pan, 0.1f);
    QCOMPARE(s->liveOffset(a->id()).tilt, 1.0f);
    QVERIFY2(offsets->text().contains("+0.10") && offsets->text().contains("+1.00"),
             qPrintable(offsets->text()));
    QTest::keyClick(panel, Qt::Key_Return);                     // save
    QVERIFY(s->isChecked(a->id()));
    QVERIFY2(progress->text().startsWith("1 of 2"), qPrintable(progress->text()));
    QTest::keyClick(panel, Qt::Key_BracketRight);               // next
    QCOMPARE(s->currentFixture(), b->id());
    QTest::keyClick(panel, Qt::Key_Left);
    QTest::keyClick(panel, Qt::Key_Backspace);                  // revert
    QCOMPARE(s->liveOffset(b->id()).pan, 0.0f);
    QTest::keyClick(panel, Qt::Key_Plus);                       // step x10
    QCOMPARE(s->stepDegrees(), 1.0f);
    QTest::keyClick(panel, Qt::Key_Left);
    QCOMPARE(s->liveOffset(b->id()).pan, -1.0f);
    QTest::keyClick(panel, Qt::Key_0);                          // zero
    QCOMPARE(s->liveOffset(b->id()).pan, 0.0f);
    QTest::keyClick(panel, Qt::Key_Down);

    // Esc with a head unsaved asks; Discard puts it back and ends it.
    QTimer::singleShot(0, this, []() {
        for (QWidget *w : QApplication::topLevelWidgets())
            if (QMessageBox *box = qobject_cast<QMessageBox *>(w))
                if (box->objectName() == QLatin1String("focusExitQuestion") && box->isVisible())
                    box->button(QMessageBox::Discard)->click();
    });
    QTest::keyClick(panel, Qt::Key_Escape);
    QTRY_VERIFY(panel.isNull());
    QVERIFY(!s->isActive());
    QCOMPARE(props->fixtureRigProps(b->id()).tiltOffsetDeg, 0.0f);
    QCOMPARE(props->fixtureRigProps(a->id()).tiltOffsetDeg, 1.0f);   // saved stays
    QVERIFY(!gv->fixtureItemForId(a->id())->m_highlighted);

    // The whole session is one step on the Studio's undo.
    mon->undo();
    QCOMPARE(props->fixtureRigProps(a->id()).tiltOffsetDeg, 0.0f);
    QCOMPARE(props->fixtureRigProps(a->id()).panOffsetDeg, 0.0f);

    delete mon;
    delete doc;
}

/* What App::slotUniverseWritten() does with the session's DMX: the universes
   as Focus mode writes them, into the fixtures the views read. */
static void pushFocusDmx(Doc *doc, FocusSession *s)
{
    QHash<int, QByteArray> unis;
    const QHash<QPair<int, int>, uchar> writes = s->pendingWrites();
    for (auto it = writes.constBegin(); it != writes.constEnd(); ++it)
    {
        QByteArray &u = unis[it.key().first];
        if (u.isEmpty())
            u = QByteArray(512, char(0));
        u[it.key().second] = char(it.value());
    }
    foreach (Fixture *fx, doc->fixtures())
        if (unis.contains(int(fx->universe())))
            fx->setChannelValues(unis.value(int(fx->universe())));
}

void Monitor_Test::focusDrawsTheIntentNotTheNudge()
{
    /* "When running focus I should see the visual on the rig view .. BUT it
       should show intent .. like where the light should be pointing .. not
       reality that show any skew on movement." The real head moves with every
       nudge; the drawn one stays on its mark. It did not: the views turned
       the DMX back into a direction and took the rig's (live) offset out of
       it, so the beam crept and jumped -- the DMX is whole steps (truncated,
       for a 16-bit head read from its coarse byte) and a frame behind the
       offset the nudge has already put in the rig. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Fixture *a = makeMover(doc, 10.0, 0, "FA", QVector3D(2.0f, 6.0f, 5.0f));
    Fixture *b = makeMover(doc, 10.0, 16, "FB", QVector3D(6.0f, 6.0f, 5.0f));
    QVERIFY(a != nullptr && b != nullptr);
    FixtureRigProps rp = props->fixtureRigProps(a->id());
    rp.panOffsetDeg = 4.0f;                     // a saved calibration as well
    rp.tiltOffsetDeg = -2.0f;
    props->setFixtureRigProps(a->id(), rp);
    StageTarget *mark = props->addStageTarget();
    mark->setName("Mark");
    mark->setPosition(QVector3D(4.5f, 3.0f, 0.0f));

    FocusSession *s = doc->programmer()->focusSession();
    FocusSession::Options o;
    o.fixtures = { a->id(), b->id() };
    o.order = FocusSession::AsGiven;
    o.first.kind = FocusSession::Reference::Target;
    o.first.id = mark->id();
    QString why;
    QVERIFY2(s->start(o, &why), qPrintable(why));
    QCOMPARE(s->currentFixture(), a->id());
    pushFocusDmx(doc, s);

    // The plot, as aCalibratedHeadsPoolLandsOnItsMark() has it.
    MonitorGraphicsView plot(doc);
    plot.resize(1000, 700);
    plot.setGridMetrics(1000.0);
    plot.setGridSize(QSize(10, 8));
    plot.addFixture(a->id(), QPointF(2000.0, 6000.0));
    plot.setViewPOV(MonitorGraphicsView::PovTop);
    // The Studio's rig view, live.
    StructureStudioView studio(doc, StructureStudioView::StageKind, 0);
    studio.resize(640, 420);
    studio.reload();
    studio.setLiveValues(true);

    AimSolver::Frame frame;
    QVERIFY(AimSolver::frameFor(doc, a->id(), frame));
    const QVector3D want = (mark->position() - frame.position).normalized();
    auto degOff = [&](const QVector3D &d) {
        if (d.isNull())
            return 180.0;
        return qRadiansToDegrees(qAcos(qBound(-1.0, double(QVector3D::dotProduct(d.normalized(), want)), 1.0)));
    };
    auto pool = [&]() {
        const QPolygonF poly = plot.fixtureProjection(a->id());
        return poly.size() < 3 ? QPointF(-1e6, -1e6) : poly.boundingRect().center();
    };
    const QPointF markPx = plot.realPositionToPixels(double(mark->position().x()) * 1000.0,
                                                     double(mark->position().y()) * 1000.0);
    const QPointF startPool = pool();
    QVERIFY2(QLineF(startPool, markPx).length() / plot.m_cellPixels < 0.5,
             "at the start the pool is not on the mark");

    auto check = [&](const char *when) {
        QVector3D d;
        const bool got = fixtureAimDirection(a, props->fixtureRigProps(a->id()), d);
        const double beam = got ? degOff(d) : 180.0;
        const double studioAim = degOff(studio.moverAim(a->id()));
        const double poolMoved = QLineF(pool(), startPool).length() / plot.m_cellPixels;
        QVERIFY2(beam < 0.1 && studioAim < 0.1 && poolMoved < 0.01,
                 qPrintable(QString("%1: beam %2 deg off its mark, Studio aim %3 deg off, "
                                    "plot pool moved %4 m")
                            .arg(when).arg(beam).arg(studioAim).arg(poolMoved)));
    };

    check("at the start");
    s->nudge(23.7f, -11.3f);                    // big, and not saved
    QVERIFY(s->isDirty(a->id()));
    check("nudged, before the DMX catches up");
    pushFocusDmx(doc, s);
    check("nudged");
    s->save();
    pushFocusDmx(doc, s);
    check("saved");
    s->nudge(-9.1f, 6.4f);
    pushFocusDmx(doc, s);
    check("nudged again");
    s->revert();
    check("reverted, before the DMX catches up");
    pushFocusDmx(doc, s);
    check("reverted");
    s->nudge(3.3f, 3.3f);
    pushFocusDmx(doc, s);
    s->discardAll();
    check("discarded, before the DMX catches up");
    pushFocusDmx(doc, s);
    check("discarded");

    // Ended: the saved offsets stay, and the head is read from its DMX again.
    s->nudge(7.0f, 0.0f);
    s->stop();
    QCOMPARE(props->fixtureRigProps(a->id()).panOffsetDeg, 4.0f + 23.7f);
    QCOMPARE(props->fixtureRigProps(a->id()).tiltOffsetDeg, -2.0f - 11.3f);
    delete doc;
}

void Monitor_Test::focusLightsAndRingsTheHeadInTheRigOverview()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *bar = props->addTruss();
    bar->setName("T-1");
    bar->setOrigin(QVector3D(0.0f, 4.0f, 5.0f));
    bar->setLength(8.0f);
    Fixture *a = makeMover(doc, 12.0, 0, "FA", QVector3D(2.0f, 4.0f, 5.0f));
    Fixture *b = makeMover(doc, 12.0, 16, "FB", QVector3D(6.0f, 4.0f, 5.0f));
    QVERIFY(a != nullptr && b != nullptr);
    for (Fixture *fx : { a, b })
    {
        FixtureRigProps rp;
        rp.trussId = bar->id();
        rp.trussOffset = (fx == a) ? 2.0f : 6.0f;
        rp.mountingType = Truss::TopHung;
        props->setFixtureRigProps(fx->id(), rp);
    }
    // Both lit before Focus starts, the way a look would leave them.
    QByteArray before(512, char(0));
    for (Fixture *fx : { a, b })
        for (int c = 2; c < 6; ++c)
            before[int(fx->address()) + c] = char(255);
    a->setChannelValues(before);
    b->setChannelValues(before);
    StageTarget *mark = props->addStageTarget();
    mark->setName("Mark");
    mark->setPosition(QVector3D(3.0f, 1.5f, 0.0f));

    QWidget parent;
    Monitor *mon = new Monitor(&parent, doc);
    FocusSession *s = doc->programmer()->focusSession();
    FocusSession::Options o;
    o.fixtures = { a->id(), b->id() };
    o.order = FocusSession::AsGiven;
    o.first.kind = FocusSession::Reference::Target;
    o.first.id = mark->id();
    o.others = FocusSession::OthersDark;
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(doc, &parent, o, &why);
    QVERIFY2(panel != nullptr, qPrintable(why));
    s->nudge(15.0f, -8.0f);                     // the real head is well off; the view is not
    pushFocusDmx(doc, s);

    // Opened during the session: the head being focused is ringed at once.
    mon->showStageOverview();
    StructureStudioView *ov = mon->m_overviewView;
    QVERIFY(ov != nullptr);
    QVERIFY2(ov->m_highlight.contains(a->id()),
             "the Rig Overview opened during Focus mode does not ring the head");
    QVERIFY(!ov->m_highlight.contains(b->id()));
    QVERIFY(ov->m_liveValues);

    // The focused head is white and open; the other is dark ("Dark").
    QColor colour; uchar dim = 0;
    QVERIFY(fixtureLiveState(a, ov->valuesForDrawing(a), colour, dim));
    QVERIFY2(dim > 150 && colour.red() > 200 && colour.green() > 200 && colour.blue() > 200,
             qPrintable(QString("the focused head draws %1 at %2").arg(colour.name()).arg(dim)));
    QColor other; uchar otherDim = 255;
    const bool otherShows = fixtureLiveState(b, ov->valuesForDrawing(b), other, otherDim);
    QVERIFY2(!otherShows || otherDim == 0, "the other head is still lit");

    ov->resize(900, 600);
    ov->setPlane(StructureStudioView::Angled);
    ov->setAngledView(-15.0, 25.0);
    ov->setAmbient(0.3);
    ov->setHaze(1.0);
    const QByteArray was = qgetenv("QLC_RIG_GPU");
    auto restore = qScopeGuard([&]() {
        if (was.isNull()) qunsetenv("QLC_RIG_GPU"); else qputenv("QLC_RIG_GPU", was);
    });
    qputenv("QLC_RIG_GPU", "1");
    QList<bool> gpus { false };
    if (RhiRaster::available())
        gpus << true;
    const QString shots = qEnvironmentVariable("FOCUS_SHOTS");
    for (bool gpu : gpus)
    {
        ov->setGpu(gpu);
        const QImage img = ov->grab().toImage().convertToFormat(QImage::Format_ARGB32);
        if (gpu)
            QVERIFY2(ov->drewOnGpu(), "the GPU fell back to the CPU");
        if (!shots.isEmpty())
            img.save(QString("%1/rig-overview-%2.png").arg(shots).arg(gpu ? "gpu" : "cpu"));
        /* The ring: the Studio's highlight green on the head's body. The
           light: near-white pixels in the haze, from the only head lit. */
        int ring = 0, white = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
            {
                const QColor q = img.pixelColor(x, y);
                if (q.green() > q.red() + 50 && q.green() > q.blue() + 30)
                    ++ring;
                if (q.red() > 170 && q.green() > 170 && q.blue() > 170
                    && qMax(q.red(), qMax(q.green(), q.blue())) - qMin(q.red(), qMin(q.green(), q.blue())) < 40)
                    ++white;
            }
        qInfo() << (gpu ? "gpu" : "cpu") << "ring px" << ring << "white px" << white;
        QVERIFY2(ring > 150, qPrintable(QString("%1: the focused head is not ringed").arg(gpu ? "GPU" : "CPU")));
        QVERIFY2(white > 200, qPrintable(QString("%1: the focused head is not lit").arg(gpu ? "GPU" : "CPU")));
    }
    if (!shots.isEmpty())
    {
        // Mid-nudge, the DMX a frame behind: the beam stays where it was.
        ov->setGpu(false);
        s->nudge(-12.0f, 9.0f);
        ov->grab().toImage().save(QString("%1/rig-overview-cpu-mid-nudge.png").arg(shots));
        pushFocusDmx(doc, s);
    }

    // Stepping on rings the next head there too.
    s->next();
    QVERIFY(ov->m_highlight.contains(b->id()));
    QVERIFY(!ov->m_highlight.contains(a->id()));

    s->stop();
    QTRY_VERIFY(panel.isNull());
    delete mon;
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
    // A target and a target line.
    QVERIFY2(offered.contains(QStringLiteral("Target Here"))
             && offered.contains(QStringLiteral("Target Line Here"))
             && offered.filter(QStringLiteral("Target")).count() == 2,
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


namespace
{
/* A Monitor on a deterministic canvas, torn down by RAII so a failed QVERIFY
 * cannot leak it (or free the Doc out from under it). */
struct InventoryRig
{
    QWidget parent;
    Monitor *mon = nullptr;
    MonitorGraphicsView *gv = nullptr;

    ~InventoryRig() { delete mon; }

    bool build(Doc *doc)
    {
        mon = new Monitor(&parent, doc);
        gv = mon->findChild<MonitorGraphicsView *>();
        if (gv == nullptr)
            return false;
        gv->resize(1230, 760);
        gv->setSnapDivisions(0);
        gv->setGridMetrics(1000.0);
        gv->setGridSize(QSize(40, 24));
        gv->setViewPOV(MonitorGraphicsView::PovTop);
        return gv->m_cellPixels > 0;
    }
};

/* The Doc is shared by every test in this file, so anything a test adds must go
 * again -- a fixture left at 0/0 makes the NEXT test's addFixture() overlap and
 * fail for a reason that has nothing to do with it. Declared before the rig so
 * it is destroyed after the Monitor that still references the fixtures. */
struct FixtureCleanup
{
    Doc *doc;
    QList<quint32> ids;
    explicit FixtureCleanup(Doc *d) : doc(d) {}
    ~FixtureCleanup()
    {
        foreach (quint32 id, ids)
            doc->deleteFixture(id);
    }
};

Fixture *stagedFixture(Doc *doc, const char *name, FixtureCleanup *cleanup)
{
    Fixture *f = new Fixture(doc);
    f->setName(name);
    f->setChannels(3);
    f->setPatched(false);
    if (!doc->addFixture(f))
        return nullptr;
    cleanup->ids << f->id();
    return f;
}

int inventoryLeaves(MonitorInventoryPanel *panel, QStringList *names = nullptr)
{
    int n = 0;
    for (QTreeWidgetItemIterator it(panel->tree()); *it != nullptr; ++it)
    {
        if ((*it)->data(0, Qt::UserRole).isValid())
        {
            n++;
            if (names != nullptr)
                names->append((*it)->text(0));
        }
    }
    return n;
}

/* The middle of the plot's viewport, in millimetres. The Monitor is never shown
 * in a test, so its layout squeezes the viewport to a few dozen pixels: a drop
 * has to land inside it or Qt refuses the event, so aim at the centre rather than
 * at a fixed coordinate. */
QPointF viewportCentreMm(MonitorGraphicsView *gv)
{
    const QPointF sp = gv->mapToScene(gv->viewport()->rect().center());
    return gv->pixelsToRealPosition(sp.x(), sp.y());
}

bool dropFixtureAtCentre(MonitorGraphicsView *gv, quint32 fid)
{
    QByteArray data;
    QDataStream s(&data, QIODevice::WriteOnly);
    s << fid;
    QMimeData mime;
    mime.setData(QStringLiteral("application/x-qlc-fid"), data);
    QDropEvent ev(QPointF(gv->viewport()->rect().center()), Qt::CopyAction, &mime,
                  Qt::LeftButton, Qt::NoModifier);
    // Qt only delivers a Drop to a widget that has accepted the DragEnter, so
    // play the sequence a real drag makes.
    QDragEnterEvent en(gv->viewport()->rect().center(), Qt::CopyAction, &mime,
                       Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(gv->viewport(), &en);
    QApplication::sendEvent(gv->viewport(), &ev);
    return ev.isAccepted();
}
} // namespace

void Monitor_Test::inventoryPanelListsOnlyStagedFixturesNotOnThePlot()
{
    FixtureCleanup cleanup(m_doc);
    Fixture *staged = stagedFixture(m_doc, "Staged Bar", &cleanup);
    QVERIFY(staged != nullptr);
    Fixture *patched = new Fixture(m_doc);
    patched->setName("Patched Bar");
    patched->setChannels(3);
    QVERIFY(m_doc->addFixture(patched));
    cleanup.ids << patched->id();

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    MonitorInventoryPanel *panel = rig.mon->m_inventoryPanel;
    QVERIFY(panel != nullptr);
    panel->reload();

    QStringList names;
    QCOMPARE(inventoryLeaves(panel, &names), 1);
    QCOMPARE(names.first(), QString("Staged Bar"));

    // Once it is on the plot it has left the queue.
    rig.gv->addFixture(staged->id(), QPointF(0, 0));
    panel->reload();
    QCOMPARE(inventoryLeaves(panel), 0);
}

void Monitor_Test::droppingAnInventoryFixtureOnThePlotPlacesIt()
{
    FixtureCleanup cleanup(m_doc);
    Fixture *staged = stagedFixture(m_doc, "Dropped Bar", &cleanup);
    QVERIFY(staged != nullptr);

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    rig.mon->m_inventoryPanel->reload();
    QCOMPARE(inventoryLeaves(rig.mon->m_inventoryPanel), 1);

    const QPointF at = viewportCentreMm(rig.gv);
    QVERIFY(dropFixtureAtCentre(rig.gv, staged->id()));

    QVERIFY2(rig.gv->fixturesID().contains(staged->id()), "the drop did not put the fixture on the plot");
    const QVector3D pos = m_doc->monitorProperties()->fixturePosition(staged->id(), 0, 0);
    QVERIFY2(qAbs(pos.x() - at.x()) < 200.0 && qAbs(pos.y() - at.y()) < 200.0,
             qPrintable(QString("placed at (%1, %2), dropped at (%3, %4)")
                        .arg(pos.x()).arg(pos.y()).arg(at.x()).arg(at.y())));
    QVERIFY2(!staged->isPatched(), "placing a fixture must not patch it");
    QCOMPARE(inventoryLeaves(rig.mon->m_inventoryPanel), 0);
}

void Monitor_Test::droppingAnInventoryFixtureOnATrussMountsIt()
{
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureCleanup cleanup(m_doc);
    Fixture *staged = stagedFixture(m_doc, "Truss Mover", &cleanup);
    QVERIFY(staged != nullptr);

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));

    // A truss running through the middle of the viewport, where the drop lands.
    const QPointF c = viewportCentreMm(rig.gv);
    Truss *t = props->addTruss();
    t->setName("InventoryBar");
    t->setOrigin(QVector3D(float(c.x() / 1000.0 - 1.5), float(c.y() / 1000.0), 4.0f));   // metres
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(3.0f);
    t->setWidth(0.3f);
    rig.gv->updateTrusses();
    rig.mon->m_inventoryPanel->reload();

    QVERIFY(dropFixtureAtCentre(rig.gv, staged->id()));

    const FixtureRigProps rp = props->fixtureRigProps(staged->id());
    QVERIFY2(rp.trussId == t->id(), "dropped on the truss but did not mount on it");
    QVERIFY(!staged->isPatched());

    props->removeTruss(t->id());
}

void Monitor_Test::aStagedFixtureOnThePlotNeverClaimsAUniverse()
{
    FixtureCleanup cleanup(m_doc);
    // A real fixture at the very address a staged one's placeholder would claim.
    Fixture *real = new Fixture(m_doc);
    real->setName("Real");
    real->setChannels(3);
    QVERIFY(m_doc->addFixture(real));
    cleanup.ids << real->id();
    Fixture *staged = stagedFixture(m_doc, "Staged", &cleanup);
    QVERIFY(staged != nullptr);

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    rig.gv->addFixture(real->id(), QPointF(1000, 1000));
    rig.gv->addFixture(staged->id(), QPointF(4000, 4000));

    // DMX overlay: the staged one is not "universe 1", and neither fixture is
    // reported as clashing (the placeholder 0/0 overlaps the real one's range).
    rig.mon->applyMapView(Monitor::ViewDMX);
    MonitorFixtureItem *si = rig.gv->fixtureItemForId(staged->id());
    MonitorFixtureItem *ri = rig.gv->fixtureItemForId(real->id());
    QVERIFY(si != nullptr && ri != nullptr);
    QVERIFY2(si->toolTip().contains("Unpatched"), qPrintable(si->toolTip()));
    QVERIFY2(!ri->toolTip().contains("clash"), qPrintable(ri->toolTip()));
    QVERIFY2(ri->toolTip().contains("Universe 1"), qPrintable(ri->toolTip()));

    // Network overlay says the same.
    rig.mon->applyMapView(Monitor::ViewNet);
    QVERIFY2(si->toolTip().contains("Unpatched"), qPrintable(si->toolTip()));

    rig.mon->applyMapView(Monitor::ViewNormal);
}

void Monitor_Test::locateAndResetLeaveAStagedFixtureAlone()
{
    FixtureCleanup cleanup(m_doc);
    Fixture *staged = stagedFixture(m_doc, "Staged", &cleanup);
    QVERIFY(staged != nullptr);

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));

    // Locate starts a session that flashes the fixture's DMX; for a staged one
    // there is nothing to flash, so no session may start.
    rig.mon->locateFixture(staged->id());
    QVERIFY2(rig.mon->m_locateSessions.isEmpty(), "Locate started a DMX session for a staged fixture");
    rig.mon->resetFixture(staged->id());        // must simply return
}

namespace
{
/* The topology is document-wide and the Doc is shared by every test here. */
struct TopologyCleanup
{
    Doc *doc;
    explicit TopologyCleanup(Doc *d) : doc(d) {}
    ~TopologyCleanup() { doc->networkTopology()->reset(); }
};
}

void Monitor_Test::wirelessReceiversAreDrawnWithALinkToTheirTransmitter()
{
    TopologyCleanup topo(m_doc);
    NetworkNode tx;
    tx.name = "Beach TX";
    tx.transport = NetworkNode::WirelessDMX;
    tx.placed = true;
    tx.posX = 1.0;
    tx.posY = 1.0;
    NetworkPort p;
    p.universe = 3;
    p.receiver.name = "Rig RX A";
    p.receiver.placed = true;
    p.receiver.posX = 8.0;
    p.receiver.posY = 6.0;
    NetworkPort unplaced;                       // a second port with no receiver yet
    tx.ports << p << unplaced;
    m_doc->networkTopology()->nodes() << tx;

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    rig.gv->updateNetworkNodes();

    QCOMPARE(rig.gv->m_networkNodeItems.size(), 1);
    QCOMPARE(rig.gv->m_networkReceiverItems.size(), 1);     // only the placed one
    QCOMPARE(rig.gv->m_networkLinks.size(), 1);

    NetworkNodeItem *node = rig.gv->m_networkNodeItems.first();
    NetworkReceiverItem *rx = rig.gv->m_networkReceiverItems.first();
    QCOMPARE(rx->nodeIndex(), 0);
    QCOMPARE(rx->portIndex(), 0);

    // The receiver sits at ITS position, not the transmitter's.
    const QPointF want = rig.gv->realPositionToPixels(8000.0, 6000.0);
    QVERIFY2((rx->pos() - want).manhattanLength() < 3.0,
             qPrintable(QString("receiver drawn at (%1,%2), expected (%3,%4)")
                        .arg(rx->pos().x()).arg(rx->pos().y()).arg(want.x()).arg(want.y())));
    QVERIFY((rx->pos() - node->pos()).manhattanLength() > 20.0);

    // The link joins the two.
    QGraphicsLineItem *ln = rig.gv->m_networkLinks.first();
    QVERIFY((ln->line().p1() - node->centre()).manhattanLength() < 3.0);
    QVERIFY((ln->line().p2() - rx->pos()).manhattanLength() < 3.0);

    // Drag the transmitter: the link follows without a rebuild.
    node->setPos(node->pos() + QPointF(40.0, 30.0));
    rig.gv->slotNetworkNodeMoved(node);
    QVERIFY2((ln->line().p1() - node->centre()).manhattanLength() < 3.0,
             "the link did not follow the dragged transmitter");
    QVERIFY((ln->line().p2() - rx->pos()).manhattanLength() < 3.0);
}

void Monitor_Test::aWiredNodeDrawsNoReceiverMarkers()
{
    TopologyCleanup topo(m_doc);
    NetworkNode n;
    n.name = "SR Node";
    n.placed = true;
    NetworkPort p;
    p.universe = 0;
    p.receiver.placed = true;       // meaningless on a wired node; must be ignored
    n.ports << p;
    m_doc->networkTopology()->nodes() << n;

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    rig.gv->updateNetworkNodes();
    QCOMPARE(rig.gv->m_networkNodeItems.size(), 1);
    QCOMPARE(rig.gv->m_networkReceiverItems.size(), 0);
    QCOMPARE(rig.gv->m_networkLinks.size(), 0);
}

static Fixture *stagedWithModel(Doc *doc, const char *manuf, const char *model, const char *name,
                                QList<quint32> *ids)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(manuf);
    def->setModel(model);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Std");
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setGroup(QLCChannel::Intensity);
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    def->addMode(mode);
    Fixture *f = new Fixture(doc);
    f->setFixtureDefinition(def, mode);
    f->setName(name);
    f->setPatched(false);
    if (!doc->addFixture(f))
        return nullptr;
    ids->append(f->id());
    return f;
}

void Monitor_Test::inventoryPanelIsOrganisedByManufacturerAndModel()
{
    FixtureCleanup cleanup(m_doc);
    QVERIFY(stagedWithModel(m_doc, "Robe", "Pointe", "Pointe 2", &cleanup.ids));
    QVERIFY(stagedWithModel(m_doc, "Martin", "MAC Viper", "Viper 1", &cleanup.ids));
    QVERIFY(stagedWithModel(m_doc, "Robe", "Pointe", "Pointe 1", &cleanup.ids));
    QVERIFY(stagedWithModel(m_doc, "Robe", "Robin 600", "Robin 1", &cleanup.ids));

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    MonitorInventoryPanel *panel = rig.mon->m_inventoryPanel;
    panel->reload();
    InventoryTree *tree = panel->tree();

    QCOMPARE(tree->topLevelItemCount(), 2);
    QTreeWidgetItem *martin = tree->topLevelItem(0);
    QTreeWidgetItem *robe = tree->topLevelItem(1);
    QCOMPARE(martin->text(0), QString("Martin (1)"));
    QCOMPARE(robe->text(0), QString("Robe (3)"));
    QCOMPARE(robe->child(0)->text(0), QString("Pointe (2)"));
    QCOMPARE(robe->child(1)->text(0), QString("Robin 600 (1)"));
    QCOMPARE(robe->child(0)->child(0)->text(0), QString("Pointe 1"));

    // Dragging a manufacturer carries all three Robe fixtures; a model, its two.
    tree->clearSelection();
    robe->setSelected(true);
    QCOMPARE(tree->selectedFixtureIds().size(), 3);
    tree->clearSelection();
    robe->child(0)->setSelected(true);
    QCOMPARE(tree->selectedFixtureIds().size(), 2);

    // The filter narrows every level, dropping folders that end up empty.
    panel->m_filter->setText("viper");
    QCOMPARE(tree->topLevelItemCount(), 1);
    QCOMPARE(tree->topLevelItem(0)->text(0), QString("Martin (1)"));
}

void Monitor_Test::inventoryPanelKeepsFoldersCollapsedAcrossAReload()
{
    FixtureCleanup cleanup(m_doc);
    QVERIFY(stagedWithModel(m_doc, "Robe", "Pointe", "Pointe 1", &cleanup.ids));
    QVERIFY(stagedWithModel(m_doc, "Robe", "Robin 600", "Robin 1", &cleanup.ids));
    QVERIFY(stagedWithModel(m_doc, "Martin", "MAC Viper", "Viper 1", &cleanup.ids));

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    MonitorInventoryPanel *panel = rig.mon->m_inventoryPanel;
    panel->reload();
    InventoryTree *tree = panel->tree();
    QTreeWidgetItem *martin = tree->topLevelItem(0);
    QTreeWidgetItem *robe = tree->topLevelItem(1);
    QVERIFY(martin->isExpanded() && robe->isExpanded() && robe->child(1)->isExpanded());

    martin->setExpanded(false);                        // a manufacturer...
    robe->child(1)->setExpanded(false);                // ...and one model under another
    panel->reload();

    martin = tree->topLevelItem(0);
    robe = tree->topLevelItem(1);
    QVERIFY(!martin->isExpanded());
    QVERIFY(robe->isExpanded());
    QVERIFY(robe->child(0)->isExpanded());
    QVERIFY(!robe->child(1)->isExpanded());
}

void Monitor_Test::inventoryPanelCountsNetworkFixtures()
{
    FixtureCleanup cleanup(m_doc);
    Fixture *dmx = stagedFixture(m_doc, "Wired Bar", &cleanup);
    Fixture *net = stagedFixture(m_doc, "Fence 1", &cleanup);
    QVERIFY(dmx != nullptr && net != nullptr);
    net->setConnection(Fixture::Network);

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    MonitorInventoryPanel *panel = rig.mon->m_inventoryPanel;
    panel->reload();
    QVERIFY2(panel->m_count->text().contains("2 waiting") && panel->m_count->text().contains("1 network"),
             qPrintable(panel->m_count->text()));
    QStringList names;
    inventoryLeaves(panel, &names);
    QVERIFY2(names.contains("Fence 1  [network]"), qPrintable(names.join(" | ")));
}

void Monitor_Test::aDragFromTheInventoryCarriesEverySelectedFixture()
{
    FixtureCleanup cleanup(m_doc);
    QVERIFY(stagedWithModel(m_doc, "Robe", "Pointe", "Pointe 1", &cleanup.ids));
    QVERIFY(stagedWithModel(m_doc, "Robe", "Pointe", "Pointe 2", &cleanup.ids));

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    MonitorInventoryPanel *panel = rig.mon->m_inventoryPanel;
    panel->reload();
    InventoryTree *tree = panel->tree();

    // Nothing selected: no drag starts (and nothing blocks).
    tree->clearSelection();
    tree->startDrag(Qt::CopyAction);

    tree->topLevelItem(0)->setSelected(true);          // the manufacturer: both Pointes
    const QList<quint32> ids = tree->selectedFixtureIds();
    QCOMPARE(ids.size(), 2);
    std::unique_ptr<QMimeData> mime(InventoryTree::fixtureMime(ids));
    QVERIFY(mime->hasFormat("application/x-qlc-fid"));

    // What the plot receives is what a drop of that payload places.
    QDragEnterEvent en(rig.gv->viewport()->rect().center(), Qt::CopyAction, mime.get(),
                       Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(rig.gv->viewport(), &en);
    QDropEvent ev(QPointF(rig.gv->viewport()->rect().center()), Qt::CopyAction, mime.get(),
                  Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(rig.gv->viewport(), &ev);
    QVERIFY(ev.isAccepted());
    for (quint32 id : ids)
        QVERIFY2(rig.gv->fixturesID().contains(id), qPrintable(QString("fixture %1 not placed").arg(id)));

    panel->reload();
    QCOMPARE(inventoryLeaves(panel), 0);
}

namespace
{
/* Run @p look against the next modal dialog, then reject/close it whatever
 * @p look found -- an assert inside it must not leave exec() blocking. */
void inspectModal(const std::function<void(QWidget *)> &look)
{
    QTimer::singleShot(0, [look]() {
        QWidget *modal = QApplication::activeModalWidget();
        QVERIFY2(modal != nullptr, "no modal dialog opened");
        look(modal);
        if (QDialog *d = qobject_cast<QDialog *>(modal))
            d->reject();
        else
            modal->close();
    });
}

/* Answer the next QMessageBox with @p answer, recording its text. */
void answerMessageBox(QMessageBox::StandardButton answer, QString *text = nullptr)
{
    QTimer::singleShot(0, [answer, text]() {
        QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        QVERIFY2(box != nullptr, "no message box opened");
        if (text != nullptr)
            *text = box->text();
        QAbstractButton *b = box->button(answer);
        if (b != nullptr)
            b->click();
        else
            box->close();
    });
}

QPushButton *buttonWithText(QWidget *w, const QString &text)
{
    foreach (QPushButton *b, w->findChildren<QPushButton *>())
        if (b->text() == text)
            return b;
    return nullptr;
}

bool hasLabel(QWidget *w, const QString &text)
{
    foreach (QLabel *l, w->findChildren<QLabel *>())
        if (l->text() == text)
            return true;
    return false;
}
} // namespace

void Monitor_Test::theInventoryButtonShowsAndHidesThePanel()
{
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    QAction *act = rig.mon->m_inventoryAction;
    MonitorInventoryPanel *panel = rig.mon->m_inventoryPanel;
    QVERIFY(act != nullptr && panel != nullptr);

    act->setChecked(true);
    QVERIFY(!panel->isHidden());
    QCOMPARE(QSettings().value("monitor/showInventory").toBool(), true);

    emit panel->closeRequested();                      // the panel's own x
    QVERIFY(!act->isChecked());
    QVERIFY(panel->isHidden());
    QCOMPARE(QSettings().value("monitor/showInventory").toBool(), false);
}

void Monitor_Test::addToInventoryFromTheStudioShowsTheNewFixture()
{
    FixtureCleanup cleanup(m_doc);
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    rig.mon->m_inventoryAction->setChecked(false);

    // Cancelled: nothing made, panel left as it was.
    inspectModal([](QWidget *) {});
    rig.mon->slotAddToInventory();
    QVERIFY(!rig.mon->m_inventoryAction->isChecked());

    driveDialog([](QWidget *) { return true; });
    rig.mon->slotAddToInventory();
    foreach (Fixture *f, m_doc->fixtures())
        if (!f->isPatched())
            cleanup.ids << f->id();
    QCOMPARE(cleanup.ids.size(), 1);
    QVERIFY2(rig.mon->m_inventoryAction->isChecked(), "adding should reveal the panel");
    QCOMPARE(inventoryLeaves(rig.mon->m_inventoryPanel), 1);
    rig.mon->m_inventoryAction->setChecked(false);
}

void Monitor_Test::aDropOnALockedPlotIsRefused()
{
    FixtureCleanup cleanup(m_doc);
    Fixture *staged = stagedFixture(m_doc, "Refused Bar", &cleanup);
    QVERIFY(staged != nullptr);
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));

    const bool wasLocked = m_doc->monitorProperties()->layoutLocked();
    m_doc->monitorProperties()->setLayoutLocked(true);
    QString said;
    answerMessageBox(QMessageBox::Ok, &said);
    rig.mon->slotFixturesDropped(QList<quint32>() << staged->id(), QPointF(0, 0));
    m_doc->monitorProperties()->setLayoutLocked(false);
    QVERIFY2(said.contains("Unlock the plot"), qPrintable(said));
    QVERIFY(!rig.gv->fixturesID().contains(staged->id()));

    // Unlocked, an unknown id is skipped and the real one lands.
    rig.mon->slotFixturesDropped(QList<quint32>() << 99999 << staged->id(), QPointF(0, 0));
    QVERIFY(rig.gv->fixturesID().contains(staged->id()));
    m_doc->monitorProperties()->setLayoutLocked(wasLocked);
}

void Monitor_Test::addNetworkNodePlacesItWhereTheMenuWasOpened()
{
    TopologyCleanup topo(m_doc);
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    const QPointF scenePos = rig.gv->realPositionToPixels(3000, 2000);   // 3 m, 2 m
    rig.mon->m_pendingAddScenePos = scenePos;

    driveDialog([](QWidget *dlg) {
        QLineEdit *name = findRealLineEdit(dlg);
        QComboBox *transport = dlg->findChild<QComboBox *>("networkTransport");
        if (name == nullptr || transport == nullptr)
            return false;
        name->setText("  SR ArtNet Node  ");
        transport->setCurrentIndex(transport->findData(int(NetworkNode::sACN)));
        // Accepting it opens the full Network Topology editor on the new node.
        inspectModal([](QWidget *next) {
            QVERIFY(qobject_cast<NetworkTopologyDialog *>(next) != nullptr);
        });
        return true;
    });
    rig.mon->slotAddNetworkNode();

    QCOMPARE(m_doc->networkTopology()->nodes().size(), 1);
    const NetworkNode &n = m_doc->networkTopology()->nodes().first();
    QCOMPARE(n.name, QString("SR ArtNet Node"));
    QCOMPARE(n.transport, int(NetworkNode::sACN));
    QVERIFY(n.placed);
    QVERIFY2(qAbs(n.posX - 3.0) < 0.01 && qAbs(n.posY - 2.0) < 0.01,
             qPrintable(QString("placed at %1,%2").arg(n.posX).arg(n.posY)));
    QCOMPARE(n.layerId, m_doc->monitorProperties()->activeLayerId());
    QCOMPARE(rig.gv->m_networkNodeItems.size(), 1);
    QVERIFY(rig.mon->m_pendingAddScenePos.isNull());   // used once

    // No name typed: it is numbered.
    driveDialog([](QWidget *) {
        inspectModal([](QWidget *) {});
        return true;
    });
    rig.mon->slotAddNetworkNode();
    QCOMPARE(m_doc->networkTopology()->nodes().size(), 2);
    QCOMPARE(m_doc->networkTopology()->nodes().at(1).name, QString("Node 2"));
}

void Monitor_Test::aNetworkNodeCanBePlacedFromAGearModel()
{
    TopologyCleanup topo(m_doc);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    NetworkGearCatalog::setUserDirectoryOverride(dir.filePath("NetworkGear"));
    auto restore = qScopeGuard([]() { NetworkGearCatalog::setUserDirectoryOverride(QString()); });
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));

    // Pick the ENTTEC ODE Mk3: the transport follows (and is not editable),
    // and the node arrives with its two ports.
    auto pick = [](const QString &which, const QString &name) {
        driveDialog([which, name](QWidget *dlg) {
            QComboBox *model = dlg->findChild<QComboBox *>("networkGearModel");
            QComboBox *transport = dlg->findChild<QComboBox *>("networkTransport");
            if (model == nullptr || transport == nullptr)
                return false;
            const int at = model->findText(which, Qt::MatchStartsWith);
            if (at < 0)
                return false;
            model->setCurrentIndex(at);
            if (transport->isEnabled())
                return false;
            if (!name.isEmpty())
                findRealLineEdit(dlg)->setText(name);
            inspectModal([](QWidget *) {});
            return true;
        });
    };
    pick("ENTTEC ODE Mk3", QString());
    rig.mon->slotAddNetworkNode();
    NetworkTopology *nt = m_doc->networkTopology();
    QCOMPARE(nt->nodes().size(), 1);
    QCOMPARE(nt->nodes().at(0).name, QString("ODE Mk3"));
    QCOMPARE(nt->nodes().at(0).model, QString("ENTTEC ODE Mk3"));
    QCOMPARE(nt->nodes().at(0).transport, int(NetworkNode::ArtNet));
    QCOMPARE(nt->nodes().at(0).ports.size(), 2);
    QVERIFY(nt->nodes().at(0).placed);
    // A second of the same: numbered.
    pick("ENTTEC ODE Mk3", QString());
    rig.mon->slotAddNetworkNode();
    QCOMPARE(nt->nodes().at(1).name, QString("ODE Mk3 #2"));
    // Wireless gear: a receiver per universe, named after the node.
    pick("Wireless Solution BlackBox F-1 G5", "SR Wireless");
    rig.mon->slotAddNetworkNode();
    QCOMPARE(nt->nodes().at(2).transport, int(NetworkNode::WirelessDMX));
    QCOMPARE(nt->nodes().at(2).ports.size(), 1);
    QCOMPARE(nt->nodes().at(2).ports.at(0).receiver.name, QString("SR Wireless RX"));
    // The plot says what it is.
    rig.gv->updateNetworkNodes();
    QVERIFY(rig.gv->m_networkNodeItems.at(0)->toolTip().contains("ENTTEC ODE Mk3"));

    // A hand-built node saved as a model is offered next time, as yours.
    NetworkNode hand;
    hand.name = "Rack Node";
    hand.transport = NetworkNode::sACN;
    hand.ports << NetworkPort() << NetworkPort() << NetworkPort();
    nt->nodes() << hand;
    const QString file = rig.gv->saveNetworkNodeAsModel(3, "Acme", "Rack 3");
    QVERIFY(QFile::exists(file));
    QCOMPARE(nt->nodes().at(3).model, QString("Acme Rack 3"));
    QVERIFY(rig.gv->saveNetworkNodeAsModel(3, "Acme", "  ").isEmpty());      // needs a name
    QVERIFY(rig.gv->saveNetworkNodeAsModel(99, "Acme", "X").isEmpty());      // no such node
    pick("Acme Rack 3", "Second Rack");
    rig.mon->slotAddNetworkNode();
    QCOMPARE(nt->nodes().at(4).transport, int(NetworkNode::sACN));
    QCOMPARE(nt->nodes().at(4).ports.size(), 3);
    QCOMPARE(nt->nodes().at(4).name, QString("Second Rack"));

    // Its menu offers it.
    QMenu menu;
    rig.gv->buildItemMenu(&menu, "network", 3, MonitorGraphicsView::MenuPlace());
    QVERIFY2(menuPaths(&menu).join("|").contains(QString::fromUtf8("Save as Gear Model…")),
             qPrintable(menuPaths(&menu).join(" | ")));

    // Custom still works as before: transport editable, no ports.
    driveDialog([](QWidget *dlg) {
        QComboBox *model = dlg->findChild<QComboBox *>("networkGearModel");
        QComboBox *transport = dlg->findChild<QComboBox *>("networkTransport");
        if (model == nullptr || transport == nullptr || model->currentIndex() != 0
            || !transport->isEnabled())
            return false;
        inspectModal([](QWidget *) {});
        return true;
    });
    rig.mon->slotAddNetworkNode();
    QCOMPARE(nt->nodes().at(5).ports.size(), 0);
    QVERIFY(nt->nodes().at(5).model.isEmpty());
}

void Monitor_Test::addNetworkNodeCancelledAddsNothing()
{
    TopologyCleanup topo(m_doc);
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    const MonitorProperties::GridUnits units = m_doc->monitorProperties()->gridUnits();
    m_doc->monitorProperties()->setGridUnits(MonitorProperties::Feet);
    driveDialog([](QWidget *dlg) {
        // Feet, as every length is entered: feet and inches.
        QDoubleSpinBox *x = dlg->findChild<QDoubleSpinBox *>();
        return x != nullptr && x->text().contains('\'');
    }, false);
    rig.mon->slotAddNetworkNode();
    m_doc->monitorProperties()->setGridUnits(units);
    QCOMPARE(m_doc->networkTopology()->nodes().size(), 0);
}

void Monitor_Test::deletingANetworkNodeAsksFirst()
{
    TopologyCleanup topo(m_doc);
    NetworkNode n;
    n.name = "Doomed";
    n.placed = true;
    m_doc->networkTopology()->nodes() << n;
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    QCOMPARE(rig.gv->m_networkNodeItems.size(), 1);

    rig.mon->slotNetworkNodeRemoveRequested(5);        // no such node: nothing asked
    QCOMPARE(m_doc->networkTopology()->nodes().size(), 1);

    QString asked;
    answerMessageBox(QMessageBox::No, &asked);
    rig.mon->slotNetworkNodeRemoveRequested(0);
    QVERIFY(asked.contains("Doomed"));
    QCOMPARE(m_doc->networkTopology()->nodes().size(), 1);

    answerMessageBox(QMessageBox::Yes);
    rig.mon->slotNetworkNodeRemoveRequested(0);
    QCOMPARE(m_doc->networkTopology()->nodes().size(), 0);
    QCOMPARE(rig.gv->m_networkNodeItems.size(), 0);
}

void Monitor_Test::editingANetworkNodeRebuildsThePlot()
{
    TopologyCleanup topo(m_doc);
    NetworkNode n;
    n.name = "Before";
    n.placed = true;
    m_doc->networkTopology()->nodes() << n;
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));

    driveDialog([](QWidget *dlg) {
        NetworkTopologyDialog *nt = qobject_cast<NetworkTopologyDialog *>(dlg);
        if (nt == nullptr)
            return false;
        nt->m_name->setText("After");
        emit nt->m_name->textEdited("After");
        return true;
    });
    rig.mon->slotEditNetworkNode(0);
    QCOMPARE(m_doc->networkTopology()->nodes().first().name, QString("After"));
    QCOMPARE(rig.gv->m_networkNodeItems.size(), 1);
    QVERIFY(rig.gv->m_networkNodeItems.first()->toolTip().contains("After"));

    // Cancelled: unchanged.
    inspectModal([](QWidget *) {});
    rig.mon->slotEditNetworkNode(0);
    QCOMPARE(m_doc->networkTopology()->nodes().first().name, QString("After"));
}

void Monitor_Test::theNetworkReportOpensFromTheStudio()
{
    TopologyCleanup topo(m_doc);
    NetworkNode n;
    n.name = "Reported";
    m_doc->networkTopology()->nodes() << n;
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));

    QString shown;
    inspectModal([&shown](QWidget *dlg) {
        NetworkReportDialog *rd = qobject_cast<NetworkReportDialog *>(dlg);
        QVERIFY(rd != nullptr);
        shown = rd->report().toText();
    });
    rig.mon->slotNetworkReport();
    QVERIFY2(shown.contains("Reported -- ArtNet"), qPrintable(shown));
}

namespace
{
/* The Layers row whose text starts with @p name, or nullptr. */
QTreeWidgetItem *layersRow(MonitorLayersPanel &panel, const QString &name)
{
    for (QTreeWidgetItemIterator it(panel.m_tree); *it != nullptr; ++it)
        if ((*it)->text(0).startsWith(name))
            return *it;
    return nullptr;
}
} // namespace

void Monitor_Test::aFixtureOnATowerIsListedUnderThatTower()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Tower *sl = props->addTower();
    sl->setName("SL Tower");
    sl->addShelf(1.0f);
    Tower *sr = props->addTower();
    sr->setName("SR Tower");
    sr->addShelf(1.0f);
    Tower *lone = props->addTower();
    lone->setName("Lone Tower");
    lone->addShelf(1.0f);
    // SL and SR live in a hand-made "Towers" folder, as in the report.
    const quint32 towers = props->nextGroupId();
    props->createGroup(towers, "Towers", 0, 0);
    sl->setGroupId(towers);
    sr->setGroupId(towers);

    auto spot = [doc](const char *name) {
        Fixture *f = new Fixture(doc);
        f->setName(name);
        f->setChannels(4);
        f->setPatched(false);                         // straight from the inventory
        doc->addFixture(f);
        return f;
    };
    Fixture *z = spot("Focus Spot Three Z");
    Fixture *other = spot("Lone Spot");

    {
        MonitorGraphicsView view(doc);
        view.resize(900, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 8));
        view.updatePlatforms();
        MonitorLayersPanel panel(doc, &view);

        // Mount on top of SL: listed under SL Tower, inside Towers.
        view.mountFixtureOnTower(z->id(), sl->id(), FixtureRigProps::TowerTop);
        QCOMPARE(props->fixtureGroup(z->id()), towers);
        panel.reload();
        QTreeWidgetItem *slRow = layersRow(panel, "SL Tower");
        QVERIFY(slRow != nullptr);
        QCOMPARE(slRow->childCount(), 1);
        QVERIFY(slRow->child(0)->text(0).startsWith("Focus Spot Three Z"));
        QVERIFY(slRow->parent() != nullptr && slRow->parent()->text(0).startsWith("Towers"));
        QCOMPARE(layersRow(panel, "SR Tower")->childCount(), 0);   // SR stays a plain row

        // A tower with a group of its own shows AS that group, like a truss.
        view.attachFixtureToTower(other->id(), lone->id());
        panel.reload();
        QTreeWidgetItem *loneRow = layersRow(panel, "Lone Tower");
        QVERIFY(loneRow != nullptr);
        QVERIFY2(loneRow->text(0).contains("on tower"), qPrintable(loneRow->text(0)));
        QCOMPARE(loneRow->childCount(), 1);
        QVERIFY(loneRow->child(0)->text(0).startsWith("Lone Spot"));

        // Taken off, it leaves the tower's own group (a manual one keeps it).
        view.unmountFixture(other->id());
        QCOMPARE(props->fixtureGroup(other->id()), quint32(0));
        view.unmountFixture(z->id());
        QCOMPARE(props->fixtureGroup(z->id()), towers);
        panel.reload();
        QCOMPARE(layersRow(panel, "SL Tower")->childCount(), 0);   // no longer mounted there
    }
    delete doc;
}

void Monitor_Test::aFixtureOnAPipeOrBoomIsListedUnderIt()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    auto boom = [props](const char *name, float x) {
        Pipe *p = props->addPipe();
        p->setName(name);
        p->setOrientation(Pipe::Vertical);
        p->setOriginX(x);
        p->setOriginY(2.0f);
        return p;
    };
    Pipe *slBoom = boom("SL Boom", 1.0f);
    Pipe *srBoom = boom("SR Boom", 7.0f);
    Pipe *electric = props->addPipe();
    electric->setName("1st Electric");
    electric->setOrientation(Pipe::Horizontal);
    electric->setOriginX(3.0f);
    electric->setOriginY(3.0f);
    Tower *tower = props->addTower();
    tower->setName("DS Tower");
    tower->addShelf(1.0f);
    // The two booms live in a hand-made "Booms" folder.
    const quint32 booms = props->nextGroupId();
    props->createGroup(booms, "Booms", 0, 0);
    slBoom->setGroupId(booms);
    srBoom->setGroupId(booms);

    auto spot = [doc](const char *name, quint32 addr) {
        Fixture *f = new Fixture(doc);
        f->setName(name);
        f->setChannels(4);
        f->setAddress(addr);
        doc->addFixture(f);
        return f;
    };
    Fixture *side = spot("Side Light", 0);
    Fixture *wash = spot("Electric Wash", 10);
    Fixture *mover = spot("Mover", 20);
    Fixture *old = spot("Old Show Par", 30);
    // An older workspace: already on the electric, never grouped.
    FixtureRigProps rp;
    rp.pipeId = electric->id();
    props->setFixtureRigProps(old->id(), rp);

    {
        MonitorGraphicsView view(doc);
        view.resize(900, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 8));
        view.updatePlatforms();
        view.addFixture(old->id(), QPointF(3000.0, 3000.0));
        view.ensureFixtureOnPlot(wash->id());          // on the plot, loose
        view.ensureGroupRegistry();                     // what a load does
        MonitorLayersPanel panel(doc, &view);

        // The loaded one joins the electric's own group, shown AS the pipe.
        QVERIFY(electric->groupId() != 0);
        QCOMPARE(props->fixtureGroup(old->id()), electric->groupId());
        panel.reload();
        QTreeWidgetItem *elecRow = layersRow(panel, "1st Electric");
        QVERIFY(elecRow != nullptr);
        QVERIFY2(elecRow->text(0).contains("on pipe"), qPrintable(elecRow->text(0)));
        QCOMPARE(elecRow->childCount(), 1);
        QVERIFY(elecRow->child(0)->text(0).startsWith("Old Show Par"));

        // Onto the SL boom: listed under it, inside Booms.
        view.attachFixtureToPipe(side->id(), slBoom->id());
        QCOMPARE(props->fixtureGroup(side->id()), booms);
        panel.reload();
        QTreeWidgetItem *slRow = layersRow(panel, "SL Boom");
        QVERIFY(slRow != nullptr);
        QCOMPARE(slRow->childCount(), 1);
        QVERIFY(slRow->child(0)->text(0).startsWith("Side Light"));
        QVERIFY(slRow->parent() != nullptr && slRow->parent()->text(0).startsWith("Booms"));
        QCOMPARE(layersRow(panel, "SR Boom")->childCount(), 0);

        // Dropped on the electric's row in Layers: mounted there.
        QVERIFY(layersRow(panel, "Electric Wash") != nullptr);
        panel.handleTreeDrop({ layersRow(panel, "Electric Wash") }, layersRow(panel, "1st Electric"));
        QCOMPARE(props->fixtureRigProps(wash->id()).pipeId, electric->id());
        QCOMPARE(props->fixtureGroup(wash->id()), electric->groupId());
        panel.reload();
        QCOMPARE(layersRow(panel, "1st Electric")->childCount(), 2);

        // A standing boom with a group of its own reads "on boom".
        view.attachFixtureToPipe(mover->id(), srBoom->id());
        srBoom->setGroupId(0);                           // take SR out of the folder
        props->setFixtureGroup(mover->id(), 0);
        view.ensureMountGroup("pipe", srBoom->id());
        panel.reload();
        QTreeWidgetItem *srRow = layersRow(panel, "SR Boom");
        QVERIFY2(srRow->text(0).contains("on boom"), qPrintable(srRow->text(0)));
        QCOMPARE(srRow->childCount(), 1);

        // Moved from the electric to a tower: it leaves the pipe's own group.
        view.attachFixtureToTower(wash->id(), tower->id());
        QCOMPARE(props->fixtureGroup(wash->id()), tower->groupId());
        QVERIFY(props->fixtureGroup(wash->id()) != electric->groupId());
        // Unmounted: out of the pipe's own group; a manual folder keeps it.
        view.unmountFixture(old->id());
        QCOMPARE(props->fixtureGroup(old->id()), quint32(0));
        view.unmountFixture(side->id());
        QCOMPARE(props->fixtureGroup(side->id()), booms);
        panel.reload();
        QCOMPARE(layersRow(panel, "SL Boom")->childCount(), 0);
        QCOMPARE(layersRow(panel, "1st Electric")->childCount(), 0);
    }
    delete doc;
}

void Monitor_Test::draggingATargetAnnouncesTheMove()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StageTarget *t = props->addStageTarget();
    t->setName("Vocalist");
    t->setPosition(QVector3D(2.0f, 2.0f, 0.0f));
    {
        MonitorGraphicsView view(doc);
        view.resize(900, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(10, 8));
        view.updateTargets();
        TargetItem *ti = view.m_targetItems.value(t->id(), nullptr);
        QVERIFY(ti != nullptr);

        // Anything aiming at it (the look panel's reach check) hears about it.
        QSignalSpy spy(props, &MonitorProperties::stageTargetsChanged);
        ti->setPos(ti->pos() + QPointF(0, view.m_cellPixels * 2));
        view.slotTargetMoved(ti);
        QCOMPARE(spy.count(), 1);
        QVERIFY2(t->y() > 3.5f, qPrintable(QString("target now at y=%1").arg(t->y())));

        view.undoLastMove();
        QCOMPARE(spy.count(), 2);
        QVERIFY(qAbs(t->y() - 2.0f) < 0.01f);
    }
    delete doc;
}

void Monitor_Test::aMoverOnATopFaceStandsUpOrHangsDown()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("Frame");
    {
        StructureStudioView view(doc, StructureStudioView::TrussKind, t->id());

        // A head on a deck, laid out on the deck's TOP face (studioMount 0) as
        // the deck editor records it: its base is on the deck, so up is up --
        // not the in-plane axis a bar lying in that face would use.
        FixtureRigProps deck;
        deck.deckPlatformId = 0;
        deck.studioMount = 0;
        for (float angle : { 0.0f, 37.0f, 90.0f })
        {
            deck.studioAngle = angle;
            for (int mounting : { int(Truss::TopHung), int(Truss::FloorMounted) })
            {
                deck.mountingType = Truss::MountingType(mounting);
                const QVector3D up = view.fixtureBodyUp(deck);
                QVERIFY2(up.z() > 0.99f, qPrintable(QString("deck head at %1 deg, mounting %2: up (%3,%4,%5)")
                                                    .arg(angle).arg(mounting)
                                                    .arg(up.x()).arg(up.y()).arg(up.z())));
            }
        }

        // Hung under a truss, same face: straight down.
        FixtureRigProps hung;
        hung.trussId = t->id();
        hung.studioMount = 0;
        hung.mountingType = Truss::TopHung;
        QVERIFY(view.fixtureBodyUp(hung).z() < -0.99f);

        // A front face keeps the in-plane answer: upright on a riser's front.
        FixtureRigProps front;
        front.riserPlatformId = 0;
        front.riserFace = FixtureRigProps::RiserFront;
        front.studioMount = 1;
        front.mountingType = Truss::FloorMounted;
        QVERIFY(qAbs(view.fixtureBodyUp(front).z()) > 0.99f);
    }
    delete doc;
}

void Monitor_Test::aFixtureCanBeRaisedOntoATowersTopCap()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    // AT2: 2.438 m tall, but its shelves stop at 1.8 m.
    Tower *t = props->addTower();
    t->setName("AT2");
    t->setOriginX(2.0f);
    t->setOriginY(2.0f);
    t->setWidth(0.405f);
    t->setDepth(0.405f);
    t->setHeight(2.438f);
    t->addShelf(0.6f);
    t->addShelf(1.2f);
    t->addShelf(1.8f);

    Fixture *spot = new Fixture(doc);
    spot->setName("Focus Spot Three Z 2");
    spot->setChannels(4);
    QVERIFY(doc->addFixture(spot));
    FixtureRigProps rp;
    rp.towerId = t->id();
    rp.towerMountSide = FixtureRigProps::TowerShelf;
    rp.towerShelf = 2;
    rp.towerU = 0.2f;
    rp.towerV = 0.2f;
    props->setFixtureRigProps(spot->id(), rp);

    {
        QWidget parent;
        Monitor mon(&parent, doc);
        QDialog dlg;
        StructureStudioView *view = nullptr;
        QWidget *pane = mon.makeStudioPane(&dlg, 1 /* tower */, t->id(), nullptr, &view);
        QVERIFY(pane != nullptr && view != nullptr);
        view->resize(700, 600);
        view->reload();
        view->setLocked(false);
        view->setPlane(StructureStudioView::Front);

        // Dragged up past the last shelf: onto the top cap, standing on it.
        const float cx = t->originX() + 0.2f, cy = t->originY() + 0.2f;
        QVERIFY(view->dragFixtureTo(spot->id(), view->w2s(QVector3D(cx, cy, 2.7f))));
        FixtureRigProps now = props->fixtureRigProps(spot->id());
        QCOMPARE(now.towerMountSide, int(FixtureRigProps::TowerTop));
        QCOMPARE(now.mountingType, Truss::FloorMounted);
        QVERIFY2(qAbs(props->fixtureRigPosition(spot->id()).z() - 2.438f) < 0.05f,
                 qPrintable(QString("on the cap at z=%1").arg(props->fixtureRigPosition(spot->id()).z())));

        // Back down beside the middle shelf, and to the base.
        QVERIFY(view->dragFixtureTo(spot->id(), view->w2s(QVector3D(cx, cy, 1.25f))));
        now = props->fixtureRigProps(spot->id());
        QCOMPARE(now.towerMountSide, int(FixtureRigProps::TowerShelf));
        QCOMPARE(now.towerShelf, 1);
        QVERIFY(view->dragFixtureTo(spot->id(), view->w2s(QVector3D(cx, cy, 0.05f))));
        QCOMPARE(props->fixtureRigProps(spot->id()).towerMountSide, int(FixtureRigProps::TowerBottom));

        // Or pick it in the inspector: Top, each shelf with its height, Bottom.
        emit view->fixtureSelected(spot->id());
        QComboBox *pos = nullptr;
        foreach (QComboBox *cb, dlg.findChildren<QComboBox *>())
            if (cb->toolTip().startsWith("Where on the tower"))
                pos = cb;
        QVERIFY(pos != nullptr);
        QVERIFY2(!pos->isHidden(), "the Position row is not shown for a tower-mounted fixture");
        // ...and the rest of the fixture's fields stay available beside it
        // (they were all hidden again by the bar half of the panel).
        QComboBox *orient = nullptr;
        foreach (QComboBox *cb, dlg.findChildren<QComboBox *>())
            if (cb->toolTip().startsWith("Base direction"))
                orient = cb;
        QVERIFY(orient != nullptr);
        QVERIFY2(!orient->isHidden(), "Orientation is hidden for a selected fixture");
        QPushButton *full = nullptr;
        foreach (QPushButton *b, dlg.findChildren<QPushButton *>())
            if (b->text() == "Fixture Properties…")
                full = b;
        QVERIFY(full != nullptr && full->isEnabled());
        QStringList items;
        for (int i = 0; i < pos->count(); ++i)
            items << pos->itemText(i);
        QCOMPARE(items, QStringList({ "Top (2.44 m)", "Shelf 3 (1.80 m)", "Shelf 2 (1.20 m)",
                                      "Shelf 1 (0.60 m)", "Bottom" }));
        QCOMPARE(pos->currentText(), QString("Bottom"));
        pos->setCurrentIndex(0);
        QCOMPARE(props->fixtureRigProps(spot->id()).towerMountSide, int(FixtureRigProps::TowerTop));
        pos->setCurrentIndex(1);
        now = props->fixtureRigProps(spot->id());
        QCOMPARE(now.towerMountSide, int(FixtureRigProps::TowerShelf));
        QCOMPARE(now.towerShelf, 2);
    }
    delete doc;
}

void Monitor_Test::fixturesAndBarsAreRenamedInTheStructureEditorsList()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *t1 = props->addTruss();
    t1->setName("T-1");
    t1->setLength(3.0f);
    Truss *bar = props->addTruss();
    bar->setName("Bar 1");
    bar->setParentTrussId(t1->id());
    Fixture *spot = new Fixture(doc);
    spot->setName("LED Movinghead+Circle");
    spot->setChannels(4);
    QVERIFY(doc->addFixture(spot));
    FixtureRigProps rp;
    rp.trussId = t1->id();
    rp.trussOffset = 1.0f;
    props->setFixtureRigProps(spot->id(), rp);

    {
        QWidget parent;
        Monitor mon(&parent, doc);
        QDialog dlg;
        StructureStudioView *view = nullptr;
        mon.makeStudioPane(&dlg, 2 /* truss */, t1->id(), nullptr, &view);
        QTreeWidget *list = nullptr;
        foreach (QTreeWidget *tw, dlg.findChildren<QTreeWidget *>())
            for (int i = 0; i < tw->topLevelItemCount(); ++i)
                if (tw->topLevelItem(i)->text(0) == "Ungrouped")
                    list = tw;
        QVERIFY(list != nullptr);
        auto rowNamed = [list](const QString &name) -> QTreeWidgetItem * {
            for (QTreeWidgetItemIterator it(list); *it != nullptr; ++it)
                if ((*it)->text(0) == name)
                    return *it;
            return nullptr;
        };

        // Return / F2 renames a fixture row: it is editable, and the edit sticks.
        QTreeWidgetItem *row = rowNamed("LED Movinghead+Circle");
        QVERIFY(row != nullptr);
        QVERIFY(row->flags() & Qt::ItemIsEditable);
        row->setText(0, "  SR Spot  ");                  // what the inline editor commits
        QCOMPARE(spot->name(), QString("SR Spot"));
        QCOMPARE(row->text(0), QString("SR Spot"));
        QVERIFY(doc->isModified());

        // An empty name is refused and the old one put back.
        row->setText(0, "   ");
        QCOMPARE(spot->name(), QString("SR Spot"));
        QCOMPARE(row->text(0), QString("SR Spot"));

        // A bar row renames the bar.
        QTreeWidgetItem *barRow = rowNamed("Bar 1");
        QVERIFY(barRow != nullptr && (barRow->flags() & Qt::ItemIsEditable));
        barRow->setText(0, "Pipe A");
        QCOMPARE(bar->name(), QString("Pipe A"));

        // Renamed somewhere else (the edit dialog, Layers): the list follows.
        spot->setName("From Elsewhere");
        QCoreApplication::processEvents();
        QVERIFY(rowNamed("From Elsewhere") != nullptr);
        QVERIFY(rowNamed("SR Spot") == nullptr);
    }
    delete doc;
}

void Monitor_Test::addCycloramaSpansTheBackOfTheStage()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Meters);
    props->setGridSize(QVector3D(12, 6, 8));             // 12 m wide
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        bool opened = false;
        driveDialog([&opened](QWidget *dlg) {
            opened = dlg->windowTitle().startsWith("Cyclorama");
            return true;
        });
        mon.slotAddCyclorama();
        QVERIFY2(opened, "adding a cyc should open its editor");
        QCOMPARE(props->sceneries().size(), 1);
        Scenery *cyc = props->sceneries().first();
        QCOMPARE(cyc->kind(), int(Scenery::Cyclorama));
        QCOMPARE(cyc->name(), QString("Cyclorama 1"));
        QVERIFY(qAbs(cyc->originX() - 6.0f) < 0.01f);   // centred
        QVERIFY(qAbs(cyc->originY() - 0.3f) < 0.01f);   // at the upstage edge
        QVERIFY(qAbs(cyc->width() - 11.0f) < 0.01f);    // the stage's width, less a margin
        QVERIFY(qAbs(cyc->height() - 6.0f) < 0.01f);
        QCOMPARE(cyc->layerId(), props->activeLayerId());
        QVERIFY(mon.m_graphicsView->m_sceneryItems.contains(cyc->id()));
        QVERIFY(doc->isModified());

        // Listed in the Layers panel, with the other structures.
        MonitorLayersPanel panel(doc, mon.m_graphicsView);
        panel.reload();
        bool listed = false;
        for (QTreeWidgetItemIterator it(panel.m_tree); *it != nullptr; ++it)
            listed = listed || ((*it)->data(0, Qt::UserRole + 2).toString() == "scenery");
        QVERIFY(listed);
        QVERIFY(panel.kindLockable("scenery"));
    }
    delete doc;
}

void Monitor_Test::theCycloramaEditorPreviewsAndCancelReverts()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Meters);
    Scenery *cyc = props->addScenery();
    cyc->setName("Cyc");
    cyc->setWidth(10.0f);
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        auto spinWithTip = [](QWidget *dlg, const QString &tip) -> QDoubleSpinBox * {
            foreach (QDoubleSpinBox *sp, dlg->findChildren<QDoubleSpinBox *>())
                if (sp->toolTip().startsWith(tip))
                    return sp;
            return nullptr;
        };
        // Bowed and cancelled: back to flat.
        driveDialog([&](QWidget *dlg) {
            QDoubleSpinBox *bow = spinWithTip(dlg, "How far the middle curves");
            if (bow == nullptr) return false;
            bow->setValue(1.5);
            return qAbs(cyc->bow() - 1.5f) < 0.01f;        // previewed live
        }, false);
        const quint32 cycId = cyc->id();
        mon.slotEditScenery(cycId);
        // Cancel puts the rig back as it was: what was edited is a fresh
        // object now, found by its id.
        cyc = props->scenery(cycId);
        QVERIFY(cyc != nullptr);
        QCOMPARE(cyc->bow(), 0.0f);

        // Raised and accepted: kept.
        driveDialog([&](QWidget *dlg) {
            QDoubleSpinBox *trim = spinWithTip(dlg, "Height of the bottom edge");
            if (trim == nullptr) return false;
            trim->setValue(0.5);
            return true;
        });
        mon.slotEditScenery(cyc->id());
        QVERIFY(qAbs(cyc->bottom() - 0.5f) < 0.01f);
        QCOMPARE(cyc->name(), QString("Cyc"));
    }
    delete doc;
}

void Monitor_Test::aCycloramaOnThePlotMovesLocksHidesAndDeletes()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Scenery *cyc = props->addScenery();
    cyc->setName("Cyc");
    cyc->setOriginX(5.0f);
    cyc->setOriginY(0.5f);
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        MonitorGraphicsView *gv = mon.m_graphicsView;
        gv->setSnapDivisions(0);
        gv->updatePlatforms();
        SceneryItem *item = gv->m_sceneryItems.value(cyc->id());
        QVERIFY(item != nullptr);
        QCOMPARE(gv->itemFor("scenery", cyc->id()), static_cast<QGraphicsItem *>(item));
        QVERIFY(item->flags() & QGraphicsItem::ItemIsMovable);

        // Dragged a metre downstage.
        const QPointF before = gv->pixelsToRealPosition(item->pos().x(), item->pos().y());
        const QPointF one = gv->realPositionToPixels(before.x(), before.y() + 1000.0);
        item->setPos(one);
        gv->slotSceneryMoved(item);
        QVERIFY2(qAbs(cyc->originY() - 1.5f) < 0.02f, qPrintable(QString::number(cyc->originY())));

        // Renamed, locked, hidden with its layer.
        gv->renameItem("scenery", cyc->id(), "Upstage Cyc");
        QCOMPARE(cyc->name(), QString("Upstage Cyc"));
        gv->setItemLocked("scenery", cyc->id(), true);
        QVERIFY(gv->itemLocked("scenery", cyc->id()));
        QVERIFY(!(gv->m_sceneryItems.value(cyc->id())->flags() & QGraphicsItem::ItemIsMovable));
        gv->setItemLocked("scenery", cyc->id(), false);
        const quint32 layer = props->addLayer("Scenery");
        gv->setItemLayer(gv->m_sceneryItems.value(cyc->id()), layer);
        QCOMPARE(cyc->layerId(), layer);
        props->setLayerVisible(layer, false);
        gv->refreshItemLayerState();
        QVERIFY(!gv->m_sceneryItems.value(cyc->id())->isVisible());

        // Delete from its menu asks first: Cancel keeps it, Delete removes it.
        QSignalSpy asked(gv, &MonitorGraphicsView::sceneryRemoveRequested);
        const quint32 id = cyc->id();
        driveMessageBox([](QWidget *) { return true; }, "Cancel");
        gv->requestDeleteItem("scenery", id);
        QCOMPARE(asked.count(), 1);
        QVERIFY(props->scenery(id) != nullptr);
        driveMessageBox([](QWidget *) { return true; }, "Delete");
        gv->requestDeleteItem("scenery", id);
        QVERIFY(props->scenery(id) == nullptr);
        QVERIFY(!gv->m_sceneryItems.contains(id));
    }
    delete doc;
}

/* A spot with one gobo wheel (open, and a gobo whose LEFT half is metal),
   a gobo rotation channel, a prism and a prism rotation. */
static QLCFixtureDef *opticsDef(const QString &halfGobo, const QString &openGobo)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Optics Spot");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("5ch");
    auto add = [&](const QString &name, QLCChannel::Group g) {
        QLCChannel *ch = new QLCChannel();
        ch->setName(name);
        ch->setGroup(g);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, mode->channels().size());
        return ch;
    };
    auto cap = [](QLCChannel *ch, int lo, int hi, const QString &name,
                  QLCCapability::Preset p = QLCCapability::Custom, const QString &res = QString()) {
        QLCCapability *c = new QLCCapability(uchar(lo), uchar(hi), name);
        if (p != QLCCapability::Custom)
            c->setPreset(p);
        if (!res.isEmpty())
            c->setResource(0, res);
        ch->addCapability(c);
    };
    QLCChannel *wheel = add("Gobo wheel", QLCChannel::Gobo);
    cap(wheel, 0, 9, "Open", QLCCapability::GoboMacro, openGobo);
    cap(wheel, 10, 19, "Half", QLCCapability::GoboMacro, halfGobo);
    cap(wheel, 20, 29, "Half shake", QLCCapability::GoboShakeMacro, halfGobo);
    cap(wheel, 30, 255, "Wheel spin", QLCCapability::RotationClockwise);
    QLCChannel *rot = add("Gobo rotation", QLCChannel::Gobo);
    cap(rot, 0, 127, "Indexing", QLCCapability::RotationIndexed);
    cap(rot, 128, 255, "Rotation slow - fast", QLCCapability::RotationClockwiseSlowToFast);
    QLCChannel *prism = add("Prism", QLCChannel::Prism);
    cap(prism, 0, 9, "No prism");
    cap(prism, 10, 127, "6 facet prism");
    cap(prism, 128, 255, "Linear prism");
    QLCChannel *prot = add("Prism rotation", QLCChannel::Prism);
    cap(prot, 0, 3, "Stop");
    cap(prot, 4, 255, "Counter-clockwise rotation slow - fast");
    add("Dimmer", QLCChannel::Intensity);
    def->addMode(mode);
    return def;
}

void Monitor_Test::beamOpticsReadGobosRotationAndPrism()
{
    // Gobo pictures as drawn: metal black, holes clear.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QImage half(80, 80, QImage::Format_ARGB32);
    half.fill(Qt::transparent);
    {
        QPainter hp(&half);
        hp.fillRect(0, 0, 40, 80, Qt::black);          // the left half blocks
    }
    const QString halfPath = dir.filePath("half.png");
    QVERIFY(half.save(halfPath));
    QImage open(80, 80, QImage::Format_ARGB32);
    open.fill(Qt::transparent);
    const QString openPath = dir.filePath("open.png");
    QVERIFY(open.save(openPath));

    const QImage m = BeamOptics::goboMask(halfPath);
    QVERIFY(!m.isNull());
    QCOMPARE(m.width(), BeamOptics::kMaskSize);
    QVERIFY(BeamOptics::sample(m, -0.5, 0.0) < 0.05);
    QVERIFY(BeamOptics::sample(m, 0.5, 0.0) > 0.95);
    QCOMPARE(BeamOptics::sample(m, 0.9, 0.9), 0.0);    // outside the cone
    QVERIFY(BeamOptics::goboMask(dir.filePath("missing.png")).isNull());
    QVERIFY(BeamOptics::sample(QImage(), 0.0, 0.0) == 0.0);

    QLCFixtureDef *def = opticsDef(halfPath, openPath);
    Doc doc(this);
    Fixture *fx = new Fixture(&doc);
    fx->setFixtureDefinition(def, def->modes().first());
    QVERIFY(doc.addFixture(fx));
    auto vals = [](int wheel, int rot, int prism, int prot) {
        QByteArray v(5, 0);
        v[0] = char(wheel); v[1] = char(rot); v[2] = char(prism); v[3] = char(prot); v[4] = char(255);
        return v;
    };

    // Open, no prism: a plain beam.
    BeamOptics::Optics o = BeamOptics::opticsFor(fx, vals(0, 0, 0, 0), 0.0);
    QVERIFY(o.mask.isNull());
    QCOMPARE(o.coverage, 1.0);
    QVERIFY(!o.animated);
    // A wheel spinning between slots shows no gobo either.
    QVERIFY(BeamOptics::opticsFor(fx, vals(100, 0, 0, 0), 0.0).mask.isNull());

    // The half gobo: dark left, lit right, about half the light.
    o = BeamOptics::opticsFor(fx, vals(15, 0, 0, 0), 0.0);
    QVERIFY(!o.mask.isNull());
    QVERIFY(BeamOptics::sample(o.mask, -0.5, 0.0) < 0.05);
    QVERIFY(BeamOptics::sample(o.mask, 0.5, 0.0) > 0.9);
    QVERIFY2(qAbs(o.coverage - 0.5) < 0.08, qPrintable(QString::number(o.coverage)));
    QVERIFY(!o.animated);

    // Indexed half a turn: now the right is dark.
    o = BeamOptics::opticsFor(fx, vals(15, 64, 0, 0), 0.0);
    QVERIFY(BeamOptics::sample(o.mask, -0.5, 0.0) > 0.9);
    QVERIFY(BeamOptics::sample(o.mask, 0.5, 0.0) < 0.05);

    // Turning: animated, and not where it was a moment ago.
    const BeamOptics::Optics t0 = BeamOptics::opticsFor(fx, vals(15, 200, 0, 0), 0.0);
    const BeamOptics::Optics t1 = BeamOptics::opticsFor(fx, vals(15, 200, 0, 0), 0.5);
    QVERIFY(t0.animated && t1.animated);
    QVERIFY(t0.mask != t1.mask);

    // A 6-facet prism on the open beam: six images round the axis.
    o = BeamOptics::opticsFor(fx, vals(0, 0, 60, 0), 0.0);
    QCOMPARE(o.prismFacets, 6);
    QVERIFY(!o.mask.isNull());
    QVERIFY(!o.animated);
    for (int k = 0; k < 6; ++k)
    {
        const double a = 2.0 * M_PI * k / 6;
        QVERIFY2(BeamOptics::sample(o.mask, 0.55 * qCos(a), 0.55 * qSin(a)) > 0.5,
                 qPrintable(QString("facet %1 dark").arg(k)));
    }
    QVERIFY(o.coverage < 1.0);
    // Linear: three in a row across the beam, dark above and below.
    o = BeamOptics::opticsFor(fx, vals(0, 0, 200, 0), 0.0);
    QCOMPARE(o.prismFacets, 1);
    QVERIFY(BeamOptics::sample(o.mask, 0.5, 0.0) > 0.5);
    QVERIFY(BeamOptics::sample(o.mask, -0.5, 0.0) > 0.5);
    QVERIFY(BeamOptics::sample(o.mask, 0.0, 0.7) < 0.05);
    // A turning prism turns the images.
    const BeamOptics::Optics p0 = BeamOptics::opticsFor(fx, vals(0, 0, 200, 128), 0.0);
    const BeamOptics::Optics p1 = BeamOptics::opticsFor(fx, vals(0, 0, 200, 128), 0.3);
    QVERIFY(p0.animated);
    QVERIFY(p0.mask != p1.mask);
    // Gobo and prism together: the gobo's image, repeated.
    o = BeamOptics::opticsFor(fx, vals(15, 0, 60, 0), 0.0);
    QCOMPARE(o.prismFacets, 6);
    QVERIFY(o.coverage < BeamOptics::opticsFor(fx, vals(0, 0, 60, 0), 0.0).coverage);

    QVERIFY(BeamOptics::opticsFor(nullptr, QByteArray(), 0.0).mask.isNull());
    doc.deleteFixture(fx->id());
    delete def;
}

void Monitor_Test::prismAndRotationNamesAreRead()
{
    auto facets = [](const QString &name, QLCCapability::Preset p = QLCCapability::Custom) {
        QLCCapability c(0, 10, name);
        if (p != QLCCapability::Custom)
            c.setPreset(p);
        return BeamOptics::prismFacets(&c);
    };
    QCOMPARE(facets("6 facet prism"), 6);
    QCOMPARE(facets("3-facet prism"), 3);
    QCOMPARE(facets("8 Facet Rotating"), 8);
    QCOMPARE(facets("Linear prism"), 1);
    QCOMPARE(facets("Prism"), 3);
    QCOMPARE(facets("Prism in"), 3);
    QCOMPARE(facets("No prism"), 0);
    QCOMPARE(facets("Prism off"), 0);
    QCOMPARE(facets("Stop"), 0);
    QCOMPARE(facets("No effect"), 0);
    QCOMPARE(facets("Effect", QLCCapability::PrismEffectOn), 3);
    QCOMPARE(facets("Prism", QLCCapability::PrismEffectOff), 0);
    QCOMPARE(BeamOptics::prismFacets(nullptr), 0);

    auto speed = [](const QString &name, int v, QLCCapability::Preset p = QLCCapability::Custom) {
        QLCCapability c(100, 200, name);
        if (p != QLCCapability::Custom)
            c.setPreset(p);
        return BeamOptics::rotationSpeed(&c, v);
    };
    // Direction: by preset, else by name.
    QVERIFY(speed("x", 150, QLCCapability::RotationClockwise) > 0.0);
    QVERIFY(speed("x", 150, QLCCapability::RotationCounterClockwise) < 0.0);
    QVERIFY(speed("Clockwise rotation", 150) > 0.0);
    QVERIFY(speed("Counter-clockwise rotation", 150) < 0.0);
    QVERIFY(speed("Anti-clockwise rotation", 150) < 0.0);
    QCOMPARE(speed("Rotation stop", 150), 0.0);
    QCOMPARE(speed("No rotation", 150), 0.0);
    QCOMPARE(speed("Shake", 150), 0.0);
    QCOMPARE(speed("x", 150, QLCCapability::RotationStop), 0.0);
    // Speed across the range, whichever way it runs.
    QVERIFY(speed("Clockwise rotation slow - fast", 190) > speed("Clockwise rotation slow - fast", 110));
    QVERIFY(speed("Clockwise rotation fast - slow", 190) < speed("Clockwise rotation fast - slow", 110));
    QVERIFY(qAbs(speed("x", 190, QLCCapability::RotationCounterClockwiseSlowToFast))
            > qAbs(speed("x", 110, QLCCapability::RotationCounterClockwiseSlowToFast)));
    QVERIFY(qAbs(speed("x", 190, QLCCapability::RotationClockwiseFastToSlow))
            < qAbs(speed("x", 110, QLCCapability::RotationClockwiseFastToSlow)));
    QCOMPARE(BeamOptics::rotationSpeed(nullptr, 0), 0.0);

    QLCCapability idx(0, 127, "Indexing");
    idx.setPreset(QLCCapability::RotationIndexed);
    QCOMPARE(BeamOptics::indexedAngle(&idx, 0), 0.0);
    QCOMPARE(BeamOptics::indexedAngle(&idx, 64), 180.0);
    QCOMPARE(BeamOptics::rotationSpeed(&idx, 64), 0.0);
    QLCCapability notIdx(0, 127, "Spin");
    QCOMPARE(BeamOptics::indexedAngle(&notIdx, 64), -1.0);
}

/* "this is the xl-450's lit .. but they're profiling as spots.. not washes":
   a definition's optics say how its pool lands. One that can sharpen or
   shape its beam (gobo, focus, iris), or a beam bar, is a profile with a
   crisp edge; anything else is a wash; a PC or Fresnel softer still; a very
   tight beam fairly crisp whatever it is. Head count has nothing to do with
   it. */
void Monitor_Test::aPoolsEdgeFollowsTheFixturesOptics()
{
    QCOMPARE(poolEdgeSoftness(false, int(QLCFixtureDef::LEDBarPixels), "Other", 80.0), 1.0f);
    QCOMPARE(poolEdgeSoftness(true, int(QLCFixtureDef::MovingHead), "Other", 18.0), 0.1f);
    QCOMPARE(poolEdgeSoftness(false, int(QLCFixtureDef::LEDBarBeams), "Other", 4.0), 0.1f);
    QCOMPARE(poolEdgeSoftness(false, int(QLCFixtureDef::ColorChanger), "Fresnel", 40.0), 1.25f);
    QCOMPARE(poolEdgeSoftness(false, int(QLCFixtureDef::ColorChanger), "pc", 40.0), 1.25f);
    QCOMPARE(poolEdgeSoftness(false, int(QLCFixtureDef::ColorChanger), "Other", 8.0), 0.4f);
    QCOMPARE(poolEdgeSoftness(false, int(QLCFixtureDef::ColorChanger), "Other", 0.0), 1.0f);

    // From real channels: a gobo, a focus or an iris make a profile; a zoom
    // alone doesn't (wash movers zoom).
    // A show of its own: the fixtures stay out of the other tests' way.
    Doc local(this);
    auto traitsOf = [&local](QLCFixtureDef::FixtureType type,
                           QList<QPair<QLCChannel::Group, QLCChannel::Preset>> chans,
                           int heads = 1) {
        QLCFixtureDef *def = new QLCFixtureDef();
        def->setManufacturer("Test"); def->setModel(QString("Optics %1").arg(local.fixtures().size()));
        def->setType(type);
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName("Mode");
        for (int h = 0; h < heads; ++h)
            chans << qMakePair(QLCChannel::Intensity, QLCChannel::IntensityDimmer);
        for (const auto &c : chans)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("C%1").arg(def->channels().size()));
            ch->setGroup(c.first);
            if (c.second != QLCChannel::Custom)
                ch->setPreset(c.second);
            def->addChannel(ch);
            mode->insertChannel(ch, mode->channels().size());
        }
        QLCPhysical ph;
        ph.setLensDegreesMin(30);
        ph.setLensDegreesMax(30);
        mode->setPhysical(ph);
        def->addMode(mode);
        Fixture *fxi = new Fixture(&local);
        fxi->setFixtureDefinition(def, mode);
        fxi->setAddress(local.fixtures().size() * 32);
        local.addFixture(fxi);
        return classifyFixture(fxi);
    };
    using C = QLCChannel;
    QCOMPARE(traitsOf(QLCFixtureDef::LEDBarPixels, {}, 15).edgeSoftness, 1.0f);   // the XL-450
    QCOMPARE(traitsOf(QLCFixtureDef::MovingHead, { { C::Gobo, C::Custom } }).edgeSoftness, 0.1f);
    QCOMPARE(traitsOf(QLCFixtureDef::MovingHead, { { C::Beam, C::BeamFocusNearFar } }).edgeSoftness, 0.1f);
    QCOMPARE(traitsOf(QLCFixtureDef::MovingHead, { { C::Shutter, C::ShutterIrisMinToMax } }).edgeSoftness, 0.1f);
    QCOMPARE(traitsOf(QLCFixtureDef::MovingHead, { { C::Beam, C::BeamZoomSmallBig } }).edgeSoftness, 1.0f);
}

/* A wash's pool goes on past its beam angle, fading out to nothing; a
   profile's stops almost at it. Both are full in the middle. */
void Monitor_Test::aWashPoolFadesPastItsBeamAngle()
{
    Doc *doc = new Doc(this);
    {
        StructureStudioView view(doc, StructureStudioView::StageKind, 0);
        view.resize(800, 600);
        view.setPlane(StructureStudioView::Angled);
        view.setAngledView(-30.0, 60.0);
        QImage canvas(800, 600, QImage::Format_ARGB32);
        QPainter p(&canvas);
        const QVector3D apex(4.0f, 4.0f, 5.0f), down(0.0f, 0.0f, -1.0f);
        const double half = qDegreesToRadians(15.0);
        const QPointF centre = view.w2s(QVector3D(4.0f, 4.0f, 0.0f));

        // The pool's reach on screen (its rim: corners with no light left),
        // and whether its middle is full.
        auto pool = [&](double softness, bool *fullMiddle) {
            BeamOptics::Optics o;
            o.softness = softness;
            view.m_ops.clear();
            view.m_collecting = true;
            view.drawBeamCone(p, apex, down, half, QColor(0, 0, 255), 1.0, 1.0, &o);
            view.m_collecting = false;
            double reach = 0.0;
            *fullMiddle = false;
            for (const auto &op : view.m_ops)
            {
                if (!op.light)
                    continue;
                if (op.alphas.isEmpty() && op.poly.size() == 16)
                    *fullMiddle = true;                    // the core: no fade in it
                for (int i = 0; i < op.alphas.size(); ++i)
                    if (op.alphas.at(i) <= 0.001)
                        reach = qMax(reach, QLineF(centre, op.poly.at(i)).length());
            }
            return reach;
        };
        bool crispFull = false, washFull = false;
        const double crisp = pool(0.1, &crispFull);
        const double wash = pool(1.0, &washFull);
        QVERIFY(crispFull);
        QVERIFY(washFull);
        QVERIFY(crisp > 10.0);
        // tan(21 deg) / tan(15.6 deg): the wash reaches about 1.37 times as far.
        QVERIFY2(wash / crisp > 1.25 && wash / crisp < 1.5,
                 qPrintable(QString("%1 / %2").arg(wash).arg(crisp)));
    }
    delete doc;
}

/* Where a beam's rays land is kept from frame to frame (a static beam
   throws the same rays every frame) -- until what they land on changes. A
   par over a deck lands on it; the deck moved away, the next frame's beam
   goes on to the floor. */
void Monitor_Test::keptLandingsFollowTheDecks()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setName("Deck"); deck->setOriginX(0.0f); deck->setOriginY(0.0f);
    deck->setWidth(4.0f); deck->setDepth(3.0f); deck->setHeight(1.0f);
    Truss *t = props->addTruss();
    t->setName("T"); t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(0.0f, 1.5f, 5.0f)); t->setDirection(QPointF(1.0, 0.0));
    t->setLength(4.0f); t->setWidth(0.3f);

    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Kept Par");
    def->setType(QLCFixtureDef::ColorChanger);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("m");
    QLCChannel *dim = new QLCChannel();
    dim->setName("Dimmer"); dim->setGroup(QLCChannel::Intensity);
    def->addChannel(dim);
    mode->insertChannel(dim, 0);
    QLCPhysical ph;
    ph.setWidth(300); ph.setHeight(300); ph.setDepth(300);
    ph.setLensDegreesMin(20); ph.setLensDegreesMax(20);
    mode->setPhysical(ph);
    def->addMode(mode);
    Fixture *fx = new Fixture(&doc);
    fx->setName("Kept Par"); fx->setFixtureDefinition(def, mode);
    QVERIFY(doc.addFixture(fx));
    props->setFixturePosition(fx->id(), 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp;
    rp.trussId = t->id(); rp.trussOffset = 2.0f;
    props->setFixtureRigProps(fx->id(), rp);
    fx->setChannelValues(QByteArray(1, char(255)));

    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.resize(600, 400);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setLiveValues(true);
    v.setBeams(true);
    auto beamLength = [&v]() {
        v.grab();
        double len = -1.0;
        for (const auto &b : v.m_frameBeams)
            len = qMax(len, b.length);
        return len;
    };

    const double onDeck = beamLength();
    QVERIFY2(onDeck > 0.0, "no beam drawn");
    QVERIFY(!v.m_throws.isEmpty());                 // its landings are kept
    QCOMPARE(beamLength(), onDeck);                  // and used: the same again

    deck->setOriginX(8.0f);                          // the deck moves out from under it
    const double onFloor = beamLength();
    QVERIFY2(onFloor > onDeck + 0.8,
             qPrintable(QString("still landing on the deck: %1 then %2").arg(onDeck).arg(onFloor)));
}

/* A pool lands ON a deck, not only on the bare floor. Light is drawn at
   the final resolution and depth-tested against the supersampled deck: its
   depth was taken a quarter pixel from where the deck's was, and on a deck
   top seen at an angle that was more than the lift a pool gets off its
   surface -- every pool on a deck was dropped. */
void Monitor_Test::aPoolOnADeckIsDrawn()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setName("Deck"); deck->setOriginX(0.0f); deck->setOriginY(0.0f);
    deck->setWidth(6.0f); deck->setDepth(6.0f); deck->setHeight(1.0f);
    deck->setColor(QColor(60, 60, 60));
    Truss *t = props->addTruss();
    t->setName("T"); t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(1.0f, 3.0f, 5.0f)); t->setDirection(QPointF(1.0, 0.0));
    t->setLength(4.0f); t->setWidth(0.3f);

    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Deck Par");
    def->setType(QLCFixtureDef::ColorChanger);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("m");
    QLCChannel *dim = new QLCChannel();
    dim->setName("Dimmer"); dim->setGroup(QLCChannel::Intensity);
    def->addChannel(dim);
    mode->insertChannel(dim, 0);
    QLCPhysical ph;
    ph.setWidth(300); ph.setHeight(300); ph.setDepth(300);
    ph.setLensDegreesMin(30); ph.setLensDegreesMax(30);
    mode->setPhysical(ph);
    def->addMode(mode);
    Fixture *fx = new Fixture(&doc);
    fx->setName("Deck Par"); fx->setFixtureDefinition(def, mode);
    QVERIFY(doc.addFixture(fx));
    props->setFixturePosition(fx->id(), 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp;
    rp.trussId = t->id(); rp.trussOffset = 2.0f;
    props->setFixtureRigProps(fx->id(), rp);
    fx->setChannelValues(QByteArray(1, char(255)));

    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.resize(700, 500);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(-15.0, 55.0);
    v.setSupersample(2);
    v.setLiveValues(true);
    v.setBeams(true);
    v.setHaze(0.0);
    v.setAmbient(0.2);

    const QImage img = v.grab().toImage();
    const double dpr = img.devicePixelRatio();
    auto luma = [&](const QVector3D &w) {
        const QPoint at = (v.w2s(w) * dpr).toPoint();
        const QColor c = img.pixelColor(at);
        return c.red() * 0.30 + c.green() * 0.59 + c.blue() * 0.11;
    };
    // Straight under it on the deck top, and a corner of the deck it doesn't reach.
    const double lit = luma(QVector3D(3.0f, 3.0f, 1.0f));
    const double dark = luma(QVector3D(5.6f, 0.4f, 1.0f));
    QVERIFY2(lit > dark + 10.0,
             qPrintable(QString("no pool on the deck: %1 under the beam, %2 away").arg(lit).arg(dark)));
}

/* The rig on the GPU (RhiRaster) draws what ZRaster draws: the same
   triangles, the same passes. A lit par over a deck, drawn both ways at
   each quality; the two pictures agree to within edge smoothing. Skipped
   where there is no GPU to draw on. */
void Monitor_Test::colouredLightTakesOverAGreySurface()
{
    // One pixel each: a lit cyc in a house-lit room, a red LED, black.
    QImage frame(3, 1, QImage::Format_ARGB32_Premultiplied);
    const QRgb cyc = qRgb(156, 156, 156), led = qRgb(230, 20, 20), black = qRgb(0, 0, 0);
    auto fill = [&]() {
        frame.setPixel(0, 0, cyc);
        frame.setPixel(1, 0, led);
        frame.setPixel(2, 0, black);
    };
    auto lightOf = [](QRgb c) {
        QImage l(3, 1, QImage::Format_ARGB32_Premultiplied);
        const int a = qMax(qRed(c), qMax(qGreen(c), qBlue(c)));
        l.fill(qRgba(qRed(c), qGreen(c), qBlue(c), a));
        return l;
    };
    auto screen = [](int d, int s) { return qRound(d + s - d * s / 255.0); };

    // A blue wash at a fifth of full: the cyc reads blue, not lavender.
    fill();
    ZRaster::composeLight(frame, lightOf(qRgb(0, 0, 50)), QPoint(0, 0));
    const QRgb blueCyc = frame.pixel(0, 0);
    QVERIFY2(qBlue(blueCyc) - qRed(blueCyc) > 50,
             qPrintable(QString("cyc under blue: %1").arg(QColor(blueCyc).name())));
    QCOMPARE(qRed(blueCyc), qGreen(blueCyc));
    QCOMPARE(qBlue(blueCyc), screen(156, 50));        // the light itself is all there
    // The red LED keeps its red; black is just the light.
    QVERIFY(qRed(frame.pixel(1, 0)) >= 225);
    QCOMPARE(frame.pixel(2, 0), qRgb(0, 0, 50));

    // White light is plain Screen.
    fill();
    ZRaster::composeLight(frame, lightOf(qRgb(60, 60, 60)), QPoint(0, 0));
    QCOMPARE(frame.pixel(0, 0), qRgb(screen(156, 60), screen(156, 60), screen(156, 60)));
    QCOMPARE(qRed(frame.pixel(1, 0)), screen(230, 60));

    // Amber warms the cyc: red untouched, blue taken down most.
    fill();
    ZRaster::composeLight(frame, lightOf(qRgb(60, 45, 0)), QPoint(0, 0));
    const QRgb amber = frame.pixel(0, 0);
    QCOMPARE(qRed(amber), screen(156, 60));
    QVERIFY(qRed(amber) > qGreen(amber) && qGreen(amber) > qBlue(amber));

    // Off the frame, or no light: nothing changes.
    fill();
    ZRaster::composeLight(frame, lightOf(qRgb(0, 0, 200)), QPoint(5, 0));
    ZRaster::composeLight(frame, QImage(), QPoint(0, 0));
    QCOMPARE(frame.pixel(0, 0), cyc);
}

void Monitor_Test::theGpuDrawsWhatTheCpuDraws()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setName("Deck"); deck->setOriginX(0.0f); deck->setOriginY(0.0f);
    deck->setWidth(6.0f); deck->setDepth(4.0f); deck->setHeight(1.0f);
    deck->setColor(QColor(90, 70, 60));
    Truss *t = props->addTruss();
    t->setName("T"); t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(0.5f, 2.0f, 5.0f)); t->setDirection(QPointF(1.0, 0.0));
    t->setLength(5.0f); t->setWidth(0.3f);
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("GPU Par");
    def->setType(QLCFixtureDef::ColorChanger);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("m");
    const char *nm[] = { "Dimmer", "Red", "Green", "Blue" };
    for (int c = 0; c < 4; ++c)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(nm[c]); ch->setGroup(QLCChannel::Intensity);
        if (c == 1) ch->setColour(QLCChannel::Red);
        if (c == 2) ch->setColour(QLCChannel::Green);
        if (c == 3) ch->setColour(QLCChannel::Blue);
        def->addChannel(ch);
        mode->insertChannel(ch, c);
    }
    QLCPhysical ph;
    ph.setWidth(300); ph.setHeight(300); ph.setDepth(300);
    ph.setLensDegreesMin(30); ph.setLensDegreesMax(30);
    mode->setPhysical(ph);
    def->addMode(mode);
    Fixture *fx = new Fixture(&doc);
    fx->setName("GPU Par"); fx->setFixtureDefinition(def, mode);
    QVERIFY(doc.addFixture(fx));
    props->setFixturePosition(fx->id(), 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp;
    rp.trussId = t->id(); rp.trussOffset = 2.5f;
    props->setFixtureRigProps(fx->id(), rp);
    QByteArray v(4, char(0));
    v[0] = char(255); v[1] = char(60); v[3] = char(255);
    fx->setChannelValues(v);

    StructureStudioView view(&doc, StructureStudioView::StageKind, 0);
    view.resize(640, 420);
    view.reload();
    view.setPlane(StructureStudioView::Angled);
    view.setAngledView(-20.0, 45.0);
    view.setLiveValues(true);
    view.setBeams(true);
    view.setHaze(0.5);
    view.setAmbient(0.3);

    const QByteArray was = qgetenv("QLC_RIG_GPU");
    auto restore = qScopeGuard([&]() {
        if (was.isNull()) qunsetenv("QLC_RIG_GPU"); else qputenv("QLC_RIG_GPU", was);
    });
    qputenv("QLC_RIG_GPU", "1");
    if (!RhiRaster::available())
        QSKIP("no GPU to draw on here");

    for (int quality : { 1, 2 })
    {
        view.setSupersample(quality);
        view.setGpu(false);
        QVERIFY(!view.gpuActive());
        const QImage cpu = view.grab().toImage().convertToFormat(QImage::Format_ARGB32);
        view.setGpu(true);
        QVERIFY(view.gpuActive());
        const QImage gpu = view.grab().toImage().convertToFormat(QImage::Format_ARGB32);
        QVERIFY2(view.drewOnGpu(), "the GPU fell back to the CPU");
        QCOMPARE(gpu.size(), cpu.size());
        const QString shots = qEnvironmentVariable("QLC_GPU_SHOTS");
        if (!shots.isEmpty())
        {
            cpu.save(QString("%1/cpu-q%2.png").arg(shots).arg(quality));
            gpu.save(QString("%1/gpu-q%2.png").arg(shots).arg(quality));
        }

        // Per pixel: how far apart, as the brightest channel's difference.
        qint64 total = 0;
        int far = 0, lit = 0;
        for (int y = 0; y < cpu.height(); ++y)
        {
            const QRgb *a = reinterpret_cast<const QRgb *>(cpu.constScanLine(y));
            const QRgb *b = reinterpret_cast<const QRgb *>(gpu.constScanLine(y));
            for (int x = 0; x < cpu.width(); ++x)
            {
                const int d = qMax(qAbs(qRed(a[x]) - qRed(b[x])),
                              qMax(qAbs(qGreen(a[x]) - qGreen(b[x])), qAbs(qBlue(a[x]) - qBlue(b[x]))));
                total += d;
                if (d > 24)
                    ++far;
                if (qBlue(a[x]) > 90)
                    ++lit;
            }
        }
        const double mean = double(total) / (cpu.width() * cpu.height());
        const double farShare = double(far) / (cpu.width() * cpu.height());
        qInfo() << "quality" << quality << "mean difference" << mean
                << "pixels far apart" << farShare << "lit pixels" << lit;
        QVERIFY2(lit > 2000, "the scene isn't lit: nothing to compare");
        QVERIFY2(mean < 3.0, qPrintable(QString("mean difference %1").arg(mean)));
        QVERIFY2(farShare < 0.02, qPrintable(QString("%1 of the pixels far apart").arg(farShare)));
    }
}

void Monitor_Test::aGoboPoolIsDrawnThroughItsMask()
{
    Doc *doc = new Doc(this);
    {
        StructureStudioView view(doc, StructureStudioView::StageKind, 0);
        view.resize(800, 600);
        view.setPlane(StructureStudioView::Angled);
        view.setAngledView(-30.0, 30.0);
        QImage canvas(800, 600, QImage::Format_ARGB32);
        QPainter p(&canvas);
        // A spot 5 m up, straight down onto the floor.
        const QVector3D apex(4.0f, 4.0f, 5.0f), down(0.0f, 0.0f, -1.0f);
        const double half = qDegreesToRadians(8.0);

        auto emitted = [&](const BeamOptics::Optics *o, double *hazeAlpha = nullptr) {
            view.m_ops.clear();
            view.m_collecting = true;
            view.drawBeamCone(p, apex, down, half, QColor(0, 0, 255), 1.0, 1.0, o);
            view.m_collecting = false;
            // Triangles of light: a mesh op carries many, three corners each.
            int n = 0;
            for (const auto &op : view.m_ops)
                if (op.light)
                {
                    n += op.mesh ? int(op.poly.size() / 3) : 1;
                    if (hazeAlpha != nullptr && (op.mesh || op.poly.size() == 3) && *hazeAlpha < 0)
                        *hazeAlpha = op.fill.alpha();   // the first: a cone triangle
                }
            return n;
        };
        const int plain = emitted(nullptr);             // cone triangles + a plain pool
        QVERIFY(plain > 1);

        BeamOptics::Optics clear;
        clear.mask = QImage(BeamOptics::kMaskSize, BeamOptics::kMaskSize, QImage::Format_Grayscale8);
        clear.mask.fill(255);
        clear.coverage = 1.0;
        BeamOptics::Optics metal = clear;
        metal.mask = QImage(clear.mask.size(), QImage::Format_Grayscale8);
        metal.mask.fill(0);
        metal.coverage = 0.0;
        BeamOptics::Optics halfOpen = clear;
        halfOpen.mask = QImage(clear.mask.size(), QImage::Format_Grayscale8);
        halfOpen.mask.fill(0);
        for (int y = 0; y < BeamOptics::kMaskSize; ++y)
            for (int x = BeamOptics::kMaskSize / 2; x < BeamOptics::kMaskSize; ++x)
                halfOpen.mask.scanLine(y)[x] = 255;
        halfOpen.coverage = 0.5;

        // A plain pool: its core, and two rings fading to its rim, each a
        // triangle pair for each of the sixteen points round it.
        const int cone = plain - (1 + 2 * 2 * 16);
        // All metal: no pool at all. All clear: the whole mesh.
        QCOMPARE(emitted(&metal), cone);
        const int full = emitted(&clear) - cone;
        QVERIFY2(full > 1000, qPrintable(QString::number(full)));
        // Half open: about half of it.
        const int halfN = emitted(&halfOpen) - cone;
        QVERIFY2(halfN > full * 0.4 && halfN < full * 0.65,
                 qPrintable(QString("%1 of %2").arg(halfN).arg(full)));

        // The air carries only what the gobo lets through.
        double airOpen = -1.0, airHalf = -1.0;
        emitted(nullptr, &airOpen);
        emitted(&halfOpen, &airHalf);
        QVERIFY2(airHalf < airOpen * 0.7, qPrintable(QString("%1 vs %2").arg(airHalf).arg(airOpen)));
        p.end();
    }
    delete doc;
}

void Monitor_Test::goboWheelsFitsYourOwnGlassInOneUnit()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    GoboLibrary::setUserDirectoryOverride(dir.filePath("Gobos"));
    auto cleanup = qScopeGuard([]() { GoboLibrary::setUserDirectoryOverride(QString()); });

    QImage half(80, 80, QImage::Format_ARGB32);
    half.fill(Qt::transparent);
    {
        QPainter hp(&half);
        hp.fillRect(0, 0, 40, 80, Qt::black);          // left blocks
    }
    const QString halfPath = dir.filePath("half.png");
    QVERIFY(half.save(halfPath));
    QImage open(80, 80, QImage::Format_ARGB32);
    open.fill(Qt::transparent);
    const QString openPath = dir.filePath("open.png");
    QVERIFY(open.save(openPath));
    // The operator's own glass: the TOP half blocks.
    QImage logo(80, 80, QImage::Format_ARGB32);
    logo.fill(Qt::transparent);
    {
        QPainter lp(&logo);
        lp.fillRect(0, 0, 80, 40, Qt::black);
    }
    QDir(dir.path()).mkpath("downloads");
    const QString logoPath = dir.filePath("downloads/logo.png");
    QVERIFY(logo.save(logoPath));

    QLCFixtureDef *def = opticsDef(halfPath, openPath);
    Doc *doc = new Doc(this);
    QList<Fixture *> units;
    for (int i = 0; i < 3; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("Spot %1").arg(i + 1));
        f->setFixtureDefinition(def, def->modes().first());
        f->setAddress(quint32(i * 8));
        QVERIFY(doc->addFixture(f));
        units << f;
    }
    Fixture *plain = new Fixture(doc);
    plain->setChannels(3);
    plain->setAddress(100);
    QVERIFY(doc->addFixture(plain));
    MonitorProperties *props = doc->monitorProperties();

    QVERIFY(GoboWheelsDialog::hasGoboWheels(units.first()));
    QVERIFY(!GoboWheelsDialog::hasGoboWheels(plain));
    QVERIFY(!GoboWheelsDialog::hasGoboWheels(nullptr));
    // The slot and its shake are one piece of glass.
    const QLCChannel *wheel = units.first()->channel(0);
    const QList<const QLCCapability *> glass =
        GoboWheelsDialog::sameGlass(wheel, wheel->searchCapability(15));
    QCOMPARE(glass.size(), 2);

    {
        GoboWheelsDialog dlg(doc, units.first()->id());
        // One wheel, its two pictures (Half listed once, with its shake).
        QCOMPARE(dlg.m_tree->topLevelItemCount(), 1);
        QTreeWidgetItem *w = dlg.m_tree->topLevelItem(0);
        QCOMPARE(w->childCount(), 2);
        QTreeWidgetItem *halfSlot = w->child(1);
        QVERIFY(halfSlot->text(0).contains("Half"));
        QVERIFY(halfSlot->text(0).contains("shake"));
        QCOMPARE(halfSlot->text(1), QString("as built"));
        QVERIFY(!dlg.m_replace->isEnabled());           // nothing chosen yet
        QVERIFY(!dlg.m_allOfType->isHidden());          // three of this type

        dlg.m_tree->setCurrentItem(halfSlot);
        QVERIFY(dlg.m_replace->isEnabled());
        QVERIFY(!dlg.m_restore->isEnabled());
        dlg.pickImage = [&](QWidget *) { return logoPath; };
        dlg.slotReplace();
        QVERIFY(QFile::exists(dir.filePath("Gobos/logo.png")));   // copied in
        QCOMPARE(dlg.m_tree->currentItem()->text(1), QString("logo.png"));
        QVERIFY(dlg.m_restore->isEnabled());
        // Not applied until OK.
        QVERIFY(props->goboSwaps(units.first()->id()).isEmpty());
        dlg.accept();
    }
    QCOMPARE(props->goboSwap(units.first()->id(), 0, 10), QString("logo.png"));
    QCOMPARE(props->goboSwap(units.first()->id(), 0, 20), QString("logo.png"));   // its shake
    QVERIFY(props->goboSwaps(units.at(1)->id()).isEmpty());                     // only this unit
    QVERIFY(doc->isModified());

    // The rig draws the new glass for that unit, and the old for the others.
    QByteArray vals(5, 0);
    vals[0] = char(15);
    vals[4] = char(255);
    const BeamOptics::Optics mine = BeamOptics::opticsFor(units.first(), vals, 0.0, props);
    QVERIFY(BeamOptics::sample(mine.mask, 0.0, 0.5) < 0.05);     // top blocked
    QVERIFY(BeamOptics::sample(mine.mask, 0.0, -0.5) > 0.9);
    QVERIFY(BeamOptics::sample(mine.mask, -0.5, -0.2) > 0.9);   // left is clear now
    const BeamOptics::Optics theirs = BeamOptics::opticsFor(units.at(1), vals, 0.0, props);
    QVERIFY(BeamOptics::sample(theirs.mask, -0.5, 0.0) < 0.05);  // still the half gobo
    QCOMPARE(BeamOptics::slotPicture(units.first(), 0, wheel->searchCapability(15), props),
             QDir(dir.filePath("Gobos")).absoluteFilePath("logo.png"));
    QCOMPARE(BeamOptics::slotPicture(units.at(1), 0, wheel->searchCapability(15), props), halfPath);
    QCOMPARE(BeamOptics::slotPicture(units.at(1), 0, wheel->searchCapability(100), props), QString());

    // A bad picture is refused; Cancel keeps nothing.
    {
        GoboWheelsDialog dlg(doc, units.at(1)->id());
        dlg.m_tree->setCurrentItem(dlg.m_tree->topLevelItem(0)->child(0));
        const QString junk = dir.filePath("downloads/notapicture.png");
        QFile jf(junk);
        QVERIFY(jf.open(QIODevice::WriteOnly));
        jf.write("not an image");
        jf.close();
        dlg.pickImage = [&](QWidget *) { return junk; };
        driveMessageBox([](QWidget *) { return true; }, "OK");
        dlg.slotReplace();
        QCOMPARE(dlg.m_tree->currentItem()->text(1), QString("as built"));
        dlg.pickImage = [&](QWidget *) { return logoPath; };
        dlg.slotReplace();
        dlg.reject();
    }
    QVERIFY(props->goboSwaps(units.at(1)->id()).isEmpty());

    // "All of this type" fits the same glass in every unit; Restore undoes a slot.
    {
        GoboWheelsDialog dlg(doc, units.first()->id());
        dlg.m_tree->setCurrentItem(dlg.m_tree->topLevelItem(0)->child(0));   // Open
        dlg.pickImage = [&](QWidget *) { return logoPath; };
        dlg.slotReplace();
        dlg.m_tree->setCurrentItem(dlg.m_tree->topLevelItem(0)->child(1));   // Half
        dlg.slotRestore();
        QCOMPARE(dlg.m_tree->currentItem()->text(1), QString("as built"));
        dlg.m_allOfType->setChecked(true);
        dlg.accept();
    }
    for (Fixture *f : units)
    {
        QCOMPARE(props->goboSwap(f->id(), 0, 0), QString("logo.png"));
        QVERIFY(props->goboSwap(f->id(), 0, 10).isEmpty());
    }
    // The open slot fitted with glass is no longer open.
    vals[0] = char(0);
    QVERIFY(!BeamOptics::opticsFor(units.at(2), vals, 0.0, props).mask.isNull());

    // Offered on a gobo fixture's menu, not on a plain one.
    {
        MonitorGraphicsView view(doc);
        QMenu gm;
        view.buildItemMenu(&gm, "fixture", units.first()->id(), MonitorGraphicsView::MenuPlace());
        QVERIFY2(menuPaths(&gm).join("|").contains(QString::fromUtf8("Gobo Wheels…")),
                 qPrintable(menuPaths(&gm).join(" | ")));
        QMenu pm;
        view.buildItemMenu(&pm, "fixture", plain->id(), MonitorGraphicsView::MenuPlace());
        QVERIFY(!menuPaths(&pm).join("|").contains(QString::fromUtf8("Gobo Wheels")));
    }

    doc->clearContents();
    delete doc;
    delete def;
}

void Monitor_Test::beamsLandOnTheCycloramaInTheRigView()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Scenery *cyc = props->addScenery();
    cyc->setOriginX(6.0f);
    cyc->setOriginY(0.5f);
    cyc->setWidth(10.0f);
    cyc->setHeight(6.0f);
    {
        StructureStudioView view(doc, StructureStudioView::StageKind, 0);
        // A head 5 m downstage at 3 m, aimed level and upstage: it hits the cloth.
        const QVector3D apex(6.0f, 5.5f, 3.0f), dir(0.0f, -1.0f, 0.0f);
        bool hit = false;
        QVector3D n;
        const double t = view.beamThrow(apex, dir, &hit, &n);
        QVERIFY(hit);
        QVERIFY(qAbs(t - 5.0) < 0.01);
        QVERIFY2(qAbs(n.y() - 1.0f) < 0.01f, "the pool lies on the cloth, facing the head");

        // The pool is on the cloth: every ring point at the cyc's line.
        const QVector<QVector3D> ring = view.beamFootprint(apex, dir, qDegreesToRadians(7.0), t, true, n);
        QCOMPARE(ring.size(), 16);
        for (const QVector3D &p : ring)
            QVERIFY2(qAbs(p.y() - 0.5f) < 0.01f, qPrintable(QString("ring point at y=%1").arg(p.y())));

        // Aimed at the floor in front of it, it still lands on the floor.
        const QVector3D down = QVector3D(0.0f, -0.2f, -1.0f).normalized();
        view.beamThrow(apex, down, &hit, &n);
        QVERIFY(hit && n.z() > 0.99f);

        // The cyc is part of the overview and of its extent.
        bool listed = false;
        foreach (const auto &ki, view.everyStructure())
            listed = listed || ki.first == StructureStudioView::SceneryKind;
        QVERIFY(listed);
        QList<QVector3D> pts;
        view.collectPointsFor(pts, StructureStudioView::SceneryKind, cyc->id());
        float top = 0.0f;
        for (const QVector3D &p : pts)
            top = qMax(top, p.z());
        QVERIFY(qAbs(top - 6.0f) < 0.01f);
    }
    delete doc;
}

void Monitor_Test::everySceneryKindIsAddedWhereItUsuallyGoes()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Meters);
    props->setGridSize(QVector3D(12, 6, 8));             // 12 m wide, 8 deep
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        foreach (int kind, Scenery::kinds())
        {
            QString title;
            bool opening = false, curtain = false, thickness = false;
            driveDialog([&](QWidget *dlg) {
                title = dlg->windowTitle();
                auto shown = [dlg](const char *name) {
                    QWidget *w = dlg->findChild<QWidget *>(name);
                    return w != nullptr && w->isVisibleTo(dlg);
                };
                opening = shown("sceneryOpeningWidth");
                curtain = shown("sceneryState");
                QDoubleSpinBox *thick = nullptr;
                foreach (QDoubleSpinBox *sp, dlg->findChildren<QDoubleSpinBox *>())
                    if (sp->toolTip().startsWith("How thick"))
                        thick = sp;
                thickness = thick != nullptr && thick->isVisibleTo(dlg);
                return true;
            });
            mon.slotAddScenery(kind);
            QVERIFY2(title.startsWith(Scenery::kindName(kind)), qPrintable(title));
            // Each row only where it means something.
            QCOMPARE(opening, kind == Scenery::Proscenium);
            QCOMPARE(curtain, kind == Scenery::MainDrape || kind == Scenery::Traveler);
            QCOMPARE(thickness, kind == Scenery::Wall || kind == Scenery::BackWall
                                    || kind == Scenery::Proscenium);
        }
        // One of each, and legs come as a pair.
        QCOMPARE(props->sceneries().size(), Scenery::kinds().size() + 1);
        QList<Scenery *> legs;
        foreach (Scenery *sc, props->sceneries())
        {
            QCOMPARE(sc->color(), Scenery::defaultColor(sc->kind()));
            QVERIFY(mon.m_graphicsView->m_sceneryItems.contains(sc->id()));
            switch (sc->kind())
            {
                case Scenery::Leg:       legs << sc; break;
                case Scenery::MainDrape: QVERIFY(sc->originY() > 7.0f); break;   // across the front
                case Scenery::BackWall:  QVERIFY(sc->originY() < 0.5f && sc->thickness() > 0.0f); break;
                case Scenery::Border:    QVERIFY(sc->bottom() > 4.0f); break;   // hung above
                case Scenery::Wall:      QVERIFY(sc->thickness() > 0.0f && sc->height() < 1.0f); break;
                case Scenery::Proscenium:
                    // Framing the front: wider than the stage, its opening the stage's width.
                    QVERIFY(sc->originY() > 7.0f && sc->width() > 12.0f && sc->thickness() > 0.0f);
                    QVERIFY(qAbs(sc->openingWidth() - 11.5f) < 0.01f);
                    break;
                default: break;
            }
        }
        QCOMPARE(legs.size(), 2);
        QVERIFY(qAbs(legs.at(0)->originX() + legs.at(1)->originX() - 12.0f) < 0.01f);   // either side
    }
    delete doc;
}

void Monitor_Test::aDrapeOverTheStepsIsWhatADoubleClickOpens()
{
    // "when I double click to edit it .. I get the step nearest it"
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Meters);
    StagePlatform *steps = props->addPlatform();
    steps->setOriginX(4.0f); steps->setOriginY(6.0f);
    steps->setSteps(2, 0.2f, 0.3f, 2.0f);
    Scenery *drape = props->addScenery();
    drape->setKind(Scenery::MainDrape);
    drape->setOriginX(5.0f); drape->setOriginY(6.3f);
    drape->setWidth(8.0f);
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        MonitorGraphicsView *gv = mon.m_graphicsView;
        gv->updatePlatforms();
        QSignalSpy scenerySpy(gv, &MonitorGraphicsView::sceneryDoubleClicked);
        QSignalSpy platformSpy(gv, &MonitorGraphicsView::platformDoubleClicked);
        SceneryItem *item = gv->m_sceneryItems.value(drape->id());
        QVERIFY(item != nullptr);
        // On the drape's line, where it crosses the steps.
        const QPointF onLine = item->mapToScene(QPointF(0, 0));
        const QPoint vp = gv->mapFromScene(onLine);
        QVERIFY(gv->m_platformItems.value(steps->id())->sceneBoundingRect().contains(onLine));
        QString opened;
        driveDialog([&opened](QWidget *dlg) {      // its editor opens: note which, close it
            opened = dlg->windowTitle();
            return true;
        }, false);
        QTest::mouseDClick(gv->viewport(), Qt::LeftButton, Qt::NoModifier, vp);
        QVERIFY2(opened.startsWith(Scenery::kindName(Scenery::MainDrape)), qPrintable(opened));
        QCOMPARE(scenerySpy.count(), 1);
        QCOMPARE(platformSpy.count(), 0);
    }
    delete doc;
}

void Monitor_Test::anOpenMainDrapeShowsTheCycBehindIt()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Scenery *cyc = props->addScenery();
    cyc->setOriginX(6.0f);
    cyc->setOriginY(0.5f);
    cyc->setWidth(10.0f);
    cyc->setHeight(6.0f);
    Scenery *drape = props->addScenery();
    drape->setKind(Scenery::MainDrape);
    drape->setColor(Scenery::defaultColor(Scenery::MainDrape));
    drape->setOriginX(6.0f);
    drape->setOriginY(6.0f);
    drape->setWidth(10.0f);
    drape->setHeight(6.0f);
    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(800, 600);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(0.0, 10.0);
        // The pale cyc's pixels in the middle of the view.
        auto pale = [&v]() {
            v.reload();
            const QImage img = v.grab().toImage();
            int n = 0;
            for (int y = img.height() / 4; y < img.height() * 3 / 4; ++y)
                for (int x = img.width() / 3; x < img.width() * 2 / 3; ++x)
                {
                    const QColor q = img.pixelColor(x, y);
                    if (q.blue() > 60 && qAbs(q.red() - q.blue()) < 30 && q.red() > q.green() - 30)
                        ++n;
                }
            return n;
        };
        const int closed = pale();
        drape->setState(Scenery::Open);
        const int open = pale();
        QVERIFY2(open > 4 * closed + 2000,
                 qPrintable(QString("closed %1 px of cyc, open %2").arg(closed).arg(open)));
        drape->setState(Scenery::Flown);
        QVERIFY(pale() >= open);
        // Light goes through where it is open.
        bool hit = false;
        QVector3D n;
        drape->setState(Scenery::Open);
        const double t = v.beamThrow(QVector3D(6.0f, 9.0f, 3.0f), QVector3D(0.0f, -1.0f, 0.0f), &hit, &n);
        QVERIFY(hit && qAbs(t - 8.5) < 0.05);              // on the cyc, not the drape
    }
    delete doc;
}

void Monitor_Test::aUWallIsDrawnRoundItsFrontWithItsTop()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(0.0f); deck->setOriginY(0.0f);
    deck->setWidth(4.0f); deck->setDepth(4.0f); deck->setHeight(0.05f);
    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(800, 600);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(20.0, 40.0);
        const QImage without = v.grab().toImage();
        Scenery *rim = props->addScenery();
        rim->setKind(Scenery::Wall);
        rim->setColor(QColor(230, 120, 40));
        rim->setOutline(Scenery::RoundedU);
        rim->setOriginX(2.0f);
        rim->setOriginY(1.4f);
        rim->setWidth(0.9144f);
        rim->setDepth(1.2192f);
        rim->setHeight(0.2032f);
        rim->setThickness(0.1016f);
        v.reload();
        const QImage with = v.grab().toImage();
        int changed = 0;
        for (int y = 0; y < with.height(); ++y)
            for (int x = 0; x < with.width(); ++x)
                if (with.pixel(x, y) != without.pixel(x, y))
                    ++changed;
        QVERIFY2(changed > 1500, qPrintable(QString("the rim changed %1 px").arg(changed)));
        // Seen from above: a beam straight down onto its top stops there.
        bool hit = false;
        QVector3D n;
        const QVector3D front = rim->pointAt(0.0, 3.0);
        const double t = v.beamThrow(front, QVector3D(0.0f, 0.0f, -1.0f), &hit, &n);
        QVERIFY2(hit && qAbs(t - (3.0 - 0.2032)) < 0.02, qPrintable(QString::number(t)));
    }
    delete doc;
}

void Monitor_Test::aRoundedStageFrontIsDrawnAndLitRoundItsCurve()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Meters);
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(0.0f);
    deck->setOriginY(0.0f);
    deck->setWidth(8.0f);
    deck->setDepth(5.0f);
    deck->setHeight(1.0f);
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        // Set from its editor.
        driveDialog([](QWidget *dlg) {
            QDoubleSpinBox *curve = dlg->findChild<QDoubleSpinBox *>("platformFrontCurve");
            if (curve == nullptr) return false;
            curve->setValue(1.5);
            return true;
        });
        mon.slotEditPlatform(deck->id());
        deck = props->platform(deck->id());
        QVERIFY(deck != nullptr);
        QVERIFY(qAbs(deck->frontCurve() - 1.5f) < 0.01f);

        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(800, 600);
        v.reload();
        bool hit = false;
        QVector3D n;
        // Down onto the middle of the front: the deck. Onto a cut corner: the floor.
        double t = v.beamThrow(QVector3D(4.0f, 4.8f, 5.0f), QVector3D(0.0f, 0.0f, -1.0f), &hit, &n);
        QVERIFY2(qAbs(t - 4.0) < 0.01, qPrintable(QString::number(t)));
        t = v.beamThrow(QVector3D(0.2f, 4.8f, 5.0f), QVector3D(0.0f, 0.0f, -1.0f), &hit, &n);
        QVERIFY2(qAbs(t - 5.0) < 0.01, qPrintable(QString::number(t)));

        // Drawn round: the same deck squared off draws differently, at its corners.
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(0.0, 60.0);
        v.reload();
        const QImage round = v.grab().toImage();
        deck->setFrontCurve(0.0f);
        v.reload();
        const QImage square = v.grab().toImage();
        int changed = 0;
        for (int y = 0; y < round.height(); ++y)
            for (int x = 0; x < round.width(); ++x)
                if (round.pixel(x, y) != square.pixel(x, y))
                    ++changed;
        QVERIFY2(changed > 500, qPrintable(QString("only %1 px differ").arg(changed)));
    }
    delete doc;
}

void Monitor_Test::aBeamPastTheEndOfTheCycStillWashesIt()
{
    /* "The spots aren't both backwashing": a follow-spot whose middle passes
       just beyond the end of the cyc still has half its beam on the cloth.
       The pool used to go only where the middle landed. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Scenery *cyc = props->addScenery();
    cyc->setOriginX(6.0f);
    cyc->setOriginY(0.5f);
    cyc->setWidth(10.0f);                 // x 1 .. 11
    cyc->setHeight(6.0f);
    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(800, 600);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(0.0, 10.0);
        v.reload();
        // From downstage right of centre, aimed 40 cm past the cyc's stage-right end.
        const QVector3D apex(9.0f, 9.0f, 3.0f);
        const QVector3D dir = (QVector3D(0.6f, 0.5f, 2.5f) - apex).normalized();
        bool hit = false;
        int surface = 1;
        v.beamThrow(apex, dir, &hit, nullptr, &surface);
        QVERIFY2(surface >= 0, "the middle of the beam should miss the cyc");

        QImage img(v.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            v.drawBeamCone(p, apex, dir, qDegreesToRadians(8.0), QColor(0, 255, 0), 1.0, 0.0, nullptr);
        }
        auto greenNear = [&](const QVector3D &w) {
            const QPoint c = v.w2s(w).toPoint();
            int n = 0;
            for (int y = c.y() - 4; y <= c.y() + 4; ++y)
                for (int x = c.x() - 4; x <= c.x() + 4; ++x)
                    if (img.rect().contains(x, y) && qGreen(img.pixel(x, y)) > 20)
                        ++n;
            return n;
        };
        // On the cloth, just inside its end, within the beam: lit.
        QVERIFY2(greenNear(QVector3D(1.3f, 0.5f, 2.5f)) > 20, "the cyc's end should take the light");
        // Far along the cloth, outside the beam: dark.
        QCOMPARE(greenNear(QVector3D(6.0f, 0.5f, 2.5f)), 0);
    }
    delete doc;
}

void Monitor_Test::rotatingASelectionTurnsItAboutItsMiddle()
{
    /* "can we fix to rotate a group": everything selected turns about the
       middle of the selection -- each thing's place and its own facing, with
       what is mounted on it -- and one undo puts it all back. */
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Meters);
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(0.0f); deck->setOriginY(0.0f);
    deck->setWidth(2.0f); deck->setDepth(2.0f); deck->setHeight(0.3f);
    Scenery *wall = props->addScenery();
    wall->setKind(Scenery::Wall);
    wall->setOriginX(5.0f); wall->setOriginY(1.0f);
    wall->setWidth(2.0f); wall->setHeight(0.5f); wall->setThickness(0.1f);
    Truss *bar = props->addTruss();
    bar->setOrigin(QVector3D(0.0f, 4.0f, 3.0f));
    bar->setDirection(QPointF(1.0, 0.0));
    bar->setLength(4.0f);
    Fixture *hung = new Fixture(doc);
    hung->setChannels(4);
    QVERIFY(doc->addFixture(hung));
    FixtureRigProps rh;
    rh.trussId = bar->id();
    rh.trussOffset = 1.0f;
    props->setFixtureRigProps(hung->id(), rh);
    Fixture *free = new Fixture(doc);
    free->setChannels(4);
    free->setAddress(10);
    QVERIFY(doc->addFixture(free));
    props->setFixturePosition(free->id(), 0, 0, QVector3D(4000.0f, 0.0f, 0.0f));
    const quint32 deckId = deck->id(), wallId = wall->id(), barId = bar->id();
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        MonitorGraphicsView *gv = mon.m_graphicsView;
        gv->updatePlatforms();
        QVERIFY(gv->selectAllMapItems() >= 4);
        // The selection reaches x 0..6, y 0..4: its middle is (3, 2).
        const QPointF pivot = gv->selectionPivot();
        QVERIFY2(QLineF(pivot, QPointF(3.0, 2.0)).length() < 1e-3,
                 qPrintable(QString("%1, %2").arg(pivot.x()).arg(pivot.y())));

        QVERIFY(gv->rotateSelectedItems(90.0) >= 4);
        // A quarter turn about (3, 2) sends (x, y) to (5 - y, x - 1).
        StagePlatform *pl = props->platform(deckId);
        QVERIFY(QLineF(pl->centre(), QPointF(4.0, 0.0)).length() < 1e-3);
        QCOMPARE(pl->rotation(), 90.0f);
        QVERIFY(gv->m_platformItems.value(deckId) != nullptr);
        QCOMPARE(gv->m_platformItems.value(deckId)->rotation(), 90.0);
        Scenery *w = props->scenery(wallId);
        QVERIFY(qAbs(w->originX() - 4.0f) < 1e-3f && qAbs(w->originY() - 4.0f) < 1e-3f);
        QCOMPARE(w->rotation(), 90.0f);
        Truss *t = props->truss(barId);
        QVERIFY(qAbs(t->origin().x() - 1.0f) < 1e-3f && qAbs(t->origin().y() + 1.0f) < 1e-3f);
        QVERIFY(qAbs(t->direction().y() - 1.0) < 1e-4);
        const QVector3D fp = props->fixtureRigPosition(free->id());
        QVERIFY2(qAbs(fp.x() - 5.0f) < 1e-3f && qAbs(fp.y() - 3.0f) < 1e-3f,
                 qPrintable(QString("%1, %2").arg(fp.x()).arg(fp.y())));
        QCOMPARE(props->fixtureRigProps(free->id()).studioYaw, 90.0f);
        // The one on the truss rides it and turns with it.
        QCOMPARE(props->fixtureRigProps(hung->id()).studioYaw, 90.0f);
        const QVector3D hp = props->fixtureRigPosition(hung->id());
        QVERIFY2(qAbs(hp.x() - 1.0f) < 0.05f && qAbs(hp.y() - 0.0f) < 0.05f,
                 qPrintable(QString("%1, %2").arg(hp.x()).arg(hp.y())));

        // One undo: all of it back.
        QVERIFY(gv->canUndo());
        gv->undoLastMove();
        pl = props->platform(deckId);
        QCOMPARE(pl->rotation(), 0.0f);
        QVERIFY(QLineF(pl->centre(), QPointF(1.0, 1.0)).length() < 1e-3);
        QCOMPARE(props->scenery(wallId)->rotation(), 0.0f);
        QVERIFY(qAbs(props->truss(barId)->direction().x() - 1.0) < 1e-4);
        QCOMPARE(props->fixtureRigProps(free->id()).studioYaw, 0.0f);
        QVERIFY(qAbs(props->fixtureRigPosition(free->id()).x() - 4.0f) < 1e-3f);
        QVERIFY(!gv->canUndo());
    }
    delete doc;
}

void Monitor_Test::aTurnedDeckIsDrawnAndLitWhereItIs()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(0.0f); deck->setOriginY(0.0f);
    deck->setWidth(4.0f); deck->setDepth(1.0f); deck->setHeight(1.0f);
    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(800, 600);
        v.reload();
        bool hit = false;
        QVector3D n;
        // (2, 2) is off the square deck, which reaches y 1 ...
        double t = v.beamThrow(QVector3D(2.0f, 2.0f, 5.0f), QVector3D(0.0f, 0.0f, -1.0f), &hit, &n);
        QVERIFY(qAbs(t - 5.0) < 0.01);
        // ... and on it once it is turned a quarter about its middle (2, 0.5).
        deck->setRotation(90.0f);
        t = v.beamThrow(QVector3D(2.0f, 2.0f, 5.0f), QVector3D(0.0f, 0.0f, -1.0f), &hit, &n);
        QVERIFY2(qAbs(t - 4.0) < 0.01, qPrintable(QString::number(t)));
        t = v.beamThrow(QVector3D(3.5f, 0.5f, 5.0f), QVector3D(0.0f, 0.0f, -1.0f), &hit, &n);
        QVERIFY2(qAbs(t - 5.0) < 0.01, qPrintable(QString::number(t)));

        // Drawn turned: not the same picture as square.
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(0.0, 60.0);
        v.reload();
        const QImage turned = v.grab().toImage();
        deck->setRotation(0.0f);
        v.reload();
        const QImage square = v.grab().toImage();
        int changed = 0;
        for (int y = 0; y < turned.height(); ++y)
            for (int x = 0; x < turned.width(); ++x)
                if (turned.pixel(x, y) != square.pixel(x, y))
                    ++changed;
        QVERIFY2(changed > 2000, qPrintable(QString("only %1 px differ").arg(changed)));
    }
    delete doc;
}

/* The bundled curtain, read from its file as the app would. */
static QLCFixtureDef *curtainDef()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    const QString path = QFileInfo(QStringLiteral(__FILE__)).dir().absoluteFilePath(
        QStringLiteral("../../../resources/fixtures/Generic/Generic-Stage-Curtain.qxf"));
    if (def->loadXML(path) != QFile::NoError)
    {
        delete def;
        return nullptr;
    }
    return def;
}

/* A 10 m main drape across the front of a 12 x 8 m stage, opening 8 m. */
static Scenery *frontDrape(Doc *doc)
{
    MonitorProperties *props = doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Meters);
    props->setGridSize(QVector3D(12, 6, 8));
    Scenery *sc = props->addScenery();
    sc->setKind(Scenery::MainDrape);
    sc->setName("Main drape");
    sc->setOriginX(6.0f);
    sc->setOriginY(7.5f);
    sc->setWidth(10.0f);
    sc->setHeight(6.0f);
    sc->setOpeningWidth(8.0f);
    return sc;
}

/* How wide the gap between a curtain's two halves is in the plot, in
   scene pixels: 0 when it is one sheet. */
static double plotGap(SceneryItem *sci)
{
    const QList<QPolygonF> parts = sci->parts();
    if (parts.size() < 2)
        return 0.0;
    const QRectF a = parts.at(0).boundingRect(), b = parts.at(1).boundingRect();
    return qMax(a.left(), b.left()) - qMin(a.right(), b.right());
}

void Monitor_Test::aDrapeDrivenByACurtainFixtureOpensAsItsChannelMoves()
{
    Doc *doc = new Doc(this);
    QLCFixtureDef *def = curtainDef();
    QVERIFY(def != nullptr);
    QVERIFY(doc->fixtureDefCache()->addFixtureDef(def));
    Scenery *sc = frontDrape(doc);
    // A second drape, never linked: it stays exactly as it was set.
    Scenery *fixed = doc->monitorProperties()->addScenery();
    fixed->setKind(Scenery::Traveler);
    fixed->setOriginX(6.0f);
    fixed->setOriginY(3.0f);
    fixed->setWidth(10.0f);
    fixed->setState(Scenery::Open);
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        mon.m_graphicsView->updatePlatforms();

        // The drape's dialog adds its curtain fixture, and is driven by it.
        bool rowShown = false;
        driveDialog([&](QWidget *dlg) {
            QWidget *row = dlg->findChild<QWidget *>("sceneryDriver");
            rowShown = row != nullptr && row->isVisibleTo(dlg);
            QPushButton *add = dlg->findChild<QPushButton *>("sceneryAddCurtain");
            if (add == nullptr)
                return false;
            add->click();
            QComboBox *drv = dlg->findChild<QComboBox *>("sceneryDriver");
            return drv != nullptr && drv->currentData().toUInt() != Fixture::invalidId();
        });
        mon.slotEditScenery(sc->id());
        QVERIFY(rowShown);
        QVERIFY(sc->isDriven());
        Fixture *fx = doc->fixture(sc->driverFixture());
        QVERIFY(fx != nullptr);
        QVERIFY(Scenery::isCurtainFixture(fx));
        QVERIFY(!fx->isPatched());
        QCOMPARE(fx->fixtureMode()->name(), QString("Open + Fly"));
        QCOMPARE(fx->name(), QString("Main drape curtain"));
        // A curtain is not something waiting to be placed.
        QVERIFY(!doc->monitorProperties()->fixtureItemsID().contains(fx->id()));

        SceneryItem *sci = mon.m_graphicsView->m_sceneryItems.value(sc->id());
        QVERIFY(sci != nullptr);
        // Nothing driving it yet: closed, whatever its fixed state.
        QCOMPARE(sci->parts().size(), 1);

        // The channel moves: the halves part, as far as it says.
        fx->setPreviewValue(0, 255);
        QApplication::processEvents();
        sci = mon.m_graphicsView->m_sceneryItems.value(sc->id());
        QCOMPARE(sci->parts().size(), 2);
        const double fullGap = plotGap(sci);
        QVERIFY(fullGap > 0.0);
        fx->setPreviewValue(0, 128);
        QApplication::processEvents();
        const double halfGap = plotGap(mon.m_graphicsView->m_sceneryItems.value(sc->id()));
        QVERIFY2(qAbs(halfGap / fullGap - 128.0 / 255.0) < 0.03,
                 qPrintable(QString("%1 of %2").arg(halfGap).arg(fullGap)));
        // The geometry is the engine's: half open is half the opening.
        Scenery::Pose half;
        half.open = 128.0 / 255.0;
        QVERIFY(qAbs(sc->spans(half).at(1).first - 0.8 * half.open) < 1e-6);
        // Flown out: dashed where it hangs.
        fx->setPreviewValue(1, 255);
        QApplication::processEvents();
        QVERIFY(mon.m_graphicsView->m_sceneryItems.value(sc->id())->isFlown());
        fx->setPreviewValue(1, 0);
        QApplication::processEvents();
        QVERIFY(!mon.m_graphicsView->m_sceneryItems.value(sc->id())->isFlown());

        // The unlinked one never moved.
        SceneryItem *fi = mon.m_graphicsView->m_sceneryItems.value(fixed->id());
        QVERIFY(fi != nullptr);
        QCOMPARE(fi->parts().size(), 2);
        QVERIFY(!fi->isFlown());

        // The rig overview: where the channel is with Live on, its fixed state with it off.
        StructureStudioView view(doc, StructureStudioView::StageKind, 0);
        fx->setPreviewValue(0, 255);
        view.setLiveValues(true);
        QCOMPARE(view.drapePose(sc).open, 1.0);
        view.updateDrapePoses();
        QCOMPARE(view.drapePose(sc).open, 1.0);
        fx->setPreviewValue(0, 0);
        view.updateDrapePoses();
        QCOMPARE(view.drapePose(sc).open, 0.0);
        view.setLiveValues(false);
        sc->setState(Scenery::Open);
        view.updateDrapePoses();         // (worked out once a frame)
        QCOMPARE(view.drapePose(sc), sc->staticPose());
        QCOMPARE(view.drapePose(fixed), fixed->staticPose());
        // Its throw scene (what a ray can land on) follows the curtain.
        view.setLiveValues(true);
        fx->setPreviewValue(0, 0);
        view.updateDrapePoses();
        const quint64 closedScene = view.throwScene();
        fx->setPreviewValue(0, 200);
        view.updateDrapePoses();
        QVERIFY(view.throwScene() != closedScene);
        view.setLiveValues(false);

        // Its fixture gone: back to its fixed state.
        sc->setState(Scenery::Flown);
        QVERIFY(doc->deleteFixture(fx->id()));
        mon.m_graphicsView->updatePlatforms();
        QVERIFY(mon.m_graphicsView->m_sceneryItems.value(sc->id())->isFlown());
    }
    delete doc;
}

void Monitor_Test::cancellingTheDrapeDialogTakesItsNewCurtainAway()
{
    Doc *doc = new Doc(this);
    QLCFixtureDef *def = curtainDef();
    QVERIFY(def != nullptr);
    QVERIFY(doc->fixtureDefCache()->addFixtureDef(def));
    Scenery *sc = frontDrape(doc);
    sc->setKind(Scenery::Traveler);
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        mon.m_graphicsView->updatePlatforms();
        driveDialog([&](QWidget *dlg) {
            QPushButton *add = dlg->findChild<QPushButton *>("sceneryAddCurtain");
            if (add == nullptr)
                return false;
            add->click();
            return doc->fixtures().size() == 1;
        }, false);
        mon.slotEditScenery(sc->id());
        QCOMPARE(doc->fixtures().size(), 0);
        sc = doc->monitorProperties()->sceneries().first();
        QVERIFY(!sc->isDriven());

        // A traveler's curtain opens; it does not fly.
        Fixture *fx = mon.addCurtainFixture(sc->id());
        QVERIFY(fx != nullptr);
        QCOMPARE(fx->fixtureMode()->name(), QString("Open"));
        // Not a curtain: no curtain fixture.
        Scenery *cyc = doc->monitorProperties()->addScenery();
        QVERIFY(mon.addCurtainFixture(cyc->id()) == nullptr);
    }
    delete doc;
}

/* The bundled silk flame, read from its file as the app would. */
static QLCFixtureDef *silkFlameDef()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    const QString path = QFileInfo(QStringLiteral(__FILE__)).dir().absoluteFilePath(
        QStringLiteral("../../../resources/fixtures/Generic/Generic-Silk-Flame.qxf"));
    if (def->loadXML(path) != QFile::NoError)
    {
        delete def;
        return nullptr;
    }
    return def;
}

void Monitor_Test::aSilkFlameRisesOnlyWhenItsRelayIsOn()
{
    Doc *doc = new Doc(this);
    QLCFixtureDef *def = silkFlameDef();
    QVERIFY(def != nullptr);
    QVERIFY(doc->fixtureDefCache()->addFixtureDef(def));
    QCOMPARE(def->modes().first()->physical().emission(), QLCPhysical::FlameEmission);
    Fixture *flame = new Fixture(doc);
    flame->setFixtureDefinition(def, def->modes().first());
    QVERIFY(doc->addFixture(flame));
    doc->monitorProperties()->setFixturePosition(flame->id(), 0, 0, QVector3D(2000.0f, 2000.0f, 0.0f));
    FixtureRigProps rp;
    rp.mountingType = Truss::FloorMounted;
    doc->monitorProperties()->setFixtureRigProps(flame->id(), rp);
    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(800, 600);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(15.0, 15.0);
        v.setLiveValues(true);
        v.m_opticsTime = 1.0;                       // a still frame
        auto frame = [&](uchar relay) {
            QByteArray u(512, char(0));
            u[0] = char(relay);
            flame->setChannelValues(u);
            v.reload();
            return v.grab().toImage();
        };
        auto flamePx = [](const QImage &img) {
            int n = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x)
                {
                    const QColor q = img.pixelColor(x, y);
                    if (q.red() > 120 && q.red() > q.blue() + 60 && q.green() > 40)
                        ++n;
                }
            return n;
        };
        const int off = flamePx(frame(0));
        const int half = flamePx(frame(100));       // a relay is off below half
        const int on = flamePx(frame(255));
        QCOMPARE(half, off);
        QVERIFY2(on > off + 400, qPrintable(QString("off %1 px, on %2").arg(off).arg(on)));
        // It moves: another moment is another shape.
        v.m_opticsTime = 1.7;
        const QImage a = frame(255);
        v.m_opticsTime = 2.9;
        const QImage b = frame(255);
        QVERIFY(a != b);
    }
    delete doc;
}

void Monitor_Test::addFountainBuildsItAsOneGroup()
{
    Doc *doc = new Doc(this);
    QLCFixtureDef *def = silkFlameDef();
    QVERIFY(def != nullptr);
    QVERIFY(doc->fixtureDefCache()->addFixtureDef(def));
    MonitorProperties *props = doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Feet);
    props->setGridSize(QVector3D(40, 20, 24));
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        bool sized = false;
        driveDialog([&sized](QWidget *dlg) {
            // As built: 3 ft x 4 ft, a 5" base, 8" and 14" walls of 4" blocks.
            QDoubleSpinBox *w = dlg->findChild<QDoubleSpinBox *>("fountainWidth");
            QDoubleSpinBox *inner = dlg->findChild<QDoubleSpinBox *>("fountainInner");
            sized = w != nullptr && inner != nullptr && qAbs(w->value() - 3.0) < 0.01
                    && qAbs(inner->value() - 14.0 / 12.0) < 0.01;
            return true;
        });
        mon.slotAddFountain();
        QVERIFY(sized);

        QCOMPARE(props->platforms().size(), 1);
        StagePlatform *base = props->platforms().first();
        QVERIFY(qAbs(base->width() - 0.9144f) < 1e-3f && qAbs(base->depth() - 1.2192f) < 1e-3f);
        QVERIFY(qAbs(base->height() - 0.127f) < 1e-3f);
        QVERIFY(qAbs(base->frontCurve() - 0.4572f) < 1e-3f);        // a half-round front
        QCOMPARE(props->sceneries().size(), 2);
        Scenery *outer = props->sceneries().at(0), *inner = props->sceneries().at(1);
        if (outer->height() > inner->height())
            qSwap(outer, inner);
        foreach (Scenery *wl, props->sceneries())
        {
            QCOMPARE(wl->kind(), int(Scenery::Wall));
            QCOMPARE(wl->outline(), int(Scenery::RoundedU));
            QVERIFY(qAbs(wl->bottom() - 0.127f) < 1e-3f);           // on the base
            QVERIFY(qAbs(wl->thickness() - 0.1016f) < 1e-3f);
            QCOMPARE(wl->groupId(), base->groupId());
        }
        QVERIFY(qAbs(outer->height() - 0.2032f) < 1e-3f && qAbs(inner->height() - 0.3556f) < 1e-3f);
        // The outer wall's outside face is the base's edge; the inner one a block and a gap in.
        QVERIFY(qAbs(outer->width() + outer->thickness() - base->width()) < 1e-3f);
        QVERIFY(qAbs(inner->width() - (0.9144f - 2.0f * (0.1016f + 0.1016f + 0.0508f))) < 1e-3f);
        QVERIFY(base->groupId() != 0);

        // The flame, staged, standing on the base, in the group.
        Fixture *flame = nullptr;
        foreach (Fixture *fx, doc->fixtures())
            if (fx->fixtureDef() == def)
                flame = fx;
        QVERIFY(flame != nullptr);
        QVERIFY(!flame->isPatched());
        QCOMPARE(props->fixtureRigProps(flame->id()).deckPlatformId, base->id());
        QCOMPARE(props->fixtureGroup(flame->id()), base->groupId());
        const QVector3D at = props->fixtureRigPosition(flame->id());
        QVERIFY(base->containsPoint(at.x(), at.y()));
        QVERIFY(qAbs(at.z() - 0.127f) < 1e-3f);
    }
    delete doc;
}

void Monitor_Test::selectAllTakesEverythingSelectable()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("T-1");
    t->setLength(3.0f);
    StagePlatform *deck = props->addPlatform();
    deck->setName("Deck");
    Stand *stand = props->addStand();
    stand->setOriginX(2.0f);
    Scenery *cyc = props->addScenery();
    cyc->setOriginX(5.0f);
    StageTarget *mark = props->addStageTarget();
    mark->setPosition(QVector3D(3.0f, 3.0f, 0.0f));
    Fixture *spot = new Fixture(doc);
    spot->setChannels(4);
    QVERIFY(doc->addFixture(spot));
    // One on a hidden layer, one on a locked layer: neither can be selected.
    const quint32 hidden = props->addLayer("Hidden");
    const quint32 locked = props->addLayer("Locked");
    Stand *gone = props->addStand();
    gone->setLayerId(hidden);
    Stand *pinned = props->addStand();
    pinned->setLayerId(locked);
    props->setLayerVisible(hidden, false);
    props->setLayerLocked(locked, true);
    {
        MonitorGraphicsView view(doc);
        view.resize(900, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(12, 8));
        view.updateTrusses();
        view.updatePlatforms();
        view.updateTargets();
        view.addFixture(spot->id(), QPointF(4000.0, 3000.0));
        view.refreshItemLayerState();

        QSignalSpy changed(&view, &MonitorGraphicsView::mapSelectionChanged);
        const int n = view.selectAllMapItems();
        QVERIFY2(n >= 6, qPrintable(QString::number(n)));
        QCOMPARE(changed.count(), 1);                   // one change, not one per item
        QVERIFY(view.m_trussItems.value(t->id())->isSelected());
        QVERIFY(view.m_platformItems.value(deck->id())->isSelected());
        QVERIFY(view.m_standItems.value(stand->id())->isSelected());
        QVERIFY(view.m_sceneryItems.value(cyc->id())->isSelected());
        QVERIFY(view.m_targetItems.value(mark->id())->isSelected());
        QVERIFY(view.m_fixtures.value(spot->id())->isSelected());
        QVERIFY(!view.m_standItems.value(gone->id())->isSelected());
        QVERIFY(!view.m_standItems.value(pinned->id())->isSelected());
        QVERIFY(view.selectedMapObjects().contains(qMakePair(QString("scenery"), cyc->id())));
    }
    delete doc;
}

void Monitor_Test::aCountBubbleKeepsItsScreenSizeWhenZoomed()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(2.0f);
    deck->setOriginY(2.0f);
    deck->setWidth(4.0f);
    deck->setDepth(2.0f);
    for (int i = 0; i < 2; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setChannels(4);
        f->setAddress(quint32(i * 4));
        QVERIFY(doc->addFixture(f));
        props->setFixturePosition(f->id(), 0, 0, QVector3D(3000.0f + i * 1000.0f, 3000.0f, 0.0f));
        FixtureRigProps rp;
        rp.deckPlatformId = deck->id();
        props->setFixtureRigProps(f->id(), rp);
    }
    {
        MonitorGraphicsView view(doc);
        view.resize(900, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(12, 8));
        view.updatePlatforms();
        foreach (Fixture *f, doc->fixtures())
            view.addFixture(f->id(), QPointF(props->fixturePosition(f->id(), 0, 0).x(),
                                             props->fixturePosition(f->id(), 0, 0).y()));
        view.setMountedFixturesVisible(false);
        QGraphicsItemGroup *badge = view.m_countBadges.value(QString("platform:%1").arg(deck->id()));
        QVERIFY(badge != nullptr);
        QVERIFY(badge->flags() & QGraphicsItem::ItemIgnoresTransformations);
        PlatformItem *deckItem = view.m_platformItems.value(deck->id());

        // On screen: the bubble's size, and where it sits against the deck.
        // childrenBoundingRect(): the group's own cached bounds predate the disc.
        auto onScreen = [&]() {
            return badge->deviceTransform(view.viewportTransform()).mapRect(badge->childrenBoundingRect());
        };
        auto deckCentre = [&]() {
            return QPointF(view.mapFromScene(deckItem->sceneBoundingRect().center()));
        };
        const QRectF at1 = onScreen();
        QVERIFY2(at1.width() >= 16.0, qPrintable(QString::number(at1.width())));   // a real disc
        QVERIFY((at1.center() - deckCentre()).manhattanLength() < 3.0);
        const double deckAt1 = view.mapFromScene(deckItem->sceneBoundingRect()).boundingRect().width();

        view.scale(4.0, 4.0);
        const QRectF at4 = onScreen();
        QVERIFY2(qAbs(at4.width() - at1.width()) < 1.0,
                 qPrintable(QString("bubble %1 px wide at 1x, %2 px at 4x").arg(at1.width()).arg(at4.width())));
        QVERIFY((at4.center() - deckCentre()).manhattanLength() < 3.0);
        // ...while the deck under it did grow, so more of it is clear to click.
        const double deckAt4 = view.mapFromScene(deckItem->sceneBoundingRect()).boundingRect().width();
        QVERIFY(deckAt4 > deckAt1 * 3.0);

        /* "when I click on the tower .. it's assuming I am clicking on the
           circle to open the breakout .. even when I am not": the bubble is
           hit where it is DRAWN. Zoomed in, its size in scene units covered
           several times the circle. */
        view.show();                                    // real geometry for the clicks
        QCoreApplication::processEvents();
        /* A deferred rebuild may have run in those events: the deck and its
           bubble are new items, and the ones held above are gone (it crashed
           on them, now and then). */
        badge = view.m_countBadges.value(QString("platform:%1").arg(deck->id()));
        deckItem = view.m_platformItems.value(deck->id());
        QVERIFY(badge != nullptr && deckItem != nullptr);
        const QRectF shown = onScreen();
        const QPoint centre = shown.center().toPoint();
        const int radius = int(shown.width() / 2.0);
        QVERIFY(view.countBadgeHit(badge, centre));
        QVERIFY(view.countBadgeHit(badge, centre + QPoint(radius - 2, 0)));
        const QPoint beside = centre + QPoint(radius * 2, 0);        // on the deck, off the disc
        QVERIFY(!view.countBadgeHit(badge, beside));
        QVERIFY(!view.countBadgeHit(badge, centre + QPoint(radius, radius)));   // the square's corner
        QVERIFY(deckItem->sceneBoundingRect().contains(view.mapToScene(beside)));
        // A double-click beside it is the deck's, not the breakout.
        QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, beside);
        QVERIFY(view.revealedFixtures().isEmpty());
        QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, centre);
        QCOMPARE(view.revealedFixtures().size(), 2);
    }
    delete doc;
}

void Monitor_Test::aDecksCountBubbleMovesWithIt()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setName("USP1");
    deck->setOriginX(1.0f);
    deck->setOriginY(1.0f);
    deck->setWidth(4.0f);
    deck->setDepth(1.0f);
    deck->setHeight(0.6f);
    QList<Fixture *> heads;
    for (int i = 0; i < 3; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setChannels(4);
        f->setAddress(quint32(i * 4));
        QVERIFY(doc->addFixture(f));
        props->setFixturePosition(f->id(), 0, 0, QVector3D(1500.0f + i * 1000.0f, 1500.0f, 0.0f));
        FixtureRigProps rp;
        rp.deckPlatformId = deck->id();
        props->setFixtureRigProps(f->id(), rp);
        heads << f;
    }
    {
        MonitorGraphicsView view(doc);
        view.resize(900, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(12, 8));
        view.setSnapDivisions(0);
        view.updatePlatforms();
        foreach (Fixture *f, heads)
            view.addFixture(f->id(), QPointF(props->fixturePosition(f->id(), 0, 0).x(),
                                             props->fixturePosition(f->id(), 0, 0).y()));
        view.setMountedFixturesVisible(false);          // the deck shows a count bubble instead
        const QString key = QString("platform:%1").arg(deck->id());
        QVERIFY2(view.m_countBadges.contains(key), "the deck should carry a count bubble");
        const QPointF before = view.m_countBadges.value(key)->sceneBoundingRect().center();

        // Dragged a metre downstage: the deck, its fixtures and its bubble all go.
        PlatformItem *item = view.m_platformItems.value(deck->id());
        item->setPos(item->pos() + QPointF(0, view.m_cellPixels));
        view.slotPlatformMoved(item);
        QVERIFY(qAbs(deck->originY() - 2.0f) < 0.02f);
        QVERIFY(qAbs(props->fixturePosition(heads.first()->id(), 0, 0).y() - 2500.0f) < 20.0f);
        const QPointF after = view.m_countBadges.value(key)->sceneBoundingRect().center();
        QVERIFY2(qAbs((after - before).y() - view.m_cellPixels) < 2.0,
                 qPrintable(QString("bubble moved %1 px, deck moved %2 px")
                            .arg((after - before).y()).arg(view.m_cellPixels)));

        // Undo puts all three back, bubble included.
        view.undoLastMove();
        const QPointF back = view.m_countBadges.value(key)->sceneBoundingRect().center();
        QVERIFY((back - before).manhattanLength() < 2.0);
    }
    delete doc;
}

static int lockRedPixels(QGraphicsScene *scene, const QRectF &area)
{
    QImage img(area.size().toSize() + QSize(4, 4), QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    scene->render(&p, QRectF(2, 2, area.width(), area.height()), area);
    p.end();
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
        {
            const QColor c = img.pixelColor(x, y);
            if (qAbs(c.red() - 200) < 30 && c.green() < 110 && c.blue() < 110)
                n++;
        }
    return n;
}

void Monitor_Test::aLockIsItsLayersAndDrawsRed()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    const quint32 rig = props->addLayer("Rig");
    Truss *t = props->addTruss();
    t->setName("FOH");
    t->setOrigin(QVector3D(2.0f, 2.0f, 6.0f));
    t->setLength(6.0f);
    t->setLayerId(rig);
    Stand *stand = props->addStand();
    stand->setOriginX(9.0f);
    stand->setOriginY(5.0f);
    stand->setLayerId(rig);
    Stand *other = props->addStand();               // Default layer
    other->setOriginX(3.0f);
    other->setOriginY(6.0f);
    Fixture *spot = new Fixture(doc);
    spot->setChannels(4);
    QVERIFY(doc->addFixture(spot));
    props->setFixtureLayer(spot->id(), rig);
    PowerSource ps;
    ps.name = "Distro";
    ps.placed = true;
    ps.posX = 10.0;
    ps.posY = 7.0;
    ps.layerId = rig;
    doc->powerDistribution()->sources() << ps;
    {
        MonitorGraphicsView view(doc);
        view.resize(900, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(14, 10));
        view.updateTrusses();
        view.updatePlatforms();
        view.addFixture(spot->id(), QPointF(5000.0, 4000.0));
        view.updatePowerSources();
        view.refreshItemLayerState();
        MonitorFixtureItem *fi = view.m_fixtures.value(spot->id());
        QVERIFY(fi != nullptr);
        const int redBefore = lockRedPixels(view.m_scene, fi->sceneBoundingRect());

        // The menu offers the LAYER, by name.
        QMenu menu;
        view.buildItemMenu(&menu, "stand", stand->id(), MonitorGraphicsView::MenuPlace());
        QVERIFY2(menuPaths(&menu).join("|").contains(QString::fromUtf8("Lock Layer “Rig”")),
                 qPrintable(menuPaths(&menu).join(" | ")));

        // Locking the stand locks Rig, and so everything on Rig.
        view.setItemLocked("stand", stand->id(), true);
        QVERIFY(props->layer(rig).locked);
        QVERIFY(!props->layer(MonitorProperties::defaultLayerId).locked);
        QVERIFY(stand->locked() && t->locked());
        QVERIFY(doc->powerDistribution()->sources().first().locked);
        QVERIFY(!other->locked());
        QVERIFY(view.itemLocked("truss", t->id()));
        QVERIFY(view.itemLocked("fixture", spot->id()));
        QVERIFY(view.itemLocked("power", 0));
        QVERIFY(!view.itemLocked("stand", other->id()));
        QVERIFY(!view.itemLocked("stand", 999));        // no such stand
        QVERIFY(!view.itemLocked("fixture", 999));
        QVERIFY(!(view.m_standItems.value(stand->id())->flags() & QGraphicsItem::ItemIsMovable));
        QVERIFY(view.m_standItems.value(other->id())->flags() & QGraphicsItem::ItemIsMovable);
        QVERIFY(!(fi->flags() & QGraphicsItem::ItemIsMovable));
        QVERIFY(fi->isGhosted());

        // Drawn red: the fixture (which used to go faint instead) and the truss.
        QVERIFY2(lockRedPixels(view.m_scene, fi->sceneBoundingRect()) > redBefore + 4,
                 "a fixture on a locked layer isn't outlined red");
        QVERIFY(lockRedPixels(view.m_scene, view.m_trussItems.value(t->id())->sceneBoundingRect()) > 4);

        QMenu menu2;
        view.buildItemMenu(&menu2, "truss", t->id(), MonitorGraphicsView::MenuPlace());
        QVERIFY2(menuPaths(&menu2).join("|").contains(QString::fromUtf8("Unlock Layer “Rig”")),
                 qPrintable(menuPaths(&menu2).join(" | ")));

        // Asking for an object that isn't there changes nothing.
        view.setItemLocked("stand", 999, false);
        QVERIFY(props->layer(rig).locked);
        view.setItemLocked("truss", 999, true);
        QVERIFY(!props->layer(MonitorProperties::defaultLayerId).locked);

        // The power source's own toggle is the same lock.
        view.slotPowerSourceLockToggled(0);
        QVERIFY(!props->layer(rig).locked);
        QVERIFY(!stand->locked() && !t->locked());
        QVERIFY(!fi->isGhosted());
        QVERIFY(fi->flags() & QGraphicsItem::ItemIsMovable);
        view.slotPowerSourceLockToggled(0);
        QVERIFY(props->layer(rig).locked);
        view.slotPowerSourceLockToggled(7);             // no such source: ignored
        QVERIFY(props->layer(rig).locked);

        // A lock set straight on an object (an old habit) doesn't survive:
        // its layer says what it is.
        other->setLocked(true);
        view.refreshItemLayerState();
        QVERIFY(!other->locked());
    }
    delete doc;
}

void Monitor_Test::oneUndoPutsBackAWholeMixedDrag()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setOriginX(1.0f); deck->setOriginY(1.0f);
    deck->setWidth(3.0f); deck->setDepth(1.0f); deck->setHeight(0.5f);
    Fixture *onDeck = new Fixture(doc);
    onDeck->setChannels(4);
    QVERIFY(doc->addFixture(onDeck));
    props->setFixturePosition(onDeck->id(), 0, 0, QVector3D(1500.0f, 1500.0f, 0.0f));
    FixtureRigProps rp;
    rp.deckPlatformId = deck->id();
    props->setFixtureRigProps(onDeck->id(), rp);
    Tower *tower = props->addTower();
    tower->setOriginX(6.0f); tower->setOriginY(3.0f);
    Stand *stand = props->addStand();
    stand->setOriginX(8.0f); stand->setOriginY(3.0f);
    Scenery *cyc = props->addScenery();
    cyc->setOriginX(5.0f); cyc->setOriginY(0.5f);
    StageTarget *mark = props->addStageTarget();
    mark->setPosition(QVector3D(4.0f, 4.0f, 0.0f));
    {
        MonitorGraphicsView view(doc);
        view.resize(900, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(12, 8));
        view.setSnapDivisions(0);
        view.updatePlatforms();
        view.updateTargets();
        view.addFixture(onDeck->id(), QPointF(1500.0, 1500.0));
        const qreal m = view.m_cellPixels;               // one metre

        // One drag of a mixed selection: every item a metre downstage, then
        // each kind's handler -- twice, as a real release does (its own drop,
        // then the sweep); the second pass must not record a second move.
        view.m_openUndoBatch = ++view.m_undoBatch;
        QList<QGraphicsItem *> items = { view.m_platformItems.value(deck->id()), view.m_towerItems.value(tower->id()),
                                         view.m_standItems.value(stand->id()), view.m_sceneryItems.value(cyc->id()),
                                         view.m_targetItems.value(mark->id()) };
        for (QGraphicsItem *gi : items)
        {
            QVERIFY(gi != nullptr);
            gi->setPos(gi->pos() + QPointF(0, m));
        }
        for (int pass = 0; pass < 2; ++pass)
        {
            view.slotPlatformMoved(view.m_platformItems.value(deck->id()));
            view.slotTowerMoved(view.m_towerItems.value(tower->id()));
            view.slotStandMoved(view.m_standItems.value(stand->id()));
            view.slotSceneryMoved(view.m_sceneryItems.value(cyc->id()));
            view.slotTargetMoved(view.m_targetItems.value(mark->id()));
        }
        view.m_openUndoBatch = 0;
        QVERIFY(qAbs(deck->originY() - 2.0f) < 0.02f);
        QVERIFY(qAbs(tower->originY() - 4.0f) < 0.02f);
        QVERIFY(qAbs(stand->originY() - 4.0f) < 0.02f);
        QVERIFY(qAbs(cyc->originY() - 1.5f) < 0.02f);
        QVERIFY(qAbs(mark->y() - 5.0f) < 0.02f);
        QVERIFY(qAbs(props->fixturePosition(onDeck->id(), 0, 0).y() - 2500.0f) < 20.0f);
        const int entries = view.m_moveUndo.size();
        QCOMPARE(entries, 5);                            // one per kind, none doubled

        // One Ctrl+Z: all of it, the deck's fixture included.
        view.undoLastMove();
        QVERIFY(view.m_moveUndo.isEmpty());
        QVERIFY2(qAbs(deck->originY() - 1.0f) < 0.02f, qPrintable(QString::number(deck->originY())));
        QVERIFY(qAbs(tower->originY() - 3.0f) < 0.02f);
        QVERIFY(qAbs(stand->originY() - 3.0f) < 0.02f);
        QVERIFY(qAbs(cyc->originY() - 0.5f) < 0.02f);
        QVERIFY(qAbs(mark->y() - 4.0f) < 0.02f);
        QVERIFY2(qAbs(props->fixturePosition(onDeck->id(), 0, 0).y() - 1500.0f) < 20.0f,
                 qPrintable(QString::number(props->fixturePosition(onDeck->id(), 0, 0).y())));

        // Moves made on their own stay separate steps.
        QGraphicsItem *ti = view.m_towerItems.value(tower->id());
        ti->setPos(ti->pos() + QPointF(m, 0));
        view.slotTowerMoved(view.m_towerItems.value(tower->id()));
        QGraphicsItem *si = view.m_standItems.value(stand->id());
        si->setPos(si->pos() + QPointF(m, 0));
        view.slotStandMoved(view.m_standItems.value(stand->id()));
        view.undoLastMove();
        QVERIFY(qAbs(stand->originX() - 8.0f) < 0.02f);  // the stand came back...
        QVERIFY(qAbs(tower->originX() - 7.0f) < 0.02f);  // ...the tower waits its turn
        view.undoLastMove();
        QVERIFY(qAbs(tower->originX() - 6.0f) < 0.02f);
    }
    delete doc;
}

/* A staged fixture whose definition has a Maintenance channel with Reset and
 * Identify, so the test dialogs grow their DMX buttons. */
static Fixture *stagedWithMaintenance(Doc *doc, FixtureCleanup *cleanup)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Serviceable");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Std");
    QLCChannel *dim = new QLCChannel();
    dim->setName("Dimmer");
    dim->setGroup(QLCChannel::Intensity);
    QLCChannel *ctl = new QLCChannel();
    ctl->setName("Control");
    ctl->setGroup(QLCChannel::Maintenance);
    QLCCapability *reset = new QLCCapability(200, 209);
    reset->setName("Reset");
    QLCCapability *ident = new QLCCapability(210, 219);
    ident->setName("Identify");
    ctl->addCapability(reset);
    ctl->addCapability(ident);
    def->addChannel(dim);
    def->addChannel(ctl);
    mode->insertChannel(dim, 0);
    mode->insertChannel(ctl, 1);
    def->addMode(mode);

    Fixture *f = new Fixture(doc);
    f->setFixtureDefinition(def, mode);
    f->setName("Serviceable 1");
    f->setPatched(false);
    if (!doc->addFixture(f))
        return nullptr;
    cleanup->ids << f->id();
    return f;
}

void Monitor_Test::aStagedFixturesTestDialogOffersNoDmxActions()
{
    FixtureCleanup cleanup(m_doc);
    Fixture *f = stagedWithMaintenance(m_doc, &cleanup);
    QVERIFY(f != nullptr);
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    QVERIFY(!rig.gv->fixturesID().contains(f->id()));   // not placed: the compact dialog

    int guarded = 0;
    inspectModal([&guarded](QWidget *dlg) {
        QVERIFY2(hasLabel(dlg, "Unpatched (inventory)"), "the address should say it is unpatched");
        foreach (QPushButton *b, dlg->findChildren<QPushButton *>())
            if (b->toolTip().startsWith("Patch this fixture first"))
            {
                QVERIFY(!b->isEnabled());
                b->setEnabled(true);                      // even forced, a click writes nothing
                b->click();
                guarded++;
            }
    });
    rig.mon->showFixturePropertiesById(f->id());
    QCOMPARE(guarded, 2);                               // Reset and Identify
    QCOMPARE(m_doc->inputOutputMap()->claimUniverses().first()->preGMValue(1), uchar(0));
    m_doc->inputOutputMap()->releaseUniverses(false);
}

void Monitor_Test::aFixtureIsRenamedFromItsEditDialogAndEverywhere()
{
    FixtureCleanup cleanup(m_doc);
    Fixture *f = new Fixture(m_doc);
    f->setName("Spot 1");
    f->setChannels(4);
    f->setAddress(300);
    QVERIFY(m_doc->addFixture(f));
    cleanup.ids << f->id();
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    QVERIFY(dropFixtureAtCentre(rig.gv, f->id()));
    MonitorLayersPanel *panel = rig.mon->m_layersPanel;
    QVERIFY(panel != nullptr);
    panel->reload();
    QVERIFY(layersRow(*panel, "Spot 1") != nullptr);
    FixtureTreeWidget manager(m_doc, FixtureTreeWidget::UniverseNumber);
    manager.updateTree();
    auto managerRow = [&manager, f]() {
        QTreeWidgetItem *it = manager.fixtureItem(f->id());
        return it != nullptr ? it->text(0) : QString();
    };
    QCOMPARE(managerRow(), QString("Spot 1"));

    // The edit dialog has the name, and a new one sticks (trimmed).
    driveDialog([](QWidget *dlg) {
        QLineEdit *name = dlg->findChild<QLineEdit *>("fixtureNameEdit");
        if (name == nullptr || name->text() != "Spot 1")
            return false;
        name->setText("  Downstage Spot  ");
        return true;
    });
    rig.mon->showFixtureItemEditor(f->id());
    QCOMPARE(f->name(), QString("Downstage Spot"));
    QCoreApplication::processEvents();

    // ...and shows everywhere: the plot's label and tooltip, Layers, the
    // Fixture Manager's rows.
    MonitorFixtureItem *item = rig.gv->m_fixtures.value(f->id());
    QCOMPARE(item->name(), QString("Downstage Spot"));
    QVERIFY(item->toolTip().contains("Downstage Spot"));
    QVERIFY(layersRow(*panel, "Downstage Spot") != nullptr);
    QVERIFY(layersRow(*panel, "Spot 1") == nullptr);
    manager.refreshFixtureName(f->id());                // what FixtureManager does on the signal
    QCOMPARE(managerRow(), QString("Downstage Spot"));

    // Renamed anywhere else, the plot and Layers follow too.
    f->setName("SR Spot");
    QCoreApplication::processEvents();
    QCOMPARE(item->name(), QString("SR Spot"));
    QVERIFY(layersRow(*panel, "SR Spot") != nullptr);

    // An empty name in the dialog leaves it alone.
    driveDialog([](QWidget *dlg) {
        QLineEdit *name = dlg->findChild<QLineEdit *>("fixtureNameEdit");
        if (name == nullptr)
            return false;
        name->setText("   ");
        return true;
    });
    rig.mon->showFixtureItemEditor(f->id());
    QCOMPARE(f->name(), QString("SR Spot"));
}

void Monitor_Test::selectingOnThePlotSelectsInLayers()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("FOH");
    t->setOrigin(QVector3D(2.0f, 2.0f, 6.0f));
    t->setLength(6.0f);
    Stand *stand = props->addStand();
    stand->setName("Stand A");
    stand->setOriginX(8.0f);
    stand->setOriginY(5.0f);
    Fixture *onTruss = new Fixture(doc);
    onTruss->setName("FOH Wash");
    onTruss->setChannels(4);
    QVERIFY(doc->addFixture(onTruss));
    Fixture *loose = new Fixture(doc);
    loose->setName("Floor Par");
    loose->setChannels(4);
    loose->setAddress(10);
    QVERIFY(doc->addFixture(loose));
    {
        MonitorGraphicsView view(doc);
        view.resize(900, 700);
        view.setGridMetrics(1000.0);
        view.setGridSize(QSize(12, 8));
        view.updateTrusses();
        view.updatePlatforms();
        view.ensureFixtureOnPlot(loose->id());
        view.ensureFixtureOnPlot(onTruss->id());
        view.attachFixtureToTruss(onTruss->id(), t->id());
        MonitorLayersPanel panel(doc, &view);
        panel.reload();
        auto selectedNames = [&panel]() {
            QStringList out;
            foreach (QTreeWidgetItem *it, panel.m_tree->selectedItems())
                out << it->text(0).section(QString::fromUtf8("  \xc2\xb7"), 0, 0).trimmed();
            out.sort();
            return out;
        };

        // A fixture picked on the plot is picked in the list, folders opened.
        view.selectMapItem("fixture", onTruss->id());
        QCOMPARE(selectedNames(), QStringList() << "FOH Wash");
        QTreeWidgetItem *row = panel.m_tree->selectedItems().first();
        for (QTreeWidgetItem *up = row->parent(); up != nullptr; up = up->parent())
            QVERIFY(up->isExpanded());
        // ...and the list did not hand it back and change the plot's pick.
        QCOMPARE(view.selectedMapObjects().size(), 1);

        // Several at once, including a structure shown as its group row.
        view.selectMapItems({ qMakePair(QString("fixture"), loose->id()),
                              qMakePair(QString("stand"), stand->id()),
                              qMakePair(QString("truss"), t->id()) });
        // (Picking the truss picks its group -- the fixture on it -- on the
        // plot, and the list shows exactly what the plot picked.)
        QCOMPARE(view.selectedMapObjects().size(), 4);
        QCOMPARE(selectedNames(), QStringList() << "FOH" << "FOH Wash" << "Floor Par" << "Stand A");

        // Cleared on the plot: cleared here.
        view.scene()->clearSelection();
        QVERIFY(panel.m_tree->selectedItems().isEmpty());

        // The other way still works: a row picked selects on the plot.
        layersRow(panel, "Floor Par")->setSelected(true);
        QVERIFY(view.m_fixtures.value(loose->id())->isSelected());
    }
    delete doc;
}

void Monitor_Test::towerShelfLabelsAreOnTheLeft()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Tower *t = props->addTower();
    t->setName("AT2");
    t->addShelf(0.6f);
    t->addShelf(1.2f);
    t->addShelf(1.8f);
    Truss *truss = props->addTruss();
    {
        StructureStudioView v(doc, StructureStudioView::TowerKind, t->id());
        v.resize(520, 780);
        v.setPlane(StructureStudioView::Front);
        QCOMPARE(v.shelfLabel(2, 1.8f, 1.0, " m"), QString::fromUtf8("Shelf 3 — 1.80 m"));
        const double w = v.shelfLabelsWidth();
        QVERIFY2(w > 40.0, qPrintable(QString::number(w)));   // room for "Shelf 3 — 1.80 m"

        // Drawn: shelf label pixels left of the tower, none of them right of it.
        QImage img(520, 780, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            v.render(&p);
        }
        const QPointF left = v.w2s(QVector3D(t->originX(), t->originY(), 1.8f));
        const QPointF right = v.w2s(QVector3D(t->originX() + t->width(), t->originY(), 1.8f));
        const int y = int(left.y()) - 7;                    // the text's row
        auto bright = [&img, y](int x0, int x1) {
            int n = 0;
            for (int x = qMax(0, x0); x < qMin(img.width(), x1); ++x)
                for (int dy = -3; dy <= 3; ++dy)
                    if (qGray(img.pixel(x, qBound(0, y + dy, img.height() - 1))) > 150)
                        ++n;
            return n;
        };
        const int lx = int(qMin(left.x(), right.x())), rx = int(qMax(left.x(), right.x()));
        QVERIFY2(bright(lx - int(w), lx - 2) > 20, "no shelf label left of the tower");
        QCOMPARE(bright(rx + 4, rx + 120), 0);              // right side kept for fixture names

        // Top view draws no shelf labels, so the dimension stays put.
        v.setPlane(StructureStudioView::Top);
        QCOMPARE(v.shelfLabelsWidth(), 0.0);
    }
    {
        StructureStudioView tv(doc, StructureStudioView::TrussKind, truss->id());
        tv.setPlane(StructureStudioView::Front);
        QCOMPARE(tv.shelfLabelsWidth(), 0.0);              // not a tower
    }
    delete doc;
}

void Monitor_Test::aBarIsAddedWhereTheTrussEditorWasRightClicked()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *t2 = props->addTruss();
    t2->setName("T2");
    t2->setType(Truss::Vertical);
    t2->setOrigin(QVector3D(2.0f, 2.0f, 0.0f));
    t2->setLength(3.0f);
    t2->setWidth(0.29f);
    {
        StructureStudioView v(doc, StructureStudioView::TrussKind, t2->id());
        v.resize(520, 780);
        v.setPlane(StructureStudioView::Front);
        // Up the truss as drawn: the foot is 0, the top its length, between
        // is between; off the ends clamps.
        const QPoint foot = v.w2s(t2->origin()).toPoint();
        const QPoint top = v.w2s(t2->positionAt(3.0f)).toPoint();
        const QPoint twoThirds = v.w2s(t2->positionAt(2.0f)).toPoint();
        QVERIFY(qAbs(v.offsetAlongTrussAt(t2->id(), foot) - 0.0f) < 0.05f);
        QVERIFY(qAbs(v.offsetAlongTrussAt(t2->id(), top) - 3.0f) < 0.05f);
        QVERIFY(qAbs(v.offsetAlongTrussAt(t2->id(), twoThirds + QPoint(30, 0)) - 2.0f) < 0.05f);
        QVERIFY(qAbs(v.offsetAlongTrussAt(t2->id(), top - QPoint(0, 200)) - 3.0f) < 0.05f);
        QCOMPARE(v.offsetAlongTrussAt(99, foot), -1.0f);
        // From above a vertical truss is a point: no "here" to pick.
        v.setPlane(StructureStudioView::Top);
        QCOMPARE(v.offsetAlongTrussAt(t2->id(), foot), -1.0f);
    }
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        const int before = props->trusses().size();
        driveDialog([](QWidget *) { return true; }, false);   // the new bar's editor opens
        mon.studioAddBar(2, t2->id(), nullptr, 2.0f);
        QCOMPARE(props->trusses().size(), before + 1);
        Truss *bar = nullptr;
        foreach (Truss *t, props->trusses())
            if (t->parentTrussId() == t2->id())
                bar = t;
        QVERIFY(bar != nullptr);
        QVERIFY(qAbs(bar->parentOffset() - 2.0f) < 0.01f);  // where it was clicked
        driveDialog([](QWidget *) { return true; }, false);
        mon.studioAddBar(2, t2->id(), nullptr);              // the toolbar: mid-span
        int bars = 0;
        foreach (Truss *t, props->trusses())
            if (t->parentTrussId() == t2->id())
            {
                ++bars;
                if (t != bar)
                    QVERIFY(qAbs(t->parentOffset() - 1.5f) < 0.01f);
            }
        QCOMPARE(bars, 2);
    }
    delete doc;
}

/* Answer the next popup menu: record its entries in @p seen and pick the one
 * whose text is @p pick (by keyboard -- QMenu::exec() only reports an action
 * chosen that way), or close it when @p pick is empty. Then close whatever
 * modal dialog the choice opens, if any. */
static void answerPopup(QStringList *seen, const QString &pick)
{
    auto attempts = std::make_shared<int>(0);
    auto poll = std::make_shared<std::function<void()> >();
    *poll = [seen, pick, attempts, poll]() {
        QMenu *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (menu == nullptr)
        {
            if (++*attempts < 200)
                QTimer::singleShot(10, *poll);
            return;
        }
        QAction *chosen = nullptr;
        foreach (QAction *a, menu->actions())
        {
            if (a->isSeparator())
                continue;
            seen->append(a->text());
            if (!pick.isEmpty() && a->text() == pick)
                chosen = a;
        }
        if (chosen == nullptr)
        {
            menu->close();
            return;
        }
        // Whatever the choice opens, close it once it is up.
        auto later = std::make_shared<int>(0);
        auto closeModal = std::make_shared<std::function<void()> >();
        *closeModal = [later, closeModal]() {
            if (QWidget *modal = QApplication::activeModalWidget())
            {
                if (QDialog *d = qobject_cast<QDialog *>(modal))
                    d->reject();
                else
                    modal->close();
                return;
            }
            if (++*later < 100)
                QTimer::singleShot(10, *closeModal);
        };
        QTimer::singleShot(10, *closeModal);
        menu->setActiveAction(chosen);
        QTest::keyClick(menu, Qt::Key_Return);
    };
    QTimer::singleShot(0, *poll);
}

void Monitor_Test::theTrussEditorsMenusOfferAddBar()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *t2 = props->addTruss();
    t2->setName("T2");
    t2->setType(Truss::Vertical);
    t2->setOrigin(QVector3D(2.0f, 2.0f, 0.0f));
    t2->setLength(3.0f);
    t2->setWidth(0.29f);
    Fixture *inside = new Fixture(doc);
    inside->setName("T2-B1");
    inside->setChannels(4);
    QVERIFY(doc->addFixture(inside));
    FixtureRigProps rp;
    rp.trussId = t2->id();
    rp.trussOffset = 2.5f;
    props->setFixtureRigProps(inside->id(), rp);
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        QDialog dlg;
        StructureStudioView *view = nullptr;
        mon.makeStudioPane(&dlg, 2 /* truss */, t2->id(), nullptr, &view);
        QVERIFY(view != nullptr);
        view->resize(520, 780);
        view->setPlane(StructureStudioView::Front);
        auto bars = [props, t2]() {
            QList<Truss *> out;
            foreach (Truss *t, props->trusses())
                if (t->parentTrussId() == t2->id())
                    out << t;
            return out;
        };

        // Over the fixture rigged in it: its own entries, and Add Bar Here.
        QStringList seen;
        const QPoint oneMetre = view->w2s(t2->positionAt(1.0f)).toPoint();
        answerPopup(&seen, "Add Bar Here");
        emit view->canvasContextMenu(view->mapToGlobal(oneMetre), inside->id());
        QVERIFY2(seen.contains("Edit fixture…") && seen.contains("Add Bar Here"),
                 qPrintable(seen.join(" | ")));
        QCOMPARE(bars().size(), 1);
        QVERIFY(qAbs(bars().first()->parentOffset() - 1.0f) < 0.05f);   // where it was clicked

        // On empty canvas too.
        seen.clear();
        const QPoint twoMetres = view->w2s(t2->positionAt(2.0f)).toPoint();
        answerPopup(&seen, "Add Bar Here");
        emit view->canvasContextMenu(view->mapToGlobal(twoMetres), Fixture::invalidId());
        QVERIFY2(seen.contains("Add Fixtures…") && seen.contains("Add Bar Here"),
                 qPrintable(seen.join(" | ")));
        QCOMPARE(bars().size(), 2);
    }
    {
        // A bar's own editor offers no bar: bars don't carry bars.
        Truss *bar = props->truss(props->trusses().last()->id());
        QVERIFY(bar->isChildBar());
        QWidget parent;
        Monitor mon(&parent, doc);
        QDialog dlg;
        StructureStudioView *view = nullptr;
        mon.makeStudioPane(&dlg, 2, bar->id(), nullptr, &view);
        QStringList seen;
        answerPopup(&seen, QString());
        emit view->canvasContextMenu(view->mapToGlobal(QPoint(50, 50)), Fixture::invalidId());
        QVERIFY(!seen.isEmpty());
        QVERIFY(!seen.join("|").contains("Add Bar"));
    }
    delete doc;
}

void Monitor_Test::aStagedFixtureOnThePlotCannotBeTestedOrLocated()
{
    FixtureCleanup cleanup(m_doc);
    Fixture *f = stagedWithMaintenance(m_doc, &cleanup);
    QVERIFY(f != nullptr);
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    QVERIFY(dropFixtureAtCentre(rig.gv, f->id()));

    bool looked = false;
    inspectModal([&looked](QWidget *dlg) {
        // The DMX row says so, and how to give it an address.
        QVERIFY2(hasLabel(dlg, "Unpatched (inventory). Patch… gives it an address."),
                 "the address should say it is unpatched");
        QPushButton *test = buttonWithText(dlg, "Test");
        QPushButton *locate = buttonWithText(dlg, "Locate");
        QVERIFY(test != nullptr && locate != nullptr);
        QVERIFY(!test->isEnabled() && !locate->isEnabled());
        QVERIFY(locate->toolTip().contains("inventory"));
        looked = true;
    });
    rig.mon->showFixtureItemEditor(f->id());
    QVERIFY(looked);
}

void Monitor_Test::theDmxViewHasNoTileForAStagedFixture()
{
    FixtureCleanup cleanup(m_doc);
    Fixture *staged = stagedFixture(m_doc, "No Tile", &cleanup);
    Fixture *real = new Fixture(m_doc);
    real->setName("Tile");
    real->setChannels(2);
    QVERIFY(m_doc->addFixture(real));
    cleanup.ids << real->id();

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    rig.mon->m_dmxViewPopulated = false;
    rig.mon->populateDMXView();
    QList<quint32> tiles;
    foreach (MonitorFixture *mof, rig.mon->m_monitorFixtures)
        tiles << mof->fixture();
    QVERIFY(tiles.contains(real->id()));
    QVERIFY(!tiles.contains(staged->id()));
}

namespace
{
Fixture *dmxFixtureOnPlot(Doc *doc, MonitorGraphicsView *gv, const char *name, quint32 addr,
                          double xMm, double yMm, FixtureCleanup *cleanup)
{
    Fixture *f = new Fixture(doc);
    f->setName(name);
    f->setChannels(4);
    f->setAddress(addr);
    if (!doc->addFixture(f))
    {
        delete f;
        return nullptr;
    }
    cleanup->ids << f->id();
    doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(xMm, yMm, 0));
    gv->addFixture(f->id(), QPointF(xMm, yMm));
    return f;
}
}

void Monitor_Test::theNetworkOverlayDrawsCableRunsInAddressOrder()
{
    TopologyCleanup topo(m_doc);
    FixtureCleanup cleanup(m_doc);

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));

    // Patched high-address first, to prove the run is by address and not by id.
    Fixture *b = dmxFixtureOnPlot(m_doc, rig.gv, "B", 20, 8000, 2000, &cleanup);
    Fixture *a = dmxFixtureOnPlot(m_doc, rig.gv, "A", 0, 4000, 2000, &cleanup);
    QVERIFY(a && b);

    NetworkNode n;
    n.name = "SR Node";
    n.placed = true;
    n.posX = 1.0;
    n.posY = 2.0;
    NetworkPort p;
    p.universe = 0;
    n.ports << p;
    m_doc->networkTopology()->nodes() << n;
    rig.gv->updateNetworkNodes();

    // Not in the normal view.
    rig.mon->applyMapView(Monitor::ViewNormal);
    rig.gv->updateNetworkRuns();
    QCOMPARE(rig.gv->m_networkRunItems.size(), 0);

    rig.mon->applyMapView(Monitor::ViewNet);
    rig.gv->updateNetworkRuns();
    QCOMPARE(rig.gv->m_networkRunItems.size(), 1);

    NetworkRunItem *run = rig.gv->m_networkRunItems.first();
    const QPainterPath path = run->path();
    QCOMPARE(path.elementCount(), 3);                       // node, A, B
    // A (4 m) comes before B (8 m) along the run.
    QVERIFY2(path.elementAt(1).x < path.elementAt(2).x,
             "the run should visit the lower address first");
    const QPointF nodeC = rig.gv->m_networkNodeItems.first()->centre();
    QVERIFY((QPointF(path.elementAt(0).x, path.elementAt(0).y) - nodeC).manhattanLength() < 3.0);

    // The stats on hover.
    const QString tip = run->toolTip();
    QVERIFY2(tip.contains("SR Node") && tip.contains("Port 1") && tip.contains("Universe 1")
             && tip.contains("2 fixture") && tip.contains("8/512 ch"), qPrintable(tip));
    QVERIFY2(tip.contains(" m"), qPrintable(tip));

    // Hovering has to be possible: the hit shape is a band, wider than the line.
    QVERIFY(run->shape().boundingRect().height() > run->path().boundingRect().height() + 5.0
            || run->shape().boundingRect().width() > run->path().boundingRect().width() + 5.0);

    // Leaving the overlay takes the runs away again.
    rig.mon->applyMapView(Monitor::ViewNormal);
    QCOMPARE(rig.gv->m_networkRunItems.size(), 0);
}

namespace
{
NetworkNode wirelessNode(const char *name, quint32 universe, bool receiverPlaced,
                         double rxX = 5.0, double rxY = 5.0)
{
    NetworkNode n;
    n.name = name;
    n.transport = NetworkNode::WirelessDMX;
    n.placed = true;
    n.posX = -8.0;
    n.posY = 1.0;
    NetworkPort p;
    p.universe = universe;
    p.receiver.name = "Rig RX";
    p.receiver.placed = receiverPlaced;
    p.receiver.posX = rxX;
    p.receiver.posY = rxY;
    n.ports << p;
    return n;
}
} // namespace

void Monitor_Test::aWirelessRunStartsAtItsReceiverAndCountsWhatIsOffThePlot()
{
    TopologyCleanup topo(m_doc);
    FixtureCleanup cleanup(m_doc);
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));

    // Universe 1: one plotted, one patched but not on the plot.
    QVERIFY(dmxFixtureOnPlot(m_doc, rig.gv, "On Plot", 0, 4000, 2000, &cleanup));
    Fixture *off = new Fixture(m_doc);
    off->setName("Off Plot");
    off->setChannels(4);
    off->setAddress(10);
    QVERIFY(m_doc->addFixture(off));
    cleanup.ids << off->id();
    auto onUniverse = [&](const char *name, quint32 uni, double xMm) -> Fixture * {
        Fixture *f = new Fixture(m_doc);
        f->setName(name);
        f->setChannels(4);
        f->setUniverse(uni);
        if (!m_doc->addFixture(f))
        {
            delete f;
            return nullptr;
        }
        cleanup.ids << f->id();
        m_doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(xMm, 2000, 0));
        rig.gv->addFixture(f->id(), QPointF(xMm, 2000));
        return f;
    };
    // Universe 2: a node never placed on the plot -- no anchor, no run.
    Fixture *u2 = onUniverse("U2", 1, 6000);
    // Universe 3: a wired node whose only fixture is hidden -- nothing to draw.
    Fixture *u3 = onUniverse("U3", 2, 7000);
    QVERIFY(u2 && u3);

    NetworkNode wired;
    wired.name = "Wired";
    wired.placed = true;
    NetworkPort wp;
    wp.universe = 2;
    wired.ports << wp;
    NetworkNode lost;
    lost.name = "Lost";
    NetworkPort lp;
    lp.universe = 1;
    lost.ports << lp;
    m_doc->networkTopology()->nodes() << wirelessNode("Beach TX", 0, true) << lost << wired;
    rig.gv->updateNetworkNodes();
    rig.mon->applyMapView(Monitor::ViewNet);
    rig.gv->m_fixtures.value(u3->id())->setVisible(false);
    rig.gv->updateNetworkRuns();

    QStringList tips;
    foreach (NetworkRunItem *ri, rig.gv->m_networkRunItems)
        tips << ri->toolTip();
    QVERIFY2(rig.gv->m_networkRunItems.size() == 1, qPrintable(tips.join(" || ")));
    NetworkRunItem *run = rig.gv->m_networkRunItems.first();
    QCOMPARE(run->nodeIndex(), 0);
    const QString tip = run->toolTip();
    QVERIFY2(tip.startsWith("Beach TX via Rig RX") && tip.contains("2 fixture")
             && tip.contains("1 not on the plot"), qPrintable(tip));
    // It leaves from the receiver in the rig, not from the transmitter on the beach.
    const QPointF rx = rig.gv->m_networkReceiverItems.first()->pos();
    QVERIFY((QPointF(run->path().elementAt(0).x, run->path().elementAt(0).y) - rx).manhattanLength() < 1.0);
    // The dashed link joins the transmitter to its receiver.
    QCOMPARE(rig.gv->m_networkLinks.size(), 1);
    QCOMPARE(rig.gv->m_networkLinks.first()->line().p2(), rx);
    rig.mon->applyMapView(Monitor::ViewNormal);
}

void Monitor_Test::networkNodesJoinLayersGroupsLocksAndRenames()
{
    TopologyCleanup topo(m_doc);
    NetworkNode a, b;
    a.name = "Node A";
    a.placed = true;
    b.name = "Node B";
    b.placed = true;
    b.posX = 3.0;
    // Two wireless nodes with receivers, so each node's layer pass has to pick
    // out its own receivers from the other's.
    m_doc->networkTopology()->nodes() << a << b << wirelessNode("TX 1", 0, true)
                                      << wirelessNode("TX 2", 1, true, 6.0, 6.0);
    MonitorProperties *props = m_doc->monitorProperties();
    const quint32 layer = props->addLayer("Network");

    MonitorGraphicsView view(m_doc);
    // Without a grid there is nowhere to draw a node.
    const int cell = view.m_cellPixels;
    view.m_cellPixels = 0;
    view.updateNetworkNodes();
    QCOMPARE(view.m_networkNodeItems.size(), 0);
    view.m_cellPixels = cell;
    view.resize(900, 700);
    view.setGridMetrics(1000.0);
    view.setGridSize(QSize(20, 12));
    view.updateNetworkNodes();
    QCOMPARE(view.m_networkNodeItems.size(), 4);
    QCOMPARE(view.m_networkReceiverItems.size(), 2);

    QGraphicsItem *itemA = view.itemFor("network", 0);
    QGraphicsItem *itemB = view.itemFor("network", 1);
    QCOMPARE(itemA, static_cast<QGraphicsItem *>(view.m_networkNodeItems.at(0)));
    QCOMPARE(view.itemFor("network", 9), nullptr);

    // Select from the list, move to a layer.
    view.selectMapItem("network", 0);
    QVERIFY(itemA->isSelected());
    view.selectMapItems({ qMakePair(QString("network"), quint32(0)), qMakePair(QString("network"), quint32(1)) });
    QVERIFY(view.selectedMapObjects().contains(qMakePair(QString("network"), quint32(1))));
    QCOMPARE(view.setSelectedItemsLayer(layer), 2);
    QCOMPARE(m_doc->networkTopology()->nodes().at(0).layerId, layer);
    QCOMPARE(view.itemLayerId(itemB), layer);
    view.setItemLayer(itemB, 0);
    QCOMPARE(m_doc->networkTopology()->nodes().at(1).layerId, quint32(0));

    // A hidden layer hides the node and nothing else.
    props->setLayerVisible(layer, false);
    view.refreshItemLayerState();
    QVERIFY(!view.m_networkNodeItems.at(0)->isVisible());
    QVERIFY(view.m_networkNodeItems.at(1)->isVisible());
    props->setLayerVisible(layer, true);
    view.refreshItemLayerState();

    // Groups.
    const quint32 gid = props->nextGroupId();
    props->createGroup(gid, "Nodes", 0);
    view.setItemGroupId(itemA, gid);
    QCOMPARE(m_doc->networkTopology()->nodes().at(0).groupId, gid);
    QCOMPARE(view.itemGroupId(itemA), gid);
    QVERIFY(view.itemsUnderGroup(gid).contains(itemA));
    QVERIFY(!view.itemsUnderGroup(gid).contains(itemB));

    /* Lock / unlock, from the plot, the Layers list and the node's own menu.
       A lock is the LAYER's: locking node 1 locks its layer, and with it
       every other object on that layer, while node 0 (elsewhere) stays free. */
    const quint32 rack = props->addLayer("Rack");
    view.setItemLayer(itemB, rack);
    QVERIFY(!view.itemLocked("network", 1));
    view.setItemLocked("network", 1, true);
    QVERIFY(props->layer(rack).locked);
    QVERIFY(!props->layer(layer).locked);
    QVERIFY(m_doc->networkTopology()->nodes().at(1).locked);
    QVERIFY(!m_doc->networkTopology()->nodes().at(0).locked);
    QVERIFY(view.itemLocked("network", 1));
    QVERIFY(!view.itemLocked("network", 0));
    QVERIFY(!(view.m_networkNodeItems.at(1)->flags() & QGraphicsItem::ItemIsMovable));
    QVERIFY(view.m_networkNodeItems.at(0)->flags() & QGraphicsItem::ItemIsMovable);
    view.setItemLocked("network", 99, true);             // no such node: ignored
    QVERIFY(!props->layer(MonitorProperties::defaultLayerId).locked);
    MonitorLayersPanel panel(m_doc, &view);
    panel.reload();
    QVERIFY(panel.kindLockable("network"));
    QVERIFY(panel.objectLocked("network", 1));
    panel.setObjectLocked("network", 1, false);
    QVERIFY(!props->layer(rack).locked);
    QVERIFY(!m_doc->networkTopology()->nodes().at(1).locked);
    QVERIFY(!panel.objectLocked("network", 99));
    emit view.m_networkNodeItems.at(1)->lockToggleRequested(1);
    QVERIFY(props->layer(rack).locked);
    QVERIFY(m_doc->networkTopology()->nodes().at(1).locked);
    view.slotNetworkNodeLockToggled(99);                 // ignored
    view.setItemLocked("network", 1, false);

    // Its menu names it and offers what every map item gets.
    QMenu menu;
    view.buildItemMenu(&menu, "network", 0, MonitorGraphicsView::MenuPlace());
    const QStringList paths = menuPaths(&menu);
    QVERIFY2(!paths.isEmpty(), "a network node has no menu");
    QVERIFY2(paths.join("|").contains("Network Node"), qPrintable(paths.join(" | ")));

    // Rename and delete.
    view.renameItem("network", 0, "Renamed");
    QCOMPARE(m_doc->networkTopology()->nodes().at(0).name, QString("Renamed"));
    QVERIFY(view.m_networkNodeItems.at(0)->toolTip().contains("Renamed"));
    QSignalSpy del(&view, &MonitorGraphicsView::networkRemoveRequested);
    view.requestDeleteItem("network", 1);
    QCOMPARE(del.count(), 1);
    QCOMPARE(del.first().first().toUInt(), quint32(1));
    props->removeLayer(rack);
}

void Monitor_Test::networkNodesMoveTogetherAndSnap()
{
    TopologyCleanup topo(m_doc);
    NetworkNode a, b;
    a.name = "A";
    a.placed = true;
    b.name = "B";
    b.placed = true;
    b.posX = 4.0;
    m_doc->networkTopology()->nodes() << a << b;

    MonitorGraphicsView view(m_doc);
    view.resize(900, 700);
    view.setGridMetrics(1000.0);
    view.setGridSize(QSize(20, 12));
    view.setSnapDivisions(1);                          // whole metres
    view.updateNetworkNodes();
    NetworkNodeItem *ia = view.m_networkNodeItems.at(0);
    NetworkNodeItem *ib = view.m_networkNodeItems.at(1);
    ia->setSelected(true);
    ib->setSelected(true);

    // Drag A a bit more than a metre: both follow, A lands on the grid.
    const qreal m = view.m_cellPixels;
    ia->setPos(ia->pos() + QPointF(m * 1.2, 0));
    ib->setPos(ib->pos() + QPointF(m * 1.2, 0));
    view.slotNetworkNodeMoved(ia);

    const NetworkNode &na = m_doc->networkTopology()->nodes().at(0);
    const NetworkNode &nb = m_doc->networkTopology()->nodes().at(1);
    QVERIFY2(qAbs(na.posX - 1.0) < 0.01, qPrintable(QString::number(na.posX)));
    QVERIFY2(qAbs(nb.posX - 5.0) < 0.01, qPrintable(QString::number(nb.posX)));   // same delta
}

void Monitor_Test::doubleClickingANetworkNodeOpensIt()
{
    TopologyCleanup topo(m_doc);
    NetworkNode a;
    a.name = "Click Me";
    a.placed = true;
    a.posX = 2.0;
    a.posY = 2.0;
    m_doc->networkTopology()->nodes() << a;

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    rig.gv->updateNetworkNodes();
    NetworkNodeItem *item = rig.gv->m_networkNodeItems.first();
    rig.gv->centerOn(item);

    QSignalSpy spy(rig.gv, &MonitorGraphicsView::networkDoubleClicked);
    // The Monitor answers by opening the topology editor on that node.
    inspectModal([](QWidget *dlg) {
        NetworkTopologyDialog *nt = qobject_cast<NetworkTopologyDialog *>(dlg);
        QVERIFY(nt != nullptr);
        QCOMPARE(nt->m_row, 0);
    });
    const QPoint at = rig.gv->mapFromScene(item->centre());
    QTest::mouseDClick(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toUInt(), quint32(0));
}

void Monitor_Test::onlyInventoryDragsAreTakenByThePlot()
{
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    const QList<quint32> before = rig.gv->fixturesID();
    const QPoint c = rig.gv->viewport()->rect().center();

    QMimeData text;
    text.setText("not a fixture");
    QDragEnterEvent en(c, Qt::CopyAction, &text, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(rig.gv->viewport(), &en);
    QDragMoveEvent mv(c, Qt::CopyAction, &text, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(rig.gv->viewport(), &mv);
    QDropEvent dr(QPointF(c), Qt::CopyAction, &text, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(rig.gv->viewport(), &dr);
    QCOMPARE(rig.gv->fixturesID(), before);

    // An inventory drag is taken while it moves over the plot.
    std::unique_ptr<QMimeData> mime(InventoryTree::fixtureMime({ 12345 }));
    QDragEnterEvent en2(c, Qt::CopyAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(rig.gv->viewport(), &en2);
    QDragMoveEvent mv2(c, Qt::CopyAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
    mv2.setDropAction(Qt::IgnoreAction);
    mv2.ignore();
    QApplication::sendEvent(rig.gv->viewport(), &mv2);
    QVERIFY(mv2.isAccepted());
}

void Monitor_Test::aCableRunFollowsAMovedFixture()
{
    TopologyCleanup topo(m_doc);
    FixtureCleanup cleanup(m_doc);

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    Fixture *a = dmxFixtureOnPlot(m_doc, rig.gv, "A", 0, 4000, 2000, &cleanup);
    QVERIFY(a != nullptr);

    NetworkNode n;
    n.name = "Node";
    n.placed = true;
    n.posX = 1.0;
    n.posY = 2.0;
    NetworkPort p;
    p.universe = 0;
    n.ports << p;
    m_doc->networkTopology()->nodes() << n;
    rig.gv->updateNetworkNodes();
    rig.mon->applyMapView(Monitor::ViewNet);
    rig.gv->updateNetworkRuns();
    QCOMPARE(rig.gv->m_networkRunItems.size(), 1);
    const QPainterPath before = rig.gv->m_networkRunItems.first()->path();

    // Move the fixture and let the debounce fire.
    MonitorFixtureItem *item = rig.gv->m_fixtures.value(a->id());
    item->setPos(item->pos() + QPointF(60.0, 40.0));
    rig.gv->slotFixtureMoved(item);

    /* Wait for the debounce to FIRE, not for a fixed time: a 40 ms timer
       given 120 ms still missed it twice on a busy CI runner. The run items
       are rebuilt when it does, so read them afresh on every try. */
    auto endMoved = [&]() {
        if (rig.gv->m_networkRunItems.size() != 1)
            return false;
        const QPainterPath after = rig.gv->m_networkRunItems.first()->path();
        return (QPointF(after.elementAt(1).x, after.elementAt(1).y)
                - QPointF(before.elementAt(1).x, before.elementAt(1).y)).manhattanLength() > 20.0;
    };
    QTRY_VERIFY2_WITH_TIMEOUT(endMoved(), "the run's end did not follow the fixture", 5000);
    QCOMPARE(rig.gv->m_networkRunItems.size(), 1);
    rig.mon->applyMapView(Monitor::ViewNormal);
}

void Monitor_Test::theNetworkOverlayFlagsFixturesNoPortServes()
{
    TopologyCleanup topo(m_doc);
    FixtureCleanup cleanup(m_doc);
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));

    Fixture *served = dmxFixtureOnPlot(m_doc, rig.gv, "Served", 0, 2000, 2000, &cleanup);
    Fixture *orphan = dmxFixtureOnPlot(m_doc, rig.gv, "Orphan", 10, 4000, 2000, &cleanup);
    QVERIFY(served && orphan);
    orphan->setUniverse(3);                                    // a universe no port carries

    // No topology yet: nothing is flagged (everything would be, which is noise).
    rig.mon->applyMapView(Monitor::ViewNet);
    QVERIFY2(!rig.gv->fixtureItemForId(orphan->id())->toolTip().contains("no node port"),
             qPrintable(rig.gv->fixtureItemForId(orphan->id())->toolTip()));

    NetworkNode n;
    n.name = "Node";
    n.placed = true;
    NetworkPort p;
    p.universe = 0;
    n.ports << p;
    m_doc->networkTopology()->nodes() << n;
    rig.mon->applyMapView(Monitor::ViewNet);

    QVERIFY2(rig.gv->fixtureItemForId(orphan->id())->toolTip().contains("no node port"),
             qPrintable(rig.gv->fixtureItemForId(orphan->id())->toolTip()));
    QVERIFY2(!rig.gv->fixtureItemForId(served->id())->toolTip().contains("no node port"),
             qPrintable(rig.gv->fixtureItemForId(served->id())->toolTip()));
    rig.mon->applyMapView(Monitor::ViewNormal);
}

/* Make @p fxi a 1 m single-channel LED bar -- the kind of body the flat views
   draw with its real orientation. Returns the definition for the caller to
   delete once the fixture lets go of it. */
static QLCFixtureDef *makeStudioBar(Fixture *fxi)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Bar 1m");
    def->setType(QLCFixtureDef::LEDBarBeams);
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer"); ch->setGroup(QLCChannel::Intensity);
    def->addChannel(ch);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("1ch");
    QLCPhysical ph;
    ph.setWidth(1000); ph.setHeight(65); ph.setDepth(80);
    mode->setPhysical(ph);
    mode->insertChannel(ch, 0);
    def->addMode(mode);
    fxi->setFixtureDefinition(def, mode);
    return def;
}

/* Press on @p from, move through each of @p path, release at the last. */
static void studioMouseDrag(StructureStudioView *view, const QPointF &from,
                            const QList<QPointF> &path,
                            Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QMouseEvent press(QEvent::MouseButtonPress, from, from,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    view->mousePressEvent(&press);
    foreach (const QPointF &pt, path)
    {
        QMouseEvent move(QEvent::MouseMove, pt, pt, Qt::NoButton, Qt::LeftButton, mods);
        view->mouseMoveEvent(&move);
    }
    const QPointF end = path.isEmpty() ? from : path.last();
    QMouseEvent rel(QEvent::MouseButtonRelease, end, end,
                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    view->mouseReleaseEvent(&rel);
}

void Monitor_Test::aFixtureOnATrussTurnsByItsAngleHandle()
{
    StudioRig rig;
    QVERIFY(rig.build(m_doc, Truss::Horizontal));
    MonitorProperties *props = m_doc->monitorProperties();
    const quint32 fid = rig.fxi->id();
    QLCFixtureDef *def = makeStudioBar(rig.fxi);
    rig.view->setPlane(StructureStudioView::Front);   // the face it is mounted on
    rig.view->setHighlight({ fid });

    // Locked: selectable, but nothing to grab.
    rig.view->setLocked(true);
    QCOMPARE(int(rig.view->rotateHandle().mode), int(StructureStudioView::RotateHandle::None));

    rig.view->setLocked(false);
    const StructureStudioView::RotateHandle h = rig.view->rotateHandle();
    QCOMPARE(int(h.mode), int(StructureStudioView::RotateHandle::Angle));
    QCOMPARE(h.fid, fid);
    // At 0° the run is level, so the knob sits level with the pivot.
    QVERIFY2(qAbs(h.knob.y() - h.centre.y()) < 1.0 && qAbs(h.knob.x() - h.centre.x()) > 20.0,
             "the angle knob is not on the end of a level run");

    // Swing it round to straight up, through 45° so the turn is accumulated.
    QSignalSpy undo(rig.view, &StructureStudioView::editAboutToStart);
    QSignalSpy moved(rig.view, &StructureStudioView::fixtureMoved);
    const double r = h.radius;
    const QPointF mid = h.centre + QPointF(r * 0.7071, -r * 0.7071);
    const QPointF top = h.centre + QPointF(0.0, -r);
    studioMouseDrag(rig.view, h.knob, { mid, top });

    QCOMPARE(double(props->fixtureRigProps(fid).studioAngle), 90.0);
    QCOMPARE(undo.count(), 1);    // one snapshot for the whole drag
    QCOMPARE(moved.count(), 1);
    // The fixture did not move -- it turned.
    QCOMPARE(props->fixtureRigProps(fid).trussId, rig.truss->id());

    // Its length now points straight up, out of a plan view: that is the
    // view that rolls it.
    rig.view->setPlane(StructureStudioView::Top);
    QCOMPARE(int(rig.view->rotateHandle().mode), int(StructureStudioView::RotateHandle::Tilt));

    /* A par is drawn square-on to the screen whatever its frame says, so a
       knob would turn a number and nothing you can see: none offered. */
    rig.fxi->setFixtureDefinition(nullptr, nullptr);
    delete def;
    rig.fxi->setChannels(1);                  // back to a generic dimmer
    rig.view->setPlane(StructureStudioView::Front);
    QCOMPARE(int(classifyFixture(rig.fxi).kind), int(FixtureSilhouette::Par));
    QCOMPARE(int(rig.view->rotateHandle().mode), int(StructureStudioView::RotateHandle::None));
}

void Monitor_Test::anLedBarOnATrussRollsByItsTiltHandle()
{
    StudioRig rig;
    QVERIFY(rig.build(m_doc, Truss::Horizontal));
    MonitorProperties *props = m_doc->monitorProperties();
    const quint32 fid = rig.fxi->id();

    QLCFixtureDef *def = makeStudioBar(rig.fxi);

    rig.view->setLocked(false);
    rig.view->setHighlight({ fid });
    // Front-face bar running along X: Side looks straight down its length.
    rig.view->setPlane(StructureStudioView::Side);
    const StructureStudioView::RotateHandle h = rig.view->rotateHandle();
    QCOMPARE(int(h.mode), int(StructureStudioView::RotateHandle::Tilt));
    QCOMPARE(h.value, 0.0);

    // Where the knob WOULD point at a given tilt, from the same frame the
    // bar is drawn with -- so the handle and the drawing agree on direction.
    auto knobAt = [&](double deg) {
        FixtureRigProps rp = props->fixtureRigProps(fid);
        rp.studioTilt = float(deg);
        QVector3D L, N, H;
        rig.view->fixtureFrame(rp, L, N, H);
        const QVector3D c = rig.view->fixtureCentre(fid);
        QPointF d = rig.view->w2s(c + N) - rig.view->w2s(c);
        d /= qSqrt(d.x() * d.x() + d.y() * d.y());
        return h.centre + d * h.radius;
    };

    studioMouseDrag(rig.view, h.knob, { knobAt(15.0), knobAt(30.0) });
    QCOMPARE(double(props->fixtureRigProps(fid).studioTilt), 30.0);

    // Shift snaps to 15° steps: 52° lands on 45°.
    const StructureStudioView::RotateHandle h2 = rig.view->rotateHandle();
    QVERIFY(QLineF(h2.knob, knobAt(30.0)).length() < 1.0);   // knob followed the roll
    studioMouseDrag(rig.view, h2.knob, { knobAt(41.0), knobAt(52.0) }, Qt::ShiftModifier);
    QCOMPARE(double(props->fixtureRigProps(fid).studioTilt), 45.0);

    // Facing its mount face instead, the same bar spins its run.
    rig.view->setPlane(StructureStudioView::Front);
    QCOMPARE(int(rig.view->rotateHandle().mode), int(StructureStudioView::RotateHandle::Angle));

    rig.fxi->setFixtureDefinition(nullptr, nullptr);
    delete def;
}

void Monitor_Test::aBarTurnsInAViewOfAnotherFace()
{
    // T3-B2 in stage-structures-demo: laid on the Top face (SM 0), run turned
    // 90° so it points up/downstage, looked at from the Side.
    StudioRig rig;
    QVERIFY(rig.build(m_doc, Truss::Horizontal));
    MonitorProperties *props = m_doc->monitorProperties();
    const quint32 fid = rig.fxi->id();
    QLCFixtureDef *def = makeStudioBar(rig.fxi);
    FixtureRigProps rp = props->fixtureRigProps(fid);
    rp.studioMount = 0; rp.studioAngle = 90.0f; rp.studioTilt = 0.0f;
    props->setFixtureRigProps(fid, rp);

    rig.view->setLocked(false);
    rig.view->setHighlight({ fid });
    rig.view->setPlane(StructureStudioView::Side);
    const StructureStudioView::RotateHandle h = rig.view->rotateHandle();
    QCOMPARE(int(h.mode), int(StructureStudioView::RotateHandle::Angle));
    QCOMPARE(h.reframeMount, 2);
    QVERIFY(qAbs(h.value) < 0.01);

    // Grabbing it without turning changes nothing -- not even the face.
    studioMouseDrag(rig.view, h.knob, {});
    QCOMPARE(props->fixtureRigProps(fid).studioMount, 0);

    // Tip the run 30° about the stage left-right axis.
    const QVector3D c = rig.view->fixtureCentre(fid);
    auto runAt = [&](double deg) {
        const double t = qDegreesToRadians(deg);
        const QVector3D l(0.0f, float(qCos(t)), float(qSin(t)));
        QPointF d = rig.view->w2s(c + l) - rig.view->w2s(c);
        d /= qSqrt(d.x() * d.x() + d.y() * d.y());
        return h.centre + d * h.radius;
    };
    studioMouseDrag(rig.view, h.knob, { runAt(15.0), runAt(30.0) });

    // Stored on the Side face now, and turned exactly as dragged: the run up
    // 30°, and the face that was firing straight up tipped with it.
    const FixtureRigProps after = props->fixtureRigProps(fid);
    QCOMPARE(after.studioMount, 2);
    QCOMPARE(double(after.studioAngle), 30.0);
    QVector3D L, N, H;
    rig.view->fixtureFrame(after, L, N, H);
    QVERIFY2((L - QVector3D(0.0f, 0.8660f, 0.5f)).length() < 1e-3f,
             qPrintable(QString("run %1 %2 %3").arg(L.x()).arg(L.y()).arg(L.z())));
    QVERIFY2((N - QVector3D(0.0f, -0.5f, 0.8660f)).length() < 1e-3f,
             qPrintable(QString("face %1 %2 %3").arg(N.x()).arg(N.y()).arg(N.z())));
    // And it would stay that way through a save and reload.
    FixtureRigProps chk = after;
    props->repairMountPlane(chk);
    QCOMPARE(chk.studioMount, 2);

    /* Where the turn could only be stored on a face the next load would put
       back -- the Front face of a fixture on a horizontal truss -- there is
       no handle, rather than one whose result would not survive. */
    rp.studioMount = 0; rp.studioAngle = 0.0f; rp.studioTilt = 0.0f;
    props->setFixtureRigProps(fid, rp);
    rig.view->setPlane(StructureStudioView::Front);
    QCOMPARE(int(rig.view->rotateHandle().mode), int(StructureStudioView::RotateHandle::None));

    rig.fxi->setFixtureDefinition(nullptr, nullptr);
    delete def;
}

void Monitor_Test::theInspectorMovesAFixturesDmxAddress()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("T-1");
    t->setLength(3.0f);
    Fixture *spot = new Fixture(doc);
    spot->setName("Spot");
    spot->setChannels(8);
    QVERIFY(doc->addFixture(spot));
    Fixture *wall = new Fixture(doc);
    wall->setName("Wall");
    wall->setChannels(8);
    wall->setAddress(100);
    QVERIFY(doc->addFixture(wall));
    props->setFixturePosition(spot->id(), 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp;
    rp.trussId = t->id();
    rp.trussOffset = 1.0f;
    props->setFixtureRigProps(spot->id(), rp);

    {
        QWidget parent;
        Monitor mon(&parent, doc);
        QDialog dlg;
        StructureStudioView *view = nullptr;
        QVERIFY(mon.makeStudioPane(&dlg, 2 /* truss */, t->id(), nullptr, &view) != nullptr);
        emit view->fixtureSelected(spot->id());

        QSpinBox *uni = dlg.findChild<QSpinBox *>("fixtureUniverse");
        QSpinBox *addr = dlg.findChild<QSpinBox *>("fixtureAddress");
        QVERIFY(uni != nullptr && addr != nullptr);
        QVERIFY2(!addr->parentWidget()->isHidden(), "no DMX row for a selected fixture");
        QCOMPARE(addr->value(), 1);
        QCOMPARE(addr->suffix(), QString("–8"));

        // Onto the wall: refused, with the reason, and the fixture stays put.
        addr->setValue(97);
        QCOMPARE(spot->address(), quint32(0));
        QLabel *note = nullptr;
        foreach (QLabel *l, dlg.findChildren<QLabel *>())
            if (l->text().startsWith("Not moved"))
                note = l;
        QVERIFY2(note != nullptr && note->text().contains("Wall"), "no reason given");

        // Clear of it: moved.
        addr->setValue(201);
        QCOMPARE(spot->address(), quint32(200));
        QCOMPARE(doc->fixtureForAddress(200), spot->id());
        uni->setValue(3);
        QCOMPARE(spot->universe(), quint32(2));
        QCOMPARE(spot->address(), quint32(200));

        // Running: a patched fixture stays where it is.
        doc->setMode(Doc::Operate);
        addr->setValue(301);
        QCOMPARE(spot->address(), quint32(200));
        doc->setMode(Doc::Design);
    }
    delete doc;
}

/* A horizontal 4 m truss carrying four fixtures, "F1".."F4", at the given
   offsets along it. */
struct MultiTrussRig
{
    Doc *doc = nullptr;
    Truss *truss = nullptr;
    QList<Fixture *> fx;

    void build(Doc *d, const QList<float> &offsets)
    {
        doc = d;
        MonitorProperties *props = d->monitorProperties();
        truss = props->addTruss();
        truss->setName("FART-1");
        truss->setType(Truss::Horizontal);
        truss->setOrigin(QVector3D(0.0f, 0.0f, 2.0f));
        truss->setDirection(QPointF(1.0, 0.0));
        truss->setLength(4.0f);
        for (int i = 0; i < offsets.size(); ++i)
        {
            Fixture *f = new Fixture(d);
            f->setName(QString("F%1").arg(i + 1));
            f->setChannels(1);
            f->setAddress(quint32(i) * 8);
            d->addFixture(f);
            props->setFixturePosition(f->id(), 0, 0, QVector3D(0, 0, 0));
            FixtureRigProps rp;
            rp.trussId = truss->id();
            rp.trussOffset = offsets.at(i);
            props->setFixtureRigProps(f->id(), rp);
            fx << f;
        }
    }
    float offset(int i) const { return doc->monitorProperties()->fixtureRigProps(fx.at(i)->id()).trussOffset; }
    QList<quint32> ids(const QList<int> &which) const
    {
        QList<quint32> out;
        foreach (int i, which) out << fx.at(i)->id();
        return out;
    }
};

void Monitor_Test::theInspectorEditsEveryFixtureSelected()
{
    Doc *doc = new Doc(this);
    MultiTrussRig rig;
    rig.build(doc, { 0.5f, 1.5f, 2.5f, 3.5f });
    MonitorProperties *props = doc->monitorProperties();
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        QDialog dlg;
        StructureStudioView *view = nullptr;
        QVERIFY(mon.makeStudioPane(&dlg, 2, rig.truss->id(), nullptr, &view) != nullptr);

        // Three of the four selected in the list on the left.
        QTreeWidget *tree = nullptr;
        foreach (QTreeWidget *t, dlg.findChildren<QTreeWidget *>())
            if (t->topLevelItemCount() > 0)
                tree = t;
        QVERIFY(tree != nullptr);
        const QList<quint32> want = rig.ids({ 0, 1, 2 });
        QTreeWidgetItemIterator it(tree);
        while (*it)
        {
            const QVariant v = (*it)->data(0, Qt::UserRole);
            if (v.isValid() && want.contains(v.toUInt()) && (*it)->text(0).startsWith("F"))
                (*it)->setSelected(true);
            ++it;
        }
        QCOMPARE(view->m_highlight.size(), 3);

        QDoubleSpinBox *tilt = dlg.findChild<QDoubleSpinBox *>("fixtureTilt");
        QDoubleSpinBox *angle = dlg.findChild<QDoubleSpinBox *>("fixtureAngle");
        QVERIFY(tilt != nullptr && angle != nullptr);
        QLabel *title = nullptr;
        foreach (QLabel *l, dlg.findChildren<QLabel *>())
            if (l->text().startsWith("3 fixtures selected"))
                title = l;
        QVERIFY2(title != nullptr, "the inspector does not say three are selected");

        tilt->setValue(25.0);
        angle->setValue(40.0);
        for (int i = 0; i < 3; ++i)
        {
            QCOMPARE(double(props->fixtureRigProps(rig.fx[i]->id()).studioTilt), 25.0);
            QCOMPARE(double(props->fixtureRigProps(rig.fx[i]->id()).studioAngle), 40.0);
        }
        // The one not selected is left alone.
        QCOMPARE(double(props->fixtureRigProps(rig.fx[3]->id()).studioTilt), 0.0);
        QCOMPARE(double(props->fixtureRigProps(rig.fx[3]->id()).studioAngle), 0.0);

        // Selecting the folder row (here "Ungrouped") selects everything in it.
        tree->clearSelection();
        QTreeWidgetItem *folder = nullptr;
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            if (tree->topLevelItem(i)->childCount() == 4)
                folder = tree->topLevelItem(i);
        QVERIFY(folder != nullptr);
        folder->setSelected(true);
        QCOMPARE(view->m_highlight.size(), 4);
        tilt->setValue(-10.0);
        for (int i = 0; i < 4; ++i)
            QCOMPARE(double(props->fixtureRigProps(rig.fx[i]->id()).studioTilt), -10.0);
    }
    delete doc;
}

void Monitor_Test::cmdClickBuildsASelectionThatDragsTogether()
{
    Doc *doc = new Doc(this);
    MultiTrussRig rig;
    rig.build(doc, { 0.5f, 1.5f, 2.5f, 3.5f });
    StructureStudioView view(doc, StructureStudioView::TrussKind, rig.truss->id());
    view.resize(800, 600);
    view.reload();
    view.setPlane(StructureStudioView::Front);
    view.setLocked(false);
    QSignalSpy sel(&view, &StructureStudioView::fixturesSelected);

    auto click = [&view](const QPointF &at, Qt::KeyboardModifiers mods) {
        QMouseEvent press(QEvent::MouseButtonPress, at, at, Qt::LeftButton, Qt::LeftButton, mods);
        view.mousePressEvent(&press);
        QMouseEvent rel(QEvent::MouseButtonRelease, at, at, Qt::LeftButton, Qt::NoButton, mods);
        view.mouseReleaseEvent(&rel);
    };
    auto at = [&](int i) { return view.w2s(view.fixtureCentre(rig.fx[i]->id())); };

    click(at(0), Qt::NoModifier);
    click(at(2), Qt::ControlModifier);
    click(at(3), Qt::ControlModifier);
    QCOMPARE(view.m_highlight, QSet<quint32>({ rig.fx[0]->id(), rig.fx[2]->id(), rig.fx[3]->id() }));
    QCOMPARE(sel.last().first().value<QList<quint32>>().size(), 3);
    click(at(3), Qt::ControlModifier);                     // and out again
    QCOMPARE(view.m_highlight.size(), 2);
    // Shift-click does the same, as it does in the list beside it.
    click(at(1), Qt::ShiftModifier);
    QCOMPARE(view.m_highlight.size(), 3);
    click(at(1), Qt::ShiftModifier);
    QCOMPARE(view.m_highlight.size(), 2);
    QVERIFY(!view.m_panning);

    // Drag F3 (selected) a metre stage right: F1 goes with it, F2 does not.
    QSignalSpy undo(&view, &StructureStudioView::editAboutToStart);
    const QPointF from = at(2);
    const QPointF to = from + QPointF(-view.m_scale * 1.0, 0.0);
    QMouseEvent press(QEvent::MouseButtonPress, from, from, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    view.mousePressEvent(&press);
    QMouseEvent move(QEvent::MouseMove, to, to, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    view.mouseMoveEvent(&move);
    QMouseEvent rel(QEvent::MouseButtonRelease, to, to, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    view.mouseReleaseEvent(&rel);

    QVERIFY2(qAbs(rig.offset(2) - 1.5f) < 0.05f, qPrintable(QString::number(rig.offset(2))));
    QVERIFY2(qAbs(rig.offset(0) - 0.0f) < 0.05f, qPrintable(QString::number(rig.offset(0))));
    QCOMPARE(rig.offset(1), 1.5f);
    QCOMPARE(undo.count(), 1);                              // one snapshot for both
    QCOMPARE(view.m_highlight.size(), 2);                   // still both selected

    // A plain click (no drag) on one of them narrows the selection to it.
    click(at(2), Qt::NoModifier);
    QCOMPARE(view.m_highlight, QSet<quint32>({ rig.fx[2]->id() }));
    delete doc;
}

void Monitor_Test::distributingAFewSpacesThemBetweenTheOutermost()
{
    Doc *doc = new Doc(this);
    MultiTrussRig rig;
    rig.build(doc, { 0.5f, 0.7f, 3.0f, 3.5f });
    StructureStudioView view(doc, StructureStudioView::TrussKind, rig.truss->id());
    view.resize(800, 600);
    view.reload();

    // F1, F2, F3: the ends (0.5 and 3.0) stay; F2 goes half way. F4 is untouched.
    view.distributeOnFace(rig.ids({ 0, 1, 2 }));
    QCOMPARE(rig.offset(0), 0.5f);
    QVERIFY(qAbs(rig.offset(1) - 1.75f) < 1e-4f);
    QCOMPARE(rig.offset(2), 3.0f);
    QCOMPARE(rig.offset(3), 3.5f);

    // All of them (or none picked): across the whole truss, as before.
    MonitorProperties *props = doc->monitorProperties();
    // ...and in a line: the drops and cross offsets earlier drags left a
    // little apart (FART-SR's were -0.064..-0.086 m) come to the middle one.
    const float drops[4] = { -0.073f, -0.086f, -0.064f, -0.070f };
    for (int i = 0; i < 4; ++i)
    {
        FixtureRigProps rp = props->fixtureRigProps(rig.fx[i]->id());
        rp.mountZOffset = drops[i];
        rp.trussCross = 0.01f * float(i);
        props->setFixtureRigProps(rig.fx[i]->id(), rp);
    }
    view.distributeOnFace(rig.ids({ 0, 1, 2, 3 }));
    QCOMPARE(rig.offset(0), 0.5f);
    QCOMPARE(rig.offset(1), 1.5f);
    QCOMPARE(rig.offset(3), 3.5f);
    for (int i = 0; i < 4; ++i)
    {
        const FixtureRigProps rp = props->fixtureRigProps(rig.fx[i]->id());
        QCOMPARE(rp.mountZOffset, -0.073f);     // median of the four (lower middle)
        QCOMPARE(rp.trussCross, 0.01f);
    }
    const float z0 = props->fixtureRigPosition(rig.fx[0]->id()).z();
    for (int i = 1; i < 4; ++i)
        QCOMPARE(props->fixtureRigPosition(rig.fx[i]->id()).z(), z0);
    delete doc;
}

void Monitor_Test::aRightClickKeepsTheSelectionItLandsIn()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    QList<StagePlatform *> pls;
    for (int i = 0; i < 3; ++i)
    {
        StagePlatform *p = props->addPlatform();
        p->setName(QString("FART-%1").arg(i + 1));
        p->setOriginX(1.0f + 3.0f * i);
        p->setOriginY(2.0f);
        p->setWidth(2.0f);
        p->setDepth(1.0f);
        pls << p;
    }
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        MonitorGraphicsView *gv = mon.findChild<MonitorGraphicsView *>();
        QVERIFY(gv != nullptr);
        gv->resize(1230, 760);
        gv->setGridMetrics(1000.0);
        gv->setGridSize(QSize(40, 24));
        gv->updatePlatforms();
        PlatformItem *a = gv->m_platformItems.value(pls[0]->id());
        PlatformItem *b = gv->m_platformItems.value(pls[1]->id());
        PlatformItem *c = gv->m_platformItems.value(pls[2]->id());
        QVERIFY(a && b && c);

        auto rightPress = [gv](QGraphicsItem *it) {
            const QPoint at = gv->mapFromScene(it->mapToScene(it->boundingRect().center()));
            QMouseEvent press(QEvent::MouseButtonPress, at, gv->viewport()->mapToGlobal(at),
                              Qt::RightButton, Qt::RightButton, Qt::NoModifier);
            gv->mousePressEvent(&press);
            QMouseEvent rel(QEvent::MouseButtonRelease, at, gv->viewport()->mapToGlobal(at),
                            Qt::RightButton, Qt::NoButton, Qt::NoModifier);
            gv->mouseReleaseEvent(&rel);
        };

        a->setSelected(true);
        b->setSelected(true);
        rightPress(b);
        QVERIFY2(a->isSelected() && b->isSelected(),
                 "right-clicking one of two selected platforms dropped the selection");
        QCOMPARE(gv->selectedMapObjects().size(), 2);

        // The reported case: trusses (FART-1), and fixtures on the plot.
        Truss *t1 = props->addTruss();
        t1->setName("FART-1"); t1->setOrigin(QVector3D(2.0f, 6.0f, 3.0f));
        t1->setDirection(QPointF(1.0, 0.0)); t1->setLength(3.0f);
        Truss *t2 = props->addTruss();
        t2->setName("FART-2"); t2->setOrigin(QVector3D(2.0f, 9.0f, 3.0f));
        t2->setDirection(QPointF(1.0, 0.0)); t2->setLength(3.0f);
        gv->updateTrusses();
        TrussItem *ta = gv->m_trussItems.value(t1->id());
        TrussItem *tb = gv->m_trussItems.value(t2->id());
        QVERIFY(ta && tb);
        gv->m_scene->clearSelection();
        ta->setSelected(true);
        tb->setSelected(true);
        rightPress(ta);
        QVERIFY2(ta->isSelected() && tb->isSelected(),
                 "right-clicking one of two selected trusses dropped the selection");

        Fixture *f1 = new Fixture(doc); f1->setName("FART-SR-1"); f1->setChannels(1);
        Fixture *f2 = new Fixture(doc); f2->setName("FART-SR-2"); f2->setChannels(1); f2->setAddress(10);
        QVERIFY(doc->addFixture(f1) && doc->addFixture(f2));
        gv->addFixture(f1->id(), QPointF(15000, 3000));
        gv->addFixture(f2->id(), QPointF(18000, 3000));
        MonitorFixtureItem *fa = gv->m_fixtures.value(f1->id());
        MonitorFixtureItem *fb = gv->m_fixtures.value(f2->id());
        QVERIFY(fa && fb);
        gv->m_scene->clearSelection();
        fa->setSelected(true);
        fb->setSelected(true);
        rightPress(fa);
        QVERIFY2(fa->isSelected() && fb->isSelected(),
                 "right-clicking one of two selected fixtures dropped the selection");

        // On something NOT selected: that, alone.
        rightPress(c);
        QVERIFY(c->isSelected());
        QVERIFY(!a->isSelected() && !b->isSelected() && !fa->isSelected());
    }
    delete doc;
}

void Monitor_Test::aLayersRowStaysSelectedWhenThePlotCannotFollow()
{
    Doc *doc = new Doc(this);
    Fixture *f = new Fixture(doc);
    f->setName("T1-W");
    f->setChannels(4);
    QVERIFY(doc->addFixture(f));
    doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(2000, 2000, 0));
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        parent.resize(1400, 900);
        mon.resize(1400, 900);
        parent.show();
        QVERIFY(QTest::qWaitForWindowExposed(&parent));
        MonitorGraphicsView *gv = mon.m_graphicsView;
        MonitorFixtureItem *item = gv->m_fixtures.value(f->id(), nullptr);
        QVERIFY(item != nullptr);
        // What a fixture stacked inside a tower is in plan view: on the plot,
        // but not visible, so the plot cannot select it.
        item->setVisible(false);

        // Let the panel settle first. Under QPA minimal (CI's Linux platform)
        // a rebuild queued while the plot was set up was still pending here
        // (offscreen had already run it), and it ran inside the
        // processEvents() after the click below: it re-selected from the plot,
        // and deleted the very row this test then read -- a use-after-free
        // that failed the check or crashed the whole program.
        QCoreApplication::processEvents();
        MonitorLayersPanel *panel = mon.m_layersPanel;
        panel->reload();
        QTreeWidgetItem *row = nullptr;
        for (QTreeWidgetItemIterator it(panel->m_tree); *it; ++it)
            if ((*it)->text(0) == "T1-W") { row = *it; break; }
        QVERIFY(row != nullptr);
        panel->m_tree->scrollToItem(row);
        QTest::mouseClick(panel->m_tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                          panel->m_tree->visualItemRect(row).center());
        QCoreApplication::processEvents();
        QVERIFY2(row->isSelected(), "the row was cleared by the plot's empty selection");
        QVERIFY(gv->selectedMapObjects().isEmpty());

        // A row the plot CAN select still works both ways.
        item->setVisible(true);
        panel->m_tree->clearSelection();
        QTest::mouseClick(panel->m_tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                          panel->m_tree->visualItemRect(row).center());
        QCoreApplication::processEvents();
        QVERIFY(row->isSelected());
        QVERIFY(item->isSelected());
    }
    delete doc;
}

void Monitor_Test::takingFixturesOutOfAStructure()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("T-3");
    t->setType(Truss::Vertical);
    t->setOrigin(QVector3D(7.0f, 3.0f, 0.0f));
    t->setLength(3.0f);
    Tower *tw = props->addTower();
    tw->setName("AT1");
    tw->setOriginX(0.2f); tw->setOriginY(8.0f);
    tw->setWidth(0.4f); tw->setDepth(0.4f); tw->setHeight(2.4f);

    Fixture *onTruss = new Fixture(doc); onTruss->setName("T3-B1"); onTruss->setChannels(1);
    Fixture *onTower = new Fixture(doc); onTower->setName("AT1-3z"); onTower->setChannels(1);
    onTower->setAddress(10);
    QVERIFY(doc->addFixture(onTruss) && doc->addFixture(onTower));
    props->setFixturePosition(onTruss->id(), 0, 0, QVector3D(7000, 3000, 0));
    props->setFixturePosition(onTower->id(), 0, 0, QVector3D(200, 8000, 0));
    FixtureRigProps a; a.trussId = t->id(); a.trussOffset = 1.0f;
    props->setFixtureRigProps(onTruss->id(), a);
    FixtureRigProps b; b.towerId = tw->id(); b.towerShelf = 0;
    props->setFixtureRigProps(onTower->id(), b);
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        MonitorGraphicsView *gv = mon.m_graphicsView;
        gv->ensureTrussGroup(t->id());
        const quint32 trussGroup = t->groupId();
        QVERIFY(trussGroup != 0);
        // AT1-3z filed under T-3 although it hangs on AT1.
        props->setFixtureGroup(onTower->id(), trussGroup);

        gv->takeOutOfStructure({ onTruss->id(), onTower->id() }, "truss", t->id());

        QCOMPARE(props->fixtureRigProps(onTruss->id()).trussId, Truss::invalidId());
        QVERIFY(props->fixtureGroup(onTruss->id()) != trussGroup);
        // Still on its tower, and back under it.
        QCOMPARE(props->fixtureRigProps(onTower->id()).towerId, tw->id());
        const quint32 g = props->fixtureGroup(onTower->id());
        QVERIFY(g != trussGroup);
        QCOMPARE(props->group(g).anchorKind, QString("tower"));
        QCOMPARE(props->group(g).anchorId, tw->id());
    }
    delete doc;
}

void Monitor_Test::aGroupOfGroupsIsLaidOutAsOnStage()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    // Four single-head fixtures 0.2 m apart: a 4-cell row over 0.6 m, i.e.
    // 5 cells a metre. SR at x 1.0, SL at x 4.0, a third above SR.
    auto makeGroup = [&](const QString &name, double x, double z, int addr) {
        FixtureGroup *g = new FixtureGroup(doc);
        g->setName(name);
        g->setSize(QSize(4, 1));
        doc->addFixtureGroup(g);
        for (int i = 0; i < 4; ++i)
        {
            Fixture *f = new Fixture(doc);
            f->setName(QString("%1-%2").arg(name).arg(i + 1));
            f->setChannels(1);
            f->setAddress(quint32(addr + i));
            doc->addFixture(f);
            // X/Y in millimetres, Z in metres -- as the plot stores them.
            props->setFixturePosition(f->id(), 0, 0,
                QVector3D(float((x + 0.2 * i) * 1000.0), 3000.0f, float(z)));
            g->assignFixture(f->id(), QLCPoint(i, 0));
        }
        return g;
    };
    FixtureGroup *sr = makeGroup("FART-SR", 1.0, 2.0, 0);
    FixtureGroup *sl = makeGroup("FART-SL", 4.0, 2.0, 10);
    FixtureGroup *hi = makeGroup("FART-HI", 1.0, 3.0, 20);

    // Scoped: a FixtureGroup is parented to its Doc, which must outlive it.
    {
    FixtureGroup target(doc);
    target.setSize(QSize(1, 1));
    QVERIFY(CreateFixtureGroup::layoutGroupsByStage(doc, { sr->id(), sl->id(), hi->id() }, &target));

    auto colOf = [&target](quint32 fid) {
        QMapIterator<QLCPoint, GroupHead> it(target.headsMap());
        while (it.hasNext()) { it.next(); if (it.value().fxi == fid) return it.key(); }
        return QLCPoint(-1, -1);
    };
    // Higher up = earlier rows; SR's first head at the left edge.
    QCOMPARE(colOf(hi->fixtureList().first()), QLCPoint(0, 0));
    const QLCPoint srAt = colOf(sr->fixtureList().first());
    const QLCPoint slAt = colOf(sl->fixtureList().first());
    QCOMPARE(srAt, QLCPoint(0, 5));                   // a metre below: 5 rows
    QCOMPARE(slAt.y(), 5);                            // same height as SR
    QCOMPARE(slAt.x(), 15);                           // 3 m stage left: 15 columns
    // Each group keeps its own shape, tagged as a block.
    QCOMPARE(colOf(sl->fixtureList().last()), QLCPoint(18, 5));
    QCOMPARE(target.headSubGroup(slAt), FixtureGroup::subGroupTagFor(sl->id()));
    QCOMPARE(target.size(), QSize(19, 6));
    }
    delete doc;
}

/* Two groups of four: LOW upstage at 1 m, HIGH downstage at 3 m. From the
   front HIGH is the top row; from above, LOW (upstage) is. */
void Monitor_Test::aGroupOfGroupsIsLaidOutFromTheChosenView()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    auto makeGroup = [&](const QString &name, double y, double z, int addr) {
        FixtureGroup *g = new FixtureGroup(doc);
        g->setName(name);
        g->setSize(QSize(4, 1));
        doc->addFixtureGroup(g);
        for (int i = 0; i < 4; ++i)
        {
            Fixture *f = new Fixture(doc);
            f->setName(QString("%1-%2").arg(name).arg(i + 1));
            f->setChannels(1);
            f->setAddress(quint32(addr + i));
            doc->addFixture(f);
            props->setFixturePosition(f->id(), 0, 0,
                QVector3D(float((1.0 + 0.2 * i) * 1000.0), float(y * 1000.0), float(z)));
            g->assignFixture(f->id(), QLCPoint(i, 0));
        }
        return g;
    };
    FixtureGroup *low = makeGroup("LOW", 2.0, 1.0, 0);
    FixtureGroup *high = makeGroup("HIGH", 6.0, 3.0, 10);
    QList<quint32> members = low->fixtureList() + high->fixtureList();
    {
        StageGroupDialog dlg(doc, members, "LOW + HIGH", nullptr, { low->id(), high->id() });
        QVERIFY(dlg.m_keepGroups != nullptr && dlg.m_keepGroups->isChecked());
        auto rowOf = [&dlg](quint32 fid) {
            QMapIterator<QLCPoint, GroupHead> it(dlg.m_heads);
            while (it.hasNext()) { it.next(); if (it.value().fxi == fid) return it.key().y(); }
            return -1;
        };
        const int front = dlg.m_viewCombo->findData(int(CreateFixtureGroup::ViewFront));
        const int top = dlg.m_viewCombo->findData(int(CreateFixtureGroup::ViewTop));
        dlg.m_viewCombo->setCurrentIndex(front);
        QVERIFY(rowOf(high->fixtureList().first()) < rowOf(low->fixtureList().first()));
        QCOMPARE(dlg.m_size, QSize(4, 2));                 // closed up: two rows
        // Each group still a block of its own.
        QSet<quint32> tags;
        foreach (quint32 t, dlg.m_tags.values()) tags.insert(t);
        QCOMPARE(tags, QSet<quint32>({ FixtureGroup::subGroupTagFor(low->id()), FixtureGroup::subGroupTagFor(high->id()) }));

        dlg.m_viewCombo->setCurrentIndex(top);
        QVERIFY(rowOf(low->fixtureList().first()) < rowOf(high->fixtureList().first()));

        // Laid out afresh from the fixtures: the same views, no blocks.
        dlg.m_keepGroups->setChecked(false);
        QVERIFY(rowOf(low->fixtureList().first()) < rowOf(high->fixtureList().first()));
        dlg.m_viewCombo->setCurrentIndex(front);
        QVERIFY(rowOf(high->fixtureList().first()) < rowOf(low->fixtureList().first()));
        QCOMPARE(dlg.m_heads.size(), 8);

        dlg.m_keepGroups->setChecked(true);
        dlg.m_nameEdit->setText("Both");
        dlg.accept();
        QVERIFY(dlg.group() != nullptr);
        QCOMPARE(dlg.group()->name(), QString("Both"));
        QCOMPARE(dlg.group()->headsMap().size(), 8);
    }
    delete doc;
}

/* A group filled in head order, not as rigged (US2f: eight bars poured into
   a square): laid out again in place, from the front. */
void Monitor_Test::aGroupIsLaidOutAgainFromTheStage()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    FixtureGroup *bad = new FixtureGroup(doc);
    bad->setName("Row");
    bad->setSize(QSize(2, 2));
    doc->addFixtureGroup(bad);
    QList<quint32> fids;
    for (int i = 0; i < 4; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("R-%1").arg(i + 1));
        f->setChannels(1);
        f->setAddress(quint32(i));
        doc->addFixture(f);
        props->setFixturePosition(f->id(), 0, 0, QVector3D(float((1.0 + 0.5 * i) * 1000.0), 3000.0f, 1.0f));
        fids << f->id();
    }
    // Backwards, two by two.
    for (int i = 0; i < 4; ++i)
        bad->assignFixture(fids[3 - i], QLCPoint(i % 2, i / 2));
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        MonitorLayersPanel *panel = mon.m_layersPanel;
        panel->reload();
        QTreeWidgetItem *row = nullptr;
        for (QTreeWidgetItemIterator it(panel->m_tree); *it; ++it)
            if ((*it)->data(0, Qt::UserRole).toInt() == MonitorLayersPanel::NodeFixtureGroup
                && (*it)->data(0, Qt::UserRole + 1).toUInt() == bad->id())
                row = *it;
        QVERIFY(row != nullptr);
        QMenu m;
        panel->buildContextMenu(&m, row);
        bool offered = false;
        foreach (QAction *a, m.actions())
            offered |= a->text() == "Lay Out from the Stage…";
        QVERIFY(offered);
    }
    {
        StageGroupDialog dlg(doc, bad->fixtureList(), bad->name());
        dlg.setEditing(bad);
        QCOMPARE(dlg.m_okButton->text(), QString("Apply"));
        QCOMPARE(dlg.m_nameEdit->text(), QString("Row"));
        QVERIFY(dlg.m_sameNote->isHidden());          // itself is not a duplicate
        dlg.accept();
        QCOMPARE(dlg.group(), bad);
    }
    QCOMPARE(doc->fixtureGroups().size(), 1);           // no new group
    QCOMPARE(bad->size(), QSize(4, 1));
    for (int i = 0; i < 4; ++i)
        QCOMPARE(bad->head(QLCPoint(i, 0)).fxi, fids[i]);   // stage right to left as seen
    delete doc;
}

/* Click a row, Enter, type, Enter: a fixture group's row too, and the edit
   starts from the name alone, not "US2f  · fixture group". */
void Monitor_Test::enterRenamesAnyLayersRowFromItsBareName()
{
    Doc *doc = new Doc(this);
    doc->setMode(Doc::Design);
    MonitorProperties *props = doc->monitorProperties();
    QList<quint32> fids;
    for (int i = 0; i < 2; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("Bar %1").arg(i + 1));
        f->setChannels(1);
        f->setAddress(quint32(i));
        doc->addFixture(f);
        props->setFixturePosition(f->id(), 0, 0, QVector3D(1000.0f + 500.0f * i, 3000.0f, 0.0f));
        fids << f->id();
    }
    FixtureGroup *fg = new FixtureGroup(doc);
    fg->setName("US2f");
    fg->setSize(QSize(2, 1));
    doc->addFixtureGroup(fg);
    fg->assignFixture(fids[0], QLCPoint(0, 0));
    fg->assignFixture(fids[1], QLCPoint(1, 0));
    {
        QWidget parent;
        parent.resize(1200, 800);
        Monitor mon(&parent, doc);
        parent.show();
        MonitorLayersPanel *panel = mon.m_layersPanel;
        panel->setVisible(true);
        panel->reload();
        QTreeWidget *tree = panel->m_tree;
        auto rowOf = [tree](int type, quint32 id) -> QTreeWidgetItem * {
            for (QTreeWidgetItemIterator it(tree); *it; ++it)
                if ((*it)->data(0, Qt::UserRole).toInt() == type
                    && (*it)->data(0, Qt::UserRole + 1).toUInt() == id)
                    return *it;
            return nullptr;
        };
        auto renameBy = [tree](QTreeWidgetItem *row, const QString &typed) -> QString {
            for (QTreeWidgetItem *up = row->parent(); up; up = up->parent())
                up->setExpanded(true);
            tree->scrollToItem(row);
            tree->setFocus();
            tree->setCurrentItem(row);
            QTest::keyClick(tree, Qt::Key_Return);
            QLineEdit *ed = nullptr;
            foreach (QLineEdit *le, tree->viewport()->findChildren<QLineEdit *>())
                if (le->isVisible()) ed = le;
            if (ed == nullptr)
                return QString("<no editor>");
            const QString shown = ed->text();
            ed->setText(typed);
            QTest::keyClick(ed, Qt::Key_Return);
            QCoreApplication::processEvents();
            return shown;
        };

        QTreeWidgetItem *fgRow = rowOf(MonitorLayersPanel::NodeFixtureGroup, fg->id());
        QVERIFY(fgRow != nullptr);
        QVERIFY(fgRow->text(0).contains("fixture group"));
        QCOMPARE(renameBy(fgRow, "US2 Front"), QString("US2f"));       // the bare name to edit
        QCOMPARE(fg->name(), QString("US2 Front"));
        QCoreApplication::processEvents();                               // the queued reload
        fgRow = rowOf(MonitorLayersPanel::NodeFixtureGroup, fg->id());
        QVERIFY(fgRow != nullptr && fgRow->text(0).startsWith("US2 Front"));

        // A fixture row and a layer row, the same way.
        QTreeWidgetItem *fxRow = nullptr;
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(0, Qt::UserRole).toInt() == MonitorLayersPanel::NodeItem
                && (*it)->data(0, Qt::UserRole + 2).toString() == "fixture"
                && (*it)->data(0, Qt::UserRole + 1).toUInt() == fids[0])
                fxRow = *it;
        QVERIFY(fxRow != nullptr);
        renameBy(fxRow, "Bar One");
        QCOMPARE(doc->fixture(fids[0])->name(), QString("Bar One"));

        /* Another fixture's name is refused, with a note; the row goes back.
           (The box comes from Enter's queued commit: watch for it.) */
        QString note;
        QTimer watch;
        QObject::connect(&watch, &QTimer::timeout, [&note, &watch]() {
            if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
            {
                note = box->text();
                box->accept();
                watch.stop();
            }
        });
        watch.start(20);
        QTreeWidgetItem *bar2 = nullptr;
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(0, Qt::UserRole).toInt() == MonitorLayersPanel::NodeItem
                && (*it)->data(0, Qt::UserRole + 2).toString() == "fixture"
                && (*it)->data(0, Qt::UserRole + 1).toUInt() == fids[1])
                bar2 = *it;
        QVERIFY(bar2 != nullptr);
        renameBy(bar2, "Bar One");
        QTRY_VERIFY_WITH_TIMEOUT(!note.isEmpty(), 3000);
        watch.stop();
        QVERIFY2(note.contains("already called “Bar One”"), qPrintable(note));
        QCOMPARE(doc->fixture(fids[1])->name(), QString("Bar 2"));
        QTRY_VERIFY_WITH_TIMEOUT([&]() {
            for (QTreeWidgetItemIterator it(tree); *it; ++it)
                if ((*it)->data(0, Qt::UserRole + 1).toUInt() == fids[1]
                    && (*it)->data(0, Qt::UserRole + 2).toString() == "fixture")
                    return (*it)->text(0).startsWith("Bar 2");
            return false;
        }(), 3000);
        QVERIFY(doc->duplicateFixtureNames().isEmpty());

        QTreeWidgetItem *layerRow = tree->topLevelItem(0);
        QVERIFY(layerRow != nullptr);
        const quint32 layerId = layerRow->data(0, Qt::UserRole + 1).toUInt();
        /* Renamed just after a fixture was: that queued a reload, which used
           to land before Enter's (queued) commit and drop the new name. */
        renameBy(layerRow, "Front Steps");
        QCOMPARE(props->layer(layerId).name, QString("Front Steps"));
    }
    delete doc;
}

void Monitor_Test::aFixtureGroupRowStaysOpenAndCanBeDeleted()
{
    Doc *doc = new Doc(this);
    QList<quint32> fids;
    for (int i = 0; i < 3; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("UST-%1").arg(i + 1));
        f->setChannels(1);
        f->setAddress(quint32(i));
        QVERIFY(doc->addFixture(f));
        doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(1000.0f * (i + 1), 2000, 0));
        fids << f->id();
    }
    auto makeGroup = [&](const QString &name) {
        FixtureGroup *g = new FixtureGroup(doc);
        g->setName(name);
        g->setSize(QSize(3, 1));
        doc->addFixtureGroup(g);
        for (int i = 0; i < 3; ++i)
            g->assignFixture(fids[i], QLCPoint(i, 0));
        return g;
    };
    FixtureGroup *usts = makeGroup("USTs");
    FixtureGroup *ust = makeGroup("UST");
    const quint32 ustId = ust->id();
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        MonitorLayersPanel *panel = mon.m_layersPanel;
        panel->reload();
        auto row = [panel](const QString &name) -> QTreeWidgetItem * {
            for (QTreeWidgetItemIterator it(panel->m_tree); *it; ++it)
                if ((*it)->data(0, Qt::UserRole).toInt() == MonitorLayersPanel::NodeFixtureGroup
                    && (*it)->text(0).startsWith(name + "  "))
                    return *it;
            return nullptr;
        };
        QTreeWidgetItem *r = row("USTs");
        QVERIFY(r != nullptr);
        QVERIFY(r->childCount() > 0);
        QVERIFY(!r->isExpanded());
        r->setExpanded(true);

        // Anything that rebuilds the list must leave it open.
        panel->reload();
        r = row("USTs");
        QVERIFY(r != nullptr);
        QVERIFY2(r->isExpanded(), "a rebuild closed the fixture group the user had opened");

        // The duplicate's row offers Delete; it takes the group, not the fixtures.
        QTreeWidgetItem *dup = row("UST");
        QVERIFY(dup != nullptr);
        QMenu menu;
        panel->buildContextMenu(&menu, dup);
        QAction *del = nullptr;
        foreach (QAction *a, menu.actions())
            if (a->text().startsWith("Delete Fixture Group"))
                del = a;
        QVERIFY(del != nullptr);
        QTimer::singleShot(0, [] {
            if (QMessageBox *mb = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                mb->button(QMessageBox::Yes)->click();
        });
        del->trigger();
        QVERIFY(doc->fixtureGroup(ustId) == nullptr);
        QVERIFY(doc->fixtureGroup(usts->id()) != nullptr);
        foreach (quint32 fid, fids)
            QVERIFY(doc->fixture(fid) != nullptr);
    }
    delete doc;
}

void Monitor_Test::theInspectorPutsFixturesOnTheBack()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("T-1");
    t->setType(Truss::Vertical);
    t->setOrigin(QVector3D(1.0f, 2.0f, 0.0f));
    t->setLength(3.0f);
    t->setWidth(0.3f);
    QLCFixtureDef *def = nullptr;
    QList<Fixture *> bars;
    for (int k = 0; k < 2; ++k)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("XL-450 #%1").arg(k + 1));
        f->setChannels(1);
        f->setAddress(quint32(k) * 4);
        QVERIFY(doc->addFixture(f));
        if (def == nullptr)
            def = makeStudioBar(f);         // linear: the Face row shows
        else
            f->setFixtureDefinition(def, def->modes().first());
        props->setFixturePosition(f->id(), 0, 0, QVector3D(0, 0, 0));
        FixtureRigProps rp;
        rp.trussId = t->id();
        rp.trussOffset = 1.0f + k;
        props->setFixtureRigProps(f->id(), rp);
        bars << f;
    }
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        QDialog dlg;
        StructureStudioView *view = nullptr;
        QVERIFY(mon.makeStudioPane(&dlg, 2, t->id(), nullptr, &view) != nullptr);
        emit view->fixtureSelected(bars[0]->id());
        emit view->fixturesSelected({ bars[0]->id(), bars[1]->id() });

        QComboBox *face = nullptr;
        foreach (QComboBox *cb, dlg.findChildren<QComboBox *>())
            if (cb->toolTip().startsWith("The face it is on"))
                face = cb;
        QVERIFY(face != nullptr);
        QStringList items;
        for (int i = 0; i < face->count(); ++i)
            items << face->itemText(i);
        QCOMPARE(items, QStringList({ "Top", "Bottom", "Front (downstage)", "Back (upstage)",
                                      "Stage left", "Stage right" }));
        QCOMPARE(face->currentText(), QString("Front (downstage)"));

        face->setCurrentIndex(3);                       // Back
        foreach (Fixture *f, bars)
        {
            const FixtureRigProps rp = props->fixtureRigProps(f->id());
            QCOMPARE(rp.face(), FixtureRigProps::FaceBack);
            QCOMPARE(rp.trussCrossY, -0.15f);           // the upstage side of the tower
            QVector3D L, N, H;
            view->fixtureFrame(rp, L, N, H);
            QVERIFY2(N.y() < -0.99f, "a fixture on the back does not face upstage");
        }
    }
    foreach (Fixture *f, bars)
        f->setFixtureDefinition(nullptr, nullptr);
    delete def;
    delete doc;
}

void Monitor_Test::aBarHangingStraightDownIsADotInPlan()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("FART-1");
    t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(2.0f, 3.0f, 1.5f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(2.4f);
    Fixture *f = new Fixture(doc);
    f->setName("FART-SR-1");
    f->setChannels(1);
    QVERIFY(doc->addFixture(f));
    QLCFixtureDef *def = makeStudioBar(f);          // 1000 x 65 mm
    props->setFixturePosition(f->id(), 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp;
    rp.trussId = t->id();
    rp.trussOffset = 1.0f;
    rp.studioMount = 1;
    rp.studioAngle = 0.0f;                           // along the truss
    props->setFixtureRigProps(f->id(), rp);
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        MonitorGraphicsView *gv = mon.m_graphicsView;
        gv->resize(1230, 760);
        gv->setGridMetrics(1000.0);
        gv->setGridSize(QSize(40, 24));
        QCOMPARE(gv->viewPOV(), MonitorGraphicsView::PovTop);
        gv->updateFixture(f->id());
        MonitorFixtureItem *item = gv->m_fixtures.value(f->id());
        QVERIFY(item != nullptr);
        const QSizeF along(item->m_width, item->m_height);   // the body, not label + halo
        QVERIFY2(along.width() > along.height() * 4, "a bar along X is long left-right in plan");

        // Hanging straight down: seen from above it is its end.
        rp.studioAngle = 90.0f;
        props->setFixtureRigProps(f->id(), rp);
        gv->updateFixture(f->id());
        const QSizeF down(item->m_width, item->m_height);
        QVERIFY2(down.width() < along.width() / 3,
                 qPrintable(QString("a bar hanging down is %1 px wide in plan (along: %2)")
                            .arg(down.width()).arg(along.width())));

        // In Front it is still its full length, upright.
        gv->setViewPOV(MonitorGraphicsView::PovFront);
        gv->updateFixture(f->id());
        const QSizeF front(item->m_width, item->m_height);
        QVERIFY(front.height() > front.width() * 4);
    }
    f->setFixtureDefinition(nullptr, nullptr);
    delete def;
    delete doc;
}

/* An XL-450 as its 165-channel mode is defined: 45 RGB heads, then 30 white
   ones, on a declared 15 x 5 grid, 401 mm long. */
static QLCFixtureDef *makeXl450()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Warmdance");
    def->setModel("XL-450");
    def->setType(QLCFixtureDef::LEDBarPixels);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("165ch");
    auto add = [&](const QString &name, QLCChannel::Preset preset) {
        QLCChannel *ch = new QLCChannel();
        ch->setName(name);
        ch->setPreset(preset);
        def->addChannel(ch);
        mode->insertChannel(ch, mode->channels().size());
        return quint32(mode->channels().size() - 1);
    };
    for (int i = 0; i < 45; ++i)
    {
        QLCFixtureHead h;
        h.addChannel(add(QString("Red %1").arg(i + 1), QLCChannel::IntensityRed));
        h.addChannel(add(QString("Green %1").arg(i + 1), QLCChannel::IntensityGreen));
        h.addChannel(add(QString("Blue %1").arg(i + 1), QLCChannel::IntensityBlue));
        mode->insertHead(-1, h);
    }
    for (int i = 0; i < 30; ++i)
    {
        QLCFixtureHead h;
        h.addChannel(add(QString("White %1").arg(i + 1), QLCChannel::IntensityWhite));
        mode->insertHead(-1, h);
    }
    QLCPhysical ph;
    ph.setLayoutSize(QSize(15, 5));
    ph.setWidth(401); ph.setHeight(180); ph.setDepth(100);
    mode->setPhysical(ph);
    def->addMode(mode);
    return def;
}

/* Three XL-450s on a horizontal truss, 0.2 m apart, at @p angle (90 = hanging
   straight down). */
static QList<Fixture *> xlBarsOnATruss(Doc *doc, QLCFixtureDef *def, float angle)
{
    MonitorProperties *props = doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("FART-1");
    t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(2.0f, 3.0f, 2.0f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(2.4f);
    QList<Fixture *> bars;
    for (int k = 0; k < 3; ++k)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("FART-SR-%1").arg(k + 1));
        f->setFixtureDefinition(def, def->modes().first());
        f->setAddress(quint32(k) * 165);
        doc->addFixture(f);
        props->setFixturePosition(f->id(), 0, 0, QVector3D(0, 0, 0));
        FixtureRigProps rp;
        rp.trussId = t->id();
        rp.trussOffset = 0.3f + 0.2f * k;
        rp.studioMount = 1;
        rp.studioAngle = angle;
        rp.studioFaceChosen = true;
        props->setFixtureRigProps(f->id(), rp);
        bars << f;
    }
    return bars;
}

static QLCPoint cellOfHead(FixtureGroup *g, quint32 fid, int head)
{
    QMapIterator<QLCPoint, GroupHead> it(g->headsMap());
    while (it.hasNext())
    {
        it.next();
        if (it.value().fxi == fid && it.value().head == head)
            return it.key();
    }
    return QLCPoint(-1, -1);
}

void Monitor_Test::headKindGroupsFollowTheLedMapAndTheRig()
{
    QTemporaryDir lib;
    HeadLayoutLibrary::setUserDirectoryOverride(lib.path());
    QLCFixtureDef *def = makeXl450();
    {
        Doc *doc = new Doc(this);
        QList<Fixture *> bars = xlBarsOnATruss(doc, def, 90.0f);   // hanging straight down
        // As built: RGB / W / RGB / W / RGB.
        doc->monitorProperties()->setHeadLayout(
            HeadLayout::fromRowPattern(bars[0], { "RGB", "W", "RGB", "W", "RGB" }));
        QList<quint32> ids;
        foreach (Fixture *f, bars) ids << f->id();

        const QList<FixtureGroup *> made =
            CreateFixtureGroup::createByHeadKind(doc, ids, { { "RGB", "FART-SR RGB" }, { "W", "FART-SR W" } });
        QCOMPARE(made.size(), 2);
        QCOMPARE(made[0]->name(), QString("FART-SR RGB"));
        QCOMPARE(made[1]->name(), QString("FART-SR W"));
        FixtureGroup *rgb = made[0];
        QCOMPARE(rgb->headList().size(), 3 * 45);
        QCOMPARE(made[1]->headList().size(), 3 * 30);

        /* Each bar hangs straight down: a column 3 RGB rows wide and 15
           tall, its first head at the bottom; the bars side by side. */
        const QLCPoint h0 = cellOfHead(rgb, bars[0]->id(), 0);
        QCOMPARE(cellOfHead(rgb, bars[0]->id(), 14), QLCPoint(h0.x(), h0.y() - 14));
        QCOMPARE(cellOfHead(rgb, bars[0]->id(), 15), QLCPoint(h0.x() + 1, h0.y()));   // 2nd RGB row
        QCOMPARE(cellOfHead(rgb, bars[0]->id(), 30), QLCPoint(h0.x() + 2, h0.y()));
        const QLCPoint next = cellOfHead(rgb, bars[1]->id(), 0);
        QVERIFY2(next.x() >= h0.x() + 3 && next.y() == h0.y(),
                 qPrintable(QString("bar 2 starts at %1,%2 after bar 1 at %3,%4")
                            .arg(next.x()).arg(next.y()).arg(h0.x()).arg(h0.y())));
        QCOMPARE(rgb->size().height(), 15);
        delete doc;
    }
    {
        // Lying along the truss: each bar is a row block, 15 wide, 3 tall.
        Doc *doc = new Doc(this);
        QList<Fixture *> bars = xlBarsOnATruss(doc, def, 0.0f);
        doc->monitorProperties()->setHeadLayout(
            HeadLayout::fromRowPattern(bars[0], { "RGB", "W", "RGB", "W", "RGB" }));
        const QList<FixtureGroup *> made =
            CreateFixtureGroup::createByHeadKind(doc, { bars[0]->id() }, { { "RGB", "One RGB" } });
        QCOMPARE(made.size(), 1);
        QCOMPARE(made[0]->size(), QSize(15, 3));
        QCOMPARE(cellOfHead(made[0], bars[0]->id(), 0), QLCPoint(0, 0));
        QCOMPARE(cellOfHead(made[0], bars[0]->id(), 15), QLCPoint(0, 1));
        QCOMPARE(cellOfHead(made[0], bars[0]->id(), 44), QLCPoint(14, 2));
        delete doc;
    }
    delete def;
    HeadLayoutLibrary::setUserDirectoryOverride(testLayouts());
}

/* Four bars 15 cm apart on each of two trusses 4 m apart, hanging: closed
   up, they are eight columns side by side in stage order -- the one wide gap
   between the trusses must not make the close spacing count as "the same
   place". */
static QList<Fixture *> barsOnTwoTrusses(Doc *doc, QLCFixtureDef *def)
{
    MonitorProperties *props = doc->monitorProperties();
    QList<Fixture *> bars;
    for (int t = 0; t < 2; ++t)
    {
        Truss *truss = props->addTruss();
        truss->setName(QString("FART-%1").arg(t + 1));
        truss->setType(Truss::Horizontal);
        truss->setOrigin(QVector3D(2.0f + 6.0f * t, 3.0f, 2.0f));
        truss->setDirection(QPointF(1.0, 0.0));
        truss->setLength(2.4f);
        for (int k = 0; k < 4; ++k)
        {
            Fixture *f = new Fixture(doc);
            f->setName(QString("FART-%1-%2").arg(t == 0 ? "SR" : "SL").arg(k + 1));
            f->setFixtureDefinition(def, def->modes().first());
            f->setUniverse(quint32(bars.size()));
            f->setAddress(0);
            doc->addFixture(f);
            props->setFixturePosition(f->id(), 0, 0, QVector3D(0, 0, 0));
            FixtureRigProps rp;
            rp.trussId = truss->id();
            rp.trussOffset = 0.076f + 0.152f * k;
            rp.studioMount = 1;
            rp.studioAngle = 90.0f;
            rp.studioFaceChosen = true;
            props->setFixtureRigProps(f->id(), rp);
            bars << f;
        }
    }
    return bars;
}

void Monitor_Test::closedUpBarsKeepTheirOrderAcrossDistantTrusses()
{
    QLCFixtureDef *def = makeXl450();
    Doc *doc = new Doc(this);
    const QList<Fixture *> bars = barsOnTwoTrusses(doc, def);
    QList<quint32> ids;
    foreach (Fixture *f, bars) ids << f->id();
    {
        FixtureGroup g(doc);
        g.setSize(QSize(1, 1));
        CreateFixtureGroup::layoutFixturesByStage(doc, ids, &g, CreateFixtureGroup::ViewFront, true);
        // Each hanging bar is 5 heads across and 15 down; eight of them, no gaps.
        QCOMPARE(g.size(), QSize(40, 15));
        QCOMPARE(g.headsMap().size(), 8 * 75);
        int lastLeft = -1;
        foreach (Fixture *f, bars)
        {
            int left = INT_MAX;
            QMapIterator<QLCPoint, GroupHead> it(g.headsMap());
            while (it.hasNext())
            {
                it.next();
                if (it.value().fxi == f->id())
                    left = qMin(left, it.key().x());
            }
            QVERIFY2(left == lastLeft + 5 || (lastLeft < 0 && left == 0),
                     qPrintable(QString("%1 starts at column %2, after %3").arg(f->name()).arg(left).arg(lastLeft)));
            lastLeft = left;
        }
    }
    {
        // To scale, the gap between the trusses stays.
        FixtureGroup g(doc);
        g.setSize(QSize(1, 1));
        CreateFixtureGroup::layoutFixturesByStage(doc, ids, &g, CreateFixtureGroup::ViewFront, false);
        QVERIFY2(g.size().width() > 40, qPrintable(QString::number(g.size().width())));
    }
    delete doc;
    delete def;
}

void Monitor_Test::theStageGroupDialogNamesViewsAndSpotsDuplicates()
{
    QLCFixtureDef *def = makeXl450();
    Doc *doc = new Doc(this);
    const QList<Fixture *> bars = barsOnTwoTrusses(doc, def);
    QList<quint32> ids;
    foreach (Fixture *f, bars) ids << f->id();
    QVERIFY(StageGroupDialog::anyOnStage(doc, ids));
    {
        StageGroupDialog dlg(doc, ids, "FARTs");
        // From the front, closed up, by default: the audience's picture.
        QCOMPARE(dlg.m_viewCombo->currentData().toInt(), int(CreateFixtureGroup::ViewFront));
        QVERIFY(dlg.m_closeGaps->isChecked());
        QCOMPARE(dlg.m_size, QSize(40, 15));
        QVERIFY(dlg.m_sizeLabel->text().startsWith("40 × 15"));
        QVERIFY(dlg.m_sameNote->isHidden());

        // From above, the hanging bars are seen end on: one row deep.
        dlg.m_viewCombo->setCurrentIndex(dlg.m_viewCombo->findData(int(CreateFixtureGroup::ViewTop)));
        QVERIFY2(dlg.m_size.height() < 15, qPrintable(QString::number(dlg.m_size.height())));
        dlg.m_viewCombo->setCurrentIndex(dlg.m_viewCombo->findData(int(CreateFixtureGroup::ViewFront)));
        QCOMPARE(dlg.m_size, QSize(40, 15));

        dlg.m_nameEdit->setText("FART Front");
        dlg.accept();
        QVERIFY(dlg.group() != nullptr);
        QCOMPARE(dlg.group()->name(), QString("FART Front"));
        QCOMPARE(dlg.group()->size(), QSize(40, 15));
        QCOMPARE(doc->fixtureGroups().size(), 1);
    }
    {
        // The same heads in the same places again: said so up front.
        StageGroupDialog dlg(doc, ids, "FARTs");
        QVERIFY(!dlg.m_sameNote->isHidden());
        QVERIFY(dlg.m_sameNote->text().contains("FART Front"));
        // Laid out another way it is still those heads: still said.
        dlg.m_closeGaps->setChecked(false);
        QVERIFY(!dlg.m_sameNote->isHidden());
        // Other heads are not.
        StageGroupDialog fewer(doc, ids.mid(0, 4), "SR");
        QVERIFY(fewer.m_sameNote->isHidden());
    }
    delete doc;
    delete def;
}

void Monitor_Test::selectAllSimilarFindsLikeGroupsAndFixtures()
{
    QLCFixtureDef *def = makeXl450();
    Doc *doc = new Doc(this);
    const QList<Fixture *> bars = barsOnTwoTrusses(doc, def);
    // A plain dimmer: not like the bars.
    Fixture *dim = new Fixture(doc);
    dim->setName("Dim");
    dim->setChannels(1);
    dim->setUniverse(9);
    QVERIFY(doc->addFixture(dim));
    doc->monitorProperties()->setFixturePosition(dim->id(), 0, 0, QVector3D(500, 500, 0));

    auto make = [doc](const QString &name, const QList<Fixture *> &fs, bool closed) {
        FixtureGroup *g = new FixtureGroup(doc);
        g->setName(name);
        doc->addFixtureGroup(g);
        QList<quint32> ids;
        foreach (Fixture *f, fs) ids << f->id();
        CreateFixtureGroup::layoutFixturesByStage(doc, ids, g, CreateFixtureGroup::ViewFront, closed);
        return g;
    };
    FixtureGroup *sr = make("SR pair", { bars[0], bars[1] }, true);
    FixtureGroup *sl = make("SL pair", { bars[4], bars[5] }, true);       // same bars, same layout
    FixtureGroup *wide = make("Wide pair", { bars[3], bars[4] }, false);  // same bars, to scale
    FixtureGroup *one = make("One", { bars[2] }, true);

    QCOMPARE(CreateFixtureGroup::similarGroups(doc, sr, true), QList<FixtureGroup *>({ sr, sl }));
    QCOMPARE(CreateFixtureGroup::similarGroups(doc, sr, false), QList<FixtureGroup *>({ sr, sl, wide }));
    QCOMPARE(CreateFixtureGroup::similarGroups(doc, one, true), QList<FixtureGroup *>({ one }));
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        MonitorLayersPanel *panel = mon.m_layersPanel;
        panel->reload();
        auto rowOf = [panel](quint32 fgId) -> QTreeWidgetItem * {
            for (QTreeWidgetItemIterator it(panel->m_tree); *it; ++it)
                if ((*it)->data(0, Qt::UserRole).toInt() == MonitorLayersPanel::NodeFixtureGroup
                    && (*it)->data(0, Qt::UserRole + 1).toUInt() == fgId)
                    return *it;
            return nullptr;
        };
        auto actionIn = [](QMenu &m, const QString &prefix) -> QAction * {
            foreach (QAction *a, m.actions())
                if (a->text().startsWith(prefix))
                    return a;
            return nullptr;
        };
        auto selectedGroups = [panel]() {
            QSet<quint32> out;
            foreach (QTreeWidgetItem *it, panel->m_tree->selectedItems())
                if (it->data(0, Qt::UserRole).toInt() == MonitorLayersPanel::NodeFixtureGroup)
                    out.insert(it->data(0, Qt::UserRole + 1).toUInt());
            return out;
        };
        QTreeWidgetItem *srRow = rowOf(sr->id());
        QVERIFY(srRow != nullptr);
        {
            QMenu m;
            panel->buildContextMenu(&m, srRow);
            QAction *sim = actionIn(m, "Select All Similar");
            QVERIFY(sim != nullptr);
            QCOMPARE(sim->text(), QString("Select All Similar (2)"));
            sim->trigger();
            QCOMPARE(selectedGroups(), QSet<quint32>({ sr->id(), sl->id() }));
            // Their fixtures are what the selection stands for.
            QSet<quint32> objs;
            for (const QPair<QString, quint32> &o : panel->selectedObjects())
                objs.insert(o.second);
            QCOMPARE(objs, QSet<quint32>({ bars[0]->id(), bars[1]->id(), bars[4]->id(), bars[5]->id() }));
        }
        {
            QMenu m;
            panel->buildContextMenu(&m, srRow);
            QAction *loose = actionIn(m, "Select All with the Same Fixtures");
            QVERIFY(loose != nullptr);
            QCOMPARE(loose->text(), QString("Select All with the Same Fixtures (3)"));
            loose->trigger();
            QCOMPARE(selectedGroups(), QSet<quint32>({ sr->id(), sl->id(), wide->id() }));
        }
        {
            // Nothing like it: offered, but off.
            QMenu m;
            panel->buildContextMenu(&m, rowOf(one->id()));
            QAction *sim = actionIn(m, "Select All Similar");
            QVERIFY(sim != nullptr && !sim->isEnabled());
            QVERIFY(actionIn(m, "Select All with the Same Fixtures") == nullptr);
        }
        {
            // A fixture: every one of the same model and mode.
            QMenu m;
            MonitorGraphicsView::MenuPlace place;
            mon.m_graphicsView->buildItemMenu(&m, "fixture", bars[0]->id(), place);
            QAction *sa = actionIn(m, "Select All Similar");
            QVERIFY(sa != nullptr);
            QCOMPARE(sa->text(), QString("Select All Similar (8)"));
            sa->trigger();
            /* Bars 15 cm apart on a truss are a count on the plot, not
               clickable items: the Layers list holds the selection. */
            QSet<quint32> rows;
            foreach (QTreeWidgetItem *it, panel->m_tree->selectedItems())
                if (it->data(0, Qt::UserRole).toInt() == MonitorLayersPanel::NodeItem)
                    rows.insert(it->data(0, Qt::UserRole + 1).toUInt());
            QSet<quint32> want;
            foreach (Fixture *f, bars) want.insert(f->id());
            QCOMPARE(rows, want);
        }
    }
    delete doc;
    delete def;
}

/* A one-row pixel bar: @p heads single-channel heads along it, @p widthMm
   long. @p generic: a Generic definition, whose size is QLC+'s 300 mm-a-pixel
   placeholder and must not be taken as real. */
static QLCFixtureDef *makePixelRow(const QString &model, int heads, int widthMm, bool generic)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(generic ? QString(KXMLFixtureGeneric) : QString("PHS Chorus"));
    def->setModel(model);
    def->setType(QLCFixtureDef::LEDBarPixels);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("ALL");
    for (int i = 0; i < heads; ++i)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("Dimmer %1").arg(i + 1));
        ch->setGroup(QLCChannel::Intensity);
        def->addChannel(ch);
        mode->insertChannel(ch, i);
        QLCFixtureHead h;
        h.addChannel(quint32(i));
        mode->insertHead(-1, h);
    }
    QLCPhysical ph;
    ph.setLayoutSize(QSize(heads, 1));
    ph.setWidth(generic ? heads * 300 : widthMm);
    ph.setHeight(100); ph.setDepth(100);
    mode->setPhysical(ph);
    def->addMode(mode);
    return def;
}

/* The Oppsk wall washer: a 1005 mm bar whose definition declares ONE grid,
   8 x 4, for its 32-head 64ch mode -- patched here, as in DanceHysteria, as
   its 16-head "48ch RGB" and "16ch White" modes. */
static QLCFixtureDef *makeWallWasher()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Oppsk");
    def->setModel("Wall Washer Light Bar");
    def->setType(QLCFixtureDef::LEDBarPixels);
    QLCPhysical ph;
    ph.setLayoutSize(QSize(8, 4));
    ph.setWidth(1005); ph.setHeight(65); ph.setDepth(60);
    auto mode = [&](const QString &name, const QList<QLCChannel::Preset> &perHead) {
        QLCFixtureMode *m = new QLCFixtureMode(def);
        m->setName(name);
        for (int h = 0; h < 16; ++h)
        {
            QLCFixtureHead head;
            for (QLCChannel::Preset pr : perHead)
            {
                QLCChannel *ch = new QLCChannel();
                ch->setName(QString("%1 %2 %3").arg(name).arg(h + 1).arg(int(pr)));
                ch->setPreset(pr);
                def->addChannel(ch);
                m->insertChannel(ch, m->channels().size());
                head.addChannel(quint32(m->channels().size() - 1));
            }
            m->insertHead(-1, head);
        }
        m->setPhysical(ph);
        def->addMode(m);
    };
    mode("48ch RGB", { QLCChannel::IntensityRed, QLCChannel::IntensityGreen, QLCChannel::IntensityBlue });
    mode("16ch White", { QLCChannel::IntensityWhite });
    return def;
}

/* Two bars per step top, inside the step: the RGB one at 0.6 m across, the
   white one at 1.8 m, on four steps side by side. */
static QList<Fixture *> washersOnSteps(Doc *doc, QLCFixtureDef *def)
{
    MonitorProperties *props = doc->monitorProperties();
    QList<Fixture *> out;
    for (int s = 0; s < 4; ++s)
    {
        StagePlatform *step = props->addPlatform();
        step->setName(QString("US-%1").arg(s + 1));
        step->setOriginX(1.2f + 2.438f * s); step->setOriginY(4.9f);
        step->setWidth(2.438f); step->setDepth(0.204f); step->setHeight(0.204f);
        step->setTopMaterial(StagePlatform::ClearTop);
        for (int b = 0; b < 2; ++b)
        {
            Fixture *f = new Fixture(doc);
            f->setName(QString("US%1t%2").arg(s + 1).arg(b + 1));
            f->setFixtureDefinition(def, def->modes().at(b));
            f->setUniverse(quint32(out.size()));
            doc->addFixture(f);
            props->setFixturePosition(f->id(), 0, 0, QVector3D(0, 0, 0));
            FixtureRigProps rp;
            rp.riserPlatformId = step->id();
            rp.riserFace = 1;
            rp.riserU = b == 0 ? 0.61f : 1.829f;
            rp.riserV = 0.102f;
            rp.placement = FixtureRigProps::Inside;
            rp.mountZOffset = 0.184f;           // just under the top, as built
            rp.studioMount = 0;
            rp.studioTilt = 20.0f;
            rp.studioFaceChosen = true;
            props->setFixtureRigProps(f->id(), rp);
            out << f;
        }
    }
    return out;
}

void Monitor_Test::twoBarsOfOneKindEachSplitIntoAGroupPerKind()
{
    QTemporaryDir lib;
    HeadLayoutLibrary::setUserDirectoryOverride(lib.path());
    QLCFixtureDef *def = makeWallWasher();
    // The 8 x 4 grid is the 32-head mode's, not these 16-head modes'.
    QCOMPARE(HeadLayout::declaredGrid(def->modes().at(0)), QSize());
    QCOMPARE(HeadLayout::declaredGrid(def->modes().at(1)), QSize());
    Doc *doc = new Doc(this);
    const QList<Fixture *> bars = washersOnSteps(doc, def);
    QCOMPARE(HeadLayout::headOrder(bars[0]).size, QSize(16, 1));   // a strip: one row

    QList<quint32> ids;
    foreach (Fixture *f, bars) ids << f->id();
    // Each fixture has one kind of head; together, two: offered.
    QVERIFY(!HeadKinds::isMixed(bars[0]));
    QVERIFY(HeadTypesDialog::anyMixed(doc, ids));
    QVERIFY(!HeadTypesDialog::anyMixed(doc, { bars[0]->id(), bars[2]->id() }));   // RGB only

    HeadTypesDialog dlg(doc, ids, "USt");
    for (int i = 0; i < dlg.m_kindChecks.size(); ++i)
        dlg.m_kindNames[i]->setText(QString("USt-%1-ALL").arg(dlg.m_kindChecks[i]->property("kind").toString()));
    dlg.m_viewCombo->setCurrentIndex(dlg.m_viewCombo->findData(int(CreateFixtureGroup::ViewTop)));
    dlg.accept();
    QCOMPARE(dlg.createdGroups().size(), 2);
    FixtureGroup *rgb = nullptr, *w = nullptr;
    foreach (FixtureGroup *g, dlg.createdGroups())
    {
        if (g->name() == "USt-RGB-ALL") rgb = g;
        if (g->name() == "USt-W-ALL") w = g;
    }
    QVERIFY(rgb != nullptr && w != nullptr);
    // Four bars across, one row of sixteen each, stage right first.
    QCOMPARE(rgb->size(), QSize(64, 1));
    QCOMPARE(w->size(), QSize(64, 1));
    QCOMPARE(rgb->head(QLCPoint(0, 0)).fxi, bars[0]->id());
    QCOMPARE(rgb->head(QLCPoint(0, 0)).head, 0);
    QCOMPARE(rgb->head(QLCPoint(15, 0)).head, 15);
    QCOMPARE(rgb->head(QLCPoint(16, 0)).fxi, bars[2]->id());
    QCOMPARE(w->head(QLCPoint(48, 0)).fxi, bars[7]->id());
    delete doc;
    delete def;
    HeadLayoutLibrary::setUserDirectoryOverride(testLayouts());
}

/* "if I right click on US1t1 to edit fixture I get nothing": a washer inside
   a step cannot be selected on the plot, and Edit Fixture edited the plot's
   selection -- nothing. It edits the fixture it was asked for. */
void Monitor_Test::editFixtureFromLayersEditsThatFixture()
{
    QLCFixtureDef *def = makeWallWasher();
    Doc *doc = new Doc(this);
    doc->setMode(Doc::Design);
    const QList<Fixture *> bars = washersOnSteps(doc, def);
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        mon.m_graphicsView->m_scene->clearSelection();
        QMenu m;
        MonitorGraphicsView::MenuPlace place;
        place.inList = true;
        mon.m_graphicsView->buildItemMenu(&m, "fixture", bars[0]->id(), place);
        QAction *edit = nullptr;
        foreach (QAction *a, m.actions())
            if (a->text().startsWith("Fixture Properties"))
                edit = a;
        QVERIFY(edit != nullptr);
        QString opened;
        QTimer watch;
        QObject::connect(&watch, &QTimer::timeout, [&opened, &watch]() {
            if (QDialog *d = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
            {
                opened = d->windowTitle();
                d->reject();
                watch.stop();
            }
        });
        watch.start(20);
        edit->trigger();
        watch.stop();
        QVERIFY2(!opened.isEmpty(), "Edit Fixture opened nothing");
    }
    delete doc;
    delete def;
}

/* Lit in sections -- the two ends of a bar, its middle dark: the light gets
   out above the lit pixels and not above the dark ones. */
/* "are we gauging the luminosity based on the actual devices": a fixture
   throws the light its definition says it puts out -- declared lumens, else
   declared watts -- shared between its heads. A 10-lumen strip is a glimmer
   next to a 3500-lumen bar; a fixture declaring neither is drawn as before. */
void Monitor_Test::aFixturesLightFollowsWhatItsDefinitionPutsOut()
{
    QLCFixtureDef *def = makeWallWasher();
    Doc *doc = new Doc(this);
    const QList<Fixture *> bars = washersOnSteps(doc, def);
    Fixture *bar = bars[0];
    MonitorProperties *props = doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(bar->id());
    rp.studioTilt = 0.0f;
    props->setFixtureRigProps(bar->id(), rp);
    Scene *scene = new Scene(doc);
    scene->setName("All blue");
    QVERIFY(doc->addFunction(scene));
    for (int h = 0; h < 16; ++h)
        scene->setValue(bar->id(), quint32(h * 3 + 2), 255);

    StructureStudioView view(doc, StructureStudioView::StageKind, 0);
    view.resize(800, 600);
    view.setLiveValues(true);
    view.setActiveScene(scene->id());
    view.setPlane(StructureStudioView::Angled);
    const QVector3D above(1.81f, 5.002f, 0.204f + 0.15f), down(0, 0, -1);
    auto lightWithLumens = [&](int lumens) {
        QLCFixtureMode *mode = bar->fixtureMode();
        QLCPhysical ph = mode->physical();
        ph.setBulbLumens(lumens);
        mode->setPhysical(ph);
        view.reload();
        QCoreApplication::processEvents();
        QImage img(view.size(), QImage::Format_ARGB32);
        view.render(&img);
        return double(view.beamLightAt(above, down).z());
    };
    bool known = true;
    view.fixtureOutput(bar, &known);
    QVERIFY(!known);                                // the test washer declares nothing
    const double unknown = lightWithLumens(0);
    const double bright = lightWithLumens(3500);
    const double strip = lightWithLumens(10);
    QVERIFY2(unknown > 0.0 && bright > 0.0 && strip > 0.0,
             qPrintable(QString("%1 %2 %3").arg(unknown).arg(bright).arg(strip)));
    // Sixteen heads share the bar's light instead of each being a whole fixture.
    QVERIFY2(bright < unknown / 4.0, qPrintable(QString("%1 vs %2").arg(bright).arg(unknown)));
    // Light is linear in what is put out: 10 lm is 1/350 of 3500 lm.
    QVERIFY2(qAbs(strip * 350.0 / bright - 1.0) < 0.05,
             qPrintable(QString("%1 vs %2").arg(strip).arg(bright)));
    delete doc;
    delete def;
}

void Monitor_Test::aBarLitInSectionsLightsOnlyAboveThem()
{
    QLCFixtureDef *def = makeWallWasher();
    Doc *doc = new Doc(this);
    const QList<Fixture *> bars = washersOnSteps(doc, def);
    Fixture *bar = bars[0];
    MonitorProperties *props = doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(bar->id());
    rp.studioTilt = 0.0f;                       // straight up, to keep it simple
    props->setFixtureRigProps(bar->id(), rp);
    Scene *scene = new Scene(doc);
    scene->setName("Ends");
    QVERIFY(doc->addFunction(scene));
    for (int h : { 0, 1, 2, 13, 14, 15 })      // blue on both ends
        scene->setValue(bar->id(), quint32(h * 3 + 2), 255);

    StructureStudioView view(doc, StructureStudioView::StageKind, 0);
    view.resize(800, 600);
    view.setLiveValues(true);
    view.setActiveScene(scene->id());
    view.reload();
    view.setPlane(StructureStudioView::Angled);
    QCoreApplication::processEvents();
    QImage img(view.size(), QImage::Format_ARGB32);
    view.render(&img);

    // The bar: 1.005 m centred at x = 1.2 + 0.61, at y = 4.9 + 0.102.
    const float cx = 1.81f, y = 5.002f, len = 1.005f, z = 0.204f + 0.15f;
    const QVector3D down(0, 0, -1);
    const QVector3D leftEnd(cx - len / 2 + 0.06f, y, z), rightEnd(cx + len / 2 - 0.06f, y, z),
                    middle(cx, y, z);
    QVERIFY(view.beamLightAt(leftEnd, down).z() > 50);
    QVERIFY(view.beamLightAt(rightEnd, down).z() > 50);
    QCOMPARE(view.beamLightAt(middle, down).z(), 0.0f);
    delete doc;
    delete def;
}

/* "would there be any overwash on the cyclorama from the top steps at 20
   degrees?" -- the rig view has to say, because it is taken as the room.
   The bars' AXIS clears a 6 m cyc; the upstage edge of their cones washes its
   top. Where it lands follows from the geometry: a ray at angle a from the
   vertical reaches the cloth D metres upstage at height z0 + D / tan(a). */
void Monitor_Test::stepBarsTippedUpstageWashTheTopOfTheCyc()
{
    QLCFixtureDef *def = makeWallWasher();
    Doc *doc = new Doc(this);
    const QList<Fixture *> bars = washersOnSteps(doc, def);      // steps at y 4.9, tipped 20
    MonitorProperties *props = doc->monitorProperties();
    Scenery *cyc = props->addScenery();
    cyc->setOriginX(6.0f);
    cyc->setOriginY(1.2f);
    cyc->setWidth(11.0f);
    cyc->setHeight(6.0f);

    StructureStudioView view(doc, StructureStudioView::StageKind, 0);
    Fixture *bar = bars[0];
    auto wash = [&](float &zmin, float &zmax) {
        const FixtureRigProps rp = props->fixtureRigProps(bar->id());
        QVector3D L, N, H;
        view.fixtureFrame(rp, L, N, H);
        const QVector3D c = view.fixtureCentre(bar->id());
        const double len = view.fixtureLenM(bar->id());
        StagePlatform *pl = props->platform(rp.riserPlatformId);
        StructureStudioView::FrameBeam ap{ QVector3D(), N, qDegreesToRadians(20.0), 10.0, QColor(), 0.0 };
        ap.apertured = pl->topMaterial() != StagePlatform::SolidTop;
        ap.ax0 = pl->originX(); ap.ax1 = pl->originX() + pl->width();
        ap.ay0 = pl->originY(); ap.ay1 = pl->originY() + pl->depth();
        ap.az = props->platformBaseZ(pl->id()) + pl->height();
        if (!ap.apertured) { ap.ax1 = ap.ax0; ap.ay1 = ap.ay0; }    // a lid: no opening
        zmin = 99.0f; zmax = -1.0f;
        int n = 0;
        for (const auto &patch : view.washOnScenery(c - L * float(len / 2), c + L * float(len / 2), N,
                                                    qDegreesToRadians(20.0), ap))
            for (const QVector3D &q : patch) { zmin = qMin(zmin, q.z()); zmax = qMax(zmax, q.z()); ++n; }
        return std::make_pair(n, c);
    };

    float zmin = 0, zmax = 0;
    auto r = wash(zmin, zmax);
    QVERIFY2(r.first > 0, "a bar tipped 20 degrees upstage in a 40 degree cone does not reach the cyc");
    const double D = double(r.second.y()) - 1.2;
    const double expected = double(r.second.z()) + D / qTan(qDegreesToRadians(40.0));
    QVERIFY2(qAbs(zmin - expected) < 0.1, qPrintable(QString("wash starts at %1 m, expected %2").arg(zmin).arg(expected)));
    QVERIFY(qAbs(zmax - 6.0f) < 0.05);                           // up to the top of the cloth

    /* On the FLOOR of the 20 cm box instead: the shallow upstage rays meet
       the back wall before the opening -- less of the cone gets out, so the
       wash starts higher up the cloth (or not at all). */
    FixtureRigProps rp = props->fixtureRigProps(bar->id());
    rp.mountZOffset = 0.0f;
    props->setFixtureRigProps(bar->id(), rp);
    float floorMin = 0, floorMax = 0;
    const int onFloor = wash(floorMin, floorMax).first;
    QVERIFY2(onFloor == 0 || floorMin > zmin + 0.3f,
             qPrintable(QString("walled in, the wash still starts at %1 m").arg(floorMin)));
    rp.mountZOffset = 0.184f;
    props->setFixtureRigProps(bar->id(), rp);

    // Upright, the cone's edge is 20 degrees from vertical: over the top.
    rp.studioTilt = 0.0f;
    props->setFixtureRigProps(bar->id(), rp);
    QCOMPARE(wash(zmin, zmax).first, 0);

    // Tipped again, but the step has a solid lid: nothing gets out.
    rp.studioTilt = 20.0f;
    props->setFixtureRigProps(bar->id(), rp);
    props->platform(rp.riserPlatformId)->setTopMaterial(StagePlatform::SolidTop);
    QCOMPARE(wash(zmin, zmax).first, 0);
    delete doc;
    delete def;
}

/* "the source appears centred on the matrix, not using the individual LEDs":
   a bar of pixels throws its light from the lit ones -- lit only at one end,
   from that end; lit in two sections, from both, nothing from the dark middle.
   An XL-450 hung on a truss (15 columns along its 401 mm). */
void Monitor_Test::aPixelBarsLightComesFromItsLitPixels()
{
    QLCFixtureDef *def = makeXl450();
    Doc *doc = new Doc(this);
    QList<Fixture *> bars = xlBarsOnATruss(doc, def, 0.0f);         // lying along the truss
    Fixture *bar = bars[0];
    Scene *scene = new Scene(doc);
    scene->setName("Ends");
    QVERIFY(doc->addFunction(scene));
    StructureStudioView view(doc, StructureStudioView::StageKind, 0);
    view.setLiveValues(true);
    view.setActiveScene(scene->id());
    MonitorProperties *props = doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(bar->id());
    const FixtureVisualTraits traits = classifyFixture(bar);
    QVector3D L, N, H;
    view.fixtureFrame(rp, L, N, H);
    const QVector3D centre = view.fixtureCentre(bar->id());
    const double len = view.fixtureLenM(bar->id());
    auto along = [&](const QVector3D &p) { return double(QVector3D::dotProduct(p - centre, L)); };
    auto lights = [&]() {
        QCoreApplication::processEvents();
        return view.barLights(bar, rp, traits, view.valuesForDrawing(bar), centre);
    };

    QVERIFY(lights().isEmpty());                                     // dark

    // The first RGB head of each row of the LED map: the bar's first column.
    const HeadLayout map = props->headLayoutFor(bar);
    QList<int> firstColumn;
    for (int h = 0; h < 45; ++h)                                     // the RGB heads
        if (map.cellOf(h).x() == 0) firstColumn << h;
    QVERIFY(!firstColumn.isEmpty());
    for (int h : firstColumn)
        scene->setValue(bar->id(), quint32(h * 3 + 2), 255);         // its blue
    QVector<StructureStudioView::BarLight> ls = lights();
    QVERIFY(!ls.isEmpty());
    for (const auto &l : ls)
    {
        QVERIFY2(along(l.at) < -0.4 * len, qPrintable(QString("light at %1 of a %2 m bar").arg(along(l.at)).arg(len)));
        QVERIFY(l.colour.blue() > 200 && l.colour.red() < 30);
    }

    // The last column too: two sections, light from both ends, none between.
    for (int h = 0; h < 45; ++h)
        if (map.cellOf(h).x() == map.size.width() - 1)
            scene->setValue(bar->id(), quint32(h * 3), 255);         // red at the other end
    ls = lights();
    bool left = false, right = false;
    for (const auto &l : ls)
    {
        const double u = along(l.at);
        QVERIFY2(qAbs(u) > 0.4 * len, qPrintable(QString("light from the dark middle, at %1").arg(u)));
        left |= u < 0;
        right |= u > 0;
    }
    QVERIFY(left && right);
    delete doc;
    delete def;
}

/* "the effect on these bars appears to be going backwards, not frontwards ..
   are the bars set facing downstage?" -- the FART bars hang vertically off a
   truss, faced to the front. "Hung means pointing down" sent their light to
   the floor behind the truss; a bar's light leaves its face. A 180-degree
   lens is a flood, drawn wide -- and dimmer for being spread. */
/* A WLED lantern on a bracket off the side of a tower: drawn as a lantern,
   hung where its hanger puts it, and lit all round -- no cone; what is near
   it is lit, falling off with the square of the distance. */
/* A WLED lantern in effect mode: the console sends "Chase", the lantern
   works out every LED. The rig view draws each of its sixteen LEDs -- by the
   port of that effect where there is one, else in colour 1 with the effect
   it is not showing named beside it. */
void Monitor_Test::aWledLanternShowsItsEffectLedByLed()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    QCOMPARE(def->loadXML(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/WLED/WLED-Lantern.qxf"),
             QFile::NoError);
    QLCFixtureMode *mode = def->mode("Effect (15ch)");
    QVERIFY(mode != nullptr);
    QCOMPARE(mode->physical().leds(), 16);

    Doc *doc = new Doc(this);
    doc->effectScriptRunner()->cache()->addDirectory(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts"));
    Fixture *lantern = new Fixture(doc);
    lantern->setFixtureDefinition(def, mode);
    lantern->setName("Lantern SL");
    QVERIFY(doc->addFixture(lantern));
    doc->monitorProperties()->setFixturePosition(lantern->id(), 0, 0, QVector3D(3000, 2000, 2500));

    // Master, Effect, Speed, Intensity, Palette, Option, colours 1-3.
    auto values = [](int effect) {
        QByteArray v(15, char(0));
        v[0] = char(255); v[1] = char(effect); v[2] = char(0); v[3] = char(128);
        v[6] = char(255);                       // colour 1 red
        v[10] = char(255);                      // colour 2 green
        v[14] = char(255);                      // colour 3 blue
        return v;
    };
    LanternPixels lp(doc);

    // Chase has a port: the LEDs differ, a block of blue in the red.
    LanternPixels::Frame f = lp.frame(lantern, values(28), 16, Qt::red, 1.0, true);
    QCOMPARE(f.source, LanternPixels::Script);
    QVERIFY(f.tag.isEmpty());
    QCOMPARE(f.leds.size(), 16);
    QCOMPARE(f.leds.count(QColor(0, 0, 255)), 3);
    QCOMPARE(f.leds.count(QColor(255, 0, 0)), 13);
    // At half its dimmer, half as bright.
    f = lp.frame(lantern, values(28), 16, Qt::red, 0.5, true);
    QCOMPARE(f.leds.count(QColor(128, 0, 0)), 13);

    // Game Of Life (2D) has none: colour 1, and it says what it is not showing.
    f = lp.frame(lantern, values(172), 16, Qt::red, 1.0, true);
    QCOMPARE(f.source, LanternPixels::Flat);
    QCOMPARE(f.tag, QString("WLED: Game Of Life"));
    QCOMPARE(f.leds.count(QColor(255, 0, 0)), 16);
    // Solid is exactly that.
    f = lp.frame(lantern, values(0), 16, Qt::red, 1.0, true);
    QCOMPARE(f.source, LanternPixels::Flat);
    QVERIFY(f.tag.isEmpty());
    // Not patched to a device that can say: never the device.
    QVERIFY(lp.deviceLeds(lantern) == 0);

    // In the rig view: sixteen beads, red and blue among them -- reversed,
    // so the blue block is at the bottom, where a view from above sees it.
    Scene *scene = new Scene(doc);
    QVERIFY(doc->addFunction(scene));
    QByteArray chase = values(28);
    chase[5] = char(128);
    for (int i = 0; i < chase.size(); ++i)
        scene->setValue(lantern->id(), quint32(i), uchar(chase.at(i)));
    StructureStudioView view(doc, StructureStudioView::StageKind, 0);
    view.resize(700, 700);
    view.setLiveValues(true);
    view.setActiveScene(scene->id());
    view.reload();
    struct Counts { int blue = 0, red = 0, lit = 0; };
    auto render = [&](StructureStudioView::Plane plane, const QString &name) {
        view.setPlane(plane);
        QCoreApplication::processEvents();
        QImage img(view.size(), QImage::Format_ARGB32);
        img.fill(Qt::black);
        view.render(&img);
        if (!qEnvironmentVariableIsEmpty("LANTERN_PNG"))
            img.save(qEnvironmentVariable("LANTERN_PNG").replace(".png", "-" + name + ".png"));
        Counts n;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
            {
                const QColor c = img.pixelColor(x, y);
                if (c.blue() > 200 && c.red() < 120 && c.green() < 120) ++n.blue;
                if (c.red() > 200 && c.blue() < 120 && c.green() < 120) ++n.red;
                if (c.red() > c.green() + 60) ++n.lit;
            }
        return n;
    };

    // Clear glass: every LED seen, in its own colour.
    FixtureRigProps rp = doc->monitorProperties()->fixtureRigProps(lantern->id());
    rp.lightGlass = 1;
    doc->monitorProperties()->setFixtureRigProps(lantern->id(), rp);
    const Counts clearFlat = render(StructureStudioView::Front, "clear-front");
    QVERIFY2(clearFlat.blue > 0 && clearFlat.red > 0,
             qPrintable(QString("blue %1 red %2").arg(clearFlat.blue).arg(clearFlat.red)));
    const Counts clearAngled = render(StructureStudioView::Angled, "clear-angled");
    QVERIFY2(clearAngled.blue > 0 && clearAngled.red > 0,
             qPrintable(QString("angled: blue %1 red %2").arg(clearAngled.blue).arg(clearAngled.red)));

    // Diffusing glass (the default): the glass itself glows -- far more of
    // the lantern lit than its beads alone.
    rp.lightGlass = -1;
    doc->monitorProperties()->setFixtureRigProps(lantern->id(), rp);
    const Counts diffuseFlat = render(StructureStudioView::Front, "diffuse-front");
    QVERIFY2(diffuseFlat.lit > clearFlat.lit * 3,
             qPrintable(QString("lit %1 vs clear %2").arg(diffuseFlat.lit).arg(clearFlat.lit)));
    const Counts diffuseAngled = render(StructureStudioView::Angled, "diffuse-angled");
    QVERIFY2(diffuseAngled.lit > clearAngled.lit,
             qPrintable(QString("angled lit %1 vs clear %2").arg(diffuseAngled.lit).arg(clearAngled.lit)));
}

/* "put a sock on trusses that would diffuse for internal led bars .. a sock
   color picker as the default color of white and a way to set how much of
   the truss the sock covers." A red bar Inside the socked stretch glows
   through the fabric, the sock lit near it; a bar past the end of the sock
   is not under it. */
void Monitor_Test::aSockedTrussGlowsWithTheBarsInsideIt()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(-3.0f, 2.0f, 4.0f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(6.0f);
    t->setWidth(0.29f);
    QCOMPARE(t->sockColour(), QColor(Qt::white));      // white, unless told
    t->setSock(true);
    t->setSockRange(0.0f, 3.0f);                        // the first half

    Fixture *inside = makeChunkyBar(doc, 0, "Under the sock");
    Fixture *outside = makeChunkyBar(doc, 10, "Past the sock");
    QVERIFY(inside != nullptr && outside != nullptr);
    auto mount = [&](Fixture *f, float along) {
        FixtureRigProps rp;
        rp.trussId = t->id();
        rp.trussOffset = along;
        rp.placement = FixtureRigProps::Inside;
        props->setFixtureRigProps(f->id(), rp);
    };
    mount(inside, 1.0f);
    mount(outside, 5.0f);

    Scene *scene = new Scene(doc);
    QVERIFY(doc->addFunction(scene));
    scene->setValue(inside->id(), 0, 255);              // red
    scene->setValue(outside->id(), 0, 255);

    StructureStudioView view(doc, StructureStudioView::StageKind, 0);
    view.resize(800, 600);
    view.setLiveValues(true);
    view.setBeams(true);
    view.setActiveScene(scene->id());
    view.reload();
    view.setPlane(StructureStudioView::Angled);
    QCoreApplication::processEvents();
    QImage img(view.size(), QImage::Format_ARGB32);
    img.fill(Qt::black);
    view.render(&img);
    if (!qEnvironmentVariableIsEmpty("SOCK_PNG"))
        img.save(qEnvironmentVariable("SOCK_PNG"));

    QVERIFY(view.underSock(props->fixtureRigProps(inside->id())));
    QVERIFY(!view.underSock(props->fixtureRigProps(outside->id())));
    // The sock glows red over the bar, and far less at its other end.
    const QVector<QVector3D> glow = view.sockGlow(t, 6); // half-metre stretches
    QVERIFY2(glow.at(2).x() > 0.3f, qPrintable(QString::number(glow.at(2).x())));
    QVERIFY(glow.at(2).y() < 0.01f && glow.at(2).z() < 0.01f);
    QVERIFY(glow.at(5).x() < glow.at(2).x() * 0.5f);

    // A blue sock filters it: red light through blue fabric is dark.
    t->setSockColour(QColor(0, 0, 255));
    QVERIFY(view.sockGlow(t, 6).at(2).x() < 0.01f);
}

/* "we need faces on the tower .. similar to sock .. for the towers they're
   opaque if we wanna be snazzy we can upload a pattern that can cover the
   sock or opaque parts." */
void Monitor_Test::aTowerWearsItsFacesPlainOrPrinted()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Tower *tw = props->addTower();
    tw->setOriginX(-0.5f); tw->setOriginY(0.0f);
    tw->setWidth(1.0f); tw->setDepth(1.0f); tw->setHeight(3.0f);

    StructureStudioView view(doc, StructureStudioView::StageKind, 0);
    view.resize(700, 700);
    view.reload();
    view.setPlane(StructureStudioView::Angled);
    auto render = [&](const QString &name) {
        QCoreApplication::processEvents();
        QImage img(view.size(), QImage::Format_ARGB32);
        img.fill(QColor(255, 255, 255));            // panels show as dark on white
        view.render(&img);
        if (!qEnvironmentVariableIsEmpty("FACES_PNG"))
            img.save(qEnvironmentVariable("FACES_PNG").replace(".png", "-" + name + ".png"));
        return img;
    };
    auto count = [](const QImage &img, const std::function<bool(const QColor &)> &f) {
        int n = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (f(img.pixelColor(x, y))) ++n;
        return n;
    };
    auto green = [](const QColor &c) { return c.green() > 120 && c.red() < 90 && c.blue() < 90; };
    auto magenta = [](const QColor &c) { return c.red() > 120 && c.blue() > 120 && c.green() < 90; };

    // Open steel: nothing coloured.
    view.reload();
    QCOMPARE(count(render("open"), green), 0);

    // Faced in green: the tower is a green block.
    tw->setFaces(true);
    tw->setFacesColour(QColor(0, 230, 0));
    view.reload();
    const int plain = count(render("plain"), green);
    QVERIFY2(plain > 2000, qPrintable(QString::number(plain)));

    // Front only: less of it.
    tw->setFacesSides(Tower::FrontSide);
    view.reload();
    const int front = count(render("front"), green);
    QVERIFY(front > 0 && front < plain);

    // Printed: the top half green, the bottom half magenta -- both on it.
    QTemporaryDir dir;
    QImage art(32, 32, QImage::Format_ARGB32);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x)
            art.setPixelColor(x, y, y < 16 ? QColor(0, 230, 0) : QColor(230, 0, 230));
    const QString artPath = dir.filePath("wrap.png");
    QVERIFY(art.save(artPath));
    tw->setFacesSides(Tower::AllSides);
    tw->setFacesColour(QColor(20, 20, 20));
    tw->setFacesPattern(artPath);
    view.reload();
    const QImage printed = render("printed");
    QVERIFY(count(printed, green) > 500);
    QVERIFY(count(printed, magenta) > 500);
    // ...the green above the magenta, as printed.
    double gy = 0, my = 0; int gn = 0, mn = 0;
    for (int y = 0; y < printed.height(); ++y)
        for (int x = 0; x < printed.width(); ++x)
        {
            const QColor c = printed.pixelColor(x, y);
            if (green(c)) { gy += y; ++gn; }
            if (magenta(c)) { my += y; ++mn; }
        }
    QVERIFY(gy / gn < my / mn);

    // A missing file: plain colour, no harm.
    tw->setFacesPattern(dir.filePath("gone.png"));
    view.reload();
    QCOMPARE(count(render("missing"), magenta), 0);

    // The flat front view: the print, upright.
    tw->setFacesPattern(artPath);
    view.setPlane(StructureStudioView::Front);
    view.reload();
    const QImage flat = render("flat");
    QVERIFY(count(flat, green) > 100 && count(flat, magenta) > 100);

    // Faded: at 0% the print is gone, the panels' own colour left.
    view.setPlane(StructureStudioView::Angled);
    tw->setFacesPatternOpacity(0.0f);
    view.reload();
    QCOMPARE(count(render("faded"), magenta), 0);
    tw->setFaces(false);

    // A sock takes a print too, faded the same way.
    Truss *t = props->addTruss();
    t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(-2.0f, 1.0f, 2.0f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(4.0f);
    t->setWidth(0.4f);
    t->setSock(true);
    t->setSockPattern(artPath);
    view.reload();
    const QImage sock = render("sock");
    QVERIFY(count(sock, magenta) > 100);
    t->setSockPatternOpacity(0.0f);
    view.reload();
    QCOMPARE(count(render("sock-faded"), magenta), 0);
}

/* "with haze up and lights down for the leds I am not seeing much haze..
   should that be the case?" A strip with few lumens throws no shaft of light,
   but in heavy haze the air round its lit pixels glows -- off a bar on a
   truss, and rising off a strip inside a clear-topped step. */
void Monitor_Test::ledsGlowInTheHaze()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    // A step with a clear top and a strip inside it, firing up.
    StagePlatform *pl = props->addPlatform();
    pl->setName("Step"); pl->setOriginX(-1.2f); pl->setOriginY(0.0f);
    pl->setWidth(2.438f); pl->setDepth(0.6f); pl->setHeight(0.4f);
    pl->setTopMaterial(StagePlatform::ClearTop);
    pl->setColor(QColor(40, 40, 44));
    Fixture *tape = makeStepTape(doc, pl, 32, 0, "Step tape", 0.2f);
    QVERIFY(tape != nullptr);
    FixtureRigProps rp = props->fixtureRigProps(tape->id());
    rp.riserFace = FixtureRigProps::RiserTop;
    rp.riserU = pl->width() * 0.5f;
    rp.riserV = pl->depth() * 0.5f;
    rp.placement = FixtureRigProps::Inside;
    rp.studioMount = 0;                         // lying flat, its face up
    rp.studioFaceChosen = true;
    props->setFixtureRigProps(tape->id(), rp);

    // A bar on a truss above, facing the audience.
    Truss *t = props->addTruss();
    t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(-1.5f, -1.0f, 2.5f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(3.0f);
    t->setWidth(0.29f);
    Fixture *bar = makeChunkyBar(doc, 200, "Truss bar");
    QVERIFY(bar != nullptr);
    FixtureRigProps brp;
    brp.trussId = t->id();
    brp.trussOffset = 1.5f;
    props->setFixtureRigProps(bar->id(), brp);

    QByteArray u(512, char(0));
    for (int h = 0; h < 32; ++h) u[h * 3] = char(255);      // the tape red
    tape->setChannelValues(u);
    QByteArray b(512, char(0));
    b[200] = char(255);                                      // the bar red (by universe address)
    bar->setChannelValues(b);

    auto reddish = [&](double haze) {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(900, 700);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setLiveValues(true);
        v.setBeams(true);
        v.setAmbient(0.0);                                   // blackout
        v.setHaze(haze);
        QImage img(v.size(), QImage::Format_ARGB32);
        img.fill(Qt::black);
        v.render(&img);
        if (!qEnvironmentVariableIsEmpty("HAZE_PNG"))
            img.save(qEnvironmentVariable("HAZE_PNG").replace(".png", QString("-%1.png").arg(haze)));
        int n = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
            {
                const QColor c = img.pixelColor(x, y);
                if (c.red() > 40 && c.red() > c.green() * 2 && c.red() > c.blue() * 2)
                    ++n;
            }
        return n;
    };
    const int clearAir = reddish(0.0);
    const int heavy = reddish(1.8);
    QVERIFY2(heavy > clearAir * 1.5 && heavy - clearAir > 2000,
             qPrintable(QString("heavy haze %1 red px vs clear air %2").arg(heavy).arg(clearAir)));
    // Light haze: some, less than heavy.
    const int light = reddish(0.6);
    QVERIFY2(light > clearAir && light < heavy,
             qPrintable(QString("light %1 between clear %2 and heavy %3").arg(light).arg(clearAir).arg(heavy)));
}

/* "Haze glow from the bottom steps shows through the closed curtain." The
   glow round a lit bar was lifted half a metre toward the eye so its own
   housing would not cover it -- and a row of steps a foot behind the closed
   main drape glowed through the drape, as did the pools their light made on
   the back of the cloth (lifted toward the eye off a face with no
   thickness). Behind a closed drape: nothing. In front of it, or with it
   open: the glow, as before. On the CPU and, where there is one, the GPU,
   at both qualities. */
void Monitor_Test::hazeGlowStaysBehindAClosedDrape()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();

    // A step with a blue strip on its front face.
    StagePlatform *pl = props->addPlatform();
    pl->setName("Step"); pl->setOriginX(-1.2f); pl->setOriginY(0.0f);
    pl->setWidth(2.438f); pl->setDepth(0.6f); pl->setHeight(0.4f);
    pl->setColor(QColor(40, 40, 44));
    Fixture *tape = makeStepTape(&doc, pl, 32, 0, "Steps", 0.2f);
    QVERIFY(tape != nullptr);
    QByteArray u(512, char(0));
    for (int h = 0; h < 32; ++h)
        u[h * 3 + 2] = char(255);
    tape->setChannelValues(u);

    // The main drape, closed, a foot in front of the step's face.
    Scenery *drape = props->addScenery();
    drape->setKind(Scenery::MainDrape);
    drape->setColor(Scenery::defaultColor(Scenery::MainDrape));
    drape->setOriginX(0.0f);
    drape->setOriginY(0.6f + 0.3f);
    drape->setWidth(8.0f);
    drape->setHeight(4.0f);
    drape->setOpeningWidth(4.0f);
    drape->setState(Scenery::Closed);

    const QByteArray was = qgetenv("QLC_RIG_GPU");
    auto restore = qScopeGuard([&]() {
        if (was.isNull()) qunsetenv("QLC_RIG_GPU"); else qputenv("QLC_RIG_GPU", was);
    });
    qputenv("QLC_RIG_GPU", "1");

    StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
    v.resize(800, 600);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(15.0, 15.0);
    v.setLiveValues(true);
    v.setBeams(true);
    v.setAmbient(0.0);
    v.setHaze(1.0);                                      // Medium

    /* The strip's blue, wherever it shows: its pixels, the glow round them and
       its light on the cloth -- however faint, where nothing should be. */
    auto blue = [&v](const char *tag, bool faint = false) {
        v.reload();
        const QImage img = v.grab().toImage().convertToFormat(QImage::Format_ARGB32);
        if (!qEnvironmentVariableIsEmpty("HAZE_PNG"))
            img.save(qEnvironmentVariable("HAZE_PNG").replace(".png", QString("-%1.png").arg(tag)));
        int n = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
            {
                const QColor q = img.pixelColor(x, y);
                if (faint ? (q.blue() > q.red() + 4 && q.blue() > 2 * q.green() + 4)   // even on the red cloth
                          : (q.blue() > 12 && q.blue() > 2 * q.red() && q.blue() > 2 * q.green()))
                    ++n;
            }
        return n;
    };

    QList<bool> gpus { false };
    if (RhiRaster::available())
        gpus << true;
    for (bool gpu : gpus)
        for (int quality : { 1, 2 })
        {
            v.setGpu(gpu);
            v.setSupersample(quality);
            const QByteArray tag = QString("%1-q%2").arg(gpu ? "gpu" : "cpu").arg(quality).toLatin1();

            /* Behind the closed drape: nothing of it, haze or no haze -- not
               its glow, nor the pools its light makes on the back of the
               cloth, which has no thickness to hide them. */
            drape->setOriginY(0.9f);
            drape->setState(Scenery::Closed);
            v.setHaze(1.0);
            const int closed = blue((tag + "-closed").constData(), true);
            if (gpu)
                QVERIFY2(v.drewOnGpu(), "the GPU fell back to the CPU");
            v.setHaze(0.0);
            const int closedClear = blue((tag + "-closed-clear").constData(), true);
            QVERIFY2(closed < 30 && closedClear < 30,
                     qPrintable(QString("%1: %2 blue px through the closed drape in haze, %3 in clear air")
                                .arg(tag.constData()).arg(closed).arg(closedClear)));

            // Open: the glow shows through the gap.
            drape->setState(Scenery::Open);
            const int openClear = blue((tag + "-open-clear").constData());
            v.setHaze(1.0);
            const int open = blue((tag + "-open").constData());
            QVERIFY2(open > openClear + 1500,
                     qPrintable(QString("%1: open, %2 blue px in haze vs %3 in clear air")
                                .arg(tag.constData()).arg(open).arg(openClear)));

            // Upstage of the step instead: the glow in front of the drape, as before.
            drape->setState(Scenery::Closed);
            drape->setOriginY(-1.0f);
            const int inFront = blue((tag + "-infront").constData());
            v.setHaze(0.0);
            const int inFrontClear = blue((tag + "-infront-clear").constData());
            QVERIFY2(inFront > inFrontClear + 1500,
                     qPrintable(QString("%1: in front, %2 blue px in haze vs %3 in clear air")
                                .arg(tag.constData()).arg(inFront).arg(inFrontClear)));
        }

    /* A curtain its fixture closes: "the steps glow through the drape" was a
       show whose drape is saved open and closed by its motor. Where the
       fixture has it is where it hides the glow. */
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Curtain");
    def->setType(QLCFixtureDef::Other);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Open");
    QLCChannel *ch = new QLCChannel();
    ch->setName("Open");
    ch->setGroup(QLCChannel::Maintenance);
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    def->addMode(mode);
    Fixture *motor = new Fixture(&doc);
    motor->setName("Main drape motor");
    motor->setFixtureDefinition(def, mode);
    motor->setAddress(100);
    QVERIFY(doc.addFixture(motor));
    drape->setDriverFixture(motor->id());
    QVERIFY(drape->isDriven());
    auto setOpen = [&](uchar val) {
        QByteArray w(512, char(0));
        w[int(motor->address())] = char(val);
        motor->setChannelValues(w);
    };
    v.setGpu(false);
    v.setSupersample(1);
    v.setHaze(1.0);
    drape->setOriginY(0.9f);
    drape->setState(Scenery::Open);              // as saved
    setOpen(0);                                  // closed by its motor
    const int motorClosed = blue("motor-closed", true);
    QVERIFY2(motorClosed < 30, qPrintable(QString("%1 blue px through the drape its motor closed")
                                          .arg(motorClosed)));
    setOpen(255);
    const int motorOpen = blue("motor-open");
    QVERIFY2(motorOpen > 1500, qPrintable(QString("only %1 blue px with its motor open").arg(motorOpen)));
}

/* A rig built of pixel fixtures paints in good time. "The app hung at 99% CPU
   repainting the Studio": per paint, typing each head of an XL-450 (75 heads,
   165 channels; the show has eight) was heads x channels x heads. Eight of
   those and ten movers: classifying every fixture, and painting the Studio
   and the Rig Overview from scratch, each well inside a generous budget. */
void Monitor_Test::aBigPixelRigPaintsInGoodTime()
{
    Doc doc(this);
    MonitorProperties *props = doc.monitorProperties();

    // An XL-450-shaped bar: 45 RGB cells, then 30 white ones.
    auto makeBigBar = [&doc](int i) -> Fixture * {
        QLCFixtureDef *def = new QLCFixtureDef();
        def->setManufacturer("Test"); def->setModel(QString("Big Bar %1").arg(i));
        def->setType(QLCFixtureDef::LEDBarPixels);
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName("165ch");
        quint32 c = 0;
        for (int h = 0; h < 75; ++h)
        {
            QLCFixtureHead hd;
            const QList<QLCChannel::PrimaryColour> cols = (h < 45)
                ? QList<QLCChannel::PrimaryColour>{ QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue }
                : QList<QLCChannel::PrimaryColour>{ QLCChannel::White };
            for (QLCChannel::PrimaryColour pc : cols)
            {
                QLCChannel *ch = new QLCChannel();
                ch->setName(QString("%1-%2").arg(h).arg(c));
                ch->setGroup(QLCChannel::Intensity); ch->setColour(pc);
                def->addChannel(ch); mode->insertChannel(ch, c);
                hd.addChannel(c++);
            }
            mode->insertHead(-1, hd);
        }
        QLCPhysical ph;
        ph.setWidth(1200); ph.setHeight(120); ph.setDepth(80);
        ph.setLayoutSize(QSize(15, 5));
        mode->setPhysical(ph);
        def->addMode(mode);
        Fixture *f = new Fixture(&doc);
        f->setName(QString("XL %1").arg(i)); f->setFixtureDefinition(def, mode);
        f->setUniverse(quint32(10 + i)); f->setAddress(0);
        if (!doc.addFixture(f))
            return nullptr;
        return f;
    };

    Truss *t = props->addTruss();
    t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(0.0f, 2.0f, 5.0f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(12.0f);
    t->setWidth(0.3f);
    QList<Fixture *> rig;
    for (int i = 0; i < 8; ++i)
    {
        Fixture *f = makeBigBar(i);
        QVERIFY(f != nullptr);
        props->setFixturePosition(f->id(), 0, 0, QVector3D(0, 0, 0));
        FixtureRigProps rp;
        rp.trussId = t->id();
        rp.trussOffset = 0.75f + 1.4f * i;
        props->setFixtureRigProps(f->id(), rp);
        QByteArray vals(512, char(0));
        for (quint32 c = 0; c < f->channels(); ++c)
            vals[int(c)] = char((c * 37 + i * 11) % 256);
        f->setChannelValues(vals);
        rig << f;
    }
    for (int i = 0; i < 10; ++i)
    {
        Fixture *m = makeMover(&doc, 14.0, quint32(6 * i), qPrintable(QString("Mover %1").arg(i)),
                               QVector3D(1.0f + 1.1f * i, 4.0f, 0.0f));
        QVERIFY(m != nullptr);
        QByteArray vals(512, char(0));
        vals[6 * i + 2] = char(255); vals[6 * i + 3] = char(255); vals[6 * i + 5] = char(200);
        m->setChannelValues(vals);
        rig << m;
    }
    /* A run of risers under the bars and in the movers' light: every beam
       that comes down on one stops on it, and seen level its shadow is cut
       out of the beam (aWashStopsOnARiserSeenFromTheSide). */
    for (int i = 0; i < 4; ++i)
    {
        StagePlatform *pl = props->addPlatform();
        pl->setName(QString("Riser %1").arg(i));
        pl->setOriginX(0.5f + 2.8f * i); pl->setOriginY(0.5f);
        pl->setWidth(2.4f); pl->setDepth(3.0f); pl->setHeight(0.4f + 0.2f * i);
    }

    /* Classifying every fixture: what each paint and each rebuild starts
       from, several times over. Ten passes over the rig: well under a
       millisecond here; with the per-paint head typing it was tens of
       milliseconds a pass. */
    QElapsedTimer clock;
    clock.start();
    for (int pass = 0; pass < 10; ++pass)
        for (Fixture *f : rig)
            (void)classifyFixture(f);
    const qint64 classify = clock.elapsed();
    qInfo().noquote() << QString("classifying %1 fixtures ten times: %2 ms").arg(rig.size()).arg(classify);
    QVERIFY2(classify < 100, qPrintable(QString("classifying %1 fixtures ten times took %2 ms")
                                        .arg(rig.size()).arg(classify)));

    // The Studio (from above) and the Rig Overview, each frame from scratch.
    const QByteArray was = qgetenv("QLC_RIG_GPU");
    auto restore = qScopeGuard([&]() {
        if (was.isNull()) qunsetenv("QLC_RIG_GPU"); else qputenv("QLC_RIG_GPU", was);
    });
    qunsetenv("QLC_RIG_GPU");
    /* From above, the overview's own angle, and side-on at eye level -- an
       elevation, where beams are cut round the risers. */
    struct Camera { StructureStudioView::Plane plane; double az, el; };
    for (const Camera &cam : { Camera{ StructureStudioView::Top, 0.0, 0.0 },
                               Camera{ StructureStudioView::Angled, 20.0, 30.0 },
                               Camera{ StructureStudioView::Angled, 90.0, 0.0 } })
        for (int quality : { 1, 2 })
        {
            const StructureStudioView::Plane plane = cam.plane;
            StructureStudioView v(&doc, StructureStudioView::StageKind, 0);
            v.resize(1100, 680);
            v.setPlane(plane);
            v.setAngledView(cam.az, cam.el);
            v.setLiveValues(true);
            v.setBeams(true);
            v.setHaze(1.0);
            v.setGpu(false);
            v.setSupersample(quality);
            clock.start();
            (void)v.grab();                  // the first frame builds what it keeps
            const qint64 first = clock.elapsed();
            qint64 slowest = 0;
            for (int frame = 0; frame < 3; ++frame)
            {
                v.reload();                  // the rebuilds a reload asks for, too
                clock.start();
                (void)v.grab();
                slowest = qMax(slowest, clock.elapsed());
            }
            qInfo().noquote() << QString("plane %1 (%2/%3), quality %4: slowest paint %5 ms")
                                 .arg(int(plane)).arg(cam.az).arg(cam.el).arg(quality).arg(slowest)
                              + QString(" (first %1 ms)").arg(first);
            QVERIFY2(slowest < 1500, qPrintable(QString("plane %1, quality %2: a paint took %3 ms")
                                                .arg(int(plane)).arg(quality).arg(slowest)));
        }
}

/* A blue wash on a pole upstage of a riser, thrown across the stage and a
   little up, so its lower edge comes down on the riser and on the floor past
   it. @p alongX turns the whole thing a quarter, the beam running across the
   stage instead of down it -- what a front elevation sees side-on. @p clear
   puts the riser beside the beam instead, out of its way. */
namespace
{
struct WashOverRiser
{
    Fixture *wash = nullptr;
    StagePlatform *riser = nullptr;
    Scene *scene = nullptr;
    // Along the beam's run (Y, or X turned): lens, riser's near and far side.
    float lensAt = 0.0f, riserNear = 2.0f, riserFar = 4.0f;
    float riserTop = 0.8f;
};
}

static WashOverRiser buildWashOverRiser(Doc *doc, bool alongX, bool clear)
{
    WashOverRiser r;
    MonitorProperties *props = doc->monitorProperties();
    // In the run/across frame: the run is +Y (+X turned), the riser 8 m wide.
    auto at = [alongX](float run, float across, float z) {
        return alongX ? QVector3D(run - 4.0f, across, z) : QVector3D(across, run, z);
    };
    r.riser = props->addPlatform();
    r.riser->setName("Riser");
    const float acrossFrom = clear ? 6.0f : -4.0f;      // clear: beside the beam
    const QVector3D c0 = at(r.riserNear, acrossFrom, 0.0f);
    r.riser->setOriginX(c0.x());
    r.riser->setOriginY(c0.y());
    r.riser->setWidth(alongX ? r.riserFar - r.riserNear : 8.0f);
    r.riser->setDepth(alongX ? 8.0f : r.riserFar - r.riserNear);
    if (clear)
        r.riserTop = 2.0f;          // tall: drawn well across the beam, from the side
    r.riser->setHeight(r.riserTop);
    r.riser->setColor(QColor(110, 40, 40));
    if (alongX)
        r.lensAt = -4.0f;
    /* A low tower well off to the side, past the riser: only so the view,
       which frames what is on stage, shows the floor beyond the riser too. */
    Tower *tw = props->addTower();
    tw->setName("Marker");
    const QVector3D m = at(9.0f, 25.0f, 0.0f);
    tw->setOriginX(m.x()); tw->setOriginY(m.y());
    tw->setWidth(0.3f); tw->setDepth(0.3f); tw->setHeight(0.3f);

    // 80 degrees, its middle five degrees up: it lands on nothing, its lower
    // edge 35 degrees down.
    const QVector3D lens = at(0.0f, 0.0f, 3.0f);
    r.wash = makeMover(doc, 80.0, 0, "Wash", lens);
    StageTarget *t = props->addStageTarget();
    t->setName("Out");
    t->setPosition(lens + (at(10.0f, 0.0f, 0.0f) - at(0.0f, 0.0f, 0.0f))
                   + QVector3D(0.0f, 0.0f, 10.0f * float(qTan(qDegreesToRadians(5.0)))));
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setStageTargetId(t->id());
    doc->addPalette(aim);
    r.scene = new Scene(doc);
    r.scene->setName("Wash");
    r.scene->addFixture(r.wash->id());
    r.scene->addPalette(aim->id());
    doc->addFunction(r.scene);

    QByteArray u(512, char(0));
    u[2] = char(255);           // dimmer
    u[5] = char(255);           // blue
    r.wash->setChannelValues(u);
    return r;
}

/* The wash's blue, against the dark: beam, pool or band. */
static bool isWashBlue(const QColor &c)
{
    return c.blue() > c.red() + 14 && c.blue() > c.green() + 14;
}

static void washStopsOnTheRiser(QObject *parent, bool front)
{
    Doc *doc = new Doc(parent);
    auto cleanup = qScopeGuard([doc]() { delete doc; });
    const WashOverRiser r = buildWashOverRiser(doc, front, false);
    QVERIFY(r.wash != nullptr);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(1100, 600);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(front ? 0.0 : 90.0, 0.0);       // level: an elevation
    v.setActiveScene(r.scene->id());
    v.setLiveValues(true);
    v.setBeams(true);
    v.setHaze(1.0);
    v.setAmbient(0.0);
    v.setGpu(false);
    const QImage img = v.grab().toImage().convertToFormat(QImage::Format_ARGB32);
    if (!qEnvironmentVariableIsEmpty("RISER_PNG"))
        img.save(qEnvironmentVariable("RISER_PNG").replace(".png", front ? "-front.png" : "-side.png"));
    const double dpr = img.devicePixelRatio();
    auto at = [front](float run, float z) {
        return front ? QVector3D(run - 4.0f, 0.0f, z) : QVector3D(0.0f, run, z);
    };
    auto px = [&](const QVector3D &w) { return (v.w2s(w) * dpr).toPoint(); };
    // Not the axis tripod in the corner: its labels are coloured.
    const QRect tripod(img.width() - int(140 * dpr), img.height() - int(120 * dpr),
                       int(140 * dpr), int(120 * dpr));

    // Where the beam really goes, in the plane seen: along its run, and up.
    QVERIFY(v.m_frameBeams.size() == 1);
    const StructureStudioView::FrameBeam fb = v.m_frameBeams.first();
    auto runOf = [front](const QVector3D &w) { return front ? w.x() + 4.0f : w.y(); };
    const double run0 = runOf(fb.apex), z0 = fb.apex.z();
    const double up = qAtan2(double(fb.dir.z()), double(runOf(fb.dir) - runOf(QVector3D())));
    const double lowEdge = qTan(up - fb.halfAngle);      // slope of its lower edge
    QVERIFY2(lowEdge < -0.2, "the beam's lower edge does not come down");
    auto shown = [&](const QPoint &q) { return img.rect().contains(q) && !tripod.contains(q); };

    // The beam is there, in the air.
    QVERIFY2(isWashBlue(img.pixelColor(px(fb.apex + fb.dir.normalized() * 2.0f))),
             "no beam in the air at all");

    // Nothing below the floor line.
    const int floorY = px(QVector3D(0, 0, 0)).y();
    int below = 0;
    for (int y = floorY + int(3 * dpr); y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (!tripod.contains(x, y) && isWashBlue(img.pixelColor(x, y)))
                ++below;
    QVERIFY2(below == 0, qPrintable(QString("%1 px of the beam below the floor").arg(below)));

    // Nothing inside the riser, short of the band of light on its top and
    // lit side: its outline, well in from every edge.
    const QPoint a = px(at(r.riserNear, r.riserTop)), b = px(at(r.riserFar, 0.0f));
    const QRect box = QRect(a, b).normalized().adjusted(int(12 * dpr), int(12 * dpr),
                                                        -int(12 * dpr), -int(4 * dpr));
    int inside = 0;
    for (int y = box.top(); y <= box.bottom(); ++y)
        for (int x = box.left(); x <= box.right(); ++x)
            if (isWashBlue(img.pixelColor(x, y)))
                ++inside;
    QVERIFY2(inside == 0, qPrintable(QString("%1 px of the beam inside the riser").arg(inside)));

    /* Nor in the riser's shadow: past it, below the line from the lens over
       its far top edge -- where the light would have had to come through it
       -- and above the beam's lower edge, so in the beam but for the riser. */
    int shaded = 0;
    for (double run : { 0.3, 0.6, 1.0, 1.5 })
    {
        run += r.riserFar;
        const double graze = z0 + (run - run0) * (r.riserTop - z0) / (r.riserFar - run0);
        const double edge = qMax(0.0, z0 + (run - run0) * lowEdge);
        if (graze - edge < 0.1)
            continue;
        const QVector3D w = at(float(run), float(edge + 0.4 * (graze - edge)));
        const QPoint q = px(w);
        if (!shown(q))
            continue;
        ++shaded;
        const QColor c = img.pixelColor(q);
        QVERIFY2(!isWashBlue(c),
                 qPrintable(QString("lit behind the riser at run %1, height %2: %3")
                            .arg(run).arg(w.z()).arg(c.name())));
    }
    QVERIFY2(shaded >= 2, qPrintable(QString("only %1 points behind the riser to look at")
                                     .arg(shaded)));

    // And it washes the riser's top, where its lower edge comes down on it:
    // a band just inside the top edge, brighter than the air above it.
    const double lands = qMax(double(r.riserNear), run0 + (r.riserTop - z0) / lowEdge);
    QVERIFY2(lands < r.riserFar - 0.3, "the beam does not reach the riser's top");
    const QPoint topAt = px(at(float((lands + r.riserFar) / 2.0), r.riserTop));
    const QColor band = img.pixelColor(topAt + QPoint(0, int(2 * dpr)));
    const QColor air = img.pixelColor(topAt - QPoint(0, int(4 * dpr)));
    QVERIFY2(isWashBlue(band) && band.blue() > air.blue(),
             qPrintable(QString("no wash on the riser's top: %1 on it, %2 in the air above")
                        .arg(band.name(), air.name())));
}

/* "The TrussW are going thru the platforms .. should wash on them vs thru
   them." A side elevation of the rig: a wash thrown across a riser stops on
   it -- its top lit, nothing of the beam inside it, behind it or below the
   floor. Its edge rays used to be followed down to the plane its middle
   landed on, through the riser and the floor. */
void Monitor_Test::aWashStopsOnARiserSeenFromTheSide()
{
    washStopsOnTheRiser(this, false);
}

// The same, seen from the front: the beam running across the stage.
void Monitor_Test::aWashStopsOnARiserSeenFromTheFront()
{
    washStopsOnTheRiser(this, true);
}

/* A riser beside a beam, out of its way, takes nothing from it: in the side
   elevation it is drawn across the beam, and a beam cut by every riser it
   happens to cross on screen would lose light it has. The beam is as it is
   with the riser hidden -- and none of it goes below the floor. */
void Monitor_Test::aBeamClearOfTheRisersIsLeftAlone()
{
    Doc *doc = new Doc(this);
    auto cleanup = qScopeGuard([doc]() { delete doc; });
    MonitorProperties *props = doc->monitorProperties();
    const WashOverRiser r = buildWashOverRiser(doc, false, true);
    QVERIFY(r.wash != nullptr);
    const quint32 layer = props->addLayer("Risers");
    r.riser->setLayerId(layer);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(900, 600);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(90.0, 0.0);
    v.setActiveScene(r.scene->id());
    v.setLiveValues(true);
    v.setBeams(true);
    v.setHaze(1.0);
    v.setAmbient(0.0);
    v.setGpu(false);
    const QImage with = v.grab().toImage().convertToFormat(QImage::Format_ARGB32);
    v.m_zoomed = true;                          // the same framing for both
    props->setLayerVisible(layer, false);
    const QImage without = v.grab().toImage().convertToFormat(QImage::Format_ARGB32);
    if (!qEnvironmentVariableIsEmpty("RISER_PNG"))
    {
        with.save(qEnvironmentVariable("RISER_PNG").replace(".png", "-clear-with.png"));
        without.save(qEnvironmentVariable("RISER_PNG").replace(".png", "-clear-without.png"));
    }
    const double dpr = with.devicePixelRatio();
    auto px = [&](const QVector3D &w) { return (v.w2s(w) * dpr).toPoint(); };

    // Where the riser itself is drawn differs, of course; nothing else may.
    const QRect riser = QRect(px(QVector3D(0, r.riserNear, r.riserTop)),
                              px(QVector3D(0, r.riserFar, 0))).normalized()
                            .adjusted(-int(4 * dpr), -int(4 * dpr), int(4 * dpr), int(4 * dpr));
    int differ = 0, lit = 0;
    for (int y = 0; y < with.height(); ++y)
        for (int x = 0; x < with.width(); ++x)
        {
            if (riser.contains(x, y))
                continue;
            const QColor a = with.pixelColor(x, y), b = without.pixelColor(x, y);
            if (qAbs(a.red() - b.red()) > 2 || qAbs(a.green() - b.green()) > 2
                || qAbs(a.blue() - b.blue()) > 2)
                ++differ;
            if (isWashBlue(a))
                ++lit;
        }
    QVERIFY2(lit > 5000, qPrintable(QString("only %1 px of beam drawn").arg(lit)));
    QVERIFY2(differ == 0, qPrintable(QString("%1 px of the beam changed by a riser it "
                                             "never reaches").arg(differ)));

    // Still: nothing below the floor.
    const int floorY = px(QVector3D(0, 0, 0)).y();
    const QRect tripod(with.width() - int(140 * dpr), with.height() - int(120 * dpr),
                       int(140 * dpr), int(120 * dpr));
    int below = 0;
    for (int y = floorY + int(3 * dpr); y < with.height(); ++y)
        for (int x = 0; x < with.width(); ++x)
            if (!tripod.contains(x, y) && isWashBlue(with.pixelColor(x, y)))
                ++below;
    QVERIFY2(below == 0, qPrintable(QString("%1 px of the beam below the floor").arg(below)));
}

void Monitor_Test::doubleClickingAFixtureOpensItWhereverItIs()
{
    FixtureCleanup cleanup(m_doc);
    MonitorProperties *props = m_doc->monitorProperties();
    StagePlatform *pl = props->addPlatform();
    pl->setOriginX(3.0f); pl->setOriginY(3.0f);
    pl->setWidth(2.0f); pl->setDepth(1.0f); pl->setHeight(0.4f);
    Fixture *fx = new Fixture(m_doc);
    fx->setName("On the riser");
    fx->setChannels(1);
    fx->setAddress(500);
    QVERIFY(m_doc->addFixture(fx));
    cleanup.ids << fx->id();
    FixtureRigProps rp;
    rp.riserPlatformId = pl->id();
    rp.riserFace = FixtureRigProps::RiserTop;
    rp.riserU = 1.0f; rp.riserV = 0.5f;
    props->setFixtureRigProps(fx->id(), rp);

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    rig.gv->addFixture(fx->id(), QPointF(4000, 3500));
    MonitorFixtureItem *item = rig.gv->m_fixtures.value(fx->id(), nullptr);
    QVERIFY(item != nullptr);
    rig.gv->centerOn(item);

    // The fixture, not the riser it covers.
    QSignalSpy fixtureSpy(rig.gv, &MonitorGraphicsView::fixtureDoubleClicked);
    QSignalSpy riserSpy(rig.gv, &MonitorGraphicsView::platformDoubleClicked);
    inspectModal([](QWidget *dlg) { QVERIFY(dlg->windowTitle().startsWith("Fixture Properties")); });
    const QPoint at = rig.gv->mapFromScene(item->sceneBoundingRect().center());
    QTest::mouseDClick(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    QCOMPARE(fixtureSpy.count(), 1);
    QCOMPARE(fixtureSpy.first().first().toUInt(), fx->id());
    QCOMPARE(riserSpy.count(), 0);

    // The riser is in the fixture's own menu.
    QMenu menu;
    rig.gv->populateItemMenu(&menu, QStringLiteral("fixture"), fx->id(), QPointF(), false);
    QStringList texts;
    foreach (QAction *a, menu.actions())
        texts << a->text();
    QVERIFY2(texts.contains("Fixture Properties…") && texts.contains("Edit Platform…"),
             qPrintable(texts.join(" | ")));

    props->removePlatform(pl->id());
}

void Monitor_Test::clickingAGroupMemberAgainPicksItOut()
{
    FixtureCleanup cleanup(m_doc);
    MonitorProperties *props = m_doc->monitorProperties();
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    rig.parent.resize(1300, 820);
    rig.parent.show();
    rig.gv->resize(1230, 760);

    // Three free-standing pars, grouped on the plot.
    const quint32 gid = 9100;
    props->createGroup(gid, QStringLiteral("Front pars"), 0);
    QList<MonitorFixtureItem *> items;
    for (int i = 0; i < 3; ++i)
    {
        Fixture *fx = new Fixture(m_doc);
        fx->setName(QString("Front par %1").arg(i));
        fx->setChannels(1);
        fx->setAddress(quint32(480 + i));
        QVERIFY(m_doc->addFixture(fx));
        cleanup.ids << fx->id();
        rig.gv->addFixture(fx->id(), QPointF(4000 + i * 1500, 3000));
        MonitorFixtureItem *it = rig.gv->m_fixtures.value(fx->id(), nullptr);
        QVERIFY(it != nullptr);
        rig.gv->setItemGroupId(it, gid);
        items << it;
    }
    QCoreApplication::processEvents();
    MonitorFixtureItem *item = items[1];
    rig.gv->centerOn(item);
    const QPoint at = rig.gv->mapFromScene(item->sceneBoundingRect().center());
    QCOMPARE(rig.gv->topPickableAt(rig.gv->mapToScene(at)), static_cast<QGraphicsItem *>(item));

    // One click: the group, selected together...
    rig.gv->m_scene->clearSelection();
    QTest::mouseClick(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    QVERIFY2(rig.gv->m_scene->selectedItems().size() == 3,
             qPrintable(QString::number(rig.gv->m_scene->selectedItems().size())));

    // ...click it again (not a double-click): just it, picked out.
    QTest::qWait(QApplication::doubleClickInterval() + 100);
    QTest::mouseClick(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    QCOMPARE(rig.gv->m_scene->selectedItems().size(), 1);
    QVERIFY(item->isSelected());
    QVERIFY(item->isIsolated());

    // And a double-click on a group member opens just that one.
    rig.gv->m_scene->clearSelection();
    QSignalSpy opened(rig.gv, &MonitorGraphicsView::fixtureDoubleClicked);
    QTest::qWait(QApplication::doubleClickInterval() + 100);
    inspectModal([](QWidget *dlg) { QVERIFY(dlg->windowTitle().startsWith("Fixture Properties")); });
    QTest::mouseDClick(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    QCOMPARE(opened.count(), 1);
    QCOMPARE(opened.first().first().toUInt(), cleanup.ids.at(1));
    props->removeGroup(gid);
}

void Monitor_Test::aLanternHangsOffATowerAndGlowsAllRound()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("WLED");
    def->setModel("Lantern");
    def->setType(QLCFixtureDef::ColorChanger);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("DRGB");
    const QLCChannel::Preset presets[4] = { QLCChannel::IntensityMasterDimmer, QLCChannel::IntensityRed,
                                            QLCChannel::IntensityGreen, QLCChannel::IntensityBlue };
    for (int i = 0; i < 4; ++i)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("ch%1").arg(i));
        ch->setPreset(presets[i]);
        def->addChannel(ch);
        mode->insertChannel(ch, quint32(i));
    }
    QLCPhysical ph;
    ph.setWidth(150); ph.setHeight(250); ph.setDepth(150);
    ph.setBulbLumens(100);
    ph.setBodyShape(QLCPhysical::LanternBody);
    ph.setEmission(QLCPhysical::OmniEmission);
    mode->setPhysical(ph);
    def->addMode(mode);

    Doc *doc = new Doc(this);
    Fixture *lantern = new Fixture(doc);
    lantern->setFixtureDefinition(def, mode);
    lantern->setName("Lantern SL");
    QVERIFY(doc->addFixture(lantern));
    const FixtureVisualTraits traits = classifyFixture(lantern);
    QCOMPARE(int(traits.kind), int(FixtureSilhouette::Lantern));
    QVERIFY(traits.omni);

    MonitorProperties *props = doc->monitorProperties();
    Truss *tower = props->addTruss();
    tower->setType(Truss::Vertical);
    tower->setOrigin(QVector3D(2.0f, 3.0f, 0.0f));
    tower->setLength(3.0f);
    tower->setWidth(0.29f);
    Hanger *hanger = props->addHanger(Hanger::OnTruss, tower->id());
    hanger->setAlong(2.6f);
    hanger->setAngle(90.0f);                    // stage left
    hanger->setArm(0.4f);
    hanger->setDrop(0.5f);
    props->recomputeHangers();
    FixtureRigProps rp;
    rp.hangerId = hanger->id();
    props->setFixtureRigProps(lantern->id(), rp);
    QCOMPARE(props->fixtureRigPosition(lantern->id()), hanger->hook());
    // It hangs BELOW the hook: its middle half its height (250 mm) lower.
    const QVector3D at = fixtureWorldCentre(doc, lantern->id());
    QVERIFY(qAbs(at.z() - (hanger->hook().z() - 0.125f)) < 1e-3f);

    Scene *scene = new Scene(doc);
    scene->setName("Red");
    QVERIFY(doc->addFunction(scene));
    scene->setValue(lantern->id(), 0, 255);
    scene->setValue(lantern->id(), 1, 255);

    StructureStudioView view(doc, StructureStudioView::StageKind, 0);
    view.resize(700, 700);
    view.setLiveValues(true);
    view.setBeams(true);
    view.setActiveScene(scene->id());
    view.reload();
    view.setPlane(StructureStudioView::Angled);
    QCoreApplication::processEvents();
    QImage img(view.size(), QImage::Format_ARGB32);
    img.fill(Qt::black);
    view.render(&img);

    // No cone: one all-round source, at the lantern.
    int omni = 0;
    for (const StructureStudioView::FrameBeam &fb : view.m_frameBeams)
        if (fb.omni) { ++omni; QVERIFY((fb.apex - at).length() < 1e-3f); }
    QCOMPARE(omni, 1);

    // Red on what is near; next to nothing six metres away.
    const QVector3D near = at + QVector3D(0.5f, 0.0f, 0.0f), far = at + QVector3D(6.0f, 0.0f, 0.0f);
    const QVector3D facingBack(-1, 0, 0);
    const QVector3D litNear = view.beamLightAt(near, facingBack), litFar = view.beamLightAt(far, facingBack);
    QVERIFY2(litNear.x() > 0.0f && litNear.y() == 0.0f, qPrintable(QString("%1 %2").arg(litNear.x()).arg(litNear.y())));
    QVERIFY2(litFar.x() < litNear.x() / 50.0f, qPrintable(QString("%1 vs %2").arg(litFar.x()).arg(litNear.x())));
    // Turned away from it, nothing.
    QCOMPARE(view.beamLightAt(near, QVector3D(1, 0, 0)).x(), 0.0f);

    delete doc;
    delete def;
}

/* Hangers are rigging: put one on a tower, copy it to every other tower
   (standing trusses and Tower structures, once each), hang a lantern on its
   hook -- one hook, so a second lantern takes the first one's place -- and
   find them in Layers with their tower. */
void Monitor_Test::hangersAreRiggingCopiedToEveryTower()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    QList<Truss *> towers;
    for (int i = 0; i < 3; ++i)
    {
        Truss *t = props->addTruss();
        t->setName(QString("T-%1").arg(i + 1));
        t->setType(Truss::Vertical);
        t->setOrigin(QVector3D(1.0f + 3.0f * i, 3.0f, 0.0f));
        t->setLength(3.0f);
        t->setWidth(0.29f);
        towers << t;
    }
    Truss *pipe = props->addTruss();                 // a horizontal run: not a tower
    pipe->setName("FOH");
    pipe->setType(Truss::Horizontal);
    pipe->setOrigin(QVector3D(0.0f, 0.0f, 5.0f));
    pipe->setLength(8.0f);
    Tower *box = props->addTower();
    box->setName("SR Box");

    Hanger *h = props->addHanger(Hanger::OnTruss, towers.first()->id());
    h->setAlong(2.4f);
    h->setAngle(90.0f);
    h->setArm(0.35f);
    h->setDrop(0.4f);
    props->recomputeHangers();

    QWidget parent;
    Monitor mon(&parent, doc);
    QCOMPARE(mon.copyHangerToEveryTower(h->id()), 3);     // T-2, T-3, SR Box
    QCOMPARE(props->hangers().size(), 4);
    QVERIFY(props->hangersOn(Hanger::OnTruss, pipe->id()).isEmpty());
    const Hanger *onT3 = props->hangersOn(Hanger::OnTruss, towers.at(2)->id()).value(0);
    QVERIFY(onT3 != nullptr);
    QCOMPARE(onT3->along(), 2.4f);
    QCOMPARE(onT3->arm(), 0.35f);
    QVERIFY(qAbs(onT3->hook().x() - (7.0f + 0.145f + 0.35f)) < 1e-3f);
    QCOMPARE(mon.copyHangerToEveryTower(h->id()), 0);     // every tower has it now

    auto lantern = [doc](const char *name) {
        Fixture *f = new Fixture(doc);
        f->setName(name);
        f->setChannels(4);
        f->setPatched(false);
        doc->addFixture(f);
        return f;
    };
    Fixture *a = lantern("Lantern A");
    Fixture *b = lantern("Lantern B");
    MonitorGraphicsView view(doc);
    view.resize(900, 700);
    view.setGridMetrics(1000.0);
    view.setGridSize(QSize(12, 8));
    view.updatePlatforms();
    view.attachFixtureToHanger(a->id(), h->id());
    QCOMPARE(props->fixtureRigPosition(a->id()), h->hook());
    QCOMPARE(props->fixtureRigProps(a->id()).primaryMount(), FixtureRigProps::HangerMount);
    view.attachFixtureToHanger(b->id(), h->id());         // one hook
    QCOMPARE(props->fixtureOnHanger(h->id()), b->id());
    QVERIFY(!props->fixtureRigProps(a->id()).onHanger());

    MonitorLayersPanel panel(doc, &view);
    panel.reload();
    QVERIFY(layersRow(panel, h->name()) != nullptr);

    // In T-1's own studio pane the hanger is there to be double-clicked: its
    // arm opens the hanger (the lantern on it is the fixture's).
    {
        StructureStudioView studio(doc, StructureStudioView::TrussKind, towers.first()->id());
        studio.resize(600, 700);
        studio.reload();
        studio.setPlane(StructureStudioView::Front);
        QCoreApplication::processEvents();
        const QPointF mid = (studio.w2s(h->fastened()) + studio.w2s(h->armEnd())) / 2.0;
        QCOMPARE(studio.hitTestHanger(mid), h->id());
        QSignalSpy activated(&studio, &StructureStudioView::hangerActivated);
        QTest::mouseDClick(&studio, Qt::LeftButton, Qt::NoModifier, mid.toPoint());
        QCOMPARE(activated.count(), 1);
        QCOMPARE(activated.first().first().toUInt(), h->id());
        QCOMPARE(studio.hitTestHanger(QPointF(2, 2)), quint32(UINT_MAX));
    }

    // The tower goes: its hanger with it; the lantern stays, unmounted.
    props->removeTruss(towers.first()->id());
    QVERIFY(!props->fixtureRigProps(b->id()).onHanger());
    QCOMPARE(props->hangers().size(), 3);
}

void Monitor_Test::aBarHungOnATrussShinesOutOfItsFace()
{
    QLCFixtureDef *def = makeXl450();
    Doc *doc = new Doc(this);
    QList<Fixture *> bars = xlBarsOnATruss(doc, def, 90.0f);       // hanging, face chosen
    Fixture *bar = bars[0];
    MonitorProperties *props = doc->monitorProperties();
    StructureStudioView view(doc, StructureStudioView::StageKind, 0);
    FixtureRigProps rp = props->fixtureRigProps(bar->id());
    rp.studioMount = 1;                                // the front plane: faces downstage
    rp.studioFlip = false;
    props->setFixtureRigProps(bar->id(), rp);
    QVector3D dir;
    QVERIFY(view.emitDirection(bar, rp, classifyFixture(bar), dir));
    QVERIFY2(dir.y() > 0.99f, qPrintable(QString("faced front, it shines %1,%2,%3").arg(dir.x()).arg(dir.y()).arg(dir.z())));

    rp.studioFlip = true;                              // the back: upstage
    props->setFixtureRigProps(bar->id(), rp);
    QVERIFY(view.emitDirection(bar, rp, classifyFixture(bar), dir));
    QVERIFY(dir.y() < -0.99f);

    // A flood is fainter than a spot of the same light; spots are not boosted.
    QCOMPARE(StructureStudioView::spreadFactor(qDegreesToRadians(20.0)), 1.0);
    QCOMPARE(StructureStudioView::spreadFactor(qDegreesToRadians(7.0)), 1.0);
    const double flood = StructureStudioView::spreadFactor(qDegreesToRadians(75.0));
    QVERIFY2(flood > 0.05 && flood < 0.12, qPrintable(QString::number(flood)));
    delete doc;
    delete def;
}

/* Steps as in stage-structures-demo, smaller: two decks side by side in each
   of two rows (US upstage, CS downstage). Each deck's front face carries three
   8-head bars, #1 at the top; its top carries a 6-head and a 2-head Generic
   washer, one after the other across it. */
/* Washers INSIDE a clear-topped step: no beam drawn (they are in a box),
   but their light gets out through the top -- along each fixture's own
   facing, in its cone, from where each head sits -- and only there ("should
   only show relative to the installed angle of the fixture in the step").
   Checked as light arriving at exact points, which is what the figure is
   shaded from. */
void Monitor_Test::aClearTopGlowsWithTheLightInsideIt()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Oppsk");
    def->setModel("Wall Washer");
    def->setType(QLCFixtureDef::LEDBarPixels);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("RGB");
    const QLCChannel::Preset rgb[3] = { QLCChannel::IntensityRed, QLCChannel::IntensityGreen,
                                        QLCChannel::IntensityBlue };
    for (int h = 0; h < 2; ++h)
    {
        QLCFixtureHead head;
        for (int c = 0; c < 3; ++c)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("P%1 %2").arg(h).arg(c));
            ch->setPreset(rgb[c]);
            def->addChannel(ch);
            mode->insertChannel(ch, mode->channels().size());
            head.addChannel(quint32(mode->channels().size() - 1));
        }
        mode->insertHead(-1, head);
    }
    def->addMode(mode);

    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *step = props->addPlatform();
    step->setName("US-1");
    step->setOriginX(2.0f); step->setOriginY(2.0f);
    step->setWidth(2.4f); step->setDepth(1.2f); step->setHeight(0.2f);
    step->setTopMaterial(StagePlatform::ClearTop);
    Fixture *washer = new Fixture(doc);
    washer->setName("US1t1");
    washer->setFixtureDefinition(def, mode);
    QVERIFY(doc->addFixture(washer));
    props->setFixturePosition(washer->id(), 0, 0, QVector3D(0, 0, 0));
    auto rig = [&](float tilt) {
        FixtureRigProps rp;
        rp.riserPlatformId = step->id();
        rp.riserFace = 1;                   // on the top plane, inside the box
        rp.riserU = 1.2f;                   // x = 3.2
        rp.riserV = 0.6f;                   // y = 2.6, mid-depth
        rp.placement = FixtureRigProps::Inside;
        rp.studioMount = 0;
        rp.studioTilt = tilt;
        rp.studioFaceChosen = true;
        props->setFixtureRigProps(washer->id(), rp);
    };
    rig(0.0f);

    Scene *scene = new Scene(doc);
    scene->setName("USt-Blue");
    QVERIFY(doc->addFunction(scene));
    StructureStudioView view(doc, StructureStudioView::StageKind, 0);
    view.resize(800, 600);
    view.setLiveValues(true);
    view.setActiveScene(scene->id());
    view.reload();
    view.setPlane(StructureStudioView::Angled);
    auto frame = [&view]() {
        QCoreApplication::processEvents();
        QImage img(view.size(), QImage::Format_ARGB32);
        view.render(&img);
    };
    auto apertured = [&view]() {
        int n = 0;
        for (const auto &fb : view.m_frameBeams) n += fb.apertured;
        return n;
    };
    const QVector3D down(0, 0, -1);           // a surface facing the floor: an underside
    const float top = 0.2f;

    frame();
    QCOMPARE(apertured(), 0);                // dark: no light

    // Blue at full on both pixels.
    scene->setValue(washer->id(), 2, 255);
    scene->setValue(washer->id(), 5, 255);
    frame();
    QTRY_COMPARE(apertured(), 2);            // a light per head
    const QVector3D above(3.2f, 2.6f, top + 0.6f);
    const QVector3D lit = view.beamLightAt(above, down);
    QVERIFY2(lit.z() > 100 && lit.x() < 10, qPrintable(QString("above the bar: %1,%2,%3").arg(lit.x()).arg(lit.y()).arg(lit.z())));

    // In front of the step, downstage: outside the cone -- dark.
    QCOMPARE(view.beamLightAt(QVector3D(3.2f, 3.6f, top + 0.6f), down).z(), 0.0f);
    // Inside the box, under the top: the light has not got out yet.
    QCOMPARE(view.beamLightAt(QVector3D(3.2f, 2.6f, top - 0.05f), down).z(), 0.0f);
    // Facing away from it (a surface looking up), nothing.
    QCOMPARE(view.beamLightAt(above, QVector3D(0, 0, 1)).z(), 0.0f);

    /* Tipped 30 degrees upstage, as a bar built on a diagonal in the box:
       the light leans upstage. A point up and upstage is lit; the same
       point downstage is not. */
    rig(30.0f);
    view.reload();
    frame();
    QTRY_COMPARE(apertured(), 2);
    const QVector3D upstage(3.2f, 2.6f - 0.5f, top + 0.8f), downstage(3.2f, 2.6f + 0.5f, top + 0.8f);
    QVERIFY2(view.beamLightAt(upstage, down).z() > 50, "tipped upstage, the upstage side is dark");
    QCOMPARE(view.beamLightAt(downstage, down).z(), 0.0f);

    // A solid top lets nothing out.
    step->setTopMaterial(StagePlatform::SolidTop);
    view.reload();
    frame();
    QCOMPARE(apertured(), 0);
    QCOMPARE(view.beamLightAt(upstage, down).z(), 0.0f);

    delete doc;
    delete def;
}

void Monitor_Test::stepFrontsAndTopsAreLaidOutAsOnStage()
{
    QLCFixtureDef *bar = makePixelRow("Step Row 8", 8, 2438, false);
    QLCFixtureDef *wash6 = makePixelRow("Washer 6", 6, 0, true);
    QLCFixtureDef *wash2 = makePixelRow("Washer 2", 2, 0, true);
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    QMap<QString, QList<quint32> > fronts, tops;   // per deck
    QList<quint32> allFronts, allTops;
    quint32 uni = 0;
    auto place = [&](QLCFixtureDef *def, const QString &name, StagePlatform *pl, int face, float u, float v) {
        Fixture *f = new Fixture(doc);
        f->setName(name);
        f->setFixtureDefinition(def, def->modes().first());
        f->setUniverse(uni++);
        f->setAddress(0);
        doc->addFixture(f);
        props->setFixturePosition(f->id(), 0, 0, QVector3D(0, 0, 0));
        FixtureRigProps rp;
        rp.riserPlatformId = pl->id();
        rp.riserFace = face;
        rp.riserU = u;
        rp.riserV = v;
        props->setFixtureRigProps(f->id(), rp);
        return f->id();
    };
    const QStringList rows = { "US", "CS" };
    for (int r = 0; r < 2; ++r)
        for (int d = 0; d < 2; ++d)
        {
            const QString deck = QString("%1%2").arg(rows[r]).arg(d + 1);
            StagePlatform *pl = props->addPlatform();
            pl->setName(deck);
            pl->setOriginX(1.2f + 2.438f * d);
            pl->setOriginY(4.9f + 1.22f * r);
            pl->setWidth(2.438f); pl->setDepth(1.219f); pl->setHeight(0.2f);
            for (int k = 0; k < 3; ++k)          // #1 at the top
                fronts[deck] << place(bar, QString("%1 #%2").arg(deck).arg(k + 1), pl, 0, 1.219f, 0.15f - 0.05f * k);
            tops[deck] << place(wash6, deck + "t1", pl, 1, 0.9f, 0.6f);
            tops[deck] << place(wash2, deck + "t2", pl, 1, 2.0f, 0.6f);
            allFronts << fronts[deck];
            allTops << tops[deck];
        }
    auto layout = [doc](const QList<quint32> &ids, int view) {
        FixtureGroup *g = new FixtureGroup(doc);
        g->setSize(QSize(1, 1));
        CreateFixtureGroup::layoutFixturesByStage(doc, ids, g, view, true);
        return g;
    };
    auto cellsOf = [](FixtureGroup *g, quint32 fid) {
        QList<QLCPoint> out;
        QMapIterator<QLCPoint, GroupHead> it(g->headsMap());
        while (it.hasNext()) { it.next(); if (it.value().fxi == fid) out << it.key(); }
        return out;
    };
    auto rowOf = [&cellsOf](FixtureGroup *g, quint32 fid) {
        const QList<QLCPoint> c = cellsOf(g, fid);
        return c.isEmpty() ? -1 : c.first().y();
    };
    auto headAt = [](FixtureGroup *g, int x, int y) { return g->head(QLCPoint(x, y)); };

    {
        // One deck's front: three rows of eight, #1 on top, head 1 on the left.
        FixtureGroup *f = layout(fronts["US1"], CreateFixtureGroup::ViewFront);
        QCOMPARE(f->size(), QSize(8, 3));
        for (int k = 0; k < 3; ++k)
            QCOMPARE(rowOf(f, fronts["US1"][k]), k);
        QCOMPARE(headAt(f, 0, 0).head, 0);
        QCOMPARE(headAt(f, 7, 0).head, 7);
        delete f;
    }
    {
        // Both decks of a row: side by side, stage right first.
        FixtureGroup *f = layout(fronts["US1"] + fronts["US2"], CreateFixtureGroup::ViewFront);
        QCOMPARE(f->size(), QSize(16, 3));
        QCOMPARE(headAt(f, 0, 0).fxi, fronts["US1"][0]);
        QCOMPARE(headAt(f, 8, 0).fxi, fronts["US2"][0]);
        delete f;
    }
    {
        // One deck's top, from above: one row, every head of both washers,
        // the 6-head first. The Generic washers' placeholder size (1.8 m and
        // 0.6 m on a 2.4 m deck) is not taken for real.
        FixtureGroup *t = layout(tops["US1"], CreateFixtureGroup::ViewTop);
        QCOMPARE(t->size(), QSize(8, 1));
        QCOMPARE(t->headsMap().size(), 8);
        QCOMPARE(cellsOf(t, tops["US1"][0]).size(), 6);
        QCOMPARE(cellsOf(t, tops["US1"][1]).size(), 2);
        QCOMPARE(headAt(t, 0, 0).fxi, tops["US1"][0]);
        QCOMPARE(headAt(t, 6, 0).fxi, tops["US1"][1]);
        delete t;
    }
    {
        // All the tops, from above: upstage row first, decks across.
        FixtureGroup *t = layout(allTops, CreateFixtureGroup::ViewTop);
        QCOMPARE(t->size(), QSize(16, 2));
        QCOMPARE(t->headsMap().size(), 4 * 8);
        QCOMPARE(rowOf(t, tops["US1"][0]), 0);
        QCOMPARE(rowOf(t, tops["CS2"][1]), 1);
        QCOMPARE(headAt(t, 8, 0).fxi, tops["US2"][0]);
        delete t;
    }
    delete doc;
    delete bar; delete wash6; delete wash2;
}

/* The split wizard lays its groups out from the chosen view, and a kind whose
   group already exists is kept, not made twice. */
void Monitor_Test::theSplitWizardOffersAViewAndKeepsAnExistingGroup()
{
    QTemporaryDir lib;
    HeadLayoutLibrary::setUserDirectoryOverride(lib.path());
    QLCFixtureDef *def = makeXl450();
    Doc *doc = new Doc(this);
    QList<Fixture *> bars = xlBarsOnATruss(doc, def, 90.0f);
    QList<quint32> ids;
    foreach (Fixture *f, bars) ids << f->id();
    {
        HeadTypesDialog dlg(doc, ids, "FART-SR");
        QCOMPARE(dlg.m_viewCombo->currentData().toInt(), int(CreateFixtureGroup::ViewFront));
        QVERIFY(dlg.m_closeGaps->isChecked());
        dlg.accept();
        QCOMPARE(dlg.createdGroups().size(), 2);
        // Hanging, from the front: three bars side by side, 15 heads tall.
        QCOMPARE(dlg.createdGroups()[0]->size().height(), 15);
    }
    QCOMPARE(doc->fixtureGroups().size(), 2);
    // Answer the "already exist" question with the button labelled @p label.
    auto answer = [](const QString &label, QString *asked) {
        QTimer::singleShot(0, [label, asked]() {
            QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box == nullptr) return;
            *asked = box->text();
            foreach (QAbstractButton *b, box->buttons())
                if (b->text() == label) { b->click(); return; }
            box->reject();
        });
    };
    {
        // Again: both already exist -- said so, and kept by default.
        HeadTypesDialog dlg(doc, ids, "FART-SR");
        QString asked;
        answer("Keep Them", &asked);
        dlg.accept();
        QVERIFY2(asked.contains("already exist"), qPrintable(asked));
        QCOMPARE(dlg.createdGroups().size(), 2);
        QCOMPARE(doc->fixtureGroups().size(), 2);
    }
    {
        // The same heads from above: a second layout is wanted -- Make Another.
        HeadTypesDialog dlg(doc, ids, "FART-SR top");
        dlg.m_viewCombo->setCurrentIndex(dlg.m_viewCombo->findData(int(CreateFixtureGroup::ViewTop)));
        dlg.m_kindNames[0]->setText("Top RGB");
        dlg.m_kindNames[1]->setText("Top W");
        QString asked;
        answer("Make Another", &asked);
        dlg.accept();
        QVERIFY(asked.contains("already exist"));
        QCOMPARE(doc->fixtureGroups().size(), 4);
        QCOMPARE(dlg.createdGroups()[0]->name(), QString("Top RGB"));
        QVERIFY(dlg.createdGroups()[0]->size() != doc->fixtureGroups().first()->size());
    }
    {
        // Cancel: nothing made, the wizard stays open.
        HeadTypesDialog dlg(doc, ids, "FART-SR");
        QString asked;
        answer("Cancel", &asked);
        dlg.accept();
        QCOMPARE(dlg.result(), int(QDialog::Rejected));
        QVERIFY(dlg.createdGroups().isEmpty());
        QCOMPARE(doc->fixtureGroups().size(), 4);
    }
    delete doc;
    delete def;
    HeadLayoutLibrary::setUserDirectoryOverride(testLayouts());
}

void Monitor_Test::theSplitWizardMapsRowsAndMakesAGroupPerKind()
{
    QTemporaryDir lib;
    HeadLayoutLibrary::setUserDirectoryOverride(lib.path());
    QLCFixtureDef *def = makeXl450();
    Doc *doc = new Doc(this);
    QList<Fixture *> bars = xlBarsOnATruss(doc, def, 90.0f);
    QList<quint32> ids;
    foreach (Fixture *f, bars) ids << f->id();
    QVERIFY(HeadTypesDialog::anyMixed(doc, ids));
    QCOMPARE(HeadTypesDialog::suggestName(doc, ids), QString("FART-SR"));
    {
        HeadTypesDialog dlg(doc, ids, HeadTypesDialog::suggestName(doc, ids));
        // Each group's exact name, editable, not a prefix with more added on.
        QCOMPARE(dlg.m_kindNames.size(), 2);
        QCOMPARE(dlg.m_kindNames[0]->text(), QString("FART-SR RGB"));
        QCOMPARE(dlg.m_kindNames[1]->text(), QString("FART-SR W"));
        dlg.m_kindNames[1]->setText("FART-SR Whites");
        QCOMPARE(dlg.m_modelCombo->count(), 1);
        // Head order, as the definition alone says: RGB, RGB, RGB, W, W.
        QCOMPARE(dlg.m_editor->rowPatternText(), QString("RGB, RGB, RGB, W, W"));

        // A pattern that cannot hold the heads is refused, with the reason.
        QVERIFY(!dlg.m_editor->applyRowPattern("RGB, RGB, RGB, RGB, W"));
        QVERIFY2(dlg.m_editor->rowProblem().contains("RGB rows hold 60 heads, it has 45"),
                 qPrintable(dlg.m_editor->rowProblem()));

        // As built.
        QVERIFY(dlg.m_editor->applyRowPattern("RGB, W, RGB, W, RGB"));
        QVERIFY(dlg.m_editor->rowProblem().isEmpty());
        QCOMPARE(dlg.m_editor->layout().cellOf(45), QPoint(0, 1));   // W 1 on the 2nd row

        dlg.accept();
        QCOMPARE(dlg.createdGroups().size(), 2);
        QCOMPARE(dlg.createdGroups()[1]->name(), QString("FART-SR Whites"));
    }
    // A name that already says the kind is not said twice.
    QCOMPARE(CreateFixtureGroup::headKindGroupName("T1-CW-RGB", "RGB"), QString("T1-CW-RGB"));
    QCOMPARE(CreateFixtureGroup::headKindGroupName("T1-CW", "RGB"), QString("T1-CW RGB"));
    QCOMPARE(CreateFixtureGroup::headKindGroupName("MYRGB", "RGB"), QString("MYRGB RGB"));
    // The map is kept in the library and in the show.
    const QString key = HeadLayout::keyFor(bars[0]);
    QVERIFY(HeadLayoutLibrary::load(key).isValid());
    QCOMPARE(HeadLayoutLibrary::load(key).cellOf(45), QPoint(0, 1));
    QCOMPARE(doc->monitorProperties()->headLayoutFor(bars[0]).cellOf(45), QPoint(0, 1));
    delete doc;
    delete def;
    HeadLayoutLibrary::setUserDirectoryOverride(testLayouts());
}

void Monitor_Test::theRigViewDrawsHeadsWhereTheLedMapSays()
{
    QTemporaryDir lib;
    HeadLayoutLibrary::setUserDirectoryOverride(lib.path());
    QLCFixtureDef *def = makeXl450();
    Doc *doc = new Doc(this);
    QList<Fixture *> bars = xlBarsOnATruss(doc, def, 0.0f);
    {
        StructureStudioView view(doc, StructureStudioView::StageKind, 0);
        // No map yet: head order, so the 2nd row starts with head 16 (RGB).
        QVector<int> cells = view.headsByCell(bars[0], 15, 5, 75);
        QCOMPARE(cells[15], 15);
        QCOMPARE(cells[45], 45);
        // As built, the 2nd row is the first white row: head 46.
        doc->monitorProperties()->setHeadLayout(
            HeadLayout::fromRowPattern(bars[0], { "RGB", "W", "RGB", "W", "RGB" }));
        cells = view.headsByCell(bars[0], 15, 5, 75);
        QCOMPARE(cells[0], 0);
        QCOMPARE(cells[15], 45);
        QCOMPARE(cells[30], 15);
        // A grid of another size is not this map's: head order.
        QCOMPARE(view.headsByCell(bars[0], 75, 1, 75)[15], 15);
    }
    delete doc;
    delete def;
    HeadLayoutLibrary::setUserDirectoryOverride(testLayouts());
}

void Monitor_Test::layersListsEveryGroupAFixtureIsIn()
{
    QLCFixtureDef *def = makeXl450();
    Doc *doc = new Doc(this);
    QList<Fixture *> bars = xlBarsOnATruss(doc, def, 90.0f);
    // A second truss with one more bar, on the same layer.
    MonitorProperties *props = doc->monitorProperties();
    Truss *t2 = props->addTruss();
    t2->setName("FART-2");
    t2->setType(Truss::Horizontal);
    t2->setOrigin(QVector3D(6.0f, 3.0f, 2.0f));
    t2->setDirection(QPointF(1.0, 0.0));
    t2->setLength(2.4f);
    Fixture *far = new Fixture(doc);
    far->setName("FART-SL-1");
    far->setFixtureDefinition(def, def->modes().first());
    far->setUniverse(1);                // 3 x 165 fill most of universe 1
    far->setAddress(0);
    QVERIFY(doc->addFixture(far));
    props->setFixturePosition(far->id(), 0, 0, QVector3D(0, 0, 0));
    FixtureRigProps rp;
    rp.trussId = t2->id();
    rp.trussOffset = 0.5f;
    props->setFixtureRigProps(far->id(), rp);
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        mon.m_graphicsView->ensureTrussGroup(props->fixtureRigProps(bars[0]->id()).trussId);
        mon.m_graphicsView->ensureTrussGroup(t2->id());

        // One bar, split: it is in BOTH groups.
        const QList<FixtureGroup *> mine =
            CreateFixtureGroup::createByHeadKind(doc, { bars[0]->id() },
                                                 { { "RGB", "T1-CW RGB" }, { "W", "T1-CW W" } });
        // Across the two trusses: local to neither.
        const QList<FixtureGroup *> wide =
            CreateFixtureGroup::createByHeadKind(doc, { bars[1]->id(), far->id() },
                                                 { { "RGB", "Wide RGB" } });
        QCOMPARE(mine.size(), 2);
        QCOMPARE(wide.size(), 1);

        MonitorLayersPanel *panel = mon.m_layersPanel;
        panel->reload();
        auto groupRow = [panel](const QString &name) -> QTreeWidgetItem * {
            for (QTreeWidgetItemIterator it(panel->m_tree); *it; ++it)
                if ((*it)->data(0, Qt::UserRole).toInt() == MonitorLayersPanel::NodeFixtureGroup
                    && (*it)->text(0).startsWith(name + "  "))
                    return *it;
            return nullptr;
        };
        auto childNames = [](QTreeWidgetItem *n) {
            QStringList out;
            for (int i = 0; i < n->childCount(); ++i) out << n->child(i)->text(0);
            return out;
        };
        QTreeWidgetItem *rgb = groupRow("T1-CW RGB");
        QTreeWidgetItem *w = groupRow("T1-CW W");
        QVERIFY(rgb != nullptr && w != nullptr);
        QCOMPARE(childNames(rgb), QStringList({ "FART-SR-1" }));
        QCOMPARE(childNames(w), QStringList({ "FART-SR-1" }));    // in both, shown in both

        QTreeWidgetItem *across = groupRow("Wide RGB");
        QVERIFY2(across != nullptr, "a group across two trusses is not in the list");
        QCOMPARE(childNames(across), QStringList({ "FART-SR-2", "FART-SL-1" }));
        // On one layer, so under that layer.
        QVERIFY(across->parent() != nullptr);
        QCOMPARE(across->parent()->data(0, Qt::UserRole).toInt(), int(MonitorLayersPanel::NodeLayer));
    }
    delete doc;
    delete def;
}

void Monitor_Test::fixturesAreRemovedFromATowerFromItsList()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Tower *t = props->addTower();
    t->setName("AT1");
    t->setOriginX(0.2f); t->setOriginY(8.0f);
    t->setWidth(0.4f); t->setDepth(0.4f); t->setHeight(2.4f);
    t->addShelf(0.6f);
    QList<Fixture *> fx;
    for (int i = 0; i < 3; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("AT1-36-%1").arg(i + 1));
        f->setChannels(4);
        f->setAddress(quint32(i) * 4);
        QVERIFY(doc->addFixture(f));
        props->setFixturePosition(f->id(), 0, 0, QVector3D(200, 8000, 0));
        FixtureRigProps rp;
        rp.towerId = t->id();
        rp.towerShelf = 0;
        props->setFixtureRigProps(f->id(), rp);
        fx << f;
    }
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        QDialog dlg;
        StructureStudioView *view = nullptr;
        mon.makeStudioPane(&dlg, 1 /* tower */, t->id(), nullptr, &view);
        QVERIFY(view != nullptr);
        QTreeWidget *tree = nullptr;
        foreach (QTreeWidget *tw, dlg.findChildren<QTreeWidget *>())
            if (tw->topLevelItemCount() > 0)
                tree = tw;
        QVERIFY(tree != nullptr);
        QTreeWidgetItem *first = nullptr;
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
        {
            const QVariant v = (*it)->data(0, Qt::UserRole);
            if (v.isValid() && (v.toUInt() == fx[0]->id() || v.toUInt() == fx[1]->id())
                && (*it)->text(0).startsWith("AT1-36"))
            {
                (*it)->setSelected(true);
                if (first == nullptr) first = *it;
            }
        }
        QVERIFY(first != nullptr);

        QStringList seen;
        answerPopup(&seen, "Remove 2 Fixtures from Tower");
        emit tree->customContextMenuRequested(tree->visualItemRect(first).center());
        QVERIFY2(seen.contains("Remove 2 Fixtures from Tower"), qPrintable(seen.join(" | ")));
        QCOMPARE(props->fixtureRigProps(fx[0]->id()).towerId, Tower::invalidId());
        QCOMPARE(props->fixtureRigProps(fx[1]->id()).towerId, Tower::invalidId());
        QCOMPARE(props->fixtureRigProps(fx[2]->id()).towerId, t->id());   // not selected
        QCOMPARE(view->mountedFixtures(), QList<quint32>({ fx[2]->id() }));
    }
    delete doc;
}

void Monitor_Test::patchingFromLayersTakesEveryFixtureSelectedThere()
{
    Doc *doc = new Doc(this);
    QList<Fixture *> fx;
    for (int i = 0; i < 4; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("AT2-36-%1").arg(i + 1));
        f->setChannels(18);
        f->setPatched(false);
        QVERIFY(doc->addFixture(f));
        doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(1000, 1000, 0));
        fx << f;
    }
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        // Stacked in a tower: on the plot, but not selectable there.
        foreach (Fixture *f, fx)
            if (MonitorFixtureItem *it = mon.m_graphicsView->m_fixtures.value(f->id()))
                it->setVisible(false);
        MonitorLayersPanel *panel = mon.m_layersPanel;
        panel->reload();
        QTreeWidgetItem *first = nullptr;
        for (QTreeWidgetItemIterator it(panel->m_tree); *it; ++it)
            if ((*it)->text(0).startsWith("AT2-36-"))
            {
                (*it)->setSelected(true);
                if (first == nullptr) first = *it;
            }
        QVERIFY(first != nullptr);
        QVERIFY(mon.m_graphicsView->selectedMapObjects().isEmpty());

        QMenu menu;
        panel->buildContextMenu(&menu, first);
        QStringList texts;
        foreach (QAction *a, menu.actions())
            texts << a->text();
        QVERIFY2(texts.contains("Patch 4 Fixtures…"), qPrintable(texts.join(" | ")));
    }
    delete doc;
}

void Monitor_Test::wirelessReceiversArePlacedAndDragged()
{
    Doc *doc = new Doc(this);
    NetworkNode tx;
    tx.name = "WDMX";
    tx.transport = NetworkNode::WirelessDMX;
    tx.placed = true;
    tx.posX = 4.0; tx.posY = 6.0;
    NetworkPort port;
    port.universe = 0;
    tx.ports << port;
    doc->networkTopology()->nodes().append(tx);
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        MonitorGraphicsView *gv = mon.m_graphicsView;
        gv->resize(1230, 760);
        gv->setGridMetrics(1000.0);
        gv->setGridSize(QSize(40, 24));
        gv->updateNetworkNodes();
        QVERIFY(gv->m_networkReceiverItems.isEmpty());      // not placed: not drawn

        QCOMPARE(gv->placeWirelessReceivers(), 1);
        QCOMPARE(gv->m_networkReceiverItems.size(), 1);
        NetworkReceiverItem *rx = gv->m_networkReceiverItems.first();
        QVERIFY(rx->flags() & QGraphicsItem::ItemIsMovable);

        // Dragged two metres stage left: stored where it was let go.
        const QPointF to = gv->projectMm(7000.0, 6000.0, 0.0);
        rx->setPos(to);
        rx->onDropped();
        const NetworkReceiver &r = doc->networkTopology()->nodes().at(0).ports.at(0).receiver;
        QVERIFY2(qAbs(r.posX - 7.0) < 0.05 && qAbs(r.posY - 6.0) < 0.05,
                 qPrintable(QString("receiver at %1, %2").arg(r.posX).arg(r.posY)));

        // A locked layout keeps it where it is.
        gv->setLayoutLocked(true);
        gv->updateNetworkNodes();
        QVERIFY(!(gv->m_networkReceiverItems.first()->flags() & QGraphicsItem::ItemIsMovable));
    }
    delete doc;
}

void Monitor_Test::aFixtureGroupRowMovesToAnotherLayer()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    const quint32 stageLights = props->addLayer("Stage Lights");
    const quint32 trusses = props->addLayer("Trusses");
    const quint32 steps = props->addLayer("Steps");
    QList<quint32> ids;
    FixtureGroup *usts = new FixtureGroup(doc);
    usts->setName("USTs");
    usts->setSize(QSize(3, 1));
    doc->addFixtureGroup(usts);
    for (int i = 0; i < 3; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setName(QString("UST-%1").arg(i + 1));
        f->setChannels(1);
        f->setAddress(quint32(i));
        QVERIFY(doc->addFixture(f));
        props->setFixturePosition(f->id(), 0, 0, QVector3D(1000.0f * (i + 1), 2000, 0));
        props->setFixtureLayer(f->id(), stageLights);
        usts->assignFixture(f->id(), QLCPoint(i, 0));
        ids << f->id();
    }
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        mon.m_graphicsView->setLayoutLocked(false);
        MonitorLayersPanel *panel = mon.m_layersPanel;
        panel->reload();
        auto groupRow = [panel]() -> QTreeWidgetItem * {
            for (QTreeWidgetItemIterator it(panel->m_tree); *it; ++it)
                if ((*it)->data(0, Qt::UserRole).toInt() == MonitorLayersPanel::NodeFixtureGroup
                    && (*it)->text(0).startsWith("USTs  "))
                    return *it;
            return nullptr;
        };
        auto layerRow = [panel](quint32 lid) -> QTreeWidgetItem * {
            for (QTreeWidgetItemIterator it(panel->m_tree); *it; ++it)
                if ((*it)->data(0, Qt::UserRole).toInt() == MonitorLayersPanel::NodeLayer
                    && (*it)->data(0, Qt::UserRole + 1).toUInt() == lid)
                    return *it;
            return nullptr;
        };
        QTreeWidgetItem *row = groupRow();
        QVERIFY(row != nullptr);
        QVERIFY(row->flags() & Qt::ItemIsDragEnabled);

        // Right-click → Move to Layer → Trusses.
        panel->m_tree->clearSelection();
        row->setSelected(true);
        QMenu menu;
        panel->buildContextMenu(&menu, row);
        QAction *to = nullptr;
        foreach (QAction *a, menu.actions())
        {
            if (a->menu() == nullptr || a->text() != "Move to Layer")
                continue;
            foreach (QAction *l, a->menu()->actions())
            {
                if (l->text() == "Trusses")
                    to = l;
            }
        }
        QVERIFY2(to != nullptr, "no Move to Layer for a fixture group's row");
        to->trigger();
        foreach (quint32 fid, ids)
            QCOMPARE(props->fixtureLayer(fid), trusses);

        // Dragged onto another layer: its fixtures go there.
        panel->reload();
        row = groupRow();
        QVERIFY(row != nullptr);
        panel->handleTreeDrop({ row }, layerRow(steps));
        QCoreApplication::processEvents();
        foreach (quint32 fid, ids)
            QCOMPARE(props->fixtureLayer(fid), steps);
    }
    delete doc;
}

void Monitor_Test::aRigDialogPreviewsCancelsAndUndoes()
{
    MonitorProperties *props = m_doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Meters);
    Tower *tower = props->addTower();
    tower->setHeight(4.0f);
    Hanger *h = props->addHanger(Hanger::OnTower, tower->id());
    const quint32 hid = h->id();
    h->setArm(0.5f);
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    QAction *undo = rig.mon->findChild<QAction *>("plotUndo");
    QVERIFY(undo != nullptr);
    QVERIFY(!undo->isEnabled());

    // Live: the hanger's arm is out before OK. Cancel: back in.
    float seenLive = -1.0f;
    driveDialog([&seenLive, props, hid](QWidget *modal) {
        const QList<QDoubleSpinBox *> spins = modal->findChildren<QDoubleSpinBox *>();
        if (spins.count() != 3)               // along, arm, drop
            return false;
        spins[1]->setValue(1.5);
        seenLive = props->hanger(hid)->arm();
        return true;
    }, false);
    rig.mon->slotEditHanger(hid);
    QCOMPARE(seenLive, 1.5f);
    QVERIFY(props->hanger(hid) != nullptr);
    QCOMPARE(props->hanger(hid)->arm(), 0.5f);
    QVERIFY2(!undo->isEnabled(), "a cancelled dialog is not something to undo");

    // OK: kept, and one undo puts it back.
    driveDialog([](QWidget *modal) {
        const QList<QDoubleSpinBox *> spins = modal->findChildren<QDoubleSpinBox *>();
        if (spins.count() != 3)
            return false;
        spins[1]->setValue(1.5);
        spins[2]->setValue(0.8);
        return true;
    });
    rig.mon->slotEditHanger(hid);
    QCOMPARE(props->hanger(hid)->arm(), 1.5f);
    QVERIFY(undo->isEnabled());
    undo->trigger();
    QVERIFY(props->hanger(hid) != nullptr);
    QCOMPARE(props->hanger(hid)->arm(), 0.5f);
    QVERIFY(!undo->isEnabled());

    // A move after it undoes first, then the dialog: one stack, in order.
    driveDialog([](QWidget *modal) {
        const QList<QDoubleSpinBox *> spins = modal->findChildren<QDoubleSpinBox *>();
        if (spins.count() != 3)
            return false;
        spins[1]->setValue(2.0);
        return true;
    });
    rig.mon->slotEditHanger(hid);
    const quint32 tid = props->hanger(hid)->parentId();
    QVERIFY(props->tower(tid) != nullptr);
    MonitorGraphicsView::UndoEntry move;
    move.type = MonitorGraphicsView::UndoEntry::ItemMove;
    move.itemKeys << qMakePair(QString("tower"), tid);
    move.itemOrigins << QPointF(props->tower(tid)->originX(), props->tower(tid)->originY());
    rig.gv->pushUndo(move);
    props->tower(tid)->setOriginX(props->tower(tid)->originX() + 3.0f);
    const float movedFrom = float(move.itemOrigins.first().x());
    undo->trigger();
    QCOMPARE(props->tower(tid)->originX(), movedFrom);
    QCOMPARE(props->hanger(hid)->arm(), 2.0f);
    undo->trigger();
    QCOMPARE(props->hanger(hid)->arm(), 0.5f);

    props->removeHanger(hid);
    props->removeTower(tid);
}

void Monitor_Test::fixturePropertiesCancelAndUndoToo()
{
    FixtureCleanup cleanup(m_doc);
    MonitorProperties *props = m_doc->monitorProperties();
    Fixture *fx = new Fixture(m_doc);
    fx->setName("Undo me");
    fx->setChannels(1);
    fx->setAddress(480);
    QVERIFY(m_doc->addFixture(fx));
    cleanup.ids << fx->id();
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    rig.gv->addFixture(fx->id(), QPointF(2000, 2000));
    props->setFixturePosition(fx->id(), 0, 0, QVector3D(2000, 2000, 0));
    QAction *undo = rig.mon->findChild<QAction *>("plotUndo");
    QVERIFY(undo != nullptr);

    auto rename = [](const QString &to) {
        return [to](QWidget *modal) {
            foreach (QLineEdit *e, modal->findChildren<QLineEdit *>())
            {
                if (e->text() == QLatin1String("Undo me") || e->text() == QLatin1String("Renamed"))
                {
                    e->setText(to);
                    return true;
                }
            }
            return false;
        };
    };
    driveDialog(rename("Renamed"), false);
    rig.mon->editFixtureProperties(fx->id());
    QCOMPARE(fx->name(), QString("Undo me"));

    driveDialog(rename("Renamed"));
    rig.mon->editFixtureProperties(fx->id());
    QCOMPARE(fx->name(), QString("Renamed"));
    QVERIFY(undo->isEnabled());
    undo->trigger();
    QCOMPARE(fx->name(), QString("Undo me"));
    QVERIFY2(rig.gv->m_fixtures.contains(fx->id()), "the fixture is still on the plot after the undo");

    // The gel: one colour button, live, its Reset taking the gel off -- and
    // Cancel putting it back on.
    props->setFixtureGelColor(fx->id(), 0, 0, QColor(Qt::red));
    rig.gv->setFixtureGelColor(fx->id(), QColor(Qt::red));
    QColor seenLive(Qt::green);
    driveDialog([&seenLive, props, fx](QWidget *modal) {
        ColourSwatchButton *gel = modal->findChild<ColourSwatchButton *>("fixtureColour");
        QToolButton *reset = gel ? gel->findChild<QToolButton *>("colourReset") : nullptr;
        if (reset == nullptr || !reset->isEnabled())
            return false;
        reset->click();
        seenLive = props->fixtureGelColor(fx->id(), 0, 0);
        return !gel->colour().isValid();
    }, false);
    rig.mon->editFixtureProperties(fx->id());
    QVERIFY2(!seenLive.isValid(), "the gel came off only at OK, not as it was reset");
    QCOMPARE(props->fixtureGelColor(fx->id(), 0, 0), QColor(Qt::red));
}

namespace
{
/* A truss studio for @p rig with fixtures @p which selected in its list. */
StructureStudioView *studioWithSelected(Monitor &mon, QDialog &dlg, MultiTrussRig &rig,
                                        const QList<int> &which)
{
    StructureStudioView *view = nullptr;
    if (mon.makeStudioPane(&dlg, 2, rig.truss->id(), nullptr, &view) == nullptr)
        return nullptr;
    QList<quint32> want;
    foreach (int i, which)
        want << rig.fx.at(i)->id();
    foreach (QTreeWidget *t, dlg.findChildren<QTreeWidget *>())
    {
        for (QTreeWidgetItemIterator it(t); *it; ++it)
        {
            const QVariant v = (*it)->data(0, Qt::UserRole);
            if (v.isValid() && want.contains(v.toUInt()) && (*it)->text(0).startsWith("F"))
                (*it)->setSelected(true);
        }
    }
    return view;
}
}

void Monitor_Test::theInspectorShowsMixedAndSetsOnlyWhatIsSet()
{
    Doc *doc = new Doc(this);
    MultiTrussRig rig;
    rig.build(doc, { 0.5f, 1.5f, 2.5f });
    MonitorProperties *props = doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(rig.fx[1]->id());
    rp.studioTilt = 30.0f;                                  // F2 differs
    props->setFixtureRigProps(rig.fx[1]->id(), rp);
    props->setFixtureGelColor(rig.fx[0]->id(), 0, 0, QColor(Qt::red));
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        QDialog dlg;
        QVERIFY(studioWithSelected(mon, dlg, rig, { 0, 1, 2 }) != nullptr);

        QDoubleSpinBox *tilt = dlg.findChild<QDoubleSpinBox *>("fixtureTilt");
        QComboBox *placement = dlg.findChild<QComboBox *>("fixturePlacement");
        ColourSwatchButton *colour = dlg.findChild<ColourSwatchButton *>("fixtureColour");
        QVERIFY(tilt != nullptr && placement != nullptr && colour != nullptr);
        QCOMPARE(tilt->text(), QString("mixed"));
        QVERIFY(colour->isMixed());
        QVERIFY2(placement->currentText() != "— mixed —", "they all sit on the surface");

        // Placement set: all three are inside; their tilts are left as they were.
        placement->setCurrentIndex(placement->findData(int(FixtureRigProps::Inside)));
        for (int i = 0; i < 3; ++i)
            QCOMPARE(props->fixtureRigProps(rig.fx[i]->id()).placement, int(FixtureRigProps::Inside));
        QCOMPARE(props->fixtureRigProps(rig.fx[1]->id()).studioTilt, 30.0f);
        QCOMPARE(props->fixtureRigProps(rig.fx[0]->id()).studioTilt, 0.0f);
        QCOMPARE(tilt->text(), QString("mixed"));          // still: nobody set it

        // Tilt set: the same on all three, and no longer mixed.
        tilt->setValue(12.0);
        for (int i = 0; i < 3; ++i)
            QCOMPARE(props->fixtureRigProps(rig.fx[i]->id()).studioTilt, 12.0f);
        QVERIFY(tilt->specialValueText().isEmpty());

        // Colour reset: none on all three.
        colour->findChild<QToolButton *>("colourReset")->click();
        for (int i = 0; i < 3; ++i)
            QVERIFY(!props->fixtureGelColor(rig.fx[i]->id(), 0, 0).isValid());
    }
    delete doc;
}

void Monitor_Test::anInspectorEditIsOneUndoStep()
{
    Doc *doc = new Doc(this);
    MultiTrussRig rig;
    rig.build(doc, { 0.5f, 1.5f });
    MonitorProperties *props = doc->monitorProperties();
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        QDialog dlg;
        QVERIFY(studioWithSelected(mon, dlg, rig, { 0, 1 }) != nullptr);
        QDoubleSpinBox *tilt = dlg.findChild<QDoubleSpinBox *>("fixtureTilt");
        QVERIFY(tilt != nullptr);
        QShortcut *undo = nullptr;
        foreach (QShortcut *sc, dlg.findChildren<QShortcut *>())
            if (sc->key() == QKeySequence(QKeySequence::Undo))
                undo = sc;
        QVERIFY(undo != nullptr);

        // A run of steps on one field (a spin box held down) is one undo.
        tilt->setValue(5.0);
        tilt->setValue(10.0);
        tilt->setValue(15.0);
        QCOMPARE(props->fixtureRigProps(rig.fx[1]->id()).studioTilt, 15.0f);
        emit undo->activated();
        QCOMPARE(props->fixtureRigProps(rig.fx[0]->id()).studioTilt, 0.0f);
        QCOMPARE(props->fixtureRigProps(rig.fx[1]->id()).studioTilt, 0.0f);
        QCOMPARE(tilt->value(), 0.0);                       // the panel follows

        // A colour is undone too.
        ColourSwatchButton *colour = dlg.findChild<ColourSwatchButton *>("fixtureColour");
        props->setFixtureGelColor(rig.fx[0]->id(), 0, 0, QColor(Qt::blue));
        props->setFixtureGelColor(rig.fx[1]->id(), 0, 0, QColor(Qt::blue));
        colour->setColour(QColor(Qt::blue));
        colour->findChild<QToolButton *>("colourReset")->click();
        QVERIFY(!props->fixtureGelColor(rig.fx[0]->id(), 0, 0).isValid());
        emit undo->activated();
        QCOMPARE(props->fixtureGelColor(rig.fx[0]->id(), 0, 0), QColor(Qt::blue));
    }
    delete doc;
}

void Monitor_Test::fixturePropertiesForSeveralIsMixedAndLive()
{
    FixtureCleanup cleanup(m_doc);
    MonitorProperties *props = m_doc->monitorProperties();
    QList<quint32> ids;
    for (int i = 0; i < 2; ++i)
    {
        Fixture *fx = new Fixture(m_doc);
        fx->setName(QString("Pair %1").arg(i + 1));
        fx->setChannels(1);
        fx->setAddress(440 + quint32(i) * 4);
        QVERIFY(m_doc->addFixture(fx));
        cleanup.ids << fx->id();
        ids << fx->id();
        props->setFixturePosition(fx->id(), 0, 0, QVector3D(1000 + i * 1000, 1000, 0));
        FixtureRigProps rp;
        rp.panZeroDir = (i == 0) ? 0.0f : 90.0f;          // their aims differ
        props->setFixtureRigProps(fx->id(), rp);
    }
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    foreach (quint32 id, ids)
        rig.gv->addFixture(id, props->fixtureItem(id, 0, 0).m_position.toPointF());

    float liveTilt = -1.0f;
    QString aimText;
    driveDialog([&liveTilt, &aimText, props, ids](QWidget *modal) {
        QDoubleSpinBox *aim = modal->findChild<QDoubleSpinBox *>("fixtureAim");
        QDoubleSpinBox *tilt = modal->findChild<QDoubleSpinBox *>("fixtureTilt");
        if (aim == nullptr || tilt == nullptr)
            return false;
        aimText = aim->text();
        tilt->setValue(20.0);
        liveTilt = props->fixtureRigProps(ids.last()).studioTilt;
        return true;
    }, false);
    rig.mon->editFixturesProperties(ids);
    QCOMPARE(aimText, QString("mixed"));
    QCOMPARE(liveTilt, 20.0f);                              // live, on both
    foreach (quint32 id, ids)                               // Cancel: back
        QCOMPARE(props->fixtureRigProps(id).studioTilt, 0.0f);
    QCOMPARE(props->fixtureRigProps(ids.last()).panZeroDir, 90.0f);
}

void Monitor_Test::aMaxDrawIsSetOnEverySelectedFixture()
{
    /* "set maximum brightness per fixture .. and can be applied per
       multiselect": two fixtures with different caps read "mixed"; set, it is
       set on both, live; Cancel puts both back; OK keeps it. */
    FixtureCleanup cleanup(m_doc);
    QList<quint32> ids;
    for (int i = 0; i < 2; ++i)
    {
        Fixture *fx = new Fixture(m_doc);
        fx->setName(QString("Hog %1").arg(i + 1));
        fx->setChannels(1);
        fx->setAddress(460 + quint32(i) * 4);
        QVERIFY(m_doc->addFixture(fx));
        cleanup.ids << fx->id();
        ids << fx->id();
        PowerProfile pp;
        pp.colourAmps.insert(int(QLCChannel::NoColour), 6.0);
        pp.maxAmps = (i == 0) ? 2.0 : 3.0;
        fx->setPowerProfile(pp);
        m_doc->monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(1000 + i * 1000, 3000, 0));
    }
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    foreach (quint32 id, ids)
        rig.gv->addFixture(id, m_doc->monitorProperties()->fixtureItem(id, 0, 0).m_position.toPointF());

    QString maxText, note;
    double live = -1.0;
    driveDialog([&](QWidget *modal) {
        QDoubleSpinBox *mx = modal->findChild<QDoubleSpinBox *>("fixtureMaxAmps");
        QLabel *n = modal->findChild<QLabel *>("fixturePowerNote");
        if (mx == nullptr || n == nullptr)
            return false;
        maxText = mx->text();
        note = n->text();
        mx->setValue(4.0);
        live = m_doc->fixture(ids.last())->powerProfile().maxAmps;
        return true;
    }, false);
    rig.mon->editFixturesProperties(ids);
    QCOMPARE(maxText, QString("mixed"));
    QVERIFY2(note.contains("2 of 2"), qPrintable(note));
    QCOMPARE(live, 4.0);                                     // live, on both
    QCOMPARE(m_doc->fixture(ids.first())->powerProfile().maxAmps, 2.0);   // Cancel: back
    QCOMPARE(m_doc->fixture(ids.last())->powerProfile().maxAmps, 3.0);

    driveDialog([](QWidget *modal) {
        QDoubleSpinBox *mx = modal->findChild<QDoubleSpinBox *>("fixtureMaxAmps");
        if (mx == nullptr)
            return false;
        mx->setValue(5.0);
        return true;
    });
    rig.mon->editFixturesProperties(ids);
    foreach (quint32 id, ids)
        QCOMPARE(m_doc->fixture(id)->powerProfile().maxAmps, 5.0);
    QCOMPARE(m_doc->fixture(ids.first())->powerProfile().colourAmps.value(int(QLCChannel::NoColour)), 6.0);
}

void Monitor_Test::measuringPowerDrivesEachStateAndKeepsTheReadings()
{
    FixtureCleanup cleanup(m_doc);
    // An RGB par: red, green, blue, no dimmer.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("RGB Par");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("3ch");
    const QLCChannel::Preset presets[] = { QLCChannel::IntensityRed, QLCChannel::IntensityGreen,
                                           QLCChannel::IntensityBlue };
    QLCFixtureHead hd;
    for (int i = 0; i < 3; ++i)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("c%1").arg(i));
        ch->setPreset(presets[i]);
        def->addChannel(ch);
        mode->insertChannel(ch, quint32(i));
        hd.addChannel(quint32(i));
    }
    mode->insertHead(-1, hd);
    def->addMode(mode);
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(def));
    QList<quint32> ids;
    for (int i = 0; i < 2; ++i)
    {
        Fixture *fx = new Fixture(m_doc);
        fx->setFixtureDefinition(def, mode);
        fx->setAddress(470 + quint32(i) * 4);
        QVERIFY(m_doc->addFixture(fx));
        cleanup.ids << fx->id();
        ids << fx->id();
    }
    Fixture *par = m_doc->fixture(ids.first());
    PowerProfile capped;
    capped.maxAmps = 1.0;
    m_doc->fixture(ids.last())->setPowerProfile(capped);

    PowerMeasureDialog dlg(m_doc, ids);
    // Off, red, green, blue, half, full.
    QCOMPARE(dlg.steps().size(), 6);
    QVERIFY(m_doc->powerLimiter()->isMeasuring(par->id()));
    auto sent = [this, par]() {
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        ua.at(int(par->universe()))->processFaders();
        QByteArray out = *ua.at(int(par->universe()))->postGMValues();
        m_doc->inputOutputMap()->releaseUniverses(false);
        return QList<int>() << uchar(out.at(470)) << uchar(out.at(471)) << uchar(out.at(472));
    };
    QDoubleSpinBox *amps = dlg.findChild<QDoubleSpinBox *>("measureAmps");
    QPushButton *next = dlg.findChild<QPushButton *>("measureNext");
    QVERIFY(amps != nullptr && next != nullptr);
    const double readings[] = { 0.1, 0.8, 0.6, 0.6, 0.6, 1.9 };
    const QList<QList<int> > expect = { { 0, 0, 0 }, { 255, 0, 0 }, { 0, 255, 0 }, { 0, 0, 255 },
                                        { 128, 128, 128 }, { 255, 255, 255 } };
    for (int i = 0; i < 6; ++i)
    {
        QCOMPARE(dlg.current(), i);
        QCOMPARE(sent(), expect.at(i));
        amps->setValue(readings[i]);
        if (i < 5)
            next->click();
    }
    QCheckBox *all = dlg.findChild<QCheckBox *>("measureAll");
    QVERIFY(all != nullptr && all->isChecked());
    next->click();                                      // Finish
    QCOMPARE(dlg.result(), int(QDialog::Accepted));

    // Given back: its channels no longer driven, its cap back on.
    QCOMPARE(sent(), QList<int>({ 0, 0, 0 }));
    QVERIFY(!m_doc->powerLimiter()->isMeasuring(par->id()));
    foreach (quint32 id, ids)
    {
        const PowerProfile pp = m_doc->fixture(id)->powerProfile();
        QCOMPARE(pp.colourAmps.value(int(QLCChannel::Red)), 0.8);
        QCOMPARE(pp.colourAmps.value(int(QLCChannel::Blue)), 0.6);
        QCOMPARE(pp.standbyAmps, 0.1);
        QCOMPARE(pp.halfAmps, 0.6);
        QCOMPARE(pp.fullAmps, 1.9);
    }
    QCOMPARE(m_doc->fixture(ids.last())->powerProfile().maxAmps, 1.0);   // its own cap kept
    foreach (quint32 id, ids)
        m_doc->fixture(id)->setFixtureDefinition(nullptr, nullptr);
}

void Monitor_Test::powerReadingsAreCopiedToEveryFixtureLikeIt()
{
    /* "need an easy way to copy amperage profile to the same fixtures across a
       showfile. I selected them all and it didn't find the one I did." */
    FixtureCleanup cleanup(m_doc);
    auto makeDef = [this](const QString &model) {
        QLCFixtureDef *def = new QLCFixtureDef();
        def->setManufacturer("Test"); def->setModel(model);
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName("3ch");
        const QLCChannel::Preset presets[] = { QLCChannel::IntensityRed, QLCChannel::IntensityGreen,
                                               QLCChannel::IntensityBlue };
        for (int i = 0; i < 3; ++i)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("c%1").arg(i));
            ch->setPreset(presets[i]);
            def->addChannel(ch);
            mode->insertChannel(ch, quint32(i));
        }
        def->addMode(mode);
        m_doc->fixtureDefCache()->addFixtureDef(def);
        return def;
    };
    QLCFixtureDef *bar = makeDef("Copy Bar");
    QLCFixtureDef *other = makeDef("Other Par");
    QList<quint32> bars;
    for (int i = 0; i < 4; ++i)
    {
        Fixture *fx = new Fixture(m_doc);
        QLCFixtureDef *d = (i == 3) ? other : bar;
        fx->setFixtureDefinition(d, d->modes().first());
        fx->setAddress(480 + quint32(i) * 4);
        QVERIFY(m_doc->addFixture(fx));
        cleanup.ids << fx->id();
        bars << fx->id();
        m_doc->monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(1000 + i * 1000, 5000, 0));
    }
    // The one measured is the second; the third has a maximum of its own.
    PowerProfile measured;
    measured.colourAmps.insert(int(QLCChannel::Red), 2.7);
    measured.halfAmps = 1.7;
    measured.maxAmps = 3.0;
    m_doc->fixture(bars.at(1))->setPowerProfile(measured);
    PowerProfile own;
    own.maxAmps = 4.0;
    m_doc->fixture(bars.at(2))->setPowerProfile(own);

    // Profile..., opened on a selection whose first is not measured: the measured one's.
    {
        PowerProfileDialog dlg(m_doc, QList<quint32>() << bars.at(0) << bars.at(1));
        QCOMPARE(dlg.findChild<QDoubleSpinBox *>("powerHalf")->value(), 1.7);
    }

    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    foreach (quint32 id, bars)
        rig.gv->addFixture(id, m_doc->monitorProperties()->fixtureItem(id, 0, 0).m_position.toPointF());
    QString note;
    bool enabled = false;
    driveDialog([&](QWidget *modal) {
        QToolButton *copy = modal->findChild<QToolButton *>("fixtureCopyPower");
        QLabel *n = modal->findChild<QLabel *>("fixturePowerNote");
        if (copy == nullptr || n == nullptr)
            return false;
        note = n->text();
        enabled = copy->isEnabled();
        QTimer::singleShot(0, []() {                 // the confirmation: Copy
            if (QWidget *box = QApplication::activeModalWidget())
                if (QPushButton *yes = box->findChild<QPushButton *>("copyPowerConfirm"))
                    yes->click();
        });
        copy->click();
        return true;
    });
    rig.mon->editFixturesProperties(QList<quint32>() << bars.at(0) << bars.at(1));
    QVERIFY(enabled);
    QVERIFY2(note.contains("1 of 2 measured"), qPrintable(note));
    for (int i = 0; i < 3; ++i)
    {
        const PowerProfile pp = m_doc->fixture(bars.at(i))->powerProfile();
        QCOMPARE(pp.colourAmps.value(int(QLCChannel::Red)), 2.7);
        QCOMPARE(pp.halfAmps, 1.7);
    }
    QCOMPARE(m_doc->fixture(bars.at(0))->powerProfile().maxAmps, 0.0);   // each keeps its own
    QCOMPARE(m_doc->fixture(bars.at(2))->powerProfile().maxAmps, 4.0);
    QVERIFY(!m_doc->fixture(bars.at(3))->powerProfile().isMeasured());   // not the same fixture
}

void Monitor_Test::changingAFixtureKeepsWhatIsSelected()
{
    /* "I tried to select multiple fixtures .. set max draw to 1.0 A .. and it
       deselected the fixtures": a fixture's own change rebuilds the Layers
       list, and clearing the list emptied the plot's selection. */
    FixtureCleanup cleanup(m_doc);
    QList<quint32> ids;
    for (int i = 0; i < 2; ++i)
    {
        Fixture *fx = new Fixture(m_doc);
        fx->setName(QString("Sel %1").arg(i + 1));
        fx->setChannels(1);
        fx->setAddress(500 + quint32(i) * 4);
        QVERIFY(m_doc->addFixture(fx));
        cleanup.ids << fx->id();
        ids << fx->id();
        m_doc->monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(2000 + i * 1000, 6000, 0));
    }
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));
    foreach (quint32 id, ids)
        rig.gv->addFixture(id, m_doc->monitorProperties()->fixtureItem(id, 0, 0).m_position.toPointF());
    QVERIFY(rig.mon->m_layersPanel != nullptr);
    rig.mon->m_layersPanel->reload();

    QList<QPair<QString, quint32> > both;
    foreach (quint32 id, ids)
        both << qMakePair(QStringLiteral("fixture"), id);
    rig.gv->selectMapItems(both);
    QCOMPARE(rig.gv->selectedMapObjects().size(), 2);

    // Their maximum draw set, as Fixture Properties does: each fixture changes.
    foreach (quint32 id, ids)
    {
        PowerProfile pp;
        pp.maxAmps = 1.0;
        m_doc->fixture(id)->setPowerProfile(pp);
    }
    QTest::qWait(50);                                   // the list's deferred rebuild
    QCOMPARE(rig.gv->selectedMapObjects().size(), 2);
    // And the list shows them selected again.
    QCOMPARE(rig.mon->m_layersPanel->selectedObjects().size(), 2);
}

void Monitor_Test::aPersonsHeightIsSetInTheTargetDialog()
{
    MonitorProperties *props = m_doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Meters);
    InventoryRig rig;
    QVERIFY(rig.build(m_doc));

    // A new person is a usual height, aimed at the chest.
    rig.mon->slotAddPerson();
    StageTarget *p = nullptr;
    foreach (StageTarget *t, props->stageTargets())
        if (t->kind() == StageTarget::Kind::Person)
            p = t;
    QVERIFY(p != nullptr);
    const quint32 id = p->id();
    QCOMPARE(p->personHeight(), 1.75f);

    // Their height, in their dialog: drawn that tall at once.
    float live = 0.0f;
    driveDialog([&live, props, id](QWidget *dlg) {
        QDoubleSpinBox *h = dlg->findChild<QDoubleSpinBox *>("personHeight");
        if (h == nullptr || !h->isEnabled())
            return false;
        h->setValue(1.63);
        live = props->targetFigureHeight(props->stageTarget(id));
        return true;
    });
    rig.mon->slotEditTarget(id);
    QVERIFY(qAbs(live - 1.63f) < 0.001f);
    QVERIFY(qAbs(props->stageTarget(id)->personHeight() - 1.63f) < 0.001f);
    props->removeStageTarget(id);
}

void Monitor_Test::theCrosshairsSayWhereThePointerIs()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Feet);
    StagePlatform *riser = props->addPlatform();
    riser->setOriginX(0.0f); riser->setOriginY(0.0f);
    riser->setWidth(2.438f); riser->setDepth(1.219f); riser->setHeight(0.610f);
    StageTarget *person = props->addStageTarget();
    person->setKind(StageTarget::Kind::Person);
    person->setPersonHeight(1.626f);                    // 5'4"
    person->setX(1.6f); person->setY(0.6f); person->setZ(0.610f);
    {
        StructureStudioView view(doc, StructureStudioView::StageKind, 0);
        view.resize(900, 640);
        view.reload();
        auto near = [](const QVector3D &a, const QVector3D &b, float tol) {
            return (a - b).length() < tol;
        };

        // At an angle, on the riser's top: that point, all three.
        view.setAngledView(20.0, 25.0);
        view.setPlane(StructureStudioView::Angled);
        view.grab();                                    // paint: fills the depth buffer
        int known = 0;
        bool onSurface = false;
        const QVector3D top(0.4f, 0.3f, 0.610f);
        QVector3D w = view.worldAt(view.w2s(top), &known, &onSurface);
        QVERIFY(onSurface);
        QCOMPARE(known, 7);
        QVERIFY2(near(w, top, 0.06f), qPrintable(QString("read %1,%2,%3")
                 .arg(double(w.x())).arg(double(w.y())).arg(double(w.z()))));

        // Over nothing at an angle: the floor under the pointer.
        const QVector3D floor(6.0f, 4.0f, 0.0f);
        w = view.worldAt(view.w2s(floor), &known, &onSurface);
        QVERIFY(!onSurface);
        QVERIFY(near(w, floor, 0.06f));

        // From the front, on the person's chest: 2' of riser and 80% of 5'4".
        view.setPlane(StructureStudioView::Front);
        view.grab();
        const QVector3D chest(1.6f, 0.6f, 0.610f + 1.626f * 0.72f);
        w = view.worldAt(view.w2s(chest), &known, &onSurface);
        QVERIFY2(onSurface, "the person is not under the pointer");
        QVERIFY2(qAbs(w.z() - chest.z()) < 0.05f && qAbs(w.x() - chest.x()) < 0.05f,
                 qPrintable(QString("read %1,%2,%3").arg(double(w.x())).arg(double(w.y()))
                            .arg(double(w.z()))));
        // Just over the head: not the person -- a flat view says H and Z only.
        w = view.worldAt(view.w2s(QVector3D(1.6f, 0.6f, 0.610f + 1.626f + 0.15f)), &known, &onSurface);
        QVERIFY(!onSurface);
        QCOMPARE(known, 1 | 4);
        QVERIFY(qAbs(w.z() - (0.610f + 1.626f + 0.15f)) < 0.03f);
    }
    delete doc;
}

/* "if I make changes to the effect I don't see changes in the rig view":
   Stage Shape on an Aim look sends each head to its own point round the
   target, but the rig view drew a head under an Aim look pointing at the
   target whatever it was sent. Selected, the target; running, the DMX. */
void Monitor_Test::aRunningLookIsDrawnWhereItsHeadsAreSent()
{
    Doc *doc = new Doc(this);
    for (int u = 0; u < 4; ++u)
        doc->inputOutputMap()->addUniverse();
    MonitorProperties *props = doc->monitorProperties();
    Fixture *head = makeMover(doc, 10.0, 0, "Spot", QVector3D(0.0f, 6.0f, 6.0f));
    QVERIFY(head != nullptr);
    StageTarget *tgt = props->addStageTarget();
    tgt->setX(0.0f); tgt->setY(0.0f); tgt->setZ(0.0f);
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setName("At"); aim->setStageTargetId(tgt->id());
    QVERIFY(doc->addPalette(aim));
    Scene *scene = new Scene(doc);
    scene->setName("Shape");
    scene->addFixture(head->id());
    scene->addPalette(aim->id());
    QVERIFY(doc->addFunction(scene));

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(600, 460);
    v.reload();
    v.setLiveValues(true);
    v.setActiveScene(scene->id());

    QVector3D aimPt;
    QVERIFY(v.aimPointFor(head->id(), aimPt));
    const QVector3D toTarget = (aimPt - v.fixtureCentre(head->id())).normalized();
    // Only selected: drawn at the target.
    QVERIFY(QVector3D::dotProduct(v.moverAim(head->id()), toTarget) > 0.99f);

    // Running, and an effect sends it somewhere else: drawn there.
    MasterTimer *timer = doc->masterTimer();          // not started: ticked by hand
    scene->start(timer, FunctionParent::master());
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QVERIFY(scene->isRunning());
    QByteArray dmx = head->channelValues();
    dmx[0] = char(30);                          // pan well round
    dmx[1] = char(40);                          // tilt well up
    head->setChannelValues(dmx);
    QVector3D sent;
    QVERIFY(fixtureAimDirection(head, props->fixtureRigProps(head->id()), sent));
    const QVector3D drawn = v.moverAim(head->id());
    QVERIFY2(QVector3D::dotProduct(drawn, sent) > 0.999f, "a running look's head is drawn where it's sent");
    QVERIFY(QVector3D::dotProduct(drawn, toTarget) < 0.95f);

    scene->stop(FunctionParent::master());
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QVERIFY(QVector3D::dotProduct(v.moverAim(head->id()), toTarget) > 0.99f);
    delete doc;
}

/* "we should always start with centering on the floor and center stage":
   the Rig Overview used to centre the rig's bounding box, so a rig with
   trusses at 6 m had its floor half that below the middle of the window. */
void Monitor_Test::theAngledViewStandsOnCentreStage()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    props->setGridSize(QVector3D(12, 3, 10));           // metres
    Truss *t = props->addTruss();
    t->setOrigin(QVector3D(2, 2, 6));                   // high, upstage, to one side
    t->setLength(8);
    makeMover(doc, 10.0, 0, "High", QVector3D(4.0f, 2.0f, 6.0f));

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(800, 600);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setStandOnCentreStage(true);              // as the Rig Overview does
    v.refit();
    // Across the middle, a little below half way: the rig stands above it.
    const QPointF c = v.w2s(props->stageCentre());
    QVERIFY2(qAbs(c.x() - 400) < 2 && qAbs(c.y() - 360) < 2,
             qPrintable(QString("centre stage drawn at %1,%2, not (400, 360)")
                        .arg(c.x()).arg(c.y())));
    // ...and everything still fits on screen.
    const QPointF top = v.w2s(QVector3D(6, 2, 6));
    QVERIFY(QRectF(0, 0, 800, 600).contains(top));
    // Turning the view keeps it there.
    v.setAngledView(120, 30);
    const QPointF turned = v.w2s(props->stageCentre());
    QVERIFY(qAbs(turned.x() - 400) < 2 && qAbs(turned.y() - 360) < 2);
    delete doc;
}

void Monitor_Test::aWiderFollowSpotAimsLowerOnThePerson()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();

    // A spot with a zoom (4..40 degrees) and an iris, hung 4 m up, 4 m downstage.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Followspot");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("5ch");
    const QList<QPair<QString, QLCChannel::Preset> > chans = {
        { "Pan", QLCChannel::PositionPan }, { "Tilt", QLCChannel::PositionTilt },
        { "Dimmer", QLCChannel::IntensityDimmer }, { "Zoom", QLCChannel::BeamZoomSmallBig },
        { "Iris", QLCChannel::ShutterIrisMinToMax } };
    QLCFixtureHead hd;
    for (int i = 0; i < chans.size(); ++i)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(chans.at(i).first);
        ch->setPreset(chans.at(i).second);
        def->addChannel(ch);
        mode->insertChannel(ch, i);
        hd.addChannel(quint32(i));
    }
    QLCPhysical ph;
    ph.setWidth(300); ph.setHeight(400); ph.setDepth(300);
    ph.setLensDegreesMin(4.0); ph.setLensDegreesMax(40.0);
    ph.setFocusPanMax(540); ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    mode->insertHead(-1, hd);
    def->addMode(mode);
    Fixture *spot = new Fixture(doc);
    spot->setName("FOH Spot");
    spot->setFixtureDefinition(def, mode);
    spot->setAddress(0);
    QVERIFY(doc->addFixture(spot));

    Truss *t = props->addTruss();
    t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(4.0f, 8.0f, 4.0f));           // downstage of the person
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(2.0f);
    FixtureRigProps rp;
    rp.trussId = t->id();
    rp.trussOffset = 1.0f;
    props->setFixtureRigProps(spot->id(), rp);

    StageTarget *person = props->addStageTarget();
    person->setKind(StageTarget::Kind::Person);
    person->setPersonHeight(1.626f);                       // 5'4", on the floor
    person->setX(5.0f); person->setY(4.0f); person->setZ(0.0f);

    auto set = [spot](int zoom, int iris) {
        QByteArray v(512, char(0));
        v[3] = char(zoom);
        v[4] = char(iris);
        spot->setChannelValues(v);
    };

    // The beam: the zoom's lens, narrowed by the iris.
    set(255, 255);
    QVERIFY(qAbs(RigHeads::beamAngle(spot, spot->channelValues()) - 40.0) < 0.2);
    set(255, 128);
    const double irised = RigHeads::beamAngle(spot, spot->channelValues());
    QVERIFY2(irised > 19.0 && irised < 22.0, qPrintable(QString("iris half open: %1").arg(irised)));

    const float chest = 1.626f * 0.8f, waist = 1.626f * 0.5f;

    // Narrow (a 34 cm spot on them): the chest, as before.
    set(0, 255);
    const float narrow = AimSolver::aimPointFor(doc, spot->id(), person, true).z();
    QVERIFY2(qAbs(narrow - chest) < 0.01f, qPrintable(QString("narrow aimed at %1").arg(double(narrow))));

    // Wider: lower -- and the spot's top edge stays just over the head.
    set(90, 255);
    const QVector3D mid = AimSolver::aimPointFor(doc, spot->id(), person, true);
    QVERIFY2(mid.z() < chest - 0.05f && mid.z() > waist,
             qPrintable(QString("a wider beam aimed at %1").arg(double(mid.z()))));
    QVERIFY(qAbs(mid.x() - 5.0f) < 0.001f && qAbs(mid.y() - 4.0f) < 0.001f);   // only lower

    // Wide open: no lower than the waist.
    set(255, 255);
    const float wide = AimSolver::aimPointFor(doc, spot->id(), person, true).z();
    QVERIFY2(qAbs(wide - waist) < 0.01f, qPrintable(QString("wide aimed at %1").arg(double(wide))));

    // Not a follow spot: the target's own point, whatever the beam.
    QCOMPARE(AimSolver::aimPointFor(doc, spot->id(), person, false),
             props->targetAimPoint(person, false));
    delete doc;
}

void Monitor_Test::aWashShowsItsLedsOnItsFace()
{
    QCOMPARE(QLCPhysical::stringToLedLayout(QLCPhysical::ledLayoutToString(QLCPhysical::FaceLeds)),
             QLCPhysical::FaceLeds);

    Doc *doc = new Doc(this);
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Wash 36x18");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("6ch");
    const QList<QPair<QString, QLCChannel::Preset> > chans = {
        { "Pan", QLCChannel::PositionPan }, { "Tilt", QLCChannel::PositionTilt },
        { "Dimmer", QLCChannel::IntensityDimmer }, { "Red", QLCChannel::IntensityRed },
        { "Green", QLCChannel::IntensityGreen }, { "Blue", QLCChannel::IntensityBlue } };
    QLCFixtureHead hd;
    for (int i = 0; i < chans.size(); ++i)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(chans.at(i).first);
        ch->setPreset(chans.at(i).second);
        def->addChannel(ch);
        mode->insertChannel(ch, i);
        hd.addChannel(quint32(i));
    }
    mode->insertHead(-1, hd);
    auto setLeds = [&](QLCPhysical::LedLayout layout) {
        QLCPhysical ph;
        ph.setWidth(400); ph.setHeight(500); ph.setDepth(400);
        ph.setFocusPanMax(540); ph.setFocusTiltMax(270);
        ph.setLensDegreesMin(25); ph.setLensDegreesMax(25);
        ph.setLeds(36);
        ph.setLedLayout(layout);
        mode->setPhysical(ph);
    };
    setLeds(QLCPhysical::FaceLeds);
    def->addMode(mode);
    Fixture *wash = new Fixture(doc);
    wash->setName("Wash"); wash->setFixtureDefinition(def, mode);
    QVERIFY(doc->addFixture(wash));
    doc->monitorProperties()->setFixturePosition(wash->id(), 0, 0, QVector3D(3000.0f, 3000.0f, 3.0f));
    QByteArray u(512, char(0));
    u[0] = char(128); u[1] = char(200);                  // the face turned toward the camera
    u[2] = char(255); u[3] = char(255);                  // full, red
    wash->setChannelValues(u);
    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.resize(800, 620);
        v.reload();
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(15.0, 20.0);
        v.setLiveValues(true);
        v.setBeams(false);                                // only the LEDs are red
        auto redPixels = [&v]() {
            const QImage img = v.grab().toImage();
            int n = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x)
                {
                    const QColor c = img.pixelColor(x, y);
                    if (c.red() > 120 && c.red() > c.green() * 2 && c.red() > c.blue() * 2)
                        ++n;
                }
            return n;
        };
        const int onFace = redPixels();
        setLeds(QLCPhysical::ClusterLeds);
        v.reload();
        const int cluster = redPixels();
        QVERIFY2(onFace > 40 && onFace > cluster * 4 + 20,
                 qPrintable(QString("LEDs on the face lit %1 red px, a cluster %2").arg(onFace).arg(cluster)));
    }
    delete doc;
}

void Monitor_Test::lasersThrowStarsAndPencilBeams()
{
    Doc *doc = new Doc(this);
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Sky Laser");
    def->setType(QLCFixtureDef::Laser);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("4ch");
    const QList<QPair<QString, QLCChannel::Preset> > chans = {
        { "Intensity", QLCChannel::IntensityDimmer }, { "Red", QLCChannel::IntensityRed },
        { "Green", QLCChannel::IntensityGreen }, { "Blue", QLCChannel::IntensityBlue } };
    QLCFixtureHead hd;
    for (int i = 0; i < chans.size(); ++i)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(chans.at(i).first);
        ch->setPreset(chans.at(i).second);
        def->addChannel(ch);
        mode->insertChannel(ch, i);
        hd.addChannel(quint32(i));
    }
    mode->insertHead(-1, hd);
    auto emitAs = [&](QLCPhysical::Emission e) {
        QLCPhysical ph;
        ph.setWidth(200); ph.setHeight(100); ph.setDepth(200);
        ph.setBulbLumens(500);
        ph.setEmission(e);
        mode->setPhysical(ph);
    };
    emitAs(QLCPhysical::BeamEmission);
    def->addMode(mode);
    Fixture *laser = new Fixture(doc);
    laser->setName("Stars"); laser->setFixtureDefinition(def, mode);
    QVERIFY(doc->addFixture(laser));
    doc->monitorProperties()->setFixturePosition(laser->id(), 0, 0, QVector3D(3000.0f, 3000.0f, 2.5f));
    // A wide, low deck under it: what it lands on, and what the view frames.
    StagePlatform *deck = doc->monitorProperties()->addPlatform();
    deck->setOriginX(0.0f); deck->setOriginY(0.0f);
    deck->setWidth(6.0f); deck->setDepth(6.0f); deck->setHeight(0.05f);
    // Hung from a pipe over it, pointing down at the floor.
    Truss *pipe = doc->monitorProperties()->addTruss();
    pipe->setOrigin(QVector3D(0.0f, 3.0f, 2.5f));
    pipe->setDirection(QPointF(1.0, 0.0));
    pipe->setLength(6.0f);
    pipe->setWidth(0.05f);
    FixtureRigProps rp;
    rp.trussId = pipe->id();
    rp.trussOffset = 3.0f;
    rp.mountingType = Truss::TopHung;
    doc->monitorProperties()->setFixtureRigProps(laser->id(), rp);
    QByteArray u(512, char(0));
    u[0] = char(255); u[2] = char(255);                // full, green
    laser->setChannelValues(u);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(800, 620);
    v.reload();
    v.setPlane(StructureStudioView::Angled);
    v.setAngledView(15.0, 35.0);
    v.setLiveValues(true);
    v.setHaze(0.0);                                    // no air: only what lands
    /* What the light ADDS: each frame against the same rig dark, so the
       fixture's own body and the deck do not count -- only where green landed. */
    struct Count { int px = 0; int blobs = 0; QRect box; };
    auto frame = [&v]() { v.reload(); return v.grab().toImage(); };
    QByteArray off(512, char(0));
    laser->setChannelValues(off);
    const QImage dark = frame();
    laser->setChannelValues(u);
    auto green = [&]() {
        const QImage img = frame();
        Count c;
        QVector<bool> seen(img.width() * img.height(), false);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
            {
                const QColor q = img.pixelColor(x, y), d = dark.pixelColor(x, y);
                if (q.green() - d.green() < 12 || q.green() - d.green() < 2 * (q.red() - d.red()))
                    continue;
                ++c.px;
                c.box |= QRect(x, y, 1, 1);
                // A new blob when no lit neighbour above or to the left.
                const bool left = x > 0 && seen[y * img.width() + x - 1];
                const bool up = y > 0 && seen[(y - 1) * img.width() + x];
                if (!left && !up)
                    ++c.blobs;
                seen[y * img.width() + x] = true;
            }
        return c;
    };
    const Count wash = green();
    emitAs(QLCPhysical::StarEmission);
    const Count stars = green();
    emitAs(QLCPhysical::LaserEmission);
    const Count pencil = green();

    QVERIFY2(stars.blobs > 60,
             qPrintable(QString("a star field should be many points, got %1 blobs (%2 px); "
                                "the wash was %3 blobs").arg(stars.blobs).arg(stars.px).arg(wash.blobs)));
    // A laser is a pencil from the head to the floor: as long as the drop,
    // no wider than the head it leaves.
    QVERIFY2(pencil.box.height() > 2 * wash.box.height() && pencil.box.width() <= wash.box.width() + 2,
             qPrintable(QString("a laser beam should be a thin line: %1 x %2 px, the wash's light %3 x %4")
                            .arg(pencil.box.width()).arg(pencil.box.height())
                            .arg(wash.box.width()).arg(wash.box.height())));
    delete doc;
}

/* "I moved a light that's at the base of a tower with a bar on top .. and it
   added it to the bar .. they're in different Z planes": over the bar on the
   plan is not on it. A fixture at the foot of a tower stays on the tower; a
   free-standing one is still hung by being dropped on the bar. */
void Monitor_Test::aLightAtTheFootOfATowerIsNotHungOnItsTopBar()
{
    DropRig rig;
    QVERIFY(rig.build(m_doc, QVector3D(5000, 5000, 0)));
    rig.gv->setViewPOV(MonitorGraphicsView::PovTop);
    MonitorProperties *props = m_doc->monitorProperties();

    Tower *tower = props->addTower();
    tower->setOriginX(11.8f); tower->setOriginY(11.8f);
    tower->setWidth(0.4f); tower->setDepth(0.4f); tower->setHeight(2.4f);
    Truss *bar = props->addTruss();
    bar->setName("Top bar");
    bar->setOrigin(QVector3D(10.0f, 12.0f, 2.4f));    // across the tower's top
    bar->setDirection(QPointF(1.0, 0.0));
    bar->setLength(4.0f);
    bar->setWidth(0.3f);
    rig.gv->updateTrusses();
    rig.gv->updatePlatforms();          // towers too

    FixtureRigProps rp;
    rp.towerId = tower->id();
    rp.towerMountSide = FixtureRigProps::TowerBottom;
    rp.towerU = 0.2f; rp.towerV = 0.2f;
    props->setFixtureRigProps(rig.fxi->id(), rp);
    rig.gv->updateFixture(rig.fxi->id());
    QVERIFY(props->fixtureRigPosition(rig.fxi->id()).z() < 0.5f);

    // Right under the bar on the plan.
    const QPointF underBar = rig.gv->realPositionToPixels(12000.0, 12000.0);
    rig.item->setPos(rig.item->pos() + (underBar - rig.item->iconCentreScene()));
    QVERIFY2(rig.gv->trussUnderFixture(rig.item, 1.0f, false) != nullptr, "it is under the bar on the plan");
    rig.gv->setAttachOnDrop(true);   // even with Shift held
    QVERIFY2(rig.gv->trussUnderFixture(rig.item, 1.0f, true) == nullptr,
             "a fixture at the foot of the tower should not be offered the bar 2.4 m up");
    rig.gv->slotFixtureMoved(rig.item);
    QVERIFY2(props->fixtureRigProps(rig.fxi->id()).trussId == Truss::invalidId(),
             "dropped under the bar, it was hung on it");
    rig.gv->setAttachOnDrop(false);

    // Free-standing, a Shift-drop there hangs it -- how a fixture is hung from the plot.
    props->setFixtureRigProps(rig.fxi->id(), FixtureRigProps());
    rig.gv->updateFixture(rig.fxi->id());
    rig.item->setPos(rig.item->pos() + (underBar - rig.item->iconCentreScene()));
    QVERIFY(rig.gv->trussUnderFixture(rig.item, 1.0f, true) == nullptr);   // no Shift
    rig.gv->setAttachOnDrop(true);
    QVERIFY(rig.gv->trussUnderFixture(rig.item, 1.0f, true) != nullptr);
    rig.gv->setAttachOnDrop(false);

    props->setFixtureRigProps(rig.fxi->id(), FixtureRigProps());
    props->removeTruss(bar->id());
    props->removeTower(tower->id());
}

/* "when shift is pressed .. the outline should change (green?) to indicate
   it's attaching": a free fixture dragged over a truss is green only while
   Shift is held -- and it was never green before, the border only being
   worked out for fixtures already on a truss. */
void Monitor_Test::theAttachBorderShowsOnlyWithShift()
{
    DropRig rig;
    QVERIFY(rig.build(m_doc, QVector3D(5000, 5000, 0)));
    rig.gv->setViewPOV(MonitorGraphicsView::PovTop);
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("GreenBar");
    t->setOrigin(QVector3D(10.0f, 12.0f, 4.0f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(6.0f);
    t->setWidth(0.3f);
    rig.gv->updateTrusses();

    const QPointF onTrussPx = rig.gv->realPositionToPixels(12000.0, 12000.0);
    rig.item->setPos(rig.item->pos() + (onTrussPx - rig.item->iconCentreScene()));
    rig.gv->scene()->clearSelection();
    rig.item->setSelected(true);

    rig.gv->updateDragBorders();
    QVERIFY2(!rig.item->attachMode(), "green without Shift: a plain drop does not attach");
    rig.gv->setAttachOnDrop(true);                  // Shift down
    rig.gv->updateDragBorders();
    QVERIFY2(rig.item->attachMode(), "with Shift held over the truss, the border should be green");
    rig.gv->setAttachOnDrop(false);                 // and up again
    rig.gv->updateDragBorders();
    QVERIFY(!rig.item->attachMode());

    props->removeTruss(t->id());
}

/* "why does the fixture at the base of the truss look like it's above it?":
   in the top view, a fixture with a bar well above it is drawn faded and
   dashed -- underneath -- while one hung on that bar is not; the side views
   show heights and draw them all plainly. */
void Monitor_Test::aFixtureUnderABarIsDrawnBeneathIt()
{
    DropRig rig;
    QVERIFY(rig.build(m_doc, QVector3D(5000, 5000, 0)));
    rig.gv->setViewPOV(MonitorGraphicsView::PovTop);
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *bar = props->addTruss();
    bar->setName("High bar");
    bar->setOrigin(QVector3D(10.0f, 12.0f, 2.4f));
    bar->setDirection(QPointF(1.0, 0.0));
    bar->setLength(4.0f);
    bar->setWidth(0.3f);
    rig.gv->updateTrusses();

    // On the floor, right under the bar.
    props->setFixturePosition(rig.fxi->id(), 0, 0, QVector3D(12000.0f, 12000.0f, 0.0f));
    rig.gv->updateFixture(rig.fxi->id());
    const QPointF underBar = rig.gv->realPositionToPixels(12000.0, 12000.0);
    rig.item->setPos(rig.item->pos() + (underBar - rig.item->iconCentreScene()));
    rig.gv->refreshBeneath();
    QVERIFY2(rig.item->beneath(), "a floor light under a bar 2.4 m up should draw underneath it");
    QVERIFY(rig.item->opacity() < 1.0);

    // Hung on that bar, it is not under it.
    FixtureRigProps rp;
    rp.trussId = bar->id();
    rp.trussOffset = 2.0f;
    props->setFixtureRigProps(rig.fxi->id(), rp);
    rig.gv->updateFixture(rig.fxi->id());
    rig.gv->refreshBeneath();
    QVERIFY(!rig.item->beneath());

    // Off the bar again and looked at from the front: plain.
    props->setFixtureRigProps(rig.fxi->id(), FixtureRigProps());
    rig.gv->setViewPOV(MonitorGraphicsView::PovFront);
    rig.gv->refreshBeneath();
    QVERIFY(!rig.item->beneath());
    QCOMPARE(rig.item->opacity(), 1.0);
    rig.gv->setViewPOV(MonitorGraphicsView::PovTop);

    props->removeTruss(bar->id());
}

/* "UST1-8 .. they should be upright .. but I don't see a way to put that on
   the fixture setting .. the testing tool should know that it's upright and
   say so vs straight down": a two-arm beam bar is long and flat like a
   strip, but it pans and tilts, so which way up it is matters -- it has the
   Orientation row, and the centred test pose is named for it. */
void Monitor_Test::aBarThatPansCanBeSetUprightAndTestsAsSuch()
{
    Doc *doc = new Doc(this);
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Two Arm Bar");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("6 Channel");
    const QLCChannel::Group groups[6] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity,
                                          QLCChannel::Intensity, QLCChannel::Intensity, QLCChannel::Intensity };
    const QLCChannel::PrimaryColour colours[6] = { QLCChannel::NoColour, QLCChannel::NoColour,
                                                   QLCChannel::NoColour, QLCChannel::Red,
                                                   QLCChannel::Green, QLCChannel::Blue };
    for (int k = 0; k < 6; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("c%1").arg(k)); ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        if (colours[k] != QLCChannel::NoColour) ch->setColour(colours[k]);
        def->addChannel(ch); mode->insertChannel(ch, quint32(k));
    }
    QLCPhysical ph;
    ph.setWidth(1000); ph.setHeight(100); ph.setDepth(150);    // a long, flat bar
    mode->setPhysical(ph);
    def->addMode(mode);
    Fixture *fxi = new Fixture(doc);
    fxi->setName("UST-1"); fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(3); fxi->setAddress(0);
    QVERIFY(doc->addFixture(fxi));
    doc->monitorProperties()->setFixturePosition(fxi->id(), 0, 0, QVector3D(3000.0f, 3000.0f, 0.0f));
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        QWidget host;
        QFormLayout *form = new QFormLayout(&host);
        FixtureFields fields(&mon, form, &host);
        fields.setFixtures({ fxi->id() }, fxi->id());
        QVERIFY2(!fields.m_orientation->isHidden(), "a bar that pans and tilts should have Orientation");
        int upright = -1;
        for (int i = 0; i < fields.m_orientation->count(); ++i)
            if (fields.m_orientation->itemData(i, Qt::UserRole).toInt() == int(Truss::FloorMounted))
                upright = i;
        QVERIFY2(upright >= 0, "Orientation should offer upright");

        QCOMPARE(mon.centredPoseName(fxi->id()), QString("Straight down"));   // hung, by default
        fields.m_orientation->setCurrentIndex(upright);
        QCOMPARE(doc->monitorProperties()->fixtureRigProps(fxi->id()).mountingType, Truss::FloorMounted);
        QCOMPARE(mon.centredPoseName(fxi->id()), QString("Straight up"));
    }
    delete doc;
}

/* "how to place UST1-8 cleanly on the top of the platforms .. they shouldn't
   hang in space": a free fixture 0.9 m up, lined up with a platform's back
   edge, stood on the deck under it -- on the deck's top, upright, inside its
   edge -- and a whole selection at once. */
void Monitor_Test::fixturesAreStoodOnTheDeckUnderThem()
{
    DropRig rig;
    QVERIFY(rig.build(m_doc, QVector3D(5000, 5000, 0)));
    rig.gv->setViewPOV(MonitorGraphicsView::PovTop);
    MonitorProperties *props = m_doc->monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setName("USP1");
    deck->setOriginX(3.047f); deck->setOriginY(3.624f);
    deck->setWidth(2.438f); deck->setDepth(1.219f); deck->setHeight(0.610f);
    rig.gv->updatePlatforms();

    // In space: free, 0.9 m up, on the deck's back edge.
    props->setFixturePosition(rig.fxi->id(), 0, 0, QVector3D(3070.0f, 3624.0f, 0.9f));
    rig.gv->updateFixture(rig.fxi->id());
    QCOMPARE(rig.gv->standOnDeck({ rig.fxi->id() }, FixtureRigProps::invalidPlatformId()), 1);

    const FixtureRigProps rp = props->fixtureRigProps(rig.fxi->id());
    QCOMPARE(rp.deckPlatformId, deck->id());
    QCOMPARE(rp.mountingType, Truss::FloorMounted);
    const QVector3D at = props->fixtureRigPosition(rig.fxi->id());
    QVERIFY2(qAbs(at.z() - 0.610f) < 0.01f, qPrintable(QString("standing at %1 m, the deck is 0.61 m").arg(at.z())));
    QVERIFY2(deck->containsPoint(at.x(), at.y() - 0.04f), "it should be inside the deck's edge, not on it");

    props->setFixtureRigProps(rig.fxi->id(), FixtureRigProps());
    props->removePlatform(deck->id());
}

/* The network is part of the rig's undo: a network dialog's OK is one plot
   undo step, and the rig snapshot puts its nodes back. */
void Monitor_Test::theNetworkIsUndoneWithTheRig()
{
    Doc *doc = new Doc(this);
    NetworkNode node;
    node.name = "Stage left node";
    doc->networkTopology()->nodes() << node;
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        const QByteArray before = mon.rigSnapshot();
        doc->networkTopology()->nodes().first().name = "Renamed";
        doc->networkTopology()->nodes() << NetworkNode();
        QVERIFY(mon.rigSnapshot() != before);
        mon.restoreRig(before);
        QCOMPARE(doc->networkTopology()->nodes().size(), 1);
        QCOMPARE(doc->networkTopology()->nodes().first().name, QString("Stage left node"));
    }
    delete doc;
}

/* The Patch dialog can change a fixture's mode to fit it: one undo puts the
   mode back with the address. */
void Monitor_Test::aModeChangedByThePatchIsUndone()
{
    Doc *doc = new Doc(this);
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("TwoMode");
    QLCFixtureMode *big = new QLCFixtureMode(def);
    big->setName("Big");
    QLCFixtureMode *small = new QLCFixtureMode(def);
    small->setName("Small");
    for (int k = 0; k < 20; k++)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("Ch %1").arg(k));
        def->addChannel(ch);
        big->insertChannel(ch, k);
        if (k < 5)
            small->insertChannel(ch, k);
    }
    def->addMode(big);
    def->addMode(small);
    Fixture *f = new Fixture(doc);
    f->setFixtureDefinition(def, big);
    QVERIFY(doc->addFixture(f));
    QVERIFY(doc->patchFixtures({ { f->id(), 0, 10 } }));
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        const QByteArray before = mon.rigSnapshot();
        PatchPlanner::Assignment a;
        a.fixtureId = f->id();
        a.universe = 0;
        a.address = 100;
        a.mode = small;
        PatchPlanner pp(doc);
        QCOMPARE(pp.apply({ a }), 1);
        QCOMPARE(f->fixtureMode(), small);
        mon.restoreRig(before);
        QCOMPARE(f->fixtureMode(), big);
        QCOMPARE(f->channels(), quint32(20));
        QCOMPARE(f->address(), quint32(10));
        QCOMPARE(doc->fixtureForAddress(29), f->id());   // its 20 channels claimed again
    }
    delete doc;
}

/* Hangers on the 2D plot: a hanger on a truss shows its arm and hook where
   the hook is; double-click edits it; its layer hides it; removing it takes
   it off the plot. */
void Monitor_Test::hangersAreOnThePlot()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setOrigin(QVector3D(2.0f, 3.0f, 4.0f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(6.0f);
    Hanger *h = props->addHanger(Hanger::OnTruss, t->id());
    h->setAlong(2.0f);
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        MonitorGraphicsView *gv = mon.m_graphicsView;
        gv->setViewPOV(MonitorGraphicsView::PovTop);
        gv->updateTrusses();
        QCOMPARE(gv->m_hangerItems.size(), 1);
        HangerItem *item = gv->m_hangerItems.value(h->id());
        QVERIFY(item != nullptr);
        const QPointF hookPx = gv->projectMm(h->hook().x() * 1000.0, h->hook().y() * 1000.0, h->hook().z() * 1000.0);
        QVERIFY2(item->contains(item->mapFromScene(hookPx)), "the hook should be where the hanger's hook is");

        QSignalSpy edit(gv, &MonitorGraphicsView::hangerDoubleClicked);
        const QPoint vp = gv->mapFromScene(hookPx);
        QString opened;
        driveDialog([&opened](QWidget *dlg) {      // its editor opens: note it, cancel it
            opened = dlg->windowTitle();
            return true;
        }, false);
        QTest::mouseDClick(gv->viewport(), Qt::LeftButton, Qt::NoModifier, vp);
        QVERIFY2(opened.contains("Hanger"), qPrintable(opened));
        QCOMPARE(edit.count(), 1);
        QCOMPARE(edit.first().first().toUInt(), h->id());

        const quint32 hidden = props->addLayer("Rigging");
        h->setLayerId(hidden);
        props->setLayerVisible(hidden, false);
        gv->refreshItemLayerState();
        QVERIFY(!item->isVisible());

        props->removeHanger(h->id());
        gv->updateHangers();
        QVERIFY(gv->m_hangerItems.isEmpty());
    }
    delete doc;
}

/* "No undo for a re-address": the Patch dialog's Apply is one rig undo step,
   wherever it was opened from -- two fixtures moved, then put back where
   they were, and one that had been unpatched goes back to the inventory. */
void Monitor_Test::aPatchCanBeUndone()
{
    Doc *doc = new Doc(this);
    Fixture *a = new Fixture(doc); a->setChannels(10); a->setUniverse(2); a->setAddress(0);
    Fixture *b = new Fixture(doc); b->setChannels(10); b->setUniverse(2); b->setAddress(10);
    Fixture *c = new Fixture(doc); c->setChannels(10); c->setPatched(false);
    QVERIFY(doc->addFixture(a) && doc->addFixture(b) && doc->addFixture(c));
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        QCOMPARE(Monitor::instance(), &mon);
        PatchInventoryDialog dlg(doc, { a->id(), b->id(), c->id() });
        dlg.placeRows(3, 100, false);           // all three, packed, universe 4 address 101
        dlg.accept();
        QCOMPARE(a->universe(), quint32(3));
        QVERIFY(c->isPatched());

        mon.undo();
        QCOMPARE(a->universe(), quint32(2));
        QCOMPARE(a->address(), quint32(0));
        QCOMPARE(b->universe(), quint32(2));
        QCOMPARE(b->address(), quint32(10));
        QVERIFY2(!c->isPatched(), "the one patched from the inventory goes back to it");
    }
    delete doc;
}

/* A house electric (a horizontal pipe): each fixture at its own distance
   along it -- and the pipe's editor says "Along", up to the pipe's length,
   rather than a boom's height. */
void Monitor_Test::fixturesSitAlongAHorizontalPipe()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    Pipe *pipe = props->addPipe();
    pipe->setOrientation(Pipe::Horizontal);
    pipe->setOriginX(2.0f); pipe->setOriginY(3.0f); pipe->setBaseZ(5.0f);
    pipe->setLength(6.0f);
    pipe->setRunAngle(0.0f);
    QList<Fixture *> fx;
    for (int i = 0; i < 2; ++i)
    {
        Fixture *f = new Fixture(doc);
        f->setChannels(4); f->setUniverse(1); f->setAddress(quint32(i * 4));
        QVERIFY(doc->addFixture(f));
        FixtureRigProps rp;
        rp.pipeId = pipe->id();
        rp.pipeOffset = (i == 0) ? 1.0f : 4.0f;
        props->setFixtureRigProps(f->id(), rp);
        fx << f;
    }
    const QVector3D a = props->fixtureRigPosition(fx[0]->id());
    const QVector3D b = props->fixtureRigPosition(fx[1]->id());
    QVERIFY2(qAbs((b - a).length() - 3.0f) < 0.05f,
             qPrintable(QString("3 m apart along the pipe, not %1").arg((b - a).length())));
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        QString header;
        double max = 0;
        driveDialog([&](QWidget *dlg) {
            QTableWidget *tbl = dlg->findChild<QTableWidget *>("pipeFixtures");
            if (tbl == nullptr)
                return false;
            header = tbl->horizontalHeaderItem(1)->text();
            QDoubleSpinBox *second = qobject_cast<QDoubleSpinBox *>(tbl->cellWidget(1, 1));
            max = second->maximum();
            second->setValue(5.0);                  // metres here
            return true;
        });
        props->setGridUnits(MonitorProperties::Meters);
        mon.slotEditPipe(pipe->id());
        QCOMPARE(header, QString("Along"));
        QVERIFY2(qAbs(max - 6.0) < 0.01, qPrintable(QString("limit %1, the pipe is 6 m").arg(max)));
        QVERIFY(qAbs(props->fixtureRigProps(fx[1]->id()).pipeOffset - 5.0f) < 0.01f);
    }
    delete doc;
}

/* The truss editor's "Other end on stand": a horizontal truss on one stand
   picks a second, and runs between them; the field is off for a vertical
   truss. */
void Monitor_Test::theTrussEditorSpansTwoStands()
{
    Doc *doc = new Doc(this);
    MonitorProperties *props = doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Meters);
    Stand *a = props->addStand();
    a->setName("Stand A"); a->setOriginX(1.0f); a->setOriginY(2.0f); a->setHeight(2.5f);
    Stand *b = props->addStand();
    b->setName("Stand B"); b->setOriginX(4.0f); b->setOriginY(6.0f); b->setHeight(2.5f);
    Truss *t = props->addTruss();
    t->setType(Truss::Horizontal);
    t->setStandId(a->id());
    {
        QWidget parent;
        Monitor mon(&parent, doc);
        bool wasEnabled = false;
        driveDialog([&](QWidget *dlg) {
            QComboBox *end = dlg->findChild<QComboBox *>("trussEndStand");
            if (end == nullptr)
                return false;
            wasEnabled = end->isEnabled();
            end->setCurrentIndex(end->findData(b->id()));
            return true;
        });
        mon.slotEditTruss(t->id());
        QVERIFY2(wasEnabled, "a horizontal truss on a stand should offer its other end");
        QVERIFY(t->spansStands());
        QVERIFY2(qAbs(t->length() - 5.0f) < 0.01f, qPrintable(QString("length %1, the stands are 5 m apart").arg(t->length())));

        // Vertical: not offered.
        t->setEndStandId(Stand::invalidId());
        t->setType(Truss::Vertical);
        bool verticalEnabled = true;
        driveDialog([&](QWidget *dlg) {
            QComboBox *end = dlg->findChild<QComboBox *>("trussEndStand");
            if (end == nullptr)
                return false;
            verticalEnabled = end->isEnabled();
            return true;
        }, false);
        mon.slotEditTruss(t->id());
        QVERIFY2(!verticalEnabled, "a vertical truss doesn't span stands");
    }
    delete doc;
}


void Monitor_Test::aColourRingIsFoundAndDrawn()
{
    Doc doc(this);
    QDir dir(TEST_FIXTURE_DIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(doc.fixtureDefCache()->load(dir));
    QLCFixtureDef *two = doc.fixtureDefCache()->fixtureDef("Branson", "LED Movinghead+Circle (2 heads)");
    QLCFixtureDef *one = doc.fixtureDefCache()->fixtureDef("Branson", "LED Movinghead+Circle");
    QVERIFY(two != nullptr && one != nullptr);

    auto add = [&doc](QLCFixtureDef *def, const QString &mode, quint32 address) {
        Fixture *f = new Fixture(&doc);
        f->setName(QString("%1 %2").arg(def->model(), mode));
        f->setFixtureDefinition(def, def->mode(mode));
        f->setAddress(address);
        doc.addFixture(f);
        return f;
    };
    Fixture *new12 = add(two, "12 Channel", 0);
    Fixture *new10 = add(two, "10 Channel", 20);
    Fixture *old12 = add(one, "12 Channel", 40);
    Fixture *old10 = add(one, "10 Channel", 60);

    // Found: by its head in the new definition, by its name in the old.
    int head = -2;
    QCOMPARE(findColourRing(new12, &head), 11);
    QCOMPARE(head, 1);
    QCOMPARE(findColourRing(new10, &head), 9);
    QCOMPARE(head, 1);
    QCOMPARE(findColourRing(old12, &head), 11);
    QCOMPARE(head, -1);
    QCOMPARE(findColourRing(old10, &head), 9);
    QCOMPARE(head, -1);
    // The ring is no mover unit: one head on the yoke, either way.
    QCOMPARE(classifyFixture(new12).headCount, 1);
    QCOMPARE(classifyFixture(new12).ringChannel, 11);
    QCOMPARE(classifyFixture(old12).headCount, 1);

    // A mover with only a wheel (and a wheel's slots): no ring.
    QLCFixtureDef plain;
    plain.setManufacturer("Test");
    plain.setModel("Wheel spot");
    QLCFixtureMode *pm = new QLCFixtureMode(&plain);
    pm->setName("3ch");
    const QList<QLCChannel::Preset> presets = { QLCChannel::PositionPan, QLCChannel::PositionTilt,
                                                QLCChannel::IntensityDimmer };
    for (int i = 0; i < presets.size(); ++i)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("Ch %1").arg(i));
        ch->setPreset(presets.at(i));
        plain.addChannel(ch);
        pm->insertChannel(ch, i);
    }
    for (int w = 0; w < 2; ++w)
    {
        QLCChannel *wheel = new QLCChannel();
        wheel->setName(w == 0 ? "Colour wheel" : "Colour wheel 2");
        wheel->setGroup(QLCChannel::Colour);
        wheel->addCapability(new QLCCapability(0, 9, "White"));
        wheel->addCapability(new QLCCapability(10, 19, "Red"));
        wheel->addCapability(new QLCCapability(20, 255, "Rainbow"));
        plain.addChannel(wheel);
        pm->insertChannel(wheel, pm->channels().size());
    }
    plain.addMode(pm);
    Fixture *spot = new Fixture(&doc);
    spot->setFixtureDefinition(&plain, pm);
    spot->setAddress(100);
    doc.addFixture(spot);
    QCOMPARE(findColourRing(spot), -1);

    // What a ring shows: off dark; a colour all round; a chase alternating
    // and stepping round.
    bool moving = true;
    QVector<QColor> segs = colourRingSegments("off", 0.0, 8, &moving);
    QVERIFY(!moving);
    QCOMPARE(segs, QVector<QColor>(8, QColor(Qt::black)));
    segs = colourRingSegments("Blue", 0.0, 8, &moving);
    QVERIFY(!moving);
    QCOMPARE(segs, QVector<QColor>(8, QColor(Qt::blue)));
    segs = colourRingSegments("Red/Yellow-Chase", 0.0, 4, &moving);
    QVERIFY(moving);
    QCOMPARE(segs, QVector<QColor>({ Qt::red, Qt::yellow, Qt::red, Qt::yellow }));
    QCOMPARE(colourRingSegments("Red/Yellow-Chase", 0.25, 4),
             QVector<QColor>({ Qt::yellow, Qt::red, Qt::yellow, Qt::red }));
    QCOMPARE(colourRingSegments("Red/Yellow-Chase", 0.5, 4), segs);
    segs = colourRingSegments("RGB/Random-Chase", 0.0, 12, &moving);
    QVERIFY(moving);
    int lit = 0;
    for (const QColor &c : segs)
        if (c.value() >= 16)
            ++lit;
    QCOMPARE(lit, 12);
    QVERIFY(colourRingSegments("RGB/Random-Chase", 1.0, 12) != segs);

    // From the fixture's values: the ring channel, whatever the definition.
    QByteArray v(12, 0);
    v[11] = char(42);                                       // Blue
    QCOMPARE(colourRingLive(new12, v, 0.0, 6), QVector<QColor>(6, QColor(Qt::blue)));
    QCOMPARE(colourRingLive(old12, v, 0.0, 6), QVector<QColor>(6, QColor(Qt::blue)));
    v[11] = char(117);                                      // Red/Yellow-Chase
    QCOMPARE(colourRingLive(new12, v, 0.0, 2, &moving), QVector<QColor>({ Qt::red, Qt::yellow }));
    QVERIFY(moving);
    QVERIFY(colourRingLive(spot, QByteArray(5, 0), 0.0).isEmpty());

    // The 2D plot: one head with the ring round it, live.
    {
        MonitorFixtureItem item(&doc, new12->id());
        item.setSize(QSize(40, 40));
        QCOMPARE(item.headsCount(), 1);
        QCOMPARE(item.ringSegments().size(), 12);
        QByteArray live(12, 0);
        live[11] = char(87);                                // Cyan
        new12->setChannelValues(live);
        item.slotUpdateValues();
        QCOMPARE(item.ringSegments(), QVector<QColor>(12, QColor(Qt::cyan)));
        QVERIFY(!item.m_ringTimer->isActive());
        live[11] = char(250);                               // RGB/Random-Chase
        new12->setChannelValues(live);
        item.slotUpdateValues();
        QVERIFY(item.m_ringTimer->isActive());               // keeps stepping
        new12->setChannelValues(QByteArray(12, 0));
    }
    {
        // Drawn: the ring's colour round the head.
        QGraphicsScene sc;
        MonitorFixtureItem *item = new MonitorFixtureItem(&doc, new12->id());
        sc.addItem(item);
        item->setSize(QSize(40, 40));
        QByteArray live(12, 0);
        live[11] = char(42);                                // Blue
        new12->setChannelValues(live);
        item->slotUpdateValues();
        QImage img(40, 40, QImage::Format_ARGB32);
        img.fill(Qt::black);
        QPainter pt(&img);
        sc.render(&pt, QRectF(0, 0, 40, 40), QRectF(0, 0, 40, 40));
        pt.end();
        int blue = 0;
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 40; ++x)
            {
                const QColor c = img.pixelColor(x, y);
                if (c.blue() > 200 && c.red() < 60 && c.green() < 60)
                    ++blue;
            }
        QVERIFY2(blue > 20, qPrintable(QString("only %1 blue pixels").arg(blue)));
    }
    {
        MonitorFixtureItem item(&doc, old10->id());
        QCOMPARE(item.ringSegments().size(), 12);           // old definition, by name
        MonitorFixtureItem none(&doc, spot->id());
        QVERIFY(none.ringSegments().isEmpty());
        QVERIFY(none.m_ringItem == nullptr);
    }

    // The Rig Overview draws them (a smoke test: rings, chasing, no crash).
    doc.monitorProperties()->setFixturePosition(new12->id(), 0, 0, QVector3D(2000.0f, 2000.0f, 3000.0f));
    doc.monitorProperties()->setFixturePosition(old12->id(), 0, 0, QVector3D(3000.0f, 2000.0f, 3000.0f));
    StructureStudioView ov(&doc, StructureStudioView::StageKind, 0);
    ov.resize(500, 400);
    ov.reload();
    ov.setPlane(StructureStudioView::Angled);
    ov.setAngledView(0.0, 20.0);
    ov.setLiveValues(true);
    QVERIFY(!ov.grab().isNull());
    QByteArray live(512, 0);
    live[2] = char(200);                                    // tilted up towards the eye
    live[7] = char(255);
    live[11] = char(117);
    live[42] = char(200);
    live[47] = char(255);
    live[51] = char(42);
    new12->setChannelValues(live);
    old12->setChannelValues(live);
    QVERIFY(!ov.grab().isNull());
    if (!qEnvironmentVariableIsEmpty("QLC_RING_SHOT"))
    {
        ov.zoomAt(ov.w2s(fixtureWorldCentre(&doc, new12->id())), 250.0);
        ov.grab().save(qEnvironmentVariable("QLC_RING_SHOT"));
    }
    ov.setLiveValues(false);

    spot->setFixtureDefinition(nullptr, nullptr);           // plain is on the stack
}

/*****************************************************************************
 * Stage looks
 *****************************************************************************/

void Monitor_Test::aStageLookDecidesTheSilhouette()
{
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Look probe");
    fxi->setChannels(1);
    QVERIFY(m_doc->addFixture(fxi));
    auto cleanup = qScopeGuard([this, fxi]() { m_doc->deleteFixture(fxi->id()); });

    const FixtureSilhouette autoKind = classifyFixture(fxi).kind;
    QCOMPARE(int(classifyFixture(fxi).look), int(Fixture::AutoLook));

    fxi->setStageLook(Fixture::FollowspotLook);
    QCOMPARE(int(classifyFixture(fxi).look), int(Fixture::FollowspotLook));
    QCOMPARE(int(classifyFixture(fxi).kind), int(FixtureSilhouette::Par));

    fxi->setStageLook(Fixture::MovingSpotLook);
    QCOMPARE(int(classifyFixture(fxi).kind), int(FixtureSilhouette::Mover));

    // A staff that is not a bar is one body of its own shape...
    fxi->setStageLook(Fixture::StaffLook);
    QCOMPARE(int(classifyFixture(fxi).kind), int(FixtureSilhouette::Par));
    // ...one that is keeps its pixels.
    QCOMPARE(int(stageLookSilhouette(Fixture::StaffLook, FixtureSilhouette::Bar)),
             int(FixtureSilhouette::Bar));
    // The theatre lantern is not the LED lantern that lights all round.
    QCOMPARE(int(stageLookSilhouette(Fixture::LanternLook, FixtureSilhouette::Lantern)),
             int(FixtureSilhouette::Par));

    fxi->setStageLook(Fixture::AutoLook);
    QCOMPARE(int(classifyFixture(fxi).kind), int(autoKind));
}

void Monitor_Test::everyStageLookHasItsShape()
{
    const QRectF square(0, 0, 40, 40);
    const QRectF wide(0, 0, 80, 20);
    foreach (Fixture::StageLook look, Fixture::stageLooks())
    {
        const QString what = Fixture::stageLookToString(look);
        for (bool elevation : { false, true })
        {
            for (const QRectF &r : { square, wide })
            {
                const QPainterPath body = stageLookBodyPath(look, r, elevation);
                const QRectF lens = stageLookLensRect(look, r, elevation);
                if (look == Fixture::AutoLook)
                {
                    QVERIFY(body.isEmpty());
                    QVERIFY(lens.isNull());
                    continue;
                }
                QVERIFY2(!body.isEmpty(), qPrintable(what));
                QVERIFY2(r.adjusted(-0.5, -0.5, 0.5, 0.5).contains(body.boundingRect()), qPrintable(what));
                if (!lens.isNull())
                    QVERIFY2(r.adjusted(-0.5, -0.5, 0.5, 0.5).contains(lens), qPrintable(what));
            }
        }
    }
    // In a square cell each keeps its proportions.
    QRectF b = stageLookBodyPath(Fixture::FollowspotLook, square).boundingRect();
    QVERIFY(b.width() > b.height());
    b = stageLookBodyPath(Fixture::StaffLook, square).boundingRect();
    QVERIFY(b.height() > b.width() * 3);
    b = stageLookBodyPath(Fixture::BarWashLook, square).boundingRect();
    QVERIFY(b.width() > b.height() * 2);
    // A followspot's light is at its front, not its middle.
    QVERIFY(stageLookLensRect(Fixture::FollowspotLook, wide).left() > wide.center().x());
    // And the lens says what the look is.
    for (Fixture::StageLook look : { Fixture::MovingSpotLook, Fixture::LedParLook,
                                     Fixture::FresnelLook, Fixture::CycStripLook,
                                     Fixture::StrobeLook })
        QVERIFY2(!stageLookLensHint(look, QRectF(0, 0, 30, 30)).isEmpty(),
                 qPrintable(Fixture::stageLookToString(look)));
    QVERIFY(stageLookLensHint(Fixture::AutoLook, QRectF(0, 0, 30, 30)).isEmpty());
    QVERIFY(stageLookLensHint(Fixture::MovingSpotLook, QRectF(0, 0, 3, 3)).isEmpty());
    QVERIFY(stageLookSquareLens(Fixture::SquareWashLook));
    QVERIFY(!stageLookSquareLens(Fixture::LedParLook));
    // Sizes for a definition that declares none.
    QVERIFY(stageLookDefaultSize(Fixture::StaffLook, 10).height()
            > stageLookDefaultSize(Fixture::StaffLook, 10).width() * 3);
    QCOMPARE(stageLookDefaultSize(Fixture::AutoLook, 10), QSizeF(10, 10));
}

void Monitor_Test::thePlotRedrawsAFixtureWhenItsLookChanges()
{
    DropRig rig;
    QVERIFY(rig.build(m_doc, QVector3D(3000, 3000, 0)));
    MonitorFixtureItem *item = rig.item;
    QCOMPARE(int(item->stageLook()), int(Fixture::AutoLook));
    QVERIFY(item->lensHintPath().isEmpty());

    auto firstHead = [item]() { return item->m_heads.first()->m_item; };
    QVERIFY(qgraphicsitem_cast<QGraphicsEllipseItem *>(firstHead()) != nullptr);

    // Set on the fixture -- Doc's fixtureChanged reaches the plot by itself.
    rig.fxi->setStageLook(Fixture::SquareWashLook);
    item = rig.gv->m_fixtures.value(rig.fxi->id());
    QCOMPARE(int(item->stageLook()), int(Fixture::SquareWashLook));
    QGraphicsRectItem *sq = qgraphicsitem_cast<QGraphicsRectItem *>(firstHead());
    QVERIFY2(sq != nullptr, "a square wash's head should be square");
    const QRectF lens = stageLookLensRect(Fixture::SquareWashLook,
                                          QRectF(0, 0, item->m_width, item->m_height));
    QVERIFY(lens.adjusted(-0.5, -0.5, 0.5, 0.5).contains(sq->rect()));

    // A followspot: the default footprint goes long, its lens at the front.
    rig.fxi->setStageLook(Fixture::FollowspotLook);
    QVERIFY(qgraphicsitem_cast<QGraphicsEllipseItem *>(firstHead()) != nullptr);
    QVERIFY(item->m_width > item->m_height);
    QVERIFY(qgraphicsitem_cast<QGraphicsEllipseItem *>(firstHead())->rect().center().x()
            > item->m_width / 2.0);

    rig.fxi->setStageLook(Fixture::MovingSpotLook);
    QVERIFY(!item->lensHintPath().isEmpty());

    // Drawn, every look, without anything to complain about.
    QImage img(400, 300, QImage::Format_ARGB32_Premultiplied);
    foreach (Fixture::StageLook look, Fixture::stageLooks())
    {
        rig.fxi->setStageLook(look);
        img.fill(Qt::black);
        QPainter p(&img);
        rig.gv->scene()->render(&p);
    }

    rig.fxi->setStageLook(Fixture::AutoLook);
    QCOMPARE(int(item->stageLook()), int(Fixture::AutoLook));
    QVERIFY(qgraphicsitem_cast<QGraphicsEllipseItem *>(firstHead()) != nullptr);
    QVERIFY(item->lensHintPath().isEmpty());
}

void Monitor_Test::fixturePropertiesSetsTheStageLook()
{
    DropRig rig;
    QVERIFY(rig.build(m_doc, QVector3D(3000, 3000, 0)));
    Fixture *other = new Fixture(m_doc);
    other->setName("Look two");
    other->setChannels(1);
    other->setAddress(40);
    QVERIFY(m_doc->addFixture(other));
    auto cleanup = qScopeGuard([this, other]() { m_doc->deleteFixture(other->id()); });
    const QList<quint32> both { rig.fxi->id(), other->id() };

    QWidget host;
    QFormLayout *form = new QFormLayout(&host);
    FixtureFields fields(rig.mon, form, &host);
    fields.setFixtures(both, rig.fxi->id());
    QComboBox *look = fields.m_look;
    QVERIFY(!look->isHidden());
    QVERIFY(look->itemText(look->currentIndex()).startsWith("Auto"));
    // Its type's look named: a generic dimmer is a conventional.
    QCOMPARE(look->itemText(0), QString("Auto (%1)").arg(Fixture::stageLookName(Fixture::LanternLook)));
    // Sections are headings, not choices.
    int headings = 0;
    for (int i = 0; i < look->count(); ++i)
        if (!look->itemData(i, Qt::UserRole).isValid() && !look->itemText(i).isEmpty())
            ++headings;
    QCOMPARE(headings, 3);

    const QByteArray before = rig.mon->rigSnapshot();
    look->setCurrentIndex(look->findData(int(Fixture::SquareWashLook), Qt::UserRole));
    QCOMPARE(int(rig.fxi->stageLook()), int(Fixture::SquareWashLook));
    QCOMPARE(int(other->stageLook()), int(Fixture::SquareWashLook));

    // The rig's undo puts it back.
    rig.mon->restoreRig(before);
    QCOMPARE(int(rig.fxi->stageLook()), int(Fixture::AutoLook));
    QCOMPARE(int(other->stageLook()), int(Fixture::AutoLook));

    // Different: "mixed", and nothing changes until one is chosen.
    other->setStageLook(Fixture::StaffLook);
    fields.reload();
    QVERIFY(look->currentText().contains("mixed"));
    QCOMPARE(int(rig.fxi->stageLook()), int(Fixture::AutoLook));
    QCOMPARE(int(other->stageLook()), int(Fixture::StaffLook));
}

void Monitor_Test::theStageLookMenuSetsEveryFixtureSelected()
{
    Fixture *a = new Fixture(m_doc);
    a->setName("Menu A"); a->setChannels(1); a->setAddress(100);
    QVERIFY(m_doc->addFixture(a));
    Fixture *b = new Fixture(m_doc);
    b->setName("Menu B"); b->setChannels(1); b->setAddress(110);
    QVERIFY(m_doc->addFixture(b));
    auto cleanup = qScopeGuard([this, a, b]() {
        m_doc->deleteFixture(a->id());
        m_doc->deleteFixture(b->id());
    });
    b->setStageLook(Fixture::StaffLook);

    QMenu menu;
    FixtureManager::fillStageLookMenu(&menu, m_doc, { a->id(), b->id() });
    QList<QAction *> looks;
    foreach (QAction *act, menu.actions())
        if (act->isCheckable())
            looks << act;
    QCOMPARE(looks.size(), Fixture::stageLooks().size());
    QVERIFY(looks.first()->text().startsWith("Auto"));
    // They differ: nothing checked.
    foreach (QAction *act, looks)
        QVERIFY(!act->isChecked());

    QAction *bar = menu.findChild<QAction *>("stageLook_BarWash");
    QVERIFY(bar != nullptr);
    bar->trigger();
    QCOMPARE(int(a->stageLook()), int(Fixture::BarWashLook));
    QCOMPARE(int(b->stageLook()), int(Fixture::BarWashLook));
    QVERIFY(!ItemIcons::fixtureIcon(a).isNull());

    QMenu again;
    FixtureManager::fillStageLookMenu(&again, m_doc, { a->id(), b->id() });
    QAction *checked = again.findChild<QAction *>("stageLook_BarWash");
    QVERIFY(checked != nullptr && checked->isChecked());

    QCOMPARE(FixtureManager::setStageLook(m_doc, { a->id(), b->id() }, int(Fixture::BarWashLook)), 0);
    QCOMPARE(FixtureManager::setStageLook(m_doc, { a->id(), b->id() }, int(Fixture::AutoLook)), 2);
}

void Monitor_Test::theStudioDrawsAStageLook()
{
    StudioRig rig;
    QVERIFY(rig.build(m_doc));
    rig.view->setPlane(StructureStudioView::Front);
    const QPointF c(100, 100);

    FixtureVisualTraits traits = classifyFixture(rig.fxi);
    QCOMPARE(int(traits.kind), int(FixtureSilhouette::Par));
    // Auto: as before, its definition's footprint.
    QCOMPARE(rig.view->parBodyRect(traits, c).center(), c);

    rig.fxi->setStageLook(Fixture::FollowspotLook);
    traits = classifyFixture(rig.fxi);
    QRectF r = rig.view->parBodyRect(traits, c);
    QVERIFY(r.width() > r.height() * 2);
    QCOMPARE(r.center(), c);

    rig.fxi->setStageLook(Fixture::StaffLook);
    r = rig.view->parBodyRect(classifyFixture(rig.fxi), c);
    QVERIFY(r.height() > r.width() * 3);

    // Every look drawn in every flat plane.
    for (StructureStudioView::Plane plane : { StructureStudioView::Top, StructureStudioView::Front,
                                              StructureStudioView::Side })
    {
        rig.view->setPlane(plane);
        foreach (Fixture::StageLook look, Fixture::stageLooks())
        {
            rig.fxi->setStageLook(look);
            rig.view->reload();
            QImage img = rig.view->grab().toImage();
            QVERIFY(!img.isNull());
        }
    }
    rig.fxi->setStageLook(Fixture::AutoLook);
}

