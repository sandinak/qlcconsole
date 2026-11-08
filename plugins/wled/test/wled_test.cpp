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
#include <QElapsedTimer>
#include <QDateTime>
#include <QtTest>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QSettings>

#include "wled_test.h"
#include "../../../engine/test/common/stdprelude.h"
#define private public
#include "wledplugin.h"
#undef private
#include "wledstate.h"

namespace
{
    /** A stand-in WLED: answers /json/state, /json/info and /json/si, and
     *  keeps what was posted to it. Its segments are WLED's own: a "seg"
     *  array element applies to the segment its "id" names -- or, with no
     *  id, the one at its INDEX in the array (WLED's json.cpp,
     *  `byte id = elem["id"] | it`); an id it does not have, without a
     *  stop, is ignored; a "seg" object without an id applies to every
     *  selected segment (all of them, here). */
    class FakeWled : public QTcpServer
    {
    public:
        QList<QJsonObject> posts;
        QList<qint64> postedAt;             ///< when each came (ms since the epoch)
        int rssi = -60;
        bool hang = false;                  ///< takes connections, never answers
        bool noSi = false;                  ///< an old WLED: /json/si is 501
        int liveAsked = 0;                  ///< GET /json/live count
        int siAsked = 0;                    ///< GET /json/si count
        QString mac;                        ///< info.mac, when set
        QByteArray live = "{\"leds\":[\"FF0000\",\"00FF00\",\"0000FF\",\"102030\"],\"n\":1}";
        /** Its segments, as /json/state says them: one over all 16 LEDs. */
        QJsonArray segs{ QJsonObject{ { "id", 0 }, { "start", 0 }, { "stop", 16 }, { "rev", false },
                                      { "mi", false }, { "fx", 0 } } };

        /** Split as a fountain's Write Segments does: @p n spans of @p len,
         *  every second reversed. */
        void split(int n, int len)
        {
            segs = QJsonArray();
            for (int k = 0; k < n; ++k)
                segs << QJsonObject{ { "id", k }, { "start", k * len }, { "stop", (k + 1) * len },
                                     { "rev", k % 2 == 1 }, { "mi", false }, { "fx", 0 } };
        }
        QJsonObject seg(int id) const
        {
            for (const QJsonValue &v : segs)
                if (v.toObject().value("id").toInt() == id)
                    return v.toObject();
            return QJsonObject();
        }
        void apply(const QJsonObject &body)
        {
            auto into = [this](int id, const QJsonObject &o) {
                for (int k = 0; k < segs.size(); ++k)
                {
                    QJsonObject s = segs.at(k).toObject();
                    if (s.value("id").toInt() != id)
                        continue;
                    for (auto it = o.begin(); it != o.end(); ++it)
                        if (it.key() != "id")
                            s[it.key()] = it.value();
                    segs[k] = s;
                    return;
                }
                // Not one it has: ignored without a stop (none sent here).
            };
            const QJsonValue v = body.value("seg");
            if (v.isObject())
            {
                const QJsonObject o = v.toObject();
                if (o.contains("id"))
                    into(o.value("id").toInt(), o);
                else
                    for (const QJsonValue &s : std::as_const(segs))
                        into(s.toObject().value("id").toInt(), o);
            }
            const QJsonArray a = v.toArray();
            for (int i = 0; i < a.size(); ++i)
            {
                const QJsonObject o = a.at(i).toObject();
                into(o.value("id").toInt(i), o);
            }
        }
        QJsonObject state() const { return QJsonObject{ { "on", true }, { "seg", segs } }; }
        QJsonObject info() const
        {
            QJsonObject i = QJsonDocument::fromJson(QString("{\"name\":\"Lantern SL\",\"ver\":\"0.15.0\","
                                                            "\"leds\":{\"count\":16},"
                                                            "\"wifi\":{\"rssi\":%1}}").arg(rssi).toUtf8()).object();
            if (!mac.isEmpty())
                i["mac"] = mac;
            return i;
        }

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
                        if (hang)
                            return;
                        QByteArray reply = "{\"success\":true}";
                        QByteArray status = "200 OK";
                        if (req.startsWith("POST /json/state"))
                        {
                            posts << QJsonDocument::fromJson(body).object();
                            postedAt << QDateTime::currentMSecsSinceEpoch();
                            apply(posts.last());
                        }
                        else if (req.startsWith("GET /json/info"))
                            reply = QJsonDocument(info()).toJson(QJsonDocument::Compact);
                        else if (req.startsWith("GET /json/si"))
                        {
                            ++siAsked;
                            if (noSi)
                            {
                                status = "501 Not Implemented";
                                reply = "{\"error\":3}";
                            }
                            else
                                reply = QJsonDocument(QJsonObject{ { "state", state() }, { "info", info() } })
                                            .toJson(QJsonDocument::Compact);
                        }
                        else if (req.startsWith("GET /json/state"))
                            reply = QJsonDocument(state()).toJson(QJsonDocument::Compact);
                        else if (req.startsWith("GET /json/live"))
                        {
                            ++liveAsked;
                            reply = live;
                        }
                        s->write("HTTP/1.1 " + status + "\r\nContent-Type: application/json\r\n"
                                 "Content-Length: " + QByteArray::number(reply.size()) + "\r\n\r\n" + reply);
                    });
                    connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
                    m_sockets << s;
                }
            });
        }
        /** Switched off: no new connections, and the open ones dropped. */
        quint16 port = 0;                   ///< kept: switched back on, it is the same lantern
        bool powerOn()
        {
            return listen(QHostAddress::LocalHost, port) && (port = serverPort()) != 0;
        }
        QString ip() const { return QString("127.0.0.1:%1").arg(port); }
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

void Wled_Test::init()
{
    QSettings().remove("wled/layouts");
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
    // Collapsed: far fewer than one a change. Room for a slow shared CI
    // runner, which managed 15 where a desk machine sends under 12.
    QVERIFY2(sent >= 1 && sent <= 25, qPrintable(QString("%1 states for 50 changes").arg(sent)));
    const QJsonArray last = wled.posts.last().value("seg").toArray().first().toObject()
                                .value("col").toArray().first().toArray();
    QCOMPARE(last, (QJsonArray{ 5, 0, 250 }));
    QVERIFY(!wled.posts.last().contains("live"));    // only the first time
    QVERIFY(plugin.targetProblems().isEmpty());
}

