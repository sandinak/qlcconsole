/*
  Q Light Controller Plus
  surfacesim_test.cpp

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
#include <QSignalSpy>
#include <QSettings>
#include <QDir>

#include "surfacesim_test.h"
#include "simmididevice.h"
#include "simopendeck.h"
#include "surfacesetup.h"
#include "opendecklink.h"
#include "controllerkeepalive.h"
#include "programmercontroller.h"
#include "surfacescenarios.h"
#include "surfacescript.h"
#include "gamepadlayout.h"
#include "hidprofile.h"
#include "controlmap.h"
#include "deskrecord.h"
#include "showtreecontroller.h"
#include "outputrulekeeper.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "showcontrol.h"
#include "mastertimer.h"
#include "functionparent.h"
#include "fixture.h"
#include "scene.h"
#include "qlcfile.h"
#include "doc.h"

namespace
{
const QString kPmj = QStringLiteral("PMJ Black 1");
const quint32 kGo = 32896, kBack = 32907, kMaster = 32768, kFavorites = 32944;

QString profileDir()
{
    return QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("inputprofiles/");
}

QString pmjFile()
{
    return profileDir() + QStringLiteral("PMJ-Black-1.qxi");
}

/* The profile in @p file, as the map holds it (loaded into it if not). */
QLCInputProfile *mapProfile(Doc *doc, const QString &file)
{
    QLCInputProfile *p = QLCInputProfile::loader(file);
    if (p == nullptr)
        return nullptr;
    InputOutputMap *iom = doc->inputOutputMap();
    if (QLCInputProfile *have = iom->profile(p->name()))
    {
        delete p;
        return have;
    }
    if (!iom->addProfile(p))
    {
        delete p;
        return nullptr;
    }
    return p;
}

/* A control with a <Feedback> (QLCInputChannel::saveXML()'s rule). */
bool hasLamp(const QLCInputChannel *ich)
{
    return ich->type() == QLCInputChannel::Button && (ich->lowerValue() != 0 || ich->upperValue() != UCHAR_MAX);
}

QString hex(const QByteArray &b)
{
    return QString::fromLatin1(b.toHex(' '));
}

/* A cue list of two cues on one dimmer, the Run tab's (its GO has a cue to play). */
quint32 runList(Doc *doc)
{
    Fixture *fxi = new Fixture(doc);
    fxi->setName(QStringLiteral("Dim"));
    fxi->setAddress(0);
    fxi->setUniverse(0);
    fxi->setChannels(1);
    doc->addFixture(fxi);
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName(QStringLiteral("All"));
    doc->addFixtureGroup(g);
    g->assignFixture(fxi->id());
    QLCPalette *full = new QLCPalette(QLCPalette::Dimmer, doc);
    full->setName(QStringLiteral("Full"));
    full->setValue(255);
    doc->addPalette(full);
    ShowTreeController *st = doc->showTree();
    const quint32 prod = st->addProduction(QStringLiteral("Show"));
    const quint32 list = st->addCueList(prod, QStringLiteral("Main"));
    for (int i = 0; i < 2; ++i)
    {
        ShowTree::Target t;
        t.groups << g->id();
        const quint32 c = st->addCue(list, i - 1, QStringLiteral("Cue %1").arg(i + 1));
        st->setLayerLook(c, st->addLayer(c, t), st->lookForPalette(full->id()));
    }
    doc->controlMap()->setRunTabList(list);
    return list;
}

/* This computer's desk role @p role on @p dev's @p channel. */
bool deskOn(Doc *doc, DeskRecord::Role role, quint32 universe, const QString &device, quint32 channel)
{
    DeskRecord *desk = doc->deskRecord();
    DeskRecord::Binding b;
    b.identity = desk->identityOfUniverse(universe);
    b.device = device;
    b.plugin = QStringLiteral("MIDI");
    b.channel = channel;
    b.control = QStringLiteral("#%1").arg(channel);
    return desk->setBinding(role, b) && desk->isLive(role);
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

/* The HID plugin's profile describing the same pad as a .qxi, by vendor. */
QString hidProfileFor(const QLCInputProfile *p)
{
    const QString m = p->manufacturer().toLower();
    if (m.contains(QLatin1String("sony")))
        return QStringLiteral("playstation.qxhid");
    if (m.contains(QLatin1String("microsoft")))
        return QStringLiteral("xbox.qxhid");
    return QString();
}
}

void SurfaceSim_Test::initTestCase()
{
    // This computer's desk is kept in the settings: the test's own, never the user's.
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
    QVERIFY(m_backups.isValid());
    SurfaceSetup::setBackupDir(m_backups.path());
}

