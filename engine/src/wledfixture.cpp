/*
  Q Light Controller Plus
  wledfixture.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "wledfixture.h"
#include "fixture.h"

namespace
{
const QLCChannel *effectChannel(const Fixture *fx)
{
    const int i = WledFixture::channel(fx, QStringLiteral("Effect"));
    return i < 0 ? nullptr : fx->fixtureMode()->channels().at(i);
}
}

bool WledFixture::runsEffects(const Fixture *fx)
{
    return fx != nullptr && fx->fixtureDef() != nullptr
           && fx->fixtureDef()->manufacturer() == QLatin1String("WLED")
           && effectChannel(fx) != nullptr;
}

bool WledFixture::isPixelString(const Fixture *fx)
{
    if (fx == nullptr)
        return false;
    const bool wled = (fx->connection() == Fixture::Network && fx->transport() == Fixture::WledTransport)
                      || (fx->fixtureDef() != nullptr
                          && fx->fixtureDef()->manufacturer().compare(QLatin1String("WLED"),
                                                                      Qt::CaseInsensitive) == 0);
    if (!wled)
        return false;
    // Driving a moving light: aimed like one.
    return fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB, 0) == QLCChannel::invalid()
           && fx->channelNumber(QLCChannel::Tilt, QLCChannel::MSB, 0) == QLCChannel::invalid();
}

int WledFixture::channel(const Fixture *fx, const QString &name)
{
    const QLCFixtureMode *mode = (fx != nullptr) ? fx->fixtureMode() : nullptr;
    if (mode == nullptr)
        return -1;
    for (int i = 0; i < mode->channels().size(); ++i)
        if (mode->channels().at(i)->name() == name)
            return i;
    return -1;
}

int WledFixture::effectValue(const Fixture *fx, const QString &effect)
{
    const QLCChannel *ch = effectChannel(fx);
    if (ch == nullptr)
        return -1;
    foreach (const QLCCapability *cap, ch->capabilities())
        if (cap->name().compare(effect, Qt::CaseInsensitive) == 0)
            return int(cap->min());
    return -1;
}

QStringList WledFixture::effectNames(const Fixture *fx)
{
    QStringList out;
    if (const QLCChannel *ch = effectChannel(fx))
    {
        foreach (const QLCCapability *cap, ch->capabilities())
            out << cap->name();
    }
    return out;
}

bool WledFixture::canFreeze(const Fixture *fx)
{
    return runsEffects(fx) && fx->transport() == Fixture::WledTransport;
}

QList<QPair<int, int> > WledFixture::settingsAtRest(const Fixture *fx)
{
    QList<QPair<int, int> > out;
    if (!runsEffects(fx))
        return out;
    static const struct { const char *name; int value; } rest[] = {
        { "Effect", 0 }, { "Effect Speed", 128 }, { "Effect Intensity", 128 },
        { "Palette", 0 }, { "Effect Option", 0 } };
    for (const auto &r : rest)
    {
        const int ch = channel(fx, QLatin1String(r.name));
        if (ch >= 0)
            out << qMakePair(ch, r.value);
    }
    return out;
}
