/*
  Q Light Controller Plus
  goguardui_test.cpp

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
#include <QDoubleSpinBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QToolButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QSettings>
#include <QLabel>
#include <QtTest>

#include <functional>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "appsettings.h"
#undef protected
#undef private
#include "programmercontroller.h"
#include "callingscriptview.h"
#include "showtreecontroller.h"
#include "showcontrol.h"
#include "showoperate.h"
#include "mastertimer.h"
#include "webaccessrun.h"
#include "cuerunner.h"
#include "padrun.h"
#include "chaser.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "doc.h"

#include "goguardui_test.h"

namespace
{
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
}

void GoGuardUi_Test::initTestCase()
{
    // No real plugin (and so no real controller of this machine).
    m_pluginDir = new QTemporaryDir();
    QVERIFY(m_pluginDir->isValid());
    qputenv("QLC_IOPLUGIN_DIR", m_pluginDir->path().toLocal8Bit());
    m_app = new App();
    m_app->init();
    m_app->showNormal();
    m_app->resize(1280, 800);
    makeShow();
    CueRunner *runTab = m_app->findChild<CueRunner *>(QStringLiteral("runTab"));
    QVERIFY(runTab != nullptr);
    m_app->m_tab->setCurrentWidget(runTab);
    m_app->doc()->setMode(Doc::Operate);
    settle(50);
}

void GoGuardUi_Test::cleanupTestCase()
{
    settle();
    if (m_app != nullptr)
    {
        m_app->doc()->programmer()->padRun()->setEnabled(false, false);
        m_app->doc()->showControl()->setGuardClock(ShowControl::GuardClock());   // NEW-API
        m_app->doc()->masterTimer()->stopAllFunctions();
        m_app->doc()->setMode(Doc::Design);
        settle(50);
        m_app->doc()->resetModified();
    }
    delete m_app;
    m_app = nullptr;
    delete m_pluginDir;
    m_pluginDir = nullptr;
}

void GoGuardUi_Test::makeShow()
{
    Doc *doc = m_app->doc();
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Guard Dimmer");
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
    Fixture *fxi = new Fixture(doc);
    fxi->setName("Dimmer 1");
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(0);
    doc->addFixture(fxi);
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer);
    full->setName("Full");
    full->setValue(255);
    doc->addPalette(full);
    ShowTreeController *st = doc->showTree();
    m_production = st->addProduction("Macbeth");
    m_list = st->addCueList(m_production, "Act One", ShowTree::ListMode::NonTracking);
    QList<quint32> cues;
    for (const char *name : { "Thunder", "Witches", "Heath", "Castle" })
        cues << st->addCue(m_list, int(cues.size()) - 1, QString::fromUtf8(name));
    ShowTree::Target t;
    t.fixtures = { fxi->id() };
    for (quint32 cue : cues)
        QVERIFY(st->setLayerLook(cue, st->addLayer(cue, t, "Front"), st->lookForPalette(full->id())));
    QVERIFY(st->setCurrentProduction(m_production) || st->currentProduction() == m_production);
    doc->showControl()->setMainCueList(st->cueList(m_list)->chaser);
    doc->operateSettings()->setRemoteGo(true);
    settle(50);
}

void GoGuardUi_Test::theConsoleTakesTheWindowFromSettings()
{
    // A home of its own (main()): nothing set, so the default.
    QCOMPARE(App::goGuardMs(), ShowControl::defaultGoGuardMs());
    QCOMPARE(m_app->doc()->showControl()->goGuardMs(), 1000);
}

void GoGuardUi_Test::everySourceIsHeldAndEverySurfaceSaysIt()
{
    Doc *doc = m_app->doc();
    ShowControl *sc = doc->showControl();
    sc->setGuardClock([this]() { return m_now; });
    Chaser *list = qobject_cast<Chaser *>(doc->function(doc->showTree()->cueList(m_list)->chaser));
    QVERIFY(list != nullptr);
    PadRun *pad = doc->programmer()->padRun();
    pad->setEnabled(true, false);
    CueRunner *runTab = m_app->findChild<CueRunner *>(QStringLiteral("runTab"));
    CueRunner vcRunner(doc, CueRunner::Compact);
    CallingScriptView script(doc, m_production);
    WebAccessRun phone(doc);
    QSignalSpy said(doc->programmer(), &ProgrammerController::operatorAnnounce);

    struct Source
    {
        const char *name;
        QString who;
        std::function<void()> go;
    };
    const QList<Source> sources = {
        { "the GO key (Cmd+Return)", "Laptop", [this]() { m_app->m_cueGoAction->trigger(); } },
        { "the Run tab's GO", "Laptop", [runTab]() { runTab->go(); } },
        { "the calling script's GO", "Laptop", [&script]() { script.go(); } },
        { "the pad", "Pad", [pad]() { pad->trigger(PadRun::Go); } },
        { "the phone Run page", "Phone", [&phone]() { phone.command(QStringLiteral("go"), true); } },
        { "a VC Cue Runner", "Cue Runner", [&vcRunner]() { vcRunner.go(); } },
    };

    QToolButton *footer = m_app->m_statusStandby;
    QVERIFY(footer != nullptr);
    /* The four surfaces, as drawn: the Run tab's line, the footer's chip,
       the phone page's state, the pad's notice. */
    auto surfaces = [&](const QString &when, const QString &want) {
        settle();
        const QString phoneSays = QJsonDocument::fromJson(phone.stateJson()).object()
                                      .value(QStringLiteral("goHeld")).toString();
        QVERIFY2(runTab->goHeldText() == want,
                 qPrintable(QString("%1: Run tab '%2', want '%3'").arg(when, runTab->goHeldText(), want)));
        QVERIFY2(phoneSays == want, qPrintable(QString("%1: phone '%2', want '%3'").arg(when, phoneSays, want)));
        if (!want.isEmpty())
        {
            QVERIFY2(footer->isVisible() && !footer->property("statusSqueezed").toBool() && footer->text() == want,
                     qPrintable(QString("%1: footer '%2', want '%3'").arg(when, footer->text(), want)));
            // The pad says it aloud (and buzzes: engine/test/goguard); its chip doesn't repeat it.
            int times = 0;
            for (const QList<QVariant> &line : said)
                times += line.at(0).toString() == want ? 1 : 0;
            QVERIFY2(times == 1, qPrintable(QString("%1: the pad said it %2 time(s)").arg(when).arg(times)));
            QVERIFY2(!m_app->padRoleChip()->text().contains(QStringLiteral("held")),
                     qPrintable(QString("%1: the Pad chip repeats it: '%2'").arg(when, m_app->padRoleChip()->text())));
        }
        else
        {
            QVERIFY2(!footer->text().contains(QStringLiteral("held")),
                     qPrintable(QString("%1: footer still '%2'").arg(when, footer->text())));
        }
    };

    for (const Source &first : sources)
        for (const Source &second : sources)
        {
            const QString pair = QString("%1, then %2").arg(first.name, second.name);
            doc->masterTimer()->stopAllFunctions();
            settle();
            m_now += 10000;
            m_app->slotUpdateStandby();
            runTab->refresh();
            first.go();
            said.clear();
            QTRY_VERIFY2_WITH_TIMEOUT(list->isRunning() && list->currentStepIndex() == 0,
                                      qPrintable(pair + ": the first GO did not play"), 3000);
            m_now += 400;
            second.go();
            settle(60);
            QVERIFY2(list->currentStepIndex() == 0,
                     qPrintable(pair + ": the cue advanced to " + QString::number(list->currentStepIndex())));
            surfaces(pair, QStringLiteral("Second GO held: %1's GO took it 0.4 s ago").arg(first.who));
        }

    // The window over, the words go from every screen.
    m_now += ShowControl::heldNoticeMs();
    m_app->slotUpdateStandby();
    runTab->refresh();
    surfaces("after its seconds", QString());
    // And the next GO plays.
    sources.first().go();
    QTRY_VERIFY_WITH_TIMEOUT(list->currentStepIndex() == 1, 3000);
    doc->masterTimer()->stopAllFunctions();
    pad->setEnabled(false, false);
}

