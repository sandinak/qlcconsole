/*
  Q Light Controller Plus
  shapehandles.cpp

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

#include <QGraphicsSimpleTextItem>
#include <QPainter>
#include <QLineF>
#include <QtMath>

#include "shapehandles.h"
#include "monitorproperties.h"
#include "stageplatform.h"
#include "scenery.h"
#include "hanger.h"
#include "truss.h"

/*********************************************************************
 * StageShape
 *********************************************************************/

QPointF StageShape::u() const
{
    const double r = qDegreesToRadians(angle);
    return QPointF(qCos(r), qSin(r));
}

QPointF StageShape::v() const
{
    const double r = qDegreesToRadians(angle);
    return QPointF(-qSin(r), qCos(r));
}

QPointF StageShape::toWorld(double lu, double lv) const
{
    return centre + u() * lu + v() * lv;
}

QPolygonF StageShape::outline() const
{
    const double hl = length / 2.0, hw = width / 2.0;
    QPolygonF p;
    p << toWorld(-hl, -hw) << toWorld(hl, -hw) << toWorld(hl, hw) << toWorld(-hl, hw);
    return p;
}

QPointF StageShape::handlePoint(Handle h, double knobGap) const
{
    const double hl = length / 2.0, hw = width / 2.0;
    switch (h)
    {
        case EndPlusU:   return toWorld(hl, 0.0);
        case EndMinusU:  return toWorld(-hl, 0.0);
        case SidePlusV:  return toWorld(0.0, hw);
        case SideMinusV: return toWorld(0.0, -hw);
        case CornerPP:   return toWorld(hl, hw);
        case CornerPM:   return toWorld(hl, -hw);
        case CornerMP:   return toWorld(-hl, hw);
        case CornerMM:   return toWorld(-hl, -hw);
        case RotateKnob: return toWorld(0.0, -hw - knobGap);
        case NoHandle:
        default:         return centre;
    }
}

QVector<StageShape::Handle> StageShape::handles() const
{
    QVector<Handle> out;
    if (stretchU)
        out << EndPlusU << EndMinusU;
    if (stretchV)
        out << SidePlusV << SideMinusV;
    if (stretchU && stretchV)
        out << CornerPP << CornerPM << CornerMP << CornerMM;
    if (canRotate)
        out << RotateKnob;
    return out;
}

static double snapped(double value, double step)
{
    return step > 0.0 ? qRound(value / step) * step : value;
}

StageShape StageShape::stretched(Handle h, const QPointF &pointer, double snapStep) const
{
    StageShape s = *this;
    const QPointF d = pointer - centre;
    const QPointF uu = u(), vv = v();
    const double lu = d.x() * uu.x() + d.y() * uu.y();
    const double lv = d.x() * vv.x() + d.y() * vv.y();
    const double hl = length / 2.0, hw = width / 2.0;
    double cu = 0.0, cv = 0.0;          // the new middle, in the old frame

    const bool plusU  = (h == EndPlusU || h == CornerPP || h == CornerPM);
    const bool minusU = (h == EndMinusU || h == CornerMP || h == CornerMM);
    const bool plusV  = (h == SidePlusV || h == CornerPP || h == CornerMP);
    const bool minusV = (h == SideMinusV || h == CornerPM || h == CornerMM);

    if (stretchU && (plusU || minusU))
    {
        double l = plusU ? lu + hl : hl - lu;
        l = qMax(minLength, snapped(l, snapStep));
        s.length = l;
        cu = plusU ? -hl + l / 2.0 : hl - l / 2.0;
    }
    if (stretchV && (plusV || minusV))
    {
        double w = plusV ? lv + hw : hw - lv;
        w = qMax(minLength, snapped(w, snapStep));
        s.width = w;
        cv = plusV ? -hw + w / 2.0 : hw - w / 2.0;
    }
    s.centre = toWorld(cu, cv);
    return s;
}

StageShape StageShape::turnedTo(double angleDeg) const
{
    StageShape s = *this;
    s.angle = normalised(angleDeg);
    return s;
}

double StageShape::angleTo(const QPointF &pointer) const
{
    const QPointF d = pointer - centre;
    return qRadiansToDegrees(qAtan2(d.y(), d.x()));
}

double StageShape::normalised(double deg)
{
    double a = std::fmod(deg, 360.0);
    if (a <= -180.0)
        a += 360.0;
    else if (a > 180.0)
        a -= 360.0;
    if (qAbs(a) < 1e-9)
        a = 0.0;
    return a;
}

