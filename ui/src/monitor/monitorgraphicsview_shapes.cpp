/*
  Q Light Controller Plus
  monitorgraphicsview_shapes.cpp

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

/* The plot's editing of stage features by hand (docs/features/
   lighting-studio.md "Stretch and turn by hand", "Grouping stage features"):

   - the stretch and rotate handles on a platform, a truss or scenery
     selected on its own, and the rotate knob on a selection of several,
     shown only while nothing holds them still; a drag shows the shape it
     will take and its size or angle in Settings' unit, and lands as one
     undo step;
   - Group ▸: New Group (no prompt), add to a group already on the layer, Remove from
     Group -- greyed, with the reason, when a lock stops it. */

#include <QApplication>
#include <QInputDialog>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QMenu>
#include <QtMath>
#include <QTimer>

#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "monitorproperties.h"
#include "measurementunits.h"
#include "stagepieceitem.h"
#include "shapehandles.h"
#include "platformitem.h"
#include "sceneryitem.h"
#include "focusmenu.h"
#include "trussitem.h"
#include "toweritem.h"
#include "standitem.h"
#include "pipeitem.h"
#include "scenery.h"
#include "pipe.h"
#include "truss.h"
#include "stageplatform.h"
#include "itemlock.h"
#include "doc.h"

namespace
{
const qreal kHandlePx = 6.0;     ///< a handle's half size on screen
const qreal kKnobGapPx = 26.0;   ///< the knob's distance off its edge on screen

bool isStructureKind(const QString &kind)
{
    return kind == QLatin1String("platform") || kind == QLatin1String("truss")
        || kind == QLatin1String("scenery") || kind == QLatin1String("tower")
        || kind == QLatin1String("pipe") || kind == QLatin1String("stand");
}
}

/*********************************************************************
 * Stretch and rotate handles
 *********************************************************************/

QPointF MonitorGraphicsView::planToScene(const QPointF &m) const
{
    return projectMm(m.x() * 1000.0, m.y() * 1000.0, 0.0);
}

QPolygonF MonitorGraphicsView::planToScene(const QPolygonF &poly) const
{
    QPolygonF out;
    for (const QPointF &p : poly)
        out << planToScene(p);
    return out;
}

QPointF MonitorGraphicsView::sceneToPlan(const QPointF &sp) const
{
    const QVector3D mm = unprojectMm(sp, QVector3D());
    return QPointF(double(mm.x()) / 1000.0, double(mm.y()) / 1000.0);
}

ShapeHandles *MonitorGraphicsView::shapeHandles() const
{
    return m_shapeHandles != nullptr && m_shapeHandles->isVisible() && m_shapeHandles->count() > 0
               ? m_shapeHandles : nullptr;
}

QString MonitorGraphicsView::shapeHeldReason() const
{
    return m_shapeHandles != nullptr && m_shapeHandles->isVisible() ? m_shapeHandles->heldReason()
                                                                     : QString();
}

QString MonitorGraphicsView::shapeHandleTip(const QPoint &viewPos) const
{
    const ShapeHandles *h = shapeHandles();
    if (h == nullptr)
        return QString();
    const StageShape::Handle over = h->handleAt(mapToScene(viewPos));
    if (over == StageShape::NoHandle)
        return QString();
    if (over == StageShape::RotateKnob)
        return tr("Drag to turn · ⌥: no 15° steps");
    if (h->stageShape().canSwing && (over == StageShape::EndPlusU || over == StageShape::EndMinusU))
        return tr("Drag to stretch or swing · Shift: 15° steps · ⌥: no snap");
    return tr("Drag to stretch · ⌥: no snap");
}

