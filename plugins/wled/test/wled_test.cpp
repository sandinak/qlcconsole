/*
  Q Light Controller Plus
  wled_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QJsonDocument>
#include <QJsonArray>
#include <QTcpServer>
#include <QTcpSocket>
#include <QPointer>
#include <QtTest>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>

#include "wled_test.h"
#include "wledplugin.h"
#include "wledstate.h"

namespace
{
    /** A stand-in WLED: answers /json/state and /json/info, and keeps what
     *  was posted to it. */
    class FakeWled : public QTcpServer
    {
    public:
        QList<QJsonObject> posts;
        int rssi = -60;
        int liveAsked = 0;                  ///< GET /json/live count
        QByteArray live = "{\"leds\":[\"FF0000\",\"00FF00\",\"0000FF\",\"102030\"],\"n\":1}";

        FakeWled()
        {
            connect(this, &QTcpServer::newConnection, this, [this]() {
                while (QTcpSocket *s = nextPendingConnection())
                {
                    connect(s, &QTcpSocket::readyRead, s, [this, s]() {
                        m_buffer[s] += s->readAll();
                        const QByteArray req = m_buffer[s];
                        const int head = req.indexOf("\r\n\r\n");
                        if (head < 0)
                            return;
                        int length = 0;
                        for (const QByteArray &line : req.left(head).split('\n'))
                            if (line.toLower().startsWith("content-length:"))
                                length = line.mid(15).trimmed().toInt();
                        if (req.size() < head + 4 + length)
                            return;
                        const QByteArray body = req.mid(head + 4, length);
                        m_buffer.remove(s);
                        QByteArray reply = "{\"success\":true}";
                        if (req.startsWith("POST /json/state"))
                            posts << QJsonDocument::fromJson(body).object();
                        else if (req.startsWith("GET /json/info"))
                            reply = QString("{\"name\":\"Lantern SL\",\"ver\":\"0.15.0\","
                                            "\"leds\":{\"count\":16},"
                                            "\"wifi\":{\"rssi\":%1}}").arg(rssi).toUtf8();
                        else if (req.startsWith("GET /json/live"))
                        {
                            ++liveAsked;
                            reply = live;
                        }
                        s->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                                 "Content-Length: " + QByteArray::number(reply.size()) + "\r\n\r\n" + reply);
                    });
                    connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
                    m_sockets << s;
                }
            });
        }
        /** Switched off: no new connections, and the open ones dropped. */
        void powerOff()
        {
            close();
            for (QTcpSocket *s : m_sockets)
                if (s != nullptr)
                    s->abort();
        }
    private:
        QHash<QTcpSocket *, QByteArray> m_buffer;
        QList<QPointer<QTcpSocket> > m_sockets;
    };

    QByteArray universe(const QList<int> &bytes, int at = 0)
    {
        QByteArray u(512, char(0));
        for (int i = 0; i < bytes.size(); ++i)
            u[at + i] = char(bytes.at(i));
        return u;
    }
}

/* WLED's own DMX "Effect" mode, channel for channel: what the WLED Effect
   and WLED Lantern definitions drive. */
