/*
  Q Light Controller Plus
  wledshow_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTimer>
#include <QSettings>
#include <QtTest>

#include "wledshow_test.h"
#include "../common/fakewled.h"
#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "monitor.h"
#include "fixturefields.h"
#undef protected
#undef private
#include "monitorproperties.h"
#include "stageplatform.h"
#include "fountain.h"
#include "pixelpath.h"
#include "truss.h"
#include "wledconfigcheck.h"
#include "ioplugincache.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "patchplanner.h"
#include "qlcioplugin.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "mastertimer.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"
#include "../../../plugins/wled/src/wledlayoutstore.h"

namespace
{
/* Answers every message box that comes up while it lives: the button whose
   text starts with one of @p accept is clicked, anything else is closed
   (a box nobody expected fails the check after, instead of hanging it).
   What each said is kept. */
class BoxAnswerer
{
public:
    explicit BoxAnswerer(const QStringList &accept) : m_accept(accept)
    {
        m_timer.setInterval(20);
        QObject::connect(&m_timer, &QTimer::timeout, [this]() {
            QWidget *modal = QApplication::activeModalWidget();
            QMessageBox *box = qobject_cast<QMessageBox *>(modal);
            if (box == nullptr)
            {
                // A plain dialog (Add Fountain): OK as it stands.
                QDialog *dlg = qobject_cast<QDialog *>(modal);
                QDialogButtonBox *bb = dlg != nullptr ? dlg->findChild<QDialogButtonBox *>() : nullptr;
                if (bb != nullptr && bb->button(QDialogButtonBox::Ok) != nullptr)
                    bb->button(QDialogButtonBox::Ok)->click();
                return;
            }
            said << box->text();
            const QList<QAbstractButton *> buttons = box->buttons();
            for (QAbstractButton *b : buttons)
                for (const QString &a : std::as_const(m_accept))
                    if (b->text().remove(QLatin1Char('&')).startsWith(a))
                    {
                        b->click();
                        return;
                    }
            unexpected << box->text();
            box->reject();
        });
        m_timer.start();
    }
    QStringList said, unexpected;

private:
    QStringList m_accept;
    QTimer m_timer;
};

QList<bool> revs(const FakeWled &w)
{
    QList<bool> out;
    for (const QJsonValue &v : w.segments())
        out << v.toObject().value("rev").toBool();
    return out;
}
}

