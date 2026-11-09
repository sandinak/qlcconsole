/*
  Q Light Controller Plus
  monitorgraphicsview.cpp

  Copyright (C) Massimo Callegari

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

#include <QContextMenuEvent>
#include <QCursor>
#include <QApplication>
#include <QTimer>
#include <QPointer>
#include <QSettings>
#include <QDateTime>
#include <QGraphicsPathItem>
#include <QDebug>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QDataStream>
#include <QMimeData>
#include <QNativeGestureEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QMenu>
#include <QInputDialog>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>

#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QGraphicsItemGroup>
#include <QtMath>
#include <QScopedValueRollback>
#include <QScopeGuard>
#include <functional>
#include <algorithm>
#include <QDialog>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFormLayout>
#include "whyonpanel.h"
#include "monitorproperties.h"
#include <cmath>
#include "monitorgraphicsview.h"
#include "blockingview.h"
#include "selectionmenu.h"
#include "programmingmanager.h"
#include "contextmenus.h"
#include "itemlock.h"
#include "stagemarklabel.h"
#include "stagepieceitem.h"
#include "righeads.h"
#include "qlcphysical.h"
#include "qlcfixturedef.h"
#include <climits>
#include "patchinventorydialog.h"
#include "structureduplicatedialog.h"
#include "showsettingsdialog.h"
#include "headtypesdialog.h"
#include "ledlayouteditor.h"
#include "monitorfixtureitem.h"
#include "pixelpathdecor.h"
#include "freestringhandles.h"
#include "pixelpath.h"
#include "fountain.h"
#include "trussitem.h"
#include "studiogroupeditor.h"
#include "studiotemplate.h"
#include "studiocomponentbrowser.h"
#include "platformitem.h"
#include "pipeitem.h"
#include "standitem.h"
#include "sceneryitem.h"
#include "gobowheelsdialog.h"
#include "networkgearcatalog.h"
#include "structurestudioview.h"
#include "toweritem.h"
#include "hangeritem.h"
#include "zoneitem.h"
#include "nolightzone.h"
#include "hanger.h"
#include "tower.h"
#include "stand.h"
#include "pipe.h"
#include "qlceventpos.h"
#include "powersourceitem.h"
#include "powerdistribution.h"
#include <QGraphicsLineItem>
#include "networknodeitem.h"
#include "networktopology.h"
#include "targetitem.h"
#include "monitorimageitem.h"
#include "truss.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "performerfill.h"
#include "monitor.h"
#include "aimsolver.h"
#include "linecurve.h"
#include "qlcfixturemode.h"
#include "qlcpalette.h"
#include "scene.h"
#include "fixturegroup.h"
#include "createfixturegroup.h"
#include "apputil.h"
#include "function.h"
#include "programmercontroller.h"
#include "doc.h"

/** Half the fixture icon's cell size (scene px). Subtract from a scene point to
 *  CENTRE the icon on it — used so a truss-bound fixture sits centred on the
 *  truss line rather than hanging its top-left corner there. */
static QPointF halfIcon(const MonitorFixtureItem *m)
{
    return QPointF(m->cellSize().width() / 2.0, m->cellSize().height() / 2.0);
}

MonitorGraphicsView::MonitorGraphicsView(Doc *doc, QWidget *parent)
    : QGraphicsView(parent)
    , m_doc(doc)
    , m_unitValue(1000)
    , m_gridEnabled(true)
    , m_gridSubdivisions(1)
    , m_layoutLocked(false)
    , m_snapDivisions(0)
    , m_bgItem(NULL)
{
    m_scene = new QGraphicsScene();
    m_scene->setSceneRect(this->rect());
    setScene(m_scene);

    // Select-together grouping: whenever the selection changes, pull in the
    // group-mates of any selected grouped item. Also re-emitted for the toolbar
    // Group/Ungroup enabled state. Kept a DIRECT (synchronous) connection so
    // selection is consistent for an immediate click-then-drag; the crash where
    // this ran mid-item-teardown is prevented by the m_itemsRebuilding guard in
    // extendSelectionToGroups (set around every update*() rebuild).
    connect(m_scene, &QGraphicsScene::selectionChanged,
            this, &MonitorGraphicsView::extendSelectionToGroups);

    /* The follow spot: its subject ringed and a line from each head, as its
       status changes and as the stick moves the subject. */
    if (m_doc != nullptr && m_doc->programmer() != nullptr)
    {
        connect(m_doc->programmer(), &ProgrammerController::followSpotStatusChanged,
                this, &MonitorGraphicsView::updateFollowSpotOverlay);
        connect(m_doc->programmer(), &ProgrammerController::followSpotPinChanged,
                this, &MonitorGraphicsView::updateFollowSpotOverlay);
    }

    /* The stage marks' spikes and numbers are sized for the screen: redrawn
       at a new zoom (rulersChanged() is how a zoom is told), and when a deck
       or a curtain they are found from is moved. */
    connect(this, &MonitorGraphicsView::rulersChanged, this, [this]() {
        MonitorProperties *props = m_doc->monitorProperties();
        if (props != nullptr && props->stageMarks() != MonitorProperties::NoStageMarks)
            updateStageMarks();
    });
    connect(this, &MonitorGraphicsView::mapStructureChanged,
            this, &MonitorGraphicsView::updateStageMarks);
    // A fixture's label and halo are screen-sized: their areas follow a zoom.
    connect(this, &MonitorGraphicsView::rulersChanged, this, &MonitorGraphicsView::refreshFixtureViewHints);
    connect(this, &MonitorGraphicsView::rulersChanged, this, &MonitorGraphicsView::refreshFollowSpotLabel);

    /* A new or opened show deletes every stage structure (MonitorProperties::
       reset()) before the view is rebuilt, and each structure's item only
       borrows it. The Doc is still emitting while it clears -- unloading the
       bench re-seeds the follow-spot pin, which repositions the target items
       -- and updateView() reads the old items (their layer, lock and group,
       their bounds) before it rebuilds them. So drop them all now, while
       their structures still exist. Reading a freed target's position locked
       a freed mutex and hung (courseshow_test, Release); reading a freed
       truss was a use-after-free on every File > New (test matrix M1). */
    if (m_doc != NULL)
    {
        connect(m_doc, &Doc::clearing, this, &MonitorGraphicsView::dropStructureItems);
        /* And one at a time: Delete, Cut, Undo of a paste remove a structure
           and only then call update...() -- whose removeItem() asked the
           old item for its bounds, from its freed structure. */
        if (MonitorProperties *props = m_doc->monitorProperties())
            connect(props, &MonitorProperties::structureAboutToBeDeleted,
                    this, &MonitorGraphicsView::dropItemFor);
    }

    // dragging on the empty canvas rubber-band selects fixtures.
    // This works both when locked and unlocked.
    setDragMode(QGraphicsView::RubberBandDrag);
    // Shift+wheel zoom centres on the cursor.
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    // Live coordinate readout needs move events with no button held.
    setMouseTracking(true);
    viewport()->setMouseTracking(true);

    // Panning via the scrollbars must keep the rulers in sync.
    connect(horizontalScrollBar(), &QScrollBar::valueChanged,
            this, [this]() { emit rulersChanged(); });
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this]() { emit rulersChanged(); });

    // Cmd/Ctrl+Z undoes the last move, paste or edit.
    QShortcut *undoSc = new QShortcut(QKeySequence::Undo, this);
    undoSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoSc, &QShortcut::activated, this, &MonitorGraphicsView::undoLastMove);

    // Cmd/Ctrl+A selects everything that can be selected.
    QShortcut *allSc = new QShortcut(QKeySequence::SelectAll, this);
    allSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(allSc, &QShortcut::activated, this, [this]() { selectAllMapItems(); });

    // Cmd/Ctrl+G groups the selection; Cmd/Ctrl+Shift+G ungroups it.
    QShortcut *groupSc = new QShortcut(QKeySequence(tr("Ctrl+G")), this);
    groupSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(groupSc, &QShortcut::activated, this, [this]() { newGroupNow(selectedMapObjects()); });
    QShortcut *ungroupSc = new QShortcut(QKeySequence(tr("Ctrl+Shift+G")), this);
    ungroupSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(ungroupSc, &QShortcut::activated, this, [this]() { ungroupSelectedItems(); });

    // ] / [ turn the selection 15 degrees clockwise / anticlockwise (seen from
    // above); } / { (Shift with them) a single degree.
    const struct { const char *key; double deg; } turns[] = {
        { "]", 15.0 }, { "[", -15.0 }, { "}", 1.0 }, { "{", -1.0 } };
    for (const auto &tk : turns)
    {
        QShortcut *sc = new QShortcut(QKeySequence(QString::fromLatin1(tk.key)), this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        const double deg = tk.deg;
        connect(sc, &QShortcut::activated, this, [this, deg]() { rotateSelectedItems(deg); });
    }

    // Refresh aim lines whenever the active scene's fixture/value list changes.
    //
    // COALESCED: updateAimLines() deletes and recreates every aim line item, and
    // a single joystick tick emits one functionChanged per fixture per channel —
    // dozens of full rebuilds for what is visually one update. Defer to the end of
    // the event-loop turn so a burst collapses into exactly one rebuild.
    connect(m_doc, &Doc::functionChanged, this, [this](quint32 fid) {
        if (fid != m_activeSceneId || m_aimLinesUpdatePending)
            return;
        m_aimLinesUpdatePending = true;
        QTimer::singleShot(0, this, [this]() {
            m_aimLinesUpdatePending = false;
            updateAimLines();
        });
    });
    /* Re-rigging a head changes what it can reach -- flipping a spot from
       standing on a tower to hanging under a bar is exactly the what-if the
       warnings exist for -- so a rig change re-checks, coalesced the same way. */
    connect(m_scene, &QGraphicsScene::selectionChanged,
            this, &MonitorGraphicsView::scheduleReachOverlay);
    /* A reveal lasts until you select something else: the fixtures a bubble
       stands for go back under it, and their tags and footprints with them. */
    /* Readouts are stacked on screen, so again when the view moves or a line
       target's readout comes or goes with its selection. */
    connect(this, &MonitorGraphicsView::rulersChanged, this, [this]() {
        if (!m_itemsRebuilding)
            stackTargetReadouts();
    });
    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this]() {
        if (!m_itemsRebuilding)    // the scene says so while items are torn down
            stackTargetReadouts();
    });
    // A free string selected on its own shows its anchors; a zoom resizes them.
    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this]() {
        if (!m_itemsRebuilding)
            refreshStringHandles();
    });
    connect(this, &MonitorGraphicsView::rulersChanged, this, [this]() {
        if (stringHandles() != nullptr)
            refreshStringHandles();
        if (m_shapeHandles != nullptr)
            refreshShapeHandles();
    });
    // A stage feature selected on its own shows its stretch and rotate handles.
    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this]() {
        if (!m_itemsRebuilding)
            refreshShapeHandles();
    });
    /* RC8: what lit lasers and star lasers throw, on the Top plot: about 15
       times a second while any could be lit (rebuildEmissionOverlay()). */
    m_emissionVisible = QSettings().value(QStringLiteral("monitor/laserOutput"), true).toBool();
    m_emissionTimer = new QTimer(this);
    m_emissionTimer->setInterval(66);
    connect(m_emissionTimer, &QTimer::timeout, this, &MonitorGraphicsView::rebuildEmissionOverlay);
    {
        auto stale = [this]() { m_emissiveStale = true; QTimer::singleShot(0, this, &MonitorGraphicsView::rebuildEmissionOverlay); };
        connect(m_doc, &Doc::fixtureAdded, this, stale);
        connect(m_doc, &Doc::fixtureRemoved, this, stale);
        connect(m_doc, &Doc::fixtureChanged, this, stale);
    }
    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this]() {
        if (m_settingRevealSelection)
            return;
        if (!m_revealedFixtures.isEmpty())
        {
            clearRevealState();
            refreshItemLayerState();
        }
        rebuildProjectionOverlay();
    });
    // A model's LED layout edited: its fixtures' heads move to their cells.
    if (m_doc->monitorProperties() != nullptr)
        connect(m_doc->monitorProperties(), &MonitorProperties::headLayoutChanged, this,
                [this](const QString &key) {
            for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
                if (HeadLayout::keyFor(m_doc->fixture(it.key())) == key)
                    it.value()->relayoutHeads();
        });
    if (m_doc->monitorProperties() != nullptr)
        connect(m_doc->monitorProperties(), &MonitorProperties::rigPropsChanged, this,
                [this](quint32) {
            scheduleReachOverlay();
            if (m_aimLinesUpdatePending)
                return;
            m_aimLinesUpdatePending = true;
            QTimer::singleShot(0, this, [this]() {
                m_aimLinesUpdatePending = false;
                updateAimLines();
            });
        });

    m_gridSize = QSize(5, 5);
    m_gridExtent = QSizeF(5, 5);

    updateGrid();
}

void MonitorGraphicsView::wheelEvent(QWheelEvent *event)
{
    // Zoom only with Shift held (centred on the cursor); otherwise let the
    // view scroll normally.
    if ((event->modifiers() & Qt::ShiftModifier) == 0)
    {
        QGraphicsView::wheelEvent(event);
        return;
    }
    const double stepFactor = 1.15;
    const double cur = viewScale();   // rotation-independent zoom
    // With Shift held, macOS reports the wheel delta on the X axis, so fall
    // back to it when Y is zero.
    int delta = event->angleDelta().y();
    if (delta == 0)
        delta = event->angleDelta().x();
    double target = cur * ((delta > 0) ? stepFactor : 1.0 / stepFactor);
    if (target < 0.25) target = 0.25;
    if (target > 6.0)  target = 6.0;
    if (cur > 0.0 && target != cur)
    {
        scale(target / cur, target / cur);
        emit rulersChanged();
    }
    event->accept();
}

/* The view's uniform zoom, independent of any rotation.
 *
 * NOT transform().m11(): once the view is turned, m11 is no longer the scale --
 * at 90 degrees it is 0 and the scale has moved into m12. Every zoom decision
 * used to read m11 directly, which under rotation would clamp the zoom to its
 * minimum and make the "a newly loaded document starts at 1:1" check fire on
 * every load. The row length is the scale whatever the angle. */
double MonitorGraphicsView::viewScale() const
{
    const QTransform t = transform();
    return std::hypot(t.m11(), t.m12());
}

/* Turn the plot in 90-degree steps. A VIEW transform: scene coordinates, and
 * therefore everything stored in the workspace, are untouched -- Qt maps mouse
 * events through the inverse, so dragging and hit-testing keep working with no
 * changes to any item. Quarter turns only, so handedness is preserved and a
 * rotated plan still tells the truth about which side of the stage something is
 * on. Text is counter-rotated (see refreshLabelRotations) because a label is
 * the one thing that must never turn with the page. */
void MonitorGraphicsView::setViewRotation(int quarterTurns)
{
    const int r = ((quarterTurns % 4) + 4) % 4;
    if (m_viewRotation == r)
        return;
    const double zoom = viewScale();
    m_viewRotation = r;

    QTransform t;
    t.rotate(90.0 * r);
    t.scale(zoom, zoom);
    setTransform(t);

    updateGrid();               // the fit depends on which way the grid is laid out
    /* ...and so does every item's place: re-placed here, not by luck. It
       used to happen only when the turn made the scroll bars come or go and
       the viewport resize ran a full refresh; with the scene centred on the
       grid that no longer happens, and fixtures stayed at the old scale. */
    refreshAllItems();          // re-pins the labels too
    emit rulersChanged();
    viewport()->update();
}

/* Keep every text item upright while the view is turned. The plot's items carry
 * their names as CHILD text items (truss/platform/tower/pipe/stand/target), so
 * one pass over the scene covers them; the two that paint text inline
 * (MonitorFixtureItem, PowerSourceItem) counter-rotate in their own paint(). */
void MonitorGraphicsView::refreshFixtureViewHints()
{
    const qreal scale = viewScale();
    foreach (MonitorFixtureItem *mfi, m_fixtures)
        if (mfi != nullptr)
            mfi->setViewHint(scale, 90.0 * m_viewRotation);
}

namespace
{
    /* setData() key of an item kept upright AS A WHOLE (keepUpright()). */
    constexpr int kUprightKey = 0x55707269;

    /* Drawn in screen space: the item, or something it hangs off, ignores the
       view's transform. The view's turn never reaches it, so it is upright
       already -- counter-rotating it as well turned it the other way (the
       blocking's "(Default)" ghosts, the count badges' numbers, a stretch
       handle's readout all lay on their side at 90 degrees). */
    bool drawnInScreenSpace(const QGraphicsItem *it)
    {
        for (const QGraphicsItem *p = it; p != nullptr; p = p->parentItem())
            if (p->flags() & QGraphicsItem::ItemIgnoresTransformations)
                return true;
        return false;
    }

    bool insideUprightHost(const QGraphicsItem *it)
    {
        for (const QGraphicsItem *p = it->parentItem(); p != nullptr; p = p->parentItem())
            if (p->data(kUprightKey).toBool())
                return true;
        return false;
    }
}

void MonitorGraphicsView::keepUpright(QGraphicsItem *item)
{
    if (item != nullptr)
        item->setData(kUprightKey, true);
}

/* "Text stays upright and the same size when the view turns" (UX rule book).
 * One rule, for every label the plot has:
 *  - a label drawn in screen space (ItemIgnoresTransformations on it or on
 *    what carries it) is left unturned: the view's rotation never reaches it;
 *  - a label on a backing (keepUpright(): a plate, a pill) turns back as a
 *    whole, so the text never leaves its plate;
 *  - any other text item is turned back by the view's turn (and by its
 *    platform's or tower's own; a truss's name runs along it, as it always
 *    has). */
void MonitorGraphicsView::refreshLabelRotations()
{
    refreshFixtureViewHints();
    const qreal counter = -90.0 * m_viewRotation;
    foreach (QGraphicsItem *it, m_scene->items())
    {
        const bool text = it->type() == QGraphicsSimpleTextItem::Type
                          || it->type() == QGraphicsTextItem::Type;
        const bool host = it->data(kUprightKey).toBool();
        if (!text && !host)
            continue;
        if (drawnInScreenSpace(it) || (text && insideUprightHost(it)))
        {
            if (it->rotation() != 0.0)
                it->setRotation(0.0);
            continue;
        }
        // A turned platform's or tower's name stays upright too (a truss's
        // runs along it, as it always has).
        qreal parentTurn = 0.0;
        QGraphicsItem *parent = it->parentItem();
        if (dynamic_cast<PlatformItem *>(parent) != nullptr || dynamic_cast<TowerItem *>(parent) != nullptr)
            parentTurn = parent->rotation();
        it->setTransformOriginPoint(it->boundingRect().center());
        it->setRotation(counter - parentTurn);
    }
}

bool MonitorGraphicsView::viewportEvent(QEvent *event)
{
    // Trackpad pinch: macOS sends a native zoom gesture (no Shift needed),
    // centred on the cursor via AnchorUnderMouse.
    if (event->type() == QEvent::NativeGesture)
    {
        QNativeGestureEvent *g = static_cast<QNativeGestureEvent *>(event);
        if (g->gestureType() == Qt::ZoomNativeGesture)
        {
            const double cur = viewScale();
            double target = cur * (1.0 + g->value());
            if (target < 0.25) target = 0.25;
            if (target > 6.0)  target = 6.0;
            if (cur > 0.0 && target != cur)
            {
                scale(target / cur, target / cur);
                emit rulersChanged();
            }
            return true;
        }
    }
    return QGraphicsView::viewportEvent(event);
}

/* One revealed fixture's tag. The fixtures a bubble hides are stacked on the
   same spot -- clicking the pile can only ever reach the top one -- so each
   gets a named tag fanned out beside the stack, and the tag is the click
   target. It is a view decoration: nothing about the fixture moves. */
class RevealTagItem : public QGraphicsPathItem
{
public:
    RevealTagItem(quint32 fid, MonitorGraphicsView *view)
        : m_fid(fid)
        , m_view(view)
    {
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::PointingHandCursor);
    }

    quint32 fixtureID() const { return m_fid; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        m_view->focusRevealedFixture(m_fid);
        event->accept();
    }

private:
    quint32 m_fid;
    MonitorGraphicsView *m_view;
};

void MonitorGraphicsView::mousePressEvent(QMouseEvent *event)
{
    // Recorded unconditionally, regardless of which branch below actually
    // handles the press, so mouseReleaseEvent's cross-kind persistence sweep
    // (see its own comment) can tell a real drag from a bare click/select —
    // the same distinction each item's own mouseReleaseEvent already makes
    // via its private m_dragMoved before deciding whether to emit
    // itemDropped() at all.
    m_pressViewPos = event->pos();
    m_lastBand = QRectF();      // Q201: a click, until a band is dragged
    m_dragRefused = false;
    m_dragRefusedNoted = false;
    m_pressOnLockedLayer = false;

    // Laying a free string, or one of its anchors grabbed: the string's.
    if (stringPress(event))
        return;
    // A stage feature's stretch or rotate handle: the drag is the handle's.
    if (shapePress(event))
        return;

    /* A no-light zone is dragged by its outline or centre handle, nothing
       else: the plot under its inside stays selectable. Something drawn on
       the outline (a fixture, a truss) keeps the click. */
    if (event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier
        && !m_layoutLocked && !m_zoneItems.isEmpty())
    {
        const QPointF sp = mapToScene(event->pos());
        const quint32 zid = zoneGrabAt(sp);
        if (zid != UINT_MAX && topPickableAt(sp) == nullptr)
        {
            m_zoneDragId = zid;
            m_zoneDragStart = sp;
            event->accept();
            return;
        }
    }

    /* A reveal is a question about one stack -- "which of these is it?" -- and
       it ends the moment you look somewhere else. Selecting another item ends
       it too (see the selectionChanged handler), but a click on bare deck
       changes no selection at all, so it has to end here or the fixtures stay
       out over their structure for good. */
    if (!m_revealedFixtures.isEmpty() && event->button() == Qt::LeftButton)
    {
        const QPointF sp = mapToScene(event->pos());
        bool onReveal = false;
        foreach (QGraphicsItem *gi, m_revealTags)
        {
            // In screen pixels: the tag is drawn in screen space.
            if (dynamic_cast<RevealTagItem *>(gi) != nullptr
                && gi->deviceTransform(viewportTransform()).mapRect(gi->boundingRect())
                       .contains(QPointF(event->pos())))
            { onReveal = true; break; }
        }
        if (!onReveal)
        {
            if (MonitorFixtureItem *mfi = dynamic_cast<MonitorFixtureItem *>(topPickableAt(sp)))
                onReveal = m_revealedFixtures.contains(mfi->fixtureID());
        }
        if (!onReveal)
        {
            clearRevealState();
            refreshItemLayerState();
            rebuildProjectionOverlay();
        }
    }

    /* An assembly entered with a double-click is left by a click anywhere
       outside it: the next click on one of its parts takes it all again. */
    if (m_enteredAssembly != 0 && event->button() != Qt::NoButton)
    {
        QGraphicsItem *hit = topPickableAt(mapToScene(event->pos()));
        while (hit != nullptr && hit->parentItem() != nullptr)
            hit = hit->parentItem();
        if (hit == nullptr || assemblyOfItem(hit) != m_enteredAssembly)
            leaveAssembly();
    }

    /* Click again: a press on a member of a group (or a truss's worth) that
       is already selected TOGETHER -- the release, if it did not drag, picks
       just that one out, to slide or drag clear (clickAgainToPickOut()).
       Not a part of an assembly: a click takes all of it, again. */
    m_clickAgainItem = nullptr;
    if (event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier && !m_layoutLocked)
    {
        QGraphicsItem *hit = topPickableAt(mapToScene(event->pos()));
        while (hit != nullptr && hit->parentItem() != nullptr)
            hit = hit->parentItem();
        MonitorFixtureItem *mfi = dynamic_cast<MonitorFixtureItem *>(hit);
        const quint32 assembly = assemblyOfItem(hit);
        if (hit != nullptr && hit->isSelected() && m_scene->selectedItems().size() > 1
            && (assembly == 0 || assembly == m_enteredAssembly)
            && (itemGroupId(hit) != 0 || (mfi != nullptr && mfi->isBoundToTruss())))
        {
            m_clickAgainItem = hit;
            m_clickAgainPos = event->pos();
        }
    }

    // "Click to place 0,0" mode: consume the click, set the origin there.
    if (m_pickingOrigin && event->button() == Qt::LeftButton)
    {
        const QPointF mm = pixelsToRealPosition(mapToScene(event->pos()).x(),
                                                mapToScene(event->pos()).y());
        setStageOriginMetres(QPointF(mm.x() / 1000.0, mm.y() / 1000.0));
        m_pickingOrigin = false;
        viewport()->unsetCursor();
        emit originPicked();
        event->accept();
        return;
    }

    /* A right-press is the start of a context menu, not a click. Handed to Qt,
       it went to the item under the pointer, which only takes the LEFT button,
       so nobody accepted it -- and Qt's answer to an unaccepted press is to
       clear the selection. The menu then opened on nothing selected ("I can't
       right click on a FART to move to a new layer .. it unselects after I
       select it"). Keep the selection when the press is on part of it;
       otherwise select what was pressed, as every other editor does. */
    if (event->button() == Qt::RightButton)
    {
        QGraphicsItem *hit = topPickableAt(mapToScene(event->pos()));
        while (hit && hit->parentItem())
            hit = hit->parentItem();
        if (hit != nullptr && (hit->flags() & QGraphicsItem::ItemIsSelectable) && !hit->isSelected())
        {
            m_scene->clearSelection();
            if (StagePieceItem *spi = dynamic_cast<StagePieceItem *>(hit))
                spi->selectNow(true);
            else
                hit->setSelected(true);
        }
        event->accept();
        return;
    }

    /* A press on a line target's handle is the start of bending or stretching
       it, with any modifier (Shift locks the drag to an axis, Alt keeps an end
       on the line's direction): the item takes it, not the selection logic. */
    if (event->button() == Qt::LeftButton)
    {
        const QPointF sp = mapToScene(event->pos());
        QGraphicsItem *hit = topPickableAt(sp);
        while (hit && hit->parentItem())
            hit = hit->parentItem();
        if (auto *tgi = dynamic_cast<TargetItem *>(hit))
        {
            if (tgi->isLine() && (tgi->flags() & QGraphicsItem::ItemIsMovable)
                && tgi->lineHandleAt(tgi->mapFromScene(sp)) >= 0)
            {
                QGraphicsView::mousePressEvent(event);
                return;
            }
        }
    }

    if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier))
    {
        QGraphicsItem *hit = itemAt(event->pos());
        if (hit == NULL)
        {
            // Shift + left-drag on empty canvas: pan.
            setDragMode(QGraphicsView::ScrollHandDrag);
            QGraphicsView::mousePressEvent(event);
            return;
        }

        // Shift + click on an item: toggle it in/out of the selection without
        // clearing other selected items (same as Ctrl+click convention).
        while (hit && hit->parentItem())
            hit = hit->parentItem();
        if (hit && (hit->flags() & QGraphicsItem::ItemIsSelectable))
        {
            const bool on = !hit->isSelected();
            /* A part of an assembly adds (or takes away) the whole of it. */
            const quint32 assembly = assemblyOfItem(hit);
            if (assembly != 0 && assembly != m_enteredAssembly)
            {
                m_extendingSelection = true;
                foreach (QGraphicsItem *gi, itemsUnderGroup(assembly))
                    gi->setSelected(on);
                m_extendingSelection = false;
            }
            hit->setSelected(on);
            if (on)
                captureMoveUndo();
            else
                emit mapSelectionChanged();
        }
        return;
    }

    // A plain click on a fixture that's selected ONLY because its truss was
    // clicked first (extendSelectionToGroups() pulled it in) needs to break
    // out solo. Qt's default press handling leaves a click on an
    // already-selected item's selection untouched (it assumes you're about
    // to drag the group) — which would otherwise mean clicking the fixture
    // right after the truss still drags both together. Force a solo
    // re-select here so a fixture click behaves the same regardless of
    // whether the truss happened to be selected first.
    if (event->button() == Qt::LeftButton && !(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)))
    {
        QGraphicsItem *hit = itemAt(event->pos());
        while (hit && hit->parentItem())
            hit = hit->parentItem();
        auto *mfi = dynamic_cast<MonitorFixtureItem *>(hit);
        if (mfi != nullptr && mfi->isSelected())
        {
            const quint32 fid = m_fixtures.key(mfi, Fixture::invalidId());
            const quint32 trussId = (fid != Fixture::invalidId())
                ? m_doc->monitorProperties()->fixtureRigProps(fid).trussId : Truss::invalidId();
            TrussItem *ti = (trussId != Truss::invalidId()) ? m_trussItems.value(trussId, nullptr) : nullptr;
            if (ti != nullptr && ti->isSelected())
            {
                m_scene->clearSelection();
                mfi->setSelected(true);
            }
        }
    }

    // Save the current selection before Qt's default press handler clears it.
    // mouseDoubleClickEvent uses this to restore a multi-selection when the user
    // double-clicks a fixture that was already part of the selection.
    m_savedSelection = m_scene->selectedItems();

    // A left-press on a fixture may begin a move: snapshot current positions so
    // the drop can be undone.
    if (event->button() == Qt::LeftButton && itemAt(event->pos()) != NULL)
        captureMoveUndo();

    QGraphicsView::mousePressEvent(event);

    /* What the press grabbed decides whether this can be a move at all. Qt
       already leaves a non-movable item where it is while the pointer moves,
       but the release used to store it anyway: the item's own drop signal and
       the cross-kind sweep below both re-read its position, snapped it to the
       grid and ran a platform's no-overlap push -- so a riser in a tight
       deck, nudged a hair by the snap, was shoved off its neighbours and
       landed decks away ("it disappeared"), and the move was saved. */
    if (event->button() == Qt::LeftButton && m_scene != nullptr)
    {
        MonitorProperties *props = m_doc->monitorProperties();
        auto onLockedLayer = [props](const QGraphicsItem *top) {
            if (top->data(ItemLock::LayerLockedKey).toBool())
                return true;
            // Stage pieces live on the Stage layer, which they don't carry.
            return dynamic_cast<const StagePieceItem *>(top) != nullptr && props != nullptr
                   && props->hasStageLayer() && props->layer(props->stageLayerId()).locked;
        };
        if (QGraphicsItem *grabber = m_scene->mouseGrabberItem())
        {
            const QGraphicsItem *top = grabber->topLevelItem();
            m_dragRefused = !itemDraggable(grabber)
                            && (top->flags() & QGraphicsItem::ItemIsSelectable);
            m_pressOnLockedLayer = m_dragRefused && onLockedLayer(top);
        }
        else
        {
            // Through an object on a locked layer: the drag rubber-bands, but say why.
            foreach (QGraphicsItem *gi, m_scene->items(mapToScene(event->pos())))
                if (onLockedLayer(gi->topLevelItem()))
                {
                    m_pressOnLockedLayer = true;
                    break;
                }
        }
    }
}

bool MonitorGraphicsView::itemDraggable(const QGraphicsItem *item)
{
    if (item == nullptr)
        return false;
    const QGraphicsItem *top = item->topLevelItem();
    // A stage piece drags by its own rule, not Qt's movable flag.
    if (const StagePieceItem *spi = dynamic_cast<const StagePieceItem *>(top))
        return spi->isMovable();
    return (item->flags() & QGraphicsItem::ItemIsMovable)
           || (top->flags() & QGraphicsItem::ItemIsMovable);
}

MonitorGraphicsView::~MonitorGraphicsView()
{
    // Sever the scene → view connections FIRST. Otherwise the removeItem() calls
    // below fire QGraphicsScene::selectionChanged, which would run
    // extendSelectionToGroups → mapSelectionChanged → Monitor::slotMap
    // SelectionChanged, touching QActions that may already be gone (crash on
    // quit). The guard flag is belt-and-braces for the same window.
    m_itemsRebuilding = true;
    if (m_scene != nullptr)
        disconnect(m_scene, nullptr, this, nullptr);

    clearFixtures();

    // Remove and delete truss items before the scene is freed so their QObject
    // connections are cleanly severed while the view still exists.
    foreach (TrussItem *ti, m_trussItems)
    {
        m_scene->removeItem(ti);
        delete ti;
    }
    m_trussItems.clear();

    foreach (PlatformItem *pi, m_platformItems)
    {
        m_scene->removeItem(pi);
        delete pi;
    }
    m_platformItems.clear();
    if (m_apronDeckItem != nullptr)
    {
        m_scene->removeItem(m_apronDeckItem);
        delete m_apronDeckItem;
        m_apronDeckItem = nullptr;
    }
    m_stagePieceItems.clear();      // the scene's to delete, below

    foreach (MonitorImageItem *ii, m_imageItems)
    {
        m_scene->removeItem(ii);
        delete ii;
    }
    m_imageItems.clear();

    foreach (TargetItem *tgi, m_targetItems)
    {
        m_scene->removeItem(tgi);
        delete tgi;
    }
    m_targetItems.clear();

    foreach (QGraphicsLineItem *li, m_aimLines)
    {
        m_scene->removeItem(li);
        delete li;
    }
    m_aimLines.clear();
    clearFollowSpotOverlay();

    foreach (QGraphicsLineItem *li, m_trussAnchorLines)
    {
        m_scene->removeItem(li);
        delete li;
    }
    m_trussAnchorLines.clear();
    m_stageMarkItems.clear();       // the scene's to delete, just below
    m_centerLineV = nullptr;        // likewise
    m_centerLineH = nullptr;

    /* Scene was created without a parent — delete it explicitly. The view
       lets go of it as it goes, which moves the scroll bars: say nothing
       then (rulersChanged() would redraw the stage marks into it). */
    const QSignalBlocker quiet(this);
    delete m_scene;
    m_scene = nullptr;
}

void MonitorGraphicsView::showEvent(QShowEvent *event)
{
    QGraphicsView::showEvent(event);
    // paintEvent() does the real work; this just guarantees one repaint.
    update();
    // The lasers' output follows again (it rests while the plot is hidden).
    QTimer::singleShot(0, this, &MonitorGraphicsView::rebuildEmissionOverlay);
}

void MonitorGraphicsView::paintEvent(QPaintEvent *event)
{
    /* Fit to the size we are actually painting at.
     *
     * The scale comes from the widget's dimensions, and every earlier attempt
     * to catch "the widget now has its real size" guessed at when that
     * happens: the constructor is too early, showEvent still reports
     * pre-layout geometry, a queued call after showEvent can land before a
     * splitter has restored its sizes, and a "looks too small" heuristic never
     * fires when the wrong size is merely wrong rather than tiny. Painting is
     * the one moment the size is known to be real, because it is the size the
     * user is about to see.
     *
     * Terminates: updateGrid() does not change the widget's size, so the next
     * paint finds m_fittedSize equal and does nothing.
     */
    /* Hold the first frame until the document's grid has been supplied.
       Monitor shows this view and fills it afterwards, so without this the
       stage is painted once at the default grid and again at the real one --
       the flash on startup. Bounded by a deadline so a path that never calls
       setGridSize() still ends up drawing something. */
    if (!m_gridAuthoritative && !m_paintDeadlinePassed)
    {
        QPainter p(viewport());
        p.fillRect(viewport()->rect(), backgroundBrush());
        QTimer::singleShot(500, this, [this]() {
            m_paintDeadlinePassed = true;
            update();
        });
        return;
    }

    if ((size() != m_fittedSize || m_gridExtent != m_fittedGrid
            || !qFuzzyCompare(m_unitValue, m_fittedUnit))
            && width() > 0 && height() > 0)
    {
        m_fittedSize = size();
        m_fittedGrid = m_gridExtent;
        m_fittedUnit = m_unitValue;
        updateGrid();
        refreshAllItems();
        emit rulersChanged();
        /* Refitted: the stage back in the middle ("STILL not centered ..
           see the scroll bars?") -- the refit that matters most happens here
           (the tab first shown, the show's grid supplied), not in a resize.
           After this paint, as scrolling repaints. */
        QTimer::singleShot(0, this, [this]() { centerOnGrid(); });
        /* The scale is derived from three inputs and a wrong result is only
           visible as "it looks wrong", which cost a long time to pin down
           once. QLC_GRIDFIT_DEBUG prints all three and the answer, so the next
           report can be diagnosed from a number instead of a screenshot. */
        if (qEnvironmentVariableIsSet("QLC_GRIDFIT_DEBUG"))
            qDebug() << "[gridfit] fit" << m_fittedSize
                     << "grid" << m_gridSize << "unit" << m_unitValue
                     << "-> cellPixels" << m_cellPixels
                     << "viewScale" << viewScale();
    }
    /* Cheap and unconditional: a turned view must never show upside-down text,
       and items appear from many paths (add/update/reload/undo). Doing it here
       means no path can forget. It is a no-op at the default rotation. */
    if (m_viewRotation != 0)
        refreshLabelRotations();

    QGraphicsView::paintEvent(event);
    drawOrientationLabels();
}

/* Stage-direction labels around the edge of the plot, same as the studio
   editor's. Painted on the VIEWPORT after the scene so they stay pinned to the
   window while the plot pans and zooms underneath.
 *
 * Directions come from how the app behaves, not from its axis comments, which
 * contradict each other: barFaceVector() (monitorproperties.cpp) says
 * "+Y = downstage (toward audience)" and maps FaceDownstage to (0,+1,0), while
 * the geometry headers and the "X (stage right):" / "Y (upstage):" spin-box
 * labels claim the opposite on both axes. Real shows settle it -- in
 * stage-structures-demo.qxw the "SR Tower" is at X=0.21 and the "SL Tower" at
 * X=11.61, and the upstage platforms are at Y=1.53 against the downstage ones
 * at Y=3.97. So +X is stage LEFT and +Y is DOWNSTAGE, which is what puts this
 * plot in the standard ground-plan orientation: audience at the bottom of the
 * page, upstage at the top, stage right on the viewer's left. */
void MonitorGraphicsView::drawOrientationLabels()
{
    if (!m_showOrientationLabels)
        return;

    QString leftLbl, rightLbl, topLbl, bottomLbl;
    switch (m_pov)
    {
    case PovTop:                                  // screen X = X, screen Y = Y
        leftLbl  = tr("stage right");  rightLbl  = tr("stage left");
        topLbl   = tr("upstage");      bottomLbl = tr("downstage");
        break;
    case PovSide:                                 // screen X = Y, screen Y = up
        leftLbl  = tr("upstage");      rightLbl  = tr("downstage");
        topLbl   = tr("up");           bottomLbl = tr("floor");
        break;
    case PovFront:
    default:                                      // screen X = X, screen Y = up
        leftLbl  = tr("stage right");  rightLbl  = tr("stage left");
        topLbl   = tr("up");           bottomLbl = tr("floor");
        break;
    }

    // Follow the view rotation, exactly as the studio editor's labels do: after
    // a quarter turn the edge that WAS the left is the top.
    for (int i = 0; i < (m_viewRotation & 3); ++i)
    {
        const QString l = leftLbl, t = topLbl, r = rightLbl, b = bottomLbl;
        topLbl = l; rightLbl = t; bottomLbl = r; leftLbl = b;   // 90 deg clockwise
    }

    QPainter p(viewport());
    QFont f = p.font(); f.setPixelSize(9); p.setFont(f);
    p.setPen(QColor(120, 160, 220, 190));
    const QRectF area = QRectF(viewport()->rect()).adjusted(6, 4, -6, -4);
    p.drawText(area, Qt::AlignVCenter | Qt::AlignLeft,  QStringLiteral("\u25C0 ") + leftLbl);
    p.drawText(area, Qt::AlignVCenter | Qt::AlignRight, rightLbl + QStringLiteral(" \u25B6"));
    p.drawText(area, Qt::AlignHCenter | Qt::AlignTop,    QStringLiteral("\u25B2 ") + topLbl);
    p.drawText(area, Qt::AlignHCenter | Qt::AlignBottom, QStringLiteral("\u25BC ") + bottomLbl);
}

void MonitorGraphicsView::setMountedFixturesVisible(bool on)
{
    /* Fixtures clamped to a truss/pipe/tower/riser/deck sit ON TOP of the
       structure that carries them, so in the top view they can cover the thing
       you are trying to click. Hiding them clears the plot for placing
       structures; the fixtures themselves are untouched, this is purely what
       is drawn. Free-standing fixtures are never hidden -- nothing is under
       them to reach. */
    if (m_showMountedFixtures == on)
        return;
    m_showMountedFixtures = on;

    MonitorProperties *props = m_doc->monitorProperties();
    QHashIterator<quint32, MonitorFixtureItem *> it(m_fixtures);
    while (it.hasNext())
    {
        it.next();
        MonitorFixtureItem *item = it.value();
        if (item == NULL)
            continue;
        const bool mounted = props->fixtureRigProps(it.key()).primaryMount()
                             != FixtureRigProps::NoMount;
        if (!mounted)
            continue;
        item->setVisible(fixtureItemShouldBeVisible(it.key()));
    }
    refreshFixtureCountBadges();
    // The badge decides what is buried, so visibility is settled afterwards.
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        if (it.value() != nullptr)
            it.value()->setVisible(fixtureItemShouldBeVisible(it.key()));
    m_scene->update();
}

void MonitorGraphicsView::refreshAllItems()
{
    // Rebuilt items come back as selected as they were (keepMapSelection()).
    const QList<QPair<QString, quint32> > selected = selectedMapObjects();
    auto keep = qScopeGuard([this, &selected]() { keepMapSelection(selected); });

    // Items rebuilt here bring fresh child text items with them, so re-pin the
    // labels upright before anything is drawn.
    refreshLabelRotations();

    QHashIterator <quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext() == true)
    {
        it.next();
        updateFixture(it.key());
    }
    updateTrusses();
    updatePlatforms();
    updateTargets();
    updatePowerSources();
    updateNetworkNodes();
    updateImages();
    // The tethers are projected like everything else, so a scale or
    // point-of-view change invalidates them too.
    updateTrussAnchorLines();
    // cell pixels / offsets may have changed: refresh snapping
    applySnapToAllItems();
    /* The overlays drawn from those items' places, placed again with them:
       a quarter turn refits the grid (it is laid out along the other axis),
       and the follow spot's ring and lines stayed where the old scale had
       put them -- off the stage, its lines running off the plot. */
    updateFollowSpotOverlay();
    if (m_emissionTimer != nullptr)          // set up once the view is
        rebuildEmissionOverlay();
    rebuildRevealTags();
    // And the labels the rebuild brought, upright before anything is drawn.
    refreshLabelRotations();
}

void MonitorGraphicsView::keepMapSelection(const QList<QPair<QString, quint32> > &selected)
{
    /* The structures, power sources and images are rebuilt as new items,
       which are not selected: a resize (the window, or the footer's scroll
       bar coming in) or a new scale dropped a selected fountain's base and
       walls and kept only its fixtures, and a turn after it left the
       structure behind. The same set again: no select-together pass, no
       scrolling to it. */
    if (selected.isEmpty())
        return;
    QScopedValueRollback<bool> ext(m_extendingSelection, true);
    for (const QPair<QString, quint32> &key : selected)
    {
        QGraphicsItem *gi = itemFor(key.first, key.second);
        if (gi == nullptr || gi->isSelected())
            continue;
        if (StagePieceItem *spi = dynamic_cast<StagePieceItem *>(gi))
            spi->selectNow(true);
        else
            gi->setSelected(true);
    }
}

void MonitorGraphicsView::resetViewZoom()
{
    /* The wheel/pinch zoom lives in the QGraphicsView TRANSFORM, which is
       separate from the grid scale and was never reset by anything -- so a
       zoom applied while looking at one workspace was still in force after
       loading the next, and no amount of re-fitting the grid would undo it
       (the grid maths cannot see it). A newly loaded document starts at 1:1. */
    if (!qFuzzyCompare(viewScale(), 1.0))
    {
        resetTransform();
        emit rulersChanged();
    }
}

void MonitorGraphicsView::setGridSize(QSizeF size)
{
    m_gridAuthoritative = true;
    /* The REAL size, in cells: a 50 ft stage in metres is 15.24 of them, and
       the outline, the centring and the off-grid count use that. m_gridSize is
       the whole cells (what the lines are counted by, and the spin boxes). */
    m_gridExtent = size;
    m_gridSize = QSize(qMax(1, int(size.width())), qMax(1, int(size.height())));
    updateGrid();
    refreshAllItems();
}

void MonitorGraphicsView::setGridSubdivisions(int subdivisions)
{
    if (subdivisions < 1)
        subdivisions = 1;
    m_gridSubdivisions = subdivisions;
    updateGrid();
}

void MonitorGraphicsView::setLayoutLocked(bool locked)
{
    m_layoutLocked = locked;
    // refreshItemLayerState folds the global lock together with each item's own
    // lock and its layer lock. A locked plot holds aim targets too, unless
    // "Targets stay adjustable" is on (setTargetsAdjustableWhenLocked()).
    refreshItemLayerState();
}

void MonitorGraphicsView::setTargetsAdjustableWhenLocked(bool on)
{
    if (m_targetsAdjustableWhenLocked == on)
        return;
    m_targetsAdjustableWhenLocked = on;
    refreshItemLayerState();
}

void MonitorGraphicsView::applyLayerLock(QGraphicsItem *gi, bool layerLocked)
{
    if (gi == nullptr)
        return;
    /* Selectable whatever its layer says: QGraphicsItem::setSelected() is a
       no-op on an item that isn't, which is what kept a layer-locked object
       from being selected even from the Layers tree. The plot leaves it alone
       instead: it takes no mouse button, so Qt hands a click to whatever is
       under it, and topPickableAt() and the rubber band skip it too. */
    gi->setFlag(QGraphicsItem::ItemIsSelectable, true);
    static const int kOpenButtonsKey = 0x4C42;   // the buttons it takes when open
    if (layerLocked)
    {
        if (gi->acceptedMouseButtons() != Qt::NoButton)
            gi->setData(kOpenButtonsKey, int(gi->acceptedMouseButtons()));
        gi->setAcceptedMouseButtons(Qt::NoButton);
    }
    else if (gi->data(kOpenButtonsKey).isValid())
    {
        gi->setAcceptedMouseButtons(Qt::MouseButtons(gi->data(kOpenButtonsKey).toInt()));
    }
    if (gi->data(ItemLock::LayerLockedKey).toBool() != layerLocked)
    {
        gi->setData(ItemLock::LayerLockedKey, layerLocked);
        gi->update();
    }
}

void MonitorGraphicsView::dropLayerLockedFromRubberBand()
{
    if (m_scene == nullptr || rubberBandRect().isNull())
        return;
    /* A rubber band selects whatever is selectable inside it, and a
       layer-locked object is selectable (for the tree). On the plot it is
       click-through, so the band passes it by -- unless it was selected
       before the band started. */
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (gi->data(ItemLock::LayerLockedKey).toBool() && !m_savedSelection.contains(gi))
            gi->setSelected(false);
}

int MonitorGraphicsView::setSelectedItemsLayer(quint32 layerId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr)
        return 0;

    PowerDistribution *pd = m_doc->powerDistribution();
    int count = 0;

    foreach (QGraphicsItem *gi, m_scene->selectedItems())
    {
        if (auto *mfi = dynamic_cast<MonitorFixtureItem *>(gi))
        {
            props->setFixtureLayer(mfi->fixtureID(), layerId);
            count++;
        }
        else if (auto *ti = dynamic_cast<TrussItem *>(gi))
        {
            if (Truss *t = ti->truss()) { t->setLayerId(layerId); count++; }
        }
        else if (auto *pi = dynamic_cast<PlatformItem *>(gi))
        {
            if (StagePlatform *p = pi->platform()) { p->setLayerId(layerId); count++; }
        }
        else if (auto *tgt = dynamic_cast<TargetItem *>(gi))
        {
            if (StageTarget *t = tgt->target()) { t->setLayerId(layerId); count++; }
        }
        else if (auto *ps = dynamic_cast<PowerSourceItem *>(gi))
        {
            const int s = ps->sourceIndex();
            if (s >= 0 && s < pd->sources().size()) { pd->sources()[s].layerId = layerId; count++; }
        }
        else if (auto *nn = dynamic_cast<NetworkNodeItem *>(gi))
        {
            NetworkTopology *nt = m_doc->networkTopology();
            const int n = nn->nodeIndex();
            if (n >= 0 && n < nt->nodes().size()) { nt->nodes()[n].layerId = layerId; count++; }
        }
    }

    if (count > 0)
    {
        // Keep groups consistent: a group the selection carries (all of it
        // selected, or a structure's own parts) follows its items onto the
        // target layer, subtree included, so its folder in the Layers tree
        // stays under the right layer. One member picked out of a folder
        // leaves the folder instead: the rest of it stays where it is.
        MonitorProperties *props = m_doc->monitorProperties();
        QSet<quint32> tops;
        foreach (QGraphicsItem *gi, m_scene->selectedItems())
        {
            const quint32 g = itemGroupId(gi);
            if (g == 0 || !props->hasGroup(g))
                continue;
            const quint32 carried = groupCarriedBySelection(gi);
            if (carried != 0)
                tops.insert(carried);
            else if (props->group(g).layerId != layerId)
                setItemGroupId(gi, 0);
        }
        foreach (quint32 top, tops)
            setGroupSubtreeLayer(top, layerId);

        m_doc->setModified();
        refreshItemLayerState();
    }
    return count;
}

bool MonitorGraphicsView::hasSelection() const
{
    return !m_scene->selectedItems().isEmpty();
}

int MonitorGraphicsView::selectedFixtureCount() const
{
    int n = 0;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (dynamic_cast<MonitorFixtureItem *>(gi)) n++;
    return n;
}

// Collect the selected free-standing fixtures (fid + item + mm position) — the
// only ones whose XY is a plain stored coordinate we can align/distribute.
static QList<QPair<quint32, MonitorFixtureItem *> >
    collectAlignableFixtures(const QList<QGraphicsItem *> &sel,
                             const QHash<quint32, MonitorFixtureItem *> &fixtures,
                             MonitorProperties *props)
{
    QList<QPair<quint32, MonitorFixtureItem *> > out;
    foreach (QGraphicsItem *gi, sel)
    {
        auto *mfi = dynamic_cast<MonitorFixtureItem *>(gi);
        if (mfi == nullptr) continue;
        const quint32 fid = fixtures.key(mfi, Fixture::invalidId());
        if (fid == Fixture::invalidId()) continue;
        const FixtureRigProps rp = props->fixtureRigProps(fid);
        if (rp.trussId != Truss::invalidId() || rp.onRiser() || rp.onDeck())
            continue;   // derived-position fixtures: skip (align their mount, not them)
        out << qMakePair(fid, mfi);
    }
    return out;
}

/* A free fixture's stored XY is a corner, so lining up stored points lined
   up CORNERS: a wide and a narrow light "aligned" left their centres apart.
   Align and Distribute work on what the plot draws -- the icon's centre and
   edges (fixtureIconRectMm()) -- and convert back to the corner only to
   store it. */
QSizeF MonitorGraphicsView::fixtureIconSizeMm(const MonitorFixtureItem *item) const
{
    if (item == nullptr || m_cellPixels <= 0)
        return QSizeF();
    const QRectF r = item->iconRect();
    const double k = double(m_unitValue) / double(m_cellPixels);   // px -> mm
    return QSizeF(r.width() * k, r.height() * k);
}

QPointF MonitorGraphicsView::freeCentreOffsetMm(quint32 fid) const
{
    /* A free or decked fixture's stored X/Y is the CORNER of its body;
       RigHeads::fixtureCentre() -- where the rig view and the beams put it --
       adds half its size. Every other mount's position is already its
       centre. Measured from that one rule rather than repeated. */
    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr)
        return QPointF();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    if (rp.trussId != Truss::invalidId() || rp.onPipe() || rp.onTower() || rp.onRiser()
        || rp.onHanger() || props->fixtureFrameGroup(fid) != 0)
        return QPointF();
    const QVector3D c = RigHeads::fixtureCentre(m_doc, fid);
    const QVector3D w = props->fixtureRigPosition(fid);
    return QPointF(double(c.x() - w.x()) * 1000.0, double(c.y() - w.y()) * 1000.0);
}

QRectF MonitorGraphicsView::fixtureIconRectMm(quint32 fid, const MonitorFixtureItem *item) const
{
    const QSizeF sz = fixtureIconSizeMm(item);
    const QPointF c = item->realPosition() + freeCentreOffsetMm(fid);
    return QRectF(c.x() - sz.width() / 2.0, c.y() - sz.height() / 2.0, sz.width(), sz.height());
}

void MonitorGraphicsView::storeAlignedFixture(quint32 fid, MonitorFixtureItem *item,
                                              const QRectF &iconMm)
{
    const QPointF cornerMm = iconMm.center() - freeCentreOffsetMm(fid);
    MonitorProperties *props = m_doc->monitorProperties();
    /* Keep the hanging height: an XY tidy-up must never drop a light to the
       floor (it wrote Z = 0, and Monitor::slotFixtureMoved then "preserved"
       that 0). */
    const float z = props->fixturePosition(fid, 0, 0).z();
    item->setRealPosition(cornerMm);
    props->setFixturePosition(fid, 0, 0, QVector3D(cornerMm.x(), cornerMm.y(), z));
    updateFixture(fid);
    emit fixtureMoved(fid, cornerMm);
}

int MonitorGraphicsView::alignSelectedFixtures(int mode)
{
    if (m_pov != PovTop)   // alignment is an XY (top-view) operation
        return 0;
    MonitorProperties *props = m_doc->monitorProperties();
    auto items = collectAlignableFixtures(m_scene->selectedItems(), m_fixtures, props);
    if (items.size() < 2)
        return 0;

    // Each icon's extent in mm: the corner is stored, the size is the icon's.
    QList<QRectF> boxes;
    double sumX = 0, sumY = 0, minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
    for (const auto &it : items)
    {
        const QRectF b = fixtureIconRectMm(it.first, it.second);
        boxes << b;
        sumX += b.center().x(); sumY += b.center().y();
        minX = qMin(minX, b.left()); maxX = qMax(maxX, b.right());
        minY = qMin(minY, b.top());  maxY = qMax(maxY, b.bottom());
    }
    const double avgX = sumX / items.size(), avgY = sumY / items.size();

    captureMoveUndo();
    for (int i = 0; i < items.size(); i++)
    {
        QRectF b = boxes.at(i);
        switch (mode)
        {
            case 0: b.moveCenter(QPointF(avgX, b.center().y())); break;  // same X (column)
            case 1: b.moveLeft(minX); break;                             // left edges
            case 2: b.moveRight(maxX); break;                            // right edges
            case 3: b.moveCenter(QPointF(b.center().x(), avgY)); break;  // same Y (row)
            case 4: b.moveTop(minY); break;                              // upstage edges
            case 5: b.moveBottom(maxY); break;                           // downstage edges
            default: break;
        }
        storeAlignedFixture(items.at(i).first, items.at(i).second, b);
    }
    commitMoveUndo();       // one Ctrl+Z puts them back (it was captured, never kept)
    m_doc->setModified();
    refreshOffGridTally();      // aligning can push a fixture off the grid
    return items.size();
}

int MonitorGraphicsView::distributeSelectedFixtures(bool horizontal)
{
    if (m_pov != PovTop)
        return 0;
    MonitorProperties *props = m_doc->monitorProperties();
    auto items = collectAlignableFixtures(m_scene->selectedItems(), m_fixtures, props);
    if (items.size() < 3)
        return 0;

    // Evenly space the CENTRES between the two outermost centres.
    auto centreOf = [this](quint32 fid, const MonitorFixtureItem *mfi) {
        return fixtureIconRectMm(fid, mfi).center();
    };
    std::sort(items.begin(), items.end(), [&](const auto &a, const auto &b) {
        const QPointF ca = centreOf(a.first, a.second), cb = centreOf(b.first, b.second);
        return horizontal ? ca.x() < cb.x() : ca.y() < cb.y();
    });
    const QPointF first = centreOf(items.first().first, items.first().second);
    const QPointF last = centreOf(items.last().first, items.last().second);
    const double lo = horizontal ? first.x() : first.y();
    const double hi = horizontal ? last.x() : last.y();
    const double step = (hi - lo) / double(items.size() - 1);

    captureMoveUndo();
    for (int i = 0; i < items.size(); i++)
    {
        QRectF b = fixtureIconRectMm(items[i].first, items[i].second);
        QPointF c = b.center();
        if (horizontal) c.setX(lo + step * i); else c.setY(lo + step * i);
        b.moveCenter(c);
        storeAlignedFixture(items[i].first, items[i].second, b);
    }
    commitMoveUndo();       // one Ctrl+Z puts them back (it was captured, never kept)
    m_doc->setModified();
    refreshOffGridTally();
    return items.size();
}

void MonitorGraphicsView::reassignLayerItems(quint32 fromLayerId, quint32 toLayerId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr || fromLayerId == toLayerId)
        return;

    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        if (props->fixtureLayer(it.key()) == fromLayerId)
            props->setFixtureLayer(it.key(), toLayerId);

    foreach (TrussItem *ti, m_trussItems)
        if (Truss *t = ti->truss()) if (t->layerId() == fromLayerId) t->setLayerId(toLayerId);

    foreach (PlatformItem *pi, m_platformItems)
        if (StagePlatform *p = pi->platform()) if (p->layerId() == fromLayerId) p->setLayerId(toLayerId);

    foreach (TargetItem *tgt, m_targetItems)
        if (StageTarget *t = tgt->target()) if (t->layerId() == fromLayerId) t->setLayerId(toLayerId);

    foreach (PipeItem *bi, m_pipeItems)
        if (Pipe *p = bi->pipe()) if (p->layerId() == fromLayerId) p->setLayerId(toLayerId);
    foreach (StandItem *si, m_standItems)
        if (Stand *s = si->stand()) if (s->layerId() == fromLayerId) s->setLayerId(toLayerId);
    foreach (SceneryItem *sc, m_sceneryItems)
        if (Scenery *s = sc->scenery()) if (s->layerId() == fromLayerId) s->setLayerId(toLayerId);
    foreach (TowerItem *twi, m_towerItems)
        if (Tower *t = twi->tower()) if (t->layerId() == fromLayerId) t->setLayerId(toLayerId);

    PowerDistribution *pd = m_doc->powerDistribution();
    for (int i = 0; i < pd->sources().size(); i++)
        if (pd->sources().at(i).layerId == fromLayerId)
            pd->sources()[i].layerId = toLayerId;

    m_doc->setModified();
    refreshItemLayerState();
}

quint32 MonitorGraphicsView::itemGroupId(QGraphicsItem *gi) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (auto *mfi = dynamic_cast<MonitorFixtureItem *>(gi))
        return props->fixtureGroup(mfi->fixtureID());
    if (auto *ti = dynamic_cast<TrussItem *>(gi))
        return ti->truss() ? ti->truss()->groupId() : 0;
    if (auto *pi = dynamic_cast<PlatformItem *>(gi))
        return pi->platform() ? pi->platform()->groupId() : 0;
    if (auto *tg = dynamic_cast<TargetItem *>(gi))
        return tg->target() ? tg->target()->groupId() : 0;
    if (auto *ps = dynamic_cast<PowerSourceItem *>(gi))
    {
        PowerDistribution *pd = m_doc->powerDistribution();
        const int s = ps->sourceIndex();
        return (s >= 0 && s < pd->sources().size()) ? pd->sources().at(s).groupId : 0;
    }
    if (auto *nn = dynamic_cast<NetworkNodeItem *>(gi))
    {
        NetworkTopology *nt = m_doc->networkTopology();
        const int n = nn->nodeIndex();
        return (n >= 0 && n < nt->nodes().size()) ? nt->nodes().at(n).groupId : 0;
    }
    if (auto *bi = dynamic_cast<PipeItem *>(gi))
        return bi->pipe() ? bi->pipe()->groupId() : 0;
    if (auto *si = dynamic_cast<StandItem *>(gi))
        return si->stand() ? si->stand()->groupId() : 0;
    if (auto *sc = dynamic_cast<SceneryItem *>(gi))
        return sc->scenery() ? sc->scenery()->groupId() : 0;
    if (auto *twi = dynamic_cast<TowerItem *>(gi))
        return twi->tower() ? twi->tower()->groupId() : 0;
    if (auto *ii = dynamic_cast<MonitorImageItem *>(gi))
        return props->image(ii->imageId()).groupId;
    return 0;
}

void MonitorGraphicsView::setItemGroupId(QGraphicsItem *gi, quint32 gid)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (auto *mfi = dynamic_cast<MonitorFixtureItem *>(gi))
        props->setFixtureGroup(mfi->fixtureID(), gid);
    else if (auto *ti = dynamic_cast<TrussItem *>(gi))
    {
        if (Truss *t = ti->truss()) t->setGroupId(gid);
    }
    else if (auto *pi = dynamic_cast<PlatformItem *>(gi))
    {
        if (StagePlatform *p = pi->platform()) p->setGroupId(gid);
    }
    else if (auto *tg = dynamic_cast<TargetItem *>(gi))
    {
        if (StageTarget *t = tg->target()) t->setGroupId(gid);
    }
    else if (auto *ps = dynamic_cast<PowerSourceItem *>(gi))
    {
        PowerDistribution *pd = m_doc->powerDistribution();
        const int s = ps->sourceIndex();
        if (s >= 0 && s < pd->sources().size())
            pd->sources()[s].groupId = gid;
    }
    else if (auto *nn = dynamic_cast<NetworkNodeItem *>(gi))
    {
        NetworkTopology *nt = m_doc->networkTopology();
        const int n = nn->nodeIndex();
        if (n >= 0 && n < nt->nodes().size())
            nt->nodes()[n].groupId = gid;
    }
    else if (auto *bi = dynamic_cast<PipeItem *>(gi))
    { if (Pipe *p = bi->pipe()) p->setGroupId(gid); }
    else if (auto *si = dynamic_cast<StandItem *>(gi))
    { if (Stand *s = si->stand()) s->setGroupId(gid); }
    else if (auto *sc = dynamic_cast<SceneryItem *>(gi))
    { if (Scenery *s = sc->scenery()) s->setGroupId(gid); }
    else if (auto *twi = dynamic_cast<TowerItem *>(gi))
    { if (Tower *t = twi->tower()) t->setGroupId(gid); }
    else if (auto *ii = dynamic_cast<MonitorImageItem *>(gi))
    {
        MonitorProperties::MonitorImage img = props->image(ii->imageId());
        img.groupId = gid;
        props->setImage(img);
    }
}

QList<QGraphicsItem *> MonitorGraphicsView::itemsInGroup(quint32 gid) const
{
    QList<QGraphicsItem *> out;
    if (gid == 0)
        return out;
    foreach (MonitorFixtureItem *i, m_fixtures)        if (itemGroupId(i) == gid) out << i;
    foreach (TrussItem *i, m_trussItems)               if (itemGroupId(i) == gid) out << i;
    foreach (PlatformItem *i, m_platformItems)         if (itemGroupId(i) == gid) out << i;
    foreach (PipeItem *i, m_pipeItems)                 if (itemGroupId(i) == gid) out << i;
    foreach (StandItem *i, m_standItems)               if (itemGroupId(i) == gid) out << i;
    foreach (SceneryItem *i, m_sceneryItems)           if (itemGroupId(i) == gid) out << i;
    foreach (TowerItem *i, m_towerItems)               if (itemGroupId(i) == gid) out << i;
    foreach (TargetItem *i, m_targetItems)             if (itemGroupId(i) == gid) out << i;
    foreach (PowerSourceItem *i, m_powerSourceItems)   if (itemGroupId(i) == gid) out << i;
    foreach (NetworkNodeItem *i, m_networkNodeItems)   if (itemGroupId(i) == gid) out << i;
    foreach (MonitorImageItem *i, m_imageItems)        if (itemGroupId(i) == gid) out << i;
    return out;
}

quint32 MonitorGraphicsView::topLevelGroup(quint32 g) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    int guard = 0;
    while (g != 0 && guard++ < 1000)
    {
        MonitorProperties::MonitorGroup mg = props->group(g);
        if (mg.id == 0 || mg.parentGroupId == 0)
            return g;   // unregistered or already top-level
        g = mg.parentGroupId;
    }
    return g;
}

bool MonitorGraphicsView::groupIsUnder(quint32 g, quint32 ancestor) const
{
    if (ancestor == 0)
        return false;
    MonitorProperties *props = m_doc->monitorProperties();
    int guard = 0;
    while (g != 0 && guard++ < 1000)
    {
        if (g == ancestor)
            return true;
        g = props->group(g).parentGroupId;
    }
    return false;
}

QList<QGraphicsItem *> MonitorGraphicsView::itemsUnderGroup(quint32 top) const
{
    QList<QGraphicsItem *> out;
    if (top == 0)
        return out;
    foreach (MonitorFixtureItem *i, m_fixtures)        if (groupIsUnder(itemGroupId(i), top)) out << i;
    foreach (TrussItem *i, m_trussItems)               if (groupIsUnder(itemGroupId(i), top)) out << i;
    foreach (PlatformItem *i, m_platformItems)         if (groupIsUnder(itemGroupId(i), top)) out << i;
    /* Booms, stands, scenery and towers too: a fountain's walls were left
       out, so its Plot layers row selected the base without them. */
    foreach (PipeItem *i, m_pipeItems)                 if (groupIsUnder(itemGroupId(i), top)) out << i;
    foreach (StandItem *i, m_standItems)               if (groupIsUnder(itemGroupId(i), top)) out << i;
    foreach (SceneryItem *i, m_sceneryItems)           if (groupIsUnder(itemGroupId(i), top)) out << i;
    foreach (TowerItem *i, m_towerItems)               if (groupIsUnder(itemGroupId(i), top)) out << i;
    foreach (TargetItem *i, m_targetItems)             if (groupIsUnder(itemGroupId(i), top)) out << i;
    foreach (PowerSourceItem *i, m_powerSourceItems)   if (groupIsUnder(itemGroupId(i), top)) out << i;
    foreach (NetworkNodeItem *i, m_networkNodeItems)   if (groupIsUnder(itemGroupId(i), top)) out << i;
    foreach (MonitorImageItem *i, m_imageItems)        if (groupIsUnder(itemGroupId(i), top)) out << i;
    return out;
}

quint32 MonitorGraphicsView::itemLayerId(QGraphicsItem *gi) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (auto *mfi = dynamic_cast<MonitorFixtureItem *>(gi))
        return props->fixtureLayer(mfi->fixtureID());
    if (auto *ti = dynamic_cast<TrussItem *>(gi))
        return ti->truss() ? ti->truss()->layerId() : 0;
    if (auto *pi = dynamic_cast<PlatformItem *>(gi))
        return pi->platform() ? pi->platform()->layerId() : 0;
    if (auto *tg = dynamic_cast<TargetItem *>(gi))
        return tg->target() ? tg->target()->layerId() : 0;
    if (auto *bi = dynamic_cast<PipeItem *>(gi))
        return bi->pipe() ? bi->pipe()->layerId() : 0;
    if (auto *si = dynamic_cast<StandItem *>(gi))
        return si->stand() ? si->stand()->layerId() : 0;
    if (auto *sc = dynamic_cast<SceneryItem *>(gi))
        return sc->scenery() ? sc->scenery()->layerId() : 0;
    if (auto *twi = dynamic_cast<TowerItem *>(gi))
        return twi->tower() ? twi->tower()->layerId() : 0;
    if (auto *ps = dynamic_cast<PowerSourceItem *>(gi))
    {
        PowerDistribution *pd = m_doc->powerDistribution();
        const int s = ps->sourceIndex();
        return (s >= 0 && s < pd->sources().size()) ? pd->sources().at(s).layerId : 0;
    }
    if (auto *nn = dynamic_cast<NetworkNodeItem *>(gi))
    {
        NetworkTopology *nt = m_doc->networkTopology();
        const int n = nn->nodeIndex();
        return (n >= 0 && n < nt->nodes().size()) ? nt->nodes().at(n).layerId : 0;
    }
    if (auto *ii = dynamic_cast<MonitorImageItem *>(gi))
        return props->image(ii->imageId()).layerId;
    return 0;
}

void MonitorGraphicsView::setItemLayer(QGraphicsItem *gi, quint32 layerId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (auto *mfi = dynamic_cast<MonitorFixtureItem *>(gi))
        props->setFixtureLayer(mfi->fixtureID(), layerId);
    else if (auto *ti = dynamic_cast<TrussItem *>(gi))
    {
        if (Truss *t = ti->truss())
        {
            t->setLayerId(layerId);
            // Its hangers go with it.
            foreach (Hanger *h, props->hangers())
                if (h->parentKind() == Hanger::OnTruss && h->parentId() == t->id())
                    h->setLayerId(layerId);
        }
    }
    else if (auto *pi = dynamic_cast<PlatformItem *>(gi))
    { if (StagePlatform *p = pi->platform()) p->setLayerId(layerId); }
    else if (auto *tg = dynamic_cast<TargetItem *>(gi))
    { if (StageTarget *t = tg->target()) t->setLayerId(layerId); }
    else if (auto *ps = dynamic_cast<PowerSourceItem *>(gi))
    {
        PowerDistribution *pd = m_doc->powerDistribution();
        const int s = ps->sourceIndex();
        if (s >= 0 && s < pd->sources().size()) pd->sources()[s].layerId = layerId;
    }
    else if (auto *nn = dynamic_cast<NetworkNodeItem *>(gi))
    {
        NetworkTopology *nt = m_doc->networkTopology();
        const int n = nn->nodeIndex();
        if (n >= 0 && n < nt->nodes().size()) nt->nodes()[n].layerId = layerId;
    }
    else if (auto *bi = dynamic_cast<PipeItem *>(gi))
    { if (Pipe *p = bi->pipe()) p->setLayerId(layerId); }
    else if (auto *si = dynamic_cast<StandItem *>(gi))
    { if (Stand *s = si->stand()) s->setLayerId(layerId); }
    else if (auto *sc = dynamic_cast<SceneryItem *>(gi))
    { if (Scenery *s = sc->scenery()) s->setLayerId(layerId); }
    else if (auto *twi = dynamic_cast<TowerItem *>(gi))
    {
        if (Tower *t = twi->tower())
        {
            t->setLayerId(layerId);
            foreach (Hanger *h, props->hangers())
                if (h->parentKind() == Hanger::OnTower && h->parentId() == t->id())
                    h->setLayerId(layerId);
        }
    }
    else if (auto *ii = dynamic_cast<MonitorImageItem *>(gi))
    {
        MonitorProperties::MonitorImage img = props->image(ii->imageId());
        img.layerId = layerId;
        props->setImage(img);
    }
}

void MonitorGraphicsView::setGroupSubtreeLayer(quint32 gid, quint32 layerId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (!props->hasGroup(gid))
        return;
    props->setGroupLayer(gid, layerId);
    // Direct member items adopt the layer.
    foreach (QGraphicsItem *gi, itemsInGroup(gid))
        setItemLayer(gi, layerId);
    // Recurse into child groups.
    foreach (const MonitorProperties::MonitorGroup &c, props->childGroups(gid))
        setGroupSubtreeLayer(c.id, layerId);
}

quint32 MonitorGraphicsView::nextMapGroupId() const
{
    // The registry is authoritative once ensureGroupRegistry() has migrated any
    // orphan item groupIds into it — so a parent group holding only sub-groups
    // (no direct items) still reserves its id.
    return m_doc->monitorProperties()->nextGroupId();
}

void MonitorGraphicsView::ensureGroupRegistry()
{
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (MonitorFixtureItem *i, m_fixtures)
        props->ensureGroup(props->fixtureGroup(i->fixtureID()), props->fixtureLayer(i->fixtureID()));
    foreach (TrussItem *i, m_trussItems)
        if (Truss *t = i->truss()) props->ensureGroup(t->groupId(), t->layerId());
    foreach (PlatformItem *i, m_platformItems)
        if (StagePlatform *p = i->platform()) props->ensureGroup(p->groupId(), p->layerId());
    foreach (TargetItem *i, m_targetItems)
        if (StageTarget *t = i->target()) props->ensureGroup(t->groupId(), t->layerId());
    foreach (TowerItem *i, m_towerItems)
        if (Tower *t = i->tower()) props->ensureGroup(t->groupId(), t->layerId());
    PowerDistribution *pd = m_doc->powerDistribution();
    for (int s = 0; s < pd->sources().size(); s++)
        props->ensureGroup(pd->sources().at(s).groupId, pd->sources().at(s).layerId);

    // Auto-group each truss with the fixtures rigged on it. Not for a bar:
    // it always joins its PARENT's group (see the explicit ensureTrussGroup
    // (parentId) calls where a bar is created/reassigned) -- calling this
    // for a bar's own id too raced the parent's own call for the SAME
    // group's anchor (both share one groupId), landing on whichever of the
    // two ran last and sometimes anchoring the group to the bar instead of
    // the truss.
    foreach (TrussItem *i, m_trussItems)
    {
        Truss *t = i->truss();
        if (t != nullptr && t->isChildBar() == false)
            ensureTrussGroup(t->id());
    }

    // Auto-group each riser with the fixtures mounted on it.
    foreach (PlatformItem *i, m_platformItems)
        if (StagePlatform *p = i->platform())
            ensurePlatformGroup(p->id());

    // ...and each tower and pipe.
    foreach (TowerItem *i, m_towerItems)
        ensureTowerGroup(i->towerId());
    foreach (PipeItem *i, m_pipeItems)
        ensureMountGroup(QStringLiteral("pipe"), i->pipeId());
}

void MonitorGraphicsView::ensurePlatformGroup(quint32 platformId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    StagePlatform *p = props->platform(platformId);
    if (p == nullptr)
        return;

    // Fixtures mounted on this riser.
    QList<quint32> mounted;
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        if (props->fixtureRigProps(it.key()).riserPlatformId == platformId)
            mounted << it.key();
    if (mounted.isEmpty())
        return;

    // Track whether this actually changed anything, exactly as
    // ensureTrussGroup() above does. Reporting a change unconditionally marks
    // the document dirty on every load of a workspace that already has its
    // groups migrated -- 43 times for a rig with 43 risers -- which starts the
    // autosave timer and makes a quit claim there are edits to lose.
    bool changed = false;

    quint32 gid = p->groupId();
    if (gid == 0 || !props->hasGroup(gid))
    {
        gid = props->nextGroupId();
        const QString nm = p->name().isEmpty() ? tr("Platform %1").arg(platformId) : p->name();
        props->createGroup(gid, nm, p->layerId(), 0);
        changed = true;
    }
    if (p->groupId() != gid)
        changed = true;
    p->setGroupId(gid);

    // Mounting is a strong signal — force the mounted fixtures into the group.
    foreach (quint32 fid, mounted)
    {
        if (props->fixtureGroup(fid) != gid)
        {
            props->setFixtureGroup(fid, gid);
            changed = true;
        }
    }

    // Anchor the group to this riser ONLY if it's dedicated (holds no other
    // structural item). A manual group of several risers stays a plain folder.
    const QString newAnchorKind = (structuralMembersOf(gid) <= 1) ? QStringLiteral("platform") : QString();
    const quint32 newAnchorId   = (structuralMembersOf(gid) <= 1) ? platformId : 0u;
    if (props->group(gid).anchorKind != newAnchorKind || props->group(gid).anchorId != newAnchorId)
    {
        props->setGroupAnchor(gid, newAnchorKind, newAnchorId);
        changed = true;
    }

    setGroupSubtreeLayer(gid, p->layerId());
    if (changed)
    {
        m_doc->setModified();
        emit mapStructureChanged();
    }
}

void MonitorGraphicsView::ensureTrussGroup(quint32 trussId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *t = props->truss(trussId);
    if (t == nullptr)
        return;

    // Which fixtures are rigged on this truss?
    QList<quint32> boundFx;
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        if (props->fixtureRigProps(it.key()).trussId == trussId)
            boundFx << it.key();
    if (boundFx.isEmpty())
        return;   // nothing attached — no group to form

    // Reuse the truss's group if it has one, else create one named for the truss.
    quint32 gid = t->groupId();
    if (gid == 0 || !props->hasGroup(gid))
    {
        gid = props->nextGroupId();
        const QString nm = t->name().isEmpty() ? tr("Truss %1").arg(trussId) : t->name();
        props->createGroup(gid, nm, t->layerId(), 0);
        t->setGroupId(gid);
    }

    // Pull in bound fixtures that are still ungrouped (leave manual groups alone).
    bool changed = (t->groupId() != gid);
    t->setGroupId(gid);
    foreach (quint32 fid, boundFx)
    {
        if (props->fixtureGroup(fid) == 0)
        {
            props->setFixtureGroup(fid, gid);
            changed = true;
        }
    }

    // Anchor to this truss only if the group is dedicated (no other structural
    // item). A manual group of several trusses/risers stays a plain folder.
    const QString newAnchorKind = (structuralMembersOf(gid) <= 1) ? QStringLiteral("truss") : QString();
    const quint32 newAnchorId   = (structuralMembersOf(gid) <= 1) ? trussId : 0u;
    if (props->group(gid).anchorKind != newAnchorKind || props->group(gid).anchorId != newAnchorId)
    {
        props->setGroupAnchor(gid, newAnchorKind, newAnchorId);
        changed = true;
    }

    setGroupSubtreeLayer(gid, t->layerId());
    if (changed)
    {
        m_doc->setModified();
        emit mapStructureChanged();
    }
}

void MonitorGraphicsView::leaveDedicatedTrussGroup(quint32 fid, quint32 trussId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    const quint32 gid = props->fixtureGroup(fid);
    if (gid == 0)
        return;
    const MonitorProperties::MonitorGroup g = props->group(gid);
    if (g.anchorKind != QStringLiteral("truss") || g.anchorId != trussId)
        return;   // a real/manual group (or anchored elsewhere) — leave it alone
    props->setFixtureGroup(fid, 0);
}

/* Towers and pipes (booms included) group the fixtures mounted on them, so
   the Layers list nests those fixtures under the structure ("SL Tower · on
   tower", "Boom SR · on pipe"). The Layers list nests by GROUP, not by mount:
   without one a mounted fixture is drawn on the structure and listed loose. */
namespace
{
    struct MountHost
    {
        QString name;
        quint32 groupId = 0;
        quint32 layerId = 0;
        std::function<void(quint32)> setGroupId;
        bool valid = false;
    };

    MountHost mountHost(MonitorProperties *props, const QString &kind, quint32 id)
    {
        MountHost h;
        if (kind == QLatin1String("tower"))
        {
            if (Tower *t = props->tower(id))
            {
                h.name = t->name().isEmpty() ? MonitorGraphicsView::tr("Tower %1").arg(id) : t->name();
                h.groupId = t->groupId();
                h.layerId = t->layerId();
                h.setGroupId = [t](quint32 g) { t->setGroupId(g); };
                h.valid = true;
            }
        }
        else if (kind == QLatin1String("pipe"))
        {
            if (Pipe *p = props->pipe(id))
            {
                h.name = p->name().isEmpty() ? MonitorGraphicsView::tr("Pipe %1").arg(id) : p->name();
                h.groupId = p->groupId();
                h.layerId = p->layerId();
                h.setGroupId = [p](quint32 g) { p->setGroupId(g); };
                h.valid = true;
            }
        }
        return h;
    }

    bool mountedOn(const FixtureRigProps &rp, const QString &kind, quint32 id)
    {
        if (kind == QLatin1String("tower"))
            return rp.onTower() && rp.towerId == id;
        if (kind == QLatin1String("pipe"))
            return rp.pipeId == id;
        return false;
    }
}

void MonitorGraphicsView::ensureMountGroup(const QString &kind, quint32 id)
{
    MonitorProperties *props = m_doc->monitorProperties();
    MountHost host = mountHost(props, kind, id);
    if (!host.valid)
        return;

    QList<quint32> mounted;
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        if (mountedOn(props->fixtureRigProps(it.key()), kind, id))
            mounted << it.key();
    if (mounted.isEmpty())
        return;

    // Only report a change when there is one (see ensurePlatformGroup()).
    bool changed = false;
    quint32 gid = host.groupId;
    if (gid == 0 || !props->hasGroup(gid))
    {
        gid = props->nextGroupId();
        props->createGroup(gid, host.name, host.layerId, 0);
        changed = true;
    }
    if (host.groupId != gid)
        changed = true;
    host.setGroupId(gid);

    foreach (quint32 fid, mounted)
    {
        if (props->fixtureGroup(fid) == 0)
        {
            props->setFixtureGroup(fid, gid);
            changed = true;
        }
    }

    // Anchored to the structure only when the group is its own; a manual
    // group of several structures stays a folder, with the structure's row as
    // the container.
    const bool own = structuralMembersOf(gid) <= 1;
    const QString newAnchorKind = own ? kind : QString();
    const quint32 newAnchorId   = own ? id : 0u;
    if (props->group(gid).anchorKind != newAnchorKind || props->group(gid).anchorId != newAnchorId)
    {
        props->setGroupAnchor(gid, newAnchorKind, newAnchorId);
        changed = true;
    }

    setGroupSubtreeLayer(gid, host.layerId);
    if (changed)
    {
        m_doc->setModified();
        emit mapStructureChanged();
    }
}

void MonitorGraphicsView::joinMountGroup(quint32 fid, const QString &kind, quint32 id)
{
    /* An explicit mount joins the structure's group, whatever group the
       fixture was in -- same as attachFixtureToTruss(). */
    ensureMountGroup(kind, id);
    const MountHost host = mountHost(m_doc->monitorProperties(), kind, id);
    if (host.valid && host.groupId != 0)
        m_doc->monitorProperties()->setFixtureGroup(fid, host.groupId);
}

void MonitorGraphicsView::leaveMountGroup(quint32 fid, const QString &kind, quint32 id)
{
    MonitorProperties *props = m_doc->monitorProperties();
    const quint32 gid = props->fixtureGroup(fid);
    if (gid == 0)
        return;
    const MonitorProperties::MonitorGroup g = props->group(gid);
    if (g.anchorKind != kind || g.anchorId != id)
        return;   // a manual group, or anchored elsewhere -- leave it alone
    props->setFixtureGroup(fid, 0);
}

void MonitorGraphicsView::leaveMountGroupsOf(quint32 fid)
{
    const FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(fid);
    if (rp.onTower())
        leaveMountGroup(fid, QStringLiteral("tower"), rp.towerId);
    if (rp.pipeId != Pipe::invalidId())
        leaveMountGroup(fid, QStringLiteral("pipe"), rp.pipeId);
}

void MonitorGraphicsView::takeOutOfStructure(const QList<quint32> &fids,
                                             const QString &kind, quint32 id)
{
    MonitorProperties *props = m_doc->monitorProperties();
    bool any = false;
    foreach (quint32 fid, fids)
    {
        const FixtureRigProps rp = props->fixtureRigProps(fid);
        const bool onIt = (kind == QLatin1String("truss") && rp.trussId == id)
                       || (kind == QLatin1String("tower") && rp.onTower() && rp.towerId == id)
                       || (kind == QLatin1String("pipe") && rp.onPipe() && rp.pipeId == id);
        if (onIt)
        {
            unmountFixture(fid);
            any = true;
            continue;
        }

        /* Filed under it, mounted elsewhere ("3z .. under T-3 .. which it
           clearly shouldn't be"): out of this structure's group, back into
           the one it really hangs on, if any. */
        const quint32 gid = props->fixtureGroup(fid);
        const MonitorProperties::MonitorGroup g = props->group(gid);
        if (gid == 0 || g.anchorKind != kind || g.anchorId != id)
            continue;
        props->setFixtureGroup(fid, 0);
        if (rp.trussId != Truss::invalidId())
        {
            ensureTrussGroup(rp.trussId);
            if (Truss *t = props->truss(rp.trussId))
                if (t->groupId() != 0)
                    props->setFixtureGroup(fid, t->groupId());
        }
        else if (rp.onTower())
            joinMountGroup(fid, QStringLiteral("tower"), rp.towerId);
        else if (rp.onPipe())
            joinMountGroup(fid, QStringLiteral("pipe"), rp.pipeId);
        updateFixture(fid);
        any = true;
    }
    if (any)
    {
        m_doc->setModified();
        refreshItemLayerState();
        emit mapStructureChanged();
    }
}

int MonitorGraphicsView::placeWirelessReceivers()
{
    MonitorProperties *props = m_doc->monitorProperties();
    QHash<quint32, QVector3D> positions;
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        positions.insert(it.key(), props->fixtureRigPosition(it.key()));
    NetworkTopology *nt = m_doc->networkTopology();
    int placed = 0;
    for (int i = 0; i < nt->nodes().size(); ++i)
        placed += nt->placeReceivers(i, m_doc, positions);
    if (placed > 0)
    {
        m_doc->setModified();
        updateNetworkNodes();
    }
    return placed;
}

void MonitorGraphicsView::ensureTowerGroup(quint32 towerId)
{
    ensureMountGroup(QStringLiteral("tower"), towerId);
}

void MonitorGraphicsView::leaveDedicatedTowerGroup(quint32 fid, quint32 towerId)
{
    leaveMountGroup(fid, QStringLiteral("tower"), towerId);
}

int MonitorGraphicsView::structuralMembersOf(quint32 gid) const
{
    if (gid == 0)
        return 0;
    int n = 0;
    // A bar hung on a truss shares that truss's group but is not a SEPARATE
    // structural item for this count -- it belongs to the truss, the way its
    // own fixtures do. Counting it as one made ensureTrussGroup() see "2
    // structural items" for a plain truss carrying one bar and treat it as a
    // multi-truss manual group, clearing the anchor that made the truss
    // render as "T-1 · on truss" -- it fell back to a bare "· frame" folder
    // instead, with the truss itself showing up as a redundant leaf inside
    // it ("what's the difference between the T-1 Frame and the T-1 Truss?").
    foreach (TrussItem *i, m_trussItems)
        if (i->truss() && i->truss()->groupId() == gid && i->truss()->isChildBar() == false) n++;
    foreach (PlatformItem *i, m_platformItems)
        if (i->platform() && i->platform()->groupId() == gid) n++;
    foreach (TowerItem *i, m_towerItems)
        if (i->tower() && i->tower()->groupId() == gid) n++;
    foreach (PipeItem *i, m_pipeItems)
        if (i->pipe() && i->pipe()->groupId() == gid) n++;
    return n;
}

int MonitorGraphicsView::groupSelectedItems()
{
    MonitorProperties *props = m_doc->monitorProperties();
    QList<QGraphicsItem *> sel = m_scene->selectedItems();

    // Count the distinct "units" in the selection: loose items plus distinct
    // top-level groups. Grouping only makes sense when combining >= 2 units
    // (otherwise you'd just wrap a single existing group in a pointless layer).
    // A member picked out of a folder on its own counts as loose: it moves
    // into the new group and the rest of its folder stays as it is. A folder
    // selected whole (from Plot layers or Select Group), or a structure with
    // its parts, nests in the new group as a unit.
    QSet<quint32> topGroups;
    QList<QGraphicsItem *> looseItems;
    foreach (QGraphicsItem *gi, sel)
    {
        const quint32 carried = groupCarriedBySelection(gi);
        if (carried == 0) looseItems << gi;
        else              topGroups.insert(carried);
    }
    const int loose = looseItems.size();
    if (loose + topGroups.size() < 2)
        return 0;

    // The new group adopts the layer the selected items already live on (the
    // most common one), so grouping same-layer items doesn't yank them onto a
    // different active layer. Ties / empty fall back to the active layer.
    QHash<quint32, int> layerVotes;
    foreach (QGraphicsItem *gi, sel)
        layerVotes[itemLayerId(gi)]++;
    quint32 layer = props->activeLayerId();
    int best = -1;
    for (auto it = layerVotes.constBegin(); it != layerVotes.constEnd(); ++it)
        if (it.value() > best) { best = it.value(); layer = it.key(); }

    const quint32 pid = props->nextGroupId();
    props->createGroup(pid, tr("Group %1").arg(pid), layer, 0);

    // Loose items become direct members of the new group, on its layer.
    foreach (QGraphicsItem *gi, looseItems)
    {
        setItemGroupId(gi, pid);
        setItemLayer(gi, layer);
    }
    // Each existing top-level group nests under the new parent, subtree onto layer.
    foreach (quint32 top, topGroups)
    {
        props->setGroupParent(top, pid);
        setGroupSubtreeLayer(top, layer);
    }

    m_doc->setModified();
    emit mapSelectionChanged();
    emit mapStructureChanged();
    return sel.size();
}

quint32 MonitorGraphicsView::createStudioGroupFromSelection()
{
    MonitorProperties *props = m_doc->monitorProperties();

    // Gather the selected fixtures.
    QList<quint32> fids;
    foreach (MonitorFixtureItem *mfi, selectedFixtureItems())
    {
        const quint32 fid = mfi->fixtureID();
        if (fid != Fixture::invalidId())
            fids << fid;
    }
    if (fids.size() < 2)
        return 0;

    // A studio group is a MonitorGroup carrying a local frame. Create a fresh
    // one on the active layer; origin = centroid of the members' CURRENT world
    // positions so nothing visually jumps when the frame is enabled.
    const quint32 gid = props->nextGroupId();
    props->createGroup(gid, tr("Studio %1").arg(gid), props->activeLayerId(), 0);

    QVector3D sum;
    foreach (quint32 fid, fids)
        sum += props->fixtureRigPosition(fid);   // metres, pre-frame
    const QVector3D origin = sum / float(fids.size());
    props->setGroupFrame(gid, origin, 0.0f);
    props->setGroupHasFrame(gid, true);

    // Membership + per-member local offset (rotation 0 → local = world - origin).
    // Capture world BEFORE joining the frame group, else fixtureRigPosition would
    // already return the (as-yet-unset) derived position.
    foreach (quint32 fid, fids)
    {
        const QVector3D world = props->fixtureRigPosition(fid);
        props->setFixtureGroup(fid, gid);
        FixtureRigProps rp = props->fixtureRigProps(fid);
        rp.groupLocal = props->worldToGroupLocal(gid, world);
        props->setFixtureRigProps(fid, rp);
    }

    setGroupSubtreeLayer(gid, props->activeLayerId());
    foreach (quint32 fid, fids)
        updateFixture(fid);
    m_doc->setModified();
    emit mapStructureChanged();
    return gid;
}

void MonitorGraphicsView::stampStudioTemplate()
{
    QList<quint32> fids;
    foreach (MonitorFixtureItem *mfi, selectedFixtureItems())
    {
        const quint32 fid = mfi->fixtureID();
        if (fid != Fixture::invalidId())
            fids << fid;
    }
    if (fids.isEmpty())
        return;

    const QString path = QFileDialog::getOpenFileName(this,
        tr("Stamp Studio Template"), QString(),
        tr("Fixture Studio Template (*.json)"));
    if (path.isEmpty())
        return;

    const int roles = StudioTemplate::roleCount(path);
    if (roles > 0 && fids.size() < roles)
    {
        const QMessageBox::StandardButton r = QMessageBox::question(this,
            tr("Stamp Template"),
            tr("The template has %1 roles but %2 fixtures are selected. "
               "Only the first %2 roles will be filled. Continue?")
                .arg(roles).arg(fids.size()));
        if (r != QMessageBox::Yes)
            return;
    }

    QString err;
    const quint32 gid = StudioTemplate::stamp(m_doc, path, fids, &err);
    if (gid == 0)
    {
        QMessageBox::warning(this, tr("Stamp Template"),
                             tr("Could not stamp template: %1").arg(err));
        return;
    }
    foreach (quint32 fid, fids)
        updateFixture(fid);
    emit mapStructureChanged();
    openStudioGroupEditor(gid);
}

void MonitorGraphicsView::browseStudioComponents()
{
    // Gather the current fixture selection (a stamp binds a component's roles to
    // these, in order). An empty selection is fine — the browser opens read-only
    // for browsing / managing the library.
    QList<quint32> fids;
    foreach (MonitorFixtureItem *mfi, selectedFixtureItems())
    {
        const quint32 fid = mfi->fixtureID();
        if (fid != Fixture::invalidId())
            fids << fid;
    }

    StudioComponentBrowser dlg(m_doc, fids, this);
    connect(&dlg, &StudioComponentBrowser::stamped, this, [this](quint32 gid) {
        MonitorProperties *props = m_doc->monitorProperties();
        foreach (quint32 fid, props->fixtureItemsID())
            if (props->fixtureFrameGroup(fid) == gid)
                updateFixture(fid);
        emit mapStructureChanged();
        openStudioGroupEditor(gid);
    });
    dlg.exec();
}

void MonitorGraphicsView::openStudioGroupForGroup(quint32 groupId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (groupId == 0 || !props->hasGroup(groupId))
        return;

    // Turn an existing plain group (e.g. "US-1") into a studio group in place.
    // A group that is already a studio group is opened as-is.
    if (!props->group(groupId).hasFrame)
    {
        const MonitorProperties::MonitorGroup g0 = props->group(groupId);

        // Members = fixtures whose group chain includes this group.
        QList<quint32> members;
        foreach (quint32 fid, props->fixtureItemsID())
        {
            quint32 g = props->fixtureGroup(fid);
            int guard = 0;
            while (g != 0 && guard++ < 64)
            {
                if (g == groupId) { members << fid; break; }
                g = props->group(g).parentGroupId;
            }
        }

        // Capture world positions BEFORE enabling the frame (afterwards
        // fixtureRigPosition would return the not-yet-set derived value).
        QMap<quint32, QVector3D> world;
        QVector3D sum;
        foreach (quint32 fid, members)
        {
            const QVector3D w = props->fixtureRigPosition(fid);
            world.insert(fid, w);
            sum += w;
        }

        // If the group is anchored to a platform, SLAVE the frame to it (origin
        // = the platform's corner) so the Front plane IS the riser face and the
        // unit follows the platform. Otherwise adopt the members' centroid.
        StagePlatform *pl = (g0.anchorKind == QStringLiteral("platform"))
                                ? props->platform(g0.anchorId) : nullptr;
        const QPointF corner = (pl != nullptr)
            ? pl->toWorld(QPointF(double(pl->originX()), double(pl->originY()))) : QPointF();
        const QVector3D origin = (pl != nullptr)
            ? QVector3D(float(corner.x()), float(corner.y()), 0.0f)
            : (members.isEmpty() ? QVector3D() : (sum / float(members.size())));
        props->setGroupFrame(groupId, origin, pl != nullptr ? pl->rotation() : 0.0f);
        props->setGroupHasFrame(groupId, true);

        foreach (quint32 fid, members)
        {
            FixtureRigProps rp = props->fixtureRigProps(fid);
            // Migrate a platform mount into the frame: the world position was
            // captured pre-frame, so convert it to a local offset and retire the
            // riser/deck mount — the slaved frame now drives the fixture (one
            // representation, no precedence collision).
            if (pl != nullptr)
            {
                if (rp.riserPlatformId == g0.anchorId)
                {
                    // Preserve the fixture's face as its studio mount plane.
                    // (The two are numbered oppositely; the mapping lives in
                    // FixtureRigProps so no caller has to remember that.)
                    rp.studioMount = FixtureRigProps::studioMountForRiserFace(rp.riserFace);
                    rp.riserPlatformId = FixtureRigProps::invalidPlatformId();
                }
                if (rp.deckPlatformId == g0.anchorId)
                {
                    rp.studioMount = 0;   // deck = top plane
                    rp.deckPlatformId = FixtureRigProps::invalidPlatformId();
                }
            }
            rp.groupLocal = props->worldToGroupLocal(groupId, world.value(fid));
            props->setFixtureRigProps(fid, rp);
            updateFixture(fid);
        }
        m_doc->setModified();
        emit mapStructureChanged();
    }

    openStudioGroupEditor(groupId);
}

void MonitorGraphicsView::openStudioGroupEditor(quint32 groupId)
{
    if (groupId == 0 || !m_doc->monitorProperties()->hasGroup(groupId))
        return;
    // The unified object editor (Monitor::openGroupStudio) now owns studio-group
    // editing; the old StudioGroupEditor lives on behind its "Group settings…"
    // button. Route the request up to the host.
    emit studioGroupEditRequested(groupId);
}

QPointF MonitorGraphicsView::selectionPivot() const
{
    const QRectF box = selectionBox();
    return box.isNull() ? QPointF() : box.center();
}

QRectF MonitorGraphicsView::selectionBox() const
{
    MonitorProperties *props = m_doc->monitorProperties();
    double x0 = 1e18, y0 = 1e18, x1 = -1e18, y1 = -1e18;
    auto add = [&](double x, double y) {
        x0 = qMin(x0, x); x1 = qMax(x1, x);
        y0 = qMin(y0, y); y1 = qMax(y1, y);
    };
    auto add3 = [&](const QVector3D &w) { add(double(w.x()), double(w.y())); };
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
    {
        if (auto *mfi = dynamic_cast<MonitorFixtureItem *>(gi))
            add3(props->fixtureRigPosition(mfi->fixtureID()));
        else if (auto *ti = dynamic_cast<TrussItem *>(gi))
        {
            if (Truss *t = ti->truss()) { add3(t->origin()); add3(t->positionAt(t->length())); }
        }
        else if (auto *pi = dynamic_cast<PlatformItem *>(gi))
        {
            if (StagePlatform *pl = pi->platform())
            {
                foreach (const QPointF &q, pl->footprint(8))
                    add(q.x(), q.y());
            }
        }
        else if (auto *tg = dynamic_cast<TargetItem *>(gi))
        {
            if (StageTarget *t = tg->target()) add(double(t->x()), double(t->y()));
        }
        else if (auto *ps = dynamic_cast<PowerSourceItem *>(gi))
        {
            const QList<PowerSource> &src = m_doc->powerDistribution()->sources();
            if (ps->sourceIndex() >= 0 && ps->sourceIndex() < src.size())
                add(src.at(ps->sourceIndex()).posX, src.at(ps->sourceIndex()).posY);
        }
        else if (auto *nn = dynamic_cast<NetworkNodeItem *>(gi))
        {
            const QList<NetworkNode> &nodes = m_doc->networkTopology()->nodes();
            if (nn->nodeIndex() >= 0 && nn->nodeIndex() < nodes.size())
                add(nodes.at(nn->nodeIndex()).posX, nodes.at(nn->nodeIndex()).posY);
        }
        else if (auto *bi = dynamic_cast<PipeItem *>(gi))
        {
            if (Pipe *pp = bi->pipe()) { add3(pp->positionAt(0.0f)); add3(pp->positionAt(pp->length())); }
        }
        else if (auto *si = dynamic_cast<StandItem *>(gi))
        {
            if (Stand *st = si->stand()) add(double(st->originX()), double(st->originY()));
        }
        else if (auto *sc = dynamic_cast<SceneryItem *>(gi))
        {
            if (Scenery *s = sc->scenery())
                for (int i = 0; i <= 4; ++i)
                    add3(s->pointAt(-1.0 + i / 2.0, s->bottom()));
        }
        else if (auto *twi = dynamic_cast<TowerItem *>(gi))
        {
            if (Tower *tw = twi->tower())
            {
                foreach (const QPointF &q, tw->corners())
                    add(q.x(), q.y());
            }
        }
        else if (auto *ii = dynamic_cast<MonitorImageItem *>(gi))
        {
            const MonitorProperties::MonitorImage img = props->image(ii->imageId());
            add(double(img.originX), double(img.originY));
            add(double(img.originX + img.width), double(img.originY + img.height));
        }
    }
    if (x0 > x1)
        return QRectF();
    // A single point still has a middle: a box too small to see, not null.
    return QRectF(QPointF(x0, y0), QPointF(qMax(x1, x0 + 1e-6), qMax(y1, y0 + 1e-6)));
}

int MonitorGraphicsView::rotateSelectedItems(double deg, const QPointF *about)
{
    MonitorProperties *props = m_doc->monitorProperties();
    const QList<QGraphicsItem *> sel = m_scene->selectedItems();
    if (sel.isEmpty() || qFuzzyIsNull(deg))
        return 0;

    const QPointF c = about != nullptr ? *about : selectionPivot();
    const double r = qDegreesToRadians(deg);
    const double cs = qCos(r), sn = qSin(r);
    // A plan point turned about the middle of the selection.
    auto turnPt = [&](double x, double y) {
        const double dx = x - c.x(), dy = y - c.y();
        return QPointF(c.x() + dx * cs - dy * sn, c.y() + dx * sn + dy * cs);
    };
    auto turnDir = [&](const QPointF &d) {
        return QPointF(d.x() * cs - d.y() * sn, d.x() * sn + d.y() * cs);
    };
    const float fdeg = float(deg);

    // One undo step: the rig as it was, and the nodes and power sources
    // (which live outside it) where they were.
    m_openUndoBatch = ++m_undoBatch;
    auto closeBatch = qScopeGuard([this]() { m_openUndoBatch = 0; });
    if (m_rigSnapshotter)
        pushRigUndo(m_rigSnapshotter());
    UndoEntry outside;
    outside.type = UndoEntry::ItemMove;

    QSet<quint32> trusses, platforms, towers, pipes, frames;
    QSet<quint32> fixtures;            // whose place is their own (free, on a deck)
    int turned = 0;

    foreach (QGraphicsItem *gi, sel)
    {
        if (auto *ti = dynamic_cast<TrussItem *>(gi))
        {
            Truss *t = ti->truss();
            if (t == nullptr || t->isChildBar())
                continue;                       // a bar follows the truss it is on
            const QPointF o = turnPt(double(t->origin().x()), double(t->origin().y()));
            t->setOrigin(QVector3D(float(o.x()), float(o.y()), t->origin().z()));
            t->setDirection(turnDir(t->direction()));
            trusses.insert(t->id());
            ++turned;
        }
        else if (auto *pi = dynamic_cast<PlatformItem *>(gi))
        {
            StagePlatform *pl = pi->platform();
            if (pl == nullptr)
                continue;
            /* Who stands on it turns with it (one selected turns itself). */
            foreach (StageTarget *t, PerformerFill::standingOn(props, pl->id()))
            {
                TargetItem *tgi = m_targetItems.value(t->id(), nullptr);
                if (tgi != nullptr && tgi->isSelected())
                    continue;
                const QPointF o = turnPt(double(t->x()), double(t->y()));
                t->setX(float(o.x()));
                t->setY(float(o.y()));
                if (t->isLine())
                {
                    QVector<QVector3D> pts = t->linePoints();
                    for (QVector3D &q : pts)
                    {
                        const QPointF d = turnDir(QPointF(double(q.x()), double(q.y())));
                        q = QVector3D(float(d.x()), float(d.y()), q.z());
                    }
                    t->setLinePoints(pts);
                }
            }
            const QPointF nc = turnPt(pl->centre().x(), pl->centre().y());
            pl->setOriginX(float(nc.x() - double(pl->width()) / 2.0));
            pl->setOriginY(float(nc.y() - double(pl->depth()) / 2.0));
            pl->setRotation(pl->rotation() + fdeg);
            platforms.insert(pl->id());
            ++turned;
        }
        else if (auto *twi = dynamic_cast<TowerItem *>(gi))
        {
            Tower *tw = twi->tower();
            if (tw == nullptr)
                continue;
            const QPointF nc = turnPt(tw->centre().x(), tw->centre().y());
            tw->setOriginX(float(nc.x() - double(tw->width()) / 2.0));
            tw->setOriginY(float(nc.y() - double(tw->depth()) / 2.0));
            tw->setRotation(tw->rotation() + fdeg);
            towers.insert(tw->id());
            ++turned;
        }
        else if (auto *bi = dynamic_cast<PipeItem *>(gi))
        {
            Pipe *pp = bi->pipe();
            if (pp == nullptr || pp->isStandMounted() || pp->isBarOnPipe())
                continue;                       // these follow what they are on
            const QPointF o = turnPt(double(pp->originX()), double(pp->originY()));
            pp->setOriginX(float(o.x()));
            pp->setOriginY(float(o.y()));
            pp->setRunAngle(pp->runAngle() + fdeg);
            pipes.insert(pp->id());
            ++turned;
        }
        else if (auto *si = dynamic_cast<StandItem *>(gi))
        {
            Stand *st = si->stand();
            if (st == nullptr)
                continue;
            const QPointF o = turnPt(double(st->originX()), double(st->originY()));
            st->setOriginX(float(o.x()));
            st->setOriginY(float(o.y()));
            st->setRotation(st->rotation() + fdeg);
            foreach (Pipe *pp, props->pipes())          // the pipe standing on it
                if (pp->isStandMounted() && pp->standId() == st->id())
                {
                    pp->setOriginX(st->originX());
                    pp->setOriginY(st->originY());
                    pp->setRunAngle(pp->runAngle() + fdeg);
                    pipes.insert(pp->id());
                }
            ++turned;
        }
        else if (auto *sc = dynamic_cast<SceneryItem *>(gi))
        {
            Scenery *s = sc->scenery();
            if (s == nullptr)
                continue;
            const QPointF o = turnPt(double(s->originX()), double(s->originY()));
            s->setOriginX(float(o.x()));
            s->setOriginY(float(o.y()));
            s->setRotation(s->rotation() + fdeg);
            ++turned;
        }
        else if (auto *tg = dynamic_cast<TargetItem *>(gi))
        {
            StageTarget *t = tg->target();
            if (t == nullptr || t->isBound())
                continue;                       // a bound mark follows its structure
            const QPointF o = turnPt(double(t->x()), double(t->y()));
            t->setX(float(o.x()));
            t->setY(float(o.y()));
            if (StageTarget::isBandKind(t->kind()))
                t->setRotation(t->rotation() + fdeg);        // the gear turns as well as moves
            ++turned;
        }
        else if (auto *ii = dynamic_cast<MonitorImageItem *>(gi))
        {
            MonitorProperties::MonitorImage img = props->image(ii->imageId());
            if (img.plane != MonitorProperties::MonitorImage::Floor)
                continue;                       // a backdrop stands where it stands
            const QPointF nc = turnPt(double(img.originX + img.width / 2.0f),
                                      double(img.originY + img.height / 2.0f));
            img.originX = float(nc.x()) - img.width / 2.0f;
            img.originY = float(nc.y()) - img.height / 2.0f;
            img.rotation += fdeg;
            props->setImage(img);
            ++turned;
        }
        else if (auto *ps = dynamic_cast<PowerSourceItem *>(gi))
        {
            QList<PowerSource> &src = m_doc->powerDistribution()->sources();
            const int i = ps->sourceIndex();
            if (i < 0 || i >= src.size())
                continue;
            outside.itemKeys << qMakePair(QStringLiteral("power"), quint32(i));
            outside.itemOrigins << QPointF(src[i].posX, src[i].posY);
            const QPointF o = turnPt(src[i].posX, src[i].posY);
            src[i].posX = o.x();
            src[i].posY = o.y();
            ++turned;
        }
        else if (auto *nn = dynamic_cast<NetworkNodeItem *>(gi))
        {
            QList<NetworkNode> &nodes = m_doc->networkTopology()->nodes();
            const int i = nn->nodeIndex();
            if (i < 0 || i >= nodes.size())
                continue;
            outside.itemKeys << qMakePair(QStringLiteral("network"), quint32(i));
            outside.itemOrigins << QPointF(nodes[i].posX, nodes[i].posY);
            const QPointF o = turnPt(nodes[i].posX, nodes[i].posY);
            nodes[i].posX = o.x();
            nodes[i].posY = o.y();
            ++turned;
        }
        else if (auto *mfi = dynamic_cast<MonitorFixtureItem *>(gi))
        {
            fixtures.insert(mfi->fixtureID());
            ++turned;
        }
    }

    // Hangers on what turned turn with it (their angle runs the other way:
    // 0 downstage, 90 stage left).
    foreach (Hanger *h, props->hangers())
        if ((h->parentKind() == Hanger::OnTruss && trusses.contains(h->parentId()))
            || (h->parentKind() == Hanger::OnTower && towers.contains(h->parentId())))
            h->setAngle(h->angle() - fdeg);

    /* The fixtures: every one selected or on something that turned has its
       facing turned; where its place is its own -- standing free, or on a
       deck -- that goes round too. Studio frames carry their members. */
    QSet<quint32> facing = fixtures;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        const quint32 fid = fx->id();
        const FixtureRigProps rp = props->fixtureRigProps(fid);
        if (trusses.contains(rp.trussId) || towers.contains(rp.towerId)
            || pipes.contains(rp.pipeId) || platforms.contains(rp.riserPlatformId))
            facing.insert(fid);
        if (platforms.contains(rp.deckPlatformId))
        {
            facing.insert(fid);
            fixtures.insert(fid);
        }
        if (rp.hangerId != UINT_MAX)
            if (Hanger *h = props->hanger(rp.hangerId))
                if ((h->parentKind() == Hanger::OnTruss && trusses.contains(h->parentId()))
                    || (h->parentKind() == Hanger::OnTower && towers.contains(h->parentId())))
                    facing.insert(fid);
        const quint32 fg = props->fixtureFrameGroup(fid);
        if (fg != 0 && (fixtures.contains(fid)
                        || (props->group(fg).anchorKind == QStringLiteral("platform")
                            && platforms.contains(props->group(fg).anchorId))))
        {
            facing.insert(fid);
            frames.insert(fg);
        }
    }
    foreach (quint32 fid, fixtures)
    {
        const FixtureRigProps rp = props->fixtureRigProps(fid);
        const bool ownPlace = rp.trussId == Truss::invalidId() && rp.pipeId == UINT_MAX
                              && rp.towerId == Tower::invalidId() && !rp.onRiser()
                              && rp.hangerId == UINT_MAX && props->fixtureFrameGroup(fid) == 0;
        if (!ownPlace)
            continue;
        const QVector3D p = props->fixturePosition(fid, 0, 0);       // mm
        const QPointF o = turnPt(double(p.x()) / 1000.0, double(p.y()) / 1000.0);
        props->setFixturePosition(fid, 0, 0, QVector3D(float(o.x() * 1000.0), float(o.y() * 1000.0), p.z()));
    }
    // A free studio frame turns about the same middle; one on a platform follows it.
    foreach (quint32 fg, frames)
    {
        const MonitorProperties::MonitorGroup g = props->group(fg);
        if (g.anchorKind == QStringLiteral("platform"))
            continue;
        const QPointF o = turnPt(double(g.origin.x()), double(g.origin.y()));
        props->setGroupFrame(fg, QVector3D(float(o.x()), float(o.y()), g.origin.z()),
                             g.rotation + fdeg);
    }
    foreach (quint32 fid, facing)
        props->turnFixtureFacing(fid, fdeg);

    props->recomputeAnchoredFrames();
    props->recomputeChildTrusses();
    props->recomputeHangers();
    if (!outside.itemKeys.isEmpty())
        pushUndo(outside);

    updateTrusses();
    updatePlatforms();
    updatePowerSources();
    updateNetworkNodes();
    updateImages();
    updateTargets();
    refreshAllItems();
    m_doc->setModified();
    emit mapStructureChanged();
    return turned;
}

int MonitorGraphicsView::ungroupSelectedItems()
{
    // Dissolve the OUTERMOST group of every selected grouped item by one level:
    // its direct child groups and items are promoted to the group's parent
    // (0 = directly under the layer for a top-level group).
    QSet<quint32> tops;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
    {
        const quint32 g = itemGroupId(gi);
        if (g != 0)
            tops.insert(topLevelGroup(g));
    }
    if (tops.isEmpty())
        return 0;

    int n = 0;
    foreach (quint32 top, tops)
    {
        n += itemsInGroup(top).size();
        ungroupById(top);
    }
    return n;
}

void MonitorGraphicsView::ungroupById(quint32 top)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (!props->hasGroup(top))
        return;

    // Promote this group's direct children (sub-groups and items) to its parent
    // (0 = directly under the layer), then drop the now-empty group.
    const quint32 parent = props->group(top).parentGroupId;
    foreach (const MonitorProperties::MonitorGroup &c, props->childGroups(top))
        props->setGroupParent(c.id, parent);
    foreach (QGraphicsItem *gi, itemsInGroup(top))
        setItemGroupId(gi, parent);
    props->removeGroup(top);

    m_doc->setModified();
    emit mapSelectionChanged();
    emit mapStructureChanged();
}

bool MonitorGraphicsView::selectionHasGroup() const
{
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (itemGroupId(gi) != 0)
            return true;
    return false;
}

bool MonitorGraphicsView::selectionGroupable() const
{
    return m_scene->selectedItems().size() >= 2;
}

void MonitorGraphicsView::selectItemsInGroup(quint32 gid)
{
    m_scene->clearSelection();
    foreach (QGraphicsItem *gi, itemsUnderGroup(gid))
        gi->setSelected(true);
}

void MonitorGraphicsView::selectMapItem(const QString &kind, quint32 id)
{
    QGraphicsItem *target = itemFor(kind, id);

    // Selecting from the tree targets exactly ONE item so it can be edited /
    // repositioned individually — suppress the select-together group extension.
    m_extendingSelection = true;
    m_scene->clearSelection();
    if (target)
    {
        if (StagePieceItem *spi = dynamic_cast<StagePieceItem *>(target))
            spi->selectNow(true);      // only ever picked on purpose
        else
            target->setSelected(true);
        target->ensureVisible();   // scroll the canvas so it's on screen
    }
    m_extendingSelection = false;
    emit mapSelectionChanged();
}

void MonitorGraphicsView::requestEditItem(const QString &kind, quint32 id)
{
    // First select just this item, then open its editor.
    selectMapItem(kind, id);
    if (kind == QStringLiteral("fixture"))       emit fixtureDoubleClicked(id);
    else if (kind == QStringLiteral("truss"))    emit trussDoubleClicked(id);
    else if (kind == QStringLiteral("platform")) emit platformDoubleClicked(id);
    else if (kind == QStringLiteral("stage"))    emit stagePieceDoubleClicked(int(id));
    else if (kind == QStringLiteral("pipe"))     emit pipeDoubleClicked(id);
    else if (kind == QStringLiteral("hanger"))   emit hangerDoubleClicked(id);
    else if (kind == QStringLiteral("stand"))    emit standDoubleClicked(id);
    else if (kind == QStringLiteral("scenery"))  emit sceneryDoubleClicked(id);
    else if (kind == QStringLiteral("tower"))    emit towerDoubleClicked(id);
    else if (kind == QStringLiteral("image"))    emit imageDoubleClicked(id);
    else if (kind == QStringLiteral("power"))    emit powerDoubleClicked(id);
    else if (kind == QStringLiteral("network"))  emit networkDoubleClicked(id);
    else if (kind == QStringLiteral("target"))   emit targetDoubleClicked(id);
}

void MonitorGraphicsView::selectMapItems(const QList<QPair<QString, quint32> > &items)
{
    /* Every row the Layers tree has selected, all of them -- the same lookup
       as a single row (itemFor()), so a stage piece or a hanger in a
       multi-selection is selected too instead of quietly dropped. */
    m_extendingSelection = true;   // suppress select-together until we're done
    m_scene->clearSelection();
    QGraphicsItem *first = nullptr;
    for (const QPair<QString, quint32> &it : items)
    {
        QGraphicsItem *target = itemFor(it.first, it.second);
        if (target == nullptr)
            continue;
        if (StagePieceItem *spi = dynamic_cast<StagePieceItem *>(target))
            spi->selectNow(true);      // only ever picked on purpose
        else
            target->setSelected(true);
        if (first == nullptr)
            first = target;
    }
    if (first != nullptr)
        first->ensureVisible();    // scroll the canvas to the first of them
    m_extendingSelection = false;
    extendSelectionToGroups();
    emit mapSelectionChanged();
}

QGraphicsItem *MonitorGraphicsView::itemFor(const QString &kind, quint32 id) const
{
    if (kind == QStringLiteral("fixture"))  return m_fixtures.value(id, nullptr);
    if (kind == QStringLiteral("truss"))    return m_trussItems.value(id, nullptr);
    if (kind == QStringLiteral("platform")) return m_platformItems.value(id, nullptr);
    if (kind == QStringLiteral("pipe"))     return m_pipeItems.value(id, nullptr);
    if (kind == QStringLiteral("stand"))    return m_standItems.value(id, nullptr);
    if (kind == QStringLiteral("scenery"))  return m_sceneryItems.value(id, nullptr);
    if (kind == QStringLiteral("tower"))    return m_towerItems.value(id, nullptr);
    if (kind == QStringLiteral("target"))   return m_targetItems.value(id, nullptr);
    if (kind == QStringLiteral("power"))
        return (int(id) < m_powerSourceItems.size()) ? m_powerSourceItems.at(int(id)) : nullptr;
    if (kind == QStringLiteral("network"))
        return (int(id) < m_networkNodeItems.size()) ? m_networkNodeItems.at(int(id)) : nullptr;
    if (kind == QStringLiteral("image"))    return m_imageItems.value(id, nullptr);
    if (kind == QStringLiteral("hanger"))   return m_hangerItems.value(id, nullptr);
    if (kind == QStringLiteral("stage"))
    {
        // A stage piece that isn't drawn has nothing on the plot to select.
        StagePieceItem *spi = m_stagePieceItems.value(int(id), nullptr);
        return (spi != nullptr && spi->isVisible()) ? spi : nullptr;
    }
    return nullptr;
}

void MonitorGraphicsView::reparentToLayer(const QList<QPair<QString, quint32> > &items, quint32 layerId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    /* A structure carrying things -- a truss shown as "T-2 · on truss" --
       moves with everything on it: its own folder goes to the layer whole.
       Pulling the truss out of its folder left its bars, hangers and
       fixtures behind in a folder that no longer stood for it, and they
       never showed as on the truss again. */
    QSet<quint32> wholeGroups;
    for (const QPair<QString, quint32> &it : items)
    {
        QGraphicsItem *gi = itemFor(it.first, it.second);
        const quint32 g = gi != nullptr ? itemGroupId(gi) : 0;
        if (g != 0 && props->hasGroup(g) && props->group(g).anchorKind == it.first
            && props->group(g).anchorId == it.second)
            wholeGroups.insert(g);
        // An assembly is one thing: it goes whole, never in pieces.
        if (const quint32 a = props->assemblyOf(g))
            wholeGroups.insert(a);
    }
    int n = 0;
    foreach (quint32 g, wholeGroups)
    {
        props->setGroupParent(g, 0);
        setGroupSubtreeLayer(g, layerId);
        n++;
    }
    for (const QPair<QString, quint32> &it : items)
    {
        QGraphicsItem *gi = itemFor(it.first, it.second);
        if (gi == nullptr || wholeGroups.contains(itemGroupId(gi))
            || wholeGroups.contains(props->assemblyOf(itemGroupId(gi))))
            continue;
        setItemGroupId(gi, 0);        // pulled out of any group
        setItemLayer(gi, layerId);
        n++;
    }
    if (n > 0)
    {
        m_doc->setModified();
        refreshItemLayerState();
        emit mapStructureChanged();
    }
}

void MonitorGraphicsView::reparentToGroup(const QList<QPair<QString, quint32> > &items, quint32 groupId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (!props->hasGroup(groupId))
        return;
    const quint32 layerId = props->group(groupId).layerId;
    int n = 0;
    for (const QPair<QString, quint32> &it : items)
    {
        QGraphicsItem *gi = itemFor(it.first, it.second);
        if (gi == nullptr)
            continue;
        setItemGroupId(gi, groupId);
        setItemLayer(gi, layerId);
        n++;
    }
    if (n > 0)
    {
        m_doc->setModified();
        refreshItemLayerState();
        emit mapStructureChanged();
    }
}

void MonitorGraphicsView::reparentGroupToLayer(quint32 groupId, quint32 layerId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (!props->hasGroup(groupId))
        return;
    props->setGroupParent(groupId, 0);
    setGroupSubtreeLayer(groupId, layerId);
    m_doc->setModified();
    refreshItemLayerState();
    emit mapStructureChanged();
}

void MonitorGraphicsView::reparentGroupToGroup(quint32 groupId, quint32 parentGroupId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (!props->hasGroup(groupId) || !props->hasGroup(parentGroupId) || groupId == parentGroupId)
        return;
    // Refuse a cycle: the new parent must not already sit under this group.
    if (groupIsUnder(parentGroupId, groupId))
        return;
    props->setGroupParent(groupId, parentGroupId);
    setGroupSubtreeLayer(groupId, props->group(parentGroupId).layerId);
    m_doc->setModified();
    refreshItemLayerState();
    emit mapStructureChanged();
}

bool MonitorGraphicsView::isGroupAnchorItem(QGraphicsItem *gi, const QString &anchorKind, quint32 anchorId) const
{
    if (anchorKind == QStringLiteral("truss"))
    {
        auto *ti = dynamic_cast<TrussItem *>(gi);
        return ti != nullptr && ti->truss() != nullptr && ti->truss()->id() == anchorId;
    }
    if (anchorKind == QStringLiteral("platform"))
    {
        auto *pi = dynamic_cast<PlatformItem *>(gi);
        return pi != nullptr && pi->platform() != nullptr && pi->platform()->id() == anchorId;
    }
    return false;
}

void MonitorGraphicsView::extendSelectionToGroups()
{
    // Skip while items are being torn down/rebuilt: the scene emits
    // selectionChanged from removeItem() and our hashes hold dangling pointers
    // mid-rebuild (dynamic_cast on those crashed).
    if (m_extendingSelection || m_itemsRebuilding)
        return;
    // When the plot is LOCKED nothing moves, so a structure's parts need not
    // come along: a click inspects just the thing clicked.
    if (m_layoutLocked)
        return;
    m_extendingSelection = true;

    MonitorProperties *props = m_doc->monitorProperties();

    /* One rule for everything on the plot: a click selects exactly what was
       clicked ("if you grab a pair with a line, just that pair .. if you
       grab an individual, just that individual"). A plot group -- a folder,
       a fill's performer lines, a fountain -- is data: it is selected from
       its Plot layers row or with Select Group, never by clicking one of
       its members. It used to be: one person of a choir took the whole set.

       The one thing a click still brings along is a structure's own parts:
       its dedicated group (anchorKind set: the auto-group ensureTrussGroup()/
       ensurePlatformGroup() keeps for a truss or riser and the bars and
       fixtures rigged on it) when the structure ITSELF is what was clicked.
       That is one item made of linked parts, like a line with its points.
       A member clicked stays solo: a fixture repositioned on its truss must
       not drag the truss along. Only the structure's own group -- not a
       folder the structure has been filed in. */
    QSet<quint32> parts;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
    {
        const quint32 g = itemGroupId(gi);
        if (g == 0 || !props->hasGroup(g))
            continue;
        /* A built assembly -- a fountain's base, walls and flame -- is one
           thing: any part clicked takes all of it, unless it was entered
           (double-click) to work on one part. */
        const quint32 a = props->assemblyOf(g);
        if (a != 0 && a != m_enteredAssembly)
        {
            parts.insert(a);
            continue;
        }
        const MonitorProperties::MonitorGroup grp = props->group(g);
        if (!grp.anchorKind.isEmpty() && isGroupAnchorItem(gi, grp.anchorKind, grp.anchorId))
            parts.insert(g);
    }
    foreach (quint32 g, parts)
        foreach (QGraphicsItem *gi, itemsUnderGroup(g))
            if (!gi->isSelected())
                gi->setSelected(true);

    m_extendingSelection = false;
    updateTrussAnchorLines();   // tether color follows the truss's selected state
    emit mapSelectionChanged();
}

quint32 MonitorGraphicsView::assemblyOfItem(QGraphicsItem *gi) const
{
    return gi != nullptr ? m_doc->monitorProperties()->assemblyOf(itemGroupId(gi)) : 0;
}

void MonitorGraphicsView::leaveAssembly()
{
    m_enteredAssembly = 0;
}

quint32 MonitorGraphicsView::groupCarriedBySelection(QGraphicsItem *gi) const
{
    const quint32 g = itemGroupId(gi);
    MonitorProperties *props = m_doc->monitorProperties();
    if (g == 0 || !props->hasGroup(g))
        return 0;
    const quint32 top = topLevelGroup(g);
    // A structure's part (a fixture on its truss) stays filed with it, and
    // an assembly's part with its assembly.
    if (!props->group(g).anchorKind.isEmpty() || props->assemblyOf(g) != 0)
        return top;
    foreach (QGraphicsItem *other, itemsUnderGroup(top))
        if (!other->isSelected())
            return 0;
    return top;
}

quint32 MonitorGraphicsView::selectableGroupOf(const QString &kind, quint32 id) const
{
    /* The group Select Group picks for an item: the folder it is filed in.
       A structure's own parts group is the structure (a click already takes
       it), so for the structure itself it is the folder around that. */
    QGraphicsItem *gi = itemFor(kind, id);
    quint32 g = (kind == QLatin1String("fixture")) ? m_doc->monitorProperties()->fixtureGroup(id)
                                                   : (gi != nullptr ? itemGroupId(gi) : 0);
    MonitorProperties *props = m_doc->monitorProperties();
    if (g == 0 || !props->hasGroup(g))
        return 0;
    const MonitorProperties::MonitorGroup grp = props->group(g);
    if (gi != nullptr && !grp.anchorKind.isEmpty() && isGroupAnchorItem(gi, grp.anchorKind, grp.anchorId))
        g = grp.parentGroupId;
    // A part of an assembly: the click already takes the assembly, so its
    // group is the folder the assembly is filed in.
    if (const quint32 a = props->assemblyOf(g))
        g = props->group(a).parentGroupId;
    return (g != 0 && props->hasGroup(g)) ? g : 0;
}

void MonitorGraphicsView::selectGroupOf(const QString &kind, quint32 id)
{
    const quint32 g = selectableGroupOf(kind, id);
    if (g == 0)
        return;
    selectItemsInGroup(g);
    emit mapSelectionChanged();
}

QString MonitorGraphicsView::similarKey(const QString &kind, quint32 id) const
{
    /* What "the same kind" means for each kind of thing on the plot, for
       Select All Similar: a fixture's model and mode; a target's kind -- and
       for people, the same set (a fill's lines; loose ones together); a
       structure's type. Empty: nothing to match. */
    MonitorProperties *props = m_doc->monitorProperties();
    if (kind == QLatin1String("fixture"))
    {
        Fixture *f = m_doc->fixture(id);
        return f != nullptr ? QStringLiteral("fixture:") + CreateFixtureGroup::fixtureModelKey(f) : QString();
    }
    if (kind == QLatin1String("target"))
    {
        const StageTarget *t = props->stageTarget(id);
        if (t == nullptr)
            return QString();
        if (t->isPerformerLine())
            return QStringLiteral("people:%1").arg(t->groupId());
        return QStringLiteral("target:") + StageTarget::kindToString(t->kind());
    }
    if (kind == QLatin1String("truss"))
    {
        const Truss *t = props->truss(id);
        return t != nullptr ? QStringLiteral("truss:%1:%2").arg(int(t->type())).arg(int(t->profile())) : QString();
    }
    if (kind == QLatin1String("platform"))
    {
        const StagePlatform *p = props->platform(id);
        return p != nullptr ? QStringLiteral("platform:%1").arg(int(p->shape())) : QString();
    }
    if (kind == QLatin1String("pipe"))
    {
        const Pipe *p = props->pipe(id);
        return p != nullptr ? QStringLiteral("pipe:%1").arg(int(p->orientation())) : QString();
    }
    if (kind == QLatin1String("scenery"))
    {
        const Scenery *s = props->scenery(id);
        return s != nullptr ? QStringLiteral("scenery:%1").arg(s->kind()) : QString();
    }
    if (kind == QLatin1String("network"))
    {
        const NetworkTopology *nt = m_doc->networkTopology();
        if (nt == nullptr || int(id) >= nt->nodes().size())
            return QString();
        return QStringLiteral("network:%1").arg(nt->nodes().at(int(id)).transport);
    }
    // One kind of each: every stand, tower, hanger, image or power source.
    if (kind == QLatin1String("stand") || kind == QLatin1String("tower") || kind == QLatin1String("hanger")
        || kind == QLatin1String("image") || kind == QLatin1String("power"))
        return itemFor(kind, id) != nullptr ? kind : QString();
    return QString();
}

QList<QPair<QString, quint32> > MonitorGraphicsView::similarObjects(const QString &kind, quint32 id) const
{
    QList<QPair<QString, quint32> > out;
    const QString key = similarKey(kind, id);
    if (key.isEmpty())
        return out;
    QList<quint32> ids;
    if (kind == QLatin1String("fixture"))       ids = m_fixtures.keys();
    else if (kind == QLatin1String("target"))   ids = m_targetItems.keys();
    else if (kind == QLatin1String("truss"))    ids = m_trussItems.keys();
    else if (kind == QLatin1String("platform")) ids = m_platformItems.keys();
    else if (kind == QLatin1String("pipe"))     ids = m_pipeItems.keys();
    else if (kind == QLatin1String("stand"))    ids = m_standItems.keys();
    else if (kind == QLatin1String("scenery"))  ids = m_sceneryItems.keys();
    else if (kind == QLatin1String("tower"))    ids = m_towerItems.keys();
    else if (kind == QLatin1String("hanger"))   ids = m_hangerItems.keys();
    else if (kind == QLatin1String("image"))    ids = m_imageItems.keys();
    else if (kind == QLatin1String("power"))
        for (int i = 0; i < m_powerSourceItems.size(); ++i) ids << quint32(i);
    else if (kind == QLatin1String("network"))
        for (int i = 0; i < m_networkNodeItems.size(); ++i) ids << quint32(i);
    std::sort(ids.begin(), ids.end());
    for (quint32 other : ids)
        if (other == id || similarKey(kind, other) == key)
            out << qMakePair(kind, other);
    return out;
}

void MonitorGraphicsView::selectSimilarTo(const QString &kind, quint32 id)
{
    const QList<QPair<QString, quint32> > same = similarObjects(kind, id);
    if (same.isEmpty())
        return;
    selectMapItems(same);
    emit selectInLayersRequested(same);     // the ones buried on a structure too
}

void MonitorGraphicsView::setStageFeaturesOnly(bool on)
{
    if (m_stageOnly == on)
        return;
    m_stageOnly = on;
    refreshItemLayerState();
}

QGraphicsItem *MonitorGraphicsView::topPickableAt(const QPointF &scenePos) const
{
    // Highest-z item at the point that isn't a ghosted (click-through) fixture.
    const QList<QGraphicsItem *> hits = m_scene->items(scenePos);
    for (QGraphicsItem *gi : hits)
    {
        QGraphicsItem *top = gi;
        while (top && top->parentItem())
            top = top->parentItem();
        if (auto *mfi = dynamic_cast<MonitorFixtureItem *>(top))
            if (mfi->isGhosted())
                continue;   // see through the ghost to the structure below
        /* Nor anything that takes no clicks at all: the count bubble on a
           buried structure, aim and anchor lines, the reach overlay. A single
           click already passes through them -- Qt skips an item that accepts
           no buttons -- but this picked them anyway, so double-clicking the
           top of a tower mostly landed on its bubble or on an anchor line and
           opened nothing ("if I move around the top and keep double clicking
           I eventually open it"). */
        if (top->acceptedMouseButtons() == Qt::NoButton)
            continue;
        return top;
    }
    return nullptr;
}

void MonitorGraphicsView::refreshItemLayerState()
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr)
        return;
    auto repaintAll = qScopeGuard([this]() { if (m_scene) m_scene->update(); });

    // What holds an item still, by SOURCE:
    //   • LAYER lock → the item can't be moved, draws red (told to the item:
    //     ItemLock::LayerLockedKey) and is CLICK-THROUGH on the plot -- a click
    //     or a rubber band reaches what is under it (Q178 #5). It stays
    //     selectable, so the Layers tree can still select it, and Properties
    //     and the menu work on it.
    //   • its OWN lock → can't be moved, draws red, but CAN still be selected
    //     (so it can be unlocked from its menu).
    //   • folder lock / global "Edit Plot" lock → can't be moved, but CAN
    //     still be selected (so it can be inspected / rubber-banded).
    auto applySelectable = [](QGraphicsItem *gi, bool open) {
        applyLayerLock(gi, !open);
    };
    // Fixtures (no per-item lock): layer drives visibility + selectability;
    // movable unless the global layout or the layer is locked.
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
    {
        const MonitorProperties::MonitorLayer lyr = props->layer(props->fixtureLayer(it.key()));
        const bool grpLock = props->groupChainLocked(props->fixtureGroup(it.key()));
        it.value()->setVisible(fixtureItemShouldBeVisible(it.key()));
        // Drawn red exactly when it's actually non-interactive — a locked
        // layer already makes it unselectable/click-through below; this
        // makes that visible, the same way as every other locked object.
        it.value()->setGhosted(lyr.locked);
        it.value()->setMovable(!m_layoutLocked && !lyr.locked && !grpLock);
        applySelectable(it.value(), !lyr.locked);
    }

    // Trusses: global + per-truss + group + layer lock all freeze dragging; only
    // the LAYER lock also removes selectability.
    foreach (TrussItem *ti, m_trussItems)
    {
        Truss *t = ti->truss();
        const MonitorProperties::MonitorLayer lyr = props->layer(t ? t->layerId() : 0);
        const bool grpLock = props->groupChainLocked(t ? t->groupId() : 0);
        ti->setVisible(lyr.visible);
        ti->setMovable(!m_layoutLocked && !(t && t->locked()) && !lyr.locked && !grpLock);
        applySelectable(ti, !lyr.locked);
    }

    // Platforms.
    foreach (PlatformItem *pi, m_platformItems)
    {
        StagePlatform *p = pi->platform();
        const MonitorProperties::MonitorLayer lyr = props->layer(p ? p->layerId() : 0);
        const bool grpLock = props->groupChainLocked(p ? p->groupId() : 0);
        pi->setVisible(lyr.visible);
        pi->setMovable(!m_layoutLocked && !(p && p->locked()) && !lyr.locked && !grpLock);
        applySelectable(pi, !lyr.locked);
    }

    // Power sources (model lives in PowerDistribution, indexed positionally).
    PowerDistribution *pd = m_doc->powerDistribution();
    foreach (PowerSourceItem *ps, m_powerSourceItems)
    {
        const int s = ps->sourceIndex();
        if (s < 0 || s >= pd->sources().size())
            continue;
        const PowerSource &src = pd->sources().at(s);
        const MonitorProperties::MonitorLayer lyr = props->layer(src.layerId);
        ps->setVisible(lyr.visible && !m_stageOnly);
        ps->setMovable(!m_layoutLocked && !src.locked && !lyr.locked);
        applySelectable(ps, !lyr.locked);
    }

    // Network nodes (model lives in NetworkTopology, indexed positionally).
    NetworkTopology *nt = m_doc->networkTopology();
    foreach (NetworkNodeItem *nn, m_networkNodeItems)
    {
        const int n = nn->nodeIndex();
        if (n < 0 || n >= nt->nodes().size())
            continue;
        const NetworkNode &node = nt->nodes().at(n);
        const MonitorProperties::MonitorLayer lyr = props->layer(node.layerId);
        nn->setVisible(lyr.visible && !m_stageOnly);
        nn->setMovable(!m_layoutLocked && !node.locked && !lyr.locked);
        applySelectable(nn, !lyr.locked);
        // Receivers and their links live on the same layer as their transmitter.
        for (int r = 0; r < m_networkReceiverItems.size(); r++)
        {
            if (m_networkReceiverItems.at(r)->nodeIndex() != n)
                continue;
            m_networkReceiverItems.at(r)->setVisible(lyr.visible && !m_stageOnly);
            if (r < m_networkLinks.size())
                m_networkLinks.at(r)->setVisible(lyr.visible && !m_stageOnly);
        }
    }

    // Targets: draggable in Design mode. The plot lock holds them like
    // everything else (Q183) unless "Targets stay adjustable" is on; a locked
    // layer always freezes them; a hidden layer hides.
    foreach (TargetItem *ti, m_targetItems)
    {
        StageTarget *t = ti->target();
        const MonitorProperties::MonitorLayer lyr = props->layer(t ? t->layerId() : 0);
        const bool grpLock = props->groupChainLocked(t ? t->groupId() : 0);
        ti->setVisible(lyr.visible && !m_stageOnly && (ti->isSelected() || targetInScope(ti->targetId())));
        /* t->locked() belongs in here. It was left out, so the per-target
           "Lock Target" menu item set a flag that this function overwrote on
           the very next refresh -- locking a target did nothing at all, while
           the equivalent line for trusses above has always honoured it. */
        ti->setMovable(m_doc->mode() == Doc::Design && targetsFreeOfPlotLock()
                       && !(t && t->locked()) && !lyr.locked && !grpLock);
        applySelectable(ti, !lyr.locked);
    }

    // Placeable images.
    for (auto it = m_imageItems.constBegin(); it != m_imageItems.constEnd(); ++it)
    {
        const MonitorProperties::MonitorImage img = props->image(it.key());
        const MonitorProperties::MonitorLayer lyr = props->layer(img.layerId);
        const bool grpLock = props->groupChainLocked(img.groupId);
        it.value()->setVisible(lyr.visible);
        it.value()->setMovable(!m_layoutLocked && !img.locked && !lyr.locked && !grpLock);
        applySelectable(it.value(), !lyr.locked);
    }

    // Pipes (booms/electrics), Stands and Towers — same rule as platforms.
    foreach (PipeItem *bi, m_pipeItems)
    {
        Pipe *p = bi->pipe();
        const MonitorProperties::MonitorLayer lyr = props->layer(p ? p->layerId() : 0);
        bi->setVisible(lyr.visible);
        bi->setMovable(!m_layoutLocked && !(p && p->locked()) && !lyr.locked);
        applySelectable(bi, !lyr.locked);
    }
    foreach (StandItem *si, m_standItems)
    {
        Stand *s = si->stand();
        const MonitorProperties::MonitorLayer lyr = props->layer(s ? s->layerId() : 0);
        si->setVisible(lyr.visible);
        si->setMovable(!m_layoutLocked && !(s && s->locked()) && !lyr.locked);
        applySelectable(si, !lyr.locked);
    }
    foreach (SceneryItem *sc, m_sceneryItems)
    {
        Scenery *s = sc->scenery();
        const MonitorProperties::MonitorLayer lyr = props->layer(s ? s->layerId() : 0);
        sc->setVisible(lyr.visible);
        sc->setMovable(!m_layoutLocked && !(s && s->locked()) && !lyr.locked);
        sc->showLabel(lyr.labels);
        applySelectable(sc, !lyr.locked);
    }
    foreach (TowerItem *twi, m_towerItems)
    {
        Tower *tw = twi->tower();
        const MonitorProperties::MonitorLayer lyr = props->layer(tw ? tw->layerId() : 0);
        twi->setVisible(lyr.visible);
        twi->setMovable(!m_layoutLocked && !(tw && tw->locked()) && !lyr.locked);
        applySelectable(twi, !lyr.locked);
    }

    // Elevation (Front/Side) views are read-only — EXCEPT bars we allow to be
    // dragged there for WYSIWYG placement (tower crossbars in Front).
    if (isElevation())
    {
        for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        {
            /* A truss-bound fixture slides along its truss (and, since the
               vertical-drag work, up and down off it). A FREE-PLACED one --
               trussId invalid -- was not draggable here at all, because
               elevationFixtureDraggable() requires a binding. That is what
               made the XL-450 impossible to grab in Side view: it is not
               bound to anything, so the elevation rule refused it outright,
               in every elevation, forever. It moves in the plane of the view
               like everything else that is unlocked. */
            const FixtureRigProps frp = props->fixtureRigProps(it.key());
            const bool bound = (frp.trussId != Truss::invalidId());
            const MonitorProperties::MonitorLayer flyr =
                props->layer(props->fixtureLayer(it.key()));
            const bool frozen = m_layoutLocked || flyr.locked
                              || props->groupChainLocked(props->fixtureGroup(it.key()));
            // Held is held in every view: a bound fixture's slide along its
            // truss ignored the plot and layer locks here.
            it.value()->setMovable(!frozen && (!bound || elevationFixtureDraggable(it.key())));
        }
        /* Trusses: a tower crossbar has always been draggable here, and a
           free-standing truss now is too -- dragging one up or down in Front is
           how you set its trim height, and refusing it silently was the "can't
           move the XL-450 and it is unlocked" report. Locks still apply. */
        foreach (TrussItem *i, m_trussItems)
        {
            Truss *t = i->truss();
            const bool locked = (t != nullptr && t->locked()) || m_layoutLocked
                              || props->layer(t ? t->layerId() : 0).locked;
            // A tower crossbar, a free truss: either drags here unless held.
            i->setMovable(!locked);
        }
        foreach (PlatformItem *i, m_platformItems)       i->setMovable(false);
        foreach (PowerSourceItem *i, m_powerSourceItems) i->setMovable(false);
        foreach (NetworkNodeItem *i, m_networkNodeItems) i->setMovable(false);
        /* Targets were frozen here outright. Their drop is unprojected for the
           current view now, so dragging one in an elevation sets its HEIGHT --
           which is the only way to give a target a Z at all. */
        foreach (TargetItem *i, m_targetItems)
        {
            StageTarget *t = i->target();
            i->setMovable(m_doc->mode() == Doc::Design && targetsFreeOfPlotLock()
                          && !(t && t->locked())
                          && !props->layer(t ? t->layerId() : 0).locked
                          && !props->groupChainLocked(t ? t->groupId() : 0));
        }
        foreach (PipeItem *i, m_pipeItems)               i->setMovable(false);
        foreach (StandItem *i, m_standItems)             i->setMovable(false);
        foreach (SceneryItem *i, m_sceneryItems)         i->setMovable(false);
        foreach (TowerItem *i, m_towerItems)             i->setMovable(false);
    }

    updateTrussAnchorLines();
    scheduleReachOverlay();
    rebuildProjectionOverlay();
    /* Badges last: they decide which fixtures are buried, so run them after the
       layer pass and settle fixture visibility once more. */
    refreshFixtureCountBadges();
    for (auto fit = m_fixtures.constBegin(); fit != m_fixtures.constEnd(); ++fit)
        if (fit.value() != nullptr)
            fit.value()->setVisible(fixtureItemShouldBeVisible(fit.key()));
    foreach (HangerItem *hgi, m_hangerItems)
    {
        // Shown with what it hangs off (Plot layers lists it there too).
        Hanger *h = props->hanger(hgi->hangerId());
        quint32 lid = h ? h->layerId() : 0;
        if (h != nullptr && h->parentKind() == Hanger::OnTruss && props->truss(h->parentId()) != nullptr)
            lid = props->truss(h->parentId())->layerId();
        else if (h != nullptr && h->parentKind() == Hanger::OnTower && props->tower(h->parentId()) != nullptr)
            lid = props->tower(h->parentId())->layerId();
        hgi->setVisible(props->layer(lid).visible && props->layer(h ? h->layerId() : 0).visible);
    }
    updateStageMarks();         // the stage pieces, on their own layer
    refreshShapeHandles();      // shown only while nothing holds it
    emit layerStateChanged();   // the rig view follows what is shown here
}

void MonitorGraphicsView::setSnapDivisions(int divisions)
{
    if (divisions < 0)
        divisions = 0;
    m_snapDivisions = divisions;
    applySnapToAllItems();
}

void MonitorGraphicsView::applySnapToItem(MonitorFixtureItem *item)
{
    if (item != NULL)
        item->setSnap(m_snapDivisions, m_cellPixels, m_xOffset, m_yOffset);
}

void MonitorGraphicsView::applySnapToAllItems()
{
    foreach (MonitorFixtureItem *item, m_fixtures)
        applySnapToItem(item);
}

void MonitorGraphicsView::setGridMetrics(float value)
{
    m_unitValue = value;
    // Everything on the stage is placed through m_unitValue, not just the
    // fixtures -- so everything has to be re-placed when it changes.
    refreshAllItems();
    emit rulersChanged();
}

quint32 MonitorGraphicsView::selectedFixtureID()
{
    MonitorFixtureItem *item = getSelectedItem();
    if (item != NULL)
        return item->fixtureID();
    else
        return Fixture::invalidId();
}

QList<quint32> MonitorGraphicsView::fixturesID() const
{
    return m_fixtures.keys();
}

/* NB throughout: read m_fixtures with value(), NEVER operator[].
   QHash::operator[] on a non-const hash INSERTS a default-constructed value
   when the key is missing, so a lookup for a fixture that is not on the plot
   (a truss-mounted one edited only in the studio, say) left {id, nullptr}
   behind. updateFixture()'s contains() guard then passed the null straight
   through to item->setSize() -- a SIGSEGV on releasing a studio drag. */
void MonitorGraphicsView::setFixtureGelColor(quint32 id, QColor col)
{
    MonitorFixtureItem *item = m_fixtures.value(id, NULL);
    if (item != NULL)
        item->setGelColor(col);
}

void MonitorGraphicsView::setFixtureRotation(quint32 id, ushort degrees)
{
    MonitorFixtureItem *item = m_fixtures.value(id, NULL);
    if (item != NULL)
        item->setRotation(degrees);
}

void MonitorGraphicsView::showFixturesLabels(bool visible)
{
    // Master toggle: bulk-set EVERY layer's labels flag, then render. The
    // per-layer flag is the source of truth for what actually shows, so the
    // per-layer toggles work regardless of this master's saved state.
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (const MonitorProperties::MonitorLayer &lyr, props->layers())
        props->setLayerLabels(lyr.id, visible);
    props->setLabelsVisible(visible);
    refreshFixtureLabels();
}

void MonitorGraphicsView::refreshFixtureLabels()
{
    // Rendering is driven purely by each item's LAYER labels flag.
    MonitorProperties *props = m_doc->monitorProperties();
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        it.value()->showLabel(props->layer(props->fixtureLayer(it.key())).labels);

    foreach (PlatformItem *pi, m_platformItems)
        if (StagePlatform *p = pi->platform())
            pi->showLabel(props->layer(p->layerId()).labels);
}

QRectF MonitorGraphicsView::itemGeometryRect(const QGraphicsItem *item)
{
    if (item == nullptr)
        return QRectF();
    if (auto *f = dynamic_cast<const MonitorFixtureItem *>(item))
        return f->iconRect();
    if (auto *t = dynamic_cast<const TargetItem *>(item))
        return t->geometryRect();
    if (auto *tr = dynamic_cast<const TrussItem *>(item))
        return tr->footprintRect();
    if (auto *tw = dynamic_cast<const TowerItem *>(item))
        return tw->footprintRect();
    if (auto *pl = dynamic_cast<const PlatformItem *>(item))
        return pl->geometryRect();
    if (auto *pp = dynamic_cast<const PipeItem *>(item))
        return pp->geometryRect();
    if (auto *st = dynamic_cast<const StandItem *>(item))
        return st->geometryRect();
    if (auto *sc = dynamic_cast<const SceneryItem *>(item))
        return sc->geometryRect();
    if (auto *sp = dynamic_cast<const StagePieceItem *>(item))
        return sp->geometryRect();
    return item->boundingRect();
}

QRectF MonitorGraphicsView::selectionViewportRect() const
{
    /* Whatever is selected, not only fixtures: a drape, a truss or a deck
       being moved reads on the rulers the same way. Each item's own
       geometry, mapped through its transform (a turned deck) and the view's
       (zoom, scroll, a quarter turn), unrounded: the band starts and ends
       where the item does. sceneBoundingRect() was the paint extent -- a
       target's name to its right made the band a foot and more wider, and
       off centre ("the band does not line up with the target"). */
    const QList<QGraphicsItem *> items = m_scene->selectedItems();
    QRectF out;
    foreach (QGraphicsItem *it, items)
    {
        const QRectF g = itemGeometryRect(it);
        if (g.isNull())
            continue;
        const QRectF vp = viewportTransform().map(it->mapToScene(g)).boundingRect();
        out = out.isNull() ? vp : out.united(vp);
    }
    return out;
}

QColor MonitorGraphicsView::fixtureGelColor(quint32 id)
{
    MonitorFixtureItem *item = m_fixtures.value(id, NULL);
    if (item == NULL)
        return QColor();

    return item->getColor();
}

/* Which structure a fixture is mounted on, as a "kind:id" key ("" = none). */
static QString mountKeyFor(const FixtureRigProps &rp)
{
    if (rp.trussId != Truss::invalidId())
        return QStringLiteral("truss:%1").arg(rp.trussId);
    if (rp.onTower())
        return QStringLiteral("tower:%1").arg(rp.towerId);
    if (rp.onPipe())
        return QStringLiteral("pipe:%1").arg(rp.pipeId);
    if (rp.onRiser())
        return QStringLiteral("platform:%1").arg(rp.riserPlatformId);
    if (rp.onDeck())
        return QStringLiteral("platform:%1").arg(rp.deckPlatformId);
    return QString();
}

/* Scene-space centre of a structure's item, where its count badge sits. */
static QPointF centreOf(const QGraphicsItem *it)
{
    return it ? it->sceneBoundingRect().center() : QPointF();
}

double MonitorGraphicsView::structureCoverageByItsFixtures(const QString &kind, quint32 id,
                                                           QList<quint32> *fixturesOut,
                                                           double *stackFactorOut) const
{
    const QGraphicsItem *host = nullptr;
    QRectF hostRect;
    /* An end-on truss in an elevation: updateTrusses() clamps a run that
       projects to near nothing (pxLen < pxWid) up to pxWid so it stays
       visible ("end-on trusses stay visible") -- but that leaves footprintRect()
       a tiny pxWid-square with no relation to how much of the truss is
       actually on screen. Fixtures spread along the real run still project
       close together there and trivially "cover" that fake square, so a
       truss running along the axis this view has collapsed reported buried
       when nothing about it is actually obscured ("t1 .. on a side view when
       the fixtures are not obscuring the truss"). Horizontal only: a Top-view
       tower's own pxLen==pxWid circle IS its real cross-section, not an
       artefact, and must still bury normally. */
    bool endOnElevation = false;
    if (kind == QStringLiteral("truss"))
    {
        TrussItem *ti = m_trussItems.value(id, nullptr);
        host = ti;
        // The BODY, not boundingRect(): a vertical truss pads its rect out to
        // cover the base plate, which is ~4x its real area and made a tower
        // buried in its own fixtures measure 31%.
        if (ti != nullptr)
        {
            hostRect = ti->mapRectToScene(ti->footprintRect());
            endOnElevation = (ti->truss() != nullptr
                               && ti->truss()->type() != Truss::Vertical
                               && ti->pxLen() <= ti->pxWid());
        }
    }
    else if (kind == QStringLiteral("tower"))
    {
        /* The tower, not its base plate: boundingRect() takes in the plate
           drawn round it in Top, about five times the tower, so a fixture
           sitting over the whole tower measured a fifth of it and never
           brought up the count ("the light overshadowing the feature"). */
        TowerItem *twi = m_towerItems.value(id, nullptr);
        host = twi;
        if (twi != nullptr)
            hostRect = twi->mapRectToScene(twi->footprintRect());
    }
    else if (kind == QStringLiteral("pipe"))     host = m_pipeItems.value(id, nullptr);
    else if (kind == QStringLiteral("platform")) host = m_platformItems.value(id, nullptr);
    if (host == nullptr)
        return 0.0;
    if (hostRect.isNull())
        hostRect = host->sceneBoundingRect();
    const double hostArea = hostRect.width() * hostRect.height();
    if (hostArea <= 0.0)
        return 0.0;

    MonitorProperties *props = m_doc->monitorProperties();
    const QString key = QStringLiteral("%1:%2").arg(kind).arg(id);

    /* Union, not a sum: two fixtures side by side on a truss cover twice as
       much as one, but two stacked on the same spot do not. */
    QRegion covered;
    double summed = 0.0;      // areas added up, overlaps counted every time
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
    {
        if (it.value() == nullptr)
            continue;
        if (mountKeyFor(props->fixtureRigProps(it.key())) != key)
            continue;
        if (fixturesOut != nullptr)
            *fixturesOut << it.key();
        /* The ICON, not sceneBoundingRect(): that includes the name label and
           the selection halo, so a fixture with a long name would "bury" a
           truss it barely touches. Measured at 129 px wide for a 300 mm
           fixture whose body is about 29 -- almost all of it text. */
        const QRectF icon = it.value()->mapRectToScene(it.value()->iconRect());
        const QRectF r = icon.intersected(hostRect);
        if (r.isEmpty() == false)
        {
            covered += r.toAlignedRect();
            summed += double(r.width()) * double(r.height());
        }
    }

    /* Range-for, not foreach: QRegion is not implicitly shared, so Q_FOREACH
       copies the whole region to walk it -- what Qt deprecates it for. */
    double coveredArea = 0.0;
    const QRegion coveredRects = covered;
    for (const QRect &r : coveredRects)
        coveredArea += double(r.width()) * double(r.height());

    /* How many fixtures deep, on average, the covered part is. 1.0 = laid out
       side by side; 8.0 = eight of them on one spot. */
    if (stackFactorOut != nullptr)
        *stackFactorOut = (coveredArea > 0.0) ? (summed / coveredArea) : 0.0;

    // Foreshortened to a fake square, not really covered -- see endOnElevation above.
    if (endOnElevation)
        return 0.0;

    return qBound(0.0, coveredArea / hostArea, 1.0);
}

void MonitorGraphicsView::refreshFixtureCountBadges()
{
    if (m_doc == nullptr || m_doc->monitorProperties() == nullptr)
        return;

    QList<QPair<QString, quint32> > structures;
    foreach (quint32 id, m_trussItems.keys())    structures << qMakePair(QStringLiteral("truss"), id);
    foreach (quint32 id, m_towerItems.keys())    structures << qMakePair(QStringLiteral("tower"), id);
    foreach (quint32 id, m_pipeItems.keys())     structures << qMakePair(QStringLiteral("pipe"), id);
    foreach (quint32 id, m_platformItems.keys()) structures << qMakePair(QStringLiteral("platform"), id);

    m_buriedByOwnFixtures.clear();
    QSet<QString> wanted;

    for (const QPair<QString, quint32> &sk : structures)
    {
        QList<quint32> fids;
        double stack = 0.0;
        const double cover = structureCoverageByItsFixtures(sk.first, sk.second, &fids, &stack);
        if (fids.isEmpty())
            continue;

        /* Unchecked: always badge, so hiding the rig never loses the count.
           Checked: badge only where the fixtures actually bury the structure. */
        /* Two different reasons to show a count, and they must not look alike.
           BURIED: the fixtures really do cover the structure in this view, so
           they are hidden whatever the toggle says. HIDDEN BY CHOICE: you
           unchecked "Fixtures on the rig", which hides every mounted fixture
           regardless of how much it covers. Drawn identically, the second reads
           as the first misfiring -- "the lights on the platforms do NOT cover
           more than 40% .. so that's a fail" -- when nothing had misfired. */
        /* AREA only, as specified. A "stacked" trigger (fixtures piled on one
           footprint, union tiny but summed area large) was tried and removed:
           it fired on the LED tape up a step's face, which is precisely the
           case that should stay visible. Stacking is still measured -- see
           stack below -- but it does not hide anything on its own. */
        const bool buries = (cover > kBuryThreshold);
        Q_UNUSED(stack)
        const bool byChoice = (m_showMountedFixtures == false);
        if (!buries && !byChoice)
            continue;

        if (m_showMountedFixtures)
        {
            // Braces: foreach expands to an if/else, so a braceless if around
            // one is a dangling else.
            foreach (quint32 fid, fids)
                m_buriedByOwnFixtures.insert(fid);
        }

        const QString key = QStringLiteral("%1:%2").arg(sk.first).arg(sk.second);
        wanted.insert(key);

        QGraphicsItemGroup *badge = m_countBadges.value(key, nullptr);
        QGraphicsEllipseItem *disc = nullptr;
        QGraphicsSimpleTextItem *num = nullptr;
        if (badge == nullptr)
        {
            badge = new QGraphicsItemGroup;
            disc = new QGraphicsEllipseItem(badge);
            num  = new QGraphicsSimpleTextItem(badge);
            badge->addToGroup(disc);
            badge->addToGroup(num);
            badge->setZValue(3.0);                    // above fixtures (2) and their halo
            badge->setAcceptedMouseButtons(Qt::NoButton);   // never steal the click
            /* "when I zoom in, the blue circles can stay the same size so
               what's under them is more easily clickable." A fixed screen
               size (and upright under plot rotation): zooming in grows the
               structure and its fixtures, never the bubble over them. */
            badge->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
            m_scene->addItem(badge);
            m_countBadges.insert(key, badge);
        }
        else
        {
            disc = dynamic_cast<QGraphicsEllipseItem *>(badge->childItems().value(0));
            num  = dynamic_cast<QGraphicsSimpleTextItem *>(badge->childItems().value(1));
        }
        if (disc == nullptr || num == nullptr)
            continue;

        const int n = fids.count();
        QFont f = font();
        f.setBold(true);
        f.setPointSizeF(9.0);
        num->setFont(f);
        num->setText(QString::number(n));
        num->setPen(Qt::NoPen);

        const QRectF tb = num->boundingRect();
        const double d = qMax(16.0, qMax(tb.width(), tb.height()) + 8.0);
        // Centred on the badge's own origin, which sits on the structure's
        // centre: an item that ignores the view's scale must be anchored at
        // the point it marks, or zooming slides it off that point.
        disc->setRect(-d * 0.5, -d * 0.5, d, d);
        /* Filled blue = these fixtures bury their structure (the mandated
           look). Hollow = they are hidden because "Fixtures on the rig" is
           off. Keyed on the MODE, not on the individual structure: with the
           toggle off every badge means the same thing, and mixing filled and
           hollow in that one view just reads as inconsistency. */
        if (m_showMountedFixtures)
        {
            disc->setBrush(QBrush(QColor(40, 110, 220)));
            disc->setPen(QPen(QColor(230, 240, 255), 1.0));
            num->setBrush(QBrush(Qt::white));
        }
        else
        {
            disc->setBrush(Qt::NoBrush);
            disc->setPen(QPen(QColor(90, 150, 230), 1.5));
            num->setBrush(QBrush(QColor(150, 195, 245)));
        }
        badge->setToolTip(m_showMountedFixtures
            ? tr("%n fixture(s) here cover this and are hidden in this view", "", n)
            : tr("%n fixture(s) here, hidden by “Fixtures on the rig”", "", n));
        num->setPos(-tb.width() * 0.5, -tb.height() * 0.5);

        const QGraphicsItem *hostItem =
              (sk.first == QStringLiteral("truss"))    ? (QGraphicsItem *)m_trussItems.value(sk.second, nullptr)
            : (sk.first == QStringLiteral("tower"))    ? (QGraphicsItem *)m_towerItems.value(sk.second, nullptr)
            : (sk.first == QStringLiteral("pipe"))     ? (QGraphicsItem *)m_pipeItems.value(sk.second, nullptr)
            : (QGraphicsItem *)m_platformItems.value(sk.second, nullptr);
        badge->setPos(centreOf(hostItem));
        badge->setVisible(true);
    }

    // Drop badges for structures that no longer need one.
    foreach (const QString &key, m_countBadges.keys())
    {
        if (wanted.contains(key))
            continue;
        QGraphicsItemGroup *g = m_countBadges.take(key);
        if (g != nullptr) { m_scene->removeItem(g); delete g; }
    }
    refreshBeneath();     // what covers what changes with the same moves
}

/* "why does the fixture at the base of the truss look like it's above it?":
   in the top view every fixture is drawn over every structure, so it can
   always be clicked -- whatever its height. One with a truss, deck, pipe or
   tower top more than half a metre above it, over it on the plan, is drawn
   faded with a dashed outline: underneath, but still there to click. What it
   is mounted on never counts. */
void MonitorGraphicsView::refreshBeneath()
{
    MonitorProperties *props = m_doc->monitorProperties();
    const bool top = (m_pov == PovTop);
    const float clearance = 0.5f;
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
    {
        MonitorFixtureItem *mfi = it.value();
        if (mfi == nullptr)
            continue;
        bool beneath = false;
        if (top)
        {
            const quint32 fid = it.key();
            const float z = props->fixtureRigPosition(fid).z();
            const QString own = PatchPlanner::mountKey(m_doc, fid);
            const QPointF c = mfi->iconCentreScene();
            auto covers = [&](const QGraphicsItem *si, const QString &key, float height) {
                return si != nullptr && si->isVisible() && key != own && height > z + clearance
                       && si->contains(si->mapFromScene(c));
            };
            for (auto t = m_trussItems.constBegin(); !beneath && t != m_trussItems.constEnd(); ++t)
                if (Truss *tr = props->truss(t.key()))
                    beneath = covers(t.value(), QStringLiteral("truss:%1").arg(t.key()),
                                     qMax(tr->origin().z(), tr->positionAt(tr->length()).z()));
            for (auto p = m_platformItems.constBegin(); !beneath && p != m_platformItems.constEnd(); ++p)
                if (StagePlatform *pl = props->platform(p.key()))
                    beneath = covers(p.value(), QStringLiteral("platform:%1").arg(p.key()),
                                     props->platformBaseZ(p.key())
                                     + pl->topAt(float(c.x()), float(c.y())));
            for (auto p = m_pipeItems.constBegin(); !beneath && p != m_pipeItems.constEnd(); ++p)
                if (Pipe *pp = props->pipe(p.key()))
                    beneath = covers(p.value(), QStringLiteral("pipe:%1").arg(p.key()),
                                     qMax(pp->positionAt(0.0f).z(), pp->positionAt(pp->length()).z()));
            for (auto t = m_towerItems.constBegin(); !beneath && t != m_towerItems.constEnd(); ++t)
                if (Tower *tw = props->tower(t.key()))
                    beneath = covers(t.value(), QStringLiteral("tower:%1").arg(t.key()), tw->height());
        }
        mfi->setBeneath(beneath);
    }
}

bool MonitorGraphicsView::fixtureItemShouldBeVisible(quint32 fid) const
{
    MonitorProperties *props = m_doc ? m_doc->monitorProperties() : NULL;
    if (props == NULL)
        return true;

    if (props->layer(props->fixtureLayer(fid)).visible == false)
        return false;
    if (m_stageOnly)
        return false;
    /* A curtain is on the plot as its drape, not as a fixture: one marked
       Curtain keeps its place, hidden, and is back where it was when it is
       unmarked. */
    if (Scenery::isCurtainFixture(m_doc->fixture(fid)))
        return false;

    /* "Fixtures on the rig", unchecked: a structure covered by its own fixtures
       cannot be clicked, so they are hidden while it is being placed. Only
       MOUNTED ones -- there is nothing under a free-standing fixture to reach. */
    const bool mounted = props->fixtureRigProps(fid).primaryMount()
                         != FixtureRigProps::NoMount;
    if (mounted && !m_showMountedFixtures)
        return false;

    /* ...unless its bubble was asked to show what it stands for, and THIS is
       the one picked out of the pile. Showing all of them was the first cut:
       "we don't have to show the actual fixture(s), just the label you have,
       that works better" -- a stack of icons on one spot tells you nothing the
       tags do not, and buries the structure again. The picked one is drawn
       because it is the one being selected, inspected and measured. */
    if (m_revealFocus >= 0 && quint32(m_revealFocus) == fid)
        return true;

    // Buried by its own kind: the structure shows a count instead.
    if (m_buriedByOwnFixtures.contains(fid))
        return false;

    return true;
}

void MonitorGraphicsView::growSceneRectToItems()
{
    if (m_scene == NULL || width() <= 0 || height() <= 0)
        return;

    /* The canvas in scene terms -- turned a quarter, its width is the scene's
       height -- with what the items cover, grown evenly about the grid's
       middle: a view that can't scroll centres the scene rect, so anything
       off one side of the stage (a house light, a label) pushed the grid
       off centre, and a turned view showed it in one corner. */
    const bool turned = (m_viewRotation & 1);
    QRectF r(0, 0, turned ? this->height() : this->width(), turned ? this->width() : this->height());
    r |= m_scene->itemsBoundingRect();
    if (m_gridEnabled && m_cellPixels > 0)
    {
        const QPointF gc(m_xOffset + m_cellPixels * m_gridExtent.width() / 2.0,
                         m_yOffset + m_cellPixels * m_gridExtent.height() / 2.0);
        const qreal hx = qMax(gc.x() - r.left(), r.right() - gc.x());
        const qreal hy = qMax(gc.y() - r.top(), r.bottom() - gc.y());
        r = QRectF(gc.x() - hx, gc.y() - hy, 2 * hx, 2 * hy);
    }
    if (r != m_scene->sceneRect())
        m_scene->setSceneRect(r);

    refreshOffGridTally();
}

void MonitorGraphicsView::refreshOffGridTally()
{
    const int off = fixturesOffGrid().count();
    if (off == m_offGridCount)
        return;
    m_offGridCount = off;
    emit offGridFixturesChanged(off);
}

QList<quint32> MonitorGraphicsView::fixturesOffGrid() const
{
    QList<quint32> out;
    MonitorProperties *props = m_doc ? m_doc->monitorProperties() : NULL;
    if (props == NULL || m_unitValue <= 0)
        return out;

    // The grid runs from the origin: gridSize is in CELLS, m_unitValue the mm
    // each cell spans.
    const double w = double(m_gridExtent.width())  * double(m_unitValue);
    const double d = double(m_gridExtent.height()) * double(m_unitValue);

    foreach (quint32 fid, props->fixtureItemsID())
    {
        const QVector3D p = props->fixturePosition(fid, 0, 0);   // mm
        if (p.x() < 0.0f || p.y() < 0.0f || double(p.x()) > w || double(p.y()) > d)
            out << fid;
    }
    return out;
}

int MonitorGraphicsView::selectAllMapItems()
{
    if (m_scene == NULL)
        return 0;
    /* One selection change, not one per item: every item's setSelected() would
       otherwise run the group-extension and overlay handlers again. */
    int n = 0;
    m_scene->blockSignals(true);
    foreach (QGraphicsItem *gi, m_scene->items())
    {
        // Top-level map objects only: labels and handles belong to their item.
        // Nor what a locked layer holds: on the plot that is click-through.
        if (gi->parentItem() != NULL || !gi->isVisible()
            || !(gi->flags() & QGraphicsItem::ItemIsSelectable)
            || gi->data(ItemLock::LayerLockedKey).toBool())
            continue;
        gi->setSelected(true);
        n++;
    }
    m_scene->blockSignals(false);
    // The scene's own handlers (group extension, overlays) run once, and
    // announce mapSelectionChanged themselves.
    emit m_scene->selectionChanged();
    return n;
}

void MonitorGraphicsView::selectFixtures(const QList<quint32> &ids)
{
    if (m_scene == NULL)
        return;
    m_scene->clearSelection();
    MonitorFixtureItem *first = NULL;
    foreach (quint32 fid, ids)
    {
        MonitorFixtureItem *item = m_fixtures.value(fid, NULL);
        if (item == NULL)
            continue;
        item->setSelected(true);
        if (first == NULL)
            first = item;
    }
    // Scrolling to it is the point: these are the fixtures you cannot find.
    if (first != NULL)
        ensureVisible(first);
}

QPointF MonitorGraphicsView::realPositionToPixels(qreal xpos, qreal ypos) const
{
    qreal realX = m_xOffset + ((xpos * m_cellPixels) / m_unitValue);
    qreal realY = m_yOffset + ((ypos * m_cellPixels) / m_unitValue);

    return QPointF(realX, realY);
}

qreal MonitorGraphicsView::floorPixelY() const
{
    // Bottom of the grid = the Z=0 floor line for elevation views.
    return m_yOffset + m_gridExtent.height() * m_cellPixels;
}

float MonitorGraphicsView::floorHeightAt(float xMetres, float yMetres) const
{
    MonitorProperties *props = m_doc ? m_doc->monitorProperties() : nullptr;
    if (props == nullptr)
        return 0.0f;

    float best = 0.0f;
    foreach (StagePlatform *p, props->platforms())
    {
        if (p == nullptr || !p->solid())
            continue;
        if (!p->containsPoint(xMetres, yMetres))      // turned or rounded, too
            continue;
        best = qMax(best, p->topAt(xMetres, yMetres));   // the tread, on steps
    }
    return best;
}

QVector3D MonitorGraphicsView::unprojectMm(const QPointF &px, const QVector3D &currentMm) const
{
    const qreal scale = (m_unitValue > 0.0) ? (qreal(m_cellPixels) / m_unitValue) : 0.0;
    if (scale <= 0.0)
        return currentMm;

    switch (m_pov)
    {
    case PovFront:  // X across, height (Z) up; depth (Y) is not on screen
        return QVector3D(float((px.x() - m_xOffset) / scale),
                         currentMm.y(),
                         float((floorPixelY() - px.y()) / scale));
    case PovSide:   // Y across, height (Z) up; X is not on screen
        return QVector3D(currentMm.x(),
                         float((px.x() - m_xOffset) / scale),
                         float((floorPixelY() - px.y()) / scale));
    case PovTop:
    default:        // X across, Y upstage; height (Z) is not on screen
        return QVector3D(float((px.x() - m_xOffset) / scale),
                         float((px.y() - m_yOffset) / scale),
                         currentMm.z());
    }
}

QPointF MonitorGraphicsView::projectMm(qreal xMm, qreal yMm, qreal zMm) const
{
    const qreal scale = (m_unitValue > 0.0) ? (qreal(m_cellPixels) / m_unitValue) : 0.0;
    switch (m_pov)
    {
    case PovFront:  // look upstage: X → right, height (Z) → up
        return QPointF(m_xOffset + xMm * scale, floorPixelY() - zMm * scale);
    case PovSide:   // look from stage right: Y (upstage) → right, height (Z) → up
        return QPointF(m_xOffset + yMm * scale, floorPixelY() - zMm * scale);
    case PovTop:
    default:
        return QPointF(m_xOffset + xMm * scale, m_yOffset + yMm * scale);
    }
}

void MonitorGraphicsView::setViewPOV(ViewPOV pov)
{
    if (pov == m_pov)
        return;
    m_pov = pov;

    /* Rebuild the whole projection. Every item is placed through projectMm(),
       so changing which way the stage is being looked at invalidates all of
       them -- one refreshAllItems() rather than a hand-picked list that has to
       be remembered whenever a new item class appears. */
    updateGrid();
    refreshAllItems();
    refreshItemLayerState();
    rebuildEmissionOverlay();
    emit rulersChanged();
}

// The fixture's 3-D long-axis (unit) direction, from its studio orientation.
// Mirrors StudioPlaneView::fixtureAxis so the main monitor and the studio editor
// agree. Defaults (Front face, angle 0) → along X, which is the common tape case.
static QVector3D fixtureLongAxis(const FixtureRigProps &rp)
{
    // The same length the rig view draws, turned with its group or host.
    return RigHeads::axis(rp);
}

void MonitorGraphicsView::updateFixture(quint32 id)
{
    scheduleNetworkRuns();
    Fixture *fxi = m_doc->fixture(id);
    if (fxi == NULL || m_fixtures.contains(id) == false)
        return;
    m_fixtures.value(id)->refreshName();
    m_fixtures.value(id)->refreshStageLook();
    // Marked Curtain (or back to a light): drawn as its drape, or again as itself.
    m_fixtures.value(id)->setVisible(fixtureItemShouldBeVisible(id));

    const QLCFixtureMode *mode = fxi->fixtureMode();
    double width = 0;
    double height = 0;

    if (mode != 0)
    {
        width = mode->physical().width();
        height = mode->physical().height();
    }

    const Fixture::StageLook drawnLook = drawnStageLook(fxi);
    if (drawnLook != Fixture::AutoLook && stageLookIgnoresPhysical(fxi))
        width = height = 0;     // made up: the look's proportions, below
    if (width <= 0 || height <= 0)
    {
        // No physical layout declared: most multi-head fixtures like this are
        // linear (LED tape / battens / pixel bars), so assume ONE ROW instead
        // of a square (which would make the heads wrap into a block). A very
        // wide:thin aspect makes setSize() lay the heads out in a single row.
        const int n = qMax(1, fxi->heads());
        width = 300.0;
        height = (n > 1) ? 300.0 / n : 300.0;
        /* A stage look knows its proportions better: a followspot long, a
           staff thin (one with several heads keeps the strip above). */
        if (drawnLook != Fixture::AutoLook && n == 1)
        {
            const QSizeF look = stageLookDefaultSize(drawnLook, 300.0, isElevation());
            width = look.width();
            height = look.height();
        }
    }

    MonitorFixtureItem *item = m_fixtures.value(id, NULL);
    if (item == NULL)
        return;   // belt and braces: never dereference what the hash handed back

    // On-screen size from the physical dimensions, keeping the TRUE aspect
    // ratio (so a thin tape stays thin and its heads lay out in the right
    // number of rows). Only enforce a minimum AREA — scaling both dimensions
    // by the same factor — so very small fixtures stay visible without being
    // squashed toward square (which used to make pixel arrays wrap). Zoom
    // provides per-LED detail.
    double pw = (double(width)  * m_cellPixels) / m_unitValue;
    double ph = (double(height) * m_cellPixels) / m_unitValue;

    // Elevation views (Front/Side) FORESHORTEN the fixture by its 3-D long-axis
    // orientation. A tape running along X shows full length in Front but
    // collapses to a dot in Side (its length is perpendicular to that view) —
    // the same projection the studio editor uses. The heads then lay out along
    // whichever screen axis survives.
    /* ...and the plan view too: it drew every bar at full length lying
       left-right, whichever way it actually ran, so a row of bars hanging
       straight down from a truss became a solid strip of overlapping 1 m bars
       ("the bars on the farts render funny in the studio view but look right
       in the rig view"). Screen X is X there and screen Y is Y; a bar
       pointing up or down is a dot. Unchanged for a bar running along X,
       which is the default. */
    {
        const QVector3D axis = fixtureLongAxis(m_doc->monitorProperties()->fixtureRigProps(id));
        const double hAxis = (m_pov == PovSide) ? qAbs(double(axis.y())) : qAbs(double(axis.x()));
        const double vAxis = (m_pov == PovTop) ? qAbs(double(axis.y()))
                                               : qAbs(double(axis.z()));   // Z is up in both elevations
        const double L = pw, Th = ph;                  // physical width / thickness (px)
        pw = qMax(3.0, L * hAxis + Th * vAxis);
        ph = qMax(3.0, L * vAxis + Th * hAxis);
    }

    const double kMinArea = 16.0 * 16.0;
    const double area = pw * ph;
    if (area > 0.0 && area < kMinArea)
    {
        const double s = sqrt(kMinArea / area);
        pw *= s;
        ph *= s;
    }
    item->setElevationView(isElevation());   // first: a look's lens depends on it
    item->setSize(QSize(qMax(1, qRound(pw)), qMax(1, qRound(ph))));

    // Riser-mounted fixtures derive their position from the platform in EVERY
    // view (top too — they sit on the riser's edge/face). Free/truss fixtures
    // keep their stored XY in top view and use the 3-D rig pos in elevation.
    const FixtureRigProps frp = m_doc->monitorProperties()->fixtureRigProps(id);
    // Riser-, pipe-, tower- and STUDIO-FRAME fixtures derive their position from
    // the structure/frame in EVERY view (they follow it), so centre the icon on
    // the derived point — otherwise the top view shows the stale stored XY (e.g.
    // a studio group distributed on a step looked un-distributed in 2D).
    const bool riser = frp.onRiser() || frp.onPipe() || frp.onTower() || frp.onHanger()
                       || frp.isHeld()      // a wand in a performer's hand
                       || m_doc->monitorProperties()->fixtureFrameGroup(id) != 0;
    // A truss-bound fixture's stored position is the point ON the truss line, so
    // centre the icon on it (like risers) instead of pinning its top-left corner.
    const bool onTruss = (frp.trussId != Truss::invalidId());
    if (riser)
    {
        // Riser-mounted: the stored U,V is the fixture CENTRE on the face, so
        // centre the item on the derived position (matches the Face Editor).
        const QVector3D w = m_doc->monitorProperties()->fixtureRigPosition(id);
        const QPointF c = projectMm(w.x() * 1000.0, w.y() * 1000.0, w.z() * 1000.0);
        item->setPos(c.x() - pw / 2.0, c.y() - ph / 2.0);
    }
    else if (m_pov == PovTop)
    {
        if (onTruss)
        {
            /* A bound fixture's position IS its binding -- truss + offset +
               cross. Placing it from the item's cached realPosition instead
               made the cache a second source of truth, and every path that
               bound a fixture without also updating the cache (attach, a
               test, a workspace load) stranded the icon wherever the cache
               last pointed. Derive from the rig, the way the elevations
               always have, and the cache cannot strand anything. */
            const QVector3D w = m_doc->monitorProperties()->fixtureRigPosition(id);
            const QPointF p = projectMm(w.x() * 1000.0, w.y() * 1000.0, w.z() * 1000.0);
            item->setPos(p - halfIcon(item));
        }
        else
        {
            /* Centred on the point its light comes from (freeCentreOffsetMm()):
               the store keeps the corner, the icon is not always the size
               of the body, and the aim lines and the rig view start at the
               body's centre. */
            /* A deck carries what stands on it (slotPlatformMoved() and a
               turn write the store): read it back, or the icon stays where
               the deck was ("the flame still didn't move"). */
            if (frp.onDeck())
            {
                const QVector3D st = m_doc->monitorProperties()->fixturePosition(id, 0, 0);
                item->setRealPosition(QPointF(double(st.x()), double(st.y())));
            }
            const QPointF off = freeCentreOffsetMm(id);
            const QPointF c = realPositionToPixels(item->realPosition().x() + off.x(),
                                                   item->realPosition().y() + off.y());
            item->setPos(c - halfIcon(item));
        }
    }
    else
    {
        const QVector3D w = m_doc->monitorProperties()->fixtureRigPosition(id);
        // A free or decked fixture by its centre, as in the top view.
        const QPointF off = freeCentreOffsetMm(id);
        const QPointF p = projectMm(w.x() * 1000.0 + off.x(), w.y() * 1000.0 + off.y(), w.z() * 1000.0);
        if (frp.onDeck())
            // Stand on the deck: the fixture's BASE sits at Z, so lift the icon
            // up by its full height and centre it horizontally over its X.
            item->setPos(p.x() - pw / 2.0, p.y() - ph);
        else
            item->setPos(p - halfIcon(item));
    }

    // Its LEDs along a path, a wand's staff, a flame's silk.
    PixelPathDecor::apply(this, m_doc, item, id);
    // A free string's anchors, when it is the one selected.
    if (m_stringHandles != nullptr || frp.pixelPath == PixelPath::FreeString)
        refreshStringHandles();

    /* The grid layout unions this too, but it runs before any item exists (and
       again on resize), so a fixture placed OUTSIDE the grid afterwards would
       still fall beyond the scroll range and be unreachable. */
    growSceneRectToItems();
}

void MonitorGraphicsView::refreshRiserFixtures()
{
    MonitorProperties *props = m_doc->monitorProperties();
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
    {
        const FixtureRigProps rp = props->fixtureRigProps(it.key());
        // Riser/deck mounts follow the platform; so do members of a studio frame
        // that is slaved to a platform (the unified placement path).
        if (rp.onRiser() || rp.onDeck() || props->fixtureFrameGroup(it.key()) != 0)
            updateFixture(it.key());
    }
}

void MonitorGraphicsView::setBackgroundImage(QString filename)
{
    m_backgroundImage = filename;
    if (m_bgItem != NULL)
    {
        m_scene->removeItem(m_bgItem);
        delete m_bgItem;
        m_bgItem = NULL;
    }
    if (filename.isEmpty() == false)
    {
        m_bgPixmap = QPixmap(m_backgroundImage);
        m_bgItem = new QGraphicsPixmapItem(m_bgPixmap);
        m_bgItem->setZValue(0); // make sure it goes on the bacground
        m_scene->addItem(m_bgItem);
    }
    updateGrid();
}

void MonitorGraphicsView::setBackgroundColor(const QColor &color)
{
    // QGraphicsView::setBackgroundBrush overrides the near-black default set
    // in initGraphicsView. Use an invalid color to restore that default.
    if (color.isValid())
        setBackgroundBrush(QBrush(color));
    else
        setBackgroundBrush(QBrush(QColor(11, 11, 11, 255)));
}

MonitorFixtureItem *MonitorGraphicsView::getSelectedItem()
{
    QHashIterator <quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext() == true)
    {
        it.next();
        MonitorFixtureItem *item = it.value();
        if (item->isSelected() == true)
            return item;
    }
    return NULL;
}

MonitorFixtureItem *MonitorGraphicsView::fixtureItemForId(quint32 fxId) const
{
    return m_fixtures.value(fxId, nullptr);
}

void MonitorGraphicsView::selectFixtureExclusive(quint32 fxId)
{
    if (m_scene != nullptr)
        m_scene->clearSelection();
    if (MonitorFixtureItem *it = fixtureItemForId(fxId))
        it->setSelected(true);
}

QList<MonitorFixtureItem *> MonitorGraphicsView::selectedFixtureItems() const
{
    QList<MonitorFixtureItem *> result;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
    {
        if (auto *mfi = dynamic_cast<MonitorFixtureItem *>(gi))
            result.append(mfi);
    }
    return result;
}

void MonitorGraphicsView::addFixture(quint32 id, QPointF pos)
{
    scheduleNetworkRuns();
    if (id == Fixture::invalidId() || m_fixtures.contains(id) == true)
        return;

    if (m_doc->fixture(id) == NULL)
        return;

    MonitorFixtureItem *item = new MonitorFixtureItem(m_doc, id);
    item->setZValue(2);
    item->setViewHint(viewScale(), 90.0 * m_viewRotation);
    item->setRealPosition(pos);
    // Apply the fixture's layer state (visibility + lock) up front so a
    // per-fixture add during fillGraphicsView doesn't need a full O(n) refresh.
    const MonitorProperties::MonitorLayer lyr =
        m_doc->monitorProperties()->layer(m_doc->monitorProperties()->fixtureLayer(id));
    item->setMovable(!m_layoutLocked && !lyr.locked);
    item->setVisible(fixtureItemShouldBeVisible(id));
    // Reflect any existing truss binding (e.g. on workspace load)
    FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(id);
    item->setBoundToTruss(rp.trussId != Truss::invalidId());
    m_fixtures[id] = item;
    m_scene->addItem(item);
    updateFixture(id);
    applySnapToItem(item);
    connect(item, SIGNAL(itemDropped(MonitorFixtureItem*)),
            this, SLOT(slotFixtureMoved(MonitorFixtureItem*)));
    connect(item, &MonitorFixtureItem::mountingChanged,
            this, [this](quint32 fid) { updateFixture(fid); });
}

bool MonitorGraphicsView::removeFixture(quint32 id)
{
    scheduleNetworkRuns();
    MonitorFixtureItem *item = NULL;

    if (id == Fixture::invalidId())
    {
        item = getSelectedItem();
        if (item != NULL)
            id = item->fixtureID();
    }
    else
        item = m_fixtures.value(id, NULL);

    if (item == NULL)
        return false;

    m_scene->removeItem(item);
    m_fixtures.take(id);
    m_doc->monitorProperties()->removeFixture(id);
    delete item;

    return true;
}

void MonitorGraphicsView::clearFixtures()
{
    QScopedValueRollback<bool> rb(m_itemsRebuilding); m_itemsRebuilding = true;
    foreach (MonitorFixtureItem *item, m_fixtures)
        delete item;
    m_fixtures.clear();
    m_addAnchorFid = UINT_MAX;
    m_stringDragIndex = -1;
    m_stringDragFid = UINT_MAX;
    if (m_stringHandles != nullptr)
        m_stringHandles->hide();
}

void MonitorGraphicsView::updateImages()
{
    QScopedValueRollback<bool> rb(m_itemsRebuilding); m_itemsRebuilding = true;
    foreach (MonitorImageItem *ii, m_imageItems)
    {
        m_scene->removeItem(ii);
        delete ii;
    }
    m_imageItems.clear();

    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr || m_cellPixels == 0)
        return;

    const qreal scale = qreal(m_cellPixels) / m_unitValue;   // scene px per mm

    // Which plane is visible in the current POV?
    int shownPlane;
    switch (m_pov)
    {
        case PovFront: shownPlane = MonitorProperties::MonitorImage::FrontBackdrop; break;
        case PovSide:  shownPlane = MonitorProperties::MonitorImage::SideBackdrop;  break;
        case PovTop:
        default:       shownPlane = MonitorProperties::MonitorImage::Floor;         break;
    }

    foreach (const MonitorProperties::MonitorImage &img, props->images())
    {
        if (img.plane != shownPlane)
            continue;

        const float wMm = img.width  * 1000.0f;
        const float hMm = img.height * 1000.0f;
        const float pxW = float(wMm * scale);
        const float pxH = float(hMm * scale);

        // Top-left corner of the rectangle, projected for this plane.
        QPointF tl;
        if (img.plane == MonitorProperties::MonitorImage::Floor)
            tl = projectMm(img.originX * 1000.0, img.originY * 1000.0, img.originZ * 1000.0);
        else if (img.plane == MonitorProperties::MonitorImage::FrontBackdrop)
            // X across, Z up: top edge = originZ + height.
            tl = projectMm(img.originX * 1000.0, 0.0, (img.originZ + img.height) * 1000.0);
        else // SideBackdrop: Y across, Z up.
            tl = projectMm(0.0, img.originY * 1000.0, (img.originZ + img.height) * 1000.0);

        MonitorImageItem *ii = new MonitorImageItem(m_doc, img.id);
        ii->setGeometry(float(tl.x()), float(tl.y()), pxW, pxH);
        ii->setImageRotation(img.rotation);
        connect(ii, SIGNAL(itemDropped(MonitorImageItem*)),
                this, SLOT(slotImageMoved(MonitorImageItem*)));
        m_scene->addItem(ii);
        m_imageItems.insert(img.id, ii);
    }
    refreshItemLayerState();
}

void MonitorGraphicsView::slotImageMoved(MonitorImageItem *item)
{
    if (item == nullptr || m_cellPixels == 0)
        return;

    MonitorProperties *props = m_doc->monitorProperties();
    const qreal scale = qreal(m_cellPixels) / m_unitValue;

    // Persist every selected image (group move) -- see slotTowerMoved()'s
    // comment for why this rebuild from the live selection is necessary.
    // Resizing only ever applies to the single grabbed item (only it could
    // have had its resize handle dragged), so that part stays scoped to
    // `item`; position applies to the whole selection.
    QList<MonitorImageItem *> moved;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (auto *ii = dynamic_cast<MonitorImageItem *>(gi); ii != nullptr && itemDraggable(ii))
            moved.append(ii);
    if (!moved.contains(item))
        moved.append(item);

    foreach (MonitorImageItem *ii, moved)
    {
        MonitorProperties::MonitorImage img = props->image(ii->imageId());
        const QPointF p = ii->pos();   // scene-pixel top-left

        // Persist the (possibly resized) rectangle: convert px extents back to metres.
        if (ii == item && scale > 0.0)
        {
            img.width  = float(ii->pixelSize().width()  / (1000.0 * scale));
            img.height = float(ii->pixelSize().height() / (1000.0 * scale));
        }

        if (img.plane == MonitorProperties::MonitorImage::Floor)
        {
            const QPointF mm = pixelsToRealPosition(p.x(), p.y());
            img.originX = float(mm.x() / 1000.0);
            img.originY = float(mm.y() / 1000.0);
        }
        else if (img.plane == MonitorProperties::MonitorImage::FrontBackdrop)
        {
            img.originX = float((p.x() - m_xOffset) / (1000.0 * scale));
            // p.y is the TOP edge → convert to Z, then subtract the height.
            const double zTop = (floorPixelY() - p.y()) / (1000.0 * scale);
            img.originZ = float(zTop - img.height);
        }
        else // SideBackdrop
        {
            img.originY = float((p.x() - m_xOffset) / (1000.0 * scale));
            const double zTop = (floorPixelY() - p.y()) / (1000.0 * scale);
            img.originZ = float(zTop - img.height);
        }
        props->setImage(img);
    }
    m_doc->setModified();
}

void MonitorGraphicsView::followParentTrusses()
{
    MonitorProperties *props = m_doc->monitorProperties();
    props->recomputeChildTrusses();
    updateTrusses();
    // Reposition fixtures riding on any child bar to the bar's new geometry.
    foreach (Truss *bar, props->trusses())
    {
        if (bar == nullptr || !bar->isChildBar())
            continue;
        for (auto fit = m_fixtures.constBegin(); fit != m_fixtures.constEnd(); ++fit)
        {
            if (props->fixtureRigProps(fit.key()).trussId != bar->id())
                continue;
            const QVector3D w = bar->positionAt(props->fixtureRigProps(fit.key()).trussOffset);
            const QPointF mm(w.x() * 1000.0, w.y() * 1000.0);
            moveFixtureTo(fit.key(), mm);
            emit fixtureMoved(fit.key(), mm);
        }
    }
}

void MonitorGraphicsView::refreshFixtureBindings()
{
    MonitorProperties *props = m_doc->monitorProperties();
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        it.value()->setBoundToTruss(
            props->fixtureRigProps(it.key()).trussId != Truss::invalidId());
}

void MonitorGraphicsView::updateTrusses()
{
    QScopedValueRollback<bool> rb(m_itemsRebuilding); m_itemsRebuilding = true;
    // Remove previous truss graphics items (label is a child of each TrussItem
    // so it is deleted automatically).
    foreach (TrussItem *ti, m_trussItems)
    {
        m_scene->removeItem(ti);
        delete ti;
    }
    m_trussItems.clear();

    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr || m_cellPixels == 0)
        return;

    foreach (Truss *t, props->trusses())
    {
        float pxWid = float(qMax(8.0, (t->width()  * 1000.0 * m_cellPixels) / m_unitValue));
        QPointF p0;
        float pxLen;
        float angleDeg = 0.0f;

        if (m_pov == PovTop)
        {
            p0 = realPositionToPixels(t->origin().x() * 1000.0, t->origin().y() * 1000.0);
            if (t->type() == Truss::Vertical)
            {
                pxLen = pxWid;   // circle; length unused
            }
            else
            {
                pxLen    = float((t->length() * 1000.0 * m_cellPixels) / m_unitValue);
                angleDeg = float(qRadiansToDegrees(qAtan2(double(t->direction().y()),
                                                          double(t->direction().x()))));
            }
        }
        else
        {
            // Elevation: project both endpoints and draw the truss as the line
            // between them (a vertical tower becomes an upright segment).
            const QVector3D o = t->origin();
            QVector3D e;
            if (t->type() == Truss::Vertical)
                // A child bar hangs DOWN from its origin; a tower stands up.
                e = QVector3D(o.x(), o.y(),
                              o.z() + (t->isChildBar() ? -t->length() : t->length()));
            else
                e = QVector3D(o.x() + t->direction().x() * t->length(),
                              o.y() + t->direction().y() * t->length(), o.z());
            p0 = projectMm(o.x() * 1000.0, o.y() * 1000.0, o.z() * 1000.0);
            const QPointF p1 = projectMm(e.x() * 1000.0, e.y() * 1000.0, e.z() * 1000.0);
            const QPointF d = p1 - p0;
            pxLen = float(qSqrt(d.x() * d.x() + d.y() * d.y()));
            angleDeg = float(qRadiansToDegrees(qAtan2(d.y(), d.x())));
            if (pxLen < pxWid) pxLen = pxWid;   // end-on trusses stay visible
        }

        TrussItem *ti = new TrussItem(t, m_doc, pxLen, pxWid);
        ti->setPos(p0);
        ti->setRotation(angleDeg);
        ti->setElevationMode(isElevation());
        // In elevation only draggable bars move; refreshItemLayerState re-confirms.
        ti->setMovable(isElevation() ? elevationBarDraggable(t)
                                     : (!m_layoutLocked && !t->locked()));
        m_scene->addItem(ti);
        m_trussItems.insert(t->id(), ti);

        connect(ti, &TrussItem::itemDropped,
                this, &MonitorGraphicsView::slotTrussMoved);
        connect(ti, &TrussItem::addFixtureRequested,
                this, &MonitorGraphicsView::addFixtureToTrussRequested);
    }

    updateHangers();      // they hang off trusses
    updateNoLightZones();
    refreshItemLayerState();
}

bool MonitorGraphicsView::elevationFixtureDraggable(quint32 fid) const
{
    if (!isElevation())
        return false;   // Front or Side elevation
    const FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(fid);
    return rp.trussId != Truss::invalidId();   // slides along its truss/bar
}

/* Whether a fixture can be taken onto a structure running from height @p z0
   to @p z1 (metres): the plan alone says nothing about height, so a light at
   the foot of a tower, moved under the bar on its top, was hung on the bar
   ("they're in different Z planes .. shouldn't have done that"). A fixture
   already mounted on something else has a height of its own, and keeps it
   unless the structure is within half a metre of it. A free-standing one is
   hung by where it is dropped on the plan, as ever -- that is how a fixture
   is hung from the plot. */
static bool atHeightOf(Doc *doc, quint32 fid, float z0, float z1)
{
    MonitorProperties *props = doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    if (rp.primaryMount() == FixtureRigProps::NoMount
        || rp.primaryMount() == FixtureRigProps::TrussMount)
        return true;
    const float z = props->fixtureRigPosition(fid).z();
    const float tolerance = 0.5f;
    return z >= qMin(z0, z1) - tolerance && z <= qMax(z0, z1) + tolerance;
}

bool MonitorGraphicsView::attachHeld() const
{
    // Asked of the keyboard itself: pressed or let go mid-drag, the event
    // state can lag the key by an event.
    return m_attachOnDrop || (QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier);
}

/* The borders a fixture being dragged shows, from the same tests the drop
   uses: green while letting go would hang it on a truss or pipe or stand it
   on a deck (Shift held, at its height), red while it is being pulled off
   the truss it is on. Worked out on every move, and when Shift is pressed or
   let go ("when shift is pressed .. the outline should change"). */
void MonitorGraphicsView::updateDragBorders()
{
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
    {
        MonitorFixtureItem *mfi = dynamic_cast<MonitorFixtureItem *>(gi);
        if (mfi == nullptr)
            continue;
        const quint32 fid = m_fixtures.key(mfi, Fixture::invalidId());
        if (fid == Fixture::invalidId())
            continue;
        const FixtureRigProps rp = props->fixtureRigProps(fid);
        if (rp.trussId == Truss::invalidId())
        {
            bool attach = trussUnderFixture(mfi, 1.0f, true) != nullptr;
            if (!attach && rp.pipeId == Pipe::invalidId())
                attach = pipeUnderFixture(mfi, 1.0f, true) != nullptr;
            if (!attach && attachHeld() && rp.pipeId == Pipe::invalidId() && !rp.onRiser()
                && m_cellPixels > 0)
            {
                const QPointF cPx = mfi->pos() + halfIcon(mfi);
                const QPointF cMm = pixelsToRealPosition(cPx.x(), cPx.y());
                const quint32 pid = props->platformIdAt(float(cMm.x() / 1000.0),
                                                         float(cMm.y() / 1000.0));
                attach = pid != FixtureRigProps::invalidPlatformId() && pid != rp.deckPlatformId;
            }
            mfi->setAttachMode(attach);
            mfi->setEscapeMode(false);
            continue;
        }
        mfi->setAttachMode(false);
        TrussItem *ti = m_trussItems.value(rp.trussId, nullptr);
        if (ti == nullptr)
            continue;
        // Same yardstick both ways: trussUnderFixture() applies the
        // two-widths rule, so "still on it" and "far enough to detach"
        // cannot disagree.
        mfi->setEscapeMode(trussUnderFixture(mfi) != ti);
    }
}

static bool isDuplicateKey(const QKeyEvent *ke)
{
    return ke->key() == Qt::Key_D
        && (ke->modifiers() & ~Qt::KeypadModifier) == Qt::ControlModifier;
}

bool MonitorGraphicsView::event(QEvent *e)
{
    /* Cmd/Ctrl+D on the plot duplicates the selection. Dump DMX is Alt+Ctrl+D, so
       the key is the plot's alone; it is claimed while something is
       selected, else it does nothing. */
    if (e->type() == QEvent::ShortcutOverride && isDuplicateKey(static_cast<QKeyEvent *>(e))
        && !m_scene->selectedItems().isEmpty())
    {
        e->accept();
        return true;
    }
    return QGraphicsView::event(e);
}

void MonitorGraphicsView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_addAnchorFid != UINT_MAX)
    {
        endAddAnchors();
        emit notice(tr("Done adding anchors."));
        event->accept();
        return;
    }
    if (isDuplicateKey(event) && !m_scene->selectedItems().isEmpty())
    {
        if (Monitor *mon = Monitor::instance())
            mon->duplicateSelected();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && m_enteredAssembly != 0)
    {
        // Out of the assembly: the part picked takes the whole of it again.
        leaveAssembly();
        extendSelectionToGroups();
        event->accept();
        return;
    }
    /* The edit keys every list has (docs/features/shortcuts.md, Edit
       keys): ⌫ takes the selection off the plot (as Remove Selected, one
       undo step; fixtures back to the Inventory), F2 renames it in Plot
       layers, ⌘F goes to Plot layers' search, Esc lets go of it. */
    const Qt::KeyboardModifiers mods = event->modifiers() & ~Qt::KeypadModifier;
    Monitor *mon = Monitor::instance();
    const bool picked = !m_scene->selectedItems().isEmpty();
    if (mon != nullptr && picked && mods == Qt::NoModifier
        && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace))
    {
        if (m_doc->mode() == Doc::Design)
            QMetaObject::invokeMethod(mon, [mon]() { mon->removeSelectionWithUndo(); }, Qt::QueuedConnection);
        event->accept();
        return;
    }
    if (mon != nullptr && picked && mods == Qt::NoModifier && event->key() == Qt::Key_F2)
    {
        mon->renameSelected();
        event->accept();
        return;
    }
    if (mon != nullptr && event->matches(QKeySequence::Find))
    {
        mon->focusLayersSearch();
        event->accept();
        return;
    }
    if (picked && mods == Qt::NoModifier && event->key() == Qt::Key_Escape)
    {
        m_scene->clearSelection();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Shift && (QGuiApplication::mouseButtons() & Qt::LeftButton))
        updateDragBorders();
    QGraphicsView::keyPressEvent(event);
}

void MonitorGraphicsView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Shift && (QGuiApplication::mouseButtons() & Qt::LeftButton))
        updateDragBorders();
    QGraphicsView::keyReleaseEvent(event);
}

TrussItem *MonitorGraphicsView::trussUnderFixture(MonitorFixtureItem *mfi,
                                                  float widths, bool forAttach) const
{
    if (mfi == nullptr)
        return nullptr;

    // The ICON's centre, not sceneBoundingRect()'s -- the label pulls that
    // one down and makes it depend on whether labels happen to be showing
    // on this layer, disagreeing with the deck-mount attach path's own
    // (stable) reading of the same fixture. See docs/dev/WORKFLOW-SUBTLETIES.md #10.
    if (forAttach && !attachHeld())
        return nullptr;     // a plain drop only moves; Shift attaches
    const QPointF centre = mfi->iconCentreScene();
    TrussItem *best = nullptr;
    float bestDist = 0.0f;

    foreach (TrussItem *ti, m_trussItems)
    {
        Truss *t = ti->truss();
        if (t == nullptr || !ti->isVisible())
            continue;
        if (forAttach && t->locked())
            continue;   // a locked structure does not acquire new members
        if (forAttach && !atHeightOf(m_doc, mfi->fixtureID(), t->origin().z(),
                                     t->positionAt(t->length()).z()))
            continue;   // over it on the plan, but not at its height

        const QPointF local = ti->mapFromScene(centre);
        float dist;
        if (t->type() == Truss::Vertical)
            dist = float(qSqrt(local.x() * local.x() + local.y() * local.y()));
        else
        {
            // Off the ends of the run is not "on the truss", however close the
            // perpendicular distance happens to be.
            if (local.x() < -ti->pxWid() || local.x() > ti->pxLen() + ti->pxWid())
                continue;
            dist = qAbs(float(local.y()));
        }

        if (dist > ti->pxWid() * widths)
            continue;
        if (best == nullptr || dist < bestDist)
        {
            best = ti;
            bestDist = dist;
        }
    }
    return best;
}

PipeItem *MonitorGraphicsView::pipeUnderFixture(MonitorFixtureItem *mfi,
                                                float widths, bool forAttach) const
{
    if (mfi == nullptr || m_cellPixels <= 0)
        return nullptr;

    // See trussUnderFixture()'s own comment on iconCentreScene() vs
    // sceneBoundingRect().center() -- same reasoning applies here.
    if (forAttach && !attachHeld())
        return nullptr;     // a plain drop only moves; Shift attaches
    const QPointF centre = mfi->iconCentreScene();
    PipeItem *best = nullptr;
    float bestDist = 0.0f;

    foreach (PipeItem *pi, m_pipeItems)
    {
        Pipe *b = pi->pipe();
        if (b == nullptr || !pi->isVisible())
            continue;
        if (forAttach && b->locked())
            continue;   // a locked structure does not acquire new members
        if (forAttach && !atHeightOf(m_doc, mfi->fixtureID(), b->positionAt(0.0f).z(),
                                     b->positionAt(b->length()).z()))
            continue;   // over it on the plan, but not at its height

        // PipeItem keeps its own screen geometry private; derive it the same
        // way updatePlatforms() built the item in the first place, straight
        // from the Pipe's own metres fields and this view's scale.
        const float pxWid = float(qMax(6.0, double(b->diameter()) * 1000.0 * m_cellPixels
                                             / m_unitValue));
        const QPointF local = pi->mapFromScene(centre);
        float dist;
        if (b->isVertical())
            dist = float(qSqrt(local.x() * local.x() + local.y() * local.y()));
        else
        {
            const float pxLen = float(double(b->length()) * 1000.0 * m_cellPixels / m_unitValue);
            // Off the ends of the run is not "on the pipe", however close the
            // perpendicular distance happens to be.
            if (local.x() < -pxWid || local.x() > pxLen + pxWid)
                continue;
            dist = qAbs(float(local.y()));
        }

        if (dist > pxWid * widths)
            continue;
        if (best == nullptr || dist < bestDist)
        {
            best = pi;
            bestDist = dist;
        }
    }
    return best;
}

void MonitorGraphicsView::attachFixtureToTrussAt(quint32 fid, TrussItem *trussItem,
                                                 MonitorFixtureItem *mfi)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr || trussItem == nullptr || mfi == nullptr)
        return;
    Truss *t = trussItem->truss();
    if (t == nullptr || m_cellPixels <= 0)
        return;

    FixtureRigProps rp = props->fixtureRigProps(fid);
    rp.clearMounts();   // the structural mounts are mutually exclusive
    rp.trussId = t->id();

    /* Where along the truss it landed, taken from the truss item's own local
       frame so it works whatever the view has done to the truss on screen.
       An END-ON truss (a left-right run seen from the Side) has no length to
       speak of, and a position along it cannot be read from a projection that
       has flattened it away -- so leave the offset alone rather than derive a
       meaningless one from a couple of pixels. */
    const double mmPerPx = double(m_unitValue) / double(m_cellPixels);
    if (t->type() != Truss::Vertical && trussItem->pxLen() > 4.0f)
    {
        const QPointF local = trussItem->mapFromScene(mfi->iconCentreScene());
        const double alongM = qBound(0.0, local.x() * mmPerPx / 1000.0,
                                     double(t->length()));
        rp.trussOffset = float(alongM);
    }
    if (m_pov == PovTop && trussItem->pxLen() > 4.0f)
    {
        /* Keep the sideways offset it was DROPPED at (clamped to the same
           two-widths zone that made the ring green) -- attaching means "hang
           it here", not "teleport it to the centreline". In an elevation the
           across axis is not on screen, so there is nothing to read. */
        const QPointF local = trussItem->mapFromScene(mfi->iconCentreScene());
        const double crossM = local.y() * mmPerPx / 1000.0;
        const double limitM = double(trussItem->pxWid() * 2.0f) * mmPerPx / 1000.0;
        rp.trussCross = float(qBound(-limitM, crossM, limitM));
    }
    else
        rp.trussCross = 0.0f;

    /* A FIXED light dropped on a chord faces out of that side (Q172 A): a
       laser on the upstage chord throws upstage, at the cyc, not down onto
       the deck under the truss. Only one with no face of its own yet; on the
       centreline it hangs and points down, as before. */
    FixtureRigProps::Face side;
    if (!rp.studioFaceChosen && !rp.fixedAim && RigHeads::isFixedUnit(m_doc->fixture(fid))
        && props->trussSideFace(rp, side))
    {
        props->setFixtureFace(rp, side);
        RigHeads::aimAtFace(rp);
    }

    props->setFixtureRigProps(fid, rp);

    /* The top view places a BOUND fixture from the item's cached realPosition,
       not from the rig -- so an attach that leaves the cache stale makes the
       very next redraw teleport the item back to wherever it was before the
       drag. That was the reported "snaps back to where it started". Derive the
       cache from the rig position just stored. */
    const QVector3D nw = props->fixtureRigPosition(fid);
    mfi->setRealPosition(QPointF(nw.x() * 1000.0, nw.y() * 1000.0));

    mfi->setBoundToTruss(true);
    mfi->setAttachMode(false);
    mfi->setEscapeMode(false);
    updateFixture(fid);

    /* Join the truss's group (create/reuse it; pull in ungrouped siblings) --
     * the same thing attachFixtureToTruss() (the explicit "Attach to Truss"
     * context-menu action) already does. Without this, a plain drag-drop
     * attach left the fixture rigged but ungrouped -- correct in the model,
     * but invisible to the Layers list's "on truss" container display until
     * the next file load (ensureGroupRegistry() forms it then). Two
     * different real gestures for "attach this fixture" produced two
     * different downstream states for no reason a user would expect. */
    ensureTrussGroup(t->id());
    if (t->groupId() != 0)
        props->setFixtureGroup(fid, t->groupId());

    m_doc->setModified();
    emit mapStructureChanged();
    refreshItemLayerState();
}

bool MonitorGraphicsView::elevationBarDraggable(const Truss *t) const
{
    if (!isElevation() || t == nullptr || !t->isChildBar())
        return false;
    const Truss *parent = m_doc->monitorProperties()->truss(t->parentTrussId());
    if (parent == nullptr)
        return false;
    // A tower crossbar is draggable in BOTH elevations: Front → height + across
    // (cross-shift); Side → height + depth (stand-off). Together they place it
    // in 3-D by switching POV.
    return (parent->type() == Truss::Vertical && t->barRun() == Truss::RunAcross);
}

void MonitorGraphicsView::slotTrussMoved(TrussItem *item)
{
    // ELEVATION drag of a bar: convert the drop into its truss-local params
    // (Along = height, cross-shift = horizontal) instead of a world origin.
    if (isElevation())
    {
        QList<TrussItem *> moved;
        foreach (QGraphicsItem *gi, m_scene->selectedItems())
            if (auto *ti = dynamic_cast<TrussItem *>(gi); ti != nullptr && itemDraggable(ti)) moved.append(ti);
        if (item != nullptr && !moved.contains(item)) moved.append(item);

        MonitorProperties *props = m_doc->monitorProperties();
        bool any = false;
        const double mPerPx = (m_cellPixels > 0)
            ? double(m_unitValue) / (double(m_cellPixels) * 1000.0) : 0.0;
        // Unit vector out of a truss face (matches the engine's barFaceVector).
        auto faceVec = [](int f) -> QVector3D {
            switch (f) {
                case Truss::FaceTop:        return QVector3D(0, 0, 1);
                case Truss::FaceDownstage:  return QVector3D(0, 1, 0);
                case Truss::FaceUpstage:    return QVector3D(0, -1, 0);
                case Truss::FaceStageRight: return QVector3D(1, 0, 0);
                case Truss::FaceStageLeft:  return QVector3D(-1, 0, 0);
                default:                    return QVector3D(0, 0, -1);
            }
        };
        foreach (TrussItem *ti, moved)
        {
            Truss *t = ti->truss();
            if (t == nullptr)
                continue;
            if (!elevationBarDraggable(t))
            {
                /* A free-standing truss has no parent to be positioned
                   relative to, so the drop is simply its new origin -- read
                   back through the same projection it was drawn with, leaving
                   the axis that is not on screen alone. */
                if ((t->locked()) || m_layoutLocked)
                    continue;
                const QVector3D curMm(t->origin().x() * 1000.0f,
                                      t->origin().y() * 1000.0f,
                                      t->origin().z() * 1000.0f);
                const QVector3D newMm = unprojectMm(ti->pos(), curMm);
                const QVector3D newOrigin(newMm.x() / 1000.0f,
                                          newMm.y() / 1000.0f,
                                          newMm.z() / 1000.0f);
                if (!qFuzzyCompare(newOrigin, t->origin()))
                {
                    t->setOrigin(newOrigin);
                    any = true;
                }
                continue;
            }
            const Truss *parent = props->truss(t->parentTrussId());
            // Drop DELTA in the two visible screen axes.
            const QPointF pExpected = projectMm(t->origin().x() * 1000.0,
                                                t->origin().y() * 1000.0,
                                                t->origin().z() * 1000.0);
            const QPointF pDrop = ti->pos();
            const double dH =  (pDrop.x() - pExpected.x()) * mPerPx;   // screen horizontal
            const double dV = -(pDrop.y() - pExpected.y()) * mPerPx;   // screen vertical (up)
            // World delta: Front sees X horizontal, Side sees Y horizontal; both
            // see Z vertical.
            const QVector3D wd = (m_pov == PovFront) ? QVector3D(float(dH), 0, float(dV))
                                                     : QVector3D(0, float(dH), float(dV));
            const QVector3D runVec(t->direction().x(), t->direction().y(), 0);
            const QVector3D faceN = faceVec(t->barFace());

            float newCross = t->barCrossShift() + QVector3D::dotProduct(wd, runVec);
            float newStand = t->barStandoff()   + QVector3D::dotProduct(wd, faceN);
            float newAlong = t->parentOffset()  + float(dV);   // tower height = Z
            if (qAbs(newCross) < 0.08f) newCross = 0.0f;        // bump-snap centred
            if (newStand < 0.0f) newStand = 0.0f;
            newAlong = qBound(0.0f, newAlong, parent ? parent->length() : newAlong);
            t->setBarCrossShift(newCross);
            t->setBarStandoff(newStand);
            t->setParentOffset(newAlong);
            any = true;
        }
        if (any)
        {
            props->recomputeChildTrusses();
            followParentTrusses();
            m_doc->setModified();
        }
        return;
    }

    // When a group/multi-selection is dragged, Qt moves every selected truss
    // together — persist them all, not just the one that received the drop.
    QList<TrussItem *> moved;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (auto *ti = dynamic_cast<TrussItem *>(gi); ti != nullptr && itemDraggable(ti))
            moved.append(ti);
    if (item != nullptr && !moved.contains(item))
        moved.append(item);

    // Snap the dragged truss's origin to the grid, then shift the rest by the
    // same delta so a grouped selection keeps its relative layout intact.
    if (m_snapDivisions > 0 && m_cellPixels > 0 && item != nullptr)
    {
        const QPointF snapped = snapScenePos(item->pos());
        const QPointF delta = snapped - item->pos();
        if (!delta.isNull())
        {
            foreach (TrussItem *ti, moved)
                ti->setPos(ti->pos() + delta);
        }
    }

    UndoEntry e; e.type = UndoEntry::TrussMove;
    foreach (TrussItem *ti, moved)
    {
        Truss *t = ti->truss();
        if (t == nullptr)
            continue;
        QPointF mm = pixelsToRealPosition(ti->pos().x(), ti->pos().y());
        QVector3D newOrigin(float(mm.x() / 1000.0), float(mm.y() / 1000.0), t->origin().z());
        if (newOrigin != t->origin())
        {
            e.trussOrigins.insert(t->id(), t->origin());
            t->setOrigin(newOrigin);
        }
    }

    if (!e.trussOrigins.isEmpty())
    {
        pushUndo(e);
        m_doc->setModified();
    }

    // Bound fixtures are physically attached: follow the truss to its new place
    // -- and so do the ones hung on its hangers.
    MonitorProperties *props = m_doc->monitorProperties();
    props->recomputeHangers();
    foreach (TrussItem *ti, moved)
    {
        Truss *t = ti->truss();
        if (t == nullptr)
            continue;
        for (auto fit = m_fixtures.constBegin(); fit != m_fixtures.constEnd(); ++fit)
        {
            const FixtureRigProps rp = props->fixtureRigProps(fit.key());
            const Hanger *hung = rp.onHanger() ? props->hanger(rp.hangerId) : nullptr;
            const bool onIt = rp.trussId == t->id()
                || (hung != nullptr && hung->parentKind() == Hanger::OnTruss && hung->parentId() == t->id());
            if (!onIt)
                continue;
            // fixtureRigPosition() (not the bare positionAt()) so a fixture
            // dropped at a cross (perpendicular) offset from the centreline
            // keeps that offset as its truss rides along with it.
            const QVector3D w = props->fixtureRigPosition(fit.key());   // world metres
            const QPointF mm(w.x() * 1000.0, w.y() * 1000.0);
            moveFixtureTo(fit.key(), mm);       // reposition the item on the canvas
            emit fixtureMoved(fit.key(), mm);   // persist via Monitor::slotFixtureMoved
        }
    }

    // Any child bar hung on a moved truss follows it. Rebuilding truss items
    // here (mid-drop) would free the item we're handling — defer to the next
    // event-loop turn, once the drop has fully unwound.
    QSet<quint32> movedIds;
    foreach (TrussItem *ti, moved)
        if (ti->truss()) movedIds.insert(ti->truss()->id());
    bool hasChildBars = false;
    foreach (const Truss *bar, props->trusses())
        if (bar->isChildBar() && movedIds.contains(bar->parentTrussId()))
        {
            hasChildBars = true;
            break;
        }
    if (hasChildBars)
        QTimer::singleShot(0, this, [this]() { followParentTrusses(); });

    updateTrussAnchorLines();
    refreshFixtureCountBadges();       // its count bubble goes where it went
}

PlatformItem *MonitorGraphicsView::newPlatformItem(StagePlatform *p)
{
    MonitorProperties *props = m_doc->monitorProperties();
    const float scale = float(m_cellPixels) / m_unitValue;
    float pxX, pxY, pxW, pxD;
    if (m_pov == PovTop)
    {
        pxX = float(m_xOffset + (p->originX() * 1000.0 * m_cellPixels) / m_unitValue);
        pxY = float(m_yOffset + (p->originY() * 1000.0 * m_cellPixels) / m_unitValue);
        pxW = float((p->width()  * 1000.0 * m_cellPixels) / m_unitValue);
        pxD = float((p->depth()  * 1000.0 * m_cellPixels) / m_unitValue);
    }
    else
    {
        // Elevation: a riser is a box from its BASE (which may be the deck of
        // a platform it sits on, not the floor) up to its deck top. height()
        // is the block's own thickness, so the absolute top is base + height.
        const float b0  = props->platformBaseZ(p->id());
        const float top = b0 + p->height();
        const QPointF tl = projectMm(p->originX() * 1000.0, p->originY() * 1000.0, top * 1000.0);
        pxX = float(tl.x());
        pxY = float(tl.y());
        pxW = float(((m_pov == PovFront) ? p->width() : p->depth()) * 1000.0f * scale);
        pxD = qMax(2.0f, float(p->height() * 1000.0f * scale));
        if (p->isRotated())
        {
            // Turned, it is seen as wide as its outline reaches.
            double ax = 1e9, ay = 1e9, bx = -1e9, by = -1e9;
            foreach (const QPointF &q, p->footprint(16))
            {
                ax = qMin(ax, q.x()); bx = qMax(bx, q.x());
                ay = qMin(ay, q.y()); by = qMax(by, q.y());
            }
            const QPointF a = projectMm(ax * 1000.0, ay * 1000.0, top * 1000.0);
            const QPointF b = projectMm(bx * 1000.0, by * 1000.0, top * 1000.0);
            pxX = float(qMin(a.x(), b.x()));
            pxW = float(qAbs(b.x() - a.x()));
        }
    }

    PlatformItem *pi = new PlatformItem(p, m_doc, pxX, pxY, pxW, pxD);
    pi->setView(m_pov == PovTop ? 0 : m_pov == PovSide ? 2 : 1);
    if (m_pov == PovTop && p->isRotated())
    {
        // Turned about its middle; its position is still the unturned corner.
        pi->setTransformOriginPoint(pxW / 2.0, pxD / 2.0);
        pi->setRotation(double(p->rotation()));
    }
    return pi;
}

void MonitorGraphicsView::updatePlatforms()
{
    /* The marks follow the deck and the curtain they are found from: redrawn
       with them, however this returns. */
    auto marks = qScopeGuard([this]() {
        updateStageMarks();
        refreshShapeHandles();      // the platform or scenery may have changed
        // A target's readout gives the deck it stands on.
        foreach (TargetItem *ti, m_targetItems)
            ti->refreshPositionReadout();
    });
    QScopedValueRollback<bool> rb(m_itemsRebuilding); m_itemsRebuilding = true;
    foreach (PlatformItem *pi, m_platformItems)
    {
        m_scene->removeItem(pi);
        delete pi;
    }
    m_platformItems.clear();
    foreach (PipeItem *bi, m_pipeItems)
    {
        m_scene->removeItem(bi);
        delete bi;
    }
    m_pipeItems.clear();
    foreach (StandItem *si, m_standItems)
    {
        m_scene->removeItem(si);
        delete si;
    }
    m_standItems.clear();
    foreach (SceneryItem *sci, m_sceneryItems)
    {
        m_scene->removeItem(sci);
        delete sci;
    }
    m_sceneryItems.clear();
    foreach (TowerItem *twi, m_towerItems)
    {
        m_scene->removeItem(twi);
        delete twi;
    }
    m_towerItems.clear();

    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr || m_cellPixels == 0)
        return;

    foreach (StagePlatform *p, props->platforms())
    {
        // The apron's own deck is the apron piece's (updateStageMarks()).
        if (props->isApronPlatform(p->id()))
            continue;
        PlatformItem *pi = newPlatformItem(p);
        pi->setMovable(!m_layoutLocked && (m_pov == PovTop || !p->isRotated()));
        pi->showLabel(m_doc->monitorProperties()->layer(p->layerId()).labels);
        m_scene->addItem(pi);
        m_platformItems.insert(p->id(), pi);

        connect(pi, &PlatformItem::itemDropped,
                this, &MonitorGraphicsView::slotPlatformMoved);
    }

    // Pipes: vertical (boom) = plan disc + dot in Top, vertical pipe in elevation;
    // horizontal (electric) = a line from start to end, projected per POV.
    auto toTopPx = [&](const QVector3D &w) {
        return QPointF(m_xOffset + (w.x() * 1000.0 * m_cellPixels) / m_unitValue,
                       m_yOffset + (w.y() * 1000.0 * m_cellPixels) / m_unitValue);
    };
    foreach (Pipe *b, props->pipes())
    {
        const float pxPipe = float((b->diameter() * 1000.0 * m_cellPixels) / m_unitValue);
        PipeItem *bi = nullptr;
        if (b->orientation() == Pipe::Horizontal)
        {
            const QVector3D s = b->positionAt(0.0f);
            const QVector3D e = b->positionAt(b->length());
            const QPointF ps = (m_pov == PovTop) ? toTopPx(s)
                             : projectMm(s.x() * 1000.0, s.y() * 1000.0, s.z() * 1000.0);
            const QPointF pe = (m_pov == PovTop) ? toTopPx(e)
                             : projectMm(e.x() * 1000.0, e.y() * 1000.0, e.z() * 1000.0);
            bi = new PipeItem(b, m_doc, float(ps.x()), float(ps.y()), 0.0f, pxPipe);
            bi->setLine(pe - ps);
        }
        else if (m_pov == PovTop)
        {
            const float pxCX = float(m_xOffset + (b->originX() * 1000.0 * m_cellPixels) / m_unitValue);
            const float pxCY = float(m_yOffset + (b->originY() * 1000.0 * m_cellPixels) / m_unitValue);
            const float pxBaseR = float((b->baseRadius() * 1000.0 * m_cellPixels) / m_unitValue);
            bi = new PipeItem(b, m_doc, pxCX, pxCY, pxBaseR, pxPipe);
        }
        else
        {
            const QPointF base = projectMm(b->originX() * 1000.0, b->originY() * 1000.0, b->baseZ() * 1000.0);
            const QPointF top  = projectMm(b->originX() * 1000.0, b->originY() * 1000.0,
                                           (b->baseZ() + b->height()) * 1000.0);
            const float pxHeight = float(qAbs(top.y() - base.y()));
            bi = new PipeItem(b, m_doc, float(base.x()), float(base.y()),
                                     0.0f, pxPipe, true, pxHeight);
        }
        bi->setMovable(!m_layoutLocked);
        bi->showLabel(m_doc->monitorProperties()->layer(b->layerId()).labels);
        m_scene->addItem(bi);
        m_pipeItems.insert(b->id(), bi);
        connect(bi, &PipeItem::itemDropped,
                this, &MonitorGraphicsView::slotPipeMoved);
        connect(bi, &PipeItem::addBarRequested,
                this, &MonitorGraphicsView::addBarToPipeRequested);
    }

    // Stands: base disc + post in Top; a vertical post in the elevations.
    foreach (Stand *s, props->stands())
    {
        const float pxBaseR = float((s->baseRadius() * 1000.0 * m_cellPixels) / m_unitValue);
        StandItem *si = nullptr;
        if (m_pov == PovTop)
        {
            const float pxCX = float(m_xOffset + (s->originX() * 1000.0 * m_cellPixels) / m_unitValue);
            const float pxCY = float(m_yOffset + (s->originY() * 1000.0 * m_cellPixels) / m_unitValue);
            si = new StandItem(s, m_doc, pxCX, pxCY, pxBaseR);
        }
        else
        {
            const QPointF base = projectMm(s->originX() * 1000.0, s->originY() * 1000.0, 0.0);
            const QPointF top  = projectMm(s->originX() * 1000.0, s->originY() * 1000.0, s->height() * 1000.0);
            si = new StandItem(s, m_doc, float(base.x()), float(base.y()),
                               pxBaseR, true, float(qAbs(top.y() - base.y())));
        }
        si->setMovable(!m_layoutLocked);
        si->showLabel(m_doc->monitorProperties()->layer(s->layerId()).labels);
        m_scene->addItem(si);
        m_standItems.insert(s->id(), si);
        connect(si, &StandItem::itemDropped, this, &MonitorGraphicsView::slotStandMoved);
    }

    /* Scenery: its line across the floor in Top, the sheet itself in the
       elevations -- a bowed cyc as the curve it really is. */
    foreach (Scenery *sc, props->sceneries())
    {
        const QPointF anchor = projectMm(sc->originX() * 1000.0, sc->originY() * 1000.0,
                                         sc->bottom() * 1000.0);
        bool flown = false;
        const Scenery::Pose pose = sceneryPose(sc);
        const QList<QPolygonF> parts = sceneryParts(sc, pose, &flown);
        SceneryItem *sci = new SceneryItem(sc, m_doc, anchor, parts, m_pov != PovTop);
        sci->setParts(anchor, parts, flown, pose);
        sci->setMovable(!m_layoutLocked);
        m_scene->addItem(sci);
        m_sceneryItems.insert(sc->id(), sci);
        connect(sci, &SceneryItem::itemDropped, this, &MonitorGraphicsView::slotSceneryMoved);
        /* A curtain driven by a fixture opens as its channel moves. The values
           change on the engine's thread; the item, the receiver, takes them on
           this one -- and goes with the item. */
        if (sc->isDriven())
            if (Fixture *drv = m_doc->fixture(sc->driverFixture()))
            {
                const quint32 sid = sc->id();
                connect(drv, &Fixture::valuesChanged, sci, [this, sid]() { refreshScenery(sid); });
            }
    }

    // Towers: footprint square in Top; box + shelf lines in the elevations.
    foreach (Tower *tw, props->towers())
    {
        TowerItem *twi = nullptr;
        if (m_pov == PovTop)
        {
            const float pxX = float(m_xOffset + (tw->originX() * 1000.0 * m_cellPixels) / m_unitValue);
            const float pxY = float(m_yOffset + (tw->originY() * 1000.0 * m_cellPixels) / m_unitValue);
            const float pxW = float((tw->width() * 1000.0 * m_cellPixels) / m_unitValue);
            const float pxD = float((tw->depth() * 1000.0 * m_cellPixels) / m_unitValue);
            twi = new TowerItem(tw, m_doc, pxX, pxY, pxW, pxD);
            if (tw->isRotated())
            {
                twi->setTransformOriginPoint(pxW / 2.0, pxD / 2.0);
                twi->setRotation(double(tw->rotation()));
            }
        }
        else
        {
            QPointF base = projectMm(tw->originX() * 1000.0, tw->originY() * 1000.0, 0.0);
            const QPointF top  = projectMm(tw->originX() * 1000.0, tw->originY() * 1000.0, tw->height() * 1000.0);
            float pxW = float(((m_pov == PovFront) ? tw->width() : tw->depth()) * 1000.0f * m_cellPixels / m_unitValue);
            if (tw->isRotated())
            {
                // Turned, it is seen as wide as its corners reach.
                double ax = 1e9, ay = 1e9, bx = -1e9, by = -1e9;
                foreach (const QPointF &q, tw->corners())
                {
                    ax = qMin(ax, q.x()); bx = qMax(bx, q.x());
                    ay = qMin(ay, q.y()); by = qMax(by, q.y());
                }
                const QPointF a = projectMm(ax * 1000.0, ay * 1000.0, 0.0);
                const QPointF b = projectMm(bx * 1000.0, by * 1000.0, 0.0);
                base.setX(qMin(a.x(), b.x()));
                pxW = float(qAbs(b.x() - a.x()));
            }
            const float pxH = float(qAbs(top.y() - base.y()));
            twi = new TowerItem(tw, m_doc, float(base.x()), float(base.y()), pxW, pxH, true);
        }
        twi->setMovable(!m_layoutLocked && (m_pov == PovTop || !tw->isRotated()));
        twi->showLabel(m_doc->monitorProperties()->layer(tw->layerId()).labels);
        m_scene->addItem(twi);
        m_towerItems.insert(tw->id(), twi);
        connect(twi, &TowerItem::itemDropped, this, &MonitorGraphicsView::slotTowerMoved);
    }

    updateHangers();      // and off towers
    updateNoLightZones();
    refreshItemLayerState();
}

void MonitorGraphicsView::dropStructureItems()
{
    QScopedValueRollback<bool> rb(m_itemsRebuilding); m_itemsRebuilding = true;
    dropTargetItems();

    // Whatever a press remembered may be one of them.
    m_savedSelection.clear();
    m_clickAgainItem = nullptr;

    auto drop = [this](auto &items) {
        foreach (auto *item, items)
        {
            m_scene->removeItem(item);
            delete item;
        }
        items.clear();
    };
    drop(m_hangerItems);
    drop(m_zoneItems);
    drop(m_trussItems);
    drop(m_platformItems);
    drop(m_pipeItems);
    drop(m_standItems);
    drop(m_sceneryItems);
    drop(m_towerItems);
    if (m_apronDeckItem != nullptr)
    {
        m_scene->removeItem(m_apronDeckItem);
        delete m_apronDeckItem;
        m_apronDeckItem = nullptr;
    }
}

void MonitorGraphicsView::dropItemFor(QObject *structure)
{
    QScopedValueRollback<bool> rb(m_itemsRebuilding); m_itemsRebuilding = true;
    /* Out of the scene and the view now; deleted later, because the
       structure may be going from its own item's menu (a target's Delete),
       which is still running. Out of the scene, it is neither drawn nor
       asked for its bounds again. */
    auto drop = [this](auto *item) {
        if (item == nullptr)
            return;
        m_savedSelection.removeAll(item);
        if (m_clickAgainItem == item)
            m_clickAgainItem = nullptr;
        if (item->scene() != nullptr)
            item->scene()->removeItem(item);
        item->deleteLater();
    };
    if (Truss *t = qobject_cast<Truss *>(structure))
    {
        drop(m_trussItems.take(t->id()));
    }
    else if (StagePlatform *p = qobject_cast<StagePlatform *>(structure))
    {
        drop(m_platformItems.take(p->id()));
        if (m_apronDeckItem != nullptr && m_apronDeckItem->platform() == p)
        {
            drop(m_apronDeckItem);
            m_apronDeckItem = nullptr;
        }
    }
    else if (Pipe *b = qobject_cast<Pipe *>(structure))
    {
        drop(m_pipeItems.take(b->id()));
    }
    else if (Stand *st = qobject_cast<Stand *>(structure))
    {
        drop(m_standItems.take(st->id()));
    }
    else if (Tower *tw = qobject_cast<Tower *>(structure))
    {
        drop(m_towerItems.take(tw->id()));
    }
    else if (Scenery *sc = qobject_cast<Scenery *>(structure))
    {
        drop(m_sceneryItems.take(sc->id()));
    }
    else if (StageTarget *tg = qobject_cast<StageTarget *>(structure))
    {
        drop(m_targetItems.take(tg->id()));
    }
    // A HangerItem keeps only its hanger's id: updateHangers() rebuilds it.
}

void MonitorGraphicsView::updateHangers()
{
    /* "Hangers on the 2D plot": the arm, chain and hook a fixture hangs from,
       placed from the truss or tower it is fastened to -- rebuilt with them. */
    foreach (HangerItem *hi, m_hangerItems)
    {
        m_scene->removeItem(hi);
        delete hi;
    }
    m_hangerItems.clear();
    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr || m_cellPixels <= 0)
        return;
    props->recomputeHangers();
    auto at = [this](const QVector3D &m) {
        return projectMm(double(m.x()) * 1000.0, double(m.y()) * 1000.0, double(m.z()) * 1000.0);
    };
    foreach (Hanger *h, props->hangers())
    {
        if (h == nullptr)
            continue;
        HangerItem *hi = new HangerItem(h->id(), at(h->fastened()), at(h->armEnd()), at(h->hook()));
        hi->setZValue(1.8);                // over the structures, under the fixtures (2)
        m_scene->addItem(hi);
        m_hangerItems.insert(h->id(), hi);
    }
}

void MonitorGraphicsView::updateNoLightZones()
{
    /* The zones that must stay dark, and where each fixed laser's H x V window
       lands: drawn in whichever view the plot shows (a zone's box in the
       elevations, both on the floor in Top), over the structures and under the
       fixtures. They take no clicks; zoneAt() finds a zone for the menu. */
    foreach (ZoneItem *zi, m_zoneItems)
    {
        m_scene->removeItem(zi);
        delete zi;
    }
    m_zoneItems.clear();
    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr || m_cellPixels <= 0)
        return;
    auto pt = [this](double xm, double ym, double zm) {
        return projectMm(xm * 1000.0, ym * 1000.0, zm * 1000.0);
    };
    foreach (const NoLightZone &z, props->noLightZones())
    {
        const double x0 = z.x, x1 = z.x + z.width, y0 = z.y, y1 = z.y + z.depth;
        const double z0 = z.floorZ, z1 = z.floorZ + z.height;
        QPolygonF poly;
        if (m_pov == PovTop)
            poly << pt(x0, y0, 0) << pt(x1, y0, 0) << pt(x1, y1, 0) << pt(x0, y1, 0);
        else if (m_pov == PovFront)
            poly << pt(x0, 0, z1) << pt(x1, 0, z1) << pt(x1, 0, z0) << pt(x0, 0, z0);
        else
            poly << pt(0, y0, z1) << pt(0, y1, z1) << pt(0, y1, z0) << pt(0, y0, z0);
        ZoneItem *zi = new ZoneItem(ZoneItem::Zone, z.id, poly,
                                    QStringLiteral("%1 (%2)").arg(z.name, NoLightZone::kindLabel(z.kind)));
        zi->setZValue(1.7);
        m_scene->addItem(zi);
        m_zoneItems.append(zi);
    }
    if (m_pov != PovTop)
        return;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr || fx->fixtureMode() == nullptr || !props->containsFixture(fx->id())
            || !RigHeads::isFixedUnit(fx))
            continue;
        const QLCPhysical &phy = fx->fixtureMode()->physical();
        if (phy.emission() != QLCPhysical::LaserEmission
            && !(fx->fixtureDef() != nullptr && fx->fixtureDef()->type() == QLCFixtureDef::Laser))
            continue;
        QVector3D dir;
        if (!RigHeads::fixedThrow(m_doc, fx->id(), dir) || dir.isNull())
            continue;
        dir.normalize();
        const QVector3D apex = RigHeads::fixtureCentre(m_doc, fx->id());
        double h, v;
        LightRays::laserWindow(phy, h, v);
        // 9 x 9, i-major, with whatever tape is on it cut off.
        const QVector<QVector3D> rays = LightRays::window(dir, h, v, props->fixtureRigProps(fx->id()).mask());
        if (rays.size() != 81)
            continue;                  // taped over: nothing leaves it
        /* Where each ray of the window ends: the first of the floor and a
           piece of scenery standing in its way (a cyc is what a sky laser
           throws onto), the same rule the rig view and Show Review use
           (Scenery::rayHit, a scrim lets the ray on through). A ray that
           meets the cyc lands ON the cyc, not on the empty floor behind it. */
        QVector<const Scenery *> screens;
        foreach (Scenery *sc, props->sceneries())
        {
            if (sc != nullptr && props->layer(sc->layerId()).visible && !sc->isSeeThrough())
                screens << sc;
        }
        QVector<bool> onScenery;
        auto landing = [&](int i, int j) {
            const QVector3D r = rays.at((i + 4) * 9 + (j + 4));
            double t = 30.0;
            if (r.z() < -1e-4f && apex.z() > 0.0f)
                t = qMin(t, double(apex.z()) / -double(r.z()));
            bool hit = false;
            foreach (const Scenery *sc, screens)
            {
                double st = 0.0;
                if (sc->rayHit(apex, r, st, t) && st > 0.05 && st < t)
                {
                    t = st;
                    hit = true;
                }
            }
            onScenery << hit;
            const double z = hit ? double(apex.z()) + double(r.z()) * t : 0.0;
            return pt(double(apex.x()) + double(r.x()) * t, double(apex.y()) + double(r.y()) * t,
                      m_pov == PovTop ? 0.0 : z);
        };
        QPolygonF poly;
        for (int j = -4; j <= 4; ++j)  poly << landing(-4, j);
        for (int i = -3; i <= 4; ++i)  poly << landing(i, 4);
        for (int j = 3; j >= -4; --j)  poly << landing(4, j);
        for (int i = 3; i >= -3; --i)  poly << landing(i, -4);
        /* The pool on the cyc, as a highlighted span along its line; the
           floor part, if any, stays the dashed outline. */
        QVector<QLineF> spans;
        for (int k = 0; k < poly.size(); ++k)
        {
            const int n = (k + 1) % poly.size();
            if (onScenery.at(k) && onScenery.at(n))
                spans << QLineF(poly.at(k), poly.at(n));
        }
        /* Never drawn out past the stage: clipped to the plot's floor. */
        const QPointF a = pt(0.0, 0.0, 0.0);
        const QPointF b = pt(double(m_gridExtent.width()) * m_unitValue / 1000.0,
                             double(m_gridExtent.height()) * m_unitValue / 1000.0, 0.0);
        const QRectF floorRect = QRectF(a, b).normalized();
        if (!floorRect.contains(poly.boundingRect()))
        {
            // Nothing of it on the floor at all: no outline (a span may remain).
            poly = poly.intersected(QPolygonF(floorRect));
            QVector<QLineF> keep;
            foreach (const QLineF &l, spans)
            {
                if (floorRect.adjusted(-1, -1, 1, 1).contains(l.p1())
                    && floorRect.adjusted(-1, -1, 1, 1).contains(l.p2()))
                    keep << l;
            }
            spans = keep;
        }
        /* Where the window crosses a no-light zone that keeps lasers out: the
           part of each ray inside it, in red, so the band's leak shows (and
           the tape that clears it shows by it going away). A ray stopped by
           the cyc first does not reach a zone behind it. */
        QVector<QLineF> leaks;
        foreach (const NoLightZone &z, props->noLightZones())
        {
            if (!z.applies(true, 0.0))
                continue;
            foreach (const QVector3D &r, rays)
            {
                double stop = 30.0;
                foreach (const Scenery *sc, screens)
                {
                    double st = 0.0;
                    if (sc->rayHit(apex, r, st, stop) && st > 0.05)
                        stop = st;
                }
                double t0, t1;
                if (!z.raySpan(apex, r, stop, t0, t1) || t1 <= t0)
                    continue;
                leaks << QLineF(pt(double(apex.x()) + double(r.x()) * t0, double(apex.y()) + double(r.y()) * t0, 0),
                                pt(double(apex.x()) + double(r.x()) * t1, double(apex.y()) + double(r.y()) * t1, 0));
            }
        }
        if (poly.isEmpty() && spans.isEmpty() && leaks.isEmpty())
            continue;
        ZoneItem *zi = new ZoneItem(ZoneItem::ScanWindow, fx->id(), poly, fx->name());
        zi->setSpans(spans);
        zi->setLeaks(leaks);
        zi->setZValue(1.6);
        m_scene->addItem(zi);
        m_zoneItems.append(zi);
    }
}

quint32 MonitorGraphicsView::zoneGrabAt(const QPointF &scenePos) const
{
    const qreal m = qMax<qreal>(0.01, qAbs(transform().m11()));
    const qreal tol = 5.0 / m;          // five screen pixels
    for (int i = m_zoneItems.size() - 1; i >= 0; --i)
        if (m_zoneItems.at(i)->grabAt(scenePos, tol))
            return m_zoneItems.at(i)->id();
    return UINT_MAX;
}

quint32 MonitorGraphicsView::zoneAt(const QPointF &scenePos) const
{
    for (int i = m_zoneItems.size() - 1; i >= 0; --i)
        if (m_zoneItems.at(i)->role() == ZoneItem::Zone && m_zoneItems.at(i)->contains(scenePos))
            return m_zoneItems.at(i)->id();
    return UINT_MAX;
}

void MonitorGraphicsView::slotTowerMoved(TowerItem *item)
{
    if (item == nullptr || m_cellPixels == 0)
        return;

    // Persist every selected tower (group move), not just the dropped one --
    // Qt's QGraphicsScene only delivers mouse press/move/release to the item
    // actually grabbed during a multi-select drag; co-selected siblings move
    // on screen (Qt repositions them internally) but never fire their own
    // itemDropped(), so without rebuilding the selection here only the one
    // item the mouse happened to grab actually got saved.
    QList<TowerItem *> moved;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (auto *ti = dynamic_cast<TowerItem *>(gi); ti != nullptr && itemDraggable(ti))
            moved.append(ti);
    if (!moved.contains(item))
        moved.append(item);

    // Snap the dragged one to the grid; the rest move by the same delta.
    if (m_snapDivisions > 0)
    {
        const QPointF delta = snapScenePos(item->pos()) - item->pos();
        if (!delta.isNull())
        {
            foreach (TowerItem *ti2, moved)
                ti2->setPos(ti2->pos() + delta);
        }
    }

    UndoEntry undo; undo.type = UndoEntry::ItemMove;
    foreach (TowerItem *ti, moved)
    {
        Tower *tw = ti->tower();
        if (tw == nullptr)
            continue;
        const QPointF c = ti->pos();
        undo.itemKeys << qMakePair(QStringLiteral("tower"), tw->id());
        undo.itemOrigins << QPointF(double(tw->originX()), double(tw->originY()));
        tw->setOriginX(float(((c.x() - m_xOffset) * m_unitValue) / (1000.0 * m_cellPixels)));
        tw->setOriginY(float(((c.y() - m_yOffset) * m_unitValue) / (1000.0 * m_cellPixels)));
        if (undo.itemOrigins.last() == QPointF(double(tw->originX()), double(tw->originY())))
        { undo.itemKeys.removeLast(); undo.itemOrigins.removeLast(); }
        // Fixtures on its shelves follow (derived) -- and those on its hangers.
        MonitorProperties *props = m_doc->monitorProperties();
        props->recomputeHangers();
        foreach (Fixture *fx, m_doc->fixtures())
        {
            const FixtureRigProps rp = props->fixtureRigProps(fx->id());
            const Hanger *hung = rp.onHanger() ? props->hanger(rp.hangerId) : nullptr;
            if (rp.towerId == tw->id()
                || (hung != nullptr && hung->parentKind() == Hanger::OnTower && hung->parentId() == tw->id()))
                updateFixture(fx->id());
        }
    }
    if (!undo.itemKeys.isEmpty())
        pushUndo(undo);
    refreshFixtureCountBadges();
    m_doc->setModified();
    emit mapStructureChanged();
}

void MonitorGraphicsView::slotStandMoved(StandItem *item)
{
    if (item == nullptr || m_cellPixels == 0)
        return;

    // Persist every selected stand (group move) -- see slotTowerMoved()'s
    // comment for why this rebuild from the live selection is necessary.
    QList<StandItem *> moved;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (auto *si = dynamic_cast<StandItem *>(gi); si != nullptr && itemDraggable(si))
            moved.append(si);
    if (!moved.contains(item))
        moved.append(item);

    // Snap the dragged one to the grid; the rest move by the same delta.
    if (m_snapDivisions > 0)
    {
        const QPointF delta = snapScenePos(item->pos()) - item->pos();
        if (!delta.isNull())
        {
            foreach (StandItem *si2, moved)
                si2->setPos(si2->pos() + delta);
        }
    }

    UndoEntry undo; undo.type = UndoEntry::ItemMove;
    foreach (StandItem *si, moved)
    {
        Stand *s = si->stand();
        if (s == nullptr)
            continue;
        const QPointF c = si->pos();
        undo.itemKeys << qMakePair(QStringLiteral("stand"), s->id());
        undo.itemOrigins << QPointF(double(s->originX()), double(s->originY()));
        s->setOriginX(float(((c.x() - m_xOffset) * m_unitValue) / (1000.0 * m_cellPixels)));
        s->setOriginY(float(((c.y() - m_yOffset) * m_unitValue) / (1000.0 * m_cellPixels)));
        if (undo.itemOrigins.last() == QPointF(double(s->originX()), double(s->originY())))
        { undo.itemKeys.removeLast(); undo.itemOrigins.removeLast(); }
    }
    if (!undo.itemKeys.isEmpty())
        pushUndo(undo);
    m_doc->monitorProperties()->recomputeStandMounts();   // pipes on it follow
    m_doc->setModified();
    emit mapStructureChanged();
}

Scenery::Pose MonitorGraphicsView::sceneryPose(const Scenery *sc) const
{
    /* A curtain driven by a fixture is where that fixture's channels put it
       (a patched one's last output, an unpatched one's preview) -- closed at
       0, whatever its fixed state says. Undriven, or its fixture gone: its
       fixed state, as it always was. */
    if (sc == nullptr)
        return Scenery::Pose();
    if (sc->isDriven())
        if (Fixture *fx = m_doc->fixture(sc->driverFixture()))
            return Scenery::poseFromValues(fx->fixtureMode(), fx->channelValues());
    return sc->staticPose();
}

QList<QPolygonF> MonitorGraphicsView::sceneryParts(const Scenery *sc, const Scenery::Pose &pose,
                                                   bool *flown) const
{
    auto at = [&](double u, double z, double off) {
        const QVector3D p = sc->pointAt(u, z, off);
        return projectMm(p.x() * 1000.0, p.y() * 1000.0, p.z() * 1000.0);
    };
    const double half = double(sc->thickness()) / 2.0;
    QList<Scenery::Piece> pieces = sc->pieces(pose);
    if (flown != nullptr)
        *flown = pieces.isEmpty();
    if (pieces.isEmpty())                    // flown out: where it hangs, dashed
        pieces << Scenery::Piece{ -1.0, 1.0, sc->bottom(), sc->bottom() + sc->height() };
    QList<QPolygonF> parts;
    for (const Scenery::Piece &pc : pieces)
    {
        const QPair<double, double> sp(pc.u0, pc.u1);
        const float lo = pc.z0, hi = pc.z1;
        const int segs = qMax(1, int(qCeil(qMax(16, sc->segments()) * (sp.second - sp.first) / 2.0)));
        QPolygonF part;
        if (m_pov == PovTop)
        {
            // Its line; a wall's footprint, out along one face and back along the other.
            for (int i = 0; i <= segs; ++i)
                part << at(sp.first + (sp.second - sp.first) * i / segs, lo, -half);
            if (half > 0.0)
                for (int i = segs; i >= 0; --i)
                    part << at(sp.first + (sp.second - sp.first) * i / segs, lo, half);
        }
        else
        {
            for (int i = 0; i <= segs; ++i)
                part << at(sp.first + (sp.second - sp.first) * i / segs, lo, 0.0);
            for (int i = segs; i >= 0; --i)
                part << at(sp.first + (sp.second - sp.first) * i / segs, hi, 0.0);
        }
        parts << part;
    }
    return parts;
}

void MonitorGraphicsView::refreshScenery(quint32 sceneryId)
{
    SceneryItem *sci = m_sceneryItems.value(sceneryId, nullptr);
    Scenery *sc = m_doc->monitorProperties()->scenery(sceneryId);
    if (sci == nullptr || sc == nullptr)
        return;
    const Scenery::Pose pose = sceneryPose(sc);
    if (sci->hasPose() && sci->pose() == pose)
        return;
    bool flown = false;
    const QList<QPolygonF> parts = sceneryParts(sc, pose, &flown);
    const QPointF anchor = projectMm(sc->originX() * 1000.0, sc->originY() * 1000.0,
                                     sc->bottom() * 1000.0);
    sci->setParts(anchor, parts, flown, pose);
    // A curtain that opened or closed can block, or clear, an aim trace.
    if (m_doc->monitorProperties()->occlusionKinds() & BeamOcclusion::Drapes)
        updateAimLines();
}

void MonitorGraphicsView::slotSceneryMoved(SceneryItem *item)
{
    if (item == nullptr || m_cellPixels == 0 || m_pov != PovTop)
        return;

    QList<SceneryItem *> moved;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (auto *sc = dynamic_cast<SceneryItem *>(gi); sc != nullptr && itemDraggable(sc))
            moved.append(sc);
    if (!moved.contains(item))
        moved.append(item);

    // Snap the dragged one to the grid; the rest move by the same delta.
    if (m_snapDivisions > 0)
    {
        const QPointF delta = snapScenePos(item->pos()) - item->pos();
        if (!delta.isNull())
        {
            foreach (SceneryItem *sc2, moved)
                sc2->setPos(sc2->pos() + delta);
        }
    }

    UndoEntry undo; undo.type = UndoEntry::ItemMove;
    foreach (SceneryItem *sci, moved)
    {
        Scenery *s = sci->scenery();
        if (s == nullptr)
            continue;
        // The item sits at the middle of the sheet's bottom edge.
        const QPointF mm = pixelsToRealPosition(sci->pos().x(), sci->pos().y());
        undo.itemKeys << qMakePair(QStringLiteral("scenery"), s->id());
        undo.itemOrigins << QPointF(double(s->originX()), double(s->originY()));
        s->setOriginX(float(mm.x() / 1000.0));
        s->setOriginY(float(mm.y() / 1000.0));
        if (undo.itemOrigins.last() == QPointF(double(s->originX()), double(s->originY())))
        { undo.itemKeys.removeLast(); undo.itemOrigins.removeLast(); }
    }
    if (!undo.itemKeys.isEmpty())
        pushUndo(undo);
    m_doc->setModified();
    emit mapStructureChanged();
}

void MonitorGraphicsView::slotPipeMoved(PipeItem *item)
{
    if (item == nullptr || m_cellPixels == 0)
        return;

    // Persist every selected pipe (group move) -- see slotTowerMoved()'s
    // comment for why this rebuild from the live selection is necessary.
    QList<PipeItem *> moved;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (auto *pi = dynamic_cast<PipeItem *>(gi); pi != nullptr && itemDraggable(pi))
            moved.append(pi);
    if (!moved.contains(item))
        moved.append(item);

    // Snap the dragged one to the grid; the rest move by the same delta.
    if (m_snapDivisions > 0)
    {
        const QPointF delta = snapScenePos(item->pos()) - item->pos();
        if (!delta.isNull())
        {
            foreach (PipeItem *pi2, moved)
                pi2->setPos(pi2->pos() + delta);
        }
    }

    UndoEntry undo; undo.type = UndoEntry::ItemMove;
    foreach (PipeItem *pi, moved)
    {
        Pipe *b = pi->pipe();
        if (b == nullptr)
            continue;
        const QPointF c = pi->pos();   // base centre in scene pixels
        undo.itemKeys << qMakePair(QStringLiteral("pipe"), b->id());
        undo.itemOrigins << QPointF(double(b->originX()), double(b->originY()));
        b->setOriginX(float(((c.x() - m_xOffset) * m_unitValue) / (1000.0 * m_cellPixels)));
        b->setOriginY(float(((c.y() - m_yOffset) * m_unitValue) / (1000.0 * m_cellPixels)));
        if (undo.itemOrigins.last() == QPointF(double(b->originX()), double(b->originY())))
        { undo.itemKeys.removeLast(); undo.itemOrigins.removeLast(); }
    }
    if (!undo.itemKeys.isEmpty())
        pushUndo(undo);
    refreshFixtureCountBadges();
    m_doc->setModified();
    emit mapStructureChanged();
}

void MonitorGraphicsView::slotPlatformMoved(PlatformItem *item)
{
    // Persist every selected platform (group move), not just the dropped one.
    QList<PlatformItem *> moved;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (auto *pi = dynamic_cast<PlatformItem *>(gi); pi != nullptr && itemDraggable(pi))
            moved.append(pi);
    if (item != nullptr && !moved.contains(item))
        moved.append(item);

    // Snap the dragged platform to the grid, then shift the rest by the same
    // delta so a grouped selection keeps its relative layout intact.
    if (m_snapDivisions > 0 && m_cellPixels > 0 && item != nullptr)
    {
        const QPointF snapped = snapScenePos(item->pos());
        const QPointF delta = snapped - item->pos();
        if (!delta.isNull())
        {
            foreach (PlatformItem *pi, moved)
                pi->setPos(pi->pos() + delta);
        }
    }

    MonitorProperties *mprops = m_doc->monitorProperties();
    UndoEntry e; e.type = UndoEntry::PlatformMove;

    /* The people standing on a riser ride it, as its deck fixtures do --
       found before anything moves, while they are still over it. One that
       was dragged with it moves itself (slotTargetMoved). */
    QHash<quint32, QList<StageTarget *> > riders;
    foreach (PlatformItem *pi, moved)
    {
        if (pi->platform() == nullptr)
            continue;
        foreach (StageTarget *t, PerformerFill::standingOn(mprops, pi->platformId()))
        {
            TargetItem *ti = m_targetItems.value(t->id(), nullptr);
            if (ti != nullptr && ti->isSelected() && itemDraggable(ti))
                continue;
            riders[pi->platformId()] << t;
        }
    }

    foreach (PlatformItem *pi, moved)
    {
        StagePlatform *p = pi->platform();
        if (p == nullptr)
            continue;
        const QPointF oldOrigin(double(p->originX()), double(p->originY()));
        QPointF mm = pixelsToRealPosition(pi->pos().x(), pi->pos().y());
        const QPointF newOrigin(mm.x() / 1000.0, mm.y() / 1000.0);
        if (newOrigin != oldOrigin)
        {
            e.platformOrigins.insert(p->id(), oldOrigin);
            p->setOriginX(float(newOrigin.x()));
            p->setOriginY(float(newOrigin.y()));
            // Deck-mounted fixtures store an ABSOLUTE XY (only their Z tracks the
            // deck), so unlike riser/frame fixtures they don't auto-follow. Carry
            // them along by the platform's delta so they ride the deck.
            const float dxMm = float((newOrigin.x() - oldOrigin.x()) * 1000.0);
            const float dyMm = float((newOrigin.y() - oldOrigin.y()) * 1000.0);
            for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
            {
                if (mprops->fixtureRigProps(it.key()).deckPlatformId != p->id())
                    continue;
                // Dragged along with it (a lasso round a fountain): it moved itself.
                if (it.value()->isSelected() && (it.value()->flags() & QGraphicsItem::ItemIsMovable))
                    continue;
                QVector3D fp = mprops->fixturePosition(it.key(), 0, 0);   // mm
                fp.setX(fp.x() + dxMm);
                fp.setY(fp.y() + dyMm);
                mprops->setFixturePosition(it.key(), 0, 0, fp);
            }
            foreach (StageTarget *t, riders.value(p->id()))
            {
                if (!e.targetPositions.contains(t->id()))
                    e.targetPositions.insert(t->id(), QPointF(double(t->x()), double(t->y())));
                t->setX(t->x() + dxMm / 1000.0f);
                t->setY(t->y() + dyMm / 1000.0f);
                t->setZ(mprops->platformHeightAt(t->x(), t->y()));
            }
            emit platformMoved(pi->platformId(),
                               QPointF(double(p->originX()), double(p->originY())));
        }
    }

    if (!e.platformOrigins.isEmpty())
    {
        pushUndo(e);
        m_doc->setModified();
    }
    if (!e.targetPositions.isEmpty())
    {
        repositionTargetItems();
        updateAimLines();
        mprops->notifyStageTargetsChanged();
    }

    // Re-slave platform-anchored studio frames, then let mounted AND frame
    // fixtures follow the platforms to their new position.
    m_doc->monitorProperties()->recomputeAnchoredFrames();
    refreshRiserFixtures();
    /* A deck's count bubble stands for the fixtures on it, which are hidden
       under it: without this the deck and its fixtures moved but the bubble
       -- all you can see of them -- stayed behind, as if they had not. */
    refreshFixtureCountBadges();
}

void MonitorGraphicsView::updatePowerSources()
{
    QScopedValueRollback<bool> rb(m_itemsRebuilding); m_itemsRebuilding = true;
    foreach (PowerSourceItem *ps, m_powerSourceItems)
    {
        m_scene->removeItem(ps);
        delete ps;
    }
    m_powerSourceItems.clear();

    if (m_cellPixels == 0)
        return;

    PowerDistribution *pd = m_doc->powerDistribution();
    for (int i = 0; i < pd->sources().size(); i++)
    {
        const PowerSource &src = pd->sources().at(i);
        // Placed sources use their stored metres; unplaced ones cascade near the
        // origin so they're visible and can be dragged into position.
        const double mx = src.placed ? src.posX : (0.5 + i * 0.4);
        const double my = src.placed ? src.posY : 0.5;
        // Power sources sit on the floor (Z=0); projectMm handles Top vs elevation.
        const QPointF px = projectMm(mx * 1000.0, my * 1000.0, 0.0);

        PowerSourceItem *ps = new PowerSourceItem(i, src.name, src.locked);
        ps->setPos(px);
        // Draggable only when neither the whole layout nor this source is locked.
        ps->setMovable(!m_layoutLocked && !src.locked);
        m_scene->addItem(ps);
        m_powerSourceItems.append(ps);

        connect(ps, &PowerSourceItem::itemDropped,
                this, &MonitorGraphicsView::slotPowerSourceMoved);
        connect(ps, &PowerSourceItem::lockToggleRequested,
                this, &MonitorGraphicsView::slotPowerSourceLockToggled);
    }

    refreshItemLayerState();
}

void MonitorGraphicsView::slotPowerSourceLockToggled(int sourceIndex)
{
    PowerDistribution *pd = m_doc->powerDistribution();
    if (sourceIndex < 0 || sourceIndex >= pd->sources().size())
        return;
    // The source's own lock.
    setItemLocked(QStringLiteral("power"), quint32(sourceIndex),
                  !itemOwnLocked(QStringLiteral("power"), quint32(sourceIndex)));
}

void MonitorGraphicsView::setPowerSourceColors(const QHash<QString, QColor> &circuitColors)
{
    PowerDistribution *pd = m_doc->powerDistribution();
    foreach (PowerSourceItem *ps, m_powerSourceItems)
    {
        const int s = ps->sourceIndex();
        QList<QColor> cols;
        if (!circuitColors.isEmpty() && s >= 0 && s < pd->sources().size())
            for (int c = 0; c < pd->sources().at(s).circuits.size(); c++)
                cols << circuitColors.value(QString("%1:%2").arg(s).arg(c),
                                            QColor(120, 120, 120));
        ps->setCircuitColors(cols);
    }
}

void MonitorGraphicsView::slotPowerSourceMoved(PowerSourceItem *item)
{
    PowerDistribution *pd = m_doc->powerDistribution();

    // Persist every selected source (group move), not just the dropped one.
    QList<PowerSourceItem *> moved;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (auto *ps = dynamic_cast<PowerSourceItem *>(gi); ps != nullptr && itemDraggable(ps))
            moved.append(ps);
    if (item != nullptr && !moved.contains(item))
        moved.append(item);

    // Snap the dragged source, shift the rest by the same delta.
    if (m_snapDivisions > 0 && m_cellPixels > 0 && item != nullptr)
    {
        const QPointF snapped = snapScenePos(item->pos());
        const QPointF delta = snapped - item->pos();
        if (!delta.isNull())
        {
            foreach (PowerSourceItem *ps, moved)
                ps->setPos(ps->pos() + delta);
        }
    }

    bool any = false;
    UndoEntry undo; undo.type = UndoEntry::ItemMove;
    foreach (PowerSourceItem *ps, moved)
    {
        const int idx = ps->sourceIndex();
        if (idx < 0 || idx >= pd->sources().size())
            continue;
        const QPointF mm = pixelsToRealPosition(ps->pos().x(), ps->pos().y());
        undo.itemKeys << qMakePair(QStringLiteral("power"), quint32(idx));
        undo.itemOrigins << QPointF(pd->sources()[idx].posX, pd->sources()[idx].posY);
        pd->sources()[idx].placed = true;
        pd->sources()[idx].posX = mm.x() / 1000.0;
        pd->sources()[idx].posY = mm.y() / 1000.0;
        if (undo.itemOrigins.last() == QPointF(pd->sources()[idx].posX, pd->sources()[idx].posY))
        { undo.itemKeys.removeLast(); undo.itemOrigins.removeLast(); }
        any = true;
    }
    if (!undo.itemKeys.isEmpty())
        pushUndo(undo);
    if (any)
        m_doc->setModified();
}

void MonitorGraphicsView::updateNetworkNodes()
{
    QScopedValueRollback<bool> rb(m_itemsRebuilding); m_itemsRebuilding = true;
    foreach (NetworkNodeItem *nn, m_networkNodeItems)
    {
        m_scene->removeItem(nn);
        delete nn;
    }
    m_networkNodeItems.clear();
    foreach (QGraphicsLineItem *ln, m_networkLinks)
    {
        m_scene->removeItem(ln);
        delete ln;
    }
    m_networkLinks.clear();
    foreach (NetworkReceiverItem *rx, m_networkReceiverItems)
    {
        m_scene->removeItem(rx);
        delete rx;
    }
    m_networkReceiverItems.clear();

    if (m_cellPixels == 0)
        return;

    NetworkTopology *nt = m_doc->networkTopology();
    for (int i = 0; i < nt->nodes().size(); i++)
    {
        const NetworkNode &node = nt->nodes().at(i);
        // Placed nodes use their stored metres; unplaced ones cascade near the
        // origin so they're visible and can be dragged into position.
        const double mx = node.placed ? node.posX : (0.5 + i * 0.4);
        const double my = node.placed ? node.posY : 0.8;   // offset from power's row
        // Network nodes sit on the floor (Z=0); projectMm handles Top vs elevation.
        const QPointF px = projectMm(mx * 1000.0, my * 1000.0, 0.0);

        NetworkNodeItem *nn = new NetworkNodeItem(i, node.name, node.transport, node.locked);
        if (!node.model.isEmpty())
            nn->setToolTip(tr("Network node: %1 (%2)").arg(node.name, node.model));
        nn->setPos(px);
        // Draggable only when neither the whole layout nor this node is locked.
        nn->setMovable(!m_layoutLocked && !node.locked);
        m_scene->addItem(nn);
        m_networkNodeItems.append(nn);

        connect(nn, &NetworkNodeItem::itemDropped,
                this, &MonitorGraphicsView::slotNetworkNodeMoved);
        connect(nn, &NetworkNodeItem::lockToggleRequested,
                this, &MonitorGraphicsView::slotNetworkNodeLockToggled);

        // A wireless port's receiver is drawn where the receiver IS, with a
        // dashed link back to the transmitter -- the transmitter's own position
        // says nothing about where the universe lands.
        if (node.isWireless())
        {
            for (int p = 0; p < node.ports.size(); p++)
            {
                const NetworkReceiver &rcv = node.ports.at(p).receiver;
                if (!rcv.placed)
                    continue;
                NetworkReceiverItem *rx = new NetworkReceiverItem(i, p, rcv.name);
                rx->setPos(projectMm(rcv.posX * 1000.0, rcv.posY * 1000.0, 0.0));
                /* Drag it to where the box is rigged -- plan view only, where
                   a screen point IS a stage X/Y, and not while the layout is
                   locked. Its dashed link follows; the drop stores it. */
                const bool movable = !m_layoutLocked && m_pov == PovTop;
                rx->setFlag(QGraphicsItem::ItemIsMovable, movable);
                rx->setFlag(QGraphicsItem::ItemIsSelectable, movable);
                rx->onMoving = [this]() { updateNetworkLinks(); };
                rx->onDropped = [this, rx]() {
                    NetworkTopology *nt = m_doc->networkTopology();
                    const int ni = rx->nodeIndex(), pi = rx->portIndex();
                    if (ni >= nt->nodes().size() || pi >= nt->nodes().at(ni).ports.size())
                        return;
                    const QPointF mm = pixelsToRealPosition(rx->pos().x(), rx->pos().y());
                    NetworkReceiver &r = nt->nodes()[ni].ports[pi].receiver;
                    if (qFuzzyCompare(r.posX + 1e3, mm.x() / 1000.0 + 1e3)
                        && qFuzzyCompare(r.posY + 1e3, mm.y() / 1000.0 + 1e3))
                        return;
                    r.posX = mm.x() / 1000.0;
                    r.posY = mm.y() / 1000.0;
                    r.placed = true;
                    m_doc->setModified();
                    scheduleNetworkRuns();
                };
                m_scene->addItem(rx);
                m_networkReceiverItems.append(rx);

                QGraphicsLineItem *ln = new QGraphicsLineItem;
                QPen pen(QColor(235, 140, 40), 1.5, Qt::DashLine);
                pen.setCosmetic(true);
                ln->setPen(pen);
                ln->setZValue(0.4);
                ln->setAcceptedMouseButtons(Qt::NoButton);
                m_scene->addItem(ln);
                m_networkLinks.append(ln);
            }
        }
    }

    updateNetworkLinks();
    refreshItemLayerState();
}

void MonitorGraphicsView::updateNetworkLinks()
{
    // updateNetworkNodes() builds receivers and links in step, each receiver
    // right after its own transmitter, so the indices always line up.
    for (int i = 0; i < m_networkReceiverItems.size(); i++)
    {
        NetworkReceiverItem *rx = m_networkReceiverItems.at(i);
        NetworkNodeItem *tx = m_networkNodeItems.at(rx->nodeIndex());
        m_networkLinks.at(i)->setLine(QLineF(tx->centre(), rx->pos()));
    }
    // A node that moved changes where its runs start.
    scheduleNetworkRuns();
}

void MonitorGraphicsView::setNetworkRunsVisible(bool on)
{
    if (m_showNetworkRuns == on)
        return;
    m_showNetworkRuns = on;
    updateNetworkRuns();
}

void MonitorGraphicsView::scheduleNetworkRuns()
{
    if (!m_showNetworkRuns)
        return;
    if (m_runsTimer == nullptr)
    {
        m_runsTimer = new QTimer(this);
        m_runsTimer->setSingleShot(true);
        m_runsTimer->setInterval(40);
        connect(m_runsTimer, &QTimer::timeout, this, &MonitorGraphicsView::updateNetworkRuns);
    }
    m_runsTimer->start();
}

void MonitorGraphicsView::updateNetworkRuns()
{
    foreach (NetworkRunItem *ri, m_networkRunItems)
    {
        m_scene->removeItem(ri);
        delete ri;
    }
    m_networkRunItems.clear();

    if (!m_showNetworkRuns || m_cellPixels == 0)
        return;

    // Where each plotted fixture is, in metres, for the engine's length estimate.
    MonitorProperties *props = m_doc->monitorProperties();
    QHash<quint32, QVector3D> positions;
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        positions.insert(it.key(), props->fixtureRigPosition(it.key()));

    const QList<NetworkRun> runs = m_doc->networkTopology()->runs(m_doc, positions);
    foreach (const NetworkRun &run, runs)
    {
        // Unanchored: nowhere to run from. Past the items: the model changed
        // and updateNetworkNodes() has not caught up yet.
        if (!run.anchored || run.nodeIndex >= m_networkNodeItems.size())
            continue;

        // Where the run starts on screen: the receiver for a wireless port
        // (an anchored wireless run always has a placed one), else the node.
        QPointF start = m_networkNodeItems.at(run.nodeIndex)->centre();
        if (run.wireless)
        {
            foreach (NetworkReceiverItem *rx, m_networkReceiverItems)
                if (rx->nodeIndex() == run.nodeIndex && rx->portIndex() == run.portIndex)
                {
                    start = rx->pos();
                    break;
                }
        }

        QPainterPath path(start);
        int vertices = 0;
        foreach (quint32 fid, run.plotted)
        {
            MonitorFixtureItem *fi = m_fixtures.value(fid, nullptr);
            if (fi == nullptr || !fi->isVisible())
                continue;
            path.lineTo(fi->iconCentreScene());
            vertices++;
        }
        if (vertices == 0)
            continue;

        QString from = run.nodeName;
        if (run.wireless && !run.receiverName.isEmpty())
            from = tr("%1 via %2").arg(run.nodeName, run.receiverName);
        QString stats = tr("%1 · Port %2 · Universe %3 · %4 fixture(s) · ~%5 · %6/512 ch")
                            .arg(from).arg(run.portIndex + 1).arg(run.universe + 1)
                            .arg(run.fixtures.size())
                            .arg(MonitorProperties::formatStageDistance(float(run.lengthMetres)))
                            .arg(run.channels);
        const int unplotted = run.fixtures.size() - run.plotted.size();
        if (unplotted > 0)
            stats += tr(" · %1 not on the plot").arg(unplotted);

        const int hue = (run.nodeIndex * 67 + run.portIndex * 23) % 360;
        NetworkRunItem *ri = new NetworkRunItem(path, QColor::fromHsv(hue, 200, 235), stats,
                                                run.nodeIndex, run.portIndex);
        m_scene->addItem(ri);
        m_networkRunItems.append(ri);
    }
}

void MonitorGraphicsView::slotNetworkNodeLockToggled(int nodeIndex)
{
    NetworkTopology *nt = m_doc->networkTopology();
    if (nodeIndex < 0 || nodeIndex >= nt->nodes().size())
        return;
    // The node's own lock.
    setItemLocked(QStringLiteral("network"), quint32(nodeIndex),
                  !itemOwnLocked(QStringLiteral("network"), quint32(nodeIndex)));
}

void MonitorGraphicsView::slotNetworkNodeMoved(NetworkNodeItem *item)
{
    NetworkTopology *nt = m_doc->networkTopology();

    // Persist every selected node (group move), not just the dropped one.
    QList<NetworkNodeItem *> moved;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (auto *nn = dynamic_cast<NetworkNodeItem *>(gi); nn != nullptr && itemDraggable(nn))
            moved.append(nn);
    if (item != nullptr && !moved.contains(item))
        moved.append(item);

    // Snap the dragged node, shift the rest by the same delta.
    if (m_snapDivisions > 0 && m_cellPixels > 0 && item != nullptr)
    {
        const QPointF snapped = snapScenePos(item->pos());
        const QPointF delta = snapped - item->pos();
        if (!delta.isNull())
        {
            foreach (NetworkNodeItem *nn, moved)
                nn->setPos(nn->pos() + delta);
        }
    }

    bool any = false;
    UndoEntry undo; undo.type = UndoEntry::ItemMove;
    foreach (NetworkNodeItem *nn, moved)
    {
        const int idx = nn->nodeIndex();
        if (idx < 0 || idx >= nt->nodes().size())
            continue;
        const QPointF mm = pixelsToRealPosition(nn->pos().x(), nn->pos().y());
        undo.itemKeys << qMakePair(QStringLiteral("network"), quint32(idx));
        undo.itemOrigins << QPointF(nt->nodes()[idx].posX, nt->nodes()[idx].posY);
        nt->nodes()[idx].placed = true;
        nt->nodes()[idx].posX = mm.x() / 1000.0;
        nt->nodes()[idx].posY = mm.y() / 1000.0;
        if (undo.itemOrigins.last() == QPointF(nt->nodes()[idx].posX, nt->nodes()[idx].posY))
        { undo.itemKeys.removeLast(); undo.itemOrigins.removeLast(); }
        any = true;
    }
    if (!undo.itemKeys.isEmpty())
        pushUndo(undo);
    // Only the links follow: a rebuild here would delete the item whose mouse
    // release is still being handled.
    updateNetworkLinks();
    if (any)
        m_doc->setModified();
}

void MonitorGraphicsView::repositionTargetItems()
{
    if (m_cellPixels == 0)
        return;
    foreach (TargetItem *ti, m_targetItems)
    {
        StageTarget *t = ti->target();
        if (t == nullptr)
            continue;
        const QVector3D p = m_doc->monitorProperties()->targetPosition(t);   // in the blocking shown
        // realPositionToPixels expects millimetres (StageTarget is in metres).
        const QPointF px = realPositionToPixels(p.x() * 1000.0, p.y() * 1000.0);
        ti->setScenePos(float(px.x()), float(px.y()));
        if (t->isLine())
            refreshLineItem(ti);
    }
    updateAimLines();   // keep aim lines tracking the moved target
    updateBlockingGhosts();
}

void MonitorGraphicsView::dropTargetItems()
{
    QScopedValueRollback<bool> rb(m_itemsRebuilding); m_itemsRebuilding = true;
    foreach (TargetItem *ti, m_targetItems)
    {
        m_scene->removeItem(ti);
        delete ti;
    }
    m_targetItems.clear();
    foreach (QGraphicsItem *g, m_blockingGhosts)
    {
        m_scene->removeItem(g);
        delete g;
    }
    m_blockingGhosts.clear();

    // Aim lines are tied to target selection; they go with the targets.
    foreach (QGraphicsLineItem *li, m_aimLines)
    {
        m_scene->removeItem(li);
        delete li;
    }
    m_aimLines.clear();
}

void MonitorGraphicsView::updateTargets()
{
    QScopedValueRollback<bool> rb(m_itemsRebuilding); m_itemsRebuilding = true;
    dropTargetItems();

    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr || m_cellPixels == 0)
        return;

    /* Which targets the focused scene aims at. This used to decide whether a
       target was DRAWN AT ALL, which made a target invisible from the moment
       it was created: you place one in the studio, no scene aims at it yet, so
       it does not exist on the canvas. A target is a piece of stage geometry
       -- it is placed, moved and named like a truss -- so it is always drawn.
       Being aimed is a temporary property of the scene on screen, and it earns
       a glow and light-path lines instead. */
    const bool firstBuild = !m_targetsSeen;
    m_targetsSeen = true;
    QSet<quint32> aimedTargetIds;
    if (m_activeSceneId != Function::invalidId())
    {
        Scene *activeScene = qobject_cast<Scene *>(m_doc->function(m_activeSceneId));
        if (activeScene)
        {
            foreach (quint32 pid, activeScene->runtimePalettes())
            {
                QLCPalette *pal = m_doc->palette(pid);
                if (pal && pal->type() == QLCPalette::Aim)
                    aimedTargetIds.insert(pal->stageTargetId());
            }
        }
    }

    foreach (StageTarget *t, props->stageTargets())
    {
        // projectMm places the target at its 3-D position for Top or elevation.
        // A line sits on its centre as the aim rule resolves it.
        // Where it stands in the blocking the Studio shows (BLOCKING.md).
        const QVector3D at = t->isLine() ? props->targetAimPoint(t) : props->targetPosition(t);
        const QPointF tp = projectMm(at.x() * 1000.0, at.y() * 1000.0, at.z() * 1000.0);
        float pxX = float(tp.x());
        float pxY = float(tp.y());

        TargetItem *ti = new TargetItem(t, m_doc, pxX, pxY);
        bool bandSideOn = (m_pov == PovSide);
        /* Where it is in crew terms, against the stage marks: read from where
           the item is while it is dragged in the Top view, from the target
           itself in an elevation (where the plot shows no Y). Unless Show >
           Stage marks > Target positions is off. */
        if (m_doc->monitorProperties() != nullptr && m_doc->monitorProperties()->targetPositionsVisible()
            && !PerformerFill::isPerformer(props, t))
        {
            ti->setPositionReadout([this, at](const QPointF &scenePos) {
                MonitorProperties *mp = m_doc->monitorProperties();
                if (mp == nullptr)
                    return QString();
                QVector3D w = at;
                if (m_pov == PovTop)
                {
                    const QPointF mm = pixelsToRealPosition(scenePos.x(), scenePos.y());
                    w = QVector3D(float(mm.x() / 1000.0), float(mm.y() / 1000.0), 0.0f);
                }
                w.setZ(mp->platformHeightAt(w.x(), w.y()));      // what it stands on
                return mp->stagePositionText(w);
            });
        }
        /* A Person is drawn standing where the aim rule stands it: on the deck
           under it, with its chest at the aim height. Taking both from
           targetAimPoint() keeps the figure and the beams on one answer. */
        /* An Area or a drum kit at its real size: width across the stage,
           depth up and down it. From the front only the width shows, from the
           side only the depth. */
        if (StageTarget::hasFootprintKind(t->kind())
            && props->targetFootprint(t).isEmpty() == false)
        {
            const double ppm = 1000.0 * m_cellPixels / m_unitValue;
            const QSizeF fp = props->targetFootprint(t);
            /* Movable gear turned on the deck: the glyph turns with it, seen
               through the plot's own turn (the house is at +y, drawn down). */
            double wide = (m_pov == PovSide) ? fp.height() : fp.width();
            bool sideOn = (m_pov == PovSide);
            if (StageTarget::isBandKind(t->kind()))
            {
                const double th = qDegreesToRadians(double(t->rotation()));
                const double c = std::abs(std::cos(th)), s = std::abs(std::sin(th));
                if (m_pov != PovTop)
                {
                    const double across = fp.width() * c + fp.height() * s;
                    const double along = fp.width() * s + fp.height() * c;
                    wide = m_pov == PovFront ? across : along;
                    sideOn = (m_pov == PovFront) ? (s > 0.7) : (s <= 0.7);
                }
                else
                {
                    const QVector3D here = props->targetPosition(t);   // in the blocking shown
                    const QPointF a = projectMm(here.x() * 1000.0, here.y() * 1000.0, 0.0);
                    const double dx = -std::sin(th), dy = std::cos(th);
                    const QPointF b = projectMm((here.x() + dx) * 1000.0, (here.y() + dy) * 1000.0, 0.0);
                    ti->setBandAngle(qRadiansToDegrees(std::atan2(b.y() - a.y(), b.x() - a.x())) - 90.0);
                }
            }
            if (m_pov == PovTop)
                ti->setFootprintPx(QSizeF(fp.width() * ppm, fp.height() * ppm));
            else if (m_pov == PovFront)
                ti->setFootprintPx(QSizeF(wide * ppm, 0.0));
            else
                ti->setFootprintPx(QSizeF(wide * ppm, 0.0));
            if (StageTarget::isBandKind(t->kind()) && m_pov != PovTop)
                bandSideOn = sideOn;
        }
        /* A band instrument stands on the deck under it, as tall as it is; the
           item draws its plan from above, its silhouette in an elevation. */
        if (StageTarget::isBandKind(t->kind()) && m_pov != PovTop)
        {
            const QVector3D aimAt = props->targetAimPoint(t, true);
            const double footMm = double(aimAt.z() - props->targetAimHeight(t, true)) * 1000.0;
            const double tallMm = double(StageTarget::bodyHeight(t->kind())) * 1000.0;
            const QPointF feet  = projectMm(aimAt.x() * 1000.0, aimAt.y() * 1000.0, footMm) - tp;
            const QPointF crown = projectMm(aimAt.x() * 1000.0, aimAt.y() * 1000.0,
                                            footMm + tallMm) - tp;
            ti->setFigure(feet, crown, 1000.0 * m_cellPixels / m_unitValue,
                          bandSideOn);
        }
        if (props->targetFigureHeight(t) > 0.0f)
        {
            const QVector3D aimAt = props->targetAimPoint(t, true);
            const double footMm = double(aimAt.z() - props->targetAimHeight(t, true)) * 1000.0;
            const double tallMm = double(props->targetFigureHeight(t)) * 1000.0;
            const QPointF feet  = projectMm(aimAt.x() * 1000.0, aimAt.y() * 1000.0, footMm) - tp;
            const QPointF crown = projectMm(aimAt.x() * 1000.0, aimAt.y() * 1000.0,
                                            footMm + tallMm) - tp;
            ti->setFigure(feet, crown, 1000.0 * m_cellPixels / m_unitValue,
                          m_pov == PovSide);
        }
        // Targets are a DESIGN-time aim point: draggable while editing (even when
        // the rig layout is locked), but frozen in Run/Operate — in a show the
        // operator drives the follow-spot pin, not the target.
        ti->setMovable(m_doc->mode() == Doc::Design && targetsFreeOfPlotLock());
        ti->setAimed(aimedTargetIds.contains(t->id()));
        m_scene->addItem(ti);
        m_targetItems.insert(t->id(), ti);
        // One placed since the show opened stays drawn, whatever the scope.
        if (!firstBuild && !m_knownTargets.contains(t->id()))
            m_placedTargets.insert(t->id());
        m_knownTargets.insert(t->id());
        {
            const MonitorProperties::MonitorLayer lyr = props->layer(t->layerId());
            ti->setVisible(lyr.visible && !m_stageOnly && targetInScope(t->id()));
        }

        connect(ti, &TargetItem::itemDropped,
                this, &MonitorGraphicsView::slotTargetMoved);
        if (t->isLine())
        {
            refreshLineItem(ti);
            connect(ti, &TargetItem::lineHandleMoved,
                    this, &MonitorGraphicsView::slotLineHandleMoved);
            connect(ti, &TargetItem::lineHandleReleased,
                    this, &MonitorGraphicsView::slotLineHandleReleased);
            connect(ti, &TargetItem::lineInsertRequested,
                    this, &MonitorGraphicsView::slotLineInsertPoint);
            connect(ti, &TargetItem::lineRemoveRequested,
                    this, &MonitorGraphicsView::slotLineRemovePoint);
        }
    }

    refreshItemLayerState();
    updateAimLines();
    updateBlockingGhosts();
    stackTargetReadouts();
}

void MonitorGraphicsView::blockingChanged()
{
    updateTargets();
}

void MonitorGraphicsView::updateBlockingGhosts()
{
    foreach (QGraphicsItem *g, m_blockingGhosts)
    {
        m_scene->removeItem(g);
        delete g;
    }
    m_blockingGhosts.clear();
    MonitorProperties *props = m_doc->monitorProperties();
    BlockingView *bv = BlockingView::of(m_doc);
    if (props == nullptr || bv == nullptr || !bv->editsSet() || m_cellPixels == 0)
        return;
    const quint32 set = bv->blocking();
    /* Where Default stands each target moved in this blocking: a dashed
       ring and a line to where it is now, so the move reads at a glance. */
    QPen pen(QColor(170, 200, 255, 200), 1.5, Qt::DashLine);
    pen.setCosmetic(true);
    foreach (TargetItem *ti, m_targetItems)
    {
        StageTarget *t = ti != nullptr ? ti->target() : nullptr;
        if (t == nullptr || !ti->isVisible() || !t->isBlockedIn(set))
            continue;
        const QVector3D def = props->targetPosition(t, MonitorProperties::defaultBlocking());
        const QVector3D now = props->targetPosition(t, set);
        if ((def - now).length() < 0.005f)
            continue;
        const QPointF a = projectMm(def.x() * 1000.0, def.y() * 1000.0, def.z() * 1000.0);
        const QPointF b = projectMm(now.x() * 1000.0, now.y() * 1000.0, now.z() * 1000.0);
        const double r = qMax(6.0, 0.25 * 1000.0 * m_cellPixels / m_unitValue);
        QGraphicsEllipseItem *ring = m_scene->addEllipse(QRectF(a.x() - r, a.y() - r, 2 * r, 2 * r), pen);
        ring->setZValue(ti->zValue() - 1);
        ring->setToolTip(tr("%1 in Default").arg(t->name()));
        QGraphicsLineItem *link = m_scene->addLine(QLineF(a, b), pen);
        link->setZValue(ti->zValue() - 1);
        /* Written as the targets' own names are: in the plot's scale and
           turned back with the view (refreshLabelRotations()). It ignored the
           view's transform, so it came out in the application's font -- three
           times a target's name when zoomed out -- and the counter-turn meant
           for scene text then laid it on its side. */
        QGraphicsSimpleTextItem *label = m_scene->addSimpleText(tr("%1 (Default)").arg(t->name()),
                                                                TargetItem::labelFont());
        label->setBrush(QColor(170, 200, 255, 220));
        label->setPos(a.x() + r, a.y() + r);
        label->setData(0, QStringLiteral("ghostLabel"));
        label->setZValue(ti->zValue() - 1);
        m_blockingGhosts << ring << link << label;
    }
}

void MonitorGraphicsView::moveTargetsInBlocking(const QList<TargetItem *> &moved, quint32 set)
{
    MonitorProperties *props = m_doc->monitorProperties();
    /* Where every dropped item says its target now is, read before anything
       is written: what a write sets off (targetMoved, a re-aim) may rebuild
       the target items, and the rest of `moved` would be freed under us. */
    QList<QPair<quint32, QVector3D>> drops;
    foreach (TargetItem *ti, moved)
    {
        StageTarget *t = ti != nullptr ? ti->target() : nullptr;
        if (t == nullptr)
            continue;
        const QVector3D cur = props->targetPosition(t, set);
        const QVector3D newMm = unprojectMm(ti->pos(), cur * 1000.0f);
        QVector3D now = newMm / 1000.0f;
        // From above, it keeps its height over whatever it now stands on.
        if (m_pov == PovTop && t->isLine())
            now.setZ(cur.z());
        else if (m_pov == PovTop)
            now.setZ(floorHeightAt(now.x(), now.y())
                     + qMax(0.0f, cur.z() - floorHeightAt(cur.x(), cur.y())));
        drops.append(qMakePair(t->id(), now));
    }

    UndoEntry e;
    e.type = UndoEntry::TargetBlock;
    e.blockingSet = set;
    for (const QPair<quint32, QVector3D> &d : std::as_const(drops))
    {
        StageTarget *t = props->stageTarget(d.first);
        if (t == nullptr)
            continue;
        const QVector3D cur = props->targetPosition(t, set);
        const QVector3D now = d.second;
        if ((now - cur).length() < 1e-4f)
            continue;
        StageTarget::Blocked b = t->blocked(set);
        e.targetBlocked.insert(t->id(), b);
        b.offset = (b.hasOffset ? b.offset : QVector3D()) + (now - cur);
        b.hasOffset = true;
        t->setBlocked(set, b);
        reaimScenesAimingAt(t->id());
        emit targetMoved(t->id(), QPointF(double(now.x()), double(now.y())));
    }
    if (!e.targetBlocked.isEmpty())
    {
        pushUndo(e);
        m_doc->setModified();
        props->notifyStageTargetsChanged();
    }
    repositionTargetItems();
    stackTargetReadouts();
}

void MonitorGraphicsView::editBlockedLine(StageTarget *t, quint32 set,
                                          const std::function<QVector<QVector3D>(QVector<QVector3D>)> &edit)
{
    const StageTarget::Blocked before = t->blocked(set);
    const QVector<QVector3D> pts = edit(t->linePointsIn(set));
    if (pts.size() < 2 || pts == t->linePointsIn(set))
        return;
    StageTarget::Blocked b = before;
    b.hasLine = true;
    b.linePoints = pts;
    t->setBlocked(set, b);
    UndoEntry e;
    e.type = UndoEntry::TargetBlock;
    e.blockingSet = set;
    e.targetBlocked.insert(t->id(), before);
    pushUndo(e);
    m_doc->setModified();
    m_doc->monitorProperties()->notifyStageTargetsChanged();
    reaimScenesAimingAt(t->id());
    if (TargetItem *ti = m_targetItems.value(t->id(), nullptr))
        refreshLineItem(ti);
    updateAimLines();
}

void MonitorGraphicsView::stackTargetReadouts()
{
    /* Two targets an inch apart printed their readouts on the same spot, so
       one read as the other's (Q179). Each readout that would cover one
       already placed goes under it instead. On screen: the labels keep their
       size at any zoom, so this runs again when the view changes. */
    if (m_scene == nullptr)
        return;
    struct Placed { TargetItem *ti; QRectF rect; QPointF anchor; };
    QList<Placed> todo;
    foreach (TargetItem *ti, m_targetItems)
    {
        if (ti == nullptr)
            continue;
        ti->setReadoutStackOffset(0.0);
        if (!ti->isVisible() || !ti->readoutShown())
            continue;
        const QPointF anchor = sceneToViewport(ti->readoutAnchorScene());
        todo << Placed{ ti, ti->readoutScreenRect().translated(anchor), anchor };
    }
    std::sort(todo.begin(), todo.end(), [](const Placed &a, const Placed &b) {
        if (!qFuzzyCompare(a.anchor.y() + 1.0, b.anchor.y() + 1.0))
            return a.anchor.y() < b.anchor.y();
        if (!qFuzzyCompare(a.anchor.x() + 1.0, b.anchor.x() + 1.0))
            return a.anchor.x() < b.anchor.x();
        return a.ti->targetId() < b.ti->targetId();
    });
    QList<QRectF> placed;
    for (const Placed &p : todo)
    {
        QRectF r = p.rect;
        for (bool moved = true; moved; )
        {
            moved = false;
            for (const QRectF &other : placed)
                if (r.intersects(other))
                {
                    r.moveTop(other.bottom() + 1.0);
                    moved = true;
                }
        }
        p.ti->setReadoutStackOffset(r.top() - p.rect.top());
        placed << r;
    }
}

void MonitorGraphicsView::slotTargetMoved(TargetItem *item)
{
    // Persist every selected target (group move), not just the dropped one.
    QList<TargetItem *> moved;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (auto *tg = dynamic_cast<TargetItem *>(gi); tg != nullptr && itemDraggable(tg))
            moved.append(tg);
    if (item != nullptr && !moved.contains(item))
        moved.append(item);

    /* Snap the dropped target, shift the rest by the same -- only when it
       was dragged: a line's handle drag ends here too (the release sweep),
       and snapping its new centre would shift the whole line. */
    auto drawnAt = [this](TargetItem *ti) {
        MonitorProperties *mp = m_doc->monitorProperties();
        StageTarget *t = ti->target();
        const QVector3D at = (t != nullptr && t->isLine()) ? mp->targetAimPoint(t)
                                                          : (t != nullptr ? mp->targetPosition(t) : QVector3D());
        return projectMm(at.x() * 1000.0, at.y() * 1000.0, at.z() * 1000.0);
    };
    if (m_snapDivisions > 0 && m_cellPixels > 0 && item != nullptr && item->target() != nullptr
        && QLineF(drawnAt(item), item->pos()).length() > 0.5)
    {
        const QPointF delta = snapScenePos(item->pos()) - item->pos();
        if (!delta.isNull())
        {
            foreach (TargetItem *ti, moved)
                ti->setPos(ti->pos() + delta);
        }
    }

    /* The Studio shows a blocking set: the drag moves the targets in it,
       never Default (BLOCKING.md; the banner over the plot says which). */
    if (BlockingView *bv = BlockingView::of(m_doc); bv != nullptr && bv->editsSet())
    {
        moveTargetsInBlocking(moved, bv->blocking());
        return;
    }

    /* Where each dropped item puts its target, read from all of them before
       anything is written: a write's signals (targetMoved, a re-aim, the
       Doc's modified) may rebuild the target items, and every TargetItem in
       `moved` would then be freed under us. From here on, ids only. */
    struct Drop { quint32 id; QPointF pos; float z; };
    QList<Drop> drops;
    foreach (TargetItem *ti, moved)
    {
        StageTarget *t = ti != nullptr ? ti->target() : nullptr;
        if (t == nullptr)
            continue;
        const QPointF oldPos(double(t->x()), double(t->y()));
        const float oldZ = t->z();

        /* Invert the SAME projection the marker was drawn with. Reading the
           drop as an X/Y move regardless of view meant that in Front or Side
           the redraw put the marker somewhere else entirely -- indistinguishable
           from "it snapped back" -- and silently rewrote the wrong axis while
           it was at it. */
        const QVector3D curMm(t->x() * 1000.0f, t->y() * 1000.0f, t->z() * 1000.0f);
        const QVector3D newMm = unprojectMm(ti->pos(), curMm);
        const QPointF newPos(double(newMm.x()) / 1000.0, double(newMm.y()) / 1000.0);
        float newZ = float(newMm.z()) / 1000.0f;

        /* Dropped in TOP view, height is not something the drag could express,
           so it is inherited from what the target now stands on: the top of any
           solid platform under it, or the deck. A target left at z=0 over a
           riser sits inside the riser, which is both wrong to look at and wrong
           to aim at -- the lights would point at the floor under someone's
           feet. Elevations set height directly, so they are left alone. */
        /* A line's centre keeps its height: its points were drawn at their
           heights, and dropping the centre onto a riser would lift the lot. */
        if (m_pov == PovTop && t->isLine())
        {
            newZ = oldZ;
        }
        else if (m_pov == PovTop)
        {
            const float deck = floorHeightAt(float(newPos.x()), float(newPos.y()));
            const float oldDeck = floorHeightAt(float(oldPos.x()), float(oldPos.y()));
            // Preserve any height the operator set ABOVE the old surface.
            newZ = deck + qMax(0.0f, oldZ - oldDeck);
        }

        // Within a tenth of a millimetre is where it was: a projection there
        // and back is not a move.
        const bool movedXY = qAbs(newPos.x() - oldPos.x()) > 1e-4 || qAbs(newPos.y() - oldPos.y()) > 1e-4;
        if (movedXY || qAbs(newZ - oldZ) > 1e-4f)
            drops.append(Drop{ t->id(), newPos, newZ });
    }
    QList<quint32> movedIds;
    foreach (TargetItem *ti, moved)
        if (ti != nullptr)
            movedIds.append(ti->targetId());

    UndoEntry e; e.type = UndoEntry::TargetMove;
    for (const Drop &d : std::as_const(drops))
    {
        StageTarget *t = m_doc->monitorProperties()->stageTarget(d.id);
        if (t == nullptr)
            continue;
        e.targetPositions.insert(t->id(), QPointF(double(t->x()), double(t->y())));
        e.targetZ.insert(t->id(), t->z());
        t->setX(float(d.pos.x()));
        t->setY(float(d.pos.y()));
        t->setZ(d.z);
        emit targetMoved(t->id(), QPointF(double(t->x()), double(t->y())));
    }
    // What the people moved hold (a wand) goes with them.
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        if (m_doc->monitorProperties()->fixtureRigProps(it.key()).isHeld())
            updateFixture(it.key());

    if (!e.targetPositions.isEmpty())
    {
        pushUndo(e);
        m_doc->setModified();
        m_doc->monitorProperties()->notifyStageTargetsChanged();
    }
    updateAimLines();

    // Snap running scenes whose palettes reference any moved target to the new
    // position immediately (override fade to 0 for one write cycle).
    foreach (const quint32 tid, movedIds)
    {
        foreach (Function *fn, m_doc->functions())
        {
            Scene *s = qobject_cast<Scene *>(fn);
            if (!s || !s->isRunning())
                continue;
            foreach (quint32 pid, s->palettes())
            {
                QLCPalette *pal = m_doc->palette(pid);
                if (pal && pal->stageTargetId() == tid)
                {
                    s->setOverrideFadeInSpeed(0);
                    s->resetRuntime();
                    QTimer::singleShot(150, s, [s]() {
                        s->setOverrideFadeInSpeed(Function::defaultSpeed());
                    });
                    break;
                }
            }
        }
    }
    stackTargetReadouts();      // where it landed may cover another's readout
}

void MonitorGraphicsView::refreshLineItem(TargetItem *item)
{
    MonitorProperties *props = m_doc->monitorProperties();
    StageTarget *t = item != nullptr ? item->target() : nullptr;
    if (props == nullptr || t == nullptr || !t->isLine())
        return;
    auto px = [this](const QVector3D &m) {
        return projectMm(double(m.x()) * 1000.0, double(m.y()) * 1000.0, double(m.z()) * 1000.0);
    };
    const QPointF centre = px(props->targetAimPoint(t));
    item->setPos(centre);
    const QVector<QVector3D> world = props->targetLinePoints(t);
    const LineCurve curve(world);
    QPolygonF poly;
    foreach (const QVector3D &s, curve.samples())
        poly << px(s) - centre;
    QVector<QPointF> handles;
    foreach (const QVector3D &w, world)
        handles << px(w) - centre;
    item->setLineGeometry(poly, curve.sampleFractions(), handles);

    /* A performer line: each person on the deck under them, their own height
       tall -- re-laid by the same fractions along the line as it is now, so
       every edit of the line (a handle, a move, undo) carries them with it. */
    if (t->isPerformerLine())
    {
        QVector<QPair<QPointF, QPointF> > crowd;
        foreach (const PerformerFill::Performer &pf, PerformerFill::people(props, t))
        {
            const QVector3D f = pf.feet * 1000.0f;
            crowd << qMakePair(projectMm(f.x(), f.y(), f.z()) - centre,
                               projectMm(f.x(), f.y(), f.z() + pf.height * 1000.0f) - centre);
        }
        item->setPerformers(crowd, 1000.0 * m_cellPixels / m_unitValue, m_pov == PovSide);
    }
}

void MonitorGraphicsView::reaimScenesAimingAt(quint32 targetId)
{
    foreach (Function *fn, m_doc->functions())
    {
        Scene *s = qobject_cast<Scene *>(fn);
        if (s == nullptr || !s->isRunning())
            continue;
        foreach (quint32 pid, s->runtimePalettes())
        {
            QLCPalette *pal = m_doc->palette(pid);
            if (pal != nullptr && pal->type() == QLCPalette::Aim && pal->stageTargetId() == targetId)
            {
                s->requestReaim();
                break;
            }
        }
    }
}

void MonitorGraphicsView::commitLineEdit(StageTarget *t, const QVector3D &posBefore,
                                         const QVector<QVector3D> &pointsBefore)
{
    t->recentreLine();
    UndoEntry e;
    e.type = UndoEntry::TargetLineEdit;
    e.targetLines.insert(t->id(), qMakePair(posBefore, pointsBefore));
    pushUndo(e);
    m_doc->setModified();
    m_doc->monitorProperties()->notifyStageTargetsChanged();
    reaimScenesAimingAt(t->id());
    if (TargetItem *ti = m_targetItems.value(t->id(), nullptr))
        refreshLineItem(ti);
    updateAimLines();
}

void MonitorGraphicsView::slotLineHandleMoved(TargetItem *item, int index, QPointF scenePos,
                                              Qt::KeyboardModifiers modifiers)
{
    MonitorProperties *props = m_doc->monitorProperties();
    StageTarget *t = item != nullptr ? item->target() : nullptr;
    if (props == nullptr || t == nullptr || !t->isLine() || t->locked())
        return;
    BlockingView *bv = BlockingView::of(m_doc);
    const quint32 set = bv != nullptr && bv->editsSet() ? bv->blocking() : 0;
    QVector<QVector3D> rel = set != 0 ? t->linePointsIn(set) : t->linePoints();
    if (index < 0 || index >= rel.size())
        return;
    const QVector3D origin = props->targetAimPoint(t);

    if (m_lineDrag.targetId != t->id() || m_lineDrag.index != index)
    {
        m_lineDrag.targetId = t->id();
        m_lineDrag.index = index;
        m_lineDrag.blocking = set;
        m_lineDrag.blockedBefore = t->blocked(set);
        m_lineDrag.posBefore = t->position();
        m_lineDrag.pointsBefore = rel;
        m_lineDrag.startWorld = origin + rel.at(index);
        m_lineDrag.startScene = item->mapToScene(item->lineHandle(index));
    }
    const QVector3D startW = m_lineDrag.startWorld;

    // Shift: along the view's horizontal or vertical only, whichever is more.
    QPointF sp = scenePos;
    if (modifiers & Qt::ShiftModifier)
    {
        const QPointF d = sp - m_lineDrag.startScene;
        if (qAbs(d.x()) >= qAbs(d.y()))
            sp.setY(m_lineDrag.startScene.y());
        else
            sp.setX(m_lineDrag.startScene.x());
    }
    // On the grid, when snap is on, as any other drag.
    sp = snapScenePos(sp);

    QVector3D w = unprojectMm(sp, startW * 1000.0f) / 1000.0f;
    /* From above, height is not something the drag can say: keep the handle
       as far above whatever is under it as it was -- the rule a point
       target's drop follows. */
    if (m_pov == PovTop)
        w.setZ(floorHeightAt(w.x(), w.y())
               + (startW.z() - floorHeightAt(startW.x(), startW.y())));

    // Alt on an end: extend or retract along the line, keeping its direction.
    const int last = rel.size() - 1;
    if ((modifiers & Qt::AltModifier) && (index == 0 || index == last))
    {
        const QVector3D neighbour = origin + m_lineDrag.pointsBefore.at(index == 0 ? 1 : last - 1);
        const QVector3D dir = (startW - neighbour).normalized();
        if (!dir.isNull())
            w = startW + dir * QVector3D::dotProduct(w - startW, dir);
    }

    rel[index] = w - origin;
    if (set != 0)
    {
        // The line's shape in this blocking (BLOCKING.md); Default keeps its own.
        StageTarget::Blocked b = t->blocked(set);
        b.hasLine = true;
        b.linePoints = rel;
        t->setBlocked(set, b);
    }
    else
        t->setLinePoints(rel);
    refreshLineItem(item);
    reaimScenesAimingAt(t->id());
    updateAimLines();
}

void MonitorGraphicsView::slotLineHandleReleased(TargetItem *item, int index)
{
    Q_UNUSED(index)
    StageTarget *t = item != nullptr ? item->target() : nullptr;
    const LineDrag drag = m_lineDrag;
    m_lineDrag = LineDrag();
    if (t == nullptr || drag.targetId != t->id())
        return;                         // pressed and let go: nothing moved
    if (drag.blocking != 0)
    {
        if (t->linePointsIn(drag.blocking) == drag.pointsBefore)
            return;
        UndoEntry e;
        e.type = UndoEntry::TargetBlock;
        e.blockingSet = drag.blocking;
        e.targetBlocked.insert(t->id(), drag.blockedBefore);
        pushUndo(e);
        m_doc->setModified();
        m_doc->monitorProperties()->notifyStageTargetsChanged();
        return;
    }
    if (t->linePoints() == drag.pointsBefore)
        return;
    commitLineEdit(t, drag.posBefore, drag.pointsBefore);
}

void MonitorGraphicsView::slotLineInsertPoint(TargetItem *item, double fraction)
{
    MonitorProperties *props = m_doc->monitorProperties();
    StageTarget *t = item != nullptr ? item->target() : nullptr;
    if (props == nullptr || t == nullptr || !t->isLine() || t->locked())
        return;
    if (BlockingView *bv = BlockingView::of(m_doc); bv != nullptr && bv->editsSet())
    {
        editBlockedLine(t, bv->blocking(), [fraction](QVector<QVector3D> pts) {
            const LineCurve curve(pts);
            pts.insert(curve.spanAt(fraction) + 1, curve.pointAt(fraction));
            return pts;
        });
        return;
    }
    const QVector3D posBefore = t->position();
    const QVector<QVector3D> before = t->linePoints();
    if (before.size() < 2)
        return;
    // On the curve where the click was, so the shape does not jump.
    const LineCurve curve(before);
    QVector<QVector3D> pts = before;
    pts.insert(curve.spanAt(fraction) + 1, curve.pointAt(fraction));
    t->setLinePoints(pts);
    commitLineEdit(t, posBefore, before);
}

void MonitorGraphicsView::slotLineRemovePoint(TargetItem *item, int index)
{
    StageTarget *t = item != nullptr ? item->target() : nullptr;
    if (t == nullptr || !t->isLine() || t->locked())
        return;
    if (BlockingView *bv = BlockingView::of(m_doc); bv != nullptr && bv->editsSet())
    {
        editBlockedLine(t, bv->blocking(), [index](QVector<QVector3D> pts) {
            if (index > 0 && index < pts.size() - 1)
                pts.removeAt(index);
            return pts;
        });
        return;
    }
    const QVector3D posBefore = t->position();
    const QVector<QVector3D> before = t->linePoints();
    // The ends stay: a line has two.
    if (index <= 0 || index >= before.size() - 1)
        return;
    QVector<QVector3D> pts = before;
    pts.removeAt(index);
    t->setLinePoints(pts);
    commitLineEdit(t, posBefore, before);
}

void MonitorGraphicsView::updateAimLines()
{
    // Remove old aim lines
    foreach (QGraphicsLineItem *li, m_aimLines)
    {
        m_scene->removeItem(li);
        delete li;
    }
    m_aimLines.clear();

    // No scene, no aim: nothing is short of reach either.
    foreach (TargetItem *ti, m_targetItems)
        ti->setReachShortfall(0, 0, QStringList());

    if (m_cellPixels == 0)
        return;

    // Aim lines require an active scene; no scene = no lines.
    if (m_activeSceneId == Function::invalidId())
        return;

    Scene *activeScene = qobject_cast<Scene *>(m_doc->function(m_activeSceneId));
    if (!activeScene)
        return;

    /* Which heads can actually get to what they are aimed at -- solved against
       the same point, by the same geometry, the Aim palette uses. A head that
       cannot reach is not pointed at the target; it is stuck at its limit
       somewhere short of it, and a line drawn as if it arrived says otherwise. */
    QHash<QPair<quint32, quint32>, AimSolver::Solution> reach;
    QHash<QPair<quint32, quint32>, QVector3D> aimedAt;  // a line's head -> its own point
    QHash<QPair<quint32, quint32>, BeamOcclusion::Hit> sight;   // line of sight
    QHash<quint32, QPair<int, int> > shortfall;        // target -> (short, aimed)
    QHash<quint32, QStringList> shortNames;
    /* One trace per fixture: the arms of a multi-arm fixture (one entry
       each, from its own origin) meet in it -- their points' middle, the
       one that reaches least, blocked if either is. */
    QHash<QPair<quint32, quint32>, int> armsSeen;
    QSet<QPair<quint32, quint32> > shortSeen;
    foreach (const AimSolver::SceneAim &a, AimSolver::sceneAims(m_doc, activeScene))
    {
        const QPair<quint32, quint32> key = qMakePair(a.fixtureId, a.targetId);
        const int seen = armsSeen.value(key, 0);
        armsSeen.insert(key, seen + 1);
        if (seen > 0)
        {
            aimedAt[key] = (aimedAt.value(key) * float(seen) + a.point) / float(seen + 1);
            if (a.solution.marginDeg < reach.value(key).marginDeg)
                reach.insert(key, a.solution);
            if (a.sight.blocked && !sight.contains(key))
                sight.insert(key, a.sight);
        }
        else
        {
            reach.insert(key, a.solution);
            aimedAt.insert(key, a.point);
            if (a.sight.blocked)
                sight.insert(key, a.sight);
        }
        QPair<int, int> &sf = shortfall[a.targetId];
        if (seen == 0)
            sf.second += 1;
        if (a.solution.reachable() == false && !shortSeen.contains(key))
        {
            shortSeen.insert(key);
            sf.first += 1;
            if (Fixture *fx = m_doc->fixture(a.fixtureId))
                shortNames[a.targetId] << fx->name();
        }
    }
    for (auto it = shortfall.constBegin(); it != shortfall.constEnd(); ++it)
        if (TargetItem *ti = m_targetItems.value(it.key(), nullptr))
            ti->setReachShortfall(it.value().first, it.value().second,
                                  shortNames.value(it.key()));

    // Build the set of fixtures in the active scene.
    QSet<quint32> candidates;
    foreach (quint32 fid, activeScene->fixtures())
        candidates.insert(fid);
    foreach (quint32 gid, activeScene->fixtureGroups())
    {
        FixtureGroup *fg = m_doc->fixtureGroup(gid);
        if (fg)
        {
            foreach (quint32 fid, fg->fixtureList())
                candidates.insert(fid);
        }
    }

    if (candidates.isEmpty())
        return;

    /* Only targets the ACTIVE SCENE aims at. Every target is drawn on the
       canvas now, so "all targets in the view" is no longer the same set as
       "targets this scene uses" -- iterating m_targetItems drew a line from
       every fixture in the scene to every target on the stage, and switching
       scenes just added the next scene's lines to the last one's.
       isAimed() is set in updateTargets() from the scene's Aim palettes, so
       the lines and the glow can never disagree about which is which. */
    bool anySelected = false;
    foreach (TargetItem *ti, m_targetItems)
        if (ti->isAimed() && ti->isSelected()) { anySelected = true; break; }

        {
    foreach (TargetItem *ti, m_targetItems)
    {
        if (!ti->isAimed())
            continue;
        if (anySelected && !ti->isSelected())
            continue;

        StageTarget *tgt = ti->target();
        const QPointF tgtCentrePx = ti->pos();
        QPen aimPen(tgt->color().isValid() ? tgt->color() : QColor(255, 180, 0), 1.5);
        aimPen.setStyle(Qt::DashLine);

        for (quint32 fid : candidates)
        {
            MonitorFixtureItem *mfi = m_fixtures.value(fid);
            if (!mfi)
                continue;

            // Only AIMABLE fixtures (movers, with Pan/Tilt) draw a src→target
            // trace — a target aims beams, and a fixed head (pixel panel, wash)
            // has nothing to aim, so a trace to it is meaningless (and the Aim
            // palette is a no-op on it anyway).
            Fixture *fxi = m_doc->fixture(fid);
            if (fxi == nullptr)
                continue;
            const bool canAim =
                fxi->channelNumber(QLCChannel::Pan,  QLCChannel::MSB) != QLCChannel::invalid() ||
                fxi->channelNumber(QLCChannel::Tilt, QLCChannel::MSB) != QLCChannel::invalid();
            if (!canAim)
                continue;

            QPointF fixPx = mfi->iconCentreScene();
            /* On a line each head has its own point: the trace goes there,
               so you can see the beams spread, converge or cross. */
            QPointF tgtPx = tgtCentrePx;
            if (tgt->isLine())
            {
                const auto ait = aimedAt.constFind(qMakePair(fid, tgt->id()));
                if (ait != aimedAt.constEnd())
                    tgtPx = projectMm(double(ait->x()) * 1000.0, double(ait->y()) * 1000.0,
                                      double(ait->z()) * 1000.0);
            }

            /* Out of reach: solid red, and it says by how much. */
            QPen pen = aimPen;
            const auto rit = reach.constFind(qMakePair(fid, tgt->id()));
            const bool cannot = (rit != reach.constEnd() && !rit.value().reachable());
            if (cannot)
            {
                pen = QPen(QColor(235, 70, 60), 2.0);
                pen.setStyle(Qt::SolidLine);
            }

            /* Something solid in the way (line of sight): red and dashed as
               far as the blocker, a cross where it stops, and only a faint
               dotted line on to where it was meant to go. */
            const auto sit = sight.constFind(qMakePair(fid, tgt->id()));
            if (!cannot && sit != sight.constEnd())
            {
                const BeamOcclusion::Hit &hit = sit.value();
                const QPointF stopPx = fixPx + (tgtPx - fixPx) * qreal(qBound(0.0f, hit.fraction, 1.0f));
                QPen blockedPen(QColor(235, 70, 60), 2.0);
                blockedPen.setStyle(Qt::DashLine);
                const QString tip = tr("%1 cannot see %2: blocked by %3, %4 out")
                                        .arg(fxi->name(), tgt->name(), hit.name,
                                             MonitorProperties::formatStageDistance(hit.distance));
                QGraphicsLineItem *toBlock = m_scene->addLine(
                    fixPx.x(), fixPx.y(), stopPx.x(), stopPx.y(), blockedPen);
                toBlock->setZValue(2.5);
                toBlock->setAcceptedMouseButtons(Qt::NoButton);
                toBlock->setToolTip(tip);
                toBlock->setData(0, QStringLiteral("blockedTrace"));
                m_aimLines.append(toBlock);
                QPen restPen(QColor(235, 70, 60, 90), 1.0);
                restPen.setStyle(Qt::DotLine);
                QGraphicsLineItem *rest = m_scene->addLine(
                    stopPx.x(), stopPx.y(), tgtPx.x(), tgtPx.y(), restPen);
                rest->setZValue(2.4);
                rest->setAcceptedMouseButtons(Qt::NoButton);
                rest->setToolTip(tip);
                m_aimLines.append(rest);
                QPen crossPen(QColor(235, 70, 60), 2.5);
                const qreal c = 5.0;
                QGraphicsLineItem *x1 = m_scene->addLine(stopPx.x() - c, stopPx.y() - c,
                                                         stopPx.x() + c, stopPx.y() + c, crossPen);
                QGraphicsLineItem *x2 = m_scene->addLine(stopPx.x() - c, stopPx.y() + c,
                                                         stopPx.x() + c, stopPx.y() - c, crossPen);
                for (QGraphicsLineItem *x : { x1, x2 })
                {
                    x->setZValue(2.6);
                    x->setAcceptedMouseButtons(Qt::NoButton);
                    x->setToolTip(tip);
                    m_aimLines.append(x);
                }
                continue;
            }

            QGraphicsLineItem *line = m_scene->addLine(
                fixPx.x(), fixPx.y(), tgtPx.x(), tgtPx.y(), pen);
            line->setZValue(2.5);
            line->setAcceptedMouseButtons(Qt::NoButton);   // a trace, not a target
            if (cannot)
                line->setToolTip(tr("%1 cannot reach %2: %3° past its pan/tilt limit")
                                     .arg(fxi->name(), tgt->name())
                                     .arg(-rit.value().marginDeg, 0, 'f', 0));
            m_aimLines.append(line);

            // Arrow head at the target end
            QPointF dir = tgtPx - fixPx;
            double len = qSqrt(dir.x() * dir.x() + dir.y() * dir.y());
            if (len > 0.0)
            {
                dir /= len;
                QPointF norm(-dir.y(), dir.x());
                QPointF tip   = tgtPx - dir * 8.0;
                QPointF left  = tip + norm * 4.0;
                QPointF right = tip - norm * 4.0;
                QGraphicsLineItem *al = m_scene->addLine(
                    tgtPx.x(), tgtPx.y(), left.x(),  left.y(),  pen);
                QGraphicsLineItem *ar = m_scene->addLine(
                    tgtPx.x(), tgtPx.y(), right.x(), right.y(), pen);
                al->setZValue(2.5); ar->setZValue(2.5);
                al->setAcceptedMouseButtons(Qt::NoButton);
                ar->setAcceptedMouseButtons(Qt::NoButton);
                m_aimLines.append(al); m_aimLines.append(ar);
            }
        }
    }
        }

    // Force a repaint of the affected region: when this runs from a target
    // REMOVAL (lines just deleted, none re-added) the scene didn't always
    // invalidate, leaving ghost traces until the next resize.
    if (m_scene != nullptr)
        m_scene->update();
}

void MonitorGraphicsView::updateTrussAnchorLines()
{
    foreach (QGraphicsLineItem *li, m_trussAnchorLines)
    {
        m_scene->removeItem(li);
        delete li;
    }
    m_trussAnchorLines.clear();

    if (m_cellPixels == 0)
        return;

    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr)
        return;

    // Neutral steady-state color matches the truss's own unselected chord
    // (TrussItem::paint()) — a structural relationship, not an alert — and
    // switches to the truss's own selected amber when the truss (and by
    // extension its bound fixtures) is the current selection, so the tether
    // reads as part of "this whole assembly is selected" rather than a
    // separate always-on color competing with it.
    const QColor neutralColor(160, 163, 172);
    const QColor selectedColor(255, 180, 0);

    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
    {
        MonitorFixtureItem *mfi = it.value();
        const FixtureRigProps rp = props->fixtureRigProps(it.key());
        if (rp.trussId == Truss::invalidId())
        {
            mfi->setTrussGroupSelected(false);
            continue;
        }
        Truss *t = props->truss(rp.trussId);
        TrussItem *ti = m_trussItems.value(rp.trussId, nullptr);
        const bool selected = (ti != nullptr && ti->isSelected());
        // Propagate regardless of truss type / tether eligibility below — the
        // fixture's own outline color should follow the truss's selection
        // even when there's no cross offset (no line drawn) or it's a tower.
        mfi->setTrussGroupSelected(selected);

        if (t == nullptr || t->type() == Truss::Vertical)
            continue;   // a tower's fixtures sit at its XY — no "across" to show

        const float halfWidthM = t->width() * 0.5f;

        /* The tether is drawn in whatever view is on screen, so BOTH ends are
           real 3-D points and projectMm() decides what that looks like.
           It used to be built purely from trussCross, with both ends sharing
           the truss's own Z -- which describes the across offset and nothing
           else. In Top that is the whole story; in an elevation the across
           axis is not on screen at all, so the line collapsed to zero length
           and vanished. And a fixture hung below its truss on mountZOffset --
           now that dragging vertically in Front sets exactly that -- had no
           across offset to speak of, so it was skipped outright and hung in
           space with nothing connecting it.

           Fixture end: its real rigged position, which already includes the
           cross offset, the mount side and the height nudge. Truss end: the
           point on the truss at the same offset, moved out to the near EDGE
           rather than the centreline, since the truss is drawn with real width
           and stopping at the centre buries half the line under it. */
        const QVector3D centreW = t->positionAt(rp.trussOffset);   // metres
        const QPointF dir = t->direction();
        const QPointF perp(-dir.y(), dir.x());

        const QVector3D fixW = props->fixtureRigPosition(it.key());
        const float edgeCross = (rp.trussCross >= 0.0f) ? halfWidthM : -halfWidthM;
        const QVector3D edgeW(centreW.x() + perp.x() * edgeCross,
                              centreW.y() + perp.y() * edgeCross,
                              centreW.z());

        const QPointF fixPx    = projectMm(fixW.x() * 1000.0, fixW.y() * 1000.0, fixW.z() * 1000.0);
        const QPointF anchorPx = projectMm(edgeW.x() * 1000.0, edgeW.y() * 1000.0, edgeW.z() * 1000.0);

        /* Skip only when the two ends land on top of each other IN THIS VIEW.
           Deciding that from trussCross alone was a Top-view judgement applied
           to every view: it hid lines that would have been perfectly visible
           in an elevation, and drew zero-length ones that could not be. */
        const QPointF d = fixPx - anchorPx;
        if ((d.x() * d.x() + d.y() * d.y()) < (3.0 * 3.0))
            continue;

        QPen tetherPen(selected ? selectedColor : neutralColor, selected ? 3.0 : 2.4);
        tetherPen.setStyle(Qt::DashLine);
        tetherPen.setCapStyle(Qt::RoundCap);

        QGraphicsLineItem *line = m_scene->addLine(
            fixPx.x(), fixPx.y(), anchorPx.x(), anchorPx.y(), tetherPen);
        line->setZValue(1.5);   // above the truss (-0.5), below fixtures (2)
        line->setAcceptedMouseButtons(Qt::NoButton);   // a tether, not a target
        m_trussAnchorLines.append(line);
    }
}

void MonitorGraphicsView::setTargetScope(bool all, const QSet<quint32> &ids)
{
    if (m_targetScopeAll == all && m_targetScope == ids)
        return;
    m_targetScopeAll = all;
    m_targetScope = ids;
    refreshTargetVisibility();
}

bool MonitorGraphicsView::targetInScope(quint32 targetId) const
{
    if (m_targetScopeAll || m_targetScope.contains(targetId) || m_placedTargets.contains(targetId))
        return true;
    // The people on the stage are the scene, not a marker the cue scope narrows.
    const MonitorProperties *props = m_doc->monitorProperties();
    return props != nullptr && PerformerFill::isPerformer(props, props->stageTarget(targetId));
}

void MonitorGraphicsView::refreshTargetVisibility()
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr)
        return;
    foreach (TargetItem *ti, m_targetItems)
    {
        StageTarget *t = ti->target();
        const MonitorProperties::MonitorLayer lyr = props->layer(t ? t->layerId() : 0);
        ti->setVisible(lyr.visible && !m_stageOnly && (ti->isSelected() || targetInScope(ti->targetId())));
    }
}

void MonitorGraphicsView::setActiveScene(quint32 sceneId)
{
    m_activeSceneId = sceneId;
    updateTargets();  // re-filter targets to those used by the new scene
}

void MonitorGraphicsView::clearFollowSpotOverlay()
{
    for (QGraphicsItem *it : std::as_const(m_followItems))
    {
        if (m_scene != nullptr && it->scene() == m_scene)
            m_scene->removeItem(it);
        delete it;
    }
    m_followItems.clear();
}

void MonitorGraphicsView::updateFollowSpotOverlay()
{
    clearFollowSpotOverlay();
    ProgrammerController *pc = m_doc != nullptr ? m_doc->programmer() : nullptr;
    MonitorProperties *props = m_doc != nullptr ? m_doc->monitorProperties() : nullptr;
    if (pc == nullptr || props == nullptr || m_scene == nullptr || m_cellPixels == 0)
        return;
    using Status = ProgrammerController::FollowSpotStatus;
    const Status st = pc->followSpotStatus();
    if (st.state == Status::Off || st.targetId == UINT_MAX)
        return;
    StageTarget *tgt = props->stageTarget(st.targetId);
    Scene *scene = qobject_cast<Scene *>(m_doc->function(st.sceneId));
    if (tgt == nullptr || scene == nullptr)
        return;

    const QColor colour = st.state == Status::Moving ? QColor(0x3e, 0xc4, 0x6d)
                        : st.state == Status::Holding ? QColor(0xe0, 0xa8, 0x20)
                                                      : QColor(0xd9, 0x53, 0x4f);
    /* Where its marker is drawn, worked out the way the marker is
       (updateTargets()): in the blocking shown, through this view's
       projection. It was the Top view's maths on the Default position. */
    const QVector3D at = tgt->isLine() ? props->targetAimPoint(tgt) : props->targetPosition(tgt);
    const QPointF tgtPx = projectMm(double(at.x()) * 1000.0, double(at.y()) * 1000.0,
                                    double(at.z()) * 1000.0);
    const QString tip = pc->followSpotStatusText();

    QList<quint32> heads = scene->fixtures();
    for (quint32 gid : scene->fixtureGroups())
        if (FixtureGroup *fg = m_doc->fixtureGroup(gid))
            for (quint32 fid : fg->fixtureList())
                if (!heads.contains(fid))
                    heads << fid;
    QPen linePen(colour, 2.0);
    linePen.setCosmetic(true);
    for (quint32 fid : std::as_const(heads))
    {
        MonitorFixtureItem *mfi = m_fixtures.value(fid);
        if (mfi == nullptr)
            continue;
        const QPointF from = mfi->iconCentreScene();
        QGraphicsLineItem *li = m_scene->addLine(from.x(), from.y(), tgtPx.x(), tgtPx.y(), linePen);
        li->setZValue(98);
        li->setAcceptedMouseButtons(Qt::NoButton);
        li->setToolTip(tip);
        li->setData(0, QStringLiteral("followLine"));
        m_followItems << li;
    }

    const qreal r = 24.0;
    QPen ringPen(colour, 3.0);
    ringPen.setCosmetic(true);
    QGraphicsEllipseItem *ring = m_scene->addEllipse(tgtPx.x() - r, tgtPx.y() - r, 2 * r, 2 * r, ringPen,
                                                     QColor(colour.red(), colour.green(), colour.blue(), 40));
    ring->setZValue(99);
    ring->setAcceptedMouseButtons(Qt::NoButton);
    ring->setToolTip(tip);
    ring->setData(0, QStringLiteral("followRing"));
    m_followItems << ring;

    /* Above the ring on SCREEN, however the plot is turned (the target's own
       name is beside it): a stage-mark label, drawn in screen space on its
       own dark plate. It was scene text with a scene plate under it, so a
       turn stood the plate on end under upright text, and put the label
       beside the ring instead of over it. */
    StageMarkLabel *label = new StageMarkLabel(
        st.state == Status::Moving ? tr("following · moving")
        : st.state == Status::Holding ? tr("following · holding") : tr("following · no stick"),
        colour, Qt::AlignHCenter | Qt::AlignBottom);
    label->setPos(tgtPx);
    label->setStackOffset(-(r * viewScale() + 2.0));
    label->setZValue(99.5);
    label->setToolTip(tip);
    label->setData(0, QStringLiteral("followLabel"));
    label->setData(1, r);
    m_scene->addItem(label);
    m_followItems << label;
}

void MonitorGraphicsView::refreshFollowSpotLabel()
{
    // Over the ring, which grows with the zoom while the label does not.
    for (QGraphicsItem *it : std::as_const(m_followItems))
        if (it->data(0).toString() == QLatin1String("followLabel"))
            static_cast<StageMarkLabel *>(it)->setStackOffset(-(it->data(1).toReal() * viewScale() + 2.0));
}

void MonitorGraphicsView::setFollowSpotPin(bool visible, float xMeters, float yMeters)
{
    if (!m_scene) return;

    // EDIT only: the joystick is AUTHORING the target, so the target MARKER must
    // track it (StageTarget::setPosition() is signal-less, so nothing else would).
    // In RUN the target moves only transiently to converge the beam — the saved
    // target marker must stay put; the followspot PIN below shows the live beam.
    if (m_doc && m_doc->mode() != Doc::Operate)
        repositionTargetItems();

    if (!visible)
    {
        if (m_fsPinCircle) m_fsPinCircle->setVisible(false);
        if (m_fsPinH)      m_fsPinH->setVisible(false);
        if (m_fsPinV)      m_fsPinV->setVisible(false);
        return;
    }

    // Convert metres → scene pixels using the same transform as TargetItem.
    if (m_cellPixels == 0) return;
    const float px = float(m_xOffset + (xMeters * 1000.0 * m_cellPixels) / m_unitValue);
    const float py = float(m_yOffset + (yMeters * 1000.0 * m_cellPixels) / m_unitValue);
    const float r  = 18.0f;  // radius in scene pixels

    // Create pin items on first use.
    if (!m_fsPinCircle)
    {
        m_fsPinCircle = new QGraphicsEllipseItem();
        m_fsPinCircle->setZValue(100);
        m_fsPinCircle->setFlag(QGraphicsItem::ItemIsSelectable, false);
        m_fsPinCircle->setFlag(QGraphicsItem::ItemIsMovable, false);
        m_scene->addItem(m_fsPinCircle);

        m_fsPinCircle->setAcceptedMouseButtons(Qt::NoButton);
        m_fsPinH = new QGraphicsLineItem();
        m_fsPinH->setZValue(101);
        m_fsPinH->setAcceptedMouseButtons(Qt::NoButton);
        m_scene->addItem(m_fsPinH);

        m_fsPinV = new QGraphicsLineItem();
        m_fsPinV->setZValue(101);
        m_fsPinV->setAcceptedMouseButtons(Qt::NoButton);
        m_scene->addItem(m_fsPinV);

        m_fsPinLabel = new QGraphicsSimpleTextItem();
        m_fsPinLabel->setZValue(102);
        m_fsPinLabel->setAcceptedMouseButtons(Qt::NoButton);
        QFont lf("Arial", 9, QFont::Bold);
        m_fsPinLabel->setFont(lf);
        m_scene->addItem(m_fsPinLabel);
    }

    // No bound/connected controller → the follow-spot can't be driven. Flag it:
    // red pin + "⚠ no joystick" label so it's obvious nothing's plugged in.
    const bool hasInput = m_doc->programmer() && m_doc->programmer()->hasFollowSpotInput();
    const QColor pinColor = hasInput ? QColor(255, 140, 0)     // orange = drivable
                                     : QColor(220, 60, 60);    // red    = no input
    const QPen pen(pinColor, 2.5);
    m_fsPinCircle->setPen(pen);
    m_fsPinCircle->setBrush(QColor(pinColor.red(), pinColor.green(), pinColor.blue(), 60));
    m_fsPinH->setPen(pen);
    m_fsPinV->setPen(pen);
    m_fsPinCircle->setToolTip(hasInput ? QObject::tr("Follow-spot beam position")
        : QObject::tr("Follow-spot has no joystick — connect/bind a controller to move it"));

    m_fsPinCircle->setRect(px - r, py - r, r * 2, r * 2);
    m_fsPinH->setLine(px - r, py, px + r, py);
    m_fsPinV->setLine(px, py - r, px, py + r);

    m_fsPinLabel->setText(hasInput ? QString() : QObject::tr("⚠ no joystick"));
    m_fsPinLabel->setBrush(QColor(220, 60, 60));
    m_fsPinLabel->setPos(px + r + 3, py - r);
    m_fsPinLabel->setVisible(!hasInput);

    m_fsPinCircle->setVisible(true);
    m_fsPinH->setVisible(true);
    m_fsPinV->setVisible(true);
}

void MonitorGraphicsView::setHintedFixtures(const QList<quint32> &ids)
{
    const QSet<quint32> idSet(ids.begin(), ids.end());
    for (auto it = m_fixtures.begin(); it != m_fixtures.end(); ++it)
        it.value()->setHinted(idSet.contains(it.key()));
}

QHash<quint32, QPointF> MonitorGraphicsView::fixtureCentres() const
{
    QHash<quint32, QPointF> out;
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        if (it.value() != nullptr && it.value()->isVisible())
            out.insert(it.key(), it.value()->sceneBoundingRect().center());
    return out;
}

void MonitorGraphicsView::highlightFixtures(const QList<quint32> &ids)
{
    const QSet<quint32> idSet(ids.begin(), ids.end());
    for (auto it = m_fixtures.begin(); it != m_fixtures.end(); ++it)
        it.value()->setHighlighted(idSet.contains(it.key()));
}

void MonitorGraphicsView::setFocusGhost(bool on, const QVector3D &point, const QString &label)
{
    if (!on)
    {
        if (m_focusGhost != nullptr)
            m_focusGhost->hide();
        if (m_focusGhostLabel != nullptr)
            m_focusGhostLabel->hide();
        return;
    }
    if (m_focusGhost == nullptr)
    {
        m_focusGhost = new QGraphicsEllipseItem();
        QPen pen(QColor(255, 192, 0), 2.0, Qt::DashLine);
        pen.setCosmetic(true);
        m_focusGhost->setPen(pen);
        m_focusGhost->setBrush(QColor(255, 192, 0, 40));
        m_focusGhost->setZValue(10000);
        m_scene->addItem(m_focusGhost);
        m_focusGhostLabel = new QGraphicsSimpleTextItem();
        m_focusGhostLabel->setBrush(QColor(255, 192, 0));
        m_focusGhostLabel->setZValue(10001);
        m_scene->addItem(m_focusGhostLabel);
    }
    const QPointF at = realPositionToPixels(double(point.x()) * 1000.0, double(point.y()) * 1000.0);
    const qreal r = qMax(6.0, double(m_cellPixels) * 0.3);   // about 0.3 m across a 1 m cell
    m_focusGhost->setRect(QRectF(at.x() - r, at.y() - r, 2 * r, 2 * r));
    m_focusGhost->show();
    m_focusGhostLabel->setText(tr("saved") + (label.isEmpty() ? QString() : QStringLiteral(" (") + label + QLatin1Char(')')));
    m_focusGhostLabel->setPos(at.x() + r + 3, at.y() - r);
    m_focusGhostLabel->show();
}

bool MonitorGraphicsView::hasFocusGhost() const
{
    return m_focusGhost != nullptr && m_focusGhost->isVisible();
}

QString MonitorGraphicsView::focusGhostLabel() const
{
    return hasFocusGhost() ? m_focusGhostLabel->text() : QString();
}

void MonitorGraphicsView::markUnsavedFixtures(const QList<quint32> &ids)
{
    const QSet<quint32> idSet(ids.begin(), ids.end());
    for (auto it = m_fixtures.begin(); it != m_fixtures.end(); ++it)
        it.value()->setUnsaved(idSet.contains(it.key()));
}

void MonitorGraphicsView::markNeedsFocusFixtures(const QList<quint32> &ids)
{
    const QSet<quint32> idSet(ids.begin(), ids.end());
    for (auto it = m_fixtures.begin(); it != m_fixtures.end(); ++it)
        it.value()->setNeedsFocus(idSet.contains(it.key()));
}

void MonitorGraphicsView::markBuriedFixtures(const QList<quint32> &ids)
{
    const QSet<quint32> idSet(ids.begin(), ids.end());
    for (auto it = m_fixtures.begin(); it != m_fixtures.end(); ++it)
        it.value()->setBuried(idSet.contains(it.key()));
}

QList<quint32> MonitorGraphicsView::buriedFixtures() const
{
    QList<quint32> out;
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        if (it.value()->isBuried())
            out << it.key();
    return out;
}

void MonitorGraphicsView::markOffRowFixtures(const QList<quint32> &ids)
{
    const QSet<quint32> idSet(ids.begin(), ids.end());
    for (auto it = m_fixtures.begin(); it != m_fixtures.end(); ++it)
        it.value()->setOffRow(idSet.contains(it.key()));
}

QList<quint32> MonitorGraphicsView::offRowFixtures() const
{
    QList<quint32> out;
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        if (it.value()->isOffRow())
            out << it.key();
    return out;
}

QList<quint32> MonitorGraphicsView::needsFocusFixtures() const
{
    QList<quint32> out;
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        if (it.value()->needsFocus())
            out << it.key();
    return out;
}

QPointF MonitorGraphicsView::pixelsToRealPosition(qreal px, qreal py)
{
    if (m_cellPixels == 0)
        return QPointF(0, 0);
    return QPointF((px - m_xOffset) * m_unitValue / m_cellPixels,
                   (py - m_yOffset) * m_unitValue / m_cellPixels);
}

/* ------------------------------------------------------------------------ *
 *  Rulers / coordinate readout / settable 0,0 origin
 *
 *  One grid cell == one display unit (metre or foot) == m_cellPixels scene
 *  pixels. Integer-unit graduations therefore fall exactly on the grid lines
 *  (scene X = m_xOffset + k*cellPixels). Everything is mapped through
 *  mapFromScene/mapToScene so it survives the view's zoom & pan transform.
 * ------------------------------------------------------------------------ */

QPointF MonitorGraphicsView::stageOriginMetres() const
{
    MonitorProperties *props = m_doc->monitorProperties();
    return props != NULL ? props->stageOrigin() : QPointF(0, 0);
}

void MonitorGraphicsView::setStageOriginMetres(const QPointF &metres)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (props != NULL)
        props->setStageOrigin(metres);
    emit rulersChanged();
}

qreal MonitorGraphicsView::originUnitsH() const
{
    // Horizontal axis is X for Top/Front, Y for Side. Origin stored in metres.
    const QPointF o = stageOriginMetres();
    const qreal metres = (m_pov == PovSide) ? o.y() : o.x();
    return (m_unitValue > 0.0) ? (metres * 1000.0 / m_unitValue) : 0.0;
}

qreal MonitorGraphicsView::originUnitsV() const
{
    // Vertical axis is Y in Top view; in elevation it's height (Z) off the
    // floor, which has no user origin.
    if (isElevation())
        return 0.0;
    const QPointF o = stageOriginMetres();
    return (m_unitValue > 0.0) ? (o.y() * 1000.0 / m_unitValue) : 0.0;
}

QString MonitorGraphicsView::unitSuffix() const
{
    // The ruler counts the grid's cells, so it names the grid's unit.
    return MeasurementUnits::symbol(m_unitValue > 500.0 ? MeasurementUnits::Metres : MeasurementUnits::Feet);
}

QString MonitorGraphicsView::axisName(bool horizontal) const
{
    // Named for the scene axis the ruler runs along, which a quarter turn swaps.
    if (!rulerAlongSceneY(horizontal))
        return (m_pov == PovSide) ? QStringLiteral("Y") : QStringLiteral("X");
    return (m_pov == PovTop) ? QStringLiteral("Y") : QStringLiteral("Z");
}

static QString formatRulerLabel(qreal value)
{
    // Whole numbers get no decimals; fractional origins get one.
    if (qAbs(value - qRound(value)) < 0.05)
        return QString::number(qRound(value));
    return QString::number(value, 'f', 1);
}

bool MonitorGraphicsView::rulerCountsFromCentreLine() const
{
    /* Side has no stage width on screen; a ⌖ origin the user placed is a
       zero they chose, and wins. */
    if (m_pov == PovSide || m_doc->monitorProperties() == nullptr)
        return false;
    return stageOriginMetres().isNull();
}

qreal MonitorGraphicsView::rulerZeroUnits(bool sceneY) const
{
    if (sceneY)
        return originUnitsV();
    if (rulerCountsFromCentreLine() && m_unitValue > 0.0)
        return qreal(m_doc->monitorProperties()->stageCentre().x()) * 1000.0 / m_unitValue;
    return originUnitsH();
}

bool MonitorGraphicsView::stageRightFirst(bool horizontal) const
{
    // +X is stage LEFT (see drawOrientationLabels()): stage right comes first
    // when scene X runs the way this ruler's pixels do.
    const QTransform t = transform();
    const qreal along = horizontal ? t.m11() : t.m12();
    return along >= 0.0;
}

bool MonitorGraphicsView::rulerAlongSceneY(bool horizontal) const
{
    // Where scene X's unit vector lands in the viewport: (m11, m12).
    const QTransform t = transform();
    const bool sceneXAcross = qAbs(t.m11()) >= qAbs(t.m12());
    return horizontal ? !sceneXAcross : sceneXAcross;
}

/* The ruler value at a scene coordinate along one scene axis: what the
   ticks are labelled with and what the footer reads, so the two agree. */
static qreal axisValueAt(qreal sceneCoord, bool sceneY, bool elevation,
                         qreal xOffset, qreal yOffset, qreal floorY, qreal cell,
                         qreal originH, qreal originV)
{
    if (!sceneY)
        return (sceneCoord - xOffset) / cell - originH;
    if (elevation)
        return (floorY - sceneCoord) / cell;          // height off the floor
    return (sceneCoord - yOffset) / cell - originV;
}

QPointF MonitorGraphicsView::viewportToReadout(const QPoint &vp) const
{
    if (m_cellPixels <= 0)
        return QPointF(0, 0);
    const QPointF scene = mapToScene(vp);
    /* Read along the rulers: (top ruler, left ruler). A quarter turn puts
       scene Y along the top. */
    auto along = [&](bool horizontal) {
        const bool sy = rulerAlongSceneY(horizontal);
        return axisValueAt(sy ? scene.y() : scene.x(), sy, isElevation(), m_xOffset, m_yOffset,
                           floorPixelY(), m_cellPixels, rulerZeroUnits(false), originUnitsV());
    };
    return QPointF(along(true), along(false));
}

QVector<MonitorGraphicsView::RulerTick> MonitorGraphicsView::rulerTicks(bool horizontal) const
{
    QVector<RulerTick> ticks;
    if (m_cellPixels <= 0)
        return ticks;

    /* One mapping for every tick: scene -> viewport through the view's own
       transform (zoom, scroll, quarter turn), unrounded. Which scene axis
       this ruler measures follows the turn: at 90/270 degrees the top ruler
       runs along scene Y. Only the coordinate along the ruler matters, so
       the other is the middle of what is visible. */
    const bool sceneY = rulerAlongSceneY(horizontal);
    const QRectF vis = mapToScene(viewport()->rect()).boundingRect();
    auto pixelOf = [&](qreal sceneCoord) {
        const QPointF sp = sceneY ? QPointF(vis.center().x(), sceneCoord)
                                  : QPointF(sceneCoord, vis.center().y());
        const QPointF v = sceneToViewport(sp);
        return horizontal ? v.x() : v.y();
    };

    if (!sceneY && rulerCountsFromCentreLine())
    {
        /* From the centre line: a tick on it, labelled CL, and whole units
           out to each side, unsigned -- the side says SR or SL, as the
           readouts do. Value keeps the sign (stage right negative). */
        const qreal anchor = m_xOffset + rulerZeroUnits(false) * m_cellPixels;
        int kMin = qFloor((vis.left()  - anchor) / m_cellPixels) - 1;
        int kMax = qCeil ((vis.right() - anchor) / m_cellPixels) + 1;
        for (int k = kMin; k <= kMax; k++)
            ticks.append({ pixelOf(anchor + qreal(k) * m_cellPixels),
                           k == 0 ? QStringLiteral("CL") : QString::number(qAbs(k)), true, qreal(k) });
    }
    else if (!sceneY)
    {
        const qreal anchor = m_xOffset;
        const qreal originU = originUnitsH();
        int kMin = qFloor((vis.left()  - anchor) / m_cellPixels) - 1;
        int kMax = qCeil ((vis.right() - anchor) / m_cellPixels) + 1;
        for (int k = kMin; k <= kMax; k++)
            ticks.append({ pixelOf(anchor + qreal(k) * m_cellPixels),
                           formatRulerLabel(qreal(k) - originU), true, qreal(k) - originU });
    }
    else if (isElevation())
    {
        // Height increases upward from the floor line.
        const qreal anchor = floorPixelY();
        int kMin = qFloor((anchor - vis.bottom()) / m_cellPixels) - 1;
        int kMax = qCeil ((anchor - vis.top())    / m_cellPixels) + 1;
        for (int k = kMin; k <= kMax; k++)
        {
            if (k < 0)
                continue;   // nothing below the floor
            ticks.append({ pixelOf(anchor - qreal(k) * m_cellPixels), formatRulerLabel(qreal(k)), true, qreal(k) });
        }
    }
    else
    {
        const qreal anchor = m_yOffset;
        const qreal originU = originUnitsV();
        int kMin = qFloor((vis.top()    - anchor) / m_cellPixels) - 1;
        int kMax = qCeil ((vis.bottom() - anchor) / m_cellPixels) + 1;
        for (int k = kMin; k <= kMax; k++)
            ticks.append({ pixelOf(anchor + qreal(k) * m_cellPixels),
                           formatRulerLabel(qreal(k) - originU), true, qreal(k) - originU });
    }
    return ticks;
}

void MonitorGraphicsView::beginPickOrigin()
{
    if (isElevation())
        return;   // origin only meaningful in Top (X,Y) view
    m_pickingOrigin = true;
    viewport()->setCursor(Qt::CrossCursor);
}

void MonitorGraphicsView::setGridVisible(bool on)
{
    m_gridVisible = on;
    foreach (QGraphicsLineItem *gi, m_gridItems)
        gi->setVisible(on);
}

void MonitorGraphicsView::setCenterLinesVisible(bool on)
{
    m_centerLinesVisible = on;
    if (m_centerLineV != nullptr) m_centerLineV->setVisible(on && !centreLineReplacesCross());
    if (m_centerLineH != nullptr) m_centerLineH->setVisible(on);
}

bool MonitorGraphicsView::centreLineReplacesCross() const
{
    /* Looking down, the stage marks' full centre line runs where the cross's
       up-and-down arm does (both at the middle of the grid's width): one line
       there, the crew's. The cross's other arm -- half way up and down the
       grid, the depth centre stage is reckoned from -- has no mark of its
       own, so it stays. An elevation draws no marks and keeps the cross. */
    const MonitorProperties *props = m_doc->monitorProperties();
    return m_pov == PovTop && props != nullptr
           && props->stageMarkShown(MonitorProperties::CentreLineFlag);
}

/* ------------------------------------------------------------------------ *
 *  Stage reference marks: centre mark / centre line, curtain line, number
 *  line. Drawn from MonitorProperties (saved with the show), Top view only.
 * ------------------------------------------------------------------------ */

void MonitorGraphicsView::updateStageMarks()
{
    if (m_scene == nullptr)
        return;
    foreach (QGraphicsItem *it, m_stageMarkItems)
    {
        m_scene->removeItem(it);
        delete it;
    }
    m_stageMarkItems.clear();
    // The full centre line takes the place of the teal cross's arm along it.
    if (m_centerLineV != nullptr)
        m_centerLineV->setVisible(m_centerLinesVisible && !centreLineReplacesCross());

    MonitorProperties *props = m_doc->monitorProperties();

    /* The apron's own deck, in every view: a deck like any other, but it is
       the apron piece's, so it is picked as that (updateStagePieceItems()).
       Kept to the curtain line and the main deck first. */
    if (m_apronDeckItem != nullptr)
    {
        m_scene->removeItem(m_apronDeckItem);
        delete m_apronDeckItem;
        m_apronDeckItem = nullptr;
    }
    if (props != nullptr)
    {
        props->syncApron();
        StagePlatform *apron = props->apronPlatform();
        // Hidden, it is still deck (Q155-2): only not drawn.
        if (apron != nullptr && m_cellPixels > 0 && props->layer(apron->layerId()).visible
            && !props->isHiddenApronPlatform(apron->id()))
        {
            m_apronDeckItem = newPlatformItem(apron);
            m_apronDeckItem->setFlag(QGraphicsItem::ItemIsMovable, false);
            m_apronDeckItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
            m_apronDeckItem->setAcceptedMouseButtons(Qt::NoButton);
            m_apronDeckItem->unsetCursor();
            m_apronDeckItem->setZValue(-0.98);          // over the deck it lies on
            m_apronDeckItem->showLabel(props->layer(apron->layerId()).labels);
            m_scene->addItem(m_apronDeckItem);
        }
    }
    auto pieces = qScopeGuard([this]() { updateStagePieceItems(); });

    if (props == nullptr || m_cellPixels <= 0 || !m_gridEnabled || m_pov != PovTop
        || props->stageMarks() == MonitorProperties::NoStageMarks)
        return;

    const qreal ppm = 1000.0 * m_cellPixels / m_unitValue;     // scene px a metre
    const qreal x0 = m_xOffset, x1 = m_xOffset + m_gridExtent.width() * m_cellPixels;
    const qreal y0 = m_yOffset;
    auto sx = [this, ppm](double m) { return m_xOffset + m * ppm; };
    auto sy = [this, ppm](double m) { return m_yOffset + m * ppm; };
    const qreal zoom = qMax(0.01, viewScale());
    const qreal onePx = 1.0 / zoom;                           // a screen pixel, in scene px

    auto add = [this](QGraphicsItem *it, const char *kind, qreal z) {
        it->setData(0, QString::fromLatin1(kind));
        it->setZValue(z);
        it->setAcceptedMouseButtons(Qt::NoButton);   // in front of the rig, never in its way
        m_scene->addItem(it);
        m_stageMarkItems.append(it);
    };
    auto line = [&](qreal ax, qreal ay, qreal bx, qreal by, const QPen &pen, const char *kind) {
        add(new QGraphicsLineItem(ax, ay, bx, by), kind, 2.2);
        static_cast<QGraphicsLineItem *>(m_stageMarkItems.last())->setPen(pen);
    };
    auto label = [&](const QString &text, const QPointF &at, const QColor &c,
                     Qt::Alignment align, const char *kind) {
        StageMarkLabel *l = new StageMarkLabel(text, c, align);
        l->setPos(at);
        l->setData(1, text);
        add(l, kind, 2.4);
    };
    /* Cosmetic pens: a hairline on screen at any zoom, dashes included. */
    auto pen = [](const QColor &c, qreal w, Qt::PenStyle style) {
        QPen p(c, w, style);
        p.setCosmetic(true);
        return p;
    };

    const double cxM = props->stageCentre().x();
    const qreal cx = sx(cxM);
    const qreal dsY = sy(props->downstageEdgeY());

    const QColor centreColour(170, 215, 255);      // pale blue
    const QColor curtainColour(240, 130, 130);     // house-curtain red, lightened
    const QColor numberColour(240, 210, 110);      // spike-tape yellow

    /* Everything is drawn ON the stage -- upstage of the front edge, which
       is often the very edge of the plot -- and clear of the orientation
       words round the plot's edges. Only what is shown: on, and its layer
       (Stage) not hidden. */
    const bool spike = props->stageMarkShown(MonitorProperties::CentreMarkFlag);
    const qreal s = 7.0 * onePx;                  // the spike's half width
    if (props->stageMarkShown(MonitorProperties::CentreLineFlag))
    {
        line(cx, dsY, cx, y0, pen(centreColour, 1.2, Qt::DashLine), "centreLine");
        label(tr("CL"), QPointF(cx, y0 + 0.75 * m_cellPixels), centreColour,
              Qt::AlignHCenter | Qt::AlignVCenter, "centreLine");       // on the line
    }
    if (spike)
    {
        /* A spike: a triangle on the deck, its tip on the edge at centre. */
        QPolygonF tri;
        tri << QPointF(cx, dsY) << QPointF(cx - s, dsY - 1.6 * s) << QPointF(cx + s, dsY - 1.6 * s);
        QGraphicsPolygonItem *pi = new QGraphicsPolygonItem(tri);
        pi->setPen(pen(centreColour, 1.0, Qt::SolidLine));
        pi->setBrush(centreColour);
        add(pi, "centreMark", 2.3);
        /* Its CL straight over it, on centre -- unless the number line runs
           through it, whose own centre label is the CL there. */
        const bool numberHere = props->stageMarkShown(MonitorProperties::NumberLineFlag)
                                && qAbs(sy(props->numberLineY()) - dsY) < 2.0 * s;
        if (!numberHere)
            label(tr("CL"), QPointF(cx, dsY - 1.6 * s), centreColour,
                  Qt::AlignHCenter | Qt::AlignBottom, "centreMark");
    }
    if (props->stageMarkShown(MonitorProperties::ApronFlag) && props->apronPlatform() != nullptr)
    {
        /* The apron is deck (its own platform, drawn above); its depth at
           centre dimensioned from the curtain line to its front. */
        const double apron = props->apronDepth();
        const qreal y = sy(props->curtainLineY());
        const qreal front = sy(props->curtainLineY() + apron);
        const qreal ax = cx + 34.0 * onePx;
        const qreal tick = 5.0 * onePx;
        const QPen dimPen = pen(curtainColour, 1.0, Qt::SolidLine);
        line(ax, y, ax, front, dimPen, "apron");
        line(ax - tick, y, ax + tick, y, dimPen, "apron");
        line(ax - tick, front, ax + tick, front, dimPen, "apron");
        label(tr("Apron %1").arg(MonitorProperties::formatStageDistance(float(apron))),
              QPointF(ax, y), curtainColour, Qt::AlignLeft | Qt::AlignTop, "apron");   // under the curtain line
    }
    if (props->stageMarkShown(MonitorProperties::CurtainLineFlag))
    {
        const qreal y = sy(props->curtainLineY());
        line(x0, y, x1, y, pen(curtainColour, 1.4, Qt::DashDotLine), "curtainLine");
        label(tr("Curtain line"), QPointF(x0, y), curtainColour,
              Qt::AlignLeft | Qt::AlignBottom, "curtainLine");

        /* What hangs downstage of the curtain line -- the FOH electric out
           over the audience -- says how far, in crew terms, just above its
           stage-left end (inside its own span, so never off the plot). */
        auto overhang = [&](const QString &name, const QVector3D &a, const QVector3D &b) {
            const float midY = (a.y() + b.y()) / 2.0f;
            if (props->curtainLineOffset(midY) <= 0.005f)
                return;
            const QVector3D end = (a.x() >= b.x()) ? a : b;
            const QString where = props->curtainLineOffsetText(midY);
            label(name.isEmpty() ? where : QStringLiteral("%1 · %2").arg(name, where),
                  QPointF(sx(end.x()), sy(end.y()) - 0.3 * ppm), curtainColour,
                  Qt::AlignRight | Qt::AlignBottom, "curtainOffset");
        };
        foreach (const Truss *t, props->trusses())
            if (t != nullptr && t->type() == Truss::Horizontal)
                overhang(t->name(), t->positionAt(0.0f), t->positionAt(t->length()));
        foreach (const Pipe *pp, props->pipes())
            if (pp != nullptr && !pp->isVertical())
                overhang(pp->name(), pp->positionAt(0.0f), pp->positionAt(pp->length()));
    }
    if (props->stageMarkShown(MonitorProperties::NumberLineFlag))
    {
        const qreal y = sy(props->numberLineY());
        const int reach = props->numberLineReach();
        const qreal unitPx = props->gridUnitMetres() * ppm;
        line(cx - reach * unitPx, y, cx + reach * unitPx, y,
             pen(numberColour, 1.2, Qt::SolidLine), "numberLine");

        /* Numbered every unit when there is room on screen, else every 2, 5,
           10... (each number needs about 22 screen pixels), with "CL" at
           centre and the long ticks on numbered places only -- the same rule
           as the Rig Overview, mirrored about centre. */
        const int stride = MonitorProperties::numberLineStride(unitPx * zoom);
        for (int k = -reach; k <= reach; ++k)
        {
            const qreal x = cx + k * unitPx;
            const bool major = MonitorProperties::numberLineMajorTick(k, stride);
            const qreal t = (major ? 6.0 : 3.5) * onePx;
            line(x, y - t, x, y + t, pen(numberColour, major ? 1.4 : 1.0, Qt::SolidLine), "numberLine");
            // Above the line (on the deck); the CL above a centre spike.
            const qreal above = (k == 0 && spike && qAbs(y - dsY) < 2.0 * s) ? 1.6 * s : t;
            if (k % stride == 0)
                label(MonitorProperties::numberLineLabel(k), QPointF(x, y - above), numberColour,
                      Qt::AlignHCenter | Qt::AlignBottom, "numberLine");
        }
        /* Which side is which: stage right is the plot's left. */
        if (reach > 0)
        {
            label(tr("SR"), QPointF(cx - reach * unitPx, y - 24.0 * onePx), numberColour,
                  Qt::AlignLeft | Qt::AlignBottom, "numberLine");
            label(tr("SL"), QPointF(cx + reach * unitPx, y - 24.0 * onePx), numberColour,
                  Qt::AlignRight | Qt::AlignBottom, "numberLine");
        }
    }
}

/* The stage pieces as objects: each a band along its line (the apron, its
   own outline) that a click selects and a drag moves up or down stage --
   the curtain line and the number line by themselves, the apron by its
   front edge. Over the decks (so a deck covering the stage does not take
   every click) and under everything rigged. */
void MonitorGraphicsView::updateStagePieceItems()
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (m_scene == nullptr || props == nullptr)
        return;
    const bool drawn = m_cellPixels > 0 && m_gridEnabled && m_pov == PovTop;
    const qreal ppm = drawn ? 1000.0 * m_cellPixels / m_unitValue : 1.0;
    auto sx = [this, ppm](double m) { return m_xOffset + m * ppm; };
    auto sy = [this, ppm](double m) { return m_yOffset + m * ppm; };
    const qreal onePx = 1.0 / qMax(0.01, viewScale());
    const qreal band = 5.0 * onePx;
    const qreal x0 = m_xOffset, x1 = m_xOffset + m_gridExtent.width() * m_cellPixels;
    const bool design = (m_doc->mode() == Doc::Design);

    foreach (MonitorProperties::StagePiece piece, MonitorProperties::stagePieces())
    {
        StagePieceItem *item = m_stagePieceItems.value(int(piece), nullptr);
        bool shown = drawn && props->stagePieceShown(piece);
        if (piece == MonitorProperties::ApronPiece && props->apronPlatform() == nullptr)
            shown = false;
        if (!shown)
        {
            if (item != nullptr)
            {
                if (item->isSelected())
                    item->selectNow(false);
                item->setVisible(false);
            }
            continue;
        }
        if (item == nullptr)
        {
            item = new StagePieceItem(int(piece));
            m_scene->addItem(item);
            m_stagePieceItems.insert(int(piece), item);
            connect(item, &StagePieceItem::dragStarted, this, &MonitorGraphicsView::stagePieceDragStarted);
            connect(item, &StagePieceItem::dragMoved, this, &MonitorGraphicsView::stagePieceDragMoved);
            connect(item, &StagePieceItem::dragFinished, this, &MonitorGraphicsView::stagePieceDragFinished);
        }
        item->setVisible(true);
        QPainterPath hit, drag;
        switch (piece)
        {
            case MonitorProperties::CentrePiece:
            {
                const qreal cx = sx(props->stageCentre().x());
                const qreal dsY = sy(props->downstageEdgeY());
                const qreal top = props->stageMarkVisible(MonitorProperties::CentreLineFlag)
                                ? m_yOffset : dsY - 12.0 * onePx;
                hit.addRect(QRectF(QPointF(cx - band, top), QPointF(cx + band, dsY)));
                hit.addRect(QRectF(QPointF(cx - 8.0 * onePx, dsY - 12.0 * onePx), QPointF(cx + 8.0 * onePx, dsY)));
                hit.setFillRule(Qt::WindingFill);
                item->setZValue(-0.6);
            }
            break;
            case MonitorProperties::CurtainPiece:
            {
                const qreal y = sy(props->curtainLineY());
                hit.addRect(QRectF(QPointF(x0, y - band), QPointF(x1, y + band)));
                drag = hit;
                item->setZValue(-0.6);
            }
            break;
            case MonitorProperties::NumberPiece:
            {
                const qreal y = sy(props->numberLineY());
                const qreal cx = sx(props->stageCentre().x());
                const qreal reach = props->numberLineReach() * props->gridUnitMetres() * ppm;
                hit.addRect(QRectF(QPointF(cx - reach - band, y - band), QPointF(cx + reach + band, y + band)));
                drag = hit;
                item->setZValue(-0.6);
            }
            break;
            case MonitorProperties::ApronPiece:
            {
                const StagePlatform *apron = props->apronPlatform();
                QPolygonF outline;
                foreach (const QPointF &pt, apron->footprint(24))
                    outline << QPointF(sx(pt.x()), sy(pt.y()));
                hit.addPolygon(outline);
                hit.closeSubpath();
                // The front edge, followed round a rounded front.
                QPainterPath front;
                const int steps = 24;
                for (int i = 0; i <= steps; ++i)
                {
                    const float x = apron->originX() + apron->width() * float(i) / float(steps);
                    const QPointF at(sx(x), sy(apron->frontEdgeAt(x)));
                    if (i == 0) front.moveTo(at); else front.lineTo(at);
                }
                QPainterPathStroker stroker;
                stroker.setWidth(2.0 * band + 2.0 * onePx);
                drag = stroker.createStroke(front);
                hit = hit.united(drag);
                item->setZValue(-0.97);                   // over its own deck, under the rig
            }
            break;
        }
        item->setHitShape(hit);
        item->setDragShape(drag);
        item->setMovable(design && !m_layoutLocked && !drag.isEmpty() && props->stagePieceMovable(piece));
        // On a locked Stage layer: click-through like every other layer-locked object.
        applyLayerLock(item, props->hasStageLayer() && props->layer(props->stageLayerId()).locked);
        item->update();
    }
}

int MonitorGraphicsView::selectedStagePiece() const
{
    for (auto it = m_stagePieceItems.constBegin(); it != m_stagePieceItems.constEnd(); ++it)
        if (it.value() != nullptr && it.value()->isVisible() && it.value()->isSelected())
            return it.key();
    return -1;
}

namespace
{
    /** Where a stage piece's line is along Y (metres): what a drag moves. */
    double stagePieceY(const MonitorProperties *props, int piece)
    {
        switch (MonitorProperties::StagePiece(piece))
        {
            case MonitorProperties::CurtainPiece: return props->curtainLineY();
            case MonitorProperties::NumberPiece:  return props->numberLineY();
            case MonitorProperties::ApronPiece:   return props->curtainLineY() + props->apronDepth();
            default: break;
        }
        return 0.0;
    }
}

void MonitorGraphicsView::stagePieceDragStarted(int piece, const QPointF &scenePos)
{
    MonitorProperties *props = m_doc->monitorProperties();
    m_stageDragBefore = m_rigSnapshotter ? m_rigSnapshotter() : QByteArray();
    const qreal ppm = 1000.0 * m_cellPixels / m_unitValue;
    if (props == nullptr || ppm <= 0.0)
        return;
    // The line keeps the gap it had to the pointer where it was taken hold of.
    m_stageDragOffset = stagePieceY(props, piece) - (scenePos.y() - m_yOffset) / ppm;
}

void MonitorGraphicsView::stagePieceDragMoved(int piece, const QPointF &scenePos)
{
    MonitorProperties *props = m_doc->monitorProperties();
    const qreal ppm = 1000.0 * m_cellPixels / m_unitValue;
    if (props == nullptr || ppm <= 0.0)
        return;
    double y = (scenePos.y() - m_yOffset) / ppm + m_stageDragOffset;
    if (m_snapDivisions > 0)
        y = (snapScenePos(QPointF(scenePos.x(), m_yOffset + y * ppm)).y() - m_yOffset) / ppm;
    switch (MonitorProperties::StagePiece(piece))
    {
        case MonitorProperties::CurtainPiece:
            props->setCurtainLineY(float(y));
        break;
        case MonitorProperties::NumberPiece:
            props->setNumberLineY(float(y));
        break;
        case MonitorProperties::ApronPiece:
            props->setApronDepth(float(qMax(0.0, y - double(props->curtainLineY()))));
        break;
        default:
            return;
    }
    updateStageMarks();
    emit stagePiecesChanged();
}

void MonitorGraphicsView::stagePieceDragFinished(int, bool moved)
{
    if (!moved)
        return;
    if (!m_stageDragBefore.isEmpty())
        pushRigUndo(m_stageDragBefore);
    m_stageDragBefore.clear();
    m_doc->setModified();
    emit stagePiecesChanged();
}

void MonitorGraphicsView::updateGrid()
{
    // It rebuilds the structures too: what was selected stays selected.
    const QList<QPair<QString, quint32> > selected = selectedMapObjects();
    auto keep = qScopeGuard([this, &selected]() { keepMapSelection(selected); });

    // removeItem() hands ownership of the item back to the caller, so it has
    // to be deleted here — otherwise every grid line is leaked, and updateGrid()
    // runs on each resizeEvent (dozens per window drag, and each rebuild now
    // makes gridSubdivisions× more lines than it used to).
    int itemsCount = m_gridItems.count();
    for (int i = 0; i < itemsCount; i++)
    {
        QGraphicsItem *item = (QGraphicsItem *)m_gridItems.takeLast();
        m_scene->removeItem(item);
        delete item;
    }
    if (m_centerLineV != nullptr) { m_scene->removeItem(m_centerLineV); delete m_centerLineV; m_centerLineV = nullptr; }
    if (m_centerLineH != nullptr) { m_scene->removeItem(m_centerLineH); delete m_centerLineH; m_centerLineH = nullptr; }

    if (m_gridEnabled == true)
    {
        m_xOffset = 0;
        m_yOffset = 0;
        /* The grid is built in SCENE coordinates, unrotated; the view turns it.
           So on an odd quarter turn the viewport's width is what the grid's
           HEIGHT has to fit into, and vice versa -- fit against the extents the
           grid will actually be laid out along, not the raw widget ones. */
        const int fitW = (m_viewRotation & 1) ? this->height() : this->width();
        const int fitH = (m_viewRotation & 1) ? this->width()  : this->height();
        const int xInc = int(fitW / m_gridExtent.width());
        const int yInc = int(fitH / m_gridExtent.height());

        /* Fit to whichever axis runs out first -- and note the <=.
         *
         * This was `if (yInc < xInc) ... else if (xInc < yInc) ...` with no
         * final else, so when the two came out EQUAL neither branch ran and
         * m_cellPixels silently kept whatever it was last set to. That is not
         * a rare tie: it is what a 40x24 grid in a 1667x991 view produces
         * (41 and 41), and the value left behind was 198, computed earlier
         * against the default 5x5 grid before the workspace had been read.
         * 40 cells at 198 px is eight of them across the window -- the studio
         * opening zoomed in. Resizing appeared to fix it only because almost
         * any other window size makes the two unequal again, at which point a
         * branch finally runs.
         */
        if (yInc <= xInc)
        {
            m_cellPixels = yInc;
            m_xOffset = int((fitW - (m_cellPixels * m_gridExtent.width())) / 2);
        }
        else
        {
            m_cellPixels = xInc;
            m_yOffset = int((fitH - (m_cellPixels * m_gridExtent.height())) / 2);
        }
        int xPos = m_xOffset;
        int yPos = m_yOffset;
        /* Every line ends where the grid does. They ran to the WIDGET's
           height and width less the offset, which is the grid's extent only
           when the grid fills the widget along that axis -- under a quarter
           turn it never does (the grid is laid out against the swapped
           extents above), so the lines overran the stage by most of a
           window and the grid read as off centre. */
        const qreal gridRight  = m_xOffset + m_gridExtent.width()  * m_cellPixels;
        const qreal gridBottom = m_yOffset + m_gridExtent.height() * m_cellPixels;

        QPen mainPen(QColor(40, 40, 40, 255));
        QPen subPen(QColor(30, 30, 30, 255));
        subPen.setStyle(Qt::DotLine);

        // draw sub-division lines first (so the main lines sit on top)
        if (m_gridSubdivisions > 1)
        {
            qreal subInc = (qreal)m_cellPixels / m_gridSubdivisions;
            const qreal bottom = gridBottom, right = gridRight;

            for (int i = 0; i < qCeil(m_gridExtent.width()); i++)
            {
                for (int s = 1; s < m_gridSubdivisions; s++)
                {
                    qreal sx = m_xOffset + i * m_cellPixels + s * subInc;
                    if (sx >= right - 0.01)
                        break;                                // past the real edge
                    QGraphicsLineItem *item = m_scene->addLine(sx, m_yOffset, sx, bottom, subPen);
                    item->setZValue(1);
                    m_gridItems.append(item);
                }
            }

            for (int i = 0; i < qCeil(m_gridExtent.height()); i++)
            {
                for (int s = 1; s < m_gridSubdivisions; s++)
                {
                    qreal sy = m_yOffset + i * m_cellPixels + s * subInc;
                    if (sy >= bottom - 0.01)
                        break;
                    QGraphicsLineItem *item = m_scene->addLine(m_xOffset, sy, right, sy, subPen);
                    item->setZValue(1);
                    m_gridItems.append(item);
                }
            }
        }

        /* A line every whole cell, and one at the real edge when the size is
           not a whole number of cells (the outline of a 15.24 m stage ends at
           15.24, not 15). */
        for (int i = 0; i <= int(m_gridExtent.width()); i++)
        {
            QGraphicsLineItem *item = m_scene->addLine(xPos, m_yOffset, xPos, gridBottom, mainPen);
            item->setZValue(1);
            xPos += m_cellPixels;
            m_gridItems.append(item);
        }
        if (gridRight - (xPos - m_cellPixels) > 0.5)
        {
            QGraphicsLineItem *item = m_scene->addLine(gridRight, m_yOffset, gridRight, gridBottom, mainPen);
            item->setZValue(1);
            m_gridItems.append(item);
        }

        for (int i = 0; i <= int(m_gridExtent.height()); i++)
        {
            QGraphicsLineItem *item = m_scene->addLine(m_xOffset, yPos, gridRight, yPos, mainPen);
            item->setZValue(1);
            yPos += m_cellPixels;
            m_gridItems.append(item);
        }
        if (gridBottom - (yPos - m_cellPixels) > 0.5)
        {
            QGraphicsLineItem *item = m_scene->addLine(m_xOffset, gridBottom, gridRight, gridBottom, mainPen);
            item->setZValue(1);
            m_gridItems.append(item);
        }

        /* The grid is drawn over trusses (z 1 against -0.5), so it must take no
           clicks: double-clicking a tower that stands on a grid line landed on
           the line and opened nothing. */
        foreach (QGraphicsLineItem *gl, m_gridItems)
            gl->setAcceptedMouseButtons(Qt::NoButton);

        // Highlight the centre vertical & horizontal axes so the stage centre
        // is obvious. Kept out of m_gridItems (and rebuilt separately below)
        // so their visibility toggles independently of the grid lines.
        QPen centerPen(QColor(0, 150, 140, 220)); // teal accent
        centerPen.setWidth(1);
        const qreal gridW = m_gridExtent.width()  * m_cellPixels;
        const qreal gridH = m_gridExtent.height() * m_cellPixels;
        /* Centre stage is MonitorProperties::stageCentre() -- what the stage
           marks are drawn from and the target readout counts SL/SR from --
           not the middle of the whole cells drawn: the grid size is cut to
           whole units here (a 50 ft stage is 15.24 m, drawn 15 cells), and
           the half-unit difference put this line 0.12 m off the readout's
           centre line in metres. */
        qreal cx = m_xOffset + gridW / 2.0;
        qreal cy = m_yOffset + gridH / 2.0;
        if (const MonitorProperties *mp = m_doc->monitorProperties())
        {
            const QVector3D sc = mp->stageCentre();
            const qreal ppmm = (m_unitValue > 0.0) ? qreal(m_cellPixels) / m_unitValue : 0.0;
            cx = m_xOffset + qreal(m_pov == PovSide ? sc.y() : sc.x()) * 1000.0 * ppmm;
            if (m_pov == PovTop)
                cy = m_yOffset + qreal(sc.y()) * 1000.0 * ppmm;
        }
        m_centerLineV = m_scene->addLine(cx, m_yOffset, cx, m_yOffset + gridH, centerPen);
        m_centerLineV->setZValue(2);
        m_centerLineV->setAcceptedMouseButtons(Qt::NoButton);   // above trusses, but not in their way
        m_centerLineV->setVisible(m_centerLinesVisible && !centreLineReplacesCross());
        m_centerLineH = m_scene->addLine(m_xOffset, cy, m_xOffset + gridW, cy, centerPen);
        m_centerLineH->setZValue(2);
        m_centerLineH->setAcceptedMouseButtons(Qt::NoButton);
        m_centerLineH->setVisible(m_centerLinesVisible);

        if (m_bgItem != NULL)
        {
            m_bgItem->setX(m_xOffset);
            m_bgItem->setY(m_yOffset);
            m_bgItem->setPixmap(m_bgPixmap.scaled(xPos - m_cellPixels - m_xOffset, yPos - m_cellPixels - m_yOffset));
        }

        // The grid geometry (cell pixels/offsets) is always computed above so
        // items position correctly; the LINES can be hidden independently.
        foreach (QGraphicsItem *gi, m_gridItems)
            gi->setVisible(m_gridVisible);
    }

    /* Match the scene rect to the viewport so that, once zoomed in, the view
       has scroll range — which is what makes wheel-zoom anchor under the cursor
       and Shift-drag panning work.
     *
       UNIONED with whatever the items actually occupy, because a QGraphicsView
       cannot scroll past its scene rect: anything placed outside the grid was
       not merely off-screen, it was unreachable — impossible to see, select or
       drag back, recoverable only by editing the .qxw by hand.
       stage-structures-demo.qxw has two fixtures stranded 13.8 m upstage
       exactly like that, left behind when they were detached from a truss.

       Costs nothing in the ordinary case: with everything on the grid the
       items' bounds sit inside the viewport and the union is the viewport. */
    growSceneRectToItems();

    updateImages();
    updateTrusses();
    updatePlatforms();
    updatePowerSources();
    updateNetworkNodes();
    updateTargets();
}

void MonitorGraphicsView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    m_fittedSize = size();
    m_fittedGrid = m_gridExtent;
    m_fittedUnit = m_unitValue;
    updateGrid();
    refreshAllItems();             // the follow spot's overlay with it
    centerOnGrid();
    emit rulersChanged();
}

void MonitorGraphicsView::centerOnGrid()
{
    /* The grid is fitted to the view, but anything placed off it (a house
       light, a fixture never placed) grows the scene beyond it, and the
       view then kept whatever scroll it had: the stage sat off to one side
       ("why is the grid not centered?"). Centre on the stage. */
    if (m_cellPixels <= 0 || !m_gridEnabled)
        return;
    centerOn(QPointF(m_xOffset + m_cellPixels * m_gridExtent.width() / 2.0,
                     m_yOffset + m_cellPixels * m_gridExtent.height() / 2.0));
}

void MonitorGraphicsView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    if (m_showOrientationLabels)
        viewport()->update();
}

void MonitorGraphicsView::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_shapeDragHandle != StageShape::NoHandle && e->button() == Qt::LeftButton)
    {
        shapeDragEnd(e->modifiers());
        e->accept();
        return;
    }
    if (m_stringDragIndex >= 0 && e->button() == Qt::LeftButton)
    {
        stringDragEnd();
        e->accept();
        return;
    }
    if (m_addAnchorFid != UINT_MAX && e->button() == Qt::LeftButton)
    {
        e->accept();            // the press placed an anchor; nothing to select
        return;
    }
    if (m_zoneDragId != UINT_MAX && e->button() == Qt::LeftButton)
    {
        const quint32 zid = m_zoneDragId;
        m_zoneDragId = UINT_MAX;
        const QVector3D a = unprojectMm(m_zoneDragStart, QVector3D());
        const QVector3D b = unprojectMm(mapToScene(e->pos()), QVector3D());
        const QVector3D d = (b - a) / 1000.0f;          // mm to stage metres
        foreach (ZoneItem *zi, m_zoneItems)
            if (zi->id() == zid)
                zi->setPos(0, 0);
        // The axes this view shows: Top x,y; Front x,z; Side y,z.
        const float dx = m_pov == PovSide ? 0.0f : d.x();
        const float dy = m_pov == PovFront ? 0.0f : d.y();
        const float dz = m_pov == PovTop ? 0.0f : d.z();
        if ((e->pos() - m_pressViewPos).manhattanLength() >= QApplication::startDragDistance()
            && (dx != 0.0f || dy != 0.0f || dz != 0.0f))
            emit zoneMoved(zid, dx, dy, dz);
        e->accept();
        return;
    }

    // The end of a right-press: the context menu has it. Not a click, not a drop.
    if (e->button() == Qt::RightButton)
    {
        e->accept();
        return;
    }

    // Click again (see mousePressEvent): after the release has done all else.
    QGraphicsItem *pickOut = nullptr;
    if (e->button() == Qt::LeftButton && m_clickAgainItem != nullptr
        && (e->pos() - m_clickAgainPos).manhattanLength() < QApplication::startDragDistance())
        pickOut = m_clickAgainItem;
    m_clickAgainItem = nullptr;
    auto clickAgain = qScopeGuard([this, pickOut]() { clickAgainToPickOut(pickOut); });

    // Everything this release records is one drag: one Ctrl+Z undoes it all.
    m_openUndoBatch = ++m_undoBatch;
    auto closeBatch = qScopeGuard([this]() { m_openUndoBatch = 0; });

    // Capture which trusses the cursor was hovering over at release time.
    // We check BOTH the last-move highlight state AND the cursor position at
    // release (mouse events can be coalesced, so the last moveEvent may not
    // represent the exact drop point — especially for tiny vertical towers).
    m_droppedOnTrussIds.clear();
    QPointF relScenePos = mapToScene(e->pos());
    for (TrussItem *ti : m_trussItems)
    {
        bool hit = ti->isHighlighted();
        if (!hit)
        {
            QPointF local = ti->mapFromScene(relScenePos);
            if (ti->truss()->type() == Truss::Vertical)
            {
                float threshold = qMax(ti->pxWid() * 2.5f, 30.0f);
                float dist = float(qSqrt(local.x() * local.x() + local.y() * local.y()));
                hit = dist <= threshold;
            }
            else
            {
                hit = ti->contains(local);
            }
        }
        if (hit)
            m_droppedOnTrussIds.insert(ti->trussId());
    }

    // Clear any truss drop-target highlights.
    for (TrussItem *ti : m_trussItems)
        ti->setHighlighted(false);

    /* Held still: nothing is stored -- not by the item's own drop signal
       (several kinds emit it on every release, drag or not) and not by the
       sweep below. */
    const bool refused = m_dragRefused && e->button() == Qt::LeftButton;
    m_dragRefused = false;
    m_pressOnLockedLayer = false;
    QObject *heldGrabber = (refused && m_scene != nullptr && m_scene->mouseGrabberItem() != nullptr)
        ? dynamic_cast<QObject *>(m_scene->mouseGrabberItem()->topLevelItem()) : nullptr;

    // The release that follows a mouseDoubleClickEvent must not emit viewClicked
    // (that would immediately close the editor the double-click just opened).
    if (m_suppressNextViewClick)
    {
        m_suppressNextViewClick = false;
        {
            const QSignalBlocker quiet(heldGrabber);
            QGraphicsView::mouseReleaseEvent(e);
        }
        if (dragMode() == QGraphicsView::ScrollHandDrag)
            setDragMode(QGraphicsView::RubberBandDrag);
        return;
    }

    emit viewClicked(e);

    {
        const QSignalBlocker quiet(heldGrabber);
        QGraphicsView::mouseReleaseEvent(e);
    }

    // A multi-selection drag only delivers mouse press/move/release to the
    // ONE item Qt actually grabbed -- every co-selected item just moved on
    // screen by Qt's own internal handling, without ever getting its own
    // mouseReleaseEvent. The line above just fired exactly one kind's
    // itemDropped()/slotXMoved (whichever kind the grabbed item was), which
    // persists every co-selected item of THAT SAME kind -- each slot already
    // rebuilds its kind's full moved-list from the live selection. But a
    // MIXED-KIND selection (e.g. fixtures dragged together with trusses)
    // never fires any slot for the kinds that weren't the grabbed item's
    // own, so they silently keep their old saved position and snap back on
    // the next reload, even though they visibly moved during the drag.
    //
    // Sweep every kind present in the current selection and invoke its slot
    // directly (a plain call, not through the itemDropped signal) with one
    // arbitrary item of that kind as the anchor -- each slot's own
    // selectedItems() rebuild takes it from there and persists the whole
    // group. Redundantly re-invoking the kind that already got a real
    // itemDropped is harmless: every slot compares the new position against
    // what it just stored, so a second call computes a zero delta and writes
    // nothing new.
    //
    // Only when an actual drag happened, though -- gated the same way each
    // item's own mouseReleaseEvent gates its itemDropped emit (m_dragMoved),
    // otherwise a bare click to select something would run every selected
    // kind's slot for no reason, needlessly marking the document modified.
    const bool wasDrag = (e->pos() - m_pressViewPos).manhattanLength()
                          >= QApplication::startDragDistance();
    if (wasDrag && !refused && m_scene != nullptr)
    {
        /* Guarded: a slot below, or whatever hears what it emits, may rebuild
           the plot's items (a resize or a blocking change runs updateTargets()
           and refreshAllItems()), and a raw pointer to the next rep would be
           left pointing at a freed item. */
        QPointer<MonitorFixtureItem> repFixture;
        QPointer<TrussItem> repTruss;
        QPointer<PlatformItem> repPlatform;
        QPointer<PipeItem> repPipe;
        QPointer<StandItem> repStand;
        QPointer<SceneryItem> repScenery;
        QPointer<TowerItem> repTower;
        QPointer<PowerSourceItem> repPower;
        QPointer<NetworkNodeItem> repNetwork;
        QPointer<MonitorImageItem> repImage;
        QPointer<TargetItem> repTarget;

        foreach (QGraphicsItem *gi, m_scene->selectedItems())
        {
            // Only what Qt actually moved: a held item rode along with nothing.
            if (!itemDraggable(gi))
                continue;
            if (repFixture.isNull())  repFixture  = dynamic_cast<MonitorFixtureItem *>(gi);
            if (repTruss.isNull())    repTruss    = dynamic_cast<TrussItem *>(gi);
            if (repPlatform.isNull()) repPlatform = dynamic_cast<PlatformItem *>(gi);
            if (repPipe.isNull())     repPipe     = dynamic_cast<PipeItem *>(gi);
            if (repStand.isNull())    repStand    = dynamic_cast<StandItem *>(gi);
            if (repScenery.isNull())  repScenery  = dynamic_cast<SceneryItem *>(gi);
            if (repTower.isNull())    repTower    = dynamic_cast<TowerItem *>(gi);
            if (repPower.isNull())    repPower    = dynamic_cast<PowerSourceItem *>(gi);
            if (repNetwork.isNull())  repNetwork  = dynamic_cast<NetworkNodeItem *>(gi);
            if (repImage.isNull())    repImage    = dynamic_cast<MonitorImageItem *>(gi);
            if (repTarget.isNull())   repTarget   = dynamic_cast<TargetItem *>(gi);
        }

        if (!repFixture.isNull())  slotFixtureMoved(repFixture);
        if (!repTruss.isNull())    slotTrussMoved(repTruss);
        if (!repPlatform.isNull()) slotPlatformMoved(repPlatform);
        if (!repPipe.isNull())     slotPipeMoved(repPipe);
        if (!repStand.isNull())    slotStandMoved(repStand);
        if (!repScenery.isNull())  slotSceneryMoved(repScenery);
        if (!repTower.isNull())    slotTowerMoved(repTower);
        if (!repPower.isNull())    slotPowerSourceMoved(repPower);
        if (!repNetwork.isNull())  slotNetworkNodeMoved(repNetwork);
        if (!repImage.isNull())    slotImageMoved(repImage);
        if (!repTarget.isNull())   slotTargetMoved(repTarget);
        refreshFixtureCountBadges();   // moved fixtures change what covers what
    }

    // Restore rubber-band selection after a shift-drag pan.
    if (dragMode() == QGraphicsView::ScrollHandDrag)
        setDragMode(QGraphicsView::RubberBandDrag);

    // Selection may have changed — rebuild aim lines for selected targets.
    updateAimLines();
}

void MonitorGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_shapeDragHandle != StageShape::NoHandle && (event->buttons() & Qt::LeftButton))
    {
        // A stretch or rotate handle, dragged: past the drag distance, it shows what it makes.
        if (m_shapeDragMoved
            || (event->pos() - m_pressViewPos).manhattanLength() >= QApplication::startDragDistance())
            shapeDragTo(mapToScene(event->pos()), event->modifiers());
        emit cursorReadout(viewportToReadout(event->pos()));
        event->accept();
        return;
    }
    if (event->buttons() == Qt::NoButton && m_shapeHandles != nullptr && m_addAnchorFid == UINT_MAX)
    {
        // Over a handle: it can be grabbed.
        const ShapeHandles *sh = shapeHandles();
        const StageShape::Handle over = sh != nullptr ? sh->handleAt(mapToScene(event->pos()))
                                                      : StageShape::NoHandle;
        if ((over != StageShape::NoHandle) != m_shapeCursor)
        {
            m_shapeCursor = over != StageShape::NoHandle;
            if (m_shapeCursor)
                viewport()->setCursor(over == StageShape::RotateKnob ? Qt::OpenHandCursor
                                                                    : AppUtil::nativeCursorShape(Qt::SizeAllCursor));
            else
                viewport()->unsetCursor();
        }
    }
    if (m_stringDragIndex >= 0 && m_stringDragFid != UINT_MAX && (event->buttons() & Qt::LeftButton))
    {
        // A free string's anchor, dragged: past the drag distance, it moves.
        if (m_stringDragMoved
            || (event->pos() - m_pressViewPos).manhattanLength() >= QApplication::startDragDistance())
            stringDragTo(m_stringDragFid, m_stringDragIndex, mapToScene(event->pos()), event->modifiers());
        emit cursorReadout(viewportToReadout(event->pos()));
        event->accept();
        return;
    }
    if (event->buttons() == Qt::NoButton && m_addAnchorFid == UINT_MAX && !m_pickingOrigin)
    {
        // Over an anchor's handle: it can be grabbed.
        const bool over = stringHandleAt(mapToScene(event->pos())) >= 0;
        if (over != m_stringCursor)
        {
            m_stringCursor = over;
            if (over)
                viewport()->setCursor(Qt::OpenHandCursor);
            else
                viewport()->unsetCursor();
        }
    }
    if (m_zoneDragId != UINT_MAX)
    {
        // The zone follows the pointer; the show changes on release.
        const QPointF d = mapToScene(event->pos()) - m_zoneDragStart;
        foreach (ZoneItem *zi, m_zoneItems)
            if (zi->role() == ZoneItem::Zone && zi->id() == m_zoneDragId)
                zi->setPos(d);
        emit cursorReadout(viewportToReadout(event->pos()));
        event->accept();
        return;
    }
    /* A press on something held still: the pointer moves, nothing else does.
       Said once on the status line (Q178), and the move never reaches the
       scene, so no item thinks it was dragged. */
    if ((event->buttons() & Qt::LeftButton) && (m_dragRefused || m_pressOnLockedLayer)
        && !m_dragRefusedNoted
        && (event->pos() - m_pressViewPos).manhattanLength() >= QApplication::startDragDistance())
    {
        m_dragRefusedNoted = true;
        emit dragRefused(m_layoutLocked ? tr("Plot locked \u2014 Edit Plot to move")
                         : m_pressOnLockedLayer
                             ? tr("Layer locked \u2014 unlock it in Layers to move")
                         : isElevation() ? tr("Not movable in this view \u2014 move it in Top")
                                         : tr("Locked \u2014 unlock it to move"));
    }
    if (m_dragRefused && (event->buttons() & Qt::LeftButton))
    {
        emit cursorReadout(viewportToReadout(event->pos()));
        event->accept();
        return;
    }

    // While a fixture item is being dragged, highlight any truss the cursor hovers.
    if (event->buttons() & Qt::LeftButton)
    {
        bool draggingFixture = false;
        foreach (QGraphicsItem *gi, m_scene->selectedItems())
        {
            if (dynamic_cast<MonitorFixtureItem *>(gi))
            {
                draggingFixture = true;
                break;
            }
        }

        // If dragging a target, keep aim lines live.
        if (!draggingFixture && m_activeSceneId != Function::invalidId())
        {
            foreach (QGraphicsItem *gi, m_scene->selectedItems())
            {
                if (dynamic_cast<TargetItem *>(gi)) { updateAimLines(); break; }
            }
        }

        if (draggingFixture)
        {
            updateDragBorders();
        }
        else
        {
            // Not dragging a fixture — ensure no stale escape-mode borders remain
            foreach (QGraphicsItem *gi, m_scene->selectedItems())
            {
                if (auto *mfi = dynamic_cast<MonitorFixtureItem *>(gi))
                {
                    mfi->setEscapeMode(false);
                    mfi->setAttachMode(false);
                }
            }
        }
    }

    // Live coordinate readout for the footer / rulers (needs mouse tracking on).
    emit cursorReadout(viewportToReadout(event->pos()));

    QGraphicsView::mouseMoveEvent(event);
    if (event->buttons() & Qt::LeftButton)
    {
        dropLayerLockedFromRubberBand();
        if (!rubberBandRect().isNull())
            m_lastBand = mapToScene(rubberBandRect()).boundingRect();
    }
}

void MonitorGraphicsView::clickAgainToPickOut(QGraphicsItem *item)
{
    if (item == nullptr || item->scene() != m_scene)
        return;
    m_extendingSelection = true;            // not select-together: just this one
    m_scene->clearSelection();
    item->setSelected(true);
    if (MonitorFixtureItem *mfi = dynamic_cast<MonitorFixtureItem *>(item))
        mfi->setIsolated(true);
    m_extendingSelection = false;
    emit mapSelectionChanged();
}

void MonitorGraphicsView::mouseDoubleClickEvent(QMouseEvent *event)
{
    // A quick second click while laying a string is the next anchor.
    if (m_addAnchorFid != UINT_MAX && event->button() == Qt::LeftButton)
    {
        // Unless its press already placed one there (Qt sends both).
        const bool placed = m_lastAnchorAdd.isValid()
                            && m_lastAnchorAdd.elapsed() < QApplication::doubleClickInterval()
                            && (event->pos() - m_lastAnchorAddPos).manhattanLength() < 4;
        if (!placed)
            addStringAnchorAt(m_addAnchorFid, mapToScene(event->pos()));
        event->accept();
        return;
    }
    if (stringHandleAt(mapToScene(event->pos())) >= 0)
    {
        event->accept();
        return;
    }
    // Suppress the viewClicked that would fire on the release following this
    // double-click (which would immediately close whatever the double-click opened).
    m_suppressNextViewClick = true;

    QPointF sp = mapToScene(event->pos());

    /* A count bubble stands for the fixtures hidden under its structure, so
       double-clicking it shows them: selected, drawn over the structure, and
       with their beams' landing areas on the deck. Clicks pass THROUGH it
       otherwise (it takes no buttons), which is what lets the structure's own
       editor open from anywhere else on it. */
    for (auto bit = m_countBadges.constBegin(); bit != m_countBadges.constEnd(); ++bit)
    {
        QGraphicsItemGroup *badge = bit.value();
        if (badge == nullptr || !badge->isVisible() || !countBadgeHit(badge, event->pos()))
            continue;
        const QStringList key = bit.key().split(QLatin1Char(':'));
        if (key.size() == 2)
        {
            revealBuriedFixtures(key.at(0), key.at(1).toUInt());
            event->accept();
            return;
        }
    }

    /* A no-light zone takes no clicks of its own: double-click on bare plot
       inside one edits it. */
    if (topPickableAt(sp) == nullptr)
    {
        const quint32 zid = zoneAt(sp);
        if (zid != UINT_MAX)
        {
            emit zoneDoubleClicked(zid);
            event->accept();
            return;
        }
    }

    QGraphicsItem *it = topPickableAt(sp);

    // Walk up to the top-level item (e.g. from a label child to TrussItem).
    while (it && it->parentItem())
        it = it->parentItem();

    // Group drill-in for STRUCTURAL items: single-click selects the whole group
    // (select-together); the FIRST double-click drills IN — selects just this
    // member so it can be dragged alone. Only this item selected → the next
    // double-click opens its editor (drill-in then act). Ungrouped items open
    // the editor straight away.
    /* Double-click OPENS: the item's own editor, grouped or not (picking one
       member out of a group is "click again" -- see clickAgainToPickOut()).
       It used to drill in first, so a grouped item took two double-clicks
       and a fixture three ways to open (CONSOLIDATION_REVIEW #3). */
    auto drilledIntoGroup = [&](QGraphicsItem *) -> bool { return false; };

    /* An assembly (a fountain: base, walls, flame) is one thing to a click;
       a double-click goes into it and picks the one part, to move or edit
       on its own. Escape, or a click outside it, comes back out. Inside
       it, a double-click opens the part's editor as anywhere else. */
    if (const quint32 assembly = assemblyOfItem(it))
    {
        if (assembly != m_enteredAssembly)
        {
            m_enteredAssembly = assembly;
            m_extendingSelection = true;
            m_scene->clearSelection();
            it->setSelected(true);
            m_extendingSelection = false;
            emit mapSelectionChanged();
            event->accept();
            return;
        }
    }

    if (auto *fi = dynamic_cast<MonitorFixtureItem *>(it))
    {
        /* A fixture: its properties, wherever it is mounted. (Its structure
           opened instead when the fixture covered it; that is "Edit <the
           structure>…" in the fixture's menu now.) A selection the user made
           of several fixtures opens for all of them; a group's or a truss's
           worth selected together by one click opens for just this one. */
        /* Whatever was selected, this one among it: a group or a truss's
           worth too ("when I select several fixtures and edit fixture
           properties .. should apply to all of them"). Click again picks one
           out of a selection first, to edit it alone. */
        if (m_savedSelection.contains(fi) && m_savedSelection.size() > 1)
        {
            m_extendingSelection = true;
            m_scene->clearSelection();
            foreach (QGraphicsItem *gi, m_savedSelection)
                gi->setSelected(true);
            m_extendingSelection = false;
        }
        emit fixtureDoubleClicked(fi->fixtureID());
        return;
    }
    if (auto *ti = dynamic_cast<TrussItem *>(it))
    {
        if (drilledIntoGroup(ti)) return;
        emit trussDoubleClicked(ti->trussId());
        return;
    }
    if (auto *hgi = dynamic_cast<HangerItem *>(it))
    {
        emit hangerDoubleClicked(hgi->hangerId());
        return;
    }
    if (auto *pi = dynamic_cast<PlatformItem *>(it))
    {
        if (drilledIntoGroup(pi)) return;
        emit platformDoubleClicked(pi->platformId());
        return;
    }
    if (auto *spi = dynamic_cast<StagePieceItem *>(it))
    {
        emit stagePieceDoubleClicked(spi->piece());
        return;
    }
    if (auto *bi = dynamic_cast<PipeItem *>(it))
    {
        if (drilledIntoGroup(bi)) return;
        emit pipeDoubleClicked(bi->pipeId());
        return;
    }
    if (auto *si = dynamic_cast<StandItem *>(it))
    {
        if (drilledIntoGroup(si)) return;
        emit standDoubleClicked(si->standId());
        return;
    }
    if (auto *sc = dynamic_cast<SceneryItem *>(it))
    {
        if (drilledIntoGroup(sc)) return;
        emit sceneryDoubleClicked(sc->sceneryId());
        return;
    }
    if (auto *twi = dynamic_cast<TowerItem *>(it))
    {
        if (drilledIntoGroup(twi)) return;
        emit towerDoubleClicked(twi->towerId());
        return;
    }
    if (auto *tgi = dynamic_cast<TargetItem *>(it))
    {
        /* A line: double-click the curve to add a point there, a mid point
           to take it out; the centre mark (or right-click > Edit) opens the
           dialog. Point targets open it from anywhere, as always. */
        if (tgi->isLine())
        {
            const QPointF local = tgi->mapFromScene(sp);
            const bool editable = (tgi->flags() & QGraphicsItem::ItemIsMovable);
            const int h = tgi->lineHandleAt(local);
            double f = 0.0;
            if (editable && h > 0 && h < tgi->lineHandleCount() - 1)
            {
                slotLineRemovePoint(tgi, h);
                return;
            }
            if (h >= 0)
                return;             // an end: nothing to add or take out
            if (editable && !tgi->onCentreMark(local) && tgi->lineHitAt(local, &f))
            {
                slotLineInsertPoint(tgi, f);
                return;
            }
        }
        emit targetDoubleClicked(tgi->targetId());
        return;
    }
    if (auto *ii = dynamic_cast<MonitorImageItem *>(it))
    {
        emit imageDoubleClicked(ii->imageId());
        return;
    }
    if (auto *ps = dynamic_cast<PowerSourceItem *>(it))
    {
        emit powerDoubleClicked(quint32(ps->sourceIndex()));
        return;
    }
    if (auto *nn = dynamic_cast<NetworkNodeItem *>(it))
    {
        emit networkDoubleClicked(quint32(nn->nodeIndex()));
        return;
    }

    QGraphicsView::mouseDoubleClickEvent(event);
}

static const char *INVENTORY_FID_MIME = "application/x-qlc-fid";

void MonitorGraphicsView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(INVENTORY_FID_MIME))
        event->acceptProposedAction();
    else
        QGraphicsView::dragEnterEvent(event);
}

void MonitorGraphicsView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(INVENTORY_FID_MIME))
        event->acceptProposedAction();
    else
        QGraphicsView::dragMoveEvent(event);
}

void MonitorGraphicsView::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasFormat(INVENTORY_FID_MIME))
    {
        QGraphicsView::dropEvent(event);
        return;
    }
    QDataStream s(event->mimeData()->data(INVENTORY_FID_MIME));
    QList<quint32> fids;
    while (!s.atEnd())
    {
        quint32 fid = 0;
        s >> fid;
        if (fid != 0 || fids.isEmpty())
            fids << fid;
    }
    event->acceptProposedAction();
    if (!fids.isEmpty())
        emit fixturesDropped(fids, mapToScene(qlcEventPos(event)));
}

void MonitorGraphicsView::finishFixtureDrop(quint32 fid)
{
    /* From the Inventory: placing it IS the point, so dropped on a truss or
       a deck it goes there, Shift or not -- Shift is for moving what is on
       the plot already. */
    QScopedValueRollback<bool> attach(m_attachOnDrop, true);
    if (MonitorFixtureItem *item = m_fixtures.value(fid, nullptr))
        slotFixtureMoved(item);
}

void MonitorGraphicsView::contextMenuEvent(QContextMenuEvent *event)
{
    QPointF sp = mapToScene(event->pos());
    if (m_swallowContextMenu)
    {
        m_swallowContextMenu = false;   // the right-click that ended laying a string
        event->accept();
        return;
    }
    // A free string's anchor: its own menu.
    if (const int handle = stringHandleAt(sp); handle >= 0)
    {
        stringHandleMenu(handle, event->globalPos());
        event->accept();
        return;
    }
    QGraphicsItem *it = topPickableAt(sp);
    while (it && it->parentItem())
        it = it->parentItem();

    /* Which item was hit, as a kind and an id -- the same key the Layers list
       uses -- so both build the one menu. */
    QString kind;
    quint32 id = 0;
    if (auto *fi = dynamic_cast<MonitorFixtureItem *>(it))    { kind = QStringLiteral("fixture");  id = fi->fixtureID(); }
    else if (auto *ti = dynamic_cast<TrussItem *>(it))        { kind = QStringLiteral("truss");    id = ti->trussId(); }
    else if (auto *pi = dynamic_cast<PlatformItem *>(it))     { kind = QStringLiteral("platform"); id = pi->platformId(); }
    else if (auto *ii = dynamic_cast<MonitorImageItem *>(it)) { kind = QStringLiteral("image");    id = ii->imageId(); }
    else if (auto *bi = dynamic_cast<PipeItem *>(it))         { kind = QStringLiteral("pipe");     id = bi->pipeId(); }
    else if (auto *si = dynamic_cast<StandItem *>(it))        { kind = QStringLiteral("stand");    id = si->standId(); }
    else if (auto *sci = dynamic_cast<SceneryItem *>(it))     { kind = QStringLiteral("scenery");  id = sci->sceneryId(); }
    else if (auto *twi = dynamic_cast<TowerItem *>(it))       { kind = QStringLiteral("tower");    id = twi->towerId(); }
    else if (auto *hgi = dynamic_cast<HangerItem *>(it))      { kind = QStringLiteral("hanger");   id = hgi->hangerId(); }
    else if (auto *tgi = dynamic_cast<TargetItem *>(it))      { kind = QStringLiteral("target");   id = tgi->targetId(); }
    else if (auto *psi = dynamic_cast<PowerSourceItem *>(it)) { kind = QStringLiteral("power");    id = quint32(psi->sourceIndex()); }
    else if (auto *nni = dynamic_cast<NetworkNodeItem *>(it)) { kind = QStringLiteral("network");  id = quint32(nni->nodeIndex()); }
    else if (auto *spi = dynamic_cast<StagePieceItem *>(it))  { kind = QStringLiteral("stage");    id = quint32(spi->piece()); }

    if (kind.isEmpty())
    {
        const quint32 zid = zoneAt(sp);
        if (zid != UINT_MAX)
        {
            QMenu zm(this);
            {
                ContextMenu cm(&zm);
                cm.add(ContextMenu::Open, tr("Edit No-light Zone…"), [this, zid]() { emit zoneDoubleClicked(zid); },
                       QStringLiteral("plotEditZone"));
                cm.add(ContextMenu::Remove, tr("Delete No-light Zone"), [this, zid]() { emit zoneRemoveRequested(zid); },
                       QStringLiteral("plotDeleteZone"));
            }
            zm.exec(event->globalPos());
            return;
        }
        // Empty canvas → the generic add/paste menu.
        emit contextMenuRequested(sp);
        return;
    }

    MenuPlace place;
    place.selection = selectedMapObjects();
    QMenu menu(this);
    buildItemMenu(&menu, kind, id, place, sp);
    if (!menu.isEmpty())
        menu.exec(event->globalPos());
}

static QString itemNameOf(Doc *doc, const QString &kind, quint32 id);

QList<QPair<QString, quint32> > MonitorGraphicsView::selectedMapObjects() const
{
    QList<QPair<QString, quint32> > out;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
    {
        if (auto *fi = dynamic_cast<MonitorFixtureItem *>(gi))     out << qMakePair(QStringLiteral("fixture"), fi->fixtureID());
        else if (auto *ti = dynamic_cast<TrussItem *>(gi))         out << qMakePair(QStringLiteral("truss"), ti->trussId());
        else if (auto *pi = dynamic_cast<PlatformItem *>(gi))      out << qMakePair(QStringLiteral("platform"), pi->platformId());
        else if (auto *ii = dynamic_cast<MonitorImageItem *>(gi))  out << qMakePair(QStringLiteral("image"), ii->imageId());
        else if (auto *bi = dynamic_cast<PipeItem *>(gi))          out << qMakePair(QStringLiteral("pipe"), bi->pipeId());
        else if (auto *si = dynamic_cast<StandItem *>(gi))         out << qMakePair(QStringLiteral("stand"), si->standId());
        else if (auto *sc = dynamic_cast<SceneryItem *>(gi))       out << qMakePair(QStringLiteral("scenery"), sc->sceneryId());
        else if (auto *twi = dynamic_cast<TowerItem *>(gi))        out << qMakePair(QStringLiteral("tower"), twi->towerId());
        else if (auto *hgi = dynamic_cast<HangerItem *>(gi))       out << qMakePair(QStringLiteral("hanger"), hgi->hangerId());
        else if (auto *tgi = dynamic_cast<TargetItem *>(gi))       out << qMakePair(QStringLiteral("target"), tgi->targetId());
        else if (auto *psi = dynamic_cast<PowerSourceItem *>(gi))  out << qMakePair(QStringLiteral("power"), quint32(psi->sourceIndex()));
        else if (auto *nni = dynamic_cast<NetworkNodeItem *>(gi))  out << qMakePair(QStringLiteral("network"), quint32(nni->nodeIndex()));
        else if (auto *spi = dynamic_cast<StagePieceItem *>(gi))   out << qMakePair(QStringLiteral("stage"), quint32(spi->piece()));
    }
    return out;
}

/* A stage piece's menu: its properties, where it is, and showing, locking
   and Auto -- the same from the plot and from Plot layers. */
void MonitorGraphicsView::buildStagePieceMenu(QMenu *menu, int piece, bool inList)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr)
        return;
    typedef MonitorProperties MP;
    const MP::StagePiece sp = MP::StagePiece(piece);
    const bool design = (m_doc->mode() == Doc::Design);
    // The canon: the plot's own (where it is, shown, locked, auto) on top,
    // its editor below.
    ContextMenu cm(menu);
    // Its name in Title Case, as menus are ("Curtain line" → "Curtain Line").
    QStringList words = MP::stagePieceName(sp).split(QLatin1Char(' '));
    for (QString &w : words)
        if (!w.isEmpty())
            w[0] = w.at(0).toUpper();
    cm.add(ContextMenu::Open, tr("Edit %1…").arg(words.join(QLatin1Char(' '))),
           [this, piece]() { emit stagePieceDoubleClicked(piece); }, QStringLiteral("stagePieceProperties"));
    if (inList)
        menu->addAction(tr("Show on Map"), this, [this, piece]() { selectMapItem(QStringLiteral("stage"), quint32(piece)); });
    else
        menu->addAction(tr("Show in Plot Layers"), this, [this, piece]() {
            emit revealInLayersRequested(QStringLiteral("stage"), quint32(piece));
        });
    auto changed = [this]() {
        m_doc->setModified();
        updateStageMarks();
        emit stagePiecesChanged();
        emit mapStructureChanged();      // the Layers list's eye and lock
    };
    QAction *shown = menu->addAction(tr("Shown"));
    shown->setObjectName(QStringLiteral("stagePieceShown"));
    shown->setCheckable(true);
    shown->setChecked(props->stagePieceVisible(sp));
    connect(shown, &QAction::toggled, this, [props, sp, changed](bool on) {
        props->setStagePieceVisible(sp, on);
        changed();
    });
    QAction *lock = menu->addAction(tr("Locked"));
    lock->setObjectName(QStringLiteral("stagePieceLocked"));
    lock->setCheckable(true);
    lock->setChecked(props->stagePieceLocked(sp));
    lock->setEnabled(design);
    connect(lock, &QAction::toggled, this, [props, sp, changed](bool on) {
        props->setStagePieceLocked(sp, on);
        changed();
    });
    if (sp != MP::CentrePiece)
    {
        const bool isAuto = (sp == MP::CurtainPiece) ? props->curtainLineIsAuto()
                          : (sp == MP::NumberPiece) ? props->numberLineIsAuto() : props->apronAuto();
        QAction *autoAct = menu->addAction(sp == MP::ApronPiece ? tr("Auto Depth (to the Deck's Front)")
                                                               : tr("Auto Position"));
        autoAct->setObjectName(QStringLiteral("stagePieceAuto"));
        autoAct->setCheckable(true);
        autoAct->setChecked(isAuto);
        autoAct->setEnabled(design);
        connect(autoAct, &QAction::toggled, this, [this, props, sp, changed](bool on) {
            const QByteArray before = m_rigSnapshotter ? m_rigSnapshotter() : QByteArray();
            if (sp == MP::CurtainPiece)
            {
                if (on) props->setCurtainLineAuto(); else props->setCurtainLineY(props->curtainLineY());
            }
            else if (sp == MP::NumberPiece)
            {
                if (on) props->setNumberLineAuto(); else props->setNumberLineY(props->numberLineY());
            }
            else
                props->setApronAuto(on);
            if (!before.isEmpty())
                pushRigUndo(before);
            changed();
        });
    }
    ContextMenu::fileAsPlace(menu);
    cm.finish();
}

void MonitorGraphicsView::buildItemMenu(QMenu *menu, const QString &kind, quint32 id,
                                        const MenuPlace &place, const QPointF &scenePos)
{
    if (menu == nullptr)
        return;
    MonitorProperties *props = m_doc->monitorProperties();
    const bool design = (m_doc->mode() == Doc::Design);

    if (kind == QLatin1String("stage"))
    {
        buildStagePieceMenu(menu, int(id), place.inList);
        return;
    }

    /* Laid out by the canon (docs/design/CONTEXT_MENUS.md): what only the
       plot does with it on top -- where it is, its mount, its locks, its
       folder and layer -- then the thing's own menu, the same wherever it
       is right-clicked: a fixture's is ContextMenus::rig, as in the
       Fixtures tab, Build and the Show Tree. */
    ContextMenu cm(menu);
    m_menuSelection = place.selection;     // what menuObjects() acts on, to the end
    auto forget = qScopeGuard([this]() { m_menuSelection.clear(); });
    populateItemMenu(menu, cm, kind, id, scenePos);

    if (place.inList)
        menu->addAction(tr("Show on Map"), this, [this, kind, id]() {
            selectMapItem(kind, id);
        });
    else
        menu->addAction(tr("Show in Plot Layers"), this, [this, kind, id]() {
            emit revealInLayersRequested(kind, id);
        });

    // Organising: the selection where the menu was opened, and this item.
    QList<QPair<QString, quint32> > objects = place.selection;
    if (!objects.contains(qMakePair(kind, id)))
        objects.prepend(qMakePair(kind, id));
    QList<QAction *> selectionItems;    // Create Fixture Group…: the canon's, under Selection ▸
    populateOrganiseMenu(menu, objects, &selectionItems);
    QList<quint32> fixtures;
    for (const auto &o : objects)
        if (o.first == QLatin1String("fixture") && !fixtures.contains(o.second))
            fixtures << o.second;

    if (kind == QLatin1String("fixture"))
    {
        // A selection with things that lock: lock them all from here too.
        if (menuObjects(kind, id).size() > 1)
            addLockAction(menu, kind, id);
        // Taking it off the plot (it stays in the show): this place's own.
        menu->addAction(tr("Remove from View"), this, [this, id]() {
            requestDeleteItem(QStringLiteral("fixture"), id);
        })->setObjectName(QStringLiteral("plotRemoveFromView"));
        ContextMenu::fileAsPlace(menu);

        // ---- The fixture's own menu: the canon's (ContextMenus::rig) ----
        if (!fixtures.contains(id))
            fixtures = { id };
        ContextMenus::RigPlace rig;
        rig.parent = this;
        rig.editable = design;
        rig.properties = [this](const QList<quint32> &ids) {
            if (ids.size() == 1)
                emit fixtureDoubleClicked(ids.first());
            else if (Monitor *mon = Monitor::instance())
                mon->editFixturesProperties(ids);
        };
        // Every fixture of the same model and mode on the plot -- all the
        // step bars, all the XL-450s -- to patch, move or group together.
        {
            rig.similarCount = qMax(1, int(similarObjects(kind, id).size()));
            rig.selectSimilar = [this, kind, id]() { selectSimilarTo(kind, id); };
        }
        rig.patch = [this](const QList<quint32> &ids) {
            PatchInventoryDialog dlg(m_doc, ids, this);
            if (dlg.exec() != QDialog::Accepted)
                return;
            foreach (quint32 f, ids)
                updateFixture(f);
            emit mapStructureChanged();
        };
        rig.createGroup = [this, fixtures]() {
            if (CreateFixtureGroup::createFromFixtures(m_doc, fixtures, this) != nullptr)
                emit mapStructureChanged();
        };
        if (place.renameInPlace)
        {
            const std::function<void()> inPlace = place.renameInPlace;
            rig.renameFixtureInPlace = [inPlace](quint32) { inPlace(); };
        }
        rig.changed = [this, fixtures]() {
            for (quint32 f : fixtures)
                updateFixture(f);
            emit mapStructureChanged();
        };
        ContextMenus::rig(cm, m_doc, QList<quint32>(), fixtures, rig);
        addSelectGroup(cm, kind, id);
        return;
    }

    // ---- A structure or a mark: its own menu ----
    ContextMenu::fileAsPlace(menu);
    if (selectionItems.size() > 0)
        for (QAction *a : selectionItems)
            cm.add(ContextMenu::Selection, a);
    /* Select All Similar, as for a fixture: every one of its kind -- the
       people of its set, the trusses of its type. */
    {
        const int similar = int(similarObjects(kind, id).size());
        if (similar > 0)
        {
            QAction *sim = cm.add(ContextMenu::Selection, ContextMenus::selectAllSimilarText(similar),
                                  [this, kind, id]() { selectSimilarTo(kind, id); },
                                  QStringLiteral("selectAllSimilar"));
            sim->setEnabled(similar > 1);           // 1: nothing like it but itself
        }
    }
    addSelectGroup(cm, kind, id);
    if (kind == QLatin1String("target"))
    {
        // The Aim palettes pointing at it, each to edit.
        QMenu *used = cm.addMenu(ContextMenu::Kind, ContextMenus::usedByText(), QStringLiteral("ctxUsedBy"));
        for (QLCPalette *pal : m_doc->palettes())
            if (pal != nullptr && pal->stageTargetId() == id)
            {
                const quint32 pid = pal->id();
                used->addAction(pal->name(), this, [pid]() { ProgrammingManager::openPalette(pid); });
            }
        if (used->isEmpty())
            used->addAction(tr("(not used)"))->setEnabled(false);
    }
    if (design)
    {
        /* A network node set up by hand can become a model to place the next
           one from (Add Network Node > Model). */
        if (kind == QLatin1String("network"))
            cm.add(ContextMenu::Kind, tr("Save as Gear Model…"), [this, id]() {
                NetworkTopology *nt = m_doc->networkTopology();
                if (int(id) >= nt->nodes().size())
                    return;
                const NetworkNode node = nt->nodes().at(int(id));
                QDialog dlg(this);
                dlg.setWindowTitle(tr("Save as Gear Model"));
                QFormLayout *form = new QFormLayout(&dlg);
                QLineEdit *make = new QLineEdit(&dlg);
                make->setPlaceholderText(tr("e.g. ENTTEC"));
                QLineEdit *model = new QLineEdit(node.model.isEmpty() ? node.name : node.model, &dlg);
                form->addRow(tr("Manufacturer:"), make);
                form->addRow(tr("Model:"), model);
                form->addRow(tr("Carries:"), new QLabel(tr("%1, %n port(s)", "", node.ports.size())
                                                      .arg(NetworkNode::transportName(node.transport)), &dlg));
                QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
                form->addRow(bb);
                connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
                connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
                if (dlg.exec() != QDialog::Accepted)
                    return;
                if (saveNetworkNodeAsModel(id, make->text(), model->text()).isEmpty())
                    QMessageBox::warning(this, tr("Save as Gear Model"),
                                         tr("The model could not be saved: it needs a name and at least one port."));
            }, QStringLiteral("plotSaveGearModel"));

        /* A truss or riser carrying fixtures has an auto-group, and with it a
           fixture layout and the option to break the group up. */
        quint32 anchorGroup = 0;
        foreach (const MonitorProperties::MonitorGroup &g, props->groups())
            if (g.anchorKind == kind && g.anchorId == id)
            {
                anchorGroup = g.id;
                break;
            }
        if (anchorGroup != 0)
        {
            const bool framed = props->group(anchorGroup).hasFrame;
            cm.add(ContextMenu::Kind, framed ? tr("Fixture Layout…") : tr("Create Fixture Layout…"),
                   [this, anchorGroup]() {
                openStudioGroupForGroup(anchorGroup);
                emit mapStructureChanged();
            }, QStringLiteral("plotFixtureLayout"));
            cm.add(ContextMenu::Kind, tr("Ungroup"), [this, anchorGroup]() { ungroupById(anchorGroup); },
                   QStringLiteral("plotUngroup"));
        }

        /* Duplicate: the copy half a metre on, as Cmd/Ctrl+D -- for this, or
           all of the selection it is part of. */
        static const QStringList copyable = { QStringLiteral("truss"), QStringLiteral("platform"),
            QStringLiteral("target"), QStringLiteral("scenery"), QStringLiteral("tower"),
            QStringLiteral("pipe"), QStringLiteral("stand") };
        if (copyable.contains(kind))
        {
            int n = 0;
            QList<QPair<QString, quint32> > dup;
            for (const auto &o : menuObjects(kind, id))
                if (copyable.contains(o.first))
                    dup << o;
            n = itemUnits(dup);
            cm.add(ContextMenu::Edit, n > 1 ? tr("Duplicate %1 Items").arg(n) : tr("Duplicate"), []() {
                if (Monitor *mon = Monitor::instance())
                    mon->duplicateSelected();
            }, QStringLiteral("plotDuplicate"))->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
        }

        const std::function<void()> inPlace = place.renameInPlace;
        // Inline in the list ("Rename"), a dialog on the plot ("Rename…").
        cm.add(ContextMenu::Edit, ContextMenus::renameText(!inPlace), [this, kind, id, inPlace]() {
            if (inPlace)
            {
                inPlace();
                return;
            }
            bool ok = false;
            const QString name = QInputDialog::getText(this, tr("Rename"), tr("Name:"),
                                                       QLineEdit::Normal, itemNameOf(m_doc, kind, id), &ok).trimmed();
            if (ok && !name.isEmpty() && renameItem(kind, id, name))
            {
                m_doc->setModified();
                emit mapStructureChanged();
            }
        }, QStringLiteral("plotRename"));
    }

    // Deleting it, last.
    addItemRemoval(cm, kind, id);
}

/* Select Group: a click takes one member; this takes the folder it is in,
   the same as clicking the folder's row in Plot layers. */
void MonitorGraphicsView::addSelectGroup(ContextMenu &cm, const QString &kind, quint32 id)
{
    const quint32 g = selectableGroupOf(kind, id);
    if (g == 0)
        return;
    const QString name = m_doc->monitorProperties()->group(g).name;
    cm.add(ContextMenu::Selection,
           name.isEmpty() ? tr("Select Group") : tr("Select Group “%1”").arg(name),
           [this, kind, id]() { selectGroupOf(kind, id); }, QStringLiteral("plotSelectGroup"));
}

/* An item's name, for prefilling Rename. */
static QString itemNameOf(Doc *doc, const QString &kind, quint32 id)
{
    MonitorProperties *props = doc->monitorProperties();
    if (kind == QLatin1String("fixture"))  { Fixture *f = doc->fixture(id);          return f ? f->name() : QString(); }
    if (kind == QLatin1String("truss"))    { Truss *t = props->truss(id);            return t ? t->name() : QString(); }
    if (kind == QLatin1String("platform")) { StagePlatform *p = props->platform(id); return p ? p->name() : QString(); }
    if (kind == QLatin1String("pipe"))     { Pipe *b = props->pipe(id);              return b ? b->name() : QString(); }
    if (kind == QLatin1String("stand"))    { Stand *st = props->stand(id);           return st ? st->name() : QString(); }
    if (kind == QLatin1String("scenery"))  { Scenery *sc = props->scenery(id);       return sc ? sc->name() : QString(); }
    if (kind == QLatin1String("tower"))    { Tower *t = props->tower(id);            return t ? t->name() : QString(); }
    if (kind == QLatin1String("target"))   { StageTarget *t = props->stageTarget(id); return t ? t->name() : QString(); }
    if (kind == QLatin1String("image"))    return props->hasImage(id) ? props->image(id).name : QString();
    if (kind == QLatin1String("power"))
    {
        PowerDistribution *pd = doc->powerDistribution();
        return (pd != nullptr && int(id) < pd->sources().size()) ? pd->sources().at(int(id)).name : QString();
    }
    if (kind == QLatin1String("network"))
    {
        NetworkTopology *nt = doc->networkTopology();
        return (nt != nullptr && int(id) < nt->nodes().size()) ? nt->nodes().at(int(id)).name : QString();
    }
    return QString();
}

QString MonitorGraphicsView::saveNetworkNodeAsModel(quint32 nodeIndex, const QString &manufacturer,
                                                    const QString &model)
{
    NetworkTopology *nt = m_doc->networkTopology();
    if (nt == nullptr || int(nodeIndex) >= nt->nodes().size())
        return QString();
    NetworkNode &node = nt->nodes()[int(nodeIndex)];
    NetworkGearModel m;
    m.manufacturer = manufacturer.trimmed();
    m.model = model.trimmed();
    m.transport = node.transport;
    m.ports = node.ports.size();
    m.builtIn = false;
    const QString path = NetworkGearCatalog::saveUserModel(m);
    if (!path.isEmpty() && node.model != m.displayName())
    {
        node.model = m.displayName();          // it IS that gear now
        m_doc->setModified();
    }
    return path;
}

bool MonitorGraphicsView::renameItem(const QString &kind, quint32 id, const QString &name)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (kind == QLatin1String("fixture"))
    {
        if (AppUtil::refuseTakenFixtureName(this, m_doc, id, name))
            return false;
        if (Fixture *f = m_doc->fixture(id)) f->setName(name);
        updateFixture(id);
    }
    else if (kind == QLatin1String("truss"))
    {
        if (Truss *t = props->truss(id)) t->setName(name);
        updateTrusses();
    }
    else if (kind == QLatin1String("platform"))
    {
        if (StagePlatform *p = props->platform(id)) p->setName(name);
        updatePlatforms();
    }
    else if (kind == QLatin1String("power"))
    {
        PowerDistribution *pd = m_doc->powerDistribution();
        if (pd != nullptr && int(id) < pd->sources().size()) pd->sources()[int(id)].name = name;
        updatePowerSources();
    }
    else if (kind == QLatin1String("network"))
    {
        NetworkTopology *nt = m_doc->networkTopology();
        if (nt != nullptr && int(id) < nt->nodes().size()) nt->nodes()[int(id)].name = name;
        updateNetworkNodes();
    }
    else if (kind == QLatin1String("image"))
    {
        if (props->hasImage(id))
        {
            MonitorProperties::MonitorImage img = props->image(id);
            img.name = name;
            props->setImage(img);
        }
        updateImages();
    }
    else if (kind == QLatin1String("pipe"))
    {
        if (Pipe *b = props->pipe(id)) b->setName(name);
        updatePlatforms();   // pipes/stands/towers redraw here
    }
    else if (kind == QLatin1String("hanger"))
    {
        if (Hanger *hg = props->hanger(id)) hg->setName(name);
    }
    else if (kind == QLatin1String("scenery"))
    {
        if (Scenery *sc = props->scenery(id)) sc->setName(name);
        updatePlatforms();
    }
    else if (kind == QLatin1String("stand"))
    {
        if (Stand *st = props->stand(id)) st->setName(name);
        updatePlatforms();
    }
    else if (kind == QLatin1String("tower"))
    {
        if (Tower *t = props->tower(id)) t->setName(name);
        updatePlatforms();
    }
    else if (kind == QLatin1String("target"))
    {
        if (StageTarget *t = props->stageTarget(id)) t->setName(name);
        updateTargets();
    }
    return true;
}

void MonitorGraphicsView::removeFromFolder(const QList<QPair<QString, quint32> > &objects)
{
    if (objects.isEmpty())
        return;
    MonitorProperties *props = m_doc->monitorProperties();

    for (const QPair<QString, quint32> &o : objects)
    {
        if (o.first == QLatin1String("fixture"))
        {
            /* Keep the fixture exactly where it is: read its world position,
               then store it as a free position once the frame and mount are
               stripped, so nothing teleports. Any structure mount goes too, so
               auto-grouping cannot put it straight back -- riser and deck
               fixtures are re-grouped with their platform on every refresh,
               which is what "remove doesn't stick" was. */
            const quint32 id = o.second;
            const QVector3D w = props->fixtureRigPosition(id);          // metres
            FixtureRigProps rp = props->fixtureRigProps(id);
            rp.riserPlatformId = FixtureRigProps::invalidPlatformId();
            rp.deckPlatformId  = FixtureRigProps::invalidPlatformId();
            rp.trussId         = Truss::invalidId();
            rp.groupLocal      = QVector3D();
            props->setFixtureRigProps(id, rp);
            props->setFixtureGroup(id, 0);
            props->setFixturePosition(id, 0, 0, QVector3D(w.x() * 1000.0f, w.y() * 1000.0f, w.z()));
            updateFixture(id);
        }
        else
        {
            QList<QPair<QString, quint32> > one;
            one << o;
            reparentToLayer(one, props->activeLayerId());
        }
    }
    m_doc->setModified();
    refreshItemLayerState();
    emit mapStructureChanged();
}

void MonitorGraphicsView::populateOrganiseMenu(QMenu *menu,
                                               const QList<QPair<QString, quint32> > &objects,
                                               QList<QAction *> *selectionItems)
{
    if (menu == nullptr || objects.isEmpty() || m_doc->mode() != Doc::Design)
        return;
    MonitorProperties *props = m_doc->monitorProperties();
    if (!menu->isEmpty())
        menu->addSeparator();

    /* Groups: the plot's own (folders in Plot layers) -- a new one, one
       already on the layer, or out of the one they are in. */
    QMenu *groupMenu = menu->addMenu(QIcon(":/group.png"), tr("Group"));
    groupMenu->setObjectName(QStringLiteral("plotGroupMenu"));
    groupMenu->menuAction()->setObjectName(QStringLiteral("plotGroup"));
    fillGroupMenu(groupMenu, objects);

    // Fixture Groups: the lighting control groups, not folders.
    QList<quint32> fixtures;
    bool allFixtures = true;
    for (const auto &o : objects)
    {
        if (o.first == QLatin1String("fixture"))
            fixtures << o.second;
        else
            allFixtures = false;
    }
    if (!fixtures.isEmpty())
    {
        QList<FixtureGroup *> holding;
        foreach (FixtureGroup *fg, m_doc->fixtureGroups())
            for (quint32 fid : fixtures)
                if (fg != nullptr && fg->fixtureList().contains(fid))
                {
                    holding << fg;
                    break;
                }
        auto removeFrom = [this, fixtures](quint32 fgId) {
            if (FixtureGroup *fg = m_doc->fixtureGroup(fgId))
                for (quint32 fid : fixtures)
                    fg->resignFixture(fid);
            m_doc->setModified();
            emit mapStructureChanged();
        };
        if (holding.size() == 1)
        {
            const quint32 fgId = holding.first()->id();
            menu->addAction(tr("Remove from Fixture Group “%1”").arg(holding.first()->name()),
                            this, [removeFrom, fgId]() { removeFrom(fgId); });
        }
        else if (holding.size() > 1)
        {
            QMenu *from = menu->addMenu(tr("Remove from Fixture Group"));
            for (FixtureGroup *fg : holding)
            {
                const quint32 fgId = fg->id();
                from->addAction(fg->name(), this, [removeFrom, fgId]() { removeFrom(fgId); });
            }
        }
        if (allFixtures)
        {
            QAction *create = new QAction(tr("Create Fixture Group from Selection…"), menu);
            connect(create, &QAction::triggered, this, [this, fixtures]() {
                if (CreateFixtureGroup::createFromFixtures(m_doc, fixtures, this) != nullptr)
                    emit mapStructureChanged();
            });
            // On the plot it goes under Selection ▸ (Q201).
            if (selectionItems != nullptr)
                *selectionItems << create;
            else
                menu->addAction(create);
        }
    }

    // Layers.
    QMenu *moveMenu = menu->addMenu(tr("Move to Layer"));
    foreach (const MonitorProperties::MonitorLayer &lyr, props->layers())
    {
        const quint32 lid = lyr.id;
        moveMenu->addAction(lyr.name, this, [this, objects, lid]() {
            const QByteArray before = m_rigSnapshotter ? m_rigSnapshotter() : QByteArray();
            reparentToLayer(objects, lid);
            if (!before.isEmpty())
                pushRigUndo(before);            // one undo step for all of them
            emit mapStructureChanged();
        });
    }
    // A layer of the plot, not of a cue: Selection ▸ New Layer from
    // Selection makes that.
    menu->addAction(tr("New Map Layer from Selection…"), this, [this, objects]() {
        MonitorProperties *p = m_doc->monitorProperties();
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("New Map Layer"), tr("Layer name:"),
                                                   QLineEdit::Normal,
                                                   tr("Layer %1").arg(p->layers().count()), &ok).trimmed();
        if (!ok || name.isEmpty())
            return;
        const quint32 lid = p->addLayer(name);
        p->setActiveLayerId(lid);
        reparentToLayer(objects, lid);
        m_doc->setModified();
        emit mapStructureChanged();
    });

    // Several at once: Lock N Items stands where one object's lock does
    // (addLockAction()).
}

static QString itemNoun(const QString &kind)
{
    if (kind == QLatin1String("truss"))    return MonitorGraphicsView::tr("Truss");
    if (kind == QLatin1String("platform")) return MonitorGraphicsView::tr("Platform");
    if (kind == QLatin1String("image"))    return MonitorGraphicsView::tr("Image");
    if (kind == QLatin1String("pipe"))     return MonitorGraphicsView::tr("Boom");
    if (kind == QLatin1String("hanger"))   return MonitorGraphicsView::tr("Hanger");
    if (kind == QLatin1String("stand"))    return MonitorGraphicsView::tr("Stand");
    if (kind == QLatin1String("scenery"))  return MonitorGraphicsView::tr("Scenery");
    if (kind == QLatin1String("tower"))    return MonitorGraphicsView::tr("Tower");
    if (kind == QLatin1String("target"))   return MonitorGraphicsView::tr("Target");
    if (kind == QLatin1String("power"))    return MonitorGraphicsView::tr("Power Source");
    if (kind == QLatin1String("network"))  return MonitorGraphicsView::tr("Network Node");
    return MonitorGraphicsView::tr("Fixture");
}

QList<QPair<QString, quint32> > MonitorGraphicsView::menuObjects(const QString &kind, quint32 id) const
{
    /* Right-clicking part of the selection acts on the selection; anything
       else has already been selected on its own by the right-press. */
    const QPair<QString, quint32> self(kind, id);
    if (m_menuSelection.size() > 1 && m_menuSelection.contains(self))
    {
        QList<QPair<QString, quint32> > out;
        for (const auto &o : m_menuSelection)
            if (o.first != QLatin1String("stage"))
                out << o;
        return out;
    }
    return { self };
}

int MonitorGraphicsView::itemUnits(const QList<QPair<QString, quint32> > &objects) const
{
    // An assembly's parts count as the one thing they are.
    QSet<quint32> assemblies;
    int n = 0;
    for (const auto &o : objects)
    {
        quint32 a = 0;
        if (o.first == QLatin1String("fixture"))
            a = m_doc->monitorProperties()->assemblyOf(m_doc->monitorProperties()->fixtureGroup(o.second));
        else
            a = assemblyOfItem(itemFor(o.first, o.second));
        if (a == 0)
            n++;
        else if (!assemblies.contains(a))
        {
            assemblies.insert(a);
            n++;
        }
    }
    return n;
}

void MonitorGraphicsView::addLockAction(QMenu *menu, const QString &kind, quint32 id)
{
    QList<QPair<QString, quint32> > lockable;
    for (const auto &o : menuObjects(kind, id))
        if (kindHasOwnLock(o.first))
            lockable << o;
    if (lockable.isEmpty())
        return;
    bool allLocked = true;
    for (const auto &o : lockable)
        allLocked = allLocked && itemOwnLocked(o.first, o.second);
    const int n = itemUnits(lockable);
    QString text;
    if (lockable.size() == 1 && lockable.first() == qMakePair(kind, id))
        text = allLocked ? tr("Unlock Position") : tr("Lock Position");
    else if (n == 1)
        text = allLocked ? tr("Unlock 1 Item") : tr("Lock 1 Item");
    else
        text = allLocked ? tr("Unlock %1 Items").arg(n) : tr("Lock %1 Items").arg(n);
    QAction *lk = menu->addAction(text, this, [this, lockable, allLocked]() {
        const QByteArray before = m_rigSnapshotter ? m_rigSnapshotter() : QByteArray();
        for (const auto &o : lockable)
            setItemLocked(o.first, o.second, !allLocked);
        if (!before.isEmpty())
            pushRigUndo(before);                // one undo step for all of them
    });
    lk->setObjectName(QStringLiteral("itemLock"));
    if (lockable.size() < menuObjects(kind, id).size())
        lk->setToolTip(tr("Fixtures have no lock of their own: lock their layer, or what they hang on"));
}

void MonitorGraphicsView::addItemRemoval(ContextMenu &cm, const QString &kind, quint32 id)
{
    if (kind == QLatin1String("power") || kind == QLatin1String("fixture"))
        return;
    /* Part of a selection: Delete takes the selection, as the toolbar's
       Remove does (one confirm naming the fixtures on them). */
    QList<QPair<QString, quint32> > removable;
    for (const auto &o : menuObjects(kind, id))
        if (o.first != QLatin1String("fixture") && o.first != QLatin1String("power")
            && o.first != QLatin1String("stage"))
            removable << o;
    if (removable.size() > 1)
    {
        cm.add(ContextMenu::Remove, tr("Delete %1 Items…").arg(itemUnits(removable)), []() {
            if (Monitor *mon = Monitor::instance())
                QMetaObject::invokeMethod(mon, "slotRemoveSelected");
        }, QStringLiteral("plotDeleteItem"));
        return;
    }
    const QString noun = itemNoun(kind);
    // A target asks first: aim palettes and scenes may depend on it.
    cm.add(ContextMenu::Remove, kind == QLatin1String("target") ? tr("Delete %1…").arg(noun) : tr("Delete %1").arg(noun),
           [this, kind, id]() { requestDeleteItem(kind, id); }, QStringLiteral("plotDeleteItem"));
}

void MonitorGraphicsView::populateItemMenu(QMenu *menu, ContextMenu &cm, const QString &kind, quint32 id,
                                           const QPointF &scenePos)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (menu == nullptr || props == nullptr)
        return;

    if (kind != QLatin1String("fixture"))
    {
        /* A structure or a mark: edit it, build on it, hold it in place,
           delete it. */
        const QString noun = itemNoun(kind);
        cm.add(ContextMenu::Open, tr("Edit %1…").arg(noun), [this, kind, id]() {
            requestEditItem(kind, id);
        }, QStringLiteral("plotEditItem"));

        /* Its like -- the other towers, the other step trusses, carrying the
           same fixtures -- patched as this one is ("the lights on T1, 2, 3, 4
           .. should all be addressed the same"). Opens the Patch view on all
           of them, matched, to look over before Apply. */
        const QString key = kind + QLatin1Char(':') + QString::number(id);
        const QStringList like = PatchPlanner::likeStructures(m_doc, key);
        if (!like.isEmpty())
        {
            cm.add(ContextMenu::Kind, tr("Patch Others Like This (%1)…").arg(like.size()),
                   [this, key, like]() { patchOthersLike(key, like); }, QStringLiteral("plotPatchOthersLike"));
        }
        /* Its whole setup onto the others of its kind (T1 to T2, T3): the
           fixtures, hung the same way, named, addressed and grouped. */
        if (!StructureDuplicate::targetsFor(m_doc, key).isEmpty())
        {
            cm.add(ContextMenu::Kind, tr("Duplicate Setup to…"), [this, key]() { duplicateSetupDialog(key); },
                   QStringLiteral("plotDuplicateSetup"));
        }

        /* One "Add" submenu per structure, rather than a handful of loose
           "Add X" entries at the top level: "the add section should probably
           have a sub menu of what can be added to keep things clean". What
           goes in it is whatever this kind of thing can actually carry. */
        QMenu *addMenu = nullptr;
        auto adds = [&]() -> QMenu * {
            if (addMenu == nullptr)
                addMenu = cm.addMenu(ContextMenu::Kind, tr("Add"), QStringLiteral("plotAddMenu"));
            return addMenu;
        };

        if (kind == QLatin1String("truss"))
        {
            Truss *t = props->truss(id);
            if (t == nullptr)
                return;
            /* At the click when there is one, else mid-span: from the list
               there is no "here". */
            float offset = t->length() * 0.5f;
            if (TrussItem *ti = m_trussItems.value(id, nullptr))
            {
                if (!scenePos.isNull() && t->type() != Truss::Vertical && ti->pxLen() > 0.0f)
                {
                    const QPointF delta = scenePos - ti->pos();
                    const double a = qDegreesToRadians(ti->rotation());
                    const double along = delta.x() * qCos(a) + delta.y() * qSin(a);
                    offset = float(qBound(0.0, along, double(ti->pxLen())) * t->length()
                                   / ti->pxLen());
                }
            }
            adds()->addAction(tr("Fixture…"), this, [this, id, offset]() {
                emit addFixtureToTrussRequested(id, offset);
            });
            adds()->addAction(tr("Bar…"), this, [this, id]() {
                emit addBarToTrussRequested(id, -1.0f);   // -1 = mid-span default
            });
        }
        else if (kind == QLatin1String("pipe"))
        {
            Pipe *b = props->pipe(id);
            if (b != nullptr && b->isVertical())
            {
                adds()->addAction(tr("Crossbar…"), this, [this, id]() {
                    emit addBarToPipeRequested(id);
                });
            }
        }

        /* Anything you can stand on can have things put on it -- "when I right
           click on a platform I get most of the platform type options .. but no
           menu to add anything like a fixture or target". The point clicked is
           the point they land on, so its deck height is theirs. */
        if (kind == QLatin1String("platform") || kind == QLatin1String("truss")
            || kind == QLatin1String("stand") || kind == QLatin1String("tower"))
        {
            const QPointF at = scenePos;
            // A truss already gets a click-position-aware, ACTUALLY-attaching
            // "Fixture…" action above (slotAddFixtureToTruss: reads the click
            // offset along the truss and sets trussId/trussOffset before
            // placing). This generic "Fixture Here…" never attaches to
            // anything at all -- MonitorGraphicsView::addFixture() has no
            // auto-snap-to-structure logic of its own, only grid-snap -- so
            // offering both on a truss meant one of the two silently produced
            // a free-floating fixture that just happened to sit near it,
            // under a name ("Here") that implied the opposite. Keep this one
            // for platform/stand/tower, which have no attaching equivalent
            // of their own.
            if (kind != QLatin1String("truss"))
            {
                adds()->addAction(tr("Fixture Here…"), this, [this, at]() {
                    emit addItemRequested(QStringLiteral("fixture"), at);
                });
            }
            adds()->addSeparator();
            adds()->addAction(tr("Target Here"), this, [this, at]() {
                emit addItemRequested(QStringLiteral("target"), at);
            });
            adds()->addAction(tr("Target Line Here"), this, [this, at]() {
                emit addItemRequested(QStringLiteral("targetline"), at);
            });
            adds()->addAction(tr("Person Here"), this, [this, at]() {
                emit addItemRequested(QStringLiteral("person"), at);
            });
            /* A riser full of people (a choir, a band): rows on every tread,
               as a set in the Performers layer. */
            if (kind == QLatin1String("platform"))
            {
                adds()->addAction(tr("People on the Risers…"), this, [this, at]() {
                    emit addItemRequested(QStringLiteral("people"), at);
                });
            }
        }

        /* What the things ON it light. Asking a truss is how you compare its
           heads' coverage without picking them out of the pile first. */
        QList<quint32> carried;
        structureCoverageByItsFixtures(kind, id, &carried);
        if (!carried.isEmpty())
        {
            menu->addSeparator();
            const bool on = structureFootprintsShown(kind, id);
            QAction *fp = menu->addAction(tr("Show Light Footprints"), this,
                                          [this, kind, id]() {
                setStructureFootprintsShown(kind, id, !structureFootprintsShown(kind, id));
            });
            fp->setCheckable(true);
            fp->setChecked(on);
            fp->setToolTip(tr("Draw where the light from each of the %n head(s) "
                              "on this lands", "", carried.count()));
        }

        menu->addSeparator();
        /* Two locks: this one thing -- or all of the selection it is part
           of -- and the layer it is on. */
        addLockAction(menu, kind, id);
        const bool layerLocked = m_doc->monitorProperties()->layer(objectLayerId(kind, id)).locked;
        const QString layerName = m_doc->monitorProperties()->layer(objectLayerId(kind, id)).name;
        menu->addAction(layerLocked ? tr("Unlock Layer “%1”").arg(layerName)
                                    : tr("Lock Layer “%1”").arg(layerName),
                        this, [this, kind, id, layerLocked]() {
            setItemLayerLocked(kind, id, !layerLocked);
        });
        /* The plot's lock is over both: either can be changed, but the
           object stays put until the plot is unlocked. */
        if (m_layoutLocked)
        {
            QAction *held = menu->addAction(tr("Held by the Plot Lock — Edit Plot to Move"));
            held->setObjectName(QStringLiteral("itemHeldByPlot"));
            held->setEnabled(false);
        }

        return;
    }

    /* ---- A fixture: edit and find it; mount it; point and place it; group
       it; take it off the plot. ---- */
    const quint32 fid = id;
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    Fixture *fxi = m_doc->fixture(fid);

    /* What a fixture is and does -- Fixture Properties…, LED Layout…,
       Locate, Patch…, Select All Similar, Gobo Wheels… -- is the canon's
       fixture menu (ContextMenus::rig), added by buildItemMenu() under
       these: what only the plot does with it. */
    /* Where this one lands, on its own -- no need to select it first, and it
       stays drawn while you look at something else. */
    {
        const bool fpOn = fixtureFootprintShown(fid);
        QAction *fp = menu->addAction(tr("Show Light Footprint"), this, [this, fid]() {
            setFixtureFootprintShown(fid, !fixtureFootprintShown(fid));
        });
        fp->setCheckable(true);
        fp->setChecked(fpOn);
        fp->setToolTip(tr("Draw where this head's beam lands, in Top view"));
        /* Tape over the front of the light: opens its Fixture Properties,
           where the Masking row is. */
        QAction *mask = menu->addAction(tr("Masking…"), this, [fid]() {
            if (Monitor *mon = Monitor::instance())
                mon->editFixturesProperties({ fid });
        });
        mask->setToolTip(tr("Tape off part of this light's beam or scan window: top, bottom, "
                            "left and right, in degrees"));
        /* A free string is laid on the plan: click where it goes. */
        if (rp.pixelPath == PixelPath::FreeString)
        {
            QAction *add = menu->addAction(tr("Add Anchor"), this, [this, fid]() { beginAddAnchors(fid); });
            add->setObjectName(QStringLiteral("plotAddStringAnchor"));
            add->setToolTip(tr("Click on the plan for each anchor after the last: where the string "
                               "is tied or runs through"));
            add->setEnabled(stringEditable(fid));
        }
    }

    // Mounting.
    menu->addSeparator();
    QMenu *mountMenu = menu->addMenu(tr("Mount on"));
    {
        /* Every selected fixture when this one is among them ("selecting
           multiple fixtures and using attach to truss still only moves 1"),
           as the Deck entry always did; each keeps its place along. */
        QList<quint32> mountIds;
        foreach (QGraphicsItem *gi, m_scene->selectedItems())
            if (MonitorFixtureItem *mfi = dynamic_cast<MonitorFixtureItem *>(gi))
                mountIds << mfi->fixtureID();
        if (!mountIds.contains(fid))
            mountIds = { fid };
        const QString many = mountIds.size() > 1 ? tr(" (%1 fixtures)").arg(mountIds.size()) : QString();

        QMenu *trussMenu = mountMenu->addMenu(tr("Truss") + many);
        const QList<Truss *> trusses = props->trusses();
        trussMenu->setEnabled(!trusses.isEmpty());
        for (Truss *t : trusses)
        {
            if (t == nullptr)
                continue;
            const quint32 tid = t->id();
            QAction *a = trussMenu->addAction(t->name().isEmpty() ? tr("Truss %1").arg(tid) : t->name(),
                                              this, [this, mountIds, tid]() {
                for (quint32 id : mountIds)
                    attachFixtureToTruss(id, tid);
            });
            a->setCheckable(true);
            a->setChecked(rp.trussId == tid);
        }

        QMenu *boomMenu = mountMenu->addMenu(tr("Boom") + many);
        const QList<Pipe *> pipes = props->pipes();
        boomMenu->setEnabled(!pipes.isEmpty());
        for (Pipe *b : pipes)
        {
            if (b == nullptr)
                continue;
            const quint32 bid = b->id();
            QAction *a = boomMenu->addAction(b->name().isEmpty() ? tr("Boom %1").arg(bid) : b->name(),
                                             this, [this, mountIds, bid]() {
                for (quint32 id : mountIds)
                    attachFixtureToPipe(id, bid);
            });
            a->setCheckable(true);
            a->setChecked(rp.pipeId == bid);
        }

        /* A hanger: a bracket and chain on a tower or truss -- named with what
           it hangs off, since "Hanger 3" alone says nothing. */
        QMenu *hangerMenu = mountMenu->addMenu(tr("Hanger") + many);
        const QList<Hanger *> hangers = props->hangers();
        hangerMenu->setEnabled(!hangers.isEmpty());
        for (Hanger *hg : hangers)
        {
            if (hg == nullptr)
                continue;
            QString on;
            if (hg->parentKind() == Hanger::OnTruss)
            {
                if (Truss *t = props->truss(hg->parentId())) on = t->name();
            }
            else if (Tower *tw = props->tower(hg->parentId()))
                on = tw->name();
            const quint32 hid = hg->id();
            QAction *a = hangerMenu->addAction(on.isEmpty() ? hg->name() : tr("%1 (on %2)").arg(hg->name(), on),
                                               this, [this, mountIds, hid]() {
                for (quint32 id : mountIds)
                    attachFixtureToHanger(id, hid);
            });
            a->setCheckable(true);
            a->setChecked(rp.hangerId == hid);
        }

        QMenu *towerMenu = mountMenu->addMenu(tr("Tower") + many);
        const QList<Tower *> towers = props->towers();
        towerMenu->setEnabled(!towers.isEmpty());
        for (Tower *tw : towers)
        {
            if (tw == nullptr)
                continue;
            const quint32 twid = tw->id();
            QMenu *spots = towerMenu->addMenu(tw->name().isEmpty() ? tr("Tower %1").arg(twid) : tw->name());
            QAction *top = spots->addAction(tr("Top of Tower"), this, [this, mountIds, twid]() {
                for (quint32 id : mountIds)
                    mountFixtureOnTower(id, twid, FixtureRigProps::TowerTop);
            });
            top->setCheckable(true);
            top->setChecked(rp.towerId == twid && rp.towerMountSide == FixtureRigProps::TowerTop);
            QAction *bottom = spots->addAction(tr("Bottom of Tower"), this, [this, mountIds, twid]() {
                for (quint32 id : mountIds)
                    mountFixtureOnTower(id, twid, FixtureRigProps::TowerBottom);
            });
            bottom->setCheckable(true);
            bottom->setChecked(rp.towerId == twid && rp.towerMountSide == FixtureRigProps::TowerBottom);
            spots->addSeparator();
            if (tw->shelfCount() == 0)
                spots->addAction(tr("(no shelves — add in the tower editor)"))->setEnabled(false);
            for (int si = 0; si < tw->shelfCount(); ++si)
            {
                QAction *a = spots->addAction(tr("Shelf %1").arg(si + 1), this, [this, mountIds, twid, si]() {
                    for (quint32 id : mountIds)
                        mountFixtureOnTower(id, twid, FixtureRigProps::TowerShelf, si);
                });
                a->setCheckable(true);
                a->setChecked(rp.towerId == twid && rp.towerMountSide == FixtureRigProps::TowerShelf
                              && rp.towerShelf == si);
            }
        }

        /* Stood on a deck -- every selected fixture at once ("how to place
           UST1-8 cleanly on the top of the platforms .. they shouldn't hang
           in space"): the deck under each, or one by name. */
        const QList<quint32> deckIds = mountIds;
        QMenu *deckMenu = mountMenu->addMenu(deckIds.size() > 1 ? tr("Deck (%1 fixtures)").arg(deckIds.size())
                                                                 : tr("Deck"));
        QList<StagePlatform *> decks = props->platforms();
        // The apron hidden: not offered (it is not drawn), though kept.
        for (int i = decks.size() - 1; i >= 0; --i)
        {
            if (decks.at(i) != nullptr && props->isHiddenApronPlatform(decks.at(i)->id()))
                decks.removeAt(i);
        }
        deckMenu->setEnabled(!decks.isEmpty());
        deckMenu->addAction(deckIds.size() > 1 ? tr("The Deck Under Each") : tr("The Deck Under It"),
                            this, [this, deckIds]() {
            standOnDeck(deckIds, FixtureRigProps::invalidPlatformId());
        });
        deckMenu->addSeparator();
        for (StagePlatform *pl : decks)
        {
            if (pl == nullptr)
                continue;
            const quint32 pid = pl->id();
            QAction *a = deckMenu->addAction(pl->name().isEmpty() ? tr("Platform %1").arg(pid) : pl->name(),
                                             this, [this, deckIds, pid]() { standOnDeck(deckIds, pid); });
            a->setCheckable(true);
            a->setChecked(rp.deckPlatformId == pid);
        }
    }

    // What it is mounted on: reach that, or come off it.
    QString mountKind, mountName;
    quint32 mountId = 0;
    if (Hanger *hg = rp.onHanger() ? props->hanger(rp.hangerId) : nullptr)
    { mountKind = QStringLiteral("hanger"); mountId = hg->id(); mountName = hg->name(); }
    else if (Truss *t = (rp.trussId != Truss::invalidId()) ? props->truss(rp.trussId) : nullptr)
    { mountKind = QStringLiteral("truss"); mountId = t->id(); mountName = t->name(); }
    else if (Pipe *b = (rp.pipeId != Pipe::invalidId()) ? props->pipe(rp.pipeId) : nullptr)
    { mountKind = QStringLiteral("pipe"); mountId = b->id(); mountName = b->name(); }
    else if (Tower *tw = (rp.towerId != Tower::invalidId()) ? props->tower(rp.towerId) : nullptr)
    { mountKind = QStringLiteral("tower"); mountId = tw->id(); mountName = tw->name(); }
    else if (StagePlatform *pl = rp.riserPlatformId != FixtureRigProps::invalidPlatformId()
                                     ? props->platform(rp.riserPlatformId)
                                 : rp.deckPlatformId != FixtureRigProps::invalidPlatformId()
                                     ? props->platform(rp.deckPlatformId) : nullptr)
    { mountKind = QStringLiteral("platform"); mountId = pl->id(); mountName = pl->name(); }
    if (!mountKind.isEmpty())
    {
        const QString label = mountName.isEmpty() ? itemNoun(mountKind) : mountName;
        menu->addAction(tr("Edit %1 “%2”…").arg(itemNoun(mountKind), label),
                        this, [this, mountKind, mountId]() {
            requestEditItem(mountKind, mountId);
        });
        menu->addAction(tr("Unmount from “%1”").arg(label), this, [this, fid]() {
            unmountFixture(fid);
        });
    }

    // Pointing and placing.
    menu->addSeparator();
    if (fxi != nullptr && fxi->channelNumber(QLCChannel::Pan, QLCChannel::MSB) != QLCChannel::invalid())
    {
        QMenu *facing = menu->addMenu(tr("Facing (pan zero)"));
        const QList<QPair<QString, float> > dirs = {
            { tr("Downstage (0°)"), 0.0f }, { tr("Stage Right (90°)"), 90.0f },
            { tr("Upstage (180°)"), 180.0f }, { tr("Stage Left (270°)"), 270.0f } };
        for (const auto &d : dirs)
        {
            const float deg = d.second;
            QAction *a = facing->addAction(d.first, this, [this, fid, deg]() {
                MonitorProperties *p = m_doc->monitorProperties();
                FixtureRigProps r = p->fixtureRigProps(fid);
                r.panZeroDir = deg;
                p->setFixtureRigProps(fid, r);
                if (MonitorFixtureItem *mfi = m_fixtures.value(fid, nullptr))
                    mfi->setFacing(deg);
                m_doc->setModified();
            });
            a->setCheckable(true);
            a->setChecked(qAbs(rp.panZeroDir - deg) < 0.5f);
        }
    }
    // In the show's units, as the plot's other dialogs.
    QAction *heightAct = menu->addAction(MonitorFixtureItem::mountingHeightText(props, fid),
                    this, [this, fid]() {
        if (!MonitorFixtureItem::askMountingHeight(this, m_doc->monitorProperties(), fid))
            return;
        m_doc->setModified();
        updateFixture(fid);
    });
    heightAct->setObjectName(QStringLiteral("plotSetMountingHeight"));

    // Grouping.
    menu->addSeparator();
    const quint32 frameGid = props->fixtureFrameGroup(fid);
    if (frameGid != 0)
        menu->addAction(tr("Edit Studio Group…"), this, [this, frameGid]() {
            openStudioGroupEditor(frameGid);
        });
    if (selectedFixtureCount() >= 2)
        menu->addAction(tr("Create Studio Group from Selection"), this, [this]() {
            const quint32 ng = createStudioGroupFromSelection();
            if (ng != 0)
                openStudioGroupEditor(ng);
        });
    menu->addAction(tr("Stamp Studio Component…"), this, [this, fid]() {
        if (selectedFixtureCount() == 0)
            selectMapItem(QStringLiteral("fixture"), fid);
        browseStudioComponents();
    });
    const quint32 gid = props->fixtureGroup(fid);
    if (gid != 0)
    {
        const bool gl = props->group(gid).locked;
        menu->addAction(gl ? tr("Unlock Group Position") : tr("Lock Group Position"),
                        this, [this, gid, gl]() {
            m_doc->monitorProperties()->setGroupLocked(gid, !gl);
            m_doc->setModified();
            refreshItemLayerState();
        });
    }

}

quint32 MonitorGraphicsView::objectLayerId(const QString &kind, quint32 id, bool *found) const
{
    if (kind == QLatin1String("fixture"))
    {
        if (found != nullptr)
            *found = m_doc->fixture(id) != nullptr;
        return m_doc->monitorProperties()->fixtureLayer(id);
    }
    QGraphicsItem *gi = itemFor(kind, id);
    if (found != nullptr)
        *found = gi != nullptr;
    return gi != nullptr ? const_cast<MonitorGraphicsView *>(this)->itemLayerId(gi) : 0;
}

bool MonitorGraphicsView::itemLocked(const QString &kind, quint32 id) const
{
    // Held by its own lock or by its layer's.
    bool found = false;
    const quint32 lid = objectLayerId(kind, id, &found);
    return found && (m_doc->monitorProperties()->layer(lid).locked || itemOwnLocked(kind, id));
}

quint32 MonitorGraphicsView::objectGroupId(const QString &kind, quint32 id) const
{
    if (kind == QLatin1String("fixture"))
        return m_doc->monitorProperties()->fixtureGroup(id);
    QGraphicsItem *gi = itemFor(kind, id);
    return gi != nullptr ? itemGroupId(gi) : 0;
}

int MonitorGraphicsView::itemHolds(const QString &kind, quint32 id) const
{
    bool found = false;
    const quint32 lid = objectLayerId(kind, id, &found);
    if (!found)
        return ItemLock::NotHeld;
    MonitorProperties *props = m_doc->monitorProperties();
    return ItemLock::holds(itemOwnLocked(kind, id), props->groupChainLocked(objectGroupId(kind, id)),
                           props->layer(lid).locked, m_layoutLocked);
}

bool MonitorGraphicsView::kindHasOwnLock(const QString &kind)
{
    return kind == QLatin1String("truss") || kind == QLatin1String("platform")
        || kind == QLatin1String("pipe") || kind == QLatin1String("stand")
        || kind == QLatin1String("tower") || kind == QLatin1String("target")
        || kind == QLatin1String("scenery") || kind == QLatin1String("image")
        || kind == QLatin1String("power") || kind == QLatin1String("network");
}

bool MonitorGraphicsView::itemOwnLocked(const QString &kind, quint32 id) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (kind == QLatin1String("truss"))    { const Truss *t = props->truss(id); return t != nullptr && t->locked(); }
    if (kind == QLatin1String("platform")) { const StagePlatform *p = props->platform(id); return p != nullptr && p->locked(); }
    if (kind == QLatin1String("pipe"))     { const Pipe *p = props->pipe(id); return p != nullptr && p->locked(); }
    if (kind == QLatin1String("stand"))    { const Stand *s = props->stand(id); return s != nullptr && s->locked(); }
    if (kind == QLatin1String("tower"))    { const Tower *t = props->tower(id); return t != nullptr && t->locked(); }
    if (kind == QLatin1String("target"))   { const StageTarget *t = props->stageTarget(id); return t != nullptr && t->locked(); }
    if (kind == QLatin1String("scenery"))  { const Scenery *s = props->scenery(id); return s != nullptr && s->locked(); }
    if (kind == QLatin1String("image"))    return props->hasImage(id) && props->image(id).locked;
    if (kind == QLatin1String("power"))
    {
        PowerDistribution *pd = m_doc->powerDistribution();
        return int(id) < pd->sources().size() && pd->sources().at(int(id)).locked;
    }
    if (kind == QLatin1String("network"))
    {
        NetworkTopology *nt = m_doc->networkTopology();
        return int(id) < nt->nodes().size() && nt->nodes().at(int(id)).locked;
    }
    return false;
}

void MonitorGraphicsView::setItemLocked(const QString &kind, quint32 id, bool locked)
{
    /* The object itself: held where it is, still selectable (so it can be
       unlocked again), drawn red. Its layer is not touched. */
    MonitorProperties *props = m_doc->monitorProperties();
    if (!kindHasOwnLock(kind) || itemOwnLocked(kind, id) == locked)
        return;
    bool done = true;
    if (kind == QLatin1String("truss"))         { if (Truss *t = props->truss(id)) t->setLocked(locked); else done = false; }
    else if (kind == QLatin1String("platform")) { if (StagePlatform *p = props->platform(id)) p->setLocked(locked); else done = false; }
    else if (kind == QLatin1String("pipe"))     { if (Pipe *p = props->pipe(id)) p->setLocked(locked); else done = false; }
    else if (kind == QLatin1String("stand"))    { if (Stand *st = props->stand(id)) st->setLocked(locked); else done = false; }
    else if (kind == QLatin1String("tower"))    { if (Tower *t = props->tower(id)) t->setLocked(locked); else done = false; }
    else if (kind == QLatin1String("target"))   { if (StageTarget *t = props->stageTarget(id)) t->setLocked(locked); else done = false; }
    else if (kind == QLatin1String("scenery"))  { if (Scenery *sc = props->scenery(id)) sc->setLocked(locked); else done = false; }
    else if (kind == QLatin1String("image"))
    {
        if (props->hasImage(id))
        {
            MonitorProperties::MonitorImage img = props->image(id);
            img.locked = locked;
            props->setImage(img);
        }
        else
        {
            done = false;
        }
    }
    else if (kind == QLatin1String("power"))
    {
        PowerDistribution *pd = m_doc->powerDistribution();
        if (int(id) < pd->sources().size())
            pd->sources()[int(id)].locked = locked;
        else
            done = false;
    }
    else if (kind == QLatin1String("network"))
    {
        NetworkTopology *nt = m_doc->networkTopology();
        if (int(id) < nt->nodes().size())
            nt->nodes()[int(id)].locked = locked;
        else
            done = false;
    }
    if (!done)
        return;
    m_doc->setModified();
    // The power and network items carry their lock: rebuilt with it.
    if (kind == QLatin1String("power"))
        updatePowerSources();
    else if (kind == QLatin1String("network"))
        updateNetworkNodes();
    refreshItemLayerState();
    if (QGraphicsItem *gi = itemFor(kind, id))
        gi->update();
    m_scene->update();
    emit mapStructureChanged();
}

void MonitorGraphicsView::setItemLayerLocked(const QString &kind, quint32 id, bool locked)
{
    /* Lock or unlock the layer it is on: everything on that layer with it.
       An unknown object is ignored -- it must not fall through to layer 0
       and lock the whole Default layer. */
    MonitorProperties *props = m_doc->monitorProperties();
    bool found = false;
    const quint32 lid = objectLayerId(kind, id, &found);
    if (!found || props->layer(lid).locked == locked)
        return;
    props->setLayerLocked(lid, locked);
    m_doc->setModified();
    refreshItemLayerState();
    m_scene->update();
    emit mapStructureChanged();
}

void MonitorGraphicsView::requestDeleteItem(const QString &kind, quint32 id)
{
    if (kind == QLatin1String("truss"))         emit trussRemoveRequested(id);
    else if (kind == QLatin1String("platform")) emit platformRemoveRequested(id);
    else if (kind == QLatin1String("image"))    emit imageRemoveRequested(id);
    else if (kind == QLatin1String("pipe"))     emit pipeRemoveRequested(id);
    else if (kind == QLatin1String("hanger"))   emit hangerRemoveRequested(id);
    else if (kind == QLatin1String("stand"))    emit standRemoveRequested(id);
    else if (kind == QLatin1String("scenery"))  emit sceneryRemoveRequested(id);
    else if (kind == QLatin1String("tower"))    emit towerRemoveRequested(id);
    else if (kind == QLatin1String("network"))  emit networkRemoveRequested(id);
    else if (kind == QLatin1String("target"))
    {
        if (TargetItem *ti = m_targetItems.value(id, nullptr))
            ti->confirmAndDelete();
    }
    else if (kind == QLatin1String("fixture"))
    {
        removeFixture(id);
        m_doc->setModified();
        emit mapStructureChanged();
    }
}

void MonitorGraphicsView::joinTowerGroup(quint32 fid, quint32 towerId)
{
    joinMountGroup(fid, QStringLiteral("tower"), towerId);
}

void MonitorGraphicsView::mountFixtureOnTower(quint32 fid, quint32 towerId, int side, int shelf)
{
    ensureFixtureOnPlot(fid);
    MonitorProperties *p = m_doc->monitorProperties();
    Tower *t = p->tower(towerId);
    if (t == nullptr)
        return;
    FixtureRigProps r = p->fixtureRigProps(fid);
    if (!(r.onTower() && r.towerId == towerId))
        leaveMountGroupsOf(fid);
    r.clearMounts();
    r.towerId = towerId;
    r.towerMountSide = side;
    if (side == FixtureRigProps::TowerShelf)
        r.towerShelf = shelf;
    else
        r.mountingType = Truss::FloorMounted;   // on the cap or the base; "hung" means nothing here
    r.towerU = t->width() * 0.5f;               // centred
    r.towerV = t->depth() * 0.5f;
    p->setFixtureRigProps(fid, r);
    updateFixture(fid);
    joinTowerGroup(fid, towerId);
    m_doc->setModified();
    emit mapStructureChanged();
    refreshItemLayerState();
}

void MonitorGraphicsView::ensureFixtureOnPlot(quint32 fid)
{
    if (m_fixtures.contains(fid) || m_doc->fixture(fid) == nullptr)
        return;
    MonitorProperties *props = m_doc->monitorProperties();
    const QVector3D w = props->fixtureRigPosition(fid);          // metres
    const QPointF mm(double(w.x()) * 1000.0, double(w.y()) * 1000.0);
    addFixture(fid, mm);
    props->setFixturePosition(fid, 0, 0, QVector3D(float(mm.x()), float(mm.y()), w.z()));
    props->setFixtureLayer(fid, props->placementLayerId());
    refreshFixtureLabels();
    m_doc->setModified();
}

void MonitorGraphicsView::unmountFixture(quint32 fid)
{
    MonitorProperties *p = m_doc->monitorProperties();
    const FixtureRigProps rp = p->fixtureRigProps(fid);
    if (rp.trussId != Truss::invalidId())
    {
        detachFixtureFromTruss(fid);
        return;
    }
    leaveMountGroupsOf(fid);
    FixtureRigProps r = rp;
    // Off a hanger: it stays where it hung.
    const QVector3D hungAt = rp.onHanger() ? p->fixtureRigPosition(fid) : QVector3D();
    r.pipeId = Pipe::invalidId();
    r.towerId = Tower::invalidId();
    r.hangerId = UINT_MAX;
    p->setFixtureRigProps(fid, r);
    if (rp.onHanger())
    {
        const QPointF mm(hungAt.x() * 1000.0, hungAt.y() * 1000.0);
        moveFixtureTo(fid, mm);
        emit fixtureMoved(fid, mm);
    }
    updateFixture(fid);
    m_doc->setModified();
    emit mapStructureChanged();
}

void MonitorGraphicsView::attachFixtureToPipe(quint32 fid, quint32 pipeId)
{
    ensureFixtureOnPlot(fid);
    MonitorProperties *props = m_doc->monitorProperties();
    Pipe *b = props->pipe(pipeId);
    if (b == nullptr)
        return;

    FixtureRigProps rp = props->fixtureRigProps(fid);
    if (rp.pipeId != pipeId)
        leaveMountGroupsOf(fid);
    // Boom mount is mutually exclusive with the other structural mounts.
    rp.clearMounts();   // ...including towerId, which this used to miss
    rp.pipeId = pipeId;
    if (rp.pipeOffset <= 0.0f)            // first mount → sit near the top
        rp.pipeOffset = b->height() * 0.85f;
    // On a VERTICAL boom a fixture can only clamp to the side, so default its
    // orientation to Sideways (base on side), not the generic default.
    if (b->isVertical())
        rp.mountingType = Truss::SideArm;
    props->setFixtureRigProps(fid, rp);

    updateFixture(fid);
    joinMountGroup(fid, QStringLiteral("pipe"), pipeId);
    m_doc->setModified();
    emit mapStructureChanged();
    refreshItemLayerState();
}

void MonitorGraphicsView::attachFixtureToHanger(quint32 fid, quint32 hangerId)
{
    ensureFixtureOnPlot(fid);
    MonitorProperties *props = m_doc->monitorProperties();
    if (props->hanger(hangerId) == nullptr)
        return;
    // One hook: whatever was on it comes off and stays where it was.
    const quint32 previous = props->fixtureOnHanger(hangerId);
    if (previous != UINT_MAX && previous != fid)
        unmountFixture(previous);

    FixtureRigProps rp = props->fixtureRigProps(fid);
    leaveMountGroupsOf(fid);
    rp.clearMounts();
    rp.hangerId = hangerId;
    rp.mountingType = Truss::TopHung;      // it hangs from the hook
    props->repairMountPlane(rp);           // and straight: no face left over
    props->setFixtureRigProps(fid, rp);
    updateFixture(fid);
    m_doc->setModified();
    emit mapStructureChanged();
    refreshItemLayerState();
}

void MonitorGraphicsView::attachFixtureToTruss(quint32 fid, quint32 trussId)
{
    // A fixture that has never been placed has no icon to snap; give it one.
    ensureFixtureOnPlot(fid);
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *t = props->truss(trussId);
    MonitorFixtureItem *mfi = m_fixtures.value(fid, nullptr);
    if (t == nullptr || mfi == nullptr || m_cellPixels == 0)
        return;

    FixtureRigProps rp = props->fixtureRigProps(fid);
    rp.clearMounts();   // the structural mounts are mutually exclusive
    rp.trussId = trussId;

    // Snap the fixture onto the nearest point of the truss line so it doesn't
    // jump — mirrors the drag-drop snap (project centre onto the truss). The
    // ICON's centre, not sceneBoundingRect()'s -- see iconCentreScene()'s own
    // comment; this function used to read one and write the other below.
    const QPointF iconCentre = mfi->iconCentreScene();
    QPointF mm = pixelsToRealPosition(iconCentre.x(), iconCentre.y());
    if (t->type() == Truss::Vertical)
    {
        mm = QPointF(t->origin().x() * 1000.0, t->origin().y() * 1000.0);
        // Keep any existing height offset; default to 0 for a fresh bind.
    }
    else
    {
        const double ox = t->origin().x() * 1000.0, oy = t->origin().y() * 1000.0;
        const double dx = t->direction().x(), dy = t->direction().y();
        double dot = (mm.x() - ox) * dx + (mm.y() - oy) * dy;
        dot = qBound(0.0, dot, t->length() * 1000.0);
        mm = QPointF(ox + dx * dot, oy + dy * dot);
        rp.trussOffset = float(dot / 1000.0);
    }
    props->setFixtureRigProps(fid, rp);
    mfi->setBoundToTruss(true);
    // Centre the icon on the truss line (not its top-left corner).
    mfi->setPos(realPositionToPixels(mm.x(), mm.y()) - halfIcon(mfi));
    mfi->setRealPosition(mm);
    emit fixtureMoved(fid, mm);

    // Join the truss's group (create/reuse it; pull in ungrouped siblings).
    ensureTrussGroup(trussId);
    if (t->groupId() != 0)
        props->setFixtureGroup(fid, t->groupId());
    m_doc->setModified();
    emit mapStructureChanged();
    refreshItemLayerState();
}

void MonitorGraphicsView::attachFixtureToTower(quint32 fid, quint32 towerId)
{
    // Like the truss and pipe attaches: a never-placed fixture gets an icon,
    // or it is rigged, invisible and not counted as on the tower.
    ensureFixtureOnPlot(fid);
    MonitorProperties *props = m_doc->monitorProperties();
    Tower *t = props->tower(towerId);
    if (t == nullptr || t->shelfCount() == 0)
        return;

    FixtureRigProps rp = props->fixtureRigProps(fid);
    // Reattaching (already shelf-mounted on this exact tower) keeps whatever
    // shelf it was already on; a fresh attach -- from a plain drop, with no
    // shelf to infer -- lands on the first one, matching the per-shelf
    // right-click menu's own default assignment. A plain drop always means a
    // SHELF (Top/Bottom are menu-only, explicit choices), so this also pulls
    // a fixture that was previously Top/Bottom-mounted back onto a shelf.
    if (!(rp.onTower() && rp.towerId == towerId))
        leaveMountGroupsOf(fid);
    const bool alreadyOnAShelf = (rp.towerId == towerId
                                   && rp.towerMountSide == FixtureRigProps::TowerShelf
                                   && rp.towerShelf < t->shelfCount());
    // Structural mounts are mutually exclusive.
    rp.trussId = Truss::invalidId();
    rp.pipeId = Pipe::invalidId();
    rp.riserPlatformId = FixtureRigProps::invalidPlatformId();
    rp.deckPlatformId = FixtureRigProps::invalidPlatformId();
    rp.clearMounts();   // the structural mounts are mutually exclusive
    rp.towerId = towerId;
    rp.towerMountSide = FixtureRigProps::TowerShelf;
    if (!alreadyOnAShelf)
        rp.towerShelf = 0;
    rp.towerU = t->width() * 0.5f;    // shelf centre
    rp.towerV = t->depth() * 0.5f;
    props->setFixtureRigProps(fid, rp);

    updateFixture(fid);
    joinTowerGroup(fid, towerId);
    m_doc->setModified();
    emit mapStructureChanged();
    refreshItemLayerState();
}

void MonitorGraphicsView::detachFixtureFromTruss(quint32 fid)
{
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(fid);
    if (rp.trussId == Truss::invalidId())
        return;
    const quint32 wasTrussId = rp.trussId;
    rp.trussId     = Truss::invalidId();
    rp.trussOffset = 0.0f;
    props->setFixtureRigProps(fid, rp);
    if (MonitorFixtureItem *mfi = m_fixtures.value(fid, nullptr))
    {
        mfi->setBoundToTruss(false);
        mfi->setEscapeMode(false);
    }
    // A fixture no longer rigged on this truss must also stop select/move-
    // together with it — otherwise an unbound, physically-unrelated fixture
    // stays permanently glued to the truss's selection (leaveDedicatedTrussGroup
    // only touches the truss's OWN auto-group; a real/manual group the user
    // built themselves is left alone).
    leaveDedicatedTrussGroup(fid, wasTrussId);
    m_doc->setModified();
    emit mapStructureChanged();
}

void MonitorGraphicsView::setReachOverlayVisible(bool on)
{
    if (m_reachVisible == on)
        return;
    m_reachVisible = on;
    rebuildReachOverlay();
}

void MonitorGraphicsView::setProjectionOverlayVisible(bool on)
{
    if (m_projectionVisible == on)
        return;
    m_projectionVisible = on;
    if (!on)
        m_footprintFixtures.clear();   // off means off: nothing lingers
    rebuildProjectionOverlay();
    emit projectionOverlayChanged(on);
}

void MonitorGraphicsView::revealBuriedFixtures(const QString &kind, quint32 id)
{
    QList<quint32> fids;
    structureCoverageByItsFixtures(kind, id, &fids);
    if (fids.isEmpty())
        return;

    m_revealOrder = fids;
    m_revealedFixtures = QSet<quint32>(fids.begin(), fids.end());
    m_revealKind = kind;
    m_revealId = id;
    /* Nothing picked yet: every one of them is on show, with its own tag, and
       the first click on a tag narrows it down. Selecting them ALL was the
       first cut, and it is what made this useless -- one blur of overlapping
       cones and no way to ask about a single head. */
    m_revealFocus = -1;

    // The bubble stands for what is now on show: it would only cover it.
    if (QGraphicsItemGroup *badge =
            m_countBadges.value(QStringLiteral("%1:%2").arg(kind).arg(id), nullptr))
        badge->setVisible(false);
    rebuildRevealTags();
    rebuildProjectionOverlay();
    m_scene->update();
}

bool MonitorGraphicsView::countBadgeHit(QGraphicsItemGroup *badge, const QPoint &viewPos) const
{
    /* In SCREEN pixels, on the disc itself. The bubble keeps one size on
       screen whatever the zoom (ItemIgnoresTransformations), so its bounds in
       scene units are not what is drawn: zoomed in they covered several times
       the circle, and a double-click on the tower around it opened the
       breakout instead of the tower ("it's assuming I am clicking on the
       circle .. even when I am not"). childrenBoundingRect(), not
       sceneBoundingRect(): a group caches its bounds when items are added,
       and the disc is sized afterwards. */
    if (badge == nullptr)
        return false;
    const QRectF onScreen =
        badge->deviceTransform(viewportTransform()).mapRect(badge->childrenBoundingRect());
    if (onScreen.isEmpty())
        return false;
    const QPointF d = QPointF(viewPos) - onScreen.center();
    const double r = qMin(onScreen.width(), onScreen.height()) * 0.5;
    return d.x() * d.x() + d.y() * d.y() <= r * r;
}

QList<quint32> MonitorGraphicsView::revealedFixtures() const
{
    return m_revealOrder;
}

QRectF MonitorGraphicsView::revealTagRect(quint32 fid) const
{
    foreach (QGraphicsItem *gi, m_revealTags)
    {
        RevealTagItem *tag = dynamic_cast<RevealTagItem *>(gi);
        if (tag != nullptr && tag->fixtureID() == fid)
            return tag->mapRectToScene(tag->boundingRect());
    }
    return QRectF();
}

void MonitorGraphicsView::focusRevealedFixture(quint32 fid)
{
    if (!m_revealedFixtures.contains(fid))
        return;

    m_revealFocus = qint64(fid);
    /* Drawn FIRST: Qt will not select an item that is not visible, and every
       fixture in the pile is hidden until one is picked. */
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        if (it.value() != nullptr)
            it.value()->setVisible(fixtureItemShouldBeVisible(it.key()));

    /* Selected on its own, so the Layers list, the inspector and every "the
       selected heads" action mean this one head. */
    m_settingRevealSelection = true;
    selectMapItem(QStringLiteral("fixture"), fid);
    m_settingRevealSelection = false;
    rebuildRevealTags();
    rebuildProjectionOverlay();
    m_scene->update();
}

void MonitorGraphicsView::clearRevealState()
{
    m_revealedFixtures.clear();
    m_revealOrder.clear();
    m_revealKind.clear();
    m_revealId = 0;
    m_revealFocus = -1;
    rebuildRevealTags();
}

/* The tags: one per revealed fixture, fanned down the side of the stack they
   came out of, each on a leader line back to it. */
void MonitorGraphicsView::rebuildRevealTags()
{
    foreach (QGraphicsItem *gi, m_revealTags)
    {
        m_scene->removeItem(gi);
        delete gi;
    }
    m_revealTags.clear();

    if (m_revealOrder.isEmpty())
        return;

    const QGraphicsItem *host =
          (m_revealKind == QStringLiteral("truss"))    ? (QGraphicsItem *)m_trussItems.value(m_revealId, nullptr)
        : (m_revealKind == QStringLiteral("tower"))    ? (QGraphicsItem *)m_towerItems.value(m_revealId, nullptr)
        : (m_revealKind == QStringLiteral("pipe"))     ? (QGraphicsItem *)m_pipeItems.value(m_revealId, nullptr)
        : (QGraphicsItem *)m_platformItems.value(m_revealId, nullptr);
    if (host == nullptr)
        return;

    const QPointF anchor = centreOf(host);
    QFont f = font();
    f.setPointSizeF(8.5);
    const QFontMetricsF fm(f);

    /* Laid out in SCREEN pixels, each tag drawn in screen space from the
       stack (ItemIgnoresTransformations): the column of names stays a column
       of upright names, the same size, at any zoom and any turn of the plot.
       Laid out in the scene, a quarter turn stood the column on its side and
       the pills overlapped. */
    const double rowH = fm.height() + 6.0;
    const double gap = 3.0;
    const double totalH = m_revealOrder.count() * (rowH + gap) - gap;
    double y = -totalH * 0.5;
    const double x = 18.0;

    MonitorProperties *props = m_doc ? m_doc->monitorProperties() : nullptr;

    foreach (quint32 fid, m_revealOrder)
    {
        Fixture *fx = m_doc->fixture(fid);
        const QString name = fx ? fx->name() : tr("Fixture %1").arg(fid);
        const bool picked = (m_revealFocus == qint64(fid));

        const double swatch = 8.0;
        const double w = fm.horizontalAdvance(name) + swatch + 16.0;
        QPainterPath p;
        p.addRoundedRect(QRectF(x, y, w, rowH), 3.0, 3.0);

        RevealTagItem *tag = new RevealTagItem(fid, this);
        tag->setPath(p);
        tag->setPos(anchor);
        tag->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        tag->setZValue(3.1);                  // above the fixtures it points at
        tag->setBrush(QBrush(picked ? QColor(40, 110, 220) : QColor(24, 32, 44, 235)));
        tag->setPen(QPen(picked ? QColor(235, 244, 255) : QColor(120, 160, 215), 1.0));
        tag->setToolTip(tr("Show where %1 lands").arg(name));

        QColor gel = props ? props->fixtureGelColor(fid, 0, 0) : QColor();
        if (!gel.isValid() || gel == QColor(Qt::black))
            gel = QColor(200, 210, 225);
        QGraphicsRectItem *chip = new QGraphicsRectItem(
            QRectF(x + 5.0, y + (rowH - swatch) * 0.5, swatch, swatch), tag);
        chip->setBrush(QBrush(gel));
        chip->setPen(QPen(QColor(15, 20, 28), 0.8));
        chip->setAcceptedMouseButtons(Qt::NoButton);

        QGraphicsSimpleTextItem *label = new QGraphicsSimpleTextItem(name, tag);
        label->setFont(f);
        label->setBrush(QBrush(picked ? Qt::white : QColor(225, 234, 245)));
        label->setPos(x + 5.0 + swatch + 5.0, y + (rowH - fm.height()) * 0.5);
        label->setAcceptedMouseButtons(Qt::NoButton);

        // The leader: this tag belongs to THAT stack, not to the deck under it.
        QGraphicsLineItem *lead = new QGraphicsLineItem(QLineF(QPointF(0.0, 0.0), QPointF(x, y + rowH * 0.5)));
        lead->setPen(QPen(QColor(120, 160, 215, 170), 0.8, Qt::DotLine));
        lead->setPos(anchor);
        lead->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        lead->setZValue(3.05);
        lead->setAcceptedMouseButtons(Qt::NoButton);
        m_scene->addItem(lead);
        m_revealTags << lead;

        m_scene->addItem(tag);
        m_revealTags << tag;
        y += rowH + gap;
    }
}

void MonitorGraphicsView::setFixtureFootprintShown(quint32 fid, bool on)
{
    if (on)
        m_footprintFixtures.insert(fid);
    else
        m_footprintFixtures.remove(fid);
    if (on && !m_projectionVisible)
        setProjectionOverlayVisible(true);   // asking for one turns them on
    else
        rebuildProjectionOverlay();
}

void MonitorGraphicsView::setFootprintsShown(const QList<quint32> &fids, bool on)
{
    foreach (quint32 fid, fids)
    {
        if (on)
            m_footprintFixtures.insert(fid);
        else
            m_footprintFixtures.remove(fid);
    }
    if (on && !fids.isEmpty() && !m_projectionVisible)
        setProjectionOverlayVisible(true);
    else
        rebuildProjectionOverlay();
}

bool MonitorGraphicsView::footprintsShown(const QList<quint32> &fids) const
{
    if (fids.isEmpty())
        return false;
    foreach (quint32 fid, fids)
    {
        if (!m_footprintFixtures.contains(fid))
            return false;
    }
    return true;
}

QList<quint32> MonitorGraphicsView::fixturesInGroup(quint32 gid) const
{
    QList<quint32> out;
    MonitorProperties *props = m_doc ? m_doc->monitorProperties() : nullptr;
    if (props == nullptr || gid == 0)
        return out;
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
    {
        if (props->fixtureGroup(it.key()) == gid)
            out << it.key();
    }
    return out;
}

void MonitorGraphicsView::setStructureFootprintsShown(const QString &kind, quint32 id, bool on)
{
    QList<quint32> fids;
    structureCoverageByItsFixtures(kind, id, &fids);
    setFootprintsShown(fids, on);
}

bool MonitorGraphicsView::structureFootprintsShown(const QString &kind, quint32 id) const
{
    QList<quint32> fids;
    const_cast<MonitorGraphicsView *>(this)->structureCoverageByItsFixtures(kind, id, &fids);
    return footprintsShown(fids);
}

QImage MonitorGraphicsView::fixtureCoverage(quint32 fid, double *cellMetres) const
{
    MonitorProperties *props = m_doc ? m_doc->monitorProperties() : nullptr;
    Fixture *fx = m_doc ? m_doc->fixture(fid) : nullptr;
    if (props == nullptr || fx == nullptr || m_cellPixels == 0)
        return QImage();

    /* Only a head that MOVES has coverage worth the name. A par throws one
       cone wherever it is bolted, and that cone is already drawn. */
    if (fx->channelNumber(QLCChannel::Pan,  QLCChannel::MSB) == QLCChannel::invalid()
        && fx->channelNumber(QLCChannel::Tilt, QLCChannel::MSB) == QLCChannel::invalid())
        return QImage();

    AimSolver::Frame frame;
    if (!AimSolver::frameFor(m_doc, fid, frame))
        return QImage();

    /* The stage, sampled finely enough to show the hole under an upright head
       -- "it can't display next to the truss on the floor" is exactly the
       shape this has to get right -- and capped so a big grid stays cheap.
       Solving is arithmetic against one Frame, so this is thousands of
       multiply-adds, not thousands of Doc lookups. */
    const double stageW = double(m_gridExtent.width())  * m_unitValue / 1000.0;
    const double stageD = double(m_gridExtent.height()) * m_unitValue / 1000.0;
    if (stageW <= 0.0 || stageD <= 0.0)
        return QImage();
    const double cell = qMax(0.05, qSqrt(stageW * stageD / 160000.0));
    const int cols = qMax(1, int(qCeil(stageW / cell)));
    const int rows = qMax(1, int(qCeil(stageD / cell)));

    QImage img(cols, rows, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QColor col = props->fixtureGelColor(fid, 0, 0);
    if (!col.isValid() || col == QColor(Qt::black))
        col = QColor(90, 160, 235);
    const QRgb lit = qPremultiply(qRgba(col.red(), col.green(), col.blue(), 64));

    bool any = false;
    for (int r = 0; r < rows; ++r)
    {
        const float y = float((r + 0.5) * cell);
        for (int c = 0; c < cols; ++c)
        {
            const float x = float((c + 0.5) * cell);
            // The surface itself: light lands ON the deck, not above it.
            const QVector3D at(x, y, props->platformHeightAt(x, y));
            if (!AimSolver::solve(frame, at).reachable())
                continue;
            img.setPixel(c, r, lit);
            any = true;
        }
    }

    if (!any)
        return QImage();
    if (cellMetres != nullptr)
        *cellMetres = cell;
    return img;
}

QPolygonF MonitorGraphicsView::fixtureProjection(quint32 fid) const
{
    QPolygonF out;
    Fixture *fx = m_doc->fixture(fid);
    MonitorProperties *props = m_doc->monitorProperties();
    if (fx == nullptr || props == nullptr || m_cellPixels == 0)
        return out;

    const FixtureVisualTraits traits = classifyFixture(fx);
    const QVector3D apex = fixtureWorldCentre(m_doc, fid);

    /* Where it points.
     *
       THE SELECTED SCENE WINS, as it does in the rig view. A head that nothing
       is driving reads as pan 0, tilt 0 -- which for a head standing upright on
       a truss is the horizon exactly, so it landed nothing and said so, while
       the scene beside it was aiming that same head at a mark on the deck. An
       aim it cannot reach is NOT used: it is stuck at its limit somewhere
       short, and its resting DMX is the honest answer there (the red aim line
       says the rest).

       Otherwise the fixture says so itself through its pan and tilt, and
       anything that cannot move throws straight down -- what a fixture hung
       over a stage does. */
    QVector3D dir;
    bool aimed = false;
    /* ...until it RUNS: then the heads go where the DMX sends them, which an
       effect on the Aim look (Stage Shape round the target, an orbit) makes
       somewhere other than the target itself. */
    /* Focus mode's head is where the session is aiming it, whatever scene
       is selected: at its reference, without the nudge (fixtureAimDirection()
       says the same, for when nothing is selected). */
    if (focusIntentDirection(m_doc, fid, dir))
        aimed = true;
    Scene *active = qobject_cast<Scene *>(m_doc->function(m_activeSceneId));
    if (!aimed && active != nullptr && !active->isRunning())
    {
        foreach (const AimSolver::SceneAim &a, AimSolver::sceneAims(m_doc, active))
        {
            if (a.fixtureId != fid || !a.solution.reachable())
                continue;
            /* The point the SCENE resolved, not one recomputed here: it
               already decided whether this scene follows a subject, and
               forcing subject height on a plain deck mark threw the pool four
               metres past it (the beam runs on through the aim point, which is
               right -- it just has to be the right point). */
            const QVector3D to = a.point - apex;
            if (to.length() > 1e-3f)
            {
                dir = to.normalized();
                aimed = true;
            }
            break;
        }
    }
    /* Not aimed by a scene: where it throws by itself -- the rule the 3D
       views use (RigHeads::fixedThrow): a fixed light's pan and tilt (Q100),
       its chosen face (a laser faced Back throws upstage, not into a pool on
       the deck under its truss), its riser or deck; down off a truss. */
    if (!aimed && !throwDirection(fid, dir))
        dir = QVector3D(0.0f, 0.0f, -1.0f);
    if (dir.z() > -0.02f)
        return out;                       // level or upward: nothing lands

    double deg = fixtureBeamAngle(fx, fx->channelValues(), traits);
    if (deg <= 0.0 || deg >= 180.0)
        deg = 14.0;                       // undeclared: an ordinary spot
    const double half = qDegreesToRadians(deg / 2.0);

    QVector3D side = QVector3D::crossProduct(QVector3D(0, 0, 1), dir);
    if (side.length() < 1e-3f)
        side = QVector3D(1, 0, 0);
    side.normalize();
    const QVector3D up = QVector3D::crossProduct(dir, side).normalized();

    /* Follow each edge ray down until it meets the floor or a deck. Marched
       rather than solved: the stage is a height field of stacked platforms,
       not one plane. */
    auto land = [&](const QVector3D &ray) {
        QVector3D p = apex;
        for (double t = 0.05; t <= 40.0; t += 0.1)
        {
            p = apex + ray * float(t);
            if (double(p.z()) <= double(props->platformHeightAt(p.x(), p.y())))
                break;
        }
        return p;
    };

    static const int RING = 20;
    for (int i = 0; i < RING; ++i)
    {
        const double a = 2.0 * M_PI * i / RING;
        const QVector3D across = side * float(qCos(a)) + up * float(qSin(a));
        const QVector3D ray = (dir * float(qCos(half)) + across * float(qSin(half))).normalized();
        if (ray.z() > -0.02f)
            return QPolygonF();           // part of the cone never comes down
        const QVector3D at = land(ray);
        out << realPositionToPixels(double(at.x()) * 1000.0, double(at.y()) * 1000.0);
    }
    return out;
}

bool MonitorGraphicsView::throwDirection(quint32 fid, QVector3D &dir) const
{
    Fixture *fx = m_doc->fixture(fid);
    MonitorProperties *props = m_doc->monitorProperties();
    if (fx == nullptr || props == nullptr)
        return false;
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const FixtureVisualTraits traits = classifyFixture(fx);
    if (traits.kind == FixtureSilhouette::Mover && fixtureAimDirection(fx, rp, dir) && !dir.isNull())
        return true;
    return RigHeads::fixedThrow(m_doc, fid, traits.headCount,
                                traits.kind == FixtureSilhouette::Mover, dir);
}

void MonitorGraphicsView::setEmissionOverlayVisible(bool on)
{
    if (m_emissionVisible == on)
        return;
    m_emissionVisible = on;
    QSettings().setValue(QStringLiteral("monitor/laserOutput"), on);
    rebuildEmissionOverlay();
}

void MonitorGraphicsView::rebuildEmissionOverlay()
{
    MonitorProperties *props = m_doc ? m_doc->monitorProperties() : nullptr;
    if (m_emissiveStale && m_doc != nullptr)
    {
        m_emissiveFixtures.clear();
        foreach (Fixture *fx, m_doc->fixtures())
        {
            const FixtureVisualTraits t = classifyFixture(fx);
            if (t.stars || t.laser)
                m_emissiveFixtures << fx->id();
        }
        m_emissiveStale = false;
    }
    const bool wanted = props != nullptr && m_emissionVisible && m_pov == PovTop
                        && !m_emissiveFixtures.isEmpty() && m_cellPixels > 0;

    /* What it would draw, said once: unchanged (a still laser at the same
       level), nothing is torn down and built again. */
    struct Out { quint32 fid; QColor colour; double level; QVector3D apex, dir; double half;
                 bool stars; int beams; double turn; };
    QList<Out> outs;
    QString sig;
    bool turning = false;
    if (wanted)
    {
        foreach (quint32 fid, m_emissiveFixtures)
        {
            Fixture *fx = m_doc->fixture(fid);
            if (fx == nullptr || !fixtureItemShouldBeVisible(fid))
                continue;
            const FixtureVisualTraits traits = classifyFixture(fx);
            const QByteArray vals = fx->channelValues();
            QColor live(Qt::white);
            uchar dim = 0;
            QVector3D dir;
            /* Lit: a level, and some colour -- "no black light": with
               red, green and blue all at 0 the level is 0 too. */
            if (!fixtureLiveState(fx, vals, live, dim) || dim <= 2 || !throwDirection(fid, dir))
                continue;
            const double declared = fixtureBeamAngle(fx, vals, traits);
            Out o;
            o.fid = fid;
            o.colour = live;
            o.level = dim / 255.0;
            o.apex = fixtureWorldCentre(m_doc, fid);
            o.dir = dir.normalized();
            o.stars = traits.stars;
            o.beams = qMax(1, traits.beamsPerHead);
            const double deg = traits.stars ? ((declared > 0.0 && declared < 170.0) ? declared : 100.0)
                                             : ((declared > 0.0 && declared < 170.0) ? declared : 40.0);
            o.half = qDegreesToRadians(deg / 2.0);
            const double rate = starTurnRate(fx, vals);
            if (rate != 0.0)
                turning = true;
            o.turn = qDegreesToRadians(rate * (QDateTime::currentMSecsSinceEpoch() % 3600000) / 1000.0);
            outs << o;
            sig += QStringLiteral("%1:%2:%3:%4,%5,%6:%7,%8,%9;")
                       .arg(fid).arg(live.name()).arg(dim)
                       .arg(double(o.apex.x())).arg(double(o.apex.y())).arg(double(o.apex.z()))
                       .arg(double(o.dir.x())).arg(double(o.dir.y())).arg(double(o.dir.z()));
        }
    }

    // About 15 times a second while any of them could be lit and showing.
    if (wanted && isVisible())
    {
        if (!m_emissionTimer->isActive())
            m_emissionTimer->start();
    }
    else
        m_emissionTimer->stop();

    // Drawn in scene pixels: a new scale or offset (a refit, a turn) is a change too.
    if (!sig.isEmpty())
        sig += QStringLiteral("@%1,%2,%3").arg(m_cellPixels).arg(m_xOffset).arg(m_yOffset);
    if (!turning && sig == m_emissionSignature && (sig.isEmpty() == m_emissionItems.isEmpty()))
        return;
    m_emissionSignature = sig;
    foreach (QGraphicsPathItem *pi, m_emissionItems)
    {
        m_scene->removeItem(pi);
        delete pi;
    }
    m_emissionItems.clear();
    m_emissionLandings.clear();
    m_emissionColours.clear();
    if (outs.isEmpty())
        return;

    /* Where a ray lands: the first of the floor or a deck (marched: the
       stage is a height field of platforms) and the scenery standing in its
       way (a cyc is how a sky laser is seen). Nothing within 24 m: none. */
    auto land = [&](const QVector3D &apex, const QVector3D &ray, QVector3D &at) {
        static const double MAX_THROW = 24.0;
        double best = -1.0;
        foreach (Scenery *sc, props->sceneries())
        {
            if (sc == nullptr || !props->layer(sc->layerId()).visible)
                continue;
            double t = 0.0;
            if (sc->rayHit(apex, ray, t, best > 0.0 ? best : MAX_THROW) && t > 0.05)
                best = t;
        }
        const double reach = best > 0.0 ? best : MAX_THROW;
        if (ray.z() < -1e-4f)
        {
            // Never past the floor; decks on the way, in 10 cm steps.
            const double floorT = -double(apex.z()) / double(ray.z());
            const double stop = qMin(reach, floorT);
            for (double t = 0.05; t < stop; t += 0.1)
            {
                const QVector3D p = apex + ray * float(t);
                if (double(p.z()) <= double(props->platformHeightAt(p.x(), p.y())))
                {
                    best = t;
                    break;
                }
            }
            if (best < 0.0 || floorT < best)
                best = (floorT > 0.05 && floorT <= MAX_THROW) ? floorT : best;
        }
        if (best <= 0.0)
            return false;
        at = apex + ray * float(best);
        return true;
    };

    foreach (const Out &o, outs)
    {
        QVector3D u = QVector3D::crossProduct(o.dir, QVector3D(0, 0, 1));
        if (u.length() < 1e-3f)
            u = QVector3D(1, 0, 0);
        u.normalize();
        const QVector3D v = QVector3D::crossProduct(o.dir, u).normalized();
        QVector<QVector3D> rays;
        if (o.stars)
        {
            // The rig view's sky: a sunflower spiral, nudged by a hash of
            // each index, the same every frame (drawStarField()).
            static const int N = 120;
            const double spread = qTan(qMin(o.half, qDegreesToRadians(80.0)));
            for (int i = 0; i < N; ++i)
            {
                double j = qSin((i + 1) * 12.9898 + o.fid * 78.233) * 43758.5453;
                j -= qFloor(j);
                const double r = spread * qSqrt((i + 0.5) / N) * (0.85 + 0.3 * j);
                const double a = i * 2.399963 + o.turn + j * 0.5;
                rays << (o.dir + u * float(r * qCos(a)) + v * float(r * qSin(a))).normalized();
            }
        }
        else
        {
            // A laser: its beams fanned across its beam angle, as in 3D.
            for (int b = 0; b < o.beams; ++b)
            {
                const double a = (o.beams > 1) ? 2.0 * o.half * ((b / double(o.beams - 1)) - 0.5) : 0.0;
                rays << (o.dir * float(qCos(a)) + u * float(qSin(a))).normalized();
            }
        }

        const QPointF head = realPositionToPixels(double(o.apex.x()) * 1000.0, double(o.apex.y()) * 1000.0);
        QPainterPath dots, lines;
        QVector<QPointF> landed;
        const double dotR = qMax(1.2, 0.03 * m_cellPixels * 1000.0 / m_unitValue);
        foreach (const QVector3D &ray, rays)
        {
            QVector3D at;
            if (!land(o.apex, ray, at))
                continue;
            landed << QPointF(double(at.x()), double(at.y()));
            const QPointF px = realPositionToPixels(double(at.x()) * 1000.0, double(at.y()) * 1000.0);
            dots.addEllipse(px, dotR, dotR);
            if (!o.stars)
            {
                lines.moveTo(head);
                lines.lineTo(px);
            }
        }
        if (landed.isEmpty())
            continue;
        m_emissionLandings.insert(o.fid, landed);
        const QColor hsv = o.colour.toHsv();
        QColor c = QColor::fromHsvF(qMax(0.0, double(hsv.hsvHueF())), hsv.hsvSaturationF() * 0.9,
                                    qMin(1.0, 0.45 + 0.55 * o.level));
        c.setAlphaF(qBound(0.35, 0.35 + 0.65 * o.level, 1.0));
        m_emissionColours.insert(o.fid, c);
        Fixture *fx = m_doc->fixture(o.fid);
        const QString tip = fx == nullptr ? QString()
                          : o.stars ? tr("%1: where its stars land").arg(fx->name())
                                    : tr("%1: where its beams land").arg(fx->name());
        if (!lines.isEmpty())
        {
            QGraphicsPathItem *li = new QGraphicsPathItem(lines);
            QPen pen(c, 1.2);
            pen.setCosmetic(true);
            li->setPen(pen);
            li->setZValue(1.7);
            li->setAcceptedMouseButtons(Qt::NoButton);
            li->setToolTip(tip);
            m_scene->addItem(li);
            m_emissionItems << li;
        }
        QGraphicsPathItem *di = new QGraphicsPathItem(dots);
        di->setPen(Qt::NoPen);
        di->setBrush(c);
        di->setZValue(1.7);
        di->setAcceptedMouseButtons(Qt::NoButton);
        di->setToolTip(tip);
        m_scene->addItem(di);
        m_emissionItems << di;
    }
}

void MonitorGraphicsView::rebuildProjectionOverlay()
{
    foreach (QGraphicsPathItem *pi, m_projectionItems)
    {
        m_scene->removeItem(pi);
        delete pi;
    }
    m_projectionItems.clear();
    foreach (QGraphicsPixmapItem *ci, m_coverageItems)
    {
        m_scene->removeItem(ci);
        delete ci;
    }
    m_coverageItems.clear();

    MonitorProperties *props = m_doc ? m_doc->monitorProperties() : nullptr;
    if (props == nullptr || m_pov != PovTop)
        return;

    /* "Light footprint checkbox in Show isn't working -- I see it when
       unchecked." It gated the SELECTION only, while a reveal and the
       per-fixture menu drew regardless, so unticking it left footprints on
       screen. It is the master switch now: off draws nothing at all, whoever
       asked for it. On, it draws what is selected plus anything named by name
       from a menu -- and the menu entries switch it on, so asking for a
       footprint always produces one. */
    if (!m_projectionVisible)
        return;

    QSet<quint32> fids = m_footprintFixtures;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
    {
        if (MonitorFixtureItem *mfi = dynamic_cast<MonitorFixtureItem *>(gi))
            fids.insert(mfi->fixtureID());
    }
    if (fids.isEmpty())
        return;

    foreach (quint32 fid, fids)
    {
        /* EVERYTHING IT CAN HIT, first and underneath: "if I select T1-Spot
           and I have footprint enabled I should see ALL THE AREA THAT THE SPOT
           CAN HIT". A moving head's footprint is the reach of its pan and tilt
           over the deck, and the cone below is only where it happens to point
           inside that. The hole in the middle of it, under a head standing on
           a truss top, is the answer to "it can't display next to the truss". */
        double cell = 0.0;
        const QImage cov = fixtureCoverage(fid, &cell);
        if (!cov.isNull() && cell > 0.0)
        {
            QGraphicsPixmapItem *ci = new QGraphicsPixmapItem(QPixmap::fromImage(cov));
            ci->setZValue(1.65);              // over the decks, under the pool
            ci->setAcceptedMouseButtons(Qt::NoButton);
            ci->setTransformationMode(Qt::FastTransformation);
            ci->setPos(realPositionToPixels(0.0, 0.0));
            ci->setScale(cell * 1000.0 * m_cellPixels / m_unitValue);
            Fixture *cfx = m_doc->fixture(fid);
            ci->setToolTip(tr("Everywhere %1 can put light, on the deck and the "
                              "platforms")
                           .arg(cfx ? cfx->name() : QString::number(fid)));
            m_scene->addItem(ci);
            m_coverageItems << ci;
        }

        const QPolygonF poly = fixtureProjection(fid);
        if (poly.size() < 3)
        {
            /* Coverage on its own is a complete answer for a head that is
               simply not pointed anywhere useful right now. */
            if (!m_coverageItems.isEmpty() && !cov.isNull())
                continue;
            /* Nothing lands, and silence reads as a broken feature. Say WHY,
               with the numbers -- because the answer is usually about how the
               head is rigged, and vague is useless. T1-Spot is the case that
               forced this: standing upright on a 3 m truss with 190 degrees of
               tilt, it reaches 5 degrees below level, so the nearest thing it
               could light is 35 m away. It is not that the view is broken; it
               is that the head cannot light this stage. */
            MonitorFixtureItem *mfi = m_fixtures.value(fid, nullptr);
            if (mfi == nullptr)
                continue;
            Fixture *fx = m_doc->fixture(fid);
            QFont nf = font();
            nf.setPointSizeF(8.0);
            const QFontMetricsF nfm(nf);
            const QString who = fx ? fx->name() : QString::number(fid);
            QString note = tr("%1: nothing lands — aimed level or up").arg(who);
            AimSolver::Frame nfr;
            if (cov.isNull() && AimSolver::frameFor(m_doc, fid, nfr) && nfr.floorMounted)
            {
                const double below = double(nfr.tiltMax) / 2.0 - 90.0;
                const double up = double(nfr.position.z());
                if (below <= 0.01)
                    note = tr("%1 can light nothing: standing upright, its %2° of "
                              "tilt never reaches below level")
                           .arg(who).arg(double(nfr.tiltMax), 0, 'f', 0);
                else
                    note = tr("%1 can light nothing here: standing upright, %2° of "
                              "tilt reaches %3° below level — %4 out from %5 up")
                           .arg(who).arg(double(nfr.tiltMax), 0, 'f', 0)
                           .arg(below, 0, 'f', 0)
                           .arg(MonitorProperties::formatStageDistance(float(up / qTan(qDegreesToRadians(below)))),
                                MonitorProperties::formatStageDistance(float(up)));
            }
            const QPointF c = mfi->iconCentreScene();
            const double nw = nfm.horizontalAdvance(note) + 8.0;
            const double nh = nfm.height() + 2.0;
            QGraphicsPathItem *holder = new QGraphicsPathItem;
            QPainterPath pp;
            pp.addRoundedRect(QRectF(c.x() - nw * 0.5, c.y() + 10.0, nw, nh), 3.0, 3.0);
            holder->setPath(pp);
            holder->setBrush(QBrush(QColor(12, 18, 26, 205)));
            holder->setPen(QPen(QColor(190, 160, 80), 1.0));
            holder->setZValue(1.75);
            holder->setAcceptedMouseButtons(Qt::NoButton);
            keepUpright(holder);              // the note and its plate turn back together
            QGraphicsSimpleTextItem *nt = new QGraphicsSimpleTextItem(note, holder);
            nt->setFont(nf);
            nt->setBrush(QBrush(QColor(240, 224, 190)));
            nt->setPos(c.x() - nw * 0.5 + 4.0, c.y() + 11.0);
            nt->setAcceptedMouseButtons(Qt::NoButton);
            m_scene->addItem(holder);
            m_projectionItems << holder;
            continue;
        }
        QColor col = props->fixtureGelColor(fid, 0, 0);
        if (!col.isValid() || col == QColor(Qt::black))
            col = QColor(90, 160, 235);
        QPainterPath path;
        path.addPolygon(poly);
        path.closeSubpath();
        QGraphicsPathItem *item = new QGraphicsPathItem(path);
        /* Hatched, not a flat wash. Flat translucent shapes piled on each other
           read as one unexplained blob -- "it's not clear to me what these cones
           represent .. if they're the blackout area" -- and hatching says
           "region", tells overlapping ones apart, and leaves the deck legible
           underneath. Light, so it is never mistaken for a no-light zone: that
           warning, when it lands, is the red one. */
        QColor fill = col;
        fill.setAlpha(150);
        QBrush brush(fill, Qt::BDiagPattern);
        item->setBrush(brush);
        item->setPen(QPen(col, 1.2));
        item->setZValue(1.7);             // over the decks, under the fixtures
        item->setAcceptedMouseButtons(Qt::NoButton);
        Fixture *fx = m_doc->fixture(fid);
        const QString name = fx ? fx->name() : QString::number(fid);
        item->setToolTip(tr("Light footprint: where %1 lands").arg(name));

        /* Named on the plot, because a shape with no name answers "where does
           light land" but not "whose light is this". */
        QFont lf = font();
        lf.setPointSizeF(8.0);
        const QFontMetricsF lfm(lf);
        const QPointF c = poly.boundingRect().center();
        const double tw = lfm.horizontalAdvance(name) + 8.0;
        const double th = lfm.height() + 2.0;
        QGraphicsRectItem *back = new QGraphicsRectItem(
            QRectF(c.x() - tw * 0.5, c.y() - th * 0.5, tw, th), item);
        back->setBrush(QBrush(QColor(12, 18, 26, 190)));
        back->setPen(QPen(col, 0.8));
        back->setAcceptedMouseButtons(Qt::NoButton);
        keepUpright(back);                    // the name and its plate turn back together
        QGraphicsSimpleTextItem *tag = new QGraphicsSimpleTextItem(name, back);
        tag->setFont(lf);
        tag->setBrush(QBrush(QColor(235, 242, 250)));
        tag->setPos(c.x() - tw * 0.5 + 4.0, c.y() - th * 0.5 + 1.0);
        tag->setAcceptedMouseButtons(Qt::NoButton);

        m_scene->addItem(item);
        m_projectionItems << item;
    }
}

void MonitorGraphicsView::scheduleReachOverlay()
{
    if (m_reachVisible == false || m_reachUpdatePending)
        return;
    m_reachUpdatePending = true;
    QTimer::singleShot(0, this, [this]() {
        m_reachUpdatePending = false;
        rebuildReachOverlay();
    });
}

void MonitorGraphicsView::rebuildReachOverlay()
{
    MonitorProperties *props = m_doc ? m_doc->monitorProperties() : nullptr;

    // The heads: selected, aimable, with rig geometry to answer from.
    QList<AimSolver::Frame> frames;
    if (m_reachVisible && props != nullptr && m_pov == PovTop && m_cellPixels > 0)
    {
        foreach (QGraphicsItem *gi, m_scene->selectedItems())
        {
            MonitorFixtureItem *mfi = dynamic_cast<MonitorFixtureItem *>(gi);
            if (mfi == nullptr)
                continue;
            Fixture *fxi = m_doc->fixture(mfi->fixtureID());
            if (fxi == nullptr)
                continue;
            if (fxi->channelNumber(QLCChannel::Pan,  QLCChannel::MSB) == QLCChannel::invalid()
                && fxi->channelNumber(QLCChannel::Tilt, QLCChannel::MSB) == QLCChannel::invalid())
                continue;
            AimSolver::Frame f;
            if (AimSolver::frameFor(m_doc, fxi->id(), f))
                frames << f;
        }
    }

    if (frames.isEmpty())
    {
        if (m_reachItem != nullptr)
            m_reachItem->setVisible(false);
        return;
    }

    /* The stage the grid covers, sampled finely enough to show a blind circle a
       metre across but capped, so a big grid does not cost a big image. */
    const double stageW = double(m_gridExtent.width())  * m_unitValue / 1000.0;
    const double stageD = double(m_gridExtent.height()) * m_unitValue / 1000.0;
    const double cell = qMax(0.05, qSqrt(stageW * stageD / 160000.0));
    const int cols = qMax(1, int(qCeil(stageW / cell)));
    const int rows = qMax(1, int(qCeil(stageD / cell)));
    const float above = props->aimSubjectHeight();

    QImage img(cols, rows, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    const QRgb blind = qPremultiply(qRgba(235, 70, 60, 90));
    for (int r = 0; r < rows; ++r)
    {
        const float y = float((r + 0.5) * cell);
        for (int c = 0; c < cols; ++c)
        {
            const float x = float((c + 0.5) * cell);
            const QVector3D at(x, y, props->platformHeightAt(x, y) + above);
            bool anyReaches = false;
            for (const AimSolver::Frame &f : frames)
                if (AimSolver::solve(f, at).reachable())
                {
                    anyReaches = true;
                    break;
                }
            if (!anyReaches)
                img.setPixel(c, r, blind);
        }
    }

    if (m_reachItem == nullptr)
    {
        m_reachItem = new QGraphicsPixmapItem;
        m_reachItem->setZValue(1.6);          // over the decks, under fixtures (2)
        m_reachItem->setAcceptedMouseButtons(Qt::NoButton);
        m_reachItem->setTransformationMode(Qt::FastTransformation);
        m_scene->addItem(m_reachItem);
    }
    m_reachItem->setPixmap(QPixmap::fromImage(img));
    m_reachItem->setPos(realPositionToPixels(0.0, 0.0));
    m_reachItem->setScale(cell * 1000.0 * m_cellPixels / m_unitValue);
    m_reachItem->setToolTip(tr("Shaded: where none of the selected heads can aim, "
                               "%1 above the deck").arg(MonitorProperties::formatStageDistance(float(above))));
    m_reachItem->setVisible(true);
}

void MonitorGraphicsView::persistElevationDrop(quint32 fid, MonitorFixtureItem *item)
{
    MonitorProperties *props = m_doc->monitorProperties();
    /* A free-placed fixture's stored position is MIXED UNITS: X and Y
       in millimetres, Z in METRES (see the tail of
       MonitorProperties::fixtureRigPosition, which divides X/Y by 1000
       and passes Z straight through). Writing a plain mm vector put a
       metre value of, say, 2500 into Z -- two and a half kilometres up
       -- and the redraw then placed the fixture nowhere near the drop,
       which reads as snapping back. Convert on the way in and out. */
    item->setAttachMode(false);

    const QVector3D stored = props->fixturePosition(fid, 0, 0);
    const QVector3D curMm(stored.x(), stored.y(), stored.z() * 1000.0f);

    /* Read the drop with the SAME anchor updateFixture places with: a free
       fixture by its centre, a decked one by the middle of its base. The
       store keeps the corner, so the centre's offset comes off again.
       Reading one anchor while the redraw placed another shifted the item
       by half an icon on every single drop -- the reported "jumps
       somewhere else". */
    const FixtureRigProps arp = props->fixtureRigProps(fid);
    const QPointF off = freeCentreOffsetMm(fid);
    QPointF dropPx = item->pos() + halfIcon(item);
    if (arp.onDeck())
        dropPx += QPointF(0.0, halfIcon(item).y());

    const QVector3D newMm = unprojectMm(dropPx, curMm + QVector3D(float(off.x()), float(off.y()), 0.0f))
                            - QVector3D(float(off.x()), float(off.y()), 0.0f);

    if (!qFuzzyCompare(newMm, curMm))
    {
        props->setFixturePosition(fid, 0, 0,
            QVector3D(newMm.x(), newMm.y(), newMm.z() / 1000.0f));
        /* Keep the top view's cache in step, or switching views after
           an elevation move snaps X/Y back to wherever they were. */
        item->setRealPosition(QPointF(double(newMm.x()), double(newMm.y())));
        updateFixture(fid);
        emit fixtureMoved(fid, QPointF(double(newMm.x()), double(newMm.y())));
        m_doc->setModified();
    }
}

void MonitorGraphicsView::slotFixtureMoved(MonitorFixtureItem *item)
{
    scheduleNetworkRuns();
    // ELEVATION drag of a truss-bound fixture: it slides ALONG its truss/bar, so
    // convert the drop delta into a new trussOffset (Front view only).
    if (isElevation() && item != nullptr)
    {
        MonitorProperties *props = m_doc->monitorProperties();
        const quint32 fid = m_fixtures.key(item, Fixture::invalidId());

        /* Free-placed fixture: no truss to slide along, so the drop simply IS
           its new position -- unprojected for the view it was dropped in, with
           the axis that is not on screen left as it was. */
        /* Several free fixtures dragged together: Qt moves them all, so every
           one is kept where it was dropped -- or, dropped on a truss, they all
           hang on it, each where it landed along it ("only moves one"). */
        QList<MonitorFixtureItem *> freeItems;
        if (fid != Fixture::invalidId() && !elevationFixtureDraggable(fid)
                && (item->flags() & QGraphicsItem::ItemIsMovable))
        {
            freeItems << item;
            foreach (QGraphicsItem *gi, m_scene->selectedItems())
            {
                MonitorFixtureItem *other = dynamic_cast<MonitorFixtureItem *>(gi);
                const quint32 ofid = other != nullptr ? m_fixtures.key(other, Fixture::invalidId())
                                                      : Fixture::invalidId();
                if (other != nullptr && other != item && ofid != Fixture::invalidId()
                        && !elevationFixtureDraggable(ofid) && (other->flags() & QGraphicsItem::ItemIsMovable))
                    freeItems << other;
            }
        }
        if (!freeItems.isEmpty())
        {
            if (TrussItem *over = trussUnderFixture(item, 1.0f, true))
            {
                for (MonitorFixtureItem *mi : freeItems)
                    attachFixtureToTrussAt(m_fixtures.key(mi), over, mi);
                updateTrussAnchorLines();
                return;
            }
            for (MonitorFixtureItem *mi : freeItems)
                persistElevationDrop(m_fixtures.key(mi), mi);
            updateTrussAnchorLines();
            return;
        }

        if (fid != Fixture::invalidId() && elevationFixtureDraggable(fid))
        {
            FixtureRigProps rp = props->fixtureRigProps(fid);
            Truss *truss = props->truss(rp.trussId);

            /* Released clear of its truss: detach here too -- the red ring
               made that promise in this view, so this view has to keep it. */
            if (truss != nullptr
                && trussUnderFixture(item) != m_trussItems.value(rp.trussId, nullptr))
            {
                const quint32 wasTrussId = rp.trussId;
                rp.trussId     = Truss::invalidId();
                rp.trussOffset = 0.0f;
                rp.trussCross  = 0.0f;
                props->setFixtureRigProps(fid, rp);
                item->setEscapeMode(false);
                item->setBoundToTruss(false);
                leaveDedicatedTrussGroup(fid, wasTrussId);

                /* Now free: persist the drop as a free position (top-left
                   anchor, mixed units -- see the free branch above). */
                const QVector3D stored = props->fixturePosition(fid, 0, 0);
                const QVector3D curMm(stored.x(), stored.y(), stored.z() * 1000.0f);
                const QVector3D off(float(freeCentreOffsetMm(fid).x()), float(freeCentreOffsetMm(fid).y()), 0.0f);
                const QVector3D newMm = unprojectMm(item->pos() + halfIcon(item), curMm + off) - off;
                props->setFixturePosition(fid, 0, 0,
                    QVector3D(newMm.x(), newMm.y(), newMm.z() / 1000.0f));
                item->setRealPosition(QPointF(double(newMm.x()), double(newMm.y())));
                updateFixture(fid);
                emit fixtureMoved(fid, QPointF(double(newMm.x()), double(newMm.y())));
                m_doc->setModified();
                updateTrussAnchorLines();
                return;
            }

            if (truss != nullptr && m_cellPixels > 0)
            {
                const double mPerPx = double(m_unitValue) / (double(m_cellPixels) * 1000.0);
                const QVector3D w = props->fixtureRigPosition(fid);   // metres
                const QPointF pExpected = projectMm(w.x() * 1000.0, w.y() * 1000.0, w.z() * 1000.0)
                                          - halfIcon(item);
                const QPointF pDrop = item->pos();
                const double dH =  (pDrop.x() - pExpected.x()) * mPerPx;   // screen horizontal
                const double dV = -(pDrop.y() - pExpected.y()) * mPerPx;   // screen vertical
                // World delta (Front: X horizontal; Side: Y horizontal), projected
                // onto the truss/bar axis → Δoffset along it.
                const QVector3D wd = (m_pov == PovFront) ? QVector3D(float(dH), 0, float(dV))
                                                         : QVector3D(0, float(dH), float(dV));
                const bool verticalTruss = (truss->type() == Truss::Vertical);
                const QVector3D axis = verticalTruss
                    ? QVector3D(0, 0, 1)
                    : QVector3D(truss->direction().x(), truss->direction().y(), 0);
                const double dOff = QVector3D::dotProduct(wd, axis);
                float off = qBound(0.0f, rp.trussOffset + float(dOff), truss->length());
                rp.trussOffset = off;

                /* Vertical drag changes the HANG HEIGHT, it is not thrown away.
                   On a horizontal truss the axis is (1,0,0) or (0,1,0), so the
                   dot product above keeps only the horizontal component and dV
                   vanished -- dragging a fixture up or down in Front did
                   nothing at all, which looked like the drag had been refused.
                   A fixture hanging on a drop below its truss is an ordinary
                   rig, and mountZOffset is the field that already describes it.
                   Skipped for a vertical truss, where up/down IS along the
                   truss and has already gone into trussOffset. */
                if (!verticalTruss && !qFuzzyIsNull(dV))
                    rp.mountZOffset = qBound(-20.0f, rp.mountZOffset + float(dV), 20.0f);

                props->setFixtureRigProps(fid, rp);
                const QVector3D nw = truss->positionAt(off);   // new world pos
                item->setRealPosition(QPointF(nw.x() * 1000.0, nw.y() * 1000.0));
                updateFixture(fid);
                emit fixtureMoved(fid, QPointF(nw.x() * 1000.0, nw.y() * 1000.0));
                m_doc->setModified();
            }
        }
        updateTrussAnchorLines();
        return;   // elevation: don't run the top-view move logic
    }

    // When a multi-selection is dragged, Qt moves all the selected
    // items together. Persist the new position of every selected item,
    // not just the one that received the drop event.
    QList<MonitorFixtureItem *> movedItems;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
    {
        MonitorFixtureItem *mfi = dynamic_cast<MonitorFixtureItem *>(gi);
        if (mfi != NULL && m_fixtures.key(mfi, Fixture::invalidId()) != Fixture::invalidId())
            movedItems.append(mfi);
    }

    // make sure the dropped item is always handled even if it
    // somehow ended up unselected
    if (movedItems.contains(item) == false && item != NULL)
        movedItems.append(item);

    // Snap the move as a GROUP: snap the dragged item to the grid, then shift
    // every moved item by the same delta. This keeps the selection's relative
    // layout intact (snapping each item independently would scramble it).
    if (m_snapDivisions > 0 && m_cellPixels > 0 && item != NULL)
    {
        // By its centre, where its light is -- not its icon's corner.
        const QPointF centre = item->pos() + halfIcon(item);
        const QPointF delta = snapScenePos(centre) - centre;
        if (delta.isNull() == false)
        {
            foreach (MonitorFixtureItem *mfi, movedItems)
                mfi->setPos(mfi->pos() + delta);
        }
    }

    /* Free fixtures dragged together onto a truss all hang on it, each where
       it landed along it, when the one under the pointer lands on it -- not
       only those that happen to overlap it ("only moves one"). */
    TrussItem *groupTruss = nullptr;
    if (item != nullptr && movedItems.size() > 1 && m_cellPixels > 0
            && m_doc->monitorProperties()->fixtureRigProps(m_fixtures.key(item, Fixture::invalidId())).trussId
               == Truss::invalidId())
        groupTruss = trussUnderFixture(item, 1.0f, true);

    foreach (MonitorFixtureItem *mfi, movedItems)
    {
        quint32 fid = m_fixtures.key(mfi);

        // Truss binding and snapping.
        //   • Already-bound: constrain to the existing truss line.
        MonitorProperties *props = m_doc->monitorProperties();
        FixtureRigProps rp = props->fixtureRigProps(fid);

        // Convert the fixture's pixel position into millimetres. Every
        // fixture is drawn CENTRED on its position, so read its centre (not
        // its top-left); a free one's store keeps the corner (freeCentreOffsetMm()).
        const QPointF mmCentre = pixelsToRealPosition(mfi->pos().x() + halfIcon(mfi).x(),
                                                      mfi->pos().y() + halfIcon(mfi).y());
        QPointF mmPos = mmCentre - freeCentreOffsetMm(fid);

        auto snapToTruss = [&](Truss *t)
        {
            if (t->type() == Truss::Vertical)
            {
                // Vertical tower: fixture stays at the tower's XY position in the 2D view.
                // Z-height (trussOffset) is set via the dialog strip — don't overwrite it here.
                mmPos = QPointF(t->origin().x() * 1000.0, t->origin().y() * 1000.0);
                props->setFixtureRigProps(fid, rp);
                mfi->setPos(realPositionToPixels(mmPos.x(), mmPos.y()) - halfIcon(mfi));
                return;
            }

            // Horizontal / Ground: project the fixture position onto the truss
            // direction for the ALONG component, but keep whatever perpendicular
            // (cross) offset it was actually dropped at — Branson asked for free
            // placement anywhere touching the truss, not a forced snap onto the
            // centreline. The red escape border (see mouseMoveEvent) already
            // governs how far off is still "touching" vs. a detach; clamp cross
            // to that same limit so the drop can never land past where it would
            // have gone red.
            double ox = t->origin().x() * 1000.0;
            double oy = t->origin().y() * 1000.0;
            double dx = t->direction().x();
            double dy = t->direction().y();
            double along = (mmPos.x() - ox) * dx + (mmPos.y() - oy) * dy;
            along = qBound(0.0, along, t->length() * 1000.0);

            // Snap along the truss to the same grid subdivision intervals.
            if (m_snapDivisions > 0 && m_unitValue > 0)
            {
                double snapMm = m_unitValue / m_snapDivisions;   // the Snap setting's step
                along = qRound(along / snapMm) * snapMm;
                along = qBound(0.0, along, t->length() * 1000.0);
            }

            double cross = (mmPos.x() - ox) * -dy + (mmPos.y() - oy) * dx;
            TrussItem *ti = m_trussItems.value(t->id(), nullptr);
            if (ti != nullptr && m_cellPixels > 0)
            {
                const double crossLimitMm = double(ti->pxWid() * 2.0f) * double(m_unitValue) / double(m_cellPixels);
                cross = qBound(-crossLimitMm, cross, crossLimitMm);
            }

            mmPos = QPointF(ox + dx * along - dy * cross, oy + dy * along + dx * cross);
            rp.trussOffset = float(along / 1000.0);
            rp.trussCross  = float(cross / 1000.0);
            props->setFixtureRigProps(fid, rp);
            mfi->setPos(realPositionToPixels(mmPos.x(), mmPos.y()) - halfIcon(mfi));
        };

        if (rp.trussId != Truss::invalidId() && m_cellPixels > 0)
        {
            if (mfi->escapeMode())
            {
                // Pulled too far off the truss — unbind
                const quint32 wasTrussId = rp.trussId;
                rp.trussId     = Truss::invalidId();
                rp.trussOffset = 0.0f;
                props->setFixtureRigProps(fid, rp);
                mfi->setEscapeMode(false);
                mfi->setBoundToTruss(false);
                // Same reasoning as detachFixtureFromTruss(): stop select/move-
                // together with a truss this fixture just got pulled off of.
                leaveDedicatedTrussGroup(fid, wasTrussId);

                /* mmPos was computed BOUND (the centre is the store); the
                   fixture ends this drop UNBOUND, whose store is the corner.
                   Recompute for the state it will be redrawn in, or the store
                   is half a body off and the re-place shifts it -- "snaps to
                   a different spot when I let go". */
                mmPos = mmCentre - freeCentreOffsetMm(fid);
            }
            else
            {
                Truss *t = props->truss(rp.trussId);

                /* Decide detach from the DROP itself, not only from the
                   escape-mode flag set during the drag. The two measured
                   different things -- escapeMode used the item's scene
                   bounding-box centre, the clamp below uses pos()+halfIcon --
                   so a fixture could be pulled clearly off its truss, fail to
                   trip escapeMode, and then be yanked back by the clamp. That
                   is the "could not move it away from the truss, snapped back
                   to centre" case: the drag was refused by a rule that had
                   already decided you had not moved far enough, using a
                   different ruler from the one doing the refusing. */
                /* One test for the whole question, and the same one the red
                   border used while dragging: is this fixture still over its
                   truss? Deriving detach separately here from mmPos and a
                   cross-limit was a second ruler that could -- and did --
                   disagree with the border the operator had just been shown. */
                TrussItem *tItem = (t != nullptr)
                    ? m_trussItems.value(t->id(), nullptr) : nullptr;
                const bool detach = (tItem == nullptr)
                                  || (trussUnderFixture(mfi) != tItem);

                if (detach)
                {
                    const quint32 wasTrussId = rp.trussId;
                    rp.trussId     = Truss::invalidId();
                    rp.trussOffset = 0.0f;
                    rp.trussCross  = 0.0f;
                    props->setFixtureRigProps(fid, rp);
                    mfi->setEscapeMode(false);
                    mfi->setBoundToTruss(false);
                    leaveDedicatedTrussGroup(fid, wasTrussId);

                    // Same anchor correction as the escape branch above.
                    mmPos = mmCentre - freeCentreOffsetMm(fid);
                }
                else if (t != nullptr)
                    snapToTruss(t);
            }
        }
        // Auto-attach on drop: dropping an unbound fixture so it visually
        // TOUCHES a truss (any overlap, not centered on the line) binds it —
        // matching the explicit right-click "Attach to Truss" / Layers-tree-
        // drag paths, which remain available too. This used to auto-bind on
        // any overlap WHILE DRAGGING across the canvas ("truss grabs
        // overlapping fixtures"), which is why it was pulled entirely; the
        // fix here is scope, not removal — slotFixtureMoved() only runs once,
        // on the completed drop (see itemDropped()), never mid-drag, so
        // passing over a truss on the way somewhere else can't trigger this.
        if (rp.trussId == Truss::invalidId() && m_cellPixels > 0)
        {
            /* Judged by trussUnderFixture(), the same test that turned the
               border green a moment ago. It used to be collidesWithItem(),
               a different and looser rule -- so a fixture could attach on
               release without ever having gone green, or go green and then
               not attach. The promise the colour makes is now the promise the
               drop keeps. */
            TrussItem *over = trussUnderFixture(mfi, 1.0f, true);
            if (over == nullptr)
                over = groupTruss;
            if (over != nullptr)
            {
                attachFixtureToTrussAt(fid, over, mfi);
                rp = props->fixtureRigProps(fid);   // attach just changed this
                /* The final setRealPosition() below must store the BOUND
                   convention (icon centre, on the truss), not the top-left mm
                   this loop derived while the fixture was still free --
                   otherwise it overwrites the cache the attach just fixed. */
                const QVector3D nw = props->fixtureRigPosition(fid);
                mmPos = QPointF(nw.x() * 1000.0, nw.y() * 1000.0);
            }
        }

        /* Same auto-attach, for a Pipe (boom or crossbar) -- previously the
           only ways to mount a fixture on a boom were the context-menu
           "Mount on > Boom" action or dropping it on the Stand's own
           Structure Studio canvas, never a plain drag here. */
        if (rp.trussId == Truss::invalidId() && rp.pipeId == Pipe::invalidId() && m_cellPixels > 0)
        {
            if (PipeItem *over = pipeUnderFixture(mfi, 1.0f, true))
            {
                attachFixtureToPipe(fid, over->pipeId());
                rp = props->fixtureRigProps(fid);
                Pipe *pipe = props->pipe(over->pipeId());
                if (pipe != nullptr && !pipe->isVertical())
                {
                    /* attachFixtureToPipe() has no drop point of its own to
                       work from (the context-menu "Mount on" action doesn't
                       have one either), so it only fills a DEFAULT offset.
                       A horizontal pipe (house electric) has a real ALONG
                       position to read from a Top-view drop, the same as a
                       Horizontal truss -- refine it here. A vertical boom's
                       height cannot be read from a Top-view point at all
                       (the same reason a Vertical truss's own drag-attach
                       leaves its offset alone too), so this only applies to
                       the horizontal case. */
                    const QPointF local = over->mapFromScene(mfi->iconCentreScene());
                    const double mmPerPx = double(m_unitValue) / double(m_cellPixels);
                    const double alongM = qBound(0.0, local.x() * mmPerPx / 1000.0,
                                                 double(pipe->length()));
                    rp.pipeOffset = float(alongM);
                    props->setFixtureRigProps(fid, rp);
                }
                const QVector3D nw = props->fixtureRigPosition(fid);
                mmPos = QPointF(nw.x() * 1000.0, nw.y() * 1000.0);
            }
        }
        mfi->setAttachMode(false);

        /* Riding its deck: the deck is in the same move (a lasso round a
           fountain), so where it lands is on the deck wherever that lands --
           judged against the deck's old place it was "off" and fell off it. */
        const bool ridingDeck = rp.onDeck() && m_platformItems.value(rp.deckPlatformId) != nullptr
                                && m_platformItems.value(rp.deckPlatformId)->isSelected();
        /* Into a fountain: one object, so no Shift -- on its base, or on its
           inner wall's top, in its group (docs/design/FOUNTAIN_WAND.md). */
        bool inFountain = ridingDeck;
        if (!ridingDeck && m_pov == PovTop && rp.trussId == Truss::invalidId()
            && rp.pipeId == Pipe::invalidId() && !rp.onRiser() && !rp.isHeld() && m_cellPixels > 0)
            inFountain = dropIntoFountain(fid, mmCentre);
        if (inFountain)
            rp = props->fixtureRigProps(fid);

        // Auto DECK-MOUNT: unlike trusses, a fixture dropped over a PLATFORM
        // stands on its deck (Z = deck top) — the user asked for this. Dragging
        // it off the platform un-decks it. Riser-face, truss and pipe mounts
        // opt out.
        if (!inFountain && rp.trussId == Truss::invalidId() && rp.pipeId == Pipe::invalidId()
                && !rp.onRiser() && m_cellPixels > 0)
        {
            const QPointF cPx = mfi->pos() + halfIcon(mfi);
            const QPointF cMm = pixelsToRealPosition(cPx.x(), cPx.y());
            const quint32 pid = props->platformIdAt(float(cMm.x() / 1000.0),
                                                     float(cMm.y() / 1000.0));
            /* Onto a deck takes Shift, like any attach; off every deck is
               leaving it, which a plain drag does, as off a truss. */
            const bool offDecks = pid == FixtureRigProps::invalidPlatformId();
            if (pid != rp.deckPlatformId && (offDecks || attachHeld()))
            {
                rp.deckPlatformId   = pid;   // invalid → un-decked
                rp.deckHeightOffset = 0.0f;  // sit on the new deck; edit Z to move it
                props->setFixtureRigProps(fid, rp);
            }
        }

        // Studio-frame member: a top-view drag repositions the fixture WITHIN
        // its group's local frame. Convert the new world position into a
        // group-local offset (frame origin/rotation stay put) rather than
        // storing an absolute position — the derived position then governs
        // rendering (see MonitorProperties::fixtureRigPosition).
        const quint32 frameGid = props->fixtureFrameGroup(fid);
        if (frameGid != 0)
        {
            const QVector3D world(float(mmPos.x() / 1000.0),
                                  float(mmPos.y() / 1000.0),
                                  props->fixtureRigPosition(fid).z());
            FixtureRigProps frp = props->fixtureRigProps(fid);
            frp.groupLocal = props->worldToGroupLocal(frameGid, world);
            props->setFixtureRigProps(fid, frp);
        }

        mfi->setRealPosition(mmPos);
        emit fixtureMoved(fid, mmPos);
    }

    commitMoveUndo();
    updateTrussAnchorLines();
}

void MonitorGraphicsView::commitMoveUndo()
{
    // Commit the undo snapshot if anything actually moved.
    if (m_pendingMoveUndo.isEmpty())
        return;
    QHash<quint32, QPointF> changed;
    QHashIterator<quint32, QPointF> it(m_pendingMoveUndo);
    while (it.hasNext())
    {
        it.next();
        MonitorFixtureItem *mfi = m_fixtures.value(it.key(), NULL);
        if (mfi != NULL && mfi->realPosition() != it.value())
            changed.insert(it.key(), it.value());
    }
    if (changed.isEmpty() == false)
    {
        UndoEntry e; e.type = UndoEntry::FixtureMove; e.fixturePositions = changed;
        pushUndo(e);
    }
    m_pendingMoveUndo.clear();
}

QPointF MonitorGraphicsView::snapScenePos(const QPointF &p) const
{
    if (m_snapDivisions <= 0 || m_cellPixels <= 0)
        return p;
    /* The step is the Snap setting (Full, 1/2, 1/4 of a cell). It was the
       grid's drawn subdivisions, so the setting only switched snapping on
       and off: with a plain grid, "1/4" still snapped to whole cells. */
    const qreal inc = qreal(m_cellPixels) / m_snapDivisions;
    const qreal sx = m_xOffset + qRound((p.x() - m_xOffset) / inc) * inc;
    const qreal sy = m_yOffset + qRound((p.y() - m_yOffset) / inc) * inc;
    return QPointF(sx, sy);
}

void MonitorGraphicsView::captureMoveUndo()
{
    m_pendingMoveUndo.clear();
    QHashIterator<quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext())
    {
        it.next();
        if (it.value() != NULL)
            m_pendingMoveUndo.insert(it.key(), it.value()->realPosition());
    }
}

void MonitorGraphicsView::recordFeaturePaste(const QList<quint32> &trussIds,
                                             const QList<quint32> &platformIds,
                                             const QList<quint32> &targetIds,
                                             const QList<quint32> &paletteIds,
                                             const QList<quint32> &fixtureIds)
{
    if (trussIds.isEmpty() && platformIds.isEmpty() && targetIds.isEmpty())
        return;

    UndoEntry e;
    e.type = UndoEntry::FeaturePaste;
    e.pastedTrussIds    = trussIds;
    e.pastedPlatformIds = platformIds;
    e.pastedTargetIds   = targetIds;
    e.pastedPaletteIds  = paletteIds;
    e.pastedFixtureIds  = fixtureIds;
    pushUndo(e);
}

void MonitorGraphicsView::pushUndo(UndoEntry e)
{
    e.batch = m_openUndoBatch;
    m_moveUndo.append(e);
    while (m_moveUndo.count() > 50)
        m_moveUndo.removeFirst();
    emit undoChanged();
}

void MonitorGraphicsView::undoLastMove()
{
    if (m_moveUndo.isEmpty())
        return;
    // Whatever moved back, its count bubble goes with it.
    auto refreshBadges = qScopeGuard([this]() { refreshFixtureCountBadges(); emit undoChanged(); });
    // The whole drag: every entry it recorded, newest first.
    const UndoEntry last = m_moveUndo.takeLast();
    undoOne(last);
    while (last.batch != 0 && !m_moveUndo.isEmpty() && m_moveUndo.last().batch == last.batch)
        undoOne(m_moveUndo.takeLast());
}

void MonitorGraphicsView::pushRigUndo(const QByteArray &before)
{
    UndoEntry e;
    e.type = UndoEntry::RigEdit;
    e.rigBefore = before;
    pushUndo(e);
}

void MonitorGraphicsView::pushUndoCallback(const std::function<void()> &undo)
{
    if (!undo)
        return;
    UndoEntry e;
    e.type = UndoEntry::Callback;
    e.callback = undo;
    pushUndo(e);
}

void MonitorGraphicsView::undoOne(const UndoEntry &entry)
{
    if (entry.type == UndoEntry::Callback)
    {
        entry.callback();
        return;
    }
    if (entry.type == UndoEntry::RigEdit)
    {
        if (m_rigRestorer)
            m_rigRestorer(entry.rigBefore);
        return;
    }
    if (entry.type == UndoEntry::DuplicateSetup)
    {
        StructureDuplicate::undo(m_doc, entry.dupUndo);
        // The Monitor drops the copies' icons as they go; a bare view does it here.
        foreach (quint32 id, entry.dupUndo.createdFixtures)
            if (m_doc->fixture(id) == nullptr && m_fixtures.contains(id))
                removeFixture(id);
        MonitorProperties *props = m_doc->monitorProperties();
        foreach (const StructureDuplicate::Removed &r, entry.dupUndo.removed)
        {
            const QVector3D at = props->fixtureProperties(r.id).m_baseItem.m_position;
            addFixture(r.id, QPointF(at.x(), at.y()));
            updateFixture(r.id);
        }
        refreshFixtureLabels();
        refreshFixtureCountBadges();
        emit mapStructureChanged();
        return;
    }
    if (entry.type == UndoEntry::ItemMove)
    {
        MonitorProperties *props = m_doc->monitorProperties();
        bool stands = false, power = false, network = false;
        for (int i = 0; i < entry.itemKeys.size(); ++i)
        {
            const QString &kind = entry.itemKeys.at(i).first;
            const quint32 id = entry.itemKeys.at(i).second;
            const QPointF o = entry.itemOrigins.at(i);
            if (kind == QLatin1String("tower"))
            { if (Tower *t = props->tower(id)) { t->setOriginX(float(o.x())); t->setOriginY(float(o.y())); } }
            else if (kind == QLatin1String("stand"))
            { if (Stand *st = props->stand(id)) { st->setOriginX(float(o.x())); st->setOriginY(float(o.y())); stands = true; } }
            else if (kind == QLatin1String("pipe"))
            { if (Pipe *b = props->pipe(id)) { b->setOriginX(float(o.x())); b->setOriginY(float(o.y())); } }
            else if (kind == QLatin1String("scenery"))
            { if (Scenery *sc = props->scenery(id)) { sc->setOriginX(float(o.x())); sc->setOriginY(float(o.y())); } }
            else if (kind == QLatin1String("power"))
            {
                PowerDistribution *pd = m_doc->powerDistribution();
                if (int(id) < pd->sources().size())
                { pd->sources()[int(id)].posX = o.x(); pd->sources()[int(id)].posY = o.y(); power = true; }
            }
            else if (kind == QLatin1String("network"))
            {
                NetworkTopology *nt = m_doc->networkTopology();
                if (int(id) < nt->nodes().size())
                { nt->nodes()[int(id)].posX = o.x(); nt->nodes()[int(id)].posY = o.y(); network = true; }
            }
        }
        if (stands)
            props->recomputeStandMounts();
        updatePlatforms();
        updateTrusses();
        // Anything mounted on what moved back follows it.
        foreach (quint32 fid, m_fixtures.keys())
            updateFixture(fid);
        if (power) updatePowerSources();
        if (network) updateNetworkNodes();
        m_doc->setModified();
        return;
    }

    if (entry.type == UndoEntry::FeaturePaste)
    {
        // The fixtures pasted with a truss first: nothing hangs on a truss
        // as it goes.
        MonitorProperties *props = m_doc->monitorProperties();
        foreach (quint32 id, entry.pastedFixtureIds)
        {
            if (m_doc->fixture(id) != nullptr)
                m_doc->deleteFixture(id);
            // deleteFixture() leaves the mount; a reused id must not inherit it.
            if (props != nullptr)
                props->removeFixtureRigProps(id);
        }
        if (props != nullptr)
        {
            foreach (quint32 id, entry.pastedTrussIds)
                props->removeTruss(id);
            foreach (quint32 id, entry.pastedPlatformIds)
                props->removePlatform(id);
            foreach (quint32 id, entry.pastedTargetIds)
                props->removeStageTarget(id);
        }
        foreach (quint32 id, entry.pastedPaletteIds)
            m_doc->deletePalette(id);

        updateTrusses();
        updatePlatforms();
        updatePowerSources();
        updateNetworkNodes();
        updateTargets();
        m_doc->setModified();
        return;
    }

    if (entry.type == UndoEntry::PlatformMove)
    {
        MonitorProperties *props = m_doc->monitorProperties();
        QHashIterator<quint32, QPointF> it(entry.platformOrigins);
        while (it.hasNext())
        {
            it.next();
            StagePlatform *p = props ? props->platform(it.key()) : nullptr;
            if (p == nullptr)
                continue;
            // The deck's fixtures rode it there (slotPlatformMoved); ride back.
            const float dxMm = float((it.value().x() - double(p->originX())) * 1000.0);
            const float dyMm = float((it.value().y() - double(p->originY())) * 1000.0);
            p->setOriginX(float(it.value().x()));
            p->setOriginY(float(it.value().y()));
            for (auto fit = m_fixtures.constBegin(); fit != m_fixtures.constEnd(); ++fit)
            {
                if (props->fixtureRigProps(fit.key()).deckPlatformId != p->id())
                    continue;
                QVector3D fp = props->fixturePosition(fit.key(), 0, 0);
                fp.setX(fp.x() + dxMm);
                fp.setY(fp.y() + dyMm);
                props->setFixturePosition(fit.key(), 0, 0, fp);
            }
        }
        // And the people who rode it, onto what is under them again.
        for (auto tit = entry.targetPositions.constBegin(); props != nullptr && tit != entry.targetPositions.constEnd(); ++tit)
            if (StageTarget *t = props->stageTarget(tit.key()))
            {
                t->setX(float(tit.value().x()));
                t->setY(float(tit.value().y()));
                t->setZ(props->platformHeightAt(t->x(), t->y()));
            }
        if (!entry.targetPositions.isEmpty())
        {
            updateTargets();
            if (props != nullptr)
                props->notifyStageTargetsChanged();
        }
        if (props != nullptr)
            props->recomputeAnchoredFrames();
        refreshRiserFixtures();
        updatePlatforms();
        updatePowerSources();
        updateNetworkNodes();
        m_doc->setModified();
        return;
    }

    if (entry.type == UndoEntry::TargetBlock)
    {
        MonitorProperties *props = m_doc->monitorProperties();
        for (auto it = entry.targetBlocked.constBegin(); it != entry.targetBlocked.constEnd(); ++it)
        {
            StageTarget *t = props ? props->stageTarget(it.key()) : nullptr;
            if (t == nullptr)
                continue;
            t->setBlocked(entry.blockingSet, it.value());
            reaimScenesAimingAt(t->id());
        }
        updateTargets();
        if (props != nullptr)
            props->notifyStageTargetsChanged();
        m_doc->setModified();
        return;
    }

    if (entry.type == UndoEntry::TargetLineEdit)
    {
        MonitorProperties *props = m_doc->monitorProperties();
        for (auto it = entry.targetLines.constBegin(); it != entry.targetLines.constEnd(); ++it)
        {
            StageTarget *t = props ? props->stageTarget(it.key()) : nullptr;
            if (t == nullptr)
                continue;
            t->setPosition(it.value().first);
            t->setLinePoints(it.value().second);
            reaimScenesAimingAt(t->id());
        }
        updateTargets();
        if (props != nullptr)
            props->notifyStageTargetsChanged();
        m_doc->setModified();
        return;
    }

    if (entry.type == UndoEntry::TargetMove)
    {
        MonitorProperties *props = m_doc->monitorProperties();
        QHashIterator<quint32, QPointF> it(entry.targetPositions);
        while (it.hasNext())
        {
            it.next();
            StageTarget *t = props ? props->stageTarget(it.key()) : nullptr;
            if (t == nullptr)
                continue;
            t->setX(float(it.value().x()));
            t->setY(float(it.value().y()));
            if (entry.targetZ.contains(it.key()))
                t->setZ(entry.targetZ.value(it.key()));
        }
        updateTargets();
        if (props != nullptr)
            props->notifyStageTargetsChanged();
        m_doc->setModified();
        return;
    }

    if (entry.type == UndoEntry::FixtureMove)
    {
        QHashIterator<quint32, QPointF> it(entry.fixturePositions);
        while (it.hasNext())
        {
            it.next();
            MonitorFixtureItem *mfi = m_fixtures.value(it.key(), NULL);
            if (mfi == NULL)
                continue;
            mfi->setRealPosition(it.value());
            updateFixture(it.key());
            emit fixtureMoved(it.key(), it.value());
        }
    }
    else // TrussMove
    {
        QHashIterator<quint32, QVector3D> it(entry.trussOrigins);
        while (it.hasNext())
        {
            it.next();
            TrussItem *ti = m_trussItems.value(it.key(), nullptr);
            if (ti == nullptr || ti->truss() == nullptr)
                continue;
            ti->truss()->setOrigin(it.value());
            // Reposition the pixel item to match the restored origin
            const QVector3D &orig = it.value();
            float pxX = float(m_xOffset + (orig.x() * 1000.0 * m_cellPixels) / m_unitValue);
            float pxY = float(m_yOffset + (orig.y() * 1000.0 * m_cellPixels) / m_unitValue);
            ti->setPos(pxX, pxY);
        }
        m_doc->setModified();
    }
}

void MonitorGraphicsView::moveFixtureTo(quint32 fid, QPointF mmPos)
{
    MonitorFixtureItem *mfi = m_fixtures.value(fid, nullptr);
    if (mfi == nullptr)
        return;
    mfi->setRealPosition(mmPos);
    updateFixture(fid);
}

void MonitorGraphicsView::patchOthersLike(const QString &key, const QStringList &like)
{
    QList<quint32> ids = PatchPlanner::fixturesOn(m_doc, key);
    foreach (const QString &other, like)
        ids << PatchPlanner::fixturesOn(m_doc, other);
    PatchInventoryDialog dlg(m_doc, ids, this);
    dlg.matchStructures(key);
    if (dlg.exec() != QDialog::Accepted)
        return;
    foreach (quint32 f, ids)
        updateFixture(f);
    emit mapStructureChanged();
}

void MonitorGraphicsView::duplicateSetupDialog(const QString &key)
{
    StructureDuplicateDialog dlg(m_doc, key, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    duplicateStructureSetup(key, dlg.targets(), dlg.options());
}

bool MonitorGraphicsView::duplicateStructureSetup(const QString &key, const QStringList &targets,
                                                  const StructureDuplicate::Options &options)
{
    const StructureDuplicate::Preview pv = StructureDuplicate::preview(m_doc, key, targets, options);
    bool changesPatch = pv.replacedFixtures > 0;
    foreach (const StructureDuplicate::Row &r, pv.rows)
        changesPatch = changesPatch || r.patched;
    if (pv.rows.isEmpty() && pv.replacedFixtures == 0)
        return false;
    // The patch is part of a Teaching show's lesson: ask first, as the Fixture Manager does.
    if (changesPatch
        && !ShowSettingsDialog::confirmPatchChange(m_doc, this, tr("Patch the duplicated fixtures")))
        return false;

    const StructureDuplicate::Result res = StructureDuplicate::apply(m_doc, key, targets, options);
    if (res.undo.isEmpty())
        return false;
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (quint32 id, res.createdIds)
    {
        const QVector3D at = props->fixtureProperties(id).m_baseItem.m_position;
        addFixture(id, QPointF(at.x(), at.y()));
    }
    UndoEntry e;
    e.type = UndoEntry::DuplicateSetup;
    e.dupUndo = res.undo;
    pushUndo(e);
    refreshFixtureLabels();
    refreshFixtureCountBadges();
    emit mapStructureChanged();
    return true;
}

int MonitorGraphicsView::standOnDeck(const QList<quint32> &fids, quint32 platformId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    int stood = 0;
    foreach (quint32 fid, fids)
    {
        if (m_doc->fixture(fid) == nullptr)
            continue;
        ensureFixtureOnPlot(fid);
        const QVector3D at = props->fixtureRigPosition(fid);       // metres
        QPointF xy(at.x(), at.y());

        // The deck under it -- or, on an edge or just off one, the nearest
        // within half a metre.
        quint32 pid = platformId;
        if (pid == FixtureRigProps::invalidPlatformId())
        {
            pid = props->platformIdAt(float(xy.x()), float(xy.y()));
            double best = 0.5;
            if (pid == FixtureRigProps::invalidPlatformId())
            {
                foreach (StagePlatform *pl, props->platforms())
                {
                    if (pl == nullptr)
                        continue;
                    const QList<QPointF> fp = pl->footprint(8);
                    for (int i = 0; i < fp.size(); ++i)
                    {
                        const QPointF a = fp.at(i), b = fp.at((i + 1) % fp.size());
                        const QPointF ab = b - a;
                        const double len2 = QPointF::dotProduct(ab, ab);
                        const double t = len2 > 0 ? qBound(0.0, QPointF::dotProduct(xy - a, ab) / len2, 1.0) : 0.0;
                        const QPointF d = xy - (a + ab * t);
                        const double dist = qSqrt(QPointF::dotProduct(d, d));
                        if (dist < best)
                        {
                            best = dist;
                            pid = pl->id();
                        }
                    }
                }
            }
        }
        StagePlatform *pl = props->platform(pid);
        if (pl == nullptr)
            continue;

        /* On it, not on its edge: a fixture lined up with a deck's edge is on
           the boundary, where the deck under it is anyone's guess. In steps
           toward the middle until well inside. */
        const QList<QPointF> fp = pl->footprint(8);
        QPointF mid;
        foreach (const QPointF &q, fp)
            mid += q;
        if (!fp.isEmpty())
            mid /= fp.size();
        auto inside = [pl](const QPointF &p) { return pl->containsPoint(float(p.x()), float(p.y())); };
        auto wellInside = [&](const QPointF &p) {
            const double m = 0.05;
            return inside(p) && inside(p + QPointF(m, 0)) && inside(p - QPointF(m, 0))
                   && inside(p + QPointF(0, m)) && inside(p - QPointF(0, m));
        };
        for (int step = 0; step < 200 && !wellInside(xy); ++step)
        {
            const QPointF d = mid - xy;
            const double len = qSqrt(QPointF::dotProduct(d, d));
            if (len < 0.01)
                break;
            xy += d * (0.02 / len);
        }

        FixtureRigProps rp = props->fixtureRigProps(fid);
        if (rp.deckPlatformId != pid)
            leaveMountGroupsOf(fid);
        rp.clearMounts();
        rp.deckPlatformId = pid;
        rp.deckHeightOffset = 0.0f;
        rp.mountingType = Truss::FloorMounted;     // standing on it: base down
        props->setFixtureRigProps(fid, rp);
        props->setFixturePosition(fid, 0, 0, QVector3D(float(xy.x() * 1000.0), float(xy.y() * 1000.0), 0.0f));
        updateFixture(fid);
        ++stood;
    }
    if (stood > 0)
    {
        m_doc->setModified();
        emit mapStructureChanged();
        refreshItemLayerState();
    }
    return stood;
}
