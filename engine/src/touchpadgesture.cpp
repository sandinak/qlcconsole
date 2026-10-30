/*
  Q Light Controller Plus
  touchpadgesture.cpp

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

#include <QtGlobal>

#include "touchpadgesture.h"

using R = GamepadRole::Role;

void TouchpadGesture::setValue(GamepadRole::Role role, float norm)
{
    switch (role)
    {
        case R::TouchX:     m_now[0].x = norm; break;
        case R::TouchY:     m_now[0].y = norm; break;
        case R::TouchDown:  m_now[0].down = norm > 0.5f; break;
        case R::Touch2X:    m_now[1].x = norm; break;
        case R::Touch2Y:    m_now[1].y = norm; break;
        case R::Touch2Down: m_now[1].down = norm > 0.5f; break;
        default: break;
    }
}

void TouchpadGesture::setClick(bool down)
{
    if (down && !m_click)
        m_clicked = true;
    m_click = down;
}

int TouchpadGesture::fingers() const
{
    return (m_now[0].down ? 1 : 0) + (m_now[1].down ? 1 : 0);
}

void TouchpadGesture::restart()
{
    m_last[0] = m_now[0];
    m_last[1] = m_now[1];
    m_moving = false;
    m_heldX = m_heldY = 0.0f;
}

TouchpadGesture::Motion TouchpadGesture::take()
{
    Motion m;
    const bool clicked = m_click || m_clicked;
    m_clicked = false;
    // A finger landed, lifted, joined or left; or the pad is pressed.
    if (clicked || m_now[0].down != m_last[0].down || m_now[1].down != m_last[1].down)
    {
        restart();
        return m;
    }
    const int n = fingers();
    if (n == 0)
        return m;

    float dx = 0.0f, dy = 0.0f;
    for (int f = 0; f < 2; ++f)
        if (m_now[f].down)
        {
            dx += m_now[f].x - m_last[f].x;
            dy += m_now[f].y - m_last[f].y;
        }
    dx /= float(n);
    dy /= float(n);
    m_last[0] = m_now[0];
    m_last[1] = m_now[1];

    m_heldX += dx;
    m_heldY += dy;
    const float far = qMax(qAbs(m_heldX), qAbs(m_heldY));
    if (far < (m_moving ? jitter() : slop()) - 1e-6f)
        return m;
    m.started = !m_moving;
    m_moving = true;
    m.kind = n == 2 ? TwoFingerDrag : Drag;
    m.dx = m_heldX;
    m.dy = m_heldY;
    m_heldX = m_heldY = 0.0f;
    return m;
}

void TouchpadGesture::reset()
{
    m_now[0] = m_now[1] = Finger();
    m_click = m_clicked = false;
    restart();
}
