/*
  Q Light Controller Plus
  freerotation.cpp

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

#include <QRegularExpression>
#include <QtMath>

#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlccapability.h"
#include "freerotation.h"
#include "qlcchannel.h"
#include "fixture.h"

namespace
{
enum Way { NoWay, Clockwise, CounterClockwise };

/* What one capability says about turning the head. */
struct CapReading
{
    Way preset = NoWay;         // from its preset
    Way words = NoWay;          // from its name
    bool turns = false;         // it spins the head (either reading)
    bool slowToFast = true;
    bool stop = false;
    bool indexed = false;
    bool points = false;        // it points the head ("Fixed position (0° to 540°)")
    double rpmLo = 0.0, rpmHi = 0.0;   // at its low and high DMX ends, from its name
};

const QRegularExpression &turnWords()
{
    static const QRegularExpression re(QStringLiteral("rotat|spin|continu|endless|infinit|non-?stop"),
                                       QRegularExpression::CaseInsensitiveOption);
    return re;
}

Way wayInWords(const QString &name)
{
    static const QRegularExpression ccw(QStringLiteral("counter|anti|\\bccw\\b|backward|reverse|\\bleft\\b"),
                                        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression cw(QStringLiteral("clockwise|\\bcw\\b|forward|\\bright\\b"),
                                       QRegularExpression::CaseInsensitiveOption);
    if (ccw.match(name).hasMatch())
        return CounterClockwise;
    if (cw.match(name).hasMatch())
        return Clockwise;
    return NoWay;
}

/* "(fast 180rpm to slow 4.4rph)": the first figure is the low DMX end's. */
void rpmInWords(const QString &name, double &lo, double &hi)
{
    static const QRegularExpression re(QStringLiteral("(\\d+(?:[.,]\\d+)?)\\s*(rpm|rph|r/min)"),
                                       QRegularExpression::CaseInsensitiveOption);
    QList<double> found;
    QRegularExpressionMatchIterator it = re.globalMatch(name);
    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        double v = m.captured(1).replace(QLatin1Char(','), QLatin1Char('.')).toDouble();
        if (m.captured(2).compare(QLatin1String("rph"), Qt::CaseInsensitive) == 0)
            v /= 60.0;
        found << v;
    }
    if (found.size() >= 2)
    {
        lo = found.at(0);
        hi = found.at(1);
    }
}

