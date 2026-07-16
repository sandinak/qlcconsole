/*
  Q Light Controller Plus
  sequenceitem.h

  Copyright (C) Massimo Callegari

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

#ifndef SEQUENCEITEM_H
#define SEQUENCEITEM_H

#include <QGraphicsItem>
#include <QObject>
#include <QAction>
#include <QFont>

#include "showitem.h"
#include "chaser.h"

/** @addtogroup ui_functions
 * @{
 */

/**
 *
 * Sequence Item. Clickable and draggable object identifying a chaser in sequence mode
 *
 */
class SequenceItem final : public ShowItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    SequenceItem(Chaser *seq, ShowFunction *func);

    /** @reimp */
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    /** @reimp */
    void setTimeScale(int val) override;

    /** @reimp */
    void setDuration(quint32 msec, bool stretch) override;

    /** @reimp */
    QString functionName() const override;

    void setSelectedStep(int idx);

    /** Return a pointer to a Chaser Function associated to this item */
    Chaser *getChaser() const;

protected:
    /** @reimp — keep the chase name in a compact top strip so it doesn't clash
     *  with the per-cue labels drawn in the body. */
    QRectF nameRect() const override;
    bool nameSingleLine() const override { return true; }

    /** @reimp */
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *) override;

    /** Drag a step boundary to retime individual cues. When the press is on an
     *  interior step divider we resize that step's hold; otherwise we defer to
     *  the base ShowItem (whole-item move / outer stretch). */
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

protected slots:
    void slotSequenceChanged(quint32);

private:
    /** Calculate sequence width for paint() and boundingRect() */
    void calculateWidth();

    /** The effective hold (duration) of step @p idx, honouring Common mode. */
    quint32 effectiveStepDuration(int idx) const;

    /** On-timeline display width (ms) of step @p idx: effectiveStepDuration(),
     *  but a nominal value for manual-GO (infinite) / un-timed (0) steps so each
     *  cue stays visible + draggable. Used by paint/calculateWidth/boundaries. */
    quint32 stepDisplayMs(int idx) const;

    /** Index of the step whose RIGHT divider sits within a few px of localX, or
     *  -1. Excludes the final divider (that's the item's stretch handle). */
    int stepBoundaryAt(qreal localX) const;

    /** Index of the cue (step) whose body contains localX, or -1. */
    int cueAt(qreal localX) const;

    /** The cue's NAME: the fired function/scene name, else "Cue N". */
    QString cueLabel(int idx) const;

    /** Rich-text tooltip for cue @p idx: chase name, cue name, timing, and the
     *  step note (description) at the bottom if present. */
    QString cueTooltip(int idx) const;

    /** Ensure per-step durations are the source of truth (switch a Common-mode
     *  chaser to PerStep, stamping each step with its current effective hold),
     *  so retiming one cue doesn't move the others. */
    void ensurePerStepDurations();

    /** Snapshot every step's current display duration (px-stable drag math). */
    void snapshotDurations();
    /** Write @p ms into step @p idx's duration (clamped to a 0.1 s minimum). */
    void setStepDuration(int idx, qint64 ms);

private:
    /** Reference to the actual Chaser Function which holds the sequence steps */
    Chaser *m_chaser;

    /** index of the selected step for highlighting (-1 if none) */
    int m_selectedStep;

    /** Ensure per-step fade in/out are the source of truth (switch Common/Default
     *  fade modes to PerStep, seeding each step) so a dragged fade sticks. */
    void ensurePerStepFades();

    /** Geometry (scene-x) of cue @p idx: its start and end. */
    void cueBounds(int idx, float &startX, float &endX) const;

    /** True while the pointer is over this item — shows the grab-hint arrows. */
    bool m_hovered = false;

    /** In-block cue drag state. */
    enum CueDrag { CueNone = 0, CueRoll, CueSlip, CueFadeIn, CueFadeOut };
    CueDrag m_cueDrag;           // active in-block edit (else defer to ShowItem)
    int m_cueDragIdx;            // roll: left cue index; slip/fade: cue index
    qreal m_cueDragPressX;       // scene-x at press
    quint32 m_cueDragOrigFade;   // the dragged fade's value at press (relative drag)
    QList<quint32> m_cueOrigDur; // per-step durations captured at press
};

/** @} */

#endif
