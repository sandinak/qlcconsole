/*
  Q Light Controller Plus
  shrinkablebar.h

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

#ifndef SHRINKABLEBAR_H
#define SHRINKABLEBAR_H

#include <QScrollArea>
#include <QScrollBar>

/** @addtogroup ui
 * @{
 */

/**
 * A row of controls that lets its page get narrower than the row: below its
 * width it scrolls sideways (a scroll bar appears under it) instead of
 * holding the whole window wide, or being cut off on the right when the
 * window is made narrower anyway. As tall as the row, plus the scroll bar
 * while there is one.
 */
class ShrinkableBar final : public QScrollArea
{
public:
    explicit ShrinkableBar(QWidget *bar, QWidget *parent = nullptr)
        : QScrollArea(parent)
    {
        setWidget(bar);
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        setBackgroundRole(bar->backgroundRole());
        setAutoFillBackground(bar->autoFillBackground());
        setPalette(bar->palette());
    }

    QSize minimumSizeHint() const override
    {
        return QSize(120, barHeight());
    }

    QSize sizeHint() const override
    {
        return QSize(widget() != nullptr ? widget()->sizeHint().width() : 120, barHeight());
    }

protected:
    void resizeEvent(QResizeEvent *e) override
    {
        QScrollArea::resizeEvent(e);
        updateGeometry();       // the scroll bar comes or goes: so does its height
    }

private:
    int barHeight() const
    {
        if (widget() == nullptr)
            return 0;
        int h = widget()->sizeHint().height();
        if (widget()->minimumSizeHint().width() > width())
            h += horizontalScrollBar()->sizeHint().height();
        return h;
    }
};

/** @} */

#endif