StageShape MonitorGraphicsView::shapeTarget(QString *why, StageShape *held) const
{
    auto refuse = [why](const QString &reason) {
        if (why != nullptr)
            *why = reason;
        return StageShape();
    };
    MonitorProperties *props = m_doc != nullptr ? m_doc->monitorProperties() : nullptr;
    if (props == nullptr || m_scene == nullptr)
        return refuse(QString());
    if (m_doc->mode() != Doc::Design)
        return refuse(tr("Operate: switch to Design to change the stage"));
    if (m_pov != PovTop)
        return refuse(tr("Stretched and turned from above (Top)"));

    const QList<QPair<QString, quint32> > sel = selectedMapObjects();
    QList<QPair<QString, quint32> > structures;
    for (const auto &o : sel)
    {
        if (o.first == QLatin1String("stage"))
            return refuse(QString());           // the stage marks have their own drags
        const int holds = itemHolds(o.first, o.second);
        if (holds != ItemLock::NotHeld)
        {
            // Picked on its own: what it would have, to draw it held.
            if (held != nullptr && sel.size() == 1 && isStructureKind(o.first))
                *held = StageShape::of(props, o.first, o.second);
            switch (ItemLock::primaryHold(holds))
            {
                case ItemLock::HeldPlot:   return refuse(tr("Plot locked — Edit Plot to change it"));
                case ItemLock::HeldLayer:  return refuse(tr("Layer locked — unlock it in Plot layers"));
                case ItemLock::HeldFolder: return refuse(tr("Group locked — unlock it in Plot layers"));
                default:                   return refuse(tr("Locked — unlock it to change it"));
            }
        }
        if (isStructureKind(o.first))
            structures << o;
    }
    if (structures.isEmpty())
        return refuse(QString());
    if (structures.size() == 1)
    {
        StageShape s = StageShape::of(props, structures.first().first, structures.first().second);
        if (s.isValid())
            return s;
        if (sel.size() == 1)
            return refuse(QString());           // a tower, a stand: no footprint to stretch
    }
    // Several (a group, or what was picked together): turned as one.
    const QRectF box = selectionBox();
    if (box.isNull())
        return refuse(QString());
    StageShape s;
    s.kind = QStringLiteral("selection");
    s.centre = box.center();
    s.length = qMax(0.2, box.width());
    s.width = qMax(0.2, box.height());
    s.canRotate = true;
    return s;
}

void MonitorGraphicsView::refreshShapeHandles()
{
    if (m_scene == nullptr)
        return;
    if (m_itemsRebuilding)
    {
        /* Items are being torn down and made again (and the selection with
           them): look again once that is over. */
        if (!m_shapeRefreshPending)
        {
            m_shapeRefreshPending = true;
            QTimer::singleShot(0, this, [this]() {
                m_shapeRefreshPending = false;
                refreshShapeHandles();
            });
        }
        return;
    }
    if (m_shapeDragHandle != StageShape::NoHandle)
        return;                                 // the drag's own until it lets go
    QString why;
    StageShape held;
    const StageShape s = shapeTarget(&why, &held);
    const double scale = viewScale() > 0.0 ? viewScale() : 1.0;
    if (!s.isValid() && held.isValid() && !why.isEmpty())
    {
        /* Selected, but a lock holds it: its outline in the lock colour and
           what holds it, so a grab that does nothing says why. */
        if (m_shapeHandles == nullptr)
        {
            m_shapeHandles = new ShapeHandles();
            m_scene->addItem(m_shapeHandles);
        }
        const QPolygonF outline = planToScene(held.outline());
        const QRectF box = outline.boundingRect();
        m_shapeHandles->setHeld(held, outline, why,
                                QPointF(box.left(), box.bottom() + 6.0 / scale), kHandlePx / scale);
        m_shapeHandles->show();
        if (m_shapeCursor)
        {
            m_shapeCursor = false;
            viewport()->unsetCursor();
        }
        return;
    }
    if (!s.isValid())
    {
        if (m_shapeHandles != nullptr)
            m_shapeHandles->hide();
        if (m_shapeCursor)
        {
            m_shapeCursor = false;
            viewport()->unsetCursor();
        }
        return;
    }
    if (m_shapeHandles == nullptr)
    {
        m_shapeHandles = new ShapeHandles();
        m_scene->addItem(m_shapeHandles);
    }
    // Scene units for a metre, from above.
    const double perMetre = QLineF(planToScene(QPointF(0, 0)), planToScene(QPointF(1, 0))).length();
    const double knobGap = perMetre > 0.0 ? (kKnobGapPx / scale) / perMetre : 0.5;
    QVector<QPair<StageShape::Handle, QPointF> > pts;
    for (StageShape::Handle h : s.handles())
        pts << qMakePair(h, planToScene(s.handlePoint(h, knobGap)));
    m_shapeHandles->setShape(s, planToScene(s.outline()), pts,
                             planToScene(s.handlePoint(StageShape::SideMinusV)), kHandlePx / scale);
    m_shapeHandles->setPreview(QPolygonF(), QString(), QPointF());
    m_shapeHandles->show();
}

