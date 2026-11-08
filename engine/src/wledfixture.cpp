/*
  Q Light Controller Plus
  wledfixture.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <climits>

#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcfixturedef.h"
#include "qlcphysical.h"
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

/*********************************************************************
 * Pixels
 *********************************************************************/

namespace
{
const char *GapPrefix = "Gap ";

/* A definition's modes (QLCFixtureDef::modes() is not const). */
QList<QLCFixtureMode *> modesOf(const QLCFixtureDef *def)
{
    return const_cast<QLCFixtureDef *>(def)->modes();
}
}

QString WledFixture::pixelModeName(bool rgbw)
{
    return rgbw ? QStringLiteral("Pixels (Multi RGBW)") : QStringLiteral("Pixels (Multi RGB)");
}

bool WledFixture::isPixelModeName(const QString &name, bool *rgbw)
{
    if (name == pixelModeName(false))
    {
        if (rgbw != nullptr)
            *rgbw = false;
        return true;
    }
    if (name == pixelModeName(true))
    {
        if (rgbw != nullptr)
            *rgbw = true;
        return true;
    }
    return false;
}

bool WledFixture::isPixelMode(const Fixture *fx)
{
    return fx != nullptr && fx->fixtureDef() != nullptr && fx->fixtureMode() != nullptr
           && fx->fixtureDef()->manufacturer().compare(QLatin1String("WLED"), Qt::CaseInsensitive) == 0
           && isPixelModeName(fx->fixtureMode()->name());
}

bool WledFixture::isRgbw(const Fixture *fx)
{
    bool rgbw = false;
    return fx != nullptr && fx->fixtureMode() != nullptr
           && isPixelModeName(fx->fixtureMode()->name(), &rgbw) && rgbw;
}

int WledFixture::dmxModeFor(const Fixture *fx)
{
    const QString name = (fx != nullptr && fx->fixtureMode() != nullptr) ? fx->fixtureMode()->name()
                                                                         : QString();
    bool rgbw = false;
    if (isPixelModeName(name, &rgbw))
        return rgbw ? DmxMultiRgbw : DmxMultiRgb;
    if (name.startsWith(QLatin1String("Effect")))
        return DmxEffect;
    if (name.startsWith(QLatin1String("Single DRGB")))
        return DmxSingleDrgb;
    if (name.startsWith(QLatin1String("Single RGB")))
        return DmxSingleRgb;
    return -1;
}

int WledFixture::ledsPerUniverse(bool rgbw)
{
    return rgbw ? 128 : 170;
}

WledFixture::UniverseSpan WledFixture::universeSpan(int leds, bool rgbw, quint32 address)
{
    UniverseSpan span;
    const int per = rgbw ? 4 : 3;
    leds = qMax(0, leds);
    address = qMin(address, quint32(511));
    const int first = int(512 - address) / per;
    span.firstUniverseLeds = first;
    if (leds <= first)
    {
        span.leds << leds;
        span.channels = leds * per;
        span.firstUniverseLeds = 0;     // one run: no gaps
        return span;
    }
    span.leds << first;
    int left = leds - first;
    span.channels = int(512 - address);
    while (left > 0)
    {
        const int here = qMin(left, ledsPerUniverse(rgbw));
        span.leds << here;
        left -= here;
        span.channels += left > 0 ? 512 : here * per;
    }
    return span;
}

bool WledFixture::sendsUniverses(const Fixture *fx)
{
    /* Every transport this show has today sends 512-channel universes
       (DMX, Art-Net, sACN; the WLED JSON output reads its fixture's channels
       out of one). A DDP output -- WLED's own pixel protocol, no universes --
       is where this would answer false. */
    Q_UNUSED(fx)
    return true;
}

int WledFixture::universeCount(const Fixture *fx)
{
    if (fx == nullptr || fx->channels() == 0)
        return 1;
    const quint32 first = fx->universeAddress();
    const quint32 last = first + fx->channels() - 1;
    return int((last >> 9) - (first >> 9)) + 1;
}

