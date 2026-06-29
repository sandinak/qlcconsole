/*
  Q Light Controller Plus
  powerdistribution.h

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

#ifndef POWERDISTRIBUTION_H
#define POWERDISTRIBUTION_H

#include <functional>
#include <QString>
#include <QList>
#include <QHash>

class QXmlStreamReader;
class QXmlStreamWriter;
class Fixture;
class Function;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCPowerDistribution QStringLiteral("PowerDistribution")

/**
 * A single breaker/circuit on a power source. Carries a rated current (the
 * breaker size, in amps), a derate factor (loads above this fraction of the
 * rating are flagged), and the list of fixtures fed by it.
 */
class PowerCircuit
{
public:
    PowerCircuit();

    QString name;
    double  voltage;            //!< circuit voltage; 0 == inherit from source
    double  ratedAmps;          //!< breaker rating in amps (e.g. 20)
    int     deratePercent;      //!< usable fraction of the rating (e.g. 80)
    QList<quint32> fixtures;    //!< assigned fixture IDs

    /** Continuous load the breaker should not exceed (amps) */
    double deratedLimit() const { return ratedAmps * deratePercent / 100.0; }

    /** Circuit voltage, falling back to the source's voltage when unset */
    double effectiveVoltage(double sourceVoltage) const
    { return voltage > 0.0 ? voltage : sourceVoltage; }
};

/**
 * A power source (distro, panel, generator leg...). Holds a nominal voltage
 * used to convert the watts of its circuits into amps, and the circuits it
 * feeds.
 */
class PowerSource
{
public:
    PowerSource();

    QString name;
    double  voltage;            //!< nominal voltage (e.g. 120)
    QList<PowerCircuit> circuits;

    // Physical location on the 2D stage plot (metres). Venue-fixed: it travels
    // with the .venue. `placed` is false until the source is dropped on the map.
    bool    placed;
    double  posX;
    double  posY;

    // UPS / battery modeling (optional). A source is treated as a UPS when
    // vaRating > 0: its load is checked against the VA capacity, and runtime is
    // estimated from a single datasheet point (runtimeMinutes at runtimeWatts),
    // scaled linearly by stored energy.
    double  vaRating;           //!< apparent-power capacity in VA; 0 == mains
    double  runtimeMinutes;     //!< datasheet runtime...
    double  runtimeWatts;       //!< ...at this load (watts)

    bool isUPS() const { return vaRating > 0.0; }

    /** Estimated runtime (minutes) at a given load; -1 if not modelable. */
    double estimateRuntimeMinutes(double loadWatts) const
    {
        if (loadWatts <= 0.0 || runtimeWatts <= 0.0 || runtimeMinutes <= 0.0)
            return -1.0;
        // Constant stored energy E = runtimeMinutes * runtimeWatts (min·W);
        // runtime at the actual load is E / load.
        return (runtimeMinutes * runtimeWatts) / loadWatts;
    }
};

/**
 * Per-document power distribution model: a set of power sources, each with
 * circuits, each circuit feeding a set of fixtures. Owned by Doc and
 * serialized into the workspace. Used by the Programming tab to estimate the
 * electrical load of a designed look and flag overloaded circuits.
 *
 * This model is pure bookkeeping; the actual load estimation lives in the
 * PowerEstimator namespace below so the engine never needs to reach into the
 * UI or the InputOutputMap.
 */
class PowerDistribution
{
public:
    PowerDistribution();

    /** Drop all sources/circuits (called when a new workspace is created) */
    void reset();

    QList<PowerSource>& sources() { return m_sources; }
    const QList<PowerSource>& sources() const { return m_sources; }

    /** Remove a fixture from whatever circuit it is on (no-op if unassigned) */
    void unassignFixture(quint32 fxId);

    /** Assign a fixture to a specific circuit, removing any prior assignment */
    void assignFixture(quint32 fxId, int sourceIdx, int circuitIdx);

    /** Called when a fixture is deleted from the document */
    void removeFixture(quint32 fxId) { unassignFixture(fxId); }

    /** The circuit a fixture belongs to, as "sourceIdx/circuitIdx", or -1/-1 */
    void circuitOf(quint32 fxId, int &sourceIdx, int &circuitIdx) const;

    bool loadXML(QXmlStreamReader &root, Doc *doc);
    /** Serialize. When includeFixtures is false (venue export), the circuit
     *  infrastructure is written without the show-specific fixture assignments. */
    bool saveXML(QXmlStreamWriter *doc, bool includeFixtures = true) const;

private:
    QList<PowerSource> m_sources;
};

/**
 * Stateless load-estimation helpers. A PreGMReader returns the pre-Grand-Master
 * DMX value for a given (universe, channel) so the estimate reflects the
 * designed look, free of blackout/Grand-Master, and the engine stays unaware of
 * how the caller reads its universes.
 */
namespace PowerEstimator
{
    typedef std::function<uchar(quint32 universe, quint32 channel)> PreGMReader;

    /** Moving fixture standby (motors/logic/fans) as a fraction of rated power,
     *  drawn even when dark (e.g. 0.15 = 15% of rated). */
    extern const double MoverStandbyFraction;
    /** Fixed standby for non-movers (LED-driver idle), in watts, drawn when dark. */
    extern const double NonMoverStandbyWatts;
    /** Per-intensity-channel watts used when a fixture has no rated power */
    extern const double FallbackWattsPerChannel;

    /** Combined intensity (dimmer x colour) of a fixture, in [0, 1] */
    double intensityFraction(const Fixture *fx, const PreGMReader &read);

    /** Estimated designed-peak watts a fixture draws at its current values */
    double fixtureWatts(const Fixture *fx, const PreGMReader &read);

    /** Full-on draw of a fixture (rated power, or channel-count estimate) */
    double fixtureFullWatts(const Fixture *fx);

    /** Total watts of all fixtures; optionally fills a fixtureId -> watts map */
    double estimateWatts(Doc *doc, const PreGMReader &read,
                         QHash<quint32, double> *perFixture = NULL);

    /** Per-fixture watts for a function's programmed state, keyed by fixture id.
     *  Scenes resolve their values + palettes; collections HTP-merge their
     *  members (simultaneous load). Keyed by fixture id. */
    QHash<quint32, double> functionFixtureWatts(Doc *doc, Function *func);
}

/** @} */

#endif
