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
#define KXMLSceneryColor    QStringLiteral("Color")
#define KXMLSceneryLocked   QStringLiteral("Locked")
#define KXMLSceneryLayerId  QStringLiteral("LayerId")
#define KXMLSceneryGroupId  QStringLiteral("GroupId")

Scenery::Scenery(quint32 id, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_color(QColor(226, 228, 236))     // a cyc is pale: it is there to take colour
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
        case Cyclorama:
        default:        return tr("Cyclorama");
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

QVector3D Scenery::pointAt(double u, double z) const
{
    u = qBound(-1.0, u, 1.0);
    const QPointF a = along(), f = facing();
    const double half = double(m_width) / 2.0;
    // A parabola: the full bow at the middle, none at the ends.
    const double back = double(m_bow) * (1.0 - u * u);
    const double x = double(m_originX) + a.x() * u * half - f.x() * back;
    const double y = double(m_originY) + a.y() * u * half - f.y() * back;
    return QVector3D(float(x), float(y), float(z));
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
    const double zLo = double(m_bottom), zHi = double(m_bottom + m_height);
    const QList<QPointF> line = baseLine(m_bow > 0.0f ? segments : 1);
    bool hit = false;
    double best = maxT;
    QVector3D bestN;
    for (int i = 0; i + 1 < line.size(); ++i)
    {
        // A vertical panel from line[i] to line[i + 1].
        const QPointF p0 = line.at(i), p1 = line.at(i + 1);
        const double ex = p1.x() - p0.x(), ey = p1.y() - p0.y();
        // Its horizontal normal.
        const double nx = -ey, ny = ex;
        const double denom = nx * double(dir.x()) + ny * double(dir.y());
        if (qAbs(denom) < 1e-9)
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
