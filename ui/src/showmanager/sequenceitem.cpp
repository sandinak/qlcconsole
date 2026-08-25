/*
  Q Light Controller Plus
  sequenceitem.cpp

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

#include <QApplication>
#include <QPainter>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QToolTip>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsView>

#include "sequenceitem.h"
#include "multitrackview.h"
#include "chaserstep.h"
#include "trackitem.h"
#include "function.h"
#include "doc.h"

/** Format a millisecond span compactly for the drag readout. */
static QString seqMsToText(quint32 ms)
{
    if (ms < 10000)
        return QString("%1 s").arg(ms / 1000.0, 0, 'f', 1);
    uint totalSec = ms / 1000;
    return QString("%1:%2").arg(totalSec / 60)
            .arg(totalSec % 60, 2, 10, QChar('0'));
}

/** Nominal on-timeline width (ms) for a manual-GO (infinite) or un-timed (0)
 *  step, so every cue stays visible and draggable. */
#define SEQ_NOMINAL_STEP_MS 3000

/** Smallest hold (ms) a cue can be dragged to. */
#define SEQ_MIN_STEP_MS 100

/** Height (px) of the top strip that carries the chase name. */
#define SEQ_TITLE_STRIP_H 15

/** Top of the fade band: fades live in the BOTTOM part of a cue so their grab
 *  points sit well clear of the top move-strip and the middle timing band. */
#define SEQ_FADE_BAND_TOP (TRACK_HEIGHT * 0.60)

/** Y (px) at which the draggable fade handle nubs are drawn — near the BOTTOM of
 *  the cue (bottom of the fade wedge's vertical edge), so there's clear vertical
 *  space between the top move-strip and the fade grab points. */
#define SEQ_HANDLE_Y (TRACK_HEIGHT - 9)

/** Draw a small horizontal ‹—› double-arrow centred at (cx, cy), half-width w —
 *  a grab-hint for a horizontal drag. */
static void seqDrawMoveArrows(QPainter *p, qreal cx, qreal cy, qreal w,
                              const QColor &col)
{
    p->save();
    p->setBrush(col);
    p->setPen(Qt::NoPen);
    QPolygonF l; l << QPointF(cx - w, cy) << QPointF(cx - w + 4, cy - 3)
                   << QPointF(cx - w + 4, cy + 3);
    QPolygonF r; r << QPointF(cx + w, cy) << QPointF(cx + w - 4, cy - 3)
                   << QPointF(cx + w - 4, cy + 3);
    p->drawPolygon(l);
    p->drawPolygon(r);
    p->setPen(QPen(col, 1));
    p->drawLine(QPointF(cx - w + 3, cy), QPointF(cx + w - 3, cy));
    p->restore();
}

SequenceItem::SequenceItem(Chaser *seq, ShowFunction *func)
    : ShowItem(func)
    , m_chaser(seq)
    , m_selectedStep(-1)
    , m_cueDrag(CueNone)
    , m_cueDragIdx(-1)
    , m_cueDragPressX(0)
    , m_cueDragOrigFade(0)
{
    Q_ASSERT(seq != NULL);

    if (func->color().isValid())
        setColor(func->color());
    else
        setColor(ShowFunction::defaultColor(Function::ChaserType));

    if (func->duration() == 0)
        func->setDuration(seq->totalDuration());

    calculateWidth();

    connect(m_chaser, SIGNAL(changed(quint32)),
            this, SLOT(slotSequenceChanged(quint32)));
    // Function::setName emits nameChanged (not changed), so repaint the title
    // strip when the chase is renamed elsewhere.
    connect(m_chaser, SIGNAL(nameChanged(quint32)),
            this, SLOT(slotSequenceChanged(quint32)));
    setIconResource(":/sequence.png");
}

quint32 SequenceItem::stepDisplayMs(int idx) const
{
    // The width a step OCCUPIES on the timeline. A manual-GO step (infinite hold)
    // or a not-yet-timed step (0) would otherwise blow the block out to a huge
    // width (the old code clamped infinite to 10,000,000 ms, so a single step
    // filled the whole view and the others were invisible). Give those a nominal
    // visible width so every cue reads as its own draggable sub-block.
    quint32 dur = effectiveStepDuration(idx);
    if (dur == Function::infiniteSpeed() || dur == 0)
        return SEQ_NOMINAL_STEP_MS;
    return dur;
}

void SequenceItem::calculateWidth()
{
    float timeUnit = 50.0 / float(getTimeScale());
    // Width = sum of the per-step DISPLAY durations, so the block always exactly
    // spans its sub-blocks (paint uses the same per-step widths).
    double totalMs = 0;
    const int count = m_chaser->stepsCount();
    for (int i = 0; i < count; i++)
        totalMs += stepDisplayMs(i);

    int newWidth = (timeUnit * float(totalMs)) / 1000.0;
    if (newWidth < timeUnit)
        newWidth = timeUnit;
    setWidth(newWidth);
}

void SequenceItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    float xpos = 0;
    float timeUnit = 50.0 / float(m_timeScale);
    int stepIdx = 0;

    ShowItem::paint(painter, option, widget);

    if (this->isSelected() == false)
    {
        m_selectedStep = -1;
        m_selectedSteps.clear();
    }

    foreach (ChaserStep step, m_chaser->steps())
    {
        uint stepFadeIn = step.fadeIn;
        uint stepFadeOut = step.fadeOut;

        if (m_chaser->fadeInMode() == Chaser::Common)
            stepFadeIn = m_chaser->fadeInSpeed();
        if (m_chaser->fadeOutMode() == Chaser::Common)
            stepFadeOut = m_chaser->fadeOutSpeed();

        // Display width per step (manual-GO / un-timed steps get a nominal width
        // so they stay visible — see stepDisplayMs()). Matches stepBoundaryAt()
        // and calculateWidth() so dividers line up with the drag hit-zones.
        quint32 stepDuration = stepDisplayMs(stepIdx);
        float stepWidth = ((timeUnit * (float)stepDuration) / 1000);

        const float bot = TRACK_HEIGHT - 3;
        // Fades live BELOW the title/move strip so they never intrude on the
        // block-move handle (object handles = top bar; fades stay under it).
        const float fadeTop = SEQ_TITLE_STRIP_H + 1;
        const float xEnd = xpos + stepWidth;

        // Shade each step (alternating) so the steps read as distinct aligned
        // blocks within the chaser rather than one flat bar.
        if (stepWidth > 1.0f)
        {
            QColor shade = (stepIdx % 2 == 0) ? m_color.lighter(118) : m_color.darker(112);
            painter->fillRect(QRectF(xpos, 0, stepWidth, TRACK_HEIGHT - 3), shade);
        }

        // Fades as filled wedges (the "beam up/down" ramp) so timing reads at a
        // glance: fade-in rises from the cue start, fade-out falls to the end.
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 55));
        {
            // Fade-in wedge + a draggable handle nub at its apex (drawn even at
            // fade 0, a bit inside the cue, so a fade can be pulled out).
            float fw = qMin(stepWidth, (timeUnit * float(stepFadeIn)) / 1000.0f);
            if (stepFadeIn > 0 && fw > 1.0f)
            {
                QPolygonF wedge;
                wedge << QPointF(xpos, bot) << QPointF(xpos + fw, fadeTop)
                      << QPointF(xpos + fw, bot);
                painter->drawPolygon(wedge);
            }
            // Handle nub near the BOTTOM of the cue; on hover it grows a ‹—›
            // arrow hint showing this is a horizontal fade-drag grab point.
            if (stepWidth > 14)
            {
                float hx = xpos + qMax(fw, 6.0f);
                painter->setBrush(QColor(120, 200, 255, 235));
                painter->drawEllipse(QPointF(hx, SEQ_HANDLE_Y), 3.5, 3.5);
                if (m_hovered)
                    seqDrawMoveArrows(painter, hx, SEQ_HANDLE_Y, 8.0,
                                      QColor(150, 215, 255, 235));
                painter->setBrush(QColor(255, 255, 255, 55));
            }
        }
        {
            float fw = qMin(stepWidth, (timeUnit * float(stepFadeOut)) / 1000.0f);
            if (stepFadeOut > 0 && fw > 1.0f)
            {
                QPolygonF wedge;
                wedge << QPointF(xEnd - fw, bot) << QPointF(xEnd - fw, fadeTop)
                      << QPointF(xEnd, bot);
                painter->drawPolygon(wedge);
            }
            if (stepWidth > 14)
            {
                float hx = xEnd - qMax(fw, 6.0f);
                painter->setBrush(QColor(120, 200, 255, 235));
                painter->drawEllipse(QPointF(hx, SEQ_HANDLE_Y), 3.5, 3.5);
                if (m_hovered)
                    seqDrawMoveArrows(painter, hx, SEQ_HANDLE_Y, 8.0,
                                      QColor(150, 215, 255, 235));
                painter->setBrush(QColor(255, 255, 255, 55));
            }
        }

        // draw selected step(s): a translucent wash over every cue in the
        // multi-selection plus a yellow outline; the anchor cue gets a brighter
        // outline so it's clear which cue Shift extends from.
        if (m_selectedSteps.contains(stepIdx) || stepIdx == m_selectedStep)
        {
            painter->setBrush(QColor(255, 235, 90, 60));
            painter->setPen(QPen(stepIdx == m_selectedStep ? Qt::yellow
                                                           : QColor(230, 200, 60), 2));
            painter->drawRect(xpos, 0, stepWidth, TRACK_HEIGHT - 3);
            painter->setBrush(Qt::NoBrush);
        }

        // Cue label, drawn BELOW the title strip so it doesn't clash with the
        // chase name: the step note (the user's cue description) when set, else
        // the fired scene's name, else "Cue N".
        const QString blockLabel = step.note.isEmpty() ? cueLabel(stepIdx) : step.note;
        painter->setPen(QPen(Qt::white, 1));
        // Keep the label in the timing band (above the fade band) so it never
        // sits over the fade grab points at the bottom.
        QRect textRect = QRect(xpos + 3, SEQ_TITLE_STRIP_H + 1,
                               stepWidth - 4, int(SEQ_FADE_BAND_TOP) - SEQ_TITLE_STRIP_H - 1);
        painter->drawText(textRect, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap,
                          blockLabel);

        xpos = xEnd;

        // draw step vertical delimiter
        painter->setPen(QPen(Qt::white, 1));
        painter->drawLine(xpos, 1, xpos, TRACK_HEIGHT - 5);

        stepIdx++;
    }

    // Title strip across the top so the chase name (drawn next by postPaint)
    // sits on its own band, clear of the cue labels below it.
    painter->fillRect(QRectF(0, 0, m_width, SEQ_TITLE_STRIP_H), QColor(0, 0, 0, 110));

    // On hover, a ‹—› arrow hint in the strip advertises the whole-block move
    // grab (object handle = top bar). Drawn at the right so it clears the name.
    if (m_hovered && m_locked == false && m_width > 60)
        seqDrawMoveArrows(painter, m_width - 34, SEQ_TITLE_STRIP_H / 2.0, 9.0,
                          QColor(230, 230, 230, 230));

    ShowItem::postPaint(painter);
}

