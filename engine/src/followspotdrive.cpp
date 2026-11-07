/*
  Q Light Controller Plus
  followspotdrive.cpp

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
#include <QtGlobal>

#include "followspotdrive.h"

void FollowSpotDrive::setSettings(const Settings &s)
{
    m_settings.topSpeed = qBound(0.05, s.topSpeed, 20.0);
    m_settings.fineShare = qBound(0.01, s.fineShare, 1.0);
    m_settings.expo = qBound(0.0, s.expo, 1.0);
    m_settings.deadzone = qBound(0.0, s.deadzone, 0.8);
}

void FollowSpotDrive::setCoarse(double x, double y)
{
    m_coarseX = qBound(-1.0, x, 1.0);
    m_coarseY = qBound(-1.0, y, 1.0);
}

void FollowSpotDrive::setFine(double x, double y)
{
    m_fineX = qBound(-1.0, x, 1.0);
    m_fineY = qBound(-1.0, y, 1.0);
}

const QVector<double> &FollowSpotDrive::multipliers()
{
    static const QVector<double> steps = { 0.25, 0.5, 1.0, 1.5, 2.0 };
    return steps;
}

bool FollowSpotDrive::stepMultiplier(int dir)
{
    const QVector<double> &steps = multipliers();
    int at = 0;
    for (int i = 0; i < steps.size(); ++i)
        if (std::abs(steps.at(i) - m_multiplier) < std::abs(steps.at(at) - m_multiplier))
            at = i;
    const int to = qBound(0, at + (dir > 0 ? 1 : -1), int(steps.size()) - 1);
    if (steps.at(to) == m_multiplier)
        return false;
    m_multiplier = steps.at(to);
    return true;
}

void FollowSpotDrive::setMultiplier(double m)
{
    m_multiplier = qBound(multipliers().first(), m, multipliers().last());
}

double FollowSpotDrive::shape(double deflection, double deadzone, double expo)
{
    const double mag = std::abs(deflection);
    if (mag <= deadzone || deadzone >= 1.0)
        return 0.0;
    const double a = qMin(1.0, (mag - deadzone) / (1.0 - deadzone));
    const double out = (1.0 - expo) * a + expo * a * a * a;
    return deflection < 0 ? -out : out;
}

QPointF FollowSpotDrive::wanted() const
{
    const Settings &s = m_settings;
    const double full = s.topSpeed * m_multiplier;
    const double x = shape(m_coarseX, s.deadzone, s.expo) + s.fineShare * shape(m_fineX, s.deadzone, s.expo);
    const double y = shape(m_coarseY, s.deadzone, s.expo) + s.fineShare * shape(m_fineY, s.deadzone, s.expo);
    return QPointF(full * x, full * y);
}

double FollowSpotDrive::Axis::step(double w, double dt)
{
    /* Critically damped toward w, solved exactly over dt (any tick length,
       no overshoot from rest): with e = v - w and c = a + k e,
         e(t) = (e0 + c t) exp(-k t),  a(t) = (a0 - k c t) exp(-k t),
       and the distance is w dt plus the integral of e(t). */
    const double k = kResponse;
    const double e0 = v - w;
    const double c = a + k * e0;
    const double E = std::exp(-k * dt);
    const double moved = w * dt + e0 * (1.0 - E) / k + c * (1.0 - E * (1.0 + k * dt)) / (k * k);
    v = w + (e0 + c * dt) * E;
    a = (a - k * c * dt) * E;
    if (w == 0.0 && std::abs(v) < kStopped && std::abs(a) < kStopped * k)
        v = a = 0.0;
    return moved;
}

QPointF FollowSpotDrive::step(double dt)
{
    if (dt <= 0.0)
        return QPointF();
    const QPointF w = wanted();
    const double dx = m_x.step(w.x(), dt);
    const double dy = m_y.step(w.y(), dt);
    return QPointF(dx, dy);
}

bool FollowSpotDrive::isActive() const
{
    if (m_x.v != 0.0 || m_y.v != 0.0 || m_x.a != 0.0 || m_y.a != 0.0)
        return true;
    const QPointF w = wanted();
    return w.x() != 0.0 || w.y() != 0.0;
}

void FollowSpotDrive::stop()
{
    m_x = Axis();
    m_y = Axis();
}

double TickClock::next()
{
    if (m_fixed > 0.0)
        return m_fixed;
    if (!m_clock.isValid())
    {
        m_clock.start();
        return m_nominal;
    }
    const double dt = m_clock.nsecsElapsed() / 1.0e9;
    m_clock.restart();
    // A stall is not one big jump; a burst (two ticks at once) is no time.
    return qBound(0.0, dt, qMax(m_nominal * 5.0, 0.1));
}
