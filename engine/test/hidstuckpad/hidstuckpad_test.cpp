/*
  Q Light Controller Plus
  hidstuckpad_test.cpp

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
#include <QElapsedTimer>
#include <QMutex>
#include <QSettings>
#include <QTemporaryDir>

#include "hidstuckpad_test.h"
#include "fakehidbackend.h"
#include "programmercontroller.h"
#include "controllerjobs.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "mastertimer.h"
#include "showcontrol.h"
#include "padrun.h"
#include "doc.h"

namespace
{
const QString kDualSensePath = QStringLiteral("USB_054c_0ce6_2124000");
const QString kDualSense = QStringLiteral("DualSense Wireless Controller");
const QString kClonePath = QStringLiteral("USB_045e_028e_2124000");
const QString kClone = QStringLiteral("Xbox 360 Controller for Windows");
const QString kStopPad = QStringLiteral("Stop Pad");
const int kLimitMs = 300;

/* Never deleted: a thread may still be leaving a call into one. */
QList<FakeHIDBackend *> s_fakes;

/* When each MasterTimer tick came, from the timer's own thread. */
QMutex s_tickLock;
QList<qint64> s_ticks;
QElapsedTimer s_clock;

qint64 worstGap(qint64 from)
{
    QMutexLocker l(&s_tickLock);
    qint64 last = -1, worst = 0;
    for (qint64 t : std::as_const(s_ticks))
    {
        if (t < from)
            continue;
        if (last >= 0)
            worst = qMax(worst, t - last);
        last = t;
    }
    return worst;
}
}

void HIDStuckPad_Test::initTestCase()
{
    m_pluginDir = new QTemporaryDir();
    QVERIFY(m_pluginDir->isValid());
    const QFileInfo hid(QStringLiteral(QLC_HID_PLUGIN_FILE));
    QVERIFY2(hid.exists(), QLC_HID_PLUGIN_FILE);
    QVERIFY(QFile::copy(hid.absoluteFilePath(), m_pluginDir->filePath(hid.fileName())));
    qputenv("QLC_HID_PROFILE_DIR", QByteArray(QLC_TEST_RESOURCE_ROOT) + "hidprofiles");
    // The other pad -- the one whose emergency stop must still stop.
    qputenv("QLC_HID_VIRTUAL_PAD", QByteArray("045e:0b13:") + kStopPad.toUtf8());
    qputenv("QLC_HID_VIRTUAL_ONLY", "1");
    qputenv("QLC_HID_OPEN_LIMIT_MS", QByteArray::number(kLimitMs));
    s_clock.start();
}

void HIDStuckPad_Test::cleanupTestCase()
{
    delete m_pluginDir;
    m_pluginDir = nullptr;
}

void HIDStuckPad_Test::init()
{
    QSettings().remove(ControllerJobs::settingsGroup());    // every pad every job
    m_doc = new Doc(this);
    m_commands.clear();
    m_doc->showControl()->setRunHandler([this](const ShowControl::Request &r, ShowControl::Route) {
        m_commands << int(r.command);
        return true;
    });
    m_doc->ioPluginCache()->load(QDir(m_pluginDir->path()));
    m_hid = m_doc->ioPluginCache()->plugin(QStringLiteral("HID"));
    QVERIFY2(m_hid != nullptr, "the HID plugin did not load");
    m_fake = new FakeHIDBackend();
    s_fakes << m_fake;
    {
        QMutexLocker l(&s_tickLock);
        s_ticks.clear();
    }
    connect(m_doc->masterTimer(), &MasterTimer::tickReady, this, []() {
        QMutexLocker l(&s_tickLock);
        s_ticks << s_clock.elapsed();
    }, Qt::DirectConnection);
}

void HIDStuckPad_Test::cleanup()
{
    if (m_fake != nullptr)
        m_fake->release();
    QTRY_COMPARE_WITH_TIMEOUT(m_fake->callsInProgress(), 0, 2000);
    m_doc->showControl()->setRunHandler(ShowControl::RunHandler());
    m_doc->masterTimer()->stop();
    delete m_doc;
    m_doc = nullptr;
    m_hid = nullptr;
    QSettings().remove(ControllerJobs::settingsGroup());
}

int HIDStuckPad_Test::channelOf(quint32 line, const QString &role) const
{
    for (quint32 ch = 0; ch < 64; ++ch)
        if (m_hid->inputChannelRole(line, ch) == role)
            return int(ch);
    return -1;
}