StageShape StageShape::of(MonitorProperties *props, const QString &kind, quint32 id)
{
    StageShape s;
    if (props == nullptr)
        return s;
    if (kind == QLatin1String("platform"))
    {
        const StagePlatform *pl = props->platform(id);
        if (pl == nullptr || props->isApronPlatform(id))
            return s;
        s.kind = kind;
        s.id = id;
        s.centre = pl->centre();
        s.angle = double(pl->rotation());
        s.length = double(pl->width());
        s.width = double(pl->depth());
        /* A flight of steps is as deep as its treads add up to: only the
           width of a tread stretches. */
        if (pl->isSteps())
        {
            const bool across = pl->stepsClimb() == StagePlatform::ClimbStageLeft
                                || pl->stepsClimb() == StagePlatform::ClimbStageRight;
            s.stretchU = !across;
            s.stretchV = across;
        }
        else
        {
            s.stretchU = s.stretchV = true;
        }
        s.canRotate = true;
        return s;
    }
    if (kind == QLatin1String("truss"))
    {
        const Truss *t = props->truss(id);
        // A bar follows its truss, a tower stands up, one on a stand follows it.
        if (t == nullptr || t->type() == Truss::Vertical || t->isChildBar() || t->isStandMounted())
            return s;
        const QPointF dir = t->direction();
        const QPointF o(double(t->origin().x()), double(t->origin().y()));
        s.kind = kind;
        s.id = id;
        s.length = double(t->length());
        s.width = qMax(0.05, double(t->width()));
        s.centre = o + dir * (s.length / 2.0);
        s.angle = qRadiansToDegrees(qAtan2(dir.y(), dir.x()));
        s.stretchU = true;
        s.canRotate = true;
        return s;
    }
    if (kind == QLatin1String("scenery"))
    {
        const Scenery *sc = props->scenery(id);
        if (sc == nullptr)
            return s;
        s.kind = kind;
        s.id = id;
        s.centre = QPointF(double(sc->originX()), double(sc->originY()));
        s.angle = double(sc->rotation());
        s.length = double(sc->width());
        // A sheet is a line on the plan; a U reaches out front by its depth.
        s.width = sc->outline() == Scenery::RoundedU ? 2.0 * double(sc->depth())
                                                     : qMax(0.1, double(sc->thickness()));
        s.stretchU = true;
        // A cyc hangs square across the back of the stage: it only stretches.
        s.canRotate = sc->kind() != Scenery::Cyclorama;
        return s;
    }
    return s;
}

bool StageShape::apply(MonitorProperties *props) const
{
    if (props == nullptr || !isValid())
        return false;
    if (kind == QLatin1String("platform"))
    {
        StagePlatform *pl = props->platform(id);
        if (pl == nullptr)
            return false;
        pl->setWidth(float(length));
        pl->setDepth(float(width));
        pl->setOriginX(float(centre.x() - length / 2.0));
        pl->setOriginY(float(centre.y() - width / 2.0));
        pl->setRotation(float(angle));
        return true;
    }
    if (kind == QLatin1String("truss"))
    {
        Truss *t = props->truss(id);
        if (t == nullptr)
            return false;
        const QPointF oldO(double(t->origin().x()), double(t->origin().y()));
        const QPointF oldDir = t->direction();
        const QPointF dir = u();
        const QPointF o = centre - dir * (length / 2.0);
        /* What hangs on it is placed by how far along it is: when the near
           end moves, each keeps its place on the stage by the same shift. */
        const QPointF moved = o - oldO;
        const float shift = float(moved.x() * oldDir.x() + moved.y() * oldDir.y());
        t->setOrigin(QVector3D(float(o.x()), float(o.y()), t->origin().z()));
        t->setDirection(dir);
        t->setLength(float(length));
        if (!qFuzzyIsNull(shift))
        {
            foreach (quint32 fid, props->fixtureItemsID())
            {
                if (!props->hasFixtureRigProps(fid))
                    continue;
                FixtureRigProps rp = props->fixtureRigProps(fid);
                if (rp.trussId != id)
                    continue;
                rp.trussOffset = qBound(0.0f, rp.trussOffset - shift, float(length));
                props->setFixtureRigProps(fid, rp);
            }
            foreach (Truss *bar, props->trusses())
                if (bar != nullptr && bar->parentTrussId() == id)
                    bar->setParentOffset(qBound(0.0f, bar->parentOffset() - shift, float(length)));
            foreach (Hanger *h, props->hangers())
                if (h != nullptr && h->parentKind() == Hanger::OnTruss && h->parentId() == id)
                    h->setAlong(qBound(0.0f, h->along() - shift, float(length)));
        }
        return true;
    }
    if (kind == QLatin1String("scenery"))
    {
        Scenery *sc = props->scenery(id);
        if (sc == nullptr)
            return false;
        sc->setOriginX(float(centre.x()));
        sc->setOriginY(float(centre.y()));
        sc->setWidth(float(length));
        sc->setRotation(float(angle));
        return true;
    }
    return false;
}

