/*
  Q Light Controller Plus
  wleddiscovery_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtTest>
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidgetItemIterator>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMenu>
#include <QBuffer>
#include <QPainter>
#include <QTextBrowser>
#include <QDir>
#include <functional>
#include <memory>

#include "../../../engine/test/common/stdprelude.h"
#include "../common/fakewled.h"
#define private public
#define protected public
#include "wleddiscovery.h"
#include "wledconfigcheck.h"
#include "fixturemanager.h"
#include "fixturetreewidget.h"
#undef protected
#undef private
#include "wleddevicewrite.h"
#include "fixturecreator.h"
#include "showstatusdialog.h"
#include "monitorproperties.h"
#include "qlcfixturedefcache.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "patchplanner.h"
#include "outputpatch.h"
#include "showstatus.h"
#include "monitor.h"
#include "truss.h"
#include "fixture.h"
#include "doc.h"

#include "wleddiscovery_test.h"

#define KColumnName 0

namespace
{
/* What wled-f992e0 (172.18.2.100, WLED 16.0.1) answered to the question
   from a port of our own, recorded on the show network 2026-11-07: PTR,
   SRV (port 80), TXT mac=, and its A record. */
const char *kRecorded =
    "000084000000000300000001055f776c6564045f746370056c6f63616c00000c000100001194001e0b776c65"
    "642d663939326530055f776c6564045f746370056c6f63616c000b776c65642d663939326530c00c00210001"
    "0000007800140000000000500b776c65642d663939326530c0170b776c65642d663939326530c00c00100001"
    "000011940011106d61633d653038636665663939326530c06400010001000000780004ac120264";

QByteArray name(const QString &dotted)
{
    QByteArray out;
    foreach (const QString &l, dotted.split('.'))
    {
        out.append(char(l.size()));
        out.append(l.toUtf8());
    }
    out.append('\0');
    return out;
}

QByteArray record(const QString &owner, quint16 type, const QByteArray &rdata)
{
    QByteArray r = name(owner);
    r.append(char(type >> 8)).append(char(type & 0xff));
    r.append("\x00\x01", 2);                  // IN
    r.append("\x00\x00\x00\x78", 4);          // 120 s
    r.append(char(rdata.size() >> 8)).append(char(rdata.size() & 0xff));
    return r + rdata;
}

/* An answer as a WLED gives it: @p instance at @p ip:@p port, its MAC. */
QByteArray answer(const QString &instance, const QString &ip, quint16 port, const QString &mac)
{
    const QString full = instance + "._wled._tcp.local";
    const QString host = instance + ".local";
    QByteArray srv("\0\0\0\0", 4);
    srv.append(char(port >> 8)).append(char(port & 0xff));
    srv += name(host);
    const QByteArray txt = QByteArray(1, char(4 + mac.size())) + "mac=" + mac.toLatin1();
    QByteArray a(4, 0);
    const quint32 v = QHostAddress(ip).toIPv4Address();
    for (int i = 0; i < 4; i++)
        a[i] = char((v >> (24 - 8 * i)) & 0xff);
    QByteArray p("\x00\x00\x84\x00\x00\x00\x00\x03\x00\x00\x00\x01", 12);
    p += record("_wled._tcp.local", 12, name(full));
    p += record(full, 33, srv);
    p += record(full, 16, txt);
    p += record(host, 1, a);
    return p;
}

/* A WLED as /json/cfg says it, as wled-f992e0 did out of the box (WLED
   16.0.1): named "WLED", sync group 1 received, lit at power-on. */
QJsonObject realCfg(int leds = 50)
{
    QJsonObject cfg;
    cfg["id"] = QJsonObject{ { "mdns", "wled-f992e0" }, { "name", "WLED" }, { "inv", "Light" }, { "sui", false } };
    cfg["nw"] = QJsonObject{ { "ins", QJsonArray{ QJsonObject{ { "ssid", "Show" }, { "pskl", 8 } } } } };
    QJsonObject recv{ { "bri", true }, { "col", true }, { "fx", true }, { "pal", true }, { "grp", 1 },
                      { "seg", false }, { "sb", false } };
    QJsonObject send{ { "en", false }, { "dir", true }, { "btn", true }, { "va", false }, { "hue", false },
                      { "grp", 1 }, { "ret", 0 } };
    QJsonObject live{ { "en", false }, { "port", 5568 }, { "timeout", 25 },
                      { "dmx", QJsonObject{ { "uni", 1 }, { "addr", 1 }, { "mode", 4 } } } };
    cfg["if"] = QJsonObject{ { "sync", QJsonObject{ { "port0", 21324 }, { "recv", recv }, { "send", send } } },
                             { "live", live } };
    cfg["def"] = QJsonObject{ { "ps", 0 }, { "on", true }, { "bri", 128 } };
    cfg["hw"] = FakeWled::ledCfg(leds);
    return cfg;
}

