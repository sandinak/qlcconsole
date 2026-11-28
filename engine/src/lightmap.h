/*
  Q Light Controller Plus - qlcconsole
  lightmap.h

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

/*
  How much light reaches a person standing on the stage: the light level at
  subject height (5' 6" unless the show says otherwise), as a light meter
  held there and pointed at each light in turn would add it up.

  A light is a cone from its lens: its output in lumens (the definition's
  declared lumens, else its declared watts at 35 lm/W, else a 100 W LED
  fixture's 3500 lm -- the rig views' own figure, fixtureLumens()) spread
  over the solid angle of its beam, times how far up it is. The level at a
  point is that intensity over the square of the distance, half as bright
  at the beam angle as in the middle and fading out past it (a wash further
  than a profile), and nothing where something solid on the stage stands
  between the light and the point (BeamOcclusion, the kinds the stage counts).

  An estimate for finding the dark spots, not a photometric report: no
  reflected light, no gobo, a multi-head fixture as one light.

  The Rig Overview's light map (StructureStudioView) and Show Review's
  PWR-14 (a cue that leaves an acting area dark) both read it. Pure: compute()
  touches no Doc and is safe on any thread.

  Coordinates as everywhere else: X stage left (+), Y downstage (+), Z up,
  metres.
*/

#ifndef LIGHTMAP_H
#define LIGHTMAP_H

#include <QVector3D>
#include <QVector>
#include <QRectF>
#include <QList>
#include <atomic>
#include <climits>

#include "qlcengine_export.h"
#include "beamocclusion.h"

class MonitorProperties;
class QLCPhysical;
class Fixture;

namespace LightMap
{
    /** Subject height when the show sets none: 5' 6" (1.6764 m). */
    QLC_ENGINE_EXPORT double defaultHeight();
    /** Below this many lux a place reads as dark when the show sets none:
     *  10 foot-candles (107.6 lx). */
    QLC_ENGINE_EXPORT double defaultDarkLux();
    /** The reference full beam the rig views size light by: a 100 W LED
     *  fixture, 3500 lm. */
    QLC_ENGINE_EXPORT double referenceLumens();

    /** What @p fx puts out, in lumens: its declared lumens, else its declared
     *  watts at 35 lm/W, else referenceLumens() with @p known false. */
    QLC_ENGINE_EXPORT double fixtureLumens(const Fixture *fx, bool *known = nullptr);
    /** The beam angle (degrees, full) to light with for @p phys: its widest
     *  declared lens, a declared 170 or more as a 150-degree flood, none as
     *  40 -- the rig views' own reading. */
    QLC_ENGINE_EXPORT double beamDegrees(const QLCPhysical &phys);

    /** One light, as the map reads it. */
    struct Source
    {
        quint32 fixture = UINT_MAX;
        QVector3D apex;             ///< the lens, metres
        QVector3D dir;              ///< unit vector along the beam
        float cosHalf = 1.0f;       ///< cos of half the beam angle
        float cosEdge = 1.0f;       ///< cos of where its edge has faded out
        float halfRad = 0.0f;
        float edgeRad = 0.0f;
        float candela = 0.0f;       ///< on the beam's axis, at its level now
        /** What it hangs on: never in its own way (BeamOcclusion::ownMounts). */
        QVector<BeamOcclusion::Exclude> exclude;
    };

    /** A source for @p lumens of light at @p level (0..1) thrown along
     *  @p dir from @p apex in a cone @p beamDeg wide. @p softness 0 is a
     *  profile (crisp: gone at 1.1 times its beam angle), 1 a wash (fading
     *  out to 1.4 times it). */
    QLC_ENGINE_EXPORT Source makeSource(quint32 fixture, const QVector3D &apex, const QVector3D &dir,
                                        double beamDeg, double lumens, double level,
                                        double softness = 0.5);

    /** How much of a beam's axis intensity reaches @p cosAngle off its axis:
     *  1 on the axis, 0.5 at the beam angle, 0 past the faded edge. */
    QLC_ENGINE_EXPORT double profile(const Source &s, double cosAngle);

    /** Lux at @p p from @p sources, with @p sight (null: nothing blocks). */
    QLC_ENGINE_EXPORT double luxAt(const QVector<Source> &sources, const BeamOcclusion::Model *sight,
                                   const QVector3D &p);

    /** A grid of points over the stage at subject height. */
    struct Grid
    {
        QRectF area;                ///< what it covers, metres (x stage left, y downstage)
        double step = 0.0;          ///< metres between points
        int cols = 0, rows = 0;
        double height = 0.0;        ///< subject height above the deck
        double darkLux = 0.0;
        QVector<float> z;           ///< each point's height: the deck there plus height
        QVector<float> lux;         ///< filled by compute()

        bool isValid() const { return cols > 0 && rows > 0; }
        int index(int col, int row) const { return row * cols + col; }
        /** The middle of cell (@p col, @p row): where it is sampled. */
        QVector3D point(int col, int row) const;
        bool dark(int col, int row) const { return lux.value(index(col, row)) < float(darkLux); }
    };

    /** Where the stage is: its whole width, and from the back wall to the
     *  downstage edge (MonitorProperties::downstageEdgeY()). */
    QLC_ENGINE_EXPORT QRectF stageArea(const MonitorProperties *mp);

    /** A grid over @p area at @p height above the deck (platforms raise it)
     *  with no more than @p maxCells points (the step grows to fit, never
     *  under 0.15 m). Read on the GUI thread: it asks @p mp. */
    QLC_ENGINE_EXPORT Grid layout(const MonitorProperties *mp, const QRectF &area, double height,
                                  double darkLux, int maxCells = 2400);

    /** Fill @p grid's lux from @p sources. Safe on any thread (the sight
     *  model is immutable). False when @p cancel was set before it finished. */
    QLC_ENGINE_EXPORT bool compute(Grid &grid, const QVector<Source> &sources,
                                   const BeamOcclusion::ModelPtr &sight,
                                   const std::atomic<bool> *cancel = nullptr);

    /** The dark spots of a computed grid: each a list of the cells (index())
     *  under its darkLux that touch side by side, the biggest first. */
    QLC_ENGINE_EXPORT QList<QVector<int>> darkSpots(const Grid &grid);
}

#endif
