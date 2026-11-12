/*
  Q Light Controller Plus - Test Unit
  systemtuning_test.cpp

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

#ifndef _WIN32
#include <sys/resource.h>
#endif
#include <cerrno>
#include <QtTest>
#include <QClipboard>
#include <QCheckBox>
#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QSettings>
#include <QElapsedTimer>
#include <QMutex>
#include <QThread>
#include <QThreadPool>
#include <QDeadlineTimer>

#include "systemtuning_test.h"
#include "systemtuningpage.h"
#include "systemtuning.h"
#include "systemprobe.h"
#include "showstatus.h"
#include "doc.h"

namespace
{
const SystemTuning::Item *find(const QList<SystemTuning::Item> &items, const QString &key)
{
    for (const SystemTuning::Item &it : items)
        if (it.key == key)
            return &it;
    return nullptr;
}

SystemTuning::Probe linuxProbe()
{
    SystemTuning::Probe p;
    p.os = SystemTuning::OsLinux;
    p.scheduling = MasterTimer::SchedulingRefused;
    p.schedulingText = QStringLiteral("SCHED_FIFO refused: Operation not permitted");
    p.rtprioLimit = 0;
    p.governors = QStringList() << QStringLiteral("ondemand");
    p.binary = QStringLiteral("/usr/bin/qlcconsole");
    return p;
}

bool hasStatus(const QString &key)
{
    for (const ShowStatus::Entry &e : ShowStatus::instance()->allEntries())
        if (e.key == key)
            return true;
    return false;
}

ShowStatus::Entry statusEntry(const QString &key)
{
    for (const ShowStatus::Entry &e : ShowStatus::instance()->allEntries())
        if (e.key == key)
            return e;
    return ShowStatus::Entry();
}
}

void SystemTuning_Test::initTestCase()
{
    m_doc = new Doc(this);
}

void SystemTuning_Test::cleanupTestCase()
{
    delete m_doc;
}

void SystemTuning_Test::init()
{
    QSettings().remove(QStringLiteral("tuning"));
    m_doc->setMode(Doc::Design);
    ShowStatus::instance()->clearStatus(SystemTuning::statusKey());
}

/****************************************************************************
 * The check
 ****************************************************************************/

void SystemTuning_Test::linuxTimerRefusedSaysHowToGrantIt()
{
    const QList<SystemTuning::Item> items = SystemTuning::check(linuxProbe());
    const SystemTuning::Item *timer = find(items, QStringLiteral("timer"));
    QVERIFY(timer != nullptr);
    QCOMPARE(timer->status, SystemTuning::CanImprove);
    QVERIFY(timer->important);
    QVERIFY(timer->state.contains(QLatin1String("refused")));
    QVERIFY(timer->state.contains(QLatin1String("real-time limit 0")));
    QVERIFY(timer->command.contains(QLatin1String("rtprio 95")));
    QVERIFY(timer->command.contains(QLatin1String("usermod -aG audio")));
    QVERIFY(timer->howTo.contains(QLatin1String("setcap cap_sys_nice=eip '/usr/bin/qlcconsole'")));
    /* Nothing to hold on Linux: no full-speed item */
    QVERIFY(find(items, QStringLiteral("fullspeed")) == nullptr);
}

void SystemTuning_Test::linuxTimerGrantedIsGood()
{
    SystemTuning::Probe p = linuxProbe();
    p.scheduling = MasterTimer::SchedulingRealtime;
    p.schedulingText = QStringLiteral("SCHED_FIFO priority 89");
    p.rtprioLimit = 95;
    const SystemTuning::Item *timer = nullptr;
    const QList<SystemTuning::Item> items = SystemTuning::check(p);
    timer = find(items, QStringLiteral("timer"));
    QCOMPARE(timer->status, SystemTuning::Good);
    QCOMPARE(timer->state, QStringLiteral("SCHED_FIFO priority 89; real-time limit 95"));
    QVERIFY(timer->command.isEmpty());

    p.rtprioLimit = -2;
    QVERIFY(find(SystemTuning::check(p), QStringLiteral("timer"))->state.endsWith(QLatin1String("unlimited")));

    p.scheduling = MasterTimer::SchedulingUnknown;
    p.schedulingText.clear();
    p.rtprioLimit = -1;
    const QList<SystemTuning::Item> before = SystemTuning::check(p);
    QCOMPARE(find(before, QStringLiteral("timer"))->status, SystemTuning::Advice);
    QCOMPARE(find(before, QStringLiteral("timer"))->state, QStringLiteral("not running yet"));
}

void SystemTuning_Test::linuxGovernorWifiKernelAndPi()
{
    SystemTuning::Probe p = linuxProbe();
    QList<SystemTuning::Item> items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("governor"))->status, SystemTuning::CanImprove);
    QVERIFY(find(items, QStringLiteral("governor"))->command.contains(QLatin1String("scaling_governor")));
    QCOMPARE(find(items, QStringLiteral("wifi"))->status, SystemTuning::Good);
    QCOMPARE(find(items, QStringLiteral("kernel"))->status, SystemTuning::Advice);
    QVERIFY(find(items, QStringLiteral("pi")) == nullptr);

    p.governors = QStringList() << QStringLiteral("performance");
    p.wirelessUp = QStringList() << QStringLiteral("wlan0") << QStringLiteral("wlan1");
    p.preemptRt = true;
    p.model = QStringLiteral("Raspberry Pi 5 Model B Rev 1.0");
    items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("governor"))->status, SystemTuning::Good);
    const SystemTuning::Item *wifi = find(items, QStringLiteral("wifi"));
    QCOMPARE(wifi->status, SystemTuning::CanImprove);
    QCOMPARE(wifi->command, QStringLiteral("sudo iw dev wlan0 set power_save off\n"
                                           "sudo iw dev wlan1 set power_save off"));
    QCOMPARE(find(items, QStringLiteral("kernel"))->status, SystemTuning::Good);
    QVERIFY(find(items, QStringLiteral("pi")) != nullptr);
    QCOMPARE(find(items, QStringLiteral("pi"))->command, QStringLiteral("vcgencmd get_throttled"));

    /* Mixed governors are not "performance"; none at all: a container */
    p.governors = QStringList() << QStringLiteral("performance") << QStringLiteral("powersave");
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("governor"))->status, SystemTuning::CanImprove);
    p.governors.clear();
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("governor"))->status, SystemTuning::NotAvailable);
}

