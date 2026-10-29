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
#include "linecurve.h"

#define KXMLStageTarget        QStringLiteral("StageTarget")
#define KXMLStageTargetID      QStringLiteral("ID")
#define KXMLStageTargetName    QStringLiteral("Name")
#define KXMLStageTargetX       QStringLiteral("X")
#define KXMLStageTargetY       QStringLiteral("Y")
#define KXMLStageTargetZ       QStringLiteral("Z")
#define KXMLStageTargetColor   QStringLiteral("Color")
#define KXMLStageTargetLocked  QStringLiteral("Locked")
#define KXMLStageTargetLayerId QStringLiteral("LayerId")
#define KXMLStageTargetGroupId QStringLiteral("GroupId")
#define KXMLStageTargetKind      QStringLiteral("Kind")
#define KXMLStageTargetAimHeight QStringLiteral("AimHeight")
#define KXMLStageTargetPersonHeight QStringLiteral("PersonHeight")
#define KXMLStageTargetCostume   QStringLiteral("Costume")
#define KXMLStageTargetBoundType QStringLiteral("BoundType")
#define KXMLStageTargetBoundId   QStringLiteral("BoundId")
#define KXMLStageTargetW         QStringLiteral("W")
#define KXMLStageTargetD         QStringLiteral("D")
#define KXMLStageTargetLinePoint QStringLiteral("LinePoint")

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
    if (a.hasAttribute(KXMLStageTargetLocked)) m_locked = (a.value(KXMLStageTargetLocked).toString() == "true");
    if (a.hasAttribute(KXMLStageTargetLayerId)) m_layerId = a.value(KXMLStageTargetLayerId).toUInt();
    if (a.hasAttribute(KXMLStageTargetGroupId)) m_groupId = a.value(KXMLStageTargetGroupId).toUInt();
    if (a.hasAttribute(KXMLStageTargetKind))
        m_kind = stringToKind(a.value(KXMLStageTargetKind).toString());
    if (a.hasAttribute(KXMLStageTargetAimHeight))
        m_aimHeightOverride = a.value(KXMLStageTargetAimHeight).toFloat();
    if (a.hasAttribute(KXMLStageTargetPersonHeight))
        m_personHeight = a.value(KXMLStageTargetPersonHeight).toFloat();
    if (a.hasAttribute(KXMLStageTargetCostume))
        m_costume = QColor(a.value(KXMLStageTargetCostume).toString());
    if (a.hasAttribute(KXMLStageTargetBoundType))
        m_boundType = a.value(KXMLStageTargetBoundType).toString();
    if (a.hasAttribute(KXMLStageTargetBoundId))
        m_boundId = a.value(KXMLStageTargetBoundId).toUInt();
    if (a.hasAttribute(KXMLStageTargetW) || a.hasAttribute(KXMLStageTargetD))
        m_footprint = QSizeF(a.value(KXMLStageTargetW).toDouble(),
                             a.value(KXMLStageTargetD).toDouble());

    /* A line's points are child elements, relative to X/Y/Z. A build that
       predates lines skips them (and reads Kind="line" as a plain mark), so
       it sees a point at the line's centre. */
    QVector<QVector3D> pts;
    while (root.readNextStartElement())
    {
        if (root.name() == KXMLStageTargetLinePoint)
        {
            const QXmlStreamAttributes pa = root.attributes();
            pts << QVector3D(pa.value(KXMLStageTargetX).toFloat(),
                             pa.value(KXMLStageTargetY).toFloat(),
                             pa.value(KXMLStageTargetZ).toFloat());
        }
        root.skipCurrentElement();
    }
    if (m_kind == Kind::Line)
        m_linePoints = pts.size() >= 2 ? pts : defaultLinePoints();
    return true;
}

QString StageTarget::kindToString(Kind k)
{
    switch (k)
    {
    case Kind::Person:    return QStringLiteral("person");
    case Kind::DrumKit:   return QStringLiteral("drumkit");
    case Kind::Structure: return QStringLiteral("structure");
    case Kind::Area:      return QStringLiteral("area");
    case Kind::Line:      return QStringLiteral("line");
    case Kind::None:
    default:              return QStringLiteral("none");
    }
}

StageTarget::Kind StageTarget::stringToKind(const QString &s, Kind fallback)
{
    const QString k = s.trimmed().toLower();
    if (k == QLatin1String("person"))    return Kind::Person;
    if (k == QLatin1String("drumkit"))   return Kind::DrumKit;
    if (k == QLatin1String("structure")) return Kind::Structure;
    if (k == QLatin1String("area"))      return Kind::Area;
    if (k == QLatin1String("line"))      return Kind::Line;
    if (k == QLatin1String("none"))      return Kind::None;
    /* Anything else came from a newer file than this build. Degrade to the
       fallback rather than guessing -- the whole reason these are names and not
       ordinals is so an unknown one is recognisably unknown. */
    return fallback;
}

