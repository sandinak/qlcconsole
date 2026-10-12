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

#include <QtMath>
#include <cmath>

#include "stageplatform.h"

#define KXMLPlatform        QStringLiteral("StagePlatform")
#define KXMLPlatformID      QStringLiteral("ID")
#define KXMLPlatformName    QStringLiteral("Name")
#define KXMLPlatformOriginX QStringLiteral("OriginX")
#define KXMLPlatformOriginY QStringLiteral("OriginY")
#define KXMLPlatformWidth   QStringLiteral("Width")
#define KXMLPlatformDepth   QStringLiteral("Depth")
#define KXMLPlatformHeight  QStringLiteral("Height")
#define KXMLPlatformTopMat  QStringLiteral("TopMaterial")
#define KXMLPlatformColor   QStringLiteral("Color")
#define KXMLPlatformLocked  QStringLiteral("Locked")
#define KXMLPlatformSolid     QStringLiteral("Solid")
#define KXMLPlatformStackable QStringLiteral("Stackable")
#define KXMLPlatformLayerId QStringLiteral("LayerId")
#define KXMLPlatformGroupId QStringLiteral("GroupId")
#define KXMLPlatformSteps   QStringLiteral("Steps")
#define KXMLPlatformRise    QStringLiteral("StepRise")
#define KXMLPlatformRun     QStringLiteral("StepRun")
#define KXMLPlatformClimb   QStringLiteral("StepClimb")
#define KXMLPlatformFrontCurve QStringLiteral("FrontCurve")
#define KXMLPlatformRotation QStringLiteral("Rotation")

