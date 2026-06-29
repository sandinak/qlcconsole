/*
  Q Light Controller Plus
  powerdistribution.cpp

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
#include <QDebug>

#include "powerdistribution.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "scenevalue.h"
#include "function.h"
#include "scene.h"
#include "collection.h"
#include "fixture.h"
#include "doc.h"

#define KXMLQLCPowerSource          QStringLiteral("Source")
#define KXMLQLCPowerSourceName      QStringLiteral("Name")
#define KXMLQLCPowerSourceVoltage   QStringLiteral("Voltage")
#define KXMLQLCPowerSourceVA        QStringLiteral("VARating")
#define KXMLQLCPowerSourceRuntimeM  QStringLiteral("RuntimeMin")
#define KXMLQLCPowerSourceRuntimeW  QStringLiteral("RuntimeWatts")
#define KXMLQLCPowerSourcePosX      QStringLiteral("PosX")
#define KXMLQLCPowerSourcePosY      QStringLiteral("PosY")
#define KXMLQLCPowerCircuit         QStringLiteral("Circuit")
#define KXMLQLCPowerCircuitName     QStringLiteral("Name")
#define KXMLQLCPowerCircuitVoltage  QStringLiteral("Voltage")
#define KXMLQLCPowerCircuitRated    QStringLiteral("RatedAmps")
#define KXMLQLCPowerCircuitDerate   QStringLiteral("Derate")
#define KXMLQLCPowerCircuitFixture  QStringLiteral("Fixture")

/****************************************************************************
 * PowerCircuit / PowerSource
 ****************************************************************************/

PowerCircuit::PowerCircuit()
    : name(QStringLiteral("Circuit"))
    , voltage(0)            // 0 == inherit the source voltage
    , ratedAmps(20)
    , deratePercent(80)
{
}

PowerSource::PowerSource()
    : name(QStringLiteral("Source"))
    , voltage(120)
    , vaRating(0)
    , runtimeMinutes(0)
    , runtimeWatts(0)
    , placed(false)
    , posX(0)
    , posY(0)
{
}

/****************************************************************************
 * PowerDistribution
 ****************************************************************************/

PowerDistribution::PowerDistribution()
{
}

void PowerDistribution::reset()
{
    m_sources.clear();
}

void PowerDistribution::unassignFixture(quint32 fxId)
{
    for (int s = 0; s < m_sources.size(); s++)
    {
        for (int c = 0; c < m_sources[s].circuits.size(); c++)
            m_sources[s].circuits[c].fixtures.removeAll(fxId);
    }
}

void PowerDistribution::assignFixture(quint32 fxId, int sourceIdx, int circuitIdx)
{
    if (sourceIdx < 0 || sourceIdx >= m_sources.size())
        return;
    if (circuitIdx < 0 || circuitIdx >= m_sources[sourceIdx].circuits.size())
        return;

    unassignFixture(fxId);
    m_sources[sourceIdx].circuits[circuitIdx].fixtures.append(fxId);
}

void PowerDistribution::circuitOf(quint32 fxId, int &sourceIdx, int &circuitIdx) const
{
    sourceIdx = -1;
    circuitIdx = -1;
    for (int s = 0; s < m_sources.size(); s++)
    {
        for (int c = 0; c < m_sources[s].circuits.size(); c++)
        {
            if (m_sources[s].circuits[c].fixtures.contains(fxId))
            {
                sourceIdx = s;
                circuitIdx = c;
                return;
            }
        }
    }
}

/****************************************************************************
 * Load & Save
 ****************************************************************************/