/* Clicks the button starting with @p button on the next message box (and
   grabs it at three sizes when QLC_SHOT_DIR is set); none comes: nothing. */
void answerBox(const QString &button, QString *text, const QString &shot = QString())
{
    auto poll = std::make_shared<std::function<void()> >();
    auto left = std::make_shared<int>(300);
    *poll = [button, text, shot, poll, left]() {
        QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (box == nullptr)
        {
            if (--*left > 0)
                QTimer::singleShot(10, *poll);
            return;
        }
        if (text != nullptr)
            *text = box->text();
        const QByteArray dir = qgetenv("QLC_SHOT_DIR");
        if (!dir.isEmpty() && !shot.isEmpty())
        {
            QDir().mkpath(QString::fromLocal8Bit(dir));
            QWidget *over = box->parentWidget();
            for (const QSize &sz : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
            {
                if (over != nullptr)
                    over->resize(sz);
                QApplication::processEvents();
                box->grab().save(QString::fromLocal8Bit(dir) + QStringLiteral("/%1-%2x%3.png")
                                     .arg(shot).arg(sz.width()).arg(sz.height()));
            }
        }
        QAbstractButton *hit = nullptr;
        for (QAbstractButton *b : box->buttons())
            if (b->text().startsWith(button))
                hit = b;
        if (hit != nullptr)
            hit->click();
        else
            box->reject();
    };
    QTimer::singleShot(0, *poll);
}

/* Grabs @p w at three sizes as QLC_SHOT_DIR/<shot>-WxH.png. */
void shots(QWidget *w, const QString &shot)
{
    const QByteArray dir = qgetenv("QLC_SHOT_DIR");
    if (dir.isEmpty())
        return;
    QDir().mkpath(QString::fromLocal8Bit(dir));
    for (const QSize &sz : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
    {
        w->resize(sz);
        QApplication::processEvents();
        w->grab().save(QString::fromLocal8Bit(dir) + QStringLiteral("/%1-%2x%3.png")
                           .arg(shot).arg(sz.width()).arg(sz.height()));
    }
}

QTreeWidgetItem *foundRow(FixtureManager &mgr, bool folder = false)
{
    for (QTreeWidgetItemIterator it(mgr.m_fixtures_tree); *it != nullptr; ++it)
    {
        const QVariant v = (*it)->data(KColumnName, PROP_FOUND);
        if (v.isValid() && v.toString().isEmpty() == folder)
            return *it;
    }
    return nullptr;
}

/* Right-clicks @p item and picks the item called @p pick (none: closes
   it), keeping the menu's texts. Grabbed as @p shot. */
QStringList rightClick(FixtureManager &mgr, QTreeWidgetItem *item, const QString &pick,
                       const QString &shot = QString())
{
    QStringList texts;
    auto poll = std::make_shared<std::function<void()> >();
    *poll = [&texts, poll, pick, shot, &mgr]() {
        QMenu *m = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (m == nullptr)
        {
            QTimer::singleShot(10, *poll);
            return;
        }
        QAction *hit = nullptr;
        foreach (QAction *a, m->actions())
            if (!a->isSeparator())
            {
                texts << a->text();
                if (a->text() == pick)
                    hit = a;
            }
        const QByteArray dir = qgetenv("QLC_SHOT_DIR");
        if (!dir.isEmpty() && !shot.isEmpty())
        {
            // The menu over the tree where it was asked for, at three sizes.
            for (const QSize &sz : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
            {
                mgr.resize(sz);
                QApplication::processEvents();
                QPixmap pm = mgr.grab();
                QPainter p(&pm);
                const QRect r = mgr.m_fixtures_tree->visualItemRect(mgr.m_fixtures_tree->currentItem());
                const QPoint at = mgr.m_fixtures_tree->viewport()->mapTo(&mgr, r.center());
                p.drawPixmap(at, m->grab());
                p.end();
                pm.save(QString::fromLocal8Bit(dir) + QStringLiteral("/%1-%2x%3.png")
                            .arg(shot).arg(sz.width()).arg(sz.height()));
            }
        }
        if (hit != nullptr)
        {
            m->setActiveAction(hit);
            QTest::keyClick(m, Qt::Key_Return);     // exec() only reports a real pick
            return;
        }
        m->close();
    };
    QTimer::singleShot(0, *poll);
    mgr.m_fixtures_tree->setCurrentItem(item);
    mgr.slotContextMenuRequested(mgr.m_fixtures_tree->visualItemRect(item).center());
    return texts;
}

Fixture *showWled(Doc *doc, const QString &name, const QString &ip)
{
    QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef("WLED", "Effect Mode");
    if (def == nullptr)
        return nullptr;
    Fixture *fx = new Fixture(doc);
    fx->setName(name);
    QLCFixtureMode *mode = nullptr;
    foreach (QLCFixtureMode *m, def->modes())
        if (mode == nullptr && m->name().startsWith("Effect"))
            mode = m;
    fx->setFixtureDefinition(def, mode);
    fx->setConnection(Fixture::Network);
    fx->setTransport(Fixture::WledTransport);
    fx->setIPAddress(ip);
    fx->setPatched(false);
    if (!doc->addFixture(fx))
        return nullptr;
    return fx;
}

bool waitFor(const std::function<bool()> &cond, int ms = 5000)
{
    QElapsedTimer t;
    t.start();
    while (!cond() && t.elapsed() < ms)
        QTest::qWait(20);
    return cond();
}
}

void WledDiscovery_Test::initTestCase()
{
    QVERIFY(QDir(TEST_PLUGINS_DIR).exists());
}

void WledDiscovery_Test::init()
{
    m_doc = new Doc(this);
    // The definition patch() makes, as the app has it.
    QLCFixtureDef *def = new QLCFixtureDef();
    QCOMPARE(def->loadXML(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/WLED/WLED-Effect-Mode.qxf"),
             QFile::NoError);
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(def));
    // The WLED output, which a WLED's own universe goes out through.
    m_doc->ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*wled*"));
    QLCIOPlugin *wled = m_doc->ioPluginCache()->plugin("WLED");
    QVERIFY2(wled != nullptr, "the WLED output plugin did not load");
    // It never gets round to asking or sending in a test.
    QVERIFY(QMetaObject::invokeMethod(wled, "setSendInterval", Q_ARG(int, 3600000)));
    QVERIFY(QMetaObject::invokeMethod(wled, "setInfoInterval", Q_ARG(int, 3600000)));
}

void WledDiscovery_Test::cleanup()
{
    ShowStatus::instance()->clearStatus("wled.moved");
    ShowStatus::instance()->clearStatus("wled.config");
    delete m_doc;
    m_doc = nullptr;
}

void WledDiscovery_Test::aRealWledsAnswerIsRead()
{
    const QByteArray packet = QByteArray::fromHex(kRecorded);
    const QList<WledDiscovery::Answer> got = WledDiscovery::parseResponse(packet);
    QCOMPARE(got.size(), 1);
    QCOMPARE(got.first().instance, QString("wled-f992e0"));
    QCOMPARE(got.first().host, QString("wled-f992e0.local"));
    QCOMPARE(int(got.first().port), 80);
    QCOMPARE(got.first().ip, QString("172.18.2.100"));
    QCOMPARE(got.first().mac, QString("e08cfef992e0"));

    // A question is not an answer; every cut of the packet is read safely.
    QVERIFY(WledDiscovery::parseResponse(WledDiscovery::browseQuery()).isEmpty());
    for (int n = 0; n < packet.size(); n++)
        WledDiscovery::parseResponse(packet.left(n));
    // A pointer loop is refused, not followed for ever.
    QByteArray loop = QByteArray::fromHex("000084000000000100000000c00c000c0001000000780002c00c");
    QVERIFY(WledDiscovery::parseResponse(loop).isEmpty());

    // Our own synthetic answer reads the same way.
    const QList<WledDiscovery::Answer> ours =
        WledDiscovery::parseResponse(answer("staff-l", "127.0.0.1", 8080, "aabbccddeeff"));
    QCOMPARE(ours.size(), 1);
    QCOMPARE(ours.first().ip, QString("127.0.0.1"));
    QCOMPARE(int(ours.first().port), 8080);
    QCOMPARE(ours.first().mac, QString("aabbccddeeff"));
}

void WledDiscovery_Test::theQuestionAsksForWleds()
{
    const QByteArray q = WledDiscovery::browseQuery();
    QCOMPARE(q.toHex(), QByteArray("000000000001000000000000055f776c6564045f746370056c6f63616c00000c8001"));
}

void WledDiscovery_Test::hostNamesAreWhatWledTakes()
{
    QCOMPARE(WledDiscovery::hostName("Staff L"), QString("staff-l"));
    QCOMPARE(WledDiscovery::hostName("Lantern #6"), QString("lantern-6"));
    QCOMPARE(WledDiscovery::hostName("  -- Staff  L -- "), QString("staff-l"));
    QCOMPARE(WledDiscovery::hostName(QString(40, 'a')), QString(32, 'a'));
    QCOMPARE(WledDiscovery::hostName("!!!"), QString());
}

void WledDiscovery_Test::onlyRigNetworksAreAskedWhileTheGateHolds()
{
    WledDiscovery found(m_doc);
    QVERIFY(found.mayAskFrom(QHostAddress("10.4.12.7")));          // no rig networks: anywhere
    m_doc->inputOutputMap()->setRigNetworks({ "172.18.2.0/24" });
    m_doc->inputOutputMap()->setTransmitGateEnabled(true);
    QVERIFY(m_doc->inputOutputMap()->transmitGateActive());
    QVERIFY(found.mayAskFrom(QHostAddress("172.18.2.101")));
    QVERIFY(!found.mayAskFrom(QHostAddress("10.4.12.7")));        // the school's network
    m_doc->inputOutputMap()->setTransmitOverride(true);            // "Transmit anyway"
    QVERIFY(found.mayAskFrom(QHostAddress("10.4.12.7")));
}

void WledDiscovery_Test::aFoundWledIsPatchedWithOneRightClick()
{
    FakeWled wled;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    wled.info = QJsonObject{ { "ver", "16.0.1" }, { "name", "Staff L" }, { "mac", "e0:8c:fe:f9:92:e0" },
                             { "leds", QJsonObject{ { "count", 50 }, { "maxseg", 32 }, { "rgbw", false } } } };
    wled.cfg = realCfg(50);

    QWidget parent;
    Monitor *mon = new Monitor(&parent, m_doc);
    WledDiscovery found(m_doc);
    QStringList asked;
    found.setSender([&asked](const QByteArray &q) { asked << QString::fromLatin1(q.toHex()); });
    FixtureManager mgr(nullptr, m_doc);
    mgr.resize(1280, 800);
    mgr.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mgr));
    QVERIFY2(foundRow(mgr, true) == nullptr, "nothing found yet: no folder");

    QCOMPARE(asked.size(), 1);                      // the tab opening asked the network
    QCOMPARE(asked.first(), QString::fromLatin1(WledDiscovery::browseQuery().toHex()));
    mgr.hide();
    mgr.show();
    QCOMPARE(asked.size(), 1);                      // not again within a few seconds
    found.receive(answer("wled-f992e0", "127.0.0.1", wled.serverPort(), "e08cfef992e0"), QHostAddress::LocalHost);
    QVERIFY(waitFor([&found]() { return found.notInShow().size() == 1; }));
    const WledDiscovery::Device d = found.notInShow().first();
    QCOMPARE(d.name, QString("Staff L"));
    QCOMPARE(d.mac, QString("e08cfef992e0"));
    QCOMPARE(d.leds, 50);
    QCOMPARE(d.ip, wled.address());

    // In the Inventory, greyed, with what it is.
    QTreeWidgetItem *row = foundRow(mgr);
    QVERIFY2(row != nullptr, "the found WLED is not in the Inventory");
    QCOMPARE(row->text(KColumnName), QString("Staff L — WLED (%1) · 50 LEDs").arg(wled.address()));
    QVERIFY(row->font(KColumnName).italic());
    QCOMPARE(foundRow(mgr, true)->text(KColumnName), QString("Found on the network (1)"));
    QCOMPARE(foundRow(mgr, true)->parent()->text(KColumnName), QString("Inventory"));    // not counted
    mgr.m_fixtures_tree->setCurrentItem(row);
    QVERIFY(mgr.m_info != nullptr && mgr.m_info->toPlainText().contains("e08cfef992e0"));
    shots(&mgr, "inventory-found");

    // Right-click ▸ Patch.
    const int fixtures = m_doc->fixtures().size();
    const QStringList texts = rightClick(mgr, row, "Patch", "inventory-found-menu");
    QCOMPARE(texts, QStringList({ "Look for WLEDs Again", "Patch" }));
    QCOMPARE(m_doc->fixtures().size(), fixtures + 1);
    Fixture *fx = m_doc->fixtures().last();
    QCOMPARE(fx->name(), QString("Staff L"));
    QCOMPARE(fx->ipAddress(), wled.address());
    QCOMPARE(fx->macAddress(), QString("e08cfef992e0"));
    QCOMPARE(fx->connection(), Fixture::Network);
    QCOMPARE(fx->transport(), Fixture::WledTransport);
    QCOMPARE(fx->fixtureDef()->model(), QString("Effect Mode"));
    QVERIFY(fx->fixtureMode()->name().startsWith("Effect"));
    QCOMPARE(m_doc->monitorProperties()->fixtureRigProps(fx->id()).lightLeds, 50);
    QVERIFY(fx->isPatched());
    QCOMPARE(PatchPlanner::wledRoute(m_doc, fx), PatchPlanner::WledRouted);
    QCOMPARE(m_doc->inputOutputMap()->outputPatch(fx->universe())->pluginName(), QString("WLED"));
    QVERIFY(found.notInShow().isEmpty());
    QVERIFY2(foundRow(mgr) == nullptr, "patched, it is no longer \"found\"");
    QVERIFY(mgr.m_fixtures_tree->fixtureItem(fx->id()) != nullptr);
    QVERIFY(wled.posted.isEmpty());                // no WledConfigCheck here: nothing offered

    // One undo: gone, its universe free, found again.
    const quint32 fid = fx->id();
    QVERIFY(mon->canUndo());
    mon->undo();
    QVERIFY(m_doc->fixture(fid) == nullptr);
    QCOMPARE(m_doc->fixtures().size(), fixtures);
    QCOMPARE(found.notInShow().size(), 1);
    mgr.updateView();
    QVERIFY(foundRow(mgr) != nullptr);
    delete mon;
}

