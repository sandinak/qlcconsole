/*
  Q Light Controller Plus
  scenery.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QtMath>

#include "scenery.h"

#define KXMLSceneryID       QStringLiteral("ID")
#define KXMLSceneryName     QStringLiteral("Name")
#define KXMLSceneryKind     QStringLiteral("Kind")
#define KXMLSceneryOriginX  QStringLiteral("OriginX")
#define KXMLSceneryOriginY  QStringLiteral("OriginY")
#define KXMLSceneryWidth    QStringLiteral("Width")
#define KXMLSceneryHeight   QStringLiteral("Height")
#define KXMLSceneryBottom   QStringLiteral("Bottom")
#define KXMLSceneryRotation QStringLiteral("Rotation")
#define KXMLSceneryBow      QStringLiteral("Bow")
#define KXMLSceneryOutline  QStringLiteral("Outline")
#define KXMLSceneryDepth    QStringLiteral("Depth")
#define KXMLSceneryThickness QStringLiteral("Thickness")
#define KXMLSceneryState    QStringLiteral("State")
#define KXMLSceneryOpenWidth  QStringLiteral("OpeningWidth")
#define KXMLSceneryOpenHeight QStringLiteral("OpeningHeight")
#define KXMLSceneryColor    QStringLiteral("Color")
#define KXMLSceneryLocked   QStringLiteral("Locked")
#define KXMLSceneryLayerId  QStringLiteral("LayerId")
#define KXMLSceneryGroupId  QStringLiteral("GroupId")

Scenery::Scenery(quint32 id, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_color(defaultColor(Cyclorama))
{
}

QString Scenery::kindName(int kind)
{
    switch (kind)
    {
        case BackWall:  return tr("Back wall");
        case MainDrape: return tr("Main drape");
        case Leg:       return tr("Leg");
        case Border:    return tr("Border");
        case Traveler:  return tr("Traveler");
        case Scrim:     return tr("Scrim");
        case Wall:      return tr("Wall");
        case Proscenium: return tr("Proscenium");
        case Cyclorama:
        default:        return tr("Cyclorama");
    }
}

QString Scenery::stateName(int state)
{
    switch (state)
    {
        case Open:  return tr("Open");
        case Flown: return tr("Flown out");
        case Closed:
        default:    return tr("Closed");
    }
}

QList<int> Scenery::kinds()
{
    return QList<int>() << Cyclorama << BackWall << Proscenium << MainDrape << Traveler
                        << Leg << Border << Scrim << Wall;
}

QColor Scenery::defaultColor(int kind)
{
    switch (kind)
    {
        case BackWall:  return QColor(70, 70, 76);      // a painted black-ish wall
        case MainDrape: return QColor(118, 22, 34);     // house red
        case Leg:
        case Border:
        case Traveler:  return QColor(28, 28, 32);      // masking black
        case Scrim:     return QColor(200, 202, 210);
        case Wall:      return QColor(150, 142, 128);   // block
        case Proscenium: return QColor(46, 44, 50);     // a dark arch
        case Cyclorama:
        default:        return QColor(226, 228, 236);   // a cyc is pale: it is there to take colour
    }
}

QPointF Scenery::along() const
{
    const double r = qDegreesToRadians(double(m_rotation));
    return QPointF(qCos(r), qSin(r));
}

QPointF Scenery::facing() const
{
    // Downstage (+Y) at rotation 0, turning with the sheet.
    const double r = qDegreesToRadians(double(m_rotation));
    return QPointF(-qSin(r), qCos(r));
}

QVector3D Scenery::pointAt(double u, double z, double offset) const
{
    u = qBound(-1.0, u, 1.0);
    const QPointF a = along(), f = facing();
    double lx, ly;      // along the sheet, toward the audience
    if (m_outline == RoundedU)
    {
        /* From the end of the stage-right side, down it, round the front
           and back up the stage-left side. The open back is the origin. */
        const double r = double(m_width) / 2.0;
        const double side = qMax(0.0, double(m_depth) - r);
        const double len = 2.0 * side + M_PI * r;
        const double t = (u + 1.0) / 2.0 * len;
        if (t <= side)
        {
            lx = -r;
            ly = t;
        }
        else if (t <= side + M_PI * r)
        {
            const double th = M_PI - (t - side) / r;
            lx = r * qCos(th);
            ly = side + r * qSin(th);
        }
        else
        {
            lx = r;
            ly = side - (t - side - M_PI * r);
        }
    }
    else
    {
        // A parabola: the full bow at the middle, none at the ends.
        lx = u * double(m_width) / 2.0;
        ly = -double(m_bow) * (1.0 - u * u);
    }
    double x = double(m_originX) + a.x() * lx + f.x() * ly;
    double y = double(m_originY) + a.y() * lx + f.y() * ly;
    if (offset != 0.0)
    {
        const QPointF n = normalAt(u);
        x += n.x() * offset;
        y += n.y() * offset;
    }
    return QVector3D(float(x), float(y), float(z));
}

