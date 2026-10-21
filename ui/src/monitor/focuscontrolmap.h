/*
  Q Light Controller Plus
  focuscontrolmap.h

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

#ifndef FOCUSCONTROLMAP_H
#define FOCUSCONTROLMAP_H

#include <QPainterPath>
#include <QWidget>
#include <QHash>
#include <QMap>
#include <QSet>

#include "gamepadrole.h"

/** @addtogroup ui UI
 * @{
 */

/**
 * Focus mode's map of its controls, drawn as the device itself: a gamepad
 * (an Xbox-style layout, which any controller whose profile gives roles is
 * drawn as, with that profile's own names for its buttons) or the keys of a
 * keyboard -- each labelled with what it does in Focus mode. What a control
 * does comes from its gamepad role (FocusSession::controlFor()), so a
 * controller is labelled by its own bindings; a control the profile does
 * not have is greyed. Controls light while they are pressed and the sticks
 * show their deflection, so the mapping can be checked on the hardware.
 * A click on a control does what pressing it would.
 */
class FocusControlMap final : public QWidget
{
    Q_OBJECT

public:
    enum Mode { ControllerMap, KeyboardMap };

    explicit FocusControlMap(QWidget *parent = nullptr);

    Mode mode() const { return m_mode; }
    void setMode(Mode mode);

    /** The controller's controls: GamepadRole::Role -> the profile's name
     *  for it (empty: named by role only). */
    void setControls(const QMap<int, QString> &controls);
    /** The D-pad's and arrows' step, for their labels. */
    void setStepDegrees(float step);

    /** A control moved: 0 or 1 for a button, 0..1 (0.5 centred, Y 0 = up)
     *  for a stick axis. */
    void setControlValue(int role, float value);
    /** A key went down or up (Qt::Key; Return/Enter, Backspace/Delete,
     *  ]/Tab, [/Backtab and +/= are one key each here). */
    void setKeyDown(int key, bool down);
    /** Nothing held, sticks centred. */
    void clearInput();

    /* What it shows -- for the panel's tests. */
    /** What the map says @p role does ("Save", "Next head"...); empty for a
     *  control Focus mode does not use. */
    QString actionText(int role) const;
    /** The name the map gives @p role: the profile's, else the usual one. */
    QString controlName(int role) const;
    /** The controller has @p role (it is not greyed). */
    bool isMapped(int role) const;
    bool isLit(int role) const;
    /** A stick's deflection as drawn, -1..1 each way (Y -1 = up). */
    QPointF stickDeflection(bool fine) const;
    /** The action a key is labelled with; empty for a key not on the map. */
    QString keyAction(int key) const;
    bool isKeyLit(int key) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /** A control on the controller map was clicked (a GamepadRole::Role). */
    void controlClicked(int role);
    /** A key on the keyboard map was clicked (a Qt::Key). */
    void keyClicked(int key);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    struct Hit
    {
        QPainterPath shape;   ///< widget coordinates
        int id;               ///< a role, or a key
        QString tip;
    };

    void paintController(QPainter &p);
    void paintKeyboard(QPainter &p);
    int hitAt(const QPointF &pos, QString *tip = nullptr) const;
    static int canonicalKey(int key);

private:
    Mode m_mode = ControllerMap;
    QMap<int, QString> m_controls;
    float m_step = 0.1f;
    QSet<int> m_lit;              ///< roles held
    float m_axis[4] = { 0.5f, 0.5f, 0.5f, 0.5f };   ///< LX LY RX RY
    QSet<int> m_keys;             ///< keys held
    QList<Hit> m_hits;
    int m_hover = -1;
};

/** @} */

#endif