void WledDiscovery_Test::aFixtureLearnsItsWledsMacAndSavesIt()
{
    FakeWled wled;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    wled.info = QJsonObject{ { "name", "WLED" }, { "mac", "e08cfef992e0" }, { "leds", QJsonObject{ { "count", 50 } } } };
    Fixture *fx = showWled(m_doc, "WLED Test", wled.address());
    QVERIFY(fx != nullptr);
    QVERIFY(fx->macAddress().isEmpty());

    WledDiscovery found(m_doc);
    found.setSender([](const QByteArray &) {});
    found.receive(answer("wled-f992e0", "127.0.0.1", wled.serverPort(), "e08cfef992e0"), QHostAddress::LocalHost);
    QVERIFY(waitFor([&found]() { return found.devices().size() == 1; }));
    QCOMPARE(fx->macAddress(), QString("e08cfef992e0"));
    QVERIFY2(found.notInShow().isEmpty(), "its IP is in the show: not \"found\"");
    QVERIFY(found.moves().isEmpty());

    // Saved with it, as an attribute older builds pass over; read back.
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    QVERIFY(fx->saveXML(&w));
    w.writeEndDocument();
    QVERIFY2(buf.data().contains(QByteArray("<IPAddress MAC=\"e08cfef992e0\">") + wled.address().toLatin1()
                                 + "</IPAddress>"), buf.data().constData());
    QXmlStreamReader r(buf.data());
    r.readNextStartElement();
    Fixture back(m_doc);
    QVERIFY(back.loadXML(r, m_doc, m_doc->fixtureDefCache()));
    QCOMPARE(back.macAddress(), QString("e08cfef992e0"));
    QCOMPARE(back.ipAddress(), wled.address());
}

