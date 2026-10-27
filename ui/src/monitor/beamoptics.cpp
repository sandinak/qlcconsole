/*
  Q Light Controller Plus
  beamoptics.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QImageReader>
#include <QRegularExpression>
#include <QHash>
#include <QMutex>
#include <QtMath>

#include "beamoptics.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "gobolibrary.h"
#include "monitorproperties.h"

namespace
{
    /** One gobo wheel in the light path: its picture's mask and how far it
     *  is turned. */
    struct Wheel
    {
        quint32 channel;
        QImage mask;
        double angleDeg;
    };

    bool isRotationPreset(QLCCapability::Preset p)
    {
        switch (p)
        {
            case QLCCapability::RotationStop:
            case QLCCapability::RotationIndexed:
            case QLCCapability::RotationClockwise:
            case QLCCapability::RotationClockwiseSlowToFast:
            case QLCCapability::RotationClockwiseFastToSlow:
            case QLCCapability::RotationCounterClockwise:
            case QLCCapability::RotationCounterClockwiseSlowToFast:
            case QLCCapability::RotationCounterClockwiseFastToSlow:
                return true;
            default:
                return false;
        }
    }

    bool hasPictures(const QLCChannel *ch)
    {
        foreach (QLCCapability *cap, ch->capabilities())
            if (cap->presetType() == QLCCapability::Picture)
                return true;
        return false;
    }

    /** A channel whose job is to turn something (a gobo or a prism). */
    bool turns(const QLCChannel *ch)
    {
        if (ch->name().contains(QLatin1String("rotat"), Qt::CaseInsensitive))
            return true;
        foreach (QLCCapability *cap, ch->capabilities())
            if (isRotationPreset(cap->preset())
                && cap->preset() != QLCCapability::RotationStop)
                return true;
        return false;
    }

    // Slowest and fastest a "slow - fast" range is drawn turning, deg/s.
    const double kSlow = 12.0;
    const double kFast = 200.0;
}

QImage BeamOptics::goboMask(const QString &path)
{
    // Asked from the rig view's drawing threads at once.
    static QHash<QString, QImage> cache;
    static QMutex lock;
    {
        QMutexLocker locker(&lock);
        auto it = cache.constFind(path);
        if (it != cache.constEnd())
            return it.value();
    }

    QImage out;
    QImageReader reader(path);
    reader.setScaledSize(QSize(kMaskSize, kMaskSize));
    const QImage src = reader.read().convertToFormat(QImage::Format_ARGB32);
    if (!src.isNull())
    {
        out = QImage(kMaskSize, kMaskSize, QImage::Format_Grayscale8);
        for (int y = 0; y < kMaskSize; ++y)
        {
            uchar *row = out.scanLine(y);
            for (int x = 0; x < kMaskSize; ++x)
            {
                const QRgb c = src.pixel(x, y);
                // Clear passes; drawn-in passes as much as it is light.
                const double a = qAlpha(c) / 255.0;
                const double lum = qGray(c) / 255.0;
                row[x] = uchar(qRound(255.0 * ((1.0 - a) + a * lum)));
            }
        }
    }
    QMutexLocker locker(&lock);
    if (cache.size() > 256)
        cache.clear();
    cache.insert(path, out);
    return out;
}

double BeamOptics::sample(const QImage &mask, double x, double y)
{
    if (mask.isNull() || x * x + y * y > 1.0)
        return 0.0;
    const int n = mask.width();
    const double fx = (x + 1.0) * 0.5 * n - 0.5;
    const double fy = (1.0 - (y + 1.0) * 0.5) * n - 0.5;   // image rows run down
    const int x0 = qBound(0, int(qFloor(fx)), n - 1), y0 = qBound(0, int(qFloor(fy)), n - 1);
    const int x1 = qMin(n - 1, x0 + 1), y1 = qMin(n - 1, y0 + 1);
    const double tx = qBound(0.0, fx - x0, 1.0), ty = qBound(0.0, fy - y0, 1.0);
    auto at = [&](int px, int py) { return mask.constScanLine(py)[px] / 255.0; };
    const double top = at(x0, y0) * (1.0 - tx) + at(x1, y0) * tx;
    const double bot = at(x0, y1) * (1.0 - tx) + at(x1, y1) * tx;
    return top * (1.0 - ty) + bot * ty;
}

