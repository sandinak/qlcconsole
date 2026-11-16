/*
  Q Light Controller Plus
  hidrescan_test.cpp

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
#include <algorithm>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTimer>

#include "hidrescan_test.h"
#include "fakehidbackend.h"
#include "hidplugin.h"

namespace
{
/* The bench's pad after View + Menu: a DualSense clone that re-enumerated
   as an Xbox 360 pad, and that macOS then held "busy". */
const QString kClonePath = QStringLiteral("USB_045e_028e_2124000");
const QString kCloneName = QStringLiteral("Xbox 360 Controller for Windows");
const QString kOtherPath = QStringLiteral("USB_054c_0ce6_14200000");
const QString kOtherName = QStringLiteral("DualSense Wireless Controller");

/* The open limit here: short, so each test stays well under a second or
   two; the app's is 2 s. */
const int kLimitMs = 300;

/* Never deleted: a thread can still be on its way out of a call into one
   when a test ends, and the process is short. */
QList<FakeHIDBackend *> s_fakes;

int countOf(const QStringList &list, const QString &name)
{
    return int(std::count(list.begin(), list.end(), name));
}
}

void HIDRescan_Test::initTestCase()
{
    qputenv("QLC_HID_VIRTUAL_ONLY", "1");      // nothing of this machine's
    qputenv("QLC_HID_OPEN_LIMIT_MS", QByteArray::number(kLimitMs));
    qputenv("QLC_HID_PROFILE_DIR", QByteArray(QLC_TEST_RESOURCE_ROOT) + "hidprofiles");
}

void HIDRescan_Test::init()
{
    m_fake = new FakeHIDBackend();
    s_fakes << m_fake;
    m_plugin = new HIDPlugin();
    m_plugin->init();
    QCOMPARE(m_plugin->inputs(), QStringList());
}

void HIDRescan_Test::cleanup()
{
    releaseFake();
    delete m_plugin;
    m_plugin = nullptr;
    QTRY_COMPARE(m_fake->callsInProgress(), 0);
    m_fake = nullptr;
}

void HIDRescan_Test::releaseFake()
{
    m_fake->release();
    QTRY_COMPARE_WITH_TIMEOUT(m_fake->callsInProgress(), 0, 2000);
    if (m_plugin != nullptr)
        QTRY_COMPARE_WITH_TIMEOUT(m_plugin->openingCount(), 0, 2000);
}

/****************************************************************************
 * A pad that stops answering
 ****************************************************************************/

void HIDRescan_Test::aStuckOpenNeverHoldsTheGui()
{
    m_fake->list(kClonePath, 0x045e, 0x028e, kCloneName, FakeHIDBackend::Forever);
    m_fake->list(kOtherPath, 0x054c, 0x0ce6, kOtherName);
    QSignalSpy problems(m_plugin, SIGNAL(targetProblemsChanged()));

    /* A timer on the GUI thread, every 20 ms: how late it ever comes. */
    QElapsedTimer clock;
    clock.start();
    qint64 last = -1, worst = 0;
    QTimer beat;
    beat.setInterval(20);
    connect(&beat, &QTimer::timeout, this, [&]() {
        const qint64 now = clock.elapsed();
        if (last >= 0)
            worst = qMax(worst, now - last);
        last = now;
    });
    beat.start();

    QElapsedTimer call;
    call.start();
    m_plugin->setTestBackend(quintptr(m_fake));
    m_plugin->rescanDevices();
    m_plugin->rescanDevices();
    QVERIFY2(call.elapsed() < 50, qPrintable(QString("asking for a scan took %1 ms").arg(call.elapsed())));

    // The other pad is there, as if nothing were wrong.
    QTRY_VERIFY_WITH_TIMEOUT(m_plugin->inputs().contains(kOtherName), 2000);

    // Past the limit, the stuck one is named for the footer -- and only it.
    QTRY_COMPARE_WITH_TIMEOUT(m_plugin->targetProblems().count(), 1, 3000);
    const QString said = m_plugin->targetProblems().first();
    QVERIFY2(said.startsWith(kCloneName + " (USB 2124000)"), qPrintable(said));
    QVERIFY2(said.contains("not answering"), qPrintable(said));
    QVERIFY2(said.contains("the show keeps running"), qPrintable(said));
    QVERIFY(problems.count() >= 1);
    QVERIFY(m_plugin->inputs().contains(kCloneName) == false);
    QCOMPARE(m_plugin->openingCount(), 1);

    QTest::qWait(300);      // and stuck a while longer
    beat.stop();
    QVERIFY2(worst < 100, qPrintable(QString("the GUI thread stalled %1 ms").arg(worst)));

    // The system lets go: the pad comes after all, and is no longer named.
    releaseFake();
    QTRY_VERIFY_WITH_TIMEOUT(m_plugin->inputs().contains(kCloneName), 2000);
    QTRY_VERIFY(m_plugin->targetProblems().isEmpty());
    QCOMPARE(countOf(m_plugin->inputs(), kOtherName), 1);
}

