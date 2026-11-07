/*
  Q Light Controller Plus
  wledconfigcheck_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>
#include <QDir>
#include <QElapsedTimer>

#include "wledconfigcheck_test.h"
#include "../common/fakewled.h"
#include "wleddevicewrite.h"
#include "monitorproperties.h"
#include "pixelpath.h"
#include "truss.h"
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
QStringList s_messages;
QtMessageHandler s_previousHandler = nullptr;

void collectMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    s_messages << msg;
    if (s_previousHandler != nullptr)
        s_previousHandler(type, context, msg);
}

int messagesWith(const QString &text)
{
    int n = 0;
    foreach (const QString &m, s_messages)
        n += m.contains(text) ? 1 : 0;
    return n;
}

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
    QCOMPARE(wled.posted.first().second,
             QJsonObject({ { "if", QJsonObject{ { "live", QJsonObject{ { "en", false } } } } } }));
    QTRY_VERIFY(check.finding(fx->id()).problems.isEmpty());
    bool still = false;
    foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
        still = still || e.key == "wled.config";
    QVERIFY(!still);
    QVERIFY(changed.count() >= 2);
}

void WledConfigCheck_Test::anUnreachableDeviceIsSaidOnceAndNeverRead()
{
    /* On the Linux soak a WLED that was switched off logged "QIODevice::read
       (QNetworkReplyHttpImpl): device not open" every 30 s: each failed reply
       was read. Now it is not read, and going quiet is said once -- and
       coming back once. A port nothing listens on refuses at once. */
    quint16 port = 0;
    {
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress::LocalHost));
        port = probe.serverPort();
    }
    const QString ip = QString("127.0.0.1:%1").arg(port);
    Fixture *fx = lantern(m_doc, ip, Fixture::WledTransport, "Effect (15ch)");
    QVERIFY(fx != nullptr);
    QVERIFY(m_doc->addFixture(fx));

    WledConfigCheck check(m_doc);
    QNetworkAccessManager *nam = check.findChild<QNetworkAccessManager *>();
    QVERIFY(nam != nullptr);
    QSignalSpy finished(nam, &QNetworkAccessManager::finished);

    s_messages.clear();
    s_previousHandler = qInstallMessageHandler(collectMessage);
    auto restore = qScopeGuard([]() { qInstallMessageHandler(s_previousHandler); });

    for (int i = 1; i <= 3; ++i)
    {
        check.checkNow();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), i, 10000);
    }
    QVERIFY(check.finding(fx->id()).problems.isEmpty());   // silent is not misconfigured
    QCOMPARE(messagesWith("device not open"), 0);
    QCOMPARE(messagesWith("does not answer"), 1);
    QCOMPARE(messagesWith("answers again"), 0);

    // Back on the network: said once, and read.
    FakeWled wled;
    if (!wled.listen(QHostAddress::LocalHost, port))
        QSKIP("the port was taken in the meantime");
    wled.cfg = liveCfg(true, 5568, 1, 1, 4);
    for (int i = 4; i <= 5; ++i)
    {
        check.checkNow();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), i, 10000);
    }
    QVERIFY(!check.finding(fx->id()).problems.isEmpty());
    QCOMPARE(messagesWith("answers again"), 1);
    QCOMPARE(messagesWith("does not answer"), 1);
    QCOMPARE(messagesWith("device not open"), 0);
}