double MonitorGraphicsView::shapeSnapStep(Qt::KeyboardModifiers mods) const
{
    if ((mods & Qt::AltModifier) || m_snapDivisions <= 0 || m_unitValue <= 0)
        return 0.0;
    return (double(m_unitValue) / double(m_snapDivisions)) / 1000.0;
}

double MonitorGraphicsView::shapeTurnTarget(double turned, bool free) const
{
    const StageShape &s = m_shapeDragStart;
    if (free)
        return s.angle + turned;
    // One feature squares up to where it faces; a selection by the turn.
    if (s.kind == QLatin1String("selection"))
        return s.angle + qRound(turned / 15.0) * 15.0;
    return qRound((s.angle + turned) / 15.0) * 15.0;
}

QString MonitorGraphicsView::shapeReadout(const StageShape &s, bool turning) const
{
    if (turning)
    {
        const double by = StageShape::normalised(s.angle - m_shapeDragStart.angle);
        if (s.kind == QLatin1String("selection"))
            return tr("Turn %1°").arg(by > 0 ? QStringLiteral("+%1").arg(by, 0, 'f', 0)
                                             : QString::number(by, 'f', 0));
        return tr("%1° (%2%3°)").arg(StageShape::normalised(s.angle), 0, 'f', 0)
                                .arg(by >= 0 ? QStringLiteral("+") : QString())
                                .arg(by, 0, 'f', 0);
    }
    // A line kept straight reads as a stretch; turned, it adds the way it faces.
    if (m_shapeSwinging && (m_shapeCaught
                            || qAbs(StageShape::normalised(s.angle - m_shapeDragStart.angle)) >= 0.05))
        return tr("Length %1 · %2°%3").arg(MeasurementUnits::formatMetres(s.length))
                                      .arg(StageShape::normalised(s.angle), 0, 'f', 0)
                                      .arg(m_shapeCaught ? tr(" → end") : QString());
    if (s.kind == QLatin1String("platform"))
        return tr("%1 × %2").arg(MeasurementUnits::formatMetres(s.length),
                                 MeasurementUnits::formatMetres(s.width));
    return tr("Length %1").arg(MeasurementUnits::formatMetres(s.length));
}

bool MonitorGraphicsView::shapePress(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return false;
    ShapeHandles *h = shapeHandles();
    if (h == nullptr)
        return false;
    const QPointF sp = mapToScene(event->pos());
    const StageShape::Handle hit = h->handleAt(sp);
    if (hit == StageShape::NoHandle)
        return false;
    // Fresh from the model: the handles are shown only while it is free.
    const StageShape s = shapeTarget();
    if (!s.isValid())
        return false;
    m_shapeDragHandle = hit;
    m_shapeDragStart = s;
    m_shapeDragNow = s;
    m_shapeDragMoved = false;
    m_shapeSwinging = false;
    m_shapeCaught = false;
    m_shapeOtherEnds = s.canSwing ? shapeSnapEnds(s) : QVector<QPointF>();
    m_shapeTurned = 0.0;
    m_shapeLastAngle = s.angleTo(sceneToPlan(sp));
    event->accept();
    return true;
}

