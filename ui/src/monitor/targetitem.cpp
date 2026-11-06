/*
  Q Light Controller Plus
  targetitem.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include <QPen>
#include <QFont>
#include <QMenu>

#include <QMessageBox>
#include <QGraphicsScene>
#include <cmath>
#include "monitorgraphicsview.h"
#include "qlcpalette.h"
#include "scene.h"
#include "monitorproperties.h"
#include "targetitem.h"
#include "itemlock.h"
#include "stagemarklabel.h"
#include "stagetarget.h"
#include "humanfigure.h"
#include "doc.h"

TargetItem::TargetItem(StageTarget *target, Doc *doc, float pxX, float pxY,
                       QGraphicsItem *parent)
    : QObject(nullptr)
    , QGraphicsItem(parent)
    , m_target(target)
    , m_doc(doc)
    , m_label(nullptr)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(3.0);   // above fixtures so it's always clickable
    // Respect a persisted per-target lock: a locked target can't be dragged.
    setFlag(ItemIsMovable, !target->locked());
    setCursor(target->locked() ? Qt::ArrowCursor : Qt::PointingHandCursor);
    setPos(pxX, pxY);

    QString labelText = target->name();
    if (target->z() > 0.0f)
        labelText += QString("\n(Z: %1)").arg(MonitorProperties::formatStageDistance(target->z()));

    m_label = new QGraphicsTextItem(labelText, this);
    QColor textColor = target->color().isValid() ? target->color().lighter(160)
                                                 : QColor(255, 220, 80);
    m_label->setDefaultTextColor(textColor);
    m_label->setFont(QFont("Arial", 7));
    m_label->setZValue(0.1);
    m_label->setPos(kRadius + 3, -kRadius);
}

quint32 TargetItem::targetId() const
{
    return m_target->id();
}

void TargetItem::setScenePos(float pxX, float pxY)
{
    setPos(pxX, pxY);
}

void TargetItem::setPositionReadout(std::function<QString(const QPointF &)> text)
{
    m_readoutText = text;
    if (!m_readoutText)
    {
        delete m_readout;
        m_readout = nullptr;
        return;
    }
    if (m_readout == nullptr)
    {
        m_readout = new StageMarkLabel(QString(), QColor(225, 230, 240),
                                       Qt::AlignHCenter | Qt::AlignTop, this);
        m_readout->setZValue(0.2);
        m_readout->setPos(0.0, kRadius);
    }
    m_readout->setText(m_readoutText(scenePos()));
    refreshReadoutVisibility();
}

void TargetItem::refreshReadoutVisibility()
{
    /* A line target's readout says where its centre is, which is not where
       the line is; next to a person or a mark it printed on top of theirs
       ("SR 1\u2033" was Line 1's, an inch from the Announcer). Shown when
       the line is the one selected (Q179). */
    if (m_readout != nullptr)
        m_readout->setVisible(!isLine() || isSelected());
}

void TargetItem::setReadoutStackOffset(qreal dy)
{
    if (m_readout != nullptr)
        m_readout->setStackOffset(dy);
}

qreal TargetItem::readoutStackOffset() const
{
    return m_readout != nullptr ? m_readout->stackOffset() : 0.0;
}

QRectF TargetItem::readoutScreenRect() const
{
    // The label ignores the view's transform: its rect is in screen pixels.
    return m_readout != nullptr ? m_readout->baseRect() : QRectF();
}

bool TargetItem::readoutShown() const
{
    return m_readout != nullptr && m_readout->isVisible() && !m_readout->text().isEmpty();
}

QPointF TargetItem::readoutAnchorScene() const
{
    return m_readout != nullptr ? m_readout->scenePos() : scenePos();
}

void TargetItem::refreshPositionReadout()
{
    if (m_readout != nullptr && m_readoutText)
        m_readout->setText(m_readoutText(scenePos()));
}

QString TargetItem::positionReadout() const
{
    return m_readout != nullptr ? m_readout->text() : QString();
}