int BeamOptics::prismFacets(const QLCCapability *cap)
{
    if (cap == nullptr)
        return 0;
    const QString name = cap->name();
    static const QRegularExpression facetsRx(QStringLiteral("(\\d+)\\s*-?\\s*facet"),
                                             QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = facetsRx.match(name);
    if (m.hasMatch())
        return qBound(2, m.captured(1).toInt(), 24);
    if (name.contains(QLatin1String("linear"), Qt::CaseInsensitive))
        return 1;
    if (cap->preset() == QLCCapability::PrismEffectOff)
        return 0;
    static const QRegularExpression offRx(QStringLiteral("\\b(no|off|stop|open|out)\\b"),
                                          QRegularExpression::CaseInsensitiveOption);
    if (cap->preset() == QLCCapability::PrismEffectOn
        || (name.contains(QLatin1String("prism"), Qt::CaseInsensitive) && !offRx.match(name).hasMatch()))
        return 3;
    return 0;
}

double BeamOptics::indexedAngle(const QLCCapability *cap, int value)
{
    if (cap == nullptr || cap->preset() != QLCCapability::RotationIndexed)
        return -1.0;
    const double span = qMax(1, int(cap->max()) - int(cap->min()) + 1);
    return 360.0 * (value - int(cap->min())) / span;
}

double BeamOptics::rotationSpeed(const QLCCapability *cap, int value)
{
    if (cap == nullptr)
        return 0.0;
    const QString name = cap->name();
    const QLCCapability::Preset p = cap->preset();
    if (p == QLCCapability::RotationStop || p == QLCCapability::RotationIndexed)
        return 0.0;

    bool ccw = false, known = true;
    switch (p)
    {
        case QLCCapability::RotationCounterClockwise:
        case QLCCapability::RotationCounterClockwiseSlowToFast:
        case QLCCapability::RotationCounterClockwiseFastToSlow:
            ccw = true;
            break;
        case QLCCapability::RotationClockwise:
        case QLCCapability::RotationClockwiseSlowToFast:
        case QLCCapability::RotationClockwiseFastToSlow:
            break;
        default:
            known = false;
    }
    if (!known)
    {
        // No preset: read the name ("Clockwise rotation fast - slow").
        if (!name.contains(QLatin1String("rotat"), Qt::CaseInsensitive)
            || name.contains(QLatin1String("stop"), Qt::CaseInsensitive)
            || name.contains(QLatin1String("no rot"), Qt::CaseInsensitive))
            return 0.0;
        ccw = name.contains(QLatin1String("counter"), Qt::CaseInsensitive)
              || name.contains(QLatin1String("anti"), Qt::CaseInsensitive)
              || name.contains(QLatin1String("ccw"), Qt::CaseInsensitive);
    }

    const double span = qMax(1, int(cap->max()) - int(cap->min()));
    const double frac = qBound(0.0, (value - int(cap->min())) / span, 1.0);
    const bool fastToSlow = p == QLCCapability::RotationClockwiseFastToSlow
        || p == QLCCapability::RotationCounterClockwiseFastToSlow
        || QRegularExpression(QStringLiteral("fast\\s*(-|to)\\s*slow"),
                              QRegularExpression::CaseInsensitiveOption).match(name).hasMatch();
    const bool slowToFast = p == QLCCapability::RotationClockwiseSlowToFast
        || p == QLCCapability::RotationCounterClockwiseSlowToFast
        || QRegularExpression(QStringLiteral("slow\\s*(-|to)\\s*fast"),
                              QRegularExpression::CaseInsensitiveOption).match(name).hasMatch();
    double speed = (kSlow + kFast) * 0.5;
    if (fastToSlow)
        speed = kFast - (kFast - kSlow) * frac;
    else if (slowToFast)
        speed = kSlow + (kFast - kSlow) * frac;
    return ccw ? -speed : speed;
}

QString BeamOptics::slotPicture(const Fixture *fx, quint32 channel, const QLCCapability *cap,
                                const MonitorProperties *props)
{
    if (fx == nullptr || cap == nullptr)
        return QString();
    if (props != nullptr)
    {
        const QString swap = props->goboSwap(fx->id(), channel, cap->min());
        if (!swap.isEmpty())
        {
            const QString path = GoboLibrary::resolve(swap);
            if (!path.isEmpty())
                return path;
        }
    }
    if (cap->presetType() != QLCCapability::Picture)
        return QString();
    return const_cast<QLCCapability *>(cap)->resource(0).toString();
}

BeamOptics::Optics BeamOptics::opticsFor(const Fixture *fx, const QByteArray &values, double seconds,
                                         const MonitorProperties *props)
{
    Optics out;
    if (fx == nullptr)
        return out;

    auto valueOf = [&](quint32 ch) {
        return ch < quint32(values.size()) ? int(uchar(values.at(int(ch)))) : 0;
    };

    QList<Wheel> wheels;
    int prism = 0;
    double prismAngle = 0.0;

    for (quint32 i = 0; i < fx->channels(); ++i)
    {
        const QLCChannel *ch = fx->channel(i);
        if (ch == nullptr || ch->controlByte() != QLCChannel::MSB)
            continue;
        const int v = valueOf(i);
        QLCCapability *cap = ch->searchCapability(uchar(v));

        if (ch->group() == QLCChannel::Gobo)
        {
            if (hasPictures(ch))
            {
                // A wheel: the picture in the slot it is on. "Open" is the
                // absence of a gobo; a wheel spinning between slots shows none.
                QImage mask;
                const QString path = slotPicture(fx, i, cap, props);
                if (!path.isEmpty()
                    && !path.contains(QLatin1String("/open."), Qt::CaseInsensitive))
                    mask = goboMask(path);
                wheels << Wheel{ i, mask, 0.0 };
            }
            else if (turns(ch) && !wheels.isEmpty())
            {
                // Gobo rotation: turns the nearest wheel before it.
                const double idx = indexedAngle(cap, v);
                const double speed = rotationSpeed(cap, v);
                if (idx >= 0.0)
                    wheels.last().angleDeg = idx;
                else if (!qFuzzyIsNull(speed))
                {
                    wheels.last().angleDeg = std::fmod(speed * seconds, 360.0);
                    if (!wheels.last().mask.isNull())
                        out.animated = true;
                }
            }
        }
        else if (ch->group() == QLCChannel::Prism)
        {
            if (turns(ch))
            {
                const double idx = indexedAngle(cap, v);
                const double speed = rotationSpeed(cap, v);
                if (idx >= 0.0)
                    prismAngle = idx;
                else if (!qFuzzyIsNull(speed))
                {
                    prismAngle = std::fmod(speed * seconds, 360.0);
                    out.animated = true;
                }
            }
            else if (prism == 0)
                prism = prismFacets(cap);
        }
    }

    QList<Wheel> inPath;
    foreach (const Wheel &w, wheels)
        if (!w.mask.isNull())
            inPath << w;
    if (prism == 0)
        out.animated = out.animated && !inPath.isEmpty();
    out.prismFacets = prism;
    if (inPath.isEmpty() && prism == 0)
        return out;                                   // an open beam

    /* Where the beam's image lands through the prism: a facet ring throws
       N smaller copies of it round the axis, a linear prism three in a row.
       Copies are scaled to keep the whole spread inside the cone. */
    struct Copy { double cx, cy, scale; };
    QList<Copy> copies;
    const double pa = qDegreesToRadians(prismAngle);
    if (prism == 0)
        copies << Copy{ 0.0, 0.0, 1.0 };
    else if (prism == 1)
    {
        for (int k = -1; k <= 1; ++k)
            copies << Copy{ 0.5 * k * qCos(pa), 0.5 * k * qSin(pa), 0.5 };
    }
    else
    {
        // Far enough out that the images read as separate, small enough that
        // the whole spread stays inside the cone.
        const double ring = 0.55, scale = 0.42;
        for (int k = 0; k < prism; ++k)
        {
            const double a = pa + 2.0 * M_PI * k / prism;
            copies << Copy{ ring * qCos(a), ring * qSin(a), scale };
        }
    }

    const int n = kMaskSize;
    QImage mask(n, n, QImage::Format_Grayscale8);
    double sum = 0.0;
    int inside = 0;
    for (int py = 0; py < n; ++py)
    {
        uchar *row = mask.scanLine(py);
        const double y = 1.0 - (py + 0.5) * 2.0 / n;
        for (int px = 0; px < n; ++px)
        {
            const double x = (px + 0.5) * 2.0 / n - 1.0;
            double t = 0.0;
            if (x * x + y * y <= 1.0)
            {
                for (const Copy &c : copies)
                {
                    const double lx = (x - c.cx) / c.scale, ly = (y - c.cy) / c.scale;
                    if (lx * lx + ly * ly > 1.0)
                        continue;
                    double through = 1.0;
                    for (const Wheel &w : inPath)
                    {
                        // Turn the wheel: sample it at the point turned back.
                        const double r = -qDegreesToRadians(w.angleDeg);
                        const double wx = lx * qCos(r) - ly * qSin(r);
                        const double wy = lx * qSin(r) + ly * qCos(r);
                        through *= sample(w.mask, wx, wy);
                    }
                    t += through;
                }
                /* A prism shares the light out between its facets, and where
                   their images cross they add up: each on its own is dimmer
                   than the open beam, the overlaps brighter. */
                if (copies.size() > 1)
                    t = qMin(1.0, t * 0.7);
                sum += t;
                ++inside;
            }
            row[px] = uchar(qRound(255.0 * t));
        }
    }
    out.mask = mask;
    out.coverage = inside > 0 ? sum / inside : 1.0;
    return out;
}
