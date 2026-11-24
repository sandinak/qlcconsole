/*
  Q Light Controller
  grandmasterslider.h

  Copyright (c) Heikki Junnila

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

#ifndef GRANDMASTERSLIDER_H
#define GRANDMASTERSLIDER_H

#include <QFrame>

#include "grandmaster.h"
#include "faderpickup.h"
#include "qlcui_export.h"

class InputOutputMap;
class QSlider;
class QLabel;

/** @addtogroup ui
 * @{
 */

class QLC_UI_EXPORT GrandMasterSlider final : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY(GrandMasterSlider)

public:
    GrandMasterSlider(QWidget* parent, InputOutputMap* ioMap);
    virtual ~GrandMasterSlider();

    bool invertedAppearance() const;

    /** "Grand Master fader: move it to 75% to take over" while the external
     *  fader waits to be picked up; empty otherwise. Also in the tooltip. */
    QString pickupHint() const;

    /** Which way to move the external fader to pick the GM up: +1 up,
     *  -1 down, 0 when it is not waiting */
    int pickupDirection() const;

    /** @p name with an arrow the way to move a waiting fader ("GM ▲") */
    static QString pickupLabel(const QString &name, int direction);

    void setInvertedAppearance(bool invert);

signals:
    /** The external fader started or stopped waiting to be picked up */
    void pickupHintChanged(const QString &hint);

private:
    void updateTooltip();
    void updateDisplayValue();
    void updatePickupHint();

protected slots:
    void slotValueChanged(int value);
    void slotGrandMasterValueChanged(uchar value);
    void slotGrandMasterValueModeChanged(GrandMaster::ValueMode mode);

protected:
    QLabel* m_valueLabel;
    QSlider* m_slider;
    QLabel* m_nameLabel;
    InputOutputMap* m_ioMap;
    QString m_pickupHint;

    /*************************************************************************
     * External input
     *************************************************************************/
private:
    void sendFeedback();

protected slots:
    void slotInputValueChanged(quint32 universe, quint32 channel, uchar value);
};

/** @} */

#endif