void SystemTuning_Test::macLowPowerModeAndAppNap()
{
    SystemTuning::Probe p;
    p.os = SystemTuning::OsMac;
    p.scheduling = MasterTimer::SchedulingRealtime;
    p.schedulingText = QStringLiteral("time-constraint policy, period 20000 us");
    p.fullSpeedSupported = true;
    p.fullSpeedInOperate = true;
    p.lowPowerMode = 1;
    QList<SystemTuning::Item> items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("timer"))->status, SystemTuning::Good);
    QCOMPARE(find(items, QStringLiteral("fullspeed"))->status, SystemTuning::Good);
    const SystemTuning::Item *lpm = find(items, QStringLiteral("lowpower"));
    QCOMPARE(lpm->status, SystemTuning::CanImprove);
    QVERIFY(lpm->important);
    QVERIFY(lpm->howTo.contains(QLatin1String("Battery")));
    QVERIFY(find(items, QStringLiteral("governor")) == nullptr);

    p.lowPowerMode = 0;
    p.fullSpeedInOperate = false;
    items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("lowpower"))->status, SystemTuning::Good);
    QCOMPARE(find(items, QStringLiteral("fullspeed"))->status, SystemTuning::CanImprove);
    QVERIFY(find(items, QStringLiteral("fullspeed"))->important == false);

    p.lowPowerMode = -1;
    p.fullSpeedSupported = false;
    items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("lowpower"))->status, SystemTuning::NotAvailable);
    QCOMPARE(find(items, QStringLiteral("fullspeed"))->status, SystemTuning::NotAvailable);
}

void SystemTuning_Test::windowsPowerPlans()
{
    SystemTuning::Probe p;
    p.os = SystemTuning::OsWindows;
    p.scheduling = MasterTimer::SchedulingRefused;
    p.schedulingText = QStringLiteral("MMCSS refused, high priority, high-resolution timer");
    p.fullSpeedSupported = true;

    p.powerPlan = QStringLiteral("saver");
    QList<SystemTuning::Item> items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("timer"))->status, SystemTuning::CanImprove);
    QVERIFY(find(items, QStringLiteral("timer"))->howTo.contains(QLatin1String("Multimedia Class Scheduler")));
    const SystemTuning::Item *plan = find(items, QStringLiteral("powerplan"));
    QCOMPARE(plan->status, SystemTuning::CanImprove);
    QVERIFY(plan->important);
    QCOMPARE(plan->command, QStringLiteral("powercfg /setactive SCHEME_MIN"));

    p.powerPlan = QStringLiteral("balanced");
    plan = nullptr;
    items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("powerplan"))->status, SystemTuning::CanImprove);
    QVERIFY(find(items, QStringLiteral("powerplan"))->important == false);

    p.powerPlan = QStringLiteral("high");
    items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("powerplan"))->status, SystemTuning::Good);
    QVERIFY(find(items, QStringLiteral("powerplan"))->command.isEmpty());

    p.powerPlan = QStringLiteral("other");
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("powerplan"))->status, SystemTuning::Advice);
    p.powerPlan.clear();
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("powerplan"))->status, SystemTuning::NotAvailable);
}

void SystemTuning_Test::everySystemListsTheTimerFirst()
{
    for (SystemTuning::Os os : { SystemTuning::OsLinux, SystemTuning::OsMac,
                                 SystemTuning::OsWindows, SystemTuning::OsOther })
    {
        SystemTuning::Probe p;
        p.os = os;
        const QList<SystemTuning::Item> items = SystemTuning::check(p);
        QVERIFY(items.isEmpty() == false);
        QCOMPARE(items.first().key, QStringLiteral("timer"));
        for (const SystemTuning::Item &it : items)
        {
            QVERIFY2(it.title.isEmpty() == false, qPrintable(it.key));
            QVERIFY2(it.what.isEmpty() == false, qPrintable(it.key));
        }
    }
}

void SystemTuning_Test::importantProblemsAreOnlyTheImportantOnes()
{
    SystemTuning::Probe p = linuxProbe();
    p.wirelessUp = QStringList() << QStringLiteral("wlan0");
    QList<SystemTuning::Item> problems = SystemTuning::importantProblems(SystemTuning::check(p));
    /* governor and Wi-Fi can improve, but only the timer is important */
    QCOMPARE(problems.count(), 1);
    QCOMPARE(problems.first().key, QStringLiteral("timer"));

    p.scheduling = MasterTimer::SchedulingRealtime;
    QVERIFY(SystemTuning::importantProblems(SystemTuning::check(p)).isEmpty());
}

void SystemTuning_Test::theRealProbeKnowsThisSystem()
{
    const SystemTuning::Probe p = SystemTuning::probe(m_doc);
#if defined(Q_OS_LINUX)
    QCOMPARE(p.os, SystemTuning::OsLinux);
    QVERIFY(p.rtprioLimit != -1);
    QCOMPARE(p.fullSpeedSupported, false);
#elif defined(Q_OS_MACOS)
    QCOMPARE(p.os, SystemTuning::OsMac);
    QVERIFY(p.lowPowerMode == 0 || p.lowPowerMode == 1);
    QCOMPARE(p.fullSpeedSupported, true);
#elif defined(Q_OS_WIN)
    QCOMPARE(p.os, SystemTuning::OsWindows);
    QVERIFY(p.powerPlan.isEmpty() == false);
    QCOMPARE(p.fullSpeedSupported, true);
#endif
    QVERIFY(p.binary.isEmpty() == false);
    QVERIFY(SystemTuning::check(p).isEmpty() == false);
}

/****************************************************************************
 * Settings
 ****************************************************************************/

void SystemTuning_Test::settingsDefaultsAndAreRemembered()
{
    QCOMPARE(SystemTuning::fullSpeedInOperate(), true);
    QCOMPARE(SystemTuning::keepDisplayAwakeInOperate(), true);
    QCOMPARE(SystemTuning::warnInOperate(), true);

    SystemTuning::setFullSpeedInOperate(false);
    SystemTuning::setKeepDisplayAwakeInOperate(false);
    SystemTuning::setWarnInOperate(false);
    QCOMPARE(SystemTuning::fullSpeedInOperate(), false);
    QCOMPARE(SystemTuning::keepDisplayAwakeInOperate(), false);
    QCOMPARE(SystemTuning::warnInOperate(), false);
    QCOMPARE(QSettings().value(QStringLiteral("tuning/fullSpeedInOperate")).toBool(), false);
}

/****************************************************************************
 * Runtime
 ****************************************************************************/

