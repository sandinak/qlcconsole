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
        /** fit() found no room for it (and wasn't to carry on into the next
         *  universe): it stays where it was -- universe/address say where. */
        bool    unfitted;

        Assignment() : fixtureId(0), universe(0), address(0), newUniverse(false),
                       mode(NULL), dimmerChannels(0), nodeIndex(-1), portIndex(-1),
                       distanceMetres(0), claimsPort(false), unfitted(false) {}
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

    /** Put @p fixtureIds -- patched or not -- at @p universe / @p address,
     *  ignoring where the ones already patched are now (they are the ones
     *  moving), and around everything else.
     *
     *  @p keepSpacing: the patched ones keep their address gaps relative to
     *  the lowest of them, as one block (it may run past the universe or
     *  onto another fixture: the plan says so in its note, and the dialog
     *  flags the conflict, rather than quietly breaking the block up); any
     *  unpatched ones follow it, packed. Otherwise every fixture, in list
     *  order, takes the first free run at or after the previous one's end,
     *  spilling into the next universe when one fills.
     *
     *  @p elsewhere: other fixtures that are going somewhere too (a plan
     *  being edited) -- fixture -> (absolute start, channels). Their present
     *  channels are free and those are not. */
    QList<Assignment> place(const QList<quint32> &fixtureIds, quint32 universe, quint32 address,
                            bool keepSpacing,
                            const QHash<quint32, quint32> &channelOverride = QHash<quint32, quint32>(),
                            const QHash<quint32, QPair<quint32, quint32> > &elsewhere
                                = QHash<quint32, QPair<quint32, quint32> >()) const;

    /** The order a group is addressed in -- one scheme, every time. */
    enum AddressOrder
    {
        ByName,         //!< by name, numbers counted as numbers (B2 before B10)
        ByKind,         //!< movers, then scanners, pixel bars, beam bars, washes,
                        //!< strobes, dimmers, effects, atmospherics; each by name
        AsTheyHang      //!< structure by structure (by name), in their order on it
    };
    /** @p fixtureIds in @p order (stable: ties keep their given order). */
    static QList<quint32> ordered(Doc *doc, const QList<quint32> &fixtureIds, AddressOrder order);

    /** Lay @p fixtureIds out in @p universe from @p address, in the order
     *  given (see ordered()), around what is
     *  there: each takes the first free run at or after the end of the one
     *  before it, so the addresses read in that order and the same group
     *  always lands the same way. What has no room is flagged unfitted and
     *  left where it was -- or, with @p carryOn, goes on into the next
     *  universe(s).
     *  @p channelOverride and @p elsewhere as for place(). */
    QList<Assignment> fit(const QList<quint32> &fixtureIds, quint32 universe, quint32 address,
                          bool carryOn,
                          const QHash<quint32, quint32> &channelOverride = QHash<quint32, quint32>(),
                          const QHash<quint32, QPair<quint32, quint32> > &elsewhere
                              = QHash<quint32, QPair<quint32, quint32> >()) const;

    /** Smaller modes that would let fit() place them all in @p universe:
     *  the biggest fixtures step down first, one mode at a time, only to
     *  modes they may take (Doc::canChangeFixtureMode()). @p current: each
     *  fixture's mode as chosen so far (else its own). Returns only the
     *  changes; @p allFit says whether they are enough. */
    QHash<quint32, QLCFixtureMode *> modesToFit(const QList<quint32> &fixtureIds, quint32 universe,
                                                quint32 address,
                                                const QHash<quint32, QLCFixtureMode *> &current,
                                                const QHash<quint32, quint32> &channelOverride,
                                                const QHash<quint32, QPair<quint32, quint32> > &elsewhere,
                                                bool *allFit) const;

    /** Commit assignments through Doc::patchFixtures() -- all of them or
     *  none, so a block of patched fixtures can shift onto its own old
     *  channels. An assignment that leaves a patched fixture where it is
     *  is skipped. For a network fixture
     *  also points its universe's output at the fixture's IP, copying the
     *  plugin and interface of an existing ArtNet/sACN universe when the
     *  universe has none. A claimsPort assignment also writes its universe
     *  onto that node port. Returns how many were patched; anything that could
     *  not be done is described in @p problems. */
    int apply(const QList<Assignment> &assignments, QStringList *problems = NULL);

    /** The name the show's own universe naming gives @p universe for what is
     *  on it now. The scheme is learned from the names it has -- "U17-US1-1",
     *  "U65-US1-Tops": a prefix, the universe's number padded as they pad
     *  it, then what is on it -- and what is on it is its fixtures' names
     *  ("Lantern #1" reads "Lantern-1"; several, what their names share).
     *  A show with no such scheme gets just that. Empty when nothing is on
     *  it. apply() names a universe this way when it patches onto one still
     *  called "Universe N"; a name somebody gave is never changed. */
    static QString universeNameFor(Doc *doc, quint32 universe);

    /*********************************************************************
     * Like structures, patched alike
     *********************************************************************/

    /** What @p fixtureId is mounted on, as "truss:2", "tower:0", "pipe:4" or
     *  "platform:1"; empty when it stands free. */
    static QString mountKey(Doc *doc, quint32 fixtureId);
    /** A structure's name, from its key ("T-2"). */
    static QString structureName(Doc *doc, const QString &key);
    /** Every fixture on the structure @p key, in their order on it: along
     *  a truss or up a pipe, shelf by shelf up a tower, across a deck. */
    static QList<quint32> fixturesOn(Doc *doc, const QString &key);
    /** The other structures of the same kind carrying the same fixtures --
     *  the same models in the same modes, in the same order -- as @p key:
     *  the other three towers when one is picked. */
    static QStringList likeStructures(Doc *doc, const QString &key);

    /** Patch @p targets the way @p pattern is patched ("the lights on T1, 2,
     *  3, 4 should all be addressed the same"): each fixture takes the
     *  address of the one in the same place on the pattern -- paired in
     *  their order on it -- in its own structure's universe. That universe is
     *  where the target's first fixture is, and the rest follow the
     *  pattern's universes from there. A target with a different number or
     *  kind of fixtures is paired as far as it goes and said so in
     *  @p problems; one with nothing patched is left alone (it has no
     *  universe to keep). Pure, like plan(): the Patch view shows it. */
    QList<Assignment> matchLike(const QString &pattern, const QStringList &targets,
                                QStringList *problems = NULL) const;

private:
    Doc *m_doc;
    bool m_useProximity;
};

/** @} */

#endif
