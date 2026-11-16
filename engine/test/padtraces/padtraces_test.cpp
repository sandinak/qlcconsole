/*
  Q Light Controller Plus
  padtraces_test.cpp

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
#include <QMutex>
#include <QSettings>
#include <QTemporaryDir>

#include "padtraces_test.h"
#include "fakehidbackend.h"
#include "padtraceplayer.h"
#include "controllerjobs.h"
#include "programmercontroller.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "mastertimer.h"
#include "showcontrol.h"
#include "padrun.h"
#include "doc.h"

namespace
{
const QString kDualSense = QStringLiteral("DualSense Wireless Controller");
const QString kXbox = QStringLiteral("Xbox 360 Controller for Windows");
const QString kDualSensePath = QStringLiteral("USB_054c_0ce6_2124000");
/* The trace's own timing, four times as fast: a chord's hold stays well over
   the 15 ms the test gives it, and the longest trace plays in about 2 s. */
const double kSpeed = 4.0;

/* Never deleted: a thread may still be leaving a call into one. */
QList<FakeHIDBackend *> s_fakes;

QString trace(const char *name)
{
    return QStringLiteral(QLC_PAD_TRACE_DIR "/") + QLatin1String(name);
}
}

void PadTraces_Test::initTestCase()
{
    m_pluginDir = new QTemporaryDir();
    QVERIFY(m_pluginDir->isValid());
    const QFileInfo hid(QStringLiteral(QLC_HID_PLUGIN_FILE));
    QVERIFY2(hid.exists(), QLC_HID_PLUGIN_FILE);
    QVERIFY(QFile::copy(hid.absoluteFilePath(), m_pluginDir->filePath(hid.fileName())));
    qputenv("QLC_HID_PROFILE_DIR", QByteArray(QLC_TEST_RESOURCE_ROOT) + "hidprofiles");
    qputenv("QLC_HID_VIRTUAL_ONLY", "1");
    qputenv("QLC_HID_OPEN_LIMIT_MS", "300");
    qunsetenv("QLC_HID_RECORD");
}

void PadTraces_Test::cleanupTestCase()
{
    delete m_pluginDir;
    m_pluginDir = nullptr;
}

void PadTraces_Test::init()
{
    QSettings().remove(ControllerJobs::settingsGroup());    // every pad every job
    if (QLatin1String(QTest::currentTestFunction()) == QLatin1String("aSessionIsRecorded"))
    {
        m_recordDir = new QTemporaryDir();
        QVERIFY(m_recordDir->isValid());
        qputenv("QLC_HID_RECORD", m_recordDir->path().toUtf8());
    }
    m_doc = new Doc(this);
    m_commands.clear();
    m_doc->showControl()->setRunHandler([this](const ShowControl::Request &r, ShowControl::Route) {
        m_commands << int(r.command);
        return true;
    });
    m_doc->ioPluginCache()->load(QDir(m_pluginDir->path()));
    m_hid = m_doc->ioPluginCache()->plugin(QStringLiteral("HID"));
    QVERIFY2(m_hid != nullptr, "the HID plugin did not load");
    m_fake = new FakeHIDBackend();
    s_fakes << m_fake;
    QMetaObject::invokeMethod(m_hid, "setTestBackend", Qt::DirectConnection,
                              Q_ARG(quintptr, quintptr(m_fake)));
    m_player = new PadTracePlayer(m_hid, m_fake);
    m_player->setSpeed(kSpeed);
}

void PadTraces_Test::teardown()
{
    if (m_doc == nullptr)
        return;
    m_fake->release();
    QTRY_COMPARE_WITH_TIMEOUT(m_fake->callsInProgress(), 0, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(m_fake->readsInProgress(), 0, 2000);
    m_doc->showControl()->setRunHandler(ShowControl::RunHandler());
    m_doc->masterTimer()->stop();
    delete m_doc;
    m_doc = nullptr;
    m_hid = nullptr;
}

void PadTraces_Test::cleanup()
{
    teardown();
    delete m_player;
    m_player = nullptr;
    delete m_recordDir;
    m_recordDir = nullptr;
    qunsetenv("QLC_HID_RECORD");
    QSettings().remove(ControllerJobs::settingsGroup());
}

int PadTraces_Test::patchPads(const QString &name)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    const QStringList inputs = m_hid->inputs();
    int universe = 1;
    for (int i = 0; i < inputs.size(); ++i)
        if (inputs.at(i) == name)
            iom->setInputPatch(quint32(universe++), QStringLiteral("HID"), QString(), quint32(i));
    return universe - 1;
}

