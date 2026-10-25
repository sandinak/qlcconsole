/*
  Q Light Controller Plus
  horizontalpan.cpp

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

#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QAbstractButton>
#include <QAbstractSlider>
#include <QApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QLineEdit>
#include <QComboBox>

#include "horizontalpan.h"

static const char *kPanProperty = "qlcHorizontalPan";
static QPointer<HorizontalPan> s_pan;

HorizontalPan::HorizontalPan(QObject *parent)
    : QObject(parent)
{
}

void HorizontalPan::enable(QAbstractScrollArea *area)
{
    if (area == nullptr)
        return;
    area->setProperty(kPanProperty, true);
    if (s_pan.isNull() && qApp != nullptr)
    {
        s_pan = new HorizontalPan(qApp);
        qApp->installEventFilter(s_pan);
    }
}

bool HorizontalPan::isEnabled(const QAbstractScrollArea *area)
{
    return area != nullptr && area->property(kPanProperty).toBool() && !s_pan.isNull();
}

QAbstractScrollArea *HorizontalPan::areaFor(QWidget *w)
{
    // A scroll bar scrolls itself.
    if (w == nullptr || qobject_cast<QScrollBar *>(w) != nullptr)
        return nullptr;
    for (; w != nullptr; w = w->parentWidget())
    {
        QAbstractScrollArea *area = qobject_cast<QAbstractScrollArea *>(w);
        if (area != nullptr && area->property(kPanProperty).toBool())
        {
            const QScrollBar *bar = area->horizontalScrollBar();
            return bar->maximum() > bar->minimum() ? area : nullptr;
        }
        if (w->isWindow())
            break;
    }
    return nullptr;
}

bool HorizontalPan::isEmptySpace(QWidget *w)
{
    for (QWidget *c = w; c != nullptr && !c->isWindow(); c = c->parentWidget())
    {
        if (qobject_cast<QAbstractSlider *>(c) != nullptr
            || qobject_cast<QAbstractButton *>(c) != nullptr
            || qobject_cast<QAbstractSpinBox *>(c) != nullptr
            || qobject_cast<QLineEdit *>(c) != nullptr
            || qobject_cast<QComboBox *>(c) != nullptr)
            return false;
        if (qobject_cast<QAbstractScrollArea *>(c) != nullptr)
            break;
    }
    return true;
}

void HorizontalPan::startPan()
{
    m_pending = false;
    m_panning = true;
    m_startValue = m_area->horizontalScrollBar()->value();
    QApplication::setOverrideCursor(Qt::ClosedHandCursor);
}

void HorizontalPan::endPan()
{
    if (m_panning)
        QApplication::restoreOverrideCursor();
    m_panning = false;
    m_pending = false;
    m_pressed.clear();
    m_area.clear();
}

bool HorizontalPan::eventFilter(QObject *obj, QEvent *ev)
{
    switch (ev->type())
    {
        case QEvent::Wheel:
        {
            QAbstractScrollArea *area = areaFor(qobject_cast<QWidget *>(obj));
            if (area == nullptr)
                return false;
            QWheelEvent *we = static_cast<QWheelEvent *>(ev);
            const QPoint px = we->pixelDelta();
            const QPoint ang = we->angleDelta();
            const bool shift = we->modifiers().testFlag(Qt::ShiftModifier);
            const bool sideways = qAbs(ang.x()) > qAbs(ang.y());
            if (!shift && !sideways)
                return false;   // a fader's, or the area's own vertical scroll

            // Shift + a mouse wheel: its turn, whichever way the system
            // reports it; a swipe: its sideways part.
            QScrollBar *bar = area->horizontalScrollBar();
            int d;
            if (!px.isNull())
                d = qAbs(px.x()) > qAbs(px.y()) ? px.x() : px.y();
            else
                d = (sideways ? ang.x() : ang.y()) * bar->singleStep() * 3 / 120;
            bar->setValue(bar->value() - d);
            return true;
        }

        case QEvent::MouseButtonPress:
        {
            QWidget *w = qobject_cast<QWidget *>(obj);
            QMouseEvent *me = static_cast<QMouseEvent *>(ev);
            if (m_panning || m_pending)
                return m_panning;
            QAbstractScrollArea *area = areaFor(w);
            if (area == nullptr)
                return false;
            if (me->button() == Qt::MiddleButton)
            {
                m_area = area;
                m_pressPos = me->globalPosition().toPoint();
                startPan();
                return true;
            }
            if (me->button() == Qt::LeftButton && isEmptySpace(w))
            {
                // Through to the widget: a click stays a click until it
                // moves far enough to be a drag.
                m_area = area;
                m_pressed = w;
                m_pressPos = me->globalPosition().toPoint();
                m_pending = true;
            }
            return false;
        }

        case QEvent::MouseMove:
        {
            if (!m_pending && !m_panning)
                return false;
            QMouseEvent *me = static_cast<QMouseEvent *>(ev);
            const QPoint pos = me->globalPosition().toPoint();
            if (m_area.isNull())
            {
                endPan();
                return false;
            }
            if (m_pending)
            {
                if ((pos - m_pressPos).manhattanLength() < QApplication::startDragDistance())
                    return false;
                // A drag: the widget pressed lets go of the click (released
                // away from itself, so it is no click), then the row pans.
                m_pending = false;
                if (!m_pressed.isNull())
                {
                    QMouseEvent cancel(QEvent::MouseButtonRelease, QPointF(-10000, -10000),
                                       QPointF(-10000, -10000), Qt::LeftButton,
                                       Qt::NoButton, me->modifiers());
                    QApplication::sendEvent(m_pressed, &cancel);
                }
                startPan();
            }
            m_area->horizontalScrollBar()->setValue(m_startValue - (pos.x() - m_pressPos.x()));
            return true;
        }

        case QEvent::MouseButtonRelease:
        {
            if (m_panning)
            {
                endPan();
                return true;
            }
            if (m_pending)
                endPan();
            return false;
        }

        default:
            return false;
    }
}