namespace
{
/* Answers the next message box: grabs it for review (QLC_SHOT_DIR set) at
   three sizes of the window it is over, keeps its text, and clicks the
   button whose text starts with @p button. Closes it whatever happens, so a
   wrong lookup fails the test instead of hanging it. */
void answerBox(const QString &button, QString *text, const QString &shot = QString())
{
    QTimer::singleShot(0, [button, text, shot]() {
        QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (box == nullptr)
            return;
        if (text != nullptr)
            *text = box->text();
        const QByteArray dir = qgetenv("QLC_SHOT_DIR");
        if (!dir.isEmpty() && !shot.isEmpty())
        {
            QDir().mkpath(QString::fromLocal8Bit(dir));
            QWidget *over = box->parentWidget();
            const QList<QSize> sizes = { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) };
            for (const QSize &sz : sizes)
            {
                if (over != nullptr)
                    over->resize(sz);
                QApplication::processEvents();
                box->grab().save(QString::fromLocal8Bit(dir) + QStringLiteral("/%1-%2x%3.png")
                                     .arg(shot).arg(sz.width()).arg(sz.height()));
            }
        }
        QAbstractButton *hit = nullptr;
        const QList<QAbstractButton *> buttons = box->buttons();
        for (QAbstractButton *b : buttons)
            if (b->text().startsWith(button))
                hit = b;
        if (hit != nullptr)
            hit->click();
        else
            box->reject();
    });
}

QJsonObject wledCfg(int leds, int outputs = 1)
{
    QJsonObject cfg = liveCfg(false, 5568, 1, 1, 4);
    cfg["hw"] = FakeWled::ledCfg(leds, outputs);
    return cfg;
}

Fixture *stringOf(Doc *doc, const QString &ip, int leds)
{
    Fixture *fx = lantern(doc, ip, Fixture::WledTransport, "Effect (15ch)");
    if (fx == nullptr || !doc->addFixture(fx))
        return nullptr;
    FixtureRigProps rp = doc->monitorProperties()->fixtureRigProps(fx->id());
    rp.lightLeds = leds;            // Fixture Properties ▸ LEDs: the string as built
    doc->monitorProperties()->setFixtureRigProps(fx->id(), rp);
    return fx;
}
}

void WledConfigCheck_Test::aDifferentLedCountIsFoundWithItsOwnFix()
{
    Fixture *fx = stringOf(m_doc, "172.18.2.100", 50);
    QVERIFY(fx != nullptr);
    const QJsonObject cfg = wledCfg(30);
    WledConfigCheck::Finding f = WledConfigCheck::evaluate(m_doc, fx, cfg);
    QCOMPARE(WledConfigCheck::fixtureLedCount(m_doc, fx), 50);
    QVERIFY2(f.problems.contains("the WLED drives 30 LEDs; this fixture has 50"),
             qPrintable(f.problems.join(" | ")));
    QCOMPARE(f.fixtureLeds, 50);
    QCOMPARE(f.deviceLeds, 30);
    QVERIFY(f.fix.isEmpty());                       // never folded into the other fixes
    QVERIFY(f.ledRefusal.isEmpty());

    // All of hw.led back as it was, only the length and the total changed.
    QJsonObject want = cfg.value("hw").toObject().value("led").toObject();
    QJsonObject out = want.value("ins").toArray().first().toObject();
    out["len"] = 50;
    want["ins"] = QJsonArray{ out };
    want["total"] = 50;
    QCOMPARE(f.ledFix, QJsonObject({ { "hw", QJsonObject{ { "led", want } } } }));
    const QJsonObject sent = f.ledFix.value("hw").toObject().value("led").toObject()
                                 .value("ins").toArray().first().toObject();
    QCOMPARE(sent.value("pin").toArray(), QJsonArray{ 16 });    // pins, type, order as they were
    QCOMPARE(sent.value("type").toInt(), 22);
    QCOMPARE(sent.value("order").toInt(), 0);

    // The same count, or none said: nothing.
    QVERIFY(WledConfigCheck::evaluate(m_doc, fx, wledCfg(50)).problems.isEmpty());
    FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(fx->id());
    rp.lightLeds = -1;                              // the definition's guess only
    m_doc->monitorProperties()->setFixtureRigProps(fx->id(), rp);
    QCOMPARE(WledConfigCheck::fixtureLedCount(m_doc, fx), 0);
    QVERIFY(WledConfigCheck::evaluate(m_doc, fx, cfg).problems.isEmpty());
}

