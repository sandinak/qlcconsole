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
#include <QtTest>

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
#include "fixture.h"
#include "doc.h"

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

    /* At 1024 wide the Grand Master stays in the footer: the chip leaves the
       hit out (the Run tab still says it), and says it again when wider. */
    {
        QWidget *gm = m_app->findChild<QWidget *>(QStringLiteral("footerGrandMaster"));
        QVERIFY(gm != nullptr);
        m_app->resize(1024, 640);
        settle(300);
        QTRY_VERIFY2(!gm->property("statusSqueezed").toBool() && gm->width() > 0,
                     qPrintable(QString("GM squeezed at 1024 (chip '%1')").arg(chip->text())));
        QVERIFY2(!chip->text().contains(QString::fromUtf8("Hit ▸")), qPrintable(chip->text()));
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
