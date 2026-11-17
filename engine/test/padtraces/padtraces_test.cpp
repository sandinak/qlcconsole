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
#include <QSignalSpy>
#include <QScopeGuard>
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
/* The trace's own timing, twice as fast: a chord's hold (350 ms) stays far
   over the 15 ms the test gives it, even on a loaded host, and the longest
   trace plays in about 5 s. */
const double kSpeed = 2.0;

/* The replay settled every scan and pad in time, and no input found no pad. */
#define VERIFY_CLEAN_PLAY() \
    do { \
        QVERIFY2(m_player->problems().isEmpty(), qPrintable(m_player->problems().join(QStringLiteral("; ")))); \
        QCOMPARE(m_player->orphanInputs(), 0); \
    } while (false)

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
    const QString current = QLatin1String(QTest::currentTestFunction());
    if (current == QLatin1String("aSessionIsRecorded") || current == QLatin1String("theIdentitiesAreTheEngines")
        || current == QLatin1String("aLongRecordingStopsAtItsCap"))
    {
        if (current == QLatin1String("aLongRecordingStopsAtItsCap"))
            qputenv("QLC_HID_RECORD_MAX_BYTES", "1500");
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
    qunsetenv("QLC_HID_RECORD_MAX_BYTES");
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
    /* The bench as it happened: Create + Options held ~3 s (View + Menu 1 s
       switches the role, which drops the arm: no button arms or disarms
       but the chord, rule 3, a role switch is one of the exceptions); the
       pad leaves; three Xbox 360 arrivals at its port, two leaving again.
       Disarmed, nothing arms it again, no stale device stays, a GO from
       what came back moves no cue. */
    flip(true);
}

void PadTraces_Test::cloneLeavesWhileArmed()
{
    /* The same flip without the hold (a clone that drops off USB while
       armed some other way, as an unplug does): the arm drops saying why. */
    flip(false);
}

void PadTraces_Test::flip(bool hold)
{
    QString error;
    QVERIFY2(m_player->load(trace("clone_flip_to_xbox.jsonl"), &error), qPrintable(error));
    if (!hold)
        m_player->setSkip([](const PadTracePlayer::Event &e) {
            const int ch = e.o.value("ch").toInt(-1);
            return e.ev == QLatin1String("input") && (ch == 12 || ch == 13); });
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

    // Create + Options (or not), then the pad is gone from the system.
    QVERIFY(m_player->playUntil([](const PadTracePlayer::Event &e) {
        return e.ev == QLatin1String("gone") && e.o.value("path").toString() == kDualSensePath; }));
    QTRY_VERIFY_WITH_TIMEOUT(run->isArmed() == false, 2000);
    if (!hold)
        QCOMPARE(run->notice(), QString("Disarmed: the controller went away."));
    QVERIFY(m_hid->inputs().contains(kDualSense) == false);

    // The clone comes back as an Xbox pad, three times over.
    m_player->playAll();
    VERIFY_CLEAN_PLAY();
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
    VERIFY_CLEAN_PLAY();
}

namespace
{
const quint32 kPadLt = 2, kPadRt = 5, kPadA = 6, kPadLb = 10, kPadRb = 11;
}

/* The pads by path: rescanned until @p count of @p name are listed. */
void PadTraces_Test::q253Scan(const QString &name, int count)
{
    QMetaObject::invokeMethod(m_hid, "rescanDevices", Qt::DirectConnection);
    QTRY_VERIFY_WITH_TIMEOUT(inputsNamed(name) == count, 3000);
    QTest::qWait(50);
}

int PadTraces_Test::q253Line(const QString &path) const
{
    int line = -1;
    QMetaObject::invokeMethod(m_hid, "inputForPath", Qt::DirectConnection,
                              Q_RETURN_ARG(int, line), Q_ARG(QString, path));
    return line;
}

bool PadTraces_Test::q253In(const QString &path, quint32 ch, uint value)
{
    bool ok = false;
    QMetaObject::invokeMethod(m_hid, "injectVirtualInput", Qt::DirectConnection, Q_RETURN_ARG(bool, ok),
                              Q_ARG(quint32, quint32(q253Line(path))), Q_ARG(quint32, ch), Q_ARG(uint, value));
    return ok;
}