void SurfaceSim_Test::init()
{
    QSettings().remove(DeskRecord::settingsGroup());
    m_doc = new Doc(this);
    QDir dir(QStringLiteral("../iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    m_stub->m_feedback.clear();
    m_stub->m_feedbackParams.clear();
    m_stub->m_feedbackLines.clear();
    m_stub->m_lineNames.clear();
}

void SurfaceSim_Test::cleanup()
{
    m_doc->showControl()->setRunHandler(ShowControl::RunHandler());
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    m_doc->masterTimer()->timerTick();
    QCoreApplication::processEvents();
    m_stub->m_lineNames.clear();
    m_stub->m_linesAreHardware = false;
    m_stub->m_networkDmx = true;
    m_stub->m_showControlSent.clear();
    m_stub->m_onShowControl = nullptr;
    m_stub->m_onRescan = nullptr;
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
    QSettings().remove(DeskRecord::settingsGroup());
    QSettings().remove(QStringLiteral("surfacesetup"));
}

void SurfaceSim_Test::theBoardSpeaksMidi()
{
    QVERIFY(mapProfile(m_doc, pmjFile()) != nullptr);
    InputOutputMap *iom = m_doc->inputOutputMap();
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj);
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    QCOMPARE(pmj.line(), 0);
    QCOMPARE(iom->inputPatch(0)->profileName(), kPmj);

    // The PMJ's own messages: MIDI channel 9, Go is note 0, Master is CC 0.
    QCOMPARE(hex(SimMidiDevice::bytesFor(kGo, 127)), QStringLiteral("98 00 7f"));
    QCOMPARE(hex(SimMidiDevice::bytesFor(kGo, 0)), QStringLiteral("88 00 00"));
    QCOMPARE(hex(SimMidiDevice::bytesFor(kMaster, 64)), QStringLiteral("b8 00 40"));
    QCOMPARE(hex(SimMidiDevice::bytesFor(kBack, 127)), QStringLiteral("98 0b 7f"));

    QSignalSpy in(iom, &InputOutputMap::inputValueChanged);
    QVERIFY(pmj.noteOn(9, 0, 127));
    QCOMPARE(pmj.decoded().last(), qMakePair(kGo, uchar(255)));
    QVERIFY(pmj.noteOff(9, 0));
    QCOMPARE(pmj.decoded().last(), qMakePair(kGo, uchar(0)));
    QVERIFY(pmj.cc(9, 0, 64));
    QCOMPARE(pmj.decoded().last(), qMakePair(kMaster, uchar(128)));
    // Through the patch and its flush, where the app listens: universe 1.
    QList<QPair<quint32, int>> heard;
    for (const QList<QVariant> &a : std::as_const(in))
        if (a.at(0).toUInt() == 0)
            heard << qMakePair(a.at(1).toUInt(), a.at(2).toInt());
    QVERIFY2(heard.contains(qMakePair(kGo, 255)) && heard.contains(qMakePair(kGo, 0))
             && heard.contains(qMakePair(kMaster, 128)), qPrintable(QString::number(heard.size())));

    // A note on the wrong MIDI channel is another control: not Go.
    QVERIFY(pmj.noteOn(1, 0, 127));
    QCOMPARE(pmj.decoded().last().first, quint32(128));

    // By name, in the profile's words; a control it does not have fails here.
    QVERIFY(pmj.press(QStringLiteral("Go")));
    QCOMPARE(pmj.decoded().last(), qMakePair(kGo, uchar(255)));
    QVERIFY(pmj.release(QStringLiteral("go")));
    QVERIFY(!pmj.press(QStringLiteral("Gp")));
    QVERIFY2(pmj.lastError().contains(QStringLiteral("'Gp'")), qPrintable(pmj.lastError()));
    QVERIFY(!pmj.raw(QByteArray::fromHex("00 7f")));      // no status byte

    // Out: what MidiPlugin writes for the app's feedback, and the lamp it lights.
    pmj.takeOutput();
    QVERIFY(iom->sendFeedBack(0, kGo, 254, QVariant()));
    const QList<SimMidiDevice::Out> out = pmj.takeOutput();
    QCOMPARE(out.size(), 1);
    QCOMPARE(hex(out.first().midi), QStringLiteral("98 00 7f"));
    QCOMPARE(pmj.lamp(QStringLiteral("Go")), 127);
    QVERIFY(iom->sendFeedBack(0, kGo, 0, QVariant()));
    QCOMPARE(pmj.lamp(kGo), 0);
    QCOMPARE(pmj.lamp(QStringLiteral("Back")), -1);      // never sent one

    // Unplugged: the system no longer lists it, and its patch waits offline.
    QVERIFY(pmj.unplug());
    QVERIFY(iom->inputPatch(0) == nullptr);
    QVERIFY(!pmj.press(QStringLiteral("Go")));
    QVERIFY(pmj.replug());
    QTRY_VERIFY(iom->inputPatch(0) != nullptr);
    QCOMPARE(iom->inputPatch(0)->profileName(), kPmj);
}

void SurfaceSim_Test::aScriptPlaysAsThePersonDidIt()
{
    QVERIFY(mapProfile(m_doc, pmjFile()) != nullptr);
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj);
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    pmj.setTimeScale(0);    // as fast as the machine is

    SurfaceScript s;
    QVERIFY2(s.parse("# Go tapped, then the Master up\n"
                     "{\"ev\":\"tap\",\"control\":\"Go\",\"hold_ms\":120}\n"
                     "{\"ev\":\"ramp\",\"control\":\"Master\",\"from\":0,\"to\":127,\"ms\":20}\n"),
             qPrintable(s.error()));
    QCOMPARE(s.size(), 2);
    QVERIFY2(s.play(&pmj), qPrintable(s.error()));
    const QList<QPair<quint32, uchar>> want = {
        { kGo, 255 }, { kGo, 0 }, { kMaster, 0 }, { kMaster, 126 }, { kMaster, 255 } };
    QCOMPARE(pmj.decoded(), want);

    // A capture of the bench (tools/surface-capture.py), replayed byte for byte.
    SurfaceScript cap;
    QVERIFY2(cap.load(QStringLiteral(SURFACESIM_SRC_DIR "/captures/pmj-back-favorites.jsonl")),
             qPrintable(cap.error()));
    const int before = int(pmj.decoded().size());
    QVERIFY2(cap.play(&pmj), qPrintable(cap.error()));
    const QList<QPair<quint32, uchar>> played = pmj.decoded().mid(before);
    QCOMPARE(played.size(), 4);
    QCOMPARE(played.at(0), qMakePair(kBack, uchar(255)));
    QCOMPARE(played.at(3), qMakePair(kFavorites, uchar(0)));

    // A bad line is named, not skipped.
    SurfaceScript bad;
    QVERIFY(!bad.parse("{\"ev\":\"tap\",\"control\":\"Go\"}\n{\"ev\":\"tapp\"}\n"));
    QVERIFY2(bad.error().startsWith(QStringLiteral("line 2")), qPrintable(bad.error()));
    // A control the board does not have fails the play at its line.
    SurfaceScript typo;
    QVERIFY(typo.parse("{\"ev\":\"press\",\"control\":\"Goo\"}\n"));
    QVERIFY(!typo.play(&pmj));
    QVERIFY2(typo.error().contains(QStringLiteral("'Goo'")), qPrintable(typo.error()));
}

