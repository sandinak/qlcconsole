/*
  Q Light Controller Plus
  padhit_test.cpp

  Copyright (C) Branson Matheson

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

#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QStatusBar>
#include <QTabWidget>
#include <QLabel>
#include <QThread>
#include <QElapsedTimer>
#include <QtTest>
#include <functional>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#undef protected
#undef private
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "iopluginstub.h"
#include "qlcioplugin.h"
#include "showcontrol.h"
#include "showoperate.h"
#include "controlmap.h"
#include "mastertimer.h"
#include "webaccessrun.h"
#include "cuerunner.h"
#include "padrun.h"
#include "qlcfile.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "effectscriptrunner.h"
#include "universe.h"
#include "functionparent.h"
#include "fixture.h"
#include "doc.h"
#include "controllerjobs.h"
#include "focuscallcue.h"
#include "focuscalls.h"
#include "focussession.h"
#include "fixturegroup.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "qlcphysical.h"
#include "qlcpoint.h"

#include "padhit_test.h"

namespace
{
const quint32 kLT = 4, kRT = 5, kA = 10, kLB = 14, kRB = 15, kView = 16, kMenu = 17;

QMap<quint32, QString> xboxRoles()
{
    return { { 0, "lx" }, { 1, "ly" }, { 2, "rx" }, { 3, "ry" }, { kLT, "lt" }, { kRT, "rt" },
             { kA, "a" }, { 11, "b" }, { 12, "x" }, { 13, "y" }, { kLB, "lb" }, { kRB, "rb" },
             { kView, "back" }, { kMenu, "start" } };
}

void settle(int ms = 0)
{
    for (int i = 0; i < 3; ++i)
    {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }
    if (ms > 0)
        QTest::qWait(ms);
}

QString hit(const char *what)
{
    return QString::fromUtf8("Hit ▸ ") + QString::fromUtf8(what);
}
}

void PadHit_Test::initTestCase()
{
    // No real plugin (and so no real controller of this machine): the stub only.
    m_pluginDir = new QTemporaryDir();
    QVERIFY(m_pluginDir->isValid());
    qputenv("QLC_IOPLUGIN_DIR", m_pluginDir->path().toLocal8Bit());
    m_app = new App();
    m_app->init();
    // GOs here come back to back on purpose: the double-GO guard (its own
    // tests, engine/test/goguard and ui/test/goguardui) is off for them.
    m_app->doc()->showControl()->setGoGuardMs(0);
    m_app->showNormal();
    m_app->resize(1280, 800);
    QDir stubDir(QStringLiteral("../../../engine/test/iopluginstub"));
    stubDir.setFilter(QDir::Files);
    stubDir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    IOPluginCache *cache = m_app->doc()->ioPluginCache();
    if (cache->plugin(QStringLiteral("I/O Plugin Stub")) == nullptr)
        cache->load(stubDir);
    m_stub = static_cast<IOPluginStub *>(cache->plugin(QStringLiteral("I/O Plugin Stub")));
    QVERIFY2(m_stub != nullptr, "the I/O plugin stub did not load");
    m_stub->m_channelRoles = xboxRoles();
    QVERIFY(m_app->doc()->inputOutputMap()->setInputPatch(0, m_stub->name(), QString(), 0));
    m_app->doc()->programmer()->padRun()->setArmChordMs(15);
    makeShow();
    // The Run tab in front, as an operator has it.
    CueRunner *runTab = m_app->findChild<CueRunner *>(QStringLiteral("runTab"));
    QVERIFY(runTab != nullptr);
    m_app->m_tab->setCurrentWidget(runTab);
    settle(50);
}

void PadHit_Test::cleanupTestCase()
{
    settle();
    if (m_app != nullptr)
    {
        m_app->doc()->programmer()->padRun()->setEnabled(false, false);
        m_app->doc()->masterTimer()->stopAllFunctions();
        m_app->doc()->setMode(Doc::Design);
        settle(50);
        m_app->doc()->resetModified();
    }
    if (m_stub != nullptr)
        m_stub->m_feedbackParams.clear();
    delete m_app;
    m_app = nullptr;
    delete m_pluginDir;
    m_pluginDir = nullptr;
}

void PadHit_Test::makeShow()
{
    Doc *doc = m_app->doc();
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Hit Dimmer");
    def->setType(QLCFixtureDef::Dimmer);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("1ch");
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setGroup(QLCChannel::Intensity);
    ch->setControlByte(QLCChannel::MSB);
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    QLCFixtureHead head;
    head.addChannel(0);
    mode->insertHead(-1, head);
    def->addMode(mode);
    QList<quint32> dimmers;
    for (int i = 0; i < 2; ++i)
    {
        Fixture *fxi = new Fixture(doc);
        fxi->setName(QString("Dimmer %1").arg(i + 1));
        fxi->setFixtureDefinition(def, mode);
        fxi->setUniverse(0);
        fxi->setAddress(quint32(i));
        doc->addFixture(fxi);
        dimmers << fxi->id();
    }
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer);
    full->setName("Full");
    full->setValue(255);
    doc->addPalette(full);
    ShowTreeController *st = doc->showTree();
    const quint32 prod = st->addProduction("Macbeth");
    m_list = st->addCueList(prod, "Act One", ShowTree::ListMode::NonTracking);
    m_cues.clear();
    for (const char *name : { "Opening", "Ballad", "Finale" })
        m_cues << st->addCue(m_list, int(m_cues.size()) - 1, QString::fromUtf8(name));
    ShowTree::Target t;
    t.fixtures = { dimmers.first() };
    for (quint32 cue : m_cues)
        QVERIFY(st->setLayerLook(cue, st->addLayer(cue, t, "Front"), st->lookForPalette(full->id())));
    ShowTree::Target h;
    h.fixtures = { dimmers.last() };
    auto addHit = [&](quint32 cue, const char *name) {
        const quint32 id = st->addHit(ShowTree::HitScope::Cue, cue, QString::fromUtf8(name));
        QVERIFY(id != ShowTree::invalidId());
        QVERIFY(st->setLayerLook(id, st->addLayer(id, h, QString::fromUtf8(name)), st->lookForPalette(full->id())));
    };
    addHit(m_cues.at(0), "Staff Slam");
    addHit(m_cues.at(0), "Thunder");
    addHit(m_cues.at(1), "Breathe");
    // A second list, for the Run tab to pick.
    m_list2 = st->addCueList(prod, "Act Two", ShowTree::ListMode::NonTracking);
    const quint32 storm = st->addCue(m_list2, -1, "Storm");
    QVERIFY(st->setLayerLook(storm, st->addLayer(storm, t, "Front"), st->lookForPalette(full->id())));
    QVERIFY(st->setCurrentProduction(prod) || st->currentProduction() == prod);
    doc->showControl()->setMainCueList(st->cueList(m_list)->chaser);
    doc->operateSettings()->setRemoteGo(true);
    settle(50);
}

void PadHit_Test::value(quint32 channel, unsigned char v)
{
    emit m_app->doc()->inputOutputMap()->inputValueChanged(0, channel, v, QString());
}

void PadHit_Test::tap(quint32 channel)
{
    value(channel, 255);
    value(channel, 0);
}

void PadHit_Test::chord(bool on)
{
    PadRun *run = m_app->doc()->programmer()->padRun();
    value(kLB, 255);
    value(kRB, 255);
    QTRY_COMPARE(run->isArmed(), on);
    value(kRB, 0);
    value(kLB, 0);
}

void PadHit_Test::go()
{
    // Armed, the safety held and A: the one GO path.
    value(kLT, 255);
    tap(kA);
    value(kLT, 0);
}

void PadHit_Test::everySurfaceNamesTheNextHit()
{
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();
    PadRun *run = doc->programmer()->padRun();
    QLabel *line = m_app->findChild<QLabel *>(QStringLiteral("crPadHit"));
    QVERIFY(line != nullptr);
    QLabel *chip = m_app->padRoleChip();
    QVERIFY(chip != nullptr);
    WebAccessRun phone(doc);

    /* Every surface, read back as drawn. @p armed: what the Run tab and the
       chip show (the console pad armed); @p page: what the phone's state
       names (the page shows it under its own arm). */
    auto surfaces = [&](const char *when, const QString &armed, const QString &page) {
        settle();
        const QString pageSays = QJsonDocument::fromJson(phone.stateJson()).object()
                                     .value(QStringLiteral("hit")).toString();
        QVERIFY2(run->armedHitText() == armed,
                 qPrintable(QString("%1: PadRun '%2', want '%3'").arg(when, run->armedHitText(), armed)));
        QVERIFY2(line->isHidden() == armed.isEmpty() && line->text() == armed,
                 qPrintable(QString("%1: Run tab '%2' (hidden %3), want '%4'")
                                .arg(when, line->text()).arg(line->isHidden()).arg(armed)));
        if (armed.isEmpty())
            QVERIFY2(!chip->text().contains(QString::fromUtf8("Hit ▸")),
                     qPrintable(QString("%1: chip '%2' names a hit").arg(when, chip->text())));
        else if (run->notice().isEmpty())      // a notice takes the hit's place for its seconds
            QVERIFY2(chip->text() == run->roleText() + QStringLiteral(" · ") + armed,
                     qPrintable(QString("%1: chip '%2', want '%3'").arg(when, chip->text(), armed)));
        QVERIFY2(pageSays == page && pageSays == run->hitText(),
                 qPrintable(QString("%1: phone '%2', PadRun '%3', want '%4'").arg(when, pageSays, run->hitText(), page)));
    };
    // A pull of RT, and what it fires on: the hit named before it.
    auto pull = [&](const char *when, const QString &armedBefore) {
        surfaces(when, armedBefore, armedBefore);
        tap(kRT);
    };

    doc->setMode(Doc::Operate);
    settle(50);
    surfaces("Tools", QString(), QString());
    run->setEnabled(true);
    chord(true);
    surfaces("armed, nothing running: no cue hit is live", QString(), QString());

    // Opening: two hits take turns.
    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(0), 3000);
    QTRY_COMPARE(run->armedHitText(), hit("Staff Slam (1 of 2)"));
    pull("Opening, before the first pull", hit("Staff Slam (1 of 2)"));
    QTRY_COMPARE(run->armedHitText(), hit("Staff Slam off (1 of 2)"));
    pull("Staff Slam on: the pull lets it go", hit("Staff Slam off (1 of 2)"));
    QTRY_COMPARE(run->armedHitText(), hit("Thunder (2 of 2)"));
    pull("Thunder's turn", hit("Thunder (2 of 2)"));
    QTRY_COMPARE(run->armedHitText(), hit("Thunder off (2 of 2)"));
    surfaces("Thunder on", hit("Thunder off (2 of 2)"), hit("Thunder off (2 of 2)"));

    // The cue moves on: Ballad's one hit, no "1 of 1".
    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(1), 3000);
    QTRY_COMPARE(run->armedHitText(), hit("Breathe"));
    surfaces("Ballad", hit("Breathe"), hit("Breathe"));

    /* At 1024 wide the Grand Master stays in the footer. Where the footer
       has no room for both (it depends on the platform's font: macOS has
       none, Windows has), the chip leaves the hit out -- the Run tab still
       says it -- and says it again when wider. */
    {
        QWidget *gm = m_app->findChild<QWidget *>(QStringLiteral("footerGrandMaster"));
        QVERIFY(gm != nullptr);
        m_app->resize(1024, 640);
        settle(300);
        QTRY_VERIFY2(!gm->property("statusSqueezed").toBool() && gm->width() > 0,
                     qPrintable(QString("GM squeezed at 1024 (chip '%1')").arg(chip->text())));
        QCOMPARE(chip->text().contains(QString::fromUtf8("Hit ▸")), !m_app->m_padChipShort);
        if (!m_app->m_padChipShort)
            QVERIFY2(chip->text().contains(QString::fromUtf8("Hit ▸ Breathe")), qPrintable(chip->text()));
        QVERIFY(chip->text().startsWith(QStringLiteral("Pad: ARMED")));
        QCOMPARE(line->text(), hit("Breathe"));
        m_app->resize(1280, 800);
        settle(300);
        QTRY_VERIFY2(chip->text().contains(QString::fromUtf8("Hit ▸")), qPrintable(chip->text()));
        QVERIFY(!gm->property("statusSqueezed").toBool());
    }

    // Disarmed: the console says nothing; the page (its own arm) still knows.
    chord(false);
    surfaces("disarmed", QString(), hit("Breathe"));
    chord(true);
    surfaces("armed again", hit("Breathe"), hit("Breathe"));

    // Finale: no hit -- nothing, not "none".
    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(2), 3000);
    QTRY_VERIFY(run->armedHitText().isEmpty());
    surfaces("Finale, no hit", QString(), QString());

    // Back to Ballad (from the laptop's Back, a GO elsewhere), then the stop.
    ShowControl::Request back;
    back.command = ShowControl::Back;
    back.chaser = st->cueList(m_list)->chaser;
    QVERIFY(doc->showControl()->run(back, ShowControl::RouteKey));
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(1), 3000);
    QTRY_COMPARE(run->armedHitText(), hit("Breathe"));
    value(kLB, 255);
    value(kRB, 255);
    value(kLT, 255);
    value(kRT, 255);
    QCOMPARE(run->state(), PadRun::Stopped);
    // ALL OFF: nothing runs, no cue hit is live.
    QTRY_VERIFY(run->hitText().isEmpty());
    surfaces("the emergency stop", QString(), QString());
    value(kRT, 0);
    value(kLT, 0);
    value(kRB, 0);
    value(kLB, 0);

    // Design: no hit fires there, so none is named.
    doc->masterTimer()->stopAllFunctions();
    doc->setMode(Doc::Design);
    settle(50);
    surfaces("Design", QString(), QString());
    run->setEnabled(false, false);
}

