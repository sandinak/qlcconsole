/*
  Q Light Controller Plus - qlcconsole
  nolightzone.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QCoreApplication>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QtMath>
#include <climits>

#include "nolightzone.h"
#include "qlcphysical.h"

QString NoLightZone::kindToString(int kind)
{
    switch (kind)
    {
        case NoBeams: return QStringLiteral("NoBeams");
        case NoLasers: return QStringLiteral("NoLasers");
        default: return QStringLiteral("NoLight");
    }
}

int NoLightZone::stringToKind(const QString &s)
{
    if (s == QLatin1String("NoBeams"))
        return NoBeams;
    if (s == QLatin1String("NoLasers"))
        return NoLasers;
    return NoLight;
}

QString NoLightZone::kindLabel(int kind)
{
    switch (kind)
    {
        case NoBeams: return QCoreApplication::translate("NoLightZone", "No beams");
        case NoLasers: return QCoreApplication::translate("NoLightZone", "No lasers");
        default: return QCoreApplication::translate("NoLightZone", "No light");
    }
}

bool NoLightZone::applies(bool laser, double coneDegrees) const
{
    switch (kind)
    {
        case NoLasers: return laser;
        case NoBeams: return laser || (coneDegrees > 0.0 && coneDegrees <= kNarrowBeamDeg);
        default: return true;
    }
}

bool NoLightZone::contains(const QVector3D &p) const
{
    return p.x() >= x && p.x() <= x + width && p.y() >= y && p.y() <= y + depth
           && p.z() >= floorZ && p.z() <= floorZ + height;
}

bool NoLightZone::raySpan(const QVector3D &p, const QVector3D &d, double range, double &t0, double &t1) const
{
    t0 = 0.0;
    t1 = range;
    // The ray comes down to the floor and stops there.
    if (d.z() < -1e-6f && p.z() > 0.0f)
        t1 = qMin(t1, double(p.z()) / -double(d.z()));
    const double lo[3] = { double(x), double(y), double(floorZ) };
    const double hi[3] = { double(x) + double(width), double(y) + double(depth),
                           double(floorZ) + double(height) };
    const double o[3] = { double(p.x()), double(p.y()), double(p.z()) };
    const double v[3] = { double(d.x()), double(d.y()), double(d.z()) };
    for (int i = 0; i < 3; ++i)
    {
        if (qAbs(v[i]) < 1e-9)
        {
            if (o[i] < lo[i] || o[i] > hi[i])
                return false;
            continue;
        }
        double a = (lo[i] - o[i]) / v[i];
        double b = (hi[i] - o[i]) / v[i];
        if (a > b)
            qSwap(a, b);
        t0 = qMax(t0, a);
        t1 = qMin(t1, b);
        if (t0 > t1)
            return false;
    }
    return true;
}

bool NoLightZone::hitByRay(const QVector3D &p, const QVector3D &d, double range, double *entryZ) const
{
    double t0, t1;
    if (contains(p))
    {
        if (entryZ != nullptr)
            *entryZ = double(p.z());
        return true;
    }
    if (!raySpan(p, d, range, t0, t1))
        return false;
    if (entryZ != nullptr)
        *entryZ = double(p.z()) + double(d.z()) * t0;
    return true;
}

bool NoLightZone::hitByRays(const QVector3D &p, const QVector<QVector3D> &rays, double range) const
{
    for (const QVector3D &r : rays)
        if (hitByRay(p, r, range))
            return true;
    return false;
}

void NoLightZone::saveXML(QXmlStreamWriter *w) const
{
    w->writeStartElement(QStringLiteral("NoLightZone"));
    w->writeAttribute(QStringLiteral("ID"), QString::number(id));
    w->writeAttribute(QStringLiteral("Name"), name);
    w->writeAttribute(QStringLiteral("X"), QString::number(double(x), 'f', 3));
    w->writeAttribute(QStringLiteral("Y"), QString::number(double(y), 'f', 3));
    w->writeAttribute(QStringLiteral("W"), QString::number(double(width), 'f', 3));
    w->writeAttribute(QStringLiteral("D"), QString::number(double(depth), 'f', 3));
    w->writeAttribute(QStringLiteral("Z"), QString::number(double(floorZ), 'f', 3));
    w->writeAttribute(QStringLiteral("H"), QString::number(double(height), 'f', 3));
    w->writeAttribute(QStringLiteral("Kind"), kindToString(kind));
    w->writeEndElement();
}

bool NoLightZone::loadXML(QXmlStreamReader &r, NoLightZone &z)
{
    if (r.name() != QLatin1String("NoLightZone"))
        return false;
    const QXmlStreamAttributes a = r.attributes();
    z.id = a.value(QStringLiteral("ID")).toUInt();
    z.name = a.value(QStringLiteral("Name")).toString();
    z.x = a.value(QStringLiteral("X")).toFloat();
    z.y = a.value(QStringLiteral("Y")).toFloat();
    z.width = qMax(0.1f, a.value(QStringLiteral("W")).toFloat());
    z.depth = qMax(0.1f, a.value(QStringLiteral("D")).toFloat());
    z.floorZ = a.value(QStringLiteral("Z")).toFloat();
    z.height = qMax(0.1f, a.value(QStringLiteral("H")).toFloat());
    z.kind = stringToKind(a.value(QStringLiteral("Kind")).toString());
    if (z.name.isEmpty())
        z.name = QStringLiteral("Zone %1").arg(z.id + 1);
    r.skipCurrentElement();
    return true;
}

/****************************************************************************
 * Rays
 ****************************************************************************/

