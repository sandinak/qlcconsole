/*
  Q Light Controller Plus
  wledconfigcheck_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

#include "wledconfigcheck_test.h"
#include "wledconfigcheck.h"
#include "ioplugincache.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "outputpatch.h"
#include "showstatus.h"
#include "fixture.h"
#include "qlcfile.h"
#include "doc.h"

namespace
{
/* A WLED that answers /json/cfg with whatever it holds, and takes a POST to
   it into what it holds -- the way WLED merges a partial config. */
class FakeWled : public QTcpServer
{
public:
    QJsonObject cfg;
    QList<QJsonObject> posted;

    FakeWled()
    {
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *s = nextPendingConnection())
                connect(s, &QTcpSocket::readyRead, s, [this, s]() { serve(s); });
        });
    }

private:
    static void merge(QJsonObject &into, const QJsonObject &from)
    {
        for (auto it = from.begin(); it != from.end(); ++it)
        {
            if (it.value().isObject() && into.value(it.key()).isObject())
            {
                QJsonObject sub = into.value(it.key()).toObject();
                merge(sub, it.value().toObject());
                into[it.key()] = sub;
            }
            else
                into[it.key()] = it.value();
        }
    }

    void serve(QTcpSocket *s)
    {
        m_buffer[s] += s->readAll();
        const QByteArray &req = m_buffer[s];
        const int head = req.indexOf("\r\n\r\n");
        if (head < 0)
            return;
        int length = 0;
        foreach (const QByteArray &line, req.left(head).split('\n'))
            if (line.toLower().startsWith("content-length:"))
                length = line.mid(15).trimmed().toInt();
        if (req.size() < head + 4 + length)
            return;
        if (req.startsWith("POST"))
        {
            const QJsonObject body = QJsonDocument::fromJson(req.mid(head + 4, length)).object();
            posted << body;
            merge(cfg, body);
        }
        const QByteArray out = req.startsWith("POST") ? QByteArray("{\"success\":true}")
                                                      : QJsonDocument(cfg).toJson(QJsonDocument::Compact);
        s->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n"
                 "Content-Length: " + QByteArray::number(out.size()) + "\r\n\r\n" + out);
        s->flush();
        s->disconnectFromHost();
        m_buffer.remove(s);
    }
    QHash<QTcpSocket *, QByteArray> m_buffer;
};

QJsonObject liveCfg(bool en, int port, int uni, int addr, int mode)
{
    QJsonObject dmx{ { "uni", uni }, { "addr", addr }, { "mode", mode } };
    QJsonObject live{ { "en", en }, { "port", port }, { "timeout", 25 }, { "dmx", dmx } };
    return QJsonObject{ { "if", QJsonObject{ { "live", live } } } };
}

Fixture *lantern(Doc *doc, const QString &ip, Fixture::Transport t, const QString &mode)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    if (def->loadXML(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/WLED/WLED-Lantern.qxf")
        != QFile::NoError)
        return nullptr;
    Fixture *fx = new Fixture(doc);
    fx->setName("Lantern #6");
    QLCFixtureMode *m = nullptr;
    foreach (QLCFixtureMode *each, def->modes())
        if (each->name() == mode)
            m = each;
    if (m == nullptr)
        return nullptr;
    fx->setFixtureDefinition(def, m);
    fx->setConnection(Fixture::Network);
    fx->setTransport(t);
    fx->setIPAddress(ip);
    return fx;
}
}

void WledConfigCheck_Test::init()
{
    m_doc = new Doc(this);
}

void WledConfigCheck_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void WledConfigCheck_Test::aLanternDrivenByItsApiMustNotTakeStreams()
{
    Fixture *fx = lantern(m_doc, "172.18.2.55", Fixture::WledTransport, "Effect (15ch)");
    QVERIFY(fx != nullptr);
    QVERIFY(WledConfigCheck::isWled(fx));

    // .55 as it was found: E1.31 pixels on universe 1, realtime on.
    WledConfigCheck::Finding f = WledConfigCheck::evaluate(m_doc, fx, liveCfg(true, 5568, 1, 1, 4));
    QCOMPARE(f.problems.size(), 1);
    QVERIFY2(f.problems.first().contains("take it over"), qPrintable(f.problems.first()));
    QCOMPARE(f.fix, QJsonObject({ { "if", QJsonObject{ { "live", QJsonObject{ { "en", false } } } } } }));

    // Streams off: nothing to say, whatever its DMX settings.
    QVERIFY(WledConfigCheck::evaluate(m_doc, fx, liveCfg(false, 5568, 1, 1, 4)).problems.isEmpty());
    delete fx;
}