void TargetItem::setMovable(bool movable)
{
    setFlag(ItemIsMovable, movable);
    setCursor(movable ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void TargetItem::setFootprintPx(const QSizeF &size)
{
    prepareGeometryChange();
    m_footprintPx = size;
    update();
}

QRectF TargetItem::footprintRect() const
{
    if (m_footprintPx.width() <= 0.0)
        return QRectF();
    // An instrument is drawn as itself (bandRect()), not as a tinted box.
    if (m_target != nullptr && StageTarget::isBandKind(m_target->kind()))
        return QRectF();
    /* A strip in an elevation: the ground it covers, seen edge on, given a few
       pixels of thickness so it can be seen at all. */
    const double h = qMax(6.0, m_footprintPx.height());
    return QRectF(-m_footprintPx.width() * 0.5, -h * 0.5, m_footprintPx.width(), h);
}

/* The band instruments' glyphs. Floors keep them readable when the whole stage
   is on screen: a rack is never smaller than a small box. */
static constexpr double kBandMinW = 16.0;
static constexpr double kBandMinH = 12.0;

QRectF TargetItem::bandRect() const
{
    if (m_target == nullptr || !StageTarget::isBandKind(m_target->kind()))
        return QRectF();
    if (m_footprintPx.width() <= 0.0)
        return QRectF();
    const double w = qMax(kBandMinW, m_footprintPx.width());
    if (m_footprintPx.height() > 0.0)           // from above
    {
        const double h = qMax(kBandMinH, m_footprintPx.height());
        return QRectF(-w * 0.5, -h * 0.5, w, h);
    }
    if (m_pxPerMetre <= 0.0)
        return QRectF();
    // In an elevation: standing on the deck, up to the top of the case.
    const double h = qMax(kBandMinH * 0.7, std::abs(m_crown.y() - m_feet.y()));
    return QRectF(m_feet.x() - w * 0.5, m_feet.y() - h, w, h);
}

void TargetItem::setBandAngle(double deg)
{
    if (qFuzzyCompare(m_bandAngle + 1.0, deg + 1.0))
        return;
    prepareGeometryChange();
    m_bandAngle = deg;
    update();
}

QRectF TargetItem::bandBounds() const
{
    const QRectF r = bandRect();
    if (!r.isValid() || m_footprintPx.height() <= 0.0 || qFuzzyIsNull(m_bandAngle))
        return r;
    return QTransform().rotate(m_bandAngle).mapRect(r);
}

QPainterPath TargetItem::bandOutline() const
{
    QPainterPath path;
    if (m_footprintPx.height() > 0.0 && !qFuzzyIsNull(m_bandAngle))
    {
        // Drawn upright, then turned about the mark.
        TargetItem *self = const_cast<TargetItem *>(this);
        const double a = m_bandAngle;
        self->m_bandAngle = 0.0;
        const QPainterPath upright = bandOutline();
        self->m_bandAngle = a;
        return QTransform().rotate(a).map(upright);
    }
    const QRectF r = bandRect();
    if (r.isValid() == false)
        return path;
    const bool top = m_footprintPx.height() > 0.0;
    const double l = r.left(), t = r.top(), w = r.width(), h = r.height();
    auto X = [&](double f) { return l + w * f; };
    auto Y = [&](double f) { return t + h * f; };
    auto stripes = [&](QPainterPath &p, double x0, double x1, double y0, double y1) {
        const int n = qBound(4, int((x1 - x0) / 4.0), 40);
        for (int i = 1; i < n; ++i)
        {
            const double x = x0 + (x1 - x0) * i / n;
            p.moveTo(x, y0);
            p.lineTo(x, y1);
        }
    };
    auto units = [&](QPainterPath &p, double x0, double x1, double y0, double y1) {
        const int n = qBound(3, int((y1 - y0) / 4.0), 12);
        for (int i = 1; i < n; ++i)
        {
            const double y = y0 + (y1 - y0) * i / n;
            p.moveTo(x0, y);
            p.lineTo(x1, y);
        }
    };
    switch (m_target->kind())
    {
    case StageTarget::Kind::Keyboard:
        if (top)
        {
            // The keybed toward the house (down the plot), key stripes across it,
            // the stand's X showing through the rest.
            path.addRoundedRect(r, 2.0, 2.0);
            const double kb = Y(0.42);
            path.moveTo(l, kb);
            path.lineTo(l + w, kb);
            stripes(path, l, l + w, kb, t + h);
            path.moveTo(X(0.12), t); path.lineTo(X(0.88), kb);
            path.moveTo(X(0.88), t); path.lineTo(X(0.12), kb);
        }
        else
        {
            // A slab on an X-stand.
            const double slab = qMax(3.0, h * 0.09);
            path.addRect(QRectF(l, t, w, slab));
            if (!m_sideOn)
                stripes(path, l, l + w, t, t + slab);
            const double a = m_sideOn ? 0.30 : 0.18, b = 1.0 - a;
            path.moveTo(X(a), t + slab); path.lineTo(X(b), t + h);
            path.moveTo(X(b), t + slab); path.lineTo(X(a), t + h);
        }
        break;
    case StageTarget::Kind::GrandPiano:
        if (top)
        {
            // The grand's case: straight bass side, the curved bent side and
            // tail, the keybed at the house edge.
            const double kb = Y(0.88);
            path.moveTo(l, t + h);
            path.lineTo(l, Y(0.2));
            path.quadTo(l, t, X(0.3), t);
            path.cubicTo(X(0.68), t - h * 0.01, X(0.62), Y(0.45), r.right(), Y(0.5));
            path.lineTo(r.right(), t + h);
            path.closeSubpath();
            path.moveTo(l, kb); path.lineTo(r.right(), kb);
            stripes(path, X(0.08), X(0.92), kb, t + h);
            path.moveTo(X(0.12), Y(0.15)); path.lineTo(X(0.12), kb);      // the lid hinge
        }
        else
        {
            // Lid up. From the side it rises toward the tail; from the front
            // the open lid shows as a leaning panel behind the case.
            const double caseTop = Y(0.25), caseBot = Y(0.45);
            path.addRect(QRectF(l, caseTop, w, caseBot - caseTop));
            QPolygonF lid;
            if (m_sideOn)
                lid << QPointF(X(0.08), caseTop) << QPointF(X(0.96), t) << QPointF(X(1.0), t + h * 0.08)
                    << QPointF(X(0.2), caseTop);
            else
                lid << QPointF(X(0.12), caseTop) << QPointF(X(0.2), t) << QPointF(X(0.96), t)
                    << QPointF(X(0.88), caseTop);
            path.addPolygon(lid);
            path.moveTo(X(0.12), caseBot); path.lineTo(X(0.12), t + h);
            path.moveTo(X(0.88), caseBot); path.lineTo(X(0.88), t + h);
            if (m_sideOn) { path.moveTo(X(0.5), caseBot); path.lineTo(X(0.5), t + h); }
        }
        break;
    case StageTarget::Kind::UprightPiano:
        if (top)
        {
            // A box against the wall, the keyboard protruding at the house edge.
            const double kb = Y(0.72);
            path.addRoundedRect(r, 1.5, 1.5);
            path.moveTo(l, kb); path.lineTo(l + w, kb);
            stripes(path, X(0.04), X(0.96), kb, t + h);
        }
        else
        {
            path.addRect(r);
            if (m_sideOn)
            {
                path.moveTo(l, Y(0.55)); path.lineTo(X(0.35), Y(0.55));       // the key ledge
                path.moveTo(X(0.35), Y(0.55)); path.lineTo(X(0.35), Y(0.35));
            }
            else
            {
                path.moveTo(l, Y(0.45)); path.lineTo(l + w, Y(0.45));
                path.moveTo(X(0.5), t); path.lineTo(X(0.5), Y(0.45));
            }
        }
        break;
    case StageTarget::Kind::Rack:
        path.addRect(r);
        if (top)
        {
            // Rack units across the front half.
            units(path, l + 2.0, l + w - 2.0, Y(0.5), t + h);
            path.moveTo(l, Y(0.5)); path.lineTo(l + w, Y(0.5));
        }
        else
            units(path, l + 2.0, l + w - 2.0, t + 2.0, t + h);
        break;
    case StageTarget::Kind::Speaker:
        if (top)
        {
            // A cabinet, its cone, and a tick out of the face it points from.
            path.addRoundedRect(r, 2.0, 2.0);
            const double cr = qMax(3.0, qMin(w, h) * 0.32);
            path.addEllipse(QPointF(l + w * 0.5, t + h * 0.5), cr, cr);
            const double dot = qMax(1.2, cr * 0.28);
            path.addEllipse(QPointF(l + w * 0.5, t + h * 0.5), dot, dot);
            path.moveTo(l + w * 0.5, t + h);
            path.lineTo(l + w * 0.5, t + h + qMax(4.0, h * 0.22));
        }
        else
        {
            // The box on four little wheels; from the front, the grille's cone.
            const double caster = h * (0.1 / 1.1);
            const QRectF box(l, t, w, h - caster);
            path.addRect(box);
            if (!m_sideOn)
            {
                const double cr = qMax(2.5, qMin(box.width(), box.height()) * 0.3);
                path.addEllipse(box.center(), cr, cr);
                path.addEllipse(box.center(), cr * 0.3, cr * 0.3);
            }
            else
                path.addRect(QRectF(l + w * 0.12, t + h * 0.12, w * 0.76, (h - caster) * 0.25));
            const double wr = qMax(1.2, caster * 0.45);
            for (double fx : { 0.12, 0.88 })
                path.addEllipse(QPointF(X(fx), t + h - wr), wr, wr);
        }
        break;
    default:
        break;
    }
    return path;
}

void TargetItem::setFigure(const QPointF &feet, const QPointF &crown,
                           double pxPerMetre, bool sideOn)
{
    prepareGeometryChange();
    m_feet = feet;
    m_crown = crown;
    m_pxPerMetre = pxPerMetre;
    m_sideOn = sideOn;
    m_figureCached = false;
    update();
}

void TargetItem::setPerformers(const QVector<QPair<QPointF, QPointF> > &feetCrowns,
                               double pxPerMetre, bool sideOn)
{
    prepareGeometryChange();
    m_performers = feetCrowns;
    m_pxPerMetre = pxPerMetre;
    m_sideOn = sideOn;
    m_figureCached = false;
    update();
}

QPainterPath TargetItem::figurePath() const
{
    /* The kind is read too: a target edited into or out of a Person keeps
       its item until the next rebuild. */
    const bool person = m_target != nullptr && m_target->kind() == StageTarget::Kind::Person;
    if (!m_figureCached || m_figureCachePerson != person)
    {
        m_figureCache = buildFigurePath();
        m_figureCached = true;
        m_figureCachePerson = person;
    }
    return m_figureCache;
}

QPainterPath TargetItem::buildFigurePath() const
{
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);    // overlapping parts, not holes
    if (m_target == nullptr || m_pxPerMetre <= 0.0)
        return path;
    if (m_target->kind() == StageTarget::Kind::Person)
        return figureOutline(m_feet, m_crown, m_pxPerMetre, m_sideOn);
    // A performer line: everyone on it.
    if (m_target->isPerformerLine())
        for (const QPair<QPointF, QPointF> &fc : m_performers)
            path.addPath(figureOutline(fc.first, fc.second, m_pxPerMetre, m_sideOn));
    return path;
}

QPainterPath TargetItem::figureOutline(const QPointF &feet, const QPointF &crown,
                                       double pxPerMetre, bool sideOn)
{
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);    // overlapping parts, not holes
    const QPointF up = crown - feet;
    const double tall = std::hypot(up.x(), up.y());
    if (tall < 4.0)
    {
        /* From above: shoulders across the stage, the head in the middle. A
           floor of a few pixels keeps it a person rather than a dot when the
           whole stage is on screen. */
        const double sw = qMax(12.0, 0.46 * pxPerMetre);
        const double sd = qMax(7.0, 0.26 * pxPerMetre);
        const double hd = qMax(6.0, 0.20 * pxPerMetre);
        path.addEllipse(feet, sw * 0.5, sd * 0.5);
        path.addEllipse(feet, hd * 0.5, hd * 0.5);
        return path;
    }

    /* Standing: the outline of the same body the rig view draws, each part
       as its left edge up and its right edge back down. From the side the
       body is seen through its depth, so the section's y stands in for x. */
    const QPointF u = up / tall;                  // toward the crown
    const QPointF v(-u.y(), u.x());               // across the body
    auto at = [&](double x, double y) {
        return feet + v * (x * tall) + u * (y * tall);
    };
    foreach (const FigureLoft &loft, humanFigureLofts())
    {
        QPolygonF outline;
        for (const FigureRing &r : loft)
        {
            const double c = sideOn ? r.cy : r.cx;
            const double w = sideOn ? r.ry : r.rx;
            outline << at(c - w, r.z);
        }
        for (int k = loft.size() - 1; k >= 0; --k)
        {
            const FigureRing &r = loft.at(k);
            const double c = sideOn ? r.cy : r.cx;
            const double w = sideOn ? r.ry : r.rx;
            outline << at(c + w, r.z);
        }
        outline << outline.first();
        path.addPolygon(outline);
    }
    // One outline for the whole body, not a stroke round every part: the
    // seams at the waist and neck read as a mannequin.
    return path.simplified();
}

