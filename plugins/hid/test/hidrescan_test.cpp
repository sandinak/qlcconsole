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
    // Every scan, open and dispose thread out, before the next test or exit.
    QTRY_COMPARE_WITH_TIMEOUT(HIDPlugin::workerThreads(), 0, 5000);
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
    /* The fake never returns: a rescan that waited on it would not return
       at all. A loaded host's scheduling is not what is measured here. */
    QVERIFY2(call.elapsed() < 1000, qPrintable(QString("asking for a scan took %1 ms").arg(call.elapsed())));

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
    /* Held by the open, the GUI would be stalled for good (the fake never
       returns), at the least for the open limit: well under that. */
    QVERIFY2(worst < 1000, qPrintable(QString("the GUI thread stalled %1 ms").arg(worst)));

    // The system lets go: the pad comes after all, and is no longer named.
    releaseFake();
    QTRY_VERIFY_WITH_TIMEOUT(m_plugin->inputs().contains(kCloneName), 2000);
    QTRY_VERIFY(m_plugin->targetProblems().isEmpty());
    QCOMPARE(countOf(m_plugin->inputs(), kOtherName), 1);
}

void HIDRescan_Test::anOpenThatTakesAWhileArrivesLate()
{
    /* Past the limit, then the system answers (released, not timed). */
    m_fake->list(kClonePath, 0x045e, 0x028e, kCloneName, FakeHIDBackend::Forever);
    m_plugin->setTestBackend(quintptr(m_fake));
    QTRY_COMPARE_WITH_TIMEOUT(m_plugin->targetProblems().count(), 1, 3000);
    QVERIFY(m_plugin->inputs().isEmpty());
    m_fake->release();
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

void HIDRescan_Test::aPadReplugedAtTheSamePathOpens()
{
    /* A pad whose open stuck leaves the list, then the system offers one
       at the same port again -- the same path (macOS: transport_vid_pid_
       location). The stuck open is given up once the pad leaves the list;
       the one back is opened afresh, while that one stays stuck. (On the
       bench the system never offered it again until a restart: this is
       for when it does, not the fix the footer promises.) */
    m_fake->list(kClonePath, 0x045e, 0x028e, kCloneName, FakeHIDBackend::Forever);
    m_plugin->setTestBackend(quintptr(m_fake));
    QTRY_COMPARE_WITH_TIMEOUT(m_plugin->targetProblems().count(), 1, 3000);

    m_fake->unlist(kClonePath);             // unplugged
    m_plugin->rescanDevices();
    QTRY_VERIFY_WITH_TIMEOUT(m_plugin->targetProblems().isEmpty(), 3000);
    QTRY_VERIFY(m_plugin->isScanning() == false);

    m_fake->list(kClonePath, 0x045e, 0x028e, kCloneName);   // back, answering
    m_plugin->rescanDevices();
    QTRY_VERIFY_WITH_TIMEOUT(m_plugin->inputs().contains(kCloneName), 3000);
    QCOMPARE(m_fake->opens(kClonePath), 2);
    QCOMPARE(m_fake->callsInProgress(), 1);     // the first, still stuck
    QVERIFY(m_plugin->targetProblems().isEmpty());

    // The first one returns at last: what it made is dropped.
    releaseFake();
    QTest::qWait(50);
    QCOMPARE(countOf(m_plugin->inputs(), kCloneName), 1);
    QVERIFY(m_plugin->targetProblems().isEmpty());
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

void HIDRescan_Test::everyChangedLineIsToldOnce()
{
    /* inputReopened(line) (third review of Q253): once for each line whose
       device is another than before -- replaced where it was, or shifted
       onto it when a device before it went (Linux pads with no location) --
       with the line as it is now; never for a line that kept its device,
       nor for one new at the end. */
    const QString a = QStringLiteral("/dev/input/js0"), b = QStringLiteral("/dev/input/js1"),
                  c = QStringLiteral("/dev/input/js2");
    const QString name = QStringLiteral("Xbox Wireless Controller");
    m_plugin->setTestBackend(quintptr(m_fake));
    QSignalSpy told(m_plugin, SIGNAL(inputReopened(quint32)));
    auto settle = [&](int count) {
        m_plugin->rescanDevices();
        QTRY_VERIFY_WITH_TIMEOUT(m_plugin->isScanning() == false && m_plugin->openingCount() == 0
                                     && countOf(m_plugin->inputs(), name) == count, 3000);
        QTest::qWait(30);
    };
    auto lines = [&]() {
        QList<quint32> out;
        for (const QList<QVariant> &args : std::as_const(told))
            out << args.at(0).toUInt();
        told.clear();
        return out;
    };
    // One by one: A on line 0, B on 1, C on 2 -- nothing reopened.
    m_fake->list(a, 0x045e, 0x0b13, name, 0, 1);
    settle(1);
    m_fake->list(b, 0x045e, 0x0b13, name, 0, 2);
    settle(2);
    m_fake->list(c, 0x045e, 0x0b13, name, 0, 3);
    settle(3);
    QCOMPARE(m_plugin->inputForPath(b), 1);
    QCOMPARE(lines(), QList<quint32>());
    // C (the last) goes: no line has another device.
    m_fake->unlist(c);
    settle(2);
    QCOMPARE(lines(), QList<quint32>());
    // A (the first) goes: B moves onto line 0 -- told, once.
    m_fake->unlist(a);
    settle(1);
    QCOMPARE(m_plugin->inputForPath(b), 0);
    QCOMPARE(lines(), QList<quint32>{ 0 });
    // B re-enumerates where it is (another system entry, same path): line 0, once.
    m_fake->unlist(b);
    m_fake->list(b, 0x045e, 0x0b13, name, 0, 4);
    m_plugin->rescanDevices();
    QTRY_COMPARE_WITH_TIMEOUT(m_fake->opens(b), 2, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(m_plugin->isScanning() == false && m_plugin->openingCount() == 0, 3000);
    QTest::qWait(30);
    QCOMPARE(countOf(m_plugin->inputs(), name), 1);
    QCOMPARE(lines(), QList<quint32>{ 0 });
    // A rescan that changes nothing tells nothing.
    settle(1);
    QCOMPARE(lines(), QList<quint32>());
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
    QVERIFY2(call.elapsed() < 1000, qPrintable(QString("asking took %1 ms").arg(call.elapsed())));
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
    /* Waiting on the stuck open would never end (the fake never returns
       unreleased): any bound is the proof, not the host's speed. */
    QVERIFY2(t.elapsed() < 2000, qPrintable(QString("the plugin took %1 ms to go").arg(t.elapsed())));
    QVERIFY(HIDPlugin::workerThreads() > 0);    // the stuck one, left to it

    // Released: the stuck thread finds the plugin gone, drops its pad, ends.
    m_fake->release();
    QTRY_COMPARE_WITH_TIMEOUT(HIDPlugin::workerThreads(), 0, 5000);
}

QTEST_MAIN(HIDRescan_Test)
