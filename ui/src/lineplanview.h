/*
  Q Light Controller Plus
  lineplanview.h

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

#ifndef LINEPLANVIEW_H
#define LINEPLANVIEW_H

#include <QWidget>
#include <QPointF>
#include <QList>
#include <QHash>

class Doc;
class Scene;
class QLCPalette;

/** @addtogroup ui UI
 * @{
 */

/**
 * The plan view of a line aim (SHOW_TREE_DESIGN.md §3B, D19): the stage
 * seen from above -- the line target, each light where it hangs, its beam
 * to the point it takes on the line (numbered as the order strip numbers
 * them), and, as rings, where the previous cue put the same lights, so a
 * Spread / Grouping / Cross change shows what the beams will sweep across
 * over the cue's fade. The geometry is the aim solver's own
 * (AimSolver::linePoints over the scene's heads, MonitorProperties'
 * targetLinePoints), the same the rig plays.
 */
class LinePlanView final : public QWidget
{
    Q_OBJECT

public:
    explicit LinePlanView(Doc *doc, QWidget *parent = nullptr);

    QSize sizeHint() const override { return QSize(260, 170); }

    /** The formation now -- Aim look @p look as @p scene plays it -- and
     *  the previous cue's for the same lights (@p prevScene / @p prevLook,
     *  null: none). */
    void setFormation(const Scene *scene, const QLCPalette *look,
                      const Scene *prevScene = nullptr, const QLCPalette *prevLook = nullptr);

    /** One light, in plan coordinates (metres: x across, z up/downstage). */
    struct Beam
    {
        quint32 fixture = 0;
        int number = 0;             ///< 1 = the light nearest the line's start
        QString name;
        QPointF from;               ///< where it hangs
        QPointF to;                 ///< its point on the line now
        bool hasPrevious = false;
        QPointF previous;           ///< its point in the previous cue
    };
    /** What it shows (tests). */
    const QList<Beam> &beams() const { return m_beams; }
    const QList<QPointF> &line() const { return m_line; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Doc *m_doc;
    QList<QPointF> m_line;
    QList<Beam> m_beams;
};

/** @} */

#endif
