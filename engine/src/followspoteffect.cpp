/*
  Q Light Controller Plus
  followspoteffect.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "followspoteffect.h"
#include "doc.h"
#include "fixture.h"
#include "qlcchannel.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "mastertimer.h"
#include "universe.h"
#include "scenevalue.h"

#include <QtMath>

FollowSpotEffect::FollowSpotEffect(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
}

FollowSpotEffect::~FollowSpotEffect()
{
    if (m_registered && m_doc && m_doc->masterTimer())
        m_doc->masterTimer()->unregisterDMXSource(this);
}

/*********************************************************************
 * Target fixtures
 *********************************************************************/

void FollowSpotEffect::setFixtures(const QList<quint32>& fixtureIds)
{
    QMutexLocker locker(&m_mutex);
    m_fixtures = fixtureIds;
    // Do NOT reset m_setpointValid here: once the operator has moved the stick
    // once, subsequent target selections should immediately follow the held
    // setpoint rather than requiring another stick move.
}

/*********************************************************************
 * Axis values
 *********************************************************************/

void FollowSpotEffect::setXNorm(float v)
{
    QMutexLocker locker(&m_mutex);
    m_xNorm = qBound(0.0f, v, 1.0f);
}

void FollowSpotEffect::setYNorm(float v)
{
    QMutexLocker locker(&m_mutex);
    m_yNorm = qBound(0.0f, v, 1.0f);
}

/*********************************************************************
 * Settings
 *********************************************************************/

void FollowSpotEffect::setSensitivity(float s)
{
    m_sensitivity = qBound(0.1f, s, 3.0f);
}

void FollowSpotEffect::setDeadzone(float d)
{
    m_deadzone = qBound(0.0f, d, 0.4f);
}

void FollowSpotEffect::setAimHeight(int cm)
{
    m_aimHeight = qBound(0, cm, 300);
}

/*********************************************************************
 * Setpoint access
 *********************************************************************/

void FollowSpotEffect::resetSetpoint()
{
    QMutexLocker l(&m_mutex);
    m_setpointValid = false;
}

/*********************************************************************
 * Enable / disable
 *********************************************************************/

void FollowSpotEffect::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;

    if (active)
    {
        m_setpointValid = false; // require one stick move to take control
        if (!m_registered && m_doc && m_doc->masterTimer())
        {
            m_doc->masterTimer()->registerDMXSource(this);
            m_registered = true;
        }
    }
    else
    {
        if (m_registered && m_doc && m_doc->masterTimer())
        {
            m_doc->masterTimer()->unregisterDMXSource(this);
            m_registered = false;
        }
    }

    emit activeChanged(active);
}

/*********************************************************************
 * DMXSource
 *********************************************************************/

void FollowSpotEffect::writeDMX(MasterTimer *timer, QList<Universe*> universes)
{
    QMutexLocker locker(&m_mutex);

    if (!m_active || m_fixtures.isEmpty())
        return;

    // Stick velocity: -1..1 centered at stick-center (0.5 normalized).
    const float xVel = (m_xNorm - 0.5f) * 2.0f;
    const float yVel = (m_yNorm - 0.5f) * 2.0f;

    // Apply deadzone to velocity (deadzone is fraction of full range each side).
    auto applyDZ = [this](float v) -> float {
        return (qAbs(v) < m_deadzone * 2.0f) ? 0.0f : v;
    };
    const float xV = applyDZ(xVel);
    const float yV = applyDZ(yVel);

    if (xV != 0.0f || yV != 0.0f)
    {
        // First stick move: initialize setpoint at center.
        if (!m_setpointValid)
        {
            m_panSetpoint  = 0.5f;
            m_tiltSetpoint = 0.5f;
            m_setpointValid = true;
        }
        // Move setpoint by velocity * speed. sensitivity=1 sweeps full range in 1s.
        const float dt = (timer->frequency() > 0)
                         ? 1.0f / (float)timer->frequency() : 0.02f;
        m_panSetpoint  = qBound(0.0f, m_panSetpoint  + xV * m_sensitivity * dt, 1.0f);
        m_tiltSetpoint = qBound(0.0f, m_tiltSetpoint + yV * m_sensitivity * dt, 1.0f);
    }

    // Don't write until the operator has moved the stick; let the scene control.
    if (!m_setpointValid)
        return;

    for (quint32 fid : m_fixtures)
    {
        Fixture *fxi = m_doc->fixture(fid);
        if (!fxi || !fxi->fixtureMode())
            continue;

        const QLCPhysical &phy = fxi->fixtureMode()->physical();

        float panMax  = (float)phy.focusPanMax();
        if (panMax  <= 0) panMax  = 360.0f;
        float tiltMax = (float)phy.focusTiltMax();
        if (tiltMax <= 0) tiltMax = 270.0f;

        float panDeg  = m_panSetpoint  * panMax;
        float tiltDeg = m_tiltSetpoint * tiltMax;

        // Aim-height bias: 5° per metre so beam hits head not floor.
        const float aimBiasRaw = (m_aimHeight / 100.0f) * 5.0f;
        tiltDeg -= aimBiasRaw;

        panDeg  = qBound(0.0f, panDeg,  panMax);
        tiltDeg = qBound(0.0f, tiltDeg, tiltMax);

        int uniIdx = (int)fxi->universe();
        if (uniIdx < 0 || uniIdx >= universes.size())
            continue;
        Universe *uni = universes.at(uniIdx);
        if (!uni)
            continue;

        const QList<SceneValue> panVals =
            fxi->positionToValues(QLCChannel::Pan, panDeg);
        for (const SceneValue &sv : panVals)
            uni->write((int)fxi->address() + (int)sv.channel, sv.value, /*forceLTP=*/true);

        const QList<SceneValue> tiltVals =
            fxi->positionToValues(QLCChannel::Tilt, tiltDeg);
        for (const SceneValue &sv : tiltVals)
            uni->write((int)fxi->address() + (int)sv.channel, sv.value, /*forceLTP=*/true);
    }
}