QVector<QPointF> MonitorGraphicsView::shapeSnapEnds(const StageShape &except) const
{
    QVector<QPointF> ends;
    MonitorProperties *props = m_doc != nullptr ? m_doc->monitorProperties() : nullptr;
    if (props == nullptr)
        return ends;
    auto addLine = [&](const QString &kind, quint32 id) {
        if (kind == except.kind && id == except.id)
            return;
        const StageShape o = StageShape::of(props, kind, id);
        if (!o.isValid())
            return;
        ends << o.endPoint(StageShape::EndPlusU) << o.endPoint(StageShape::EndMinusU);
    };
    foreach (Truss *t, props->trusses())
        if (t != nullptr)
            addLine(QStringLiteral("truss"), t->id());
    foreach (Pipe *b, props->pipes())
        if (b != nullptr)
            addLine(QStringLiteral("pipe"), b->id());
    foreach (Scenery *sc, props->sceneries())
        if (sc != nullptr)
            addLine(QStringLiteral("scenery"), sc->id());
    foreach (StagePlatform *pl, props->platforms())
    {
        if (pl == nullptr)
            continue;
        const StageShape o = StageShape::of(props, QStringLiteral("platform"), pl->id());
        if (o.isValid())
            ends << o.outline();
    }
    return ends;
}

void MonitorGraphicsView::shapeDragTo(const QPointF &scenePos, Qt::KeyboardModifiers mods)
{
    if (m_shapeDragHandle == StageShape::NoHandle || m_shapeHandles == nullptr)
        return;
    const QPointF p = sceneToPlan(scenePos);
    const bool turning = m_shapeDragHandle == StageShape::RotateKnob;
    if (turning)
    {
        const double a = m_shapeDragStart.angleTo(p);
        m_shapeTurned += StageShape::normalised(a - m_shapeLastAngle);
        m_shapeLastAngle = a;
        m_shapeDragNow = m_shapeDragStart.turnedTo(shapeTurnTarget(m_shapeTurned, mods & Qt::AltModifier));
    }
    else if (m_shapeDragStart.canSwing
             && (m_shapeDragHandle == StageShape::EndPlusU || m_shapeDragHandle == StageShape::EndMinusU))
    {
        /* A line's end goes where it is put and the far end stays, so it
           lengthens and turns at once. It catches another end near it,
           keeps straight within a few degrees of the way it faced, Shift
           keeps it to 15°, else it lands on the Snap grid; ⌥ lets it go
           free of all of them. */
        m_shapeSwinging = true;
        if (mods & Qt::AltModifier)
        {
            m_shapeCaught = false;
            m_shapeDragNow = m_shapeDragStart.swung(m_shapeDragHandle, p);
        }
        else
        {
            const double scale = viewScale() > 0.0 ? viewScale() : 1.0;
            const double perMetre = QLineF(planToScene(QPointF(0, 0)), planToScene(QPointF(1, 0))).length();
            const double magnet = perMetre > 0.0 ? (10.0 / scale) / perMetre : 0.0;
            m_shapeDragNow = m_shapeDragStart.swungSnapped(m_shapeDragHandle, p, m_shapeOtherEnds, magnet,
                                                           shapeSnapStep(Qt::NoModifier),
                                                           mods & Qt::ShiftModifier, 3.0, &m_shapeCaught);
        }
    }
    else
    {
        m_shapeSwinging = false;
        m_shapeCaught = false;
        m_shapeDragNow = m_shapeDragStart.stretched(m_shapeDragHandle, p, shapeSnapStep(mods));
    }
    m_shapeDragMoved = true;
    // The readout sits off the pointer, so the pointer never hides it.
    const double scale = viewScale() > 0.0 ? viewScale() : 1.0;
    m_shapeHandles->setPreview(planToScene(m_shapeDragNow.outline()),
                               shapeReadout(m_shapeDragNow, turning),
                               scenePos + QPointF(14.0 / scale, 14.0 / scale));
}

