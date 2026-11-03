/*
  Q Light Controller Plus - qlcconsole
  placementcheck.h

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

#ifndef PLACEMENTCHECK_H
#define PLACEMENTCHECK_H

#include <QList>
#include <QString>
#include <QVector3D>

#include "qlcengine_export.h"

class Doc;

/**
 * Whether a fixture's body is buried in a solid stage feature (docs/design/
 * SHOW_REVIEW.md, FIX-15). The body is the box the rig view draws -- its
 * Physical Dimensions in the fixture's own frame (RigHeads::frame()), standing
 * on its position for a floor light and hanging from it for a hung one -- and
 * the features are the decks, risers and flights of steps with a solid top.
 * Pure reads, except fix(). One answer for Show Review (FIX-15), the footer's
 * "fixtures.buried" entry and the plot's mark.
 *
 * FIX-16 is its neighbour: a light left on the floor beside a deck that its
 * row-mates stand on (see OffRow). Row-mates are, in this order, the other
 * lights of the same fixture type in (1) a fixture group it belongs to (the
 * smallest first), (2) the same name stem ("UST-" for UST-2), (3) a line:
 * within 0.3 m of its Y. It fires when more than half of them (and at least
 * two) stand on a deck and the light is within a body length (0.5 m at least)
 * of one of those decks' edge.
 *
 * Not flagged: a fixture rigged to a structure -- clamped to a truss, pipe,
 * tower, hanger or a studio frame (those are placed by the structure) -- or
 * standing on / bolted to a platform (that platform's own volume is its mount);
 * a platform with a clear or open top (lights are rigged inside those on
 * purpose); a body that only touches a surface or sinks less than
 * tolerance().
 */
class QLC_ENGINE_EXPORT PlacementCheck
{
public:
    /** A body may sink this far (metres) into a surface, or reach this far into
     *  one sideways, before it counts: touching surfaces and rounding are not
     *  a placement mistake. */
    static float tolerance() { return 0.03f; }

    struct Buried
    {
        bool buried = false;        //!< false: clear (the rest is empty)
        quint32 fixtureId = 0;
        quint32 platformId = 0;     //!< the feature it is deepest in
        float depth = 0.0f;         //!< how far it is in, metres
        bool canSit = false;        //!< its middle is over a top it could stand on
        quint32 sitId = 0;          //!< that platform
        float topZ = 0.0f;          //!< its top surface, metres above the floor
    };

    /** Every patched-or-not fixture on the plot whose body is buried, worst
     *  first-found per fixture, in fixture-id order. */
    static QList<Buried> buried(Doc *doc);
    /** The one fixture; buried is false when it is clear. */
    static Buried check(Doc *doc, quint32 fixtureId);

    /** "UST-2 is inside Deck SR" */
    static QString problemText(Doc *doc, const Buried &b);
    /** "Sit it on Deck SR (0.61 m)", or "Move it clear of Deck SR" when its
     *  middle is not over a top it could stand on. */
    static QString fixText(Doc *doc, const Buried &b);
    /** Sit it on the top: a deck mount with no offset, XY kept, so it follows
     *  the platform. False when there is nothing to sit on. */
    static bool fix(Doc *doc, const Buried &b);

    /** A light on the floor beside the deck its row-mates stand on (FIX-16). */
    struct OffRow
    {
        bool off = false;           //!< false: clear (the rest is empty)
        quint32 fixtureId = 0;
        quint32 sitId = 0;          //!< the nearest deck the row uses
        float topZ = 0.0f;          //!< its top, metres above the floor
        float distance = 0.0f;      //!< from the light to that deck's edge, metres
        QList<quint32> mates;       //!< the row-mates on a deck, in name order
        int rowSize = 0;            //!< all its row-mates (on a deck or not)
        QList<quint32> decks;       //!< every deck those mates stand on
        QString row;                //!< how the row was found: "group Name", "name UST-", "line"
    };

    /** Every fixture on the floor whose row stands on a deck, in fixture-id
     *  order. Pure reads. */
    static QList<OffRow> offRow(Doc *doc);
    /** The one fixture; off is false when it is clear. */
    static OffRow checkRow(Doc *doc, quint32 fixtureId);

    /** "UST-2 is on the floor; UST-1, UST-3…UST-8 are on Deck USP1 (0.61 m)" */
    static QString problemText(Doc *doc, const OffRow &r);
    /** "Sit it on Deck USP1 (0.61 m)" */
    static QString fixText(Doc *doc, const OffRow &r);
    /** Sit it on the nearest deck its row uses: the same deck mount as the
     *  other fix(). False when there is nothing to sit on. */
    static bool fix(Doc *doc, const OffRow &r);

    /** The deck a fixture stands on: a deck mount, or a free light whose
     *  height is a solid top under it; 0xFFFFFFFF when it is on the floor, on
     *  a structure or nowhere. Exposed for tests. */
    static quint32 deckOf(Doc *doc, quint32 fixtureId);

    /** The 8 corners of a fixture's body in the world (metres); false when it
     *  has no place or is bound to a structure. Exposed for tests. */
    static bool body(Doc *doc, quint32 fixtureId, QVector3D corners[8]);
};

#endif
