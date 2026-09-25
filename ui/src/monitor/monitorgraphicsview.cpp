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
#include <QApplication>
#include <QTimer>
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
#include "monitorproperties.h"
#include <cmath>
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "trussitem.h"
#include "studiogroupeditor.h"
#include "studiotemplate.h"
#include "studiocomponentbrowser.h"
#include "platformitem.h"
#include "pipeitem.h"
#include "standitem.h"
#include "toweritem.h"
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
#include "monitor.h"
#include "aimsolver.h"
#include "qlcfixturemode.h"
#include "qlcpalette.h"
#include "scene.h"
#include "fixturegroup.h"
#include "createfixturegroup.h"
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

    // Cmd/Ctrl+Z undoes the last fixture move.
    QShortcut *undoSc = new QShortcut(QKeySequence::Undo, this);
    undoSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoSc, &QShortcut::activated, this, &MonitorGraphicsView::undoLastMove);

    // Cmd/Ctrl+G groups the selection; Cmd/Ctrl+Shift+G ungroups it.
    QShortcut *groupSc = new QShortcut(QKeySequence(tr("Ctrl+G")), this);
    groupSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(groupSc, &QShortcut::activated, this, [this]() { groupSelectedItems(); });
    QShortcut *ungroupSc = new QShortcut(QKeySequence(tr("Ctrl+Shift+G")), this);
    ungroupSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(ungroupSc, &QShortcut::activated, this, [this]() { ungroupSelectedItems(); });

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
    refreshLabelRotations();
    emit rulersChanged();
    viewport()->update();
}

/* Keep every text item upright while the view is turned. The plot's items carry
 * their names as CHILD text items (truss/platform/tower/pipe/stand/target), so
 * one pass over the scene covers them; the two that paint text inline
 * (MonitorFixtureItem, PowerSourceItem) counter-rotate in their own paint(). */