void SystemTuning_Test::fullSpeedIsHeldOnlyDuringOperate()
{
    QList<QPair<bool, bool> > calls;
    SystemTuning tuning(m_doc);
    tuning.setHold([&calls](bool on, bool display) { calls << qMakePair(on, display); });
    tuning.setProbe([]() { SystemTuning::Probe p; return p; });
    QCOMPARE(SystemTuning::instance(), &tuning);
    QCOMPARE(tuning.isHeld(), false);

    m_doc->setMode(Doc::Operate);
    QCOMPARE(tuning.isHeld(), true);
    QCOMPARE(calls.count(), 1);
    QCOMPARE(calls.last(), qMakePair(true, true));

    m_doc->setMode(Doc::Design);
    QCOMPARE(tuning.isHeld(), false);
    QCOMPARE(calls.count(), 2);
    QCOMPARE(calls.last(), qMakePair(false, false));

    /* Turned off: Operate holds nothing, and nothing is called */
    SystemTuning::setFullSpeedInOperate(false);
    m_doc->setMode(Doc::Operate);
    QCOMPARE(tuning.isHeld(), false);
    QCOMPARE(calls.count(), 2);

    /* Turned on during Operate (Settings OK): held at once */
    SystemTuning::setFullSpeedInOperate(true);
    tuning.apply();
    QCOMPARE(tuning.isHeld(), true);
    QCOMPARE(calls.last(), qMakePair(true, true));
    m_doc->setMode(Doc::Design);
}

void SystemTuning_Test::keepDisplayAwakeGoesWithTheHold()
{
    QList<QPair<bool, bool> > calls;
    SystemTuning tuning(m_doc);
    tuning.setHold([&calls](bool on, bool display) { calls << qMakePair(on, display); });
    tuning.setProbe([]() { SystemTuning::Probe p; return p; });

    SystemTuning::setKeepDisplayAwakeInOperate(true);
    m_doc->setMode(Doc::Operate);
    QCOMPARE(calls.last(), qMakePair(true, true));
    /* Changed while held: the hold is replaced */
    SystemTuning::setKeepDisplayAwakeInOperate(false);
    tuning.apply();
    QCOMPARE(calls.last(), qMakePair(true, false));
    m_doc->setMode(Doc::Design);
    QCOMPARE(calls.last(), qMakePair(false, false));
}

void SystemTuning_Test::showStatusSaysAnImportantProblemOnlyInOperate()
{
    SystemTuning tuning(m_doc);
    tuning.setHold([](bool, bool) {});
    tuning.setProbe([]() { return linuxProbe(); });
    const QString key = SystemTuning::statusKey();

    tuning.apply();
    QVERIFY(hasStatus(key) == false);

    m_doc->setMode(Doc::Operate);
    QVERIFY(hasStatus(key));
    const ShowStatus::Entry e = statusEntry(key);
    QCOMPARE(e.severity, ShowStatus::Warning);
    QCOMPARE(e.items.count(), 1);
    QVERIFY(e.items.first().startsWith(QLatin1String("Real-time DMX timer: SCHED_FIFO refused")));

    /* Fixed (granted): gone on the next look */
    tuning.setProbe([]() {
        SystemTuning::Probe p = linuxProbe();
        p.scheduling = MasterTimer::SchedulingRealtime;
        return p;
    });
    tuning.apply();
    QVERIFY(hasStatus(key) == false);

    tuning.setProbe([]() { return linuxProbe(); });
    tuning.apply();
    QVERIFY(hasStatus(key));
    m_doc->setMode(Doc::Design);
    QVERIFY(hasStatus(key) == false);
}

void SystemTuning_Test::showStatusCanBeTurnedOff()
{
    SystemTuning tuning(m_doc);
    tuning.setHold([](bool, bool) {});
    tuning.setProbe([]() { return linuxProbe(); });
    SystemTuning::setWarnInOperate(false);
    m_doc->setMode(Doc::Operate);
    QVERIFY(hasStatus(SystemTuning::statusKey()) == false);
    SystemTuning::setWarnInOperate(true);
    tuning.apply();
    QVERIFY(hasStatus(SystemTuning::statusKey()));
    m_doc->setMode(Doc::Design);
}

/****************************************************************************
 * The page
 ****************************************************************************/

void SystemTuning_Test::thePageListsTheItemsAndCopiesTheCommand()
{
    SystemTuningPage page(m_doc);
    /* The page's first fill used this machine's real probe and selected
       whatever that found to improve; setProbe() keeps the selection by key.
       Start from none, or the test depends on the host it runs on (the Pi). */
    page.itemsTree()->setCurrentItem(nullptr);
    page.setProbe([]() { return linuxProbe(); });
    page.show();

    QTreeWidget *tree = page.itemsTree();
    const QList<SystemTuning::Item> items = SystemTuning::check(linuxProbe());
    QCOMPARE(tree->topLevelItemCount(), items.count());
    QCOMPARE(tree->topLevelItem(0)->text(1), QStringLiteral("Real-time DMX timer"));
    /* The first thing to improve is selected, its command shown */
    QCOMPARE(tree->currentItem(), tree->topLevelItem(0));
    QVERIFY(tree->topLevelItem(0)->text(0).contains(QLatin1String("Can improve")));
    QVERIFY(page.commandEdit()->isVisible());
    QVERIFY(page.commandEdit()->toPlainText().contains(QLatin1String("rtprio 95")));

    QGuiApplication::clipboard()->clear();
    QTest::mouseClick(page.copyButton(), Qt::LeftButton);
    QCOMPARE(QGuiApplication::clipboard()->text(), page.commandEdit()->toPlainText());

    /* The kernel item has no command: no box, no Copy */
    for (int i = 0; i < tree->topLevelItemCount(); i++)
        if (tree->topLevelItem(i)->data(0, Qt::UserRole).toString() == QLatin1String("kernel"))
            tree->setCurrentItem(tree->topLevelItem(i));
    QVERIFY(page.commandEdit()->isVisible() == false);
    QVERIFY(page.copyButton()->isVisible() == false);

    /* Check again keeps the selection */
    page.refresh();
    QCOMPARE(tree->currentItem()->data(0, Qt::UserRole).toString(), QStringLiteral("kernel"));
}