QRectF SequenceItem::nameRect() const
{
    // Compact top strip; leave room for the type badge in the top-right corner.
    return QRectF(4, 0, qMax(10, m_width - 24), SEQ_TITLE_STRIP_H);
}

void SequenceItem::setTimeScale(int val)
{
    ShowItem::setTimeScale(val);
    calculateWidth();
}

void SequenceItem::setDuration(quint32 msec, bool stretch)
{
    Q_UNUSED(stretch)
    // Stretching the whole block (outer right edge) extends/shrinks ONLY the
    // LAST cue — every other cue keeps the timing you set. (The old behaviour
    // scaled all steps proportionally, which lost per-cue timings.)
    const int count = m_chaser->stepsCount();
    if (count == 0)
        return;

    ensurePerStepDurations();

    quint32 total = 0;
    for (int i = 0; i < count; i++)
        total += stepDisplayMs(i);

    qint64 delta = qint64(msec) - qint64(total);
    setStepDuration(count - 1, qint64(stepDisplayMs(count - 1)) + delta);

    prepareGeometryChange();
    calculateWidth();
    if (m_function)
        m_function->setDuration(m_chaser->totalDuration());
    updateTooltip();
}

QString SequenceItem::functionName() const
{
    if (m_chaser)
        return m_chaser->name();
    return QString();
}

void SequenceItem::setSelectedStep(int idx)
{
    m_selectedStep = idx;
    update();
}

Chaser *SequenceItem::getChaser() const
{
    return m_chaser;
}

void SequenceItem::slotSequenceChanged(quint32)
{
    prepareGeometryChange();
    calculateWidth();
    if (m_function)
        m_function->setDuration(m_chaser->totalDuration());
    updateTooltip();
}

quint32 SequenceItem::effectiveStepDuration(int idx) const
{
    if (idx < 0 || idx >= m_chaser->stepsCount())
        return 0;
    if (m_chaser->durationMode() == Chaser::Common)
        return m_chaser->duration();
    return m_chaser->steps().at(idx).duration;
}

int SequenceItem::stepBoundaryAt(qreal localX) const
{
    const float timeUnit = 50.0f / float(m_timeScale);
    const int count = m_chaser->stepsCount();
    float xpos = 0;
    for (int i = 0; i < count; i++)
    {
        xpos += (timeUnit * float(stepDisplayMs(i))) / 1000.0f;
        // The final divider coincides with the item's right stretch handle —
        // leave that to ShowItem so whole-item resize still works.
        if (i < count - 1 && qAbs(localX - xpos) <= 4.0)
            return i;
    }
    return -1;
}

int SequenceItem::cueAt(qreal localX) const
{
    const float timeUnit = 50.0f / float(m_timeScale);
    const int count = m_chaser->stepsCount();
    float xpos = 0;
    for (int i = 0; i < count; i++)
    {
        float w = (timeUnit * float(stepDisplayMs(i))) / 1000.0f;
        if (localX >= xpos && localX < xpos + w)
            return i;
        xpos += w;
    }
    return -1;
}