void GoGuardUi_Test::backIsHeldTheStopNever()
{
    Doc *doc = m_app->doc();
    ShowControl *sc = doc->showControl();
    sc->setGuardClock([this]() { return m_now; });
    Chaser *list = qobject_cast<Chaser *>(doc->function(doc->showTree()->cueList(m_list)->chaser));
    CueRunner *runTab = m_app->findChild<CueRunner *>(QStringLiteral("runTab"));
    WebAccessRun phone(doc);
    PadRun *pad = doc->programmer()->padRun();

    doc->masterTimer()->stopAllFunctions();
    m_now += 10000;
    runTab->go();
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning(), 3000);
    m_now += 2000;
    runTab->go();
    QTRY_VERIFY_WITH_TIMEOUT(list->currentStepIndex() == 1, 3000);
    m_now += 300;
    runTab->back();
    settle(60);
    QCOMPARE(list->currentStepIndex(), 1);
    QCOMPARE(runTab->goHeldText(), QStringLiteral("BACK held: Laptop's GO took it 0.3 s ago"));

    // The phone's emergency stop, inside the window: never held.
    m_now += 100;
    QCOMPARE(phone.command(QStringLiteral("alloff"), true), WebAccessRun::Done);
    QTRY_VERIFY_WITH_TIMEOUT(!list->isRunning(), 3000);
    // The pad's, right after a GO: never held either.
    m_now += 10000;
    runTab->go();
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning(), 3000);
    m_now += 100;
    pad->setEnabled(true, false);
    pad->emergencyStop();
    QTRY_VERIFY_WITH_TIMEOUT(!list->isRunning(), 3000);
    QCOMPARE(pad->state(), PadRun::Stopped);
    pad->setEnabled(false, false);
}