void TargetItem::setLineGeometry(const QPolygonF &curve, const QVector<double> &fractions,
                                 const QVector<QPointF> &handles)
{
    prepareGeometryChange();
    m_curve = curve;
    m_curveFractions = fractions;
    m_handles = handles;
    refreshReadoutVisibility();
    update();
}

static constexpr double kHandleRadius = 6.0;    // px: grabbing a handle
static constexpr double kCurveGrab = 6.0;       // px either side of the curve

int TargetItem::lineHandleAt(const QPointF &local) const
{
    /* The nearest handle in reach. A mid point within a pixel of an end wins:
       an end can always be grabbed further out, a mid point under it cannot. */
    int best = -1;
    double bestD = 0.0;
    for (int i = 0; i < m_handles.size(); ++i)
    {
        const double d = QLineF(local, m_handles.at(i)).length();
        if (d > kHandleRadius)
            continue;
        const bool isMid = (i > 0 && i < m_handles.size() - 1);
        if (best < 0 || d < bestD - (isMid ? 1.0 : 0.0))
        {
            best = i;
            bestD = d;
        }
    }
    return best;
}

bool TargetItem::lineHitAt(const QPointF &local, double *fraction) const
{
    if (m_curve.size() < 2)
        return false;
    double best = -1.0, bestF = 0.0;
    for (int i = 0; i + 1 < m_curve.size(); ++i)
    {
        const QPointF a = m_curve.at(i), b = m_curve.at(i + 1);
        const QPointF ab = b - a;
        const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
        double u = len2 > 0.0 ? QPointF::dotProduct(local - a, ab) / len2 : 0.0;
        u = qBound(0.0, u, 1.0);
        const double d = QLineF(local, a + ab * u).length();
        if (best < 0.0 || d < best)
        {
            best = d;
            const double fa = m_curveFractions.value(i), fb = m_curveFractions.value(i + 1);
            bestF = fa + (fb - fa) * u;
        }
    }
    if (best < 0.0 || best > kCurveGrab)
        return false;
    if (fraction != nullptr)
        *fraction = bestF;
    return true;
}

