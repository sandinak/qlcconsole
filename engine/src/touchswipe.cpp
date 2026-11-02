/*
  Q Light Controller Plus
  touchswipe.cpp

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

#include <cmath>

#include "touchswipe.h"

using R = GamepadRole::Role;

void TouchSwipe::setValue(R role, float norm)
{
    switch (role)
    {
        case R::TouchX:     m_x = norm; break;
        case R::TouchY:     m_y = norm; break;
        case R::TouchDown:  m_down = norm > 0.5f; break;
        case R::Touch2Down: m_down2 = norm > 0.5f; break;
        default: break;
    }
}

void TouchSwipe::setClick(bool down)
{
    m_click = down;
}

void TouchSwipe::reset()
{
    m_down = m_down2 = m_click = false;
    m_tracking = m_spent = false;
    m_wait = 0.0;
}

TouchSwipe::Swipe TouchSwipe::take(double dt)
{
    if (m_wait > 0.0)
        m_wait -= dt;
    if (!m_down)
    {
        m_tracking = m_spent = false;
        return None;
    }
    if (!m_tracking)
    {
        // A new touch: counted from where it landed.
        m_tracking = true;
        m_spent = false;
        m_fromX = m_x;
        m_fromY = m_y;
    }
    // A second finger or a click is no swipe: spoilt until the lift.
    if (m_down2 || m_click)
        m_spent = true;
    if (m_spent)
        return None;

    const float dx = m_x - m_fromX;
    const float dy = m_y - m_fromY;
    const float ax = std::fabs(dx);
    const float ay = std::fabs(dy);
    Swipe s = None;
    if (ax >= threshold() && ay <= ax * straightness())
        s = dx > 0.0f ? Right : Left;
    else if (ay >= threshold() && ax <= ay * straightness())
        s = dy > 0.0f ? Down : Up;
    if (s == None)
        return None;
    m_spent = true;
    if (m_wait > 0.0)
        return None;
    m_wait = cooldown();
    return s;
}
