/*
  Q Light Controller Plus
  iolock_test.cpp

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
#include <QThread>

#include "stdprelude.h"
// The plugin is put in the Doc's cache by hand: not loaded from disk.
#define private public
#include "ioplugincache.h"
#undef private

#include "iolock_test.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "qlcioplugin.h"
#include "mastertimer.h"
#include "doc.h"

namespace
{
/* How long a call to the stuck device takes: well past the tick (20 ms)
   and the bound the tests hold the output to. */
const int kStallMs = 400;
/* The worst gap allowed between two ticks, or two frames of a universe on
   a device that answers. The old code's was a whole stall or two. */
const int kGapMs = 250;

QElapsedTimer s_clock;

/* When each MasterTimer tick came, from the timer's own thread. */
QMutex s_tickLock;
QList<qint64> s_ticks;

qint64 worstGap(const QList<qint64> &times, qint64 from)
{
    qint64 last = -1, worst = 0;
    for (qint64 t : times)
    {
        if (t < from)
            continue;
        if (last >= 0)
            worst = qMax(worst, t - last);
        last = t;
    }
    return worst;
}

qint64 worstTickGap(qint64 from)
{
    QMutexLocker l(&s_tickLock);
    return worstGap(s_ticks, from);
}
}

/* Output and input lines whose device calls (open, close) on the stuck
   output line, or on any input while inputs are stuck, take kStallMs --
   a device the system does not answer for. Its lines are hardware: they
   come and go (followInputDevices(), retryOfflinePatches()). */
class StuckPlugin final : public QLCIOPlugin
{
public:
    void init() override {}
    QString name() const override { return QStringLiteral("Stuck"); }
    int capabilities() const override { return Output | Input | Feedback; }
    QString pluginInfo() const override { return QStringLiteral("fake"); }
    bool linesAreHardware() const override { return true; }

    QStringList outputs() override { return { QStringLiteral("Stuck DMX"), QStringLiteral("Healthy DMX") }; }
    QStringList inputs() override
    {
        QMutexLocker l(&m_lock);
        return m_inputs;
    }
    void setInputs(const QStringList &inputs)
    {
        QMutexLocker l(&m_lock);
        m_inputs = inputs;
    }

    bool openOutput(quint32 line, quint32) override
    {
        stallOutput(line);
        return m_opens.load();
    }
    void closeOutput(quint32 line, quint32) override { stallOutput(line); }
    bool openInput(quint32, quint32) override
    {
        stallInput();
        return true;
    }
    void closeInput(quint32, quint32) override { stallInput(); }

    void writeUniverse(quint32 universe, quint32 line, const QByteArray &, bool) override
    {
        QMutexLocker l(&m_lock);
        m_writes[qMakePair(universe, line)] << s_clock.elapsed();
    }

    QList<qint64> writes(quint32 universe, quint32 line) const
    {
        QMutexLocker l(&m_lock);
        return m_writes.value(qMakePair(universe, line));
    }

    /** Device calls that stalled */
    int stalls() const { return m_stalls.load(); }

    /** The output line 0's device stops answering for a while each call */
    std::atomic<bool> m_outputStuck { false };
    /** Every input's does */
    std::atomic<bool> m_inputsStuck { false };
    /** What openOutput() says */
    std::atomic<bool> m_opens { true };

private:
    void stallOutput(quint32 line)
    {
        if (line == 0 && m_outputStuck.load())
            stall();
    }
    void stallInput()
    {
        if (m_inputsStuck.load())
            stall();
    }
    void stall()
    {
        m_stalls.fetch_add(1);
        QThread::msleep(kStallMs);
    }

    mutable QMutex m_lock;
    QStringList m_inputs { QStringLiteral("Pad A"), QStringLiteral("Pad B") };
    QMap<QPair<quint32, quint32>, QList<qint64> > m_writes;
    std::atomic<int> m_stalls { 0 };
};

void IOLock_Test::init()
{
    s_clock.start();
    {
        QMutexLocker l(&s_tickLock);
        s_ticks.clear();
    }
    m_doc = new Doc(this);
    m_plugin = new StuckPlugin();
    m_doc->ioPluginCache()->m_plugins.append(m_plugin);    // the cache deletes it
    // As IOPluginCache::load() does for a plugin it loads.
    connect(m_plugin, SIGNAL(configurationChanged()), m_doc->ioPluginCache(), SLOT(slotConfigurationChanged()));
    connect(m_doc->masterTimer(), &MasterTimer::tickReady, this, []() {
        QMutexLocker l(&s_tickLock);
        s_ticks << s_clock.elapsed();
    }, Qt::DirectConnection);
    QVERIFY(m_doc->inputOutputMap()->universesCount() >= 4);
}

