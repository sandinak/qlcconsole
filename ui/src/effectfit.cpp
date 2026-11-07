/*
  Q Light Controller Plus
  effectfit.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QCoreApplication>
#include <QRegularExpression>
#include <QFileInfo>
#include <QFile>
#include <QHash>
#include <QSet>

#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "effectfit.h"
#include "fixture.h"
#include "doc.h"

#include "wledconfigcheck.h"
#include "freerotation.h"

#define tr(s) QCoreApplication::translate("EffectFit", s)

EffectFit::Caps EffectFit::capsOf(const Fixture *fixture)
{
    Caps c;
    if (fixture == nullptr)
        return c;
    c.wled = WledConfigCheck::isWled(fixture);
    const QLCFixtureMode *mode = fixture->fixtureMode();
    if (mode == nullptr)
        return c;
    c.heads = qMax(1, mode->heads().size());
    c.freeRotate = FreeRotation::canSpin(fixture);
    for (quint32 i = 0; i < fixture->channels(); ++i)
    {
        const QLCChannel *ch = mode->channel(i);
        if (ch == nullptr)
            continue;
        // The same reading the engine makes when it picks a script's fixtures.
        switch (ch->group())
        {
            case QLCChannel::Pan:
            case QLCChannel::Tilt:
                c.panTilt = true;
                break;
            case QLCChannel::Intensity:
            case QLCChannel::Colour:
                if (ch->colour() == QLCChannel::Red || ch->colour() == QLCChannel::Green
                    || ch->colour() == QLCChannel::Blue)
                    c.rgb = true;
                else if (ch->colour() == QLCChannel::UV)
                    c.uv = true;
                else if (ch->group() == QLCChannel::Colour)
                    c.colorWheel = true;
                else
                    c.dimmer = true;
                break;
            case QLCChannel::Shutter:
                c.shutter = true;
                break;
            case QLCChannel::Gobo:
                c.gobo = true;
                break;
            default:
                break;
        }
        const QLCChannel::Preset p = ch->preset();
        const QString n = ch->name();
        if (p == QLCChannel::BeamZoomSmallBig || p == QLCChannel::BeamZoomBigSmall
            || p == QLCChannel::BeamZoomFine || p == QLCChannel::BeamFocusNearFar
            || p == QLCChannel::BeamFocusFarNear || p == QLCChannel::BeamFocusFine
            || p == QLCChannel::ShutterIrisMinToMax || p == QLCChannel::ShutterIrisMaxToMin
            || p == QLCChannel::ShutterIrisFine
            || (ch->group() == QLCChannel::Beam
                && (n.contains(QLatin1String("zoom"), Qt::CaseInsensitive)
                    || n.contains(QLatin1String("iris"), Qt::CaseInsensitive)
                    || n.contains(QLatin1String("focus"), Qt::CaseInsensitive))))
            c.beam = true;
    }
    return c;
}

bool EffectFit::Targets::anyWled() const
{
    for (const Caps &c : fixtures)
        if (c.wled)
            return true;
    return false;
}

int EffectFit::Targets::totalHeads() const
{
    int n = 0;
    for (const Caps &c : fixtures)
        n += c.heads;
    return n;
}

EffectFit::Targets EffectFit::targetsOf(Doc *doc, const QList<quint32> &fixtureIds)
{
    Targets t;
    QSet<quint32> seen;
    for (quint32 id : fixtureIds)
    {
        if (seen.contains(id))
            continue;
        seen.insert(id);
        if (const Fixture *f = doc != nullptr ? doc->fixture(id) : nullptr)
            t.fixtures << capsOf(f);
    }
    return t;
}

EffectFit::Need EffectFit::wledNeed()
{
    Need n;
    n.wled = true;
    return n;
}

EffectFit::Need EffectFit::scriptNeed(const QStringList &fixtureTypes, const QString &scriptPath,
                                      const QString &wledPort)
{
    Need n;
    n.wled = !wledPort.isEmpty();
    for (const QString &t : fixtureTypes)
        n.types << t.toLower();
    n.matrix = n.types.contains(QLatin1String("matrix"));
    n.types.removeAll(QStringLiteral("matrix"));

    /* What it emits says more than what it declares: a gobo chase is
       declared for moving heads, and needs a gobo wheel on them. Read once
       per file version. */
    struct Intents { qint64 stamp = -1; bool gobo = false; bool beam = false; };
    static QHash<QString, Intents> cache;
    if (!scriptPath.isEmpty())
    {
        const QFileInfo fi(scriptPath);
        const qint64 stamp = fi.lastModified().toMSecsSinceEpoch() ^ fi.size();
        Intents in = cache.value(scriptPath);
        if (in.stamp != stamp)
        {
            in = Intents();
            in.stamp = stamp;
            QFile f(scriptPath);
            if (f.open(QIODevice::ReadOnly))
            {
                const QString src = QString::fromUtf8(f.readAll());
                static const QRegularExpression gobo(QStringLiteral("[{,]\\s*gobo\\s*:"));
                static const QRegularExpression beam(QStringLiteral("[{,]\\s*(iris|zoom|focus)\\s*:"));
                in.gobo = gobo.match(src).hasMatch();
                in.beam = beam.match(src).hasMatch();
            }
            cache.insert(scriptPath, in);
        }
        n.gobo = in.gobo;
        n.beam = !in.gobo && in.beam;
    }
    return n;
}

