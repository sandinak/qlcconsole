/*
  Q Light Controller Plus
  stand.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "stand.h"

#define KXMLStand           QStringLiteral("Stand")
#define KXMLStandID         QStringLiteral("ID")
#define KXMLStandName       QStringLiteral("Name")
#define KXMLStandOriginX    QStringLiteral("OriginX")
#define KXMLStandOriginY    QStringLiteral("OriginY")
#define KXMLStandHeight     QStringLiteral("Height")
#define KXMLStandBaseRadius QStringLiteral("BaseRadius")
#define KXMLStandColor      QStringLiteral("Color")
#define KXMLStandLocked     QStringLiteral("Locked")
#define KXMLStandLayerId    QStringLiteral("LayerId")
#define KXMLStandGroupId    QStringLiteral("GroupId")

Stand::Stand(quint32 id, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_color(QColor(120, 120, 130, 220))
{
}

bool Stand::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLStand)
        return false;

    QXmlStreamAttributes a = root.attributes();
    if (a.hasAttribute(KXMLStandID))         m_id      = a.value(KXMLStandID).toUInt();
    if (a.hasAttribute(KXMLStandName))       m_name    = a.value(KXMLStandName).toString();
    if (a.hasAttribute(KXMLStandOriginX))    m_originX = a.value(KXMLStandOriginX).toFloat();
    if (a.hasAttribute(KXMLStandOriginY))    m_originY = a.value(KXMLStandOriginY).toFloat();
    if (a.hasAttribute(KXMLStandHeight))     setHeight(a.value(KXMLStandHeight).toFloat());
    if (a.hasAttribute(KXMLStandBaseRadius)) setBaseRadius(a.value(KXMLStandBaseRadius).toFloat());
    if (a.hasAttribute(KXMLStandColor))      m_color   = QColor(a.value(KXMLStandColor).toString());
    if (a.hasAttribute(KXMLStandLocked))     m_locked  = (a.value(KXMLStandLocked).toString() == "true");
    if (a.hasAttribute(KXMLStandLayerId))    m_layerId = a.value(KXMLStandLayerId).toUInt();
    if (a.hasAttribute(KXMLStandGroupId))    m_groupId = a.value(KXMLStandGroupId).toUInt();

    root.skipCurrentElement();
    return true;
}

bool Stand::saveXML(QXmlStreamWriter *doc) const
{
    doc->writeStartElement(KXMLStand);
    doc->writeAttribute(KXMLStandID,         QString::number(m_id));
    doc->writeAttribute(KXMLStandName,       m_name);
    doc->writeAttribute(KXMLStandOriginX,    QString::number(double(m_originX),    'f', 3));
    doc->writeAttribute(KXMLStandOriginY,    QString::number(double(m_originY),    'f', 3));
    doc->writeAttribute(KXMLStandHeight,     QString::number(double(m_height),     'f', 3));
    doc->writeAttribute(KXMLStandBaseRadius, QString::number(double(m_baseRadius), 'f', 3));
    if (m_color.isValid())
        doc->writeAttribute(KXMLStandColor, m_color.name());
    if (m_locked)
        doc->writeAttribute(KXMLStandLocked, "true");
    if (m_layerId != 0)
        doc->writeAttribute(KXMLStandLayerId, QString::number(m_layerId));
    if (m_groupId != 0)
        doc->writeAttribute(KXMLStandGroupId, QString::number(m_groupId));
    doc->writeEndElement();
    return true;
}