void IOLock_Test::cleanup()
{
    m_plugin->m_outputStuck = false;
    m_plugin->m_inputsStuck = false;
    m_doc->masterTimer()->stop();
    delete m_doc;
    m_doc = nullptr;
    m_plugin = nullptr;
}

int IOLock_Test::sentBetween(quint32 universe, quint32 line, qint64 from, qint64 to) const
{
    int n = 0;
    for (qint64 t : m_plugin->writes(universe, line))
        if (t > from && t < to)
            n++;
    return n;
}

void IOLock_Test::start()
{
    // As the app runs: the universe threads, the timer.
    m_doc->inputOutputMap()->startUniverses();
    m_doc->masterTimer()->start();
    QTest::qWait(100);
}

void IOLock_Test::deviceListChanged()
{
    QMetaObject::invokeMethod(m_plugin, "configurationChanged", Qt::DirectConnection);
}

void IOLock_Test::aDeviceThatAnswersIsPatchedAsBefore()
{
    /* The device calls after the claim: a patch to a device that answers
       is opened, sends, and says a failed open, as ever. */
    InputOutputMap *iom = m_doc->inputOutputMap();
    start();

    QVERIFY(iom->setOutputPatch(1, m_plugin->name(), QString(), 1));
    OutputPatch *op = iom->outputPatch(1);
    QVERIFY(op != nullptr);
    QVERIFY(op->isPatched());
    QCOMPARE(op->output(), quint32(1));
    QTRY_VERIFY_WITH_TIMEOUT(m_plugin->writes(1, 1).count() > 3, 2000);

    QVERIFY(iom->setInputPatch(2, m_plugin->name(), QString(), 0));
    QVERIFY(iom->inputPatch(2) != nullptr);
    QCOMPARE(iom->inputPatch(2)->inputUID(), QStringLiteral("Pad A"));

    m_plugin->m_opens = false;
    QVERIFY(iom->setOutputPatch(3, m_plugin->name(), QString(), 1) == false);
    m_plugin->m_opens = true;
    QCOMPARE(m_plugin->stalls(), 0);
}

void IOLock_Test::theDmxTicksWhileAStuckOutputIsPatchedReconnectedAndRemoved()
{
    /* A DMX interface on a USB hub a device wedged: every open and close of
       it hangs. Patched, reconnected (a device came or went), given a
       feedback patch and unpatched -- before 0.4.12 each with the universes
       claimed, the tick stopped for every call. Universe 1 on a device that
       answers keeps writing throughout. */
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setOutputPatch(0, m_plugin->name(), QString(), 1));
    start();

    m_plugin->m_outputStuck = true;
    const qint64 from = s_clock.elapsed();
    QVERIFY(iom->setOutputPatch(1, m_plugin->name(), QString(), 0));            // open
    const qint64 opened = s_clock.elapsed();
    QVERIFY(iom->outputPatch(1)->isPatched());
    /* Not ready while its open is in flight: universe 2 sent nothing to the
       line (its thread ran, the tick went on), then sends once it is open. */
    QVERIFY2(sentBetween(1, 0, from, opened - 50) == 0,
             qPrintable(QString("%1 frame(s) sent while the line was opening").arg(sentBetween(1, 0, from, opened - 50))));
    QTRY_VERIFY_WITH_TIMEOUT(sentBetween(1, 0, opened, s_clock.elapsed()) > 0, 1000);
    const qint64 reconnecting = s_clock.elapsed();
    deviceListChanged();                                                        // close + open
    const qint64 reconnected = s_clock.elapsed();
    QVERIFY2(sentBetween(1, 0, reconnecting + 50, reconnected - 50) == 0,
             qPrintable(QString("%1 frame(s) sent while the line was reconnecting")
                        .arg(sentBetween(1, 0, reconnecting + 50, reconnected - 50))));
    QTRY_VERIFY_WITH_TIMEOUT(sentBetween(1, 0, reconnected, s_clock.elapsed()) > 0, 1000);
    QVERIFY(iom->setOutputPatch(2, m_plugin->name(), QString(), 0, true));      // feedback open
    QVERIFY(iom->setOutputPatch(1, KOutputNone, QString(), QLCIOPlugin::invalidLine()));       // close
    QVERIFY(iom->setOutputPatch(2, KOutputNone, QString(), QLCIOPlugin::invalidLine(), true)); // close
    QTest::qWait(100);
    const qint64 until = s_clock.elapsed();

    QVERIFY(iom->outputPatch(1) == nullptr);
    QVERIFY(iom->feedbackPatch(2) == nullptr);
    QVERIFY2(m_plugin->stalls() >= 6, qPrintable(QString::number(m_plugin->stalls())));
    QVERIFY2(until - from >= 6 * kStallMs, qPrintable(QString::number(until - from)));

    const qint64 tickGap = worstTickGap(from);
    QVERIFY2(tickGap < kGapMs, qPrintable(QString("the MasterTimer stopped for %1 ms").arg(tickGap)));
    const QList<qint64> healthy = m_plugin->writes(0, 1);
    const qint64 frameGap = worstGap(healthy, from);
    QVERIFY2(frameGap < kGapMs, qPrintable(QString("universe 1's output stopped for %1 ms").arg(frameGap)));
    QVERIFY(healthy.isEmpty() == false && healthy.last() > until - kGapMs);
}