bool HIDStuckPad_Test::inject(quint32 line, const QString &role, uchar value)
{
    const int ch = channelOf(line, role);
    if (ch < 0)
        return false;
    bool ok = false;
    QMetaObject::invokeMethod(m_hid, "injectVirtualInput", Qt::DirectConnection,
                              Q_RETURN_ARG(bool, ok), Q_ARG(quint32, line),
                              Q_ARG(quint32, quint32(ch)), Q_ARG(uint, uint(value)));
    return ok;
}

int HIDStuckPad_Test::stops() const
{
    return int(m_commands.count(int(ShowControl::AllOff)));
}

void HIDStuckPad_Test::theShowRunsOnWhileAPadHangs()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    PadRun *run = m_doc->programmer()->padRun();

    /* The DualSense on a cable, and the stop pad. */
    m_fake->list(kDualSensePath, 0x054c, 0x0ce6, kDualSense);
    QMetaObject::invokeMethod(m_hid, "setTestBackend", Qt::DirectConnection,
                              Q_ARG(quintptr, quintptr(m_fake)));
    QTRY_VERIFY_WITH_TIMEOUT(m_hid->inputs().contains(kDualSense), 2000);
    const quint32 stopLine = quint32(m_hid->inputs().indexOf(kStopPad));
    const quint32 padLine = quint32(m_hid->inputs().indexOf(kDualSense));
    QVERIFY(m_hid->inputs().contains(kStopPad));
    QVERIFY(iom->setInputPatch(0, QStringLiteral("HID"), QString(), stopLine));
    QVERIFY(iom->setInputPatch(1, QStringLiteral("HID"), QString(), padLine));
    QVERIFY(channelOf(padLine, "lb") >= 0);

    // As the app runs: the universe threads (which pass the input on), the timer.
    iom->startUniverses();
    m_doc->masterTimer()->start();
    run->setEnabled(true);
    run->setArmChordMs(15);

    /* Armed from the DualSense. */
    QVERIFY(inject(padLine, "lb", 255));
    QVERIFY(inject(padLine, "rb", 255));
    QTRY_VERIFY_WITH_TIMEOUT(run->isArmed(), 2000);
    QVERIFY(inject(padLine, "rb", 0));
    QVERIFY(inject(padLine, "lb", 0));

    /* View + Menu on the clone: it drops off USB and comes back as an Xbox
       360 pad at the same port, which never finishes opening. */
    const qint64 from = s_clock.elapsed();
    m_fake->unlist(kDualSensePath);
    m_fake->list(kClonePath, 0x045e, 0x028e, kClone, FakeHIDBackend::Forever);
    QElapsedTimer call;
    call.start();
    m_hid->rescan();
    QVERIFY2(call.elapsed() < 50, qPrintable(QString("rescan() took %1 ms").arg(call.elapsed())));

    // As for an unplug: disarmed, said why, and no stale device left.
    QTRY_VERIFY_WITH_TIMEOUT(run->isArmed() == false, 2000);
    QCOMPARE(run->notice(), QString("Disarmed: the controller went away."));
    QVERIFY(m_hid->inputs().contains(kDualSense) == false);

    // The stuck one is named for the footer.
    QTRY_COMPARE_WITH_TIMEOUT(m_hid->targetProblems().count(), 1, 2000);
    QVERIFY2(m_hid->targetProblems().first().startsWith(kClone), qPrintable(m_hid->targetProblems().first()));

    /* While it hangs: asked again (the hotplug, the stale check), a patch
       reconnected -- and the DMX output ticks on time throughout. */
    m_hid->rescan();
    QMetaObject::invokeMethod(m_hid, "configurationChanged", Qt::DirectConnection);
    QTest::qWait(400);
    QCOMPARE(m_fake->opens(kClonePath), 1);
    QCOMPARE(m_fake->callsInProgress(), 1);

    /* The other pad's emergency stop: all four. */
    QVERIFY(inject(stopLine, "lb", 255));
    QVERIFY(inject(stopLine, "rb", 255));
    QVERIFY(inject(stopLine, "lt", 255));
    QVERIFY(inject(stopLine, "rt", 255));
    QTRY_COMPARE_WITH_TIMEOUT(stops(), 1, 2000);
    for (const char *role : { "rt", "lt", "rb", "lb" })
        QVERIFY(inject(stopLine, role, 0));

    const qint64 gap = worstGap(from);
    QVERIFY2(gap < 250, qPrintable(QString("the DMX output stalled %1 ms").arg(gap)));
    {
        QMutexLocker l(&s_tickLock);
        QVERIFY2(s_ticks.count() > 20, qPrintable(QString::number(s_ticks.count())));
    }
    QCOMPARE(m_fake->callsInProgress(), 1);     // still stuck all along
}

QTEST_MAIN(HIDStuckPad_Test)
