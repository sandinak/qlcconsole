/*
  Q Light Controller Plus
  encoderattributes.cpp

  Copyright (C) Branson Matheson

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

#include <QVector>
#include <QColor>
#include <QSet>
#include <cmath>
#include <climits>

#include "encoderattributes.h"
#include "qlccapability.h"
#include "encoderbank.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

namespace
{
const quint32 kNone = UINT_MAX;

/** One channel (a pair when it has a fine byte) of one fixture. */
struct Axis
{
    quint32 fixture = kNone;
    quint32 msb = kNone;
    quint32 lsb = kNone;
    const QLCChannel *channel = nullptr;
    bool selector = false;      //!< steps through the channel's named ranges
    bool centred = false;       //!< pan and tilt: home is the middle

    int range() const { return lsb != kNone ? 65535 : 255; }
};

/** The three emitters of one head. */
struct Trio
{
    quint32 fixture = kNone;
    quint32 ch[3] = { kNone, kNone, kNone };
    bool cmy = false;
};

bool nameHas(const QLCChannel *c, const char *word)
{
    return c->name().contains(QLatin1String(word), Qt::CaseInsensitive);
}

bool isFine(const QLCChannel *c)
{
    return c->controlByte() == QLCChannel::LSB;
}

/** Which Beam slot a channel is (-1: none). */
int beamSlot(const QLCChannel *c)
{
    if (isFine(c))
        return -1;
    const QLCChannel::Preset p = c->preset();
    const bool beam = c->group() == QLCChannel::Beam;
    if (p == QLCChannel::BeamZoomSmallBig || p == QLCChannel::BeamZoomBigSmall
        || (beam && nameHas(c, "zoom")))
        return 0;
    if (p == QLCChannel::BeamFocusNearFar || p == QLCChannel::BeamFocusFarNear
        || (beam && nameHas(c, "focus")))
        return 1;
    if (p == QLCChannel::ShutterIrisMinToMax || p == QLCChannel::ShutterIrisMaxToMin
        || ((beam || c->group() == QLCChannel::Shutter) && nameHas(c, "iris")))
        return 2;
    if (beam && nameHas(c, "frost"))
        return 3;
    return -1;
}

/** Which Gobo / prism slot a channel is (-1: none). */
int goboSlot(const QLCChannel *c)
{
    if (isFine(c))
        return -1;
    const QLCChannel::Preset p = c->preset();
    if (c->group() == QLCChannel::Gobo)
    {
        if (p == QLCChannel::GoboWheelFine || p == QLCChannel::GoboIndexFine)
            return -1;
        if (p == QLCChannel::GoboIndex || nameHas(c, "rotat") || nameHas(c, "index") || nameHas(c, "spin"))
            return 1;
        return 0;
    }
    if (c->group() == QLCChannel::Prism)
    {
        if (p == QLCChannel::PrismRotationSlowFast || p == QLCChannel::PrismRotationFastSlow
            || nameHas(c, "rotat"))
            return 3;
        return 2;
    }
    return -1;
}

/** The dimmer: the master, else the first plain intensity channel. */
quint32 dimmerOf(const Fixture *f)
{
    const quint32 master = f->masterIntensityChannel();
    if (master != QLCChannel::invalid())
        return master;
    for (quint32 i = 0; i < f->channels(); ++i)
    {
        const QLCChannel *c = f->channel(i);
        if (c != nullptr && c->group() == QLCChannel::Intensity && c->colour() == QLCChannel::NoColour
            && !isFine(c))
            return i;
    }
    return kNone;
}

QList<Trio> trios(const Fixture *f, quint32 fixtureId)
{
    QList<Trio> list;
    for (int h = 0; h < f->heads(); ++h)
    {
        Trio t;
        t.fixture = fixtureId;
        const QVector<quint32> rgb = f->rgbChannels(h);
        const QVector<quint32> cmy = rgb.size() == 3 ? QVector<quint32>() : f->cmyChannels(h);
        const QVector<quint32> &use = rgb.size() == 3 ? rgb : cmy;
        if (use.size() != 3)
            continue;
        t.cmy = rgb.size() != 3;
        for (int i = 0; i < 3; ++i)
            t.ch[i] = use.at(i);
        list << t;
    }
    return list;
}

