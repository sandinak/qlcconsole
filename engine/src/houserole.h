/*
  Q Light Controller Plus
  houserole.h

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

#ifndef HOUSEROLE_H
#define HOUSEROLE_H

#include <climits>
#include <QString>
#include <QList>
#include <QMap>
#include <QSet>

#include "qlcengine_export.h"

class FixtureGroup;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * House roles (docs/design/VENUE_ROLES_DATUM.md section 3, board Q286-Q289):
 * a fixed set of jobs a room's lights do -- the house lights, the front
 * wash, the work lights -- so a cue written on the job ("House to half")
 * plays in any room on whichever of its lights fill it.
 *
 * A role is a tag on one ordinary fixture group (FixtureGroup::role()),
 * filled per venue by the venue's house fixtures as any group is
 * (VenueRig::Fill). Nothing new plays them: the standard cues are reusable
 * cues on the role groups, and their Global hits are uses of them.
 */
namespace HouseRole
{
    enum Role
    {
        None = 0,
        HouseLights,
        Aisle,
        Front,
        Top,
        Side,
        Back,
        Cyc,
        Work
    };

    /** The eight roles, in the order the venue page lists them. */
    QLC_ENGINE_EXPORT QList<Role> roles();
    /** "House", "Aisle", "Front", ... (the XML words); empty for None. */
    QLC_ENGINE_EXPORT QString xmlName(Role role);
    /** The other way; anything else is None. */
    QLC_ENGINE_EXPORT Role fromXmlName(const QString &text);
    /** The role group's name, as made: "House Lights", "Stage Wash Front" ... */
    QLC_ENGINE_EXPORT QString groupName(Role role);
    /** The folder the role groups are made in ("Roles"). */
    QLC_ENGINE_EXPORT QString folder();

    /** The group tagged @p role (null: not made yet). */
    QLC_ENGINE_EXPORT FixtureGroup *group(const Doc *doc, Role role);
    /** Whether the show has made its role groups (any is tagged). */
    QLC_ENGINE_EXPORT bool hasRoles(const Doc *doc);

    /*********************************************************************
     * Suggesting fills (section 3.5)
     *********************************************************************/

    /** One house fixture's suggested role, and why ("\"House\" in its
     *  name", "a moving light: add it by hand"). */
    struct Suggestion
    {
        quint32 fixture = UINT_MAX;
        Role role = None;
        QString why;
    };

    /** Each house fixture of the venue applied now (VenueRig::
     *  venueFixtures()) with a role, or none, and the reason: by the words
     *  in its name, else in its groups' names; a moving light is none; a
     *  CYC by its model; else where it hangs against the show's curtain
     *  line. Never on channels the venue keeps (not ours), nor an
     *  unpatched one. In fixture-id order. Empty with no venue applied. */
    QLC_ENGINE_EXPORT QList<Suggestion> suggest(const Doc *doc);

    /** Roles with no group yet whose name a plain group of the show has
     *  already ("Cyc"): Fill Roles asks once whether to use it. */
    QLC_ENGINE_EXPORT QList<Role> nameClashes(const Doc *doc);

    /*********************************************************************
     * The standard cues (section 3.3)
     *********************************************************************/

    /** One layer of a standard cue: a role at a level (0..255). */
    struct Layer
    {
        Role role = None;
        int level = 0;
    };
    struct StandardCue
    {
        QString key;            //!< stable, never translated: ReusableCue::standard
        QString name;
        QList<Layer> layers;
        uint fade = 0;          //!< ms, in and out
    };
    /** House to half, House out, House full, Stage up, Stage out, Work
     *  lights (board Q288). */
    QLC_ENGINE_EXPORT QList<StandardCue> standardCues();
    /** The reusable cue of standard cue @p key ("house-half"), whatever
     *  it is called now; invalid when there is none. */
    QLC_ENGINE_EXPORT quint32 standardCue(const Doc *doc, const QString &key);

    /** What the first Fill Roles made (and every later one added back). */
    struct Made
    {
        int groups = 0;
        int palettes = 0;
        int cues = 0;
        int hits = 0;
        /** His groups of a role's name not used for it because they hold
         *  tour lights: the role was made beside each. */
        QStringList besides;
        /** No production yet: the standard cues and hits wait for one. */
        bool waiting = false;
    };

    /** Make what is missing, never changing what is there: the role groups
     *  (in the Roles folder; for a role in @p useExisting, the show's group
     *  of that name is tagged instead, else a "Cyc (role)" is made beside
     *  it), the palettes (Dimmer full, 50%, out and Colour Open White, an
     *  existing one with exactly those values reused), the standard
     *  reusable cues (Dimmer + Open White on each role) and, when the show
     *  has a production, one override Global hit per standard cue (at the
     *  top of the tree: a Global hit keeps no folder). A cue or hit he edited is left as it is; one he
     *  deleted is made again. */
    QLC_ENGINE_EXPORT Made ensure(Doc *doc, const QSet<Role> &useExisting = QSet<Role>());

    /** Every role group's lights that are not House (a group from an old
     *  file, an owner changed since) taken out: a standard cue never drives
     *  a tour light (board Q289). The groups changed; with @p tell each
     *  says so (else the caller does). */
    QLC_ENGINE_EXPORT QSet<FixtureGroup *> keepHouseOnly(Doc *doc, bool tell = true);

    /** Fill Roles (section 3.5): with venue @p venue applied, each role in
     *  @p rows becomes filled by exactly those of its house fixtures (the
     *  tour heads in the group are kept); roles not in @p rows are left
     *  as they are. ensure() first. Written to the venue's file. False,
     *  with @p why, when @p venue is not the venue applied. */
    QLC_ENGINE_EXPORT bool fillRoles(Doc *doc, const QString &venue, const QMap<Role, QList<quint32>> &rows,
                                     const QSet<Role> &useExisting = QSet<Role>(), Made *made = nullptr,
                                     QString *why = nullptr);

    /*********************************************************************
     * Saying so on screen (section 3.4)
     *********************************************************************/

    /** The role of group @p groupId (None: not a role group). */
    QLC_ENGINE_EXPORT Role roleOf(const Doc *doc, quint32 groupId);
    /** A layer on @p groups that are all role groups with no light in
     *  them: "none in PMS Auditeria" (the venue applied; "none in this
     *  show" with none), with ": call the house desk" for House Lights
     *  when the venue keeps channels of its own. Empty otherwise -- a
     *  layer on any light, or on a group that is no role. */
    QLC_ENGINE_EXPORT QString emptyText(const Doc *doc, const QList<quint32> &groups);
}

/** @} */

#endif