QPointF Scenery::normalAt(double u) const
{
    // The line's direction there, turned a quarter toward the front.
    const double e = 1e-4;
    const double u0 = qMax(-1.0, u - e), u1 = qMin(1.0, u + e);
    const QVector3D p0 = pointAt(u0, 0.0), p1 = pointAt(u1, 0.0);
    double tx = double(p1.x() - p0.x()), ty = double(p1.y() - p0.y());
    const double len = qSqrt(tx * tx + ty * ty);
    if (len < 1e-12)
        return facing();
    tx /= len;
    ty /= len;
    return QPointF(-ty, tx);
}

QList<QPair<double, double> > Scenery::spans() const
{
    QList<QPair<double, double> > out;
    if (hasState() && m_state == Flown)
        return out;
    if (hasState() && m_state == Open)
    {
        /* Open by its opening, centred; each half gathered into what is left
           at its end. An opening as wide as the curtain (never set for it)
           leaves about a sixth of the width each side. */
        const double gap = (m_openWidth < m_width) ? double(m_openWidth) : 0.68 * double(m_width);
        const double f = qBound(0.0, gap / double(m_width), 0.98);
        out << qMakePair(-1.0, -f) << qMakePair(f, 1.0);
        return out;
    }
    out << qMakePair(-1.0, 1.0);
    return out;
}

QList<Scenery::Piece> Scenery::pieces() const
{
    QList<Piece> out;
    const float lo = m_bottom, hi = m_bottom + m_height;
    if (m_kind == Proscenium)
    {
        // The opening, centred: the piers are what is left either side.
        const double f = qBound(0.0, double(m_openWidth) / double(m_width), 0.98);
        const float head = qMin(hi, lo + m_openHeight);
        out << Piece{ -1.0, -f, lo, hi } << Piece{ f, 1.0, lo, hi };
        if (head < hi)
            out << Piece{ -f, f, head, hi };
        return out;
    }
    typedef QPair<double, double> Span;
    foreach (const Span &sp, spans())
        out << Piece{ sp.first, sp.second, lo, hi };
    return out;
}

int Scenery::segments() const
{
    if (m_outline == RoundedU)
        return 40;
    return (m_bow != 0.0f) ? 16 : 1;
}

QList<QPointF> Scenery::baseLine(int segments) const
{
    segments = qMax(1, segments);
    QList<QPointF> out;
    for (int i = 0; i <= segments; ++i)
    {
        const QVector3D p = pointAt(-1.0 + 2.0 * i / segments, m_bottom);
        out << QPointF(double(p.x()), double(p.y()));
    }
    return out;
}