void SurfaceSim_Test::everyProfileControlReachesTheApp_data()
{
    QTest::addColumn<QString>("file");
    QDir dir(profileDir(), QStringLiteral("*.qxi"), QDir::Name, QDir::Files);
    const QStringList files = dir.entryList();
    // Not a silent pass on an empty or missing folder.
    QVERIFY2(files.size() >= 40, qPrintable(QStringLiteral("%1 profiles in %2").arg(files.size()).arg(dir.path())));
    for (const QString &f : files)
        QTest::newRow(qPrintable(f)) << dir.filePath(f);
}

void SurfaceSim_Test::everyProfileControlReachesTheApp()
{
    QFETCH(QString, file);
    QLCInputProfile *p = mapProfile(m_doc, file);
    QVERIFY2(p != nullptr, qPrintable(file + QStringLiteral(" does not load")));
    QVERIFY2(!p->channels().isEmpty(), "a profile with no controls");
    InputOutputMap *iom = m_doc->inputOutputMap();
    QSignalSpy in(iom, &InputOutputMap::inputValueChanged);
    auto arrived = [&in](quint32 ch, int v) {
        for (const QList<QVariant> &a : std::as_const(in))
            if (a.at(0).toUInt() == 0 && a.at(1).toUInt() == ch && a.at(2).toInt() == v)
                return true;
        return false;
    };
    QStringList problems;
    const QMap<quint32, QLCInputChannel *> chans = p->channels();
    int lamps = 0;

    if (p->type() == QLCInputProfile::MIDI)
    {
        SimMidiDevice dev(m_doc, m_stub, p->name(), 0, p->name());
        dev.setTimeScale(0);
        QVERIFY2(dev.plug(), qPrintable(dev.lastError()));
        for (auto it = chans.constBegin(); it != chans.constEnd(); ++it)
        {
            const quint32 ch = it.key();
            const QString what = QStringLiteral("%1 '%2'").arg(ch).arg(it.value()->name());
            const QByteArray on = SimMidiDevice::bytesFor(ch, 127);
            if (on.isEmpty())
            {
                problems << what + QStringLiteral(": no MIDI message arrives on this channel");
                continue;
            }
            in.clear();
            if (!dev.raw(on))
            {
                problems << what + QStringLiteral(": ") + dev.lastError();
                continue;
            }
            const QPair<quint32, uchar> got = dev.decoded().last();
            if (got.first != ch)
            {
                problems << what + QStringLiteral(": the board's %1 arrives as channel %2").arg(hex(on)).arg(got.first);
                continue;
            }
            if (!arrived(ch, got.second))
                problems << what + QStringLiteral(": %1 never reached the app").arg(hex(on));
            // A note or a CC is let go: the release arrives too.
            if ((ch & 0x0FFF) <= CHANNEL_OFFSET_NOTE_MAX)
            {
                dev.raw(SimMidiDevice::bytesFor(ch, 0));
                if (!arrived(ch, 0))
                    problems << what + QStringLiteral(": its release never reached the app");
            }
            if (hasLamp(it.value()))
            {
                lamps++;
                dev.takeOutput();
                for (const uchar level : { it.value()->upperValue(), it.value()->lowerValue() })
                {
                    if (!iom->sendFeedBack(0, ch, level, QVariant()))
                    {
                        problems << what + QStringLiteral(": its lamp has no feedback line");
                        break;
                    }
                    const QList<SimMidiDevice::Out> out = dev.takeOutput();
                    if (out.size() != 1 || out.first().midi.size() < 2)
                    {
                        problems << what + QStringLiteral(": its lamp got %1 MIDI messages").arg(out.size());
                        break;
                    }
                    // The MIDI it gets addresses the control that has the lamp.
                    const QByteArray m = out.first().midi;
                    quint32 back = 0;
                    uchar v = 0;
                    if (!QLCMIDIProtocol::midiToInput(uchar(m.at(0)), uchar(m.at(1)), m.size() > 2 ? uchar(m.at(2)) : 0,
                                                      SimMidiDevice::kOmni, &back, &v)
                        || (back & 0x0FFF) != (ch & 0x0FFF))
                        problems << what + QStringLiteral(": its lamp (%1) lights channel %2").arg(hex(m)).arg(back);
                }
            }
        }
    }
    else
    {
        /* HID, OSC, the Enttec wings: the device's own plugin makes the
           channel numbers; from its signal on, the path is the same. */
        m_stub->m_linesAreHardware = true;
        m_stub->m_lineNames = QStringList{ p->name() };
        QVERIFY(iom->setInputPatch(0, m_stub->name(), p->name(), 0, p->name()));
        for (auto it = chans.constBegin(); it != chans.constEnd(); ++it)
        {
            in.clear();
            m_stub->emitValueChanged(UINT_MAX, 0, it.key(), 255);
            simFlushInputs(iom);
            m_stub->emitValueChanged(UINT_MAX, 0, it.key(), 0);
            simFlushInputs(iom);
            QCoreApplication::processEvents();
            if (!arrived(it.key(), 255) || !arrived(it.key(), 0))
                problems << QStringLiteral("%1 '%2': never reached the app").arg(it.key()).arg(it.value()->name());
        }
    }

    /* A pad's .qxi and the HID plugin's own profile (.qxhid) describe one
       pad: every control the plugin reads has its channel here, a stick or
       trigger is a slider, a button a button. */
    const QString qxhid = p->type() == QLCInputProfile::HID ? hidProfileFor(p) : QString();
    if (!qxhid.isEmpty())
    {
        QScopedPointer<HIDProfile> hp(HIDProfile::load(QStringLiteral(QLC_TEST_RESOURCE_ROOT) + "hidprofiles/" + qxhid));
        QVERIFY2(!hp.isNull(), qPrintable(qxhid));
        auto check = [&](quint32 ch, const QString &hidName, bool axis) {
            QLCInputChannel *ich = p->channel(ch);
            if (ich == nullptr)
                problems << QStringLiteral("%1 (%2 in %3): no channel %1 here").arg(ch).arg(hidName, qxhid);
            else if (axis != (ich->type() == QLCInputChannel::Slider))
                problems << QStringLiteral("%1 '%2': a %3 in %4, typed %5 here").arg(ch).arg(ich->name(),
                            axis ? QStringLiteral("stick or trigger") : QStringLiteral("button"), qxhid,
                            QLCInputChannel::typeToString(ich->type()));
        };
        for (int a = 0; a < hp->axisCount(); ++a)
            check(quint32(a), hp->axisName(a), true);
        // Past the buttons, a touchpad finger's place and touch: values, not buttons.
        for (int b = 0; b < qMin(hp->buttonCount(), int(GamepadLayout::ButtonCount)); ++b)
            if (!hp->buttonName(b).isEmpty())
                check(quint32(GamepadLayout::AxisCount + b), hp->buttonName(b), false);
    }

    if (lamps > 0)
        qInfo().noquote() << p->name() << ":" << chans.size() << "controls," << lamps << "lamps";
    QVERIFY2(problems.isEmpty(), qPrintable(p->name() + QStringLiteral(":\n  ") + problems.join(QStringLiteral("\n  "))));
}