void WledConfigCheck_Test::aMultiOutputWledIsRefused()
{
    Fixture *fx = stringOf(m_doc, "172.18.2.100", 50);
    QVERIFY(fx != nullptr);
    WledConfigCheck::Finding f = WledConfigCheck::evaluate(m_doc, fx, wledCfg(15, 2));
    QCOMPARE(f.deviceLeds, 30);
    QVERIFY(f.ledFix.isEmpty());
    QVERIFY2(f.ledRefusal.contains("2 outputs"), qPrintable(f.ledRefusal));

    // A 2D matrix: its size is the device's own 2D settings.
    QJsonObject cfg = wledCfg(30);
    QJsonObject hw = cfg.value("hw").toObject(), led = hw.value("led").toObject();
    led["matrix"] = QJsonObject{ { "mpc", 1 } };
    hw["led"] = led;
    cfg["hw"] = hw;
    f = WledConfigCheck::evaluate(m_doc, fx, cfg);
    QVERIFY(f.ledFix.isEmpty());
    QVERIFY(f.ledRefusal.contains("2D"));

    // Fix on Device says so, and writes nothing.
    WledConfigCheck check(m_doc);
    FakeWled wled;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    wled.cfg = wledCfg(15, 2);
    fx->setIPAddress(wled.address());
    check.checkNow();
    QTRY_VERIFY(!check.finding(fx->id()).ledRefusal.isEmpty());
    QString text;
    answerBox("OK", &text);
    QVERIFY(!check.confirmFix(nullptr, { fx->id() }));
    QVERIFY2(text.contains("can't be set from here"), qPrintable(text));
    QVERIFY(wled.posted.isEmpty());
}

void WledConfigCheck_Test::aHeldClassroomWritesNothing()
{
    m_doc->inputOutputMap()->setTeachingHold(true);
    QVERIFY(WledDeviceWrite::heldReason(m_doc, "172.18.2.100").contains("Teaching mode holds real output"));
    QVERIFY(WledDeviceWrite::heldReason(m_doc, "127.0.0.1:8080").isEmpty());   // this computer itself
    const QString ip = "10.255.255.1";
    Fixture *fx = stringOf(m_doc, ip, 50);
    QVERIFY(fx != nullptr);

    // What it said, as if read: streams on (a fix) and 30 LEDs (a count).
    WledConfigCheck check(m_doc);
    QNetworkAccessManager *nam = check.findChild<QNetworkAccessManager *>();
    QSignalSpy requests(nam, &QNetworkAccessManager::finished);
    QJsonObject cfg = wledCfg(30);
    cfg["if"] = liveCfg(true, 5568, 1, 1, 4).value("if");
    check.receive(ip, QJsonDocument(cfg).toJson(), true);
    const WledConfigCheck::Finding f = check.finding(fx->id());
    QVERIFY(!f.fix.isEmpty());
    QVERIFY(!f.ledFix.isEmpty());

    // Fix on Device: refused with the reason, nothing asked or written.
    QString text;
    answerBox("OK", &text);
    QVERIFY(!check.confirmFix(nullptr, { fx->id() }));
    QVERIFY2(text.startsWith(QString("Nothing was written to WLED (%1): Teaching mode holds real output").arg(ip)),
             qPrintable(text));
    QVERIFY(!check.setLedCount(fx->id()));
    QVERIFY(check.writeStatus(ip).contains("Teaching mode"));
    check.fix({ fx->id() });
    QTest::qWait(200);
    QCOMPARE(requests.count(), 0);

    // Let through: it may.
    m_doc->inputOutputMap()->setTeachingHold(false);
    QVERIFY(WledDeviceWrite::heldReason(m_doc, ip).isEmpty());
}

