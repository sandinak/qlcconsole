/*
  Q Light Controller Plus
  horizontalpan.h

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

#ifndef HORIZONTALPAN_H
#define HORIZONTALPAN_H

#include <QPointer>
#include <QObject>
#include <QPoint>

class QAbstractScrollArea;
class QWidget;

/** @addtogroup ui
 * @{
 */

/** Sideways scrolling for a long row of sliders (a scene's fixture tab with
 *  many channels), where a plain wheel turn over a fader moves the fader:
 *
 *  - Shift + wheel (or a sideways trackpad swipe) scrolls the row sideways,
 *    over a fader too;
 *  - a drag with the left button that starts on the space between the
 *    controls (not on a fader, a button or a value box) pans the row, as
 *    does a drag with the middle button anywhere on it.
 *
 *  A plain wheel turn still moves the fader under the pointer, as in QLC+.
 *  One application-wide event filter serves every scroll area enable()d;
 *  an area with nothing to scroll sideways is left alone. */
class HorizontalPan final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(HorizontalPan)

public:
    /** Give @a area the gestures above. */
    static void enable(QAbstractScrollArea *area);
    /** Whether @a area has them. */
    static bool isEnabled(const QAbstractScrollArea *area);

protected:
    /** @reimp */
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    explicit HorizontalPan(QObject *parent);
    /** The enabled area @a w is in, with somewhere to scroll sideways. */
    static QAbstractScrollArea *areaFor(QWidget *w);
    /** Not a control a press means something to (a fader, a button...). */
    static bool isEmptySpace(QWidget *w);
    void startPan();
    void endPan();

private:
    QPointer<QAbstractScrollArea> m_area;
    QPointer<QWidget> m_pressed;    //!< the widget a left press went to
    QPoint m_pressPos;              //!< global
    int m_startValue = 0;
    bool m_pending = false;         //!< left press on empty space, not moved yet
    bool m_panning = false;
};

/** @} */

#endif