void SurfaceSim_Test::aStateDumpAfterAReplugFiresNoAllOff()
{
    QVERIFY(mapProfile(m_doc, pmjFile()) != nullptr);
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj);
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    ControlMap *cm = m_doc->controlMap();
    cm->setStopDebounceMs(0);      // every press its own: the dump is what is judged
    // The stop learned on Favorites, the key the PMJ gives no job.
    cm->setBinding({ 0, kFavorites, ControlMap::emergencyStop(), ControlMap::Toggle });
    QList<int> commands;
    m_doc->showControl()->setRunHandler([&commands](const ShowControl::Request &r, ShowControl::Route) {
        commands << int(r.command);
        return true;
    });
    auto stops = [&commands]() { return int(commands.count(int(ShowControl::AllOff))); };

    // The path works: a press on the board is one ALL OFF.
    QVERIFY(pmj.tap(QStringLiteral("Favorites")));
    QTRY_COMPARE(stops(), 1);

    /* The cable out and back: the board reports the key at rest, then
       echoes its lamp as a note on (OpenDeck's state dump on connect) --
       nobody pressed anything. */
    QVERIFY2(SurfaceScenarios::unplugReplug(&pmj, { { QStringLiteral("Favorites"), 0 },
                                                    { QStringLiteral("Favorites"), 127 },
                                                    { QStringLiteral("Favorites"), 0 } }),
             qPrintable(pmj.lastError()));
    QTest::qWait(20);
    QCoreApplication::processEvents();
    QCOMPARE(stops(), 1);
}

/* Phase R on this computer's desk, through the board's own bytes: the
   PMJ's STOP and GO desk roles see OpenDeck's dump on reconnect (at rest,
   lamp echoed as a note on, at rest) and fire nothing; a release and a
   press 300 ms on is one GO. */
