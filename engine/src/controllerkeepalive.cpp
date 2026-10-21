/*
  Q Light Controller Plus
  controllerkeepalive.cpp

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

#include <QPointer>
#include <QTimer>
#include <QDebug>

#include <functional>

#include "controllerkeepalive.h"

#if defined(Q_OS_MACOS)
/* controllerkeepalive_mac.mm */
namespace KeepAlivePlatform
{
    void *beginActivity(const QString &reason);
    void endActivity(void *token);
    int inputMonitoring();
    void requestInputMonitoring();
    void *observe(std::function<void(int)> changed);
    void unobserve(void *observer);
}
#endif

namespace
{
#if defined(Q_OS_MACOS)
class MacBackend final : public ControllerKeepAlive::Backend
{
public:
    ~MacBackend() override { endActivity(); }
    void beginActivity(const QString &reason) override
    {
        if (m_token == nullptr)
            m_token = KeepAlivePlatform::beginActivity(reason);
    }
    void endActivity() override
    {
        if (m_token != nullptr)
            KeepAlivePlatform::endActivity(m_token);
        m_token = nullptr;
    }
    ControllerKeepAlive::Permission inputMonitoring() const override
    {
        return ControllerKeepAlive::Permission(KeepAlivePlatform::inputMonitoring());
    }
    void requestInputMonitoring() override { KeepAlivePlatform::requestInputMonitoring(); }

private:
    void *m_token = nullptr;
};
#else
class NullBackend final : public ControllerKeepAlive::Backend
{
public:
    void beginActivity(const QString &) override {}
    void endActivity() override {}
    ControllerKeepAlive::Permission inputMonitoring() const override
    {
        return ControllerKeepAlive::PermissionNotNeeded;
    }
};
#endif
}

ControllerKeepAlive::ControllerKeepAlive(QObject *parent)
    : QObject(parent)
{
#if defined(Q_OS_MACOS)
    m_backend = new MacBackend;
    QPointer<ControllerKeepAlive> self(this);
    m_observer = KeepAlivePlatform::observe([self](int state) {
        if (!self)
            return;
        if (state < 0)      // unlocked
            self->screenUnlocked();
        else
            self->setSystemState(SystemState(state));
    });
#else
    m_backend = new NullBackend;
#endif
    m_wakeTimer = new QTimer(this);
    m_wakeTimer->setSingleShot(true);
    m_wakeTimer->setInterval(20000);
    connect(m_wakeTimer, &QTimer::timeout, this, [this]() {
        // No input since the wake, but nothing says it is lost either.
        if (m_system == Waking)
            setSystemState(Awake);
    });
}

ControllerKeepAlive::~ControllerKeepAlive()
{
#if defined(Q_OS_MACOS)
    KeepAlivePlatform::unobserve(m_observer);
#endif
    if (m_held)
        m_backend->endActivity();
    delete m_backend;
}

void ControllerKeepAlive::setBackend(Backend *backend)
{
    if (backend == nullptr || backend == m_backend)
        return;
    if (m_held)
        m_backend->endActivity();
    delete m_backend;
    m_backend = backend;
    if (m_held)
        m_backend->beginActivity(QStringLiteral("A game controller is driving the rig"));
}

/*****************************************************************************
 * Who wants the controller
 *****************************************************************************/

void ControllerKeepAlive::setFocusActive(bool active)
{
    if (m_focus == active)
        return;
    m_focus = active;
    updateHeld();
    updatePaused();
}

void ControllerKeepAlive::setFollowSpotActive(bool active)
{
    if (m_followSpot == active)
        return;
    m_followSpot = active;
    updateHeld();
    updatePaused();
}

void ControllerKeepAlive::updateHeld()
{
    const bool want = isWanted();
    if (want == m_held)
        return;
    m_held = want;
    if (want)
        m_backend->beginActivity(m_focus ? QStringLiteral("Focus mode is reading a game controller")
                                         : QStringLiteral("The follow spot is reading a game controller"));
    else
        m_backend->endActivity();
    emit heldChanged(m_held);
}

/*****************************************************************************
 * The controller's state
 *****************************************************************************/

void ControllerKeepAlive::setController(bool present, const QString &problem)
{
    if (present == m_present && problem == m_problem)
        return;
    m_present = present;
    m_problem = problem;
    updatePaused();
}

void ControllerKeepAlive::noteInput()
{
    if (m_system == Waking)
        setSystemState(Awake);
}

void ControllerKeepAlive::setSystemState(SystemState state)
{
    if (state == m_system)
        return;
    m_system = state;
    if (state == Waking)
        m_wakeTimer->start();
    else
        m_wakeTimer->stop();
    updatePaused();
}

void ControllerKeepAlive::screenUnlocked()
{
    if (m_system == ScreenLocked)
        setSystemState(Awake);
}

void ControllerKeepAlive::setWakeGrace(int ms)
{
    m_wakeTimer->setInterval(qMax(0, ms));
}

void ControllerKeepAlive::updatePaused()
{
    QString reason;
    if (isWanted())
    {
        if (m_system == Asleep)
            reason = tr("the Mac is asleep");
        else if (!m_problem.isEmpty())
            reason = m_problem;
        else if (!m_present)
            reason = tr("no controller is connected. Switch it on (hold the Xbox button) "
                        "or plug it in.");
        else if (m_system == Waking)
            reason = tr("finding the controller again after the Mac slept. "
                        "Press a button on it.");
        /* Locked: a controller is still read behind the lock screen, so it
           is not paused -- nothing is said that is not known. */
    }
    if (reason == m_reason)
        return;
    m_reason = reason;
    emit pausedChanged(!m_reason.isEmpty(), m_reason);
}

ControllerKeepAlive::Permission ControllerKeepAlive::inputMonitoring() const
{
    return m_backend->inputMonitoring();
}

void ControllerKeepAlive::requestInputMonitoring()
{
    m_backend->requestInputMonitoring();
}

QString ControllerKeepAlive::permissionHint() const
{
    switch (inputMonitoring())
    {
        case PermissionDenied:
            return tr("Input Monitoring is off for qlcconsole. A game controller does not "
                      "normally need it, but if yours stops while another app is in front, "
                      "turn qlcconsole on in System Settings \342\206\222 Privacy & Security "
                      "\342\206\222 Input Monitoring, then quit and reopen it.");
        case PermissionUnknown:
            return tr("macOS has not been asked about Input Monitoring for qlcconsole. A game "
                      "controller does not normally need it; if yours stops while another app "
                      "is in front, ask for it here.");
        case PermissionGranted:
        case PermissionNotNeeded:
            break;
    }
    return QString();
}