void WledShow_Test::anEffectFromAShowPlaysOnEverySpanOfAFountain()
{
    QElapsedTimer took;
    took.start();

    /* The device as it comes: streaming E1.31 pixels, 30 LEDs set up, one
       segment over them. */
    FakeWled wled;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    QJsonObject dmx{ { "uni", 1 }, { "addr", 1 }, { "mode", 4 } };
    wled.cfg = QJsonObject{ { "if", QJsonObject{ { "live", QJsonObject{ { "en", true }, { "port", 5568 },
                                                                       { "timeout", 25 }, { "dmx", dmx } } } } } };
    wled.cfg["hw"] = FakeWled::ledCfg(30);

    Doc doc(this);
    doc.ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*wled*"));
    QLCIOPlugin *plugin = doc.ioPluginCache()->plugin("WLED");
    QVERIFY2(plugin != nullptr, "the WLED output plugin did not load");
    // Quicker than on stage: states every 30 ms, the device asked every 80.
    QVERIFY(QMetaObject::invokeMethod(plugin, "setSendInterval", Q_ARG(int, 30)));
    QVERIFY(QMetaObject::invokeMethod(plugin, "setInfoInterval", Q_ARG(int, 80)));

    // Add Fixture: a WLED Effect Mode, 15 channels, on the network by WLED's API.
    QLCFixtureDef *def = new QLCFixtureDef();
    QCOMPARE(def->loadXML(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/WLED/WLED-Effect-Mode.qxf"),
             QFile::NoError);
    QVERIFY(doc.fixtureDefCache()->addFixtureDef(def));
    QLCFixtureMode *mode = nullptr;
    for (QLCFixtureMode *each : def->modes())
        if (each->name() == "Effect (15ch)")
            mode = each;
    QVERIFY(mode != nullptr);
    Fixture *fx = new Fixture(&doc);
    fx->setName("Sprinkles");
    fx->setFixtureDefinition(def, mode);
    fx->setConnection(Fixture::Network);
    fx->setTransport(Fixture::WledTransport);
    fx->setIPAddress(wled.address());
    fx->setPatched(false);                      // staged: Patch puts it somewhere
    QVERIFY(doc.addFixture(fx));
    const quint32 fid = fx->id();
    MonitorProperties *props = doc.monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(fid);
    rp.lightLeds = 50;                          // Fixture Properties ▸ LEDs: the string as built
    props->setFixtureRigProps(fid, rp);

    // Patch: the universe goes out through the WLED output, at the device.
    PatchPlanner planner(&doc);
    QStringList problems;
    QCOMPARE(planner.apply(planner.plan({ fid }), &problems), 1);
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join("; ")));
    OutputPatch *op = doc.inputOutputMap()->outputPatch(fx->universe());
    QVERIFY(op != nullptr);
    QCOMPARE(op->pluginName(), QString("WLED"));
    QCOMPARE(op->getPluginParameters().value("outputIP").toString(), wled.address());

    // The settings check finds what is wrong with it, and Fix on Device puts it right.
    WledConfigCheck check(&doc);
    check.setLedWaits(300, 3000, 40);
    check.checkNow();
    QTRY_VERIFY(!check.finding(fid).problems.isEmpty());
    QCOMPARE(check.finding(fid).deviceLeds, 30);
    {
        BoxAnswerer yes({ "Yes", "Set 50 LEDs" });
        QVERIFY(check.confirmFix(nullptr, { fid }));
        QTRY_VERIFY_WITH_TIMEOUT(wled.info.value("leds").toObject().value("count").toInt() == 50, 5000);
        bool busy = true;
        QTRY_VERIFY((check.writeStatus(wled.address(), &busy), !busy));
        QVERIFY2(yes.unexpected.isEmpty(), qPrintable(yes.unexpected.join(" | ")));
        QCOMPARE(yes.said.size(), 2);           // the settings, then the count
    }
    QCOMPARE(wled.cfg.value("if").toObject().value("live").toObject().value("en").toBool(), false);
    check.checkNow();
    QTRY_VERIFY(check.finding(fid).problems.isEmpty());

    // In a fountain, its string looped across the gap: two points, two spans each.
    QWidget parent;
    parent.resize(1400, 900);
    Monitor *mon = new Monitor(&parent, &doc);
    {
        BoxAnswerer ok({});
        mon->slotAddFountain();
    }
    QVERIFY(!props->platforms().isEmpty());
    const Fountain::Parts f = Fountain::ofGroup(props, props->platforms().first()->groupId());
    QVERIFY(f.isValid());
    const QPointF c = props->platforms().first()->centre();
    props->setFixturePosition(fid, 0, 0, QVector3D(float(c.x()) * 1000.0f, float(c.y()) * 1000.0f, 0.0f));
    QVERIFY(Fountain::mountIn(&doc, props, fid, c.x(), c.y()));

    QWidget host;
    QFormLayout *form = new QFormLayout(&host);
    FixtureFields fields(mon, form, &host);
    fields.setFixtures({ fid }, fid);
    const int idx = fields.m_pixelPath->findData(int(PixelPath::SprinkleLoops));
    QVERIFY(idx >= 0);
    auto *model = qobject_cast<QStandardItemModel *>(fields.m_pixelPath->model());
    QVERIFY(model != nullptr && model->item(idx)->isEnabled());        // it is in a fountain
    fields.m_pixelPath->setCurrentIndex(idx);
    fields.m_sprPoints->setValue(2);
    fields.m_sprPer->setValue(2);
    QCOMPARE(props->fixtureRigProps(fid).pixelPath, int(PixelPath::SprinkleLoops));

    // Write Segments…: one segment a span, every second one reversed.
    fields.setFixtures({ fid }, fid);
    QVERIFY(fields.m_sprWrite->isEnabled());
    fields.m_confirmWrite = [](const QString &, const QString &, int, int) { return true; };
    fields.m_sprWrite->click();
    QTRY_COMPARE(fields.m_sprNote->text(), QString("Wrote 4 segments to WLED (%1).").arg(wled.address()));
    QCOMPARE(wled.segments().size(), 4);
    QCOMPARE(revs(wled), (QList<bool>{ false, true, false, true }));
    QCOMPARE(wled.segments().at(3).toObject().value("stop").toInt(), 50);
    // Their ways kept as the spans' own, where the output looks for them:
    // a Reverse playing when the app quits or crashes is never taken for them.
    const QString kept = WledLayoutStore::deviceKey(QString(), wled.address());
    QCOMPARE(QSettings().value(kept + "/segments").toString(), QString("0:0-12,1:12-25,2:25-37,3:37-50"));
    QCOMPARE(QSettings().value(kept + "/rev").toString(), QString("0101"));

    /* A scene: effect 9 at speed 200, palette 6, Reverse (Option 128), red
       over blue -- played, through the universe and the WLED output. */
    Scene *scene = new Scene(&doc);
    QVERIFY(doc.addFunction(scene));
    scene->addFixture(fid);
    const int values[15] = { 255, 9, 200, 100, 6, 128, 255, 0, 0, 0, 0, 255, 0, 0, 0 };
    for (int ch = 0; ch < 15; ++ch)
        scene->setValue(fid, quint32(ch), uchar(values[ch]));
    doc.setMode(Doc::Operate);
    doc.inputOutputMap()->startUniverses();     // as the app does at start
    doc.masterTimer()->start();
    scene->start(doc.masterTimer(), FunctionParent::master());

    /* On EVERY segment: the effect, its speed, palette and colours -- and
       each turned round from its own way, not all set to one. */
    auto played = [&wled]() {
        const QJsonArray segs = wled.segments();
        if (segs.size() != 4)
            return false;
        for (const QJsonValue &v : segs)
        {
            const QJsonObject s = v.toObject();
            if (s.value("fx").toInt() != 9 || s.value("sx").toInt() != 200 || s.value("pal").toInt() != 6
                || s.value("col").toArray().at(0).toArray() != QJsonArray({ 255, 0, 0 })
                || s.value("col").toArray().at(1).toArray() != QJsonArray({ 0, 0, 255 }))
                return false;
        }
        return true;
    };
    QTRY_VERIFY_WITH_TIMEOUT(played() && revs(wled) == (QList<bool>{ true, false, true, false }), 5000);
    QCOMPARE(wled.state.value("on").toBool(), true);
    QCOMPARE(wled.state.value("bri").toInt(), 255);
    // The spans are where Write Segments put them: the look did not move them.
    QCOMPARE(wled.segments().at(1).toObject().value("start").toInt(), 12);
    QCOMPARE(wled.segments().at(1).toObject().value("stop").toInt(), 25);
    // Out of realtime mode the first time, by the API (no stream: it is off).
    bool liveCleared = false;
    for (const auto &p : std::as_const(wled.posted))
        liveCleared = liveCleared || (p.first.endsWith("/json/state") && p.second.contains("live")
                                      && !p.second.value("live").toBool(true));
    QVERIFY(liveCleared);

    // Reverse off (the scene played again without it): each span runs its own way.
    scene->stop(FunctionParent::master());
    QTRY_VERIFY(!scene->isRunning());
    scene->setValue(fid, 5, 0);
    scene->start(doc.masterTimer(), FunctionParent::master());
    QTRY_VERIFY_WITH_TIMEOUT(revs(wled) == (QList<bool>{ false, true, false, true }), 5000);
    QVERIFY(played());

    scene->stop(FunctionParent::master());
    doc.masterTimer()->stop();
    delete mon;
    qInfo().noquote() << "the whole path took" << took.elapsed() << "ms";
}

QTEST_MAIN(WledShow_Test)