CapReading readCap(const QLCCapability *cap)
{
    CapReading r;
    const QString name = cap->name();
    switch (cap->preset())
    {
        case QLCCapability::RotationClockwise:
        case QLCCapability::RotationClockwiseSlowToFast:
            r.preset = Clockwise;
            break;
        case QLCCapability::RotationClockwiseFastToSlow:
            r.preset = Clockwise;
            r.slowToFast = false;
            break;
        case QLCCapability::RotationCounterClockwise:
        case QLCCapability::RotationCounterClockwiseSlowToFast:
            r.preset = CounterClockwise;
            break;
        case QLCCapability::RotationCounterClockwiseFastToSlow:
            r.preset = CounterClockwise;
            r.slowToFast = false;
            break;
        case QLCCapability::RotationStop:
            r.stop = true;
            return r;
        case QLCCapability::RotationIndexed:
            r.indexed = true;
            return r;
        case QLCCapability::FastToSlow:
            r.slowToFast = false;
            break;
        default:
            break;
    }

    const bool turnsInWords = turnWords().match(name).hasMatch();
    static const QRegularExpression pointWords(QStringLiteral("position|fixed|indexed"),
                                               QRegularExpression::CaseInsensitiveOption);
    if (r.preset == NoWay && !turnsInWords && pointWords.match(name).hasMatch())
    {
        r.points = true;
        return r;
    }
    if (r.preset == NoWay && !turnsInWords)
    {
        static const QRegularExpression stopWords(QStringLiteral("\\bstop|no rotation|no movement"),
                                                  QRegularExpression::CaseInsensitiveOption);
        r.stop = stopWords.match(name).hasMatch();
        return r;
    }
    static const QRegularExpression stopped(QStringLiteral("\\bstop|\\bno (rotation|movement|function)"),
                                            QRegularExpression::CaseInsensitiveOption);
    if (r.preset == NoWay && stopped.match(name).hasMatch())
    {
        r.stop = true;
        return r;
    }
    r.words = wayInWords(name);
    r.turns = r.preset != NoWay || r.words != NoWay;

    if (cap->preset() != QLCCapability::FastToSlow && cap->preset() != QLCCapability::SlowToFast
        && cap->preset() != QLCCapability::RotationClockwiseFastToSlow
        && cap->preset() != QLCCapability::RotationClockwiseSlowToFast
        && cap->preset() != QLCCapability::RotationCounterClockwiseFastToSlow
        && cap->preset() != QLCCapability::RotationCounterClockwiseSlowToFast)
    {
        static const QRegularExpression f2s(QStringLiteral("fast\\W*(to|-+>?|\\x2013|\\x2192)\\W*slow|decreas"),
                                            QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression s2f(QStringLiteral("slow\\W*(to|-+>?|\\x2013|\\x2192)\\W*fast|increas"),
                                            QRegularExpression::CaseInsensitiveOption);
        if (f2s.match(name).hasMatch())
            r.slowToFast = false;
        else if (s2f.match(name).hasMatch())
            r.slowToFast = true;
    }
    rpmInWords(name, r.rpmLo, r.rpmHi);
    if (r.rpmLo > 0.0 && r.rpmHi > 0.0 && r.rpmLo != r.rpmHi)
        r.slowToFast = r.rpmLo < r.rpmHi;
    return r;
}

/* The wider of a way's ranges wins (a definition with two clockwise bands). */
void keep(FreeRotation::Range &into, const QLCCapability *cap, const CapReading &r)
{
    if (into.valid && int(cap->max()) - int(cap->min()) <= int(into.hi) - int(into.lo))
        return;
    into.valid = true;
    into.lo = cap->min();
    into.hi = cap->max();
    into.slowToFast = r.slowToFast;
    into.rpmSlow = into.rpmFast = 0.0;
    if (r.rpmLo > 0.0 && r.rpmHi > 0.0)
    {
        into.rpmSlow = qMin(r.rpmLo, r.rpmHi);
        into.rpmFast = qMax(r.rpmLo, r.rpmHi);
    }
}

bool isAxis(const QLCChannel *ch, FreeRotation::Axis axis)
{
    return ch != nullptr && ch->group() == (axis == FreeRotation::Pan ? QLCChannel::Pan : QLCChannel::Tilt)
           && ch->controlByte() == QLCChannel::MSB;
}
}

FreeRotation::Spin FreeRotation::read(const QLCChannel *ch, bool pointing)
{
    Spin s;
    if (ch == nullptr)
        return s;
    const QList<QLCCapability *> caps = ch->capabilities();
    QList<CapReading> readings;
    bool presetCw = false, presetCcw = false, wordsCw = false, wordsCcw = false;
    bool indexed = false, points = false;
    for (const QLCCapability *cap : caps)
    {
        const CapReading r = readCap(cap);
        readings << r;
        presetCw = presetCw || r.preset == Clockwise;
        presetCcw = presetCcw || r.preset == CounterClockwise;
        wordsCw = wordsCw || (r.turns && r.words == Clockwise);
        wordsCcw = wordsCcw || (r.turns && r.words == CounterClockwise);
        indexed = indexed || r.indexed;
        points = points || r.points;
    }
    /* The presets say which way, unless they give one way only and the
       words both (the Robin Spikie tags "Forwards" and "Backwards" both
       RotationClockwise). */
    const bool byWords = (presetCw != presetCcw) ? (wordsCw && wordsCcw) : !(presetCw || presetCcw);
    for (int i = 0; i < caps.size(); ++i)
    {
        const CapReading &r = readings.at(i);
        if (r.stop && s.stop < 0)
            s.stop = caps.at(i)->middle();
        if (!r.turns)
            continue;
        const Way way = byWords ? (r.words != NoWay ? r.words : r.preset)
                                : (r.preset != NoWay ? r.preset : r.words);
        if (way == Clockwise)
            keep(s.cw, caps.at(i), r);
        else if (way == CounterClockwise)
            keep(s.ccw, caps.at(i), r);
    }
    if (!s.cw.valid && !s.ccw.valid)
        return Spin();
    s.channel = 0;
    s.points = indexed || points;
    s.band = indexed || (pointing && points);
    if (s.band)
        s.stop = -1;                // it stops by pointing
    return s;
}

