/*
  Q Light Controller Plus - Unit test
  controllerkeepalive_test.cpp

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

#include <QtTest>
#include <QSignalSpy>

#include "controllerkeepalive_test.h"
#include "controllerkeepalive.h"

namespace
{
/** Counts what the platform would be asked to do. */
class FakeBackend final : public ControllerKeepAlive::Backend
{
public:
    FakeBackend(int *begun, int *ended, int *held)
        : m_begun(begun), m_ended(ended), m_held(held) {}
    void beginActivity(const QString &reason) override
    {
        ++*m_begun;
        ++*m_held;
        m_reason = reason;
    }
    void endActivity() override
    {
        ++*m_ended;
        --*m_held;
    }
    ControllerKeepAlive::Permission inputMonitoring() const override { return m_permission; }
    void requestInputMonitoring() override { m_permission = ControllerKeepAlive::PermissionGranted; }

    ControllerKeepAlive::Permission m_permission = ControllerKeepAlive::PermissionGranted;
    QString m_reason;

private:
    int *m_begun;
    int *m_ended;
    int *m_held;
};
}

void ControllerKeepAlive_Test::theActivityFollowsWhoWantsIt()
{
    int begun = 0, ended = 0, held = 0;
    ControllerKeepAlive keep;
    keep.setBackend(new FakeBackend(&begun, &ended, &held));
    QSignalSpy heldSpy(&keep, &ControllerKeepAlive::heldChanged);

    QVERIFY(!keep.isHeld());
    QCOMPARE(begun, 0);

    // Focus mode starts: taken.
    keep.setFocusActive(true);
    QVERIFY(keep.isHeld());
    QCOMPARE(begun, 1);
    QCOMPARE(held, 1);
    QCOMPARE(heldSpy.count(), 1);

    // The follow spot as well: still the one activity.
    keep.setFollowSpotActive(true);
    QCOMPARE(begun, 1);
    QCOMPARE(held, 1);

    // Focus mode stops; the follow spot still has it.
    keep.setFocusActive(false);
    QVERIFY(keep.isHeld());
    QCOMPARE(ended, 0);

    // Both off: let go.
    keep.setFollowSpotActive(false);
    QVERIFY(!keep.isHeld());
    QCOMPARE(ended, 1);
    QCOMPARE(held, 0);
    QCOMPARE(heldSpy.count(), 2);

    // Saying it again changes nothing.
    keep.setFollowSpotActive(false);
    QCOMPARE(ended, 1);

    // Held when it goes: let go then.
    keep.setFocusActive(true);
    QCOMPARE(held, 1);
}

void ControllerKeepAlive_Test::aNewBackendTakesItOver()
{
    int begunA = 0, endedA = 0, heldA = 0;
    int begunB = 0, endedB = 0, heldB = 0;
    {
        ControllerKeepAlive keep;
        keep.setBackend(new FakeBackend(&begunA, &endedA, &heldA));
        keep.setFocusActive(true);
        QCOMPARE(heldA, 1);
        keep.setBackend(new FakeBackend(&begunB, &endedB, &heldB));
        QCOMPARE(heldA, 0);
        QCOMPARE(heldB, 1);
    }
    // Destroyed while held: released.
    QCOMPARE(heldB, 0);
    QCOMPARE(endedB, 1);
}

void ControllerKeepAlive_Test::pausedSaysWhy()
{
    int begun = 0, ended = 0, held = 0;
    ControllerKeepAlive keep;
    keep.setBackend(new FakeBackend(&begun, &ended, &held));
    QSignalSpy paused(&keep, &ControllerKeepAlive::pausedChanged);

    // Nobody wants the controller: nothing to say, whatever its state.
    keep.setController(false);
    QVERIFY(!keep.isPaused());
    QCOMPARE(paused.count(), 0);

    // Wanted, and none connected.
    keep.setFocusActive(true);
    QVERIFY(keep.isPaused());
    QVERIFY2(keep.pauseReason().contains("no controller"), qPrintable(keep.pauseReason()));
    QCOMPARE(paused.count(), 1);
    QCOMPARE(paused.last().at(0).toBool(), true);

    // Connected: fine.
    keep.setController(true);
    QVERIFY(!keep.isPaused());
    QCOMPARE(paused.last().at(0).toBool(), false);
    QVERIFY(paused.last().at(1).toString().isEmpty());

    // The plugin knows it is blocked: its reason, word for word.
    const QString why = "macOS is not letting qlcconsole read the controller.";
    keep.setController(true, why);
    QVERIFY(keep.isPaused());
    QCOMPARE(keep.pauseReason(), why);
    QCOMPARE(paused.last().at(1).toString(), why);
    keep.setController(true);
    QVERIFY(!keep.isPaused());

    // Asleep.
    keep.setSystemState(ControllerKeepAlive::Asleep);
    QVERIFY(keep.isPaused());
    QVERIFY2(keep.pauseReason().contains("asleep"), qPrintable(keep.pauseReason()));

    // Locked: still read behind the lock, so not paused.
    keep.setSystemState(ControllerKeepAlive::ScreenLocked);
    QVERIFY(!keep.isPaused());

    // Unwanted again: nothing said, whatever is wrong.
    keep.setController(false);
    QVERIFY(keep.isPaused());
    keep.setFocusActive(false);
    QVERIFY(!keep.isPaused());
}