void MonitorGraphicsView::shapeDragEnd(Qt::KeyboardModifiers)
{
    const StageShape start = m_shapeDragStart;
    const StageShape now = m_shapeDragNow;
    const bool turning = m_shapeDragHandle == StageShape::RotateKnob;
    const bool moved = m_shapeDragMoved;
    m_shapeDragHandle = StageShape::NoHandle;
    m_shapeDragMoved = false;
    m_shapeSwinging = false;
    m_shapeCaught = false;
    m_shapeOtherEnds.clear();
    auto refresh = qScopeGuard([this]() { refreshShapeHandles(); });
    if (!moved || !start.isValid())
        return;
    MonitorProperties *props = m_doc->monitorProperties();

    if (turning)
    {
        const double by = StageShape::normalised(now.angle - start.angle);
        if (qAbs(by) < 0.05)
            return;
        // Everything selected turns about the knob's middle: one undo step.
        const QPointF about = start.centre;
        rotateSelectedItems(by, &about);
        return;
    }

    // Let go where it started (to the millimetre): nothing to change.
    if (qAbs(now.length - start.length) < 5e-4 && qAbs(now.width - start.width) < 5e-4
        && QLineF(now.centre, start.centre).length() < 5e-4
        && qAbs(StageShape::normalised(now.angle - start.angle)) < 0.01)
        return;
    const QList<QPair<QString, quint32> > selected = selectedMapObjects();
    const QByteArray before = m_rigSnapshotter ? m_rigSnapshotter() : QByteArray();
    if (!now.apply(props))
        return;
    props->recomputeAnchoredFrames();
    props->recomputeChildTrusses();
    props->recomputePipeAnchors();
    props->recomputeHangers();
    if (!before.isEmpty())
        pushRigUndo(before);
    if (now.kind == QLatin1String("truss"))
    {
        updateTrusses();
        foreach (quint32 fid, props->fixtureItemsID())
        {
            if (props->fixtureRigProps(fid).trussId == now.id)
                updateFixture(fid);
        }
    }
    else
    {
        updatePlatforms();
        refreshRiserFixtures();
        if (now.kind == QLatin1String("pipe"))
        {
            // Its lights, and those on the bars it carries, ride it.
            foreach (quint32 fid, props->fixtureItemsID())
                if (props->hasFixtureRigProps(fid) && props->fixtureRigProps(fid).onPipe())
                    updateFixture(fid);
        }
    }
    keepMapSelection(selected);
    m_doc->setModified();
    emit mapStructureChanged();
}

/*********************************************************************
 * Group ▸
 *********************************************************************/

quint32 MonitorGraphicsView::filedGroupOf(const QString &kind, quint32 id) const
{
    return selectableGroupOf(kind, id);
}

quint32 MonitorGraphicsView::objectsLayer(const QList<QPair<QString, quint32> > &objects) const
{
    QHash<quint32, int> votes;
    for (const auto &o : objects)
    {
        bool found = false;
        const quint32 lid = objectLayerId(o.first, o.second, &found);
        if (found)
            votes[lid]++;
    }
    quint32 layer = m_doc->monitorProperties()->activeLayerId();
    int best = -1;
    for (auto it = votes.constBegin(); it != votes.constEnd(); ++it)
        if (it.value() > best || (it.value() == best && it.key() < layer))
        {
            best = it.value();
            layer = it.key();
        }
    return layer;
}

/* The locked one in @p gid's chain, itself or a folder above it; 0 if none. */
static quint32 lockedInChain(MonitorProperties *props, quint32 gid)
{
    for (int guard = 0; gid != 0 && props->hasGroup(gid) && guard < 64; ++guard)
    {
        if (props->group(gid).locked)
            return gid;
        gid = props->group(gid).parentGroupId;
    }
    return 0;
}