bool Scenery::rayHit(const QVector3D &origin, const QVector3D &dir, double &t,
                     double maxT, int segments, QVector3D *normal) const
{
    const int own = this->segments();
    const int full = (own == 1) ? 1 : qMax(segments, own);
    const double half = double(m_thickness) / 2.0;
    QList<double> faces;
    if (half > 0.0)
        faces << -half << half;
    else
        faces << 0.0;

    bool hit = false;
    double best = maxT;
    QVector3D bestN;
    foreach (const Piece &pc, pieces())
    {
        const double zLo = double(pc.z0), zHi = double(pc.z1);
        const QPair<double, double> sp(pc.u0, pc.u1);
        const int n = qMax(1, int(qCeil(full * (sp.second - sp.first) / 2.0)));
        foreach (double off, faces)
        {
            for (int i = 0; i < n; ++i)
            {
                // A vertical panel from one point on the line to the next.
                const double ua = sp.first + (sp.second - sp.first) * i / n;
                const double ub = sp.first + (sp.second - sp.first) * (i + 1) / n;
                const QVector3D pa = pointAt(ua, 0.0, off), pb = pointAt(ub, 0.0, off);
                const QPointF p0(double(pa.x()), double(pa.y())), p1(double(pb.x()), double(pb.y()));
                const double ex = p1.x() - p0.x(), ey = p1.y() - p0.y();
                // Its horizontal normal.
                const double nx = -ey, ny = ex;
                const double denom = nx * double(dir.x()) + ny * double(dir.y());
                if (qAbs(denom) < 1e-12)
                    continue;                         // running parallel to it
                const double tt = (nx * (p0.x() - double(origin.x())) + ny * (p0.y() - double(origin.y()))) / denom;
                if (tt <= 1e-6 || tt >= best)
                    continue;
                const double hx = double(origin.x()) + double(dir.x()) * tt;
                const double hy = double(origin.y()) + double(dir.y()) * tt;
                const double hz = double(origin.z()) + double(dir.z()) * tt;
                if (hz < zLo || hz > zHi)
                    continue;
                // Within the panel's length?
                const double len2 = ex * ex + ey * ey;
                if (len2 < 1e-18)
                    continue;
                const double s = ((hx - p0.x()) * ex + (hy - p0.y()) * ey) / len2;
                if (s < 0.0 || s > 1.0)
                    continue;
                best = tt;
                hit = true;
                // Facing back along the ray, whichever side of the cloth it came from.
                bestN = QVector3D(float(nx), float(ny), 0.0f).normalized();
                if (denom > 0.0)
                    bestN = -bestN;
            }

            /* A wall's top: light coming down on it lands there; and the
               underside of a header, for light coming up. Between the faces
               of each panel, a quad on that plane. */
            if (half <= 0.0 || off > -half || qAbs(double(dir.z())) < 1e-9)
                continue;
            const double capZ = (dir.z() < 0.0f) ? zHi : zLo;
            if (dir.z() > 0.0f && capZ <= double(m_bottom) + 1e-4)
                continue;                             // its foot is on the floor
            const double tt = (capZ - double(origin.z())) / double(dir.z());
            if (tt <= 1e-6 || tt >= best)
                continue;
            const QPointF h(double(origin.x()) + double(dir.x()) * tt,
                            double(origin.y()) + double(dir.y()) * tt);
            for (int i = 0; i < n; ++i)
            {
                const double ua = sp.first + (sp.second - sp.first) * i / n;
                const double ub = sp.first + (sp.second - sp.first) * (i + 1) / n;
                const QVector3D a = pointAt(ua, 0.0), b = pointAt(ub, 0.0);
                const double ex = double(b.x() - a.x()), ey = double(b.y() - a.y());
                const double len2 = ex * ex + ey * ey;
                if (len2 < 1e-18)
                    continue;
                const double s = ((h.x() - double(a.x())) * ex + (h.y() - double(a.y())) * ey) / len2;
                if (s < 0.0 || s > 1.0)
                    continue;
                const double cx = double(a.x()) + ex * s - h.x(), cy = double(a.y()) + ey * s - h.y();
                if (cx * cx + cy * cy > half * half)
                    continue;
                best = tt;
                hit = true;
                bestN = QVector3D(0.0f, 0.0f, dir.z() < 0.0f ? 1.0f : -1.0f);
                break;
            }
        }
    }
    if (hit)
    {
        t = best;
        if (normal != nullptr)
            *normal = bestN;
    }
    return hit;
}