void SystemTuning_Test::thePageSavesItsSettings()
{
    QList<QPair<bool, bool> > calls;
    SystemTuning tuning(m_doc);
    tuning.setHold([&calls](bool on, bool display) { calls << qMakePair(on, display); });
    tuning.setProbe([]() { SystemTuning::Probe p; return p; });
    m_doc->setMode(Doc::Operate);

    SystemTuningPage page(m_doc);
    QCOMPARE(page.fullSpeedCheck()->isChecked(), true);
    QCOMPARE(page.displayCheck()->isChecked(), true);
    QCOMPARE(page.warnCheck()->isChecked(), true);
    page.fullSpeedCheck()->setChecked(false);
    page.warnCheck()->setChecked(false);
    /* Nothing saved before OK */
    QCOMPARE(SystemTuning::fullSpeedInOperate(), true);
    page.apply();
    QCOMPARE(SystemTuning::fullSpeedInOperate(), false);
    QCOMPARE(SystemTuning::warnInOperate(), false);
    /* ... and applied: let go during this Operate */
    QCOMPARE(tuning.isHeld(), false);
    QCOMPARE(calls.last(), qMakePair(false, false));
    m_doc->setMode(Doc::Design);
}

void SystemTuning_Test::theFullSpeedBoxUpdatesTheList()
{
    SystemTuningPage page(m_doc);
    page.setProbe([]() {
        SystemTuning::Probe p;
        p.os = SystemTuning::OsMac;
        p.fullSpeedSupported = true;
        p.lowPowerMode = 0;
        return p;
    });
    QTreeWidget *tree = page.itemsTree();
    QTreeWidgetItem *row = nullptr;
    auto fullSpeedRow = [tree]() -> QTreeWidgetItem * {
        for (int i = 0; i < tree->topLevelItemCount(); i++)
            if (tree->topLevelItem(i)->data(0, Qt::UserRole).toString() == QLatin1String("fullspeed"))
                return tree->topLevelItem(i);
        return nullptr;
    };
    row = fullSpeedRow();
    QVERIFY(row != nullptr);
    QCOMPARE(row->text(0), SystemTuning::statusText(SystemTuning::Good));
    page.fullSpeedCheck()->setChecked(false);
    row = fullSpeedRow();
    QCOMPARE(row->text(0), SystemTuning::statusText(SystemTuning::CanImprove));
}

/****************************************************************************
 * What else is going on
 ****************************************************************************/

namespace
{
SystemTuning::Probe macProbe()
{
    SystemTuning::Probe p;
    p.os = SystemTuning::OsMac;
    p.scheduling = MasterTimer::SchedulingRealtime;
    p.fullSpeedSupported = true;
    p.lowPowerMode = 0;
    return p;
}

SystemProbe::ProcessUse proc(const char *name, double cpu)
{
    SystemProbe::ProcessUse use;
    use.name = QString::fromUtf8(name);
    use.cpu = cpu;
    return use;
}
}

void SystemTuning_Test::toolOutputIsRead()
{
    const QList<SystemProbe::ProcessUse> table = SystemProbe::parseProcessTable(
        QStringLiteral("  PID  %CPU COMM\n    1   1.8 launchd\n  255   0,5 Google Chrome\n 4242  90.0 me\n"), 4242);
    QCOMPARE(table.size(), 2);
    QCOMPARE(table.at(1).name, QStringLiteral("Google Chrome"));
    QCOMPARE(table.at(1).cpu, 0.5);

    QCOMPARE(SystemProbe::parseScreenLock(QStringLiteral("2026-11-09 sysadminctl[1] screenLock delay is immediate")), 1);
    QCOMPARE(SystemProbe::parseScreenLock(QStringLiteral("x screenLock delay is 300 seconds")), 1);
    QCOMPARE(SystemProbe::parseScreenLock(QStringLiteral("x screenLock is off")), 0);
    QCOMPARE(SystemProbe::parseScreenLock(QString()), -1);

    QCOMPARE(SystemProbe::parseIdleMinutes(QStringLiteral(" disksleep  10\n displaysleep         120\n")), 120);
    QCOMPARE(SystemProbe::parseIdleMinutes(QStringLiteral(" disksleep  10\n")), -1);

    QCOMPARE(SystemProbe::parseBackupRunning(QStringLiteral("{\n    Percent = \"-1\";\n    Running = 0;\n}")), 0);
    QCOMPARE(SystemProbe::parseBackupRunning(QStringLiteral("    Running = 1;")), 1);
    QCOMPARE(SystemProbe::parseBackupRunning(QString()), -1);

    QCOMPARE(SystemProbe::parseThrottled(QStringLiteral("throttled=0x0\n")), 0);
    QCOMPARE(SystemProbe::parseThrottled(QStringLiteral("throttled=0x50005\n")), 0x50005);
    QCOMPARE(SystemProbe::parseThrottled(QStringLiteral("VCHI failed")), -1);

    QString name;
    qint64 ticks = 0;
    QVERIFY(SystemProbe::parseProcStat(
        QStringLiteral("77 (tmux: (server) x) S 1 77 77 0 -1 4194560 1 0 0 0 30 12 0 0 20 0"), &name, &ticks));
    QCOMPARE(name, QStringLiteral("tmux: (server) x"));
    QCOMPARE(ticks, qint64(42));
    QVERIFY(SystemProbe::parseProcStat(QStringLiteral("garbage"), &name, &ticks) == false);
}

void SystemTuning_Test::frameTimingCountsLateAndDropped()
{
    Doc doc(nullptr);
    MasterTimer *timer = doc.masterTimer();
    const uint period = MasterTimer::tick();

    /* The first frame has nothing to be late against */
    timer->timerTick(true);
    QCOMPARE(timer->frameTiming().frames, quint64(0));

    /* Sleeps only overshoot, so a gap is never shorter than asked: at least
       this many, and a busy host may make the first one dropped as well */
    QTest::qSleep(int(period) * 2);
    timer->timerTick(true);
    MasterTimer::FrameTiming t = timer->frameTiming();
    QCOMPARE(t.frames, quint64(1));
    QCOMPARE(t.late, quint64(1));

    QTest::qSleep(int(period) * 3);
    timer->timerTick(true);
    t = timer->frameTiming();
    QCOMPARE(t.frames, quint64(2));
    QCOMPARE(t.late, quint64(2));
    QVERIFY(t.dropped >= 1);

    /* A direct call is not a frame */
    timer->timerTick();
    QCOMPARE(timer->frameTiming().frames, quint64(2));
}

