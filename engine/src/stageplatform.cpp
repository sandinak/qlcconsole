/*
  Q Light Controller Plus
  stageplatform.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "stageplatform.h"

#define KXMLPlatform        QStringLiteral("StagePlatform")
#define KXMLPlatformID      QStringLiteral("ID")
#define KXMLPlatformName    QStringLiteral("Name")
#define KXMLPlatformOriginX QStringLiteral("OriginX")
#define KXMLPlatformOriginY QStringLiteral("OriginY")
#define KXMLPlatformWidth   QStringLiteral("Width")
#define KXMLPlatformDepth   QStringLiteral("Depth")
#define KXMLPlatformHeight  QStringLiteral("Height")
#define KXMLPlatformColor   QStringLiteral("Color")

StagePlatform::StagePlatform(quint32 id, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_color(QColor(80, 120, 200, 180))
{
}

bool StagePlatform::containsPoint(float x, float y) const
{
    return x >= m_originX && x <= m_originX + m_width
        && y >= m_originY && y <= m_originY + m_depth;
}

bool StagePlatform::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLPlatform)
        return false;

    QXmlStreamAttributes a = root.attributes();
    if (a.hasAttribute(KXMLPlatformName))   m_name    = a.value(KXMLPlatformName).toString();
    if (a.hasAttribute(KXMLPlatformOriginX)) m_originX = a.value(KXMLPlatformOriginX).toFloat();
    if (a.hasAttribute(KXMLPlatformOriginY)) m_originY = a.value(KXMLPlatformOriginY).toFloat();
    if (a.hasAttribute(KXMLPlatformWidth))   setWidth(a.value(KXMLPlatformWidth).toFloat());
    if (a.hasAttribute(KXMLPlatformDepth))   setDepth(a.value(KXMLPlatformDepth).toFloat());
    if (a.hasAttribute(KXMLPlatformHeight))  setHeight(a.value(KXMLPlatformHeight).toFloat());
    if (a.hasAttribute(KXMLPlatformColor))   m_color = QColor(a.value(KXMLPlatformColor).toString());

    root.skipCurrentElement();
    return true;
}

bool StagePlatform::saveXML(QXmlStreamWriter *doc) const
{
    doc->writeStartElement(KXMLPlatform);
    doc->writeAttribute(KXMLPlatformID,      QString::number(m_id));
    doc->writeAttribute(KXMLPlatformName,    m_name);
    doc->writeAttribute(KXMLPlatformOriginX, QString::number(double(m_originX), 'f', 3));
    doc->writeAttribute(KXMLPlatformOriginY, QString::number(double(m_originY), 'f', 3));
    doc->writeAttribute(KXMLPlatformWidth,   QString::number(double(m_width),   'f', 3));
    doc->writeAttribute(KXMLPlatformDepth,   QString::number(double(m_depth),   'f', 3));
    doc->writeAttribute(KXMLPlatformHeight,  QString::number(double(m_height),  'f', 3));
    if (m_color.isValid())
        doc->writeAttribute(KXMLPlatformColor, m_color.name());
    doc->writeEndElement();
    return true;
}
