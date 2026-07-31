/*
  Q Light Controller Plus
  boom.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "boom.h"

#define KXMLBoom            QStringLiteral("Boom")
#define KXMLBoomID          QStringLiteral("ID")
#define KXMLBoomName        QStringLiteral("Name")
#define KXMLBoomOriginX     QStringLiteral("OriginX")
#define KXMLBoomOriginY     QStringLiteral("OriginY")
#define KXMLBoomBaseZ       QStringLiteral("BaseZ")
#define KXMLBoomHeight      QStringLiteral("Height")
#define KXMLBoomDiameter    QStringLiteral("Diameter")
#define KXMLBoomBaseRadius  QStringLiteral("BaseRadius")
#define KXMLBoomParentTruss QStringLiteral("ParentTruss")
#define KXMLBoomTrussOffset QStringLiteral("TrussOffset")
#define KXMLBoomColor       QStringLiteral("Color")
#define KXMLBoomLocked      QStringLiteral("Locked")
#define KXMLBoomLayerId     QStringLiteral("LayerId")
#define KXMLBoomGroupId     QStringLiteral("GroupId")

Boom::Boom(quint32 id, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_color(QColor(150, 150, 160, 220))
{
}

QVector3D Boom::positionAt(float offset) const
{
    return QVector3D(m_originX, m_originY, m_baseZ + offset);
}

bool Boom::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLBoom)
        return false;

    QXmlStreamAttributes a = root.attributes();
    if (a.hasAttribute(KXMLBoomID))         m_id      = a.value(KXMLBoomID).toUInt();
    if (a.hasAttribute(KXMLBoomName))       m_name    = a.value(KXMLBoomName).toString();
    if (a.hasAttribute(KXMLBoomOriginX))    m_originX = a.value(KXMLBoomOriginX).toFloat();
    if (a.hasAttribute(KXMLBoomOriginY))    m_originY = a.value(KXMLBoomOriginY).toFloat();
    if (a.hasAttribute(KXMLBoomBaseZ))      setBaseZ(a.value(KXMLBoomBaseZ).toFloat());
    if (a.hasAttribute(KXMLBoomHeight))     setHeight(a.value(KXMLBoomHeight).toFloat());
    if (a.hasAttribute(KXMLBoomDiameter))   setDiameter(a.value(KXMLBoomDiameter).toFloat());
    if (a.hasAttribute(KXMLBoomBaseRadius)) setBaseRadius(a.value(KXMLBoomBaseRadius).toFloat());
    if (a.hasAttribute(KXMLBoomParentTruss)) m_parentTrussId = a.value(KXMLBoomParentTruss).toUInt();
    if (a.hasAttribute(KXMLBoomTrussOffset)) m_trussOffset   = a.value(KXMLBoomTrussOffset).toFloat();
    if (a.hasAttribute(KXMLBoomColor))      m_color   = QColor(a.value(KXMLBoomColor).toString());
    if (a.hasAttribute(KXMLBoomLocked))     m_locked  = (a.value(KXMLBoomLocked).toString() == "true");
    if (a.hasAttribute(KXMLBoomLayerId))    m_layerId = a.value(KXMLBoomLayerId).toUInt();
    if (a.hasAttribute(KXMLBoomGroupId))    m_groupId = a.value(KXMLBoomGroupId).toUInt();

    root.skipCurrentElement();
    return true;
}

bool Boom::saveXML(QXmlStreamWriter *doc) const
{
    doc->writeStartElement(KXMLBoom);
    doc->writeAttribute(KXMLBoomID,         QString::number(m_id));
    doc->writeAttribute(KXMLBoomName,       m_name);
    doc->writeAttribute(KXMLBoomOriginX,    QString::number(double(m_originX),    'f', 3));
    doc->writeAttribute(KXMLBoomOriginY,    QString::number(double(m_originY),    'f', 3));
    if (m_baseZ != 0.0f)
        doc->writeAttribute(KXMLBoomBaseZ,  QString::number(double(m_baseZ),      'f', 3));
    doc->writeAttribute(KXMLBoomHeight,     QString::number(double(m_height),     'f', 3));
    doc->writeAttribute(KXMLBoomDiameter,   QString::number(double(m_diameter),   'f', 3));
    doc->writeAttribute(KXMLBoomBaseRadius, QString::number(double(m_baseRadius), 'f', 3));
    if (m_parentTrussId != UINT_MAX)
    {
        doc->writeAttribute(KXMLBoomParentTruss, QString::number(m_parentTrussId));
        doc->writeAttribute(KXMLBoomTrussOffset, QString::number(double(m_trussOffset), 'f', 3));
    }
    if (m_color.isValid())
        doc->writeAttribute(KXMLBoomColor, m_color.name());
    if (m_locked)
        doc->writeAttribute(KXMLBoomLocked, "true");
    if (m_layerId != 0)
        doc->writeAttribute(KXMLBoomLayerId, QString::number(m_layerId));
    if (m_groupId != 0)
        doc->writeAttribute(KXMLBoomGroupId, QString::number(m_groupId));
    doc->writeEndElement();
    return true;
}
