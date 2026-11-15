/*
  Q Light Controller Plus - Unit test
  dmxusb_test.cpp

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

#include "writehealth.h"
#include "dmxinterface.h"
#include "enttecdmxusbopen.h"
#include "enttecdmxusbpro.h"
#include "dmxusb_test.h"

#include <QMutex>
#include <QElapsedTimer>
#include <QThread>

/* A USB DMX interface that records every write (the widget's thread sends
   on its own). */
class FakeDMXInterface final : public DMXInterface
{
public:
    FakeDMXInterface() : DMXInterface("FAKE0001", "Fake", "Test", 0x0403, 0x6001) {}
    Type type() const override { return libFTDI; }
    QString typeString() const override { return "fake"; }
    bool open() override { m_open = true; return true; }
    bool openByPID(const int) override { m_open = true; return true; }
    bool close() override { m_open = false; return true; }
    bool isOpen() const override { return m_open; }
    bool reset() override { return true; }
    bool setLineProperties() override { return true; }
    bool setBaudRate() override { return true; }
    bool setFlowControl() override { return true; }
    bool setLowLatency(bool) override { return true; }
    bool clearRts() override { return true; }
    bool purgeBuffers() override { return true; }
    bool setBreak(bool) override { return true; }
    bool write(const QByteArray &data) override
    {
        QMutexLocker l(&m_mutex);
        m_writes << data;
        return true;
    }
    QByteArray read(int) override { return QByteArray(); }
    uchar readByte(bool *ok = NULL) override { if (ok) *ok = false; return 0; }

    QList<QByteArray> writes() { QMutexLocker l(&m_mutex); return m_writes; }

private:
    bool m_open = false;
    QMutex m_mutex;
    QList<QByteArray> m_writes;
};

static const int Laser = 14;

/* The DMX value at @p address in the last DMX frame written, or -1.
   @p header: bytes before channel 1 (Open: the start code; Pro: the
   message header and the start code). @p label: the Pro's send-DMX label,
   0 for the Open (every write is a frame). */
static int lastSent(const QList<QByteArray> &writes, int header, char label, int address)
{
    for (int i = writes.size() - 1; i >= 0; --i)
    {
        const QByteArray &w = writes.at(i);
        if (label != 0 && (w.size() < 2 || w.at(1) != label))
            continue;
        if (w.size() <= header + address)
            continue;
        return uchar(w.at(header + address));
    }
    return -1;
}

static bool waitFor(const std::function<bool()> &cond, int ms = 2000)
{
    QElapsedTimer t;
    t.start();
    while (!cond() && t.elapsed() < ms)
        QThread::msleep(2);
    return cond();
}

void DMXUSB_Test::openDmxSendsTheClosingFrameBeforeClosing()
{
    int lost = 0;
    const int trials = 5;
    for (int trial = 0; trial < trials; ++trial)
    {
        FakeDMXInterface *iface = new FakeDMXInterface();
        EnttecDMXUSBOpen widget(iface, 0);
        QVERIFY(widget.open(0, false));

        // Mid-show: the laser on, the frame on the wire.
        QByteArray on(512, char(0));
        on[Laser] = char(255);
        widget.writeUniverse(0, 0, on, true);
        QVERIFY(waitFor([&]() { return lastSent(iface->writes(), 1, 0, Laser) == 255; }));

        // closeLine(): the closing frame, then closeOutput() at once.
        QByteArray closing(512, char(0));
        widget.writeUniverse(0, 0, closing, true);
        widget.close(0, false);

        const int last = lastSent(iface->writes(), 1, 0, Laser);
        if (last != 0)
            ++lost;
        qInfo("Open DMX trial %d: the last frame on the wire has the laser at %d", trial, last);
    }
    QVERIFY2(lost == 0, qPrintable(QString("Enttec Open DMX: the closing frame never reached the wire "
                                           "in %1 of %2 closes -- the laser stays where it was").arg(lost).arg(trials)));
}

void DMXUSB_Test::proSendsTheClosingFrameBeforeClosing()
{
    int lost = 0;
    const int trials = 5;
    for (int trial = 0; trial < trials; ++trial)
    {
        FakeDMXInterface *iface = new FakeDMXInterface();
        EnttecDMXUSBPro widget(iface, 0, 0);
        QVERIFY(widget.open(0, false));

        QByteArray on(512, char(0));
        on[Laser] = char(255);
        // writeUniverse() needs the line open (the thread opens it)
        QVERIFY(waitFor([&]() { return widget.isOpen(); }));
        widget.writeUniverse(0, 0, on, true);
        // Pro message: 0x7E, label 6, len LSB, len MSB, start code, data
        QVERIFY(waitFor([&]() { return lastSent(iface->writes(), 5, char(6), Laser) == 255; }));

        QByteArray closing(512, char(0));
        widget.writeUniverse(0, 0, closing, true);
        widget.close(0, false);

        const int last = lastSent(iface->writes(), 5, char(6), Laser);
        if (last != 0)
            ++lost;
        qInfo("Pro trial %d: the last frame on the wire has the laser at %d", trial, last);
    }
    QVERIFY2(lost == 0, qPrintable(QString("Enttec DMX USB Pro: the closing frame never reached the wire "
                                           "in %1 of %2 closes -- the Pro keeps sending the laser on").arg(lost).arg(trials)));
}

