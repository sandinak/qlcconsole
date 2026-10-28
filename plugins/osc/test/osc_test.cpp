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
#include "oscplugin.h"
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

namespace
{
    float floatAt(const QByteArray &d, int at)
    {
        const quint32 bits = qFromBigEndian<quint32>(d.constData() + at);
        float f;
        memcpy(&f, &bits, 4);
        return f;
    }

    /* The next datagram on @p rx, or an empty one when none comes. A send
       to 127.0.0.1 has arrived by the time writeDatagram() returns on every
       OS this runs on; the wait is only a bound. */
    QByteArray nextDatagram(QUdpSocket &rx, int waitMs = 3000)
    {
        if (!rx.hasPendingDatagrams() && !rx.waitForReadyRead(waitMs))
            return QByteArray();
        QByteArray d(int(rx.pendingDatagramSize()), '\0');
        rx.readDatagram(d.data(), d.size());
        return d;
    }

    /* The one message in @p d, as the input would read it */
    bool parseOne(const QByteArray &d, QString &path, QByteArray &values)
    {
        const QList<QPair<QString, QByteArray> > msgs = OSCPacketizer().parsePacket(d);
        if (msgs.count() != 1)
            return false;
        path = msgs.first().first;
        values = msgs.first().second;
        return true;
    }
}

void OSC_Test::dmxAndGenericMessageLayout()
{
    OSCPacketizer p;
    QByteArray d;

    // "/1/dmx/23" is 9 bytes: a NUL and padding to 12, ",f" to 4, a float
    p.setupOSCDmx(d, 1, 23, 255);
    QCOMPARE(d.size(), 20);
    QCOMPARE(d.left(12), QByteArray("/1/dmx/23\0\0\0", 12));
    QCOMPARE(d.mid(12, 4), QByteArray(",f\0\0", 4));
    QCOMPARE(floatAt(d, 16), 1.0f);
    // A path that fills its 4 bytes still gets its NUL: 4 more
    p.setupOSCDmx(d, 0, 0, 51);
    QCOMPARE(d.left(12), QByteArray("/0/dmx/0\0\0\0\0", 12));
    QCOMPARE(floatAt(d, 16), 0.2f);

    QString path;
    QByteArray values;
    QVERIFY(parseOne(d, path, values));
    QCOMPARE(path, QString("/0/dmx/0"));
    QCOMPARE(values, QByteArray(1, char(51)));

    // Generic: several floats, the type tags padded too
    QString gpath("/xy");
    QByteArray gvals = QByteArray::fromHex("00ff");
    p.setupOSCGeneric(d, gpath, "ff", gvals);
    QCOMPARE(d.size(), 4 + 4 + 8);
    QCOMPARE(d.left(8), QByteArray("/xy\0,ff\0", 8));
    QCOMPARE(floatAt(d, 8), 0.0f);
    QCOMPARE(floatAt(d, 12), 1.0f);
    QVERIFY(parseOne(d, path, values));
    QCOMPARE(path, QString("/xy"));
    QCOMPARE(values, gvals);

    QString none;
    p.setupOSCGeneric(d, none, "f", gvals);
    QVERIFY(d.isEmpty());
}

void OSC_Test::universeDefaultsAndTheirReturns()
{
    OSCController c(QStringLiteral("127.0.0.1"), OSCController::Output, 3);
    QCOMPARE(c.line(), 3u);
    QCOMPARE(c.getNetworkIP(), QHostAddress(QHostAddress::LocalHost));
    c.addUniverse(2, OSCController::Output);
    UniverseInfo *info = c.getUniverseInfo(2);
    QVERIFY(info != nullptr);
    QCOMPARE(int(info->inputPort), 7702);
    QCOMPARE(int(info->outputPort), 9002);
    QCOMPARE(int(info->feedbackPort), 9002);
    QCOMPARE(info->outputAddress, QHostAddress(QHostAddress::LocalHost));
    QCOMPARE(info->feedbackAddress, QHostAddress(QHostAddress::LocalHost));
    QVERIFY(info->inputSocket.isNull());                 // an output listens to nothing
    QCOMPARE(int(c.type()), int(OSCController::Output));

    /* The setters say whether the value is the default: the plugin then
       stores no parameter. */
    QCOMPARE(c.setOutputPort(2, 9002), true);
    QCOMPARE(c.setOutputPort(2, 9100), false);
    QCOMPARE(c.setFeedbackPort(2, 9002), true);
    QCOMPARE(c.setFeedbackPort(2, 9101), false);
    QCOMPARE(c.setOutputIPAddress(2, "127.0.0.1"), true);
    QCOMPARE(c.setOutputIPAddress(2, "10.0.0.5"), false);
    QCOMPARE(c.getUniverseInfo(2)->outputAddress, QHostAddress("10.0.0.5"));
    QCOMPARE(c.setFeedbackIPAddress(2, "127.0.0.1"), true);
    QCOMPARE(c.setFeedbackIPAddress(2, "10.0.0.6"), false);
    QCOMPARE(int(c.getUniverseInfo(2)->outputPort), 9100);
    QCOMPARE(int(c.getUniverseInfo(2)->feedbackPort), 9101);

    // Not ours
    QCOMPARE(c.setOutputPort(9, 9009), false);
    QCOMPARE(c.setFeedbackPort(9, 9009), false);
    QCOMPARE(c.setOutputIPAddress(9, "127.0.0.1"), false);
    QCOMPARE(c.setFeedbackIPAddress(9, "127.0.0.1"), false);
    QCOMPARE(c.setInputPort(9, 7709), false);
    QVERIFY(c.getUniverseInfo(9) == nullptr);

    // On a real address the default target is nobody until set
    OSCController lan(QStringLiteral("172.18.2.219"), OSCController::Output, 0);
    lan.addUniverse(0, OSCController::Output);
    QVERIFY(lan.getUniverseInfo(0)->outputAddress.isNull());
    QCOMPARE(lan.setOutputIPAddress(0, ""), true);
    QCOMPARE(lan.setOutputIPAddress(0, "172.18.2.10"), false);

    c.removeUniverse(2, OSCController::Output);
    QCOMPARE(c.universesList(), QList<quint32>());
}