float StageTarget::defaultAimHeight(Kind k)
{
    switch (k)
    {
    case Kind::Person:  return 1.4f;    ///< chest, not feet
    case Kind::DrumKit: return 1.0f;    ///< around the rims and cymbals
    case Kind::None:
    case Kind::Structure:
    case Kind::Area:
    case Kind::Line:                    ///< its points carry their own heights
    default:            return 0.0f;    ///< the surface itself
    }
}

bool StageTarget::saveXML(QXmlStreamWriter *doc) const
{
    doc->writeStartElement(KXMLStageTarget);
    doc->writeAttribute(KXMLStageTargetID,    QString::number(m_id));
    doc->writeAttribute(KXMLStageTargetName,  m_name);
    // A venue's position is the venue's (Q128): the show keeps its own.
    const QVector3D saved = m_hasBase ? m_base : position();
    doc->writeAttribute(KXMLStageTargetX,     QString::number(double(saved.x()), 'f', 3));
    doc->writeAttribute(KXMLStageTargetY,     QString::number(double(saved.y()), 'f', 3));
    doc->writeAttribute(KXMLStageTargetZ,     QString::number(double(saved.z()), 'f', 3));
    if (m_color.isValid())
        doc->writeAttribute(KXMLStageTargetColor, m_color.name());
    if (m_locked)
        doc->writeAttribute(KXMLStageTargetLocked, "true");
    if (m_layerId != 0)
        doc->writeAttribute(KXMLStageTargetLayerId, QString::number(m_layerId));
    if (m_groupId != 0)
        doc->writeAttribute(KXMLStageTargetGroupId, QString::number(m_groupId));
    /* Only written when they say something. A file full of Kind="none" and
       zero footprints is noise, and an older build reading this one should see
       exactly the target it used to. */
    if (m_kind != Kind::None)
        doc->writeAttribute(KXMLStageTargetKind, kindToString(m_kind));
    if (hasAimHeightOverride())
        doc->writeAttribute(KXMLStageTargetAimHeight,
                            QString::number(double(m_aimHeightOverride), 'f', 3));
    if (hasPersonHeight())
        doc->writeAttribute(KXMLStageTargetPersonHeight,
                            QString::number(double(m_personHeight), 'f', 3));
    if (m_costume.isValid())
        doc->writeAttribute(KXMLStageTargetCostume, m_costume.name());
    if (isBound())
    {
        doc->writeAttribute(KXMLStageTargetBoundType, m_boundType);
        doc->writeAttribute(KXMLStageTargetBoundId, QString::number(m_boundId));
    }
    if (m_footprint.isEmpty() == false)
    {
        doc->writeAttribute(KXMLStageTargetW,
                            QString::number(m_footprint.width(), 'f', 3));
        doc->writeAttribute(KXMLStageTargetD,
                            QString::number(m_footprint.height(), 'f', 3));
    }
    if (m_kind == Kind::Line)
    {
        foreach (const QVector3D &p, linePoints())
        {
            doc->writeStartElement(KXMLStageTargetLinePoint);
            doc->writeAttribute(KXMLStageTargetX, QString::number(double(p.x()), 'f', 3));
            doc->writeAttribute(KXMLStageTargetY, QString::number(double(p.y()), 'f', 3));
            doc->writeAttribute(KXMLStageTargetZ, QString::number(double(p.z()), 'f', 3));
            doc->writeEndElement();
        }
    }
    doc->writeEndElement();
    return true;
}

QVector<QVector3D> StageTarget::linePointsAround(const QVector3D &origin) const
{
    QVector<QVector3D> pts = linePoints();
    for (QVector3D &p : pts)
        p += origin;
    return pts;
}

void StageTarget::recentreLine()
{
    QMutexLocker l(&m_positionMutex);
    if (m_linePoints.size() < 2)
        return;
    QVector3D shift;
    m_linePoints = LineCurve::recentred(m_linePoints, &shift);
    m_position += shift;
}

QVector<QVector3D> StageTarget::defaultLinePoints()
{
    return { QVector3D(-2.0f, 0.0f, 0.0f), QVector3D(2.0f, 0.0f, 0.0f) };
}

void StageTarget::makeLine()
{
    m_kind = Kind::Line;
    QMutexLocker l(&m_positionMutex);
    if (m_linePoints.size() < 2)
        m_linePoints = defaultLinePoints();
}