bool EffectFit::typesMatch(const QStringList &types, const Caps &c)
{
    if (types.isEmpty())
        return true;                            // no types declared: any fixture
    for (const QString &tl : types)
    {
        if ((tl == QLatin1String("rgb") || tl == QLatin1String("rgbw") || tl == QLatin1String("rgba")
             || tl == QLatin1String("rgbwa") || tl == QLatin1String("rgbwauv") || tl == QLatin1String("wash")
             || tl == QLatin1String("color")) && c.rgb)
            return true;
        if ((tl == QLatin1String("moving") || tl == QLatin1String("mover") || tl == QLatin1String("movers")
             || tl == QLatin1String("pantilt") || tl == QLatin1String("spot") || tl == QLatin1String("beam"))
            && c.panTilt)
            return true;
        if (tl == QLatin1String("dimmer") && c.dimmer)
            return true;
        if (tl == QLatin1String("freerotate") && c.freeRotate)
            return true;
        if (tl == QLatin1String("shutter") && c.shutter)
            return true;
        if (tl == QLatin1String("colorwheel") && c.colorWheel)
            return true;
        if ((tl == QLatin1String("uv") || tl == QLatin1String("uvonly")) && c.uv)
            return true;
    }
    return false;
}

EffectFit::Result EffectFit::fit(const Need &need, const Targets &targets)
{
    Result r;
    r.of = targets.fixtures.size();
    if (!targets.known())
    {
        r.fits = true;
        return r;                               // nothing to judge by yet
    }

    const bool grid = targets.totalHeads() >= 2;
    for (const Caps &c : targets.fixtures)
    {
        bool ok;
        if (need.wled)
            ok = c.wled;
        else if (need.matrix)
            ok = grid && (c.rgb || c.dimmer || c.colorWheel);
        else if (need.gobo)
            ok = c.gobo && typesMatch(need.types, c);
        else if (need.beam)
            ok = c.beam && typesMatch(need.types, c);
        else
            ok = typesMatch(need.types, c);
        if (ok)
            ++r.runsOn;
    }
    r.fits = r.runsOn > 0;

    if (r.fits)
    {
        if (r.runsOn < r.of && need.types == QStringList(QStringLiteral("freerotate")))
            r.why = tr("Runs on %1 of %2 fixtures (%3 have no free rotation)")
                    .arg(r.runsOn).arg(r.of).arg(r.of - r.runsOn);
        else if (r.runsOn < r.of)
            r.why = tr("Runs on %1 of %2 fixtures").arg(r.runsOn).arg(r.of);
        return r;
    }

    if (need.wled)
        r.why = tr("Needs a WLED device; none of these fixtures is one");
    else if (need.matrix)
        r.why = grid ? tr("Needs colour or a dimmer to drive; these fixtures have neither")
                     : tr("Needs a grid of heads (a pixel bar, or several fixtures together)");
    else if (need.gobo)
        r.why = tr("Needs a gobo wheel; none of these fixtures has one");
    else if (need.beam)
        r.why = tr("Needs zoom, iris or focus; none of these fixtures has one");
    else
    {
        QStringList what;
        for (const QString &t : need.types)
        {
            QString w;
            if (t == QLatin1String("moving") || t == QLatin1String("mover") || t == QLatin1String("movers")
                || t == QLatin1String("pantilt") || t == QLatin1String("spot") || t == QLatin1String("beam"))
                w = tr("pan/tilt heads");
            else if (t == QLatin1String("dimmer"))
                w = tr("a dimmer");
            else if (t == QLatin1String("freerotate"))
                w = tr("free (endless) pan or tilt rotation");
            else if (t == QLatin1String("colorwheel"))
                w = tr("a colour wheel");
            else if (t == QLatin1String("shutter"))
                w = tr("a shutter");
            else if (t == QLatin1String("uv") || t == QLatin1String("uvonly"))
                w = tr("a UV channel");
            else
                w = tr("colour mixing (RGB)");
            if (!what.contains(w))
                what << w;
        }
        r.why = tr("Needs %1; none of these fixtures has it").arg(what.join(tr(" or ")));
    }
    return r;
}