void HIDRescan_Test::anOpenThatTakesAWhileArrivesLate()
{
    m_fake->list(kClonePath, 0x045e, 0x028e, kCloneName, kLimitMs * 2);
    m_plugin->setTestBackend(quintptr(m_fake));
    QTRY_COMPARE_WITH_TIMEOUT(m_plugin->targetProblems().count(), 1, 2000);
    QVERIFY(m_plugin->inputs().isEmpty());
    // It answers at last: listed, not named.
    QTRY_VERIFY_WITH_TIMEOUT(m_plugin->inputs().contains(kCloneName), 3000);
    QTRY_VERIFY(m_plugin->targetProblems().isEmpty());
    QCOMPARE(m_fake->opens(kClonePath), 1);
}

void HIDRescan_Test::aPadThatGoesMidOpenIsNotAdded()
{
    /* The clone dropping off USB while it is opened: the open comes back
       later with a device for an entry the system no longer lists. */
    m_fake->list(kClonePath, 0x045e, 0x028e, kCloneName, FakeHIDBackend::Forever);
    m_plugin->setTestBackend(quintptr(m_fake));
    QTRY_COMPARE_WITH_TIMEOUT(m_plugin->targetProblems().count(), 1, 2000);

    m_fake->unlist(kClonePath);
    m_plugin->rescanDevices();
    // Gone: nothing to name, nothing the operator can do about it.
    QTRY_VERIFY_WITH_TIMEOUT(m_plugin->targetProblems().isEmpty(), 2000);

    releaseFake();
    QTest::qWait(50);
    QVERIFY2(m_plugin->inputs().isEmpty(), qPrintable(m_plugin->inputs().join(", ")));
}

