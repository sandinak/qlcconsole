/*
  Q Light Controller Plus
  headtypegroups.h

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

#ifndef HEADTYPEGROUPS_H
#define HEADTYPEGROUPS_H

#include <QCoreApplication>
#include <QStringList>
#include <QList>
#include <QSize>
#include <QMap>

#include "grouphead.h"
#include "qlcpoint.h"

class FixtureGroup;
class Doc;

/** @addtogroup ui_fixtures
 * @{
 */

/** Fixture groups made from a selection of fixtures and groups: one with
 *  every head (each group selected a block in it, as it is laid out), a
 *  group per TYPE of head across them all (HeadKinds::typeOf(): eight bars of
 *  16 RGB pixels and 4 whites make "Bar · RGB pixels" with 128 heads and
 *  "Bar · White pixels" with 32), or a group per type of head for each
 *  fixture ("Bar 1 · RGB pixels", "Bar 1 · White pixels", ...).
 *
 *  plan() works the groups out -- names, heads, layout -- without touching
 *  the show, so the dialog can preview them and the tests can check them;
 *  create() makes them. No widgets here. */
class HeadTypeGroups
{
    Q_DECLARE_TR_FUNCTIONS(HeadTypeGroups)

public:
    enum Split
    {
        OneGroup = 0,           ///< every head of every fixture
        PerType = 1,            ///< a group per head type, across the fixtures
        PerTypePerFixture = 2   ///< a group per head type, for each fixture
    };

    struct Options
    {
        Split split = PerType;
        /** Groups selected as well as the fixtures. A group per type takes
         *  their fixtures' heads; one group takes each as a block, keeping
         *  its layout (below), the fixtures in a row under them. */
        QList<quint32> groupIds;
        /** OneGroup with groups: each a block as it is laid out now
         *  (CreateFixtureGroup::appendSelection()), rather than every
         *  fixture laid out afresh. */
        bool groupsAsBlocks = true;
        /** OneGroup off the stage: the grid the heads fill, row by row, in
         *  the fixtures' 2D-plot order; invalid: CreateFixtureGroup::
         *  suggestGrid(). */
        QSize grid;
        /** PerType: a group per type AND fixture model, rather than the same
         *  type on different models sharing one. */
        bool keepModelsApart = false;
        int view = -1;          ///< CreateFixtureGroup::StageView, for placed fixtures
        bool closeGaps = true;  ///< placed fixtures: in order, empty rows/columns dropped
        /** What the groups are named after; empty: what each group's
         *  fixtures' names share (HeadTypesDialog::suggestName()), else
         *  what the whole selection's do. A group per fixture always takes
         *  the fixture's name. */
        QString baseName;
        QString folder;         ///< the groups' folder path (empty: none)
    };

    struct Planned
    {
        /** Stable across re-plans with other names/layout options, so an
         *  edited name or an unticked group can be kept: "all", the type
         *  ("RGB", "W|<model>" with models apart), or "<fixture id>|<type>". */
        QString key;
        QString name;
        QString path;
        QString type;                   ///< HeadKinds::typeOf(); empty for OneGroup
        QList<quint32> fixtures;
        QSize size;
        QMap<QLCPoint, GroupHead> heads;
        QMap<QLCPoint, quint32> subGroups;  ///< the blocks' sub-group tags
        bool onStage = false;           ///< laid out from where its fixtures stand
        bool byGrid = false;            ///< filled into Options::grid, row by row
        bool asBlocks = false;          ///< the groups selected, as blocks
        quint32 sameAs;                 ///< an existing group with exactly these heads
        Planned();
    };

    /** The groups @p opt makes of @p fixtureIds (and @p opt's groups), in
     *  order. */
    static QList<Planned> plan(Doc *doc, const QList<quint32> &fixtureIds, const Options &opt);

    /** Make @p groups in @p doc (as planned, names and paths included), in
     *  one go. Returns them. */
    static QList<FixtureGroup *> create(Doc *doc, const QList<Planned> &groups);

    /** The distinct head types of @p fixtureIds, in the order they first
     *  appear, with how many heads of each (for "128 RGB, 32 White"). */
    static QList<QPair<QString, int> > typeCounts(Doc *doc, const QList<quint32> &fixtureIds);

    /** The name groups of @p fixtureIds are made under when none is given. */
    static QString baseNameFor(Doc *doc, const QList<quint32> &fixtureIds);

    /** @p fixtureIds, and the fixtures of @p groupIds after them, each once. */
    static QList<quint32> fixturesOf(Doc *doc, const QList<quint32> &fixtureIds,
                                     const QList<quint32> &groupIds);

    /** True when every one of @p fixtureIds has more than one head of
     *  @p type on it and the type does not move: its heads are pixels. */
    static bool arePixels(Doc *doc, const QList<quint32> &fixtureIds, const QString &type);

    /** "<base> · <type said plainly>", "<base> · <type> pixels" when
     *  @p pixels ("Bar · RGB pixels"; "Par · RGB"; "Spot · RGBW movers"). */
    static QString groupName(const QString &base, const QString &type, bool pixels = false);
};

/** @} */

#endif
