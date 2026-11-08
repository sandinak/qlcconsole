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
#include <QLineEdit>
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
#include "addfixture.h"
#undef protected
#undef private
#include "monitorproperties.h"
#include "stageplatform.h"
#include "fountain.h"
#include "pixelpath.h"
#include "truss.h"
#include "wledconfigcheck.h"
#include "fixturecreator.h"
#include "ioplugincache.h"
#include "iopluginstub.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "patchplanner.h"
#include "qlcioplugin.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "mastertimer.h"
#include "fixture.h"
#include "effectruns.h"
#include "qlcpalette.h"
#include "speedmasters.h"
#include "wledfixture.h"
#include "scene.h"
#include "qlcpalette.h"
#include "universe.h"
#include "doc.h"
#include "fixturegroup.h"
#include "programmercontroller.h"
#include "showcontrol.h"
#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "showtreeview.h"
#include "contextmenus.h"
#include <QTabWidget>
#include <QTreeWidget>
#include <atomic>
#include <algorithm>
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

/* Run @p fill on the Add Fixture dialog once it is up, then press OK: the
   dialog is modal and blocks addFromDialog(), so this comes from a timer. */
void driveAddDialog(std::function<void(AddFixture *)> fill)
{
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [fill, poll]() {
        AddFixture *dlg = qobject_cast<AddFixture *>(QApplication::activeModalWidget());
        if (dlg == nullptr)
        {
            QTimer::singleShot(10, *poll);
            return;
        }
        fill(dlg);
        dlg->m_buttonBox->button(QDialogButtonBox::Ok)->click();
    };
    QTimer::singleShot(0, *poll);
}

QList<bool> revs(const FakeWled &w)
{
    QList<bool> out;
    for (const QJsonValue &v : w.segments())
        out << v.toObject().value("rev").toBool();
    return out;
}

/* One WLED Effect Mode (15ch) on the network by WLED's API, patched to a
   universe out through the real WLED output at @p wled, and a Monitor --
   the Locate the app's right-click and Fixtures & Groups both drive. */
struct LocateRig
{
    FakeWled wled;
    Doc doc;
    QWidget parent;
    Monitor *mon = nullptr;
    quint32 fid = 0;
    QLCFixtureDef *def = nullptr;

    LocateRig() : doc(nullptr) {}
    ~LocateRig()
    {
        doc.masterTimer()->stop();
        delete mon;
    }