void PadTraces_Test::startRunning()
{
    m_doc->inputOutputMap()->startUniverses();
    m_doc->masterTimer()->start();
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    run->setArmChordMs(15);
}

int PadTraces_Test::commandCount(int command) const
{
    return int(m_commands.count(command));
}

int PadTraces_Test::inputsNamed(const QString &name) const
{
    return int(m_hid->inputs().count(name));
}

void PadTraces_Test::cloneFlipToXbox()
{
    /* A cabled DualSense clone, armed and used; Create + Options held for
       ~3 s; it leaves; three Xbox 360 arrivals at its port, two leaving
       again. The operator's rules: the arm drops, saying why; nothing arms
       it again; no stale device stays; a GO from what came back moves no
       cue. */
    QString error;
    QVERIFY2(m_player->load(trace("clone_flip_to_xbox.jsonl"), &error), qPrintable(error));
    PadRun *run = m_doc->programmer()->padRun();

    QVERIFY(m_player->step() != nullptr);       // the DualSense arrives
    QTRY_VERIFY_WITH_TIMEOUT(m_hid->inputs().contains(kDualSense), 2000);
    QCOMPARE(patchPads(kDualSense), 1);
    startRunning();

    // LB + RB arm, LT + A is a GO.
    QVERIFY(m_player->playUntil([](const PadTracePlayer::Event &e) { return e.t >= 1500; }));
    QTRY_VERIFY_WITH_TIMEOUT(run->isArmed(), 2000);
    QVERIFY(m_player->playUntil([](const PadTracePlayer::Event &e) { return e.t >= 2100; }));
    QTRY_COMPARE_WITH_TIMEOUT(commandCount(int(ShowControl::Go)), 1, 2000);
    QVERIFY(run->isArmed());

    // Create + Options held, then the pad is gone from the system.
    QVERIFY(m_player->playUntil([](const PadTracePlayer::Event &e) {
        return e.ev == QLatin1String("gone") && e.o.value("path").toString() == kDualSensePath; }));
    QTRY_VERIFY_WITH_TIMEOUT(run->isArmed() == false, 2000);
    QCOMPARE(run->notice(), QString("Disarmed: the controller went away."));
    QVERIFY(m_hid->inputs().contains(kDualSense) == false);

    // The clone comes back as an Xbox pad, three times over.
    m_player->playAll();
    QCOMPARE(m_player->orphanInputs(), 0);
    QTest::qWait(100);
    QVERIFY2(run->isArmed() == false, "the arm came back by itself");
    QCOMPARE(commandCount(int(ShowControl::Go)), 1);    // the GO from the Xbox pad moved nothing
    QCOMPARE(commandCount(int(ShowControl::AllOff)), 0);
    QCOMPARE(inputsNamed(kDualSense), 0);
    QCOMPARE(inputsNamed(kXbox), 1);                    // the two that left are not kept
    QCOMPARE(m_hid->targetProblems().count(), 0);
    int opening = -1;
    QMetaObject::invokeMethod(m_hid, "openingCount", Qt::DirectConnection, Q_RETURN_ARG(int, opening));
    QCOMPARE(opening, 0);
}

void PadTraces_Test::twinListing()
{
    /* One pad listed twice with the same identity: one input, one runner
       (one GO for one LT + A), one stop for the emergency stop. */
    QString error;
    QVERIFY2(m_player->load(trace("twin_listing.jsonl"), &error), qPrintable(error));
    PadRun *run = m_doc->programmer()->padRun();

    QVERIFY(m_player->step() != nullptr);
    QVERIFY(m_player->step() != nullptr);       // the second listing of it
    QTRY_VERIFY_WITH_TIMEOUT(m_hid->inputs().contains(kDualSense), 2000);
    QCOMPARE(inputsNamed(kDualSense), 1);
    QCOMPARE(patchPads(kDualSense), 1);
    startRunning();

    QVERIFY(m_player->playUntil([](const PadTracePlayer::Event &e) { return e.t >= 1300; }));
    QTRY_VERIFY_WITH_TIMEOUT(run->isArmed(), 2000);
    QVERIFY(m_player->playUntil([](const PadTracePlayer::Event &e) { return e.t >= 2200; }));
    QTRY_COMPARE_WITH_TIMEOUT(commandCount(int(ShowControl::Go)), 1, 2000);
    m_player->playAll();
    QTRY_COMPARE_WITH_TIMEOUT(commandCount(int(ShowControl::AllOff)), 1, 2000);
    QTest::qWait(100);
    QCOMPARE(commandCount(int(ShowControl::Go)), 1);
    QCOMPARE(commandCount(int(ShowControl::AllOff)), 1);
    QVERIFY(run->isArmed() == false);           // the stop drops the arm
    QCOMPARE(inputsNamed(kDualSense), 1);
}

