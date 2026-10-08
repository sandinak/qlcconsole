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
