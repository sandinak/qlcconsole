/*
  Q Light Controller Plus
  followspotmover.cpp

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

#include "followspotmover.h"
#include "monitorproperties.h"
#include "mastertimer.h"
#include "stagetarget.h"
#include "scene.h"
#include "doc.h"

FollowSpotMover::FollowSpotMover(Doc *doc)
    : m_doc(doc)
    , m_clock(MasterTimer::tick() / 1000.0)
{
}

FollowSpotMover::~FollowSpotMover()
{
    if (m_registered && m_doc != nullptr && m_doc->masterTimer() != nullptr)
        m_doc->masterTimer()->unregisterDMXSource(this);
}

void FollowSpotMover::follow(quint32 sceneId, quint32 targetId)
{
    bool reg = false, unreg = false;
    {
        QMutexLocker l(&m_mutex);
        if (sceneId == m_sceneId && targetId == m_targetId)
            return;
        // Another subject (or none): it starts from rest, not with the last one's speed.
        if (targetId != m_targetId || sceneId == UINT_MAX)
            m_drive.stop();
        m_sceneId = sceneId;
        m_scene = sceneId == UINT_MAX ? nullptr : qobject_cast<Scene *>(m_doc->function(sceneId));
        m_targetId = sceneId == UINT_MAX ? UINT_MAX : targetId;
        if (sceneId != UINT_MAX && !m_registered)
        {
            m_registered = reg = true;
            m_clock.reset();
        }
        else if (sceneId == UINT_MAX && m_registered)
        {
            m_registered = false;
            unreg = true;
        }
    }
    // Outside the lock: the MasterTimer holds its list's while it ticks this.
    MasterTimer *mt = m_doc->masterTimer();
    if (mt == nullptr)
        return;
    if (reg)
        mt->registerDMXSource(this);
    else if (unreg)
        mt->unregisterDMXSource(this);
}

quint32 FollowSpotMover::sceneId() const
{
    QMutexLocker l(&m_mutex);
    return m_sceneId;
}

quint32 FollowSpotMover::targetId() const
{
    QMutexLocker l(&m_mutex);
    return m_targetId;
}

bool FollowSpotMover::isOn() const
{
    QMutexLocker l(&m_mutex);
    return m_sceneId != UINT_MAX;
}

void FollowSpotMover::setSettings(const FollowSpotDrive::Settings &s)
{
    QMutexLocker l(&m_mutex);
    m_drive.setSettings(s);
}

FollowSpotDrive::Settings FollowSpotMover::settings() const
{
    QMutexLocker l(&m_mutex);
    return m_drive.settings();
}

void FollowSpotMover::setSticks(double coarseX, double coarseY, double fineX, double fineY)
{
    QMutexLocker l(&m_mutex);
    m_drive.setCoarse(coarseX, coarseY);
    m_drive.setFine(fineX, fineY);
}

bool FollowSpotMover::stepMultiplier(int dir)
{
    QMutexLocker l(&m_mutex);
    return m_drive.stepMultiplier(dir);
}

double FollowSpotMover::multiplier() const
{
    QMutexLocker l(&m_mutex);
    return m_drive.multiplier();
}

void FollowSpotMover::setMultiplier(double m)
{
    QMutexLocker l(&m_mutex);
    m_drive.setMultiplier(m);
}

QPointF FollowSpotMover::velocity() const
{
    QMutexLocker l(&m_mutex);
    return m_drive.velocity();
}

bool FollowSpotMover::isActive() const
{
    QMutexLocker l(&m_mutex);
    return m_drive.isActive();
}

void FollowSpotMover::stop()
{
    QMutexLocker l(&m_mutex);
    m_drive.stop();
}

int FollowSpotMover::takeMoves()
{
    QMutexLocker l(&m_mutex);
    const int n = m_moves;
    m_moves = 0;
    return n;
}

void FollowSpotMover::setFixedDt(double dt)
{
    QMutexLocker l(&m_mutex);
    m_clock.setFixed(dt);
}

void FollowSpotMover::step(double dt)
{
    QMutexLocker l(&m_mutex);
    if (m_sceneId == UINT_MAX)
        return;
    const QPointF d = m_drive.step(dt);
    if (m_targetId == UINT_MAX || (d.x() == 0.0 && d.y() == 0.0))
        return;
    MonitorProperties *mp = m_doc->monitorProperties();
    StageTarget *t = mp != nullptr ? mp->stageTarget(m_targetId) : nullptr;
    if (t == nullptr)
        return;
    /* Across the floor at its own height, in the live cue's blocking (a
       person on a riser stays on it; Default stays where it was). */
    const quint32 blocking = mp->liveBlocking();
    QVector3D p = t->positionIn(blocking);
    p.setX(qBound(-kReach, p.x() + float(d.x()), kReach));
    p.setY(qBound(-kReach, p.y() + float(d.y()), kReach));
    mp->setTargetPositionIn(t, blocking, p);
    /* Re-resolve only the position looks into the scene's faders, at 0
       time, on its next write (the next frame): the heads converge on the
       moved subject without the dimmer/colour faders being rebuilt. */
    if (m_scene != nullptr)
        m_scene->requestReaim();
    ++m_moves;
}

void FollowSpotMover::writeDMX(MasterTimer *timer, QList<Universe *> universes)
{
    Q_UNUSED(timer)
    Q_UNUSED(universes)
    double dt;
    {
        QMutexLocker l(&m_mutex);
        dt = m_clock.next();
        ++m_frames;
    }
    step(dt);
}

void FollowSpotMover::mainTick(double nominal)
{
    {
        QMutexLocker l(&m_mutex);
        if (m_sceneId == UINT_MAX || m_frames != m_framesSeen)
        {
            m_framesSeen = m_frames;
            m_idleTicks = 0;
            return;
        }
        if (++m_idleTicks < 3)
            return;
    }
    step(nominal);
}
