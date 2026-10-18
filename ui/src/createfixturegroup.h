/*
  Q Light Controller
  createfixturegroup.h

  Copyright (c) Heikki Junnila

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

#ifndef CREATEFIXTUREGROUP_H
#define CREATEFIXTUREGROUP_H

#include <QDialog>
#include <QList>

#include "ui_createfixturegroup.h"
#include "grouphead.h"
#include "qlcpoint.h"
#include <QMap>
#include <functional>

class Doc;
class FixtureGroup;
class QLCFixtureMode;

/** @addtogroup ui_fixtures
 * @{
 */

class CreateFixtureGroup final : public QDialog, public Ui_CreateFixtureGroup
{
    Q_OBJECT
    Q_DISABLE_COPY(CreateFixtureGroup)

public:
    CreateFixtureGroup(QWidget* parent);
    ~CreateFixtureGroup();

    QString name() const;

    void setSize(const QSize& size);
    QSize size() const;

    /** The ONE way to create a fixture group from a selection, shared by the
     *  Fixture Manager, the Programming tab and the Studio so the flow is
     *  consistent everywhere: suggest a grid (from the fixtures' 2-D layout when
     *  placed, else a near-square of the head count), prompt name+size, then
     *  arrange the fixtures into the grid IN PHYSICAL ORDER (rows top→bottom,
     *  columns left→right). Returns the new group, or NULL if cancelled/empty. */
    static FixtureGroup *createFromFixtures(Doc *doc,
                                            const QList<quint32> &fixtureIds,
                                            QWidget *parent);

    /** The grid half of createFromFixtures(), off the stage: @p fixtureIds
     *  sorted into physical order by their place on the 2D plot (rows top to
     *  bottom, columns left to right; unplaced ones after, as picked), and
     *  every head of each assigned in turn to the next free cell of
     *  @p target's grid, row by row. Set @p target's size first. */
    static void layoutFixturesInGrid(Doc *doc, const QList<quint32> &fixtureIds,
                                     FixtureGroup *target);

    /** Copy @p groupIds (each as a block keeping its own layout, tagged as a
     *  sub-group so it moves as a unit, one block under the other) and then
     *  @p fixtureIds (every head, in a row of their own) into @p target,
     *  below whatever it already holds, growing its grid to fit. Heads it
     *  already has are skipped. No signals blocked, no Doc modified flag:
     *  the caller's to do. */
    static void appendSelection(Doc *doc, FixtureGroup *target,
                                const QList<quint32> &fixtureIds,
                                const QList<quint32> &groupIds);

    /** A group OF GROUPS, laid out the way they stand on stage: each group in
     *  @p groupIds goes in as a block keeping its own head layout (tagged as a
     *  sub-group, so it moves as a unit), placed where its fixtures are --
     *  across by stage position (stage right on the left, as the audience
     *  sees it), up and down by height, or by upstage/downstage when they are
     *  all at one height -- in one scale taken from the groups' own layouts,
     *  so the spacing between them is the rig's. On stage, StageGroupDialog
     *  asks the name and the view (from the front, from above) and previews
     *  it.
     *  Returns the new group, or NULL if cancelled/empty. */
    static FixtureGroup *createFromGroups(Doc *doc, const QList<quint32> &groupIds,
                                          QWidget *parent);
    /** The layout half of createFromGroups(), into @p target (which should be
     *  empty). Pure: no dialog. Returns false when there was nothing to place.
     *  @p view: StageView -- from the front, up the grid is up; from above, up
     *  the grid is upstage. @p ordinal: the groups in stage order, gaps closed
     *  up, rather than to scale. Each group keeps its own layout either way. */
    static bool layoutGroupsByStage(Doc *doc, const QList<quint32> &groupIds,
                                    FixtureGroup *target, int view = -1,
                                    bool ordinal = false);

    /** One block for layoutBlocksByStage(): heads in their own grid, where it
     *  is (its fixtures), and -- for one fixture's LED grid, whose fixtures
     *  are one point -- how many cells it has to the metre. */
    struct StageBlock
    {
        quint32 tag = 0;                   ///< sub-group tag for its heads (0 = none)
        QMap<QLCPoint, GroupHead> heads;
        QList<quint32> fixtures;
        double cellsPerMetre = 0.0;        ///< 0 = take it from the fixtures' spread
    };
    /** Lay @p blocks out as they stand on stage (see createFromGroups()).
     *  @p vertical: 1 = by height, 0 = by depth, -1 = decide from the blocks.
     *  @p ordinal: in stage ORDER rather than to scale -- blocks overlapping
     *  side to side (or up and down) share a band, bands in order. */
    static bool layoutBlocksByStage(Doc *doc, const QList<StageBlock> &blocks,
                                    FixtureGroup *target, int vertical = -1,
                                    bool ordinal = false);

