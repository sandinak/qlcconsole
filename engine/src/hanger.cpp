/*
  Q Light Controller Plus
  hanger.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QtMath>

#include "hanger.h"

#define KXMLHanger          QStringLiteral("Hanger")
#define KXMLHangerID        QStringLiteral("ID")
#define KXMLHangerName      QStringLiteral("Name")
#define KXMLHangerParent    QStringLiteral("Parent")
#define KXMLHangerParentId  QStringLiteral("ParentId")
#define KXMLHangerAlong     QStringLiteral("Along")
#define KXMLHangerAngle     QStringLiteral("Angle")
#define KXMLHangerArm       QStringLiteral("Arm")
#define KXMLHangerDrop      QStringLiteral("Drop")
#define KXMLHangerLayerId   QStringLiteral("LayerId")
#define KXMLHangerLocked    QStringLiteral("Locked")

Hanger::Hanger(quint32 id, QObject *parent)
    : QObject(parent)
    , m_id(id)
{
}

QVector3D Hanger::direction() const
{
    const double a = qDegreesToRadians(double(m_angle));
    return QVector3D(float(qSin(a)), float(qCos(a)), 0.0f);
}

bool Hanger::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLHanger)
        return false;
    const QXmlStreamAttributes a = root.attributes();
    m_name = a.value(KXMLHangerName).toString();
    m_parentKind = (a.value(KXMLHangerParent).toString() == QLatin1String("tower")) ? OnTower : OnTruss;
    m_parentId = a.hasAttribute(KXMLHangerParentId) ? a.value(KXMLHangerParentId).toUInt() : UINT_MAX;
    m_along = a.value(KXMLHangerAlong).toFloat();
    m_angle = a.value(KXMLHangerAngle).toFloat();
    m_arm   = qMax(0.0f, a.value(KXMLHangerArm).toFloat());
    m_drop  = qMax(0.0f, a.value(KXMLHangerDrop).toFloat());
    m_layerId = a.value(KXMLHangerLayerId).toUInt();
    m_locked = a.value(KXMLHangerLocked).toString() == QLatin1String("1");
    root.skipCurrentElement();
    return true;
}

bool Hanger::saveXML(QXmlStreamWriter *doc) const
{
    doc->writeStartElement(KXMLHanger);
    doc->writeAttribute(KXMLHangerID, QString::number(m_id));
    doc->writeAttribute(KXMLHangerName, m_name);
    doc->writeAttribute(KXMLHangerParent, m_parentKind == OnTower ? QStringLiteral("tower")
                                                                   : QStringLiteral("truss"));
    doc->writeAttribute(KXMLHangerParentId, QString::number(m_parentId));
    doc->writeAttribute(KXMLHangerAlong, QString::number(double(m_along), 'f', 3));
    doc->writeAttribute(KXMLHangerAngle, QString::number(double(m_angle), 'f', 1));
    doc->writeAttribute(KXMLHangerArm,   QString::number(double(m_arm),   'f', 3));
    doc->writeAttribute(KXMLHangerDrop,  QString::number(double(m_drop),  'f', 3));
    if (m_layerId != 0)
        doc->writeAttribute(KXMLHangerLayerId, QString::number(m_layerId));
    if (m_locked)
        doc->writeAttribute(KXMLHangerLocked, QStringLiteral("1"));
    doc->writeEndElement();
    return true;
}
