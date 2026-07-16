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
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>

#include "sequenceitem.h"
#include "chaserstep.h"
#include "trackitem.h"
#include "function.h"
#include "doc.h"

/** Nominal on-timeline width (ms) for a manual-GO (infinite) or un-timed (0)
 *  step, so every cue stays visible and draggable. */
#define SEQ_NOMINAL_STEP_MS 3000

/** Smallest hold (ms) a cue can be dragged to. */
#define SEQ_MIN_STEP_MS 100

SequenceItem::SequenceItem(Chaser *seq, ShowFunction *func)
    : ShowItem(func)
    , m_chaser(seq)
    , m_selectedStep(-1)
    , m_cueDrag(CueNone)
    , m_cueDragIdx(-1)
    , m_cueDragPressX(0)
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
        m_selectedStep = -1;

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

        const float top = 1.0f;
        const float bot = TRACK_HEIGHT - 3;
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
        if (stepFadeIn > 0)
        {
            float fw = qMin(stepWidth, (timeUnit * float(stepFadeIn)) / 1000.0f);
            if (fw > 1.0f)
            {
                QPolygonF wedge;
                wedge << QPointF(xpos, bot) << QPointF(xpos + fw, top)
                      << QPointF(xpos + fw, bot);
                painter->drawPolygon(wedge);
            }
        }
        if (stepFadeOut > 0)
        {
            float fw = qMin(stepWidth, (timeUnit * float(stepFadeOut)) / 1000.0f);
            if (fw > 1.0f)
            {
                QPolygonF wedge;
                wedge << QPointF(xEnd - fw, bot) << QPointF(xEnd - fw, top)
                      << QPointF(xEnd, bot);
                painter->drawPolygon(wedge);
            }
        }

        // draw selected step
        if (stepIdx == m_selectedStep)
        {
            painter->setPen(QPen(Qt::yellow, 2));
            painter->setBrush(QBrush(Qt::NoBrush));
            painter->drawRect(xpos, 0, stepWidth, TRACK_HEIGHT - 3);
        }

        // Cue label: the cue's name (note → fired-function name → "Cue N").
        painter->setPen(QPen(Qt::white, 1));
        QRect textRect = QRect(xpos + 3, 1, stepWidth - 4, TRACK_HEIGHT - 4);
        painter->drawText(textRect, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap,
                          cueLabel(stepIdx));

        xpos = xEnd;

        // draw step vertical delimiter
        painter->setPen(QPen(Qt::white, 1));
        painter->drawLine(xpos, 1, xpos, TRACK_HEIGHT - 5);

        stepIdx++;
    }

    ShowItem::postPaint(painter);
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
    if (idx < 0 || idx >= m_chaser->stepsCount())
        return QString();
    const ChaserStep s = m_chaser->steps().at(idx);
    if (s.note.isEmpty() == false)
        return s.note;
    Function *f = (m_chaser->doc() != NULL) ? m_chaser->doc()->function(s.fid) : NULL;
    if (f != NULL)
        return f->name();
    return tr("Cue %1").arg(idx + 1);
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

void SequenceItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    // Split cursor over an interior divider (roll); move cursor over a cue body
    // (slip); otherwise the base ShowItem cursors (open-hand / resize handles).
    if (m_editable && m_locked == false && edgeAt(event->pos().x()) == NoEdge
        && stepBoundaryAt(event->pos().x()) >= 0)
        setCursor(Qt::SplitHCursor);
    else if (m_editable && m_locked == false && edgeAt(event->pos().x()) == NoEdge
             && (event->modifiers() & Qt::AltModifier) == 0)
        setCursor(Qt::SizeHorCursor);
    else
        ShowItem::hoverMoveEvent(event);
}

void SequenceItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // Alt/Option forces a whole-block move (defer to ShowItem). Otherwise, an
    // interior divider = ROLL (retime the two adjacent cues, total fixed); a cue
    // body = SLIP (move that cue, keep its length, neighbours absorb).
    const bool alt = event->modifiers() & Qt::AltModifier;
    if (m_editable && m_locked == false && event->button() == Qt::LeftButton
        && alt == false && edgeAt(event->pos().x()) == NoEdge)
    {
        int div = stepBoundaryAt(event->pos().x());
        int cue = (div < 0) ? cueAt(event->pos().x()) : -1;

        // Slip needs a neighbour on each side; edge cues fall through to a
        // whole-block move (there's nothing to absorb the shift otherwise).
        if (div >= 0 || (cue > 0 && cue < m_chaser->stepsCount() - 1))
        {
            ensurePerStepDurations();
            snapshotDurations();
            m_cueDrag = (div >= 0) ? CueRoll : CueSlip;
            m_cueDragIdx = (div >= 0) ? div : cue;
            m_cueDragPressX = event->scenePos().x();
            m_selectedStep = m_cueDragIdx;
            setFlag(QGraphicsItem::ItemIsMovable, false);
            setSelected(true);
            update();
            event->accept();
            return;
        }
    }
    ShowItem::mousePressEvent(event);
}

void SequenceItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_cueDrag != CueNone && m_cueOrigDur.count() == m_chaser->stepsCount())
    {
        const float timeUnit = 50.0f / float(m_timeScale);
        const qint64 ddt = qint64(((event->scenePos().x() - m_cueDragPressX) / timeUnit) * 1000.0);
        const int i = m_cueDragIdx;

        if (m_cueDrag == CueRoll)
        {
            // Move the divider between cue i and i+1: grow one, shrink the other,
            // keeping their sum (and every downstream cue) fixed.
            const qint64 sum = qint64(m_cueOrigDur.at(i)) + qint64(m_cueOrigDur.at(i + 1));
            qint64 left = qint64(m_cueOrigDur.at(i)) + ddt;
            left = qBound<qint64>(SEQ_MIN_STEP_MS, left, sum - SEQ_MIN_STEP_MS);
            setStepDuration(i, left);
            setStepDuration(i + 1, sum - left);
        }
        else // CueSlip: move cue i, keep its length; prev grows, next shrinks.
        {
            const qint64 prevSum = qint64(m_cueOrigDur.at(i - 1)) + qint64(m_cueOrigDur.at(i + 1));
            qint64 prev = qint64(m_cueOrigDur.at(i - 1)) + ddt;
            prev = qBound<qint64>(SEQ_MIN_STEP_MS, prev, prevSum - SEQ_MIN_STEP_MS);
            setStepDuration(i - 1, prev);
            setStepDuration(i + 1, prevSum - prev);
        }

        prepareGeometryChange();
        calculateWidth();
        if (m_function)
            m_function->setDuration(m_chaser->totalDuration());
        update();
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

void SequenceItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *)
{
    QMenu menu;
    QFont menuFont = qApp->font();
    menuFont.setPixelSize(14);
    menu.setFont(menuFont);

    foreach (QAction *action, getDefaultActions())
        menu.addAction(action);

    menu.exec(QCursor::pos());
}