void PadTraces_Test::triggerRest()
{
    /* Triggers resting at ~8 % and the sticks' jitter: the chord still
       arms, but A with no LT held is no GO (the resting trigger is not the
       safety), and nothing fires or stops. */
    QString error;
    QVERIFY2(m_player->load(trace("trigger_rest.jsonl"), &error), qPrintable(error));
    PadRun *run = m_doc->programmer()->padRun();

    QVERIFY(m_player->step() != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(m_hid->inputs().contains(kDualSense), 2000);
    QCOMPARE(patchPads(kDualSense), 1);
    startRunning();

    // Resting alone: never armed.
    QVERIFY(m_player->playUntil([](const PadTracePlayer::Event &e) { return e.t >= 700; }));
    QVERIFY(run->isArmed() == false);
    QVERIFY(m_commands.isEmpty());

    // The chord arms it, with the triggers resting all the while.
    QVERIFY(m_player->playUntil([](const PadTracePlayer::Event &e) { return e.t >= 1500; }));
    QTRY_VERIFY_WITH_TIMEOUT(run->isArmed(), 2000);

    // Three presses of A, no LT.
    m_player->playAll();
    QTest::qWait(100);
    QCOMPARE(m_player->orphanInputs(), 0);
    QVERIFY(run->isArmed());
    QVERIFY2(m_commands.isEmpty(), qPrintable(QString("a resting trigger moved the show: %1 commands").arg(m_commands.size())));
}

void PadTraces_Test::aSessionIsRecorded()
{
    /* QLC_HID_RECORD: the same flip, recorded as it is played -- every
       arrival, departure and delivered input, in time order, with the
       identity ControllerJobs gives. The file loads as a trace itself. */
    QString error;
    QVERIFY2(m_player->load(trace("clone_flip_to_xbox.jsonl"), &error), qPrintable(error));
    m_player->setSpeed(0);      // the timing is not what is under test
    QVERIFY(m_player->step() != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(m_hid->inputs().contains(kDualSense), 2000);
    QCOMPARE(patchPads(kDualSense), 1);
    startRunning();
    m_player->playAll();
    QTest::qWait(100);
    teardown();     // the plugin goes: its recorder writes what is queued and closes

    const QStringList files = QDir(m_recordDir->path()).entryList(QStringList() << "pads-*.jsonl", QDir::Files);
    QCOMPARE(files.size(), 1);
    const QString file = m_recordDir->filePath(files.first());
    PadTracePlayer back(nullptr, nullptr);
    QVERIFY2(back.load(file, &error), qPrintable(error));

    int arrived = 0, gone = 0, inputs = 0;
    qint64 last = 0;
    for (const PadTracePlayer::Event &e : back.events())
    {
        QVERIFY2(e.t >= last, "time went backwards");
        last = e.t;
        if (e.ev == QLatin1String("arrived"))
        {
            arrived++;
            const QString name = e.o.value("name").toString();
            const QString location = e.o.value("location").toString();
            QCOMPARE(location, QString("USB 2124000"));
            QCOMPARE(e.o.value("identity").toString(),
                     ControllerJobs::identityFor(QStringLiteral("HID"), name, location));
            QCOMPARE(e.o.value("axes").toInt(), 6);
            QVERIFY(e.o.value("buttons").toInt() > 0);
            if (arrived == 1)
            {
                QCOMPARE(name, kDualSense);
                QCOMPARE(e.o.value("vid").toString(), QString("054c"));
                QCOMPARE(e.o.value("pid").toString(), QString("0ce6"));
                QCOMPARE(e.o.value("serial").toString(), QString("SYN0001"));
            }
        }
        else if (e.ev == QLatin1String("gone"))
            gone++;
        else if (e.ev == QLatin1String("input"))
        {
            inputs++;
            QVERIFY(e.o.contains("ch") && e.o.contains("value") && !e.o.value("path").toString().isEmpty());
        }
    }
    QCOMPARE(arrived, 4);   // the DualSense, the Xbox three times
    QCOMPARE(gone, 3);      // the DualSense, the Xbox twice
    QVERIFY2(inputs >= 10, qPrintable(QString::number(inputs)));
}

QTEST_MAIN(PadTraces_Test)
