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
#include <QShortcut>

#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QGraphicsItemGroup>
#include <QtMath>
#include "monitorproperties.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "trussitem.h"
#include "platformitem.h"
#include "targetitem.h"
#include "truss.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "qlcfixturemode.h"
#include "qlcpalette.h"
#include "scene.h"
#include "function.h"
#include "doc.h"

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

    // dragging on the empty canvas rubber-band selects fixtures.
    // This works both when locked and unlocked.
    setDragMode(QGraphicsView::RubberBandDrag);
    // Shift+wheel zoom centres on the cursor.
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    // Cmd/Ctrl+Z undoes the last fixture move.
    QShortcut *undoSc = new QShortcut(QKeySequence::Undo, this);
    undoSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoSc, &QShortcut::activated, this, &MonitorGraphicsView::undoLastMove);

    // Refresh aim lines whenever the active scene's fixture/value list changes.
    connect(m_doc, &Doc::functionChanged, this, [this](quint32 fid) {
        if (fid == m_activeSceneId)
            updateAimLines();
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
        scale(target / cur, target / cur);
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
                scale(target / cur, target / cur);
            return true;
        }
    }
    return QGraphicsView::viewportEvent(event);
}

void MonitorGraphicsView::mousePressEvent(QMouseEvent *event)
{
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
    foreach (MonitorFixtureItem *item, m_fixtures)
        item->setMovable(!locked);
    foreach (TrussItem *ti, m_trussItems)
        ti->setMovable(!locked);  // TrussItem::setMovable respects per-truss lock internally
    foreach (PlatformItem *pi, m_platformItems)
        pi->setMovable(!locked);
    foreach (TargetItem *ti, m_targetItems)
        ti->setMovable(!locked);
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

    item->setPos(realPositionToPixels(item->realPosition().x(), item->realPosition().y()));
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
    item->setMovable(!m_layoutLocked);
    // Reflect any existing truss binding (e.g. on workspace load)
    FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(id);
    item->setBoundToTruss(rp.trussId != Truss::invalidId());
    m_fixtures[id] = item;
    m_scene->addItem(item);
    updateFixture(id);
    applySnapToItem(item);
    connect(item, SIGNAL(itemDropped(MonitorFixtureItem*)),
            this, SLOT(slotFixtureMoved(MonitorFixtureItem*)));
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
    foreach (MonitorFixtureItem *item, m_fixtures)
        delete item;
    m_fixtures.clear();
}

void MonitorGraphicsView::updateTrusses()
{
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
        QPointF p0 = realPositionToPixels(t->origin().x() * 1000.0,
                                          t->origin().y() * 1000.0);

        float pxWid = float(qMax(8.0, (t->width()  * 1000.0 * m_cellPixels) / m_unitValue));
        float pxLen;
        float angleDeg = 0.0f;

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

        TrussItem *ti = new TrussItem(t, m_doc, pxLen, pxWid);
        ti->setPos(p0);
        ti->setRotation(angleDeg);
        ti->setMovable(!m_layoutLocked && !t->locked());
        m_scene->addItem(ti);
        m_trussItems.insert(t->id(), ti);

        connect(ti, &TrussItem::itemDropped,
                this, &MonitorGraphicsView::slotTrussMoved);
        connect(ti, &TrussItem::addFixtureRequested,
                this, &MonitorGraphicsView::addFixtureToTrussRequested);
    }
}

void MonitorGraphicsView::slotTrussMoved(TrussItem *item)
{
    // Convert current scene position back to world metres and persist.
    QPointF sp  = item->pos();               // scene px (origin of truss)
    QPointF mm  = pixelsToRealPosition(sp.x(), sp.y());
    Truss  *t   = item->truss();
    QVector3D newOrigin(float(mm.x() / 1000.0), float(mm.y() / 1000.0), t->origin().z());

    if (newOrigin != t->origin())
    {
        UndoEntry e; e.type = UndoEntry::TrussMove;
        e.trussOrigins.insert(t->id(), t->origin());
        m_moveUndo.append(e);
        while (m_moveUndo.count() > 50)
            m_moveUndo.removeFirst();
    }

    t->setOrigin(newOrigin);
    m_doc->setModified();
}

void MonitorGraphicsView::updatePlatforms()
{
    foreach (PlatformItem *pi, m_platformItems)
    {
        m_scene->removeItem(pi);
        delete pi;
    }
    m_platformItems.clear();

    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr || m_cellPixels == 0)
        return;

    foreach (StagePlatform *p, props->platforms())
    {
        float pxX = float(m_xOffset + (p->originX() * 1000.0 * m_cellPixels) / m_unitValue);
        float pxY = float(m_yOffset + (p->originY() * 1000.0 * m_cellPixels) / m_unitValue);
        float pxW = float((p->width()  * 1000.0 * m_cellPixels) / m_unitValue);
        float pxD = float((p->depth()  * 1000.0 * m_cellPixels) / m_unitValue);

        PlatformItem *pi = new PlatformItem(p, m_doc, pxX, pxY, pxW, pxD);
        pi->setMovable(!m_layoutLocked);
        m_scene->addItem(pi);
        m_platformItems.insert(p->id(), pi);

        connect(pi, &PlatformItem::itemDropped,
                this, &MonitorGraphicsView::slotPlatformMoved);
    }
}

