/*
  Q Light Controller Plus
  stagetarget.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "stagetarget.h"

#define KXMLStageTarget        QStringLiteral("StageTarget")
#define KXMLStageTargetID      QStringLiteral("ID")
#define KXMLStageTargetName    QStringLiteral("Name")
#define KXMLStageTargetX       QStringLiteral("X")
#define KXMLStageTargetY       QStringLiteral("Y")
#define KXMLStageTargetZ       QStringLiteral("Z")
#define KXMLStageTargetColor   QStringLiteral("Color")

StageTarget::StageTarget(quint32 id, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_position(0.0f, 0.0f, 0.0f)
    , m_color(QColor(255, 180, 0, 220))  // amber
{
}

bool StageTarget::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLStageTarget)
        return false;

    QXmlStreamAttributes a = root.attributes();
    // Restore the PERSISTED id. saveXML() writes it, and Aim palettes /
    // followspot bindings reference targets by it — so if it is not read back,
    // the loader's sequentially-assigned id silently re-numbers the targets and
    // every stored reference points at the wrong target (or none). Files older
    // than the ID attribute fall back to the id the caller assigned.
    if (a.hasAttribute(KXMLStageTargetID))    m_id   = a.value(KXMLStageTargetID).toUInt();
    if (a.hasAttribute(KXMLStageTargetName))  m_name = a.value(KXMLStageTargetName).toString();
    if (a.hasAttribute(KXMLStageTargetX))     m_position.setX(a.value(KXMLStageTargetX).toFloat());
    if (a.hasAttribute(KXMLStageTargetY))     m_position.setY(a.value(KXMLStageTargetY).toFloat());
    if (a.hasAttribute(KXMLStageTargetZ))     m_position.setZ(a.value(KXMLStageTargetZ).toFloat());
    if (a.hasAttribute(KXMLStageTargetColor)) m_color = QColor(a.value(KXMLStageTargetColor).toString());

    root.skipCurrentElement();
    return true;
}

bool StageTarget::saveXML(QXmlStreamWriter *doc) const
{
    doc->writeStartElement(KXMLStageTarget);
    doc->writeAttribute(KXMLStageTargetID,    QString::number(m_id));
    doc->writeAttribute(KXMLStageTargetName,  m_name);
    doc->writeAttribute(KXMLStageTargetX,     QString::number(double(m_position.x()), 'f', 3));
    doc->writeAttribute(KXMLStageTargetY,     QString::number(double(m_position.y()), 'f', 3));
    doc->writeAttribute(KXMLStageTargetZ,     QString::number(double(m_position.z()), 'f', 3));
    if (m_color.isValid())
        doc->writeAttribute(KXMLStageTargetColor, m_color.name());
    doc->writeEndElement();
    return true;
}