void Wled_Test::aStepGoesAtOnceAndOnlyAFadeGlides()
{
    // The rule itself, at a send every 100 ms (a glide of a tenth).
    const QJsonObject lit = WledState::decode(universe({ 255, 9, 128, 128, 0, 0, 255, 0, 0 }), 1, 15, 100);
    QCOMPARE(lit.value("transition").toInt(), 1);
    QCOMPARE(WledPlugin::paced(lit, QJsonObject(), 0, 1000, 100).value("transition").toInt(), 0);
    const QJsonObject dimmer = WledState::decode(universe({ 200, 9, 128, 128, 0, 0, 255, 0, 0 }), 1, 15, 100);
    QCOMPARE(WledPlugin::paced(dimmer, lit, 900, 1000, 100).value("transition").toInt(), 1);   // a fade
    QCOMPARE(WledPlugin::paced(dimmer, lit, 500, 1000, 100).value("transition").toInt(), 0);   // after a stand
    const QJsonObject solid = WledState::decode(universe({ 255, 0, 128, 128, 0, 0, 255, 255, 255 }), 1, 15, 100);
    QCOMPARE(WledPlugin::paced(solid, lit, 950, 1000, 100).value("transition").toInt(), 0);    // another effect
    const QJsonObject dark = WledState::decode(universe({ 0, 0, 128, 128, 0, 0, 255, 255, 255 }), 1, 15, 100);
    QCOMPARE(WledPlugin::paced(dark, solid, 950, 1000, 100).value("transition").toInt(), 0);   // off
    QCOMPARE(WledPlugin::paced(solid, dark, 950, 1000, 100).value("transition").toInt(), 0);   // on
    QVERIFY(WledPlugin::sameLook(WledPlugin::paced(lit, QJsonObject(), 0, 1000, 100), lit));

    // On the wire: a look held, then a quick flash off and on, then a fade.
    FakeWled wled;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    WledPlugin plugin;
    plugin.init();
    plugin.setSendInterval(100);
    QVERIFY(plugin.openOutput(0, 0));
    plugin.setParameter(0, 0, QLCIOPlugin::Output, WLED_OUTPUTIP,
                        QString("127.0.0.1:%1").arg(wled.serverPort()));
    plugin.setParameter(0, 0, QLCIOPlugin::Output, WLED_CHANNELS, 15);
    plugin.writeUniverse(0, 0, universe({ 255, 9, 128, 128, 0, 0, 255, 0, 0 }), true);
    QTRY_COMPARE(wled.posts.size(), 1);
    QTest::qWait(300);
    plugin.writeUniverse(0, 0, universe({ 0, 9, 128, 128, 0, 0, 255, 0, 0 }), true);
    QTRY_COMPARE(wled.posts.size(), 2);
    plugin.writeUniverse(0, 0, universe({ 255, 9, 128, 128, 0, 0, 255, 0, 0 }), true);
    QTRY_COMPARE(wled.posts.size(), 3);
    for (int i = 0; i < 3; ++i)
        QCOMPARE(wled.posts.at(i).value("transition").toInt(-1), 0);
    QTest::qWait(300);
    // Held, the same look is not sent again for its transition alone.
    QCOMPARE(wled.posts.size(), 3);
    for (int v = 1; v <= 40; ++v)
    {
        plugin.writeUniverse(0, 0, universe({ 255 - v * 2, 9, 128, 128, 0, 0, 255, 0, 0 }), true);
        QTest::qWait(10);
    }
    QTest::qWait(300);
    int glided = 0;
    for (int i = 3; i < wled.posts.size(); ++i)
        glided += wled.posts.at(i).value("transition").toInt() == 1 ? 1 : 0;
    QVERIFY2(glided >= 1, qPrintable(QString("%1 of %2 fade states glided")
                                         .arg(glided).arg(wled.posts.size() - 3)));
}

namespace
{
    /** Waits (up to @p ms) for @p wled's post @p n, in 1 ms steps rather
     *  than QTRY's 50: how long it took to come, -1 if it never did. */
    qint64 waitForPost(const FakeWled &wled, int n, qint64 since, int ms = 2000)
    {
        QElapsedTimer t;
        t.start();
        while (wled.posts.size() <= n && t.elapsed() < ms)
            QTest::qWait(1);
        return wled.posts.size() > n ? wled.postedAt.at(n) - since : -1;
    }
}

/* UX-73: a snap reaches the device when it happens, not on the next send
   tick. The tick is a second here: anything it delivered would take
   hundreds of milliseconds. */
void Wled_Test::aSnapGoesAtOnceNotOnTheNextTick()
{
    FakeWled wled;
    QVERIFY(wled.powerOn());
    WledPlugin plugin;
    plugin.init();
    plugin.setSendInterval(1000);
    plugin.setInfoInterval(100000);
    QVERIFY(plugin.openOutput(0, 0));
    plugin.setParameter(0, 0, QLCIOPlugin::Output, WLED_OUTPUTIP, wled.ip());
    plugin.setParameter(0, 0, QLCIOPlugin::Output, WLED_CHANNELS, 15);

    // The first look: its segments asked, then it -- both at once.
    qint64 at = QDateTime::currentMSecsSinceEpoch();
    plugin.writeUniverse(0, 0, universe({ 255, 9, 128, 128, 0, 0, 255, 0, 0 }), true);
    qint64 took = waitForPost(wled, 0, at);
    QVERIFY2(took >= 0 && took < 400, qPrintable(QString("the first look took %1 ms").arg(took)));

    /* Snaps: dark, lit, another effect, dark... each one at once, with no
       glide. The slowest has room for a loaded machine; the middle one is
       what a snap costs. */
    QList<qint64> took5;
    const QList<QList<int> > snaps{ { 0, 9, 128, 128, 0, 0, 255, 0, 0 },
                                    { 255, 9, 128, 128, 0, 0, 255, 0, 0 },
                                    { 255, 42, 128, 128, 0, 0, 0, 0, 255 },
                                    { 0, 42, 128, 128, 0, 0, 0, 0, 255 },
                                    { 255, 0, 128, 128, 0, 0, 255, 255, 255 } };
    for (int k = 0; k < snaps.size(); ++k)
    {
        QTest::qWait(20);
        const int n = wled.posts.size();
        at = QDateTime::currentMSecsSinceEpoch();
        plugin.writeUniverse(0, 0, universe(snaps.at(k)), true);
        took = waitForPost(wled, n, at);
        QVERIFY2(took >= 0 && took < 250, qPrintable(QString("snap %1 took %2 ms").arg(k).arg(took)));
        QCOMPARE(wled.posts.at(n).value("transition").toInt(-1), 0);
        took5 << took;
    }
    std::sort(took5.begin(), took5.end());
    qInfo() << "snap to the device, ms:" << took5;
    QVERIFY2(took5.at(2) <= 30, qPrintable(QString("a snap takes %1 ms").arg(took5.at(2))));

    // A burst in one frame is one request; a held look, none.
    QTest::qWait(50);
    const int n = wled.posts.size();
    for (int i = 0; i < 5; ++i)
        plugin.writeUniverse(0, 0, universe({ 0, 0, 128, 128, 0, 0, 255, 255, 255 }), true);
    QVERIFY(waitForPost(wled, n, 0) >= 0);
    QTest::qWait(300);
    QCOMPARE(wled.posts.size(), n + 1);
}

