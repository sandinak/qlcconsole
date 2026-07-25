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
#include <QTimer>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QMenu>

#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QGraphicsItemGroup>
#include <QtMath>
#include <QScopedValueRollback>
#include "monitorproperties.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "trussitem.h"
#include "studiogroupeditor.h"
#include "platformitem.h"
#include "powersourceitem.h"
#include "powerdistribution.h"
#include "targetitem.h"
#include "monitorimageitem.h"
#include "truss.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "qlcfixturemode.h"
#include "qlcpalette.h"
#include "scene.h"
#include "fixturegroup.h"
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
    const double cur = transform().m11(); // current uniform scale
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

bool MonitorGraphicsView::viewportEvent(QEvent *event)
{
    // Trackpad pinch: macOS sends a native zoom gesture (no Shift needed),
    // centred on the cursor via AnchorUnderMouse.
    if (event->type() == QEvent::NativeGesture)
    {
        QNativeGestureEvent *g = static_cast<QNativeGestureEvent *>(event);
        if (g->gestureType() == Qt::ZoomNativeGesture)
        {
            const double cur = transform().m11();
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

void MonitorGraphicsView::mousePressEvent(QMouseEvent *event)
{
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

    // Scene was created without a parent — delete it explicitly.
    delete m_scene;
    m_scene = nullptr;
}

void MonitorGraphicsView::setGridSize(QSize size)
{
    m_gridSize = size;
    updateGrid();
    QHashIterator <quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext() == true)
    {
        it.next();
        updateFixture(it.key());
    }
    // cell pixels / offsets may have changed: refresh snapping
    applySnapToAllItems();
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
    foreach (TargetItem *i, m_targetItems)             if (itemGroupId(i) == gid) out << i;
    foreach (PowerSourceItem *i, m_powerSourceItems)   if (itemGroupId(i) == gid) out << i;
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
    if (auto *ps = dynamic_cast<PowerSourceItem *>(gi))
    {
        PowerDistribution *pd = m_doc->powerDistribution();
        const int s = ps->sourceIndex();
        return (s >= 0 && s < pd->sources().size()) ? pd->sources().at(s).layerId : 0;
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
    PowerDistribution *pd = m_doc->powerDistribution();
    for (int s = 0; s < pd->sources().size(); s++)
        props->ensureGroup(pd->sources().at(s).groupId, pd->sources().at(s).layerId);

    // Auto-group each truss with the fixtures rigged on it.
    foreach (TrussItem *i, m_trussItems)
        if (Truss *t = i->truss())
            ensureTrussGroup(t->id());

    // Auto-group each riser with the fixtures mounted on it.
    foreach (PlatformItem *i, m_platformItems)
        if (StagePlatform *p = i->platform())
            ensurePlatformGroup(p->id());
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

    quint32 gid = p->groupId();
    if (gid == 0 || !props->hasGroup(gid))
    {
        gid = props->nextGroupId();
        const QString nm = p->name().isEmpty() ? tr("Platform %1").arg(platformId) : p->name();
        props->createGroup(gid, nm, p->layerId(), 0);
        p->setGroupId(gid);
    }
    p->setGroupId(gid);
    // Mounting is a strong signal — force the mounted fixtures into the group.
    foreach (quint32 fid, mounted)
        props->setFixtureGroup(fid, gid);

    // Anchor the group to this riser ONLY if it's dedicated (holds no other
    // structural item). A manual group of several risers stays a plain folder.
    if (structuralMembersOf(gid) <= 1)
        props->setGroupAnchor(gid, QStringLiteral("platform"), platformId);
    else
        props->setGroupAnchor(gid, QString(), 0);

    setGroupSubtreeLayer(gid, p->layerId());
    m_doc->setModified();
    emit mapStructureChanged();
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

int MonitorGraphicsView::structuralMembersOf(quint32 gid) const
{
    if (gid == 0)
        return 0;
    int n = 0;
    foreach (TrussItem *i, m_trussItems)
        if (i->truss() && i->truss()->groupId() == gid) n++;
    foreach (PlatformItem *i, m_platformItems)
        if (i->platform() && i->platform()->groupId() == gid) n++;
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

void MonitorGraphicsView::openStudioGroupEditor(quint32 groupId)
{
    if (groupId == 0 || !m_doc->monitorProperties()->hasGroup(groupId))
        return;
    StudioGroupEditor dlg(m_doc, groupId, this);
    connect(&dlg, &StudioGroupEditor::changed, this, [this, groupId]() {
        MonitorProperties *props = m_doc->monitorProperties();
        foreach (quint32 fid, props->fixtureItemsID())
            if (props->fixtureFrameGroup(fid) == groupId)
                updateFixture(fid);
        emit mapStructureChanged();
    });
    dlg.exec();
}

int MonitorGraphicsView::ungroupSelectedItems()
{
    MonitorProperties *props = m_doc->monitorProperties();

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
    else if (kind == QStringLiteral("target"))
        target = m_targetItems.value(id, nullptr);
    else if (kind == QStringLiteral("power"))
        target = (int(id) < m_powerSourceItems.size()) ? m_powerSourceItems.at(int(id)) : nullptr;
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
    else if (kind == QStringLiteral("image"))    emit imageDoubleClicked(id);
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
        else if (kind == QStringLiteral("target"))   target = m_targetItems.value(id, nullptr);
        else if (kind == QStringLiteral("power"))
            target = (int(id) < m_powerSourceItems.size()) ? m_powerSourceItems.at(int(id)) : nullptr;
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
    if (kind == QStringLiteral("target"))   return m_targetItems.value(id, nullptr);
    if (kind == QStringLiteral("power"))
        return (int(id) < m_powerSourceItems.size()) ? m_powerSourceItems.at(int(id)) : nullptr;
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

void MonitorGraphicsView::extendSelectionToGroups()
{
    // Skip while items are being torn down/rebuilt: the scene emits
    // selectionChanged from removeItem() and our hashes hold dangling pointers
    // mid-rebuild (dynamic_cast on those crashed).
    if (m_extendingSelection || m_itemsRebuilding)
        return;
    m_extendingSelection = true;

    // Selecting any grouped item selects its whole OUTERMOST group subtree.
    QSet<quint32> tops;
    foreach (QGraphicsItem *gi, m_scene->selectedItems())
    {
        const quint32 g = itemGroupId(gi);
        if (g != 0)
            tops.insert(topLevelGroup(g));
    }
    foreach (quint32 top, tops)
        foreach (QGraphicsItem *gi, itemsUnderGroup(top))
            if (!gi->isSelected())
                gi->setSelected(true);

    m_extendingSelection = false;
    emit mapSelectionChanged();
}

void MonitorGraphicsView::setStageFeaturesOnly(bool on)
{
    if (m_stageOnly == on)
        return;
    m_stageOnly = on;
    refreshItemLayerState();
}

void MonitorGraphicsView::setBuildFocus(bool on)
{
    if (m_buildFocus == on)
        return;
    m_buildFocus = on;
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
        it.value()->setVisible(lyr.visible && !m_stageOnly);   // hidden in stage view
        it.value()->setGhosted(m_buildFocus);                  // faint in build focus
        it.value()->setMovable(!m_layoutLocked && !lyr.locked && !grpLock && !m_buildFocus);
        applySelectable(it.value(), !lyr.locked && !m_buildFocus);
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
        ps->setVisible(lyr.visible && !m_stageOnly && !m_buildFocus);
        ps->setMovable(!m_layoutLocked && !src.locked && !lyr.locked);
        applySelectable(ps, !lyr.locked);
    }

    // Targets: aim points stay draggable in Design mode regardless of the global
    // layout lock, but a locked layer still freezes them; a hidden layer hides.
    foreach (TargetItem *ti, m_targetItems)
    {
        StageTarget *t = ti->target();
        const MonitorProperties::MonitorLayer lyr = props->layer(t ? t->layerId() : 0);
        const bool grpLock = props->groupChainLocked(t ? t->groupId() : 0);
        ti->setVisible(lyr.visible && !m_stageOnly && !m_buildFocus);
        ti->setMovable(m_doc->mode() == Doc::Design && !lyr.locked && !grpLock);
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

    // Elevation (Front/Side) views are read-only — EXCEPT bars we allow to be
    // dragged there for WYSIWYG placement (tower crossbars in Front).
    if (isElevation())
    {
        for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
            it.value()->setMovable(elevationFixtureDraggable(it.key()));
        foreach (TrussItem *i, m_trussItems)             i->setMovable(elevationBarDraggable(i->truss()));
        foreach (PlatformItem *i, m_platformItems)       i->setMovable(false);
        foreach (PowerSourceItem *i, m_powerSourceItems) i->setMovable(false);
        foreach (TargetItem *i, m_targetItems)           i->setMovable(false);
    }
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
    QHashIterator <quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext() == true)
    {
        it.next();
        updateFixture(it.key());
    }
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

void MonitorGraphicsView::setFixtureGelColor(quint32 id, QColor col)
{
    MonitorFixtureItem *item = m_fixtures[id];
    if (item != NULL)
        item->setGelColor(col);
}

void MonitorGraphicsView::setFixtureRotation(quint32 id, ushort degrees)
{
    MonitorFixtureItem *item = m_fixtures[id];
    if (item != NULL)
        item->setRotation(degrees);
}

void MonitorGraphicsView::showFixturesLabels(bool visible)
{
    foreach (MonitorFixtureItem *item, m_fixtures)
        item->showLabel(visible);
}

QColor MonitorGraphicsView::fixtureGelColor(quint32 id)
{
    MonitorFixtureItem *item = m_fixtures[id];
    if (item == NULL)
        return QColor();

    return item->getColor();
}

QPointF MonitorGraphicsView::realPositionToPixels(qreal xpos, qreal ypos)
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

    // Rebuild the whole projection. updateGrid() re-lays the grid and rebuilds
    // trusses/platforms/power/targets; fixtures are repositioned explicitly.
    updateGrid();
    QHashIterator<quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext()) { it.next(); updateFixture(it.key()); }
    refreshItemLayerState();
    emit rulersChanged();
}

void MonitorGraphicsView::updateFixture(quint32 id)
{
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

    MonitorFixtureItem *item = m_fixtures[id];

    // On-screen size from the physical dimensions, keeping the TRUE aspect
    // ratio (so a thin tape stays thin and its heads lay out in the right
    // number of rows). Only enforce a minimum AREA — scaling both dimensions
    // by the same factor — so very small fixtures stay visible without being
    // squashed toward square (which used to make pixel arrays wrap). Zoom
    // provides per-LED detail.
    double pw = (double(width)  * m_cellPixels) / m_unitValue;
    double ph = (double(height) * m_cellPixels) / m_unitValue;
    const double kMinArea = 16.0 * 16.0;
    const double area = pw * ph;
    if (area > 0.0 && area < kMinArea)
    {
        const double s = sqrt(kMinArea / area);
        pw *= s;
        ph *= s;
    }
    item->setSize(QSize(qMax(1, qRound(pw)), qMax(1, qRound(ph))));

    // Riser-mounted fixtures derive their position from the platform in EVERY
    // view (top too — they sit on the riser's edge/face). Free/truss fixtures
    // keep their stored XY in top view and use the 3-D rig pos in elevation.
    const FixtureRigProps frp = m_doc->monitorProperties()->fixtureRigProps(id);
    const bool riser = frp.onRiser();
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
        const QPointF p = realPositionToPixels(item->realPosition().x(), item->realPosition().y());
        item->setPos(onTruss ? (p - halfIcon(item)) : p);
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
}

void MonitorGraphicsView::refreshRiserFixtures()
{
    MonitorProperties *props = m_doc->monitorProperties();
    for (auto it = m_fixtures.constBegin(); it != m_fixtures.constEnd(); ++it)
    {
        const FixtureRigProps rp = props->fixtureRigProps(it.key());
        if (rp.onRiser() || rp.onDeck())   // deck fixtures follow the platform top too
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
    item->setVisible(lyr.visible);
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
    MonitorFixtureItem *item = NULL;

    if (id == Fixture::invalidId())
    {
        item = getSelectedItem();
        if (item != NULL)
            id = item->fixtureID();
    }
    else
        item = m_fixtures[id];

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
    MonitorProperties::MonitorImage img = props->image(item->imageId());
    const qreal scale = qreal(m_cellPixels) / m_unitValue;
    const QPointF p = item->pos();   // scene-pixel top-left

    // Persist the (possibly resized) rectangle: convert px extents back to metres.
    if (scale > 0.0)
    {
        img.width  = float(item->pixelSize().width()  / (1000.0 * scale));
        img.height = float(item->pixelSize().height() / (1000.0 * scale));
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
            if (!elevationBarDraggable(t))
                continue;
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
            const QVector3D w = t->positionAt(rp.trussOffset);   // world metres
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
            // Elevation: a riser is a box from the floor up to its height. Draw
            // its front (X extent) or side (Y extent) face.
            const float h = p->height();
            const QPointF tl = projectMm(p->originX() * 1000.0, p->originY() * 1000.0, h * 1000.0);
            pxX = float(tl.x());
            pxY = float(tl.y());
            pxW = float(((m_pov == PovFront) ? p->width() : p->depth()) * 1000.0f * scale);
            pxD = qMax(2.0f, float(h * 1000.0f * scale));
        }

        PlatformItem *pi = new PlatformItem(p, m_doc, pxX, pxY, pxW, pxD);
        pi->setMovable(!m_layoutLocked);
        m_scene->addItem(pi);
        m_platformItems.insert(p->id(), pi);

        connect(pi, &PlatformItem::itemDropped,
                this, &MonitorGraphicsView::slotPlatformMoved);
    }

    refreshItemLayerState();
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
            foreach (PlatformItem *pi, moved)
                pi->setPos(pi->pos() + delta);
    }

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

    // Fixtures mounted on these risers follow them to the new position.
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
            foreach (PowerSourceItem *ps, moved)
                ps->setPos(ps->pos() + delta);
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

    // Only show targets referenced by the active scene's Aim palettes.
    // With no scene focused there is nothing to aim, so show nothing.
    QSet<quint32> visibleTargetIds;
    if (m_activeSceneId != Function::invalidId())
    {
        Scene *activeScene = qobject_cast<Scene *>(m_doc->function(m_activeSceneId));
        if (activeScene)
        {
            foreach (quint32 pid, activeScene->palettes())
            {
                QLCPalette *pal = m_doc->palette(pid);
                if (pal && pal->type() == QLCPalette::Aim)
                    visibleTargetIds.insert(pal->stageTargetId());
            }
        }
    }

    foreach (StageTarget *t, props->stageTargets())
    {
        if (!visibleTargetIds.contains(t->id()))
            continue;

        // projectMm places the target at its 3-D position for Top or elevation.
        const QPointF tp = projectMm(t->x() * 1000.0, t->y() * 1000.0, t->z() * 1000.0);
        float pxX = float(tp.x());
        float pxY = float(tp.y());

        TargetItem *ti = new TargetItem(t, m_doc, pxX, pxY);
        // Targets are a DESIGN-time aim point: draggable while editing (even when
        // the rig layout is locked), but frozen in Run/Operate — in a show the
        // operator drives the follow-spot pin, not the target.
        ti->setMovable(m_doc->mode() == Doc::Design);
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
        QPointF mm = pixelsToRealPosition(ti->pos().x(), ti->pos().y());
        const QPointF newPos(mm.x() / 1000.0, mm.y() / 1000.0);
        if (newPos != oldPos)
        {
            e.targetPositions.insert(t->id(), oldPos);
            t->setX(float(newPos.x()));
            t->setY(float(newPos.y()));
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

    if (m_cellPixels == 0)
        return;

    // Aim lines require an active scene; no scene = no lines.
    if (m_activeSceneId == Function::invalidId())
        return;

    Scene *activeScene = qobject_cast<Scene *>(m_doc->function(m_activeSceneId));
    if (!activeScene)
        return;

    // Build the set of fixtures in the active scene.
    QSet<quint32> candidates;
    foreach (quint32 fid, activeScene->fixtures())
        candidates.insert(fid);
    foreach (quint32 gid, activeScene->fixtureGroups())
    {
        FixtureGroup *fg = m_doc->fixtureGroup(gid);
        if (fg)
            foreach (quint32 fid, fg->fixtureList())
                candidates.insert(fid);
    }

    if (candidates.isEmpty())
        return;

    // If a target is selected, draw only to selected targets.
    // If no target is selected, draw to all targets in the view.
    bool anySelected = false;
    foreach (TargetItem *ti, m_targetItems)
        if (ti->isSelected()) { anySelected = true; break; }

    foreach (TargetItem *ti, m_targetItems)
    {
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

            QPointF fixPx = mfi->sceneBoundingRect().center();

            QGraphicsLineItem *line = m_scene->addLine(
                fixPx.x(), fixPx.y(), tgtPx.x(), tgtPx.y(), aimPen);
            line->setZValue(2.5);
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
                    tgtPx.x(), tgtPx.y(), left.x(),  left.y(),  aimPen);
                QGraphicsLineItem *ar = m_scene->addLine(
                    tgtPx.x(), tgtPx.y(), right.x(), right.y(), aimPen);
                al->setZValue(2.5); ar->setZValue(2.5);
                m_aimLines.append(al); m_aimLines.append(ar);
            }
        }
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

        m_fsPinH = new QGraphicsLineItem();
        m_fsPinH->setZValue(101);
        m_scene->addItem(m_fsPinH);

        m_fsPinV = new QGraphicsLineItem();
        m_fsPinV->setZValue(101);
        m_scene->addItem(m_fsPinV);

        m_fsPinLabel = new QGraphicsSimpleTextItem();
        m_fsPinLabel->setZValue(102);
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

    if (m_gridEnabled == true)
    {
        m_xOffset = 0;
        m_yOffset = 0;
        int xInc = this->width() / m_gridSize.width();
        int yInc = this->height() / m_gridSize.height();
        if (yInc < xInc)
        {
            m_cellPixels = yInc;
            m_xOffset = (this->width() - (m_cellPixels * m_gridSize.width())) / 2;
        }
        else if (xInc < yInc)
        {
            m_cellPixels = xInc;
            m_yOffset = (this->height() - (m_cellPixels * m_gridSize.height())) / 2;
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

        // Highlight the centre vertical & horizontal axes so the stage centre
        // is obvious.
        QPen centerPen(QColor(0, 150, 140, 220)); // teal accent
        centerPen.setWidth(1);
        const qreal gridW = m_gridSize.width()  * m_cellPixels;
        const qreal gridH = m_gridSize.height() * m_cellPixels;
        const qreal cx = m_xOffset + gridW / 2.0;
        const qreal cy = m_yOffset + gridH / 2.0;
        QGraphicsLineItem *cv = m_scene->addLine(cx, m_yOffset, cx, m_yOffset + gridH, centerPen);
        cv->setZValue(2);
        m_gridItems.append(cv);
        QGraphicsLineItem *chz = m_scene->addLine(m_xOffset, cy, m_xOffset + gridW, cy, centerPen);
        chz->setZValue(2);
        m_gridItems.append(chz);

        if (m_bgItem != NULL)
        {
            m_bgItem->setX(m_xOffset);
            m_bgItem->setY(m_yOffset);
            m_bgItem->setPixmap(m_bgPixmap.scaled(xPos - m_cellPixels - m_xOffset, yPos - m_cellPixels - m_yOffset));
        }
    }

    // Match the scene rect to the viewport so that, once zoomed in, the view
    // has scroll range — which is what makes wheel-zoom anchor under the cursor
    // and Shift-drag panning work.
    if (width() > 0 && height() > 0)
        m_scene->setSceneRect(0, 0, this->width(), this->height());

    updateImages();
    updateTrusses();
    updatePlatforms();
    updatePowerSources();
    updateTargets();
}

void MonitorGraphicsView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    updateGrid();
    QHashIterator <quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext() == true)
    {
        it.next();
        updateFixture(it.key());
    }
    applySnapToAllItems();
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
                if (rp.trussId == Truss::invalidId()) continue;
                TrussItem *ti = m_trussItems.value(rp.trussId, nullptr);
                if (!ti) continue;

                QPointF local = ti->mapFromScene(mfi->sceneBoundingRect().center());
                float distPx;
                if (ti->truss()->type() == Truss::Vertical)
                    // Radial distance from tower centre
                    distPx = float(qSqrt(local.x() * local.x() + local.y() * local.y()));
                else
                    // Perpendicular distance from truss centreline
                    distPx = qAbs(float(local.y()));
                mfi->setEscapeMode(distPx > ti->pxWid() * 2.0f);
            }
        }
        else
        {
            // Not dragging a fixture — ensure no stale escape-mode borders remain
            foreach (QGraphicsItem *gi, m_scene->selectedItems())
            {
                if (auto *mfi = dynamic_cast<MonitorFixtureItem *>(gi))
                    mfi->setEscapeMode(false);
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
    QGraphicsItem *it = topPickableAt(sp);

    // Walk up to the top-level item (e.g. from a label child to TrussItem).
    while (it && it->parentItem())
        it = it->parentItem();

    if (auto *fi = dynamic_cast<MonitorFixtureItem *>(it))
    {
        // Grouped OR truss-bound fixture: the FIRST double-click DRILLS IN —
        // selects just this fixture (distinct cyan-dashed highlight) so it can
        // be slid along its truss or dragged clear to detach. Single-click still
        // selects the whole group / truss.
        //
        // A SECOND double-click on the already-drilled-in fixture opens its
        // editor (drill-in then act — no need to reach for the right-click menu).
        if ((itemGroupId(fi) != 0 || fi->isBoundToTruss()) && !fi->isIsolated())
        {
            m_extendingSelection = true;   // suppress select-together
            m_scene->clearSelection();
            fi->setSelected(true);
            fi->setIsolated(true);
            m_extendingSelection = false;
            emit mapSelectionChanged();
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
        emit trussDoubleClicked(ti->trussId());
        return;
    }
    if (auto *pi = dynamic_cast<PlatformItem *>(it))
    {
        emit platformDoubleClicked(pi->platformId());
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

    QGraphicsView::mouseDoubleClickEvent(event);
}

void MonitorGraphicsView::contextMenuEvent(QContextMenuEvent *event)
{
    QPointF sp = mapToScene(event->pos());
    QGraphicsItem *it = topPickableAt(sp);
    while (it && it->parentItem())
        it = it->parentItem();

    MonitorProperties *props = m_doc->monitorProperties();

    if (auto *fi = dynamic_cast<MonitorFixtureItem *>(it))
    {
        const quint32 fid = fi->fixtureID();
        const FixtureRigProps rp = props->fixtureRigProps(fid);
        QMenu menu(this);

        QAction *editAct = menu.addAction(tr("Edit Fixture…"));

        // When the fixture sits ON a truss (and hides it), offer a way to reach
        // the truss underneath — click-through to its editor.
        QAction *editTrussAct = nullptr;
        if (rp.trussId != Truss::invalidId())
        {
            Truss *t = props->truss(rp.trussId);
            const QString tn = (t && !t->name().isEmpty()) ? t->name()
                                                           : tr("Truss %1").arg(rp.trussId);
            editTrussAct = menu.addAction(tr("Edit Truss “%1”…").arg(tn));
        }
        menu.addSeparator();

        // Attach to truss ▶ (explicit — 2D drop-to-bind was removed).
        QMenu *attach = menu.addMenu(tr("Attach to Truss"));
        const QList<Truss *> trusses = props->trusses();
        if (trusses.isEmpty())
            attach->setEnabled(false);
        for (Truss *t : trusses)
        {
            if (t == nullptr)
                continue;
            const quint32 tid = t->id();
            QAction *a = attach->addAction(t->name().isEmpty()
                                           ? tr("Truss %1").arg(tid) : t->name());
            a->setCheckable(true);
            a->setChecked(rp.trussId == tid);
            connect(a, &QAction::triggered, this, [this, fid, tid]() {
                attachFixtureToTruss(fid, tid);
            });
        }

        QAction *detachAct = menu.addAction(tr("Remove from Truss"));
        detachAct->setEnabled(rp.trussId != Truss::invalidId());
        connect(detachAct, &QAction::triggered, this, [this, fid]() {
            detachFixtureFromTruss(fid);
        });

        // Group position lock (when this fixture belongs to a group).
        const quint32 gid = itemGroupId(fi);
        QAction *grpLockAct = nullptr;
        if (gid != 0)
        {
            const bool gl = props->group(gid).locked;
            grpLockAct = menu.addAction(gl ? tr("Unlock Group Position")
                                           : tr("Lock Group Position"));
        }

        // Fixture Studio: build a studio group (rigid unit with a local frame)
        // from the current multi-selection, or edit the one this fixture is in.
        menu.addSeparator();
        const quint32 frameGid = props->fixtureFrameGroup(fid);
        QAction *makeStudioAct = nullptr;
        QAction *editStudioAct = nullptr;
        if (frameGid != 0)
            editStudioAct = menu.addAction(tr("Edit Studio Group…"));
        if (selectedFixtureCount() >= 2)
            makeStudioAct = menu.addAction(tr("Create Studio Group from Selection"));

        menu.addSeparator();
        QAction *removeAct = menu.addAction(tr("Remove from View"));

        QAction *chosen = menu.exec(event->globalPos());
        if (chosen == editAct)
            emit fixtureDoubleClicked(fid);
        else if (editTrussAct && chosen == editTrussAct)
            emit trussDoubleClicked(rp.trussId);
        else if (makeStudioAct && chosen == makeStudioAct)
        {
            const quint32 ng = createStudioGroupFromSelection();
            if (ng != 0)
                openStudioGroupEditor(ng);
        }
        else if (editStudioAct && chosen == editStudioAct)
            openStudioGroupEditor(frameGid);
        else if (chosen == removeAct)
        {
            removeFixture(fid);
            m_doc->setModified();
            emit mapStructureChanged();
        }
        else if (grpLockAct && chosen == grpLockAct)
        {
            props->setGroupLocked(gid, !props->group(gid).locked);
            m_doc->setModified();
            refreshItemLayerState();
        }
        return;
    }

    if (auto *ti = dynamic_cast<TrussItem *>(it))
    {
        Truss *t = ti->truss();
        const quint32 tid = ti->trussId();
        const bool locked = (t && t->locked());
        QMenu menu(this);
        QAction *editAct   = menu.addAction(tr("Edit Truss…"));
        QAction *addBarAct = menu.addAction(tr("Add Bar…"));
        addBarAct->setToolTip(tr("Hang a cross-bar on this truss (mid-span); "
                                 "edit it to set run, position and drop"));
        QAction *deleteAct = menu.addAction(tr("Delete Truss"));
        menu.addSeparator();
        QAction *lockAct = menu.addAction(locked ? tr("Unlock Position")
                                                 : tr("Lock Position"));
        lockAct->setToolTip(tr("Freeze this truss in place — overrides the layer lock"));
        QAction *chosen = menu.exec(event->globalPos());
        if (chosen == editAct)
            emit trussDoubleClicked(tid);
        else if (chosen == addBarAct)
            emit addBarToTrussRequested(tid, -1.0f);   // -1 = mid-span default
        else if (chosen == deleteAct)
            emit trussRemoveRequested(tid);
        else if (chosen == lockAct && t)
        {
            t->setLocked(!t->locked());
            m_doc->setModified();
            refreshItemLayerState();
            ti->update();
        }
        return;
    }

    if (auto *pi = dynamic_cast<PlatformItem *>(it))
    {
        StagePlatform *p = pi->platform();
        const quint32 pid = pi->platformId();
        const bool locked = (p && p->locked());
        QMenu menu(this);
        QAction *editAct   = menu.addAction(tr("Edit Platform…"));
        QAction *deleteAct = menu.addAction(tr("Delete Platform"));
        menu.addSeparator();
        QAction *lockAct = menu.addAction(locked ? tr("Unlock Position")
                                                 : tr("Lock Position"));
        lockAct->setToolTip(tr("Freeze this platform in place — overrides the layer lock"));
        QAction *chosen = menu.exec(event->globalPos());
        if (chosen == editAct)
            emit platformDoubleClicked(pid);
        else if (chosen == deleteAct)
            emit platformRemoveRequested(pid);
        else if (chosen == lockAct && p)
        {
            p->setLocked(!p->locked());
            m_doc->setModified();
            refreshItemLayerState();
            pi->update();
        }
        return;
    }

    if (auto *ii = dynamic_cast<MonitorImageItem *>(it))
    {
        const quint32 iid = ii->imageId();
        const bool locked = props->image(iid).locked;
        QMenu menu(this);
        QAction *editAct   = menu.addAction(tr("Edit Image…"));
        QAction *deleteAct = menu.addAction(tr("Delete Image"));
        menu.addSeparator();
        QAction *lockAct = menu.addAction(locked ? tr("Unlock Position")
                                                 : tr("Lock Position"));
        QAction *chosen = menu.exec(event->globalPos());
        if (chosen == editAct)
            emit imageDoubleClicked(iid);
        else if (chosen == deleteAct)
            emit imageRemoveRequested(iid);
        else if (chosen == lockAct)
        {
            MonitorProperties::MonitorImage img = props->image(iid);
            img.locked = !img.locked;
            props->setImage(img);
            m_doc->setModified();
            refreshItemLayerState();
            ii->update();
        }
        return;
    }

    // Empty canvas (or any other item) → the generic add/paste menu.
    emit contextMenuRequested(sp);
}

void MonitorGraphicsView::attachFixtureToTruss(quint32 fid, quint32 trussId)
{
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *t = props->truss(trussId);
    MonitorFixtureItem *mfi = m_fixtures.value(fid, nullptr);
    if (t == nullptr || mfi == nullptr || m_cellPixels == 0)
        return;

    FixtureRigProps rp = props->fixtureRigProps(fid);
    rp.trussId = trussId;

    // Snap the fixture onto the nearest point of the truss line so it doesn't
    // jump — mirrors the drag-drop snap (project centre onto the truss).
    QPointF mm = pixelsToRealPosition(mfi->sceneBoundingRect().center().x(),
                                      mfi->sceneBoundingRect().center().y());
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

void MonitorGraphicsView::detachFixtureFromTruss(quint32 fid)
{
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(fid);
    if (rp.trussId == Truss::invalidId())
        return;
    rp.trussId     = Truss::invalidId();
    rp.trussOffset = 0.0f;
    props->setFixtureRigProps(fid, rp);
    if (MonitorFixtureItem *mfi = m_fixtures.value(fid, nullptr))
    {
        mfi->setBoundToTruss(false);
        mfi->setEscapeMode(false);
    }
    // Leave it in the truss group for now (the user can regroup) — detaching is
    // about the rig binding, not the spatial grouping.
    m_doc->setModified();
    emit mapStructureChanged();
}

void MonitorGraphicsView::slotFixtureMoved(MonitorFixtureItem *item)
{
    // ELEVATION drag of a truss-bound fixture: it slides ALONG its truss/bar, so
    // convert the drop delta into a new trussOffset (Front view only).
    if (isElevation() && item != nullptr)
    {
        MonitorProperties *props = m_doc->monitorProperties();
        const quint32 fid = m_fixtures.key(item, Fixture::invalidId());
        if (fid != Fixture::invalidId() && elevationFixtureDraggable(fid))
        {
            FixtureRigProps rp = props->fixtureRigProps(fid);
            Truss *truss = props->truss(rp.trussId);
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
                const QVector3D axis = (truss->type() == Truss::Vertical)
                    ? QVector3D(0, 0, 1)
                    : QVector3D(truss->direction().x(), truss->direction().y(), 0);
                const double dOff = QVector3D::dotProduct(wd, axis);
                float off = qBound(0.0f, rp.trussOffset + float(dOff), truss->length());
                rp.trussOffset = off;
                props->setFixtureRigProps(fid, rp);
                const QVector3D nw = truss->positionAt(off);   // new world pos
                updateFixture(fid);
                emit fixtureMoved(fid, QPointF(nw.x() * 1000.0, nw.y() * 1000.0));
                m_doc->setModified();
            }
        }
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
            foreach (MonitorFixtureItem *mfi, movedItems)
                mfi->setPos(mfi->pos() + delta);
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

            // Horizontal / Ground: project the fixture position onto the truss direction.
            double ox = t->origin().x() * 1000.0;
            double oy = t->origin().y() * 1000.0;
            double dx = t->direction().x();
            double dy = t->direction().y();
            double dot = (mmPos.x() - ox) * dx + (mmPos.y() - oy) * dy;
            dot = qBound(0.0, dot, t->length() * 1000.0);

            // Snap along the truss to the same grid subdivision intervals.
            if (m_snapDivisions > 0 && m_gridSubdivisions > 0 && m_unitValue > 0)
            {
                double snapMm = m_unitValue / m_gridSubdivisions;
                dot = qRound(dot / snapMm) * snapMm;
                dot = qBound(0.0, dot, t->length() * 1000.0);
            }

            mmPos = QPointF(ox + dx * dot, oy + dy * dot);
            rp.trussOffset = float(dot / 1000.0);
            props->setFixtureRigProps(fid, rp);
            mfi->setPos(realPositionToPixels(mmPos.x(), mmPos.y()) - halfIcon(mfi));
        };

        if (rp.trussId != Truss::invalidId() && m_cellPixels > 0)
        {
            if (mfi->escapeMode())
            {
                // Pulled too far off the truss — unbind
                rp.trussId     = Truss::invalidId();
                rp.trussOffset = 0.0f;
                props->setFixtureRigProps(fid, rp);
                mfi->setEscapeMode(false);
                mfi->setBoundToTruss(false);
            }
            else
            {
                // Already bound — constrain to the current truss.
                Truss *t = props->truss(rp.trussId);
                if (t != nullptr)
                    snapToTruss(t);
            }
        }
        // NOTE: dropping an unbound fixture onto a truss no longer auto-binds it
        // (the "truss grabs overlapping fixtures" problem). Truss assignment is
        // now explicit — via the fixture's right-click "Attach to Truss" menu or
        // by dragging it onto the truss node in the Layers tree. A fixture that
        // is already bound still snaps/constrains and detaches on escape (above).

        // Auto DECK-MOUNT: unlike trusses, a fixture dropped over a PLATFORM
        // stands on its deck (Z = deck top) — the user asked for this. Dragging
        // it off the platform un-decks it. Riser-face and truss mounts opt out.
        if (rp.trussId == Truss::invalidId() && !rp.onRiser() && m_cellPixels > 0)
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
