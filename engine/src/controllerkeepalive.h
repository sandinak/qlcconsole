/*
  Q Light Controller Plus
  controllerkeepalive.h

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

#ifndef CONTROLLERKEEPALIVE_H
#define CONTROLLERKEEPALIVE_H

#include <QObject>
#include <QString>

#include "qlcengine_export.h"

class QTimer;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Keeps a game controller working while Focus mode or the follow spot uses
 * it, whatever window is in front -- and says so when it is not.
 *
 * While either is on, it holds an activity with the system (macOS:
 * NSProcessInfo beginActivityWithOptions: NSActivityUserInitiated |
 * NSActivityLatencyCritical). Without one, a Mac puts an app that is behind
 * other windows, or whose screen has slept, into App Nap: its timers -- the
 * 50 Hz joystick tick among them -- are coalesced to seconds apart, and a
 * held stick all but stops moving the head. UserInitiated also keeps the Mac
 * itself from idling to sleep under a focus session (the display may still
 * sleep). It lets go when both are off.
 *
 * It also works out whether the controller is PAUSED and why -- the Mac is
 * asleep, the controller has gone, macOS refuses access to it, or it is
 * being found again after a sleep -- for Focus mode's panel to say instead
 * of going quiet. Only what is known is reported: a controller that simply
 * is not being touched is not "paused".
 *
 * Platform glue (the activity, the sleep/wake/lock notifications, the Input
 * Monitoring permission) sits behind Backend, which a test replaces.
 */
class QLC_ENGINE_EXPORT ControllerKeepAlive final : public QObject
{
    Q_OBJECT

public:
    /** macOS's Input Monitoring permission (IOHIDCheckAccess). */
    enum Permission { PermissionNotNeeded, PermissionUnknown, PermissionGranted, PermissionDenied };

    /** What the system is doing, as far as the controller is concerned. */
    enum SystemState
    {
        Awake,
        Asleep,         ///< the Mac is going to sleep: nothing is read
        Waking,         ///< it has woken: the controller is being found again
        ScreenLocked    ///< locked, or another user's session is in front
    };

    class Backend
    {
    public:
        virtual ~Backend() {}
        virtual void beginActivity(const QString &reason) = 0;
        virtual void endActivity() = 0;
        virtual Permission inputMonitoring() const = 0;
        virtual void requestInputMonitoring() {}
    };

    /** With the platform's backend (a do-nothing one off macOS), watching
     *  sleep, wake and the screen lock. */
    explicit ControllerKeepAlive(QObject *parent = nullptr);
    ~ControllerKeepAlive() override;

    /** Replace the backend (a test's); takes ownership. A held activity
     *  moves to the new one. */
    void setBackend(Backend *backend);

    /*********************************************************************
     * Who wants the controller
     *********************************************************************/
public:
    void setFocusActive(bool active);
    void setFollowSpotActive(bool active);
    bool isWanted() const { return m_focus || m_followSpot; }
    /** The activity is held. */
    bool isHeld() const { return m_held; }

    /*********************************************************************
     * The controller's state
     *********************************************************************/
public:
    /** Whether a controller Focus mode / the follow spot can use is
     *  connected, and, if the plugin knows input from it is blocked, why
     *  (QLCIOPlugin::inputProblem()). */
    void setController(bool present, const QString &problem = QString());
    /** Input came from a controller: whatever was being waited for is
     *  over. */
    void noteInput();
    void setSystemState(SystemState state);
    SystemState systemState() const { return m_system; }
    /** The screen was unlocked: that ends a lock, not a wake -- the Mac
     *  usually sleeps behind the lock, and is then still finding the pad. */
    void screenUnlocked();

    /** How long after a wake the controller is "being found again" without
     *  any input before that is no longer said (ms). */
    void setWakeGrace(int ms);

    bool isPaused() const { return !m_reason.isEmpty(); }
    /** Why, for "Controller paused: <reason>"; empty when it is not. */
    QString pauseReason() const { return m_reason; }

    /** macOS's Input Monitoring permission for this app. */
    Permission inputMonitoring() const;
    /** Ask macOS for it (shows the system's prompt, once). */
    void requestInputMonitoring();
    /** What to tell the user about it, when it might matter; empty when
     *  not. */
    QString permissionHint() const;

signals:
    void heldChanged(bool held);
    void pausedChanged(bool paused, const QString &reason);

private:
    void updateHeld();
    void updatePaused();

private:
    Backend *m_backend = nullptr;
    void *m_observer = nullptr;   ///< the platform's notification observers
    bool m_focus = false;
    bool m_followSpot = false;
    bool m_held = false;
    bool m_present = false;
    QString m_problem;
    SystemState m_system = Awake;
    QTimer *m_wakeTimer = nullptr;
    QString m_reason;
};

/** @} */

#endif