/* A look moved in black (MarkPlanner): its effect, palette and colours went
   while it was off. The GO is then its brightness alone. */
void Wled_Test::aLookMovedInBlackGoesOnWithItsBrightnessAlone()
{
    FakeWled wled;
    QVERIFY(wled.powerOn());
    WledPlugin plugin;
    plugin.init();
    plugin.setSendInterval(100);
    plugin.setInfoInterval(100000);
    QVERIFY(plugin.openOutput(0, 0));
    plugin.setParameter(0, 0, QLCIOPlugin::Output, WLED_OUTPUTIP, wled.ip());
    plugin.setParameter(0, 0, QLCIOPlugin::Output, WLED_CHANNELS, 15);

    plugin.writeUniverse(0, 0, universe({ 0, 0, 128, 128, 0, 0, 255, 255, 255 }), true);
    QTRY_COMPARE(wled.posts.size(), 1);
    QTest::qWait(250);
    // Moved in black: the next look's effect and colours, still off.
    plugin.writeUniverse(0, 0, universe({ 0, 89, 200, 128, 6, 0, 255, 0, 0, 0, 0, 255 }), true);
    QTRY_COMPARE(wled.posts.size(), 2);
    QCOMPARE(wled.posts.at(1).value("on").toBool(true), false);
    QCOMPARE(wled.posts.at(1).value("seg").toArray().first().toObject().value("fx").toInt(), 89);
    QTest::qWait(250);
    // GO.
    plugin.writeUniverse(0, 0, universe({ 255, 89, 200, 128, 6, 0, 255, 0, 0, 0, 0, 255 }), true);
    QTRY_COMPARE(wled.posts.size(), 3);
    const QJsonObject go = wled.posts.at(2);
    QCOMPARE(go.value("on").toBool(), true);
    QCOMPARE(go.value("bri").toInt(), 255);
    QCOMPARE(go.value("transition").toInt(-1), 0);
    QVERIFY2(!go.contains("seg"), QJsonDocument(go).toJson(QJsonDocument::Compact).constData());
    // The device shows the look all the same.
    QCOMPARE(wled.seg(0).value("fx").toInt(), 89);
    QCOMPARE(wled.seg(0).value("col").toArray().at(0).toArray(), (QJsonArray{ 255, 0, 0 }));

    /* Not moved in black: an effect set a frame before its dimmer (as the
       engine writes a WLED look). The dark change waits for the brightness
       and both go as one -- sent alone, it would hold the brightness up for
       a Wi-Fi round trip (one request in flight). */
    plugin.writeUniverse(0, 0, universe({ 0, 89, 200, 128, 6, 0, 255, 0, 0, 0, 0, 255 }), true);
    QTRY_COMPARE(wled.posts.size(), 4);
    QTest::qWait(250);
    QElapsedTimer frame;
    frame.start();
    plugin.writeUniverse(0, 0, universe({ 0, 42, 200, 128, 6, 0, 0, 0, 255 }), true);
    QTest::qWait(20);
    plugin.writeUniverse(0, 0, universe({ 255, 42, 200, 128, 6, 0, 0, 0, 255 }), true);
    const bool inOneFrame = frame.elapsed() < 40;  // a loaded machine may take longer
    QTRY_VERIFY(!wled.posts.isEmpty() && wled.posts.last().value("on").toBool());
    QTest::qWait(200);
    QCOMPARE(wled.seg(0).value("fx").toInt(), 42);
    if (inOneFrame)
    {
        QCOMPARE(wled.posts.size(), 5);
        QCOMPARE(wled.posts.last().value("seg").toArray().first().toObject().value("fx").toInt(), 42);
    }
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

/* A backup lantern: "outputIP2" (and on), each with an address and channel
   count of its own only if it says so. They are kept as parameters -- what a
   saved show writes back -- and the ones that are not ours are not. */
void Wled_Test::backupDevicesAreParametersOfTheirOwn()
{
    int kind = -1;
    QCOMPARE(WledPlugin::parameterSlot("outputIP", &kind), 0);
    QCOMPARE(kind, 0);
    QCOMPARE(WledPlugin::parameterSlot("outputIP2", &kind), 1);
    QCOMPARE(kind, 0);
    QCOMPARE(WledPlugin::parameterSlot("wledAddress3", &kind), 2);
    QCOMPARE(kind, 1);
    QCOMPARE(WledPlugin::parameterSlot("wledChannels2", &kind), 1);
    QCOMPARE(kind, 2);
    QCOMPARE(WledPlugin::parameterSlot("outputIP1"), -1);
    QCOMPARE(WledPlugin::parameterSlot("outputIP02"), -1);
    QCOMPARE(WledPlugin::parameterSlot("outputIP17"), -1);
    QCOMPARE(WledPlugin::parameterSlot("outputUni"), -1);
    QCOMPARE(WledPlugin::parameterSlot("outputIPx"), -1);

    WledPlugin plugin;
    plugin.init();
    plugin.setInfoInterval(100000);         // never asks anyone: nothing listens there
    plugin.setSendInterval(100000);
    QVERIFY(plugin.openOutput(0, 1));
    const auto set = [&](const char *name, const QVariant &v) {
        plugin.setParameter(1, 0, QLCIOPlugin::Output, name, v);
    };
    set("outputIP", "127.0.0.1:1");
    set("wledAddress", 1);
    set("wledChannels", 15);
    set("outputIP2", "127.0.0.1:2");
    set("outputIP3", "127.0.0.1:3");
    set("wledAddress3", 16);
    set("wledChannels3", 4);
    set("outputUni", 3);                    // Art-Net's: not ours, not kept

    QMap<QString, QVariant> p = plugin.getParameters(1, 0, QLCIOPlugin::Output);
    QCOMPARE(p.keys(), (QStringList{ "outputIP", "outputIP2", "outputIP3", "wledAddress",
                                      "wledAddress3", "wledChannels", "wledChannels3" }));
    QCOMPARE(p.value("outputIP2").toString(), QString("127.0.0.1:2"));
    QCOMPARE(plugin.answering(1), qMakePair(0, 3));
    const QString info = plugin.outputInfo(0);
    QVERIFY2(info.contains("3 devices, 0 of 3 answering"), qPrintable(info));
    QVERIFY2(info.contains("127.0.0.1:3 (channels 16&ndash;19)"), qPrintable(info));
    QVERIFY2(info.contains("127.0.0.1:2 (channels 1&ndash;15)"), qPrintable(info));

    // Taken off again: gone from what is kept, and from what is sent to.
    plugin.unSetParameter(1, 0, QLCIOPlugin::Output, "outputIP3");
    plugin.unSetParameter(1, 0, QLCIOPlugin::Output, "wledAddress3");
    plugin.unSetParameter(1, 0, QLCIOPlugin::Output, "wledChannels3");
    p = plugin.getParameters(1, 0, QLCIOPlugin::Output);
    QVERIFY(!p.contains("outputIP3") && !p.contains("wledAddress3"));
    QCOMPARE(plugin.answering(1), qMakePair(0, 2));

    // One device, as every show before backups: just that one.
    set("outputIP2", "");
    plugin.unSetParameter(1, 0, QLCIOPlugin::Output, "outputIP2");
    QCOMPARE(plugin.answering(1), qMakePair(0, 1));
    QVERIFY(plugin.outputInfo(0).contains("Universe 2 &rarr; 127.0.0.1:1 (channels 1&ndash;15)"));
}

/* The point of a backup: the same look, on both, without programming it twice. */
void Wled_Test::everyDeviceIsSentTheSameLook()
{
    FakeWled a, b, c;
    QVERIFY(a.powerOn() && b.powerOn() && c.powerOn());
    WledPlugin plugin;
    plugin.init();
    plugin.setSendInterval(40);
    plugin.setInfoInterval(40);
    QVERIFY(plugin.openOutput(0, 0));
    plugin.setParameter(0, 0, QLCIOPlugin::Output, WLED_OUTPUTIP, a.ip());
    plugin.setParameter(0, 0, QLCIOPlugin::Output, WLED_CHANNELS, 15);
    plugin.setParameter(0, 0, QLCIOPlugin::Output, "outputIP2", b.ip());
    // A third reading its own 4 channels further on: a solid colour.
    plugin.setParameter(0, 0, QLCIOPlugin::Output, "outputIP3", c.ip());
    plugin.setParameter(0, 0, QLCIOPlugin::Output, "wledAddress3", 16);
    plugin.setParameter(0, 0, QLCIOPlugin::Output, "wledChannels3", 4);

    QByteArray look = universe({ 200, 9, 128, 64, 6, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255,
                                 100, 1, 2, 3 });
    plugin.writeUniverse(0, 0, look, true);
    QTRY_COMPARE(a.posts.size(), 1);
    QTRY_COMPARE(b.posts.size(), 1);
    QTRY_COMPARE(c.posts.size(), 1);
    QCOMPARE(a.posts.first(), b.posts.first());
    QCOMPARE(b.posts.first().value("live").toBool(true), false);   // out of realtime, each
    QCOMPARE(c.posts.first().value("bri").toInt(), 100);
    QCOMPARE(c.posts.first().value("seg").toArray().first().toObject()
                 .value("col").toArray().first().toArray(), (QJsonArray{ 1, 2, 3 }));

    look[0] = char(50);
    plugin.writeUniverse(0, 0, look, true);
    QTRY_COMPARE(a.posts.size(), 2);
    QTRY_COMPARE(b.posts.size(), 2);
    QCOMPARE(a.posts.last(), b.posts.last());
    QCOMPARE(b.posts.last().value("bri").toInt(), 50);
    QTest::qWait(200);
    QCOMPARE(c.posts.size(), 1);            // its channels did not change

    QTRY_COMPARE(plugin.answering(0), qMakePair(3, 3));
    QVERIFY(plugin.outputProblems(0, 0).isEmpty());
    QVERIFY(plugin.targetProblems().isEmpty());
    QVERIFY(plugin.outputInfo(0).contains("3 devices, 3 of 3 answering"));
}

/* A backup is there for the night the primary dies -- so a dead one, even
   one that takes the connection and never answers, must not slow the other. */
void Wled_Test::aDeadDeviceDoesNotHoldUpTheOthers()
{
    FakeWled primary, backup;
    QVERIFY(primary.powerOn() && backup.powerOn());
    primary.hang = true;
    WledPlugin plugin;
    plugin.init();
    plugin.setSendInterval(40);
    plugin.setInfoInterval(60);
    QVERIFY(plugin.openOutput(0, 4));
    plugin.setParameter(4, 0, QLCIOPlugin::Output, WLED_OUTPUTIP, primary.ip());
    plugin.setParameter(4, 0, QLCIOPlugin::Output, "outputIP2", backup.ip());

    // A fade while the primary hangs: the backup follows it to the end.
    /* Timed to the backup's FIRST post, not to the end of the fade: the
       fade's 30 waits of 15 ms took well over a second on a loaded CI
       runner, which failed a whole-run limit with nothing wrong. A backup
       held up by the primary would get nothing until its 2 s timeout. */
    QElapsedTimer t;
    t.start();
    qint64 firstBackupPost = -1;
    for (int v = 1; v <= 30; ++v)
    {
        plugin.writeUniverse(4, 0, universe({ 255, 0, 128, 128, 0, 0, v * 8, 0, 0 }), true);
        QTest::qWait(15);
        if (firstBackupPost < 0 && !backup.posts.isEmpty())
            firstBackupPost = t.elapsed();
    }
    if (firstBackupPost < 0)
    {
        QTRY_VERIFY_WITH_TIMEOUT(!backup.posts.isEmpty(), 1500);
        firstBackupPost = t.elapsed();
    }
    QVERIFY2(firstBackupPost < 1500, qPrintable(QString("the backup waited on the primary's timeout "
                                                        "(first post after %1 ms)").arg(firstBackupPost)));
    QTRY_VERIFY_WITH_TIMEOUT(!backup.posts.isEmpty()
                             && backup.posts.last().value("seg").toArray().first().toObject()
                                    .value("col").toArray().first().toArray() == (QJsonArray{ 240, 0, 0 }),
                             1500);
    QVERIFY(backup.posts.size() >= 3);      // it kept up, not one state at the end
    QVERIFY(primary.posts.size() <= 1);

    // Said: the universe is still on stage, one of its devices is not.
    QTRY_VERIFY_WITH_TIMEOUT(plugin.outputProblems(0, 4)
                             == QStringList{ QString("1 of 2 answering — %1 not answering").arg(primary.ip()) },
                             8000);
    QVERIFY(plugin.targetProblems().contains(primary.ip() + " — not answering · 1 of 2 answering"));
    QCOMPARE(plugin.answering(4), qMakePair(1, 2));
    QVERIFY(plugin.outputInfo(0).contains("2 devices, 1 of 2 answering"));

    // And it goes on: the next change still reaches the backup at once.
    const int before = backup.posts.size();
    plugin.writeUniverse(4, 0, universe({ 255, 0, 128, 128, 0, 0, 0, 0, 255 }), true);
    QTRY_VERIFY_WITH_TIMEOUT(backup.posts.size() > before, 600);
}

/* Switched off and on again, a lantern has forgotten the look: it is sent
   again, in full, as soon as it answers -- though nothing changed meanwhile. */
void Wled_Test::aDeviceThatComesBackIsSentTheLookAgain()
{
    FakeWled primary, backup;
    QVERIFY(primary.powerOn() && backup.powerOn());
    WledPlugin plugin;
    plugin.init();
    plugin.setSendInterval(40);
    plugin.setInfoInterval(60);
    QVERIFY(plugin.openOutput(0, 0));
    plugin.setParameter(0, 0, QLCIOPlugin::Output, WLED_OUTPUTIP, primary.ip());
    plugin.setParameter(0, 0, QLCIOPlugin::Output, "outputIP2", backup.ip());
    const QByteArray look = universe({ 255, 0, 128, 128, 0, 0, 0, 255, 0 });
    plugin.writeUniverse(0, 0, look, true);
    QTRY_COMPARE(backup.posts.size(), 1);
    QTRY_COMPARE(plugin.answering(0), qMakePair(2, 2));

    backup.powerOff();
    QTRY_VERIFY_WITH_TIMEOUT(plugin.answering(0) == qMakePair(1, 2), 8000);
    QVERIFY(plugin.targetProblems().join(";").contains(backup.ip() + " — not answering"));

    backup.posts.clear();
    QVERIFY(backup.powerOn());              // same lantern, same address
    QTRY_VERIFY_WITH_TIMEOUT(!backup.posts.isEmpty(), 8000);
    QCOMPARE(backup.posts.first().value("live").toBool(true), false);
    QCOMPARE(backup.posts.first().value("seg").toArray().first().toObject()
                 .value("col").toArray().first().toArray(), (QJsonArray{ 0, 255, 0 }));
    QTRY_COMPARE(plugin.answering(0), qMakePair(2, 2));
    QTRY_VERIFY(plugin.targetProblems().isEmpty());
    QVERIFY(plugin.outputProblems(0, 0).isEmpty());
    // The primary, which never went away, was not sent it again.
    QCOMPARE(primary.posts.size(), 1);
}

void Wled_Test::theRigViewWatchesTheFirstDeviceThatAnswers()
{
    FakeWled primary, backup;
    QVERIFY(primary.powerOn() && backup.powerOn());
    backup.live = "{\"leds\":[\"00FF00\"],\"n\":1}";
    WledPlugin plugin;
    plugin.init();
    plugin.setInfoInterval(40);
    plugin.setLiveInterval(40);
    QVERIFY(plugin.openOutput(0, 2));
    plugin.setParameter(2, 0, QLCIOPlugin::Output, WLED_OUTPUTIP, primary.ip());
    plugin.setParameter(2, 0, QLCIOPlugin::Output, "outputIP2", backup.ip());
    plugin.watchTargetPixels(2, true);
    QTRY_COMPARE(plugin.targetPixels(2), (QVector<quint32>{ 0xFF0000, 0x00FF00, 0x0000FF, 0x102030 }));
    QCOMPARE(backup.liveAsked, 0);          // the primary answers: only it is asked

    primary.powerOff();
    QTRY_VERIFY_WITH_TIMEOUT(plugin.targetPixels(2) == (QVector<quint32>{ 0x00FF00 }), 8000);
    QCOMPARE(plugin.targetPixelCount(2), 16);
    plugin.watchTargetPixels(2, false);
}

/* The rig network gate's per-device decision is worked out when the
   device's address is set and when the gate refreshes (the networks changed;
   the engine's reconnect tick), never on the send tick, which only reads
   Device::gateOpen. Nothing here is sent anywhere: init() is not called (no
   send timers), and a route probe is a UDP connect, which sends nothing. */
void Wled_Test::rigNetworkGateIsDecidedOffTheSendTick()
{
    WledPlugin plugin;
    QVERIFY(plugin.openOutput(0, 0));
    QVERIFY(plugin.openOutput(0, 1));
    auto open = [&plugin](quint32 universe) {
        QMutexLocker locker(&plugin.m_mutex);
        return plugin.m_targets.value(universe).devices.value(0).gateOpen;
    };

    // No networks: everything open, no probe needed
    plugin.setParameter(0, 0, QLCIOPlugin::Output, WLED_OUTPUTIP, QString("198.51.100.7"));
    plugin.setParameter(1, 0, QLCIOPlugin::Output, WLED_OUTPUTIP, QString("127.0.0.1"));
    QVERIFY(open(0));
    QVERIFY(open(1));

    // A rig network this machine is not on (TEST-NET-1): the device reached
    // through some other route is held, loopback never is
    plugin.setTransmitNetworks(QStringList() << "192.0.2.0/24");
    QVERIFY(open(0) == false);
    QVERIFY(open(1));

    // A device addressed while the gate is on is decided as it is set
    plugin.setParameter(0, 0, QLCIOPlugin::Output, WLED_OUTPUTIP, QString("198.51.100.8"));
    QVERIFY(open(0) == false);
    plugin.setParameter(0, 0, QLCIOPlugin::Output, WLED_OUTPUTIP, QString("wled.local"));
    QVERIFY(open(0) == false);                  // a name cannot be checked

    // The tick: a stale answer is put right
    plugin.setParameter(0, 0, QLCIOPlugin::Output, WLED_OUTPUTIP, QString("198.51.100.8"));
    {
        QMutexLocker locker(&plugin.m_mutex);
        plugin.m_targets[0].devices[0].gateOpen = true;
    }
    plugin.refreshTransmitGate();
    QVERIFY(open(0) == false);

    // The networks cleared: open again at once
    plugin.setTransmitNetworks(QStringList());
    QVERIFY(open(0));
    QVERIFY(open(1));
}

namespace
{
    /** The fifteen Effect channels: effect @p fx at speed @p sx, option
     *  @p option (0 plain, 1 mirror, 2 reverse, 3 both), red on blue. */
    QByteArray effect(int fx, int sx, int option)
    {
        return universe({ 255, fx, sx, 128, 6, option * 64, 255, 0, 0, 0, 0, 255, 0, 0, 0 });
    }

    QList<bool> revs(const FakeWled &w)
    {
        QList<bool> out;
        for (const QJsonValue &v : w.segs)
            out << v.toObject().value("rev").toBool();
        return out;
    }

    QList<int> effects(const FakeWled &w)
    {
        QList<int> out;
        for (const QJsonValue &v : w.segs)
            out << v.toObject().value("fx").toInt();
        return out;
    }

    void aim(WledPlugin &plugin, quint32 universe, const FakeWled &w)
    {
        plugin.init();
        plugin.setSendInterval(30);
        plugin.setInfoInterval(60);
        QVERIFY(plugin.openOutput(0, universe));
        plugin.setParameter(universe, 0, QLCIOPlugin::Output, WLED_OUTPUTIP, w.ip());
        plugin.setParameter(universe, 0, QLCIOPlugin::Output, WLED_CHANNELS, 15);
    }
}

/* The fake is WLED's own reading of "seg", which the rest relies on: what
   was sent before this fix -- one element, no id -- reaches segment 0 only
   (seen on a WLED 16.0.1: segment 1 kept effect 0). */
void Wled_Test::theFakeAppliesASegmentWithoutAnIdByItsIndex()
{
    FakeWled w;
    w.split(2, 25);
    w.apply(QJsonObject{ { "seg", QJsonArray{ QJsonObject{ { "fx", 9 } } } } });
    QCOMPARE(effects(w), (QList<int>{ 9, 0 }));
    w.apply(QJsonObject{ { "seg", QJsonArray{ QJsonObject{ { "id", 1 }, { "fx", 7 } } } } });
    QCOMPARE(effects(w), (QList<int>{ 9, 7 }));
    w.apply(QJsonObject{ { "seg", QJsonArray{ QJsonObject{ { "id", 5 }, { "fx", 3 } } } } });
    QCOMPARE(effects(w), (QList<int>{ 9, 7 }));             // none such: ignored
    w.apply(QJsonObject{ { "seg", QJsonObject{ { "fx", 2 } } } });
    QCOMPARE(effects(w), (QList<int>{ 2, 2 }));             // the object: all selected
}

/* A fountain's string, one segment per span, every second reversed: the
   effect plays on every span, each its own way, from the first state sent. */
void Wled_Test::anEffectReachesEverySegment()
{
    FakeWled w;
    w.split(4, 4);
    QVERIFY(w.powerOn());
    WledPlugin plugin;
    aim(plugin, 0, w);
    plugin.writeUniverse(0, 0, effect(9, 200, 0), true);
    QTRY_COMPARE(w.posts.size(), 1);
    QVERIFY(w.siAsked >= 1);                        // asked before it was sent
    QCOMPARE(effects(w), (QList<int>{ 9, 9, 9, 9 }));
    QCOMPARE(revs(w), (QList<bool>{ false, true, false, true }));   // the layout, kept
    const QJsonArray sent = w.posts.first().value("seg").toArray();
    QCOMPARE(sent.size(), 4);
    for (int k = 0; k < 4; ++k)
    {
        const QJsonObject o = sent.at(k).toObject();
        QCOMPARE(o.value("id").toInt(), k);
        QCOMPARE(o.value("sx").toInt(), 200);
        QCOMPARE(o.value("mi").toBool(), false);
        QCOMPARE(o.value("col").toArray().at(1).toArray(), (QJsonArray{ 0, 0, 255 }));
    }
    QCOMPARE(w.seg(3).value("col").toArray().first().toArray(), (QJsonArray{ 255, 0, 0 }));

    // Held: nothing more, polls or no polls.
    QTest::qWait(300);
    QCOMPARE(w.posts.size(), 1);
}

/* Bit 5 of the Option channel (32): a look's effect that ran Once and
   holds -- WLED stops on its frame ("frz"). The top bits keep their way;
   clear, it runs again (always said, so no freeze outlives its look). */
void Wled_Test::theOptionsFreezeBitIsReadAsFrz()
{
    auto segOf = [](int option) {
        return WledState::decode(universe({ 255, 9, 128, 64, 6, option, 255, 0, 0 }), 1, 15)
            .value("seg").toArray().first().toObject();
    };
    QCOMPARE(segOf(0).value("frz").toBool(true), false);
    QCOMPARE(segOf(31).value("frz").toBool(true), false);
    QCOMPARE(segOf(32).value("frz").toBool(), true);
    QCOMPARE(segOf(63).value("frz").toBool(), true);
    const QJsonObject held = segOf(128 + 32);           // Reverse, frozen
    QCOMPARE(held.value("frz").toBool(), true);
    QCOMPARE(held.value("rev").toBool(), true);
    QCOMPARE(held.value("mi").toBool(), false);
    QCOMPARE(held.value("fx").toInt(), 9);
    QCOMPARE(segOf(224).value("mi").toBool(), true);    // Mirror + reverse, frozen
    QCOMPARE(segOf(224).value("rev").toBool(), true);
    // A solid colour mode has no option channel: nothing about freezing.
    QVERIFY(!WledState::decode(universe({ 128, 10, 20, 30 }), 1, 4)
                 .value("seg").toArray().first().toObject().contains("frz"));
}

void Wled_Test::aFreezeReachesEverySegmentAndLetsGoAgain()
{
    FakeWled w;
    w.split(4, 4);
    QVERIFY(w.powerOn());
    WledPlugin plugin;
    aim(plugin, 0, w);
    auto frozen = [&w]() {
        QList<bool> out;
        for (const QJsonValue &v : std::as_const(w.segs))
            out << v.toObject().value("frz").toBool();
        return out;
    };
    QByteArray u = effect(9, 200, 2);                   // reverse
    plugin.writeUniverse(0, 0, u, true);
    QTRY_COMPARE(effects(w), (QList<int>{ 9, 9, 9, 9 }));
    QCOMPARE(frozen(), (QList<bool>{ false, false, false, false }));
    QCOMPARE(revs(w), (QList<bool>{ true, false, true, false }));

    // Held at the end of its run: every segment stops on its frame, each
    // still turned its own way, and nothing else changes.
    u[5] = char(128 + 32);
    plugin.writeUniverse(0, 0, u, true);
    QTRY_COMPARE(frozen(), (QList<bool>{ true, true, true, true }));
    QCOMPARE(revs(w), (QList<bool>{ true, false, true, false }));
    QCOMPARE(effects(w), (QList<int>{ 9, 9, 9, 9 }));
    const QJsonArray sent = w.posts.last().value("seg").toArray();
    QCOMPARE(sent.size(), 4);
    for (int k = 0; k < 4; ++k)
    {
        QCOMPARE(sent.at(k).toObject().value("id").toInt(), k);
        QCOMPARE(sent.at(k).toObject().value("frz").toBool(), true);
    }

    // Started again: it runs.
    u[5] = char(128);
    plugin.writeUniverse(0, 0, u, true);
    QTRY_COMPARE(frozen(), (QList<bool>{ false, false, false, false }));
    QCOMPARE(effects(w), (QList<int>{ 9, 9, 9, 9 }));
}

/* Reverse (the Option channel) turns each segment round from the way IT
   runs; off again, each runs its own way. Mirror is the same on all. */
void Wled_Test::reverseTurnsEachSegmentRoundFromItsOwnWay()
{
    FakeWled w;
    w.split(4, 4);
    QVERIFY(w.powerOn());
    WledPlugin plugin;
    aim(plugin, 0, w);
    plugin.writeUniverse(0, 0, effect(9, 128, 2), true);            // reverse
    QTRY_COMPARE(revs(w), (QList<bool>{ true, false, true, false }));
    QCOMPARE(effects(w), (QList<int>{ 9, 9, 9, 9 }));

    // Held through several polls: it says back what it was sent, and that
    // is not taken for a new layout.
    const int posts = w.posts.size();
    const int asked = w.siAsked;
    QTRY_VERIFY(w.siAsked >= asked + 3);
    QCOMPARE(w.posts.size(), posts);
    QCOMPARE(revs(w), (QList<bool>{ true, false, true, false }));

    plugin.writeUniverse(0, 0, effect(9, 128, 0), true);            // plain
    QTRY_COMPARE(revs(w), (QList<bool>{ false, true, false, true }));
    plugin.writeUniverse(0, 0, effect(9, 128, 3), true);            // mirror and reverse
    QTRY_COMPARE(revs(w), (QList<bool>{ true, false, true, false }));
    for (const QJsonValue &v : std::as_const(w.segs))
        QCOMPARE(v.toObject().value("mi").toBool(), true);
    plugin.writeUniverse(0, 0, effect(9, 128, 1), true);            // mirror
    QTRY_COMPARE(revs(w), (QList<bool>{ false, true, false, true }));
}

/* Write Segments again (fewer spans, other ways): the next answer says so,
   and the look -- reversed -- goes to the new segments, from their ways. */
void Wled_Test::aNewLayoutIsLearnedAndReverseStartsOverFromIt()
{
    FakeWled w;
    w.split(4, 4);
    QVERIFY(w.powerOn());
    WledPlugin plugin;
    aim(plugin, 0, w);
    plugin.writeUniverse(0, 0, effect(9, 128, 2), true);
    QTRY_COMPARE(revs(w), (QList<bool>{ true, false, true, false }));

    // Written as three spans, the middle one reversed; their effect is
    // WLED's new-segment default.
    w.segs = QJsonArray{
        QJsonObject{ { "id", 0 }, { "start", 0 }, { "stop", 5 }, { "rev", false }, { "fx", 0 } },
        QJsonObject{ { "id", 1 }, { "start", 5 }, { "stop", 10 }, { "rev", true }, { "fx", 0 } },
        QJsonObject{ { "id", 2 }, { "start", 10 }, { "stop", 16 }, { "rev", false }, { "fx", 0 } } };
    QTRY_COMPARE(effects(w), (QList<int>{ 9, 9, 9 }));
    QCOMPARE(revs(w), (QList<bool>{ true, false, true }));
    QCOMPARE(w.posts.last().value("seg").toArray().size(), 3);
    plugin.writeUniverse(0, 0, effect(9, 128, 0), true);
    QTRY_COMPARE(revs(w), (QList<bool>{ false, true, false }));
}

/* The same segments, one turned round by hand (its own UI, or Write
   Segments over the same spans): that is its way now, and Reverse flips
   from it. */
void Wled_Test::aSegmentTurnedRoundBySomeoneElseKeepsThatWay()
{
    FakeWled w;
    w.split(2, 8);
    QVERIFY(w.powerOn());
    WledPlugin plugin;
    aim(plugin, 0, w);
    plugin.writeUniverse(0, 0, effect(9, 128, 2), true);
    QTRY_COMPARE(revs(w), (QList<bool>{ true, false }));

    // Written back to the layout over the flip: flipped again from it.
    w.split(2, 8);
    QCOMPARE(revs(w), (QList<bool>{ false, true }));
    QTRY_COMPARE(revs(w), (QList<bool>{ true, false }));
    QCOMPARE(effects(w), (QList<int>{ 9, 9 }));

    // Segment 0 turned round by hand: its way is reversed now.
    QJsonObject s0 = w.seg(0);
    s0["rev"] = false;
    w.segs[0] = s0;
    const int posts = w.posts.size();
    QTRY_VERIFY(w.posts.size() > posts);
    QCOMPARE(revs(w), (QList<bool>{ true, false }));
    plugin.writeUniverse(0, 0, effect(9, 128, 0), true);
    QTRY_COMPARE(revs(w), (QList<bool>{ false, true }));
}

/* One segment, as every lantern before fountains: the same state as ever,
   now naming segment 0; Reverse sets its way as it did. */
void Wled_Test::aSingleSegmentDeviceIsSentAsBefore()
{
    FakeWled w;
    QVERIFY(w.powerOn());
    WledPlugin plugin;
    aim(plugin, 0, w);
    const QByteArray look = effect(9, 128, 2);
    plugin.writeUniverse(0, 0, look, true);
    QTRY_COMPARE(w.posts.size(), 1);
    QJsonObject was = WledState::decode(look, 1, 15, 30);
    QJsonObject one = was.value("seg").toArray().first().toObject();
    one["id"] = 0;
    was["seg"] = QJsonArray{ one };
    was["live"] = false;
    QCOMPARE(w.posts.first(), was);
    QCOMPARE(revs(w), (QList<bool>{ true }));
    plugin.writeUniverse(0, 0, effect(9, 128, 0), true);
    QTRY_COMPARE(revs(w), (QList<bool>{ false }));
}

/* A WLED that has no /json/si (before 0.10) answers it with an error: it
   is asked /json/info instead -- answering, not a problem -- and sent the
   one element without an id, as before. */
void Wled_Test::anOldWledWithoutStateAndInfoIsSentAsBefore()
{
    FakeWled w;
    w.noSi = true;
    QVERIFY(w.powerOn());
    WledPlugin plugin;
    aim(plugin, 0, w);
    plugin.writeUniverse(0, 0, effect(9, 128, 0), true);
    QTRY_COMPARE(w.posts.size(), 1);
    QVERIFY(!w.posts.first().value("seg").toArray().first().toObject().contains("id"));
    QCOMPARE(effects(w), (QList<int>{ 9 }));
    QTRY_COMPARE(plugin.targetPixelCount(0), 16);       // its info, from /json/info
    QCOMPARE(plugin.answering(0), qMakePair(1, 1));
    QVERIFY(plugin.targetProblems().isEmpty());
}

/* A backup has segments of its own: each device is sent ITS segments, each
   turned from its own way. */
void Wled_Test::backupsKeepSegmentLayoutsOfTheirOwn()
{
    FakeWled primary, backup;
    primary.split(4, 4);
    QVERIFY(primary.powerOn() && backup.powerOn());
    WledPlugin plugin;
    aim(plugin, 0, primary);
    plugin.setParameter(0, 0, QLCIOPlugin::Output, "outputIP2", backup.ip());
    plugin.writeUniverse(0, 0, effect(9, 128, 2), true);
    QTRY_COMPARE(revs(primary), (QList<bool>{ true, false, true, false }));
    QTRY_COMPARE(revs(backup), (QList<bool>{ true }));
    QCOMPARE(effects(primary), (QList<int>{ 9, 9, 9, 9 }));
    QCOMPARE(effects(backup), (QList<int>{ 9 }));
    QCOMPARE(backup.posts.last().value("seg").toArray().size(), 1);
}

/* Unpatched, or the app quitting (the output closes): every segment the
   effect's Reverse turned round is sent back its own way -- and a device
   that takes the connection and never answers does not hold the close up
   for more than about a second. */
void Wled_Test::closingTheOutputPutsEachSegmentBackItsOwnWay()
{
    FakeWled w;
    w.split(2, 8);
    QVERIFY(w.powerOn());
    {
        WledPlugin plugin;
        aim(plugin, 0, w);
        plugin.writeUniverse(0, 0, effect(9, 128, 2), true);
        QTRY_COMPARE(revs(w), (QList<bool>{ true, false }));
        plugin.closeOutput(0, 0);
        QTRY_COMPARE(revs(w), (QList<bool>{ false, true }));
        QCOMPARE(effects(w), (QList<int>{ 9, 9 }));            // only the ways
    }
    // Quitting with it open: the same, from the plugin going away.
    {
        WledPlugin plugin;
        aim(plugin, 0, w);
        plugin.writeUniverse(0, 0, effect(9, 128, 2), true);
        QTRY_COMPARE(revs(w), (QList<bool>{ true, false }));
    }
    QTRY_COMPARE(revs(w), (QList<bool>{ false, true }));
    // Not turned round: nothing sent on the way out.
    {
        WledPlugin plugin;
        aim(plugin, 0, w);
        plugin.writeUniverse(0, 0, effect(9, 128, 0), true);
        QTRY_VERIFY(!w.posts.isEmpty() && w.posts.last().contains("bri"));
        QTest::qWait(150);                      // taken, and said so
        const int posts = w.posts.size();
        plugin.closeOutput(0, 0);
        QTest::qWait(100);
        QCOMPARE(w.posts.size(), posts);
    }
    // Gone wrong (answers nothing): the close is not held up for long.
    {
        WledPlugin plugin;
        aim(plugin, 0, w);
        plugin.writeUniverse(0, 0, effect(9, 128, 2), true);
        QTRY_COMPARE(revs(w), (QList<bool>{ true, false }));
        w.hang = true;
        QElapsedTimer t;
        t.start();
        plugin.closeOutput(0, 0);
        QVERIFY2(t.elapsed() < 1500, qPrintable(QString("the close took %1 ms").arg(t.elapsed())));
        w.hang = false;
    }
}

/* The way out did not reach it (it hung as the app quit): left turned
   round. The next run, Reverse off, takes the layout's ways from what was
   kept -- not the turned-round ones the device says. */
void Wled_Test::aRestartAfterQuittingWithReverseOnTakesTheKeptWays()
{
    FakeWled w;
    w.split(2, 8);
    w.mac = "AA:BB:CC:00:11:22";
    QVERIFY(w.powerOn());
    {
        WledPlugin plugin;
        aim(plugin, 0, w);
        plugin.writeUniverse(0, 0, effect(9, 128, 2), true);
        QTRY_COMPARE(revs(w), (QList<bool>{ true, false }));
        w.hang = true;                          // the way out never lands
    }
    QTest::qWait(100);
    w.hang = false;
    QCOMPARE(revs(w), (QList<bool>{ true, false }));   // left turned round

    WledPlugin again;
    aim(again, 0, w);
    again.writeUniverse(0, 0, effect(9, 128, 0), true);
    QTRY_COMPARE(revs(w), (QList<bool>{ false, true }));
    again.writeUniverse(0, 0, effect(9, 128, 2), true);
    QTRY_COMPARE(revs(w), (QList<bool>{ true, false }));
}

/* A crash mid-Reverse: no way out at all. The next run -- the device at
   another address now, the same MAC -- takes the kept ways. */
void Wled_Test::aCrashWithReverseOnIsPutRightFromTheKeptWays()
{
    FakeWled w;
    w.split(4, 4);
    w.mac = "aa:bb:cc:33:44:55";
    QVERIFY(w.powerOn());
    WledPlugin *crashed = new WledPlugin;
    aim(*crashed, 0, w);
    crashed->writeUniverse(0, 0, effect(9, 128, 2), true);
    QTRY_COMPARE(revs(w), (QList<bool>{ true, false, true, false }));
    {
        // Gone without a word: nothing of it runs on the way out.
        QMutexLocker locker(&crashed->m_mutex);
        crashed->m_targets.clear();
    }
    delete crashed;
    QCOMPARE(revs(w), (QList<bool>{ true, false, true, false }));

    // Back on another address (DHCP), as it was left.
    FakeWled same;
    same.segs = w.segs;
    same.mac = "AA-BB-CC-33-44-55";
    QVERIFY(same.powerOn());
    WledPlugin plugin;
    aim(plugin, 0, same);
    plugin.writeUniverse(0, 0, effect(9, 128, 0), true);
    QTRY_COMPARE(revs(same), (QList<bool>{ false, true, false, true }));

    // A layout not kept is taken as the device says it.
    FakeWled other;
    other.split(2, 8);
    QJsonObject s0 = other.seg(0);
    s0["rev"] = true;
    other.segs[0] = s0;
    QVERIFY(other.powerOn());
    plugin.setParameter(0, 0, QLCIOPlugin::Output, "outputIP2", other.ip());
    QTRY_VERIFY(!other.posts.isEmpty());
    QCOMPARE(revs(other), (QList<bool>{ true, true }));
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