/** The axes @a slot of @a bank is on @a f (not Hue / Saturation). */
QList<Axis> axesOf(const Fixture *f, quint32 id, int bank, int slot)
{
    QList<Axis> list;
    auto add = [&](quint32 msb, quint32 lsb, bool selector, bool centred) {
        if (msb == kNone || msb >= f->channels())
            return;
        Axis a;
        a.fixture = id;
        a.msb = msb;
        a.lsb = (lsb != kNone && lsb < f->channels()) ? lsb : kNone;
        a.channel = f->channel(msb);
        a.selector = selector;
        a.centred = centred;
        if (a.channel != nullptr)
            list << a;
    };

    switch (bank)
    {
    case EncoderBank::IntensityBank:
        add(dimmerOf(f), kNone, false, false);
        break;
    case EncoderBank::ColourBank:
        if (slot >= 2 && slot <= 4)
        {
            for (const Trio &t : trios(f, id))
                add(t.ch[slot - 2], kNone, false, false);
        }
        else if (slot >= 5 && slot <= 7)
        {
            const QLCChannel::PrimaryColour col = slot == 5 ? QLCChannel::White
                                                : slot == 6 ? QLCChannel::Amber : QLCChannel::UV;
            for (quint32 i = 0; i < f->channels(); ++i)
            {
                const QLCChannel *c = f->channel(i);
                if (c != nullptr && c->group() == QLCChannel::Intensity && c->colour() == col && !isFine(c))
                {
                    add(i, kNone, false, false);
                    break;
                }
            }
        }
        break;
    case EncoderBank::PositionBank:
    {
        const int type = slot == 0 ? QLCChannel::Pan : QLCChannel::Tilt;
        add(f->channelNumber(type, QLCChannel::MSB), f->channelNumber(type, QLCChannel::LSB), false, true);
        break;
    }
    case EncoderBank::BeamBank:
        for (quint32 i = 0; i < f->channels(); ++i)
        {
            const QLCChannel *c = f->channel(i);
            if (c != nullptr && beamSlot(c) == slot)
            {
                add(i, kNone, false, false);
                break;
            }
        }
        break;
    case EncoderBank::GoboBank:
        for (quint32 i = 0; i < f->channels(); ++i)
        {
            const QLCChannel *c = f->channel(i);
            if (c != nullptr && goboSlot(c) == slot)
            {
                add(i, kNone, slot == 0 || slot == 2, false);
                break;
            }
        }
        break;
    }
    return list;
}

bool isHueSat(int bank, int slot)
{
    return bank == EncoderBank::ColourBank && (slot == 0 || slot == 1);
}

int get16(Scene *scene, const Axis &a)
{
    const bool has = scene->checkValue(SceneValue(a.fixture, a.msb));
    int hi = has ? scene->value(a.fixture, a.msb) : a.channel->defaultValue();
    if (!has && a.centred && a.channel->defaultValue() == 0)
        hi = 127;
    if (a.lsb == kNone)
        return hi;
    const bool hasLo = scene->checkValue(SceneValue(a.fixture, a.lsb));
    const int lo = hasLo ? scene->value(a.fixture, a.lsb) : (has ? 0 : 255);
    return (hi << 8) | lo;
}

void set16(Scene *scene, const Axis &a, int v)
{
    v = qBound(0, v, a.range());
    if (a.lsb == kNone)
    {
        scene->setValue(a.fixture, a.msb, uchar(v));
        return;
    }
    scene->setValue(a.fixture, a.msb, uchar(v >> 8));
    scene->setValue(a.fixture, a.lsb, uchar(v & 0xFF));
}

/** Steps through the channel's named ranges: the value that @a detents
 *  ranges from the one @a v is in starts. */
int stepRanges(const QLCChannel *c, int v, int detents)
{
    QList<QLCCapability *> caps = c->capabilities();
    if (caps.size() < 2)
        return qBound(0, v + detents, 255);
    int at = 0;
    for (int i = 0; i < caps.size(); ++i)
        if (v >= caps.at(i)->min())
            at = i;
    // Part way into a range, the first turn down goes to the range's start.
    if (detents < 0 && v > caps.at(at)->min())
        ++detents;
    const int to = qBound(0, at + detents, int(caps.size()) - 1);
    return caps.at(to)->min();
}

