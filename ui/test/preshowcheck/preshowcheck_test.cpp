/*
  Q Light Controller Plus - Unit test
  preshowcheck_test.cpp

  Before the house opens (docs/features/dress-rehearsal.md), offscreen.

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
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>
#include <QDir>

#include "preshowcheck_test.h"
#include "../common/shotdark.h"
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "showstatusdialog.h"
#include "showreviewpanel.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "preshowcheck.h"
#include "showstatus.h"
#include "showreview.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "scenery.h"
#include "qlcfile.h"
#include "fixture.h"
#include "doc.h"

namespace
{
Fixture *dimmer(Doc *doc, const QString &name, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Lamp");
    def->setType(QLCFixtureDef::Dimmer);
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setPreset(QLCChannel::IntensityDimmer);
    def->addChannel(ch);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Mode");
    mode->insertChannel(ch, 0);
    def->addMode(mode);
    Fixture *fx = new Fixture(doc);
    fx->setName(name);
    fx->setFixtureDefinition(def, mode);
    fx->setAddress(address);
    doc->addFixture(fx);
    return fx;
}

/* What this computer says, by hand: a Mac on mains with nothing wrong. */
SystemTuning::Probe goodMac()
{
    SystemTuning::Probe p;
    p.os = SystemTuning::OsMac;
    p.scheduling = MasterTimer::SchedulingNormal;
    p.fullSpeedSupported = true;
    p.fullSpeedInOperate = true;
    p.keepDisplayAwake = true;
    p.onBattery = 0;
    p.thermal = 0;
    return p;
}

/* The I/O stub (the engine's test plugin: sends nowhere) as universe 1's output. */
bool patchStubOutput(Doc *doc)
{
    IOPluginCache *cache = doc->ioPluginCache();
    if (cache->plugin(QStringLiteral("I/O Plugin Stub")) == nullptr)
    {
        QDir stubDir(QStringLiteral("../../../engine/test/iopluginstub"));
        stubDir.setFilter(QDir::Files);
        stubDir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
        cache->load(stubDir);
    }
    if (cache->plugin(QStringLiteral("I/O Plugin Stub")) == nullptr)
        return false;
    return doc->inputOutputMap()->setOutputPatch(0, QStringLiteral("I/O Plugin Stub"), QString(), 0);
}

const QList<QPair<QString, QSize>> kSizes {
    { "1024x768", QSize(1024, 768) }, { "1280x800", QSize(1280, 800) }, { "1920x1080", QSize(1920, 1080) } };

struct Calls
{
    int performance = 0;
    int quickFocus = 0;
    bool autosaveOn = true;
    QString autosaveFailure;
    QList<InputDeviceNotices::Device> pads;
    SystemTuning::Probe probe = goodMac();
};
Calls s_calls;

void stubHooks()
{
    s_calls = Calls();
    PreshowCheck::Hooks h;
    h.probe = []() { return s_calls.probe; };
    h.pads = []() { return s_calls.pads; };
    h.autosaveOn = []() { return s_calls.autosaveOn; };
    h.autosaveMinutes = []() { return 5; };
    h.autosaveFailure = []() { return s_calls.autosaveFailure; };
    h.turnAutosaveOn = []() { s_calls.autosaveOn = true; };
    h.openPerformance = [](QWidget *) { s_calls.performance++; };
    h.quickFocus = [](QWidget *) { s_calls.quickFocus++; };
    PreshowCheck::setHooks(h);
}
}

void PreshowCheck_Test::init()
{
    m_doc = new Doc(this);
    // A blocker: two lamps on the same channels.
    dimmer(m_doc, "Lamp 1", 0);
    Fixture *b = dimmer(m_doc, "Lamp 2", 5);
    b->setAddress(0);
    stubHooks();
}

void PreshowCheck_Test::cleanup()
{
    for (const ShowStatus::Entry &e : ShowStatus::instance()->allEntries())
        ShowStatus::instance()->clearStatus(e.key);
    PreshowCheck::setHooks(PreshowCheck::Hooks());
    // The test's fixes captured its locals: none outlives it.
    for (const char *key : { "connections.universe", "outputs.problems" })
        ShowStatus::instance()->setAction(key, "Open Connections", [](QWidget *, const QVariantList &) {});
    delete m_doc;
    m_doc = nullptr;
}

