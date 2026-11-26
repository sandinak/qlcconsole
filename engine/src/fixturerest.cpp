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
/** Whole words, lower case: "Vegas Sign" has no "gas", "Flameless
 *  candle" no "flame" (053p review). */
QStringList tokensOf(const QString &text)
{
    QStringList out;
    QString cur;
    for (const QChar c : text.toLower())
    {
        if (c.isLetterOrNumber())
            cur += c;
        else if (!cur.isEmpty())
        {
            out << cur;
            cur.clear();
        }
    }
    if (!cur.isEmpty())
        out << cur;
    return out;
}

bool hasWord(const QStringList &tokens, std::initializer_list<const char *> words)
{
    for (const char *w : words)
        if (tokens.contains(QLatin1String(w)))
            return true;
    return false;
}

/** "cold spark": the words one after the other */
bool hasPhrase(const QStringList &tokens, const char *first, const char *second)
{
    for (int i = 0; i + 1 < tokens.size(); ++i)
        if (tokens.at(i) == QLatin1String(first) && tokens.at(i + 1) == QLatin1String(second))
            return true;
    return false;
}

/** A definition's model run together ("Eco2Jet", "StadiumShot III"):
 *  its words are inside it. */
bool modelHas(const Fixture *fx, std::initializer_list<const char *> parts)
{
    if (fx->fixtureDef() == nullptr)
        return false;
    const QString m = fx->fixtureDef()->model().toLower();
    for (const char *p : parts)
        if (m.contains(QLatin1String(p)))
            return true;
    return false;
}

/** The fixture's own name and its definition's manufacturer and model, as
 *  words: what a guess reads. */
QStringList wordsOf(const Fixture *fx)
{
    QString t = fx->name();
    if (fx->fixtureDef() != nullptr)
        t += QLatin1Char(' ') + fx->fixtureDef()->manufacturer() + QLatin1Char(' ') + fx->fixtureDef()->model();
    return tokensOf(t);
}