void SurfaceSim_Test::aStateDumpFiresNoDeskRole()
{
    QVERIFY(mapProfile(m_doc, pmjFile()) != nullptr);
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj);
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    runList(m_doc);
    QVERIFY(deskOn(m_doc, DeskRecord::Stop, 0, kPmj, kFavorites));
    QVERIFY(deskOn(m_doc, DeskRecord::Go, 0, kPmj, kGo));
    ControlMap *cm = m_doc->controlMap();
    cm->setDumpWindowMs(5000);         // never over while the test runs
    QList<int> commands;
    m_doc->showControl()->setRunHandler([&commands](const ShowControl::Request &r, ShowControl::Route) {
        commands << int(r.command);
        return true;
    });
    auto count = [&commands](ShowControl::Command c) { return int(commands.count(int(c))); };

    // The path works: a tap on the board is one ALL OFF, one GO.
    QVERIFY(pmj.tap(QStringLiteral("Favorites")));
    QVERIFY(pmj.tap(QStringLiteral("Go")));
    settle();
    QCOMPARE(count(ShowControl::AllOff), 1);
    QCOMPARE(count(ShowControl::Go), 1);

    // The cable out and back, and the board's dump of both keys.
    const QString fav = QStringLiteral("Favorites"), go = QStringLiteral("Go");
    QVERIFY2(SurfaceScenarios::unplugReplug(&pmj, { { fav, 0 }, { fav, 127 }, { fav, 0 },
                                                    { go, 0 }, { go, 127 }, { go, 0 } }),
             qPrintable(pmj.lastError()));
    settle();
    QVERIFY2(m_doc->deskRecord()->isLive(DeskRecord::Go), "the desk's GO is not live after the replug");
    QCOMPARE(count(ShowControl::AllOff), 1);
    QCOMPARE(count(ShowControl::Go), 1);
    // The other shape, a key dumped as pressed.
    QVERIFY2(SurfaceScenarios::unplugReplug(&pmj, { { go, 127 } }), qPrintable(pmj.lastError()));
    settle();
    QCOMPARE(count(ShowControl::Go), 1);

    // 300 ms on, a hand: a release and a press, one GO.
    QTest::qWait(300);
    QVERIFY(pmj.set(go, 0));
    QVERIFY(pmj.tap(go));
    settle();
    QCOMPARE(count(ShowControl::Go), 2);
    cm->setDumpWindowMs(500);
}

/* This computer's STOP and GO on a feedback surface get their lamps from
   the Control Map's one lamp writer, with the show's stop's and GO's rules:
   on the PMJ (a board that idles dim) STOP lit always and blinking after an
   ALL OFF; on the APC mini the table's levels. */
void SurfaceSim_Test::deskLampsAreTheControlMaps_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<bool>("pmj");
    QTest::newRow("PMJ Black 1") << pmjFile() << true;
    QTest::newRow("APC mini") << profileDir() + QStringLiteral("Akai-APCMini.qxi") << false;
}

void SurfaceSim_Test::deskLampsAreTheControlMaps()
{
    QFETCH(QString, file);
    QFETCH(bool, pmj);
    QLCInputProfile *p = mapProfile(m_doc, file);
    QVERIFY(p != nullptr);
    SimMidiDevice dev(m_doc, m_stub, p->name(), 0, p->name());
    QVERIFY2(dev.plug(), qPrintable(dev.lastError()));
    quint32 stopCh = kFavorites, goCh = kGo;
    if (!pmj)
    {
        // The APC's first two buttons with a lamp.
        QList<quint32> lit;
        const QMap<quint32, QLCInputChannel *> chans = p->channels();
        for (auto it = chans.constBegin(); it != chans.constEnd() && lit.count() < 2; ++it)
            if (it.value()->type() == QLCInputChannel::Button)
                lit << it.key();
        QCOMPARE(lit.count(), 2);
        stopCh = lit.at(0);
        goCh = lit.at(1);
    }
    const int lampLit = 127;
    const int stopReady = pmj ? 127 : 15;   // the PMJ: lit always; the table: dim (30, velocity 15)
    const int goReady = pmj ? 127 : 15;
    const int goDark = pmj ? 15 : 0;        // the PMJ's dim is its dark
    runList(m_doc);
    dev.forgetLamps();

    QVERIFY(deskOn(m_doc, DeskRecord::Stop, 0, p->name(), stopCh));
    QVERIFY(deskOn(m_doc, DeskRecord::Go, 0, p->name(), goCh));
    settle();
    ControlMap *cm = m_doc->controlMap();
    QCOMPARE(cm->deskLamps().count(), 2);
    QCOMPARE(cm->controlLamp(0, stopCh), ControlMap::LampStandby);
    QCOMPARE(cm->controlLamp(0, goCh), ControlMap::LampStandby);
    QCOMPARE(dev.lamp(stopCh), stopReady);
    QCOMPARE(dev.lamp(goCh), goReady);

    // The desk's STOP pressed: ALL OFF. The stop's light says it; GO goes dark.
    QVERIFY(dev.tap(QStringLiteral("#%1").arg(stopCh)));
    settle(20);
    QTRY_COMPARE(cm->controlLamp(0, stopCh), ControlMap::LampStopped);
    settle(20);
    QTRY_COMPARE(dev.lamp(goCh), goDark);
    if (pmj)
    {
        // Lit and dim in turn, until the next GO.
        const int before = dev.lampMessages(stopCh);
        QTRY_VERIFY(dev.lampMessages(stopCh) >= before + 2);
        QTRY_COMPARE(dev.lamp(stopCh), 15);
        QTRY_COMPARE(dev.lamp(stopCh), lampLit);
    }
    else
    {
        QTRY_COMPARE(dev.lamp(stopCh), lampLit);
    }
    m_doc->outputRuleKeeper()->noteGo();     // the next GO ends the stop
    settle(20);
    QTRY_COMPARE(dev.lamp(stopCh), stopReady);

    // The desk lets go of the control: dark (the show binds nothing there).
    QVERIFY(m_doc->deskRecord()->clear(DeskRecord::Stop));
    settle();
    QCOMPARE(cm->deskLamps().count(), 1);
    QCOMPARE(dev.lamp(stopCh), 0);
}

