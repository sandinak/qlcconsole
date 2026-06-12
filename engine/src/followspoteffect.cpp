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
#include "inputoutputmap.h"
#include "scenevalue.h"

#include <QtMath>

FollowSpotEffect::FollowSpotEffect(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
}

FollowSpotEffect::~FollowSpotEffect()
{
    disconnectInput();
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
}

/*********************************************************************
 * Input binding
 *********************************************************************/

void FollowSpotEffect::startBindX()
{
    connectInput();
    m_capturingX = true;
    m_capturingY = false;
}

void FollowSpotEffect::startBindY()
{
    connectInput();
    m_capturingX = false;
    m_capturingY = true;
}

void FollowSpotEffect::clearBindings()
{
    m_xBound = false;
    m_yBound = false;
    m_capturingX = false;
    m_capturingY = false;
    emit bindingChanged();
}

bool FollowSpotEffect::isXBound() const
{
    return m_xBound;
}

bool FollowSpotEffect::isYBound() const
{
    return m_yBound;
}

QString FollowSpotEffect::xBindingLabel() const
{
    if (!m_xBound)
        return tr("—");
    return tr("Universe %1, Ch %2").arg(m_xUniverse + 1).arg(m_xChannel + 1);
}

QString FollowSpotEffect::yBindingLabel() const
{
    if (!m_yBound)
        return tr("—");
    return tr("Universe %1, Ch %2").arg(m_yUniverse + 1).arg(m_yChannel + 1);
}

void FollowSpotEffect::connectInput()
{
    if (m_inputConnected || !m_doc || !m_doc->inputOutputMap())
        return;
    connect(m_doc->inputOutputMap(),
            SIGNAL(inputValueChanged(quint32, quint32, uchar, const QString&)),
            this,
            SLOT(slotInputValueChanged(quint32, quint32, uchar, const QString&)));
    m_inputConnected = true;
}

void FollowSpotEffect::disconnectInput()
{
    if (!m_inputConnected || !m_doc || !m_doc->inputOutputMap())
        return;
    disconnect(m_doc->inputOutputMap(),
               SIGNAL(inputValueChanged(quint32, quint32, uchar, const QString&)),
               this,
               SLOT(slotInputValueChanged(quint32, quint32, uchar, const QString&)));
    m_inputConnected = false;
}

void FollowSpotEffect::slotInputValueChanged(quint32 universe, quint32 channel,
                                              uchar value, const QString& key)
{
    Q_UNUSED(key)

    if (m_capturingX)
    {
        m_xUniverse = universe;
        m_xChannel  = channel;
        m_xBound    = true;
        m_capturingX = false;
        emit bindingChanged();
        return;
    }
    if (m_capturingY)
    {
        m_yUniverse = universe;
        m_yChannel  = channel;
        m_yBound    = true;
        m_capturingY = false;
        emit bindingChanged();
        return;
    }

    if (m_xBound && universe == m_xUniverse && channel == m_xChannel)
    {
        QMutexLocker locker(&m_mutex);
        m_xNorm = value / 255.0f;
    }
    else if (m_yBound && universe == m_yUniverse && channel == m_yChannel)
    {
        QMutexLocker locker(&m_mutex);
        m_yNorm = value / 255.0f;
    }
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
        connectInput();
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
    Q_UNUSED(timer)

    QMutexLocker locker(&m_mutex);

    if (!m_active || m_fixtures.isEmpty())
        return;

    // Apply deadzone: values within ±deadzone of centre are snapped to centre.
    auto applyDZ = [this](float norm) -> float {
        float centered = norm - 0.5f;   // -0.5..+0.5
        float dz = m_deadzone * 0.5f;   // half-range deadzone
        if (qAbs(centered) < dz)
            centered = 0.0f;
        return centered + 0.5f;         // back to 0..1
    };

    const float xNorm = applyDZ(m_xNorm);
    const float yNorm = applyDZ(m_yNorm);

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

        float panDeg  = xNorm * panMax  * m_sensitivity;
        float tiltDeg = yNorm * tiltMax * m_sensitivity;
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
            uni->write((int)fxi->address() + (int)sv.channel, sv.value,
                       /*forceLTP=*/true);

        const QList<SceneValue> tiltVals =
            fxi->positionToValues(QLCChannel::Tilt, tiltDeg);
        for (const SceneValue &sv : tiltVals)
            uni->write((int)fxi->address() + (int)sv.channel, sv.value,
                       /*forceLTP=*/true);
    }
}
