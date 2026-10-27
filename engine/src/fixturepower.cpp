/*
  Q Light Controller Plus
  fixturepower.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QtMath>
#include <QSet>

#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "fixturepower.h"
#include "qlcchannel.h"
#include "fixture.h"

#define KXMLPowerMax      QStringLiteral("Max")
#define KXMLPowerFull     QStringLiteral("Full")
#define KXMLPowerHalf     QStringLiteral("Half")
#define KXMLPowerStandby  QStringLiteral("Standby")
#define KXMLPowerColour   QStringLiteral("Colour")
#define KXMLPowerName     QStringLiteral("Name")
#define KXMLPowerAmps     QStringLiteral("Amps")

/*****************************************************************************
 * PowerProfile
 *****************************************************************************/

bool PowerProfile::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLFixturePower)
        return false;
    const QXmlStreamAttributes a = root.attributes();
    maxAmps = a.value(KXMLPowerMax).toString().toDouble();
    fullAmps = a.value(KXMLPowerFull).toString().toDouble();
    halfAmps = a.value(KXMLPowerHalf).toString().toDouble();
    standbyAmps = a.value(KXMLPowerStandby).toString().toDouble();
    colourAmps.clear();
    while (root.readNextStartElement())
    {
        if (root.name() == KXMLPowerColour)
        {
            const QXmlStreamAttributes ca = root.attributes();
            const QString name = ca.value(KXMLPowerName).toString();
            const int colour = (name == QLatin1String("Lamp"))
                               ? int(QLCChannel::NoColour) : int(QLCChannel::stringToColour(name));
            colourAmps.insert(colour, ca.value(KXMLPowerAmps).toString().toDouble());
        }
        root.skipCurrentElement();
    }
    return true;
}

void PowerProfile::saveXML(QXmlStreamWriter *doc) const
{
    if (!isMeasured() && maxAmps <= 0.0)
        return;
    doc->writeStartElement(KXMLFixturePower);
    if (maxAmps > 0.0)
        doc->writeAttribute(KXMLPowerMax, QString::number(maxAmps, 'g', 6));
    if (fullAmps > 0.0)
        doc->writeAttribute(KXMLPowerFull, QString::number(fullAmps, 'g', 6));
    if (halfAmps > 0.0)
        doc->writeAttribute(KXMLPowerHalf, QString::number(halfAmps, 'g', 6));
    if (standbyAmps > 0.0)
        doc->writeAttribute(KXMLPowerStandby, QString::number(standbyAmps, 'g', 6));
    for (auto it = colourAmps.constBegin(); it != colourAmps.constEnd(); ++it)
    {
        doc->writeStartElement(KXMLPowerColour);
        doc->writeAttribute(KXMLPowerName, it.key() == int(QLCChannel::NoColour)
                                               ? QStringLiteral("Lamp")
                                               : QLCChannel::colourToString(QLCChannel::PrimaryColour(it.key())));
        doc->writeAttribute(KXMLPowerAmps, QString::number(it.value(), 'g', 6));
        doc->writeEndElement();
    }
    doc->writeEndElement();
}

/*****************************************************************************
 * PowerModel
 *****************************************************************************/