/* Q314: the show's GO of the Run tab's list is a fallback, armed only while
   this computer has no live GO. The Control Map hears the desk change
   (DeskRecord::changed): the show's GO's lamp goes dark when the desk's GO
   is live, and lights again when it is turned off. */
void SurfaceSim_Test::theShowsGoLampFollowsTheDesk()
{
    QLCInputProfile *p = mapProfile(m_doc, profileDir() + QStringLiteral("Akai-APCMini.qxi"));
    QVERIFY(p != nullptr);
    SimMidiDevice apc(m_doc, m_stub, p->name(), 0, p->name());
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    QList<quint32> lit;
    const QMap<quint32, QLCInputChannel *> chans = p->channels();
    for (auto it = chans.constBegin(); it != chans.constEnd() && lit.count() < 2; ++it)
        if (it.value()->type() == QLCInputChannel::Button)
            lit << it.key();
    QCOMPARE(lit.count(), 2);
    const quint32 list = runList(m_doc);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, lit.at(0), ControlMap::cueList(list), ControlMap::Toggle });
    settle();
    QCOMPARE(apc.lamp(lit.at(0)), 15);          // ready (dim)

    QSignalSpy arming(cm, &ControlMap::armingChanged);
    QVERIFY(deskOn(m_doc, DeskRecord::Go, 0, p->name(), lit.at(1)));
    settle();
    QVERIFY(arming.count() >= 1);
    QVERIFY(!cm->deskBlockReason(cm->bindingsOn(0, lit.at(0)).first()).isEmpty());
    QTRY_COMPARE(apc.lamp(lit.at(0)), 0);       // the fallback is not armed here
    QCOMPARE(apc.lamp(lit.at(1)), 15);          // the desk's GO is the ready one

    QVERIFY(m_doc->deskRecord()->setOn(DeskRecord::Go, false));
    settle();
    QTRY_COMPARE(apc.lamp(lit.at(0)), 15);      // armed again
    QCOMPARE(apc.lamp(lit.at(1)), 0);           // the desk let go of its GO
}

void SurfaceSim_Test::twoIdenticalBoardsAreTwoControllers()
{
    QLCInputProfile *p = mapProfile(m_doc, profileDir() + QStringLiteral("Akai-APCMini.qxi"));
    QVERIFY(p != nullptr);
    SimMidiDevice one(m_doc, m_stub, QStringLiteral("APC MINI"), 0, p->name(), QString(), 0);
    SimMidiDevice two(m_doc, m_stub, QStringLiteral("APC MINI"), 1, p->name(), QString(), 1);
    QVERIFY2(one.plug(), qPrintable(one.lastError()));
    QVERIFY2(two.plug(), qPrintable(two.lastError()));
    QCOMPARE(one.line(), 0);
    QCOMPARE(two.line(), 1);
    /* Each universe on its own board's line (setInputPatch() once took the
       first line of the name: the second board was heard nowhere). */
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->inputPatch(1) != nullptr);
    QCOMPARE(iom->inputPatch(1)->input(), quint32(two.line()));

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName(QStringLiteral("Dim 1"));
    fxi->setAddress(0);
    fxi->setUniverse(0);
    fxi->setChannels(1);
    m_doc->addFixture(fxi);
    Scene *s = new Scene(m_doc);
    s->setName(QStringLiteral("Warm"));
    s->addFixture(fxi->id());
    s->setValue(fxi->id(), 0, 255);
    m_doc->addFunction(s);
    // The scene learned on the second board's first pad.
    quint32 pad = UINT_MAX;
    const QMap<quint32, QLCInputChannel *> pads = p->channels();
    for (auto it = pads.constBegin(); it != pads.constEnd() && pad == UINT_MAX; ++it)
        if (it.value()->type() == QLCInputChannel::Button)
            pad = it.key();
    QVERIFY(pad != UINT_MAX);
    m_doc->controlMap()->setBinding({ 1, pad, ControlMap::function(s->id()), ControlMap::Toggle });

    QVERIFY(one.tap(QStringLiteral("#%1").arg(pad)));
    m_doc->masterTimer()->timerTick();
    QCoreApplication::processEvents();
    QVERIFY2(!s->isRunning(), "the first board's pad played the second board's scene");
    QVERIFY(two.tap(QStringLiteral("#%1").arg(pad)));
    m_doc->masterTimer()->timerTick();
    QCoreApplication::processEvents();
    QTRY_VERIFY2(s->isRunning(), "the second board's pad did not play its scene");
}

/****************************************************************************
 * Setting up a controller: the device's own config (SurfaceSetup)
 ****************************************************************************/

namespace
{
/* The board in the PMJ's real layout: each button's LED, as his profiler's
   identify found them (SurfaceSetup's shipped pairing). */
void pmjLayout(SimOpenDeck &deck, const QList<SurfaceSetup::LampPair> &pairs)
{
    for (const SurfaceSetup::LampPair &p : pairs)
        deck.setUnder(p.note, p.led);
}
}