void PadTraces_Test::aShiftedLineIsNoSafety()
{
    /* Two pads of one name with no location (Linux joysticks, /dev/input/
       js*), patched to universes 1 and 2. A arms and holds LT; A goes, so B
       moves onto A's line and universe 1 hears B -- with no patch change.
       That is pad A lost: disarmed, saying so; B's LT + A plays nothing
       (board Q253's fourth review). Through the real HID plugin. */
    const QString name = QStringLiteral("Xbox Wireless Controller");
    const QString a = QStringLiteral("/dev/input/js0"), b = QStringLiteral("/dev/input/js1");
    QStringList heard;
    const QMetaObject::Connection listen = connect(m_doc->inputOutputMap(), &InputOutputMap::inputValueChanged, this,
        [&heard](quint32 u, quint32 ch, uchar v) { heard << QString("u%1:ch%2=%3").arg(u).arg(ch).arg(v); });
    auto unlisten = qScopeGuard([&]() { disconnect(listen); });
    m_fake->list(a, 0x045e, 0x028e, name, 0, 1);
    q253Scan(name, 1);
    m_fake->list(b, 0x045e, 0x028e, name, 0, 2);
    q253Scan(name, 2);
    QCOMPARE(q253Line(a), 0);
    QCOMPARE(q253Line(b), 1);
    QCOMPARE(patchPads(name), 2);
    startRunning();
    PadRun *run = m_doc->programmer()->padRun();
    // A arms and holds the safety (heard: the patch delivers once a frame).
    QVERIFY(q253In(a, kPadLb, 255));
    QVERIFY(q253In(a, kPadRb, 255));
    QTRY_VERIFY_WITH_TIMEOUT(run->isArmed(), 2000);
    QVERIFY(q253In(a, kPadRb, 0));
    QVERIFY(q253In(a, kPadLb, 0));
    QVERIFY(q253In(a, kPadLt, 255));
    QTRY_VERIFY_WITH_TIMEOUT(heard.contains(QStringLiteral("u1:ch2=255")), 2000);
    /* A goes: B is on line 0 now. Universe 2 follows B there; universe 1
       does not take B (another patch follows it): A's universe waits,
       offline, and pad A is lost. */
    m_fake->unlist(a);
    q253Scan(name, 1);
    QCOMPARE(q253Line(b), 0);
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY2(iom->inputPatch(1) == nullptr || iom->inputPatch(1)->input() == QLCIOPlugin::invalidLine(),
             qPrintable(QString("universe 1 took line %1").arg(iom->inputPatch(1)->input())));
    QCOMPARE(iom->inputPatch(2)->input(), quint32(0));
    QTRY_VERIFY2_WITH_TIMEOUT(!run->isArmed(), "the arm stayed with pad A gone", 2000);
    QCOMPARE(run->roleText(), QString("Pad: Run"));
    QCOMPARE(run->chipNotice(), QString("Disarmed: the controller went away."));
    // B's LT + A: nothing plays (B has not armed).
    heard.clear();
    QVERIFY(q253In(b, kPadLt, 255));
    QTRY_VERIFY_WITH_TIMEOUT(heard.contains(QStringLiteral("u2:ch2=255")), 2000);
    QVERIFY(q253In(b, kPadA, 255));
    QVERIFY(q253In(b, kPadA, 0));
    QTest::qWait(100);
    QCOMPARE(commandCount(int(ShowControl::Go)), 0);
    QVERIFY(q253In(b, kPadLt, 0));
}

void PadTraces_Test::anotherPadsDropLeavesTheRunnerAsItWas_data()
{
    QTest::addColumn<bool>("located");
    QTest::newRow("macOS-like: each pad its USB location") << true;
    QTest::newRow("Linux-like: no location, one name") << false;
}

