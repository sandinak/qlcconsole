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

Tower::Tower(quint32 id, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_color(QColor(90, 100, 120, 220))
{
}

void Tower::addShelf(float z)
{
    m_shelves.append(z);
    std::sort(m_shelves.begin(), m_shelves.end());
}

void Tower::removeShelf(int i)
{
    if (i >= 0 && i < m_shelves.size())
        m_shelves.removeAt(i);
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
    foreach (float z, m_shelves)
    {
        doc->writeStartElement(KXMLTowerShelf);
        doc->writeAttribute(KXMLTowerShelfZ, QString::number(double(z), 'f', 3));
        doc->writeEndElement();
    }
    doc->writeEndElement();
    return true;
}
