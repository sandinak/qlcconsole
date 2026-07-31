/*
  Q Light Controller Plus
  pipe.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "pipe.h"

#define KXMLPipe            QStringLiteral("Pipe")
#define KXMLPipeLegacyBoom  QStringLiteral("Boom")   // pre-unify name
#define KXMLPipeID          QStringLiteral("ID")
#define KXMLPipeName        QStringLiteral("Name")
#define KXMLPipeOrient      QStringLiteral("Orientation")
#define KXMLPipeOriginX     QStringLiteral("OriginX")
#define KXMLPipeOriginY     QStringLiteral("OriginY")
#define KXMLPipeBaseZ       QStringLiteral("BaseZ")
#define KXMLPipeLength      QStringLiteral("Length")
#define KXMLPipeLegacyHt    QStringLiteral("Height")  // boom stored "Height"
#define KXMLPipeRunAngle    QStringLiteral("RunAngle")
#define KXMLPipeDiameter    QStringLiteral("Diameter")
#define KXMLPipeBaseRadius  QStringLiteral("BaseRadius")
#define KXMLPipeParentTruss QStringLiteral("ParentTruss")
#define KXMLPipeTrussOffset QStringLiteral("TrussOffset")
#define KXMLPipeStand       QStringLiteral("Stand")
#define KXMLPipeStandOffset QStringLiteral("StandOffset")
#define KXMLPipeParentPipe  QStringLiteral("ParentPipe")
#define KXMLPipeParentPipeOfs QStringLiteral("ParentPipeOffset")
#define KXMLPipeColor       QStringLiteral("Color")
#define KXMLPipeLocked      QStringLiteral("Locked")
#define KXMLPipeLayerId     QStringLiteral("LayerId")
#define KXMLPipeGroupId     QStringLiteral("GroupId")

Pipe::Pipe(quint32 id, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_color(QColor(150, 150, 160, 220))
{
}

bool Pipe::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLPipe && root.name() != KXMLPipeLegacyBoom)
        return false;

    QXmlStreamAttributes a = root.attributes();
    if (a.hasAttribute(KXMLPipeID))         m_id      = a.value(KXMLPipeID).toUInt();
    if (a.hasAttribute(KXMLPipeName))       m_name    = a.value(KXMLPipeName).toString();
    if (a.hasAttribute(KXMLPipeOrient))     m_orientation = Orientation(a.value(KXMLPipeOrient).toInt());
    if (a.hasAttribute(KXMLPipeOriginX))    m_originX = a.value(KXMLPipeOriginX).toFloat();
    if (a.hasAttribute(KXMLPipeOriginY))    m_originY = a.value(KXMLPipeOriginY).toFloat();
    if (a.hasAttribute(KXMLPipeBaseZ))      setBaseZ(a.value(KXMLPipeBaseZ).toFloat());
    if (a.hasAttribute(KXMLPipeLength))     setLength(a.value(KXMLPipeLength).toFloat());
    else if (a.hasAttribute(KXMLPipeLegacyHt)) setLength(a.value(KXMLPipeLegacyHt).toFloat());
    if (a.hasAttribute(KXMLPipeRunAngle))   m_runAngle = a.value(KXMLPipeRunAngle).toFloat();
    if (a.hasAttribute(KXMLPipeDiameter))   setDiameter(a.value(KXMLPipeDiameter).toFloat());
    if (a.hasAttribute(KXMLPipeBaseRadius)) setBaseRadius(a.value(KXMLPipeBaseRadius).toFloat());
    if (a.hasAttribute(KXMLPipeParentTruss)) m_parentTrussId = a.value(KXMLPipeParentTruss).toUInt();
    if (a.hasAttribute(KXMLPipeTrussOffset)) m_trussOffset   = a.value(KXMLPipeTrussOffset).toFloat();
    if (a.hasAttribute(KXMLPipeStand))      m_standId = a.value(KXMLPipeStand).toUInt();
    if (a.hasAttribute(KXMLPipeStandOffset)) m_standOffset = a.value(KXMLPipeStandOffset).toFloat();
    if (a.hasAttribute(KXMLPipeParentPipe)) m_parentPipeId = a.value(KXMLPipeParentPipe).toUInt();
    if (a.hasAttribute(KXMLPipeParentPipeOfs)) m_parentPipeOffset = a.value(KXMLPipeParentPipeOfs).toFloat();
    if (a.hasAttribute(KXMLPipeColor))      m_color   = QColor(a.value(KXMLPipeColor).toString());
    if (a.hasAttribute(KXMLPipeLocked))     m_locked  = (a.value(KXMLPipeLocked).toString() == "true");
    if (a.hasAttribute(KXMLPipeLayerId))    m_layerId = a.value(KXMLPipeLayerId).toUInt();
    if (a.hasAttribute(KXMLPipeGroupId))    m_groupId = a.value(KXMLPipeGroupId).toUInt();

    root.skipCurrentElement();
    return true;
}

bool Pipe::saveXML(QXmlStreamWriter *doc) const
{
    doc->writeStartElement(KXMLPipe);
    doc->writeAttribute(KXMLPipeID,         QString::number(m_id));
    doc->writeAttribute(KXMLPipeName,       m_name);
    doc->writeAttribute(KXMLPipeOrient,     QString::number(int(m_orientation)));
    doc->writeAttribute(KXMLPipeOriginX,    QString::number(double(m_originX),    'f', 3));
    doc->writeAttribute(KXMLPipeOriginY,    QString::number(double(m_originY),    'f', 3));
    if (m_baseZ != 0.0f)
        doc->writeAttribute(KXMLPipeBaseZ,  QString::number(double(m_baseZ),      'f', 3));
    doc->writeAttribute(KXMLPipeLength,     QString::number(double(m_length),     'f', 3));
    if (m_orientation == Horizontal)
        doc->writeAttribute(KXMLPipeRunAngle, QString::number(double(m_runAngle), 'f', 1));
    doc->writeAttribute(KXMLPipeDiameter,   QString::number(double(m_diameter),   'f', 3));
    doc->writeAttribute(KXMLPipeBaseRadius, QString::number(double(m_baseRadius), 'f', 3));
    if (m_parentTrussId != UINT_MAX)
    {
        doc->writeAttribute(KXMLPipeParentTruss, QString::number(m_parentTrussId));
        doc->writeAttribute(KXMLPipeTrussOffset, QString::number(double(m_trussOffset), 'f', 3));
    }
    if (m_standId != UINT_MAX)
    {
        doc->writeAttribute(KXMLPipeStand, QString::number(m_standId));
        if (m_standOffset >= 0.0f)
            doc->writeAttribute(KXMLPipeStandOffset, QString::number(double(m_standOffset), 'f', 3));
    }
    if (m_parentPipeId != UINT_MAX)
    {
        doc->writeAttribute(KXMLPipeParentPipe, QString::number(m_parentPipeId));
        doc->writeAttribute(KXMLPipeParentPipeOfs, QString::number(double(m_parentPipeOffset), 'f', 3));
    }
    if (m_color.isValid())
        doc->writeAttribute(KXMLPipeColor, m_color.name());
    if (m_locked)
        doc->writeAttribute(KXMLPipeLocked, "true");
    if (m_layerId != 0)
        doc->writeAttribute(KXMLPipeLayerId, QString::number(m_layerId));
    if (m_groupId != 0)
        doc->writeAttribute(KXMLPipeGroupId, QString::number(m_groupId));
    doc->writeEndElement();
    return true;
}