void WledConfigCheck_Test::anArtNetLanternMustListenWhereItIsPatched()
{
    m_doc->ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*artnet*"));
    QVERIFY2(m_doc->ioPluginCache()->plugin("ArtNet") != nullptr, "the Art-Net plugin did not load");
    InputOutputMap *io = m_doc->inputOutputMap();
    while (io->universesCount() < 3)
        io->addUniverse();
    QVERIFY(io->setOutputPatch(2, "ArtNet", "", 0));
    io->outputPatch(2)->setPluginParameter("outputUni", 5);

    Fixture *fx = lantern(m_doc, "172.18.2.56", Fixture::ArtNetTransport, "Effect (15ch)");
    QVERIFY(fx != nullptr);
    fx->setUniverse(2);
    fx->setAddress(10);
    QVERIFY(m_doc->addFixture(fx));

    // Listening for E1.31 pixels at universe 1, address 1.
    WledConfigCheck::Finding f = WledConfigCheck::evaluate(m_doc, fx, liveCfg(true, 5568, 1, 1, 4));
    QCOMPARE(f.problems.size(), 4);                      // protocol, universe, address, mode
    const QJsonObject live = f.fix.value("if").toObject().value("live").toObject();
    QCOMPARE(live.value("port").toInt(), 6454);
    QCOMPARE(live.value("dmx").toObject().value("uni").toInt(), 5);
    QCOMPARE(live.value("dmx").toObject().value("addr").toInt(), 11);
    QCOMPARE(live.value("dmx").toObject().value("mode").toInt(), 3);   // WLED's Effect
    QVERIFY(!live.contains("en"));                       // it does listen already
    QVERIFY(f.changes.join(";").contains("Art-Net"));

    // Set up as patched: nothing to say. Not listening at all: that.
    QVERIFY(WledConfigCheck::evaluate(m_doc, fx, liveCfg(true, 6454, 5, 11, 3)).problems.isEmpty());
    f = WledConfigCheck::evaluate(m_doc, fx, liveCfg(false, 6454, 5, 11, 3));
    QCOMPARE(f.problems.size(), 1);
    QCOMPARE(f.fix.value("if").toObject().value("live").toObject().value("en").toBool(), true);
}

void WledConfigCheck_Test::aMismatchIsFoundShownAndFixed()
{
    FakeWled wled;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    wled.cfg = liveCfg(true, 5568, 1, 1, 4);
    const QString ip = QString("127.0.0.1:%1").arg(wled.serverPort());
    Fixture *fx = lantern(m_doc, ip, Fixture::WledTransport, "Effect (15ch)");
    QVERIFY(fx != nullptr);
    QVERIFY(m_doc->addFixture(fx));

    WledConfigCheck check(m_doc);
    QCOMPARE(WledConfigCheck::instance(), &check);
    QSignalSpy changed(&check, &WledConfigCheck::changed);
    check.checkNow();
    QTRY_VERIFY(!check.finding(fx->id()).problems.isEmpty());

    // In the footer, by name, with its fixture for the fix.
    ShowStatus::Entry entry;
    foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
        if (e.key == "wled.config")
            entry = e;
    QCOMPARE(entry.severity, ShowStatus::Warning);
    QCOMPARE(entry.items.size(), 1);
    QVERIFY2(entry.items.first().startsWith("Lantern #6 — "), qPrintable(entry.items.first()));
    QCOMPARE(entry.itemData.first().toUInt(), fx->id());

    // Fixed on the device: exactly the fix written, read back, cleared.
    check.fix({ fx->id() });
    QTRY_COMPARE(wled.posted.size(), 1);
    QCOMPARE(wled.posted.first(),
             QJsonObject({ { "if", QJsonObject{ { "live", QJsonObject{ { "en", false } } } } } }));
    QTRY_VERIFY(check.finding(fx->id()).problems.isEmpty());
    bool still = false;
    foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
        still = still || e.key == "wled.config";
    QVERIFY(!still);
    QVERIFY(changed.count() >= 2);
}

QTEST_MAIN(WledConfigCheck_Test)