void MonitorGraphicsView::refreshLabelRotations()
{
    const qreal counter = -90.0 * m_viewRotation;
    foreach (QGraphicsItem *it, m_scene->items())
    {
        if (it->type() == QGraphicsSimpleTextItem::Type || it->type() == QGraphicsTextItem::Type)
        {
            it->setTransformOriginPoint(it->boundingRect().center());
            it->setRotation(counter);
        }
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
            if (dynamic_cast<RevealTagItem *>(gi) != nullptr
                && gi->mapRectToScene(gi->boundingRect()).contains(sp))
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

    // Triple-click on a fixture that mouseDoubleClickEvent just isolated (drilled
    // into) rather than opened an editor for: Qt has no native triple-click event,
    // so the third rapid press just arrives here as an ordinary press. Treat it as
    // "open this fixture's editor now" instead of requiring a whole separate
    // second double-click to reach the same result.
    if (event->button() == Qt::LeftButton && m_lastFixtureDoubleClickItem != nullptr
        && m_lastFixtureDoubleClickTimer.isValid()
        && m_lastFixtureDoubleClickTimer.elapsed() <= QApplication::doubleClickInterval())
    {
        // Same resolution mouseDoubleClickEvent used to pick this fixture in the
        // first place (topPickableAt, not plain itemAt) -- keeps the two clicks'
        // hit-testing consistent (it sees through a ghosted click-through fixture).
        QGraphicsItem *hit = topPickableAt(mapToScene(event->pos()));
        MonitorFixtureItem *fi = (hit == m_lastFixtureDoubleClickItem)
            ? dynamic_cast<MonitorFixtureItem *>(hit) : nullptr;
        m_lastFixtureDoubleClickItem = nullptr;
        m_lastFixtureDoubleClickTimer.invalidate();
        if (fi != nullptr)
        {
            m_suppressNextViewClick = true;
            emit fixtureDoubleClicked(fi->fixtureID());
            event->accept();
            return;
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
            hit->setSelected(!hit->isSelected());
            if (hit->isSelected())
                captureMoveUndo();
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

    foreach (QGraphicsLineItem *li, m_trussAnchorLines)
    {
        m_scene->removeItem(li);
        delete li;
    }
    m_trussAnchorLines.clear();

    // Scene was created without a parent — delete it explicitly.
    delete m_scene;
    m_scene = nullptr;
}

void MonitorGraphicsView::showEvent(QShowEvent *event)
{
    QGraphicsView::showEvent(event);
    // paintEvent() does the real work; this just guarantees one repaint.
    update();
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

    if ((size() != m_fittedSize || m_gridSize != m_fittedGrid
            || !qFuzzyCompare(m_unitValue, m_fittedUnit))
            && width() > 0 && height() > 0)
    {
        m_fittedSize = size();
        m_fittedGrid = m_gridSize;
        m_fittedUnit = m_unitValue;
        updateGrid();
        refreshAllItems();
        emit rulersChanged();
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

void MonitorGraphicsView::setGridSize(QSize size)
{
    m_gridAuthoritative = true;
    m_gridSize = size;
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
    // lock and its layer lock. Targets stay movable when only the layout is
    // locked (the operator still nudges aim targets for the selected scene) —
    // that exception lives in refreshItemLayerState.
    refreshItemLayerState();
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
        // Keep groups consistent: any group represented in the selection follows
        // its items onto the target layer (subtree included), so its folder in
        // the Layers tree stays under the right layer.
        MonitorProperties *props = m_doc->monitorProperties();
        QSet<quint32> tops;
        foreach (QGraphicsItem *gi, m_scene->selectedItems())
        {
            const quint32 g = itemGroupId(gi);
            if (g != 0 && props->hasGroup(g))
                tops.insert(topLevelGroup(g));
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

int MonitorGraphicsView::alignSelectedFixtures(int mode)
{
    if (m_pov != PovTop)   // alignment is an XY (top-view) operation
        return 0;
    MonitorProperties *props = m_doc->monitorProperties();
    auto items = collectAlignableFixtures(m_scene->selectedItems(), m_fixtures, props);
    if (items.size() < 2)
        return 0;

    // Gather current mm positions.
    double sumX = 0, sumY = 0, minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
    for (const auto &it : items)
    {
        const QPointF p = it.second->realPosition();
        sumX += p.x(); sumY += p.y();
        minX = qMin(minX, p.x()); maxX = qMax(maxX, p.x());
        minY = qMin(minY, p.y()); maxY = qMax(maxY, p.y());
    }
    const double avgX = sumX / items.size(), avgY = sumY / items.size();

    captureMoveUndo();
    for (const auto &it : items)
    {
        QPointF p = it.second->realPosition();
        switch (mode)
        {
            case 0: p.setX(avgX); break;   // same X (column)
            case 1: p.setX(minX); break;   // left
            case 2: p.setX(maxX); break;   // right
            case 3: p.setY(avgY); break;   // same Y (row)
            case 4: p.setY(minY); break;   // top (upstage)
            case 5: p.setY(maxY); break;   // bottom (downstage)
            default: break;
        }
        it.second->setRealPosition(p);
        it.second->setPos(realPositionToPixels(p.x(), p.y()));
        props->setFixturePosition(it.first, 0, 0, QVector3D(p.x(), p.y(), 0));
        emit fixtureMoved(it.first, p);
    }
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

    // Sort by the axis, then evenly space between the two extremes.
    std::sort(items.begin(), items.end(), [horizontal](const auto &a, const auto &b) {
        return horizontal ? a.second->realPosition().x() < b.second->realPosition().x()
                          : a.second->realPosition().y() < b.second->realPosition().y();
    });
    const double lo = horizontal ? items.first().second->realPosition().x()
                                 : items.first().second->realPosition().y();
    const double hi = horizontal ? items.last().second->realPosition().x()
                                 : items.last().second->realPosition().y();
    const double step = (hi - lo) / double(items.size() - 1);

    captureMoveUndo();
    for (int i = 0; i < items.size(); i++)
    {
        QPointF p = items[i].second->realPosition();
        if (horizontal) p.setX(lo + step * i); else p.setY(lo + step * i);
        items[i].second->setRealPosition(p);
        items[i].second->setPos(realPositionToPixels(p.x(), p.y()));
        props->setFixturePosition(items[i].first, 0, 0, QVector3D(p.x(), p.y(), 0));
        emit fixtureMoved(items[i].first, p);
    }
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
    { if (Truss *t = ti->truss()) t->setLayerId(layerId); }
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
    else if (auto *twi = dynamic_cast<TowerItem *>(gi))
    { if (Tower *t = twi->tower()) t->setLayerId(layerId); }
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

    // ...and each tower.
    foreach (TowerItem *i, m_towerItems)
        ensureTowerGroup(i->towerId());
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

void MonitorGraphicsView::ensureTowerGroup(quint32 towerId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    Tower *t = props->tower(towerId);
    if (t == nullptr)
        return;

    QList<quint32> mounted;
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
        if (props->fixtureRigProps(it.key()).towerId == towerId)
            mounted << it.key();
    if (mounted.isEmpty())
        return;

    // Only report a change when there is one (see ensurePlatformGroup()).
    bool changed = false;
    quint32 gid = t->groupId();
    if (gid == 0 || !props->hasGroup(gid))
    {
        gid = props->nextGroupId();
        const QString nm = t->name().isEmpty() ? tr("Tower %1").arg(towerId) : t->name();
        props->createGroup(gid, nm, t->layerId(), 0);
        changed = true;
    }
    if (t->groupId() != gid)
        changed = true;
    t->setGroupId(gid);

    foreach (quint32 fid, mounted)
    {
        if (props->fixtureGroup(fid) == 0)
        {
            props->setFixtureGroup(fid, gid);
            changed = true;
        }
    }

    // Anchored to the tower only when the group is its own; a manual group of
    // several structures stays a folder, with the tower's row as the container.
    const QString newAnchorKind = (structuralMembersOf(gid) <= 1) ? QStringLiteral("tower") : QString();
    const quint32 newAnchorId   = (structuralMembersOf(gid) <= 1) ? towerId : 0u;
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

void MonitorGraphicsView::leaveDedicatedTowerGroup(quint32 fid, quint32 towerId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    const quint32 gid = props->fixtureGroup(fid);
    if (gid == 0)
        return;
    const MonitorProperties::MonitorGroup g = props->group(gid);
    if (g.anchorKind != QStringLiteral("tower") || g.anchorId != towerId)
        return;   // a manual group, or anchored elsewhere -- leave it alone
    props->setFixtureGroup(fid, 0);
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
    return n;
}

int MonitorGraphicsView::groupSelectedItems()
{
    MonitorProperties *props = m_doc->monitorProperties();
    QList<QGraphicsItem *> sel = m_scene->selectedItems();

    // Count the distinct "units" in the selection: loose items plus distinct
    // top-level groups. Grouping only makes sense when combining >= 2 units
    // (otherwise you'd just wrap a single existing group in a pointless layer).
    QSet<quint32> topGroups;
    int loose = 0;
    foreach (QGraphicsItem *gi, sel)
    {
        const quint32 g = itemGroupId(gi);
        if (g == 0) loose++;
        else        topGroups.insert(topLevelGroup(g));
    }
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
    foreach (QGraphicsItem *gi, sel)
    {
        if (itemGroupId(gi) == 0)
        {
            setItemGroupId(gi, pid);
            setItemLayer(gi, layer);
        }
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
        const QVector3D origin = (pl != nullptr)
            ? QVector3D(pl->originX(), pl->originY(), 0.0f)
            : (members.isEmpty() ? QVector3D() : (sum / float(members.size())));
        props->setGroupFrame(groupId, origin, 0.0f);
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
    QGraphicsItem *target = nullptr;
    if (kind == QStringLiteral("fixture"))
        target = m_fixtures.value(id, nullptr);
    else if (kind == QStringLiteral("truss"))
        target = m_trussItems.value(id, nullptr);
    else if (kind == QStringLiteral("platform"))
        target = m_platformItems.value(id, nullptr);
    else if (kind == QStringLiteral("pipe"))
        target = m_pipeItems.value(id, nullptr);
    else if (kind == QStringLiteral("stand"))
        target = m_standItems.value(id, nullptr);
    else if (kind == QStringLiteral("tower"))
        target = m_towerItems.value(id, nullptr);
    else if (kind == QStringLiteral("target"))
        target = m_targetItems.value(id, nullptr);
    else if (kind == QStringLiteral("power"))
        target = (int(id) < m_powerSourceItems.size()) ? m_powerSourceItems.at(int(id)) : nullptr;
    else if (kind == QStringLiteral("network"))
        target = (int(id) < m_networkNodeItems.size()) ? m_networkNodeItems.at(int(id)) : nullptr;
    else if (kind == QStringLiteral("image"))
        target = m_imageItems.value(id, nullptr);

    // Selecting from the tree targets exactly ONE item so it can be edited /
    // repositioned individually — suppress the select-together group extension.
    m_extendingSelection = true;
    m_scene->clearSelection();
    if (target)
    {
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
    else if (kind == QStringLiteral("pipe"))     emit pipeDoubleClicked(id);
    else if (kind == QStringLiteral("stand"))    emit standDoubleClicked(id);
    else if (kind == QStringLiteral("tower"))    emit towerDoubleClicked(id);
    else if (kind == QStringLiteral("image"))    emit imageDoubleClicked(id);
    else if (kind == QStringLiteral("power"))    emit powerDoubleClicked(id);
    else if (kind == QStringLiteral("network"))  emit networkDoubleClicked(id);
    else if (kind == QStringLiteral("target"))   emit targetDoubleClicked(id);
}

void MonitorGraphicsView::selectMapItems(const QList<QPair<QString, quint32> > &items)
{
    m_extendingSelection = true;   // suppress select-together until we're done
    m_scene->clearSelection();
    for (const QPair<QString, quint32> &it : items)
    {
        const QString &kind = it.first;
        const quint32 id = it.second;
        QGraphicsItem *target = nullptr;
        if (kind == QStringLiteral("fixture"))       target = m_fixtures.value(id, nullptr);
        else if (kind == QStringLiteral("truss"))    target = m_trussItems.value(id, nullptr);
        else if (kind == QStringLiteral("platform")) target = m_platformItems.value(id, nullptr);
        else if (kind == QStringLiteral("pipe"))     target = m_pipeItems.value(id, nullptr);
        else if (kind == QStringLiteral("stand"))    target = m_standItems.value(id, nullptr);
        else if (kind == QStringLiteral("tower"))    target = m_towerItems.value(id, nullptr);
        else if (kind == QStringLiteral("target"))   target = m_targetItems.value(id, nullptr);
        else if (kind == QStringLiteral("power"))
            target = (int(id) < m_powerSourceItems.size()) ? m_powerSourceItems.at(int(id)) : nullptr;
        else if (kind == QStringLiteral("network"))
            target = (int(id) < m_networkNodeItems.size()) ? m_networkNodeItems.at(int(id)) : nullptr;
        else if (kind == QStringLiteral("image"))    target = m_imageItems.value(id, nullptr);
        if (target)
            target->setSelected(true);
    }
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
    if (kind == QStringLiteral("tower"))    return m_towerItems.value(id, nullptr);
    if (kind == QStringLiteral("target"))   return m_targetItems.value(id, nullptr);
    if (kind == QStringLiteral("power"))
        return (int(id) < m_powerSourceItems.size()) ? m_powerSourceItems.at(int(id)) : nullptr;
    if (kind == QStringLiteral("network"))
        return (int(id) < m_networkNodeItems.size()) ? m_networkNodeItems.at(int(id)) : nullptr;
    if (kind == QStringLiteral("image"))    return m_imageItems.value(id, nullptr);
    return nullptr;
}

void MonitorGraphicsView::reparentToLayer(const QList<QPair<QString, quint32> > &items, quint32 layerId)
{
    int n = 0;
    for (const QPair<QString, quint32> &it : items)
    {
        QGraphicsItem *gi = itemFor(it.first, it.second);
        if (gi == nullptr)
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
    // When the plot is LOCKED nothing moves, so select-together (whose whole point
    // is dragging a group as one) just gets in the way of picking a single item to
    // inspect. Locked → single-click selects the individual; leave it un-extended.
    if (m_layoutLocked)
        return;
    m_extendingSelection = true;

    MonitorProperties *props = m_doc->monitorProperties();

    // Selecting any grouped item selects its whole OUTERMOST group subtree —
    // EXCEPT for a dedicated structural group (anchorKind set: the auto-group
    // ensureTrussGroup()/ensurePlatformGroup() maintains for a truss/riser and
    // its rigged fixtures). That relationship is parent→child, not peers:
    // the truss already carries its bound fixtures when IT moves (see
    // slotTrussMoved(), independent of selection), but a fixture being
    // repositioned relative to its truss must not drag the truss along too.
    // So only extend a dedicated group when the anchor ITSELF (the truss/
    // platform) is what got selected — clicking a member stays solo. A group
    // the user has built out further manually (anchor cleared once it holds
    // more than one structural item — see structuralMembersOf()) falls back
    // to the normal symmetric select-together below.
    QSet<quint32> tops;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
    {
        const quint32 g = itemGroupId(gi);
        if (g == 0)
            continue;
        const quint32 top = topLevelGroup(g);
        const MonitorProperties::MonitorGroup grp = props->group(top);
        if (!grp.anchorKind.isEmpty() && !isGroupAnchorItem(gi, grp.anchorKind, grp.anchorId))
            continue;
        tops.insert(top);
    }
    foreach (quint32 top, tops)
        foreach (QGraphicsItem *gi, itemsUnderGroup(top))
            if (!gi->isSelected())
                gi->setSelected(true);

    m_extendingSelection = false;
    updateTrussAnchorLines();   // tether color follows the truss's selected state
    emit mapSelectionChanged();
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

    // Lock semantics differ by SOURCE (per the user's model):
    //   • LAYER lock   → the item can't be moved AND can't be selected.
    //   • per-item lock / global "Edit Plot" lock → can't be moved, but CAN
    //     still be selected (so it can be inspected / unlocked / rubber-banded).
    // A helper keeps ItemIsSelectable in sync and drops a stale selection when an
    // item becomes unselectable.
    auto applySelectable = [](QGraphicsItem *gi, bool selectable) {
        gi->setFlag(QGraphicsItem::ItemIsSelectable, selectable);
        if (!selectable && gi->isSelected())
            gi->setSelected(false);
    };

    // Fixtures (no per-item lock): layer drives visibility + selectability;
    // movable unless the global layout or the layer is locked.
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
    {
        const MonitorProperties::MonitorLayer lyr = props->layer(props->fixtureLayer(it.key()));
        const bool grpLock = props->groupChainLocked(props->fixtureGroup(it.key()));
        it.value()->setVisible(fixtureItemShouldBeVisible(it.key()));
        // Ghosted (faint) exactly when it's actually non-interactive — a
        // locked layer already makes it unselectable/click-through below;
        // this just makes that visible instead of a silent trap.
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

    // Targets: aim points stay draggable in Design mode regardless of the global
    // layout lock, but a locked layer still freezes them; a hidden layer hides.
    foreach (TargetItem *ti, m_targetItems)
    {
        StageTarget *t = ti->target();
        const MonitorProperties::MonitorLayer lyr = props->layer(t ? t->layerId() : 0);
        const bool grpLock = props->groupChainLocked(t ? t->groupId() : 0);
        ti->setVisible(lyr.visible && !m_stageOnly);
        /* t->locked() belongs in here. It was left out, so the per-target
           "Lock Target" menu item set a flag that this function overwrote on
           the very next refresh -- locking a target did nothing at all, while
           the equivalent line for trusses above has always honoured it. */
        ti->setMovable(m_doc->mode() == Doc::Design
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
            it.value()->setMovable(bound ? elevationFixtureDraggable(it.key())
                                         : !frozen);
        }
        /* Trusses: a tower crossbar has always been draggable here, and a
           free-standing truss now is too -- dragging one up or down in Front is
           how you set its trim height, and refusing it silently was the "can't
           move the XL-450 and it is unlocked" report. Locks still apply. */
        foreach (TrussItem *i, m_trussItems)
        {
            Truss *t = i->truss();
            const bool locked = (t != nullptr && t->locked()) || m_layoutLocked;
            i->setMovable(elevationBarDraggable(t) || !locked);
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
            i->setMovable(m_doc->mode() == Doc::Design && !(t && t->locked()));
        }
        foreach (PipeItem *i, m_pipeItems)               i->setMovable(false);
        foreach (StandItem *i, m_standItems)             i->setMovable(false);
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

QRect MonitorGraphicsView::selectionViewportRect() const
{
    const QList<MonitorFixtureItem *> items = selectedFixtureItems();
    if (items.isEmpty())
        return QRect();
    QRectF scene;
    foreach (MonitorFixtureItem *it, items)
        scene = scene.isNull() ? it->sceneBoundingRect()
                               : scene.united(it->sceneBoundingRect());
    return mapFromScene(scene).boundingRect();
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
    else if (kind == QStringLiteral("tower"))    host = m_towerItems.value(id, nullptr);
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
        disc->setRect(0, 0, d, d);
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
        num->setPos((d - tb.width()) * 0.5, (d - tb.height()) * 0.5);

        const QGraphicsItem *hostItem =
              (sk.first == QStringLiteral("truss"))    ? (QGraphicsItem *)m_trussItems.value(sk.second, nullptr)
            : (sk.first == QStringLiteral("tower"))    ? (QGraphicsItem *)m_towerItems.value(sk.second, nullptr)
            : (sk.first == QStringLiteral("pipe"))     ? (QGraphicsItem *)m_pipeItems.value(sk.second, nullptr)
            : (QGraphicsItem *)m_platformItems.value(sk.second, nullptr);
        const QPointF c = centreOf(hostItem);
        badge->setPos(c.x() - d * 0.5, c.y() - d * 0.5);
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

    QRectF r(0, 0, this->width(), this->height());
    r |= m_scene->itemsBoundingRect();
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
    const double w = double(m_gridSize.width())  * double(m_unitValue);
    const double d = double(m_gridSize.height()) * double(m_unitValue);

    foreach (quint32 fid, props->fixtureItemsID())
    {
        const QVector3D p = props->fixturePosition(fid, 0, 0);   // mm
        if (p.x() < 0.0f || p.y() < 0.0f || double(p.x()) > w || double(p.y()) > d)
            out << fid;
    }
    return out;
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
    return m_yOffset + qreal(m_gridSize.height()) * m_cellPixels;
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
        if (xMetres < p->originX() || xMetres > p->originX() + p->width())
            continue;
        if (yMetres < p->originY() || yMetres > p->originY() + p->depth())
            continue;
        best = qMax(best, p->height());
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
    emit rulersChanged();
}

// The fixture's 3-D long-axis (unit) direction, from its studio orientation.
// Mirrors StudioPlaneView::fixtureAxis so the main monitor and the studio editor
// agree. Defaults (Front face, angle 0) → along X, which is the common tape case.
static QVector3D fixtureLongAxis(const FixtureRigProps &rp)
{
    const double th = qDegreesToRadians(double(rp.studioAngle));
    const double c = qCos(th), s = qSin(th);
    switch (rp.studioMount)
    {
    case 0:  return QVector3D(float(c), float(s), 0.0f);   // Top/deck: X-Y
    case 2:  return QVector3D(0.0f, float(c), float(s));   // Side: Y-Z
    case 1:
    default: return QVector3D(float(c), 0.0f, float(s));   // Front/face: X-Z
    }
}

void MonitorGraphicsView::updateFixture(quint32 id)
{
    scheduleNetworkRuns();
    Fixture *fxi = m_doc->fixture(id);
    if (fxi == NULL || m_fixtures.contains(id) == false)
        return;

    const QLCFixtureMode *mode = fxi->fixtureMode();
    double width = 0;
    double height = 0;

    if (mode != 0)
    {
        width = mode->physical().width();
        height = mode->physical().height();
    }

    if (width <= 0 || height <= 0)
    {
        // No physical layout declared: most multi-head fixtures like this are
        // linear (LED tape / battens / pixel bars), so assume ONE ROW instead
        // of a square (which would make the heads wrap into a block). A very
        // wide:thin aspect makes setSize() lay the heads out in a single row.
        const int n = qMax(1, fxi->heads());
        width = 300.0;
        height = (n > 1) ? 300.0 / n : 300.0;
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
    if (isElevation())
    {
        const QVector3D axis = fixtureLongAxis(m_doc->monitorProperties()->fixtureRigProps(id));
        const double hAxis = (m_pov == PovFront) ? qAbs(double(axis.x())) : qAbs(double(axis.y()));
        const double vAxis = qAbs(double(axis.z()));   // vertical screen axis is Z in both
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
    item->setSize(QSize(qMax(1, qRound(pw)), qMax(1, qRound(ph))));
    item->setElevationView(isElevation());

    // Riser-mounted fixtures derive their position from the platform in EVERY
    // view (top too — they sit on the riser's edge/face). Free/truss fixtures
    // keep their stored XY in top view and use the 3-D rig pos in elevation.
    const FixtureRigProps frp = m_doc->monitorProperties()->fixtureRigProps(id);
    // Riser-, pipe-, tower- and STUDIO-FRAME fixtures derive their position from
    // the structure/frame in EVERY view (they follow it), so centre the icon on
    // the derived point — otherwise the top view shows the stale stored XY (e.g.
    // a studio group distributed on a step looked un-distributed in 2D).
    const bool riser = frp.onRiser() || frp.onPipe() || frp.onTower()
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
            const QPointF p = realPositionToPixels(item->realPosition().x(), item->realPosition().y());
            item->setPos(p);
        }
    }
    else
    {
        const QVector3D w = m_doc->monitorProperties()->fixtureRigPosition(id);
        const QPointF p = projectMm(w.x() * 1000.0, w.y() * 1000.0, w.z() * 1000.0);
        if (frp.onDeck())
            // Stand on the deck: the fixture's BASE sits at Z, so lift the icon
            // up by its full height and centre it horizontally over its X.
            item->setPos(p.x() - pw / 2.0, p.y() - ph);
        else
            item->setPos(onTruss ? (p - halfIcon(item)) : p);
    }

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
        if (auto *ii = dynamic_cast<MonitorImageItem *>(gi))
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

    refreshItemLayerState();
}

bool MonitorGraphicsView::elevationFixtureDraggable(quint32 fid) const
{
    if (!isElevation())
        return false;   // Front or Side elevation
    const FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(fid);
    return rp.trussId != Truss::invalidId();   // slides along its truss/bar
}

TrussItem *MonitorGraphicsView::trussUnderFixture(MonitorFixtureItem *mfi,
                                                  float widths, bool forAttach) const
{
    if (mfi == nullptr)
        return nullptr;

    // The ICON's centre, not sceneBoundingRect()'s -- the label pulls that
    // one down and makes it depend on whether labels happen to be showing
    // on this layer, disagreeing with the deck-mount attach path's own
    // (stable) reading of the same fixture. See WORKFLOW-SUBTLETIES.md #10.
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
            if (auto *ti = dynamic_cast<TrussItem *>(gi)) moved.append(ti);
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
        if (auto *ti = dynamic_cast<TrussItem *>(gi))
            moved.append(ti);
    if (item != nullptr && !moved.contains(item))
        moved.append(item);

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
        m_moveUndo.append(e);
        while (m_moveUndo.count() > 50)
            m_moveUndo.removeFirst();
        m_doc->setModified();
    }

    // Bound fixtures are physically attached: follow the truss to its new place.
    MonitorProperties *props = m_doc->monitorProperties();
    foreach (TrussItem *ti, moved)
    {
        Truss *t = ti->truss();
        if (t == nullptr)
            continue;
        for (auto fit = m_fixtures.constBegin(); fit != m_fixtures.constEnd(); ++fit)
        {
            const FixtureRigProps rp = props->fixtureRigProps(fit.key());
            if (rp.trussId != t->id())
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
}

void MonitorGraphicsView::updatePlatforms()
{
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
    foreach (TowerItem *twi, m_towerItems)
    {
        m_scene->removeItem(twi);
        delete twi;
    }
    m_towerItems.clear();

    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr || m_cellPixels == 0)
        return;

    const float scale = float(m_cellPixels) / m_unitValue;
    foreach (StagePlatform *p, props->platforms())
    {
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
        }

        PlatformItem *pi = new PlatformItem(p, m_doc, pxX, pxY, pxW, pxD);
        pi->setMovable(!m_layoutLocked);
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
        }
        else
        {
            const QPointF base = projectMm(tw->originX() * 1000.0, tw->originY() * 1000.0, 0.0);
            const QPointF top  = projectMm(tw->originX() * 1000.0, tw->originY() * 1000.0, tw->height() * 1000.0);
            const float pxW = float(((m_pov == PovFront) ? tw->width() : tw->depth()) * 1000.0f * m_cellPixels / m_unitValue);
            const float pxH = float(qAbs(top.y() - base.y()));
            twi = new TowerItem(tw, m_doc, float(base.x()), float(base.y()), pxW, pxH, true);
        }
        twi->setMovable(!m_layoutLocked);
        twi->showLabel(m_doc->monitorProperties()->layer(tw->layerId()).labels);
        m_scene->addItem(twi);
        m_towerItems.insert(tw->id(), twi);
        connect(twi, &TowerItem::itemDropped, this, &MonitorGraphicsView::slotTowerMoved);
    }

    refreshItemLayerState();
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
        if (auto *ti = dynamic_cast<TowerItem *>(gi))
            moved.append(ti);
    if (!moved.contains(item))
        moved.append(item);

    foreach (TowerItem *ti, moved)
    {
        Tower *tw = ti->tower();
        if (tw == nullptr)
            continue;
        const QPointF c = ti->pos();
        tw->setOriginX(float(((c.x() - m_xOffset) * m_unitValue) / (1000.0 * m_cellPixels)));
        tw->setOriginY(float(((c.y() - m_yOffset) * m_unitValue) / (1000.0 * m_cellPixels)));
        // Fixtures on its shelves follow (derived).
        foreach (Fixture *fx, m_doc->fixtures())
            if (m_doc->monitorProperties()->fixtureRigProps(fx->id()).towerId == tw->id())
                updateFixture(fx->id());
    }
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
        if (auto *si = dynamic_cast<StandItem *>(gi))
            moved.append(si);
    if (!moved.contains(item))
        moved.append(item);

    foreach (StandItem *si, moved)
    {
        Stand *s = si->stand();
        if (s == nullptr)
            continue;
        const QPointF c = si->pos();
        s->setOriginX(float(((c.x() - m_xOffset) * m_unitValue) / (1000.0 * m_cellPixels)));
        s->setOriginY(float(((c.y() - m_yOffset) * m_unitValue) / (1000.0 * m_cellPixels)));
    }
    m_doc->monitorProperties()->recomputeStandMounts();   // pipes on it follow
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
        if (auto *pi = dynamic_cast<PipeItem *>(gi))
            moved.append(pi);
    if (!moved.contains(item))
        moved.append(item);

    foreach (PipeItem *pi, moved)
    {
        Pipe *b = pi->pipe();
        if (b == nullptr)
            continue;
        const QPointF c = pi->pos();   // base centre in scene pixels
        b->setOriginX(float(((c.x() - m_xOffset) * m_unitValue) / (1000.0 * m_cellPixels)));
        b->setOriginY(float(((c.y() - m_yOffset) * m_unitValue) / (1000.0 * m_cellPixels)));
    }
    m_doc->setModified();
    emit mapStructureChanged();
}

void MonitorGraphicsView::slotPlatformMoved(PlatformItem *item)
{
    // Persist every selected platform (group move), not just the dropped one.
    QList<PlatformItem *> moved;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (auto *pi = dynamic_cast<PlatformItem *>(gi))
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
                QVector3D fp = mprops->fixturePosition(it.key(), 0, 0);   // mm
                fp.setX(fp.x() + dxMm);
                fp.setY(fp.y() + dyMm);
                mprops->setFixturePosition(it.key(), 0, 0, fp);
            }
            emit platformMoved(pi->platformId(),
                               QPointF(double(p->originX()), double(p->originY())));
        }
    }

    if (!e.platformOrigins.isEmpty())
    {
        m_moveUndo.append(e);
        while (m_moveUndo.count() > 50)
            m_moveUndo.removeFirst();
        m_doc->setModified();
    }

    // Re-slave platform-anchored studio frames, then let mounted AND frame
    // fixtures follow the platforms to their new position.
    m_doc->monitorProperties()->recomputeAnchoredFrames();
    refreshRiserFixtures();
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
    pd->sources()[sourceIndex].locked = !pd->sources()[sourceIndex].locked;
    m_doc->setModified();
    // Rebuild so the item picks up its new movability, cursor and red tint.
    updatePowerSources();
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
        if (auto *ps = dynamic_cast<PowerSourceItem *>(gi))
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
    foreach (PowerSourceItem *ps, moved)
    {
        const int idx = ps->sourceIndex();
        if (idx < 0 || idx >= pd->sources().size())
            continue;
        const QPointF mm = pixelsToRealPosition(ps->pos().x(), ps->pos().y());
        pd->sources()[idx].placed = true;
        pd->sources()[idx].posX = mm.x() / 1000.0;
        pd->sources()[idx].posY = mm.y() / 1000.0;
        any = true;
    }
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

    const bool feet = (props->gridUnits() == MonitorProperties::Feet);
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
        QString stats = tr("%1 · Port %2 · Universe %3 · %4 fixture(s) · ~%5 %6 · %7/512 ch")
                            .arg(from).arg(run.portIndex + 1).arg(run.universe + 1)
                            .arg(run.fixtures.size())
                            .arg(run.lengthMetres * (feet ? 3.28084 : 1.0), 0, 'f', 1)
                            .arg(feet ? tr("ft") : tr("m")).arg(run.channels);
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
    nt->nodes()[nodeIndex].locked = !nt->nodes()[nodeIndex].locked;
    m_doc->setModified();
    // Rebuild so the item picks up its new movability, cursor and red tint.
    updateNetworkNodes();
}

void MonitorGraphicsView::slotNetworkNodeMoved(NetworkNodeItem *item)
{
    NetworkTopology *nt = m_doc->networkTopology();

    // Persist every selected node (group move), not just the dropped one.
    QList<NetworkNodeItem *> moved;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (auto *nn = dynamic_cast<NetworkNodeItem *>(gi))
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
    foreach (NetworkNodeItem *nn, moved)
    {
        const int idx = nn->nodeIndex();
        if (idx < 0 || idx >= nt->nodes().size())
            continue;
        const QPointF mm = pixelsToRealPosition(nn->pos().x(), nn->pos().y());
        nt->nodes()[idx].placed = true;
        nt->nodes()[idx].posX = mm.x() / 1000.0;
        nt->nodes()[idx].posY = mm.y() / 1000.0;
        any = true;
    }
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
        const QVector3D p = t->position();
        // realPositionToPixels expects millimetres (StageTarget is in metres).
        const QPointF px = realPositionToPixels(p.x() * 1000.0, p.y() * 1000.0);
        ti->setScenePos(float(px.x()), float(px.y()));
    }
    updateAimLines();   // keep aim lines tracking the moved target
}

void MonitorGraphicsView::updateTargets()
{
    QScopedValueRollback<bool> rb(m_itemsRebuilding); m_itemsRebuilding = true;
    foreach (TargetItem *ti, m_targetItems)
    {
        m_scene->removeItem(ti);
        delete ti;
    }
    m_targetItems.clear();

    // Aim lines are tied to target selection; rebuild them too.
    foreach (QGraphicsLineItem *li, m_aimLines)
    {
        m_scene->removeItem(li);
        delete li;
    }
    m_aimLines.clear();

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
    QSet<quint32> aimedTargetIds;
    if (m_activeSceneId != Function::invalidId())
    {
        Scene *activeScene = qobject_cast<Scene *>(m_doc->function(m_activeSceneId));
        if (activeScene)
        {
            foreach (quint32 pid, activeScene->palettes())
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
        const QPointF tp = projectMm(t->x() * 1000.0, t->y() * 1000.0, t->z() * 1000.0);
        float pxX = float(tp.x());
        float pxY = float(tp.y());

        TargetItem *ti = new TargetItem(t, m_doc, pxX, pxY);
        /* A Person is drawn standing where the aim rule stands it: on the deck
           under it, with its chest at the aim height. Taking both from
           targetAimPoint() keeps the figure and the beams on one answer. */
        /* An Area or a drum kit at its real size: width across the stage,
           depth up and down it. From the front only the width shows, from the
           side only the depth. */
        if ((t->kind() == StageTarget::Kind::Area || t->kind() == StageTarget::Kind::DrumKit)
            && t->footprint().isEmpty() == false)
        {
            const double ppm = 1000.0 * m_cellPixels / m_unitValue;
            const QSizeF fp = t->footprint();
            if (m_pov == PovTop)
                ti->setFootprintPx(QSizeF(fp.width() * ppm, fp.height() * ppm));
            else if (m_pov == PovFront)
                ti->setFootprintPx(QSizeF(fp.width() * ppm, 0.0));
            else
                ti->setFootprintPx(QSizeF(fp.height() * ppm, 0.0));
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
        ti->setMovable(m_doc->mode() == Doc::Design);
        ti->setAimed(aimedTargetIds.contains(t->id()));
        m_scene->addItem(ti);
        m_targetItems.insert(t->id(), ti);

        connect(ti, &TargetItem::itemDropped,
                this, &MonitorGraphicsView::slotTargetMoved);
    }

    refreshItemLayerState();
    updateAimLines();
}

void MonitorGraphicsView::slotTargetMoved(TargetItem *item)
{
    // Persist every selected target (group move), not just the dropped one.
    QList<TargetItem *> moved;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
        if (auto *tg = dynamic_cast<TargetItem *>(gi))
            moved.append(tg);
    if (item != nullptr && !moved.contains(item))
        moved.append(item);

    UndoEntry e; e.type = UndoEntry::TargetMove;
    foreach (TargetItem *ti, moved)
    {
        StageTarget *t = ti->target();
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
        if (m_pov == PovTop)
        {
            const float deck = floorHeightAt(float(newPos.x()), float(newPos.y()));
            const float oldDeck = floorHeightAt(float(oldPos.x()), float(oldPos.y()));
            // Preserve any height the operator set ABOVE the old surface.
            newZ = deck + qMax(0.0f, oldZ - oldDeck);
        }

        if (newPos != oldPos || !qFuzzyCompare(newZ, oldZ))
        {
            e.targetPositions.insert(t->id(), oldPos);
            t->setX(float(newPos.x()));
            t->setY(float(newPos.y()));
            t->setZ(newZ);
            emit targetMoved(ti->targetId(), QPointF(double(t->x()), double(t->y())));
        }
    }

    if (!e.targetPositions.isEmpty())
    {
        m_moveUndo.append(e);
        while (m_moveUndo.count() > 50)
            m_moveUndo.removeFirst();
        m_doc->setModified();
    }
    updateAimLines();

    // Snap running scenes whose palettes reference any moved target to the new
    // position immediately (override fade to 0 for one write cycle).
    foreach (TargetItem *ti, moved)
    {
        const quint32 tid = ti->targetId();
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
    QHash<quint32, QPair<int, int> > shortfall;        // target -> (short, aimed)
    QHash<quint32, QStringList> shortNames;
    foreach (const AimSolver::SceneAim &a, AimSolver::sceneAims(m_doc, activeScene))
    {
        reach.insert(qMakePair(a.fixtureId, a.targetId), a.solution);
        QPair<int, int> &sf = shortfall[a.targetId];
        sf.second += 1;
        if (a.solution.reachable() == false)
        {
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
        QPointF tgtPx = ti->pos();
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

            /* Out of reach: solid red, and it says by how much. */
            QPen pen = aimPen;
            const auto rit = reach.constFind(qMakePair(fid, tgt->id()));
            const bool cannot = (rit != reach.constEnd() && !rit.value().reachable());
            if (cannot)
            {
                pen = QPen(QColor(235, 70, 60), 2.0);
                pen.setStyle(Qt::SolidLine);
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

void MonitorGraphicsView::setActiveScene(quint32 sceneId)
{
    m_activeSceneId = sceneId;
    updateTargets();  // re-filter targets to those used by the new scene
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

void MonitorGraphicsView::highlightFixtures(const QList<quint32> &ids)
{
    const QSet<quint32> idSet(ids.begin(), ids.end());
    for (auto it = m_fixtures.begin(); it != m_fixtures.end(); ++it)
        it.value()->setHighlighted(idSet.contains(it.key()));
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
    return (m_unitValue > 500.0) ? QStringLiteral("m") : QStringLiteral("ft");
}

QString MonitorGraphicsView::axisName(bool horizontal) const
{
    if (horizontal)
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

QPointF MonitorGraphicsView::viewportToReadout(const QPoint &vp) const
{
    if (m_cellPixels <= 0)
        return QPointF(0, 0);
    const QPointF scene = mapToScene(vp);
    const qreal h = (scene.x() - m_xOffset) / m_cellPixels - originUnitsH();
    qreal v;
    if (isElevation())
        v = (floorPixelY() - scene.y()) / m_cellPixels;   // height off the floor
    else
        v = (scene.y() - m_yOffset) / m_cellPixels - originUnitsV();
    return QPointF(h, v);
}

QVector<MonitorGraphicsView::RulerTick> MonitorGraphicsView::rulerTicks(bool horizontal) const
{
    QVector<RulerTick> ticks;
    if (m_cellPixels <= 0)
        return ticks;

    const QRectF vis = mapToScene(viewport()->rect()).boundingRect();

    if (horizontal)
    {
        const qreal anchor = m_xOffset;
        const qreal originU = originUnitsH();
        int kMin = qFloor((vis.left()  - anchor) / m_cellPixels) - 1;
        int kMax = qCeil ((vis.right() - anchor) / m_cellPixels) + 1;
        for (int k = kMin; k <= kMax; k++)
        {
            const qreal sceneX = anchor + qreal(k) * m_cellPixels;
            const qreal px = mapFromScene(QPointF(sceneX, vis.top())).x();
            ticks.append({ px, formatRulerLabel(qreal(k) - originU), true });
        }
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
            const qreal sceneY = anchor - qreal(k) * m_cellPixels;
            const qreal px = mapFromScene(QPointF(vis.left(), sceneY)).y();
            ticks.append({ px, formatRulerLabel(qreal(k)), true });
        }
    }
    else
    {
        const qreal anchor = m_yOffset;
        const qreal originU = originUnitsV();
        int kMin = qFloor((vis.top()    - anchor) / m_cellPixels) - 1;
        int kMax = qCeil ((vis.bottom() - anchor) / m_cellPixels) + 1;
        for (int k = kMin; k <= kMax; k++)
        {
            const qreal sceneY = anchor + qreal(k) * m_cellPixels;
            const qreal px = mapFromScene(QPointF(vis.left(), sceneY)).y();
            ticks.append({ px, formatRulerLabel(qreal(k) - originU), true });
        }
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
    if (m_centerLineV != nullptr) m_centerLineV->setVisible(on);
    if (m_centerLineH != nullptr) m_centerLineH->setVisible(on);
}

void MonitorGraphicsView::updateGrid()
{
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
        const int xInc = fitW / m_gridSize.width();
        const int yInc = fitH / m_gridSize.height();

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
            m_xOffset = (fitW - (m_cellPixels * m_gridSize.width())) / 2;
        }
        else
        {
            m_cellPixels = xInc;
            m_yOffset = (fitH - (m_cellPixels * m_gridSize.height())) / 2;
        }
        int xPos = m_xOffset;
        int yPos = m_yOffset;

        QPen mainPen(QColor(40, 40, 40, 255));
        QPen subPen(QColor(30, 30, 30, 255));
        subPen.setStyle(Qt::DotLine);

        // draw sub-division lines first (so the main lines sit on top)
        if (m_gridSubdivisions > 1)
        {
            qreal subInc = (qreal)m_cellPixels / m_gridSubdivisions;
            int bottom = this->height() - m_yOffset;
            int right = this->width() - m_xOffset;

            for (int i = 0; i < m_gridSize.width(); i++)
            {
                for (int s = 1; s < m_gridSubdivisions; s++)
                {
                    qreal sx = m_xOffset + i * m_cellPixels + s * subInc;
                    QGraphicsLineItem *item = m_scene->addLine(sx, m_yOffset, sx, bottom, subPen);
                    item->setZValue(1);
                    m_gridItems.append(item);
                }
            }

            for (int i = 0; i < m_gridSize.height(); i++)
            {
                for (int s = 1; s < m_gridSubdivisions; s++)
                {
                    qreal sy = m_yOffset + i * m_cellPixels + s * subInc;
                    QGraphicsLineItem *item = m_scene->addLine(m_xOffset, sy, right, sy, subPen);
                    item->setZValue(1);
                    m_gridItems.append(item);
                }
            }
        }

        for (int i = 0; i < m_gridSize.width() + 1; i++)
        {
            QGraphicsLineItem *item = m_scene->addLine(xPos, m_yOffset, xPos, this->height() - m_yOffset,
                                                       mainPen);
            item->setZValue(1);
            xPos += m_cellPixels;
            m_gridItems.append(item);
        }

        for (int i = 0; i < m_gridSize.height() + 1; i++)
        {
            QGraphicsLineItem *item = m_scene->addLine(m_xOffset, yPos, this->width() - m_xOffset, yPos,
                                                       mainPen);
            item->setZValue(1);
            yPos += m_cellPixels;
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
        const qreal gridW = m_gridSize.width()  * m_cellPixels;
        const qreal gridH = m_gridSize.height() * m_cellPixels;
        const qreal cx = m_xOffset + gridW / 2.0;
        const qreal cy = m_yOffset + gridH / 2.0;
        m_centerLineV = m_scene->addLine(cx, m_yOffset, cx, m_yOffset + gridH, centerPen);
        m_centerLineV->setZValue(2);
        m_centerLineV->setAcceptedMouseButtons(Qt::NoButton);   // above trusses, but not in their way
        m_centerLineV->setVisible(m_centerLinesVisible);
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
    m_fittedGrid = m_gridSize;
    m_fittedUnit = m_unitValue;
    updateGrid();
    refreshAllItems();
    emit rulersChanged();
}

void MonitorGraphicsView::mouseReleaseEvent(QMouseEvent *e)
{
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

    // The release that follows a mouseDoubleClickEvent must not emit viewClicked
    // (that would immediately close the editor the double-click just opened).
    if (m_suppressNextViewClick)
    {
        m_suppressNextViewClick = false;
        QGraphicsView::mouseReleaseEvent(e);
        if (dragMode() == QGraphicsView::ScrollHandDrag)
            setDragMode(QGraphicsView::RubberBandDrag);
        return;
    }

    emit viewClicked(e);

    QGraphicsView::mouseReleaseEvent(e);

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
    if (wasDrag && m_scene != nullptr)
    {
        MonitorFixtureItem *repFixture = nullptr;
        TrussItem          *repTruss   = nullptr;
        PlatformItem       *repPlatform = nullptr;
        PipeItem           *repPipe    = nullptr;
        StandItem          *repStand   = nullptr;
        TowerItem          *repTower   = nullptr;
        PowerSourceItem    *repPower   = nullptr;
        NetworkNodeItem    *repNetwork = nullptr;
        MonitorImageItem   *repImage   = nullptr;
        TargetItem         *repTarget  = nullptr;

        foreach (QGraphicsItem *gi, m_scene->selectedItems())
        {
            if (repFixture == nullptr)  repFixture  = dynamic_cast<MonitorFixtureItem *>(gi);
            if (repTruss == nullptr)    repTruss    = dynamic_cast<TrussItem *>(gi);
            if (repPlatform == nullptr) repPlatform = dynamic_cast<PlatformItem *>(gi);
            if (repPipe == nullptr)     repPipe     = dynamic_cast<PipeItem *>(gi);
            if (repStand == nullptr)    repStand    = dynamic_cast<StandItem *>(gi);
            if (repTower == nullptr)    repTower    = dynamic_cast<TowerItem *>(gi);
            if (repPower == nullptr)    repPower    = dynamic_cast<PowerSourceItem *>(gi);
            if (repNetwork == nullptr)  repNetwork  = dynamic_cast<NetworkNodeItem *>(gi);
            if (repImage == nullptr)    repImage    = dynamic_cast<MonitorImageItem *>(gi);
            if (repTarget == nullptr)   repTarget   = dynamic_cast<TargetItem *>(gi);
        }

        if (repFixture != nullptr)  slotFixtureMoved(repFixture);
        if (repTruss != nullptr)    slotTrussMoved(repTruss);
        if (repPlatform != nullptr) slotPlatformMoved(repPlatform);
        if (repPipe != nullptr)     slotPipeMoved(repPipe);
        if (repStand != nullptr)    slotStandMoved(repStand);
        if (repTower != nullptr)    slotTowerMoved(repTower);
        if (repPower != nullptr)    slotPowerSourceMoved(repPower);
        if (repNetwork != nullptr)  slotNetworkNodeMoved(repNetwork);
        if (repImage != nullptr)    slotImageMoved(repImage);
        if (repTarget != nullptr)   slotTargetMoved(repTarget);
    }

    // Restore rubber-band selection after a shift-drag pan.
    if (dragMode() == QGraphicsView::ScrollHandDrag)
        setDragMode(QGraphicsView::RubberBandDrag);

    // Selection may have changed — rebuild aim lines for selected targets.
    updateAimLines();
}

void MonitorGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
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
            MonitorProperties *props = m_doc->monitorProperties();

            // NOTE: trusses are no longer highlighted as drop targets while
            // dragging — dropping a fixture on a truss no longer binds it
            // (assignment is explicit now). The red escape border below still
            // shows when an ALREADY-bound fixture is being pulled off its truss.

            // Show red escape-from-truss border when pulling a bound fixture far off
            foreach (QGraphicsItem *gi, m_scene->selectedItems())
            {
                MonitorFixtureItem *mfi = dynamic_cast<MonitorFixtureItem *>(gi);
                if (!mfi || !mfi->isBoundToTruss()) continue;
                quint32 fid = m_fixtures.key(mfi, Fixture::invalidId());
                if (fid == Fixture::invalidId()) continue;
                FixtureRigProps rp = props->fixtureRigProps(fid);
                if (rp.trussId == Truss::invalidId())
                {
                    /* Unbound: green while it is over a truss, so releasing
                       there visibly means "attach". The mirror of the red
                       escape border below. */
                    mfi->setAttachMode(trussUnderFixture(mfi, 1.0f, true) != nullptr);
                    continue;
                }
                mfi->setAttachMode(false);
                TrussItem *ti = m_trussItems.value(rp.trussId, nullptr);
                if (!ti) continue;

                // Same yardstick both ways: trussUnderFixture() applies the
                // two-widths rule, so "still on it" and "far enough to detach"
                // cannot disagree.
                mfi->setEscapeMode(trussUnderFixture(mfi) != ti);
            }
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
}

void MonitorGraphicsView::mouseDoubleClickEvent(QMouseEvent *event)
{
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
        /* childrenBoundingRect(), not sceneBoundingRect(): a group caches its
           bounds when items are added, and the disc is sized afterwards, so
           the cached rect is empty. */
        if (badge == nullptr || !badge->isVisible()
            || !badge->mapRectToScene(badge->childrenBoundingRect()).contains(sp))
            continue;
        const QStringList key = bit.key().split(QLatin1Char(':'));
        if (key.size() == 2)
        {
            revealBuriedFixtures(key.at(0), key.at(1).toUInt());
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
    auto drilledIntoGroup = [&](QGraphicsItem *item) -> bool {
        if (m_layoutLocked || item == nullptr)         return false;
        if (itemGroupId(item) == 0)                    return false;   // not grouped
        if (m_scene->selectedItems().size() <= 1)      return false;   // already isolated
        m_extendingSelection = true;                   // suppress select-together
        m_scene->clearSelection();
        item->setSelected(true);
        m_extendingSelection = false;
        emit mapSelectionChanged();
        return true;
    };

    if (auto *fi = dynamic_cast<MonitorFixtureItem *>(it))
    {
        // A fixture mounted ON a feature → open that FEATURE's editor (the fixture
        // sits in its tree/inspector). This is the common "double-click the riser"
        // case, where the feature is covered by its own fixtures. Truss binding is
        // deliberately NOT included here -- a truss-bound fixture instead follows
        // the drill-in-then-edit path below (first double-click isolates the
        // fixture, second opens ITS OWN editor); a truss is a long, mostly-
        // uncovered feature, so its own editor is still one double-click away by
        // clicking the truss itself elsewhere.
        MonitorProperties *mp = m_doc->monitorProperties();
        const FixtureRigProps rp = mp->fixtureRigProps(fi->fixtureID());
        /* Ask WHICH structure through the one shared accessor, so this can
           never disagree with the position resolver again. It used to test the
           ids in its own order -- deck before truss -- so a fixture carrying a
           stale second mount was DRAWN on its truss but opened the editor for
           an unrelated platform. (A truss deliberately falls through to the
           drill-in path below; it is long and mostly uncovered, so its own
           editor is a double-click away on the truss itself.) */
        switch (rp.primaryMount())
        {
        case FixtureRigProps::PipeMount:  emit pipeDoubleClicked(rp.pipeId);   return;
        case FixtureRigProps::TowerMount: emit towerDoubleClicked(rp.towerId); return;
        case FixtureRigProps::RiserMount: emit platformDoubleClicked(rp.riserPlatformId); return;
        case FixtureRigProps::DeckMount:  emit platformDoubleClicked(rp.deckPlatformId);  return;
        case FixtureRigProps::TrussMount:                 // drill-in path below
        case FixtureRigProps::NoMount:
        default: break;
        }
        if (const quint32 fg = mp->fixtureFrameGroup(fi->fixtureID()))
        {
            const MonitorProperties::MonitorGroup g = mp->group(fg);
            if (g.anchorKind == QStringLiteral("platform")) { emit platformDoubleClicked(g.anchorId); return; }
        }

        // Grouped OR truss-bound fixture: the FIRST double-click DRILLS IN —
        // selects just this fixture (distinct cyan-dashed highlight) so it can
        // be slid along its truss or dragged clear to detach. Single-click still
        // selects the whole group / truss.
        //
        // A SECOND double-click on the already-drilled-in fixture opens its
        // editor (drill-in then act — no need to reach for the right-click menu).
        // When the plot is LOCKED, drilling-in (to slide/detach) is meaningless,
        // so a double-click always opens the editor straight away.
        if (!m_layoutLocked && (itemGroupId(fi) != 0 || fi->isBoundToTruss()) && !fi->isIsolated())
        {
            m_extendingSelection = true;   // suppress select-together
            m_scene->clearSelection();
            fi->setSelected(true);
            fi->setIsolated(true);
            m_extendingSelection = false;
            emit mapSelectionChanged();
            // Arm the triple-click shortcut (see mousePressEvent): a further
            // rapid click on this same fixture opens its editor right away,
            // rather than requiring a whole separate second double-click.
            m_lastFixtureDoubleClickItem = fi;
            m_lastFixtureDoubleClickTimer.restart();
            return;
        }

        // If the double-clicked fixture was part of a multi-selection, restore
        // that selection so the editor opens for all of them.
        if (m_savedSelection.contains(fi) && m_savedSelection.size() > 1)
        {
            m_scene->clearSelection();
            foreach (QGraphicsItem *gi, m_savedSelection)
                gi->setSelected(true);
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
    if (auto *pi = dynamic_cast<PlatformItem *>(it))
    {
        if (drilledIntoGroup(pi)) return;
        emit platformDoubleClicked(pi->platformId());
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
    if (auto *twi = dynamic_cast<TowerItem *>(it))
    {
        if (drilledIntoGroup(twi)) return;
        emit towerDoubleClicked(twi->towerId());
        return;
    }
    if (auto *tgi = dynamic_cast<TargetItem *>(it))
    {
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
    if (MonitorFixtureItem *item = m_fixtures.value(fid, nullptr))
        slotFixtureMoved(item);
}

void MonitorGraphicsView::contextMenuEvent(QContextMenuEvent *event)
{
    QPointF sp = mapToScene(event->pos());
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
    else if (auto *twi = dynamic_cast<TowerItem *>(it))       { kind = QStringLiteral("tower");    id = twi->towerId(); }
    else if (auto *tgi = dynamic_cast<TargetItem *>(it))      { kind = QStringLiteral("target");   id = tgi->targetId(); }
    else if (auto *psi = dynamic_cast<PowerSourceItem *>(it)) { kind = QStringLiteral("power");    id = quint32(psi->sourceIndex()); }
    else if (auto *nni = dynamic_cast<NetworkNodeItem *>(it)) { kind = QStringLiteral("network");  id = quint32(nni->nodeIndex()); }

    if (kind.isEmpty())
    {
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

/* The graphics item standing for kind/id, if the plot has one. */
static QGraphicsItem *mapItemFor(const QString &kind, quint32 id,
                                 const QHash<quint32, MonitorFixtureItem *> &fixtures,
                                 const QHash<quint32, TrussItem *> &trusses,
                                 const QHash<quint32, PlatformItem *> &platforms,
                                 const QHash<quint32, PipeItem *> &pipes,
                                 const QHash<quint32, StandItem *> &stands,
                                 const QHash<quint32, TowerItem *> &towers,
                                 const QHash<quint32, TargetItem *> &targets)
{
    if (kind == QLatin1String("fixture"))  return fixtures.value(id, nullptr);
    if (kind == QLatin1String("truss"))    return trusses.value(id, nullptr);
    if (kind == QLatin1String("platform")) return platforms.value(id, nullptr);
    if (kind == QLatin1String("pipe"))     return pipes.value(id, nullptr);
    if (kind == QLatin1String("stand"))    return stands.value(id, nullptr);
    if (kind == QLatin1String("tower"))    return towers.value(id, nullptr);
    if (kind == QLatin1String("target"))   return targets.value(id, nullptr);
    return nullptr;
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
        else if (auto *twi = dynamic_cast<TowerItem *>(gi))        out << qMakePair(QStringLiteral("tower"), twi->towerId());
        else if (auto *tgi = dynamic_cast<TargetItem *>(gi))       out << qMakePair(QStringLiteral("target"), tgi->targetId());
        else if (auto *psi = dynamic_cast<PowerSourceItem *>(gi))  out << qMakePair(QStringLiteral("power"), quint32(psi->sourceIndex()));
        else if (auto *nni = dynamic_cast<NetworkNodeItem *>(gi))  out << qMakePair(QStringLiteral("network"), quint32(nni->nodeIndex()));
    }
    return out;
}

void MonitorGraphicsView::buildItemMenu(QMenu *menu, const QString &kind, quint32 id,
                                        const MenuPlace &place, const QPointF &scenePos)
{
    if (menu == nullptr)
        return;
    MonitorProperties *props = m_doc->monitorProperties();
    const bool design = (m_doc->mode() == Doc::Design);

    // 1. The thing.
    populateItemMenu(menu, kind, id, scenePos, false);

    // 2. Where it is, and its name.
    if (!menu->isEmpty())
        menu->addSeparator();
    if (place.inList)
        menu->addAction(tr("Show on Map"), this, [this, kind, id]() {
            selectMapItem(kind, id);
        });
    else
        menu->addAction(tr("Show in Layers"), this, [this, kind, id]() {
            emit revealInLayersRequested(kind, id);
        });
    if (design)
    {
        const std::function<void()> inPlace = place.renameInPlace;
        menu->addAction(tr("Rename…"), this, [this, kind, id, inPlace]() {
            if (inPlace)
            {
                inPlace();
                return;
            }
            bool ok = false;
            const QString name = QInputDialog::getText(this, tr("Rename"), tr("Name:"),
                                                       QLineEdit::Normal, itemNameOf(m_doc, kind, id), &ok).trimmed();
            if (ok && !name.isEmpty())
            {
                renameItem(kind, id, name);
                m_doc->setModified();
                emit mapStructureChanged();
            }
        });

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
            menu->addAction(framed ? tr("Fixture Layout…") : tr("Create Fixture Layout…"),
                            this, [this, anchorGroup]() {
                openStudioGroupForGroup(anchorGroup);
                emit mapStructureChanged();
            });
            menu->addAction(tr("Ungroup"), this, [this, anchorGroup]() {
                ungroupById(anchorGroup);
            });
        }
    }

    // 3. Organising: the selection where the menu was opened, and this item.
    QList<QPair<QString, quint32> > objects = place.selection;
    if (!objects.contains(qMakePair(kind, id)))
        objects.prepend(qMakePair(kind, id));
    populateOrganiseMenu(menu, objects);

    // 4. Removing, last.
    addItemRemoval(menu, kind, id);
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

void MonitorGraphicsView::renameItem(const QString &kind, quint32 id, const QString &name)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (kind == QLatin1String("fixture"))
    {
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
                                               const QList<QPair<QString, quint32> > &objects)
{
    if (menu == nullptr || objects.isEmpty() || m_doc->mode() != Doc::Design)
        return;
    MonitorProperties *props = m_doc->monitorProperties();
    if (!menu->isEmpty())
        menu->addSeparator();

    auto groupOf = [this](const QString &kind, quint32 id) -> quint32 {
        QGraphicsItem *gi = mapItemFor(kind, id, m_fixtures, m_trussItems, m_platformItems,
                                       m_pipeItems, m_standItems, m_towerItems, m_targetItems);
        if (gi != nullptr)
            return itemGroupId(gi);
        return (kind == QLatin1String("fixture")) ? m_doc->monitorProperties()->fixtureGroup(id) : 0;
    };

    // Folders.
    QList<QPair<QString, quint32> > inFolder;
    for (const auto &o : objects)
        if (groupOf(o.first, o.second) != 0)
            inFolder << o;
    if (!inFolder.isEmpty())
        menu->addAction(inFolder.size() > 1 ? tr("Remove %1 Items from Folder").arg(inFolder.size())
                                            : tr("Remove from Folder"),
                        this, [this, inFolder]() {
            removeFromFolder(inFolder);
        });
    if (objects.size() >= 2)
        menu->addAction(tr("New Folder from Selection…"), this, [this, objects, groupOf]() {
            /* Nested inside the folder they already share, if they share one;
               otherwise a folder of its own on the first object's layer. */
            quint32 parent = groupOf(objects.first().first, objects.first().second);
            for (const auto &o : objects)
                if (groupOf(o.first, o.second) != parent)
                {
                    parent = 0;
                    break;
                }
            MonitorProperties *p = m_doc->monitorProperties();
            quint32 layer = p->activeLayerId();
            if (parent != 0)
                layer = p->group(parent).layerId;
            else if (QGraphicsItem *gi = mapItemFor(objects.first().first, objects.first().second,
                                                    m_fixtures, m_trussItems, m_platformItems,
                                                    m_pipeItems, m_standItems, m_towerItems,
                                                    m_targetItems))
                layer = itemLayerId(gi);
            const quint32 gid = p->nextGroupId();
            bool ok = false;
            const QString name = QInputDialog::getText(this, tr("New Folder"), tr("Folder name:"),
                                                       QLineEdit::Normal, tr("Folder %1").arg(gid),
                                                       &ok).trimmed();
            if (!ok || name.isEmpty())
                return;
            p->createGroup(gid, name, layer, parent);
            reparentToGroup(objects, gid);
            m_doc->setModified();
            emit mapStructureChanged();
        });

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
            menu->addAction(tr("Create Fixture Group from Selection…"), this, [this, fixtures]() {
                if (CreateFixtureGroup::createFromFixtures(m_doc, fixtures, this) != nullptr)
                    emit mapStructureChanged();
            });
    }

    // Layers.
    QMenu *moveMenu = menu->addMenu(tr("Move to Layer"));
    foreach (const MonitorProperties::MonitorLayer &lyr, props->layers())
    {
        const quint32 lid = lyr.id;
        moveMenu->addAction(lyr.name, this, [this, objects, lid]() {
            reparentToLayer(objects, lid);
            emit mapStructureChanged();
        });
    }
    menu->addAction(tr("New Layer from Selection…"), this, [this, objects]() {
        MonitorProperties *p = m_doc->monitorProperties();
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("New Layer"), tr("Layer name:"),
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

    // Several at once: one object's lock is in its own section.
    QList<QPair<QString, quint32> > lockable;
    for (const auto &o : objects)
        if (o.first != QLatin1String("fixture"))
            lockable << o;
    if (lockable.size() >= 2)
    {
        menu->addAction(tr("Lock Selected"), this, [this, lockable]() {
            for (const auto &o : lockable)
                setItemLocked(o.first, o.second, true);
            emit mapStructureChanged();
        });
        menu->addAction(tr("Unlock Selected"), this, [this, lockable]() {
            for (const auto &o : lockable)
                setItemLocked(o.first, o.second, false);
            emit mapStructureChanged();
        });
    }
}

static QString itemNoun(const QString &kind)
{
    if (kind == QLatin1String("truss"))    return MonitorGraphicsView::tr("Truss");
    if (kind == QLatin1String("platform")) return MonitorGraphicsView::tr("Platform");
    if (kind == QLatin1String("image"))    return MonitorGraphicsView::tr("Image");
    if (kind == QLatin1String("pipe"))     return MonitorGraphicsView::tr("Boom");
    if (kind == QLatin1String("stand"))    return MonitorGraphicsView::tr("Stand");
    if (kind == QLatin1String("tower"))    return MonitorGraphicsView::tr("Tower");
    if (kind == QLatin1String("target"))   return MonitorGraphicsView::tr("Target");
    if (kind == QLatin1String("power"))    return MonitorGraphicsView::tr("Power Source");
    if (kind == QLatin1String("network"))  return MonitorGraphicsView::tr("Network Node");
    return MonitorGraphicsView::tr("Fixture");
}

void MonitorGraphicsView::addItemRemoval(QMenu *menu, const QString &kind, quint32 id)
{
    if (menu == nullptr || kind == QLatin1String("power"))
        return;
    if (!menu->isEmpty())
        menu->addSeparator();
    if (kind == QLatin1String("fixture"))
    {
        menu->addAction(tr("Remove from View"), this, [this, id]() {
            requestDeleteItem(QStringLiteral("fixture"), id);
        });
        return;
    }
    const QString noun = itemNoun(kind);
    // A target asks first: aim palettes and scenes may depend on it.
    menu->addAction(kind == QLatin1String("target")
                        ? tr("Delete %1…").arg(noun) : tr("Delete %1").arg(noun),
                    this, [this, kind, id]() {
        requestDeleteItem(kind, id);
    });
}

void MonitorGraphicsView::populateItemMenu(QMenu *menu, const QString &kind, quint32 id,
                                           const QPointF &scenePos, bool withRemoval)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (menu == nullptr || props == nullptr)
        return;

    if (kind != QLatin1String("fixture"))
    {
        /* A structure or a mark: edit it, build on it, hold it in place,
           delete it. */
        const QString noun = itemNoun(kind);
        menu->addAction(tr("Edit %1…").arg(noun), this, [this, kind, id]() {
            requestEditItem(kind, id);
        });

        /* One "Add" submenu per structure, rather than a handful of loose
           "Add X" entries at the top level: "the add section should probably
           have a sub menu of what can be added to keep things clean". What
           goes in it is whatever this kind of thing can actually carry. */
        QMenu *addMenu = nullptr;
        auto adds = [&]() -> QMenu * {
            if (addMenu == nullptr)
            {
                menu->addSeparator();
                addMenu = menu->addMenu(tr("Add"));
            }
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
            adds()->addAction(tr("Person Here"), this, [this, at]() {
                emit addItemRequested(QStringLiteral("person"), at);
            });
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
        const bool locked = itemLocked(kind, id);
        menu->addAction(locked ? tr("Unlock Position") : tr("Lock Position"),
                        this, [this, kind, id, locked]() {
            setItemLocked(kind, id, !locked);
        });

        if (withRemoval)
            addItemRemoval(menu, kind, id);
        return;
    }

    /* ---- A fixture: edit and find it; mount it; point and place it; group
       it; take it off the plot. ---- */
    const quint32 fid = id;
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    Fixture *fxi = m_doc->fixture(fid);

    menu->addAction(tr("Edit Fixture…"), this, [this, fid]() {
        emit fixtureDoubleClicked(fid);
    });
    menu->addAction(tr("Locate"), this, [this, fid]() {
        /* The selected fixtures on the plot and this one: selecting a truss's
           worth of heads and asking where they are lights all of them. */
        QList<quint32> ids;
        foreach (QGraphicsItem *gi, m_scene->selectedItems())
            if (MonitorFixtureItem *mfi = dynamic_cast<MonitorFixtureItem *>(gi))
                if (!ids.contains(mfi->fixtureID()))
                    ids << mfi->fixtureID();
        if (!ids.contains(fid))
            ids << fid;
        if (Monitor::instance() != nullptr)
            Monitor::instance()->locateFixtures(ids);
    });
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
    }

    // Mounting.
    menu->addSeparator();
    QMenu *mountMenu = menu->addMenu(tr("Mount on"));
    {
        QMenu *trussMenu = mountMenu->addMenu(tr("Truss"));
        const QList<Truss *> trusses = props->trusses();
        trussMenu->setEnabled(!trusses.isEmpty());
        for (Truss *t : trusses)
        {
            if (t == nullptr)
                continue;
            const quint32 tid = t->id();
            QAction *a = trussMenu->addAction(t->name().isEmpty() ? tr("Truss %1").arg(tid) : t->name(),
                                              this, [this, fid, tid]() {
                attachFixtureToTruss(fid, tid);
            });
            a->setCheckable(true);
            a->setChecked(rp.trussId == tid);
        }

        QMenu *boomMenu = mountMenu->addMenu(tr("Boom"));
        const QList<Pipe *> pipes = props->pipes();
        boomMenu->setEnabled(!pipes.isEmpty());
        for (Pipe *b : pipes)
        {
            if (b == nullptr)
                continue;
            const quint32 bid = b->id();
            QAction *a = boomMenu->addAction(b->name().isEmpty() ? tr("Boom %1").arg(bid) : b->name(),
                                             this, [this, fid, bid]() {
                attachFixtureToPipe(fid, bid);
            });
            a->setCheckable(true);
            a->setChecked(rp.pipeId == bid);
        }

        QMenu *towerMenu = mountMenu->addMenu(tr("Tower"));
        const QList<Tower *> towers = props->towers();
        towerMenu->setEnabled(!towers.isEmpty());
        for (Tower *tw : towers)
        {
            if (tw == nullptr)
                continue;
            const quint32 twid = tw->id();
            QMenu *spots = towerMenu->addMenu(tw->name().isEmpty() ? tr("Tower %1").arg(twid) : tw->name());
            QAction *top = spots->addAction(tr("Top of tower"), this, [this, fid, twid]() {
                mountFixtureOnTower(fid, twid, FixtureRigProps::TowerTop);
            });
            top->setCheckable(true);
            top->setChecked(rp.towerId == twid && rp.towerMountSide == FixtureRigProps::TowerTop);
            QAction *bottom = spots->addAction(tr("Bottom of tower"), this, [this, fid, twid]() {
                mountFixtureOnTower(fid, twid, FixtureRigProps::TowerBottom);
            });
            bottom->setCheckable(true);
            bottom->setChecked(rp.towerId == twid && rp.towerMountSide == FixtureRigProps::TowerBottom);
            spots->addSeparator();
            if (tw->shelfCount() == 0)
                spots->addAction(tr("(no shelves — add in the tower editor)"))->setEnabled(false);
            for (int si = 0; si < tw->shelfCount(); ++si)
            {
                QAction *a = spots->addAction(tr("Shelf %1").arg(si + 1), this, [this, fid, twid, si]() {
                    mountFixtureOnTower(fid, twid, FixtureRigProps::TowerShelf, si);
                });
                a->setCheckable(true);
                a->setChecked(rp.towerId == twid && rp.towerMountSide == FixtureRigProps::TowerShelf
                              && rp.towerShelf == si);
            }
        }
    }

    // What it is mounted on: reach that, or come off it.
    QString mountKind, mountName;
    quint32 mountId = 0;
    if (Truss *t = (rp.trussId != Truss::invalidId()) ? props->truss(rp.trussId) : nullptr)
    { mountKind = QStringLiteral("truss"); mountId = t->id(); mountName = t->name(); }
    else if (Pipe *b = (rp.pipeId != Pipe::invalidId()) ? props->pipe(rp.pipeId) : nullptr)
    { mountKind = QStringLiteral("pipe"); mountId = b->id(); mountName = b->name(); }
    else if (Tower *tw = (rp.towerId != Tower::invalidId()) ? props->tower(rp.towerId) : nullptr)
    { mountKind = QStringLiteral("tower"); mountId = tw->id(); mountName = tw->name(); }
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
    const QVector3D fp = props->fixturePosition(fid, 0, 0);
    menu->addAction(tr("Set Mounting Height… (%1 m)").arg(double(fp.z()) / 1000.0, 0, 'f', 2),
                    this, [this, fid]() {
        MonitorProperties *p = m_doc->monitorProperties();
        const QVector3D cur = p->fixturePosition(fid, 0, 0);
        bool ok = false;
        const double h = QInputDialog::getDouble(this, tr("Mounting Height"),
                             tr("Height above the floor (metres):"),
                             double(cur.z()) / 1000.0, 0.0, 100.0, 2, &ok);
        if (!ok)
            return;
        QVector3D np = cur;
        np.setZ(float(h * 1000.0));
        p->setFixturePosition(fid, 0, 0, np);
        m_doc->setModified();
        updateFixture(fid);
    });

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

    // Removing, last.
    if (withRemoval)
        addItemRemoval(menu, kind, fid);
}

bool MonitorGraphicsView::itemLocked(const QString &kind, quint32 id) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (kind == QLatin1String("truss"))    { Truss *t = props->truss(id);        return t && t->locked(); }
    if (kind == QLatin1String("platform")) { StagePlatform *p = props->platform(id); return p && p->locked(); }
    if (kind == QLatin1String("pipe"))     { Pipe *b = props->pipe(id);          return b && b->locked(); }
    if (kind == QLatin1String("stand"))    { Stand *s = props->stand(id);        return s && s->locked(); }
    if (kind == QLatin1String("tower"))    { Tower *t = props->tower(id);        return t && t->locked(); }
    if (kind == QLatin1String("target"))   { StageTarget *t = props->stageTarget(id); return t && t->locked(); }
    if (kind == QLatin1String("image"))    return props->image(id).locked;
    if (kind == QLatin1String("power"))
    {
        PowerDistribution *pd = m_doc->powerDistribution();
        return pd != nullptr && int(id) < pd->sources().size() && pd->sources().at(int(id)).locked;
    }
    if (kind == QLatin1String("network"))
    {
        NetworkTopology *nt = m_doc->networkTopology();
        return nt != nullptr && int(id) < nt->nodes().size() && nt->nodes().at(int(id)).locked;
    }
    return false;
}

void MonitorGraphicsView::setItemLocked(const QString &kind, quint32 id, bool locked)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (kind == QLatin1String("truss"))         { if (Truss *t = props->truss(id)) t->setLocked(locked); }
    else if (kind == QLatin1String("platform")) { if (StagePlatform *p = props->platform(id)) p->setLocked(locked); }
    else if (kind == QLatin1String("pipe"))     { if (Pipe *b = props->pipe(id)) b->setLocked(locked); }
    else if (kind == QLatin1String("stand"))    { if (Stand *s = props->stand(id)) s->setLocked(locked); }
    else if (kind == QLatin1String("tower"))    { if (Tower *t = props->tower(id)) t->setLocked(locked); }
    else if (kind == QLatin1String("target"))   { if (StageTarget *t = props->stageTarget(id)) t->setLocked(locked); }
    else if (kind == QLatin1String("image"))
    {
        MonitorProperties::MonitorImage img = props->image(id);
        img.locked = locked;
        props->setImage(img);
    }
    else if (kind == QLatin1String("power"))
    {
        PowerDistribution *pd = m_doc->powerDistribution();
        if (pd == nullptr || int(id) >= pd->sources().size())
            return;
        pd->sources()[int(id)].locked = locked;
    }
    else if (kind == QLatin1String("network"))
    {
        NetworkTopology *nt = m_doc->networkTopology();
        if (nt == nullptr || int(id) >= nt->nodes().size())
            return;
        nt->nodes()[int(id)].locked = locked;
    }
    else
        return;
    m_doc->setModified();
    refreshItemLayerState();
    if (kind == QLatin1String("target"))
        updateTargets();
    else if (kind == QLatin1String("power"))
        updatePowerSources();
    else if (kind == QLatin1String("network"))
        updateNetworkNodes();
    m_scene->update();
}

void MonitorGraphicsView::requestDeleteItem(const QString &kind, quint32 id)
{
    if (kind == QLatin1String("truss"))         emit trussRemoveRequested(id);
    else if (kind == QLatin1String("platform")) emit platformRemoveRequested(id);
    else if (kind == QLatin1String("image"))    emit imageRemoveRequested(id);
    else if (kind == QLatin1String("pipe"))     emit pipeRemoveRequested(id);
    else if (kind == QLatin1String("stand"))    emit standRemoveRequested(id);
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
    /* The Layers list nests a fixture under a structure by GROUP, not by
       mount: mounted but ungrouped, it is drawn on the tower and listed
       loose. Same as attachFixtureToTruss(): an explicit mount joins the
       structure's group, whatever group the fixture was in. */
    ensureTowerGroup(towerId);
    if (Tower *t = m_doc->monitorProperties()->tower(towerId))
        if (t->groupId() != 0)
            m_doc->monitorProperties()->setFixtureGroup(fid, t->groupId());
}

void MonitorGraphicsView::mountFixtureOnTower(quint32 fid, quint32 towerId, int side, int shelf)
{
    ensureFixtureOnPlot(fid);
    MonitorProperties *p = m_doc->monitorProperties();
    Tower *t = p->tower(towerId);
    if (t == nullptr)
        return;
    FixtureRigProps r = p->fixtureRigProps(fid);
    if (r.onTower() && r.towerId != towerId)
        leaveDedicatedTowerGroup(fid, r.towerId);
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
    props->setFixtureLayer(fid, props->activeLayerId());
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
    if (rp.onTower())
        leaveDedicatedTowerGroup(fid, rp.towerId);
    FixtureRigProps r = rp;
    r.pipeId = Pipe::invalidId();
    r.towerId = Tower::invalidId();
    p->setFixtureRigProps(fid, r);
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
    if (rp.onTower())
        leaveDedicatedTowerGroup(fid, rp.towerId);
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
    m_doc->setModified();
    emit mapStructureChanged();
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
    if (rp.onTower() && rp.towerId != towerId)
        leaveDedicatedTowerGroup(fid, rp.towerId);
    const bool alreadyOnAShelf = (rp.towerId == towerId
                                   && rp.towerMountSide == FixtureRigProps::TowerShelf
                                   && rp.towerShelf < quint32(t->shelfCount()));
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

    const double rowH = fm.height() + 6.0;
    const double gap = 3.0;
    const double totalH = m_revealOrder.count() * (rowH + gap) - gap;
    double y = anchor.y() - totalH * 0.5;
    const double x = anchor.x() + 18.0;

    MonitorProperties *props = m_doc ? m_doc->monitorProperties() : nullptr;

    foreach (quint32 fid, m_revealOrder)
    {
        Fixture *fx = m_doc->fixture(fid);
        const QString name = fx ? fx->name() : tr("Fixture %1").arg(fid);
        const bool picked = (m_revealFocus == qint64(fid));

        const double swatch = 8.0;
        const double w = fm.horizontalAdvance(name) + swatch + 16.0;
        QPainterPath p;
        p.addRoundedRect(QRectF(0, 0, w, rowH), 3.0, 3.0);

        RevealTagItem *tag = new RevealTagItem(fid, this);
        tag->setPath(p);
        tag->setPos(x, y);
        tag->setZValue(3.1);                  // above the fixtures it points at
        tag->setBrush(QBrush(picked ? QColor(40, 110, 220) : QColor(24, 32, 44, 235)));
        tag->setPen(QPen(picked ? QColor(235, 244, 255) : QColor(120, 160, 215), 1.0));
        tag->setToolTip(tr("Show where %1 lands").arg(name));

        QColor gel = props ? props->fixtureGelColor(fid, 0, 0) : QColor();
        if (!gel.isValid() || gel == QColor(Qt::black))
            gel = QColor(200, 210, 225);
        QGraphicsRectItem *chip = new QGraphicsRectItem(
            QRectF(5.0, (rowH - swatch) * 0.5, swatch, swatch), tag);
        chip->setBrush(QBrush(gel));
        chip->setPen(QPen(QColor(15, 20, 28), 0.8));
        chip->setAcceptedMouseButtons(Qt::NoButton);

        QGraphicsSimpleTextItem *label = new QGraphicsSimpleTextItem(name, tag);
        label->setFont(f);
        label->setBrush(QBrush(picked ? Qt::white : QColor(225, 234, 245)));
        label->setPos(5.0 + swatch + 5.0, (rowH - fm.height()) * 0.5);
        label->setAcceptedMouseButtons(Qt::NoButton);

        // The leader: this tag belongs to THAT stack, not to the deck under it.
        QGraphicsLineItem *lead = new QGraphicsLineItem(
            QLineF(anchor, QPointF(x, y + rowH * 0.5)));
        lead->setPen(QPen(QColor(120, 160, 215, 170), 0.8, Qt::DotLine));
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
    const double stageW = double(m_gridSize.width())  * m_unitValue / 1000.0;
    const double stageD = double(m_gridSize.height()) * m_unitValue / 1000.0;
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
    const FixtureRigProps rp = props->fixtureRigProps(fid);
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
    if (Scene *active = qobject_cast<Scene *>(m_doc->function(m_activeSceneId)))
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
    if (!aimed && (!fixtureAimDirection(fx, rp, dir) || dir.isNull()))
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
                              "tilt reaches %3° below level — %4 m out from %5 m up")
                           .arg(who).arg(double(nfr.tiltMax), 0, 'f', 0)
                           .arg(below, 0, 'f', 0)
                           .arg(up / qTan(qDegreesToRadians(below)), 0, 'f', 0)
                           .arg(up, 0, 'f', 1);
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
    const double stageW = double(m_gridSize.width())  * m_unitValue / 1000.0;
    const double stageD = double(m_gridSize.height()) * m_unitValue / 1000.0;
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
                               "%1 m above the deck").arg(double(above), 0, 'f', 1));
    m_reachItem->setVisible(true);
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
        if (fid != Fixture::invalidId() && !elevationFixtureDraggable(fid)
                && (item->flags() & QGraphicsItem::ItemIsMovable))
        {
            /* A free-placed fixture's stored position is MIXED UNITS: X and Y
               in millimetres, Z in METRES (see the tail of
               MonitorProperties::fixtureRigPosition, which divides X/Y by 1000
               and passes Z straight through). Writing a plain mm vector put a
               metre value of, say, 2500 into Z -- two and a half kilometres up
               -- and the redraw then placed the fixture nowhere near the drop,
               which reads as snapping back. Convert on the way in and out. */
            /* Released over a truss: bind to it. Attaching is not a top-view
               privilege -- hanging a light on a bar is exactly the thing you
               do while looking at the bar from the front. */
            if (TrussItem *over = trussUnderFixture(item, 1.0f, true))
            {
                attachFixtureToTrussAt(fid, over, item);
                updateTrussAnchorLines();
                return;
            }
            item->setAttachMode(false);

            const QVector3D stored = props->fixturePosition(fid, 0, 0);
            const QVector3D curMm(stored.x(), stored.y(), stored.z() * 1000.0f);

            /* Read the drop with the SAME anchor updateFixture places with.
               An unbound elevation fixture is anchored top-left (setPos(p)),
               a decked one bottom-centre. Reading the centre here while the
               redraw placed the top-left shifted the item by half an icon on
               every single drop -- the reported "jumps somewhere else". */
            const FixtureRigProps arp = props->fixtureRigProps(fid);
            QPointF dropPx = item->pos();
            if (arp.onDeck())
                dropPx += halfIcon(item) + QPointF(0.0, halfIcon(item).y());

            const QVector3D newMm = unprojectMm(dropPx, curMm);

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
                const QVector3D newMm = unprojectMm(item->pos(), curMm);
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
        const QPointF snapped = snapScenePos(item->pos());
        const QPointF delta = snapped - item->pos();
        if (delta.isNull() == false)
        {
            foreach (MonitorFixtureItem *mfi, movedItems)
                mfi->setPos(mfi->pos() + delta);
        }
    }

    foreach (MonitorFixtureItem *mfi, movedItems)
    {
        quint32 fid = m_fixtures.key(mfi);

        // Truss binding and snapping.
        //   • Already-bound: constrain to the existing truss line.
        MonitorProperties *props = m_doc->monitorProperties();
        FixtureRigProps rp = props->fixtureRigProps(fid);

        // Convert the fixture's pixel position into millimetres. A truss-bound
        // fixture is drawn CENTRED on its position, so read its centre (not its
        // top-left) to project onto the truss.
        const bool boundNow = (rp.trussId != Truss::invalidId());
        const QPointF refPx = boundNow ? (mfi->pos() + halfIcon(mfi)) : mfi->pos();
        QPointF mmPos;
        mmPos.setX(((refPx.x() - m_xOffset) * m_unitValue) / m_cellPixels);
        mmPos.setY(((refPx.y() - m_yOffset) * m_unitValue) / m_cellPixels);

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
            if (m_snapDivisions > 0 && m_gridSubdivisions > 0 && m_unitValue > 0)
            {
                double snapMm = m_unitValue / m_gridSubdivisions;
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

                /* mmPos was computed with the BOUND anchor (icon centre); the
                   fixture ends this drop UNBOUND, and the unbound top view
                   stores and places by the TOP-LEFT corner. Recompute with the
                   anchor of the state it will be redrawn in, or the store is
                   half an icon off and the re-place shifts it -- "snaps to a
                   different spot when I let go". */
                mmPos.setX(((mfi->pos().x() - m_xOffset) * m_unitValue) / m_cellPixels);
                mmPos.setY(((mfi->pos().y() - m_yOffset) * m_unitValue) / m_cellPixels);
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
                    mmPos.setX(((mfi->pos().x() - m_xOffset) * m_unitValue) / m_cellPixels);
                    mmPos.setY(((mfi->pos().y() - m_yOffset) * m_unitValue) / m_cellPixels);
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
            if (TrussItem *over = trussUnderFixture(mfi, 1.0f, true))
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

        // Auto DECK-MOUNT: unlike trusses, a fixture dropped over a PLATFORM
        // stands on its deck (Z = deck top) — the user asked for this. Dragging
        // it off the platform un-decks it. Riser-face, truss and pipe mounts
        // opt out.
        if (rp.trussId == Truss::invalidId() && rp.pipeId == Pipe::invalidId()
                && !rp.onRiser() && m_cellPixels > 0)
        {
            const QPointF cPx = mfi->pos() + halfIcon(mfi);
            const QPointF cMm = pixelsToRealPosition(cPx.x(), cPx.y());
            const quint32 pid = props->platformIdAt(float(cMm.x() / 1000.0),
                                                     float(cMm.y() / 1000.0));
            if (pid != rp.deckPlatformId)
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

    // Commit the undo snapshot if anything actually moved.
    if (m_pendingMoveUndo.isEmpty() == false)
    {
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
            m_moveUndo.append(e);
            while (m_moveUndo.count() > 50)
                m_moveUndo.removeFirst();
        }
        m_pendingMoveUndo.clear();
    }

    updateTrussAnchorLines();
}

QPointF MonitorGraphicsView::snapScenePos(const QPointF &p) const
{
    if (m_snapDivisions <= 0 || m_cellPixels <= 0 || m_gridSubdivisions <= 0)
        return p;
    // Snap to the visual grid subdivision lines so snap points align with what the user sees.
    const qreal inc = qreal(m_cellPixels) / m_gridSubdivisions;
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
                                             const QList<quint32> &paletteIds)
{
    if (trussIds.isEmpty() && platformIds.isEmpty() && targetIds.isEmpty())
        return;

    UndoEntry e;
    e.type = UndoEntry::FeaturePaste;
    e.pastedTrussIds    = trussIds;
    e.pastedPlatformIds = platformIds;
    e.pastedTargetIds   = targetIds;
    e.pastedPaletteIds  = paletteIds;
    m_moveUndo.append(e);
    while (m_moveUndo.count() > 50)
        m_moveUndo.removeFirst();
}

void MonitorGraphicsView::undoLastMove()
{
    if (m_moveUndo.isEmpty())
        return;
    const UndoEntry entry = m_moveUndo.takeLast();

    if (entry.type == UndoEntry::FeaturePaste)
    {
        MonitorProperties *props = m_doc->monitorProperties();
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
            p->setOriginX(float(it.value().x()));
            p->setOriginY(float(it.value().y()));
        }
        updatePlatforms();
        updatePowerSources();
        updateNetworkNodes();
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
        }
        updateTargets();
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