/** Raw values of @a scene a look drives, as fixture << 32 | channel. */
QSet<quint64> drivenSet(Doc *doc, Scene *scene)
{
    QSet<quint64> set;
    if (scene == nullptr || scene->palettes().isEmpty())
        return set;
    for (quint32 pid : scene->palettes())
    {
        QLCPalette *pal = doc->palette(pid);
        if (pal == nullptr)
            continue;
        const int offset = QLCPalette::colorSetOffset(doc, scene->palettes(), pid);
        const QList<SceneValue> a = pal->valuesFromFixtureGroups(doc, scene->fixtureGroups(), nullptr, offset);
        const QList<SceneValue> b = pal->valuesFromFixtures(doc, scene->fixtures(), nullptr, offset);
        for (const SceneValue &v : a)
            set.insert((quint64(v.fxi) << 32) | v.channel);
        for (const SceneValue &v : b)
            set.insert((quint64(v.fxi) << 32) | v.channel);
    }
    return set;
}

quint64 keyOf(quint32 fx, quint32 ch) { return (quint64(fx) << 32) | ch; }

QColor colourOf(Scene *scene, const Trio &t)
{
    auto v = [&](int i) {
        const int raw = scene->checkValue(SceneValue(t.fixture, t.ch[i])) ? scene->value(t.fixture, t.ch[i]) : 0;
        return t.cmy ? 255 - raw : raw;
    };
    return QColor(v(0), v(1), v(2));
}

void setColour(Scene *scene, const Trio &t, const QColor &c)
{
    const int rgb[3] = { c.red(), c.green(), c.blue() };
    for (int i = 0; i < 3; ++i)
        scene->setValue(t.fixture, t.ch[i], uchar(t.cmy ? 255 - rgb[i] : rgb[i]));
}

bool trioDriven(const QSet<quint64> &driven, const Trio &t)
{
    for (int i = 0; i < 3; ++i)
        if (driven.contains(keyOf(t.fixture, t.ch[i])))
            return true;
    return false;
}
}