    bool setUp()
    {
        if (!wled.listen(QHostAddress::LocalHost))
            return false;
        // Locate as it comes: white, full, three flashes.
        QSettings().remove("monitor/locate");
        doc.ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*wled*"));
        QLCIOPlugin *plugin = doc.ioPluginCache()->plugin("WLED");
        if (plugin == nullptr)
            return false;
        // A state every 50 ms -- with the glide that pace sets (a tenth of a
        // second) wherever a step does not take it off; the device asked
        // seldom, so the asks do not take the sends' turns.
        QMetaObject::invokeMethod(plugin, "setSendInterval", Q_ARG(int, 50));
        QMetaObject::invokeMethod(plugin, "setInfoInterval", Q_ARG(int, 5000));

        def = new QLCFixtureDef();
        if (def->loadXML(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/WLED/WLED-Effect-Mode.qxf")
            != QFile::NoError || !doc.fixtureDefCache()->addFixtureDef(def))
            return false;
        QLCFixtureMode *mode = nullptr;
        for (QLCFixtureMode *each : def->modes())
            if (each->name() == "Effect (15ch)")
                mode = each;
        if (mode == nullptr)
            return false;
        Fixture *fx = new Fixture(&doc);
        fx->setName("Sprinkles");
        fx->setFixtureDefinition(def, mode);
        fx->setConnection(Fixture::Network);
        fx->setTransport(Fixture::WledTransport);
        fx->setIPAddress(wled.address());
        fx->setPatched(false);
        if (!doc.addFixture(fx))
            return false;
        fid = fx->id();
        PatchPlanner planner(&doc);
        QStringList problems;
        if (planner.apply(planner.plan({ fid }), &problems) != 1 || !problems.isEmpty())
            return false;
        OutputPatch *op = doc.inputOutputMap()->outputPatch(fx->universe());
        if (op == nullptr || op->pluginName() != "WLED")
            return false;

        parent.resize(1400, 900);
        mon = new Monitor(&parent, &doc);
        doc.setMode(Doc::Operate);
        doc.inputOutputMap()->startUniverses();
        doc.masterTimer()->start();
        return true;
    }

    /** A look: Fireworks Starburst on the device, red. */
    Scene *starburst()
    {
        QLCPalette *fx = new QLCPalette(QLCPalette::Effect, &doc);
        fx->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/wled-fireworks-starburst.js");
        fx->setWledEffect("Fireworks Starburst");
        fx->setEffectParamValue("speed", 200);
        fx->setEffectParamValue("palette", 6);
        doc.addPalette(fx);
        QLCPalette *red = new QLCPalette(QLCPalette::Color, &doc);
        red->setValue(QColor(255, 0, 0));
        doc.addPalette(red);
        Scene *scene = new Scene(&doc);
        doc.addFunction(scene);
        scene->addFixture(fid);
        scene->setValue(fid, 0, 255);           // the dimmer up
        scene->addPalette(fx->id());
        scene->addPalette(red->id());
        return scene;
    }

    /** The device's segment 0 as it now stands. */
    QJsonObject seg() const { return wled.segments().at(0).toObject(); }
    bool showsStarburst() const
    {
        return seg().value("fx").toInt() == 89
               && seg().value("col").toArray().at(0).toArray() == QJsonArray({ 255, 0, 0 });
    }
};

struct Sent
{
    int whites = 0, darks = 0, others = 0, glided = 0;
    QStringList seen;
};

/** What the device was sent from post @p from on, Locate's way: Solid white
 *  at full, dark, or anything else; and how many asked for a glide. A post
 *  without segments leaves them as the device had them (only what changed
 *  is sent), so each is read against the last segments sent before it. */
Sent sentSince(const FakeWled &wled, int from)
{
    Sent out;
    QJsonObject s;
    for (int i = 0; i < wled.posted.size(); ++i)
    {
        if (!wled.posted.at(i).first.endsWith("/json/state"))
            continue;
        const QJsonObject b = wled.posted.at(i).second;
        if (b.contains("seg"))
            s = b.value("seg").toArray().at(0).toObject();
        if (i < from)
            continue;
        out.seen << QString::fromUtf8(QJsonDocument(b).toJson(QJsonDocument::Compact));
        if (b.value("transition").toInt(-1) != 0)
            ++out.glided;
        if (!b.value("on").toBool())
            ++out.darks;
        else if (s.value("fx").toInt() == 0 && b.value("bri").toInt() == 255
                 && s.value("col").toArray().at(0).toArray() == QJsonArray({ 255, 255, 255 }))
            ++out.whites;
        else
            ++out.others;
    }
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

    // A WLED Effect Mode definition, as the fixture library has it.
    QLCFixtureDef *def = new QLCFixtureDef();
    QCOMPARE(def->loadXML(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/WLED/WLED-Effect-Mode.qxf"),
             QFile::NoError);
    QVERIFY(doc.fixtureDefCache()->addFixtureDef(def));

    /* The show already sends universe 1 as Art-Net to a node, with a dimmer
       on it: the WLED must not land there (it never hears Art-Net). */
    QLCIOPlugin *artnet = nullptr;
    {
        doc.ioPluginCache()->load(QDir(TEST_STUB_DIR, "*iopluginstub*"));
        foreach (QLCIOPlugin *p, doc.ioPluginCache()->plugins())
            if (p->name() != "WLED")
                artnet = p;
        QVERIFY2(artnet != nullptr, "the I/O plugin stub did not load");
        static_cast<IOPluginStub *>(artnet)->m_nameOverride = "ArtNet";   // never sends anywhere
    }
    QVERIFY(doc.inputOutputMap()->setOutputPatch(0, "ArtNet", QString(), 0));
    doc.inputOutputMap()->outputPatch(0)->setPluginParameter("outputIP", "10.255.255.10");
    doc.inputOutputMap()->setUniverseName(0, "U01-Office");
    Fixture *dimmer = new Fixture(&doc);
    dimmer->setName("Desk Lamp");
    dimmer->setChannels(1);
    dimmer->setUniverse(0);
    dimmer->setAddress(0);
    QVERIFY(doc.addFixture(dimmer));

    /* Add Fixture, as the Fixture Manager runs it, opened in that universe:
       Effect (15ch), Network (WLED, JSON over Wi-Fi), its IP. Universe and
       address are not asked: it gets a universe of its own. */
    bool fieldsOff = false;
    QString said;
    driveAddDialog([&](AddFixture *dlg) {
        dlg->fillTree("WLED", "Effect Mode");
        const int m = dlg->m_modeCombo->findText("Effect (15ch)");
        dlg->m_modeCombo->setCurrentIndex(m);
        dlg->slotModeActivated(m);
        dlg->m_nameEdit->setText("Sprinkles");
        dlg->slotNameEdited("Sprinkles");
        dlg->m_connectionCombo->setCurrentIndex(dlg->m_connectionCombo->findData(
            AddFixture::connectionKey(Fixture::Network, Fixture::WledTransport)));
        dlg->m_ipEdit->setText(wled.address());
        fieldsOff = !dlg->m_universeCombo->isEnabled() && !dlg->m_addressSpin->isEnabled();
        said = dlg->m_ownUniverseLabel->isHidden() ? QString() : dlg->m_ownUniverseLabel->text();
    });
    const QList<quint32> made = FixtureCreator::addFromDialog(nullptr, &doc, false, 0, nullptr);
    QVERIFY(fieldsOff);
    QCOMPARE(said, QString("Gets a universe of its own, sent to its IP."));
    QCOMPARE(made.size(), 1);
    Fixture *fx = doc.fixture(made.first());
    QVERIFY(fx != nullptr);
    QCOMPARE(fx->channels(), 15u);
    QCOMPARE(fx->transport(), Fixture::WledTransport);
    const quint32 fid = fx->id();
    MonitorProperties *props = doc.monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(fid);
    rp.lightLeds = 50;                          // Fixture Properties ▸ LEDs: the string as built
    props->setFixtureRigProps(fid, rp);

    // Patched already: on a universe of its own, out through the WLED output, at the device.
    QVERIFY(fx->isPatched());
    QVERIFY(fx->universe() != 0);
    QCOMPARE(PatchPlanner::wledRoute(&doc, fx), PatchPlanner::WledRouted);
    OutputPatch *op = doc.inputOutputMap()->outputPatch(fx->universe());
    QVERIFY(op != nullptr);
    QCOMPARE(op->pluginName(), QString("WLED"));
    QCOMPARE(op->getPluginParameters().value("outputIP").toString(), wled.address());
    QCOMPARE(op->getPluginParameters().value("wledAddress").toInt(), int(fx->address()) + 1);
    QCOMPARE(op->getPluginParameters().value("wledChannels").toInt(), 15);
    // The Art-Net universe is as it was.
    QCOMPARE(doc.inputOutputMap()->outputPatch(0)->pluginName(), QString("ArtNet"));
    QCOMPARE(doc.inputOutputMap()->outputPatch(0)->getPluginParameters().value("outputIP").toString(),
             QString("10.255.255.10"));
    QCOMPARE(doc.inputOutputMap()->getUniverseNameByID(0), QString("U01-Office"));

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
    QTRY_VERIFY(!scene->isRunning());

    /* A WLED look, as the Look Editor makes it: WLED's own Fireworks
       Starburst, Runs: Once (0.6 s), At the end: Hold. The device runs it
       for the run, then every segment freezes on its frame -- it does not
       go round again. */
    QLCPalette *burst = new QLCPalette(QLCPalette::Effect, &doc);
    burst->setName("WLED Fireworks Starburst");
    burst->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/wled-fireworks-starburst.js");
    burst->setWledEffect("Fireworks Starburst");
    burst->setEffectParamValue(EffectRuns::runsKey(), 1);
    burst->setEffectParamValue(EffectRuns::atEndKey(), 0);
    burst->setEffectParamValue(EffectRuns::lengthKey(), 600);
    QVERIFY(doc.addPalette(burst));
    Scene *look = new Scene(&doc);
    QVERIFY(doc.addFunction(look));
    look->addFixture(fid);
    // The look's own, under the effect: full, red -- no effect settings.
    look->setValue(fid, 0, 255);
    look->setValue(fid, 6, 255);
    look->addPalette(burst->id());
    const int starburst = WledFixture::effectValue(fx, "Fireworks Starburst");
    QVERIFY(starburst > 0);
    auto every = [&wled](const char *key, const QJsonValue &want) {
        const QJsonArray segs = wled.segments();
        if (segs.size() != 4)
            return false;
        for (const QJsonValue &v : segs)
            if (v.toObject().value(QLatin1String(key)) != want)
                return false;
        return true;
    };
    look->start(doc.masterTimer(), FunctionParent::master());
    QTRY_VERIFY_WITH_TIMEOUT(every("fx", starburst) && every("frz", false), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(every("frz", true), 5000);
    QVERIFY(every("fx", starburst));                    // held: still the burst, stopped
    QCOMPARE(wled.state.value("on").toBool(), true);
    QCOMPARE(revs(wled), (QList<bool>{ false, true, false, true }));   // each its own way still

    // Fired again: it runs again, unfrozen -- and freezes at the end again.
    look->stop(FunctionParent::master());
    QTRY_VERIFY(!look->isRunning());
    look->start(doc.masterTimer(), FunctionParent::master());
    QTRY_VERIFY_WITH_TIMEOUT(every("fx", starburst) && every("frz", false), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(every("frz", true), 5000);

    // At the end: Release. After its run it lets go: the look's own red
    // comes back, solid (its effect settings at rest, not latched on the
    // burst), nothing frozen.
    look->stop(FunctionParent::master());
    QTRY_VERIFY(!look->isRunning());
    burst->setEffectParamValue(EffectRuns::atEndKey(), 1);
    look->start(doc.masterTimer(), FunctionParent::master());
    QTRY_VERIFY_WITH_TIMEOUT(every("fx", starburst) && every("frz", false), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(every("fx", 0), 5000);
    QVERIFY(every("frz", false));
    for (const QJsonValue &v : wled.segments())
        QCOMPARE(v.toObject().value("col").toArray().at(0).toArray(), QJsonArray({ 255, 0, 0 }));
    QVERIFY(every("pal", 0));
    look->stop(FunctionParent::master());
    QTRY_VERIFY(!look->isRunning());

    /* The next cue sets only a colour (amber, a Colour look), and the burst
       before it lets go: the WLED goes solid amber -- the burst does not
       stay on with the new colour. */
    burst->setEffectParamValue(EffectRuns::runsKey(), 0);         // Loop
    burst->setEffectParamValue("palette", 35);
    look->start(doc.masterTimer(), FunctionParent::master());
    QTRY_VERIFY_WITH_TIMEOUT(every("fx", starburst) && every("pal", 35), 5000);
    QLCPalette *amber = new QLCPalette(QLCPalette::Color, &doc);
    amber->setName("Amber");
    amber->setValue(QColor(255, 176, 0));
    QVERIFY(doc.addPalette(amber));
    Scene *colour = new Scene(&doc);
    QVERIFY(doc.addFunction(colour));
    colour->addFixture(fid);
    colour->setValue(fid, 0, 255);
    colour->addPalette(amber->id());
    colour->start(doc.masterTimer(), FunctionParent::master());
    QTest::qWait(200);
    QVERIFY(every("fx", starburst));                    // the burst is still on top
    look->stop(FunctionParent::master());
    QTRY_VERIFY_WITH_TIMEOUT(every("fx", 0), 5000);
    QVERIFY(every("pal", 0));
    QVERIFY(every("frz", false));
    for (const QJsonValue &v : wled.segments())
        QCOMPARE(v.toObject().value("col").toArray().at(0).toArray(), QJsonArray({ 255, 176, 0 }));
    colour->stop(FunctionParent::master());
    doc.masterTimer()->stop();
    delete mon;
    qInfo().noquote() << "the whole path took" << took.elapsed() << "ms";
}

void WledShow_Test::locateFlashesAWledPlayingAnEffectSolidWhiteAndGivesTheLookBack()
{
    LocateRig rig;
    QVERIFY(rig.setUp());

    // A plain dimmer on a universe of its own, located in the same go.
    Fixture *dim = new Fixture(&rig.doc);
    dim->setName("Dimmer");
    QLCFixtureDef *dimDef = dim->genericDimmerDef(1);
    dim->setFixtureDefinition(dimDef, dim->genericDimmerMode(dimDef, 1));
    Fixture *wledFx = rig.doc.fixture(rig.fid);
    dim->setUniverse(wledFx->universe() == 0 ? 1 : 0);
    dim->setAddress(0);
    QVERIFY(rig.doc.addFixture(dim));

    Scene *scene = rig.starburst();
    scene->start(rig.doc.masterTimer(), FunctionParent::master());
    QTRY_VERIFY_WITH_TIMEOUT(rig.showsStarburst(), 5000);
    QCOMPARE(rig.wled.state.value("on").toBool(), true);

    auto dimmerLevel = [&rig, dim]() {
        QList<Universe *> ua = rig.doc.inputOutputMap()->claimUniverses();
        const uchar v = ua.at(int(dim->universe()))->preGMValue(0);
        rig.doc.inputOutputMap()->releaseUniverses(false);
        return int(v);
    };
    QCOMPARE(dimmerLevel(), 0);

    const int from = int(rig.wled.posted.size());
    bool dimmerFlashed = false;
    rig.mon->locateFixtures({ rig.fid, dim->id() });
    QVERIFY(rig.mon->m_locateSessions.contains(rig.fid));
    QVERIFY(rig.mon->m_locateSessions.contains(dim->id()));
    // Over the look: Solid white at full on the device itself.
    QTRY_VERIFY_WITH_TIMEOUT((dimmerFlashed = dimmerFlashed || dimmerLevel() == 255,
                              rig.seg().value("fx").toInt() == 0
                              && rig.seg().value("col").toArray().at(0).toArray()
                                     == QJsonArray({ 255, 255, 255 })), 2000);
    QCOMPARE(rig.wled.state.value("bri").toInt(), 255);
    QTRY_VERIFY_WITH_TIMEOUT((dimmerFlashed = dimmerFlashed || dimmerLevel() == 255,
                              rig.mon->m_locateSessions.isEmpty()), 5000);
    QVERIFY2(dimmerFlashed, "the plain dimmer located with the WLED never came up");

    // Then the look again, effect and colour, at full.
    QTRY_VERIFY_WITH_TIMEOUT(rig.showsStarburst(), 3000);
    QCOMPARE(rig.wled.state.value("on").toBool(), true);
    QCOMPARE(rig.wled.state.value("bri").toInt(), 255);
    QTRY_COMPARE(dimmerLevel(), 0);

    /* What the device was sent: a flash, dark, a flash ... -- each a step
       (transition 0), the first flash too and the look coming back -- and
       nothing in between but the look. */
    const Sent sent = sentSince(rig.wled, from);
    const QString all = sent.seen.join("\n");
    QVERIFY2(sent.whites >= 2 && sent.whites <= 3, qPrintable(all));
    QVERIFY2(sent.darks >= 2 && sent.darks <= 3, qPrintable(all));
    QVERIFY2(sent.glided == 0, qPrintable(all));
    QVERIFY2(sent.others >= 1, qPrintable(all));          // the look again
    const QJsonObject last = sent.seen.isEmpty() ? QJsonObject()
        : QJsonDocument::fromJson(sent.seen.last().toUtf8()).object();
    QCOMPARE(last.value("seg").toArray().at(0).toObject().value("fx").toInt(), 89);

    scene->stop(FunctionParent::master());
}

/* A look of Fireworks Starburst at Speed 100 on the WLED: Own speed ×2
   sends it twice as fast, on a speed master at 60 BPM half (at 120, as
   set), and the master's tempo moves it while it plays. Before, the device
   got the look's Speed whatever Own speed or the master said. */
void WledShow_Test::aWledEffectFollowsItsOwnSpeedAndItsSpeedMaster()
{
    LocateRig rig;
    QVERIFY(rig.setUp());
    Scene *scene = rig.starburst();
    QLCPalette *burst = nullptr;
    for (quint32 pid : scene->palettes())
        if (QLCPalette *p = rig.doc.palette(pid))
            if (p->type() == QLCPalette::Effect)
                burst = p;
    QVERIFY(burst != nullptr);
    burst->setEffectParamValue("speed", 100);
    burst->setEffectParamValue("__ownRate", 2.0);
    auto speed = [&rig]() { return rig.seg().value("sx").toInt(); };

    scene->start(rig.doc.masterTimer(), FunctionParent::master());
    QTRY_VERIFY_WITH_TIMEOUT(rig.seg().value("fx").toInt() == 89, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(speed(), 200, 3000);          // Own speed ×2

    SpeedMasters *sm = rig.doc.speedMasters();
    sm->setBpm(0, 60);
    burst->setEffectParamValue("__speedMaster", SpeedMasters::Master1);
    QTRY_COMPARE_WITH_TIMEOUT(speed(), 50, 3000);           // on the master, at 60
    sm->setBpm(0, 180);
    QTRY_COMPARE_WITH_TIMEOUT(speed(), 150, 3000);          // it follows the tempo
    sm->setBpm(0, 120);
    QTRY_COMPARE_WITH_TIMEOUT(speed(), 100, 3000);          // as set

    scene->stop(FunctionParent::master());
    QTRY_VERIFY(!scene->isRunning());
}

void WledShow_Test::locateLeavesAWledNothingPlaysOnAsItWas()
{
    LocateRig rig;
    QVERIFY(rig.setUp());

    // A look played and let go of: the device off, still on its effect.
    Scene *scene = rig.starburst();
    scene->start(rig.doc.masterTimer(), FunctionParent::master());
    QTRY_VERIFY_WITH_TIMEOUT(rig.showsStarburst(), 5000);
    scene->stop(FunctionParent::master());
    QTRY_VERIFY(!scene->isRunning());
    QTRY_VERIFY_WITH_TIMEOUT(!rig.wled.state.value("on").toBool(true), 3000);
    QTest::qWait(200);
    const int effectBefore = rig.seg().value("fx").toInt();

    const int from = int(rig.wled.posted.size());
    rig.mon->locateFixture(rig.fid);
    QTRY_VERIFY_WITH_TIMEOUT(rig.wled.state.value("on").toBool()
                             && rig.seg().value("fx").toInt() == 0, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(rig.mon->m_locateSessions.isEmpty(), 5000);
    QTest::qWait(300);

    // As it was: off, and its effect what the look had left, not Solid.
    QCOMPARE(rig.wled.state.value("on").toBool(), false);
    QCOMPARE(rig.seg().value("fx").toInt(), effectBefore);
    const Sent sent = sentSince(rig.wled, from);
    QVERIFY2(sent.whites >= 2 && sent.glided == 0, qPrintable(sent.seen.join("\n")));
}

void WledShow_Test::locateGivesBackAHeldTrackedEffectWithItsNewColour()
{
    LocateRig rig;
    QVERIFY(rig.setUp());
    Doc &doc = rig.doc;
    const int starburst = WledFixture::effectValue(doc.fixture(rig.fid), "Fireworks Starburst");
    QVERIFY(starburst > 0);

    // As F-11 builds it: the burst, Runs Once, At the end Hold.
    QLCPalette *burst = new QLCPalette(QLCPalette::Effect, &doc);
    burst->setName("WLED Fireworks Starburst");
    burst->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/wled-fireworks-starburst.js");
    burst->setWledEffect("Fireworks Starburst");
    burst->setEffectParamValue(EffectRuns::runsKey(), 1);
    burst->setEffectParamValue(EffectRuns::atEndKey(), 0);
    burst->setEffectParamValue(EffectRuns::lengthKey(), 600);
    QVERIFY(doc.addPalette(burst));
    QLCPalette *amber = new QLCPalette(QLCPalette::Color, &doc);
    amber->setName("Amber");
    amber->setValue(QColor(255, 176, 0));
    QVERIFY(doc.addPalette(amber));

    FixtureGroup *g = new FixtureGroup(&doc);
    g->setName("Sprinkles");
    doc.addFixtureGroup(g);
    g->assignFixture(rig.fid);
    ShowTree::Target target;
    target.groups << g->id();

    // A Tracking list: the burst in cue 1, a colour only in cue 2.
    ShowTreeController *st = doc.showTree();
    const quint32 prod = st->addProduction("Office");
    const quint32 list = st->addCueList(prod, "WLED test", ShowTree::ListMode::Tracking);
    const quint32 c1 = st->addCue(list, -1, "Burst");
    const quint32 c2 = st->addCue(list, 0, "Amber");
    QVERIFY(c1 != 0 && c2 != 0);
    QVERIFY(st->setTargetLook(c1, target, st->lookForPalette(burst->id())));
    QVERIFY(st->setTargetLook(c2, target, st->lookForPalette(amber->id())));
    QVERIFY(st->setCurrentProduction(prod));

    /* The app's tabs, as the person has them: the Show Tree, and the
       Lighting Studio (the Monitor) beside it. In Design the cue selected
       plays (the preview). */
    doc.setMode(Doc::Design);
    QTabWidget tabs;
    ShowTreeView *view = new ShowTreeView(&tabs, &doc);
    tabs.addTab(view, "Show Tree");
    tabs.addTab(rig.mon, "Lighting Studio");
    // The Monitor is the rig's: given back before the tabs go, pass or fail.
    struct GiveBack
    {
        QTabWidget *tabs; LocateRig *rig;
        ~GiveBack() { tabs->removeTab(tabs->indexOf(rig->mon)); rig->mon->setParent(&rig->parent); }
    } giveBack{ &tabs, &rig };
    tabs.setCurrentWidget(view);
    tabs.resize(1400, 900);
    tabs.show();
    QVERIFY(QTest::qWaitForWindowExposed(&tabs));
    auto select = [view](quint32 cueId) {
        for (QTreeWidgetItemIterator it(view->tree()); *it; ++it)
            if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode
                && (*it)->data(0, ShowTreeView::IdRole).toUInt() == cueId)
            {
                view->tree()->setCurrentItem(*it);
                return true;
            }
        return false;
    };
    auto col = [&rig]() { return rig.seg().value("col").toArray().at(0).toArray(); };
    const QJsonArray amberCol({ 255, 176, 0 });

    QTRY_VERIFY(select(c1));
    QTRY_VERIFY_WITH_TIMEOUT(rig.seg().value("fx").toInt() == starburst
                             && rig.seg().value("frz").toBool(), 5000);
    QVERIFY(select(c2));
    // The burst tracks into the colour cue: it plays in amber, then holds.
    QTRY_VERIFY2_WITH_TIMEOUT(col() == amberCol && rig.seg().value("frz").toBool(),
                              qPrintable(QString::fromUtf8(QJsonDocument(rig.seg()).toJson(QJsonDocument::Compact))), 5000);
    QTest::qWait(300);
    const QJsonObject before = rig.seg();
    const QJsonObject stateBefore = rig.wled.state;
    QCOMPARE(before.value("fx").toInt(), starburst);
    QCOMPARE(st->previewState(), ShowTreeController::PreviewState::Playing);

    // Right-click ▸ Locate, as the Show Tree's Library and tree offer it.
    ContextMenus::locate(&doc, { rig.fid }, view);
    QTRY_VERIFY_WITH_TIMEOUT(rig.seg().value("fx").toInt() == 0
                             && col() == QJsonArray({ 255, 255, 255 }), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(rig.mon->m_locateSessions.isEmpty(), 5000);

    // Exactly what was showing: the held burst in amber, frozen, at full.
    QTRY_VERIFY2_WITH_TIMEOUT(rig.seg().value("fx").toInt() == starburst && col() == amberCol,
                              qPrintable(QString::fromUtf8(QJsonDocument(rig.seg()).toJson(QJsonDocument::Compact))), 3000);
    QTest::qWait(300);
    const QJsonObject after = rig.seg();
    QCOMPARE(after.value("fx").toInt(), before.value("fx").toInt());
    QCOMPARE(after.value("col").toArray(), before.value("col").toArray());
    QCOMPARE(after.value("frz").toBool(), before.value("frz").toBool());
    QCOMPARE(after.value("pal").toInt(), before.value("pal").toInt());
    QCOMPARE(rig.wled.state.value("on").toBool(), stateBefore.value("on").toBool());
    QCOMPARE(rig.wled.state.value("bri").toInt(), stateBefore.value("bri").toInt());
    // From where it was asked: the Show Tree stays, and so does its cue.
    QCOMPARE(tabs.currentWidget(), static_cast<QWidget *>(view));
    QCOMPARE(st->previewState(), ShowTreeController::PreviewState::Playing);

    st->stopPreview();
}

void WledShow_Test::aSnapCueReachesTheStaffWithTheStage()
{
    FakeWled wled;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    Doc doc(this);
    doc.ioPluginCache()->load(QDir(TEST_PLUGINS_DIR, "*wled*"));
    QLCIOPlugin *plugin = doc.ioPluginCache()->plugin("WLED");
    QVERIFY2(plugin != nullptr, "the WLED output plugin did not load");
    /* The send tick at a second: a state it delivered would come hundreds
       of milliseconds after the DMX. A snap must not wait for it. */
    QVERIFY(QMetaObject::invokeMethod(plugin, "setSendInterval", Q_ARG(int, 1000)));
    QVERIFY(QMetaObject::invokeMethod(plugin, "setInfoInterval", Q_ARG(int, 60000)));

    // The stage's DMX: Art-Net, as on his rig (a stub that sends nothing).
    QLCIOPlugin *artnet = nullptr;
    doc.ioPluginCache()->load(QDir(TEST_STUB_DIR, "*iopluginstub*"));
    foreach (QLCIOPlugin *p, doc.ioPluginCache()->plugins())
        if (p->name() != "WLED")
            artnet = p;
    QVERIFY2(artnet != nullptr, "the I/O plugin stub did not load");
    static_cast<IOPluginStub *>(artnet)->m_nameOverride = "ArtNet";
    QVERIFY(doc.inputOutputMap()->setOutputPatch(0, "ArtNet", QString(), 0));

    auto dimmers = [&doc](const QString &name, int count, quint32 address) {
        FixtureGroup *g = new FixtureGroup(&doc);
        g->setName(name);
        doc.addFixtureGroup(g);
        for (int i = 0; i < count; ++i)
        {
            Fixture *f = new Fixture(&doc);
            f->setName(QString("%1 %2").arg(name).arg(i + 1));
            f->setChannels(1);
            f->setUniverse(0);
            f->setAddress(address + quint32(i));
            doc.addFixture(f);
            g->assignFixture(f->id());
        }
        return g->id();
    };
    const quint32 wash = dimmers("Wash", 4, 0);         // the stage: black at the spell
    const quint32 steps = dimmers("Steps", 6, 10);      // the steps: an effect from her

    // The staff: a WLED on its own API, its own universe, at the device.
    QLCFixtureDef *def = new QLCFixtureDef();
    QCOMPARE(def->loadXML(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/WLED/WLED-Effect-Mode.qxf"),
             QFile::NoError);
    QVERIFY(doc.fixtureDefCache()->addFixtureDef(def));
    Fixture *fx = new Fixture(&doc);
    fx->setName("Staff");
    fx->setFixtureDefinition(def, def->mode("Effect (15ch)"));
    fx->setConnection(Fixture::Network);
    fx->setTransport(Fixture::WledTransport);
    fx->setIPAddress(wled.address());
    fx->setPatched(false);
    QVERIFY(doc.addFixture(fx));
    {
        PatchPlanner planner(&doc);
        QStringList problems;
        QCOMPARE(planner.apply(planner.plan({ fx->id() }), &problems), 1);
    }
    QCOMPARE(doc.inputOutputMap()->outputPatch(fx->universe())->pluginName(), QString("WLED"));
    FixtureGroup *staffGroup = new FixtureGroup(&doc);
    staffGroup->setName("Staff");
    doc.addFixtureGroup(staffGroup);
    staffGroup->assignFixture(fx->id());
    const quint32 staff = staffGroup->id();
    const int starburst = WledFixture::effectValue(fx, "Fireworks Starburst");

    auto dimmerLook = [&doc](int level) {
        QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, &doc);
        p->setValue(level);
        doc.addPalette(p);
        return p->id();
    };
    const quint32 zero = dimmerLook(0), full = dimmerLook(255);
    QLCPalette *burst = new QLCPalette(QLCPalette::Effect, &doc);
    burst->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/wled-fireworks-starburst.js");
    burst->setWledEffect("Fireworks Starburst");
    burst->setEffectParamValue("speed", 200);
    doc.addPalette(burst);
    QLCPalette *red = new QLCPalette(QLCPalette::Color, &doc);
    red->setValue(QColor(255, 0, 0));
    doc.addPalette(red);
    QLCPalette *phaser = new QLCPalette(QLCPalette::Effect, &doc);
    phaser->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/dimmer-phaser.js");
    doc.addPalette(phaser);

    // The cue list: preshow, the spell, preshow, the spell...
    ShowTreeController *st = doc.showTree();
    const quint32 prod = st->addProduction("Spell");
    const quint32 list = st->addCueList(prod, "Act 1");
    auto target = [](quint32 g) { ShowTree::Target t; t.groups << g; return t; };
    auto off = [st](quint32 pid) { ShowTree::Look l = st->lookForPalette(pid); l.palettes.clear(); l.none = true; return l; };
    const int goes = 6;
    for (int k = 0; k < goes; ++k)
    {
        const bool spell = k % 2 == 1;
        const quint32 cue = st->addCue(list, k - 1, spell ? "Spell" : "Preshow");
        QVERIFY(cue != 0);
        QVERIFY(st->setTargetLook(cue, target(wash), st->lookForPalette(spell ? zero : full)));
        QVERIFY(st->setTargetLook(cue, target(steps), st->lookForPalette(spell ? full : zero)));
        QVERIFY(st->setTargetLook(cue, target(steps), spell ? st->lookForPalette(phaser->id()) : off(phaser->id())));
        QVERIFY(st->setTargetLook(cue, target(staff), st->lookForPalette(spell ? full : zero)));
        QVERIFY(st->setTargetLook(cue, target(staff), st->lookForPalette(red->id())));
        QVERIFY(st->setTargetLook(cue, target(staff), spell ? st->lookForPalette(burst->id()) : off(burst->id())));
    }
    QVERIFY(st->setCurrentProduction(prod));
    doc.programmer()->setAutoMoveInBlack(true);
    doc.programmer()->setMoveInBlackDarkGapMs(0);

    /* When each frame went out: a universe written, on the engine's own
       thread -- the frame the wash (DMX), the steps' effect (DMX) and the
       staff's own universe (handed to the WLED output) first show what the
       cue asks. */
    std::atomic<bool> spellNext{ false }, armed{ false };
    std::atomic<qint64> washAt{ 0 }, stepsAt{ 0 }, staffAt{ 0 };
    Universe *stage = doc.inputOutputMap()->universes().at(0);
    connect(stage, &Universe::universeWritten, stage, [&](quint32, const QByteArray &data) {
        if (!armed.load() || data.size() < 11)
            return;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (washAt.load() == 0 && uchar(data.at(0)) == (spellNext.load() ? 0 : 255))
            washAt.store(now);
        if (stepsAt.load() == 0 && (uchar(data.at(10)) > 0) == spellNext.load())
            stepsAt.store(now);
    }, Qt::DirectConnection);
    Universe *own = doc.inputOutputMap()->universes().at(int(fx->universe()));
    const int master = int(fx->address());
    connect(own, &Universe::universeWritten, own, [&](quint32, const QByteArray &data) {
        if (armed.load() && staffAt.load() == 0 && data.size() > master
            && (uchar(data.at(master)) > 0) == spellNext.load())
            staffAt.store(QDateTime::currentMSecsSinceEpoch());
    }, Qt::DirectConnection);

    doc.setMode(Doc::Operate);
    doc.inputOutputMap()->startUniverses();
    doc.masterTimer()->start();
    QTest::qWait(200);

    auto statePosts = [&wled]() {
        QList<int> out;
        for (int i = 0; i < wled.posted.size(); ++i)
            if (wled.posted.at(i).first.endsWith("/json/state"))
                out << i;
        return out;
    };
    QList<qint64> ownLag, stageLag, stepsLag;
    int brightnessOnly = 0;
    for (int k = 0; k < goes; ++k)
    {
        const bool spell = k % 2 == 1;
        if (spell)
        {
            // Dark, the staff is already on the spell's look (moved in black).
            QTRY_VERIFY_WITH_TIMEOUT(!wled.state.value("on").toBool()
                                         && wled.segments().at(0).toObject().value("fx").toInt() == starburst,
                                     3000);
            QTest::qWait(50);
        }
        const int before = statePosts().size();
        armed.store(false);
        washAt.store(0);
        stepsAt.store(0);
        staffAt.store(0);
        spellNext.store(spell);
        armed.store(true);
        ShowControl::Request go;
        go.command = ShowControl::Go;
        QVERIFY(doc.showControl()->run(go, ShowControl::RouteOther));
        // The staff told what this cue makes it: on for the spell, off for preshow.
        int post = -1;
        QElapsedTimer waited;
        waited.start();
        while (waited.elapsed() < 3000)
        {
            const QList<int> p = statePosts();
            for (int i = before; i < p.size() && post < 0; ++i)
                if (wled.posted.at(p.at(i)).second.value("on").toBool(!spell) == spell)
                    post = p.at(i);
            if (post >= 0 && washAt.load() != 0 && stepsAt.load() != 0 && staffAt.load() != 0)
                break;
            QTest::qWait(1);
        }
        QVERIFY2(washAt.load() != 0 && stepsAt.load() != 0 && staffAt.load() != 0,
                 qPrintable(QString("GO %1: a universe never showed the cue").arg(k)));
        QVERIFY2(post >= 0, qPrintable(QString("GO %1: the staff was never sent it").arg(k)));
        const QJsonObject body = wled.posted.at(post).second;
        const qint64 at = wled.postedAt.at(post);
        ownLag << at - staffAt.load();
        stageLag << at - washAt.load();
        stepsLag << at - stepsAt.load();
        QVERIFY2(at - washAt.load() < 250,
                 qPrintable(QString("GO %1: the staff came %2 ms after the stage").arg(k).arg(at - washAt.load())));
        QCOMPARE(body.value("transition").toInt(-1), 0);       // a snap, no glide
        if (spell && !body.contains("seg"))
            ++brightnessOnly;
        QTest::qWait(150);
    }
    armed.store(false);
    auto median = [](QList<qint64> l) { std::sort(l.begin(), l.end()); return l.at(l.size() / 2); };
    qInfo() << "the staff's POST after its own universe's frame, ms:" << ownLag;
    qInfo() << "after the steps' effect frame (DMX):" << stepsLag;
    qInfo() << "after the wash's frame (DMX):" << stageLag;
    /* The WLED output's part: the frame handed to it to the device having
       it. Loopback has no Wi-Fi in it; what is left is the wake-up (2 ms
       coalescing), the event loop's turn and the HTTP request -- about
       6 ms on an idle desk machine. 30 leaves five times that for a loaded
       gate machine; the middle of six GOs, so one stalled turn does not
       fail it. The old send tick (here a second) would be hundreds. */
    QVERIFY2(median(ownLag) <= 30,
             qPrintable(QString("a snap reaches the staff %1 ms after its frame").arg(median(ownLag))));
    /* Against the stage: the staff comes with the steps' effect -- both
       effect layers, which draw their first frame on the event loop, a
       frame (20 ms) after a plain layer like the wash. Each comes within
       the same 30 ms of the steps, and within two frames more of the wash. */
    QVERIFY2(median(stepsLag) <= 30,
             qPrintable(QString("the staff comes %1 ms after the steps' effect").arg(median(stepsLag))));
    QVERIFY2(median(stageLag) <= 70,
             qPrintable(QString("the staff comes %1 ms after the stage").arg(median(stageLag))));
    // Moved in black: every spell GO was its brightness alone.
    QCOMPARE(brightnessOnly, goes / 2);
    QCOMPARE(wled.segments().at(0).toObject().value("fx").toInt(), starburst);
    QCOMPARE(wled.state.value("on").toBool(), true);

    doc.programmer()->setAutoMoveInBlack(false);
    for (Function *f : doc.functions())
        f->stop(FunctionParent::master());
    QTest::qWait(100);
    doc.masterTimer()->stop();
}

QTEST_MAIN(WledShow_Test)
