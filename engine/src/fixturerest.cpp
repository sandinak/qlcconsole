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
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "fixturerest.h"
#include "outputrulekeeper.h"
#include "scenery.h"
#include "monitorproperties.h"
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

namespace
{
bool hasAny(const QString &text, std::initializer_list<const char *> words)
{
    for (const char *w : words)
        if (text.contains(QLatin1String(w)))
            return true;
    return false;
}

/** The fixture's own name, its definition's model and its mode's channel
 *  names, lower case: what a guess reads words in. */
QString wordsOf(const Fixture *fx, bool channels)
{
    QString t = fx->name();
    if (fx->fixtureDef() != nullptr)
        t += QLatin1Char(' ') + fx->fixtureDef()->model();
    if (channels && fx->fixtureMode() != nullptr)
        for (const QLCChannel *ch : fx->fixtureMode()->channels())
            if (ch != nullptr)
                t += QLatin1Char(' ') + ch->name();
    return t.toLower();
}

bool drivesScenery(const Fixture *fx)
{
    Doc *doc = qobject_cast<Doc *>(fx->parent());
    if (doc == nullptr || doc->monitorProperties() == nullptr)
        return false;
    for (const Scenery *sc : doc->monitorProperties()->sceneries())
        if (sc != nullptr && sc->driverFixture() == fx->id())
            return true;
    return false;
}
}

QLCPhysical::LaserClass FixtureRest::laserClass(const Fixture *fx)
{
    if (fx == nullptr)
        return QLCPhysical::NoLaserClass;
    /* The unit's own label first (Fixture Properties' Laser class: 1, 2,
       3R, 3B, 4), then its definition's. */
    if (Doc *doc = qobject_cast<Doc *>(fx->parent()))
        if (doc->monitorProperties() != nullptr)
            switch (doc->monitorProperties()->fixtureRigProps(fx->id()).laserClass)
            {
                case 1: return QLCPhysical::LaserClass1;
                case 2: return QLCPhysical::LaserClass2;
                case 3: return QLCPhysical::LaserClass3R;
                case 4: return QLCPhysical::LaserClass3B;
                case 5: return QLCPhysical::LaserClass4;
                default: break;
            }
    if (fx->fixtureMode() != nullptr)
        return fx->fixtureMode()->physical().laserClass();
    return fx->fixtureDef() != nullptr ? fx->fixtureDef()->physical().laserClass()
                                       : QLCPhysical::NoLaserClass;
}

Fixture::Behaviour FixtureRest::guessBehaviour(const Fixture *fx)
{
    if (fx == nullptr)
        return Fixture::LightBehaviour;
    // Scenery: the plot's word for it first, then what it is called.
    if (Scenery::isCurtainFixture(fx) || drivesScenery(fx))
        return Fixture::MachineryBehaviour;
    const QString names = wordsOf(fx, false);
    const bool lit = fx->fixtureMode() == nullptr || fx->fixtureMode()->physical().emitsLight();
    if (hasAny(names, { "kabuki" }) || (hasAny(names, { "drop", "release" }) && hasAny(names, { "scrim", "curtain", "drape" })))
        return Fixture::OneShotBehaviour;
    if (hasAny(names, { "turntable", "revolve", "winch", "hoist", "scrim", "drape", "curtain" }) && !lit)
        return Fixture::MachineryBehaviour;

    // Something in the air, a flame or a gas: a hazard.
    const QLCFixtureDef *def = fx->fixtureDef();
    if (def != nullptr && (def->type() == QLCFixtureDef::Hazer || def->type() == QLCFixtureDef::Smoke))
        return Fixture::HazardBehaviour;
    if (QLCPhysical::isHazardousLaserClass(laserClass(fx)))
        return Fixture::HazardBehaviour;
    const bool silk = names.contains(QLatin1String("silk"))
                      || (fx->fixtureMode() != nullptr
                          && fx->fixtureMode()->physical().emission() == QLCPhysical::FlameEmission);
    if (hasAny(names, { "co2", "confetti", "sparkular", "cold spark", "spark machine", "pyro",
                        "fog", "haze", "smoke", "propane", "gas " })
        || (names.contains(QLatin1String("flame")) && !silk))
        return Fixture::HazardBehaviour;

    // A relay, house lights: a switch.
    const Fixture::OutputRules rules = fx->outputRules();
    if ((rules & Fixture::Switch) || (rules & Fixture::OnAtOpen) || hasAny(names, { "relay", "house" }))
        return Fixture::SwitchBehaviour;
    return Fixture::LightBehaviour;
}

