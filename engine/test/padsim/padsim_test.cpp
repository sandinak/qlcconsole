/*
  Q Light Controller Plus
  padsim_test.cpp

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

#include <QtTest>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "padsim_test.h"
#include "simpad.h"
#include "surfacescenarios.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "controllerjobs.h"
#include "ioplugincache.h"
#include "showcontrol.h"
#include "mastertimer.h"
#include "functionparent.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "function.h"
#include "padrun.h"
#include "doc.h"

namespace
{
/* Never deleted: a plugin thread may still be leaving a call into one. */
QList<FakeHIDBackend *> s_fakes;
}

void PadSim_Test::initTestCase()
{
    m_pluginDir = new QTemporaryDir();
    QVERIFY(m_pluginDir->isValid());
    const QFileInfo hid(QStringLiteral(QLC_HID_PLUGIN_FILE));
    QVERIFY2(hid.exists(), QLC_HID_PLUGIN_FILE);
    QVERIFY(QFile::copy(hid.absoluteFilePath(), m_pluginDir->filePath(hid.fileName())));
    qputenv("QLC_HID_PROFILE_DIR", QByteArray(QLC_TEST_RESOURCE_ROOT) + "hidprofiles");
    qputenv("QLC_HID_VIRTUAL_ONLY", "1");      // never this machine's own pads
}

void PadSim_Test::cleanupTestCase()
{
    delete m_pluginDir;
    m_pluginDir = nullptr;
}

void PadSim_Test::init()
{
    QSettings().remove(ControllerJobs::settingsGroup());    // every pad every job
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
    QMetaObject::invokeMethod(m_hid, "setTestBackend", Qt::DirectConnection, Q_ARG(quintptr, quintptr(m_fake)));
}

void PadSim_Test::cleanup()
{
    if (m_doc != nullptr)
    {
        m_doc->programmer()->padRun()->setEnabled(false, false);
        m_doc->showControl()->setRunHandler(ShowControl::RunHandler());
        for (Function *f : m_doc->functions())
            f->stop(FunctionParent::master());
        m_doc->masterTimer()->timerTick();
        m_doc->setMode(Doc::Design);
    }
    if (m_fake != nullptr)
        m_fake->release();
    QTRY_COMPARE_WITH_TIMEOUT(m_fake->callsInProgress(), 0, 2000);
    delete m_doc;
    m_doc = nullptr;
    m_hid = nullptr;
    QSettings().remove(ControllerJobs::settingsGroup());
}

void PadSim_Test::makeShow()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Sim Dimmer");
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
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Dimmer %1").arg(i + 1));
        fxi->setFixtureDefinition(def, mode);
        fxi->setUniverse(0);
        fxi->setAddress(quint32(i));
        m_doc->addFixture(fxi);
        dimmers << fxi->id();
    }
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer);
    full->setName("Full");
    full->setValue(255);
    m_doc->addPalette(full);

    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Spring");
    m_list = st->addCueList(prod, "Main", ShowTree::ListMode::NonTracking);
    m_cue1 = st->addCue(m_list, -1, "Opening");
    m_cue2 = st->addCue(m_list, 0, "Ballad");
    ShowTree::Target t;
    t.fixtures = { dimmers.first() };
    for (quint32 cue : { m_cue1, m_cue2 })
        QVERIFY(st->setLayerLook(cue, st->addLayer(cue, t, "Front"), st->lookForPalette(full->id())));
    ShowTree::Target h;
    h.fixtures = { dimmers.last() };
    auto addHit = [&](ShowTree::HitScope scope, quint32 place, const char *name) {
        const quint32 id = st->addHit(scope, place, QString::fromUtf8(name));
        if (id == ShowTree::invalidId()
            || !st->setLayerLook(id, st->addLayer(id, h, QString::fromUtf8(name)), st->lookForPalette(full->id())))
            return ShowTree::invalidId();
        return id;
    };
    // By place: the production's hit is live all show; the Ballad's only while it plays.
    m_breathe = addHit(ShowTree::HitScope::Production, prod, "Breathe");
    m_strobe = addHit(ShowTree::HitScope::Cue, m_cue2, "Strobe");
    QVERIFY(m_breathe != ShowTree::invalidId());
    QVERIFY(m_strobe != ShowTree::invalidId());
}