void HIDRescan_Test::aPadBackAtTheSamePathReplacesTheOld()
{
    /* Re-enumerated at the same port: another system entry at the same
       path, whose open hangs. The old one reads nothing: it goes once the
       new one is past the limit, not left patched and silent. */
    m_fake->list(kClonePath, 0x045e, 0x028e, kCloneName, 0, 1);
    m_plugin->setTestBackend(quintptr(m_fake));
    QTRY_VERIFY_WITH_TIMEOUT(m_plugin->inputs().contains(kCloneName), 2000);

    m_fake->unlist(kClonePath);
    m_fake->list(kClonePath, 0x045e, 0x028e, kCloneName, FakeHIDBackend::Forever, 2);
    QSignalSpy config(m_plugin, SIGNAL(configurationChanged()));
    m_plugin->rescanDevices();
    QTRY_COMPARE_WITH_TIMEOUT(m_fake->opens(kClonePath), 2, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(m_plugin->inputs().contains(kCloneName) == false, 2000);
    QVERIFY(config.count() >= 1);
    QCOMPARE(m_plugin->targetProblems().count(), 1);

    releaseFake();
    QTRY_VERIFY_WITH_TIMEOUT(m_plugin->inputs().contains(kCloneName), 2000);
    QCOMPARE(countOf(m_plugin->inputs(), kCloneName), 1);
    QTRY_VERIFY(m_plugin->targetProblems().isEmpty());
}

void HIDRescan_Test::aPadListedTwiceIsOneDevice()
{
    m_fake->list(kOtherPath, 0x054c, 0x0ce6, kOtherName);
    m_fake->list(kOtherPath, 0x054c, 0x0ce6, kOtherName);
    m_plugin->setTestBackend(quintptr(m_fake));
    QTRY_VERIFY_WITH_TIMEOUT(m_plugin->inputs().contains(kOtherName), 2000);
    m_plugin->rescanDevices();
    QTRY_VERIFY(m_plugin->isScanning() == false);
    QTest::qWait(50);
    QCOMPARE(countOf(m_plugin->inputs(), kOtherName), 1);
    QCOMPARE(m_fake->opens(kOtherPath), 1);
}

void HIDRescan_Test::oneOpenPerPathHoweverOftenItRescans()
{
    m_fake->list(kClonePath, 0x045e, 0x028e, kCloneName, FakeHIDBackend::Forever);
    m_plugin->setTestBackend(quintptr(m_fake));
    for (int i = 0; i < 10; ++i)
    {
        m_plugin->rescanDevices();
        QTest::qWait(40);
    }
    QTRY_VERIFY(m_plugin->isScanning() == false);
    QTest::qWait(kLimitMs);
    QCOMPARE(m_fake->opens(kClonePath), 1);
    QCOMPARE(m_plugin->openingCount(), 1);
    QCOMPARE(m_fake->callsInProgress(), 1);
}

void HIDRescan_Test::oneScanAtATime()
{
    /* The system's list itself hangs: asked again and again, one scan --
       and it is named. */
    m_fake->list(kOtherPath, 0x054c, 0x0ce6, kOtherName);
    m_fake->setEnumerateBlocks(true);
    QElapsedTimer call;
    call.start();
    m_plugin->setTestBackend(quintptr(m_fake));
    for (int i = 0; i < 5; ++i)
        m_plugin->rescanDevices();
    QVERIFY2(call.elapsed() < 50, qPrintable(QString("asking took %1 ms").arg(call.elapsed())));
    QTRY_COMPARE_WITH_TIMEOUT(m_plugin->targetProblems().count(), 1, 2000);
    QVERIFY2(m_plugin->targetProblems().first().contains("device list is not answering"),
             qPrintable(m_plugin->targetProblems().first()));
    QCOMPARE(m_fake->enumerations(), 1);
    QVERIFY(m_plugin->isScanning());

    releaseFake();
    QTRY_VERIFY_WITH_TIMEOUT(m_plugin->inputs().contains(kOtherName), 2000);
    QTRY_VERIFY(m_plugin->isScanning() == false);
    QTRY_VERIFY(m_plugin->targetProblems().isEmpty());
    // The ones asked meanwhile: one more scan after it, not five.
    QVERIFY2(m_fake->enumerations() <= 2, qPrintable(QString::number(m_fake->enumerations())));
    QCOMPARE(m_fake->maxConcurrentEnumerations(), 1);
}

void HIDRescan_Test::thePluginGoesWithAnOpenStuck()
{
    m_fake->list(kClonePath, 0x045e, 0x028e, kCloneName, FakeHIDBackend::Forever);
    m_fake->list(kOtherPath, 0x054c, 0x0ce6, kOtherName);
    m_plugin->setTestBackend(quintptr(m_fake));
    QTRY_VERIFY_WITH_TIMEOUT(m_plugin->inputs().contains(kOtherName), 2000);
    QVERIFY(m_plugin->openInput(0, 0));      // its reader running too
    QCOMPARE(m_fake->callsInProgress(), 1);

    QElapsedTimer t;
    t.start();
    delete m_plugin;    // closing the show, quitting the app
    m_plugin = nullptr;
    QVERIFY2(t.elapsed() < 1000, qPrintable(QString("the plugin took %1 ms to go").arg(t.elapsed())));
    // The stuck thread finds the plugin gone and drops what it made.
}

QTEST_MAIN(HIDRescan_Test)
