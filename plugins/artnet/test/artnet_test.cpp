/*
  Q Light Controller Plus
  artnet_test.cpp

  Copyright (c) Jano Svitok

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

#define private public
#include "artnet_test.h"
#include "artnetpacketizer.h"
#undef private

/****************************************************************************
 * ArtNet tests
 ****************************************************************************/

void ArtNet_Test::setupArtNetDmx()
{
    ArtNetPacketizer ap;

    QByteArray data;
    const QByteArray empty;
    const QByteArray fifty(50, 10);
    const QByteArray fiftyone(51, 10);
    const QByteArray full(512, 20);

    // empty data
    ap.setupArtNetDmx(data, 0, empty);

    QCOMPARE(data.size(), 20);
    QCOMPARE(data.data(), "Art-Net");

    // full data
    ap.setupArtNetDmx(data, 0, full);

    QCOMPARE(data.size(), 18 + 512);
    QCOMPARE(data.data(), "Art-Net");

    // partial data
    ap.setupArtNetDmx(data, 0, fifty);

    QCOMPARE(data.size(), 18 + 50);
    QCOMPARE(data.data(), "Art-Net");

    ap.setupArtNetDmx(data, 0, fiftyone);

    QCOMPARE(data.size(), 18 + 52);
    QCOMPARE(data.data(), "Art-Net");
}

QTEST_MAIN(ArtNet_Test)

/** A real ArtPollReply, captured off the wire from the rig's CR041R
 *  ArtNet->DMX gateway at 172.18.2.10. Using a genuine packet rather than a
 *  hand-built one is the point: the parser reads two dozen fields at fixed
 *  offsets, and an off-by-one in any of them is invisible against a fixture
 *  built from the same assumptions as the code. The expected values below were
 *  cross-checked against what DMX-Workshop reports for this node. */
static const char cr041rPollReply[] = {
    char(0x41), char(0x72), char(0x74), char(0x2d), char(0x4e), char(0x65), char(0x74), char(0x00), char(0x00), char(0x21), char(0xac), char(0x12),
    char(0x02), char(0x0a), char(0x36), char(0x19), char(0x00), char(0x0e), char(0x00), char(0x00), char(0x00), char(0x22), char(0x00), char(0x00),
    char(0x7a), char(0x70), char(0x43), char(0x52), char(0x30), char(0x34), char(0x31), char(0x52), char(0x5f), char(0x30), char(0x30), char(0x31),
    char(0x00), char(0x00), char(0xdc), char(0x00), char(0x00), char(0x20), char(0x70), char(0xb5), char(0x43), char(0x52), char(0x30), char(0x34),
    char(0x31), char(0x52), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x30), char(0x30), char(0x30), char(0x36), char(0x20), char(0x5b), char(0x38), char(0x34), char(0x34), char(0x33), char(0x5d), char(0x20),
    char(0x41), char(0x72), char(0x74), char(0x4e), char(0x65), char(0x74), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x04), char(0x80), char(0x80), char(0x80), char(0x80), char(0x00), char(0x00),
    char(0x00), char(0x00), char(0x80), char(0x80), char(0x80), char(0x80), char(0x00), char(0x01), char(0x02), char(0x03), char(0x00), char(0x01),
    char(0x02), char(0x03), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x00), char(0x02), char(0x4d), char(0x48),
    char(0x12), char(0x02), char(0x0a), char(0xac), char(0x12), char(0x02), char(0x0a), char(0x01), char(0x08), char(0x00)
};

void ArtNet_Test::fillArtPollReplyInfo()
{
    ArtNetPacketizer ap;
    ArtNetNodeInfo info;

    const QByteArray reply(cr041rPollReply, sizeof(cr041rPollReply));
    QCOMPARE(reply.size(), 214);
    QVERIFY(ap.fillArtPollReplyInfo(reply, info) == true);

    QCOMPARE(info.shortName, QString("CR041R_001"));
    QCOMPARE(info.longName, QString("CR041R"));
    QCOMPARE(info.ipAddress, QString("172.18.2.10"));
    QCOMPARE(info.macAddress, QString("02:4D:48:12:02:0A"));
    QCOMPARE(int(info.firmwareVersion), 14);        // DMX-Workshop: "V0.14"
    QCOMPARE(int(info.oemCode), 0x0022);
    QCOMPARE(int(info.estaCode), 0x707A);           // EstaMan is little-endian
    QCOMPARE(info.portsNumber, 4);
    QCOMPARE(int(info.netSwitch), 0);
    QCOMPARE(int(info.subSwitch), 0);

    // Four output ports carrying universes 0..3.
    for (int i = 0; i < 4; i++)
    {
        QVERIFY((info.portTypes[i] & 0x80) != 0);   // output
        QCOMPARE(int(info.swOut[i]), i);
    }

    /* Capability is Status1 bit 1. This node is transmit-only -- DMX-Workshop
       reports "Node is not RDM capable (Unidirectional DMX)" and it never
       answers an ArtTodRequest. Note its GoodOutput bit 3 ("RDM disabled") is
       clear, which reads like RDM support if you check the wrong bit; that
       misreading is exactly what this assertion guards against. */
    QCOMPARE(info.rdmCapable, false);
    QVERIFY((info.goodOutput[0] & 0x08) == 0);

    // Too short to parse must be rejected rather than read past the end.
    QVERIFY(ap.fillArtPollReplyInfo(reply.left(100), info) == false);
    QVERIFY(ap.fillArtPollReplyInfo(QByteArray(), info) == false);
}
