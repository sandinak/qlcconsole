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

class MonitorProperties;
class StagePlatform;
class StageTarget;

/** @addtogroup engine Engine
 * @{
 */

/** Filling risers with people: a choir on its steps, a band on its decks.
 *
 *  A performer is an ordinary Person target (StageTarget::Kind::Person), so
 *  it is drawn, lit, aimed at, saved, copied, layered and grouped like the
 *  one you place by hand. What this adds is the placing: a row of people on
 *  every tread of a flight of steps, or across a deck, at the right height
 *  and facing the house -- in a "Performers" layer of their own, as one
 *  group (the set) that selects and moves as a piece. */
namespace PerformerFill
{
    /** How tall the people in a set are. */
    enum HeightPreset { Children = 0, Students = 1, Adults = 2 };
    QLC_ENGINE_EXPORT float presetHeight(HeightPreset p);

    struct Options
    {
        /** Metres between people along a row, used when perRow is 0. */
        float spacing = 0.6f;
        /** This many to a row, spread evenly; 0 = by spacing. */
        int   perRow = 0;
        /** Each row shifted half a place from the one below, so every face
         *  is seen between the two in front ("windows"). By spacing only. */
        bool  stagger = true;
        float height = 1.75f;
        QColor costume;              ///< invalid = the neutral body
        QString setName;             ///< the group's name; empty = "Performers"
        /** People already standing on these risers in the Performers layer
         *  go first, so filling again changes the set rather than doubling
         *  it. One an aim palette uses is kept. */
        bool  replace = true;
    };

    /** Where the people stand on @p p: feet positions, metres, world. One row
     *  per tread of a flight of steps; a deck holds as many rows as its
     *  depth gives room for (0.9 m each), the front row downstage. */
    QLC_ENGINE_EXPORT QList<QVector3D> spots(const MonitorProperties *props, const StagePlatform *p,
                           const Options &o);

    /** The layer performers go in, made the first time. */
    QLC_ENGINE_EXPORT quint32 ensureLayer(MonitorProperties *props);
    /** True when @p t is one of a set: a person in the Performers layer.
     *  The views leave its position readout off -- seventy stage positions
     *  over a choir read as nothing. */
    QLC_ENGINE_EXPORT bool isPerformer(const MonitorProperties *props, const StageTarget *t);

    /** The Person targets standing on @p platformId: not bound to a
     *  structure, and the platform is the topmost deck under their feet.
     *  They move with it. */
    QLC_ENGINE_EXPORT QList<StageTarget *> standingOn(const MonitorProperties *props, quint32 platformId);

    struct Result
    {
        QList<quint32> targets;      ///< made, in fill order
        quint32 groupId = 0;         ///< the set; 0 when nothing was made
        quint32 layerId = 0;
        int     replaced = 0;
    };

    /** Stand people on each of @p platformIds. @p inUse says which targets
     *  something else uses (an aim palette): those are never replaced. */
    QLC_ENGINE_EXPORT Result fill(MonitorProperties *props, const QList<quint32> &platformIds,
                const Options &o, const std::function<bool(quint32)> &inUse = {});
}

/** @} */

#endif