void Wled_Test::effectChannelsAreReadAsWledReadsThem()
{
    // At address 11: dimmer, effect 9, speed, intensity, palette 6, option 1 (mirror), 3 colours.
    const QByteArray u = universe({ 200, 9, 128, 64, 6, 70, 255, 0, 0, 0, 255, 0, 0, 0, 255 }, 10);
    const QJsonObject s = WledState::decode(u, 11, 15, 100);
    QCOMPARE(s.value("on").toBool(), true);
    QCOMPARE(s.value("bri").toInt(), 200);
    QCOMPARE(s.value("transition").toInt(), 1);
    const QJsonObject seg = s.value("seg").toArray().first().toObject();
    QCOMPARE(seg.value("fx").toInt(), 9);
    QCOMPARE(seg.value("sx").toInt(), 128);
    QCOMPARE(seg.value("ix").toInt(), 64);
    QCOMPARE(seg.value("pal").toInt(), 6);
    QCOMPARE(seg.value("mi").toBool(), true);
    QCOMPARE(seg.value("rev").toBool(), false);
    const QJsonArray col = seg.value("col").toArray();
    QCOMPARE(col.size(), 3);
    QCOMPARE(col.at(0).toArray(), (QJsonArray{ 255, 0, 0 }));
    QCOMPARE(col.at(1).toArray(), (QJsonArray{ 0, 255, 0 }));
    QCOMPARE(col.at(2).toArray(), (QJsonArray{ 0, 0, 255 }));

    // Dimmer at zero: WLED has no brightness 0 -- it is off.
    const QJsonObject off = WledState::decode(universe({ 0 }), 1, 15);
    QCOMPARE(off.value("on").toBool(), false);
    QVERIFY(off.value("bri").toInt() >= 1);
}

void Wled_Test::colourModesAreSolidColours()
{
    const QJsonObject drgb = WledState::decode(universe({ 128, 10, 20, 30 }), 1, 4);
    QCOMPARE(drgb.value("bri").toInt(), 128);
    QJsonObject seg = drgb.value("seg").toArray().first().toObject();
    QCOMPARE(seg.value("fx").toInt(), 0);
    QCOMPARE(seg.value("col").toArray().first().toArray(), (QJsonArray{ 10, 20, 30 }));

    const QJsonObject rgb = WledState::decode(universe({ 10, 20, 30 }), 1, 3);
    QCOMPARE(rgb.value("bri").toInt(), 255);
    seg = rgb.value("seg").toArray().first().toObject();
    QCOMPARE(seg.value("col").toArray().first().toArray(), (QJsonArray{ 10, 20, 30 }));
}

/* What makes it better than Art-Net on Wi-Fi: a held look is sent once; a
   fade of fifty changes goes out as a handful of states, the last of them
   the look it ended on; the first one takes WLED out of realtime mode. */
void Wled_Test::onlyChangesAreSentAndBurstsCollapse()
{
    FakeWled wled;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    WledPlugin plugin;
    plugin.init();
    plugin.setSendInterval(40);
    QVERIFY(plugin.openOutput(0, 0));
    plugin.setParameter(0, 0, QLCIOPlugin::Output, WLED_OUTPUTIP,
                        QString("127.0.0.1:%1").arg(wled.serverPort()));
    plugin.setParameter(0, 0, QLCIOPlugin::Output, WLED_CHANNELS, 15);

    const QByteArray held = universe({ 255, 0, 128, 128, 0, 0, 255, 0, 0 });
    for (int i = 0; i < 20; ++i)
        plugin.writeUniverse(0, 0, held, i == 0);
    QTRY_COMPARE(wled.posts.size(), 1);
    QCOMPARE(wled.posts.first().value("live").toBool(true), false);
    QTest::qWait(300);
    QCOMPARE(wled.posts.size(), 1);                    // held: nothing more

    // A fade: fifty steps as fast as they come.
    for (int v = 1; v <= 50; ++v)
    {
        plugin.writeUniverse(0, 0, universe({ 255, 0, 128, 128, 0, 0, 255 - v * 5, 0, v * 5 }), true);
        QTest::qWait(4);
    }
    QTest::qWait(400);
    const int sent = wled.posts.size() - 1;
    QVERIFY2(sent >= 1 && sent <= 12, qPrintable(QString("%1 states for 50 changes").arg(sent)));
    const QJsonArray last = wled.posts.last().value("seg").toArray().first().toObject()
                                .value("col").toArray().first().toArray();
    QCOMPARE(last, (QJsonArray{ 5, 0, 250 }));
    QVERIFY(!wled.posts.last().contains("live"));    // only the first time
    QVERIFY(plugin.targetProblems().isEmpty());
}

