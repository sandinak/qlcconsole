/*
  Q Light Controller Plus
  structureduplicate.h

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

#ifndef STRUCTUREDUPLICATE_H
#define STRUCTUREDUPLICATE_H

#include <climits>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include "qlcengine_export.h"
#include "monitorproperties.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/** Duplicate a structure's setup (T1 to T2, T3 ...).
 *
 *  A structure here is what PatchPlanner::mountKey() names ("truss:2",
 *  "tower:0", "pipe:4", "platform:1"). The fixtures hung directly on it are
 *  copied onto the other structures of its kind: the same definition, mode
 *  and settings, the same place on the structure (mount offsets, face,
 *  orientation and calibration offsets are the fixture's own and travel
 *  with it), a name that follows the show's own, addresses, and a place in
 *  the groups the originals are in.
 *
 *  preview() decides all of it without changing anything; apply() does it
 *  and returns what undo() needs to put it all back as one step.
 *
 *  Not part of "the setup" (yet): hangers and child bars, and the lights
 *  hung on them. */
class QLC_ENGINE_EXPORT StructureDuplicate
{
public:
    /** Where the copies are patched. */
    enum PatchMode
    {
        NextFree,       ///< the next free addresses after the originals, spilling on
        Offset,         ///< the original's address + the step, copy by copy
        Unpatched       ///< left in the inventory to address later
    };
    /** What to do with a target that already carries fixtures. */
    enum Existing
    {
        Skip,           ///< leave the target alone
        Add,            ///< copy alongside what is there
        Replace         ///< remove what is there first
    };
    /** What a group the originals are in gets. */
    enum GroupMode
    {
        AddToGroups,    ///< the copies join the originals' groups
        ParallelGroups  ///< a group of their own, named by the structure ("T2 Spots")
    };

    struct Options
    {
        PatchMode patch = NextFree;
        /** Offset: addresses per copy (the Nth target is N steps on from the
         *  original) and universes per copy. */
        int addressStep = 0;
        int universeStep = 0;
        /** Fixture groups (Fixture Manager) and the plot's map groups are
         *  two kinds of group, each decided on its own.
         *  decided: fixture groups take the copies (a group of "all spots"
         *  should hold them); map groups go parallel (a folder on the plot
         *  is mostly one structure's). A group NAMED for the structure is
         *  always paralleled, whichever is chosen: adding T2's lights to
         *  "T1 Spots" would be wrong. */
        GroupMode fixtureGroups = AddToGroups;
        GroupMode mapGroups = ParallelGroups;
        /** Per target key, else @c defaultExisting.
         *  decided: Skip (the safe one; nothing is removed or doubled). */
        QHash<QString, Existing> existing;
        Existing defaultExisting = Skip;

        Existing existingFor(const QString &target) const
        { return existing.value(target, defaultExisting); }
    };

    /** One copy to be made. */
    struct Row
    {
        quint32 sourceId = 0;       ///< the original
        QString target;             ///< the structure key it goes on
        QString name;               ///< what it is called
        bool patched = false;
        quint32 universe = 0;       ///< zero-based
        quint32 address = 0;        ///< zero-based
        quint32 channels = 0;
        QString note;               ///< a caveat: why it is left unpatched, say
        bool clash = false;         ///< wanted an address it cannot have
    };

    /** One group line of the preview. */
    struct GroupLine
    {
        QString kind;               ///< "fixture group" / "map group"
        QString name;               ///< the group (the new one, when parallel)
        QString source;             ///< the original's name
        QString target;             ///< the structure key it is for
        bool parallel = false;
        int members = 0;            ///< copies it holds
    };

    struct Preview
    {
        QList<Row> rows;
        QList<GroupLine> groups;
        QStringList skipped;        ///< targets left alone (keys)
        QStringList problems;
        /** Targets that already carry fixtures: key -> how many. */
        QMap<QString, int> occupied;
        /** Fixtures a Replace removes that scenes use, and those scenes. */
        int replacedFixtures = 0;
        int replacedInScenes = 0;
    };

    /** A fixture taken off a target by Replace, to be put back. */
    struct Removed
    {
        quint32 id = 0;
        QByteArray xml;
        FixtureRigProps rig;
        FixturePreviewItem item;
    };

    /** A fixture group as it was, to be put back. */
    struct GroupState
    {
        quint32 id = 0;
        QByteArray xml;
    };

    /** Everything undo() needs. */
    struct Undo
    {
        QList<quint32> createdFixtures;
        QList<Removed> removed;
        QList<quint32> createdFixtureGroups;
        QList<GroupState> groupsBefore;
        QList<quint32> createdMapGroups;
        bool isEmpty() const
        { return createdFixtures.isEmpty() && removed.isEmpty(); }
    };

    struct Result
    {
        int created = 0;
        int patched = 0;
        int replaced = 0;
        QStringList skipped;
        QStringList problems;
        QList<quint32> createdIds;
        Undo undo;
    };

    /** The structures @p key can be duplicated to: the others of its kind,
     *  by name. Empty when @p key carries nothing to copy. */
    static QStringList targetsFor(Doc *doc, const QString &key);

    /** What a duplicate of @p source onto @p targets would do. Changes
     *  nothing. */
    static Preview preview(Doc *doc, const QString &source, const QStringList &targets,
                           const Options &options);

    /** Do it. The Result's undo puts everything back. */
    static Result apply(Doc *doc, const QString &source, const QStringList &targets,
                        const Options &options);

    /** Put back what apply() did. */
    static void undo(Doc *doc, const Undo &undo);

    /** @p name with the structure's name swapped for the target's, the way
     *  the show writes them: "T1-Spot 3" on T1 is "T2-Spot 3" on T2, and
     *  "T-1" is found in "T1 Wash" too. A name with no trace of the
     *  structure gets the target's name in front ("T2 Spot 3"). */
    static QString renamed(const QString &name, const QString &sourceStructure,
                           const QString &targetStructure);
};

/** @} */

#endif
