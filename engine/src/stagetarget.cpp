/*
  Q Light Controller Plus
  stagetarget.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <algorithm>
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
#define KXMLStageTargetRotation QStringLiteral("Rotation")
#define KXMLStageTargetLocked  QStringLiteral("Locked")
#define KXMLStageTargetLayerId QStringLiteral("LayerId")
#define KXMLStageTargetGroupId QStringLiteral("GroupId")
#define KXMLStageTargetKind      QStringLiteral("Kind")
#define KXMLStageTargetAimHeight QStringLiteral("AimHeight")
#define KXMLStageTargetPersonHeight QStringLiteral("PersonHeight")
#define KXMLStageTargetCostume   QStringLiteral("Costume")
#define KXMLStageTargetPeople    QStringLiteral("People")
#define KXMLStageTargetSpread    QStringLiteral("HeightSpread")
#define KXMLStageTargetSeed      QStringLiteral("Seed")
#define KXMLStageTargetBoundType QStringLiteral("BoundType")
#define KXMLStageTargetBoundId   QStringLiteral("BoundId")
#define KXMLStageTargetW         QStringLiteral("W")
#define KXMLStageTargetD         QStringLiteral("D")
#define KXMLStageTargetLinePoint QStringLiteral("LinePoint")
#define KXMLStageTargetBlocked   QStringLiteral("Blocked")
#define KXMLStageTargetSet       QStringLiteral("Set")
#define KXMLStageTargetDX        QStringLiteral("DX")
#define KXMLStageTargetDY        QStringLiteral("DY")
#define KXMLStageTargetDZ        QStringLiteral("DZ")

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
    if (a.hasAttribute(KXMLStageTargetPeople))
        setPerformerCount(a.value(KXMLStageTargetPeople).toInt());
    if (a.hasAttribute(KXMLStageTargetSpread))
        setHeightSpread(a.value(KXMLStageTargetSpread).toFloat());
    if (a.hasAttribute(KXMLStageTargetSeed))
        m_seed = a.value(KXMLStageTargetSeed).toUInt();
    if (a.hasAttribute(KXMLStageTargetBoundType))
        m_boundType = a.value(KXMLStageTargetBoundType).toString();
    if (a.hasAttribute(KXMLStageTargetBoundId))
        m_boundId = a.value(KXMLStageTargetBoundId).toUInt();
    if (a.hasAttribute(KXMLStageTargetRotation))
        m_rotation = a.value(KXMLStageTargetRotation).toFloat();
    if (a.hasAttribute(KXMLStageTargetW) || a.hasAttribute(KXMLStageTargetD))
        m_footprint = QSizeF(a.value(KXMLStageTargetW).toDouble(),
                             a.value(KXMLStageTargetD).toDouble());

    /* A line's points are child elements, relative to X/Y/Z. A build that
       predates lines skips them (and reads Kind="line" as a plain mark), so
       it sees a point at the line's centre. */
    QVector<QVector3D> pts;
    m_blocked.clear();
    while (root.readNextStartElement())
    {
        if (root.name() == KXMLStageTargetLinePoint)
        {
            const QXmlStreamAttributes pa = root.attributes();
            pts << QVector3D(pa.value(KXMLStageTargetX).toFloat(),
                             pa.value(KXMLStageTargetY).toFloat(),
                             pa.value(KXMLStageTargetZ).toFloat());
        }
        else if (root.name() == KXMLStageTargetBlocked)
        {
            /* Where it stands in a blocking set (docs/design/BLOCKING.md):
               an older build skips it and sees Default. */
            const QXmlStreamAttributes ba = root.attributes();
            const quint32 set = ba.value(KXMLStageTargetSet).toUInt();
            Blocked b;
            if (ba.hasAttribute(KXMLStageTargetDX) || ba.hasAttribute(KXMLStageTargetDY)
                || ba.hasAttribute(KXMLStageTargetDZ))
            {
                b.hasOffset = true;
                b.offset = QVector3D(ba.value(KXMLStageTargetDX).toFloat(),
                                     ba.value(KXMLStageTargetDY).toFloat(),
                                     ba.value(KXMLStageTargetDZ).toFloat());
            }
            if (ba.hasAttribute(KXMLStageTargetW) || ba.hasAttribute(KXMLStageTargetD))
            {
                b.hasFootprint = true;
                b.footprint = QSizeF(ba.value(KXMLStageTargetW).toDouble(),
                                     ba.value(KXMLStageTargetD).toDouble());
            }
            while (root.readNextStartElement())
            {
                if (root.name() == KXMLStageTargetLinePoint)
                {
                    const QXmlStreamAttributes pa = root.attributes();
                    b.linePoints << QVector3D(pa.value(KXMLStageTargetX).toFloat(),
                                              pa.value(KXMLStageTargetY).toFloat(),
                                              pa.value(KXMLStageTargetZ).toFloat());
                }
                root.skipCurrentElement();
            }
            b.hasLine = b.linePoints.size() >= 2;
            if (!b.hasLine)
                b.linePoints.clear();
            if (set != 0 && !b.isEmpty())
                m_blocked.insert(set, b);
            continue;       // its end element is read
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
    case Kind::Keyboard:  return QStringLiteral("keyboard");
    case Kind::GrandPiano:   return QStringLiteral("grandpiano");
    case Kind::UprightPiano: return QStringLiteral("uprightpiano");
    case Kind::Rack:      return QStringLiteral("rack");
    case Kind::Speaker:   return QStringLiteral("speaker");
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
    if (k == QLatin1String("keyboard"))  return Kind::Keyboard;
    if (k == QLatin1String("grandpiano")) return Kind::GrandPiano;
    if (k == QLatin1String("uprightpiano")) return Kind::UprightPiano;
    if (k == QLatin1String("rack"))      return Kind::Rack;
    if (k == QLatin1String("speaker"))   return Kind::Speaker;
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
    case Kind::Keyboard:     return 1.1f;   ///< the hands and face of the player
    case Kind::GrandPiano:   return 1.1f;
    case Kind::UprightPiano: return 1.2f;
    case Kind::Rack:         return 1.0f;
    case Kind::Speaker:      return 0.8f;   ///< the cone, on its casters
    case Kind::None:
    case Kind::Structure:
    case Kind::Area:
    case Kind::Line:                    ///< its points carry their own heights
    default:            return 0.0f;    ///< the surface itself
    }
}