void GoGuardUi_Test::theSettingChangesTheWindow()
{
    ShowControl *sc = m_app->doc()->showControl();
    {
        AppSettings dlg(m_app);
        QDoubleSpinBox *spin = dlg.findChild<QDoubleSpinBox *>(QStringLiteral("settingsGoGuard"));
        QVERIFY(spin != nullptr);
        QCOMPARE(spin->value(), 1.0);
        spin->setValue(2.5);
        dlg.accept();
    }
    QCOMPARE(sc->goGuardMs(), 2500);
    QCOMPARE(QSettings().value(App::goGuardSettingKey()).toInt(), 2500);
    {
        AppSettings dlg(m_app);
        QDoubleSpinBox *spin = dlg.findChild<QDoubleSpinBox *>(QStringLiteral("settingsGoGuard"));
        QCOMPARE(spin->value(), 2.5);
        spin->setValue(0.0);
        QCOMPARE(spin->text(), QStringLiteral("Off"));
        dlg.accept();
    }
    QCOMPARE(sc->goGuardMs(), 0);
    // Off: two GOs a moment apart both play.
    Doc *doc = m_app->doc();
    sc->setGuardClock([this]() { return m_now; });
    Chaser *list = qobject_cast<Chaser *>(doc->function(doc->showTree()->cueList(m_list)->chaser));
    CueRunner *runTab = m_app->findChild<CueRunner *>(QStringLiteral("runTab"));
    doc->masterTimer()->stopAllFunctions();
    m_now += 10000;
    runTab->go();
    QTRY_VERIFY_WITH_TIMEOUT(list->isRunning(), 3000);
    m_now += 100;
    m_app->m_cueGoAction->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(list->currentStepIndex() == 1, 3000);
    doc->masterTimer()->stopAllFunctions();
    m_app->setGoGuardMs(ShowControl::defaultGoGuardMs());
    QCOMPARE(sc->goGuardMs(), 1000);
}

void GoGuardUi_Test::pictures()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    Doc *doc = m_app->doc();
    doc->showControl()->setGuardClock(ShowControl::GuardClock());   // NEW-API: the real clock
    Chaser *list = qobject_cast<Chaser *>(doc->function(doc->showTree()->cueList(m_list)->chaser));
    CueRunner *runTab = m_app->findChild<CueRunner *>(QStringLiteral("runTab"));
    PadRun *pad = doc->programmer()->padRun();
    pad->setEnabled(true, false);
    // What an operator sees once an earlier press's notice has gone.
    QTRY_VERIFY_WITH_TIMEOUT(pad->notice().isEmpty(), PadRun::noticeMs() + 1000);
    const QList<QPair<QSize, QString> > sizes = { { QSize(1024, 640), "1024x640" },
                                                  { QSize(1280, 800), "1280x800" },
                                                  { QSize(1920, 1080), "1920x1080" } };
    for (const auto &sz : sizes)
    {
        doc->masterTimer()->stopAllFunctions();
        m_app->resize(sz.first);
        settle(1200);                       // any window before is over
        // The student's GO from the laptop, the pad's 0.4 s later.
        runTab->go();
        QTRY_VERIFY_WITH_TIMEOUT(list->isRunning(), 3000);
        QTest::qWait(400);
        pad->trigger(PadRun::Go);
        settle(150);
        QVERIFY2(m_app->m_statusStandby->isVisible() && !m_app->m_statusStandby->property("statusSqueezed").toBool()
                     && m_app->m_statusStandby->width() > 0,
                 qPrintable("the footer's Next chip is off the footer at " + sz.second));
        QVERIFY2(runTab->styleSheet().isEmpty(), qPrintable("the Run tab is framed in Pad: Run: " + runTab->styleSheet()));
        const QString base = dir + QStringLiteral("/goguard-runtab-%1").arg(sz.second);
        QVERIFY(m_app->grab().save(base + QStringLiteral(".png")));
        QVERIFY(m_app->statusBar()->grab().save(base + QStringLiteral("-footer.png")));
    }
    pad->setEnabled(false, false);
    doc->masterTimer()->stopAllFunctions();
    m_app->resize(1280, 800);
    settle();

    AppSettings dlg(m_app);
    dlg.show();
    dlg.resize(dlg.sizeHint());
    settle(150);
    QVERIFY(dlg.grab().save(dir + QStringLiteral("/goguard-settings-general.png")));
}

int main(int argc, char **argv)
{
    // A home of its own: nothing touches yours (as padhit).
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    GoGuardUi_Test test;
    return QTest::qExec(&test, argc, argv);
}