bool Scenery::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLScenery)
        return false;

    const QXmlStreamAttributes a = root.attributes();
    if (a.hasAttribute(KXMLSceneryID))       m_id = a.value(KXMLSceneryID).toUInt();
    if (a.hasAttribute(KXMLSceneryName))     m_name = a.value(KXMLSceneryName).toString();
    if (a.hasAttribute(KXMLSceneryKind))     m_kind = a.value(KXMLSceneryKind).toInt();
    if (a.hasAttribute(KXMLSceneryOriginX))  m_originX = a.value(KXMLSceneryOriginX).toFloat();
    if (a.hasAttribute(KXMLSceneryOriginY))  m_originY = a.value(KXMLSceneryOriginY).toFloat();
    if (a.hasAttribute(KXMLSceneryWidth))    setWidth(a.value(KXMLSceneryWidth).toFloat());
    if (a.hasAttribute(KXMLSceneryHeight))   setHeight(a.value(KXMLSceneryHeight).toFloat());
    if (a.hasAttribute(KXMLSceneryBottom))   setBottom(a.value(KXMLSceneryBottom).toFloat());
    if (a.hasAttribute(KXMLSceneryRotation)) m_rotation = a.value(KXMLSceneryRotation).toFloat();
    if (a.hasAttribute(KXMLSceneryBow))      setBow(a.value(KXMLSceneryBow).toFloat());
    if (a.hasAttribute(KXMLSceneryOutline))  setOutline(a.value(KXMLSceneryOutline).toString() == "U" ? RoundedU : SheetOutline);
    if (a.hasAttribute(KXMLSceneryDepth))    setDepth(a.value(KXMLSceneryDepth).toFloat());
    if (a.hasAttribute(KXMLSceneryThickness)) setThickness(a.value(KXMLSceneryThickness).toFloat());
    if (a.hasAttribute(KXMLSceneryOpenWidth)) setOpeningWidth(a.value(KXMLSceneryOpenWidth).toFloat());
    if (a.hasAttribute(KXMLSceneryOpenHeight)) setOpeningHeight(a.value(KXMLSceneryOpenHeight).toFloat());
    if (a.hasAttribute(KXMLSceneryState))
    {
        const QString st = a.value(KXMLSceneryState).toString();
        setState(st == "Open" ? Open : st == "Flown" ? Flown : Closed);
    }
    if (a.hasAttribute(KXMLSceneryColor))    m_color = QColor(a.value(KXMLSceneryColor).toString());
    if (a.hasAttribute(KXMLSceneryLocked))   m_locked = (a.value(KXMLSceneryLocked).toString() == "true");
    if (a.hasAttribute(KXMLSceneryLayerId))  m_layerId = a.value(KXMLSceneryLayerId).toUInt();
    if (a.hasAttribute(KXMLSceneryGroupId))  m_groupId = a.value(KXMLSceneryGroupId).toUInt();

    root.skipCurrentElement();
    return true;
}

bool Scenery::saveXML(QXmlStreamWriter *doc) const
{
    doc->writeStartElement(KXMLScenery);
    doc->writeAttribute(KXMLSceneryID,      QString::number(m_id));
    doc->writeAttribute(KXMLSceneryName,    m_name);
    doc->writeAttribute(KXMLSceneryKind,    QString::number(m_kind));
    doc->writeAttribute(KXMLSceneryOriginX, QString::number(double(m_originX), 'f', 3));
    doc->writeAttribute(KXMLSceneryOriginY, QString::number(double(m_originY), 'f', 3));
    doc->writeAttribute(KXMLSceneryWidth,   QString::number(double(m_width),   'f', 3));
    doc->writeAttribute(KXMLSceneryHeight,  QString::number(double(m_height),  'f', 3));
    if (m_bottom != 0.0f)
        doc->writeAttribute(KXMLSceneryBottom, QString::number(double(m_bottom), 'f', 3));
    if (m_rotation != 0.0f)
        doc->writeAttribute(KXMLSceneryRotation, QString::number(double(m_rotation), 'f', 1));
    if (m_bow != 0.0f)
        doc->writeAttribute(KXMLSceneryBow, QString::number(double(m_bow), 'f', 3));
    if (m_outline == RoundedU)
    {
        doc->writeAttribute(KXMLSceneryOutline, "U");
        doc->writeAttribute(KXMLSceneryDepth, QString::number(double(m_depth), 'f', 3));
    }
    if (m_thickness != 0.0f)
        doc->writeAttribute(KXMLSceneryThickness, QString::number(double(m_thickness), 'f', 3));
    if (m_state != Closed)
        doc->writeAttribute(KXMLSceneryState, m_state == Open ? "Open" : "Flown");
    if (m_kind == Proscenium)
    {
        doc->writeAttribute(KXMLSceneryOpenWidth, QString::number(double(m_openWidth), 'f', 3));
        doc->writeAttribute(KXMLSceneryOpenHeight, QString::number(double(m_openHeight), 'f', 3));
    }
    else if (hasState())
        doc->writeAttribute(KXMLSceneryOpenWidth, QString::number(double(m_openWidth), 'f', 3));
    if (m_color.isValid())
        doc->writeAttribute(KXMLSceneryColor, m_color.name());
    if (m_locked)
        doc->writeAttribute(KXMLSceneryLocked, "true");
    if (m_layerId != 0)
        doc->writeAttribute(KXMLSceneryLayerId, QString::number(m_layerId));
    if (m_groupId != 0)
        doc->writeAttribute(KXMLSceneryGroupId, QString::number(m_groupId));
    doc->writeEndElement();
    return true;
}