void PadHit_Test::thePhonePlaysTheRunTabsList()
{
    Doc *doc = m_app->doc();
    PadRun *run = doc->programmer()->padRun();
    WebAccessRun phone(doc);
    auto list = [&]() {
        return QJsonDocument::fromJson(phone.stateJson()).object().value(QStringLiteral("list")).toString();
    };
    QCOMPARE(list(), QStringLiteral("Act One"));
    QCOMPARE(run->listName(run->listChaser()), QStringLiteral("Act One"));
    doc->controlMap()->setRunTabList(m_list2);
    settle();
    QCOMPARE(run->listName(run->listChaser()), QStringLiteral("Act Two"));
    QCOMPARE(list(), QStringLiteral("Act Two"));
    QCOMPARE(QJsonDocument::fromJson(phone.stateJson()).object().value(QStringLiteral("next")).toObject()
                 .value(QStringLiteral("name")).toString(), QStringLiteral("Storm"));
    doc->controlMap()->setRunTabList(ShowTree::invalidId());
    settle();
    QCOMPARE(list(), QStringLiteral("Act One"));
}

/* The product rule: a cue that fires while a hit is firing -- the hit
   completes. A GO moves the list on and touches nothing of a hit playing. */
namespace
{
struct Undo
{
    std::function<void()> f;
    ~Undo() { if (f) f(); }
};
}