Fixture::Behaviour FixtureRest::behaviour(const Fixture *fx)
{
    if (fx == nullptr)
        return Fixture::LightBehaviour;
    const int set = fx->behaviourSetting();
    return set >= 0 ? Fixture::Behaviour(set) : guessBehaviour(fx);
}

bool FixtureRest::isHazardousLaserAsLight(const Fixture *fx)
{
    return fx != nullptr && fx->behaviourOverridden()
        && fx->behaviourSetting() == int(Fixture::LightBehaviour)
        && QLCPhysical::isHazardousLaserClass(laserClass(fx));
}

bool FixtureRest::isHazard(const Fixture *fx)
{
    return behaviour(fx) == Fixture::HazardBehaviour;
}

bool FixtureRest::freezesOnStop(const Fixture *fx)
{
    if (fx == nullptr)
        return false;
    const Fixture::Behaviour b = behaviour(fx);
    return b == Fixture::MachineryBehaviour || b == Fixture::OneShotBehaviour
        || b == Fixture::SwitchBehaviour;
}

bool FixtureRest::holdsPosition(const Doc *doc, quint32 fixtureId, quint32 channel)
{
    const Fixture *fx = doc != nullptr ? doc->fixture(fixtureId) : nullptr;
    return fx != nullptr && holdsPosition(fx->channel(channel));
}

bool FixtureRest::holdsPositionAt(const Doc *doc, quint32 universeAddress)
{
    if (doc == nullptr)
        return false;
    const Fixture *fx = doc->fixture(doc->fixtureForAddress(universeAddress));
    if (fx == nullptr || universeAddress < fx->universeAddress())
        return false;
    return holdsPosition(fx->channel(universeAddress - fx->universeAddress()));
}

uchar FixtureRest::restValue(const Fixture *fx, quint32 channel)
{
    const QLCChannel *ch = fx != nullptr ? fx->channel(channel) : nullptr;
    if (ch == nullptr)
        return 0;
    if (makesAtmosphere(fx) || isLaser(fx) || isAtmosphereChannel(ch) || isHazard(fx))
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
        // A curtain, a relay, a drop stays where it is: its rest is a move.
        if (freezesOnStop(fx))
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

void FixtureRest::holdShowOnlyHeads(Doc *doc)
{
    if (doc == nullptr || doc->inputOutputMap() == nullptr)
        return;
    // The universes' own snapshot: safe from the thread a stop comes on.
    for (Universe *u : doc->inputOutputMap()->universesSnapshot())
        if (u != nullptr && u->showOnlyChannelCount() > 0)
            u->holdShowOnlyPositions();
}

void FixtureRest::stopAll(Doc *doc, int fadeMs)
{
    if (doc == nullptr)
        return;
    /* Q326: timed or at once, a Show-only head stays where it is -- held
       before anything stops, so a fade out fades its pan and tilt to where
       they are (nothing) while the rest fades to safe. */
    holdShowOnlyHeads(doc);
    // And every curtain where it is, mid-move too, before its cue lets go.
    if (OutputRuleKeeper *rules = doc->outputRuleKeeper())
        rules->holdStageMachinery();
    MasterTimer *mt = doc->masterTimer();
    // Both return once every function has stopped; their faders may still
    // be fading out (a scene's own fade out, or this one).
    if (fadeMs <= 0)
        mt->stopAllFunctions();
    else
        mt->fadeAndStopAll(fadeMs);
    restAfterStop(doc);
}

void FixtureRest::restAfterStop(Doc *doc)
{
    if (doc == nullptr)
        return;
    /* At rest now: a fade out that has not started yet fades to the rest
       values (it fades an LTP channel to what is under it). And again once
       the fades are over, for one that had already started. Looked at for
       up to a minute and a half (a 60 s scene fade out). */
    putAllAtRest(doc);
    restWhenFadedOut(doc, 90000 / (4 * int(MasterTimer::tick())));
}