void PadTraces_Test::anotherPadsDropLeavesTheRunnerAsItWas()
{
    /* Three pads; C (the last) runs cues. The pads before it go, one at a
       time, so C moves down a line each time and its universe's patch
       follows it -- the same device: C stays armed, its held LT still a
       safety (LT + A plays), and a stop squeezed three of four across a
       drop completes with the fourth. No reading of C's is lost. */
    QFETCH(bool, located);
    const QString name = QStringLiteral("Xbox Wireless Controller");
    QStringList paths;
    for (int i = 0; i < 3; ++i)
        paths << (located ? QStringLiteral("USB_045e_028e_%1000000").arg(i + 1)
                          : QStringLiteral("/dev/input/js%1").arg(i));
    const QString c = paths.at(2);
    for (int i = 0; i < 3; ++i)
    {
        m_fake->list(paths.at(i), 0x045e, 0x028e, name, 0, quint64(i + 1));
        q253Scan(name, i + 1);
    }
    QCOMPARE(q253Line(c), 2);
    QCOMPARE(patchPads(name), 3);       // universes 1, 2, 3; C on 3
    startRunning();
    PadRun *run = m_doc->programmer()->padRun();
    QStringList heard;
    const QMetaObject::Connection listen = connect(m_doc->inputOutputMap(), &InputOutputMap::inputValueChanged, this,
        [&heard](quint32 u, quint32 ch, uchar v) { heard << QString("u%1:ch%2=%3").arg(u).arg(ch).arg(v); });
    auto unlisten = qScopeGuard([&]() { disconnect(listen); });
    // C arms and holds LT.
    QVERIFY(q253In(c, kPadLb, 255));
    QVERIFY(q253In(c, kPadRb, 255));
    QTRY_VERIFY_WITH_TIMEOUT(run->isArmed(), 2000);
    QCOMPARE(run->armedUniverse(), quint32(3));
    QVERIFY(q253In(c, kPadRb, 0));
    QVERIFY(q253In(c, kPadLb, 0));
    QVERIFY(q253In(c, kPadLt, 255));
    QTRY_VERIFY_WITH_TIMEOUT(heard.contains(QStringLiteral("u3:ch2=255")), 2000);

    // The first pad goes: C moves to line 1, still universe 3's.
    m_fake->unlist(paths.at(0));
    q253Scan(name, 2);
    QCOMPARE(q253Line(c), 1);
    InputOutputMap *iom = m_doc->inputOutputMap();
    QCOMPARE(iom->inputPatch(3)->input(), quint32(1));
    QCOMPARE(iom->inputPatch(2)->input(), quint32(0));
    QVERIFY2(iom->inputPatch(1) == nullptr || iom->inputPatch(1)->input() == QLCIOPlugin::invalidLine(),
             "the gone pad's universe took another pad's line");
    QVERIFY2(run->isArmed() && run->armedUniverse() == 3,
             qPrintable(QString("another pad's drop disarmed the runner: '%1'").arg(run->notice())));
    // Its LT, held throughout, is still the safety: A plays.
    QVERIFY(q253In(c, kPadA, 255));
    QVERIFY(q253In(c, kPadA, 0));
    QTRY_VERIFY2_WITH_TIMEOUT(commandCount(int(ShowControl::Go)) == 1,
                              qPrintable(QString("C's LT + A after another pad's drop: '%1'").arg(run->notice())), 2000);

    // Three of the four held; the second pad goes; the fourth stops.
    heard.clear();
    QVERIFY(q253In(c, kPadLb, 255));
    QVERIFY(q253In(c, kPadRb, 255));
    QTRY_VERIFY_WITH_TIMEOUT(heard.contains(QStringLiteral("u3:ch10=255")) && heard.contains(QStringLiteral("u3:ch11=255")), 2000);
    m_fake->unlist(paths.at(1));
    q253Scan(name, 1);
    QCOMPARE(q253Line(c), 0);
    QCOMPARE(iom->inputPatch(3)->input(), quint32(0));
    QVERIFY(iom->inputPatch(2) == nullptr || iom->inputPatch(2)->input() == QLCIOPlugin::invalidLine());
    QCOMPARE(commandCount(int(ShowControl::AllOff)), 0);
    QVERIFY(q253In(c, kPadRt, 255));
    QTRY_VERIFY2_WITH_TIMEOUT(commandCount(int(ShowControl::AllOff)) == 1,
                              "a stop squeezed three of four across another pad's drop did not fire", 2000);
    for (quint32 ch : { kPadRt, kPadLt, kPadRb, kPadLb })
        QVERIFY(q253In(c, ch, 0));
}