    /** Each of @p fixtureIds' heads of @p kind ("RGB", "W" -- HeadKinds) as a
     *  block: its cells from the model's LED map (MonitorProperties::
     *  headLayoutFor()), rows and columns of other kinds squeezed out, turned
     *  the way the fixture is rigged (a bar hanging straight down is a column).
     *  @p vertical receives the choice made for the page's down direction. */
    static QList<StageBlock> headKindBlocks(Doc *doc, const QList<quint32> &fixtureIds,
                                            const QString &kind, int *vertical = NULL,
                                            int view = -1);

    /** headKindBlocks(), by head TYPE (HeadKinds::typeOf(): the kind, and
     *  whether it moves) rather than kind; an empty @p type takes every head. */
    static QList<StageBlock> headTypeBlocks(Doc *doc, const QList<quint32> &fixtureIds,
                                            const QString &type, int *vertical = NULL,
                                            int view = -1);

    /** Which way a group is laid out: as seen from the front (the audience's
     *  view: across, and up the page is up) or from above (across, and up
     *  the page is upstage). Auto picks whichever the fixtures spread in. */
    enum StageView { ViewAuto = -1, ViewTop = 0, ViewFront = 1 };

    /** @p fixtureIds' heads (of @p kind, or all of them when empty) laid out
     *  into @p target as seen from @p view: each fixture's own LED grid where
     *  it stands and turned the way it is rigged. @p closeGaps keeps the order
     *  but drops empty rows and columns, for a solid pixel map. */
    static bool layoutFixturesByStage(Doc *doc, const QList<quint32> &fixtureIds,
                                      FixtureGroup *target, int view, bool closeGaps,
                                      const QString &kind = QString());

    /** Drop @p grp's empty rows and columns, keeping the order of the rest. */
    static void closeGaps(FixtureGroup *grp);

    /** An existing group holding exactly the same heads as @p heads (in any
     *  layout), or NULL -- so the same group is not made twice. */
    static FixtureGroup *findSameGroup(Doc *doc, const QMap<QLCPoint, GroupHead> &heads,
                                       const FixtureGroup *except = NULL);

    /** The groups like @p fg, @p fg first: the same fixture models and modes
     *  (as many of each), and when @p sameLayout also the same grid -- each
     *  cell the same head of the same model -- so the four step fronts, each
     *  its own eight bars, find each other. */
    static QList<FixtureGroup *> similarGroups(Doc *doc, const FixtureGroup *fg, bool sameLayout);

    /** A fixture's model and mode as one key; a generic dimmer's type and
     *  channel count, as those have no model. */
    static QString fixtureModelKey(const Fixture *fxi);

    /** A fixture group per head kind, each laid out as on stage from the LED
     *  maps. @p named: (kind, the group's name) for each group to make, in
     *  order. No dialog. */
    static QList<FixtureGroup *> createByHeadKind(Doc *doc, const QList<quint32> &fixtureIds,
                                                  const QList<QPair<QString, QString> > &named,
                                                  int view = ViewAuto, bool closeGaps = false);
    /** The name a kind's group is offered under: "<base> <kind>", or just the
     *  base when it already ends with that kind ("T1-CW-RGB" for RGB). */
    static QString headKindGroupName(const QString &base, const QString &kind);

    /** Suggest a grid size for @p fixtureIds — a real cols×rows from their 2-D
     *  positions if placed, otherwise a near-square of the total head count. */
    static QSize suggestGrid(Doc *doc, const QList<quint32> &fixtureIds);

private:
    /** The blocks of headKindBlocks() for the heads @p keep accepts. */
    static QList<StageBlock> headBlocks(Doc *doc, const QList<quint32> &fixtureIds,
                                        const std::function<bool(const QLCFixtureMode *, int)> &keep,
                                        int *vertical, int view);
};

/** @} */

#endif