void SystemTuning_Test::measuredItemFollowsTheLateFrames()
{
    SystemTuning::Probe p = macProbe();
    p.frames.frames = 100;
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("measured"))->status, SystemTuning::Advice);

    p.frames.frames = 10000;
    p.frames.late = 2;
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("measured"))->status, SystemTuning::Good);

    p.frames.late = 50;
    QList<SystemTuning::Item> items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("measured"))->status, SystemTuning::CanImprove);
    QVERIFY(find(items, QStringLiteral("measured"))->important == false);

    p.frames.late = 200;
    p.frames.dropped = 3;
    p.sendP99Ms = 6.25;
    items = SystemTuning::check(p);
    QVERIFY(find(items, QStringLiteral("measured"))->important);
    QVERIFY(find(items, QStringLiteral("measured"))->state.contains(QLatin1String("200 late of 10000 (2.00%), 3 dropped")));
    QVERIFY(find(items, QStringLiteral("measured"))->state.contains(QLatin1String("6.2 ms"))
            || find(items, QStringLiteral("measured"))->state.contains(QLatin1String("6.3 ms")));
}

void SystemTuning_Test::universeThreadsAdviceFollowsTheNumbers()
{
    /* Measured on Linux only: only there does it say to change something */
    SystemTuning::Probe p = linuxProbe();
    p.scheduling = MasterTimer::SchedulingRealtime;
    p.patchedUniverses = 128;
    p.binary = QStringLiteral("/opt/qlcconsole");
    const QList<SystemTuning::Item> items = SystemTuning::check(p);
    const SystemTuning::Item *u = find(items, QStringLiteral("universes"));
    QCOMPARE(u->status, SystemTuning::CanImprove);
    QVERIFY(u->command.startsWith(QLatin1String("QLC_UNIVERSE_SCHED=rt '/opt/qlcconsole'")));
    QVERIFY(u->important == false);

    p.patchedUniverses = 8;
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("universes"))->status, SystemTuning::Good);

    SystemTuning::Probe mac = macProbe();
    mac.patchedUniverses = 128;
    QCOMPARE(find(SystemTuning::check(mac), QStringLiteral("universes"))->status, SystemTuning::Advice);

    p.universeScheduling = Universe::ThreadsRealtime;
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("universes"))->status, SystemTuning::Good);
    p.universeScheduling = Universe::ThreadsRefused;
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("universes"))->status, SystemTuning::CanImprove);
}

void SystemTuning_Test::theMachineItemKnowsTheArchitecture()
{
    SystemTuning::Probe p = macProbe();
    p.arch = p.buildArch = QStringLiteral("arm64");
    p.cpuName = QStringLiteral("Apple M4 Max");
    p.performanceCores = 10;
    p.logicalCores = 14;
    QList<SystemTuning::Item> items = SystemTuning::check(p);
    const SystemTuning::Item *m = find(items, QStringLiteral("machine"));
    QCOMPARE(m->status, SystemTuning::Good);
    QCOMPARE(m->state, QStringLiteral("Apple M4 Max, 10 performance + 4 efficiency cores, arm64"));
    QVERIFY(m->what.contains(QLatin1String("Apple silicon")));

    p.arch = p.buildArch = QStringLiteral("x86_64");
    p.performanceCores = p.logicalCores = 12;
    items = SystemTuning::check(p);
    QVERIFY(find(items, QStringLiteral("machine"))->what.contains(QLatin1String("Intel Mac")));

    /* Rosetta reports the translated architecture, so it is its own flag */
    p.translated = true;
    items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("machine"))->status, SystemTuning::CanImprove);
    QVERIFY(find(items, QStringLiteral("machine"))->important);
    QVERIFY(find(items, QStringLiteral("machine"))->howTo.contains(QLatin1String("Rosetta")));

    /* A 32-bit build on a 64-bit Pi */
    SystemTuning::Probe pi = linuxProbe();
    pi.arch = QStringLiteral("arm64");
    pi.buildArch = QStringLiteral("arm");
    items = SystemTuning::check(pi);
    QCOMPARE(find(items, QStringLiteral("machine"))->status, SystemTuning::CanImprove);
    QVERIFY(find(items, QStringLiteral("machine"))->howTo.contains(QLatin1String("arm64")));
}

void SystemTuning_Test::batteryAndHeatAndThePi()
{
    SystemTuning::Probe p = macProbe();
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("power"))->status, SystemTuning::NotAvailable);
    p.onBattery = 1;
    p.batteryPercent = 42;
    p.thermal = 2;
    QList<SystemTuning::Item> items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("power"))->state, QStringLiteral("on battery, 42%"));
    QVERIFY(find(items, QStringLiteral("power"))->important);
    QVERIFY(find(items, QStringLiteral("thermal"))->important);

    p.onBattery = 0;
    p.thermal = 1;
    items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("power"))->status, SystemTuning::Good);
    QCOMPARE(find(items, QStringLiteral("thermal"))->status, SystemTuning::Advice);

    SystemTuning::Probe pi = linuxProbe();
    pi.model = QStringLiteral("Raspberry Pi 5 Model B Rev 1.0");
    pi.piThrottled = 0;
    QCOMPARE(find(SystemTuning::check(pi), QStringLiteral("pi"))->status, SystemTuning::Good);
    pi.piThrottled = 0x50000;
    items = SystemTuning::check(pi);
    QCOMPARE(find(items, QStringLiteral("pi"))->status, SystemTuning::CanImprove);
    QVERIFY(find(items, QStringLiteral("pi"))->important == false);
    pi.piThrottled = 0x50005;
    items = SystemTuning::check(pi);
    QVERIFY(find(items, QStringLiteral("pi"))->important);
    QVERIFY(find(items, QStringLiteral("pi"))->state.contains(QLatin1String("power supply")));
}

void SystemTuning_Test::wifiMattersMostWhenItCarriesDmx()
{
    SystemTuning::Probe p = macProbe();
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("wifi"))->status, SystemTuning::Good);

    p.wirelessUp << QStringLiteral("en0");
    p.wirelessAddresses << QStringLiteral("192.168.1.20");
    p.dmxAddresses << QStringLiteral("172.18.2.40");
    QList<SystemTuning::Item> items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("wifi"))->status, SystemTuning::CanImprove);
    QVERIFY(find(items, QStringLiteral("wifi"))->important == false);
    QVERIFY(find(items, QStringLiteral("wifi"))->command.isEmpty());

    p.dmxAddresses << QStringLiteral("192.168.1.20");
    items = SystemTuning::check(p);
    QVERIFY(find(items, QStringLiteral("wifi"))->important);
    QVERIFY(find(items, QStringLiteral("wifi"))->state.startsWith(QLatin1String("carrying DMX")));
}