/** A gel or colour name after "flame" ("Flame Red", Lee 164): a light. */
bool flameIsAColour(const QStringList &tokens)
{
    static const char *const colours[] = { "red", "orange", "yellow", "amber", "pink", "blue", "green",
                                           "gold", "magenta", "purple", "white" };
    for (int i = 0; i + 1 < tokens.size(); ++i)
        if (tokens.at(i) == QLatin1String("flame"))
            for (const char *c : colours)
                if (tokens.at(i + 1) == QLatin1String(c))
                    return true;
    return false;
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

namespace
{
Fixture::Behaviour guessUncached(const Fixture *fx);
}

Fixture::Behaviour FixtureRest::guessBehaviour(const Fixture *fx)
{
    if (fx == nullptr)
        return Fixture::LightBehaviour;
    const int kept = fx->cachedGuess();
    if (kept >= 0)
        return Fixture::Behaviour(kept);
    const Fixture::Behaviour b = guessUncached(fx);
    fx->setCachedGuess(int(b));
    return b;
}

namespace
{
Fixture::Behaviour guessUncached(const Fixture *fx)
{
    using namespace FixtureRest;
    // Scenery: the plot's word for it first, then what it is called.
    if (Scenery::isCurtainFixture(fx) || drivesScenery(fx))
        return Fixture::MachineryBehaviour;
    const QStringList names = wordsOf(fx);
    const QLCFixtureDef *def = fx->fixtureDef();
    const bool generic = def == nullptr || def->manufacturer() == KXMLFixtureGeneric;

    /* A light by its name or its definition ("Backdrop curtain wash", a
       moving head): never Machinery or a One-shot by name alone. A generic
       dimmer says nothing of what it feeds (a kabuki on a relay channel). */
    const bool lit = (!generic && fx->fixtureMode() != nullptr && fx->fixtureMode()->physical().emitsLight())
                     || hasWord(names, { "wash", "spot", "par", "pars", "light", "lights", "led", "lamp",
                                         "fresnel", "profile", "cyc", "strip", "beam", "flood", "pc" });
    if (!lit)
    {
        if (hasWord(names, { "kabuki" })
            || (hasWord(names, { "drop", "release" }) && hasWord(names, { "scrim", "curtain", "drape" })))
            return Fixture::OneShotBehaviour;
        if (hasWord(names, { "turntable", "revolve", "winch", "hoist", "scrim", "drape", "curtain" }))
            return Fixture::MachineryBehaviour;
    }

    // Something in the air, a flame or a gas: a hazard.
    if (def != nullptr && (def->type() == QLCFixtureDef::Hazer || def->type() == QLCFixtureDef::Smoke))
        return guessHazardKind(fx);
    if (QLCPhysical::isHazardousLaserClass(laserClass(fx)))
        return guessHazardKind(fx);
    // A special-effects maker's (MagicFX: confetti, CO2, flame), not its fan.
    if (def != nullptr && hasWord(tokensOf(def->manufacturer()), { "magicfx" })
        && def->type() != QLCFixtureDef::Fan)
        return guessHazardKind(fx);
    if (modelHas(fx, { "co2", "confetti", "streamer", "sparkular", "pyro" }))
        return guessHazardKind(fx);
    const bool silk = hasWord(names, { "silk" })
                      || (fx->fixtureMode() != nullptr
                          && fx->fixtureMode()->physical().emission() == QLCPhysical::FlameEmission);
    if (hasWord(names, { "co2", "confetti", "streamer", "streamers", "sparkular", "pyro", "pyrotechnic",
                         "pyrotechnics", "fog", "fogger", "foggers", "haze", "hazer", "hazers", "smoke",
                         "propane", "gas", "cannon", "cannons" })
        || hasPhrase(names, "cold", "spark") || hasPhrase(names, "spark", "machine")
        || (hasWord(names, { "flame", "flames", "flamethrower" }) && !silk && !flameIsAColour(names)))
        return guessHazardKind(fx);

    // A relay, house lights: a switch.
    const Fixture::OutputRules rules = fx->outputRules();
    if ((rules & Fixture::Switch) || (rules & Fixture::OnAtOpen) || hasWord(names, { "relay" }))
        return Fixture::SwitchBehaviour;
    return Fixture::LightBehaviour;
}
}

Fixture::Behaviour FixtureRest::guessHazardKind(const Fixture *fx)
{
    if (fx == nullptr)
        return Fixture::HazardBurstBehaviour;
    /* A burst first: a CO2 jet is a "Smoke" definition (MagicFX's Eco2Jet),
       confetti an "Effect". */
    const QStringList names = wordsOf(fx);
    if (modelHas(fx, { "co2", "confetti", "flame", "spark", "pyro", "stadium", "streamer", "blaster", "cannon" })
        || hasWord(names, { "co2", "confetti", "flame", "flames", "sparkular", "pyro", "cannon", "streamer" }))
        return Fixture::HazardBurstBehaviour;
    // Something that runs: in the air (fog, haze), or a beam (a laser).
    const QLCFixtureDef *def = fx->fixtureDef();
    if (def != nullptr && (def->type() == QLCFixtureDef::Hazer || def->type() == QLCFixtureDef::Smoke
                           || def->type() == QLCFixtureDef::Laser))
        return Fixture::HazardRunningBehaviour;
    if (laserClass(fx) != QLCPhysical::NoLaserClass)
        return Fixture::HazardRunningBehaviour;
    if (hasWord(names, { "fog", "fogger", "haze", "hazer", "smoke", "laser", "lasers" })
        || modelHas(fx, { "fog", "haze", "smoke", "laser" }))
        return Fixture::HazardRunningBehaviour;
    // A flame, a CO2 jet, confetti, a spark machine, pyro -- or unknown:
    // the deadman, the stricter of the two.
    return Fixture::HazardBurstBehaviour;
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
    return Fixture::isHazardBehaviour(int(behaviour(fx)));
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

namespace
{
/** restValue() with whether @p fx is a hazard worked out once (a stop
 *  walks every channel of every fixture). */
uchar restValueOf(const Fixture *fx, quint32 channel, bool off)
{
    const QLCChannel *ch = fx != nullptr ? fx->channel(channel) : nullptr;
    if (ch == nullptr)
        return 0;
    if (off || FixtureRest::isAtmosphereChannel(ch))
        return 0;
    return ch->defaultValue();
}

bool restsOff(const Fixture *fx)
{
    return FixtureRest::makesAtmosphere(fx) || FixtureRest::isLaser(fx) || FixtureRest::isHazard(fx);
}
}

uchar FixtureRest::restValue(const Fixture *fx, quint32 channel)
{
    return restValueOf(fx, channel, fx != nullptr && restsOff(fx));
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
        const bool off = restsOff(fx);
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
            unis.at(uni)->setChannelDefaultValue(ushort(abs & 511), restValueOf(fx, i, off));
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
