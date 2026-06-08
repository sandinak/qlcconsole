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
            return QVector3D(m_origin.x(), m_origin.y(),
                             m_origin.z() + offset);
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
    doc->writeEndElement();
    return true;
}