void WledDiscovery_Test::aWledAtANewAddressIsFollowed()
{
    QWidget parent;
    Monitor *mon = new Monitor(&parent, m_doc);
    Fixture *fx = showWled(m_doc, "WLED Test", "127.0.0.100");
    QVERIFY(fx != nullptr);
    fx->setMacAddress("e08cfef992e0");
    QCOMPARE(FixtureCreator::routeWled(nullptr, m_doc, { fx->id() }, false), 1);
    const quint32 uni = fx->universe();
    QCOMPARE(m_doc->inputOutputMap()->outputPatch(uni)->getPluginParameters().value("outputIP").toString(),
             QString("127.0.0.100"));

    // It answers at .105 now (its info is not needed to know it is it).
    WledDiscovery found(m_doc);
    found.setSender([](const QByteArray &) {});
    WledDiscovery::Device d;
    d.mac = "e08cfef992e0";
    d.ip = "127.0.0.105";
    d.name = "WLED";
    d.seenAt = QDateTime::currentMSecsSinceEpoch();
    found.m_devices.insert(d.mac, d);
    found.reconcile();
    QCOMPARE(found.moves().size(), 1);
    QVERIFY(found.notInShow().isEmpty());
    ShowStatus::Entry e;
    foreach (const ShowStatus::Entry &each, ShowStatus::instance()->allEntries())
        if (each.key == "wled.moved")
            e = each;
    QCOMPARE(e.severity, ShowStatus::Warning);
    QCOMPARE(e.items, QStringList({ "WLED Test is now at .105 (was 127.0.0.100)" }));
    QCOMPARE(e.itemData, QVariantList() << fx->id());
    {
        ShowStatus::instance()->setAction("wled.moved", "Follow It", [](QWidget *, const QVariantList &) {});
        ShowStatusDialog dlg;
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        const QList<QTreeWidgetItem *> rows = dlg.tree()->findItems(e.items.first(),
                                                                   Qt::MatchExactly | Qt::MatchRecursive);
        QVERIFY(!rows.isEmpty());
        dlg.tree()->setCurrentItem(rows.first());
        shots(&dlg, "footer-moved");
    }

    // Follow It: its IP, and its own universe aimed there; one undo.
    QVERIFY(found.follow(nullptr, fx->id()));
    QCOMPARE(fx->ipAddress(), QString("127.0.0.105"));
    QCOMPARE(fx->universe(), uni);
    QCOMPARE(m_doc->inputOutputMap()->outputPatch(uni)->getPluginParameters().value("outputIP").toString(),
             QString("127.0.0.105"));
    QVERIFY(found.moves().isEmpty());
    bool still = false;
    foreach (const ShowStatus::Entry &each, ShowStatus::instance()->allEntries())
        still = still || each.key == "wled.moved";
    QVERIFY2(!still, "followed: the line goes");

    mon->undo();
    QCOMPARE(fx->ipAddress(), QString("127.0.0.100"));
    QCOMPARE(m_doc->inputOutputMap()->outputPatch(uni)->getPluginParameters().value("outputIP").toString(),
             QString("127.0.0.100"));
    delete mon;
}