QString SequenceItem::cueLabel(int idx) const
{
    // The cue's NAME = the scene/function it fires (its note is a separate
    // "description", shown only in the tooltip).
    if (idx < 0 || idx >= m_chaser->stepsCount())
        return QString();
    const ChaserStep s = m_chaser->steps().at(idx);
    Function *f = (m_chaser->doc() != NULL) ? m_chaser->doc()->function(s.fid) : NULL;
    if (f != NULL)
        return f->name();
    return tr("Cue %1").arg(idx + 1);
}

QString SequenceItem::cueTooltip(int idx) const
{
    QString head = QString("<b>%1</b>").arg(m_chaser->name().toHtmlEscaped());
    if (idx < 0 || idx >= m_chaser->stepsCount())
        return head;

    const ChaserStep s = m_chaser->steps().at(idx);
    uint fi = (m_chaser->fadeInMode() == Chaser::Common) ? m_chaser->fadeInSpeed() : s.fadeIn;
    uint fo = (m_chaser->fadeOutMode() == Chaser::Common) ? m_chaser->fadeOutSpeed() : s.fadeOut;
    quint32 dur = effectiveStepDuration(idx);

    quint32 start = getStartTime();
    for (int k = 0; k < idx; k++)
        start += stepDisplayMs(k);

    QString t = head;
    t += QString("<br>%1").arg(tr("Cue %1: %2").arg(idx + 1)
                               .arg(cueLabel(idx).toHtmlEscaped()));
    t += QString("<br>%1").arg(tr("fade in %1 · hold %2 · fade out %3 · starts %4")
            .arg(seqMsToText(fi)).arg(seqMsToText(dur))
            .arg(seqMsToText(fo)).arg(seqMsToText(start)));
    // Description (the step note), at the bottom if present.
    if (s.note.isEmpty() == false)
        t += QString("<br><i>%1</i>").arg(s.note.toHtmlEscaped());
    return t;
}

void SequenceItem::ensurePerStepDurations()
{
    if (m_chaser->durationMode() == Chaser::PerStep)
        return;
    // Stamp each step with its current effective hold, then switch mode so
    // future edits are per-step.
    const quint32 common = m_chaser->duration();
    for (int i = 0; i < m_chaser->stepsCount(); i++)
    {
        ChaserStep s = m_chaser->steps().at(i);
        s.duration = common;
        m_chaser->replaceStep(s, i);
    }
    m_chaser->setDurationMode(Chaser::PerStep);
}

void SequenceItem::ensurePerStepFades()
{
    if (m_chaser->fadeInMode() != Chaser::PerStep)
    {
        const quint32 common = m_chaser->fadeInSpeed();
        for (int i = 0; i < m_chaser->stepsCount(); i++)
        {
            ChaserStep s = m_chaser->steps().at(i);
            if (m_chaser->fadeInMode() == Chaser::Common)
                s.fadeIn = common;
            m_chaser->replaceStep(s, i);
        }
        m_chaser->setFadeInMode(Chaser::PerStep);
    }
    if (m_chaser->fadeOutMode() != Chaser::PerStep)
    {
        const quint32 common = m_chaser->fadeOutSpeed();
        for (int i = 0; i < m_chaser->stepsCount(); i++)
        {
            ChaserStep s = m_chaser->steps().at(i);
            if (m_chaser->fadeOutMode() == Chaser::Common)
                s.fadeOut = common;
            m_chaser->replaceStep(s, i);
        }
        m_chaser->setFadeOutMode(Chaser::PerStep);
    }
}

void SequenceItem::cueBounds(int idx, float &startX, float &endX) const
{
    const float timeUnit = 50.0f / float(m_timeScale);
    float xpos = 0;
    for (int i = 0; i < m_chaser->stepsCount(); i++)
    {
        float w = (timeUnit * float(stepDisplayMs(i))) / 1000.0f;
        if (i == idx) { startX = xpos; endX = xpos + w; return; }
        xpos += w;
    }
    startX = endX = 0;
}

void SequenceItem::snapshotDurations()
{
    m_cueOrigDur.clear();
    for (int i = 0; i < m_chaser->stepsCount(); i++)
        m_cueOrigDur.append(stepDisplayMs(i));
}