void SurfaceSim_Test::theLampCheckReadsTheBoard()
{
    QVERIFY(mapProfile(m_doc, pmjFile()) != nullptr);
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj);
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SimOpenDeck deck(m_stub, &pmj);
    SurfaceSetup *setup = m_doc->surfaceSetup();
    QVERIFY(setup != nullptr);
    QCOMPARE(setup->universes(), QList<quint32>() << 0);
    QCOMPARE(setup->lastCheck(0).state, SurfaceSetup::Check::NotChecked);

    // Every one of the 54 paired buttons: its LED is on note i, channel 1.
    const SurfaceSetup::Check c = setup->check(0);
    QCOMPARE(c.state, SurfaceSetup::Check::Misplaced);
    QCOMPARE(c.lamps, 54);
    QCOMPARE(c.misplaced, 54);
    QVERIFY2(c.text.startsWith(QStringLiteral("54 of 54 lamps aren't under their buttons")), qPrintable(c.text));
    // The 128 LEDs were read in parts of 32 (a part-0 read would see 32).
    QCOMPARE(setup->lampCount(0), 128);
    QCOMPARE(deck.writes(), 0);

    // One lamp put right by hand on the board: 53 left.
    deck.setValue(4, 3, 17, 0);
    deck.setValue(4, 7, 17, 9);
    QCOMPARE(setup->check(0).misplaced, 53);
    QVERIFY(setup->lastCheck(0).text.startsWith(QStringLiteral("53 of 54")));
    // A static LED does not listen, whatever its note.
    deck.setValue(4, 5, 17, 10);
    QCOMPARE(setup->check(0).misplaced, 54);
}

void SurfaceSim_Test::alignBacksUpAndUndoRestores()
{
    QVERIFY(mapProfile(m_doc, pmjFile()) != nullptr);
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj);
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SimOpenDeck deck(m_stub, &pmj);
    SurfaceSetup *setup = m_doc->surfaceSetup();
    pmjLayout(deck, setup->pairing(0));
    const auto factory = deck.tables();

    // Go (note 0, channel 9) lights nothing as it left the factory.
    QCOMPARE(deck.litBy(9, 0), -1);
    QVERIFY(setup->lastBackup(0).isEmpty());

    QString error;
    QVERIFY2(setup->align(0, false, &error), qPrintable(error));
    // The backup first: on disk, the whole factory config.
    const QString backup = setup->lastBackup(0);
    QVERIFY(!backup.isEmpty());
    QVERIFY(backup.startsWith(m_backups.path()));
    OpenDeckLink::Tables saved;
    QVERIFY(OpenDeckLink::loadBackup(backup, &saved));
    QCOMPARE(saved, factory);

    // Each lamp is under its own button now: Go lights the LED under Go.
    QCOMPARE(deck.litBy(9, 0), deck.ledUnder(0));
    QCOMPARE(deck.litBy(9, 0), 17);
    for (const SurfaceSetup::LampPair &p : setup->pairing(0))
        QCOMPARE(deck.litBy(9, p.note), deck.ledUnder(p.note));
    QCOMPARE(deck.writes(), 54 * 4);
    QCOMPARE(setup->lastCheck(0).state, SurfaceSetup::Check::Aligned);
    QVERIFY(setup->lastCheck(0).text.contains(QStringLiteral("All 54 lamps are under their buttons")));
    QCOMPARE(setup->check(0).misplaced, 0);

    // Undo Align: the factory order back, the aligned one kept (redo).
    const auto aligned = deck.tables();
    QVERIFY2(setup->undoAlign(0, false, &error), qPrintable(error));
    QCOMPARE(deck.tables(), factory);
    QCOMPARE(deck.litBy(9, 0), -1);
    QVERIFY(setup->lastBackup(0).isEmpty());
    const QStringList kept = QDir(m_backups.path()).entryList({ QStringLiteral("*pre-restore*.json") });
    QCOMPARE(kept.size(), 1);
    QVERIFY(OpenDeckLink::loadBackup(m_backups.path() + QLatin1Char('/') + kept.first(), &saved));
    QCOMPARE(saved, aligned);
    // Nothing left to undo.
    QVERIFY(!setup->undoAlign(0, false, &error));
}

void SurfaceSim_Test::aWriteIsRefusedInOperate()
{
    QVERIFY(mapProfile(m_doc, pmjFile()) != nullptr);
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj);
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SimOpenDeck deck(m_stub, &pmj);
    SurfaceSetup *setup = m_doc->surfaceSetup();
    m_doc->setMode(Doc::Operate);
    QString error;
    QVERIFY(!setup->align(0, false, &error));
    QVERIFY2(error.contains(QStringLiteral("Operate")), qPrintable(error));
    QVERIFY(!setup->beginIdentify(0, false, &error));
    QCOMPARE(deck.writes(), 0);
    QVERIFY(setup->lastBackup(0).isEmpty());
    // Reading is not writing: the check still works in Operate.
    QCOMPARE(setup->check(0).state, SurfaceSetup::Check::Misplaced);
    // Confirmed by the person: written.
    QVERIFY2(setup->align(0, true, &error), qPrintable(error));
    QCOMPARE(deck.writes(), 54 * 4);
    m_doc->setMode(Doc::Design);
}

void SurfaceSim_Test::aSilentBoardIsSaidAndNotWritten()
{
    QVERIFY(mapProfile(m_doc, pmjFile()) != nullptr);
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj);
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SimOpenDeck deck(m_stub, &pmj);
    deck.setDeaf(true);
    SurfaceSetup *setup = m_doc->surfaceSetup();
    setup->setTimeout(30);
    const SurfaceSetup::Check c = setup->check(0);
    QCOMPARE(c.state, SurfaceSetup::Check::NoAnswer);
    QVERIFY2(c.text.contains(QStringLiteral("did not answer")), qPrintable(c.text));
    QString error;
    QVERIFY(!setup->align(0, false, &error));
    QVERIFY2(error.contains(QStringLiteral("nothing was written")), qPrintable(error));
    QCOMPARE(deck.writes(), 0);
    QVERIFY(QDir(m_backups.path()).entryList(QDir::Files).isEmpty());
}