void OSC_Test::onlyChangedChannelsAreSent()
{
    QUdpSocket rx;
    QVERIFY(rx.bind(QHostAddress::LocalHost, 0));
    OSCController c(QStringLiteral("127.0.0.1"), OSCController::Output, 0);
    c.addUniverse(1, OSCController::Output);
    c.setOutputPort(1, rx.localPort());

    QByteArray dmx(512, '\0');
    dmx[0] = char(255);
    dmx[9] = char(51);
    c.sendDmx(1, dmx);
    QString path;
    QByteArray values;
    QVERIFY(parseOne(nextDatagram(rx), path, values));
    QCOMPARE(path, QString("/1/dmx/0"));
    QCOMPARE(values, QByteArray(1, char(255)));
    QVERIFY(parseOne(nextDatagram(rx), path, values));
    QCOMPARE(path, QString("/1/dmx/9"));
    QCOMPARE(values, QByteArray(1, char(51)));
    QCOMPARE(c.getPacketSentNumber(), quint64(2));

    // The same frame again: nothing; one channel back to 0: that one
    c.sendDmx(1, dmx);
    dmx[9] = '\0';
    c.sendDmx(1, dmx);
    QVERIFY(parseOne(nextDatagram(rx), path, values));
    QCOMPARE(path, QString("/1/dmx/9"));
    QCOMPARE(values, QByteArray(1, '\0'));
    QVERIFY(nextDatagram(rx, 50).isEmpty());
    QCOMPARE(c.getPacketSentNumber(), quint64(3));

    // Held off the rig network: nothing
    c.m_held.store(true);
    dmx[1] = char(1);
    c.sendDmx(1, dmx);
    QVERIFY(nextDatagram(rx, 50).isEmpty());
}

void OSC_Test::feedbackKeepsMultiValuePaths()
{
    QUdpSocket rx;
    QVERIFY(rx.bind(QHostAddress::LocalHost, 0));
    OSCController c(QStringLiteral("127.0.0.1"), OSCController::Input, 0);
    c.addUniverse(0, OSCController::Input);
    c.setInputPort(0, freeUdpPort());
    c.setFeedbackPort(0, rx.localPort());
    QString path;
    QByteArray values;

    // A single value
    c.sendFeedback(0, 0, 255, "/1/fader1");
    QVERIFY(parseOne(nextDatagram(rx), path, values));
    QCOMPARE(path, QString("/1/fader1"));
    QCOMPARE(values, QByteArray(1, char(255)));

    /* "/xy_1" is the second value of "/xy": the message carries both, the
       other one as last sent */
    c.sendFeedback(0, 0, 51, "/1/xy_1");
    QVERIFY(parseOne(nextDatagram(rx), path, values));
    QCOMPARE(path, QString("/1/xy"));
    QCOMPARE(values, QByteArray::fromHex("0033"));
    c.sendFeedback(0, 0, 255, "/1/xy_0");
    QVERIFY(parseOne(nextDatagram(rx), path, values));
    QCOMPARE(path, QString("/1/xy"));
    QCOMPARE(values, QByteArray::fromHex("ff33"));

    // No key: the path the channel's hash came from
    const quint16 hash = c.getHash("/2/push3");
    c.sendFeedback(0, hash, 255, QString());
    QVERIFY(parseOne(nextDatagram(rx), path, values));
    QCOMPARE(path, QString("/2/push3"));

    // No key and no such hash: no message at all
    const quint64 sent = c.getPacketSentNumber();
    c.sendFeedback(0, quint16(hash + 1), 255, QString());
    QVERIFY(nextDatagram(rx, 50).isEmpty());
    QCOMPARE(c.getPacketSentNumber(), sent);
}