bool PowerDistribution::loadXML(QXmlStreamReader &root, Doc *doc)
{
    Q_UNUSED(doc)

    if (root.name() != KXMLQLCPowerDistribution)
    {
        qWarning() << Q_FUNC_INFO << "Power distribution node not found";
        return false;
    }

    m_sources.clear();

    while (root.readNextStartElement())
    {
        if (root.name() != KXMLQLCPowerSource)
        {
            qWarning() << Q_FUNC_INFO << "Unknown power tag:" << root.name();
            root.skipCurrentElement();
            continue;
        }

        PowerSource src;
        QXmlStreamAttributes sattrs = root.attributes();
        if (sattrs.hasAttribute(KXMLQLCPowerSourceVoltage))
            src.voltage = sattrs.value(KXMLQLCPowerSourceVoltage).toString().toDouble();
        if (sattrs.hasAttribute(KXMLQLCPowerSourceVA))
            src.vaRating = sattrs.value(KXMLQLCPowerSourceVA).toString().toDouble();
        if (sattrs.hasAttribute(KXMLQLCPowerSourceRuntimeM))
            src.runtimeMinutes = sattrs.value(KXMLQLCPowerSourceRuntimeM).toString().toDouble();
        if (sattrs.hasAttribute(KXMLQLCPowerSourceRuntimeW))
            src.runtimeWatts = sattrs.value(KXMLQLCPowerSourceRuntimeW).toString().toDouble();
        if (sattrs.hasAttribute(KXMLQLCPowerSourcePosX) &&
            sattrs.hasAttribute(KXMLQLCPowerSourcePosY))
        {
            src.placed = true;
            src.posX = sattrs.value(KXMLQLCPowerSourcePosX).toString().toDouble();
            src.posY = sattrs.value(KXMLQLCPowerSourcePosY).toString().toDouble();
        }

        while (root.readNextStartElement())
        {
            if (root.name() == KXMLQLCPowerSourceName)
            {
                src.name = root.readElementText();
            }
            else if (root.name() == KXMLQLCPowerCircuit)
            {
                PowerCircuit cir;
                QXmlStreamAttributes cattrs = root.attributes();
                if (cattrs.hasAttribute(KXMLQLCPowerCircuitVoltage))
                    cir.voltage = cattrs.value(KXMLQLCPowerCircuitVoltage).toString().toDouble();
                if (cattrs.hasAttribute(KXMLQLCPowerCircuitRated))
                    cir.ratedAmps = cattrs.value(KXMLQLCPowerCircuitRated).toString().toDouble();
                if (cattrs.hasAttribute(KXMLQLCPowerCircuitDerate))
                    cir.deratePercent = cattrs.value(KXMLQLCPowerCircuitDerate).toString().toInt();

                while (root.readNextStartElement())
                {
                    if (root.name() == KXMLQLCPowerCircuitName)
                    {
                        cir.name = root.readElementText();
                    }
                    else if (root.name() == KXMLQLCPowerCircuitFixture)
                    {
                        bool ok = false;
                        quint32 fxId = root.readElementText().toUInt(&ok);
                        if (ok)
                            cir.fixtures.append(fxId);
                    }
                    else
                    {
                        root.skipCurrentElement();
                    }
                }
                src.circuits.append(cir);
            }
            else
            {
                root.skipCurrentElement();
            }
        }
        m_sources.append(src);
    }

    int nc = 0;
    foreach (const PowerSource &s, m_sources) nc += s.circuits.size();
    qCritical() << "[PWRIO] loadXML: sources=" << m_sources.size() << "circuits=" << nc;
    return true;
}

