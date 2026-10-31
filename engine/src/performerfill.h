/*
  Q Light Controller Plus
  performerfill.h

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

#ifndef PERFORMERFILL_H
#define PERFORMERFILL_H

#include <QColor>
#include <QList>
#include <QString>
#include <QVector3D>
#include <functional>

#include "qlcengine_export.h"

class Doc;
class MonitorProperties;
class StagePlatform;
class StageTarget;

/** @addtogroup engine Engine
 * @{
 */

/** People on the stage: performer lines.
 *
 *  A performer line is a Line target (StageTarget::isPerformerLine()) with a
 *  count of people spread evenly along it -- the first on one end, the last
 *  on the other -- each with their feet on the highest stage surface under
 *  them (a step's tread, a riser, the apron, the deck), so a line across a
 *  flight of steps stands people at every height it crosses, and moving the
 *  riser under them moves their feet. Their heights are a mean give or take
 *  a spread, each person's drawn from the line's seed so it is the same
 *  every time the show opens. The line is drawn, edited, aimed at, layered,
 *  grouped and saved like any line.
 *
 *  Several lines make a choir: "Fill with people" drops one per tier of the
 *  selected risers, in a "Performers" layer of their own, as one group. */
namespace PerformerFill
{
    /** How tall the people on a line are, on average. */
    enum HeightPreset { Children = 0, Students = 1, Adults = 2 };
    QLC_ENGINE_EXPORT float presetHeight(HeightPreset p);

    /** One person on a line: where their feet are (metres, world) and how
     *  tall they are. */
    struct Performer
    {
        QVector3D feet;
        float height = 0.0f;
    };

    /** The people on performer line @p t, first end to last; empty for any
     *  other target. */
    QLC_ENGINE_EXPORT QList<Performer> people(const MonitorProperties *props, const StageTarget *t);

    /** Person @p index's height on @p t: the mean (personHeight()) give or
     *  take heightSpread(), the same for the same seed. */
    QLC_ENGINE_EXPORT float personHeight(const StageTarget *t, int index);

    struct Options
    {
        /** Metres between people along a row, used when perRow is 0. */
        float spacing = 0.6f;
        /** This many to a row, spread evenly; 0 = by spacing. */
        int   perRow = 0;
        /** Each row shifted half a place from the one below, so every face
         *  is seen between the two in front ("windows"). By spacing only. */
        bool  stagger = true;
        float height = 1.60f;        ///< the mean, metres
        float spread = 0.0f;         ///< +/- metres
        QColor costume;              ///< invalid = the neutral body
        QString setName;             ///< the group's name; empty = "Performers"
        /** The performer lines and people already standing on these risers
         *  in the Performers layer go first, so filling again changes the
         *  set rather than doubling it. One an aim palette uses is kept. */
        bool  replace = true;
    };

    /** A row of people on a riser: its two end people (feet, metres, world)
     *  and how many stand on it. */
    struct Row
    {
        QVector3D a, b;
        int count = 0;
    };

    /** The rows @p o gives on @p p: one along the middle of each tread of a
     *  flight of steps, the bottom one first; a deck a row to every 0.9 m of
     *  its depth, the front one first. */
    QLC_ENGINE_EXPORT QList<Row> rows(const MonitorProperties *props, const StagePlatform *p,
                                      const Options &o);

    /** A new performer line from @p a to @p b with @p count people, in
     *  @p layerId (heights, costume and name from @p o; a fresh seed). */
    QLC_ENGINE_EXPORT StageTarget *addLine(MonitorProperties *props, const QVector3D &a,
                                           const QVector3D &b, int count, const Options &o,
                                           quint32 layerId);

    /** The layer performers go in, made the first time. */
    QLC_ENGINE_EXPORT quint32 ensureLayer(MonitorProperties *props);
    /** True when @p t is a performer line, or a person in the Performers
     *  layer. The views leave its position readout off -- seventy stage
     *  positions over a choir read as nothing. */
    QLC_ENGINE_EXPORT bool isPerformer(const MonitorProperties *props, const StageTarget *t);

    /** The people and performer lines standing on @p platformId: not bound
     *  to a structure, and the platform is the topmost deck under them (a
     *  line: under its centre). They move with it. */
    QLC_ENGINE_EXPORT QList<StageTarget *> standingOn(const MonitorProperties *props, quint32 platformId);

    /** The other performer lines in @p t's set (the group a fill makes, or
     *  one the user grouped): same group, not @p t. Empty for a line on its
     *  own. */
    QLC_ENGINE_EXPORT QList<StageTarget *> setMates(const MonitorProperties *props, const StageTarget *t);

    /** What a set shares when one line's look is changed in one place:
     *  costume colour, mean height and the give or take. Each line keeps its
     *  own seed (its own people) and its own count. Returns how many lines
     *  changed. */
    QLC_ENGINE_EXPORT int copyLookToSet(const MonitorProperties *props, const StageTarget *from);

    /** The targets an aim-target list offers. The lines of a fill (a set of
     *  performer lines) are left out, so a choir of fifteen rows does not
     *  bury the targets by name; one on its own is listed, and so is any
     *  line an aim palette uses or @p keepId (what the list already holds
     *  selected). */
    QLC_ENGINE_EXPORT QList<StageTarget *> pickable(Doc *doc, quint32 keepId = 0xFFFFFFFFu);

    struct Result
    {
        QList<quint32> targets;      ///< the lines made, in fill order
        int     people = 0;          ///< on them
        quint32 groupId = 0;         ///< the set; 0 when nothing was made
        quint32 layerId = 0;
        int     replaced = 0;        ///< lines and people taken away
    };

    /** A performer line on every row of each of @p platformIds. @p inUse
     *  says which targets something else uses (an aim palette): those are
     *  never replaced. */
    QLC_ENGINE_EXPORT Result fill(MonitorProperties *props, const QList<quint32> &platformIds,
                                  const Options &o, const std::function<bool(quint32)> &inUse = {});
}

/** @} */

#endif