PowerModel PowerModel::build(const Fixture *fx, const PowerProfile &profile)
{
    PowerModel m;
    if (fx == nullptr || fx->fixtureMode() == nullptr || !profile.isMeasured())
        return m;
    const QLCFixtureMode *mode = fx->fixtureMode();
    const QVector<QLCChannel *> chans = mode->channels();

    auto isLevel = [](const QLCChannel *ch) {
        return ch != nullptr && ch->group() == QLCChannel::Intensity
               && ch->controlByte() == QLCChannel::MSB;
    };

    // The dimmers: the fixture's master, and each head's own.
    const quint32 master = fx->masterIntensityChannel();
    QVector<int> headDimmer(chans.size(), -1);
    for (int h = 0; h < mode->heads().size(); ++h)
    {
        const QList<quint32> hc = mode->heads().at(h).channels();
        int dim = -1;
        foreach (quint32 c, hc)
            if (c < quint32(chans.size()) && isLevel(chans.at(int(c)))
                && chans.at(int(c))->colour() == QLCChannel::NoColour && c != master)
                dim = int(c);
        foreach (quint32 c, hc)
            if (c < quint32(chans.size()))
                headDimmer[int(c)] = dim;
    }

    // What emits: the colours; a fixture with none, its plain dimmers.
    QVector<int> emitters;
    for (int i = 0; i < chans.size(); ++i)
        if (isLevel(chans.at(i)) && chans.at(i)->colour() != QLCChannel::NoColour)
            emitters << i;
    const bool lampsOnly = emitters.isEmpty();
    if (lampsOnly)
        for (int i = 0; i < chans.size(); ++i)
            if (isLevel(chans.at(i)))
                emitters << i;
    if (emitters.isEmpty())
        return m;

    // How many pixels of each colour.
    QMap<int, int> count;
    foreach (int i, emitters)
        count[lampsOnly ? int(QLCChannel::NoColour) : int(chans.at(i)->colour())]++;

    /* Each colour's draw at full, standby taken out. Measured, or the rest
       of an everything-at-full reading spread over the colours not measured,
       or (with no such reading) the average of the ones that were. */
    const double s = qMax(0.0, profile.standbyAmps);
    QMap<int, double> excess;
    double known = 0.0;
    QList<int> unknown;
    for (auto it = count.constBegin(); it != count.constEnd(); ++it)
    {
        if (profile.colourAmps.contains(it.key()))
        {
            excess[it.key()] = qMax(0.0, profile.colourAmps.value(it.key()) - s);
            known += excess[it.key()];
        }
        else
            unknown << it.key();
    }
    if (!unknown.isEmpty())
    {
        double each = 0.0;
        if (profile.fullAmps > 0.0)
            each = qMax(0.0, (profile.fullAmps - s) - known) / unknown.size();
        else if (!excess.isEmpty())
            each = known / excess.size();
        foreach (int c, unknown)
            excess[c] = each;
    }

    double total = 0.0;
    for (auto it = excess.constBegin(); it != excess.constEnd(); ++it)
        total += it.value();
    if (total <= 0.0)
        return m;

    foreach (int i, emitters)
    {
        const int colour = lampsOnly ? int(QLCChannel::NoColour) : int(chans.at(i)->colour());
        Emitter e;
        e.offset = i;
        e.share = excess.value(colour) / qMax(1, count.value(colour));
        if (!lampsOnly)
        {
            if (master != QLCChannel::invalid() && int(master) != i)
                e.dimmers << int(master);
            if (headDimmer.at(i) >= 0 && headDimmer.at(i) != i && !e.dimmers.contains(headDimmer.at(i)))
                e.dimmers << headDimmer.at(i);
        }
        m.emitters << e;
    }

    // The curve: everything at half against everything at full.
    if (profile.halfAmps > s)
    {
        const double g = qLn((profile.halfAmps - s) / total) / qLn(0.5);
        m.gamma = qBound(0.5, g, 3.0);
    }
    m.standby = s;
    m.maxAmps = qMax(0.0, profile.maxAmps);
    return m;
}

double PowerModel::amps(const std::function<uchar(int)> &value) const
{
    double sum = 0.0;
    foreach (const Emitter &e, emitters)
    {
        double level = value(e.offset) / 255.0;
        foreach (int d, e.dimmers)
            level *= value(d) / 255.0;
        if (level > 0.0)
            sum += e.share * qPow(level, gamma);
    }
    return standby + sum;
}

double PowerModel::limitFactor(double amps) const
{
    if (maxAmps <= 0.0 || amps <= maxAmps)
        return 1.0;
    if (maxAmps <= standby || amps <= standby)
        return 0.0;
    return qPow((maxAmps - standby) / (amps - standby), 1.0 / gamma);
}