namespace EncoderAttributes
{

float step(int bank, int slot, bool fine)
{
    if (bank == EncoderBank::ColourBank && slot == 0)
        return fine ? 1.0f / 360.0f : 5.0f / 360.0f;       // 1 or 5 degrees of hue
    if (bank == EncoderBank::PositionBank)
        return fine ? 0.0005f : 0.005f;
    return fine ? 0.004f : 0.02f;
}

bool available(Doc *doc, const QList<quint32> &fixtures, int bank, int slot)
{
    for (quint32 id : fixtures)
    {
        const Fixture *f = doc->fixture(id);
        if (f == nullptr)
            continue;
        if (isHueSat(bank, slot) ? !trios(f, id).isEmpty() : !axesOf(f, id, bank, slot).isEmpty())
            return true;
        // A dimmerless colour fixture has intensity as the strongest colour.
        if (bank == EncoderBank::IntensityBank && !trios(f, id).isEmpty())
            return true;
    }
    return false;
}

bool read(Doc *doc, Scene *scene, const QList<quint32> &fixtures, int bank, int slot, float *norm)
{
    if (scene == nullptr || norm == nullptr)
        return false;
    for (quint32 id : fixtures)
    {
        const Fixture *f = doc->fixture(id);
        if (f == nullptr)
            continue;
        if (isHueSat(bank, slot))
        {
            const QList<Trio> list = trios(f, id);
            if (list.isEmpty())
                continue;
            float h, s, v;
            colourOf(scene, list.first()).getHsvF(&h, &s, &v);
            *norm = slot == 0 ? qMax(0.0f, h) : s;
            return true;
        }
        const QList<Axis> axes = axesOf(f, id, bank, slot);
        if (!axes.isEmpty())
        {
            *norm = float(get16(scene, axes.first())) / float(axes.first().range());
            return true;
        }
        if (bank == EncoderBank::IntensityBank)
        {
            const QList<Trio> list = trios(f, id);
            if (!list.isEmpty())
            {
                const QColor c = colourOf(scene, list.first());
                *norm = float(qMax(c.red(), qMax(c.green(), c.blue()))) / 255.0f;
                return true;
            }
        }
    }
    return false;
}

bool drivenByLook(Doc *doc, Scene *scene, quint32 fixture, quint32 channel)
{
    return drivenSet(doc, scene).contains(keyOf(fixture, channel));
}

namespace
{
/** Hue or saturation of every head, by @a detents (or home when @a go is 0). */
bool adjustHueSat(Doc *doc, Scene *scene, const QList<quint32> &fixtures, int slot, int detents,
                  bool fine, bool home, const QSet<quint64> &driven)
{
    bool wrote = false;
    for (quint32 id : fixtures)
    {
        const Fixture *f = doc->fixture(id);
        if (f == nullptr)
            continue;
        for (const Trio &t : trios(f, id))
        {
            if (trioDriven(driven, t))
                continue;
            float h, s, v;
            colourOf(scene, t).getHsvF(&h, &s, &v);
            if (v <= 0.0f)
                v = 1.0f;       // black shows nothing of a hue: bring it up
            if (slot == 0)
            {
                if (s <= 0.0f)
                    s = 1.0f;
                h = home ? 0.0f : (h < 0.0f ? 0.0f : h) + detents * step(EncoderBank::ColourBank, 0, fine);
                h -= std::floor(h);     // the wheel wraps
            }
            else
            {
                h = h < 0.0f ? 0.0f : h;
                s = home ? 1.0f : qBound(0.0f, s + detents * step(EncoderBank::ColourBank, 1, fine), 1.0f);
            }
            setColour(scene, t, QColor::fromHsvF(h, s, v));
            wrote = true;
        }
    }
    return wrote;
}
}

bool adjust(Doc *doc, Scene *scene, const QList<quint32> &fixtures, int bank, int slot, int detents, bool fine)
{
    if (scene == nullptr || detents == 0 || slot < 0 || slot >= EncoderBank::slotCount(bank))
        return false;
    const QSet<quint64> driven = drivenSet(doc, scene);

    if (isHueSat(bank, slot))
        return adjustHueSat(doc, scene, fixtures, slot, detents, fine, false, driven);

    bool wrote = false;
    for (quint32 id : fixtures)
    {
        const Fixture *f = doc->fixture(id);
        if (f == nullptr)
            continue;
        const QList<Axis> axes = axesOf(f, id, bank, slot);
        for (const Axis &a : axes)
        {
            if (driven.contains(keyOf(a.fixture, a.msb)))
                continue;
            const int cur = get16(scene, a);
            int to;
            if (a.selector && !fine)
                to = stepRanges(a.channel, cur, detents);
            else
                to = cur + detents * qMax(1, int(std::lround(step(bank, slot, fine) * a.range())));
            set16(scene, a, to);
            wrote = true;
        }
        if (axes.isEmpty() && bank == EncoderBank::IntensityBank)
        {
            // No dimmer: intensity is the strongest of the colour emitters.
            for (const Trio &t : trios(f, id))
            {
                if (trioDriven(driven, t))
                    continue;
                const QColor c = colourOf(scene, t);
                const int top = qMax(c.red(), qMax(c.green(), c.blue()));
                const int unit = qMax(1, int(std::lround(step(bank, slot, fine) * 255.0f)));
                const int to = qBound(0, top + detents * unit, 255);
                QColor n;
                if (top == 0)
                    n = QColor(to, to, to);
                else
                    n = QColor(int(std::lround(float(c.red()) * to / top)),
                               int(std::lround(float(c.green()) * to / top)),
                               int(std::lround(float(c.blue()) * to / top)));
                setColour(scene, t, n);
                wrote = true;
            }
        }
    }
    return wrote;
}

bool home(Doc *doc, Scene *scene, const QList<quint32> &fixtures, int bank, int slot)
{
    if (scene == nullptr || slot < 0 || slot >= EncoderBank::slotCount(bank))
        return false;
    const QSet<quint64> driven = drivenSet(doc, scene);
    if (isHueSat(bank, slot))
        return adjustHueSat(doc, scene, fixtures, slot, 0, false, true, driven);

    bool wrote = false;
    for (quint32 id : fixtures)
    {
        const Fixture *f = doc->fixture(id);
        if (f == nullptr)
            continue;
        const QList<Axis> axes = axesOf(f, id, bank, slot);
        for (const Axis &a : axes)
        {
            if (driven.contains(keyOf(a.fixture, a.msb)))
                continue;
            set16(scene, a, a.centred ? a.range() / 2 + 1 : int(a.channel->defaultValue()) << (a.lsb != kNone ? 8 : 0));
            wrote = true;
        }
        if (axes.isEmpty() && bank == EncoderBank::IntensityBank)
            for (const Trio &t : trios(f, id))
            {
                if (trioDriven(driven, t))
                    continue;
                setColour(scene, t, QColor(0, 0, 0));
                wrote = true;
            }
    }
    return wrote;
}

}