void PadTraces_Test::aPadReenumeratedKeepsItsUniverse_data()
{
    QTest::addColumn<bool>("located");
    QTest::newRow("macOS-like: each pad its USB location") << true;
    QTest::newRow("Linux-like: no location, one name") << false;
}

void PadTraces_Test::aPadReenumeratedKeepsItsUniverse()
{
    /* Two pads of one name; A runs cues. B re-enumerates where it is (a
       power cycle, a sleep: the same path, another device): universe 2
       stays on B's line -- its new device, its stop live -- and never
       takes A's; A keeps its arm (round-5 review). */
    QFETCH(bool, located);
    const QString name = QStringLiteral("Xbox Wireless Controller");
    const QString a = located ? QStringLiteral("USB_045e_028e_1000000") : QStringLiteral("/dev/input/js0");
    const QString b = located ? QStringLiteral("USB_045e_028e_2000000") : QStringLiteral("/dev/input/js1");
    m_fake->list(a, 0x045e, 0x028e, name, 0, 1);
    q253Scan(name, 1);
    m_fake->list(b, 0x045e, 0x028e, name, 0, 2);
    q253Scan(name, 2);
    QCOMPARE(patchPads(name), 2);
    startRunning();
    PadRun *run = m_doc->programmer()->padRun();
    QVERIFY(q253In(a, kPadLb, 255));
    QVERIFY(q253In(a, kPadRb, 255));
    QTRY_VERIFY_WITH_TIMEOUT(run->isArmed(), 2000);
    QVERIFY(q253In(a, kPadRb, 0));
    QVERIFY(q253In(a, kPadLb, 0));
    // B re-enumerates at its path.
    m_fake->unlist(b);
    m_fake->list(b, 0x045e, 0x028e, name, 0, 3);
    QMetaObject::invokeMethod(m_hid, "rescanDevices", Qt::DirectConnection);
    QTRY_COMPARE_WITH_TIMEOUT(m_fake->opens(b), 2, 3000);
    QTest::qWait(150);
    InputOutputMap *iom = m_doc->inputOutputMap();
    QCOMPARE(q253Line(a), 0);
    QCOMPARE(q253Line(b), 1);
    QCOMPARE(iom->inputPatch(1)->input(), quint32(0));
    QVERIFY2(iom->inputPatch(2) != nullptr && iom->inputPatch(2)->input() == 1,
             qPrintable(QString("universe 2 went to line %1, not B's own")
                        .arg(iom->inputPatch(2) != nullptr ? int(iom->inputPatch(2)->input()) : -1)));
    QVERIFY2(run->isArmed() && run->armedUniverse() == 1, qPrintable(run->notice()));
    // B's stop, on its own universe, stops.
    for (quint32 ch : { kPadLb, kPadRb, kPadLt, kPadRt })
        QVERIFY(q253In(b, ch, 255));
    QTRY_COMPARE_WITH_TIMEOUT(commandCount(int(ShowControl::AllOff)), 1, 2000);
    for (quint32 ch : { kPadRt, kPadLt, kPadRb, kPadLb })
        QVERIFY(q253In(b, ch, 0));
}