bool PowerDistribution::saveXML(QXmlStreamWriter *doc, bool includeFixtures) const
{
    Q_ASSERT(doc != NULL);

    int dbgC = 0;
    foreach (const PowerSource &s, m_sources) dbgC += s.circuits.size();
    qCritical() << "[PWRIO] saveXML: sources=" << m_sources.size() << "circuits=" << dbgC
                << "fixtures=" << includeFixtures;

    if (m_sources.isEmpty())
        return true;

    doc->writeStartElement(KXMLQLCPowerDistribution);

    foreach (const PowerSource &src, m_sources)
    {
        doc->writeStartElement(KXMLQLCPowerSource);
        doc->writeAttribute(KXMLQLCPowerSourceVoltage, QString::number(src.voltage));
        if (src.vaRating > 0.0)
            doc->writeAttribute(KXMLQLCPowerSourceVA, QString::number(src.vaRating));
        if (src.runtimeMinutes > 0.0)
            doc->writeAttribute(KXMLQLCPowerSourceRuntimeM, QString::number(src.runtimeMinutes));
        if (src.runtimeWatts > 0.0)
            doc->writeAttribute(KXMLQLCPowerSourceRuntimeW, QString::number(src.runtimeWatts));
        if (src.placed)
        {
            doc->writeAttribute(KXMLQLCPowerSourcePosX, QString::number(src.posX));
            doc->writeAttribute(KXMLQLCPowerSourcePosY, QString::number(src.posY));
        }
        doc->writeTextElement(KXMLQLCPowerSourceName, src.name);

        foreach (const PowerCircuit &cir, src.circuits)
        {
            doc->writeStartElement(KXMLQLCPowerCircuit);
            if (cir.voltage > 0.0)
                doc->writeAttribute(KXMLQLCPowerCircuitVoltage, QString::number(cir.voltage));
            doc->writeAttribute(KXMLQLCPowerCircuitRated, QString::number(cir.ratedAmps));
            doc->writeAttribute(KXMLQLCPowerCircuitDerate, QString::number(cir.deratePercent));
            doc->writeTextElement(KXMLQLCPowerCircuitName, cir.name);

            if (includeFixtures)
                foreach (quint32 fxId, cir.fixtures)
                    doc->writeTextElement(KXMLQLCPowerCircuitFixture, QString::number(fxId));

            doc->writeEndElement(); // Circuit
        }
        doc->writeEndElement(); // Source
    }

    doc->writeEndElement(); // PowerDistribution

    return true;
}

/****************************************************************************
 * PowerEstimator
 ****************************************************************************/

namespace PowerEstimator
{
    const double MoverStandbyFraction = 0.15;   // movers idle at ~15% of rated
    const double NonMoverStandbyWatts = 2.0;     // small LED-driver idle when dark
    const double FallbackWattsPerChannel = 40.0;
}

double PowerEstimator::intensityFraction(const Fixture *fx, const PreGMReader &read)
{
    if (fx == NULL || fx->fixtureMode() == NULL)
        return 0.0;

    const QVector<QLCChannel*> chans = fx->fixtureMode()->channels();

    double dimmer = -1.0;   // -1 == fixture has no master/dimmer channel
    double colourSum = 0.0;
    int colourCount = 0;

    for (int i = 0; i < chans.size(); i++)
    {
        const QLCChannel *ch = chans.at(i);
        if (ch->group() != QLCChannel::Intensity)
            continue;
        // Only the coarse byte carries the meaningful level; skip fine channels
        if (ch->controlByte() != QLCChannel::MSB)
            continue;

        // read() expects a within-universe DMX address (0-511); fx->address()
        // is the fixture's start address in its universe, i its channel offset.
        const uchar val = read(fx->universe(), fx->address() + i);
        const double frac = val / 255.0;

        if (ch->colour() == QLCChannel::NoColour)
        {
            // Master dimmer; if several exist, the brightest governs
            if (frac > dimmer)
                dimmer = frac;
        }
        else
        {
            colourSum += frac;
            colourCount++;
        }
    }

    const double masterFrac = (dimmer < 0.0) ? 1.0 : dimmer;
    const double colourFrac = (colourCount == 0) ? 1.0 : (colourSum / colourCount);
    return masterFrac * colourFrac;
}

// Full-on draw of a fixture: its rated power, or a channel-count estimate when
// the definition declares none.
double PowerEstimator::fixtureFullWatts(const Fixture *fx)
{
    if (fx == NULL || fx->fixtureMode() == NULL)
        return 0.0;

    double rated = fx->fixtureMode()->physical().powerConsumption();
    if (rated <= 0.0)
    {
        int intensityChans = 0;
        const QVector<QLCChannel*> chans = fx->fixtureMode()->channels();
        for (int i = 0; i < chans.size(); i++)
        {
            const QLCChannel *ch = chans.at(i);
            if (ch->group() == QLCChannel::Intensity && ch->controlByte() == QLCChannel::MSB)
                intensityChans++;
        }
        rated = intensityChans * FallbackWattsPerChannel;
    }
    return rated;
}

