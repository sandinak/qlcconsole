/*
  Q Light Controller Plus
  e131_test.cpp

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

#include <QTest>
#include <QSignalSpy>
#include <QRegularExpression>
#include <QNetworkInterface>

#define private public
#include "e131_test.h"
#include "e131controller.h"
#include "e131plugin.h"
#include "e131packetizer.h"
#undef private

static const qint64 kFrameMs = 23;

/** A controller on loopback with output universes @a universes. Binding its
 *  socket to 127.0.0.1 sends nothing, and nothing here writes to it. */
static E131Controller *testController(QObject *parent, QList<quint32> universes)
{
    QNetworkAddressEntry entry;
    entry.setIp(QHostAddress::LocalHost);
    entry.setPrefixLength(8);
    E131Controller *ctrl = new E131Controller(QNetworkInterface(), entry, 0, parent);
    foreach (quint32 u, universes)
        ctrl->addUniverse(u, E131Controller::Output);
    ctrl->m_testNowMs = 0;
    return ctrl;
}

/** Feed universe @a u one result every @a periodMs for @a forMs, as a send
 *  to its multicast address would; count announcements in @a changes */
static void feed(E131Controller *ctrl, quint32 u, bool ok, qint64 forMs,
                 qint64 periodMs, int &changes)
{
    UniverseInfo &info = ctrl->m_universeMap[u];
    const qint64 end = ctrl->m_testNowMs + forMs;
    for (; ctrl->m_testNowMs < end; ctrl->m_testNowMs += periodMs)
        if (ctrl->reportSendResult(info, u, info.outputMcastAddress, ok))
            changes++;
}

void E131_Test::transientFailuresAreNotReported()
{
    QObject owner;
    E131Controller *ctrl = testController(&owner, { 0 });
    int changes = 0;
    for (int i = 0; i < 5; i++)
    {
        feed(ctrl, 0, true, 2000, kFrameMs, changes);
        feed(ctrl, 0, false, 500, kFrameMs, changes);
    }
    feed(ctrl, 0, true, 2000, kFrameMs, changes);
    QCOMPARE(changes, 0);
    QVERIFY(ctrl->outputProblems().isEmpty());
    QCOMPARE(ctrl->getPacketSentNumber() > 0, true);
}

void E131_Test::sustainedFailureIsReportedOnceAndRecovers()
{
    QObject owner;
    E131Controller *ctrl = testController(&owner, { 2 });
    int changes = 0;
    feed(ctrl, 2, false, 60000, kFrameMs, changes);
    QCOMPARE(changes, 1);

    const QList<QPair<quint32, QString> > p = ctrl->outputProblems();
    QCOMPARE(p.count(), 1);
    QCOMPARE(p.first().first, quint32(2));
    QVERIFY2(QRegularExpression("^E1\\.31 to 239\\.255\\.0\\.3: not reachable since \\d\\d:\\d\\d$")
                 .match(p.first().second).hasMatch(), qPrintable(p.first().second));

    // Working again: cleared after 30 s of good sends, not on the first
    feed(ctrl, 2, true, 29000, kFrameMs, changes);
    QCOMPARE(changes, 1);
    QCOMPARE(ctrl->outputProblems().count(), 1);
    feed(ctrl, 2, true, 2000, kFrameMs, changes);
    QCOMPARE(changes, 2);
    QVERIFY(ctrl->outputProblems().isEmpty());
}

void E131_Test::alternatingResultsDoNotToggle()
{
    QObject owner;
    E131Controller *ctrl = testController(&owner, { 0 });
    UniverseInfo &info = ctrl->m_universeMap[0];
    int changes = 0;
    bool ok = true;
    for (int i = 0; i < 5000; i++, ctrl->m_testNowMs += kFrameMs)
    {
        if (ctrl->reportSendResult(info, 0, info.outputMcastAddress, ok))
            changes++;
        ok = !ok;
    }
    QCOMPARE(changes, 1);
    QCOMPARE(ctrl->outputProblems().count(), 1);
}