static constexpr double kDiamond = 4.0;        // px: a line's centre mark, half its width
static constexpr double kDiamondClear = 8.0;   // px: a handle this near hides it

bool TargetItem::onCentreMark(const QPointF &local) const
{
    // A line's centre is a small diamond on the curve: grabbing it never
    // reaches over a handle beside it.
    return QLineF(local, QPointF(0, 0)).length() <= (isLine() ? kHandleRadius : double(kRadius));
}

bool TargetItem::showsCentreDiamond() const
{
    if (!isLine())
        return false;
    for (const QPointF &h : m_handles)
        if (QLineF(h, QPointF(0, 0)).length() < kDiamondClear)
            return false;
    return true;
}

QPainterPath TargetItem::shape() const
{
    if (!isLine())
    {
        QPainterPath path;
        path.addRect(boundingRect());
        return path;
    }
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    // The centre: a small disc, not the point target's ring (u2).
    path.addEllipse(QPointF(0, 0), kHandleRadius, kHandleRadius);
    QPainterPath curve;
    curve.addPolygon(m_curve);
    QPainterPathStroker stroker;
    stroker.setWidth(kCurveGrab * 2.0);
    stroker.setCapStyle(Qt::RoundCap);
    path.addPath(stroker.createStroke(curve));
    for (const QPointF &h : m_handles)
        path.addEllipse(h, kHandleRadius + 1.0, kHandleRadius + 1.0);
    if (m_label != nullptr)
        path.addRect(m_label->mapRectToParent(m_label->boundingRect()));
    return path;
}