QString MonitorGraphicsView::groupingBlocked(const QList<QPair<QString, quint32> > &objects) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (m_layoutLocked)
        return tr("The plot is locked: Edit Plot to change its groups");
    for (const auto &o : objects)
    {
        bool found = false;
        const quint32 lid = objectLayerId(o.first, o.second, &found);
        if (found && props->layer(lid).locked)
            return tr("Layer “%1” is locked: unlock it in Plot layers").arg(props->layer(lid).name);
        if (const quint32 g = lockedInChain(props, filedGroupOf(o.first, o.second)))
            return tr("Group “%1” is locked: unlock it in Plot layers").arg(props->group(g).name);
    }
    return QString();
}

QString MonitorGraphicsView::groupTargetBlocked(quint32 gid) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (m_layoutLocked)
        return tr("The plot is locked: Edit Plot to change its groups");
    if (!props->hasGroup(gid))
        return QString();
    const quint32 lid = props->group(gid).layerId;
    if (props->layer(lid).locked)
        return tr("Layer “%1” is locked: unlock it in Plot layers").arg(props->layer(lid).name);
    if (const quint32 g = lockedInChain(props, gid))
        return tr("Group “%1” is locked: unlock it in Plot layers").arg(props->group(g).name);
    return QString();
}

QList<quint32> MonitorGraphicsView::groupsToAddTo(const QList<QPair<QString, quint32> > &objects) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const quint32 layer = objectsLayer(objects);
    QList<MonitorProperties::MonitorGroup> found;
    foreach (const MonitorProperties::MonitorGroup &g, props->groups())
    {
        /* A plain group the user made: not a structure's own (it stands for
           the truss), not a studio frame (its members are placed in it), not
           a built assembly or anything inside one. */
        if (g.layerId != layer || !g.anchorKind.isEmpty() || g.hasFrame || g.assembly
            || props->assemblyOf(g.id) != 0)
            continue;
        found << g;
    }
    std::sort(found.begin(), found.end(), [](const MonitorProperties::MonitorGroup &a,
                                              const MonitorProperties::MonitorGroup &b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
    QList<quint32> out;
    for (const auto &g : found)
        out << g.id;
    return out;
}

int MonitorGraphicsView::fileInGroup(const QList<QPair<QString, quint32> > &objects, quint32 gid)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (!props->hasGroup(gid))
        return 0;
    const quint32 layer = props->group(gid).layerId;
    QSet<QGraphicsItem *> chosen;
    for (const auto &o : objects)
        if (QGraphicsItem *gi = itemFor(o.first, o.second))
            chosen.insert(gi);

    QList<quint32> nest;                // groups that go in whole
    QList<QGraphicsItem *> loose;
    for (const auto &o : objects)
    {
        QGraphicsItem *gi = itemFor(o.first, o.second);
        if (gi == nullptr)
            continue;
        const quint32 g = itemGroupId(gi);
        if (g != 0 && props->hasGroup(g))
        {
            const MonitorProperties::MonitorGroup grp = props->group(g);
            // A structure goes with what is on it: its own group nests.
            if (!grp.anchorKind.isEmpty() && isGroupAnchorItem(gi, grp.anchorKind, grp.anchorId))
            {
                if (!nest.contains(g))
                    nest << g;
                continue;
            }
            // An assembly is one thing.
            if (const quint32 a = props->assemblyOf(g))
            {
                if (!nest.contains(a))
                    nest << a;
                continue;
            }
            // A part of a structure (a light on its truss) stays on it.
            if (!grp.anchorKind.isEmpty())
                continue;
            // A folder chosen whole nests whole.
            const quint32 top = topLevelGroup(g);
            if (top != gid && !groupIsUnder(gid, top))
            {
                bool whole = true;
                foreach (QGraphicsItem *other, itemsUnderGroup(top))
                    if (!chosen.contains(other))
                    {
                        whole = false;
                        break;
                    }
                if (whole)
                {
                    if (!nest.contains(top))
                        nest << top;
                    continue;
                }
            }
        }
        loose << gi;
    }

    int n = 0;
    for (quint32 g : nest)
    {
        if (g == gid || groupIsUnder(gid, g) || props->group(g).parentGroupId == gid)
            continue;
        props->setGroupParent(g, gid);
        setGroupSubtreeLayer(g, layer);
        ++n;
    }
    for (QGraphicsItem *gi : loose)
    {
        if (itemGroupId(gi) == gid)
            continue;
        setItemGroupId(gi, gid);
        setItemLayer(gi, layer);
        ++n;
    }
    return n;
}