void PadSim_Test::eachReportReadsAsThePluginReadsIt_data()
{
    QTest::addColumn<int>("model");
    QTest::newRow("DualSense") << int(SimPad::DualSense);
    QTest::newRow("DualShock 4") << int(SimPad::DualShock4);
    QTest::newRow("Xbox (GIP)") << int(SimPad::XboxUsb);
}

void PadSim_Test::eachReportReadsAsThePluginReadsIt()
{
    QFETCH(int, model);
    SimPad pad(m_doc, m_hid, m_fake, SimPad::Model(model));
    QVERIFY2(pad.plug(), qPrintable(pad.lastError()));
    // The simulator and the plugin agree on every role's channel.
    const QStringList mismatches = pad.roleMismatches();
    QVERIFY2(mismatches.isEmpty(), qPrintable(mismatches.join(QStringLiteral("\n"))));

    InputOutputMap *iom = m_doc->inputOutputMap();
    QSignalSpy in(iom, &InputOutputMap::inputValueChanged);
    QStringList problems;
    const QStringList buttons = { "a", "b", "x", "y", "lb", "rb", "back", "start", "ls", "rs", "guide",
                                  "dpad_up", "dpad_down", "dpad_left", "dpad_right" };
    for (const QString &role : buttons)
    {
        if (SimPad::Model(model) == SimPad::XboxUsb && role == QLatin1String("guide"))
            continue;   // the Xbox button is a report of its own (0x07)
        in.clear();
        if (!pad.press(role))
        {
            problems << role + QStringLiteral(": ") + pad.lastError();
            continue;
        }
        const int ch = pad.channelOf(role);
        bool heard = false, alone = true;
        for (const QList<QVariant> &a : std::as_const(in))
        {
            if (a.at(2).toInt() == 0)
                continue;
            if (a.at(1).toInt() == ch && a.at(2).toInt() == 255)
                heard = true;
            else if (a.at(1).toInt() >= GamepadLayout::AxisCount)
                alone = false;      // another button down with it
        }
        if (!heard)
            problems << QStringLiteral("%1: its report never reached channel %2").arg(role).arg(ch);
        if (!alone)
            problems << QStringLiteral("%1: its report pressed another button too").arg(role);
        pad.release(role);
    }
    // A trigger pulled all the way, a stick pushed right: their axes, full travel.
    for (const QString &axis : { QStringLiteral("rt"), QStringLiteral("lt") })
    {
        in.clear();
        pad.set(axis, 255);
        bool full = false;
        for (const QList<QVariant> &a : std::as_const(in))
            if (a.at(1).toInt() == pad.channelOf(axis) && a.at(2).toInt() >= 250)
                full = true;
        if (!full)
            problems << axis + QStringLiteral(": a full pull did not reach its axis");
        pad.set(axis, 0);
    }
    in.clear();
    pad.set(QStringLiteral("lx"), 255);
    bool right = false;
    for (const QList<QVariant> &a : std::as_const(in))
        if (a.at(1).toInt() == pad.channelOf(QStringLiteral("lx")) && a.at(2).toInt() >= 250)
            right = true;
    if (!right)
        problems << QStringLiteral("lx: pushed right did not reach its axis");
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QStringLiteral("\n"))));
}