/*********************************************************************
 * ShapeHandles
 *********************************************************************/

ShapeHandles::ShapeHandles()
{
    // Over every structure; clicks are the plot's to work out.
    setZValue(9400);
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);
    m_label = new QGraphicsSimpleTextItem(this);
    // The readout reads the same however the plot is zoomed or turned.
    m_label->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    QFont f = m_label->font();
    f.setBold(true);
    m_label->setFont(f);
    m_label->setBrush(QColor(255, 255, 255));
    m_label->setPen(QPen(QColor(20, 20, 20), 0.6));
    m_label->setZValue(1);
    m_label->hide();
}

void ShapeHandles::setShape(const StageShape &shape, const QPolygonF &outline,
                            const QVector<QPair<StageShape::Handle, QPointF> > &points,
                            const QPointF &pivot, qreal radius)
{
    prepareGeometryChange();
    m_shape = shape;
    m_outline = outline;
    m_points = points;
    m_pivot = pivot;
    m_radius = qMax(0.5, radius);
    recomputeBounds();
    update();
}

void ShapeHandles::recomputeBounds()
{
    QRectF r = m_outline.boundingRect() | m_ghost.boundingRect();
    for (const auto &p : std::as_const(m_points))
        r |= QRectF(p.second, QSizeF(0, 0));
    m_bounds = r.adjusted(-m_radius * 3, -m_radius * 3, m_radius * 3, m_radius * 3);
}

StageShape::Handle ShapeHandles::handleAt(const QPointF &scenePos) const
{
    StageShape::Handle best = StageShape::NoHandle;
    qreal bestD = m_radius * 1.8;
    for (const auto &p : m_points)
    {
        const qreal d = QLineF(p.second, scenePos).length();
        if (d <= bestD)
        {
            bestD = d;
            best = p.first;
        }
    }
    return best;
}

QPointF ShapeHandles::handlePos(StageShape::Handle h) const
{
    for (const auto &p : m_points)
        if (p.first == h)
            return p.second;
    return QPointF();
}

void ShapeHandles::setPreview(const QPolygonF &ghost, const QString &readout, const QPointF &at)
{
    prepareGeometryChange();
    m_ghost = ghost;
    recomputeBounds();
    if (ghost.isEmpty() || readout.isEmpty())
    {
        m_label->hide();
    }
    else
    {
        m_label->setText(readout);
        m_label->setPos(at);
        m_label->show();
    }
    update();
}

QString ShapeHandles::readout() const
{
    return m_label->isVisible() ? m_label->text() : QString();
}

QRectF ShapeHandles::boundingRect() const
{
    return m_bounds;
}

void ShapeHandles::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (m_points.isEmpty())
        return;
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QColor accent(0, 190, 255);
    const qreal w = m_radius / 3.5;

    // Its footprint, and while dragging, the one it will take.
    QPen edge(accent, w * 0.7);
    painter->setPen(edge);
    painter->setBrush(Qt::NoBrush);
    painter->drawPolygon(m_outline);
    if (!m_ghost.isEmpty())
    {
        QPen dash(QColor(255, 210, 60), w);
        dash.setStyle(Qt::DashLine);
        painter->setPen(dash);
        painter->setBrush(QColor(255, 210, 60, 40));
        painter->drawPolygon(m_ghost);
    }

    // The knob's stem from the edge it stands off.
    const QPointF knob = handlePos(StageShape::RotateKnob);
    if (!knob.isNull())
    {
        painter->setPen(QPen(accent, w * 0.6));
        painter->drawLine(m_pivot, knob);
    }

    for (const auto &p : std::as_const(m_points))
    {
        painter->setPen(QPen(QColor(20, 20, 20), w));
        if (p.first == StageShape::RotateKnob)
        {
            painter->setBrush(accent);
            painter->drawEllipse(p.second, m_radius * 1.1, m_radius * 1.1);
            // A turning arrow on it, so it reads as "turn" and not "stretch".
            painter->setPen(QPen(QColor(20, 20, 20), w * 0.8));
            painter->setBrush(Qt::NoBrush);
            const QRectF arc(p.second.x() - m_radius * 0.6, p.second.y() - m_radius * 0.6,
                             m_radius * 1.2, m_radius * 1.2);
            painter->drawArc(arc, 30 * 16, 280 * 16);
        }
        else
        {
            painter->setBrush(QColor(255, 255, 255));
            const qreal s = (p.first >= StageShape::CornerPP) ? m_radius * 0.8 : m_radius;
            painter->drawRect(QRectF(p.second.x() - s, p.second.y() - s, 2 * s, 2 * s));
        }
    }
}
