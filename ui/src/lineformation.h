/*
  Q Light Controller Plus
  lineformation.h

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

#ifndef LINEFORMATION_H
#define LINEFORMATION_H

#include <QPointF>
#include <QPolygonF>
#include <QString>
#include <QColor>
#include <QList>

#include "linespread.h"
#include "beamlanding.h"
#include "showtreemodel.h"
#include "qlcui_export.h"

class Doc;
class Scene;
class QLCPalette;
class ShowTreeController;

/** @addtogroup ui UI
 * @{
 */

/**
 * What a line aim does to its lights, for drawing: the one model behind the
 * order strip (LineSpreadPreview) and the plan view (LinePlanView), so the
 * Look Editor's Aim page, a Show Tree layer's Position rows and a Formation
 * palette's preview all draw the same thing.
 *
 * It is worked out by the engine's own rule -- AimSolver::orderedLineHeads,
 * LineSpread::fractions / groupOf (what AimSolver::lineFractions uses) and
 * AimSolver::linePoints for the stage points -- so the picture cannot
 * disagree with what the rig plays.
 *
 * Each light knows its group: lights that move together share a colour
 * (groupColour(), a fixed order of seven colour-blind-checked hues) and a
 * bracket on the strip. Each light also knows its SLOT -- where it would land
 * spread evenly end to end in the order it hangs -- so a connector from slot
 * to point shows what crosses: the connectors (and the plan's beams) cross
 * exactly when the lights' order on the line is not the order they hang in.
 */
class QLC_UI_EXPORT LineFormation
{
public:
    struct Light
    {
        quint32 fixture = 0;        ///< 0: a stand-in (no scene to say which lights)
        int number = 0;             ///< 1 = the light nearest the line's start
        int group = 0;              ///< 0 = the first group along the line
        double slot = 0.5;          ///< 0..1: where it hangs, evenly in order
        double fraction = 0.5;      ///< 0..1: where it lands on the line
        QString name;
        bool placed = false;        ///< from/to are known (a plan to draw)
        QPointF from;               ///< plan: where it hangs (metres, x / z)
        QPointF to;                 ///< plan: its point on the line
        /** A raw position (positions()): where the beam lands and what it
         *  hits ("\u2248 Line 1", "off stage"). */
        BeamLanding::Kind landing = BeamLanding::NoGeometry;
        QString hits;
    };

    /** @p look's lights as @p scene plays it. No scene (or no line): six
     *  stand-ins, numbered, grouped and spread as the look says. */
    static LineFormation build(Doc *doc, const Scene *scene, const QLCPalette *look);

    /** A RAW position -- stored pan / tilt, not an aim: where each head's
     *  beam lands for @p palettes (later ones win per channel), solved
     *  forward from its DMX (BeamLanding::land, the rig view's math).
     *  @p groups are the heads by group: one colour each when there are
     *  several. Heads with no rig geometry or no pan / tilt are left out. */
    static LineFormation positions(Doc *doc, const QList<QList<quint32>> &groups,
                                   const QList<const QLCPalette *> &palettes);
    /** A Show Tree layer target's heads that can point, by group: each of
     *  its groups (a set as its groups) in order, then its own fixtures. */
    static QList<QList<quint32>> targetHeads(Doc *doc, ShowTreeController *st, const ShowTree::Target &target);
    /** Built by positions(). */
    bool isRawPosition() const { return m_raw; }
    /** positions(): one line for the lot ("lands DS centre, 2 heads off stage"). */
    QString landingSummary() const { return m_landingSummary; }

    const QList<Light> &lights() const { return m_lights; }
    const LineSpread::Settings &settings() const { return m_settings; }
    int groupCount() const { return m_groupCount; }
    /** More than one light moves together: worth colouring by group. */
    bool grouped() const;
    /** How many pairs of lights land in the other order to the one they
     *  hang in: 0 for no crossing, more when beams cross. */
    int crossings() const;
    /** Cross is a choice here but Spread is not below 0, so nothing crosses
     *  (yet). */
    bool crossIdle() const;
    /** One line saying what the colours and lines show ("Pairs: 4 groups.
     *  Groups swap sides: 12 beam crossings."). */
    QString caption() const;

    /** The colour of group @p group: blue, orange, aqua, violet, red, green,
     *  magenta, then round again (each neighbour pair, the last with the
     *  first, passes the colour-blind check on the dark plan surface). */
    static QColor groupColour(int group);
    /** The ink for a number on groupColour(). */
    static QColor groupInk() { return QColor(255, 255, 255); }
    /** A filled arrowhead for a line running from @p tail to @p tip: a
     *  triangle with its point at @p tip, @p size long. Empty when the two
     *  are closer than @p size (no direction to show). */
    static QPolygonF arrowhead(const QPointF &tail, const QPointF &tip, double size);
    /** Where a line to @p tip, drawn with an arrowhead of @p size, should
     *  end so the line does not poke out of the arrowhead's point. */
    static QPointF arrowBase(const QPointF &tail, const QPointF &tip, double size);
    /** Point fill and ink when lights are not grouped. */
    static QColor pointColour() { return QColor(255, 210, 120); }
    static QColor pointInk() { return QColor(20, 20, 24); }
    /** The colour for @p light: its group's when grouped(), else the point colour. */
    QColor colourFor(const Light &light) const;

private:
    QList<Light> m_lights;
    LineSpread::Settings m_settings;
    int m_groupCount = 0;
    bool m_raw = false;
    QString m_landingSummary;
};

/** @} */

#endif
