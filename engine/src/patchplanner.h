/*
  Q Light Controller Plus
  patchplanner.h

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

#ifndef PATCHPLANNER_H
#define PATCHPLANNER_H

#include <QStringList>
#include <QList>
#include <QHash>

class Doc;
class QLCFixtureMode;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Works out where inventory (unpatched) fixtures should go, and applies it.
 *
 * DMX fixtures are packed first-fit into the existing universes, spilling into
 * new ones. Network fixtures are unicast: each distinct IP gets a universe of
 * its own (fixtures behind one IP share it), and a DMX fixture is never put in
 * a universe that carries network fixtures, since a universe has one output
 * and so one destination.
 *
 * Proximity (N6, see NETWORK_CABLING_DESIGN.md): a DMX fixture that is on the
 * plot goes to the nearest node port with room -- nearest by where the port's
 * universe physically lands (the receiver for a wireless port, else the node),
 * least-loaded first among equally near ports. A universe already on the port
 * is filled first-fit; failing that, a free port on that node claims a new
 * universe, which apply() writes onto the port. A fixture that is not on the
 * plot, or has no node with room anywhere, falls back to first-fit. Placed
 * fixtures are planned before the rest, so first-fit never takes the room the
 * fixtures standing next to a node were going to use.
 *
 * plan() is pure -- it reads the Doc and changes nothing -- so the UI can show
 * the proposal and let the LD adjust it before apply() commits it.
 */
class PatchPlanner
{
public:
    struct Assignment
    {
        quint32 fixtureId;
        quint32 universe;       //!< zero-based; may be past the last existing universe
        quint32 address;        //!< zero-based within the universe
        bool    newUniverse;    //!< the universe doesn't exist yet
        QString note;           //!< a caveat for the LD, empty when there is none

        /** Set the fixture to this mode when applying (NULL = keep its mode).
         *  A mode is picked at patch time because it decides the DMX footprint. */
        QLCFixtureMode *mode;
        /** For a generic dimmer: set this many channels when applying (0 = keep). */
        quint32 dimmerChannels;

        /** The node port that carries this universe to the fixture, as
         *  proposed by proximity (-1 when first-fit placed it). */
        int     nodeIndex;
        int     portIndex;
        double  distanceMetres;  //!< fixture to the port's anchor, on the plan
        /** The port had no universe: apply() gives it this one. */
        bool    claimsPort;

        Assignment() : fixtureId(0), universe(0), address(0), newUniverse(false),
                       mode(NULL), dimmerChannels(0), nodeIndex(-1), portIndex(-1),
                       distanceMetres(0), claimsPort(false) {}
    };

    explicit PatchPlanner(Doc *doc);

    /** Place plotted DMX fixtures by their nearest node port (default on).
     *  Off, every DMX fixture is packed first-fit as before N6. */
    void setUseProximity(bool on) { m_useProximity = on; }
    bool useProximity() const { return m_useProximity; }

    /** Propose a universe and address for each unpatched fixture in
     *  @p fixtureIds (patched and unknown ids are skipped). */
    QList<Assignment> plan(const QList<quint32> &fixtureIds) const;

    /** As above, but fixtures in @p channelOverride are planned at that many
     *  channels instead of their current count -- the footprint they will have
     *  once the mode chosen for them at patch time is applied. */
    QList<Assignment> plan(const QList<quint32> &fixtureIds,
                           const QHash<quint32, quint32> &channelOverride) const;

    /** Commit assignments through Doc::patchFixture(). For a network fixture
     *  also points its universe's output at the fixture's IP, copying the
     *  plugin and interface of an existing ArtNet/sACN universe when the
     *  universe has none. A claimsPort assignment also writes its universe
     *  onto that node port. Returns how many were patched; anything that could
     *  not be done is described in @p problems. */
    int apply(const QList<Assignment> &assignments, QStringList *problems = NULL);

private:
    Doc *m_doc;
    bool m_useProximity;
};

/** @} */

#endif