void PreshowCheck_Test::outputsReachTheirNodes()
{
    QVariantList opened;
    ShowStatus::instance()->setAction("connections.universe", "Show in Connections",
        [&opened](QWidget *, const QVariantList &items) { opened = items; });
    ShowStatus::instance()->setAction("outputs.problems", "Open Connections",
        [&opened](QWidget *, const QVariantList &items) { opened = items; opened << QStringLiteral("problems"); });

    // Fixtures on universe 1, no output anywhere: red.
    {
        PreshowCheck page(m_doc);
        QCOMPARE(page.line("outputs").state, PreshowCheck::NotReady);
        QVERIFY2(page.line("outputs").text.contains("No universe has an output"),
                 qPrintable(page.line("outputs").text));
        QVERIFY(page.fixButton("outputs") != nullptr);
        page.fixButton("outputs")->click();
        QCOMPARE(opened, QVariantList() << 0u);
    }

    // An output: green.
    QVERIFY2(patchStubOutput(m_doc), "the I/O plugin stub did not load");
    {
        PreshowCheck page(m_doc);
        QCOMPARE(page.line("outputs").state, PreshowCheck::Ready);
        QVERIFY(page.fixButton("outputs") == nullptr);
        QVERIFY(page.stateLabel("outputs")->text().contains("Ready"));

        // A node that stopped answering (the footer's entry): red, its fix.
        ShowStatus::instance()->setStatus("outputs.problems", ShowStatus::Warning, "1 network output problem",
                                          QString(), QStringList() << "Art-Net to 10.0.0.9: not answering (universe 1)",
                                          QVariantList() << 0u);
        QTRY_COMPARE(page.line("outputs").state, PreshowCheck::NotReady);
        QVERIFY(page.line("outputs").items.first().contains("not answering"));
        QCOMPARE(page.fixButton("outputs")->text(), QString("Open Connections"));
        opened.clear();
        page.fixButton("outputs")->click();
        QCOMPARE(opened, QVariantList() << 0u << QStringLiteral("problems"));
        // Gone again: green again.
        ShowStatus::instance()->clearStatus("outputs.problems");
        QTRY_COMPARE(page.line("outputs").state, PreshowCheck::Ready);
    }
}

void PreshowCheck_Test::showReviewBlockers()
{
    QWidget host;
    ShowReviewPanel::setShared(m_doc, &host);
    {
        PreshowCheck page(m_doc);
        QVERIFY(m_doc->showReview()->lastResult().ran);            // opened: checked
        QCOMPARE(page.line("review").state, PreshowCheck::NotReady);
        QVERIFY(page.line("review").items.first().startsWith("PAT-01"));
        QCOMPARE(page.fixButton("review")->text(), QString("Open Show Review"));
        page.fixButton("review")->click();
        QVERIFY(ShowReviewPanel::shared() != nullptr);
        QVERIFY(ShowReviewPanel::shared()->isVisible());
        QCOMPARE(ShowReviewPanel::shared()->selectedFindings().count(), 1);
        QCOMPARE(ShowReviewPanel::shared()->selectedFindings().first().check, QString("PAT-01"));
        ShowReviewPanel::shared()->close();
    }
    ShowReviewPanel::setShared(nullptr, nullptr);

    // The overlap moved: no blocker or warning left.
    m_doc->fixtures().last()->setAddress(10);
    PreshowCheck page(m_doc);
    QVERIFY2(page.line("review").state == PreshowCheck::Ready, qPrintable(page.line("review").text));
    QVERIFY(page.fixButton("review") == nullptr);
}

void PreshowCheck_Test::localNetwork()
{
    // Refused: a Mac whose every Art-Net output fails.
    s_calls.probe.networkOutputs = 2;
    s_calls.probe.failingOutputs = 2;
    PreshowCheck page(m_doc);
    QCOMPARE(page.line("localnetwork").state, PreshowCheck::NotReady);
    QVERIFY(page.line("localnetwork").text.contains("Local Network"));
    QVERIFY(page.stateLabel("localnetwork")->text().contains("✕"));
    page.fixButton("localnetwork")->click();
    QCOMPARE(s_calls.performance, 1);

    // Allowed: one sending.
    s_calls.probe.failingOutputs = 1;
    page.refresh();
    QCOMPARE(page.line("localnetwork").state, PreshowCheck::Ready);
    QVERIFY(page.fixButton("localnetwork") == nullptr);

    // No network output, or not a Mac: nothing to allow.
    s_calls.probe.networkOutputs = 0;
    page.refresh();
    QCOMPARE(page.line("localnetwork").state, PreshowCheck::NotUsed);
    s_calls.probe.os = SystemTuning::OsLinux;
    s_calls.probe.networkOutputs = 2;
    page.refresh();
    QCOMPARE(page.line("localnetwork").state, PreshowCheck::NotUsed);
}