void SystemTuning_Test::localNetworkOnlyWhenEveryOutputFails()
{
    SystemTuning::Probe p = macProbe();
    QVERIFY(find(SystemTuning::check(p), QStringLiteral("localnetwork")) == nullptr);
    p.networkOutputs = 4;
    p.failingOutputs = 1;
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("localnetwork"))->status, SystemTuning::Good);
    p.failingOutputs = 4;
    const QList<SystemTuning::Item> items = SystemTuning::check(p);
    QVERIFY(find(items, QStringLiteral("localnetwork"))->important);
    QVERIFY(find(items, QStringLiteral("localnetwork"))->howTo.contains(QLatin1String("Local Network")));

    SystemTuning::Probe lin = linuxProbe();
    lin.networkOutputs = lin.failingOutputs = 4;
    QVERIFY(find(SystemTuning::check(lin), QStringLiteral("localnetwork")) == nullptr);
}

void SystemTuning_Test::screenLockFollowsKeepDisplayAwake()
{
    SystemTuning::Probe p = macProbe();
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("screenlock"))->status, SystemTuning::Advice);

    p.screenLock = 1;
    p.idleMinutes = 10;
    QList<SystemTuning::Item> items = SystemTuning::check(p);
    const SystemTuning::Item *s = find(items, QStringLiteral("screenlock"));
    QCOMPARE(s->status, SystemTuning::CanImprove);
    QVERIFY(s->important);
    QCOMPARE(s->state, QStringLiteral("locks after 10 min idle"));
    QVERIFY(s->howTo.contains(QLatin1String("Keep the display awake")));

    /* Sleeps without a password: worth fixing, not an emergency */
    p.screenLock = 0;
    items = SystemTuning::check(p);
    QVERIFY(find(items, QStringLiteral("screenlock"))->important == false);

    p.screenLock = 1;
    p.idleMinutes = 0;
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("screenlock"))->status, SystemTuning::Good);

    p.idleMinutes = 10;
    p.keepDisplayAwake = true;
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("screenlock"))->status, SystemTuning::Good);

    /* The hold goes with full speed: without it the box does nothing */
    p.fullSpeedInOperate = false;
    items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("screenlock"))->status, SystemTuning::CanImprove);
    QVERIFY(find(items, QStringLiteral("screenlock"))->howTo.contains(QLatin1String("Run at full speed")));

    /* Linux has no hold: the box can't help, so it says what will */
    SystemTuning::Probe lin = linuxProbe();
    lin.screenLock = 1;
    lin.idleMinutes = 5;
    lin.keepDisplayAwake = true;
    items = SystemTuning::check(lin);
    QCOMPARE(find(items, QStringLiteral("screenlock"))->status, SystemTuning::CanImprove);
    QVERIFY(find(items, QStringLiteral("screenlock"))->command.contains(QLatin1String("lock-enabled false")));
}

void SystemTuning_Test::theKeepAwakeBoxUpdatesTheList()
{
    SystemTuningPage page(m_doc);
    page.setProbe([]() {
        SystemTuning::Probe p = macProbe();
        p.screenLock = 1;
        p.idleMinutes = 10;
        return p;
    });
    QTreeWidget *tree = page.itemsTree();
    auto status = [tree]() {
        for (int i = 0; i < tree->topLevelItemCount(); i++)
            if (tree->topLevelItem(i)->data(0, Qt::UserRole).toString() == QLatin1String("screenlock"))
                return tree->topLevelItem(i)->text(0);
        return QString();
    };
    /* On by default */
    QCOMPARE(status(), SystemTuning::statusText(SystemTuning::Good));
    page.displayCheck()->setChecked(false);
    QVERIFY(status().contains(SystemTuning::statusText(SystemTuning::CanImprove)));
    page.displayCheck()->setChecked(true);
    QCOMPARE(status(), SystemTuning::statusText(SystemTuning::Good));
}

void SystemTuning_Test::backupsRunningIdleAndBuiltIn()
{
    SystemTuning::Probe p = macProbe();
    p.processesKnown = true;
    /* iCloud is on every Mac: idle, it is not worth a line */
    p.processes << proc("bird", 0.1);
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("backups"))->status, SystemTuning::Good);

    p.processes << proc("SynologyDriveFileProvider", 0.2);
    QList<SystemTuning::Item> items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("backups"))->status, SystemTuning::CanImprove);
    QVERIFY(find(items, QStringLiteral("backups"))->important == false);
    QCOMPARE(find(items, QStringLiteral("backups"))->state, QStringLiteral("idle, could start: Synology Drive"));

    p.backupScheduled = 1;
    items = SystemTuning::check(p);
    QVERIFY(find(items, QStringLiteral("backups"))->state.contains(QLatin1String("Time Machine")));
    QVERIFY(find(items, QStringLiteral("backups"))->howTo.contains(QLatin1String("Back up frequency")));

    /* Working now */
    p.processes << proc("bird", 40.0) << proc("bztransmit64", 12.0);
    items = SystemTuning::check(p);
    QVERIFY(find(items, QStringLiteral("backups"))->important);
    QVERIFY(find(items, QStringLiteral("backups"))->state.contains(QLatin1String("iCloud Drive")));
    QVERIFY(find(items, QStringLiteral("backups"))->state.contains(QLatin1String("Backblaze")));

    /* rsync only exists while it works; case and .exe don't matter */
    SystemTuning::Probe win;
    win.os = SystemTuning::OsWindows;
    win.processesKnown = true;
    win.processes << proc("rclone.exe", 0.0) << proc("OneDrive.exe", 0.0);
    items = SystemTuning::check(win);
    QCOMPARE(find(items, QStringLiteral("backups"))->state, QStringLiteral("running now: rclone"));

    SystemTuning::Probe tm = macProbe();
    tm.backupRunning = 1;
    items = SystemTuning::check(tm);
    QVERIFY(find(items, QStringLiteral("backups"))->important);
    QCOMPARE(find(items, QStringLiteral("backups"))->command, QStringLiteral("tmutil stopbackup"));
}

void SystemTuning_Test::updatesPendingRunningAndScheduled()
{
    SystemTuning::Probe win;
    win.os = SystemTuning::OsWindows;
    QCOMPARE(find(SystemTuning::check(win), QStringLiteral("updates"))->status, SystemTuning::Advice);
    win.rebootPending = true;
    QVERIFY(find(SystemTuning::check(win), QStringLiteral("updates"))->important);

    SystemTuning::Probe mac = macProbe();
    mac.processesKnown = true;
    mac.processes << proc("softwareupdated", 0.1);
    QCOMPARE(find(SystemTuning::check(mac), QStringLiteral("updates"))->status, SystemTuning::Good);
    mac.autoUpdates = 1;
    QList<SystemTuning::Item> items = SystemTuning::check(mac);
    QCOMPARE(find(items, QStringLiteral("updates"))->status, SystemTuning::CanImprove);
    QVERIFY(find(items, QStringLiteral("updates"))->howTo.contains(QLatin1String("Software Update")));
    mac.processes << proc("softwareupdated", 30.0);
    QVERIFY(find(SystemTuning::check(mac), QStringLiteral("updates"))->important);

    SystemTuning::Probe lin = linuxProbe();
    lin.autoUpdates = 1;
    QVERIFY(find(SystemTuning::check(lin), QStringLiteral("updates"))->command.contains(QLatin1String("apt-daily")));

    /* Debian's unattended-upgrade-shutdown sits there all day, doing nothing */
    SystemTuning::Probe deb = linuxProbe();
    deb.processesKnown = true;
    deb.processes << proc("unattended-upgr", 0.0);
    QCOMPARE(find(SystemTuning::check(deb), QStringLiteral("updates"))->status, SystemTuning::Good);
}