bool FreeRotation::spinsOnly(const QLCChannel *ch)
{
    if (ch == nullptr || (ch->group() != QLCChannel::Pan && ch->group() != QLCChannel::Tilt))
        return false;
    const Spin s = read(ch);
    return s.canSpin() && !s.points;
}

FreeRotation::Spin FreeRotation::find(const QLCFixtureMode *mode, int head, Axis axis)
{
    if (mode == nullptr)
        return Spin();
    QList<quint32> chans;
    quint32 pointing = UINT_MAX;
    if (head >= 0 && head < mode->heads().size())
    {
        const QLCFixtureHead &h = mode->heads().at(head);
        chans = h.channels();
        pointing = h.channelNumber(axis == Pan ? QLCChannel::Pan : QLCChannel::Tilt, QLCChannel::MSB);
    }
    else
        pointing = mode->channelNumber(axis == Pan ? QLCChannel::Pan : QLCChannel::Tilt, QLCChannel::MSB);

    auto look = [&](const QList<quint32> &in) {
        Spin best;
        for (quint32 c : in)
        {
            const QLCChannel *ch = mode->channel(c);
            if (!isAxis(ch, axis))
                continue;
            Spin s = read(ch, c == pointing);
            if (!s.canSpin())
                continue;
            s.channel = c;
            // One of its own is better than a band on the pointing channel.
            if (!best.canSpin() || (best.band && !s.band))
                best = s;
        }
        return best;
    };

    QList<quint32> all;
    for (quint32 c = 0; c < quint32(mode->channels().size()); ++c)
        if (isAxis(mode->channel(c), axis))
            all << c;
    if (all.isEmpty())
        return Spin();                      // nothing on this axis at all
    const Spin s = look(chans.isEmpty() ? all : chans);
    if (s.canSpin() || chans.isEmpty())
        return s;
    // A head without one of its own: the channels that belong to no head.
    QList<quint32> loose;
    for (quint32 c : all)
        if (mode->headForChannel(c) < 0)
            loose << c;
    return look(loose);
}

FreeRotation::Spin FreeRotation::find(const Fixture *fixture, int head, Axis axis)
{
    return fixture == nullptr ? Spin() : find(fixture->fixtureMode(), head, axis);
}

bool FreeRotation::canSpin(const Fixture *fixture, int head)
{
    if (fixture == nullptr || fixture->fixtureMode() == nullptr)
        return false;
    const int heads = qMax(1, fixture->heads());
    for (int h = 0; h < heads; ++h)
    {
        if (head >= 0 && h != head)
            continue;
        if (find(fixture, h, Pan).canSpin() || find(fixture, h, Tilt).canSpin())
            return true;
    }
    return false;
}

int FreeRotation::Spin::value(double speed) const
{
    if (!canSpin())
        return -1;
    if (speed == 0.0)
        return stop;
    const Range &want = speed > 0.0 ? cw : ccw;
    const Range &r = want.valid ? want : (speed > 0.0 ? ccw : cw);
    const double s = qBound(0.0, qAbs(speed), 1.0);
    const double span = double(r.hi) - double(r.lo);
    const double v = r.slowToFast ? r.lo + s * span : r.hi - s * span;
    return qBound(int(r.lo), qRound(v), int(r.hi));
}

double FreeRotation::Spin::speedForRpm(double rpm, bool clockwise) const
{
    const Range &want = clockwise ? cw : ccw;
    const Range &r = want.valid ? want : (clockwise ? ccw : cw);
    if (!r.hasRpm())
        return -1.0;
    return qBound(0.0, (rpm - r.rpmSlow) / (r.rpmFast - r.rpmSlow), 1.0);
}

double FreeRotation::Spin::speedOf(uchar dmx) const
{
    if (!canSpin())
        return 0.0;
    for (int way = 0; way < 2; ++way)
    {
        const Range &r = way == 0 ? cw : ccw;
        if (!r.valid || dmx < r.lo || dmx > r.hi)
            continue;
        const double span = qMax(1.0, double(r.hi) - double(r.lo));
        double s = (double(dmx) - double(r.lo)) / span;
        if (!r.slowToFast)
            s = 1.0 - s;
        // The slowest end is still turning.
        s = qMax(s, 1e-3);
        return way == 0 ? s : -s;
    }
    return 0.0;
}