/* Review 12: DMXUSB::rescanWidgets() (any DMX USB hot-plug, the config
   dialog's Refresh) writes each Show-only universe's closing frame to its
   widget, then closes every open line in turn, then deletes the widgets.
   Review 11 took the wait out of the Pro's close() (moved to flush(), which
   only the engine's flushOutput() calls) -- the rescan never calls it. On
   a Pro with two output ports, the port closed first is marked closed at
   once and its thread skips it: the closing frame is never sent, and the
   Pro repeats the last frame it got for as long as it has power. */
void DMXUSB_Test::review12_aRescanSendsTheClosingFrameOnEveryPortOfAPro()
{
    int lost = 0;
    const int trials = 5;
    for (int trial = 0; trial < trials; ++trial)
    {
        FakeDMXInterface *iface = new FakeDMXInterface();
        EnttecDMXUSBPro widget(iface, 0, 0);
        widget.setPortsMapping(QList<int>() << (DMXUSBWidget::DMX | DMXUSBWidget::Output)
                                            << (DMXUSBWidget::DMX | DMXUSBWidget::Output));
        QVERIFY(widget.open(0, false));
        QVERIFY(waitFor([&]() { return widget.isOpen() && widget.openPortsCount() == 1; }));
        QVERIFY(widget.open(1, false));
        QVERIFY(waitFor([&]() { return widget.openPortsCount() == 2; }));

        // Port 1: the laser rig's universe, laser on. Port 2: another universe.
        QByteArray on(512, char(0));
        on[Laser] = char(255);
        QByteArray other(512, char(0));
        other[3] = char(100);
        widget.writeUniverse(0, 0, on, true);
        widget.writeUniverse(1, 1, other, true);
        QVERIFY(waitFor([&]() { return lastSent(iface->writes(), 5, char(6), Laser) == 255; }));

        // rescanWidgets(): the closing frames (Show-only universes only),
        // then every open line closed, in the plugin's universe order.
        QByteArray closing(512, char(0));
        widget.writeUniverse(0, 0, closing, true);
        widget.close(0, false);
        widget.close(1, false);

        const int last = lastSent(iface->writes(), 5, char(6), Laser);
        if (last != 0)
            ++lost;
        qInfo("Pro two ports, trial %d: the last frame on port 1 has the laser at %d", trial, last);
    }
    QVERIFY2(lost == 0, qPrintable(QString("Enttec Pro, two ports: a rescan's closing frame never reached "
                                           "port 1 in %1 of %2 rescans -- the laser stays on").arg(lost).arg(trials)));
}

/* 44 frames a second: one write every 23 ms. */
static const qint64 Frame = 23;

void DMXUSB_Test::aWidgetThatStopsTakingDataIsAProblem()
{
    WriteHealth h;
    qint64 t = 0;
    for (; t < 2000; t += Frame)
        QVERIFY(!h.record(true, t));
    QVERIFY(h.problem().isEmpty());

    // Unplugged: every write refused from here on.
    int changes = 0;
    const qint64 pulled = t;
    for (; t < pulled + 3000; t += Frame)
        changes += h.record(false, t) ? 1 : 0;
    QCOMPARE(changes, 1);
    QVERIFY(h.isFailing());
    QVERIFY2(h.problem().startsWith("not taking data since "), qPrintable(h.problem()));
}

void DMXUSB_Test::aFewLostFramesAreNot()
{
    WriteHealth h;
    qint64 t = 0;
    for (int i = 0; i < 200; i++, t += Frame)
        QVERIFY(!h.record(i % 50 != 7 && i % 50 != 8, t));    // two refused now and then
    QVERIFY(!h.isFailing());
    QVERIFY(h.problem().isEmpty());
}

void DMXUSB_Test::itClearsOnceWritesHaveGoneThroughAWhile()
{
    WriteHealth h;
    qint64 t = 0;
    for (; t <= WriteHealth::FailAfterMs + Frame; t += Frame)
        h.record(false, t);
    QVERIFY(h.isFailing());

    // Plugged back in: a moment of writes is not enough, a while is.
    const qint64 back = t;
    int changes = 0;
    for (; t < back + WriteHealth::RecoverAfterMs - Frame; t += Frame)
        changes += h.record(true, t) ? 1 : 0;
    QCOMPARE(changes, 0);
    QVERIFY(h.isFailing());
    // A refused write in that while starts it again.
    h.record(false, t);
    t += Frame;
    const qint64 again = t;
    for (; t < again + WriteHealth::RecoverAfterMs + Frame; t += Frame)
        changes += h.record(true, t) ? 1 : 0;
    QCOMPARE(changes, 1);
    QVERIFY(!h.isFailing());
    QVERIFY(h.problem().isEmpty());
}

QTEST_GUILESS_MAIN(DMXUSB_Test)