void PreshowCheck_Test::padsPresent()
{
    QVariantList opened;
    ShowStatus::instance()->setAction("connections.universe", "Show in Connections",
        [&opened](QWidget *, const QVariantList &items) { opened = items; });
    PreshowCheck page(m_doc);
    QCOMPARE(page.line("pads").state, PreshowCheck::NotUsed);

    InputDeviceNotices::Device pad;
    pad.universe = 2;
    pad.plugin = "HID";
    pad.name = "DualSense Wireless Controller";
    pad.connected = false;
    s_calls.pads << pad;
    page.refresh();
    QCOMPARE(page.line("pads").state, PreshowCheck::NotReady);
    QVERIFY(page.line("pads").items.first().contains("DualSense"));
    QVERIFY(page.line("pads").items.first().contains("universe 3"));
    page.fixButton("pads")->click();
    QCOMPARE(opened, QVariantList() << 2u);

    s_calls.pads.first().connected = true;
    page.refresh();
    QCOMPARE(page.line("pads").state, PreshowCheck::Ready);
    QVERIFY(page.fixButton("pads") == nullptr);
}

void PreshowCheck_Test::autosave()
{
    s_calls.autosaveOn = false;
    PreshowCheck page(m_doc);
    QCOMPARE(page.line("autosave").state, PreshowCheck::Check);
    QCOMPARE(page.fixButton("autosave")->text(), QString("Turn It On"));
    page.fixButton("autosave")->click();
    QVERIFY(s_calls.autosaveOn);                    // the app's own setting
    QTRY_COMPARE(page.line("autosave").state, PreshowCheck::Ready);
    QVERIFY(page.line("autosave").text.contains("every 5 minute"));

    s_calls.autosaveFailure = "couldn't write /Volumes/Show/show.qxw.autosave";
    page.refresh();
    QCOMPARE(page.line("autosave").state, PreshowCheck::NotReady);
    QVERIFY(page.line("autosave").text.contains("couldn't write"));
}

void PreshowCheck_Test::thisComputer()
{
    s_calls.probe.onBattery = 1;
    s_calls.probe.batteryPercent = 60;
    PreshowCheck page(m_doc);
    QCOMPARE(page.line("power").state, PreshowCheck::Check);
    QVERIFY(page.line("power").text.contains("60%"));
    s_calls.probe.batteryPercent = 15;
    page.refresh();
    QCOMPARE(page.line("power").state, PreshowCheck::NotReady);
    s_calls.probe.onBattery = 0;
    page.refresh();
    QCOMPARE(page.line("power").state, PreshowCheck::Ready);

    // The screen held awake during Operate: green. Off, and it locks: check.
    QCOMPARE(page.line("screen").state, PreshowCheck::Ready);
    s_calls.probe.keepDisplayAwake = false;
    s_calls.probe.screenLock = 1;
    s_calls.probe.idleMinutes = 5;
    page.refresh();
    QCOMPARE(page.line("screen").state, PreshowCheck::Check);
    page.fixButton("screen")->click();
    QCOMPARE(s_calls.performance, 1);

    // Wi-Fi up, DMX wired: fine. DMX over it: check.
    s_calls.probe.wirelessUp = QStringList() << "en0";
    s_calls.probe.wirelessAddresses = QStringList() << "192.168.1.20";
    page.refresh();
    QCOMPARE(page.line("wifi").state, PreshowCheck::Ready);
    s_calls.probe.dmxAddresses = QStringList() << "192.168.1.20";
    page.refresh();
    QCOMPARE(page.line("wifi").state, PreshowCheck::Check);
}

void PreshowCheck_Test::readinessFooter()
{
    PreshowCheck page(m_doc);
    QCOMPARE(page.line("readiness").state, PreshowCheck::Ready);
    ShowStatus::instance()->setStatus("simpledesk.held", ShowStatus::Error, "Simple Desk holds 3 channels");
    QTRY_COMPARE(page.line("readiness").state, PreshowCheck::NotReady);
    QCOMPARE(page.line("readiness").items, QStringList() << "Simple Desk holds 3 channels");
    page.fixButton("readiness")->click();
    QTRY_VERIFY(page.findChild<ShowStatusDialog *>() != nullptr);
    // Covered by its own line, not this one.
    ShowStatus::instance()->clearStatus("simpledesk.held");
    ShowStatus::instance()->setStatus("input.devices", ShowStatus::Warning, "A controller is away");
    QTRY_COMPARE(page.line("readiness").state, PreshowCheck::Ready);
}

