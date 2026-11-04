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
#include <QPainter>
#include <QPainterPath>
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

    /** A small, stable generator (xorshift): the same stand-in on every run
     *  and platform, which qHash and QRandomGenerator don't promise. */
    struct Rng
    {
        quint32 s;
        explicit Rng(quint32 seed) : s(seed ? seed : 0x9e3779b9u) { next(); next(); }
        quint32 next()
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return s;
        }
        double unit() { return (next() & 0xffffff) / double(0x1000000); }
        double range(double a, double b) { return a + (b - a) * unit(); }
    };

    quint32 fnv(const QString &text)
    {
        quint32 h = 2166136261u;
        const QByteArray b = text.toUtf8();
        for (char c : b)
            h = (h ^ quint8(c)) * 16777619u;
        return h;
    }

    int leadingCount(const QString &name, int fallback)
    {
        static const QRegularExpression rx(QStringLiteral("(\\d+)"));
        const QRegularExpressionMatch m = rx.match(name);
        return m.hasMatch() ? qBound(2, m.captured(1).toInt(), 12) : fallback;
    }

    /** What the name says the glass looks like. */
    enum Look { Dots, Split, Rings, Swirl, Zebra, Tetris, Scatter, Petals, Triangles };

    bool lookFromName(const QString &name, Look *look)
    {
        auto has = [&](const char *w) { return name.contains(QLatin1String(w), Qt::CaseInsensitive); };
        if (has("random") || has("scatter") || has("break"))
            *look = Scatter;
        else if (has("concen") || has("ring"))
            *look = Rings;
        else if (has("split") || has("slice") || has("pie"))
            *look = Split;
        else if (has("swirl") || has("spiral") || has("vortex"))
            *look = Swirl;
        else if (has("flower") || has("lotus") || has("rose") || has("petal") || has("bloom"))
            *look = Petals;
        else if (has("triang") || has("pyramid") || has("delta"))
            *look = Triangles;
        else if (has("zebra") || has("stripe"))
            *look = Zebra;
        else if (has("tetris") || has("block") || has("square"))
            *look = Tetris;
        else if (has("spot") || has("dot") || has("star") || has("circle"))
            *look = Dots;
        else
            return false;
        return true;
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

QImage BeamOptics::standInMask(const QString &slotName, int index)
{
    static QHash<QString, QImage> cache;
    static QMutex lock;
    const QString base = GoboLibrary::slotBaseName(slotName);
    if (base.isEmpty() || GoboLibrary::isOpenSlotName(base))
        return QImage();
    const QString key = QString::number(index) + QLatin1Char('|') + base;
    {
        QMutexLocker locker(&lock);
        auto it = cache.constFind(key);
        if (it != cache.constEnd())
            return it.value();
    }

    Rng rng(fnv(base) ^ (quint32(index) * 2654435761u));
    Look look;
    if (!lookFromName(base, &look))
        look = Look(rng.next() % 6);               // Dots..Tetris, not the named-only looks
    const int n = leadingCount(base, 3 + int(rng.next() % 3));

    const int size = kMaskSize;
    QImage out(size, size, QImage::Format_Grayscale8);
    out.fill(0);                                   // the metal: black blocks
    {
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(size * 0.5, size * 0.5);
        p.scale(size * 0.5, -size * 0.5);          // beam coordinates, y up
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        const double rot = rng.range(0.0, M_PI * 2.0);
        auto dot = [&](double cx, double cy, double r) { p.drawEllipse(QPointF(cx, cy), r, r); };

        switch (look)
        {
            case Dots:
            {
                const double ring = n <= 2 ? 0.45 : 0.52;
                const double r = qBound(0.12, 0.62 / (1.0 + n * 0.45) + 0.08, 0.34);
                for (int k = 0; k < n; ++k)
                {
                    const double a = rot + 2.0 * M_PI * k / n;
                    dot(ring * qCos(a), ring * qSin(a), r);
                }
                if (n > 4)
                    dot(0.0, 0.0, r);
                break;
            }
            case Split:
            {
                // A disc cut into n pie slices by dark spokes.
                const int parts = qMax(2, n);
                const double gap = 0.09;
                for (int k = 0; k < parts; ++k)
                {
                    const double a0 = 360.0 * k / parts + qRadiansToDegrees(rot);
                    const double span = 360.0 / parts;
                    QPainterPath path;
                    path.moveTo(0.0, 0.0);
                    path.arcTo(QRectF(-0.92, -0.92, 1.84, 1.84), a0 + span * gap * 3.0,
                               span - span * gap * 6.0);
                    path.closeSubpath();
                    p.drawPath(path);
                }
                break;
            }
            case Rings:
            {
                // Rings broken by a few dark bridges, so the centre isn't loose.
                QPen pen(Qt::white, 0.17, Qt::SolidLine, Qt::FlatCap);
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                const double radii[3] = { 0.78, 0.50, 0.22 };
                for (int ri = 0; ri < 3; ++ri)
                {
                    const double r = radii[ri];
                    const int bridges = 3 + ri;
                    for (int k = 0; k < bridges; ++k)
                    {
                        const double a0 = qRadiansToDegrees(rot) + 360.0 * k / bridges + 360.0 * ri / 7.0;
                        const double span = 360.0 / bridges - 28.0;
                        p.drawArc(QRectF(-r, -r, 2 * r, 2 * r), int(a0 * 16.0), int(span * 16.0));
                    }
                }
                p.setPen(Qt::NoPen);
                p.setBrush(Qt::white);
                dot(0.0, 0.0, 0.1);
                break;
            }
            case Swirl:
            {
                // Spiral arms from the middle out.
                const int arms = qBound(2, n, 4);
                p.setPen(QPen(Qt::white, 0.16, Qt::SolidLine, Qt::RoundCap));
                p.setBrush(Qt::NoBrush);
                for (int k = 0; k < arms; ++k)
                {
                    QPainterPath path;
                    const double a0 = rot + 2.0 * M_PI * k / arms;
                    for (int i = 0; i <= 24; ++i)
                    {
                        const double t = i / 24.0;
                        const double r = 0.1 + 0.8 * t, a = a0 + t * 2.2;
                        const QPointF pt(r * qCos(a), r * qSin(a));
                        if (i == 0)
                            path.moveTo(pt);
                        else
                            path.lineTo(pt);
                    }
                    p.drawPath(path);
                }
                break;
            }
            case Zebra:
            {
                // Parallel bars, wavy so they don't read as a grille.
                p.setPen(QPen(Qt::white, 0.17, Qt::SolidLine, Qt::FlatCap));
                p.setBrush(Qt::NoBrush);
                const int bars = 5;
                const double wave = rng.range(0.05, 0.14);
                p.rotate(qRadiansToDegrees(rot));
                for (int k = 0; k < bars; ++k)
                {
                    const double y0 = -0.8 + 1.6 * k / (bars - 1);
                    QPainterPath path;
                    for (int i = 0; i <= 20; ++i)
                    {
                        const double x = -1.0 + 2.0 * i / 20.0;
                        const QPointF pt(x, y0 + wave * qSin(x * 4.0 + k));
                        if (i == 0)
                            path.moveTo(pt);
                        else
                            path.lineTo(pt);
                    }
                    p.drawPath(path);
                }
                break;
            }
            case Tetris:
            {
                // Squares in a tetromino: four cells of a 4x4 grid, joined.
                const double cell = 0.34;
                static const int shapes[5][4][2] = {
                    { {0,0}, {1,0}, {2,0}, {1,1} },     // T
                    { {0,0}, {0,1}, {0,2}, {1,2} },     // L
                    { {0,0}, {1,0}, {1,1}, {2,1} },     // Z
                    { {0,0}, {1,0}, {0,1}, {1,1} },     // O
                    { {0,0}, {1,0}, {2,0}, {3,0} } };   // I
                const int sh = int(rng.next() % 5);
                int minx = 9, miny = 9, maxx = -9, maxy = -9;
                for (int c = 0; c < 4; ++c)
                {
                    minx = qMin(minx, shapes[sh][c][0]); maxx = qMax(maxx, shapes[sh][c][0]);
                    miny = qMin(miny, shapes[sh][c][1]); maxy = qMax(maxy, shapes[sh][c][1]);
                }
                const double ox = -(minx + maxx + 1) * 0.5 * cell, oy = -(miny + maxy + 1) * 0.5 * cell;
                p.rotate(90.0 * (rng.next() % 4));
                for (int c = 0; c < 4; ++c)
                    p.drawRect(QRectF(ox + shapes[sh][c][0] * cell + 0.015, oy + shapes[sh][c][1] * cell + 0.015,
                                      cell - 0.03, cell - 0.03));
                break;
            }
            case Petals:
            {
                // Petals round a middle: an ellipse each, turned.
                const int petals = n > 4 ? qMin(n, 9) : 6 + int(rng.next() % 3);
                for (int k = 0; k < petals; ++k)
                {
                    p.save();
                    p.rotate(qRadiansToDegrees(rot) + 360.0 * k / petals);
                    p.drawEllipse(QPointF(0.5, 0.0), 0.36, 0.14 + 0.4 / petals);
                    p.restore();
                }
                dot(0.0, 0.0, 0.16);
                break;
            }
            case Triangles:
            {
                const int count = 3;
                for (int k = 0; k < count; ++k)
                {
                    p.save();
                    p.rotate(qRadiansToDegrees(rot) + 360.0 * k / count);
                    QPainterPath tri;
                    tri.moveTo(0.82, 0.0);
                    tri.lineTo(0.12, 0.38);
                    tri.lineTo(0.12, -0.38);
                    tri.closeSubpath();
                    p.drawPath(tri);
                    p.restore();
                }
                break;
            }
            case Scatter:
            {
                const int count = 9 + int(rng.next() % 6);
                for (int k = 0; k < count; ++k)
                {
                    const double a = rng.range(0.0, 2.0 * M_PI), r = 0.8 * qSqrt(rng.unit());
                    dot(r * qCos(a), r * qSin(a), rng.range(0.06, 0.17));
                }
                break;
            }
        }
    }
    QMutexLocker locker(&lock);
    if (cache.size() > 512)
        cache.clear();
    cache.insert(key, out);
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
    if (!GoboLibrary::hasPicture(cap))
        return QString();
    return GoboLibrary::resolve(const_cast<QLCCapability *>(cap)->resource(0).toString());
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
            const bool wheel = hasPictures(ch) || !GoboLibrary::standInGlass(ch).isEmpty();
            if (wheel && cap != nullptr)
            {
                /* A wheel: the picture in the slot it is on, or -- where the
                   definition names a slot but has no usable picture for it
                   (none, a folder, a missing file) -- a generated stand-in,
                   unless the show fitted a picture. "Open" is the absence of
                   a gobo; a wheel spinning between slots shows none. */
                const QLCCapability *slot = cap;
                bool spinning = false;
                if (GoboLibrary::isRotationPreset(cap->preset()))
                    slot = nullptr;
                else if (GoboLibrary::isRotationSlotName(cap->name()))
                {
                    // "Gobo Rotate Speed": turns the glass before this range.
                    spinning = true;
                    slot = nullptr;
                    foreach (QLCCapability *c, GoboLibrary::standInCaps(ch))
                        if (c->min() < cap->min())
                            slot = c;
                }
                QImage mask;
                bool shake = false;
                if (slot != nullptr)
                {
                    const QString path = slotPicture(fx, i, slot, props);
                    if (!path.isEmpty())
                    {
                        if (!path.contains(QLatin1String("/open."), Qt::CaseInsensitive))
                            mask = goboMask(path);
                    }
                    else
                    {
                        const int idx = GoboLibrary::standInIndex(ch, slot);
                        if (idx >= 0)
                        {
                            mask = standInMask(slot->name(), idx);
                            shake = GoboLibrary::isShakeCap(slot);
                        }
                    }
                }
                Wheel w{ i, mask, 0.0 };
                if (!mask.isNull() && shake && !spinning)
                {
                    // A shake: a quick small wobble either way.
                    w.angleDeg = 9.0 * std::sin(seconds * 2.0 * M_PI * 3.0);
                    out.animated = true;
                }
                else if (!mask.isNull() && spinning)
                {
                    const double span = qMax(1, int(cap->max()) - int(cap->min()));
                    const double frac = qBound(0.0, (v - int(cap->min())) / span, 1.0);
                    const double speed = kSlow + (kFast - kSlow) * frac;
                    w.angleDeg = std::fmod(speed * seconds, 360.0);
                    out.animated = true;
                }
                wheels << w;
            }
            else if (wheel)
                wheels << Wheel{ i, QImage(), 0.0 };
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
