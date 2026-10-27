/*
  Q Light Controller Plus - qlcconsole
  osc_test.cpp

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
#include <QUdpSocket>
#include <QRandomGenerator>
#include <QtEndian>
#include <cmath>
#include <limits>

#include "hostilecase.h"
#include "osc_test.h"
#include "oscpacketizer.h"

#define private public
#define protected public
#include "osccontroller.h"
#undef private
#undef protected

namespace
{
    /* An OSC string: the bytes, a NUL, padded to 4. */
    QByteArray oscString(const QByteArray &s)
    {
        QByteArray out = s;
        out.append('\0');
        while (out.size() % 4)
            out.append('\0');
        return out;
    }

    QByteArray be32(quint32 v)
    {
        QByteArray out(4, '\0');
        qToBigEndian(v, out.data());
        return out;
    }

    QByteArray floatMessage(const QByteArray &path, float f)
    {
        quint32 bits;
        memcpy(&bits, &f, 4);
        return oscString(path) + oscString(",f") + be32(bits);
    }

    QByteArray bundleHeader()
    {
        return QByteArray("#bundle", 8) + QByteArray(8, '\0');
    }

    quint16 freeUdpPort()
    {
        QUdpSocket s;
        s.bind(QHostAddress::LocalHost, 0);
        return s.localPort();
    }
}

void OSC_Test::validMessagesStillParse()
{
    OSCPacketizer p;
    const QList<QPair<QString, QByteArray>> m = p.parsePacket(floatMessage("/1/fader1", 0.5f));
    QCOMPARE(m.count(), 1);
    QCOMPARE(m.first().first, QString("/1/fader1"));
    QCOMPARE(m.first().second.size(), 1);
    QCOMPARE(uchar(m.first().second.at(0)), uchar(127));
}

void OSC_Test::validBundlesStillParse()
{
    OSCPacketizer p;
    const QByteArray a = floatMessage("/a", 1.0f);
    const QByteArray b = floatMessage("/b", 0.0f);
    const QByteArray bundle = bundleHeader() + be32(a.size()) + a + be32(b.size()) + b;
    const QList<QPair<QString, QByteArray>> m = p.parsePacket(bundle);
    QCOMPARE(m.count(), 2);
    QCOMPARE(m.at(0).first, QString("/a"));
    QCOMPARE(m.at(1).first, QString("/b"));
}

/* A bundle element size of 0xFFFFFFFC is -4 as an int: the parser stepped
   forward 4 for the size and back 4 for the "message", forever. One UDP
   datagram froze the thread that reads OSC input (the GUI thread). */
void OSC_Test::aBundleWithANegativeSizeEnds()
{
    HOSTILE_CASE(5000);
    OSCPacketizer p;
    const QByteArray bundle = bundleHeader() + be32(0xFFFFFFFC) + floatMessage("/a", 1.0f);
    p.parsePacket(bundle);
}

/* A size past the end, but positive as an int, must not read past it. */
void OSC_Test::aBundleWithAHugeSizeEnds()
{
    HOSTILE_CASE(5000);
    OSCPacketizer p;
    p.parsePacket(bundleHeader() + be32(0x7FFFFFF0) + floatMessage("/a", 1.0f));
    p.parsePacket(bundleHeader() + be32(0x80000000) + floatMessage("/a", 1.0f));
}

/* A bundle that ends one to three bytes into an element size. */
void OSC_Test::aBundleCutInsideASizeIsDropped()
{
    HOSTILE_CASE(5000);
    OSCPacketizer p;
    for (int extra = 1; extra <= 3; extra++)
    {
        const QByteArray bundle = bundleHeader() + QByteArray(4, '\0') + QByteArray(extra, '\x01');
        p.parsePacket(bundle);
    }
}

/* A type tag string with no NUL: the tag loop walked off the end
   (QByteArray::at() past size: an assert in Debug). */
void OSC_Test::tagsWithNoEndAreDropped()
{
    HOSTILE_CASE(5000);
    OSCPacketizer p;
    p.parsePacket(oscString("/a") + QByteArray(",fffffff"));
    p.parsePacket(QByteArray("/a,"));
    p.parsePacket(QByteArray(","));
}

void OSC_Test::aStringWithNoEndIsDropped()
{
    HOSTILE_CASE(5000);
    OSCPacketizer p;
    p.parsePacket(oscString("/a") + oscString(",si") + QByteArray("no terminator here"));
    p.parsePacket(oscString("/a") + oscString(",ssssssss") + QByteArray("x"));
}

/* NaN, infinities and floats far out of 0..1 became (char)(255 * f): undefined
   behaviour, and in practice any value. They must come out as 0..255. */