void ControllerKeepAlive_Test::wakingEndsWithInputOrTime()
{
    int begun = 0, ended = 0, held = 0;
    ControllerKeepAlive keep;
    keep.setBackend(new FakeBackend(&begun, &ended, &held));
    keep.setFollowSpotActive(true);
    keep.setController(true);

    // Woken: being found again, until something comes in from it.
    keep.setSystemState(ControllerKeepAlive::Waking);
    QVERIFY(keep.isPaused());
    QVERIFY2(keep.pauseReason().contains("after the Mac slept"), qPrintable(keep.pauseReason()));
    keep.noteInput();
    QVERIFY(!keep.isPaused());
    QCOMPARE(keep.systemState(), ControllerKeepAlive::Awake);

    // Or until the grace runs out, with nothing to say it is lost.
    keep.setWakeGrace(50);
    keep.setSystemState(ControllerKeepAlive::Waking);
    QVERIFY(keep.isPaused());
    QTRY_VERIFY(!keep.isPaused());
    QCOMPARE(keep.systemState(), ControllerKeepAlive::Awake);

    // Input with nothing waited for changes nothing.
    keep.noteInput();
    QCOMPARE(keep.systemState(), ControllerKeepAlive::Awake);
}

void ControllerKeepAlive_Test::unlockingIsNotWaking()
{
    ControllerKeepAlive keep;
    int begun = 0, ended = 0, held = 0;
    keep.setBackend(new FakeBackend(&begun, &ended, &held));
    keep.setFocusActive(true);
    keep.setController(true);

    keep.setSystemState(ControllerKeepAlive::ScreenLocked);
    keep.screenUnlocked();
    QCOMPARE(keep.systemState(), ControllerKeepAlive::Awake);

    // Slept behind the lock: unlocking does not say the pad is back.
    keep.setSystemState(ControllerKeepAlive::ScreenLocked);
    keep.setSystemState(ControllerKeepAlive::Asleep);
    keep.setSystemState(ControllerKeepAlive::Waking);
    keep.screenUnlocked();
    QCOMPARE(keep.systemState(), ControllerKeepAlive::Waking);
    QVERIFY(keep.isPaused());
}

void ControllerKeepAlive_Test::thePermissionHint()
{
    int begun = 0, ended = 0, held = 0;
    ControllerKeepAlive keep;
    FakeBackend *backend = new FakeBackend(&begun, &ended, &held);
    keep.setBackend(backend);

    backend->m_permission = ControllerKeepAlive::PermissionGranted;
    QVERIFY(keep.permissionHint().isEmpty());
    backend->m_permission = ControllerKeepAlive::PermissionNotNeeded;
    QVERIFY(keep.permissionHint().isEmpty());

    backend->m_permission = ControllerKeepAlive::PermissionDenied;
    QVERIFY2(keep.permissionHint().contains("Input Monitoring"), qPrintable(keep.permissionHint()));
    QVERIFY(keep.permissionHint().contains("Privacy & Security"));

    backend->m_permission = ControllerKeepAlive::PermissionUnknown;
    QVERIFY(!keep.permissionHint().isEmpty());
    keep.requestInputMonitoring();
    QCOMPARE(keep.inputMonitoring(), ControllerKeepAlive::PermissionGranted);
    QVERIFY(keep.permissionHint().isEmpty());
}

QTEST_GUILESS_MAIN(ControllerKeepAlive_Test)