void WledDiscovery_Test::driftIsAFixOnDeviceFinding()
{
    Fixture *fx = showWled(m_doc, "Staff L", "127.0.0.100");
    QVERIFY(fx != nullptr);
    const WledConfigCheck::Finding f = WledConfigCheck::evaluate(m_doc, fx, realCfg());
    QCOMPARE(f.problems, QStringList({
        "is called \"WLED\" on the device, \"Staff L\" in this show",
        "takes its brightness, colour, effect, palette from other WLEDs (sync group 1): one WLED's "
        "button or app would change it",
        "lights up when it powers on (50 %), before the show drives it" }));
    const QJsonObject want{
        { "id", QJsonObject{ { "name", "Staff L" }, { "mdns", "staff-l" } } },
        { "if", QJsonObject{ { "sync", QJsonObject{ { "recv", QJsonObject{ { "bri", false }, { "col", false },
                                                                         { "fx", false }, { "pal", false } } } } } } },
        { "def", QJsonObject{ { "on", false } } } };
    QCOMPARE(f.fix, want);
    QVERIFY(!f.fix.contains("nw") && !f.fix.contains("hw"));    // Wi-Fi, LEDs, pins: never

    // As the show has it: nothing to say.
    QJsonObject right = realCfg();
    right["id"] = QJsonObject{ { "mdns", "my-own" }, { "name", "Staff L" } };   // its own mDNS name stays
    QJsonObject sync = right["if"].toObject()["sync"].toObject();
    sync["recv"] = QJsonObject{ { "bri", false }, { "col", false }, { "fx", false }, { "pal", false }, { "grp", 1 } };
    QJsonObject ifs = right["if"].toObject();
    ifs["sync"] = sync;
    right["if"] = ifs;
    right["def"] = QJsonObject{ { "on", false }, { "bri", 128 } };
    QVERIFY2(WledConfigCheck::evaluate(m_doc, fx, right).problems.isEmpty(),
             qPrintable(WledConfigCheck::evaluate(m_doc, fx, right).problems.join("; ")));

    // A Max draw, its LED supply not known: nothing guessed, only said.
    PowerProfile pp = fx->powerProfile();
    pp.maxAmps = 0.25;                              // 30 W at 120 V
    fx->setPowerProfile(pp);
    QCOMPARE(WledConfigCheck::limiterMilliamps(m_doc, fx), 0);
    const WledConfigCheck::Finding u = WledConfigCheck::evaluate(m_doc, fx, right);
    QVERIFY(u.problems.isEmpty());
    QVERIFY(u.fix.isEmpty());
    QCOMPARE(u.notes, QStringList({ "Brightness limit: not set from the show (LED supply voltage "
                                    "unknown); the WLED's own is 850 mA" }));

    // Its LED supply known (5 V): the limit, all of hw.led as it said with
    // only the limits changed.
    FixtureRigProps supply = m_doc->monitorProperties()->fixtureRigProps(fx->id());
    supply.ledSupplyVolts = 5.0f;
    m_doc->monitorProperties()->setFixtureRigProps(fx->id(), supply);
    QCOMPARE(WledConfigCheck::limiterMilliamps(m_doc, fx), 6000);   // 30 W of 5 V LEDs: 6 A
    supply.ledSupplyVolts = 12.0f;
    m_doc->monitorProperties()->setFixtureRigProps(fx->id(), supply);
    QCOMPARE(WledConfigCheck::limiterMilliamps(m_doc, fx), 2500);   // of 12 V LEDs: 2.5 A
    supply.ledSupplyVolts = 5.0f;
    m_doc->monitorProperties()->setFixtureRigProps(fx->id(), supply);
    const WledConfigCheck::Finding p = WledConfigCheck::evaluate(m_doc, fx, right);
    QCOMPARE(p.problems, QStringList({ "its brightness limit is 850 mA; this fixture's Max draw is 6000 mA of LEDs" }));
    QCOMPARE(p.changes, QStringList({ "Brightness limit: 6000 mA" }));
    QVERIFY(p.notes.isEmpty());
    QJsonObject led = FakeWled::ledCfg(50).value("led").toObject();
    led["maxpwr"] = 6000;
    QJsonArray ins = led["ins"].toArray();
    QJsonObject out = ins.first().toObject();
    out["maxpwr"] = 6000;
    ins[0] = out;
    led["ins"] = ins;
    QCOMPARE(p.fix, QJsonObject({ { "hw", QJsonObject{ { "led", led } } } }));
    // Pins, type and colour order go back exactly as they were.
    QCOMPARE(p.fix["hw"].toObject()["led"].toObject()["ins"].toArray().first().toObject().value("pin"),
             FakeWled::ledCfg(50)["led"].toObject()["ins"].toArray().first().toObject().value("pin"));

    // With a count to set too, the count keeps the new limiter.
    FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(fx->id());
    rp.lightLeds = 60;
    m_doc->monitorProperties()->setFixtureRigProps(fx->id(), rp);
    const WledConfigCheck::Finding c = WledConfigCheck::evaluate(m_doc, fx, right);
    QCOMPARE(c.ledFix["hw"].toObject()["led"].toObject()["maxpwr"].toInt(), 6000);
    QCOMPARE(c.ledFix["hw"].toObject()["led"].toObject()["ins"].toArray().first().toObject()["len"].toInt(), 60);
}