QRectF TargetItem::geometryRect() const
{
    QRectF rect(-kRadius, -kRadius, 2 * kRadius, 2 * kRadius);
    const QPainterPath fig = figurePath();
    if (fig.isEmpty() == false)
        rect |= fig.boundingRect();
    const QRectF foot = footprintRect();
    if (foot.isValid())
        rect |= foot;
    if (bandRect().isValid())
        rect |= bandBounds();
    if (isLine())
        rect |= m_curve.boundingRect();
    return rect;
}

QRectF TargetItem::boundingRect() const
{
    /* The crosshair arms already reach 1.4x the radius, and the aimed halo
       reaches three rings beyond it. The rect covers the largest of those
       unconditionally rather than changing with state: a bounding rect that
       shrinks when the glow turns off leaves the outer rings smeared on the
       canvas until something else forces a repaint. */
    const qreal pad = 3.0;
    const qreal r = kRadius + 3 * 4.0 + pad;
    QRectF rect(-r, -r, r * 2, r * 2);
    // Room for the "cannot reach" count under the mark, whether shown or not
    // (same reason as above: a rect that shrinks leaves the text smeared).
    rect |= QRectF(-kRadius - pad, kRadius, 90.0, 18.0);
    const QPainterPath fig = figurePath();
    if (fig.isEmpty() == false)
        rect |= fig.boundingRect().adjusted(-pad, -pad, pad, pad);
    const QRectF foot = footprintRect();
    if (foot.isValid())
        rect |= foot.adjusted(-pad, -pad, pad, pad);
    if (bandRect().isValid())
        rect |= bandBounds().adjusted(-pad, -pad, pad, pad);
    if (isLine())
    {
        const double r = kHandleRadius + 4.0;
        rect |= m_curve.boundingRect().adjusted(-r, -r, r, r);
        for (const QPointF &h : m_handles)
            rect |= QRectF(h.x() - r, h.y() - r, 2 * r, 2 * r);
    }
    return rect;
}