void E131_Test::aNewTargetStartsClean()
{
    QObject owner;
    E131Controller *ctrl = testController(&owner, { 0 });
    int changes = 0;
    feed(ctrl, 0, false, 5000, kFrameMs, changes);
    QCOMPARE(ctrl->outputProblems().count(), 1);

    // Retargeted to unicast: the old address's failures say nothing about it
    UniverseInfo &info = ctrl->m_universeMap[0];
    QVERIFY(ctrl->reportSendResult(info, 0, QHostAddress("10.0.0.7"), true));
    QVERIFY(ctrl->outputProblems().isEmpty());
}

void E131_Test::pluginGroupsAProblemsUniverses()
{
    E131Plugin plugin;
    E131Controller *ctrl = testController(&plugin, { 0, 1 });

    // Both universes aimed at one address that can't be reached
    for (quint32 u = 0; u < 2; u++)
        ctrl->m_universeMap[u].outputMcastAddress = QHostAddress("239.255.0.9");
    int changes = 0;
    for (int i = 0; i < 100; i++, ctrl->m_testNowMs += kFrameMs)
        for (quint32 u = 0; u < 2; u++)
            if (ctrl->reportSendResult(ctrl->m_universeMap[u], u, QHostAddress("239.255.0.9"), false))
                changes++;
    QCOMPARE(changes, 2);

    E131IO io;
    io.controller = ctrl;
    plugin.m_IOmapping.clear();
    plugin.m_IOmapping.append(io);

    const QStringList all = plugin.targetProblems();
    QCOMPARE(all.count(), 1);
    QVERIFY2(all.first().startsWith("E1.31 to 239.255.0.9: not reachable since ")
             && all.first().endsWith(" (universes 1, 2)"), qPrintable(all.first()));
    QCOMPARE(plugin.outputProblems(0, 1).count(), 1);
    QVERIFY(plugin.outputProblems(0, 7).isEmpty());
    QVERIFY(plugin.outputProblems(3, 0).isEmpty());

    plugin.m_IOmapping.clear();
}

QTEST_MAIN(E131_Test)

void E131_Test::dmxPacketsAreBoundedByTheDatagramAndAUniverse()
{
    E131Packetizer p("02:00:00:00:00:01");
    QByteArray full;
    p.setupE131Dmx(full, 0x0102, 100, QByteArray(4, char(0x33)));
    QCOMPARE(full.size(), 126 + 4);

    QByteArray dmx;
    quint32 universe = 0;
    QVERIFY(p.checkPacket(full));
    QVERIFY(p.fillDMXdata(full, dmx, universe));
    QCOMPARE(universe, quint32(0x0102));
    QCOMPARE(dmx, QByteArray(4, char(0x33)));

    /* 125 bytes passes checkPacket() but has no start code. */
    QByteArray b = full.left(125);
    QVERIFY(p.checkPacket(b));
    QVERIFY(!p.fillDMXdata(b, dmx, universe));
    QVERIFY(dmx.isEmpty());

    /* A count of 0 (no start code counted) is no channels, not the rest of
       the datagram; a count past 513 is one universe. */
    QByteArray zero = full;
    zero[123] = char(0x00);
    zero[124] = char(0x00);
    QVERIFY(p.fillDMXdata(zero, dmx, universe));
    QVERIFY(dmx.isEmpty());

    QByteArray big;
    p.setupE131Dmx(big, 1, 100, QByteArray(700, char(0x44)));
    QVERIFY(p.fillDMXdata(big, dmx, universe));
    QCOMPARE(dmx.size(), 512);

    /* Claiming more than it holds: what is there. */
    QByteArray liar = full;
    liar[123] = char(0x02);
    liar[124] = char(0x01);
    QVERIFY(p.fillDMXdata(liar, dmx, universe));
    QCOMPARE(dmx.size(), 4);
}