QSizeF StageTarget::defaultFootprint(Kind k)
{
    switch (k)
    {
    case Kind::Keyboard:     return QSizeF(1.4, 0.5);
    case Kind::GrandPiano:   return QSizeF(1.5, 1.8);
    case Kind::UprightPiano: return QSizeF(1.5, 0.6);
    case Kind::Rack:         return QSizeF(0.6, 0.6);
    case Kind::Speaker:      return QSizeF(0.55, 0.5);
    default:                 return QSizeF();
    }
}

float StageTarget::bodyHeight(Kind k)
{
    switch (k)
    {
    case Kind::Keyboard:     return 0.95f;  ///< the keybed on its stand
    case Kind::GrandPiano:   return 1.0f;   ///< the case; the lid goes higher
    case Kind::UprightPiano: return 1.3f;
    case Kind::Rack:         return 1.2f;
    case Kind::Speaker:      return 1.1f;   ///< 1.0 m box on 0.1 m casters
    default:                 return 0.0f;
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
    /* People on a line: an older build reads it as the plain line it is. */
    if (m_performerCount > 0)
    {
        doc->writeAttribute(KXMLStageTargetPeople, QString::number(m_performerCount));
        if (m_heightSpread > 0.0f)
            doc->writeAttribute(KXMLStageTargetSpread, QString::number(double(m_heightSpread), 'f', 3));
        doc->writeAttribute(KXMLStageTargetSeed, QString::number(m_seed));
    }
    if (isBound())
    {
        doc->writeAttribute(KXMLStageTargetBoundType, m_boundType);
        doc->writeAttribute(KXMLStageTargetBoundId, QString::number(m_boundId));
    }
    if (!qFuzzyIsNull(m_rotation))
        doc->writeAttribute(KXMLStageTargetRotation, QString::number(double(m_rotation), 'f', 2));
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
    /* Blocking overrides, by set id: in id order, so a save is stable. */
    QList<quint32> sets = blockedSets();
    std::sort(sets.begin(), sets.end());
    for (quint32 set : sets)
    {
        const Blocked b = blocked(set);
        doc->writeStartElement(KXMLStageTargetBlocked);
        doc->writeAttribute(KXMLStageTargetSet, QString::number(set));
        if (b.hasOffset)
        {
            doc->writeAttribute(KXMLStageTargetDX, QString::number(double(b.offset.x()), 'f', 3));
            doc->writeAttribute(KXMLStageTargetDY, QString::number(double(b.offset.y()), 'f', 3));
            doc->writeAttribute(KXMLStageTargetDZ, QString::number(double(b.offset.z()), 'f', 3));
        }
        if (b.hasFootprint)
        {
            doc->writeAttribute(KXMLStageTargetW, QString::number(b.footprint.width(), 'f', 3));
            doc->writeAttribute(KXMLStageTargetD, QString::number(b.footprint.height(), 'f', 3));
        }
        if (b.hasLine)
            for (const QVector3D &p : b.linePoints)
            {
                doc->writeStartElement(KXMLStageTargetLinePoint);
                doc->writeAttribute(KXMLStageTargetX, QString::number(double(p.x()), 'f', 3));
                doc->writeAttribute(KXMLStageTargetY, QString::number(double(p.y()), 'f', 3));
                doc->writeAttribute(KXMLStageTargetZ, QString::number(double(p.z()), 'f', 3));
                doc->writeEndElement();
            }
        doc->writeEndElement();
    }
    doc->writeEndElement();
    return true;
}

/****************************************************************************
 * Blocking
 ****************************************************************************/

StageTarget::Blocked StageTarget::blocked(quint32 set) const
{
    QMutexLocker l(&m_positionMutex);
    return m_blocked.value(set);
}

void StageTarget::setBlocked(quint32 set, const Blocked &b)
{
    if (set == 0)
        return;     // Default is the target itself
    QMutexLocker l(&m_positionMutex);
    if (b.isEmpty())
        m_blocked.remove(set);
    else
        m_blocked.insert(set, b);
}

bool StageTarget::isBlockedIn(quint32 set) const
{
    QMutexLocker l(&m_positionMutex);
    return m_blocked.contains(set);
}

QList<quint32> StageTarget::blockedSets() const
{
    QMutexLocker l(&m_positionMutex);
    return m_blocked.keys();
}

QVector3D StageTarget::positionIn(quint32 set) const
{
    QMutexLocker l(&m_positionMutex);
    if (set != 0 && !m_blocked.isEmpty())
    {
        auto it = m_blocked.constFind(set);
        if (it != m_blocked.constEnd() && it->hasOffset)
            return m_position + it->offset;
    }
    return m_position;
}

QVector<QVector3D> StageTarget::linePointsIn(quint32 set) const
{
    QMutexLocker l(&m_positionMutex);
    if (set != 0 && !m_blocked.isEmpty())
    {
        auto it = m_blocked.constFind(set);
        if (it != m_blocked.constEnd() && it->hasLine)
            return it->linePoints;
    }
    return m_linePoints;
}

QSizeF StageTarget::footprintIn(quint32 set) const
{
    QMutexLocker l(&m_positionMutex);
    if (set != 0 && !m_blocked.isEmpty())
    {
        auto it = m_blocked.constFind(set);
        if (it != m_blocked.constEnd() && it->hasFootprint)
            return it->footprint;
    }
    return m_footprint;
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
