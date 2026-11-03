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
#include <QPointer>
#include <QRectF>

#include "lineformation.h"
#include "areacover.h"

class Doc;
class Scene;
class QLCPalette;
class QToolButton;
class LineSpreadPreview;
class QLabel;

/** @addtogroup ui UI
 * @{
 */

/**
 * The plan view of a line aim (docs/design/SHOW_TREE_DESIGN.md §3B, D19): the stage
 * seen from above -- the line target, each light where it hangs, its beam
 * to the point it takes on the line (numbered as the order strip numbers
 * them), and, as rings, where the previous cue put the same lights, so a
 * Spread / Grouping / Cross change shows what the beams will sweep across
 * over the cue's fade. The geometry is the aim solver's own
 * (AimSolver::linePoints over the scene's heads, MonitorProperties'
 * targetLinePoints), the same the rig plays -- through LineFormation, the
 * order strip's model too.
 *
 * Grouping and Cross show as colour and lines: each light (its square, its
 * beam, its point) wears its group's colour when lights move together, and
 * since every beam is drawn from where the light hangs to its point, groups
 * swapping sides -- or flipping -- are seen as beams crossing. A caption
 * along the bottom says what the colours and lines show. Names, when the
 * view is big enough for them, are laid out so they never overlap: a name
 * that would sit on another drops a row (with a leader to its light), and
 * one with no room is left to the tooltip.
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

    /** A STORED position instead (a PanTilt palette, any non-Aim position):
     *  each head of @p groups where it hangs, its beam to where it lands for
     *  @p palettes (solved forward from the DMX), the stage's outline and the
     *  stage targets for reference. The caption names what it hits. */
    void setPositions(const QList<QList<quint32>> &groups, const QList<const QLCPalette *> &palettes);

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
        int group = 0;              ///< 0 = the first group along the line
        QColor colour;              ///< its group's colour (or the plain point colour)
        bool lands = true;          ///< a stored position: on the stage (else off it / in the air)
        QString hits;               ///< a stored position: what it hits
    };
    /** What it shows (tests). */
    const QList<Beam> &beams() const { return m_beams; }
    const QList<QPointF> &line() const { return m_line; }
    /** The model it draws (tests). */
    const LineFormation &formation() const { return m_form; }
    /** Where the names were drawn at the last paint, one per named light
     *  shown (tests). */
    const QList<QRectF> &labelRects() const { return m_labelRects; }
    /** The points drawn at the last paint: how many lights each stands for
     *  (more than 1: points that landed together, drawn once "xN"; tests). */
    const QList<int> &pointMarks() const { return m_pointMarks; }
    /** The arrowheads drawn at the last paint, one per beam long enough to
     *  show a direction: each a triangle whose point is at the edge of the
     *  light's landing dot (tests). */
    const QList<QPolygonF> &arrows() const { return m_arrows; }
    /** The caption under the plan (what the colours and lines show; for
     *  an area, how much of it the beams cover and where the gap is). */
    QString caption() const { return isArea() ? m_cover.caption() : m_form.caption(); }

    /** An Aim at an Area it covers (AreaCover, docs/design/AREA_COVER.md):
     *  the area, each light's zone and its beam's footprint at subject
     *  height, the gaps hatched. */
    bool isArea() const { return m_cover.valid(); }
    const AreaCover::Coverage &coverage() const { return m_cover; }
    /** The hatched gap rectangles drawn at the last paint (tests). */
    int gapsDrawn() const { return m_gapsDrawn; }

    /** Drawn dimmed: what it shows is the last plan, not the selection's. */
    void setDimmed(bool dimmed);
    bool isDimmed() const { return m_dimmed; }
    /** Whether the view offers to open in a window (the corner button, right-click). */
    void setPoppable(bool poppable);
    /** Dot and label size at this widget size (1 at the embedded size; tests). */
    double drawScale() const;

signals:
    void popOutRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void setArea(const Scene *scene, const QLCPalette *look, const class StageTarget *target);

    Doc *m_doc;
    QList<QPointF> m_line;
    QList<Beam> m_beams;
    LineFormation m_form;
    QList<QRectF> m_labelRects;
    QList<int> m_pointMarks;
    QList<QPolygonF> m_arrows;
    AreaCover::Coverage m_cover;                //!< an area aim: its zones, beams and gaps
    int m_gapsDrawn = 0;
    QRectF m_stage;                             //!< stored position: the stage outline
    QList<QPair<QPointF, QString>> m_marks;     //!< stored position: point targets
    QList<QList<QPointF>> m_otherLines;         //!< stored position: line targets
    bool m_dimmed = false;
    bool m_poppable = true;
    QToolButton *m_popOut = nullptr;
};

/**
 * The order strip and the plan in a window of their own (the pop-out of a
 * line aim's Position rows): resizable, non-modal, one at a time, scaled to
 * the window. It is fed by the Show Tree's inspector each time it rebuilds
 * the layer page, so it follows every edit and the selection; with no line
 * aim selected it says so and keeps the last plan, dimmed. Its size and
 * place are remembered (QSettings showtree/linePlanWindow/geometry).
 */
class LinePlanWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit LinePlanWindow(Doc *doc, QWidget *parent = nullptr);
    ~LinePlanWindow() override;

    /** The same arguments the embedded views take. */
    void setFormation(const Scene *scene, const QLCPalette *look,
                      const Scene *prevScene, const QLCPalette *prevLook);
    /** A stored position (LinePlanView::setPositions()): the plan alone. */
    void setPositions(const QList<QList<quint32>> &groups, const QList<const QLCPalette *> &palettes);
    /** The selection has no line aim: keep the last plan, dimmed, and say so. */
    void setNoAim();

    LineSpreadPreview *strip() const { return m_strip; }
    LinePlanView *plan() const { return m_plan; }
    QString message() const;

    static QString geometryKey();

protected:
    void hideEvent(QHideEvent *) override;
    void closeEvent(QCloseEvent *) override;

private:
    void saveGeometryNow();

    LineSpreadPreview *m_strip;
    LinePlanView *m_plan;
    QLabel *m_note;
};

/** @} */

#endif