void PadTraces_Test::aPadUnpluggedAndBackTakesNoOtherPad()
{
    /* Two pads of one name; A goes: its universe waits offline, B's
       universe follows B onto line 0 and B (now the runner) stays armed.
       A comes back (after B in the list): its universe reopens on A, not
       on B (round-5 review). */
    const QString name = QStringLiteral("Xbox Wireless Controller");
    const QString a = QStringLiteral("/dev/input/js0"), b = QStringLiteral("/dev/input/js1");
    m_fake->list(a, 0x045e, 0x028e, name, 0, 1);
    q253Scan(name, 1);
    m_fake->list(b, 0x045e, 0x028e, name, 0, 2);
    q253Scan(name, 2);
    QCOMPARE(patchPads(name), 2);
    startRunning();
    PadRun *run = m_doc->programmer()->padRun();
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(q253In(b, kPadLb, 255));
    QVERIFY(q253In(b, kPadRb, 255));
    QTRY_VERIFY_WITH_TIMEOUT(run->isArmed(), 2000);
    QCOMPARE(run->armedUniverse(), quint32(2));
    QVERIFY(q253In(b, kPadRb, 0));
    QVERIFY(q253In(b, kPadLb, 0));
    m_fake->unlist(a);
    q253Scan(name, 1);
    QCOMPARE(q253Line(b), 0);
    QCOMPARE(iom->inputPatch(2)->input(), quint32(0));
    QVERIFY(iom->inputPatch(1) == nullptr || iom->inputPatch(1)->input() == QLCIOPlugin::invalidLine());
    QVERIFY2(run->isArmed() && run->armedUniverse() == 2, qPrintable(run->notice()));
    // A is back.
    m_fake->list(a, 0x045e, 0x028e, name, 0, 4);
    q253Scan(name, 2);
    QCOMPARE(q253Line(a), 1);
    QTRY_VERIFY_WITH_TIMEOUT(iom->inputPatch(1) != nullptr && iom->inputPatch(1)->input() == 1, 3000);
    QCOMPARE(iom->inputPatch(2)->input(), quint32(0));
    QVERIFY2(run->isArmed() && run->armedUniverse() == 2, qPrintable(run->notice()));
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
    QVERIFY(m_player->playUntil([](const PadTracePlayer::Event &e) { return e.t >= 1600; }));
    QTRY_VERIFY_WITH_TIMEOUT(run->isArmed(), 2000);

    // Three presses of A, no LT.
    m_player->playAll();
    QTest::qWait(100);
    VERIFY_CLEAN_PLAY();
    QVERIFY(run->isArmed());
    QVERIFY2(m_commands.isEmpty(), qPrintable(QString("a resting trigger moved the show: %1 commands").arg(m_commands.size())));
}