void WledDiscovery_Test::setUpAsksOnceWritesTheKeysAndReadsBack()
{
    FakeWled wled;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    wled.cfg = realCfg(50);
    Fixture *fx = showWled(m_doc, "Staff L", wled.address());
    QVERIFY(fx != nullptr);

    WledConfigCheck check(m_doc);
    QWidget over;
    over.resize(1280, 800);
    over.show();

    // A Max draw but no LED supply: said in the question, never written.
    PowerProfile pp = fx->powerProfile();
    pp.maxAmps = 0.25;
    fx->setPowerProfile(pp);

    // Not Now: nothing written.
    QString text;
    answerBox("Not Now", &text);
    check.offerSetUp(&over, fx->id());
    QVERIFY(waitFor([&text]() { return !text.isEmpty(); }));
    QVERIFY(wled.posted.isEmpty());

    // Set Up Device: exactly the keys, then read back.
    text.clear();
    answerBox("Set Up Device", &text, "setup-confirm");
    check.offerSetUp(&over, fx->id());
    QVERIFY(waitFor([&wled]() { return wled.posted.size() == 1; }));
    QVERIFY2(text.contains(QString("Set up “Staff L” (WLED (%1)) the way this show drives it?")
                               .arg(wled.address())), qPrintable(text));
    QVERIFY2(text.contains("• Name: Staff L<"), qPrintable(text));
    QVERIFY2(text.contains("• Network name: staff-l.local<"), qPrintable(text));
    QVERIFY2(text.contains("• Sync: takes nothing from other WLEDs<"), qPrintable(text));
    QVERIFY2(text.contains("• Power-on: dark until the show drives it<"), qPrintable(text));
    QVERIFY2(text.contains("Brightness limit: not set from the show (LED supply voltage unknown); "
                           "the WLED's own is 850 mA"), qPrintable(text));
    QVERIFY2(text.contains("Wi-Fi, LED type, pins and colour order stay as they are"), qPrintable(text));
    QCOMPARE(wled.posted.first().first, QByteArray("/json/cfg"));
    const QJsonObject sent = wled.posted.first().second;
    QCOMPARE(sent, QJsonObject({
        { "id", QJsonObject{ { "name", "Staff L" }, { "mdns", "staff-l" } } },
        { "if", QJsonObject{ { "sync", QJsonObject{ { "recv", QJsonObject{ { "bri", false }, { "col", false },
                                                                         { "fx", false }, { "pal", false } } } } } } },
        { "def", QJsonObject{ { "on", false } } } }));
    QVERIFY(waitFor([&check, &wled]() { return check.writeStatus(wled.address()).startsWith("Set up"); }));
    QCOMPARE(check.writeStatus(wled.address()),
             QString("Set up WLED (%1) the way this show drives it.").arg(wled.address()));
    // The device holds it, its Wi-Fi and LEDs as they were; no finding left.
    QCOMPARE(wled.cfg["id"].toObject()["name"].toString(), QString("Staff L"));
    QCOMPARE(wled.cfg["nw"], realCfg()["nw"]);
    QCOMPARE(wled.cfg["hw"], realCfg()["hw"]);
    QVERIFY(check.finding(fx->id()).problems.isEmpty());

    // Again: nothing to change, nothing asked.
    bool asked = false;
    QTimer::singleShot(400, [&asked]() { asked = QApplication::activeModalWidget() != nullptr;
                                          if (asked) QApplication::activeModalWidget()->close(); });
    check.offerSetUp(&over, fx->id());
    QTest::qWait(600);
    QVERIFY(!asked);
    QCOMPARE(wled.posted.size(), 1);
    QCOMPARE(check.writeStatus(wled.address()),
             QString("WLED (%1) is set up the way this show drives it.").arg(wled.address()));
}