void WledConfigCheck_Test::theLedCountIsSetAndReadBack()
{
    QFETCH(bool, needsReboot);
    FakeWled wled;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    wled.cfg = wledCfg(30);
    wled.countNeedsReboot = needsReboot;
    Fixture *fx = stringOf(m_doc, wled.address(), 50);
    QVERIFY(fx != nullptr);

    WledConfigCheck check(m_doc);
    check.setLedWaits(300, 3000, 50);
    check.checkNow();
    QTRY_COMPARE(check.finding(fx->id()).deviceLeds, 30);

    // Cancel: nothing written.
    QString text;
    answerBox("Cancel", &text);
    QVERIFY(!check.confirmFix(nullptr, { fx->id() }));
    QVERIFY2(text.contains("A wrong count can leave LEDs dark"), qPrintable(text));
    QVERIFY(text.contains("reboot"));
    QVERIFY(text.contains("Undo can't take it back"));
    QVERIFY(wled.posted.isEmpty());

    // Set: one write, busy, then waited for and read back.
    QWidget over;
    over.resize(1280, 800);
    QSignalSpy wrote(&check, &WledConfigCheck::writeChanged);
    answerBox("Set 50 LEDs", &text, needsReboot ? QString() : QStringLiteral("ledcount-confirm"));
    QVERIFY(check.confirmFix(&over, { fx->id() }));
    bool busy = false;
    QVERIFY(!check.writeStatus(wled.address(), &busy).isEmpty());
    QVERIFY(busy);
    QTRY_VERIFY_WITH_TIMEOUT((check.writeStatus(wled.address(), &busy), !busy), 8000);
    QCOMPARE(check.writeStatus(wled.address()),
             QString("WLED (%1) now drives 50 LEDs.").arg(wled.address()));
    QCOMPARE(wled.posted.first().first, QByteArray("/json/cfg"));
    QCOMPARE(wled.posted.first().second.value("hw").toObject().value("led").toObject()
                 .value("ins").toArray().first().toObject().value("len").toInt(), 50);
    QCOMPARE(wled.reboots, needsReboot ? 1 : 0);
    QCOMPARE(wled.posted.size(), needsReboot ? 2 : 1);
    QTRY_VERIFY(check.finding(fx->id()).problems.isEmpty());
    QVERIFY(wrote.count() >= 2);
}

void WledConfigCheck_Test::theLedCountIsSetAndReadBack_data()
{
    QTest::addColumn<bool>("needsReboot");
    QTest::newRow("takes it at once (16.x)") << false;
    QTest::newRow("only after a reboot") << true;
}