void PadTraces_Test::triggerRestDuringTheChord()
{
    /* What the first version of the trace above found: the triggers keep
       reporting their rest (a pad sends what changed, and resting triggers
       jitter) while LB + RB are held. The chord fired once; the next trigger
       report restarts it (PadRun::armChordUpdate() starts the timer again
       when it is not running, and the fired chord left it stopped), and it
       fires again -- the arm flaps on and off. Rule 3: only a fresh chord
       changes the arm. Fixed in padrun.cpp in 0.4.12 (after a chord fires or
       is refused, nothing fires again until both bumpers are released). */
    QString error;
    QVERIFY2(m_player->load(trace("trigger_rest_in_chord.jsonl"), &error), qPrintable(error));
    PadRun *run = m_doc->programmer()->padRun();

    QVERIFY(m_player->step() != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(m_hid->inputs().contains(kDualSense), 2000);
    QCOMPARE(patchPads(kDualSense), 1);
    startRunning();

    QSignalSpy changes(run, &PadRun::armedChanged);
    QVERIFY(m_player->playUntil([](const PadTracePlayer::Event &e) { return e.t >= 1600; }));
    // The chord fires once (a slow timer is late, not absent) and only once.
    QTRY_VERIFY_WITH_TIMEOUT(changes.count() >= 1, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!run->isArmChordPending(), 3000);
    QCOMPARE(int(changes.count()), 1);
    m_player->playAll();
    QCOMPARE(int(changes.count()), 1);  // the rest of the hold changes nothing
    QVERIFY(m_commands.isEmpty());      // and nothing fired meanwhile
    VERIFY_CLEAN_PLAY();
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

void PadTraces_Test::theIdentitiesAreTheEngines()
{
    /* The recorder mirrors ControllerJobs::identityFor() (the plugin does not
       link the engine): every branch of it, so the two cannot drift. */
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath("identities.jsonl");
    {
        QFile f(file);
        QVERIFY(f.open(QIODevice::WriteOnly));
        // a Bluetooth address in the name; a port; neither (no location in the path).
        f.write("{\"t\":0,\"ev\":\"arrived\",\"path\":\"Bluetooth_054c_0ce6_1234\",\"vid\":\"054c\",\"pid\":\"0ce6\","
                "\"name\":\"Wireless Controller 12:34:56:AB:CD:EF\"}\n"
                "{\"t\":10,\"ev\":\"arrived\",\"path\":\"USB_045e_028e_2124000\",\"vid\":\"045e\",\"pid\":\"028e\","
                "\"name\":\"Xbox 360 Controller for Windows\"}\n"
                "{\"t\":20,\"ev\":\"arrived\",\"path\":\"hidraw7\",\"vid\":\"045e\",\"pid\":\"0b13\","
                "\"name\":\"Some Pad\"}\n");
    }
    PadTracePlayer one(m_hid, m_fake);
    QString error;
    QVERIFY2(one.load(file, &error), qPrintable(error));
    one.setSpeed(0);
    one.playAll();
    QVERIFY2(one.problems().isEmpty(), qPrintable(one.problems().join("; ")));
    QTRY_COMPARE_WITH_TIMEOUT(m_hid->inputs().size(), 3, 2000);
    teardown();

    const QStringList files = QDir(m_recordDir->path()).entryList(QStringList() << "pads-*.jsonl", QDir::Files);
    QCOMPARE(files.size(), 1);
    PadTracePlayer back(nullptr, nullptr);
    QVERIFY2(back.load(m_recordDir->filePath(files.first()), &error), qPrintable(error));
    QHash<QString, QString> identity;
    for (const PadTracePlayer::Event &e : back.events())
    {
        if (e.ev != QLatin1String("arrived"))
            continue;
        const QString name = e.o.value("name").toString();
        const QString got = e.o.value("identity").toString();
        QCOMPARE(got, ControllerJobs::identityFor(QStringLiteral("HID"), name, e.o.value("location").toString()));
        identity.insert(name, got);
    }
    QCOMPARE(identity.size(), 3);
    QCOMPARE(identity.value("Wireless Controller 12:34:56:AB:CD:EF"), QString("mac:12:34:56:ab:cd:ef"));
    QVERIFY(identity.value("Xbox 360 Controller for Windows").startsWith("port:HID:"));
    QCOMPARE(identity.value("Some Pad"), QString("name:HID:Some Pad"));
}

void PadTraces_Test::aLongRecordingStopsAtItsCap()
{
    /* A file past its cap (here 1500 bytes) ends with a "truncated" event,
       and keeps nothing after it. */
    QString error;
    QVERIFY2(m_player->load(trace("clone_flip_to_xbox.jsonl"), &error), qPrintable(error));
    m_player->setSpeed(0);
    QVERIFY(m_player->step() != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(m_hid->inputs().contains(kDualSense), 2000);
    QCOMPARE(patchPads(kDualSense), 1);
    startRunning();
    m_player->playAll();
    QTest::qWait(100);
    teardown();

    const QStringList files = QDir(m_recordDir->path()).entryList(QStringList() << "pads-*.jsonl", QDir::Files);
    QCOMPARE(files.size(), 1);
    QFile f(m_recordDir->filePath(files.first()));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = f.readAll().trimmed().split('\n');
    QVERIFY(lines.size() > 3);
    QVERIFY2(lines.last().contains("\"ev\":\"truncated\""), lines.last().constData());
    QVERIFY2(f.size() < 4000, qPrintable(QString::number(f.size())));   // far short of the full trace
    for (const QByteArray &l : lines.mid(0, lines.size() - 1))
        QVERIFY(!l.contains("truncated"));
}

QTEST_MAIN(PadTraces_Test)