void WledDiscovery_Test::aBatchIsSetUpWithOneQuestion()
{
    FakeWled left, right;
    QVERIFY(left.listen(QHostAddress::LocalHost));
    QVERIFY(right.listen(QHostAddress::LocalHost));
    left.info = QJsonObject{ { "name", "Staff L" }, { "mac", "aa0000000001" }, { "leds", QJsonObject{ { "count", 50 } } } };
    right.info = QJsonObject{ { "name", "Staff R" }, { "mac", "aa0000000002" }, { "leds", QJsonObject{ { "count", 50 } } } };
    left.cfg = realCfg(50);
    right.cfg = realCfg(50);

    QWidget parent;
    Monitor *mon = new Monitor(&parent, m_doc);
    WledConfigCheck check(m_doc);
    WledDiscovery found(m_doc);
    found.setSender([](const QByteArray &) {});
    FixtureManager mgr(nullptr, m_doc);
    mgr.resize(1280, 800);
    mgr.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mgr));
    found.receive(answer("wled-000001", "127.0.0.1", left.serverPort(), "aa0000000001"), QHostAddress::LocalHost);
    found.receive(answer("wled-000002", "127.0.0.1", right.serverPort(), "aa0000000002"), QHostAddress::LocalHost);
    QVERIFY(waitFor([&found]() { return found.notInShow().size() == 2; }));

    // The folder stands for both: one Patch, one question naming each.
    QTreeWidgetItem *folder = foundRow(mgr, true);
    QVERIFY(folder != nullptr);
    int boxes = 0;
    QString text;
    auto poll = std::make_shared<std::function<void()> >();
    auto left_ = std::make_shared<int>(400);
    *poll = [&boxes, &text, poll, left_]() {
        if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
        {
            ++boxes;
            text = box->text();
            const QByteArray dir = qgetenv("QLC_SHOT_DIR");
            if (!dir.isEmpty())
                for (const QSize &sz : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
                {
                    if (QWidget *over = box->parentWidget())
                        over->window()->resize(sz);
                    QApplication::processEvents();
                    box->grab().save(QString::fromLocal8Bit(dir) + QStringLiteral("/setup-confirm-batch-%1x%2.png")
                                         .arg(sz.width()).arg(sz.height()));
                }
            QAbstractButton *hit = nullptr;
            for (QAbstractButton *b : box->buttons())
                if (b->text() == "Set Up 2 Devices")
                    hit = b;
            if (hit != nullptr)
                hit->click();
            else
                box->reject();
        }
        if (--*left_ > 0)
            QTimer::singleShot(10, *poll);
    };
    const QStringList texts = rightClick(mgr, folder, "Patch 2 WLEDs");
    QVERIFY2(texts.contains("Patch 2 WLEDs"), qPrintable(texts.join(" | ")));
    QTimer::singleShot(0, *poll);
    QVERIFY(waitFor([&left, &right]() { return left.posted.size() == 1 && right.posted.size() == 1; }));
    QTest::qWait(300);
    QCOMPARE(boxes, 1);
    QVERIFY2(text.startsWith("Set up these 2 WLEDs the way this show drives them?"), qPrintable(text));
    QVERIFY2(text.contains(QString("<b>Staff L</b> (WLED (%1))").arg(left.address())), qPrintable(text));
    QVERIFY2(text.contains(QString("<b>Staff R</b> (WLED (%1))").arg(right.address())), qPrintable(text));
    QCOMPARE(left.posted.first().second["id"].toObject()["name"].toString(), QString("Staff L"));
    QCOMPARE(right.posted.first().second["id"].toObject()["mdns"].toString(), QString("staff-r"));
    QCOMPARE(m_doc->fixtures().size(), 2);
    delete mon;
}