void WledConfigCheck_Test::writeSegmentsAsksWritesAndReadsBack()
{
    FakeWled wled;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    wled.info["leds"] = QJsonObject{ { "count", 50 }, { "maxseg", 32 } };
    QVector<SprinkleSpan> spans;
    for (int k = 0; k < 4; ++k)
    {
        SprinkleSpan sp;
        sp.firstLed = k * 12 + 1;
        sp.lastLed = k * 12 + 10;               // two dark between spans
        sp.point = 0.5f;
        spans << sp;
    }

    // Cancelled once read: nothing written.
    {
        WledDeviceWrite w;
        QSignalSpy done(&w, &WledDeviceWrite::finished);
        int asked = -1, had = -1;
        w.writeSegments(wled.address(), spans, [&](int n, int existing) { asked = n; had = existing; return false; });
        QVERIFY(w.isBusy());
        QVERIFY(w.busyText().startsWith("Reading WLED ("));
        QTRY_COMPARE(done.count(), 1);
        QCOMPARE(asked, 4);
        QCOMPARE(had, 1);
        QCOMPARE(done.first().at(1).toString(), QString("Nothing was written."));
        QVERIFY(wled.posted.isEmpty());
    }
    // Written and read back.
    {
        WledDeviceWrite w;
        QSignalSpy done(&w, &WledDeviceWrite::finished);
        QStringList steps;
        connect(&w, &WledDeviceWrite::progress, &w, [&steps](const QString &t) { steps << t; });
        w.writeSegments(wled.address(), spans, [](int, int) { return true; });
        QTRY_COMPARE(done.count(), 1);
        QVERIFY2(done.first().at(0).toBool(), qPrintable(done.first().at(1).toString()));
        QCOMPARE(done.first().at(1).toString(), QString("Wrote 4 segments to WLED (%1).").arg(wled.address()));
        QVERIFY(steps.join("|").contains("Writing 4 segments to WLED"));
        QCOMPARE(wled.posted.size(), 1);
        QCOMPARE(wled.posted.first().first, QByteArray("/json/state"));
        const QJsonArray segs = wled.segments();
        QCOMPARE(segs.size(), 4);
        QCOMPARE(segs.at(3).toObject().value("start").toInt(), 36);
        QCOMPARE(segs.at(3).toObject().value("stop").toInt(), 46);
        QCOMPARE(segs.at(1).toObject().value("rev").toBool(), true);
        QVERIFY(!w.isBusy());
    }
    // Fewer spans than it has segments: the rest cleared.
    {
        WledDeviceWrite w;
        QSignalSpy done(&w, &WledDeviceWrite::finished);
        w.writeSegments(wled.address(), spans.mid(0, 2), [](int, int) { return true; });
        QTRY_COMPARE(done.count(), 1);
        QVERIFY(done.first().at(0).toBool());
        QCOMPARE(wled.segments().size(), 2);
    }
    // More than it keeps: refused, before anything is asked or written.
    {
        wled.info["leds"] = QJsonObject{ { "count", 50 }, { "maxseg", 3 } };
        wled.posted.clear();
        WledDeviceWrite w;
        QSignalSpy done(&w, &WledDeviceWrite::finished);
        bool asked = false;
        w.writeSegments(wled.address(), spans, [&asked](int, int) { asked = true; return true; });
        QTRY_COMPARE(done.count(), 1);
        QVERIFY(!done.first().at(0).toBool());
        QVERIFY(done.first().at(1).toString().contains("at most 3 segments"));
        QVERIFY(done.first().at(1).toString().contains("ledmap"));
        QVERIFY(!asked);
        QVERIFY(wled.posted.isEmpty());
    }
    // A device that does not answer: said so.
    {
        wled.silent = true;
        WledDeviceWrite w;
        QSignalSpy done(&w, &WledDeviceWrite::finished);
        w.writeSegments(wled.address(), spans, [](int, int) { return true; });
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 8000);
        QVERIFY2(done.first().at(1).toString().contains("did not answer"),
                 qPrintable(done.first().at(1).toString()));
    }
}

void WledConfigCheck_Test::theConfirmationsSayWhatTheyDo()
{
    QWidget over;
    over.resize(1280, 800);
    QString text;
    answerBox("Write 16 Segments", &text, "segments-confirm");
    QVERIFY(WledDeviceWrite::confirmSegments(&over, "Sprinkles", "172.18.2.100", 16, 1));
    QVERIFY2(text.contains("Write 16 segments to the WLED at 172.18.2.100?"), qPrintable(text));
    QVERIFY(text.contains("every second one reversed"));
    QVERIFY(text.contains("Undo can't take it back"));
    answerBox("Cancel", &text);
    QVERIFY(!WledDeviceWrite::confirmSegments(&over, "Sprinkles", "172.18.2.100", 16, 1));

    answerBox("Set 50 LEDs", &text);
    QVERIFY(WledDeviceWrite::confirmLedCount(&over, "Sprinkles", "172.18.2.100", 30, 50));
    QVERIFY(text.contains("Set the WLED at 172.18.2.100 to drive 50 LEDs? It drives 30 now"));
    answerBox("Cancel", &text);
    QVERIFY(!WledDeviceWrite::confirmLedCount(&over, "Sprinkles", "172.18.2.100", 30, 50));
}