void SystemTuning_Test::otherProgramsAddUpByName()
{
    SystemTuning::Probe p = macProbe();
    p.performanceCores = 4;
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("load"))->status, SystemTuning::NotAvailable);

    p.processesKnown = true;
    p.processes << proc("launchd", 1.0);
    QCOMPARE(find(SystemTuning::check(p), QStringLiteral("load"))->status, SystemTuning::Good);

    p.processes << proc("moc", 40.0) << proc("moc", 40.0) << proc("mds_stores", 60.0);
    QList<SystemTuning::Item> items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("load"))->state, QStringLiteral("moc 80%, Spotlight indexing 60%"));
    QVERIFY(find(items, QStringLiteral("load"))->important == false);

    /* Half the performance cores: the show will feel it */
    p.processes << proc("clang", 120.0);
    items = SystemTuning::check(p);
    QVERIFY(find(items, QStringLiteral("load"))->important);
    QVERIFY(find(items, QStringLiteral("load"))->state.startsWith(QLatin1String("clang 120%")));
}

void SystemTuning_Test::memoryDiskAndRigSettings()
{
    SystemTuning::Probe p = macProbe();
    p.memory = 0;
    p.diskFreeMb = 100000;
    QList<SystemTuning::Item> items = SystemTuning::check(p);
    QCOMPARE(find(items, QStringLiteral("memory"))->status, SystemTuning::Good);
    QCOMPARE(find(items, QStringLiteral("appsettings"))->status, SystemTuning::Good);

    p.memory = 2;
    p.diskFreeMb = 512;
    p.rigOperateFps = 60;
    p.rigSupersample = 4;
    p.rigGpu = false;
    items = SystemTuning::check(p);
    QVERIFY(find(items, QStringLiteral("memory"))->important);
    QVERIFY(find(items, QStringLiteral("memory"))->state.contains(QLatin1String("512 MB")));
    QCOMPARE(find(items, QStringLiteral("appsettings"))->status, SystemTuning::CanImprove);
    QVERIFY(find(items, QStringLiteral("appsettings"))->state.contains(QLatin1String("60 frames")));
    QVERIFY(find(items, QStringLiteral("appsettings"))->state.contains(QLatin1String("graphics card")));
}

void SystemTuning_Test::theLookRunsBelowTheShow()
{
    SystemTuning tuning(m_doc);
    tuning.setHold([](bool, bool) {});
    tuning.setProbe([]() { return macProbe(); });

    QAtomicInt priority(-1);
    tuning.setHostGather([&priority](SystemTuning::Probe *p, bool deep) {
        priority.storeRelaxed(int(QThread::currentThread()->priority()));
        p->deepLooked = deep;
    });
    m_doc->setMode(Doc::Operate);
    QTRY_COMPARE(priority.loadRelaxed(), int(QThread::LowestPriority));
    m_doc->setMode(Doc::Design);
    QThreadPool::globalInstance()->waitForDone(2000);

#if defined(Q_OS_WIN)
    QSKIP("Windows runs no tools");
#else
    /* What a real tool gets: the highest niceness the system allows, without
       a child process modifier (that makes QProcess fork the whole app) */
    bool ok = false;
    const int nice = SystemProbe::runTool(QStringLiteral("/bin/sh"),
                                          QStringList() << QStringLiteral("-c")
                                                        << QStringLiteral("ps -o nice= -p $$"),
                                          QDeadlineTimer(5000)).trimmed().toInt(&ok);
    QVERIFY(ok);
    /* Below the show: nicer than the app itself. How far up a host lets it
       go varies (a GitHub macOS runner stops at 10), so not a fixed 19. */
    errno = 0;
    const int own = getpriority(PRIO_PROCESS, 0);
    QVERIFY(errno == 0);
    QVERIFY2(nice > own && nice >= 10, qPrintable(QString("tool %1, app %2").arg(nice).arg(own)));
#endif
}

void SystemTuning_Test::hostLookRunsOffTheUiThread()
{
    SystemTuning tuning(m_doc);
    tuning.setHold([](bool, bool) {});
    tuning.setProbe([]() { return macProbe(); });

    QAtomicPointer<QThread> ranOn(nullptr);
    tuning.setHostGather([&ranOn](SystemTuning::Probe *p, bool deep) {
        ranOn.storeRelaxed(QThread::currentThread());
        p->deepLooked = deep;
        p->onBattery = 1;
    });

    m_doc->setMode(Doc::Operate);
    QVERIFY(hasStatus(SystemTuning::statusKey()) == false); /* not yet: it is on the worker */
    QTRY_VERIFY(hasStatus(SystemTuning::statusKey()));
    QVERIFY(ranOn.loadRelaxed() != nullptr);
    QVERIFY(ranOn.loadRelaxed() != QThread::currentThread());
    QVERIFY(statusEntry(SystemTuning::statusKey()).items.first().startsWith(QLatin1String("Mains power")));

    /* An answer that lands after the show left Operate says nothing */
    tuning.recheckNow();
    m_doc->setMode(Doc::Design);
    QVERIFY(hasStatus(SystemTuning::statusKey()) == false);
    QTest::qWait(100);
    QVERIFY(hasStatus(SystemTuning::statusKey()) == false);
}