void MonitorGraphicsView::slotPlatformMoved(PlatformItem *item)
{
    QPointF sp = item->pos();
    QPointF mm = pixelsToRealPosition(sp.x(), sp.y());
    StagePlatform *p = item->platform();
    p->setOriginX(float(mm.x() / 1000.0));
    p->setOriginY(float(mm.y() / 1000.0));
    m_doc->setModified();
    emit platformMoved(item->platformId(),
                       QPointF(double(p->originX()), double(p->originY())));
}

void MonitorGraphicsView::updateTargets()
{
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

    foreach (StageTarget *t, props->stageTargets())
    {
        float pxX = float(m_xOffset + (t->x() * 1000.0 * m_cellPixels) / m_unitValue);
        float pxY = float(m_yOffset + (t->y() * 1000.0 * m_cellPixels) / m_unitValue);

        TargetItem *ti = new TargetItem(t, m_doc, pxX, pxY);
        ti->setMovable(!m_layoutLocked);
        m_scene->addItem(ti);
        m_targetItems.insert(t->id(), ti);

        connect(ti, &TargetItem::itemDropped,
                this, &MonitorGraphicsView::slotTargetMoved);
    }

    updateAimLines();
}

void MonitorGraphicsView::slotTargetMoved(TargetItem *item)
{
    QPointF sp = item->pos();
    QPointF mm = pixelsToRealPosition(sp.x(), sp.y());
    StageTarget *t = item->target();
    t->setX(float(mm.x() / 1000.0));
    t->setY(float(mm.y() / 1000.0));
    m_doc->setModified();
    updateAimLines();
    emit targetMoved(item->targetId(), QPointF(double(t->x()), double(t->y())));

    // Snap running scenes whose palettes reference this target to the new
    // position immediately (override fade to 0 for one write cycle).
    const quint32 tid = item->targetId();
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

    MonitorProperties *props = m_doc->monitorProperties();
    if (props == nullptr)
        return;

    // Only draw aim lines when a specific scene is active in the Programming tab.
    // Lines show only for palettes that (a) belong to the active scene and
    // (b) link to a target via stageTargetId.
    if (m_activeSceneId == Function::invalidId())
        return;

    Scene *activeScene = qobject_cast<Scene *>(m_doc->function(m_activeSceneId));
    if (!activeScene)
        return;

    const QMap<quint32, FixtureRigProps> &rigMap = props->fixtureRigPropsMap();

    // Build the set of fixtures in this scene once.
    QSet<quint32> sceneFixtures;
    foreach (quint32 fid, activeScene->fixtures())
        sceneFixtures.insert(fid);

    // Each fixture points to at most one target — the first one it's matched to.
    QSet<quint32> assignedFixtures;

    foreach (TargetItem *ti, m_targetItems)
    {
        StageTarget *tgt = ti->target();
        const quint32 targetId = tgt->id();

        // Only draw if at least one of this scene's palettes links to this target.
        bool linked = false;
        foreach (quint32 pid, activeScene->palettes())
        {
            QLCPalette *pal = m_doc->palette(pid);
            if (pal && pal->stageTargetId() == targetId)
            { linked = true; break; }
        }

        if (!linked)
            continue;

        if (sceneFixtures.isEmpty())
            continue;

        QPointF tgtPx = ti->pos();
        QPen aimPen(tgt->color().isValid() ? tgt->color() : QColor(255, 180, 0), 1.0);
        aimPen.setStyle(Qt::DashLine);

        for (quint32 fid : sceneFixtures)
        {
            if (assignedFixtures.contains(fid))
                continue; // already drawn a line to another target for this fixture

            if (!m_fixtures.contains(fid))
                continue;

            // Only fixtures on a truss have a meaningful 3D rig position
            auto rigIt = rigMap.find(fid);
            if (rigIt == rigMap.constEnd() || rigIt.value().trussId == Truss::invalidId())
                continue;

            MonitorFixtureItem *mfi = m_fixtures.value(fid);
            if (!mfi)
                continue;

            assignedFixtures.insert(fid);
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
    updateAimLines();
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

void MonitorGraphicsView::updateGrid()
{
    int itemsCount = m_gridItems.count();
    for (int i = 0; i < itemsCount; i++)
        m_scene->removeItem((QGraphicsItem *)m_gridItems.takeLast());

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

    updateTrusses();
    updatePlatforms();
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

        if (draggingFixture)
        {
            QPointF scenePos = mapToScene(event->pos());
            MonitorProperties *props = m_doc->monitorProperties();

            // Highlight trusses under cursor.
            // Vertical towers have a tiny footprint — use an expanded radius so
            // the user can reliably hover the cursor over them.
            for (TrussItem *ti : m_trussItems)
            {
                bool hit;
                QPointF local = ti->mapFromScene(scenePos);
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
                ti->setHighlighted(hit);
            }

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

    QGraphicsView::mouseMoveEvent(event);
}

void MonitorGraphicsView::mouseDoubleClickEvent(QMouseEvent *event)
{
    // Suppress the viewClicked that would fire on the release following this
    // double-click (which would immediately close whatever the double-click opened).
    m_suppressNextViewClick = true;

    QPointF sp = mapToScene(event->pos());
    QGraphicsItem *it = m_scene->itemAt(sp, QTransform());

    // Walk up to the top-level item (e.g. from a label child to TrussItem).
    while (it && it->parentItem())
        it = it->parentItem();

    if (auto *fi = dynamic_cast<MonitorFixtureItem *>(it))
    {
        // If the fixture sits on a vertical truss, open the truss editor instead:
        // the fixture is stacked directly on the tower icon and there is no other
        // way to reach the truss edit dialog by clicking.
        if (fi->isBoundToTruss())
        {
            MonitorProperties *props = m_doc->monitorProperties();
            FixtureRigProps rp = props->fixtureRigProps(fi->fixtureID());
            if (rp.trussId != Truss::invalidId())
            {
                Truss *t = props->truss(rp.trussId);
                if (t && t->type() == Truss::Vertical)
                {
                    emit trussDoubleClicked(rp.trussId);
                    return;
                }
            }
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

    QGraphicsView::mouseDoubleClickEvent(event);
}

void MonitorGraphicsView::contextMenuEvent(QContextMenuEvent *event)
{
    QPointF sp = mapToScene(event->pos());
    QGraphicsItem *it = m_scene->itemAt(sp, QTransform());
    while (it && it->parentItem())
        it = it->parentItem();

    bool isFixture = dynamic_cast<MonitorFixtureItem *>(it) != nullptr;
    bool isTruss   = dynamic_cast<TrussItem *>(it) != nullptr;

    if (!isFixture && !isTruss)
    {
        emit contextMenuRequested(sp);
        return;
    }
    QGraphicsView::contextMenuEvent(event);
}

void MonitorGraphicsView::slotFixtureMoved(MonitorFixtureItem *item)
{
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

        // Convert the pixel position of the fixture into position in millimeters.
        QPointF mmPos;
        mmPos.setX(((mfi->x() - m_xOffset) * m_unitValue) / m_cellPixels);
        mmPos.setY(((mfi->y() - m_yOffset) * m_unitValue) / m_cellPixels);

        // Truss binding and snapping.
        //   • Already-bound: constrain to the existing truss line.
        //   • Unbound (only the directly-dragged item): auto-bind if dropped on a truss.
        MonitorProperties *props = m_doc->monitorProperties();
        FixtureRigProps rp = props->fixtureRigProps(fid);

        auto snapToTruss = [&](Truss *t)
        {
            if (t->type() == Truss::Vertical)
            {
                // Vertical tower: fixture stays at the tower's XY position in the 2D view.
                // Z-height (trussOffset) is set via the dialog strip — don't overwrite it here.
                mmPos = QPointF(t->origin().x() * 1000.0, t->origin().y() * 1000.0);
                props->setFixtureRigProps(fid, rp);
                mfi->setPos(realPositionToPixels(mmPos.x(), mmPos.y()));
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
            mfi->setPos(realPositionToPixels(mmPos.x(), mmPos.y()));
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
        else
        {
            // Not yet bound: check if this fixture landed on a truss.
            // Applies to all selected fixtures, not just the primary dragged one,
            // so the whole selection can bind in one drag.
            //
            // Primary check: did the cursor hover over a truss at release?
            // (Captured in mouseReleaseEvent via m_droppedOnTrussIds — reliable
            //  for tiny vertical-tower footprints where fixture-centre geometry
            //  doesn't match the cursor's position.)
            // Fallback: geometric fixture-centre containment for horizontal trusses.
            QPointF sceneCenter = mfi->sceneBoundingRect().center();
            for (TrussItem *ti : m_trussItems)
            {
                bool hit = m_droppedOnTrussIds.contains(ti->trussId());
                if (!hit)
                {
                    QPointF local = ti->mapFromScene(sceneCenter);
                    hit = ti->contains(local);
                }
                if (hit)
                {
                    rp.trussId = ti->trussId();
                    snapToTruss(ti->truss());
                    mfi->setBoundToTruss(true);
                    break;
                }
            }
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

void MonitorGraphicsView::undoLastMove()
{
    if (m_moveUndo.isEmpty())
        return;
    const UndoEntry entry = m_moveUndo.takeLast();

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
            if (ti == nullptr)
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