quint32 MonitorGraphicsView::newGroupFrom(const QList<QPair<QString, quint32> > &objects,
                                          const QString &name)
{
    MonitorProperties *props = m_doc->monitorProperties();
    const QString nm = name.trimmed();
    if (objects.isEmpty() || nm.isEmpty() || !groupingBlocked(objects).isEmpty())
        return 0;
    /* Inside the group they already share, if they share one; otherwise a
       group of its own on their layer. */
    quint32 parent = filedGroupOf(objects.first().first, objects.first().second);
    for (const auto &o : objects)
        if (filedGroupOf(o.first, o.second) != parent)
        {
            parent = 0;
            break;
        }
    if (parent != 0 && !groupTargetBlocked(parent).isEmpty())
        parent = 0;
    const quint32 layer = parent != 0 ? props->group(parent).layerId : objectsLayer(objects);
    const QByteArray before = m_rigSnapshotter ? m_rigSnapshotter() : QByteArray();
    const quint32 gid = props->nextGroupId();
    props->createGroup(gid, nm, layer, parent);
    if (fileInGroup(objects, gid) == 0)
    {
        props->removeGroup(gid);
        return 0;
    }
    if (!before.isEmpty())
        pushRigUndo(before);
    m_doc->setModified();
    refreshItemLayerState();
    emit mapSelectionChanged();
    emit mapStructureChanged();
    return gid;
}

quint32 MonitorGraphicsView::newGroupNow(const QList<QPair<QString, quint32> > &objects)
{
    if (objects.isEmpty())
        return 0;
    const QString blocked = groupingBlocked(objects);
    if (!blocked.isEmpty())
    {
        emit dragRefused(blocked);
        return 0;
    }
    /* No dialog: the group is made at once as "Group N", and the Plot layers
       list (when it is open) puts the name in place to type over. */
    MonitorProperties *props = m_doc->monitorProperties();
    const quint32 gid = newGroupFrom(objects, tr("Group %1").arg(props->nextGroupId()));
    if (gid != 0)
        emit groupCreated(gid);
    return gid;
}

int MonitorGraphicsView::addToGroup(const QList<QPair<QString, quint32> > &objects, quint32 gid)
{
    if (objects.isEmpty() || !groupingBlocked(objects).isEmpty() || !groupTargetBlocked(gid).isEmpty())
        return 0;
    const QByteArray before = m_rigSnapshotter ? m_rigSnapshotter() : QByteArray();
    const int n = fileInGroup(objects, gid);
    if (n == 0)
        return 0;
    if (!before.isEmpty())
        pushRigUndo(before);
    m_doc->setModified();
    refreshItemLayerState();
    emit mapSelectionChanged();
    emit mapStructureChanged();
    return n;
}

