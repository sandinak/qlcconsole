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

#include <QtTest>
#include <QClipboard>
#include <QCheckBox>
#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>

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
    QCOMPARE(SystemTuning::keepDisplayAwakeInOperate(), false);
    QCOMPARE(SystemTuning::warnInOperate(), true);

    SystemTuning::setFullSpeedInOperate(false);
    SystemTuning::setKeepDisplayAwakeInOperate(true);
    SystemTuning::setWarnInOperate(false);
    QCOMPARE(SystemTuning::fullSpeedInOperate(), false);
    QCOMPARE(SystemTuning::keepDisplayAwakeInOperate(), true);
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
    QCOMPARE(calls.last(), qMakePair(true, false));

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
    QCOMPARE(calls.last(), qMakePair(true, false));
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
    QCOMPARE(page.displayCheck()->isChecked(), false);
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

QTEST_MAIN(SystemTuning_Test)