void PreshowCheck_Test::neverColourAlone()
{
    s_calls.autosaveOn = false;
    PreshowCheck page(m_doc);
    QVERIFY(page.lines().count() >= 10);
    for (const PreshowCheck::Line &l : page.lines())
    {
        QLabel *state = page.stateLabel(l.key);
        QVERIFY(state != nullptr);
        QVERIFY2(state->text().contains(PreshowCheck::stateSymbol(l.state))
                 && state->text().contains(PreshowCheck::stateWord(l.state)), qPrintable(l.key));
        QVERIFY2(!l.text.isEmpty(), qPrintable(l.key + " says nothing"));
    }
    QVERIFY2(page.summary()->text().startsWith("✕ Not ready"), qPrintable(page.summary()->text()));
    QCOMPARE(page.line("walk").state, PreshowCheck::ByHand);
    page.fixButton("walk")->click();
    QCOMPARE(s_calls.quickFocus, 1);
    QVERIFY(page.checklistButton() != nullptr);
    QCOMPARE(page.property("helpTopic").toString(), QString("dress-rehearsal.md"));
}

void PreshowCheck_Test::reviewPanelOpensIt()
{
    ShowReviewPanel panel(m_doc);
    QPushButton *open = panel.findChild<QPushButton *>("showReviewPreshow");
    QVERIFY(open != nullptr);
    open->click();
    PreshowCheck *page = panel.findChild<PreshowCheck *>();
    QVERIFY(page != nullptr);
    QVERIFY(page->isVisible());
    page->close();
}

namespace
{
/* A front light thrown onto a cyc in two cues (PWR-12, whose Fix goes to
   the cue's layer). Returns the cue ids. */
QList<quint32> backwashShow(Doc *doc)
{
    MonitorProperties *mp = doc->monitorProperties();
    mp->setGridSize(QVector3D(10, 3, 14));
    Scenery *cyc = mp->addScenery();
    cyc->setName("Cyc");
    cyc->setOriginX(5.0f);
    cyc->setOriginY(2.0f);
    Fixture *front = dimmer(doc, "Front 1", 20);
    FixturePreviewItem item;
    item.m_baseItem.m_position = QVector3D(5000.0f, 12000.0f, 6.0f);
    mp->setFixtureProperties(front->id(), item);
    FixtureRigProps rp = mp->fixtureRigProps(front->id());
    rp.fixedAim = true;
    rp.panZeroDir = 180;
    rp.aimDown = 20;
    mp->setFixtureRigProps(front->id(), rp);
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, doc);
    full->setValue(255);
    full->setName("Full");
    doc->addPalette(full);
    ShowTreeController *st = doc->showTree();
    const quint32 lid = st->addCueList(st->addProduction("Show"), "Act 1");
    QList<quint32> cues { st->addCue(lid, -1, "One") };
    cues << st->addCue(lid, 0, "Two");
    for (quint32 c : cues)
    {
        ShowTree::Target t;
        t.fixtures = { front->id() };
        const quint32 layer = st->addLayer(c, t);
        ShowTree::Look d;
        d.key = ShowTree::KeyDimmer;
        d.palettes = { full->id() };
        st->setLayerLook(c, layer, d);
    }
    return cues;
}
}

void PreshowCheck_Test::fixOnAGoneCueSaysSo()
{
    const QList<quint32> cues = backwashShow(m_doc);
    // The place is still there: not gone (a layer of the cue, here).
    QVERIFY(ShowReviewPanel::gonePlace(m_doc, "cue", QVariantList() << cues.first()).isEmpty());
    const quint32 layer = m_doc->showTree()->cue(cues.first())->layers.first().id;
    QVERIFY(ShowReviewPanel::gonePlace(m_doc, "cue", QVariantList() << layer).isEmpty());

    bool jumped = false;
    ShowReviewPanel::setJumper([&jumped](QWidget *, const QString &, const QVariantList &) {
        jumped = true;
        return true;
    });
    ShowReviewPanel panel(m_doc);
    panel.recheck();
    QVERIFY(panel.selectCheck("PWR-12"));
    // Both cues deleted after the check: Fix ▸ says the cue is gone and
    // goes nowhere.
    for (quint32 c : cues)
        QVERIFY(m_doc->showTree()->removeCue(c));
    QVERIFY(panel.selectCheck("PWR-12"));               // still listed: not re-checked
    panel.fixButton()->click();
    QVERIFY(!jumped);
    QVERIFY2(panel.summary()->text().contains("That cue is gone"), qPrintable(panel.summary()->text()));
    QVERIFY(panel.summary()->text().contains("Re-check"));
    QVERIFY(!ShowReviewPanel::gonePlace(m_doc, "cue", QVariantList() << cues.first()).isEmpty());
    QVERIFY(!ShowReviewPanel::gonePlace(m_doc, "function", QVariantList() << 4242u).isEmpty());
    ShowReviewPanel::setJumper(ShowReviewPanel::Jumper());
}