/* Against a real WLED, by hand only: QLC_WLED_LIVE=<ip[:port]> (and
   QLC_WLED_LIVE_LEDS, default 50). Sets its LED count to that through
   setLedCount(), writes four spans as segments and reads them back, then
   puts back one segment over the whole string. Only WLED's JSON API is
   used: no DMX is sent. Skipped in the gate. */
void WledConfigCheck_Test::liveDevice()
{
    const QString ip = qEnvironmentVariable("QLC_WLED_LIVE");
    if (ip.isEmpty())
        QSKIP("QLC_WLED_LIVE not set: no real device");
    const int leds = qEnvironmentVariableIntValue("QLC_WLED_LIVE_LEDS") > 0
                         ? qEnvironmentVariableIntValue("QLC_WLED_LIVE_LEDS") : 50;
    Fixture *fx = stringOf(m_doc, ip, leds);
    QVERIFY(fx != nullptr);
    WledConfigCheck check(m_doc);
    QSignalSpy changed(&check, &WledConfigCheck::changed);
    check.checkNow();
    QTest::qWait(3000);
    const WledConfigCheck::Finding f = check.finding(fx->id());
    qInfo().noquote() << "live: finding" << f.problems << "device" << f.deviceLeds << "fixture" << f.fixtureLeds;
    if (!f.ledFix.isEmpty())
    {
        qInfo().noquote() << "live: /json/cfg POST" << QJsonDocument(f.ledFix).toJson(QJsonDocument::Compact);
        QVERIFY(check.setLedCount(fx->id()));
        bool busy = true;
        QStringList seen;
        QElapsedTimer t;
        t.start();
        while (busy && t.elapsed() < 90000)
        {
            QTest::qWait(250);
            const QString now = check.writeStatus(ip, &busy);
            if (seen.isEmpty() || seen.last() != now)
                seen << now;
        }
        qInfo().noquote() << "live: LED count steps:" << seen.join(" | ") << "in" << t.elapsed() << "ms";
        QCOMPARE(check.writeStatus(ip), QString("WLED (%1) now drives %2 LEDs.").arg(ip).arg(leds));
        QTRY_VERIFY_WITH_TIMEOUT(check.finding(fx->id()).deviceLeds == 0, 10000);   // the count matches now
    }

    auto write = [&](const QVector<SprinkleSpan> &spans) {
        WledDeviceWrite w;
        QSignalSpy done(&w, &WledDeviceWrite::finished);
        QStringList steps;
        connect(&w, &WledDeviceWrite::progress, &w, [&steps](const QString &t) { if (!t.isEmpty()) steps << t; });
        w.writeSegments(ip, spans, [](int n, int existing) {
            qInfo().noquote() << "live: asked to write" << n << "segments over" << existing;
            return true;
        });
        if (!QTest::qWaitFor([&]() { return done.count() > 0; }, 20000))
            return QString("no answer");
        qInfo().noquote() << "live: steps" << steps.join(" | ");
        return done.first().at(1).toString();
    };
    QVector<SprinkleSpan> four;
    const int firsts[] = { 1, 13, 27, 41 }, lasts[] = { 10, 24, 38, 50 };
    for (int k = 0; k < 4; ++k)
    {
        SprinkleSpan sp;
        sp.firstLed = firsts[k];
        sp.lastLed = lasts[k];
        four << sp;
    }
    QString msg = write(four);
    qInfo().noquote() << "live: write 4:" << msg;
    QCOMPARE(msg, QString("Wrote 4 segments to WLED (%1).").arg(ip));
    SprinkleSpan whole;
    whole.firstLed = 1;
    whole.lastLed = leds;
    msg = write({ whole });
    qInfo().noquote() << "live: restore 1:" << msg;
    QCOMPARE(msg, QString("Wrote 1 segment to WLED (%1).").arg(ip));
}

QTEST_MAIN(WledConfigCheck_Test)
