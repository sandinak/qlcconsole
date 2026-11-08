/*
  Q Light Controller Plus
  fixturerest.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QList>
#include <QTimer>

#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "qlcchannel.h"
#include "fixturerest.h"
#include "universe.h"
#include "genericfader.h"
#include "mastertimer.h"
#include "fixture.h"
#include "doc.h"

bool FixtureRest::makesAtmosphere(const Fixture *fx)
{
    if (fx == nullptr || fx->fixtureDef() == nullptr)
        return false;
    switch (fx->fixtureDef()->type())
    {
        case QLCFixtureDef::Hazer:
        case QLCFixtureDef::Smoke:
        case QLCFixtureDef::Fan:
            return true;
        default:
            return false;
    }
}

bool FixtureRest::isLaser(const Fixture *fx)
{
    return fx != nullptr && fx->fixtureDef() != nullptr
        && fx->fixtureDef()->type() == QLCFixtureDef::Laser;
}

bool FixtureRest::isAtmosphereChannel(const QLCChannel *ch)
{
    if (ch == nullptr)
        return false;
    const QString n = ch->name().toLower();
    return n.contains(QLatin1String("fog")) || n.contains(QLatin1String("haze"))
        || n.contains(QLatin1String("smoke"));
}

bool FixtureRest::holdsPosition(const QLCChannel *ch)
{
    return ch != nullptr
        && (ch->group() == QLCChannel::Pan || ch->group() == QLCChannel::Tilt);
}

uchar FixtureRest::restValue(const Fixture *fx, quint32 channel)
{
    const QLCChannel *ch = fx != nullptr ? fx->channel(channel) : nullptr;
    if (ch == nullptr)
        return 0;
    if (makesAtmosphere(fx) || isLaser(fx) || isAtmosphereChannel(ch))
        return 0;
    return ch->defaultValue();
}

int FixtureRest::putAllAtRest(Doc *doc)
{
    if (doc == nullptr)
        return 0;
    /* Not claimed: setChannelDefaultValue() takes each universe's frame
       lock, as a patch does, so a value lands between two frames. */
    const QList<Universe *> unis = doc->inputOutputMap()->universes();
    int written = 0;
    for (Fixture *fx : doc->fixtures())
    {
        if (fx == nullptr || !fx->isPatched())
            continue;
        const quint32 base = fx->universeAddress();
        for (quint32 i = 0; i < fx->channels(); ++i)
        {
            const quint32 abs = base + i;
            const int uni = int(abs >> 9);
            if (uni >= unis.size() || unis.at(uni) == nullptr)
                break;
            /* Pan and tilt stay where they are: a definition's default is
               usually 0, one end of travel, and a panic button must not
               swing every head across the stage. */
            if (holdsPosition(fx->channel(i)))
                continue;
            unis.at(uni)->setChannelDefaultValue(ushort(abs & 511), restValue(fx, i));
            ++written;
        }
    }
    return written;
}

namespace
{
/** A fader still on its way out (a scene's fade out, a Fade and stop). */
bool anyFadingOut(Doc *doc)
{
    for (Universe *u : doc->inputOutputMap()->universes())
    {
        if (u == nullptr)
            continue;
        for (const QSharedPointer<GenericFader> &f : u->faders())
            if (!f.isNull() && f->isFadingOut())
                return true;
    }
    return false;
}

/** At rest again once the last fade out is over -- a fading fader still
 *  writes the value it is leaving -- unless a cue started in the meantime:
 *  it owns the stage again. Gives up after @p triesLeft looks. */
void restWhenFadedOut(Doc *doc, int triesLeft)
{
    QTimer::singleShot(4 * int(MasterTimer::tick()), doc, [doc, triesLeft]() {
        if (doc->masterTimer()->runningFunctions() > 0)
            return;
        if (anyFadingOut(doc) && triesLeft > 0)
        {
            restWhenFadedOut(doc, triesLeft - 1);
            return;
        }
        FixtureRest::putAllAtRest(doc);
    });
}
}

void FixtureRest::stopAll(Doc *doc, int fadeMs)
{
    if (doc == nullptr)
        return;
    MasterTimer *mt = doc->masterTimer();
    // Both return once every function has stopped; their faders may still
    // be fading out (a scene's own fade out, or this one).
    if (fadeMs <= 0)
        mt->stopAllFunctions();
    else
        mt->fadeAndStopAll(fadeMs);
    /* At rest now: a fade out that has not started yet fades to the rest
       values (it fades an LTP channel to what is under it). And again once
       the fades are over, for one that had already started. Looked at for
       up to a minute and a half (a 60 s scene fade out). */
    putAllAtRest(doc);
    restWhenFadedOut(doc, 90000 / (4 * int(MasterTimer::tick())));
}