void IOLock_Test::theDmxTicksWhileAStuckInputMovesGoesAndComesBack()
{
    /* A controller whose reader stopped answering: every close and open of
       it waits (HIDDevice::kReaderStopMs, or for good on a wedged hub).
       Patched (setInputPatch), moved to another line when a device came
       before it (followInputDevices), gone (kept offline) and back
       (retryOfflinePatches) -- the DMX output ticks on throughout. */
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setOutputPatch(0, m_plugin->name(), QString(), 1));
    start();

    m_plugin->m_inputsStuck = true;
    const qint64 from = s_clock.elapsed();
    QVERIFY(iom->setInputPatch(1, m_plugin->name(), QString(), 0));             // open
    QCOMPARE(iom->inputPatch(1)->inputUID(), QStringLiteral("Pad A"));

    // A device came before it: Pad A is on line 1 now.
    m_plugin->setInputs({ QStringLiteral("Pad Z"), QStringLiteral("Pad A"), QStringLiteral("Pad B") });
    deviceListChanged();                                    // close + open, then reconnected
    QCOMPARE(iom->inputPatch(1)->input(), quint32(1));

    // Gone: kept offline.
    m_plugin->setInputs({ QStringLiteral("Pad Z"), QStringLiteral("Pad B") });
    deviceListChanged();                                    // close
    QVERIFY(iom->inputPatch(1) == nullptr || iom->inputPatch(1)->isPatched() == false);

    // Back: reopened by itself.
    m_plugin->setInputs({ QStringLiteral("Pad Z"), QStringLiteral("Pad A"), QStringLiteral("Pad B") });
    deviceListChanged();                                    // open
    QVERIFY(iom->inputPatch(1) != nullptr);
    QCOMPARE(iom->inputPatch(1)->inputUID(), QStringLiteral("Pad A"));
    QCOMPARE(iom->inputPatch(1)->input(), quint32(1));
    QTest::qWait(100);

    QVERIFY2(m_plugin->stalls() >= 6, qPrintable(QString::number(m_plugin->stalls())));
    const qint64 tickGap = worstTickGap(from);
    QVERIFY2(tickGap < kGapMs, qPrintable(QString("the MasterTimer stopped for %1 ms").arg(tickGap)));
    const qint64 frameGap = worstGap(m_plugin->writes(0, 1), from);
    QVERIFY2(frameGap < kGapMs, qPrintable(QString("universe 1's output stopped for %1 ms").arg(frameGap)));
}

void IOLock_Test::theDmxTicksWhileAControllerSlotsStuckDeviceMoves()
{
    /* A controller slot (Pad A, else Pad B) whose live pad moves line when
       a device comes before it: applyInputSlot() reopens it there. */
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setOutputPatch(0, m_plugin->name(), QString(), 1));
    QVERIFY(iom->setInputPatch(1, m_plugin->name(), QString(), 0));
    QVERIFY(iom->addInputSlotDevice(1, m_plugin->name(), QStringLiteral("Pad B")));
    QCOMPARE(iom->inputPatch(1)->inputUID(), QStringLiteral("Pad A"));
    start();

    m_plugin->m_inputsStuck = true;
    const qint64 from = s_clock.elapsed();
    m_plugin->setInputs({ QStringLiteral("Pad Z"), QStringLiteral("Pad A"), QStringLiteral("Pad B") });
    deviceListChanged();                                    // close + open, then reconnected
    QCOMPARE(iom->inputPatch(1)->inputUID(), QStringLiteral("Pad A"));
    QCOMPARE(iom->inputPatch(1)->input(), quint32(1));
    QTest::qWait(100);

    QVERIFY2(m_plugin->stalls() >= 2, qPrintable(QString::number(m_plugin->stalls())));
    const qint64 tickGap = worstTickGap(from);
    QVERIFY2(tickGap < kGapMs, qPrintable(QString("the MasterTimer stopped for %1 ms").arg(tickGap)));
    const qint64 frameGap = worstGap(m_plugin->writes(0, 1), from);
    QVERIFY2(frameGap < kGapMs, qPrintable(QString("universe 1's output stopped for %1 ms").arg(frameGap)));
}

QTEST_MAIN(IOLock_Test)