void Wled_Test::aWeakOrSilentLanternIsAProblem()
{
    FakeWled wled;
    wled.rssi = -82;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    WledPlugin plugin;
    plugin.init();
    plugin.setSendInterval(40);
    plugin.setInfoInterval(40);
    QVERIFY(plugin.openOutput(0, 3));
    const QString ip = QString("127.0.0.1:%1").arg(wled.serverPort());
    plugin.setParameter(3, 0, QLCIOPlugin::Output, WLED_OUTPUTIP, ip);
    QSignalSpy changed(&plugin, &QLCIOPlugin::targetProblemsChanged);
    QSignalSpy reconfigured(&plugin, &QLCIOPlugin::configurationChanged);

    QTRY_VERIFY(plugin.targetProblems().join(";").contains("weak Wi-Fi (-82 dBm)"));
    QTRY_VERIFY(changed.count() >= 1);
    QVERIFY(plugin.outputInfo(0).contains("Lantern SL"));

    // Gone: not answering.
    wled.powerOff();
    plugin.writeUniverse(3, 0, universe({ 255, 1 }), true);
    QTRY_VERIFY_WITH_TIMEOUT(plugin.targetProblems().join(";").contains(ip + " — not answering"), 8000);

    /* "patched the lanterns and the app appears hung": a problem is never a
       configurationChanged() -- that reconnects every output under
       InputOutputMap's lock, and closing an output came straight back here.
       Closing one says nothing there and then, either. */
    QCOMPARE(reconfigured.count(), 0);
    const int before = changed.count();
    plugin.closeOutput(0, 3);
    QCOMPARE(changed.count(), before);                  // not from inside the close
    QTRY_VERIFY(changed.count() == before + 1);         // afterwards
    QCOMPARE(reconfigured.count(), 0);
}

/* The rig view asks what a lantern is SHOWING -- its effect worked out on
   the lantern -- and only while it is watching: on Wi-Fi every request
   counts. */
void Wled_Test::aWatchedLanternSaysWhatItsLedsShow()
{
    FakeWled wled;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    WledPlugin plugin;
    plugin.init();
    plugin.setInfoInterval(40);
    plugin.setLiveInterval(40);
    QVERIFY(plugin.openOutput(0, 2));
    plugin.setParameter(2, 0, QLCIOPlugin::Output, WLED_OUTPUTIP,
                        QString("127.0.0.1:%1").arg(wled.serverPort()));

    // How many LEDs it drives, from its info.
    QTRY_COMPARE(plugin.targetPixelCount(2), 16);
    // Not watched: never asked.
    QTest::qWait(200);
    QCOMPARE(wled.liveAsked, 0);
    QVERIFY(plugin.targetPixels(2).isEmpty());

    QSignalSpy pixels(&plugin, &QLCIOPlugin::targetPixelsChanged);
    plugin.watchTargetPixels(2, true);
    QTRY_COMPARE(plugin.targetPixels(2), (QVector<quint32>{ 0xFF0000, 0x00FF00, 0x0000FF, 0x102030 }));
    QVERIFY(pixels.count() >= 1);
    QCOMPARE(pixels.first().first().toUInt(), 2u);

    // It moves on; so does the answer -- and only a change is signalled.
    const int before = pixels.count();
    wled.live = "{\"leds\":[\"000000\",\"FFFFFF\"],\"n\":1}";
    QTRY_COMPARE(plugin.targetPixels(2), (QVector<quint32>{ 0x000000, 0xFFFFFF }));
    QTest::qWait(200);
    QCOMPARE(pixels.count(), before + 1);

    // Watched twice, let go once: still watched. Let go again: not asked.
    plugin.watchTargetPixels(2, true);
    plugin.watchTargetPixels(2, false);
    const int asked = wled.liveAsked;
    QTRY_VERIFY(wled.liveAsked > asked);
    plugin.watchTargetPixels(2, false);
    QVERIFY(plugin.targetPixels(2).isEmpty());
    QTest::qWait(100);                  // one may have been in flight
    const int stopped = wled.liveAsked;
    QTest::qWait(250);
    QCOMPARE(wled.liveAsked, stopped);

    // A watch outlives its output closing and opening again.
    plugin.watchTargetPixels(2, true);
    plugin.closeOutput(0, 2);
    QVERIFY(plugin.openOutput(0, 2));
    plugin.setParameter(2, 0, QLCIOPlugin::Output, WLED_OUTPUTIP,
                        QString("127.0.0.1:%1").arg(wled.serverPort()));
    QTRY_VERIFY(!plugin.targetPixels(2).isEmpty());
    plugin.watchTargetPixels(2, false);
}

