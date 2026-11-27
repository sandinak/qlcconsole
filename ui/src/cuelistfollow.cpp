/*
  Q Light Controller Plus
  cuelistfollow.cpp

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

#include <QAbstractItemView>
#include <QElapsedTimer>
#include <QTreeWidget>
#include <QScrollBar>
#include <QTimer>
#include <QEvent>

#include "cuelistfollow.h"

static const qint64 kWheelQuietMs = 500;

static qint64 nowMs()
{
    static QElapsedTimer clock;
    if (!clock.isValid())
        clock.start();
    return clock.elapsed() + 1;
}

CueListFollow::CueListFollow(QTreeWidget *tree)
    : QObject(tree)
    , m_tree(tree)
{
    m_tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tree->viewport()->installEventFilter(this);
    m_tree->verticalScrollBar()->installEventFilter(this);
    // A scroll bar drag is a hand on the list as well.
    connect(m_tree->verticalScrollBar(), &QAbstractSlider::rangeChanged, this, &CueListFollow::pad);
    pad(m_tree->verticalScrollBar()->minimum(), m_tree->verticalScrollBar()->maximum());
}

bool CueListFollow::userActive() const
{
    return m_mouseDown || (m_wheelAt != 0 && nowMs() - m_wheelAt < kWheelQuietMs);
}

void CueListFollow::follow(QTreeWidgetItem *item)
{
    m_item = item;
    m_touched = false;
    if (item == nullptr)
        return;
    if (userActive())
    {
        m_pending = true;   // waits for the release / the wheel to stop
        return;
    }
    apply();
}

void CueListFollow::apply()
{
    m_pending = false;
    if (m_item == nullptr || m_item->treeWidget() != m_tree)
        return;     // none, or a row since removed
    QScrollBar *bar = m_tree->verticalScrollBar();
    const QRect rect = m_tree->visualItemRect(m_item);
    if (!rect.isValid() || rect.height() <= 0)
        return;
    const int viewport = m_tree->viewport()->height();
    const int contentTop = rect.top() + bar->value();       // pixel mode: value is the top
    const int want = contentTop - (viewport - rect.height()) / 2;
    bar->setValue(qBound(bar->minimum(), want, bar->maximum()));
    // A view too small for the rule (under a row high) still shows the row.
    const QRect after = m_tree->visualItemRect(m_item);
    if (after.top() < 0 || after.bottom() > m_tree->viewport()->height())
        m_tree->scrollTo(m_tree->indexFromItem(m_item), QAbstractItemView::EnsureVisible);
}

/* The scroll bar's range as the tree sets it, plus half a view less half a
   row, so the last row can reach the middle. A list that fits needs none. */
void CueListFollow::pad(int min, int max)
{
    if (m_padding)
        return;
    QScrollBar *bar = m_tree->verticalScrollBar();
    int extra = 0;
    if (max > min && m_tree->topLevelItemCount() > 0)
    {
        // The last visible row's height stands for a row.
        const int row = qMax(1, m_tree->sizeHintForRow(0));
        extra = qMax(0, (m_tree->viewport()->height() - row) / 2);
    }
    m_pad = extra;
    if (extra == 0)
        return;
    m_padding = true;
    bar->setRange(min, max + extra);
    m_padding = false;
}

bool CueListFollow::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type())
    {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
            m_mouseDown = true;
            m_touched = true;
        break;
        case QEvent::MouseButtonRelease:
            m_mouseDown = false;
            if (m_pending)
            {
                // The click has been handled by now: centre what it picked.
                QPointer<CueListFollow> self(this);
                QTimer::singleShot(0, this, [self]() {
                    if (self != nullptr && self->m_pending && !self->userActive())
                        self->apply();
                });
            }
        break;
        case QEvent::Wheel:
            m_wheelAt = nowMs();
            m_touched = true;
        break;
        case QEvent::Resize:
            // The view settled to a new size: the active row goes back to the middle.
            if (watched == m_tree->viewport() && m_item != nullptr && !m_touched && !userActive())
                QTimer::singleShot(0, this, [this]() { if (!m_touched && !userActive()) apply(); });
        break;
        default:
        break;
    }
    return QObject::eventFilter(watched, event);
}
