/*
  Q Light Controller Plus
  truss.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QtMath>

#include "truss.h"

#define KXMLTruss           QStringLiteral("Truss")
#define KXMLTrussID         QStringLiteral("ID")
#define KXMLTrussName       QStringLiteral("Name")
#define KXMLTrussType       QStringLiteral("Type")
#define KXMLTrussProfile    QStringLiteral("Profile")
#define KXMLTrussLength     QStringLiteral("Length")
#define KXMLTrussWidth      QStringLiteral("Width")
#define KXMLTrussOriginX    QStringLiteral("OriginX")
#define KXMLTrussOriginY    QStringLiteral("OriginY")
#define KXMLTrussOriginZ    QStringLiteral("OriginZ")
#define KXMLTrussDirX       QStringLiteral("DirX")
#define KXMLTrussDirY       QStringLiteral("DirY")
#define KXMLTrussLocked     QStringLiteral("Locked")
#define KXMLTrussLayerId    QStringLiteral("LayerId")
#define KXMLTrussGroupId    QStringLiteral("GroupId")

Truss::Truss(quint32 id, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_name(QString("Truss %1").arg(id + 1))
    , m_type(Horizontal)
    , m_origin(0, 0, 6)         // sensible default: 6 m high
    , m_direction(1, 0)         // running stage-right
    , m_length(6.0f)
    , m_width(0.29f)            // standard 12" square truss
    , m_profile(SquareTruss)
{
}

void Truss::setDirection(const QPointF &d)
{
    float len = qSqrt(d.x() * d.x() + d.y() * d.y());
    if (len < 1e-6f)
        m_direction = QPointF(1, 0);
    else
        m_direction = QPointF(d.x() / len, d.y() / len);
}

QVector3D Truss::positionAt(float offset) const
{
    switch (m_type)
    {
        case Horizontal:
            return QVector3D(m_origin.x() + m_direction.x() * offset,
                             m_origin.y() + m_direction.y() * offset,
                             m_origin.z());
        case Vertical:
            // A child BAR of vertical run is a hanging drop: it extends DOWNWARD
            // from its origin (the attach point, minus the drop). A free-standing
            // vertical truss (a tower) extends upward as before.
            return QVector3D(m_origin.x(), m_origin.y(),
                             isChildBar() ? m_origin.z() - offset
                                          : m_origin.z() + offset);
        case Ground:
        default:
            // offset interpreted as distance along direction in XY plane
            return QVector3D(m_origin.x() + m_direction.x() * offset,
                             m_origin.y() + m_direction.y() * offset,
                             0.0f);
    }
}

QString Truss::typeToString(TrussType t)
{
    switch (t)
    {
        case Horizontal:    return "Horizontal";
        case Vertical:      return "Vertical";
        case Ground:        return "Ground";
    }
    return "Horizontal";
}

Truss::TrussType Truss::stringToType(const QString &s)
{
    if (s == "Vertical")    return Vertical;
    if (s == "Ground")      return Ground;
    return Horizontal;
}

QString Truss::profileToString(Profile p)
{
    switch (p)
    {
        case SquareTruss:   return "Square";
        case TriangleTruss: return "Triangle";
        case IBeam:         return "IBeam";
        case Pipe:          return "Pipe";
        case Other:         return "Other";
    }
    return "Square";
}

Truss::Profile Truss::stringToProfile(const QString &s)
{
    if (s == "Triangle")    return TriangleTruss;
    if (s == "IBeam")       return IBeam;
    if (s == "Pipe")        return Pipe;
    if (s == "Other")       return Other;
    return SquareTruss;
}

QString Truss::mountingToString(MountingType m)
{
    switch (m)
    {
        case TopHung:       return "TopHung";
        case FloorMounted:  return "FloorMounted";
        case SideArm:       return "SideArm";
    }
    return "TopHung";
}

Truss::MountingType Truss::stringToMounting(const QString &s)
{
    if (s == "FloorMounted") return FloorMounted;
    if (s == "SideArm")      return SideArm;
    return TopHung;
}

bool Truss::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLTruss)
        return false;

    QXmlStreamAttributes attrs = root.attributes();
    m_id      = attrs.value(KXMLTrussID).toUInt();
    m_name    = attrs.value(KXMLTrussName).toString();
    m_type    = stringToType(attrs.value(KXMLTrussType).toString());
    m_profile = stringToProfile(attrs.value(KXMLTrussProfile).toString());
    m_length  = attrs.value(KXMLTrussLength).toFloat();
    m_width   = attrs.value(KXMLTrussWidth).toFloat();

    float ox = attrs.value(KXMLTrussOriginX).toFloat();
    float oy = attrs.value(KXMLTrussOriginY).toFloat();
    float oz = attrs.value(KXMLTrussOriginZ).toFloat();
    m_origin  = QVector3D(ox, oy, oz);

    setDirection(QPointF(attrs.value(KXMLTrussDirX).toFloat(),
                         attrs.value(KXMLTrussDirY).toFloat()));
    m_locked = (attrs.value(KXMLTrussLocked).toString() == "true");
    m_layerId = attrs.value(KXMLTrussLayerId).toUInt();
    m_groupId = attrs.value(KXMLTrussGroupId).toUInt();
    if (attrs.hasAttribute(QStringLiteral("ParentTruss")))
    {
        m_parentTrussId = attrs.value(QStringLiteral("ParentTruss")).toUInt();
        m_parentOffset  = attrs.value(QStringLiteral("ParentOffset")).toFloat();
        if (attrs.hasAttribute(QStringLiteral("BarFace")))
        {
            m_barFace       = attrs.value(QStringLiteral("BarFace")).toInt();
            m_barStandoff   = attrs.value(QStringLiteral("BarStandoff")).toFloat();
            m_barRun        = attrs.value(QStringLiteral("BarRun")).toInt();
            m_barCrossShift = attrs.value(QStringLiteral("BarCross")).toFloat();
        }
        else
        {
            // MIGRATE the old parentDrop/direction bar model: an old drop was a
            // straight-down offset → Bottom face + standoff; a Vertical bar was a
            // hanging drop, everything else ran along the truss.
            m_barFace     = FaceBottom;
            m_barStandoff = attrs.value(QStringLiteral("ParentDrop")).toFloat();
            m_barRun      = (m_type == Vertical) ? RunDrop : RunAlong;
        }
    }
    if (attrs.hasAttribute(QStringLiteral("Stand")))
        m_standId = attrs.value(QStringLiteral("Stand")).toUInt();

    root.skipCurrentElement();
    return true;
}

bool Truss::saveXML(QXmlStreamWriter *doc) const
{
    doc->writeStartElement(KXMLTruss);
    doc->writeAttribute(KXMLTrussID,      QString::number(m_id));
    doc->writeAttribute(KXMLTrussName,    m_name);
    doc->writeAttribute(KXMLTrussType,    typeToString(m_type));
    doc->writeAttribute(KXMLTrussProfile, profileToString(m_profile));
    doc->writeAttribute(KXMLTrussLength,  QString::number(double(m_length), 'f', 3));
    doc->writeAttribute(KXMLTrussWidth,   QString::number(double(m_width),  'f', 3));
    doc->writeAttribute(KXMLTrussOriginX, QString::number(double(m_origin.x()), 'f', 3));
    doc->writeAttribute(KXMLTrussOriginY, QString::number(double(m_origin.y()), 'f', 3));
    doc->writeAttribute(KXMLTrussOriginZ, QString::number(double(m_origin.z()), 'f', 3));
    doc->writeAttribute(KXMLTrussDirX,    QString::number(double(m_direction.x()), 'f', 6));
    doc->writeAttribute(KXMLTrussDirY,    QString::number(double(m_direction.y()), 'f', 6));
    if (m_locked)
        doc->writeAttribute(KXMLTrussLocked, "true");
    if (m_layerId != 0)
        doc->writeAttribute(KXMLTrussLayerId, QString::number(m_layerId));
    if (m_groupId != 0)
        doc->writeAttribute(KXMLTrussGroupId, QString::number(m_groupId));
    if (isChildBar())
    {
        doc->writeAttribute(QStringLiteral("ParentTruss"),  QString::number(m_parentTrussId));
        doc->writeAttribute(QStringLiteral("ParentOffset"), QString::number(double(m_parentOffset), 'f', 3));
        doc->writeAttribute(QStringLiteral("BarFace"),      QString::number(m_barFace));
        doc->writeAttribute(QStringLiteral("BarStandoff"),  QString::number(double(m_barStandoff), 'f', 3));
        doc->writeAttribute(QStringLiteral("BarRun"),       QString::number(m_barRun));
        if (m_barCrossShift != 0.0f)
            doc->writeAttribute(QStringLiteral("BarCross"), QString::number(double(m_barCrossShift), 'f', 3));
    }
    if (isStandMounted())
        doc->writeAttribute(QStringLiteral("Stand"), QString::number(m_standId));
    doc->writeEndElement();
    return true;
}