void WledDiscovery_Test::aHeldClassroomIsNotSetUp()
{
    m_doc->inputOutputMap()->setTeachingHold(true);
    const QString ip = "10.255.255.1";              // never asked
    Fixture *fx = showWled(m_doc, "Staff L", ip);
    QVERIFY(fx != nullptr);
    WledConfigCheck check(m_doc);
    QNetworkAccessManager *nam = check.findChild<QNetworkAccessManager *>();
    QSignalSpy requests(nam, &QNetworkAccessManager::finished);
    check.offerSetUp(nullptr, fx->id());
    QTest::qWait(100);
    QCOMPARE(requests.count(), 0);
    QVERIFY(check.writeStatus(ip).contains("Teaching mode holds real output"));
}

void WledDiscovery_Test::liveBrowse()
{
    if (qEnvironmentVariableIntValue("QLC_WLED_LIVE_BROWSE") != 1)
        QSKIP("QLC_WLED_LIVE_BROWSE not set: no real network");
    WledDiscovery found(m_doc);
    found.browseNow();
    waitFor([&found]() { return !found.devices().isEmpty(); }, 4000);
    QTest::qWait(1000);
    foreach (const WledDiscovery::Device &d, found.devices())
        qInfo().noquote() << "live:" << d.name << WledDiscovery::describe(d) << "mac" << d.mac
                          << "host" << d.host << "ver" << d.version << "rgbw" << d.rgbw;
    QVERIFY(!found.devices().isEmpty());
}

QTEST_MAIN(WledDiscovery_Test)
