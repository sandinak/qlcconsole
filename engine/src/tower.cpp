/*
  Q Light Controller Plus
  tower.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <algorithm>
#include <cmath>
#include <QtMath>

#include "tower.h"

#define KXMLTower        QStringLiteral("Tower")
#define KXMLTowerID      QStringLiteral("ID")
#define KXMLTowerName    QStringLiteral("Name")
#define KXMLTowerOriginX QStringLiteral("OriginX")
#define KXMLTowerOriginY QStringLiteral("OriginY")
#define KXMLTowerWidth   QStringLiteral("Width")
#define KXMLTowerDepth   QStringLiteral("Depth")
#define KXMLTowerHeight  QStringLiteral("Height")
#define KXMLTowerColor   QStringLiteral("Color")
#define KXMLTowerLocked  QStringLiteral("Locked")
#define KXMLTowerLayerId QStringLiteral("LayerId")
#define KXMLTowerGroupId QStringLiteral("GroupId")
#define KXMLTowerShelf   QStringLiteral("Shelf")
#define KXMLTowerShelfZ  QStringLiteral("Z")
#define KXMLTowerRotation QStringLiteral("Rotation")

void Tower::setRotation(float deg)
{
    double d = std::fmod(double(deg), 360.0);
    if (d > 180.0) d -= 360.0;
    if (d <= -180.0) d += 360.0;
    m_rotation = (qAbs(d) < 1e-4) ? 0.0f : float(d);
}

static QPointF turnAbout(const QPointF &p, const QPointF &about, double deg)
{
    if (deg == 0.0)
        return p;
    const double r = qDegreesToRadians(deg);
    const double c = qCos(r), s = qSin(r);
    const double dx = p.x() - about.x(), dy = p.y() - about.y();
    return QPointF(about.x() + dx * c - dy * s, about.y() + dx * s + dy * c);
}

QPointF Tower::toWorld(const QPointF &local) const
{
    return turnAbout(local, centre(), double(m_rotation));
}

QPointF Tower::toLocal(const QPointF &world) const
{
    return turnAbout(world, centre(), -double(m_rotation));
}

QList<QPointF> Tower::corners() const
{
    const double x0 = double(m_originX), y0 = double(m_originY);
    const double x1 = x0 + double(m_width), y1 = y0 + double(m_depth);
    return QList<QPointF>() << toWorld(QPointF(x0, y0)) << toWorld(QPointF(x1, y0))
                            << toWorld(QPointF(x1, y1)) << toWorld(QPointF(x0, y1));
}

Tower::Tower(quint32 id, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_color(QColor(90, 100, 120, 220))
{
}

void Tower::addShelf(float z)
{
    // Reject an effective duplicate (within 1cm of an existing shelf) rather
    // than silently creating two shelves that read identically in the editor
    // (same rounded height, same drawn line) with no visible sign a second
    // one even exists -- confirmed as a real, reachable case: clicking
    // "Add shelf" without changing the height spinbox from a value that
    // already matched an existing shelf created exactly this with no warning.
    for (float existing : m_shelves)
    {
        if (qAbs(existing - z) < 0.01f)
            return;
    }
    m_shelves.append(z);
    std::sort(m_shelves.begin(), m_shelves.end());
}

void Tower::removeShelf(int i)
{
    if (i >= 0 && i < m_shelves.size())
        m_shelves.removeAt(i);
}

void Tower::setShelfHeight(int i, float z)
{
    if (i < 0 || i >= m_shelves.size())
        return;
    // Same duplicate guard as addShelf(), against every OTHER shelf (not
    // itself, or trivially "z is within 1cm of its own current value" would
    // always block the edit).
    for (int j = 0; j < m_shelves.size(); ++j)
    {
        if (j != i && qAbs(m_shelves.at(j) - z) < 0.01f)
            return;
    }
    m_shelves[i] = z;   // no sort: see the header comment
}

bool Tower::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLTower)
        return false;

    QXmlStreamAttributes a = root.attributes();
    if (a.hasAttribute(KXMLTowerID))      m_id      = a.value(KXMLTowerID).toUInt();
    if (a.hasAttribute(KXMLTowerName))    m_name    = a.value(KXMLTowerName).toString();
    if (a.hasAttribute(KXMLTowerOriginX)) m_originX = a.value(KXMLTowerOriginX).toFloat();
    if (a.hasAttribute(KXMLTowerOriginY)) m_originY = a.value(KXMLTowerOriginY).toFloat();
    if (a.hasAttribute(KXMLTowerWidth))   setWidth(a.value(KXMLTowerWidth).toFloat());
    if (a.hasAttribute(KXMLTowerDepth))   setDepth(a.value(KXMLTowerDepth).toFloat());
    if (a.hasAttribute(KXMLTowerHeight))  setHeight(a.value(KXMLTowerHeight).toFloat());
    if (a.hasAttribute(KXMLTowerColor))   m_color   = QColor(a.value(KXMLTowerColor).toString());
    if (a.hasAttribute(KXMLTowerLocked))  m_locked  = (a.value(KXMLTowerLocked).toString() == "true");
    if (a.hasAttribute(KXMLTowerLayerId)) m_layerId = a.value(KXMLTowerLayerId).toUInt();
    if (a.hasAttribute(KXMLTowerGroupId)) m_groupId = a.value(KXMLTowerGroupId).toUInt();
    if (a.hasAttribute(KXMLTowerRotation)) setRotation(a.value(KXMLTowerRotation).toFloat());
    m_faces = a.hasAttribute(QStringLiteral("Faces"));
    if (m_faces)
        setFacesSides(a.value(QStringLiteral("Faces")).toInt());
    if (a.hasAttribute(QStringLiteral("FacesColour")))
        setFacesColour(QColor(a.value(QStringLiteral("FacesColour")).toString()));
    if (a.hasAttribute(QStringLiteral("FacesFrom")))
        m_facesFrom = a.value(QStringLiteral("FacesFrom")).toFloat();
    if (a.hasAttribute(QStringLiteral("FacesTo")))
        m_facesTo = a.value(QStringLiteral("FacesTo")).toFloat();
    m_facesPattern = a.value(QStringLiteral("FacesPattern")).toString();
    if (a.hasAttribute(QStringLiteral("FacesPatternOpacity")))
        setFacesPatternOpacity(a.value(QStringLiteral("FacesPatternOpacity")).toFloat());

    m_shelves.clear();
    while (root.readNextStartElement())
    {
        if (root.name() == KXMLTowerShelf)
        {
            m_shelves.append(root.attributes().value(KXMLTowerShelfZ).toFloat());
            root.skipCurrentElement();
        }
        else
        {
            root.skipCurrentElement();
        }
    }
    std::sort(m_shelves.begin(), m_shelves.end());
    return true;
}

bool Tower::saveXML(QXmlStreamWriter *doc) const
{
    doc->writeStartElement(KXMLTower);
    doc->writeAttribute(KXMLTowerID,      QString::number(m_id));
    doc->writeAttribute(KXMLTowerName,    m_name);
    doc->writeAttribute(KXMLTowerOriginX, QString::number(double(m_originX), 'f', 3));
    doc->writeAttribute(KXMLTowerOriginY, QString::number(double(m_originY), 'f', 3));
    doc->writeAttribute(KXMLTowerWidth,   QString::number(double(m_width),   'f', 3));
    doc->writeAttribute(KXMLTowerDepth,   QString::number(double(m_depth),   'f', 3));
    doc->writeAttribute(KXMLTowerHeight,  QString::number(double(m_height),  'f', 3));
    if (m_color.isValid())
        doc->writeAttribute(KXMLTowerColor, m_color.name());
    if (m_locked)
        doc->writeAttribute(KXMLTowerLocked, "true");
    if (m_layerId != 0)
        doc->writeAttribute(KXMLTowerLayerId, QString::number(m_layerId));
    if (m_groupId != 0)
        doc->writeAttribute(KXMLTowerGroupId, QString::number(m_groupId));
    if (m_rotation != 0.0f)
        doc->writeAttribute(KXMLTowerRotation, QString::number(double(m_rotation), 'f', 2));
    if (m_faces)
    {
        doc->writeAttribute(QStringLiteral("Faces"), QString::number(m_facesSides));
        doc->writeAttribute(QStringLiteral("FacesColour"), m_facesColour.name());
        doc->writeAttribute(QStringLiteral("FacesFrom"), QString::number(double(m_facesFrom), 'f', 3));
        if (m_facesTo >= 0.0f)
            doc->writeAttribute(QStringLiteral("FacesTo"), QString::number(double(m_facesTo), 'f', 3));
        if (!m_facesPattern.isEmpty())
            doc->writeAttribute(QStringLiteral("FacesPattern"), m_facesPattern);
        if (m_facesPatternOpacity < 1.0f)
            doc->writeAttribute(QStringLiteral("FacesPatternOpacity"),
                                QString::number(double(m_facesPatternOpacity), 'f', 2));
    }
    foreach (float z, m_shelves)
    {
        doc->writeStartElement(KXMLTowerShelf);
        doc->writeAttribute(KXMLTowerShelfZ, QString::number(double(z), 'f', 3));
        doc->writeEndElement();
    }
    doc->writeEndElement();
    return true;
}