QLCFixtureDef *WledFixture::pixelDefinition(const QLCFixtureDef *like, int leds, bool rgbw,
                                            int universeAddress)
{
    leds = qBound(1, leds, MaxPixels);
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(like != nullptr ? like->manufacturer() : QStringLiteral("WLED"));
    def->setModel(like != nullptr ? like->model() : QStringLiteral("Pixels"));
    def->setType(like != nullptr ? like->type() : QLCFixtureDef::LEDBarPixels);
    def->setAuthor(like != nullptr ? like->author() : QStringLiteral("qlcconsole"));

    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(pixelModeName(rgbw));

    const int per = rgbw ? 4 : 3;
    const QLCChannel::PrimaryColour colours[] = { QLCChannel::Red, QLCChannel::Green,
                                                  QLCChannel::Blue, QLCChannel::White };
    const char *names[] = { "Red", "Green", "Blue", "White" };
    QList<QLCChannel *> all;
    QVector<QLCFixtureHead> heads;
    heads.reserve(leds);
    /* Over universes: the LED the first one ends before, then every
       ledsPerUniverse() more. One run (or one that fits): never. */
    int next = -1;
    if (universeAddress >= 0)
    {
        const UniverseSpan span = universeSpan(leds, rgbw, quint32(universeAddress));
        if (span.leds.size() > 1)
            next = span.firstUniverseLeds;
    }
    int gaps = 0;
    int pos = qMax(0, universeAddress);         // where in its universe the next channel is
    for (int led = 0; led < leds; ++led)
    {
        if (led == next)
        {
            /* The rest of this universe: WLED starts the next LED on the
               next universe's first channel. */
            const int fill = (512 - pos % 512) % 512;
            for (int g = 0; g < fill; ++g)
            {
                QLCChannel *gap = new QLCChannel();
                gap->setName(QStringLiteral("%1%2").arg(QLatin1String(GapPrefix)).arg(++gaps));
                gap->setGroup(QLCChannel::Nothing);
                gap->addCapability(new QLCCapability(0, UCHAR_MAX, QStringLiteral("Unused")));
                all << gap;
            }
            pos += fill;
            next += ledsPerUniverse(rgbw);
        }
        QLCFixtureHead head;
        for (int c = 0; c < per; ++c)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QStringLiteral("%1 %2").arg(QLatin1String(names[c])).arg(led + 1));
            ch->setGroup(QLCChannel::Intensity);
            ch->setColour(colours[c]);
            ch->addCapability(new QLCCapability(0, UCHAR_MAX, QLatin1String(names[c])));
            head.addChannel(quint32(all.size()));
            all << ch;
        }
        pos += per;
        heads << head;
    }
    // Straight into the lists: addChannel()/insertChannel() search them each
    // time, which is slow for thousands of channels.
    for (int i = 0; i < all.size(); ++i)
        def->addChannel(all.at(i));
    for (int i = 0; i < all.size(); ++i)
        mode->insertChannel(all.at(i), quint32(i));
    for (const QLCFixtureHead &h : heads)
        mode->insertHead(-1, h);

    QLCPhysical phys;
    if (like != nullptr && !modesOf(like).isEmpty())
        phys = modesOf(like).first()->physical();
    else
        phys = mode->physical();
    /* Its LEDs are its heads now, lit by what is sent to each: not the
       body's own LEDs, which stand for what the device draws itself. */
    phys.setLeds(0);
    mode->setPhysical(phys);
    def->addMode(mode);
    return def;
}

bool WledFixture::isPixelDefinition(const QLCFixtureDef *def)
{
    return def != nullptr && def->definitionSourceFile().isEmpty()
           && def->manufacturer().compare(QLatin1String("WLED"), Qt::CaseInsensitive) == 0
           && modesOf(def).size() == 1 && isPixelModeName(modesOf(def).first()->name());
}

QLCFixtureDef *WledFixture::copyPixelDefinition(const QLCFixtureDef *def)
{
    if (!isPixelDefinition(def))
        return nullptr;
    const QLCFixtureMode *mode = modesOf(def).first();
    bool rgbw = false;
    isPixelModeName(mode->name(), &rgbw);
    /* Its first gap says where in its universe it starts: the first
       universe's LEDs and gap fill it to channel 512. No gap: one run. */
    int address = -1;
    const QVector<QLCChannel *> chans = mode->channels();
    for (int i = 0; i < chans.size(); ++i)
    {
        if (!chans.at(i)->name().startsWith(QLatin1String(GapPrefix)))
            continue;
        int g = i;
        while (g < chans.size() && chans.at(g)->name().startsWith(QLatin1String(GapPrefix)))
            ++g;
        address = 512 - g;
        break;
    }
    if (address < 0)
    {
        /* No gap: either one run, or RGBW from address 1 (128 LEDs fill a
           universe exactly). Laid out from address 1 again, which gives the
           same channels for either. */
        address = 0;
    }
    QLCFixtureDef *copy = pixelDefinition(def, mode->heads().size(), rgbw, address);
    // Its body as it was (pixelDefinition() takes the one it is like).
    copy->modes().first()->setPhysical(mode->physical());
    return copy;
}

int WledFixture::ledsForChannels(int channels, bool rgbw, quint32 address, bool universes)
{
    const int per = rgbw ? 4 : 3;
    if (!universes)
        return qMax(1, channels / per);
    int n = qMax(1, channels / per);
    while (n > 1 && universeSpan(n, rgbw, address).channels > channels)
        --n;
    return n;
}

bool WledFixture::applyPixels(Fixture *fx, const QLCFixtureDef *like, int leds, bool rgbw)
{
    if (fx == nullptr)
        return false;
    if (like == nullptr)
        like = fx->fixtureDef();
    if (like == nullptr)
        return false;
    const bool universes = sendsUniverses(fx);
    const UniverseSpan span = universeSpan(leds, rgbw, fx->address());
    QLCFixtureDef *def = pixelDefinition(like, leds, rgbw, universes ? int(fx->address()) : -1);
    if (like == fx->fixtureDef() && isPixelDefinition(like))
        def->modes().first()->setPhysical(fx->fixtureMode()->physical());
    fx->adoptFixtureDefinition(def, def->modes().first());
    fx->setCrossUniverse(universes && span.leds.size() > 1);
    return true;
}

void WledFixture::refitPixels(Fixture *fx)
{
    if (!isPixelMode(fx) || !isPixelDefinition(fx->fixtureDef()))
        return;
    const int leds = fx->heads();
    const bool rgbw = isRgbw(fx);
    const UniverseSpan want = universeSpan(leds, rgbw, fx->address());
    if (int(fx->channels()) == want.channels && fx->crossUniverse() == (want.leds.size() > 1))
        return;
    applyPixels(fx, fx->fixtureDef(), leds, rgbw);
}