void TargetItem::setReachShortfall(int unreachable, int aimed, const QStringList &names)
{
    if (m_unreachable == unreachable && m_aimedHeads == aimed)
        return;
    m_unreachable = unreachable;
    m_aimedHeads = aimed;
    setToolTip(unreachable > 0
               ? tr("%1 of %2 heads aimed here cannot reach it:\n%3")
                     .arg(unreachable).arg(aimed).arg(names.join(QStringLiteral(", ")))
               : QString());
    update();
}

void TargetItem::setAimed(bool aimed)
{
    if (m_aimed == aimed)
        return;
    m_aimed = aimed;
    update();
}

void TargetItem::paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget * /*widget*/)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool selected = option->state & QStyle::State_Selected;
    /* Always its own colour -- the one set in Edit Target. Locked (its own
       lock or its layer's) is said by a red ring round the mark, the way
       every other locked thing is outlined red: painting the whole target
       red lost the colour, and a line drawn red read as the wrong colour. */
    const QColor col = m_target->color().isValid() ? m_target->color() : QColor(255, 180, 0);
    const bool locked = ItemLock::drawnLocked(this, m_target->locked());

    /* The ground it covers, under everything else: tinted so the stage under
       it still reads, outlined so its edge is exact. */
    const QRectF foot = footprintRect();
    if (foot.isValid())
    {
        QColor tint = col;
        tint.setAlpha(selected ? 70 : 45);
        QPen edge(col, selected ? 2.0 : 1.4);
        edge.setStyle(Qt::DashLine);
        painter->setPen(edge);
        painter->setBrush(tint);
        painter->drawRoundedRect(foot, 3.0, 3.0);
    }

    /* A band instrument: the thing itself, in the target's colour, drawn the
       way the kit's footprint is (same weight, tint and selected state). */
    if (bandRect().isValid())
    {
        QColor tint = col;
        tint.setAlpha(selected ? 90 : 60);
        painter->setPen(QPen(col, selected ? 2.0 : 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(tint);
        painter->drawPath(bandOutline());
    }

    /* A line: the curve, its ends as squares and its mid points as dots, and
       a small diamond on the curve at its centre -- where every light goes
       at spread 0. No ring or crosshair: the point target's mark, a little
       off the user's mid handle, read as a misplaced target (u2). Selected
       and locked are drawn along the curve. */
    if (isLine())
    {
        /* The people on a performer line, under the line, in what they wear. */
        const QPainterPath crowd = figurePath();
        if (!crowd.isEmpty())
        {
            QColor fill = m_target->costumeColor().isValid() ? m_target->costumeColor() : col;
            fill.setAlpha(selected ? 150 : 100);
            painter->setPen(QPen(fill.darker(140), 1.0));
            painter->setBrush(fill);
            painter->drawPath(crowd);
        }
        QPainterPath curvePath;
        curvePath.addPolygon(m_curve);
        if (selected)
        {
            QColor glow = col.lighter(130);
            glow.setAlpha(110);
            QPen glowPen(glow, 9.0);
            glowPen.setCapStyle(Qt::RoundCap);
            glowPen.setJoinStyle(Qt::RoundJoin);
            painter->setPen(glowPen);
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(curvePath);
        }
        if (locked)
        {
            QPainterPathStroker stroker;
            stroker.setWidth(10.0);
            stroker.setCapStyle(Qt::RoundCap);
            stroker.setJoinStyle(Qt::RoundJoin);
            QPen lockPen(QColor(200, 60, 60), 1.6);
            lockPen.setStyle(Qt::DashLine);
            painter->setPen(lockPen);
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(stroker.createStroke(curvePath).simplified());
        }
        QPen linePen(col, selected ? 3.5 : 2.5);
        linePen.setCapStyle(Qt::RoundCap);
        linePen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(linePen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPolyline(m_curve);
        for (int i = 0; i < m_handles.size(); ++i)
        {
            const QPointF h = m_handles.at(i);
            const bool isEnd = (i == 0 || i == m_handles.size() - 1);
            painter->setPen(QPen(col.darker(160), 1.2));
            painter->setBrush(selected ? col.lighter(130) : col);
            if (isEnd)
                painter->drawRect(QRectF(h.x() - kHandleRadius + 1.0, h.y() - kHandleRadius + 1.0,
                                         2 * kHandleRadius - 2.0, 2 * kHandleRadius - 2.0));
            else
                painter->drawEllipse(h, kHandleRadius - 1.5, kHandleRadius - 1.5);
        }
        if (showsCentreDiamond())
        {
            QPolygonF diamond;
            diamond << QPointF(0, -kDiamond) << QPointF(kDiamond, 0)
                    << QPointF(0, kDiamond) << QPointF(-kDiamond, 0);
            painter->setBrush(Qt::NoBrush);   // hollow: the stage reads through it
            painter->setPen(QPen(selected ? col.lighter(130) : col, 1.6));
            painter->drawPolygon(diamond);
        }
        // Heads that cannot reach: the count beside the readout, in red.
        if (m_unreachable > 0)
        {
            const QColor warn(235, 70, 60);
            QFont f = painter->font();
            f.setBold(true);
            f.setPointSizeF(8.0);
            painter->setFont(f);
            painter->setPen(warn);
            painter->drawText(QRectF(-kRadius, kRadius + 1.0, 88.0, 16.0),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              QStringLiteral("\u26A0 %1/%2").arg(m_unreachable).arg(m_aimedHeads));
        }
        return;
    }

    /* What stands here, under the mark: the mark still says where it is aimed,
       the figure says at whom. */
    const QPainterPath fig = figurePath();
    if (fig.isEmpty() == false)
    {
        // What they wear, when a costume is set; else the target's colour.
        QColor fill = (m_target != nullptr && m_target->costumeColor().isValid())
                      ? m_target->costumeColor() : col;
        fill.setAlpha(selected ? 150 : 100);
        painter->setPen(QPen(col, 1.2));
        painter->setBrush(fill);
        painter->drawPath(fig);
    }

    /* Glow for a target the focused scene actually aims at. Every target is
       drawn, because a target exists as soon as it is placed in the studio --
       but "something is pointing at this one right now" is the thing worth
       finding on a crowded stage, so it gets a halo rather than a different
       shape. Concentric fading rings: cheap, does not move the item's centre,
       and still legible against both the light and dark canvas. */
    if (m_aimed)
    {
        painter->setBrush(Qt::NoBrush);
        for (int i = 3; i >= 1; i--)
        {
            QColor halo = col;
            halo.setAlpha(30 + 20 * (3 - i));
            painter->setPen(QPen(halo, 3.0));
            painter->drawEllipse(QPointF(0, 0),
                                 double(kRadius) + i * 4.0,
                                 double(kRadius) + i * 4.0);
        }
    }

    // Outer ring
    QPen ringPen(col, selected ? 2.5 : 1.5);
    painter->setPen(ringPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QPointF(0, 0), double(kRadius), double(kRadius));

    // Inner dot
    QColor dotColor = col;
    dotColor.setAlpha(selected ? 220 : 160);
    painter->setBrush(QBrush(dotColor));
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(QPointF(0, 0), 3.0, 3.0);

    /* Heads that cannot reach: a red ring and a count under the mark. Drawn
       whether or not the target is selected, because this is the one thing
       about a target you need to see without looking for it. */
    if (m_unreachable > 0)
    {
        const QColor warn(235, 70, 60);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(warn, 2.5));
        painter->drawEllipse(QPointF(0, 0), double(kRadius) + 2.0, double(kRadius) + 2.0);
        QFont f = painter->font();
        f.setBold(true);
        f.setPointSizeF(8.0);
        painter->setFont(f);
        painter->setPen(warn);
        painter->drawText(QRectF(-kRadius, kRadius + 1.0, 88.0, 16.0),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QStringLiteral("\u26A0 %1/%2").arg(m_unreachable).arg(m_aimedHeads));
    }

    // Crosshair lines
    painter->setPen(QPen(col, 1.0));
    const float arm = kRadius * 1.4f;
    const float gap = 4.0f;
    painter->drawLine(QPointF(-arm, 0), QPointF(-gap, 0));
    painter->drawLine(QPointF( gap, 0), QPointF( arm, 0));
    painter->drawLine(QPointF(0, -arm), QPointF(0, -gap));
    painter->drawLine(QPointF(0,  gap), QPointF(0,  arm));

    // Locked: a red dashed ring outside the mark.
    if (locked)
    {
        QPen lockPen(QColor(200, 60, 60), 2.0);
        lockPen.setStyle(Qt::DashLine);
        painter->setPen(lockPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(0, 0), double(kRadius) + 6.0, double(kRadius) + 6.0);
    }

    // Selection glow ring
    if (selected)
    {
        QPen glowPen(col.lighter(130), 3.5);
        glowPen.setStyle(Qt::SolidLine);
        painter->setPen(glowPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(0, 0), double(kRadius) + 3.0, double(kRadius) + 3.0);
    }
}

QVariant TargetItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    // Moved -- dragged, or put somewhere new: say where it is now.
    if (change == ItemPositionHasChanged && m_readout != nullptr && m_readoutText)
        m_readout->setText(m_readoutText(value.toPointF()));
    if (change == ItemSelectedHasChanged)
        refreshReadoutVisibility();
    return QGraphicsItem::itemChange(change, value);
}

void TargetItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    /* A line's handle is dragged on its own: the line bends or stretches, the
       target does not move. Only where the target could be dragged at all. */
    m_dragHandle = -1;
    if (event->button() == Qt::LeftButton && isLine() && (flags() & ItemIsMovable))
    {
        const int h = lineHandleAt(event->pos());
        if (h >= 0)
        {
            m_dragHandle = h;
            if (!isSelected())
            {
                if (scene() != nullptr)
                    scene()->clearSelection();
                setSelected(true);
            }
            event->accept();
            return;
        }
    }
    QGraphicsItem::mousePressEvent(event);
}

void TargetItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragHandle >= 0)
    {
        emit lineHandleMoved(this, m_dragHandle, event->scenePos(), event->modifiers());
        event->accept();
        return;
    }
    QGraphicsItem::mouseMoveEvent(event);
}

void TargetItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragHandle >= 0)
    {
        const int h = m_dragHandle;
        m_dragHandle = -1;
        event->accept();
        emit lineHandleReleased(this, h);
        return;
    }
    QGraphicsItem::mouseReleaseEvent(event);
    emit itemDropped(this);
}


/** Delete this target, saying first what else it will take with it.
 *
 *  A target is not an isolated marker: Aim palettes point at it, and scenes
 *  use those palettes. Deleting one silently would leave those palettes aiming
 *  at an id that no longer resolves -- the lights keep whatever position they
 *  last had and nothing says why. So name the scenes before asking, because
 *  "2 scenes use this" is the fact that decides the answer.
 */
