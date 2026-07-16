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

SequenceItem::SequenceItem(Chaser *seq, ShowFunction *func)
    : ShowItem(func)
    , m_chaser(seq)
    , m_selectedStep(-1)
    , m_stepResizeIdx(-1)
    , m_stepResizeOrigDur(0)
    , m_stepResizePressX(0)
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

void SequenceItem::calculateWidth()
{
    int newWidth = 0;
    quint32 seq_duration = m_chaser->totalDuration();
    float timeUnit = 50.0 / float(getTimeScale());

    if (seq_duration == Function::infiniteSpeed())
    {
        newWidth = timeUnit * 10000;
    }
    else
    {
        if (seq_duration != 0)
            newWidth = (timeUnit * float(seq_duration)) / 1000.0;

        if (newWidth < timeUnit)
            newWidth = timeUnit;
    }
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
        uint stepDuration = step.duration;

        if (m_chaser->fadeInMode() == Chaser::Common)
            stepFadeIn = m_chaser->fadeInSpeed();
        if (m_chaser->fadeOutMode() == Chaser::Common)
            stepFadeOut = m_chaser->fadeOutSpeed();
        if (m_chaser->durationMode() == Chaser::Common)
            stepDuration = m_chaser->duration();

        // avoid hanging on infinite duration
        if (stepDuration == Function::infiniteSpeed())
            stepDuration = 10 * 1000 * 1000;

        float stepWidth = ((timeUnit * (float)stepDuration) / 1000);

        // Shade each step (alternating) so the steps read as distinct aligned
        // blocks within the chaser rather than one flat bar.
        if (stepWidth > 1.0f)
        {
            QColor shade = (stepIdx % 2 == 0) ? m_color.lighter(118) : m_color.darker(112);
            painter->fillRect(QRectF(xpos, 0, stepWidth, TRACK_HEIGHT - 3), shade);
        }

        // draw fade in line
        if (stepFadeIn > 0)
        {
            int fadeXpos = xpos + ((timeUnit * (float)stepFadeIn) / 1000);
            // doesn't even draw it if too small
            if (fadeXpos - xpos > 5)
            {
                painter->setPen(QPen(Qt::gray, 1));
                painter->drawLine(xpos, TRACK_HEIGHT - 4, fadeXpos, 1);
            }
        }
        // draw selected step
        if (stepIdx == m_selectedStep)
        {
            painter->setPen(QPen(Qt::yellow, 2));
            painter->setBrush(QBrush(Qt::NoBrush));
            painter->drawRect(xpos, 0, stepWidth, TRACK_HEIGHT - 3);
        }

        // Step label (falls back to the step number when unnamed), top-aligned
        // and clipped to the step so each aligned step is identifiable.
        QString label = step.note.isEmpty() ? tr("%1").arg(stepIdx + 1) : step.note;
        painter->setPen(QPen(Qt::white, 1));
        QRect textRect = QRect(xpos + 2, 1, stepWidth - 3, TRACK_HEIGHT - 4);
        painter->drawText(textRect, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, label);

        xpos += stepWidth;

        // draw step vertical delimiter
        painter->setPen(QPen(Qt::white, 1));
        painter->drawLine(xpos, 1, xpos, TRACK_HEIGHT - 5);

        // draw fade out line
        if (stepFadeOut > 0)
        {
            int fadeXpos = xpos + ((timeUnit * (float)stepFadeOut) / 1000);
            // doesn't even draw it if too small
            if (fadeXpos - xpos > 5)
            {
                painter->setPen(QPen(Qt::gray, 1));
                painter->drawLine(xpos, 1, fadeXpos, TRACK_HEIGHT - 4);
            }
        }
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
    m_chaser->setTotalDuration(msec);
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
        quint32 dur = effectiveStepDuration(i);
        if (dur == Function::infiniteSpeed())
            dur = 10 * 1000 * 1000;
        xpos += (timeUnit * float(dur)) / 1000.0f;
        // The final divider coincides with the item's right stretch handle —
        // leave that to ShowItem so whole-item resize still works.
        if (i < count - 1 && qAbs(localX - xpos) <= 4.0)
            return i;
    }
    return -1;
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

void SequenceItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    // Split cursor over an interior step divider to advertise cue retiming.
    if (m_editable && m_locked == false && stepBoundaryAt(event->pos().x()) >= 0)
        setCursor(Qt::SplitHCursor);
    else
        ShowItem::hoverMoveEvent(event);
}

void SequenceItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_editable && m_locked == false && event->button() == Qt::LeftButton
        && edgeAt(event->pos().x()) == NoEdge)
    {
        int idx = stepBoundaryAt(event->pos().x());
        if (idx >= 0)
        {
            ensurePerStepDurations();
            m_stepResizeIdx = idx;
            m_stepResizeOrigDur = m_chaser->steps().at(idx).duration;
            m_stepResizePressX = event->scenePos().x();
            m_selectedStep = idx;
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
    if (m_stepResizeIdx >= 0)
    {
        const float timeUnit = 50.0f / float(m_timeScale);
        const qreal dx = event->scenePos().x() - m_stepResizePressX;
        qint64 newDur = qint64(m_stepResizeOrigDur) + qint64((dx / timeUnit) * 1000.0);
        if (newDur < 100)
            newDur = 100;                   // keep a cue at least 0.1 s

        ChaserStep s = m_chaser->steps().at(m_stepResizeIdx);
        if (quint32(newDur) != s.duration)
        {
            s.duration = quint32(newDur);
            m_chaser->replaceStep(s, m_stepResizeIdx);
            prepareGeometryChange();
            calculateWidth();
            if (m_function)
                m_function->setDuration(m_chaser->totalDuration());
            update();
        }
        event->accept();
        return;
    }
    ShowItem::mouseMoveEvent(event);
}

void SequenceItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_stepResizeIdx >= 0)
    {
        m_stepResizeIdx = -1;
        setFlag(QGraphicsItem::ItemIsMovable, m_locked == false && m_editable);
        updateTooltip();
        // Commit through the resize path (marks modified + resolves overlaps).
        emit itemResized(this, false);
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