void OSC_Test::outOfRangeNumbersAreClamped()
{
    OSCPacketizer p;
    struct { float in; uchar out; } cases[] = {
        { std::numeric_limits<float>::quiet_NaN(), 0 },
        { std::numeric_limits<float>::infinity(), 255 },
        { -std::numeric_limits<float>::infinity(), 0 },
        { 1e30f, 255 },
        { -1e30f, 0 },
        { 2.0f, 255 },
        { -0.5f, 0 },
    };
    for (const auto &c : cases)
    {
        const QList<QPair<QString, QByteArray>> m = p.parsePacket(floatMessage("/x", c.in));
        QCOMPARE(m.count(), 1);
        QCOMPARE(m.first().second.size(), 1);
        QVERIFY2(uchar(m.first().second.at(0)) == c.out,
                 qPrintable(QString("%1 gave %2, wanted %3").arg(double(c.in))
                            .arg(uchar(m.first().second.at(0))).arg(c.out)));
    }
}

/* A fixed seed: 20000 random and mutated datagrams. */
void OSC_Test::randomDatagramsNeverCrash()
{
    HOSTILE_CASE(60000);
    QRandomGenerator rng(20261027);
    OSCPacketizer p;
    const QByteArray seeds[] = {
        floatMessage("/1/fader1", 0.5f),
        bundleHeader() + be32(12) + floatMessage("/a", 1.0f),
        oscString("/a") + oscString(",sifdtb") + oscString("s") + be32(1),
    };
    for (int i = 0; i < 20000; i++)
    {
        QByteArray d = seeds[i % 3];
        const int flips = 1 + int(rng.bounded(8));
        for (int f = 0; f < flips; f++)
        {
            switch (rng.bounded(3))
            {
                case 0:
                    if (!d.isEmpty())
                        d[int(rng.bounded(d.size()))] = char(rng.bounded(256));
                break;
                case 1:
                    d.truncate(int(rng.bounded(d.size() + 1)));
                break;
                default:
                    d.append(char(rng.bounded(256)));
                break;
            }
        }
        p.parsePacket(d);
    }
}

/* The controller for 127.0.0.1 listened for OSC on every interface: a sender
   on any network the computer is on (the venue's Wi-Fi) reached the input. */
void OSC_Test::theInputListensOnlyOnItsInterface()
{
    OSCController c(QStringLiteral("127.0.0.1"), OSCController::Input, 0);
    c.addUniverse(0, OSCController::Input);
    c.setInputPort(0, freeUdpPort());
    UniverseInfo *info = c.getUniverseInfo(0);
    QVERIFY(info != nullptr);
    QVERIFY(info->inputSocket);
    QCOMPARE(info->inputSocket->localAddress(), QHostAddress(QHostAddress::LocalHost));
}

/* Every distinct multi-value path was kept for the life of the session: a
   sender inventing paths grows memory without end. */
void OSC_Test::manyPathsDoNotGrowMemoryWithoutBound()
{
    OSCController c(QStringLiteral("127.0.0.1"), OSCController::Input, 0);
    c.addUniverse(0, OSCController::Input);
    c.setInputPort(0, freeUdpPort());
    UniverseInfo *info = c.getUniverseInfo(0);
    QVERIFY(info != nullptr && info->inputSocket);
    for (int i = 0; i < 20000; i++)
    {
        const QByteArray path = "/flood/" + QByteArray::number(i);
        const QByteArray msg = oscString(path) + oscString(",ff") + be32(0) + be32(0);
        c.handlePacket(info->inputSocket.data(), msg, QHostAddress::LocalHost);
    }
    QVERIFY2(info->multipartCache.size() <= 4096,
             qPrintable(QString("%1 paths kept").arg(info->multipartCache.size())));
}

/* Behaviour, not a bug: anyone who can send UDP to the port drives the
   input -- there is no allow-list of senders. This pins what loopback does
   so a future allow-list has a test to change. */
void OSC_Test::aLoopbackSenderDrivesTheInput()
{
    OSCController c(QStringLiteral("127.0.0.1"), OSCController::Input, 0);
    c.addUniverse(0, OSCController::Input);
    const quint16 port = freeUdpPort();
    c.setInputPort(0, port);
    QSignalSpy spy(&c, SIGNAL(valueChanged(quint32,quint32,quint32,uchar,QString)));
    QUdpSocket tx;
    tx.writeDatagram(floatMessage("/1/go", 1.0f), QHostAddress::LocalHost, port);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 3000);
    QCOMPARE(spy.first().at(4).toString(), QString("/1/go"));
}

QTEST_GUILESS_MAIN(OSC_Test)