void SequenceItem::setStepDuration(int idx, qint64 ms)
{
    if (idx < 0 || idx >= m_chaser->stepsCount())
        return;
    if (ms < SEQ_MIN_STEP_MS)
        ms = SEQ_MIN_STEP_MS;
    ChaserStep s = m_chaser->steps().at(idx);
    if (s.duration == quint32(ms))
        return;
    s.duration = quint32(ms);
    // Keep fade-in within the new hold so the wedge never exceeds the cue.
    if (s.fadeIn > s.duration)
        s.fadeIn = s.duration;
    m_chaser->replaceStep(s, idx);
}

// Bands (top→bottom within a cue): TITLE strip = whole-block move; TIMING band
// (middle) = roll dividers / slip cues; FADE band (bottom) = per-cue fades. The
// fade grab is at the bottom so it's well clear of the top move-strip.

void SequenceItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    m_hovered = true;
    update();
    ShowItem::hoverEnterEvent(event);
}

void SequenceItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_hovered = false;
    update();
    ShowItem::hoverLeaveEvent(event);
}

void SequenceItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    const qreal x = event->pos().x();
    const qreal y = event->pos().y();

    // Cue-aware tooltip: chase name + the hovered cue's name, timing and (if set)
    // its description.
    int cue = cueAt(x);
    if (cue >= 0)
        setToolTip(cueTooltip(cue));

    if (m_editable && y < SEQ_TITLE_STRIP_H && m_locked == false)
        setCursor(Qt::OpenHandCursor);                   // title strip = move block
    else if (m_editable && y >= SEQ_FADE_BAND_TOP && cue >= 0)
        setCursor(Qt::SizeHorCursor);                    // fade band (bottom)
    else if (m_editable && edgeAt(x) == NoEdge && stepBoundaryAt(x) >= 0)
        setCursor(Qt::SplitHCursor);                     // divider = roll
    else if (m_editable && edgeAt(x) == NoEdge && cue >= 0)
        setCursor(Qt::SizeHorCursor);                    // cue body = slip
    else
        ShowItem::hoverMoveEvent(event);
}

void SequenceItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    const bool alt = event->modifiers() & Qt::AltModifier;
    const qreal x = event->pos().x();
    const qreal y = event->pos().y();
    const bool inFadeBand = (y >= SEQ_FADE_BAND_TOP);

    // 1. Whole-block move: Alt-drag anywhere, or drag the top title strip.
    //    Disabled when position-locked (cue/fade editing below still works).
    if (m_locked == false && event->button() == Qt::LeftButton
        && (alt || y < SEQ_TITLE_STRIP_H))
    {
        ShowItem::mousePressEvent(event);
        return;
    }

    // 2. Outer stretch handle (right edge = extend last cue) — but NOT in the
    //    fade band, so a first/last cue's fade handle near the block edge is not
    //    stolen by the resize grab. Grab the edge lower down (timing band).
    if (m_locked == false && inFadeBand == false && edgeAt(x) != NoEdge)
    {
        ShowItem::mousePressEvent(event);
        return;
    }

    // 3. In-block cue editing — allowed even when position-locked, if the
    //    track/master gate (m_editable) permits.
    if (m_editable && event->button() == Qt::LeftButton)
    {
        int cue = cueAt(x);

        // Multi-select: a Shift / Ctrl / Cmd click on a cue changes the SELECTION
        // (build a group to delete / move / scale) and does NOT start a drag.
        const Qt::KeyboardModifiers selMods =
            event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::MetaModifier);
        if (selMods && cue >= 0)
        {
            selectCue(cue, event->modifiers());
            setSelected(true);
            update();
            event->accept();
            return;
        }

        // Fade band: grab the fade-in or fade-out — whichever handle the press is
        // CLOSER to (so both are reachable). The drag is RELATIVE to the fade's
        // value at press, so it never jumps to the cursor.
        if (inFadeBand && cue >= 0)
        {
            ensurePerStepDurations();
            ensurePerStepFades();
            const float timeUnit = 50.0f / float(m_timeScale);
            float sx, ex;
            cueBounds(cue, sx, ex);
            const ChaserStep s = m_chaser->steps().at(cue);
            const float fwIn  = qMax((timeUnit * float(s.fadeIn))  / 1000.0f, 6.0f);
            const float fwOut = qMax((timeUnit * float(s.fadeOut)) / 1000.0f, 6.0f);
            const float hxIn  = sx + fwIn;      // fade-in handle x
            const float hxOut = ex - fwOut;     // fade-out handle x
            const bool isIn = qAbs(x - hxIn) <= qAbs(x - hxOut);
            m_cueDrag = isIn ? CueFadeIn : CueFadeOut;
            m_cueDragOrigFade = isIn ? s.fadeIn : s.fadeOut;
            m_cueDragIdx = cue;
            m_cueDragPressX = event->scenePos().x();
            selectCue(cue, Qt::NoModifier);
            setFlag(QGraphicsItem::ItemIsMovable, false);
            setSelected(true);
            update();
            event->accept();
            return;
        }

        // Timing band: divider = roll, cue body = slip (interior cues only).
        if (edgeAt(x) == NoEdge)
        {
            const int bodyCue = cue;
            int div = stepBoundaryAt(x);

            // Group move: a plain drag on a cue that's part of a contiguous
            // interior multi-selection slides the whole group as a unit (the
            // cues either side absorb the slide; the group keeps its timing).
            int gLo, gHi;
            if (div < 0 && bodyCue >= 0 && m_selectedSteps.count() > 1
                && isCueSelected(bodyCue) && selectionContiguousInterior(gLo, gHi))
            {
                ensurePerStepDurations();
                snapshotDurations();
                m_cueDrag = CueGroupMove;
                m_groupLo = gLo;
                m_groupHi = gHi;
                m_cueDragPressX = event->scenePos().x();
                setFlag(QGraphicsItem::ItemIsMovable, false);
                setSelected(true);
                update();
                event->accept();
                return;
            }

            cue = (div < 0) ? cue : -1;
            if (div >= 0 || (cue > 0 && cue < m_chaser->stepsCount() - 1))
            {
                ensurePerStepDurations();
                snapshotDurations();
                m_cueDrag = (div >= 0) ? CueRoll : CueSlip;
                m_cueDragIdx = (div >= 0) ? div : cue;
                m_cueDragPressX = event->scenePos().x();
                selectCue(m_cueDragIdx, Qt::NoModifier);
                setFlag(QGraphicsItem::ItemIsMovable, false);
                setSelected(true);
                update();
                event->accept();
                return;
            }
            // A plain click on a cue body that isn't a drag target (first/last
            // cue) still selects that single cue.
            if (bodyCue >= 0)
            {
                selectCue(bodyCue, Qt::NoModifier);
                setSelected(true);
                update();
                event->accept();
                return;
            }
        }
    }
    ShowItem::mousePressEvent(event);
}

void SequenceItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    // Fade drag: set the cue's fade-in / fade-out from the cursor position.
    if ((m_cueDrag == CueFadeIn || m_cueDrag == CueFadeOut)
        && m_cueDragIdx >= 0 && m_cueDragIdx < m_chaser->stepsCount())
    {
        const float timeUnit = 50.0f / float(m_timeScale);
        const int i = m_cueDragIdx;
        const qint64 dur = qint64(stepDisplayMs(i));
        // RELATIVE to the value at press (drag from where you grabbed — no jump).
        // Dragging RIGHT grows fade-in; dragging LEFT grows fade-out.
        const qint64 ddt = qint64(((event->scenePos().x() - m_cueDragPressX) / timeUnit) * 1000.0);
        ChaserStep s = m_chaser->steps().at(i);
        QString readout;
        if (m_cueDrag == CueFadeIn)
        {
            // Never overlap the fade-out: fadeIn + fadeOut <= cue duration.
            qint64 ms = qBound<qint64>(0, qint64(m_cueDragOrigFade) + ddt,
                                       dur - qint64(s.fadeOut));
            s.fadeIn = quint32(ms);
            readout = tr("%1 fade in %2").arg(cueLabel(i)).arg(seqMsToText(quint32(ms)));
        }
        else
        {
            qint64 ms = qBound<qint64>(0, qint64(m_cueDragOrigFade) - ddt,
                                       dur - qint64(s.fadeIn));
            s.fadeOut = quint32(ms);
            readout = tr("%1 fade out %2").arg(cueLabel(i)).arg(seqMsToText(quint32(ms)));
        }
        m_chaser->replaceStep(s, i);
        update();
        QToolTip::showText(event->screenPos(), readout);
        event->accept();
        return;
    }

    if (m_cueDrag != CueNone && m_cueOrigDur.count() == m_chaser->stepsCount())
    {
        const float timeUnit = 50.0f / float(m_timeScale);
        const qint64 ddt = qint64(((event->scenePos().x() - m_cueDragPressX) / timeUnit) * 1000.0);
        const int i = m_cueDragIdx;
        MultiTrackView *view = qobject_cast<MultiTrackView *>(
                    scene() != NULL ? scene()->views().value(0) : NULL);
        const quint32 itemStart = getStartTime();
        QString readout;

        if (m_cueDrag == CueRoll)
        {
            // Divider between cue i and i+1. Snap its ABSOLUTE timeline position
            // to grid/markers/playhead, then split the pair around it (their sum
            // and every downstream cue stay fixed).
            qint64 prefix = itemStart;
            for (int k = 0; k < i; k++) prefix += m_cueOrigDur.at(k);
            const qint64 sum = qint64(m_cueOrigDur.at(i)) + qint64(m_cueOrigDur.at(i + 1));
            qint64 dividerAbs = prefix + qint64(m_cueOrigDur.at(i)) + ddt;
            if (view != NULL)
                dividerAbs = view->snapTimeMs(quint32(qMax<qint64>(0, dividerAbs)));
            qint64 left = qBound<qint64>(SEQ_MIN_STEP_MS, dividerAbs - prefix, sum - SEQ_MIN_STEP_MS);
            setStepDuration(i, left);
            setStepDuration(i + 1, sum - left);
            readout = tr("%1: %2  |  %3: %4")
                    .arg(cueLabel(i)).arg(seqMsToText(quint32(left)))
                    .arg(cueLabel(i + 1)).arg(seqMsToText(quint32(sum - left)));
        }
        else if (m_cueDrag == CueGroupMove)
        {
            // Slide the whole selected run [a..b] (their durations unchanged): the
            // cue before (a-1) grows/shrinks and the cue after (b+1) absorbs it.
            const int a = m_groupLo, b = m_groupHi;
            if (a >= 1 && b + 1 < m_cueOrigDur.count())
            {
                qint64 prefix = itemStart;
                for (int k = 0; k < a - 1; k++) prefix += m_cueOrigDur.at(k);
                const qint64 giveSum = qint64(m_cueOrigDur.at(a - 1)) + qint64(m_cueOrigDur.at(b + 1));
                qint64 startAbs = prefix + qint64(m_cueOrigDur.at(a - 1)) + ddt;
                if (view != NULL)
                    startAbs = view->snapTimeMs(quint32(qMax<qint64>(0, startAbs)));
                qint64 prev = qBound<qint64>(SEQ_MIN_STEP_MS, startAbs - prefix, giveSum - SEQ_MIN_STEP_MS);
                setStepDuration(a - 1, prev);
                setStepDuration(b + 1, giveSum - prev);
                readout = tr("Moved %1 cues → starts %2")
                        .arg(b - a + 1).arg(seqMsToText(quint32(prefix + prev)));
            }
        }
        else // CueSlip: move cue i (keep its length); prev grows, next shrinks.
        {
            qint64 prefix = itemStart;
            for (int k = 0; k < i - 1; k++) prefix += m_cueOrigDur.at(k);
            const qint64 prevSum = qint64(m_cueOrigDur.at(i - 1)) + qint64(m_cueOrigDur.at(i + 1));
            qint64 startAbs = prefix + qint64(m_cueOrigDur.at(i - 1)) + ddt;
            if (view != NULL)
                startAbs = view->snapTimeMs(quint32(qMax<qint64>(0, startAbs)));
            qint64 prev = qBound<qint64>(SEQ_MIN_STEP_MS, startAbs - prefix, prevSum - SEQ_MIN_STEP_MS);
            setStepDuration(i - 1, prev);
            setStepDuration(i + 1, prevSum - prev);
            readout = tr("%1 starts %2  (hold %3)")
                    .arg(cueLabel(i)).arg(seqMsToText(quint32(prefix + prev)))
                    .arg(seqMsToText(m_cueOrigDur.at(i)));
        }

        prepareGeometryChange();
        calculateWidth();
        if (m_function)
            m_function->setDuration(m_chaser->totalDuration());
        update();
        // Live numeric readout at the cursor.
        QToolTip::showText(event->screenPos(), readout);
        event->accept();
        return;
    }
    ShowItem::mouseMoveEvent(event);
}

void SequenceItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_cueDrag != CueNone)
    {
        m_cueDrag = CueNone;
        m_cueDragIdx = -1;
        m_groupLo = m_groupHi = -1;
        m_cueOrigDur.clear();
        setFlag(QGraphicsItem::ItemIsMovable, m_locked == false && m_editable);
        updateTooltip();
        if (m_chaser->doc() != NULL)
            m_chaser->doc()->setModified();
        event->accept();
        return;
    }
    ShowItem::mouseReleaseEvent(event);
}

void SequenceItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;
    QFont menuFont = qApp->font();
    menuFont.setPixelSize(14);
    menu.setFont(menuFont);

    // Right-clicking a cue that isn't part of the current selection selects just
    // that cue, so the menu acts on what you clicked.
    if (m_editable && event != NULL)
    {
        int cue = cueAt(event->pos().x());
        if (cue >= 0 && isCueSelected(cue) == false)
        {
            selectCue(cue, Qt::NoModifier);
            setSelected(true);
            update();
        }
    }

    // Cue (multi-)selection actions.
    if (m_editable && m_selectedSteps.isEmpty() == false)
    {
        const int n = m_selectedSteps.count();
        QAction *del = menu.addAction(n > 1 ? tr("Delete %1 selected cues").arg(n)
                                            : tr("Delete cue"));
        connect(del, &QAction::triggered, this, [this]() { deleteSelectedCues(); });

        QAction *scale = menu.addAction(n > 1 ? tr("Scale %1 cues' timing…").arg(n)
                                              : tr("Scale cue timing…"));
        connect(scale, &QAction::triggered, this, [this]() {
            bool ok = false;
            double pct = QInputDialog::getDouble(
                (scene() && !scene()->views().isEmpty()) ? scene()->views().first() : NULL,
                tr("Scale cue timing"),
                tr("Stretch/compress the selected cue(s) by percent\n"
                   "(200 = twice as long, 50 = half):"),
                100.0, 1.0, 10000.0, 0, &ok);
            if (ok && pct > 0.0)
                scaleSelectedCues(pct / 100.0);
        });
        menu.addSeparator();
    }

    foreach (QAction *action, getDefaultActions())
        menu.addAction(action);

    menu.exec(QCursor::pos());
}