void SurfaceSim_Test::identifyPairsLampsAndPutsTheBoardBack()
{
    QVERIFY(mapProfile(m_doc, pmjFile()) != nullptr);
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj);
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SimOpenDeck deck(m_stub, &pmj);
    SurfaceSetup *setup = m_doc->surfaceSetup();
    // A board whose LEDs are Static: nothing would light for Identify.
    for (int i = 0; i < SimOpenDeck::Leds; ++i)
        deck.setValue(4, 5, i, 10);
    const auto before = deck.tables();

    QString error;
    QVERIFY2(setup->beginIdentify(0, false, &error), qPrintable(error));
    QCOMPARE(setup->lampCount(0), 128);
    for (int i = 0; i < SimOpenDeck::Leds; ++i)
        QCOMPARE(deck.litBy(1, i), i);

    // LED 17 flashes (note 17, channel 1); the person presses Go, under it.
    pmj.forgetLamps();
    setup->flashLamp(0, 17, true);
    const QList<SimMidiDevice::Out> out = pmj.takeOutput();
    QCOMPARE(out.size(), 1);
    QCOMPARE(QString::fromLatin1(out.first().midi.toHex(' ')), QStringLiteral("90 11 7f"));
    QCOMPARE(deck.litBy(1, 17), 17);
    SurfaceSetup::LampPair pair;
    QVERIFY(SurfaceSetup::pairFromChannel(kGo, 17, &pair));
    QCOMPARE(pair.channel, 9);
    QCOMPARE(pair.note, 0);
    QVERIFY(!SurfaceSetup::pairFromChannel(kMaster, 1, &pair));     // a fader: no lamp

    QVERIFY2(setup->endIdentify(0, { pair }, &error), qPrintable(error));
    QCOMPARE(deck.tables(), before);
    // The pairing found is this board's now (in place of the shipped one).
    QCOMPARE(setup->pairing(0).size(), 1);
    QCOMPARE(setup->pairing(0).first().led, 17);
    QCOMPARE(setup->check(0).lamps, 1);
}

void SurfaceSim_Test::theInitIsSentOnPatchReplugAndWake()
{
    const QString apc = QStringLiteral("Akai APC40 mkII");
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("Akai-APC40-mkII.qxi")) != nullptr);
    const QString ableton = QStringLiteral("f0 47 7f 29 60 00 04 41 09 00 05 f7");
    auto sends = [this, &ableton]() {
        int n = 0;
        for (const auto &m : std::as_const(m_stub->m_showControlSent))
            n += hex(m.second) == ableton ? 1 : 0;
        return n;
    };
    SimMidiDevice board(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, apc);
    QVERIFY2(board.plug(), qPrintable(board.lastError()));
    SurfaceSetup *setup = m_doc->surfaceSetup();
    QCOMPARE(setup->universes(), QList<quint32>() << 0);
    // Patched: its Ableton mode, out of its own port.
    QVERIFY(sends() >= 1);
    QCOMPARE(int(m_stub->m_showControlSent.last().first), board.line());
    QCOMPARE(setup->lastCheck(0).state, SurfaceSetup::Check::InitOnly);
    QVERIFY(setup->lastCheck(0).text.contains(QStringLiteral("Ableton mode")));

    // A replug: sent again (the board lost it with its power).
    m_stub->m_showControlSent.clear();
    QVERIFY(board.unplug());
    m_stub->m_showControlSent.clear();
    QVERIFY(board.replug());
    QTRY_VERIFY(sends() >= 1);

    // A wake: sent again once the devices are back.
    m_stub->m_showControlSent.clear();
    ControllerKeepAlive *ka = m_doc->programmer()->controllerKeepAlive();
    QVERIFY(ka != nullptr);
    emit ka->woke(60000);
    QCOMPARE(sends(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(sends() == 1, 4000);

    // A board with nothing to set (the PMJ is set by its check) gets no init.
    QVERIFY(!setup->sendInit(5));
}

void SurfaceSim_Test::aRescanThatFindsNothingSaysSo()
{
    m_stub->m_linesAreHardware = true;
    m_stub->m_lineNames = { QStringLiteral("PMJ Black 1") };
    QStringList found;
    const QString nothing = SurfaceSetup::rescanControllers(m_doc, &found);
    QVERIFY(found.isEmpty());
    QVERIFY2(nothing.startsWith(QStringLiteral("No new controller found.")), qPrintable(nothing));
    QVERIFY(nothing.contains(QStringLiteral("power")));
    QVERIFY(nothing.contains(QStringLiteral("charge-only")));
    QVERIFY(m_stub->m_rescanCalled > 0);

    // The APC40 mkII powered on: found by name.
    m_stub->m_onRescan = [this]() { m_stub->m_lineNames << QStringLiteral("APC40 mkII"); };
    const QString text = SurfaceSetup::rescanControllers(m_doc, &found);
    QCOMPARE(found, QStringList() << QStringLiteral("APC40 mkII"));
    QCOMPARE(text, QStringLiteral("Found APC40 mkII."));
}

QTEST_MAIN(SurfaceSim_Test)