QTEST_MAIN(Wled_Test)

namespace
{
QJsonObject httpJson(QNetworkAccessManager &nam, const QString &url,
                     const QByteArray &post = QByteArray())
{
    QNetworkRequest req{ QUrl(url) };
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(4000);
    QNetworkReply *r = post.isEmpty() ? nam.get(req) : nam.post(req, post);
    QSignalSpy done(r, &QNetworkReply::finished);
    done.wait(5000);
    const QJsonObject o = QJsonDocument::fromJson(r->readAll()).object();
    r->deleteLater();
    return o;
}
}

void Wled_Test::aRealLanternShowsTheLook()
{
    const QString ip = qEnvironmentVariable("WLED_REAL_IP");
    if (ip.isEmpty())
        QSKIP("set WLED_REAL_IP to run against a real WLED");

    QNetworkAccessManager nam;
    const QJsonObject before = httpJson(nam, QString("http://%1/json/state").arg(ip));
    QVERIFY2(!before.isEmpty(), "the device did not answer /json/state");

    WledPlugin plugin;
    plugin.init();
    plugin.setSendInterval(40);
    plugin.setInfoInterval(200);
    QVERIFY(plugin.openOutput(0, 5));
    plugin.setParameter(5, 0, QLCIOPlugin::Output, WLED_OUTPUTIP, ip);
    plugin.setParameter(5, 0, QLCIOPlugin::Output, WLED_ADDRESS, 1);
    plugin.setParameter(5, 0, QLCIOPlugin::Output, WLED_CHANNELS, 15);

    // Dim red, solid: brightness 64, effect 0, colour 1 = 255,0,0.
    plugin.writeUniverse(5, 0, universe({ 64, 0, 128, 128, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0 }), true);
    QJsonObject now;
    QTRY_VERIFY_WITH_TIMEOUT((now = httpJson(nam, QString("http://%1/json/state").arg(ip)),
                              now.value("bri").toInt() == 64), 6000);
    const QJsonObject seg = now.value("seg").toArray().at(0).toObject();
    QCOMPARE(now.value("on").toBool(), true);
    QCOMPARE(seg.value("fx").toInt(), 0);
    const QJsonArray col = seg.value("col").toArray().at(0).toArray();
    QCOMPARE(col.at(0).toInt(), 255);
    QCOMPARE(col.at(1).toInt(), 0);
    QCOMPARE(col.at(2).toInt(), 0);

    // It answers, on usable Wi-Fi: not a problem.
    QTRY_VERIFY_WITH_TIMEOUT(plugin.outputInfo(0).contains("WLED "), 6000);
    QVERIFY2(plugin.targetProblems().isEmpty(), qPrintable(plugin.targetProblems().join("; ")));

    // Closing the output (as a reconnect does) says nothing from inside it.
    QSignalSpy reconfigured(&plugin, &QLCIOPlugin::configurationChanged);
    plugin.closeOutput(0, 5);
    QCOMPARE(reconfigured.count(), 0);

    // As it was.
    QJsonObject restore;
    restore["on"] = before.value("on");
    restore["bri"] = before.value("bri");
    restore["seg"] = before.value("seg");
    httpJson(nam, QString("http://%1/json/state").arg(ip), QJsonDocument(restore).toJson());
}