static void frameOf(const QVector3D &dir, QVector3D &u, QVector3D &v)
{
    u = QVector3D::crossProduct(dir, QVector3D(0, 0, 1));
    if (u.length() < 1e-3f)
        u = QVector3D(1, 0, 0);
    u.normalize();
    v = QVector3D::crossProduct(dir, u).normalized();
}

QVector<QVector3D> LightRays::cone(const QVector3D &dir, double halfRad)
{
    QVector<QVector3D> rays;
    rays.reserve(49);
    rays << dir;
    QVector3D u, v;
    frameOf(dir, u, v);
    for (int ring = 1; ring <= 3; ++ring)
    {
        const double c = halfRad * ring / 3.0;
        for (int i = 0; i < 16; ++i)
        {
            const double a = i * M_PI / 8.0;
            rays << (dir * float(qCos(c))
                     + (u * float(qCos(a)) + v * float(qSin(a))) * float(qSin(c))).normalized();
        }
    }
    return rays;
}

QVector<QVector3D> LightRays::window(const QVector3D &dir, double hDeg, double vDeg)
{
    QVector<QVector3D> rays;
    QVector3D u, v;
    frameOf(dir, u, v);
    // u is horizontal (across the throw); v the other way across it.
    const double th = qTan(qDegreesToRadians(qMin(hDeg, 160.0) / 2.0));
    const double tv = qTan(qDegreesToRadians(qMin(vDeg, 160.0) / 2.0));
    const int n = 4;
    for (int i = -n; i <= n; ++i)
        for (int j = -n; j <= n; ++j)
            rays << (dir + u * float(th * i / n) + v * float(tv * j / n)).normalized();
    return rays;
}

void LightRays::laserWindow(const QLCPhysical &phy, double &hDeg, double &vDeg)
{
    const double mx = qMax(phy.lensDegreesMin(), phy.lensDegreesMax());
    const double mn = qMin(phy.lensDegreesMin(), phy.lensDegreesMax());
    hDeg = mx > 0.0 ? mx : 40.0;
    vDeg = (mn > 0.0 && mn < mx) ? mn : hDeg;
}

void LightRays::angles(const QVector3D &dir, const QVector3D &ray, double &ax, double &ay)
{
    QVector3D u, v;
    frameOf(dir, u, v);
    const double f = double(QVector3D::dotProduct(ray, dir));
    ax = qRadiansToDegrees(qAtan2(double(QVector3D::dotProduct(ray, u)), f));
    ay = qRadiansToDegrees(qAtan2(double(QVector3D::dotProduct(ray, v)), f));
}

QVector<QVector3D> LightRays::cone(const QVector3D &dir, double halfRad, const LightMask &mask)
{
    QVector<QVector3D> rays = cone(dir, halfRad);
    if (mask.isNone())
        return rays;
    const double half = qRadiansToDegrees(halfRad);
    QVector<QVector3D> kept;
    for (const QVector3D &r : rays)
    {
        double ax, ay;
        angles(dir, r, ax, ay);
        if (ax < -half + double(mask.left) - 1e-6 || ax > half - double(mask.right) + 1e-6
            || ay < -half + double(mask.top) - 1e-6 || ay > half - double(mask.bottom) + 1e-6)
            continue;
        kept << r;
    }
    return kept;
}

QVector<QVector3D> LightRays::window(const QVector3D &dir, double hDeg, double vDeg,
                                     const LightMask &mask)
{
    if (mask.isNone())
        return window(dir, hDeg, vDeg);
    QVector3D u, v;
    frameOf(dir, u, v);
    const double h = qMin(hDeg, 160.0) / 2.0, vv = qMin(vDeg, 160.0) / 2.0;
    const double x0 = -h + double(mask.left), x1 = h - double(mask.right);
    const double y0 = -vv + double(mask.top), y1 = vv - double(mask.bottom);
    QVector<QVector3D> rays;
    if (x1 <= x0 || y1 <= y0)
        return rays;
    const double tx0 = qTan(qDegreesToRadians(x0)), tx1 = qTan(qDegreesToRadians(x1));
    const double ty0 = qTan(qDegreesToRadians(y0)), ty1 = qTan(qDegreesToRadians(y1));
    const int n = 4;
    for (int i = -n; i <= n; ++i)
        for (int j = -n; j <= n; ++j)
        {
            const double fx = (i + n) / double(2 * n), fy = (j + n) / double(2 * n);
            rays << (dir + u * float(tx0 + (tx1 - tx0) * fx) + v * float(ty0 + (ty1 - ty0) * fy)).normalized();
        }
    return rays;
}