int PadHit_Test::level(int channel)
{
    Doc *doc = m_app->doc();
    QList<Universe *> us = doc->inputOutputMap()->claimUniverses();
    const int v = int(uchar(us.at(0)->preGMValues().at(channel)));
    doc->inputOutputMap()->releaseUniverses(false);
    return v;
}

void PadHit_Test::aHitPlayingWhenAGoComesCompletes()
{
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();
    PadRun *run = doc->programmer()->padRun();
    doc->masterTimer()->stopAllFunctions();
    settle(100);
    // A temporary cue after Opening that also drives the hit's own channel,
    // lower than the hit: the hit must stay on top of it.
    QLCPalette *half = new QLCPalette(QLCPalette::Dimmer);
    half->setName("Half (hit GO test)");
    half->setValue(60);
    doc->addPalette(half);
    ShowTree::Target both;
    for (Fixture *fxi : doc->fixtures())
        both.fixtures << fxi->id();
    const quint32 overlap = st->addCue(m_list, 0, "Overlap");
    QVERIFY(overlap != ShowTree::invalidId());
    Undo undo;
    undo.f = [&]() {
        doc->masterTimer()->stopAllFunctions();
        st->removeCue(overlap);
        doc->deletePalette(half->id());
        doc->setMode(Doc::Design);
        run->setEnabled(false, false);
        settle(50);
    };
    QVERIFY(st->setLayerLook(overlap, st->addLayer(overlap, both, "Both"), st->lookForPalette(half->id())));
    settle(50);

    doc->setMode(Doc::Operate);
    settle(50);
    run->setEnabled(true);
    if (!run->isArmed())
        chord(true);
    QVERIFY(run->isArmed());
    QSignalSpy fired(run, &PadRun::hitFired);

    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(0), 3000);
    tap(kRT);
    QTRY_COMPARE_WITH_TIMEOUT(fired.count(), 1, 2000);
    const quint32 hitId = fired.last().at(0).toUInt();
    QVERIFY(fired.last().at(1).toBool());
    QCOMPARE(st->hitScope(hitId), ShowTree::HitScope::Cue);
    Function *hitFn = doc->function(st->lookFunction(hitId));
    QVERIFY(hitFn != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(hitFn->isRunning(), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(level(1), 255, 2000);

    // The GO: Overlap plays its 60 on the hit's channel, the hit keeps 255.
    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == overlap, 3000);
    QVERIFY2(!st->isHitArmed(hitId), "the hit's cue is no longer the running one");
    for (int i = 0; i < 8; ++i)
    {
        QTest::qWait(100);
        QVERIFY2(hitFn->isRunning(), qPrintable(QString("the GO stopped the hit (%1)").arg(i)));
        QVERIFY2(level(1) == 255, qPrintable(QString("the hit's channel is %1 after the GO").arg(level(1))));
        QCOMPARE(level(0), 60);         // the new cue is playing, on its own channel
    }

    // An RT pull on a cue with no hit of its own does not touch the old one.
    tap(kRT);
    settle(200);
    QVERIFY(hitFn->isRunning());
    QCOMPARE(level(1), 255);

    // On to Ballad: only Breathe responds to RT; the first hit plays on.
    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(1), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(run->armedHitText(), hit("Breathe"), 2000);
    tap(kRT);
    QTRY_COMPARE_WITH_TIMEOUT(fired.count(), 2, 2000);
    QVERIFY(fired.last().at(0).toUInt() != hitId);
    QVERIFY(fired.last().at(1).toBool());
    const quint32 breatheId = fired.last().at(0).toUInt();
    Function *breatheFn = doc->function(st->lookFunction(breatheId));
    QVERIFY(breatheFn != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(breatheFn->isRunning(), 2000);
    tap(kRT);                           // Breathe off
    QTRY_COMPARE_WITH_TIMEOUT(fired.count(), 3, 2000);
    QVERIFY(!fired.last().at(1).toBool());
    QVERIFY(fired.last().at(0).toUInt() != hitId);
    settle(200);
    QVERIFY2(hitFn->isRunning(), "a pull on the new cue let the previous cue's hit go");
    QCOMPARE(level(1), 255);

    // Let go at last: what the hit was holding is the cue's under it. Ballad
    // drives only the first dimmer, so the hit's channel is the dark.
    hitFn->stop(FunctionParent::master());
    QTRY_COMPARE_WITH_TIMEOUT(level(1), 0, 2000);
}

void PadHit_Test::aOneShotHitEndsOnItsOwnTimeNotAtTheGo()
{
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();
    PadRun *run = doc->programmer()->padRun();
    doc->masterTimer()->stopAllFunctions();
    settle(100);
    ShowTree::Target both;
    for (Fixture *fxi : doc->fixtures())
        both.fixtures << fxi->id();
    ShowTree::Target onHit;
    onHit.fixtures = { doc->fixtures().last()->id() };
    QLCPalette *half = new QLCPalette(QLCPalette::Dimmer);
    half->setName("Half (one-shot GO test)");
    half->setValue(60);
    doc->addPalette(half);
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer);
    full->setName("Full (one-shot GO test)");
    full->setValue(255);
    doc->addPalette(full);
    QLCPalette *breathe = new QLCPalette(QLCPalette::Effect);
    breathe->setName("Breathe (one-shot GO test)");
    breathe->setScriptPath(QStringLiteral(QLC_TEST_RESOURCE_ROOT) + "effectscripts/dimmer-phaser.js");
    doc->addPalette(breathe);
    breathe->setEffectParamValue("waveform", 0);
    breathe->setEffectParamValue("rate", 1.0);              // a 1 s breath
    breathe->setEffectParamValue("minDim", 0.05);
    breathe->setEffectParamValue("maxDim", 1.0);
    breathe->setEffectParamValue("spread", 0);
    breathe->setEffectParamValue("__runs", 1);              // once
    // Finale has no hit of its own: the production's one-shot is the one armed.
    const quint32 encore = st->addCue(m_list, int(m_cues.size()) - 1, "Encore");
    QVERIFY(encore != ShowTree::invalidId());
    const quint32 flash = st->addHit(ShowTree::HitScope::Production, st->currentProduction(), "Flash (one-shot)");
    QVERIFY(flash != ShowTree::invalidId());
    Undo undo;
    undo.f = [&]() {
        doc->masterTimer()->stopAllFunctions();
        st->removeCue(flash);
        st->removeCue(encore);
        doc->deletePalette(breathe->id());
        doc->deletePalette(full->id());
        doc->deletePalette(half->id());
        doc->setMode(Doc::Design);
        run->setEnabled(false, false);
        settle(50);
    };
    QVERIFY(st->setLayerLook(encore, st->addLayer(encore, both, "Both"), st->lookForPalette(half->id())));
    const quint32 layer = st->addLayer(flash, onHit, "Flash");
    QVERIFY(st->setLayerLook(flash, layer, st->lookForPalette(full->id())));
    ShowTree::Look fx = st->lookForPalette(breathe->id());
    fx.fadeIn = ShowTree::Look::FadeSnap;
    fx.fadeOut = ShowTree::Look::FadeSnap;
    QVERIFY(st->setLayerLook(flash, layer, fx));
    settle(50);

    doc->setMode(Doc::Operate);
    settle(50);
    run->setEnabled(true);
    if (!run->isArmed())
        chord(true);
    QVERIFY(run->isArmed());
    QSignalSpy fired(run, &PadRun::hitFired);

    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(0), 3000);
    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(1), 3000);
    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(2), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!run->armedHitText().isEmpty(), 2000);
    tap(kRT);
    QTRY_COMPARE_WITH_TIMEOUT(fired.count(), 1, 2000);
    QCOMPARE(fired.last().at(0).toUInt(), flash);
    Function *hitFn = doc->function(st->lookFunction(flash));
    QVERIFY(hitFn != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(hitFn->isRunning(), 2000);
    EffectScriptRunner *esr = doc->effectScriptRunner();
    QVERIFY(esr != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(esr->oneShotState(hitFn->id()), 0, 1000);   // mid-breath

    // The GO, mid-breath: Encore (60 on both) plays, the breath is not cut.
    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == encore, 3000);
    QVERIFY2(esr->oneShotState(hitFn->id()) == 0, "the GO ended the one-shot");
    QVERIFY(hitFn->isRunning());
    // Over the new cue's 60, the breath rises to its top.
    QTRY_VERIFY2_WITH_TIMEOUT(level(1) > 120, qPrintable(QString::number(level(1))), 1500);
    // It ends on its own time, and lets go by itself (Q239): the breath done,
    // the hit stops and the cue's own 60 comes back on the hit's channel.
    QTRY_VERIFY_WITH_TIMEOUT(!hitFn->isRunning(), 4000);
    QTRY_COMPARE_WITH_TIMEOUT(level(1), 60, 1000);
    QCOMPARE(level(0), 60);             // the cue plays on its own channel
    // Every surface says the next action is a fresh pull, never "off".
    QLabel *line = m_app->findChild<QLabel *>(QStringLiteral("crPadHit"));
    QVERIFY(line != nullptr);
    WebAccessRun phone(doc);
    QTRY_COMPARE_WITH_TIMEOUT(run->armedHitText(), hit("Flash (one-shot)"), 1000);
    settle();
    QCOMPARE(line->text(), hit("Flash (one-shot)"));
    QCOMPARE(QJsonDocument::fromJson(phone.stateJson()).object().value(QStringLiteral("hit")).toString(),
             hit("Flash (one-shot)"));
    /* The footer chip says it too -- unless the footer is too narrow for the
       hit beside the Grand Master (it depends on the platform's font, as in
       everySurfaceNamesTheNextHit), or a notice holds it. Never "off". */
    QVERIFY2(!m_app->padRoleChip()->text().contains(QStringLiteral("Flash (one-shot) off")),
             qPrintable(m_app->padRoleChip()->text()));
    QVERIFY2(m_app->padRoleChip()->text().contains(hit("Flash (one-shot)")) || m_app->m_padChipShort
                 || !run->notice().isEmpty(),
             qPrintable(m_app->padRoleChip()->text()));
    // The next pull fires it again, as a pull, not as a let-go.
    tap(kRT);
    QTRY_COMPARE_WITH_TIMEOUT(fired.count(), 2, 2000);
    QVERIFY(fired.last().at(1).toBool());
    QTRY_VERIFY_WITH_TIMEOUT(hitFn->isRunning(), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(run->armedHitText(), hit("Flash (one-shot) off"), 1000);
}

void PadHit_Test::aOneShotPlaysOutAcrossAnotherPadsFocus()
{
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();
    ProgrammerController *pc = doc->programmer();
    PadRun *run = pc->padRun();
    InputOutputMap *iom = doc->inputOutputMap();
    ControllerJobs *jobs = pc->controllerJobs();
    FocusCallCue *fcc = pc->focusCallCue();
    doc->masterTimer()->stopAllFunctions();
    settle(100);

    // The one-shot: a 1 s breath, once, on the production (as above).
    ShowTree::Target onHit;
    onHit.fixtures = { doc->fixtures().at(1)->id() };
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer);
    full->setName("Full (focus one-shot test)");
    full->setValue(255);
    doc->addPalette(full);
    QLCPalette *breathe = new QLCPalette(QLCPalette::Effect);
    breathe->setName("Breathe (focus one-shot test)");
    breathe->setScriptPath(QStringLiteral(QLC_TEST_RESOURCE_ROOT) + "effectscripts/dimmer-phaser.js");
    doc->addPalette(breathe);
    breathe->setEffectParamValue("waveform", 0);
    breathe->setEffectParamValue("rate", 1.0);
    breathe->setEffectParamValue("minDim", 0.05);
    breathe->setEffectParamValue("maxDim", 1.0);
    breathe->setEffectParamValue("spread", 0);
    breathe->setEffectParamValue("__runs", 1);
    const quint32 flash = st->addHit(ShowTree::HitScope::Production, st->currentProduction(), "Flare (one-shot)");
    QVERIFY(flash != ShowTree::invalidId());
    const quint32 layer = st->addLayer(flash, onHit, "Flare");
    QVERIFY(st->setLayerLook(flash, layer, st->lookForPalette(full->id())));
    ShowTree::Look fx = st->lookForPalette(breathe->id());
    fx.fadeIn = ShowTree::Look::FadeSnap;
    fx.fadeOut = ShowTree::Look::FadeSnap;
    QVERIFY(st->setLayerLook(flash, layer, fx));

    // A venue focus cue on a list of its own, on one moving spot.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Hit Focus Spot");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("3ch");
    QLCFixtureHead head;
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity };
    for (int k = 0; k < 3; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("Ch %1").arg(k));
        ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
        head.addChannel(k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    mode->insertHead(-1, head);
    def->addMode(mode);
    Fixture *spot = new Fixture(doc);
    spot->setName("Hit Focus Spot");
    spot->setFixtureDefinition(def, mode);
    spot->setUniverse(0);
    spot->setAddress(200);
    doc->addFixture(spot);
    MonitorProperties *props = doc->monitorProperties();
    props->setFixturePosition(spot->id(), 0, 0, QVector3D(2000.0f, 2000.0f, 5.0f));
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName("Hit Focus Spots");
    g->setSize(QSize(1, 1));
    g->assignFixture(spot->id(), QLCPoint(0, 0));
    doc->addFixtureGroup(g);
    StageTarget *dsc = props->addStageTarget();
    dsc->setName("DSC (focus one-shot test)");
    dsc->setPosition(QVector3D(5.0f, 6.0f, 0.0f));
    FocusCall call;
    call.groups << g->id();
    call.target.kind = FocusSession::Reference::Target;
    call.target.id = dsc->id();
    pc->focusCalls()->setCalls({ call });
    const quint32 preshow = st->addCueList(st->currentProduction(), "Pre-show (focus one-shot test)");
    const quint32 focusCue = st->addCue(preshow, -1, "Focus");
    QVERIFY(st->setCuesFocusCall({ focusCue }, true));
    quint32 onCue = focusCue;
    fcc->setRunningCueProbe([&onCue](quint32) { return onCue; });
    QMetaObject::Connection leave = connect(pc->focusSession(), &FocusSession::exitRequested,
                                            pc->focusSession(), &FocusSession::finish);

    // Two pads: A (line 0) runs cues only, B (line 1) does focus only.
    m_stub->m_lineNames = QStringList() << "Pad A (AA:BB:CC:00:00:01)" << "Pad B (AA:BB:CC:00:00:02)";
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    QVERIFY(iom->setInputPatch(1, m_stub->name(), QString(), 1));
    jobs->setJobs(jobs->identityOfUniverse(0), ControllerJobs::RunCues);
    jobs->setJobs(jobs->identityOfUniverse(1), ControllerJobs::Focus);
    auto onB = [&](quint32 ch, uchar v) { emit iom->inputValueChanged(1, ch, v, QString()); };
    auto ltMenuOnB = [&]() {
        onB(kLT, 255);
        onB(kMenu, 255);
        onB(kMenu, 0);
        onB(kLT, 0);
    };
    auto undo = qScopeGuard([&]() {
        if (pc->focusSession()->isActive())
            pc->focusSession()->finish();
        QObject::disconnect(leave);
        fcc->setRunningCueProbe({});
        pc->focusCalls()->setCalls({});
        doc->masterTimer()->stopAllFunctions();
        run->setEnabled(false, false);
        jobs->resetAll();
        iom->setInputPatch(1, QString(), QString(), QLCIOPlugin::invalidLine());
        m_stub->m_lineNames.clear();
        st->removeCueList(preshow);
        st->removeCue(flash);
        doc->deleteFixtureGroup(g->id());
        doc->deleteFixture(spot->id());
        doc->deletePalette(breathe->id());
        doc->deletePalette(full->id());
        doc->setMode(Doc::Design);
        settle(50);
    });
    settle(50);

    doc->setMode(Doc::Operate);
    settle(50);
    run->setEnabled(true);
    if (!run->isArmed())
        chord(true);
    QVERIFY(run->isArmed());
    QCOMPARE(run->armedUniverse(), quint32(0));
    QSignalSpy fired(run, &PadRun::hitFired);
    // Finale has no hit of its own: the production's one-shot is the one armed.
    for (int i = 0; i < 3; ++i)
    {
        go();
        QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(i), 3000);
    }
    QTRY_VERIFY_WITH_TIMEOUT(run->armedHits() == QList<quint32>({ flash }), 2000);
    tap(kRT);
    QTRY_COMPARE_WITH_TIMEOUT(fired.count(), 1, 2000);
    Function *hitFn = doc->function(st->lookFunction(flash));
    QVERIFY(hitFn != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(hitFn->isRunning(), 2000);
    EffectScriptRunner *esr = doc->effectScriptRunner();
    QVERIFY(esr != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(esr->oneShotState(hitFn->id()), 0, 1000);   // mid-breath

    // B starts the venue focus mid-breath: the one-shot is not cut.
    fcc->cueCurrent(preshow, focusCue);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    ltMenuOnB();
    QTRY_COMPARE_WITH_TIMEOUT(fcc->state(), FocusCallCue::Running, 2000);
    QCOMPARE(run->state(), PadRun::Focus);
    QVERIFY(run->isArmed());
    QVERIFY2(hitFn->isRunning() && esr->oneShotState(hitFn->id()) == 0, "the focus start cut the one-shot");
    // It runs its time and lets go by itself, focus or not.
    QTRY_VERIFY_WITH_TIMEOUT(!hitFn->isRunning(), 4000);
    // A new pull on A meanwhile: refused, and the screen says why.
    tap(kRT);
    settle(50);
    QCOMPARE(fired.count(), 1);
    QVERIFY(!hitFn->isRunning());
    QCOMPARE(run->notice(), QString::fromUtf8("Venue focus is running on Pad B (…00:02): nothing plays until it ends."));

    // B ends the focus: A, still armed, plays it again with one pull.
    ltMenuOnB();
    QTRY_VERIFY_WITH_TIMEOUT(!pc->focusSession()->isActive(), 2000);
    QVERIFY(run->isArmed());
    tap(kRT);
    QTRY_COMPARE_WITH_TIMEOUT(fired.count(), 2, 2000);
    QVERIFY(fired.last().at(1).toBool());
}

void PadHit_Test::eachNewSetOfHitsStartsAtOneOfN()
{
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();
    PadRun *run = doc->programmer()->padRun();
    doc->masterTimer()->stopAllFunctions();
    settle(100);
    doc->setMode(Doc::Operate);
    settle(50);
    run->setEnabled(true);
    if (!run->isArmed())
        chord(true);
    QVERIFY(run->isArmed());
    Undo undo;
    undo.f = [&]() {
        doc->masterTimer()->stopAllFunctions();
        doc->setMode(Doc::Design);
        run->setEnabled(false, false);
        settle(50);
    };
    // Opening: two hits. Pull one (Staff Slam on), let it go (turn 2: Thunder).
    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(0), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(run->armedHitText(), hit("Staff Slam (1 of 2)"), 2000);
    tap(kRT);
    QTRY_COMPARE_WITH_TIMEOUT(run->armedHitText(), hit("Staff Slam off (1 of 2)"), 2000);
    tap(kRT);
    QTRY_COMPARE_WITH_TIMEOUT(run->armedHitText(), hit("Thunder (2 of 2)"), 2000);
    // On to a cue with a different set: its one hit, then back to Opening: 1 of 2.
    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(1), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(run->armedHitText(), hit("Breathe"), 2000);
    ShowControl::Request back;
    back.command = ShowControl::Back;
    back.chaser = st->cueList(m_list)->chaser;
    QVERIFY(doc->showControl()->run(back, ShowControl::RouteKey));
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(0), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(run->armedHitText(), hit("Staff Slam (1 of 2)"), 2000);
    // And what it fires is hit 1, not the old turn's.
    QSignalSpy fired(run, &PadRun::hitFired);
    tap(kRT);
    QTRY_COMPARE_WITH_TIMEOUT(fired.count(), 1, 2000);
    QCOMPARE(st->cue(fired.last().at(0).toUInt())->name, QString("Staff Slam"));
}

void PadHit_Test::aPullAsTheCueChangesStartsAtOneOfN()
{
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();
    PadRun *run = doc->programmer()->padRun();
    doc->masterTimer()->stopAllFunctions();
    settle(100);
    doc->setMode(Doc::Operate);
    settle(50);
    run->setEnabled(true);
    if (!run->isArmed())
        chord(true);
    QVERIFY(run->isArmed());
    Undo undo;
    undo.f = [&]() {
        doc->masterTimer()->stopAllFunctions();
        doc->setMode(Doc::Design);
        run->setEnabled(false, false);
        settle(50);
    };
    /* Without the event loop: what a loaded machine does between the
       MasterTimer's thread moving the cue on and the main thread hearing of
       it (functionStarted and currentStepChanged come queued). */
    auto spinUntil = [](const std::function<bool()> &done, int ms) {
        QElapsedTimer t;
        t.start();
        while (!done() && t.elapsed() < ms)
            QThread::msleep(2);
        return done();
    };
    // Ballad's one hit pulled and let go: the turn has moved on from 1.
    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(0), 3000);
    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(1), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(run->armedHitText(), hit("Breathe"), 2000);
    settle(50);
    tap(kRT);
    QTRY_COMPARE_WITH_TIMEOUT(run->armedHitText(), hit("Breathe off"), 2000);
    tap(kRT);
    QTRY_COMPARE_WITH_TIMEOUT(run->armedHitText(), hit("Breathe"), 2000);
    settle(50);

    // Back to Opening, and the pull comes before the main thread has heard.
    QSignalSpy fired(run, &PadRun::hitFired);
    ShowControl::Request back;
    back.command = ShowControl::Back;
    back.chaser = st->cueList(m_list)->chaser;
    QVERIFY(doc->showControl()->run(back, ShowControl::RouteKey));
    QVERIFY(spinUntil([&]() { return st->runningCue(m_list) == m_cues.at(0); }, 3000));
    // A new set: 1 of 2, said and fired, though nothing has been told yet.
    QCOMPARE(run->armedHitText(), hit("Staff Slam (1 of 2)"));
    tap(kRT);
    QCOMPARE(fired.count(), 1);
    QCOMPARE(st->cue(fired.last().at(0).toUInt())->name, QString("Staff Slam"));
    // Once heard, the turn the pull moved on stands: let go, Thunder is next.
    QTRY_COMPARE_WITH_TIMEOUT(run->armedHitText(), hit("Staff Slam off (1 of 2)"), 2000);
    settle(300);                        // the one-shot tick and the queued news
    tap(kRT);
    QTRY_COMPARE_WITH_TIMEOUT(run->armedHitText(), hit("Thunder (2 of 2)"), 2000);
    settle(300);
    QCOMPARE(run->armedHitText(), hit("Thunder (2 of 2)"));
}