void SystemTuning_Test::aLightLookKeepsTheDeepAnswers()
{
    SystemTuning tuning(m_doc);
    tuning.setHold([](bool, bool) {});
    tuning.setProbe([]() { return macProbe(); });
    tuning.setHostGather([](SystemTuning::Probe *p, bool deep) {
        p->deepLooked = deep;
        if (deep)
            p->backupRunning = 1;
    });

    /* The full look runs outside Operate */
    tuning.recheckNow();
    QThreadPool::globalInstance()->waitForDone(2000);
    QTest::qWait(50);

    /* Operate's looks are light: they don't run tmutil. Not knowing isn't
       "it stopped". */
    m_doc->setMode(Doc::Operate);
    const QString key = SystemTuning::statusKey();
    QTRY_VERIFY(hasStatus(key));
    QVERIFY(statusEntry(key).items.first().contains(QLatin1String("Time Machine")));
    tuning.recheckNow();
    QTest::qWait(150);
    QVERIFY(hasStatus(key));
    QVERIFY(statusEntry(key).items.first().contains(QLatin1String("Time Machine")));
    m_doc->setMode(Doc::Design);
}

void SystemTuning_Test::operateRunsNoToolsAndDesignDoes()
{
    SystemTuning tuning(m_doc);
    tuning.setHold([](bool, bool) {});
    tuning.setProbe([]() { return macProbe(); });
    QMutex mutex;
    QList<bool> looks;
    tuning.setHostGather([&mutex, &looks](SystemTuning::Probe *p, bool deep) {
        QMutexLocker locker(&mutex);
        looks << deep;
        p->deepLooked = deep;
    });
    const auto lookCount = [&mutex, &looks]() { QMutexLocker locker(&mutex); return looks.count(); };

    /* Operate: light looks only, and nothing may start a process */
    m_doc->setMode(Doc::Operate);
    QVERIFY(SystemProbe::toolsAllowed() == false);
    QTRY_COMPARE(lookCount(), 1);
    QThreadPool::globalInstance()->waitForDone(2000);
    QTest::qWait(50);
    tuning.recheckNow();
    QTRY_COMPARE(lookCount(), 2);
    QThreadPool::globalInstance()->waitForDone(2000);
    QTest::qWait(50);
    {
        QMutexLocker locker(&mutex);
        QCOMPARE(looks, QList<bool>() << false << false);
    }

    /* Whoever asks: the real full look stays light, and no tool runs */
    const int started = SystemProbe::toolsStarted();
    SystemTuning::Probe real = SystemTuning::probeDoc(m_doc);
    SystemTuning::probeHost(&real, true);
    QVERIFY(real.deepLooked == false);
    QVERIFY(real.processes.isEmpty());
#if !defined(Q_OS_WIN)
    QVERIFY(SystemProbe::runTool(QStringLiteral("/bin/echo"), QStringList() << QStringLiteral("hi"),
                                 QDeadlineTimer(5000)).isEmpty());
#endif
    QCOMPARE(SystemProbe::toolsStarted(), started);

    /* Back in Design the full look runs again, at once */
    m_doc->setMode(Doc::Design);
    QVERIFY(SystemProbe::toolsAllowed());
    QTRY_COMPARE(lookCount(), 3);
    {
        QMutexLocker locker(&mutex);
        QCOMPARE(looks.last(), true);
    }
    QThreadPool::globalInstance()->waitForDone(2000);
#if !defined(Q_OS_WIN)
    QCOMPARE(SystemProbe::runTool(QStringLiteral("/bin/echo"), QStringList() << QStringLiteral("hi"),
                                  QDeadlineTimer(5000)).trimmed(), QStringLiteral("hi"));
    QCOMPARE(SystemProbe::toolsStarted(), started + 1);
#endif
}

void SystemTuning_Test::thePageLooksLightlyInOperate()
{
    SystemTuning tuning(m_doc);
    tuning.setHold([](bool, bool) {});
    QVERIFY(SystemTuning::instance() == &tuning);

    /* What the last full look found, before Operate */
    SystemTuning::Probe deep = macProbe();
    deep.deepLooked = true;
    deep.backupRunning = 1;
    tuning.rememberLook(&deep);

    m_doc->setMode(Doc::Operate);
    const int started = SystemProbe::toolsStarted();
    SystemTuningPage page(m_doc);
    QCOMPARE(SystemProbe::toolsStarted(), started);
    QVERIFY(page.fullCheckLabel()->text().contains(QLatin1String("outside Operate")));
    bool timeMachine = false;
    for (int i = 0; i < page.itemsTree()->topLevelItemCount(); i++)
        timeMachine |= page.itemsTree()->topLevelItem(i)->text(2).contains(QLatin1String("Time Machine"));
    QVERIFY(timeMachine);
    m_doc->setMode(Doc::Design);

    /* Outside Operate the page's look is the full one, and says when */
    page.refresh();
    QVERIFY(SystemTuning::lastFullCheck().isValid());
    QVERIFY(page.fullCheckLabel()->text().contains(
        QLocale().toString(SystemTuning::lastFullCheck().time(), QLocale::ShortFormat)));
}

void SystemTuning_Test::theRealLookIsBounded()
{
    QElapsedTimer t;
    t.start();
    const SystemTuning::Probe p = SystemTuning::probe(m_doc);
    /* The look's own limit plus the first CPU sample's wait, with slack for
       a loaded gate host; a hung tool must not hold the Settings dialog */
    QVERIFY2(t.elapsed() < 5000, qPrintable(QString::number(t.elapsed())));
    QVERIFY(p.arch.isEmpty() == false);
    QVERIFY(p.deepLooked);
    QVERIFY(p.logicalCores >= p.performanceCores);
    QVERIFY(SystemProbe::parseScreenLock(QString()) == -1);
    /* QLC_TUNING_DUMP=dir: what this machine really answers, by hand, and
       the page at three sizes for a visual check. Never in the gate. */
    const QString dump = qEnvironmentVariable("QLC_TUNING_DUMP");
    if (dump.isEmpty() == false)
    {
        qInfo("%lld ms", qint64(t.elapsed()));
        for (const SystemTuning::Item &it : SystemTuning::check(p))
            qInfo("%-13s %-13s %s%s", qPrintable(it.key), qPrintable(SystemTuning::statusText(it.status)),
                  qPrintable(it.state), (it.important && it.status == SystemTuning::CanImprove) ? " [important]" : "");
        SystemTuningPage page(m_doc);
        page.setProbe([p]() { return p; });
        page.show();
        for (const QSize &size : { QSize(640, 520), QSize(900, 640), QSize(1400, 900) })
        {
            page.resize(size);
            QApplication::processEvents();
            page.grab().save(QStringLiteral("%1/performance-%2.png").arg(dump).arg(size.width()));
        }
    }
#if defined(Q_OS_MACOS)
    QVERIFY(p.thermal >= 0);
    QVERIFY(p.onBattery >= 0);
    QVERIFY(p.memory >= 0);
#endif
}

QTEST_MAIN(SystemTuning_Test)