void OSC_Test::multiValueInputIsSplitPerValue()
{
    OSCController c(QStringLiteral("127.0.0.1"), OSCController::Input, 4);
    c.addUniverse(0, OSCController::Input);
    c.setInputPort(0, freeUdpPort());
    QSignalSpy spy(&c, SIGNAL(valueChanged(quint32,quint32,quint32,uchar,QString)));
    UniverseInfo *info = c.getUniverseInfo(0);

    // Two floats on one path: two values, "/xy_0" and "/xy_1"
    QByteArray msg = oscString("/xy") + oscString(",ff") + be32(0) + be32(0x3f800000);
    c.handlePacket(info->inputSocket.data(), msg, QHostAddress::LocalHost);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).toUInt(), 0u);
    QCOMPARE(spy.at(0).at(1).toUInt(), 4u);
    QCOMPARE(spy.at(0).at(4).toString(), QString("/xy_0"));
    QCOMPARE(int(spy.at(0).at(3).value<uchar>()), 0);
    QCOMPARE(spy.at(1).at(4).toString(), QString("/xy_1"));
    QCOMPARE(int(spy.at(1).at(3).value<uchar>()), 255);
    QCOMPARE(spy.at(1).at(2).toUInt(), uint(c.getHash("/xy_1")));
    QCOMPARE(info->multipartCache.value("/xy"), QByteArray::fromHex("00ff"));
    QCOMPARE(c.getPacketReceivedNumber(), quint64(1));

    // A socket that isn't this universe's: counted, not delivered
    QUdpSocket other;
    c.handlePacket(&other, floatMessage("/1/go", 1.0f), QHostAddress::LocalHost);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(c.getPacketReceivedNumber(), quint64(2));

    // A message with no values: nothing
    c.handlePacket(info->inputSocket.data(), oscString("/1/go") + oscString(","), QHostAddress::LocalHost);
    QCOMPARE(spy.count(), 2);
}

void OSC_Test::pluginKeepsOnlyWhatDiffersFromTheDefault()
{
    OSCPlugin plugin;
    OSCIO io;
    io.IPAddress = "127.0.0.1";
    io.controller = nullptr;
    plugin.m_IOmapping.clear();
    plugin.m_IOmapping.append(io);
    typedef QLCIOPlugin P;

    QVERIFY(!plugin.openOutput(2, 1));                  // no such line
    QVERIFY(plugin.openOutput(0, 1));
    OSCController *ctrl = plugin.m_IOmapping.at(0).controller;
    QVERIFY(ctrl != nullptr);
    auto out = [&plugin]() { return plugin.getParameters(1, 0, P::Output); };

    QUdpSocket rx;
    QVERIFY(rx.bind(QHostAddress::LocalHost, 0));
    plugin.setParameter(1, 0, P::Output, OSC_OUTPUTPORT, rx.localPort());
    QCOMPARE(out().value(OSC_OUTPUTPORT).toUInt(), uint(rx.localPort()));
    plugin.setParameter(1, 0, P::Output, OSC_OUTPUTIP, "127.0.0.1");   // the default here
    QVERIFY(!out().contains(OSC_OUTPUTIP));
    plugin.setParameter(1, 0, P::Output, OSC_OUTPUTIP, "127.0.0.2");
    QCOMPARE(out().value(OSC_OUTPUTIP).toString(), QString("127.0.0.2"));
    plugin.setParameter(1, 0, P::Output, OSC_OUTPUTIP, "127.0.0.1");
    QVERIFY(!out().contains(OSC_OUTPUTIP));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("is not a valid OSC parameter"));
    plugin.setParameter(1, 0, P::Output, "colour", "red");
    QCOMPARE(out().count(), 1);

    // Its frames reach the controller and go out
    QByteArray dmx(512, '\0');
    dmx[4] = char(255);
    plugin.writeUniverse(1, 0, dmx, true);
    plugin.writeUniverse(1, 5, dmx, true);                // no such line
    QString path;
    QByteArray values;
    QVERIFY(parseOne(nextDatagram(rx), path, values));
    QCOMPARE(path, QString("/1/dmx/4"));

    // Feedback goes through the line's controller to its feedback port
    plugin.setParameter(1, 0, P::Output, OSC_FEEDBACKPORT, rx.localPort());
    QCOMPARE(out().value(OSC_FEEDBACKPORT).toUInt(), uint(rx.localPort()));
    plugin.sendFeedBack(1, 0, 0, 255, QVariant(QString("/1/toggle1")));
    plugin.sendFeedBack(1, 9, 0, 255, QVariant(QString("/1/toggle1")));   // no such line
    QVERIFY(parseOne(nextDatagram(rx), path, values));
    QCOMPARE(path, QString("/1/toggle1"));
    QVERIFY(nextDatagram(rx, 50).isEmpty());

    // Closed: its parameters go, and the controller with its last universe
    plugin.closeOutput(0, 1);
    QVERIFY(out().isEmpty());
    QVERIFY(plugin.m_IOmapping.at(0).controller == nullptr);
}

QTEST_GUILESS_MAIN(OSC_Test)