void PadHit_Test::pictures()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    Doc *doc = m_app->doc();
    ShowTreeController *st = doc->showTree();
    PadRun *run = doc->programmer()->padRun();
    doc->masterTimer()->stopAllFunctions();
    settle(100);
    doc->setMode(Doc::Operate);
    run->setEnabled(true);
    chord(true);
    auto shots = [&](const QString &state) {
        // What an operator sees once a press's notice has gone.
        QTRY_VERIFY_WITH_TIMEOUT(run->notice().isEmpty(), PadRun::noticeMs() + 1000);
        const QList<QPair<QSize, QString> > sizes = { { QSize(1024, 640), "1024x640" },
                                                      { QSize(1280, 800), "1280x800" },
                                                      { QSize(1920, 1080), "1920x1080" } };
        for (const auto &sz : sizes)
        {
            m_app->resize(sz.first);
            settle(150);
            const QString base = dir + QStringLiteral("/runtab-%1-%2").arg(state, sz.second);
            QVERIFY(m_app->grab().save(base + QStringLiteral(".png")));
            QVERIFY(m_app->statusBar()->grab().save(base + QStringLiteral("-footer.png")));
        }
        m_app->resize(1280, 800);
        settle();
    };
    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(0), 3000);
    QTest::qWait(400);      // the GO's own flash over
    shots("armed-two-hits");
    tap(kRT);
    QTest::qWait(300);
    shots("armed-hit-on");
    go();
    QTRY_VERIFY_WITH_TIMEOUT(st->runningCue(m_list) == m_cues.at(1), 3000);
    QTest::qWait(400);
    shots("armed-one-hit");
    chord(false);
    doc->masterTimer()->stopAllFunctions();
    doc->setMode(Doc::Design);
    run->setEnabled(false, false);
    settle(50);
}

int main(int argc, char **argv)
{
    // A home of its own: nothing touches yours (as padrunrules).
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    PadHit_Test test;
    return QTest::qExec(&test, argc, argv);
}