void TargetItem::confirmAndDelete()
{
    if (m_doc == NULL || m_target == NULL)
        return;

    MonitorProperties *props = m_doc->monitorProperties();
    if (props == NULL)
        return;

    const quint32 tid = m_target->id();
    const QString tname = m_target->name();

    // Palettes aiming here, and the scenes that use them.
    QList<quint32> boundPalettes;
    foreach (QLCPalette *pal, m_doc->palettes())
    {
        if (pal != NULL && pal->stageTargetId() == tid)
            boundPalettes << pal->id();
    }

    QStringList sceneNames;
    if (boundPalettes.isEmpty() == false)
    {
        foreach (Function *f, m_doc->functions())
        {
            Scene *sc = qobject_cast<Scene *>(f);
            if (sc == NULL)
                continue;
            foreach (quint32 pid, sc->palettes())
            {
                if (boundPalettes.contains(pid))
                { sceneNames << sc->name(); break; }
            }
        }
    }

    QString question = tr("Delete target \"%1\"?").arg(tname);
    if (sceneNames.isEmpty() == false)
    {
        question += tr("\n\n%n scene(s) aim at it: %1.\n"
                       "They will keep the Aim look but it will no longer "
                       "resolve, so those fixtures hold their last position.",
                       "", sceneNames.size()).arg(sceneNames.join(", "));
    }
    else if (boundPalettes.isEmpty() == false)
    {
        question += tr("\n\n%n Aim palette(s) point at it and will stop "
                       "resolving.", "", boundPalettes.size());
    }

    if (QMessageBox::question(nullptr, tr("Delete target"), question,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes)
        return;

    /* The view owns this item and rebuilds the whole set, so ask it to --
       deleting ourselves from inside our own event handler is not survivable.
       Found first: removing the target takes this item out of the scene
       (MonitorGraphicsView::dropItemFor()), so it has no view after. */
    MonitorGraphicsView *view =
        qobject_cast<MonitorGraphicsView *>(scene() ? scene()->views().value(0) : nullptr);
    Doc *doc = m_doc;
    props->removeStageTarget(tid);
    doc->setModified();
    if (view != nullptr)
        QMetaObject::invokeMethod(view, "updateTargets", Qt::QueuedConnection);
}