int MonitorGraphicsView::removeFromGroup(const QList<QPair<QString, quint32> > &objects)
{
    MonitorProperties *props = m_doc->monitorProperties();
    if (objects.isEmpty() || !groupingBlocked(objects).isEmpty())
        return 0;
    const QByteArray before = m_rigSnapshotter ? m_rigSnapshotter() : QByteArray();
    int n = 0;
    QList<QPair<QString, quint32> > offStructure;
    for (const auto &o : objects)
    {
        const quint32 filed = filedGroupOf(o.first, o.second);
        QGraphicsItem *gi = itemFor(o.first, o.second);
        if (filed == 0 || gi == nullptr)
            continue;
        // One level out, on the same layer.
        const quint32 out = props->group(filed).parentGroupId;
        const quint32 own = itemGroupId(gi);
        const MonitorProperties::MonitorGroup grp = props->group(own);
        if (own != 0 && !grp.anchorKind.isEmpty() && isGroupAnchorItem(gi, grp.anchorKind, grp.anchorId))
            props->setGroupParent(own, out);            // a structure with what is on it
        else if (const quint32 a = props->assemblyOf(own))
            props->setGroupParent(a, out);
        else if (o.first == QLatin1String("fixture") && !grp.anchorKind.isEmpty())
        {
            offStructure << o;                          // off its truss or deck, as before
            continue;
        }
        else
            setItemGroupId(gi, out);
        ++n;
    }
    if (!offStructure.isEmpty())
    {
        removeFromFolder(offStructure);
        n += offStructure.size();
    }
    if (n == 0)
        return 0;
    if (!before.isEmpty())
        pushRigUndo(before);
    m_doc->setModified();
    refreshItemLayerState();
    emit mapSelectionChanged();
    emit mapStructureChanged();
    return n;
}

void MonitorGraphicsView::fillGroupMenu(QMenu *menu, const QList<QPair<QString, quint32> > &objects)
{
    if (menu == nullptr)
        return;
    MonitorProperties *props = m_doc->monitorProperties();
    menu->setToolTipsVisible(true);
    if (menu->objectName().isEmpty())
        menu->setObjectName(QStringLiteral("plotGroupMenu"));
    const QString blocked = objects.isEmpty() ? tr("Select what to group first")
                                              : groupingBlocked(objects);

    QAction *made = menu->addAction(QIcon(":/group.png"), tr("New Group"), this, [this, objects]() {
        newGroupNow(objects);
    });
    made->setObjectName(QStringLiteral("plotNewGroup"));
    made->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    FocusMenu::setAvailable(made, blocked,
                            tr("Put these in a group of their own: they select and move together"));

    const QList<quint32> groups = objects.isEmpty() ? QList<quint32>() : groupsToAddTo(objects);
    menu->addSeparator();
    if (groups.isEmpty())
    {
        QAction *none = menu->addAction(tr("No Groups on This Layer Yet"));
        none->setObjectName(QStringLiteral("plotNoGroups"));
        none->setEnabled(false);
    }
    for (quint32 gid : groups)
    {
        // Its place in Plot layers: "Band / Risers".
        QString label = props->group(gid).name;
        for (quint32 p = props->group(gid).parentGroupId, guard = 0; p != 0 && props->hasGroup(p) && guard < 64;
             p = props->group(p).parentGroupId, ++guard)
            label = props->group(p).name + QStringLiteral(" / ") + label;
        QAction *a = menu->addAction(label, this, [this, objects, gid]() { addToGroup(objects, gid); });
        a->setObjectName(QStringLiteral("plotAddToGroup"));
        a->setData(gid);
        bool allIn = true;
        for (const auto &o : objects)
            if (filedGroupOf(o.first, o.second) != gid)
            {
                allIn = false;
                break;
            }
        a->setCheckable(true);
        a->setChecked(allIn);
        const QString why = !blocked.isEmpty() ? blocked
                          : allIn ? tr("Already in “%1”").arg(props->group(gid).name)
                                  : groupTargetBlocked(gid);
        FocusMenu::setAvailable(a, why, tr("Add these to “%1”").arg(props->group(gid).name));
    }

    QList<QPair<QString, quint32> > inGroup;
    for (const auto &o : objects)
        if (filedGroupOf(o.first, o.second) != 0)
            inGroup << o;
    if (!inGroup.isEmpty())
    {
        menu->addSeparator();
        QAction *rem = menu->addAction(inGroup.size() > 1 ? tr("Remove %1 from Their Groups").arg(inGroup.size())
                                                          : tr("Remove from Group"),
                                       this, [this, inGroup]() { removeFromGroup(inGroup); });
        rem->setObjectName(QStringLiteral("plotRemoveFromGroup"));
        FocusMenu::setAvailable(rem, groupingBlocked(inGroup),
                                tr("Take them out of the group they are in; they stay where they are"));
    }
}