/* QLC_SHOT_DIR (the gate's --shots): the Review panel and the readiness
   dialog, where a crew looks before a show, and the page, at three sizes. */
void PreshowCheck_Test::screenshots()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    const QString tag = qEnvironmentVariable("QLC_SHOT_TAG", "after");
    shotUseDarkTheme();
    ShowStatus::instance()->setAction("outputs.problems", "Open Connections", [](QWidget *, const QVariantList &) {});
    ShowStatus::instance()->setAction("connections.universe", "Show in Connections", [](QWidget *, const QVariantList &) {});
    ShowStatus::instance()->setStatus("outputs.problems", ShowStatus::Warning, "1 network output problem",
                                      "Art-Net nodes that stopped answering.",
                                      QStringList() << "Art-Net to 172.18.2.50: not answering (universe 1)",
                                      QVariantList() << 0u);
    // A show on the night: a pad away, on battery, autosave off.
    InputDeviceNotices::Device pad;
    pad.universe = 2;
    pad.name = "DualSense Wireless Controller";
    s_calls.pads << pad;
    pad.universe = 3;
    pad.name = "Xbox Wireless Controller";
    pad.connected = true;
    s_calls.pads << pad;
    s_calls.probe.onBattery = 1;
    s_calls.probe.batteryPercent = 62;
    s_calls.probe.networkOutputs = 2;
    s_calls.autosaveOn = false;
    m_doc->showReview()->run();
    ShowReviewPanel panel(m_doc);
    panel.setFilter(ShowReviewPanel::All);
    ShowStatusDialog status;
    PreshowCheck page(m_doc);
    for (const auto &sz : kSizes)
    {
        for (QWidget *w : { static_cast<QWidget *>(&panel), static_cast<QWidget *>(&status),
                            static_cast<QWidget *>(&page) })
        {
            w->resize(sz.second);
            w->show();
        }
        QTest::qWait(40);
        QVERIFY(panel.grab().save(QStringLiteral("%1/%2-review-%3.png").arg(dir, tag, sz.first)));
        QVERIFY(status.grab().save(QStringLiteral("%1/%2-readiness-%3.png").arg(dir, tag, sz.first)));
        QVERIFY(page.grab().save(QStringLiteral("%1/%2-preshow-%3.png").arg(dir, tag, sz.first)));
    }
    // Its own size, opened: what a crew sees first.
    page.resize(page.sizeHint().expandedTo(QSize(720, 620)));
    QTest::qWait(40);
    QVERIFY(page.grab().save(QStringLiteral("%1/%2-preshow-opened.png").arg(dir, tag)));

    // Everything well: the green page.
    ShowStatus::instance()->clearStatus("outputs.problems");
    m_doc->fixtures().at(1)->setAddress(10);
    QVERIFY(patchStubOutput(m_doc));
    s_calls.pads.removeFirst();
    s_calls.probe.onBattery = 0;
    s_calls.autosaveOn = true;
    page.refresh();
    QTest::qWait(200);
    QVERIFY(page.grab().save(QStringLiteral("%1/%2-preshow-ready.png").arg(dir, tag)));

    // The gone cue, said.
    const QList<quint32> cues = backwashShow(m_doc);
    panel.recheck();
    QVERIFY(panel.selectCheck("PWR-12"));
    for (quint32 c : cues)
        m_doc->showTree()->removeCue(c);
    panel.fixOrSay();
    panel.resize(1024, 768);
    QTest::qWait(40);
    QVERIFY(panel.grab().save(QStringLiteral("%1/%2-review-gonecue.png").arg(dir, tag)));
}

QTEST_MAIN(PreshowCheck_Test)