void PadSim_Test::dualSenseR2FiresTheHitByPlace()
{
    makeShow();
    SimPad pad(m_doc, m_hid, m_fake, SimPad::DualSense);
    QVERIFY2(pad.plug(), qPrintable(pad.lastError()));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    run->setArmChordMs(15);
    m_doc->setMode(Doc::Operate);
    pad.takeFeedback();

    // L1 + R1 together: armed. The pad's light says so.
    QVERIFY(pad.press(QStringLiteral("l1")));
    QVERIFY(pad.press(QStringLiteral("r1")));
    QTRY_VERIFY(run->isArmed());
    QVERIFY(pad.release(QStringLiteral("r1")));
    QVERIFY(pad.release(QStringLiteral("l1")));
    QVERIFY(run->isArmed());
    QTRY_COMPARE(pad.lastLight(), PadRun::armedColour().name());

    // The production's hit is armed; the Ballad's is not (the Ballad is not playing).
    QCOMPARE(run->armedHits(), QList<quint32>({ m_breathe }));
    QSignalSpy fired(run, &PadRun::hitFired);
    QVERIFY(pad.press(QStringLiteral("r2")));
    QVERIFY(pad.release(QStringLiteral("r2")));
    QTRY_COMPARE(fired.count(), 1);
    QCOMPARE(fired.at(0).at(0).toUInt(), m_breathe);
    QVERIFY(fired.at(0).at(1).toBool());
    Function *f = m_doc->function(m_doc->showTree()->lookFunction(m_breathe));
    QVERIFY(f != nullptr);
    m_doc->masterTimer()->timerTick();
    QVERIFY(f->isRunning());
    // A hit is not a run command: the GO path saw nothing.
    QVERIFY(m_commands.isEmpty());

    // Pulled again: let go.
    QVERIFY(pad.press(QStringLiteral("r2")));
    QVERIFY(pad.release(QStringLiteral("r2")));
    QTRY_COMPARE(fired.count(), 2);
    QVERIFY(!fired.at(1).at(1).toBool());
}

void PadSim_Test::dualSenseEStopChord()
{
    makeShow();
    SimPad pad(m_doc, m_hid, m_fake, SimPad::DualSense);
    QVERIFY2(pad.plug(), qPrintable(pad.lastError()));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    run->setArmChordMs(15);
    m_doc->setMode(Doc::Operate);
    QVERIFY(pad.chord({ QStringLiteral("l1"), QStringLiteral("r1") }, 30));
    QTRY_VERIFY(run->isArmed());

    // All four, as a hand closes on them: bumpers, then both triggers.
    for (const char *c : { "l1", "r1", "l2", "r2" })
        QVERIFY(pad.press(QString::fromLatin1(c)));
    QTRY_COMPARE(int(m_commands.count(int(ShowControl::AllOff))), 1);
    for (const char *c : { "r2", "l2", "r1", "l1" })
        QVERIFY(pad.release(QString::fromLatin1(c)));
    pad.wait(50);
    // One stop, not one per control let go; disarmed; the pad red.
    QCOMPARE(int(m_commands.count(int(ShowControl::AllOff))), 1);
    QCOMPARE(m_commands.size(), 1);
    QVERIFY(!run->isArmed());
    QTRY_COMPARE(pad.lastLight(), PadRun::stoppedColour().name());
}

void PadSim_Test::stickDriftMovesNothing()
{
    makeShow();
    SimPad pad(m_doc, m_hid, m_fake, SimPad::DualSense);
    QVERIFY2(pad.plug(), qPrintable(pad.lastError()));
    PadRun *run = m_doc->programmer()->padRun();
    run->setEnabled(true);
    m_doc->setMode(Doc::Operate);
    const quint32 before = m_doc->showTree()->editedCue();
    QVERIFY(pad.drift(QStringLiteral("lx"), 9));
    QVERIFY(pad.drift(QStringLiteral("ry"), -7));
    pad.wait(30);
    QVERIFY(m_commands.isEmpty());
    QVERIFY(!run->isArmed());
    QVERIFY(!run->isWalking());
    QCOMPARE(m_doc->showTree()->editedCue(), before);
}

QTEST_MAIN(PadSim_Test)