StagePlatform::StagePlatform(quint32 id, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_color(QColor(80, 120, 200, 180))
{
}

void StagePlatform::setRotation(float deg)
{
    // -180 .. 180, so a turn round and back reads as no turn at all.
    double d = std::fmod(double(deg), 360.0);
    if (d > 180.0) d -= 360.0;
    if (d <= -180.0) d += 360.0;
    m_rotation = (qAbs(d) < 1e-4) ? 0.0f : float(d);
}

QPointF StagePlatform::centre() const
{
    return QPointF(double(m_originX) + double(m_width) / 2.0,
                   double(m_originY) + double(m_depth) / 2.0);
}

static QPointF turn(const QPointF &p, const QPointF &about, double deg)
{
    if (deg == 0.0)
        return p;
    const double r = qDegreesToRadians(deg);
    const double c = qCos(r), s = qSin(r);
    const double dx = p.x() - about.x(), dy = p.y() - about.y();
    return QPointF(about.x() + dx * c - dy * s, about.y() + dx * s + dy * c);
}

QPointF StagePlatform::toWorld(const QPointF &local) const
{
    return turn(local, centre(), double(m_rotation));
}

QPointF StagePlatform::toLocal(const QPointF &world) const
{
    return turn(world, centre(), -double(m_rotation));
}

bool StagePlatform::containsPoint(float x, float y) const
{
    const QPointF l = toLocal(QPointF(double(x), double(y)));
    const float lx = float(l.x()), ly = float(l.y());
    return lx >= m_originX && lx <= m_originX + m_width
        && ly >= m_originY && ly <= frontEdgeAt(lx);
}

QList<QPointF> StagePlatform::corners(const Block &b) const
{
    return QList<QPointF>() << toWorld(QPointF(double(b.x0), double(b.y0)))
                            << toWorld(QPointF(double(b.x1), double(b.y0)))
                            << toWorld(QPointF(double(b.x1), double(b.y1)))
                            << toWorld(QPointF(double(b.x0), double(b.y1)));
}

float StagePlatform::frontCurve() const
{
    if (m_shape == StepsShape)
        return 0.0f;
    return qMin(m_frontCurve, qMin(m_width / 2.0f, m_depth));
}

float StagePlatform::frontEdgeAt(float x) const
{
    const float front = m_originY + m_depth;
    const double s = double(frontCurve());
    if (s <= 0.0)
        return front;
    // The circle through both front corners and the middle of the front.
    const double half = double(m_width) / 2.0;
    const double r = (half * half + s * s) / (2.0 * s);
    const double dx = qBound(-half, double(x) - (double(m_originX) + half), half);
    return float(double(front) - r + qSqrt(qMax(0.0, r * r - dx * dx)));
}

QList<QPointF> StagePlatform::footprint(int segments) const
{
    QList<QPointF> out;
    foreach (const QPointF &q, localFootprint(segments))
        out << toWorld(q);
    return out;
}

QList<QPointF> StagePlatform::localFootprint(int segments) const
{
    QList<QPointF> out;
    const double x0 = double(m_originX), x1 = double(m_originX + m_width);
    const double y0 = double(m_originY);
    out << QPointF(x0, y0) << QPointF(x1, y0);
    if (!hasRoundedFront())
    {
        out << QPointF(x1, y0 + double(m_depth)) << QPointF(x0, y0 + double(m_depth));
        return out;
    }
    segments = qMax(2, segments);
    for (int i = 0; i <= segments; ++i)
    {
        const double x = x1 - (x1 - x0) * i / segments;
        out << QPointF(x, double(frontEdgeAt(float(x))));
    }
    return out;
}

float StagePlatform::stepWidth() const
{
    return (m_climb == ClimbStageLeft || m_climb == ClimbStageRight) ? m_depth : m_width;
}

void StagePlatform::setSteps(int count, float rise, float run, float width, Climb climb)
{
    m_shape = StepsShape;
    m_stepCount = qMax(1, count);
    m_stepRise = qMax(0.01f, rise);
    m_stepRun = qMax(0.01f, run);
    m_climb = climb;
    const float along = float(m_stepCount) * m_stepRun;
    if (climb == ClimbStageLeft || climb == ClimbStageRight)
    {
        setWidth(along);
        setDepth(width);
    }
    else
    {
        setWidth(width);
        setDepth(along);
    }
    setHeight(float(m_stepCount) * m_stepRise);
}

/* How far into the flight (x, y) is, measured from the bottom step's edge
   in the direction of the climb. */
static float intoFlight(const StagePlatform *p, float x, float y)
{
    switch (p->stepsClimb())
    {
    case StagePlatform::ClimbDownstage:  return y - p->originY();
    case StagePlatform::ClimbStageLeft:  return x - p->originX();
    case StagePlatform::ClimbStageRight: return p->originX() + p->width() - x;
    case StagePlatform::ClimbUpstage:
    default:                             return p->originY() + p->depth() - y;
    }
}

float StagePlatform::topAt(float x, float y) const
{
    if (m_shape != StepsShape)
        return m_height;
    // Off the edge counts as the nearest tread: a fixture standing at the
    // very edge is on that step, not on the floor.
    const QPointF l = toLocal(QPointF(double(x), double(y)));
    const float d = qBound(0.0f, intoFlight(this, float(l.x()), float(l.y())), float(m_stepCount) * m_stepRun);
    const int k = qBound(0, int(d / m_stepRun), m_stepCount - 1);
    return float(k + 1) * m_stepRise;
}

QList<StagePlatform::Block> StagePlatform::blocks() const
{
    QList<Block> out;
    if (m_shape != StepsShape)
    {
        out << Block{ m_originX, m_originY, m_originX + m_width, m_originY + m_depth, m_height };
        return out;
    }
    const float x1 = m_originX + m_width, y1 = m_originY + m_depth;
    for (int k = 0; k < m_stepCount; ++k)
    {
        const float a = float(k) * m_stepRun, b = float(k + 1) * m_stepRun;
        const float h = float(k + 1) * m_stepRise;
        switch (m_climb)
        {
        case ClimbDownstage:  out << Block{ m_originX, m_originY + a, x1, m_originY + b, h }; break;
        case ClimbStageLeft:  out << Block{ m_originX + a, m_originY, m_originX + b, y1, h }; break;
        case ClimbStageRight: out << Block{ x1 - b, m_originY, x1 - a, y1, h }; break;
        case ClimbUpstage:
        default:              out << Block{ m_originX, y1 - b, x1, y1 - a, h }; break;
        }
    }
    return out;
}

bool StagePlatform::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLPlatform)
        return false;

    QXmlStreamAttributes a = root.attributes();
    // Restore the persisted id rather than keeping the loader's assigned one —
    // see the equivalent comment in StageTarget::loadXML.
    if (a.hasAttribute(KXMLPlatformID))     m_id      = a.value(KXMLPlatformID).toUInt();
    if (a.hasAttribute(KXMLPlatformName))   m_name    = a.value(KXMLPlatformName).toString();
    if (a.hasAttribute(KXMLPlatformOriginX)) m_originX = a.value(KXMLPlatformOriginX).toFloat();
    if (a.hasAttribute(KXMLPlatformOriginY)) m_originY = a.value(KXMLPlatformOriginY).toFloat();
    if (a.hasAttribute(KXMLPlatformWidth))   setWidth(a.value(KXMLPlatformWidth).toFloat());
    if (a.hasAttribute(KXMLPlatformDepth))   setDepth(a.value(KXMLPlatformDepth).toFloat());
    if (a.hasAttribute(KXMLPlatformHeight))  setHeight(a.value(KXMLPlatformHeight).toFloat());
    if (a.hasAttribute(KXMLPlatformTopMat))
        setTopMaterial(TopMaterial(a.value(KXMLPlatformTopMat).toInt()));
    if (a.hasAttribute(KXMLPlatformColor))   m_color = QColor(a.value(KXMLPlatformColor).toString());
    if (a.hasAttribute(KXMLPlatformLocked))  m_locked = (a.value(KXMLPlatformLocked).toString() == "true");
    // Solid defaults ON (legacy platforms without the attribute are solid).
    if (a.hasAttribute(KXMLPlatformSolid))     m_solid = (a.value(KXMLPlatformSolid).toString() == "true");
    if (a.hasAttribute(KXMLPlatformStackable)) m_stackable = (a.value(KXMLPlatformStackable).toString() == "true");
    if (a.hasAttribute(KXMLPlatformLayerId)) m_layerId = a.value(KXMLPlatformLayerId).toUInt();
    if (a.hasAttribute(KXMLPlatformGroupId)) m_groupId = a.value(KXMLPlatformGroupId).toUInt();
    if (a.hasAttribute(KXMLPlatformFrontCurve)) setFrontCurve(a.value(KXMLPlatformFrontCurve).toFloat());
    if (a.hasAttribute(KXMLPlatformRotation)) setRotation(a.value(KXMLPlatformRotation).toFloat());
    if (a.hasAttribute(KXMLPlatformSteps))
    {
        // Width/Depth/Height above are the flight's outside already; this
        // only says how it is divided.
        m_shape = StepsShape;
        m_stepCount = qMax(1, a.value(KXMLPlatformSteps).toInt());
        if (a.hasAttribute(KXMLPlatformRise)) m_stepRise = qMax(0.01f, a.value(KXMLPlatformRise).toFloat());
        if (a.hasAttribute(KXMLPlatformRun))  m_stepRun  = qMax(0.01f, a.value(KXMLPlatformRun).toFloat());
        if (a.hasAttribute(KXMLPlatformClimb))
            m_climb = Climb(qBound(0, a.value(KXMLPlatformClimb).toInt(), 3));
    }

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
    // Written only when it is not the default, so existing files do not churn.
    if (m_topMaterial != SolidTop)
        doc->writeAttribute(KXMLPlatformTopMat, QString::number(int(m_topMaterial)));
    if (m_color.isValid())
        doc->writeAttribute(KXMLPlatformColor, m_color.name());
    if (m_locked)
        doc->writeAttribute(KXMLPlatformLocked, "true");
    if (!m_solid)                                    // solid is the default
        doc->writeAttribute(KXMLPlatformSolid, "false");
    if (m_stackable)
        doc->writeAttribute(KXMLPlatformStackable, "true");
    if (m_layerId != 0)
        doc->writeAttribute(KXMLPlatformLayerId, QString::number(m_layerId));
    if (m_groupId != 0)
        doc->writeAttribute(KXMLPlatformGroupId, QString::number(m_groupId));
    if (m_frontCurve > 0.0f)
        doc->writeAttribute(KXMLPlatformFrontCurve, QString::number(double(m_frontCurve), 'f', 3));
    if (m_rotation != 0.0f)
        doc->writeAttribute(KXMLPlatformRotation, QString::number(double(m_rotation), 'f', 2));
    if (m_shape == StepsShape)              // a plain box writes nothing new
    {
        doc->writeAttribute(KXMLPlatformSteps, QString::number(m_stepCount));
        doc->writeAttribute(KXMLPlatformRise, QString::number(double(m_stepRise), 'f', 4));
        doc->writeAttribute(KXMLPlatformRun, QString::number(double(m_stepRun), 'f', 4));
        if (m_climb != ClimbUpstage)
            doc->writeAttribute(KXMLPlatformClimb, QString::number(int(m_climb)));
    }
    doc->writeEndElement();
    return true;
}