void SequenceItem::selectCue(int idx, Qt::KeyboardModifiers mods)
{
    const int count = m_chaser ? m_chaser->stepsCount() : 0;
    if (idx < 0 || idx >= count)
        return;

    if ((mods & Qt::ShiftModifier) && m_selectedStep >= 0 && m_selectedStep < count)
    {
        // Range from the anchor to idx (inclusive); anchor stays put.
        m_selectedSteps.clear();
        const int lo = qMin(m_selectedStep, idx);
        const int hi = qMax(m_selectedStep, idx);
        for (int i = lo; i <= hi; i++)
            m_selectedSteps.insert(i);
    }
    else if (mods & (Qt::ControlModifier | Qt::MetaModifier))
    {
        // Toggle this cue in/out of the selection.
        if (m_selectedSteps.contains(idx))
        {
            m_selectedSteps.remove(idx);
            if (m_selectedStep == idx)
                m_selectedStep = m_selectedSteps.isEmpty() ? -1 : *m_selectedSteps.constBegin();
        }
        else
        {
            m_selectedSteps.insert(idx);
            m_selectedStep = idx;   // moved anchor
        }
    }
    else
    {
        // Plain: single selection.
        m_selectedSteps.clear();
        m_selectedSteps.insert(idx);
        m_selectedStep = idx;
    }
}

bool SequenceItem::isCueSelected(int idx) const
{
    return idx == m_selectedStep || m_selectedSteps.contains(idx);
}

void SequenceItem::deleteSelectedCues()
{
    if (m_chaser == NULL || m_selectedSteps.isEmpty())
        return;

    QList<int> idxs = m_selectedSteps.values();
    std::sort(idxs.begin(), idxs.end());

    QWidget *parent = (scene() && !scene()->views().isEmpty()) ? scene()->views().first() : NULL;
    const QString msg = (idxs.count() == 1)
        ? tr("Delete cue %1 from \"%2\"?\nThis can't be undone.")
              .arg(idxs.first() + 1).arg(m_chaser->name())
        : tr("Delete %1 cues from \"%2\"?\nThis can't be undone.")
              .arg(idxs.count()).arg(m_chaser->name());
    if (QMessageBox::question(parent, tr("Delete cues"), msg,
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    // Remove high→low so earlier indices stay valid as we delete.
    for (int i = idxs.count() - 1; i >= 0; i--)
        m_chaser->removeStep(idxs.at(i));

    m_selectedSteps.clear();
    m_selectedStep = -1;

    prepareGeometryChange();
    calculateWidth();
    if (m_function)
        m_function->setDuration(m_chaser->totalDuration());
    updateTooltip();
    update();
}

bool SequenceItem::selectionContiguousInterior(int &lo, int &hi) const
{
    if (m_chaser == NULL || m_selectedSteps.isEmpty())
        return false;
    QList<int> idxs = m_selectedSteps.values();
    std::sort(idxs.begin(), idxs.end());
    lo = idxs.first();
    hi = idxs.last();
    if (hi - lo + 1 != idxs.count())        // must be a solid run
        return false;
    if (lo <= 0 || hi >= m_chaser->stepsCount() - 1)   // needs a give-cue each side
        return false;
    return true;
}

void SequenceItem::scaleSelectedCues(double factor)
{
    if (m_chaser == NULL || m_selectedSteps.isEmpty() || factor <= 0.0)
        return;

    ensurePerStepDurations();
    QList<int> idxs = m_selectedSteps.values();
    std::sort(idxs.begin(), idxs.end());
    foreach (int i, idxs)
        setStepDuration(i, qint64(qRound64(double(stepDisplayMs(i)) * factor)));

    prepareGeometryChange();
    calculateWidth();
    if (m_function)
        m_function->setDuration(m_chaser->totalDuration());
    updateTooltip();
    update();
}