double PowerEstimator::fixtureWatts(const Fixture *fx, const PreGMReader &read)
{
    const double rated = fixtureFullWatts(fx);
    if (rated <= 0.0)
        return 0.0;

    const double frac = intensityFraction(fx, read);
    const bool mover = (fx->type() == QLCFixtureDef::MovingHead ||
                        fx->type() == QLCFixtureDef::Scanner);

    // Standby is drawn even when dark: movers idle a fraction of rated
    // (motors/logic/fans); non-movers draw a small fixed LED-driver idle. The
    // lamp scales the remainder with intensity, so full-on == rated.
    double standby = mover ? rated * MoverStandbyFraction : NonMoverStandbyWatts;
    if (standby > rated)
        standby = rated;
    return standby + (rated - standby) * frac;
}

double PowerEstimator::estimateWatts(Doc *doc, const PreGMReader &read,
                                     QHash<quint32, double> *perFixture)
{
    if (doc == NULL)
        return 0.0;

    double total = 0.0;
    foreach (Fixture *fx, doc->fixtures())
    {
        if (fx == NULL)
            continue;
        const double w = fixtureWatts(fx, read);
        if (perFixture != NULL)
            perFixture->insert(fx->id(), w);
        total += w;
    }
    return total;
}

// Resolve a scene's programmed channel values statically (its own direct values
// plus every applied palette), HTP-merging into `out` (key = universe*512+addr).
static void resolveSceneInto(Doc *doc, Scene *scene, QHash<quint32, uchar> &out)
{
    const QList<quint32> sceneFx = scene->fixtures();
    const QList<quint32> sceneGrp = scene->fixtureGroups();

    QList<SceneValue> all = scene->values();
    foreach (quint32 pid, scene->palettes())
    {
        QLCPalette *pal = doc->palette(pid);
        if (pal == NULL)
            continue;
        all += pal->valuesFromFixtures(doc, sceneFx);
        all += pal->valuesFromFixtureGroups(doc, sceneGrp);
    }
    foreach (const SceneValue &sv, all)
    {
        Fixture *fx = doc->fixture(sv.fxi);
        if (fx == NULL)
            continue;
        const quint32 key = fx->universe() * 512u + fx->address() + sv.channel;
        // HTP: a collection runs its scenes at once, so a fixture lit by several
        // takes the highest level.
        if (uchar(sv.value) > out.value(key, 0))
            out.insert(key, uchar(sv.value));
    }
}

// Recursively resolve a function into channel values: scenes directly,
// collections by HTP-merging all members (so the simultaneous load is captured).
static void resolveFunctionInto(Doc *doc, Function *f,
                                QHash<quint32, uchar> &out, QSet<quint32> &visited)
{
    if (f == NULL || visited.contains(f->id()))
        return;
    visited.insert(f->id());

    if (Scene *sc = qobject_cast<Scene*>(f))
        resolveSceneInto(doc, sc, out);
    else if (Collection *col = qobject_cast<Collection*>(f))
        foreach (quint32 mid, col->functions())
            resolveFunctionInto(doc, doc->function(mid), out, visited);
    // Chasers run one step at a time; per-step worst case is handled separately.
}

QHash<quint32, double> PowerEstimator::functionFixtureWatts(Doc *doc, Function *func)
{
    QHash<quint32, double> perFixture;
    if (doc == NULL || func == NULL)
        return perFixture;

    // Reuse the exact same intensity→watts maths as the live footer by serving
    // the resolved channel values through a PreGMReader.
    QHash<quint32, uchar> resolved;
    QSet<quint32> visited;
    resolveFunctionInto(doc, func, resolved, visited);

    PreGMReader reader =
        [&resolved](quint32 uni, quint32 addr) -> uchar
        { return resolved.value(uni * 512u + addr, 0); };

    estimateWatts(doc, reader, &perFixture);
    return perFixture;
}
