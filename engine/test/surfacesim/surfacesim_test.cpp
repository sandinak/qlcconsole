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
#include <QBuffer>
#include <QXmlStreamWriter>

#include "surfacesim_test.h"
#include "simmididevice.h"
#include "simopendeck.h"
#include "simapc64.h"
#include "qlcinputchannel.h"
#include "surfacesetup.h"
#include "surfaceautopatch.h"
#include "surfaceboardlog.h"
#include "outputpatch.h"
#include "opendecklink.h"
#include "controllerkeepalive.h"
#include "programmercontroller.h"
#include "surfacescenarios.h"
#include "surfacescript.h"
#include "gamepadlayout.h"
#include "hidprofile.h"
#include "controlmap.h"
#include "deskrecord.h"
#include "surfacelayout.h"
#include "pageset.h"
#include <QTemporaryDir>
#include <QRegularExpression>
#include <memory>
#include <vector>
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
    // Each test's backups are its own.
    QDir(m_backups.path()).removeRecursively();
    QDir().mkpath(m_backups.path());
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

/* Phase R through the PMJ's own bytes: a key really held across the
   replug (aftertouch, an axis used as GO) keeps resending 127. The dump's
   press is ignored and the key counts as held, so the resends after the
   window are that one press: no GO until a release then a press. The
   desk's GO on Go, a show trigger (a cue list's GO) on Back. */
void SurfaceSim_Test::aKeyHeldAcrossAReplugGoesOnlyAfterARelease()
{
    QVERIFY(mapProfile(m_doc, pmjFile()) != nullptr);
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj);
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    m_doc->showControl()->setGoGuardMs(0);      // not under test here
    const quint32 list = runList(m_doc);
    ControlMap *cm = m_doc->controlMap();
    cm->setDumpWindowMs(cm->dumpBurstMs() + 150);
    const QString go = QStringLiteral("Go"), back = QStringLiteral("Back");
    // The key held through the cable out and back (the dump says 127), and
    // still held: resends inside the window, then past it.
    auto heldAcross = [&](const QString &key) {
        if (!SurfaceScenarios::unplugReplug(&pmj, { { key, 127 } }))
            return false;
        settle();
        pmj.wait(cm->dumpBurstMs() + 50);
        if (!pmj.set(key, 127))
            return false;
        pmj.wait(200);
        const bool ok = pmj.set(key, 127) && pmj.set(key, 127);
        settle();
        return ok;
    };

    // This computer's GO.
    QVERIFY(deskOn(m_doc, DeskRecord::Go, 0, kPmj, kGo));
    QSignalSpy deskFired(m_doc->deskRecord(), &DeskRecord::fired);
    QVERIFY2(heldAcross(go), qPrintable(pmj.lastError()));
    QVERIFY2(deskFired.isEmpty(), "the desk's GO held across the replug GOed on a resend");
    QVERIFY(pmj.set(go, 0));
    QVERIFY(pmj.set(go, 127));
    settle();
    QCOMPARE(deskFired.count(), 1);
    QVERIFY(pmj.set(go, 0));
    settle();

    // A show trigger: the cue list's GO on Back (the desk's GO let go, so
    // the show's trigger on the Run tab's list is armed).
    QVERIFY(m_doc->deskRecord()->setOn(DeskRecord::Go, false));
    cm->setBinding({ 0, kBack, ControlMap::cueList(list), ControlMap::Toggle });
    settle();
    QSignalSpy showFired(cm, &ControlMap::fired);
    QVERIFY2(heldAcross(back), qPrintable(pmj.lastError()));
    QVERIFY2(showFired.isEmpty(), "a show trigger held across the replug GOed on a resend");
    QVERIFY(pmj.set(back, 0));
    QVERIFY(pmj.set(back, 127));
    settle();
    QCOMPARE(showFired.count(), 1);
    QVERIFY(pmj.set(back, 0));
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
 * The surface map's layouts, and the default roles they seed
 ****************************************************************************/

namespace
{
const QString kApc = QStringLiteral("Akai APC40 mkII");
const quint32 kApcPlay = 128 + 91, kApcRecord = 128 + 93, kApcStopAll = 128 + 81, kApcMaster = 14;

QString apcFile()
{
    return profileDir() + QStringLiteral("Akai-APC40-mkII.qxi");
}
}

void SurfaceSim_Test::everyLayoutHoldsToItsProfile()
{
    const QStringList files = SurfaceLayout::builtInFiles();
    QVERIFY2(files.size() >= 4, qPrintable(files.join(", ")));
    // The profiles by name, from the source folder.
    QHash<QString, QString> byName;
    QDir dir(profileDir(), QStringLiteral("*.qxi"), QDir::Name, QDir::Files);
    for (const QString &f : dir.entryList())
        if (QLCInputProfile *p = QLCInputProfile::loader(dir.filePath(f)))
        {
            byName.insert(p->name(), dir.filePath(f));
            delete p;
        }
    QStringList seen;
    for (const QString &file : files)
    {
        QString error;
        const SurfaceLayout l = SurfaceLayout::fromFile(file, &error);
        QVERIFY2(l.isValid(), qPrintable(file + QStringLiteral(": ") + error));
        QVERIFY2(byName.contains(l.profileName()), qPrintable(file + QStringLiteral(": no profile ") + l.profileName()));
        QLCInputProfile *p = QLCInputProfile::loader(byName.value(l.profileName()));
        QVERIFY(p != nullptr);
        const QStringList problems = l.selfTest(p);
        QVERIFY2(problems.isEmpty(), qPrintable(file + QStringLiteral(":\n  ") + problems.join(QStringLiteral("\n  "))));
        QCOMPARE(l.controls().size() > 0, true);
        delete p;
        seen << l.profileName();
        QVERIFY(SurfaceLayout::forProfile(l.profileName()).isValid());
    }
    const QStringList needed = { kApc, kPmj, QStringLiteral("MaizeDMX Playback Controller"),
                                 QStringLiteral("Akai APC64") };
    for (const QString &must : needed)
        QVERIFY2(seen.contains(must), qPrintable(must));
    QVERIFY(!SurfaceLayout::forProfile(QStringLiteral("Akai APC Mini")).isValid());
}

void SurfaceSim_Test::theLayoutSelfTestCatchesABrokenOne()
{
    QLCInputProfile *p = QLCInputProfile::loader(apcFile());
    QVERIFY(p != nullptr);
    QVERIFY(!SurfaceLayout::builtInFiles().isEmpty());     // the resource is there
    QFile f(QStringLiteral(":/surfacelayouts/Akai-APC40-mkII.layout.json"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray good = f.readAll();
    QVERIFY(SurfaceLayout::fromJson(good).selfTest(p).isEmpty());

    auto problemsOf = [p](QByteArray json) { return SurfaceLayout::fromJson(json).selfTest(p).join(QStringLiteral("\n")); };
    // PLAY gone: channel 219 is not placed (and GO is on nothing).
    QByteArray missing = good;
    missing.replace("{\"ch\": 219,", "{\"ch\": 99999,");
    QString text = problemsOf(missing);
    QVERIFY2(text.contains(QStringLiteral("channel 219 (Play) is not placed")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("channel 99999 is not in the profile")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("role 'go' is on channel 219")), qPrintable(text));
    // RECORD placed twice, on PLAY's spot: twice and an overlap.
    QByteArray twice = good;
    twice.replace("{\"ch\": 219,", "{\"ch\": 221,");
    text = problemsOf(twice);
    QVERIFY2(text.contains(QStringLiteral("channel 221 (Record) is placed 2 times")), qPrintable(text));
    QByteArray overlap = good;
    overlap.replace("{\"ch\": 221, \"x\": 11.272", "{\"ch\": 221, \"x\": 10.281");
    text = problemsOf(overlap);
    QVERIFY2(text.contains(QStringLiteral("channels 219 and 221 overlap")), qPrintable(text));
    // The Grand Master on a button.
    QByteArray gm = good;
    gm.replace("{\"role\": \"gm\", \"ch\": 14,", "{\"role\": \"gm\", \"ch\": 208,");
    text = problemsOf(gm);
    QVERIFY2(text.contains(QStringLiteral("role 'gm' is on Master, a button")), qPrintable(text));
    delete p;
}

void SurfaceSim_Test::aNewBoardSeedsItsDefaultRoles()
{
    QVERIFY(mapProfile(m_doc, apcFile()) != nullptr);
    DeskRecord *desk = m_doc->deskRecord();
    QVERIFY2(!desk->anySet(), "not a fresh computer");
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc);
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    settle();
    QVERIFY2(desk->isLive(DeskRecord::Go), qPrintable(desk->stateText(DeskRecord::Go)));
    QCOMPARE(desk->binding(DeskRecord::Go).channel, kApcPlay);
    QCOMPARE(desk->binding(DeskRecord::Back).channel, kApcRecord);
    QCOMPARE(desk->binding(DeskRecord::Stop).channel, kApcStopAll);
    QCOMPARE(desk->binding(DeskRecord::GrandMaster).channel, kApcMaster);
    QVERIFY(desk->seededDevices().contains(desk->identityOfUniverse(0)));

    // PLAY GOes the Run tab's list, through the one run path.
    runList(m_doc);
    QList<int> commands;
    m_doc->showControl()->setRunHandler([&commands](const ShowControl::Request &r, ShowControl::Route) {
        commands << int(r.command);
        return true;
    });
    QVERIFY(apc.tap(QStringLiteral("#%1").arg(kApcPlay)));
    settle();
    QCOMPARE(int(commands.count(int(ShowControl::Go))), 1);
    QVERIFY(apc.tap(QStringLiteral("#%1").arg(kApcRecord)));
    settle();
    QCOMPARE(int(commands.count(int(ShowControl::Back))), 1);
}

void SurfaceSim_Test::aLearnedRoleWinsOverTheDefault()
{
    QVERIFY(mapProfile(m_doc, apcFile()) != nullptr);
    QVERIFY(mapProfile(m_doc, pmjFile()) != nullptr);
    // GO learned first, on another board.
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 1, kPmj);
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    QVERIFY(deskOn(m_doc, DeskRecord::Go, 1, kPmj, kFavorites));
    const QString pmjId = m_doc->deskRecord()->identityOfUniverse(1);

    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc);
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    settle();
    DeskRecord *desk = m_doc->deskRecord();
    QCOMPARE(desk->binding(DeskRecord::Go).identity, pmjId);
    QCOMPARE(desk->binding(DeskRecord::Go).channel, kFavorites);
    // The rest are the APC's defaults.
    QCOMPARE(desk->binding(DeskRecord::Back).channel, kApcRecord);
    QCOMPARE(desk->binding(DeskRecord::GrandMaster).channel, kApcMaster);

    runList(m_doc);
    QList<int> commands;
    m_doc->showControl()->setRunHandler([&commands](const ShowControl::Request &r, ShowControl::Route) {
        commands << int(r.command);
        return true;
    });
    QVERIFY(apc.tap(QStringLiteral("#%1").arg(kApcPlay)));
    settle();
    QCOMPARE(int(commands.count(int(ShowControl::Go))), 0);
    QVERIFY(pmj.tap(QStringLiteral("Favorites")));
    settle();
    QCOMPARE(int(commands.count(int(ShowControl::Go))), 1);
}

void SurfaceSim_Test::aClearedDefaultStaysCleared()
{
    QVERIFY(mapProfile(m_doc, apcFile()) != nullptr);
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc);
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    settle();
    DeskRecord *desk = m_doc->deskRecord();
    QVERIFY(desk->isSet(DeskRecord::Go));
    QVERIFY(desk->clear(DeskRecord::Go));
    // Out and in again, and patched again: still cleared.
    QVERIFY(SurfaceScenarios::unplugReplug(&apc));
    m_doc->inputOutputMap()->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine());
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    settle();
    QVERIFY(!desk->isSet(DeskRecord::Go));
    QVERIFY(desk->isSet(DeskRecord::Back));
    QVERIFY(!desk->seedDefaults());
}

void SurfaceSim_Test::aDefaultLeavesTheShowsTriggerAlone()
{
    QVERIFY(mapProfile(m_doc, apcFile()) != nullptr);
    // The open show already has PLAY: a scene's trigger.
    Fixture *fxi = new Fixture(m_doc);
    fxi->setAddress(0);
    fxi->setUniverse(0);
    fxi->setChannels(1);
    m_doc->addFixture(fxi);
    Scene *sc = new Scene(m_doc);
    sc->addFixture(fxi->id());
    sc->setValue(fxi->id(), 0, 255);
    m_doc->addFunction(sc);
    m_doc->controlMap()->setDumpWindowMs(0);
    m_doc->controlMap()->setBinding({ 0, kApcPlay, ControlMap::function(sc->id()), ControlMap::Toggle });
    DeskRecord *desk = m_doc->deskRecord();
    QVERIFY2(!desk->anySet(), "not a fresh computer");
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc);
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    settle();
    // GO is left to learn; the other defaults are seeded.
    QVERIFY2(!desk->isSet(DeskRecord::Go), qPrintable(desk->stateText(DeskRecord::Go)));
    QCOMPARE(desk->binding(DeskRecord::Back).channel, kApcRecord);
    QCOMPARE(desk->binding(DeskRecord::Stop).channel, kApcStopAll);
    // PLAY still plays the show's scene.
    QTest::qWait(300);
    QVERIFY(apc.tap(QStringLiteral("#%1").arg(kApcPlay)));
    settle();
    m_doc->masterTimer()->timerTick();
    QCoreApplication::processEvents();
    QVERIFY2(sc->isRunning(), "the seeded desk GO took PLAY from the show's trigger");
    m_doc->controlMap()->setDumpWindowMs(500);
}

void SurfaceSim_Test::theMaizeDmxSeedsItsDefaults()
{
    const QString name = QStringLiteral("MaizeDMX Playback Controller");
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("MaizeDMX-Playback-Controller.qxi")) != nullptr);
    // The system names its port "USB2.0-MIDI": the type is told by the profile.
    SimMidiDevice maize(m_doc, m_stub, QStringLiteral("USB2.0-MIDI Port 1"), 0, name);
    QVERIFY2(maize.plug(), qPrintable(maize.lastError()));
    settle();
    DeskRecord *desk = m_doc->deskRecord();
    QCOMPARE(desk->binding(DeskRecord::Go).channel, quint32(128 + 13));
    QCOMPARE(desk->binding(DeskRecord::Back).channel, quint32(128 + 14));
    QCOMPARE(desk->binding(DeskRecord::GrandMaster).channel, quint32(10));
    QCOMPARE(desk->binding(DeskRecord::Stop).channel, quint32(128 + 29));
    runList(m_doc);
    QList<int> commands;
    m_doc->showControl()->setRunHandler([&commands](const ShowControl::Request &r, ShowControl::Route) {
        commands << int(r.command);
        return true;
    });
    QVERIFY(maize.tap(QStringLiteral("GO")));
    settle();
    QCOMPARE(int(commands.count(int(ShowControl::Go))), 1);
}

void SurfaceSim_Test::theApc64SeedsItsDefaults()
{
    const QString name = QStringLiteral("Akai APC64");
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("Akai-APC64.qxi")) != nullptr);
    // Session mode, its DAW port: everything on one port.
    SimMidiDevice apc64(m_doc, m_stub, QStringLiteral("APC64 DAW"), 0, name);
    QVERIFY2(apc64.plug(), qPrintable(apc64.lastError()));
    settle();
    DeskRecord *desk = m_doc->deskRecord();
    // As the APC40's (Q329): PLAY, RECORD, STOP ALL CLIPS; the GM touch strip 8.
    QCOMPARE(desk->binding(DeskRecord::Go).channel, quint32(128 + 91));
    QCOMPARE(desk->binding(DeskRecord::Back).channel, quint32(128 + 92));
    QCOMPARE(desk->binding(DeskRecord::Stop).channel, quint32(128 + 119));
    QCOMPARE(desk->binding(DeskRecord::GrandMaster).channel, quint32((7 << 12) | 513));
    // Pads on MIDI channel 7 and buttons on 1 share note numbers: two channels.
    QLCInputProfile *p = apc64.profile();
    QVERIFY(p->channel((6 << 12) | (128 + 23)) != nullptr);
    QVERIFY(p->channel(128 + 91) != nullptr);
    const SurfaceLayout layout = SurfaceLayout::forProfile(name);
    QCOMPARE(layout.displayLines(), 3);
    QCOMPARE(layout.displayChars(), 8);
    // The encoder and its push are one control.
    QCOMPARE(layout.indexOf(90), layout.indexOf(128 + 90));
    runList(m_doc);
    QList<int> commands;
    m_doc->showControl()->setRunHandler([&commands](const ShowControl::Request &r, ShowControl::Route) {
        commands << int(r.command);
        return true;
    });
    QVERIFY(apc64.tap(QStringLiteral("Play")));
    settle();
    QCOMPARE(int(commands.count(int(ShowControl::Go))), 1);
    // Pad 23 (channel 7) is not PLAY's neighbour on channel 1: no GO.
    QVERIFY(apc64.tap(QStringLiteral("#%1").arg((6 << 12) | (128 + 23))));
    settle();
    QCOMPARE(int(commands.count(int(ShowControl::Go))), 1);
}

void SurfaceSim_Test::aProfileIsChosenByTheDevice()
{
    for (const char *f : { "Akai-APC40-mkII.qxi", "Akai-APC40.qxi", "Akai-APC64.qxi", "Akai-APCMini-mk2.qxi",
                           "Akai-APCMini.qxi", "PreSonus-FaderPort-2.qxi", "Akai-LPD8.qxi", "Akai-LPD8-mk2.qxi" })
        QVERIFY(mapProfile(m_doc, profileDir() + QLatin1String(f)) != nullptr);
    struct Row { const char *device; const char *profile; };
    const Row rows[] = {
        { "APC40 mkII", "Akai APC40 mkII" },
        { "APC40", "Akai APC40" },                          // not the mkII's
        { "APC64 DAW (APC64)", "Akai APC64" },              // its DAW port only
        { "APC mini mk2 Control", "Akai APC Mini mk2" },
        { "APC MINI", "Akai APC Mini" },                    // not the mk2
        { "PreSonus FP2", "PreSonus FaderPort 2" },
        { "LPD8", "Akai LPD8" },
        { "LPD8 mk2", "Akai LPD8 mk2" },                   // not the original's
        { "LPD8 MIDI 1", "Akai LPD8" },                     // Linux's port name
        { "APC64 Notes (APC64)", "" },                      // another port of the APC64
        { "USB2.0-MIDI Port 1", "" },                       // a name many boxes share
    };
    InputOutputMap *iom = m_doc->inputOutputMap();
    std::vector<std::unique_ptr<SimMidiDevice>> boards;
    quint32 u = 0;
    for (const Row &r : rows)
    {
        boards.emplace_back(new SimMidiDevice(m_doc, m_stub, QLatin1String(r.device), u));
        QVERIFY2(boards.back()->plug(), qPrintable(boards.back()->lastError()));
        InputPatch *ip = iom->inputPatch(u);
        QVERIFY(ip != nullptr);
        QCOMPARE(ip->profile() != nullptr ? ip->profile()->name() : QString(), QLatin1String(r.profile));
        ++u;
    }
    // A name with Windows' unit prefix is the same board.
    QCOMPARE(iom->profileForDevice(QStringLiteral("2- APC MINI")), QStringLiteral("Akai APC Mini"));
    QCOMPARE(iom->profileForDevice(QStringLiteral("apc40 MKII")), kApc);

    // A profile chosen by a person is kept, whatever the device is called.
    SimMidiDevice chosen(m_doc, m_stub, QStringLiteral("APC40 mkII"), u, QStringLiteral("Akai APC Mini"));
    QVERIFY2(chosen.plug(), qPrintable(chosen.lastError()));
    QCOMPARE(iom->inputPatch(u)->profile()->name(), QStringLiteral("Akai APC Mini"));
}

void SurfaceSim_Test::theApcMiniMk2SeedsItsDefaults()
{
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("Akai-APCMini-mk2.qxi")) != nullptr);
    const SurfaceLayout layout = SurfaceLayout::forProfile(QStringLiteral("Akai APC Mini mk2"));
    QVERIFY(layout.isValid());
    QCOMPARE(layout.controls().size(), 64 + 8 + 8 + 1 + 9);
    // Patched by its port's name only (as the show has it): no profile chosen.
    SimMidiDevice mini(m_doc, m_stub, QStringLiteral("APC mini mk2 Control"), 0);
    QVERIFY2(mini.plug(), qPrintable(mini.lastError()));
    settle();
    DeskRecord *desk = m_doc->deskRecord();
    QCOMPARE(desk->binding(DeskRecord::Go).channel, quint32(240));      // the top scene button
    QCOMPARE(desk->binding(DeskRecord::Back).channel, quint32(241));
    QCOMPARE(desk->binding(DeskRecord::Stop).channel, quint32(250));    // Shift
    QCOMPARE(desk->binding(DeskRecord::GrandMaster).channel, quint32(56));
    runList(m_doc);
    QList<int> commands;
    m_doc->showControl()->setRunHandler([&commands](const ShowControl::Request &r, ShowControl::Route) {
        commands << int(r.command);
        return true;
    });
    QVERIFY(mini.tap(QStringLiteral("#240")));
    settle();
    QCOMPARE(int(commands.count(int(ShowControl::Go))), 1);

    // Left and Right page the faders (Design): both lit, a press steps and the page key's lamp says so.
    const QList<PageSet> sets = PageSet::forProfile(QStringLiteral("Akai APC Mini mk2"));
    QVERIFY(!sets.isEmpty());
    QCOMPARE(sets.first().pages().size(), 2);
    QCOMPARE(sets.first().nextKey(), quint32(235));
    QCOMPARE(sets.first().previousKey(), quint32(234));
    QVERIFY2(sets.first().check(SurfaceLayout::forProfile(QStringLiteral("Akai APC Mini mk2")),
                                m_doc->inputOutputMap()->profile(QStringLiteral("Akai APC Mini mk2"))).isEmpty(),
             "the page set holds to the layout");
    // A press of Right steps to Move and lights both page keys (there is a page each way).
    QVERIFY(mini.tap(QStringLiteral("#235")));
    settle();
    QVERIFY(mini.lamp(235) > 0);
    QVERIFY(mini.lamp(234) > 0);
}

void SurfaceSim_Test::theLpd8sSeedTheirDefaultsAndGuardTheStop_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("port");
    QTest::addColumn<QString>("profile");
    QTest::addColumn<quint32>("pad1");      // Pad 1's channel (note 36)
    QTest::addColumn<quint32>("k1");        // K1's channel
    // The original: everything on MIDI channel 1, the knobs CC 0-7.
    QTest::newRow("LPD8") << "Akai-LPD8.qxi" << "LPD8" << "Akai LPD8" << quint32(128 + 36) << quint32(0);
    // The mk2: the pads on channel 10, the knobs CC 70-77 on channel 1.
    QTest::newRow("LPD8 mk2") << "Akai-LPD8-mk2.qxi" << "LPD8 mk2" << "Akai LPD8 mk2"
                              << quint32((9 << 12) | (128 + 36)) << quint32(70);
}

void SurfaceSim_Test::theLpd8sSeedTheirDefaultsAndGuardTheStop()
{
    QFETCH(QString, file);
    QFETCH(QString, port);
    QFETCH(QString, profile);
    QFETCH(quint32, pad1);
    QFETCH(quint32, k1);
    QVERIFY(mapProfile(m_doc, profileDir() + file) != nullptr);
    const SurfaceLayout layout = SurfaceLayout::forProfile(profile);
    QVERIFY(layout.isValid());
    QCOMPARE(layout.controls().size(), 8 + 8);
    QCOMPARE(layout.shiftChannel(), pad1);
    // Patched by its port's name only: its profile and its defaults follow.
    SimMidiDevice lpd(m_doc, m_stub, port, 0);
    QVERIFY2(lpd.plug(), qPrintable(lpd.lastError()));
    settle();
    QCOMPARE(m_doc->inputOutputMap()->inputPatch(0)->profileName(), profile);
    DeskRecord *desk = m_doc->deskRecord();
    QCOMPARE(desk->binding(DeskRecord::Go).channel, pad1 + 3);          // Pad 4
    QCOMPARE(desk->binding(DeskRecord::Back).channel, pad1 + 2);        // Pad 3
    QCOMPARE(desk->binding(DeskRecord::Stop).channel, pad1 + 7);        // Pad 8
    QCOMPARE(desk->binding(DeskRecord::GrandMaster).channel, k1);       // K1
    QVERIFY(desk->shiftGuardsStop(0));
    QVERIFY(desk->shiftGuardsRelease(0));

    runList(m_doc);
    QList<int> commands;
    ShowControl *sc = m_doc->showControl();
    sc->setGoGuardMs(0);
    // GO and Back counted; the stop carried out, so the release guard has a stop to guard.
    sc->setRunHandler([&commands, sc](const ShowControl::Request &r, ShowControl::Route) {
        commands << int(r.command);
        return r.command == ShowControl::AllOff ? sc->execute(r) : true;
    });
    const QString padGo = QStringLiteral("#%1").arg(pad1 + 3);
    const QString padBack = QStringLiteral("#%1").arg(pad1 + 2);
    const QString padStop = QStringLiteral("#%1").arg(pad1 + 7);
    const QString padShift = QStringLiteral("#%1").arg(pad1);
    QVERIFY(lpd.tap(padGo));
    settle();
    QCOMPARE(int(commands.count(int(ShowControl::Go))), 1);
    QVERIFY(lpd.tap(padBack));
    settle();
    QCOMPARE(int(commands.count(int(ShowControl::Back))), 1);

    // Pad 8 alone: no stop, and the footer is told why.
    QSignalSpy fired(desk, &DeskRecord::fired);
    QSignalSpy refused(desk, &DeskRecord::refused);
    QVERIFY(lpd.tap(padStop));
    settle();
    QCOMPARE(fired.count(), 0);
    QCOMPARE(refused.count(), 1);
    // In the board's own words: its pads have no Shift or Stop printed on them.
    QCOMPARE(refused.first().first().toString(), QStringLiteral("%1: Pad 1 + Pad 8 stops.").arg(layout.shortName()));
    QVERIFY(!m_doc->outputRuleKeeper()->emergencyStopped());
    // Pad 1 held: Pad 8 stops.
    QVERIFY(lpd.chord({ padShift, padStop }));
    settle();
    QCOMPARE(fired.count(), 1);
    QCOMPARE(fired.first().first().toString(), QStringLiteral("ALL OFF"));
    QVERIFY(m_doc->outputRuleKeeper()->emergencyStopped());
    // Stopped: Pad 4 alone does not release it.
    const int gos = int(commands.count(int(ShowControl::Go)));
    QVERIFY(lpd.tap(padGo));
    settle();
    QCOMPARE(int(commands.count(int(ShowControl::Go))), gos);
    QVERIFY(m_doc->outputRuleKeeper()->emergencyStopped());
    QCOMPARE(refused.last().first().toString(),
             QStringLiteral("%1: stopped. Pad 1 + Pad 4 releases the stop.").arg(layout.shortName()));
    // STOP moved to Pad 7: the refusal names Pad 7, not the drawing's Pad 8.
    QString why;
    QVERIFY2(desk->assignRole(DeskRecord::Stop, 0, pad1 + 6, &why), qPrintable(why));
    QVERIFY(lpd.tap(QStringLiteral("#%1").arg(pad1 + 6)));
    settle();
    QCOMPARE(refused.last().first().toString(), QStringLiteral("%1: Pad 1 + Pad 7 stops.").arg(layout.shortName()));
    // K1 moving the master is the virtual bench's (theLpd8Mk2PadsReachTheirJobs): past the plug's dump window.
}

void SurfaceSim_Test::theLpd8Mk2PadsArePaintedBySysEx()
{
    // The message: 7 header bytes, 8 pads x 6, F7; 255 is 01 7F.
    const QByteArray white = SurfaceSetup::lpd8Mk2Colours({ QColor(255, 255, 255) });
    QCOMPARE(int(white.size()), 7 + 48 + 1);
    QCOMPARE(hex(white.left(13)), QStringLiteral("f0 47 7f 4c 06 00 30 01 7f 01 7f 01 7f"));
    QCOMPARE(white.mid(13, 42), QByteArray(42, 0));
    QCOMPARE(uchar(white.at(55)), uchar(0xF7));
    QCOMPARE(hex(SurfaceSetup::lpd8Mk2Colours({ QColor(0, 0, 0), QColor(130, 0, 5) }).mid(13, 6)),
             QStringLiteral("01 02 00 00 00 05"));

    const QString mk2 = QStringLiteral("Akai LPD8 mk2");
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("Akai-LPD8-mk2.qxi")) != nullptr);
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("Akai-LPD8.qxi")) != nullptr);
    const quint32 pad1 = (9 << 12) | (128 + 36);
    auto paints = [this]() {
        QList<QByteArray> out;
        for (const auto &m : std::as_const(m_stub->m_showControlSent))
            if (m.second.startsWith(QByteArray::fromHex("F0477F4C060030")))
                out << m.second;
        return out;
    };
    /* A pad's colour in a paint: its six bytes put back together. */
    auto padIn = [](const QByteArray &m, int pad) {
        auto v = [&m, pad](int c) { return (uchar(m.at(7 + pad * 6 + c * 2)) << 7) | uchar(m.at(8 + pad * 6 + c * 2)); };
        return QColor(v(0), v(1), v(2));
    };
    m_stub->m_showControlSent.clear();
    SimMidiDevice board(m_doc, m_stub, QStringLiteral("LPD8 mk2"), 0, mk2);
    QVERIFY2(board.plug(), qPrintable(board.lastError()));
    settle();
    SurfaceSetup *setup = m_doc->surfaceSetup();
    QCOMPARE(setup->universes(), QList<quint32>() << 0);
    // Patched: the pads painted (none lit yet), out of its own port.
    QVERIFY(!paints().isEmpty());
    QCOMPARE(int(m_stub->m_showControlSent.last().first), board.line());
    QVERIFY(setup->lastCheck(0).text.contains(QStringLiteral("pads' colours")));

    QTRY_VERIFY(!setup->initAgainPending(0));
    settle(50);

    /* GO's lamp (Pad 4) and a free pad's (Pad 6), sent in one turn: none
       goes out at once, one paint of the whole board follows. */
    InputOutputMap *iom = m_doc->inputOutputMap();
    m_stub->m_showControlSent.clear();
    iom->sendFeedBack(0, pad1 + 3, 255, QVariant());
    iom->sendFeedBack(0, pad1 + 5, 255, QVariant());
    iom->sendFeedBack(0, 70, 200, QVariant());              // K1 (the GM): no pad
    QCOMPARE(int(paints().size()), 0);                      // not one per lamp
    QList<QColor> now = setup->padColours(0);
    QCOMPARE(int(now.size()), 8);
    QCOMPARE(now.at(3), QColor(0, 255, 0));                 // GO: green
    QCOMPARE(now.at(5), QColor(0, 90, 255));                // a free pad: blue
    QCOMPARE(now.at(0), QColor(0, 0, 0));                   // Shift, unlit
    settle();
    QVERIFY(!paints().isEmpty());
    QCOMPARE(paints().last(), SurfaceSetup::lpd8Mk2Colours(setup->padColours(0)));
    QVERIFY(int(paints().size()) <= 2);
    // Dim ("ready"), then off.
    iom->sendFeedBack(0, pad1 + 3, 30, QVariant());
    QVERIFY(setup->padColours(0).at(3).green() > 0 && setup->padColours(0).at(3).green() < 64);
    iom->sendFeedBack(0, pad1 + 3, 0, QVariant());
    QCOMPARE(setup->padColours(0).at(3), QColor(0, 0, 0));
    // STOP's lamp is red, Back's amber, Shift's white.
    iom->sendFeedBack(0, pad1 + 7, 255, QVariant());
    iom->sendFeedBack(0, pad1 + 2, 255, QVariant());
    iom->sendFeedBack(0, pad1, 255, QVariant());
    QCOMPARE(setup->padColours(0).at(7), QColor(255, 0, 0));
    QCOMPARE(setup->padColours(0).at(2), QColor(255, 150, 0));
    QCOMPARE(setup->padColours(0).at(0), QColor(255, 255, 255));
    settle();
    QCOMPARE(padIn(paints().last(), 7), setup->padColours(0).at(7));
    // A replug: painted as the app has them, not left in the board's own colours.
    QVERIFY(board.unplug());
    m_stub->m_showControlSent.clear();
    QVERIFY(board.replug());
    QTRY_VERIFY(!paints().isEmpty());
    QTRY_VERIFY(!setup->initAgainPending(0));
    settle(50);
    QCOMPARE(paints().last(), SurfaceSetup::lpd8Mk2Colours(setup->padColours(0)));

    // The lamp test lights each pad through the same message.
    m_stub->m_showControlSent.clear();
    setup->setLampTestStepMs(5);
    QString why;
    QVERIFY2(setup->startLampTest(0, &why), qPrintable(why));
    QTRY_VERIFY_WITH_TIMEOUT(!setup->lampTestRunning(), 3000);
    bool sawPad1 = false;
    for (const QByteArray &p : paints())
        sawPad1 = sawPad1 || padIn(p, 0) != QColor(0, 0, 0);
    QVERIFY2(sawPad1, "the lamp test never lit Pad 1");

    // The original LPD8 lights by note: never sent the mk2's SysEx.
    m_stub->m_showControlSent.clear();
    SimMidiDevice old(m_doc, m_stub, QStringLiteral("LPD8"), 1, QStringLiteral("Akai LPD8"));
    QVERIFY2(old.plug(), qPrintable(old.lastError()));
    iom->sendFeedBack(1, 128 + 39, 255, QVariant());
    settle();
    for (const auto &sent : std::as_const(m_stub->m_showControlSent))
        QVERIFY2(sent.first != quint32(old.line()), qPrintable(hex(sent.second)));
}

void SurfaceSim_Test::anOlderCopyOfASurfaceProfileIsNotUsed()
{
    // A copy that lacks controls the layout draws: the old QLC+ APC40 mkII.
    QFile built(apcFile());
    QVERIFY(built.open(QIODevice::ReadOnly));
    QString text = QString::fromUtf8(built.readAll());
    const QRegularExpression block(QStringLiteral("<Channel Number=\"4117\">.*?</Channel>\\r?\\n"),
                                   QRegularExpression::DotMatchesEverythingOption);
    QVERIFY(text.contains(block));
    QString old = text;
    old.remove(block);
    QTemporaryDir user;
    QVERIFY(user.isValid());
    const QString oldPath = user.filePath(QStringLiteral("Akai-APC40-mkII.qxi"));
    QFile out(oldPath);
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write(old.toUtf8());
    out.close();

    auto dirOf = [](const QString &path) {
        QDir d(path);
        d.setFilter(QDir::Files);
        d.setNameFilters(QStringList() << QStringLiteral("*.qxi"));
        return d;
    };
    InputOutputMap *iom = m_doc->inputOutputMap();
    iom->loadProfiles(dirOf(user.path()));              // the user's folder first, as the app does
    iom->loadProfiles(dirOf(profileDir()));
    QLCInputProfile *used = iom->profile(kApc);
    QVERIFY(used != nullptr);
    QVERIFY2(used->channel(4117) != nullptr, "the user's older copy shadows the built-in profile");
    QVERIFY(QDir::cleanPath(used->path()) != QDir::cleanPath(oldPath));
    QCOMPARE(iom->shadowedProfiles().size(), 1);
    QVERIFY(iom->shadowedProfiles().first().builtInUsed);
    QCOMPARE(QDir::cleanPath(iom->shadowedProfiles().first().path), QDir::cleanPath(oldPath));
    const QStringList notes = iom->shadowedProfileNotes();
    QCOMPARE(notes.size(), 1);
    QVERIFY2(notes.first().contains(QDir::cleanPath(oldPath)) && notes.first().contains(QStringLiteral("older")),
             qPrintable(notes.first()));
    QVERIFY2(QFileInfo::exists(oldPath), "the user's file is left alone");
    // The board's Match is still the type's: a patch by name finds it.
    QCOMPARE(iom->profileForDevice(QStringLiteral("APC40 mkII")), kApc);

    // A copy that still fits the layout is the user's own, and is used.
    QTemporaryDir fresh;
    QVERIFY(fresh.isValid());
    const QString goodPath = fresh.filePath(QStringLiteral("Akai-APC40-mkII.qxi"));
    QVERIFY(QFile::copy(apcFile(), goodPath));
    Doc other(this);
    other.inputOutputMap()->loadProfiles(dirOf(fresh.path()));
    other.inputOutputMap()->loadProfiles(dirOf(profileDir()));
    QCOMPARE(QDir::cleanPath(other.inputOutputMap()->profile(kApc)->path()), QDir::cleanPath(goodPath));
    QVERIFY(other.inputOutputMap()->shadowedProfileNotes().isEmpty());
}

void SurfaceSim_Test::aGoneBoardHandsItsRolesToTheNewOne()
{
    const QString maizeName = QStringLiteral("MaizeDMX Playback Controller");
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("MaizeDMX-Playback-Controller.qxi")) != nullptr);
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("Akai-APC64.qxi")) != nullptr);
    SimMidiDevice maize(m_doc, m_stub, QStringLiteral("USB2.0-MIDI Port 1"), 0, maizeName);
    QVERIFY2(maize.plug(), qPrintable(maize.lastError()));
    settle();
    DeskRecord *desk = m_doc->deskRecord();
    const QString maizeId = desk->identityOfUniverse(0);
    QCOMPARE(desk->binding(DeskRecord::Go).identity, maizeId);
    // The old board is unplugged; a new surface comes online.
    QVERIFY(maize.unplug());
    settle();
    QSignalSpy cut(desk, &DeskRecord::cutOver);
    SimMidiDevice apc64(m_doc, m_stub, QStringLiteral("APC64 DAW"), 1, QStringLiteral("Akai APC64"));
    QVERIFY2(apc64.plug(), qPrintable(apc64.lastError()));
    settle();
    QCOMPARE(cut.count(), 1);
    const QString apcId = desk->identityOfUniverse(1);
    QVERIFY(apcId != maizeId);
    // The core roles (the spotter's hazard roles are learned, never seeded).
    for (DeskRecord::Role r : DeskRecord::roles())
        if (!DeskRecord::isHazardRole(r))
            QCOMPARE(desk->binding(r).identity, apcId);
    QCOMPARE(desk->binding(DeskRecord::Go).channel, quint32(128 + 91));
    QCOMPARE(desk->binding(DeskRecord::Back).channel, quint32(128 + 92));
    QCOMPARE(desk->binding(DeskRecord::Stop).channel, quint32(128 + 119));
    QCOMPARE(desk->binding(DeskRecord::GrandMaster).channel, quint32((7 << 12) | 513));
    const QString text = cut.first().first().toString();
    QVERIFY2(text.contains(QStringLiteral("GO, Back, STOP and Grand Master moved to APC64 from USB2.0-MIDI Port 1")),
             qPrintable(text));
    QVERIFY(desk->isLive(DeskRecord::Go));

    // Undo: the roles go back to the board they came from, and stay there.
    QVERIFY(desk->canUndoCutOver());
    QVERIFY(desk->undoCutOver());
    QCOMPARE(desk->binding(DeskRecord::Go).identity, maizeId);
    QCOMPARE(desk->binding(DeskRecord::Go).channel, quint32(128 + 13));
    QCOMPARE(desk->binding(DeskRecord::GrandMaster).channel, quint32(10));
    QVERIFY(!desk->canUndoCutOver());
    QVERIFY(!desk->seedDefaults());
    QCOMPARE(desk->binding(DeskRecord::Go).identity, maizeId);
    QCOMPARE(cut.count(), 1);
}

void SurfaceSim_Test::aBoardThatIsHereKeepsItsRoles()
{
    const QString maizeName = QStringLiteral("MaizeDMX Playback Controller");
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("MaizeDMX-Playback-Controller.qxi")) != nullptr);
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("Akai-APC64.qxi")) != nullptr);
    SimMidiDevice maize(m_doc, m_stub, QStringLiteral("USB2.0-MIDI Port 1"), 0, maizeName);
    QVERIFY2(maize.plug(), qPrintable(maize.lastError()));
    settle();
    DeskRecord *desk = m_doc->deskRecord();
    const QString maizeId = desk->identityOfUniverse(0);
    QSignalSpy cut(desk, &DeskRecord::cutOver);
    // A role the Maize has no default for: free, so the APC64 may take it later.
    SimMidiDevice apc64(m_doc, m_stub, QStringLiteral("APC64 DAW"), 1, QStringLiteral("Akai APC64"));
    QVERIFY2(apc64.plug(), qPrintable(apc64.lastError()));
    settle();
    QCOMPARE(cut.count(), 0);
    for (DeskRecord::Role r : DeskRecord::roles())
        if (!DeskRecord::isHazardRole(r))
            QCOMPARE(desk->binding(r).identity, maizeId);
    QVERIFY(!desk->canUndoCutOver());
    // Its replug moves nothing either.
    QVERIFY(SurfaceScenarios::unplugReplug(&apc64));
    settle();
    QCOMPARE(desk->binding(DeskRecord::Go).identity, maizeId);

    // The Maize lets go of GO: the APC64, which waited for it, is given it.
    QVERIFY(desk->clear(DeskRecord::Go));
    QVERIFY(desk->seedDefaults());
    QCOMPARE(desk->binding(DeskRecord::Go).identity, desk->identityOfUniverse(1));
    QCOMPARE(desk->binding(DeskRecord::Go).channel, quint32(128 + 91));
    QCOMPARE(desk->binding(DeskRecord::Back).identity, maizeId);
    QCOMPARE(cut.count(), 0);
}

void SurfaceSim_Test::aLampForAControlTheBoardLacksIsNotSent()
{
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("Akai-APCMini-mk2.qxi")) != nullptr);
    SimMidiDevice mini(m_doc, m_stub, QStringLiteral("APC mini mk2 Control"), 0);
    QVERIFY2(mini.plug(), qPrintable(mini.lastError()));
    const quint32 list = runList(m_doc);
    ControlMap *cm = m_doc->controlMap();
    mini.forgetLamps();
    const quint32 mine = 128 + 10;              // a pad of the board
    const quint32 foreign = (8 << 12) | 129;    // MIDI channel 9, note 1: learned on the PMJ
    cm->setBinding({ 0, mine, ControlMap::cueList(list), ControlMap::Toggle });
    cm->setBinding({ 0, foreign, ControlMap::cueList(list), ControlMap::Toggle });
    cm->resendFeedback();
    settle(50);
    QVERIFY2(mini.lampMessages(mine) >= 1, "the board's own pad gets its lamp");
    QCOMPARE(mini.lampMessages(foreign), 0);
}

void SurfaceSim_Test::theMaizeDmxHasAPager()
{
    const QString name = QStringLiteral("MaizeDMX Playback Controller");
    QLCInputProfile *p = mapProfile(m_doc, profileDir() + QStringLiteral("MaizeDMX-Playback-Controller.qxi"));
    QVERIFY(p != nullptr);
    const QList<PageSet> sets = PageSet::forProfile(name);
    QVERIFY(!sets.isEmpty());
    const PageSet &set = sets.first();
    QCOMPARE(set.pages().size(), 2);
    QCOMPARE(set.pages().at(0).name, QStringLiteral("Colour"));
    QCOMPARE(set.pages().at(1).name, QStringLiteral("Move"));
    QCOMPARE(set.pages().at(0).key, quint32(171));      // X7
    QCOMPARE(set.pages().at(1).key, quint32(172));      // X9
    QVERIFY2(set.check(SurfaceLayout::forProfile(name), p).isEmpty(),
             qPrintable(set.check(SurfaceLayout::forProfile(name), p).join(QStringLiteral("\n"))));
    // The keys are neither a desk role nor a lamp-less control: both have a lamp.
    const SurfaceLayout layout = SurfaceLayout::forProfile(name);
    for (quint32 k : { quint32(171), quint32(172) })
    {
        QVERIFY(layout.roleOn(k).isEmpty());
        QCOMPARE(layout.controls().at(layout.indexOf(k)).lamp, SurfaceLayout::Led);
    }
    // The LEDs above the faders are lights, never knobs.
    int leds = 0;
    for (const SurfaceLayout::Control &c : layout.controls())
    {
        QVERIFY2(c.kind != SurfaceLayout::Knob && c.kind != SurfaceLayout::Encoder, qPrintable(c.face));
        leds += c.kind == SurfaceLayout::Indicator ? 1 : 0;
    }
    QCOMPARE(leds, 11);
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

void SurfaceSim_Test::identifyHoldsTheKeysJobs()
{
    QVERIFY(mapProfile(m_doc, pmjFile()) != nullptr);
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj);
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SimOpenDeck deck(m_stub, &pmj);
    runList(m_doc);
    QVERIFY(deskOn(m_doc, DeskRecord::Stop, 0, kPmj, kFavorites));
    QVERIFY(deskOn(m_doc, DeskRecord::Go, 0, kPmj, kGo));
    QVERIFY(deskOn(m_doc, DeskRecord::Back, 0, kPmj, kBack));
    // A show trigger on 1-Down (note 1): a scene.
    Fixture *fxi = m_doc->fixtures().isEmpty() ? nullptr : m_doc->fixtures().first();
    QVERIFY(fxi != nullptr);
    Scene *sc = new Scene(m_doc);
    sc->setName(QStringLiteral("Warm"));
    sc->addFixture(fxi->id());
    sc->setValue(fxi->id(), 0, 255);
    m_doc->addFunction(sc);
    const quint32 down1 = pmj.channelOf(QStringLiteral("1-Down"));
    m_doc->controlMap()->setBinding({ 0, down1, ControlMap::function(sc->id()), ControlMap::Toggle });
    ControlMap *cm = m_doc->controlMap();
    cm->setDumpWindowMs(0);
    QList<int> commands;
    m_doc->showControl()->setRunHandler([&commands](const ShowControl::Request &r, ShowControl::Route) {
        commands << int(r.command);
        return true;
    });
    auto count = [&commands](ShowControl::Command c) { return int(commands.count(int(c))); };
    QTest::qWait(300);      // past a fresh patch's dump window

    SurfaceSetup *setup = m_doc->surfaceSetup();
    QString error;
    QVERIFY2(setup->beginIdentify(0, false, &error), qPrintable(error));
    QCOMPARE(m_doc->inputOutputMap()->setupListening(), quint32(0));
    // The presses still reach what pairs them (raw input)...
    QSignalSpy raw(m_doc->inputOutputMap(), &InputOutputMap::inputValueChanged);
    QVERIFY(pmj.tap(QStringLiteral("Go")));
    QVERIFY(pmj.tap(QStringLiteral("Back")));
    QVERIFY(pmj.tap(QStringLiteral("1-Down")));
    settle();
    m_doc->masterTimer()->timerTick();
    QCoreApplication::processEvents();
    bool heardGo = false;
    for (const QList<QVariant> &a : std::as_const(raw))
        heardGo = heardGo || (a.at(1).toUInt() == kGo && a.at(2).toUInt() > 0);
    QVERIFY(heardGo);
    SurfaceSetup::LampPair pair;
    QVERIFY(SurfaceSetup::pairFromChannel(kGo, 17, &pair));
    // ...and do no job: no GO, no Back, no scene.
    QCOMPARE(count(ShowControl::Go), 0);
    QCOMPARE(count(ShowControl::Back), 0);
    QVERIFY2(!sc->isRunning(), "1-Down played its scene during Identify");
    // The desk's STOP still stops.
    QVERIFY(pmj.tap(QStringLiteral("Favorites")));
    settle();
    QCOMPARE(count(ShowControl::AllOff), 1);

    QVERIFY2(setup->endIdentify(0, { pair }, &error), qPrintable(error));
    QCOMPARE(m_doc->inputOutputMap()->setupListening(), UINT_MAX);
    // Identify over: Go is the desk's GO again.
    QTest::qWait(300);
    QVERIFY(pmj.tap(QStringLiteral("Go")));
    settle();
    QCOMPARE(count(ShowControl::Go), 1);
    m_doc->showControl()->setRunHandler(ShowControl::RunHandler());
    cm->setDumpWindowMs(500);
}

void SurfaceSim_Test::identifyHasTheLampsThenGivesThemBack()
{
    QVERIFY(mapProfile(m_doc, pmjFile()) != nullptr);
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj);
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SimOpenDeck deck(m_stub, &pmj);
    // A show trigger on 1-Down: the Control Map owns its lamp.
    Scene *sc = new Scene(m_doc);
    m_doc->addFunction(sc);
    const quint32 down1 = pmj.channelOf(QStringLiteral("1-Down"));
    m_doc->controlMap()->setBinding({ 0, down1, ControlMap::function(sc->id()), ControlMap::Toggle });
    settle();
    InputOutputMap *iom = m_doc->inputOutputMap();
    auto sentTo = [this](quint32 channel) {
        int n = 0;
        for (const QPair<quint32, uchar> &f : std::as_const(m_stub->m_feedback))
            n += f.first == channel ? 1 : 0;
        return n;
    };

    SurfaceSetup *setup = m_doc->surfaceSetup();
    QString error;
    QVERIFY2(setup->beginIdentify(0, false, &error), qPrintable(error));
    m_stub->m_feedback.clear();
    // Identify flashes lamp after lamp: nothing else lights the board meanwhile.
    QVERIFY(!iom->sendFeedBack(0, down1, 255, QVariant()));
    m_doc->controlMap()->resendFeedback();
    QCOMPARE(sentTo(down1), 0);
    // Its own flashes still go out.
    setup->flashLamp(0, 3, true);
    QCOMPARE(sentTo(128 + 3), 1);

    // Identify over: each owner sends its own lamp again, recorded as sent.
    m_stub->m_feedback.clear();
    QVERIFY2(setup->endIdentify(0, {}, &error), qPrintable(error));
    QCOMPARE(iom->setupListening(), UINT_MAX);
    QVERIFY2(sentTo(down1) >= 1, "1-Down's lamp was not sent again after Identify");
    uchar last = 0, wire = 0;
    for (const QPair<quint32, uchar> &f : std::as_const(m_stub->m_feedback))
        if (f.first == down1)
            wire = f.second;
    QVERIFY(iom->lastFeedback(0, down1, &last));
    QCOMPARE(last, wire);
}

void SurfaceSim_Test::theApc64IsNamedAndItsModeChecked()
{
    // The shipped profile (resources/inputprofiles/Akai-APC64.qxi): its pads on channel 7.
    QLCInputProfile *p = mapProfile(m_doc, profileDir() + QStringLiteral("Akai-APC64.qxi"));
    QVERIFY(p != nullptr);
    const QString name = QStringLiteral("Akai APC64");
    QCOMPARE(p->name(), name);
    QVERIFY(p->channel(quint32((6 << 12) | 128)) != nullptr);   // Pad 8-1

    SimMidiDevice board(m_doc, m_stub, QStringLiteral("APC64 DAW"), 0, name);
    SimApc64 apc(m_stub, &board);
    QVERIFY2(board.plug(), qPrintable(board.lastError()));
    SurfaceSetup *setup = m_doc->surfaceSetup();
    QCOMPARE(setup->universes(), QList<quint32>() << 0);
    // Patched: the display is the app's, and says what it is.
    QVERIFY(apc.owned());
    QCOMPARE(apc.text(0), QStringLiteral("qlc"));
    QCOMPARE(apc.text(1), QStringLiteral("console"));
    QCOMPARE(apc.text(2), QString());
    QCOMPARE(apc.malformed(), 0);
    QCOMPARE(hex(SurfaceSetup::apc64Text(0, QStringLiteral("qlc"))),
             QStringLiteral("f0 47 00 53 10 00 05 00 71 6c 63 00 f7"));
    QCOMPARE(hex(SurfaceSetup::apc64Text(2, QString())), QStringLiteral("f0 47 00 53 10 00 02 02 00 f7"));

    // Its power cut and back (a replug): named again.
    QVERIFY(board.unplug());
    apc.powerCycle();
    QVERIFY(board.replug());
    QTRY_VERIFY(apc.owned());
    QCOMPARE(apc.text(1), QStringLiteral("console"));

    // Session mode: asked for by a press. A pad on channel 1 is another mode.
    QVERIFY(setup->lastCheck(0).text.contains(QStringLiteral("Session mode: not checked yet")));
    Scene *sc = new Scene(m_doc);
    m_doc->addFunction(sc);
    const quint32 pad1 = quint32((6 << 12) | 128);
    m_doc->controlMap()->setBinding({ 0, pad1, ControlMap::function(sc->id()), ControlMap::Toggle });
    QVERIFY(setup->beginModeCheck(0));
    QCOMPARE(setup->lastCheck(0).state, SurfaceSetup::Check::WaitingForPress);
    QVERIFY(setup->lastCheck(0).text.startsWith(QStringLiteral("Press SESSION on the APC64")));
    QVERIFY(board.noteOn(1, 0, 127));
    QVERIFY(board.noteOff(1, 0));
    QCOMPARE(setup->lastCheck(0).state, SurfaceSetup::Check::WrongMode);
    QVERIFY2(setup->lastCheck(0).text.contains(QStringLiteral("channel 1")), qPrintable(setup->lastCheck(0).text));
    QTRY_COMPARE(m_doc->inputOutputMap()->setupListening(), UINT_MAX);
    QVERIFY(setup->beginModeCheck(0));
    QVERIFY(board.noteOn(7, 0, 127));
    QVERIFY(board.noteOff(7, 0));
    m_doc->masterTimer()->timerTick();
    QCoreApplication::processEvents();
    QCOMPARE(setup->lastCheck(0).state, SurfaceSetup::Check::InMode);
    QVERIFY2(!sc->isRunning(), "the pad that answered the mode check played its trigger");
    QVERIFY(!setup->isWaitingForPress(0));

    // The app lets go: the display is the APC64's again.
    {
        SurfaceSetup leaving(m_doc);
    }
    QVERIFY(!apc.owned());
}


namespace
{
const QStringList kApc64Ports = { QStringLiteral("APC64 DAW (APC64)"), QStringLiteral("APC64 Notes (APC64)"),
                                  QStringLiteral("APC64 MIDI (APC64)"), QStringLiteral("APC64 Custom (APC64)") };

void loadAllProfiles(Doc *doc)
{
    doc->inputOutputMap()->loadProfiles(QDir(profileDir()));
}
}

void SurfaceSim_Test::realPortNamesFindTheirProfiles()
{
    loadAllProfiles(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    // As CoreMIDI names them on dink (2026-11-27).
    QCOMPARE(iom->profileForDevice("APC40 mkII"), QString("Akai APC40 mkII"));
    QCOMPARE(iom->profileForDevice("APC64 DAW (APC64)"), QString("Akai APC64"));
    QCOMPARE(iom->profileForDevice("APC mini mk2 Control"), QString("Akai APC Mini mk2"));
    QCOMPARE(iom->profileForDevice("PreSonus FP2"), QString("PreSonus FaderPort 2"));
    // Case, a Windows unit, a Linux port suffix, the maker's name.
    QCOMPARE(iom->profileForDevice("apc40 MKII"), QString("Akai APC40 mkII"));
    QCOMPARE(iom->profileForDevice("2- APC40 mkII"), QString("Akai APC40 mkII"));
    QCOMPARE(iom->profileForDevice("APC40 mkII MIDI 1"), QString("Akai APC40 mkII"));
    QCOMPARE(iom->profileForDevice("Akai APC40 mkII"), QString("Akai APC40 mkII"));
    QCOMPARE(iom->profileForDevice("PreSonus FP2 (PreSonus FP2)"), QString("PreSonus FaderPort 2"));
    // Never another model's: the mk1 profiles do not take the mk2 boards.
    QCOMPARE(iom->profileForDevice("APC40"), QString("Akai APC40"));
    QCOMPARE(iom->profileForDevice("APC MINI"), QString("Akai APC Mini"));
    // A board's other ports: not it, but known as part of it.
    for (const QString &port : { QString("APC64 Notes (APC64)"), QString("APC64 MIDI (APC64)"),
                                 QString("APC64 Custom (APC64)") })
    {
        QCOMPARE(iom->profileForDevice(port), QString());
        QCOMPARE(iom->profileOfOtherPort(port), QString("Akai APC64"));
    }
    QCOMPARE(iom->profileForDevice("APC mini mk2 Notes"), QString());
    QCOMPARE(iom->profileOfOtherPort("APC mini mk2 Notes"), QString("Akai APC Mini mk2"));
    QCOMPARE(iom->profileOfOtherPort("APC64 DAW (APC64)"), QString());
    // The MaizeDMX's port is a name many boxes share: asked about, never matched.
    QCOMPARE(iom->profileForDevice("USB2.0-MIDI Port 1"), QString());
    QCOMPARE(iom->profileToAskFor("USB2.0-MIDI Port 1"), QString("MaizeDMX Playback Controller"));
    // Something else entirely: nothing.
    QCOMPARE(iom->profileForDevice("Launchkey MK4 49 MIDI Out"), QString());
    QCOMPARE(iom->profileOfOtherPort("Launchkey MK4 49 MIDI Out"), QString());
}

void SurfaceSim_Test::aKnownBoardIsSetUpWhenPluggedIn()
{
    loadAllProfiles(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    SurfaceAutoPatch *ap = m_doc->surfaceSetup()->autoPatch();
    SurfaceAutoPatch::setDeclined(kApc64Ports.first(), false);
    ap->setDelay(0);
    ap->setEnabled(true, false);
    m_stub->m_linesAreHardware = true;
    m_stub->m_networkDmx = false;
    m_stub->m_hasFeedback = true;
    m_stub->m_acceptShowControl = true;
    m_stub->m_showControlSent.clear();
    QSignalSpy patched(ap, &SurfaceAutoPatch::autoPatched);
    // Plugged in: its four ports appear.
    m_stub->m_lineNames = kApc64Ports;
    emit m_stub->configurationChanged();
    QTRY_COMPARE(patched.count(), 1);
    const quint32 u = patched.first().at(0).toUInt();
    QCOMPARE(patched.first().at(1).toString(), QString("Akai APC64"));
    InputPatch *ip = iom->inputPatch(u);
    QVERIFY(ip != nullptr);
    QCOMPARE(ip->inputName(), QString("APC64 DAW (APC64)"));
    QCOMPARE(ip->profileName(), QString("Akai APC64"));
    QVERIFY(iom->feedbackPatch(u) != nullptr && iom->feedbackPatch(u)->isPatched());
    QCOMPARE(iom->feedbackPatch(u)->outputName(), QString("APC64 DAW (APC64)"));
    // The other ports: no universe has them.
    int inputs = 0;
    for (quint32 i = 0; i < iom->universesCount(); ++i)
        if (iom->inputPatch(i) != nullptr && iom->inputPatch(i)->plugin() != nullptr)
            ++inputs;
    QCOMPARE(inputs, 1);
    // Its init (the display taken) went out of its DAW port, and is in its log.
    bool display = false;
    for (const auto &m : std::as_const(m_stub->m_showControlSent))
        display = display || (hex(m.second) == QStringLiteral("f0 47 00 53 1c 00 01 01 f7")
                              && m.first == quint32(kApc64Ports.indexOf("APC64 DAW (APC64)")));
    QVERIFY(display);
    const QString log = m_doc->surfaceSetup()->boardLog()->text(u);
    QVERIFY2(log.contains("Recognised APC64 DAW (APC64) as Akai APC64"), qPrintable(log));
    QVERIFY2(log.contains("F0 47 00 53 1C 00 01 01 F7"), qPrintable(log));

    // Undo: let go, and not set up by itself again when replugged.
    QVERIFY(ap->undo(u));
    QVERIFY(iom->inputPatch(u) == nullptr || iom->inputPatch(u)->plugin() == nullptr);
    m_stub->m_lineNames = QStringList() << QStringLiteral("(no device)");
    emit m_stub->configurationChanged();
    m_stub->m_lineNames = kApc64Ports;
    emit m_stub->configurationChanged();
    settle(50);
    QCOMPARE(patched.count(), 1);
    SurfaceAutoPatch::setDeclined(kApc64Ports.first(), false);
    ap->setEnabled(false);
    m_stub->m_hasFeedback = false;
    m_stub->m_acceptShowControl = false;
}

void SurfaceSim_Test::aSharedPortNameIsAskedAboutThenRemembered()
{
    loadAllProfiles(m_doc);
    const QString maize = QStringLiteral("USB2.0-MIDI Port 1");
    InputOutputMap::setConfirmedProfile(maize, QString());
    InputOutputMap *iom = m_doc->inputOutputMap();
    SurfaceAutoPatch *ap = m_doc->surfaceSetup()->autoPatch();
    ap->setDelay(0);
    ap->setEnabled(true, false);
    m_stub->m_linesAreHardware = true;
    m_stub->m_networkDmx = false;
    m_stub->m_hasFeedback = true;
    QSignalSpy patched(ap, &SurfaceAutoPatch::autoPatched);
    QSignalSpy asked(ap, &SurfaceAutoPatch::askToUse);
    m_stub->m_lineNames = QStringList() << maize;     // its Port 2 is an output only
    emit m_stub->configurationChanged();
    QTRY_COMPARE(asked.count(), 1);
    QCOMPARE(asked.first().at(1).toString(), maize);
    QCOMPARE(asked.first().at(2).toString(), QString("MaizeDMX Playback Controller"));
    settle(20);
    QCOMPARE(patched.count(), 0);
    // "Use It": set up, and this computer knows it from now on.
    const quint32 u = ap->useAs(m_stub, maize, QStringLiteral("MaizeDMX Playback Controller"));
    QVERIFY(u != UINT_MAX);
    QCOMPARE(iom->inputPatch(u)->profileName(), QString("MaizeDMX Playback Controller"));
    QCOMPARE(iom->feedbackPatch(u)->outputName(), maize);
    QCOMPARE(iom->profileForDevice(maize), QString("MaizeDMX Playback Controller"));
    InputOutputMap::setConfirmedProfile(maize, QString());
    QCOMPARE(iom->profileForDevice(maize), QString());
    ap->setEnabled(false);
    m_stub->m_hasFeedback = false;
}

namespace
{
// dink's MIDI ports on 2026-11-27, in CoreMIDI's order.
const QStringList kDinkPorts = { QStringLiteral("Launchkey MK4 49 MIDI Out"), QStringLiteral("Launchkey MK4 49 DAW Out"),
                                 QStringLiteral("Network RTP Session 1"), QStringLiteral("OpenDeck | PMJ_BLACK_1") };
const QString kPmjPort = QStringLiteral("OpenDeck | PMJ_BLACK_1");
}

/* Branson, build 4910: "LEDs are not lighting on the PMJ still" -- and "when
   that board comes online it should be configured correctly". His show's U01
   had the PMJ as its input and its feedback on "Network RTP Session 1", left
   by the APC40 patched there before: every lamp went to the session. When
   the show loads with the PMJ here, and when the PMJ is plugged in again,
   the feedback is put on the PMJ's own port, said with Undo, and logged;
   Undo puts his patch back and it is then left alone. */
void SurfaceSim_Test::aBoardBackWithAStaleFeedbackIsRepaired()
{
    loadAllProfiles(m_doc);
    QSettings().remove(QStringLiteral("surfaces/keepPatch/") + kPmjPort);
    InputOutputMap *iom = m_doc->inputOutputMap();
    SurfaceAutoPatch *ap = m_doc->surfaceSetup()->autoPatch();
    ap->setDelay(0);
    m_stub->m_linesAreHardware = true;
    m_stub->m_networkDmx = false;
    m_stub->m_hasFeedback = true;
    m_stub->m_acceptShowControl = true;
    m_stub->m_lineNames = kDinkPorts;
    emit m_stub->configurationChanged();
    QCOMPARE(iom->profileForDevice(kPmjPort), QString("PMJ Black 1"));
    QCOMPARE(iom->profileForDevice("Network RTP Session 1"), QString());
    auto hisPatch = [&]() {
        QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 3, QStringLiteral("PMJ Black 1")));
        QVERIFY(iom->setOutputPatch(0, m_stub->name(), QString(), 2, true));
        QCOMPARE(iom->feedbackPatch(0)->outputName(), QString("Network RTP Session 1"));
    };
    hisPatch();
    QSignalSpy repaired(ap, &SurfaceAutoPatch::repaired);

    // The show loaded (or the app started) with the PMJ here: the scan.
    ap->setEnabled(true, true);
    QCOMPARE(repaired.count(), 1);
    QCOMPARE(repaired.first().at(0).toUInt(), 0u);
    QCOMPARE(repaired.first().at(1).toString(), QString("PMJ Black 1"));
    QCOMPARE(repaired.first().at(2).toString(), QString("feedback repaired to its own port"));
    QCOMPARE(iom->feedbackPatch(0)->outputName(), kPmjPort);
    QCOMPARE(iom->inputPatch(0)->profileName(), QString("PMJ Black 1"));
    const QString log = m_doc->surfaceSetup()->boardLog()->text(0);
    QVERIFY2(log.contains("Repaired PMJ Black 1: feedback repaired to its own port (it was Network RTP Session 1)"),
             qPrintable(log));
    // A lamp now reaches the PMJ's port.
    m_stub->m_feedbackLines.clear();
    QVERIFY(iom->sendFeedBack(0, 5, 127, QVariant()));
    QCOMPARE(m_stub->m_feedbackLines, QList<quint32>({ 3u }));
    // Nothing more to do on the next scan.
    ap->scanNow();
    QCOMPARE(repaired.count(), 1);

    // Undo: his patch back, and left as it is from then on.
    QVERIFY(ap->undoRepair(0));
    QCOMPARE(iom->feedbackPatch(0)->outputName(), QString("Network RTP Session 1"));
    ap->scanNow();
    QCOMPARE(repaired.count(), 1);
    QSettings().remove(QStringLiteral("surfaces/keepPatch/") + kPmjPort);

    // Plugged out and in again with the stale patch: repaired on its return.
    ap->scanNow();
    QCOMPARE(repaired.count(), 2);
    hisPatch();
    m_stub->m_lineNames = kDinkPorts.mid(0, 3);
    emit m_stub->configurationChanged();
    settle(20);
    m_stub->m_lineNames = kDinkPorts;
    emit m_stub->configurationChanged();
    QTRY_COMPARE(repaired.count(), 3);
    QCOMPARE(iom->feedbackPatch(0)->outputName(), kPmjPort);

    ap->setEnabled(false);
    m_stub->m_hasFeedback = false;
    m_stub->m_acceptShowControl = false;
}

/* A feedback set on purpose to another port that is here and is not a
   board (a Launchkey's) is a choice: never touched. A missing profile is
   still put right. */
void SurfaceSim_Test::aFeedbackChosenOnPurposeIsLeft()
{
    loadAllProfiles(m_doc);
    QSettings().remove(QStringLiteral("surfaces/keepPatch/") + kPmjPort);
    InputOutputMap *iom = m_doc->inputOutputMap();
    SurfaceAutoPatch *ap = m_doc->surfaceSetup()->autoPatch();
    ap->setDelay(0);
    m_stub->m_linesAreHardware = true;
    m_stub->m_networkDmx = false;
    m_stub->m_hasFeedback = true;
    m_stub->m_lineNames = kDinkPorts;
    emit m_stub->configurationChanged();
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 3, QStringLiteral("PMJ Black 1")));
    QVERIFY(iom->setOutputPatch(0, m_stub->name(), QString(), 0, true));
    QSignalSpy repaired(ap, &SurfaceAutoPatch::repaired);
    ap->setEnabled(true, true);
    QCOMPARE(repaired.count(), 0);
    QCOMPARE(iom->feedbackPatch(0)->outputName(), QString("Launchkey MK4 49 MIDI Out"));

    // No profile: given its own, the feedback still left.
    QVERIFY(iom->setInputProfile(0, QString()));
    ap->scanNow();
    QCOMPARE(repaired.count(), 1);
    QCOMPARE(repaired.first().at(2).toString(), QString("profile set to PMJ Black 1"));
    QCOMPARE(iom->inputPatch(0)->profileName(), QString("PMJ Black 1"));
    QCOMPARE(iom->feedbackPatch(0)->outputName(), QString("Launchkey MK4 49 MIDI Out"));
    ap->setEnabled(false);
    m_stub->m_hasFeedback = false;
}

namespace
{
const QString kDs4 = QStringLiteral(" DUALSHOCK 4 Wireless Controller (DE:4C:09:77:77:93)");

/* A universe's patch, as a person reads it in Connections: input (plugin,
   line, profile, its parameters) and feedback (plugin, line, parameters). */
QString patchState(InputOutputMap *iom, quint32 u)
{
    QStringList out;
    if (InputPatch *ip = iom->inputPatch(u))
    {
        out << QStringLiteral("in %1 %2 %3").arg(ip->pluginName()).arg(ip->input()).arg(ip->profileName());
        const QMap<QString, QVariant> pp = ip->getPluginParameters();
        for (auto it = pp.cbegin(); it != pp.cend(); ++it)
            out << QStringLiteral("in.%1=%2").arg(it.key(), it.value().toString());
    }
    if (OutputPatch *fb = iom->feedbackPatch(u))
    {
        out << QStringLiteral("fb %1 %2 %3").arg(fb->pluginName()).arg(fb->output()).arg(fb->outputName());
        const QMap<QString, QVariant> pp = fb->getPluginParameters();
        for (auto it = pp.cbegin(); it != pp.cend(); ++it)
            out << QStringLiteral("fb.%1=%2").arg(it.key(), it.value().toString());
    }
    return out.join(QStringLiteral("; "));
}
}

/* Branson, build 4910: the DualShock 4 had to be patched and its profile set
   by hand: "this needs to be automagic when it appears". A pad line that
   appears is patched as the pad by itself, on the first free universe, with
   no input profile (its roles come from its HID profile), and said. */
void SurfaceSim_Test::aPadIsSetUpWhenItAppears()
{
    loadAllProfiles(m_doc);
    SurfaceAutoPatch::setDeclined(kDs4, false);
    QSettings().remove(QStringLiteral("surfaces/lastUniverse/") + kDs4.trimmed());
    InputOutputMap *iom = m_doc->inputOutputMap();
    SurfaceAutoPatch *ap = m_doc->surfaceSetup()->autoPatch();
    ap->setDelay(0);
    ap->setEnabled(true, false);
    m_stub->m_linesAreHardware = true;
    m_stub->m_networkDmx = false;
    m_stub->m_lineNames = QStringList() << QStringLiteral("Launchkey MK4 49 MIDI Out");
    emit m_stub->configurationChanged();
    QSignalSpy pad(ap, &SurfaceAutoPatch::padPatched);
    m_stub->m_lineNames << kDs4;
    m_stub->m_lineDeviceKind[1] = QStringLiteral("dualshock4");
    emit m_stub->configurationChanged();
    QTRY_COMPARE(pad.count(), 1);
    const quint32 u = pad.first().at(0).toUInt();
    QCOMPARE(pad.first().at(1).toString(), QString("DUALSHOCK 4"));
    QCOMPARE(pad.first().at(2).toString(), kDs4);
    InputPatch *ip = iom->inputPatch(u);
    QVERIFY(ip != nullptr && ip->plugin() == m_stub);
    QCOMPARE(ip->input(), 1u);
    QVERIFY(ip->profile() == nullptr);
    QVERIFY(m_doc->programmer()->isPadUniverse(u));
    QVERIFY2(m_doc->surfaceSetup()->boardLog()->text(u).contains("as a game pad (dualshock4)"),
             qPrintable(m_doc->surfaceSetup()->boardLog()->text(u)));
    // The Launchkey (no board, no pad) is left.
    QVERIFY(iom->universesWithInputOn(m_stub->name(), 0).isEmpty());
    // Undo: let go, and not set up by itself again on this computer.
    QVERIFY(ap->undo(u));
    QVERIFY(iom->universesWithInputOn(m_stub->name(), 1).isEmpty());
    ap->scanNow();
    QCOMPARE(pad.count(), 1);
    SurfaceAutoPatch::setDeclined(kDs4, false);
    ap->setEnabled(false);
    m_stub->m_lineDeviceKind.clear();
}

/* His U04-Control: the DualShock 4 patched by hand on HID line 0 with no
   input profile -- right for a pad: nothing to repair, and it is a pad. A
   pad left with a board's profile and the board's feedback (a universe an
   APC had) is put right when it comes online. */
void SurfaceSim_Test::aHandPatchedPadIsRepaired()
{
    loadAllProfiles(m_doc);
    QSettings().remove(QStringLiteral("surfaces/keepPatch/") + kDs4.trimmed());
    InputOutputMap *iom = m_doc->inputOutputMap();
    SurfaceAutoPatch *ap = m_doc->surfaceSetup()->autoPatch();
    ap->setDelay(0);
    m_stub->m_linesAreHardware = true;
    m_stub->m_networkDmx = false;
    m_stub->m_hasFeedback = true;
    m_stub->m_lineNames = QStringList() << kDs4 << QStringLiteral("Network RTP Session 1");
    m_stub->m_lineDeviceKind[0] = QStringLiteral("dualshock4");
    emit m_stub->configurationChanged();
    while (iom->universesCount() < 4)
        QVERIFY(iom->addUniverse());
    QVERIFY(iom->setInputPatch(3, m_stub->name(), QString(), 0));
    QSignalSpy repaired(ap, &SurfaceAutoPatch::repaired);
    QSignalSpy pad(ap, &SurfaceAutoPatch::padPatched);
    ap->setEnabled(true, true);
    QCOMPARE(repaired.count(), 0);
    QCOMPARE(pad.count(), 0);                  // the show has it: not patched again
    QVERIFY(m_doc->programmer()->isPadUniverse(3));

    // A board's profile and a session's feedback on it: repaired.
    QVERIFY(iom->setInputProfile(3, QStringLiteral("Akai APC40 mkII")));
    QVERIFY(iom->setOutputPatch(3, m_stub->name(), QString(), 1, true));
    ap->scanNow();
    QCOMPARE(repaired.count(), 1);
    QCOMPARE(repaired.first().at(1).toString(), QString("DUALSHOCK 4"));
    QCOMPARE(repaired.first().at(2).toString(),
             QString("feedback back on its own line, board profile Akai APC40 mkII taken off"));
    QVERIFY(iom->inputPatch(3)->profile() == nullptr);
    QVERIFY(iom->feedbackPatch(3) == nullptr || !iom->feedbackPatch(3)->isPatched());
    QCOMPARE(iom->inputPatch(3)->input(), 0u);
    ap->setEnabled(false);
    m_stub->m_hasFeedback = false;
    m_stub->m_lineDeviceKind.clear();
}

/* The Linux gate, 2026-11-28 (padbenchshow under load): a pad set up by
   itself while a show was being built stayed patched as that pad when the
   show was then given an "any game controller" slot on another universe,
   and the show file named the pad. The stand-in is let go, the slot takes
   the pad, the file keeps the slot alone, and no pad is set up beside it. */
void SurfaceSim_Test::aPadSetUpByItselfYieldsToTheShowsSlot()
{
    loadAllProfiles(m_doc);
    SurfaceAutoPatch::setDeclined(kDs4, false);
    QSettings().remove(QStringLiteral("surfaces/lastUniverse/") + kDs4.trimmed());
    InputOutputMap *iom = m_doc->inputOutputMap();
    SurfaceAutoPatch *ap = m_doc->surfaceSetup()->autoPatch();
    ap->setDelay(0);
    ap->setEnabled(true, false);
    m_stub->m_linesAreHardware = true;
    m_stub->m_networkDmx = false;
    m_stub->m_lineNames = QStringList() << kDs4;
    m_stub->m_lineDeviceKind[0] = QStringLiteral("dualshock4");
    QSignalSpy pad(ap, &SurfaceAutoPatch::padPatched);
    emit m_stub->configurationChanged();
    QTRY_COMPARE(pad.count(), 1);
    const quint32 standIn = pad.first().at(0).toUInt();
    QVERIFY(iom->inputPatch(standIn) != nullptr && iom->inputPatch(standIn)->plugin() == m_stub);

    // The show's pad: any game controller, on another universe.
    const quint32 slotUni = standIn + 1;
    while (iom->universesCount() <= slotUni)
        QVERIFY(iom->addUniverse());
    Universe::InputSlotDevice any;
    any.plugin = m_stub->name();
    any.anyGamepad = true;
    iom->setInputSlot(slotUni, { any }, InputOutputMap::standardChannelRoles());

    QVERIFY2(iom->inputPatch(standIn) == nullptr || iom->inputPatch(standIn)->plugin() == nullptr,
             "the pad set up by itself is still patched beside the show's slot");
    QCOMPARE(iom->inputSlotLive(slotUni), 0);
    QVERIFY(iom->inputPatch(slotUni) != nullptr);
    QCOMPARE(iom->inputPatch(slotUni)->inputName(), kDs4);
    QVERIFY(!SurfaceAutoPatch::isDeclined(kDs4));          // let go, not declined

    QBuffer buf;
    QVERIFY(buf.open(QIODevice::WriteOnly));
    QXmlStreamWriter xml(&buf);
    QVERIFY(iom->saveXML(&xml));
    const QString saved = QString::fromUtf8(buf.data());
    QVERIFY2(!saved.contains(QStringLiteral("DUALSHOCK 4")), qPrintable(saved));
    QVERIFY2(saved.contains(QStringLiteral("Any=\"True\"")), qPrintable(saved));

    // The slot is the show's pad: a second pad is not set up beside it.
    m_stub->m_lineNames << QStringLiteral(" DualSense Wireless Controller (DE:4C:09:77:77:94)");
    m_stub->m_lineDeviceKind[1] = QStringLiteral("dualsense");
    emit m_stub->configurationChanged();
    QTest::qWait(50);
    QCOMPARE(pad.count(), 1);

    iom->setInputSlot(slotUni, QList<Universe::InputSlotDevice>());
    ap->setEnabled(false);
    m_stub->m_lineDeviceKind.clear();
}

/* 5.1: Branson fixed his U01 by patching the PMJ again by hand. The repair
   on appear leaves exactly that patch: the same input line, profile and
   parameters (his MIDI channel), the same feedback line and parameters. */
void SurfaceSim_Test::theRepairIsTheSameAsAHandRepatch()
{
    loadAllProfiles(m_doc);
    QSettings().remove(QStringLiteral("surfaces/keepPatch/") + kPmjPort);
    InputOutputMap *iom = m_doc->inputOutputMap();
    SurfaceAutoPatch *ap = m_doc->surfaceSetup()->autoPatch();
    ap->setDelay(0);
    m_stub->m_linesAreHardware = true;
    m_stub->m_networkDmx = false;
    m_stub->m_hasFeedback = true;
    m_stub->m_lineNames = kDinkPorts;
    emit m_stub->configurationChanged();
    auto hisPatch = [&]() {
        QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 3, QStringLiteral("PMJ Black 1")));
        iom->inputPatch(0)->setPluginParameter(QStringLiteral("midichannel"), 16);    // his: Omni
        QVERIFY(iom->setOutputPatch(0, m_stub->name(), QString(), 2, true));
    };
    // By hand: the PMJ's port picked for its feedback, as in Connections.
    hisPatch();
    QVERIFY(iom->setOutputPatch(0, m_stub->name(), QString(), 3, true));
    const QString byHand = patchState(iom, 0);
    QVERIFY2(byHand.contains("fb " + m_stub->name() + " 3 " + kPmjPort), qPrintable(byHand));
    // The same stale patch, repaired when the board comes online.
    hisPatch();
    QVERIFY(patchState(iom, 0) != byHand);
    ap->setEnabled(true, true);
    QCOMPARE(patchState(iom, 0), byHand);
    ap->setEnabled(false);
    m_stub->m_hasFeedback = false;
}

void SurfaceSim_Test::theBoardLogSaysWhatTheBoardSentAndWasSent()
{
    const QString fp = QStringLiteral("PreSonus FaderPort 2");
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("PreSonus-FaderPort-2.qxi")) != nullptr);
    SimMidiDevice board(m_doc, m_stub, QStringLiteral("PreSonus FP2"), 0, fp);
    QVERIFY2(board.plug(), qPrintable(board.lastError()));
    SurfaceBoardLog *log = m_doc->surfaceSetup()->boardLog();
    log->clear(0);
    // The fader moved: three steps, each one (not one per frame).
    QVERIFY(board.raw(QByteArray::fromHex("e0005a")));
    QVERIFY(board.raw(QByteArray::fromHex("e0005b")));
    QVERIFY(board.raw(QByteArray::fromHex("e0405c")));
    // Shift held, then Stop.
    QVERIFY(board.raw(QByteArray::fromHex("90467f")));
    QVERIFY(board.raw(QByteArray::fromHex("905d7f")));
    settle();
    int bends = 0;
    for (const SurfaceBoardLog::Entry &e : log->entries(0))
        if (e.dir == SurfaceBoardLog::In && e.text.contains("Pitch bend") && e.text.contains("(Fader)"))
            ++bends;
    QCOMPARE(bends, 3);
    const QString text = log->text(0);
    QVERIFY2(text.contains("Input: PreSonus FP2"), qPrintable(text));
    QVERIFY2(text.contains("Profile: PreSonus FaderPort 2"), qPrintable(text));
    QVERIFY2(text.contains(QRegularExpression("IN +90 46 7F +Note 70 on, ch 1, velocity 127 \\(Shift\\)")), qPrintable(text));
    QVERIFY2(text.contains("Note 93 on, ch 1, velocity 127 (Stop)"), qPrintable(text));
    // A lamp out: its bytes as the plugin sends them.
    QVERIFY(m_doc->inputOutputMap()->sendFeedBack(0, 222, 255, QVariant()));
    QVERIFY2(log->text(0).contains(QRegularExpression("OUT +90 5E 7F +Note 94 on, ch 1, velocity 127 \\(Play\\)")),
             qPrintable(log->text(0)));
    // The motor fader sent to the Grand Master: an out line, and the why.
    QVERIFY(m_doc->inputOutputMap()->sendFeedBack(0, 513, 128, QVariant()));
    QVERIFY2(log->text(0).contains("Pitch bend, ch 1 = 8192 of 16383 (Fader)"), qPrintable(log->text(0)));
    // Timestamped "HH:mm:ss.zzz".
    QVERIFY(QRegularExpression("\\n\\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d  IN ").match(text).hasMatch());
    // The last 500 only.
    for (int i = 0; i < 700; ++i)
        log->note(0, QString::number(i));
    QCOMPARE(log->entries(0).size(), SurfaceBoardLog::kSize);
    QCOMPARE(log->entries(0).last().text, QString("699"));
}

void SurfaceSim_Test::theInitGoesAgainOnceThePortHasSettled()
{
    const QString apc = QStringLiteral("Akai APC40 mkII");
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("Akai-APC40-mkII.qxi")) != nullptr);
    m_doc->surfaceSetup()->setInitAgainDelay(10);
    m_stub->m_acceptShowControl = true;
    m_stub->m_showControlSent.clear();
    const QString ableton = QStringLiteral("f0 47 7f 29 60 00 04 41 09 00 05 f7");
    auto sends = [this, &ableton]() {
        int n = 0;
        for (const auto &m : std::as_const(m_stub->m_showControlSent))
            n += hex(m.second) == ableton ? 1 : 0;
        return n;
    };
    SimMidiDevice board(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, apc);
    QVERIFY2(board.plug(), qPrintable(board.lastError()));
    const int first = sends();
    QVERIFY(first >= 1);
    QTRY_VERIFY(sends() > first);
    // Each went to its own port, and the log says so.
    for (const auto &m : std::as_const(m_stub->m_showControlSent))
        if (hex(m.second) == ableton)
            QCOMPARE(int(m.first), board.line());
    const QString log = m_doc->surfaceSetup()->boardLog()->text(0);
    QVERIFY2(log.contains("Init (Ableton mode), on patch") && log.contains("again, after its port opened"),
             qPrintable(log));
    m_doc->surfaceSetup()->setInitAgainDelay(600);
    m_stub->m_acceptShowControl = false;
}

void SurfaceSim_Test::theLampTestLightsEveryLampThenGivesThemBack()
{
    const QString maize = QStringLiteral("MaizeDMX Playback Controller");
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("MaizeDMX-Playback-Controller.qxi")) != nullptr);
    SimMidiDevice board(m_doc, m_stub, QStringLiteral("USB2.0-MIDI Port 1"), 0, maize);
    QVERIFY2(board.plug(), qPrintable(board.lastError()));
    SurfaceSetup *ss = m_doc->surfaceSetup();
    const QList<quint32> lamps = ss->lampChannels(0);
    QVERIFY(lamps.contains(128 + 13));      // GO's lamp
    QVERIFY(lamps.contains(173));           // LED 1
    ss->setLampTestStepMs(1);
    m_stub->m_feedback.clear();
    QSignalSpy released(m_doc->inputOutputMap(), &InputOutputMap::boardLampsReleased);
    QString why;
    QVERIFY2(ss->startLampTest(0, &why), qPrintable(why));
    QTRY_VERIFY(!ss->lampTestRunning());
    // Each lamp lit (the profile's 127, sent as 255), and all let go after.
    QSet<quint32> lit;
    for (const auto &f : std::as_const(m_stub->m_feedback))
        if (f.second == 255)
            lit.insert(f.first);
    for (quint32 ch : lamps)
        QVERIFY2(lit.contains(ch), qPrintable(QString::number(ch)));
    QVERIFY(released.count() >= 1);
    QVERIFY(!m_doc->inputOutputMap()->isSetupHeld(0));
    // No feedback line: said, nothing sent.
    QVERIFY(m_doc->inputOutputMap()->setOutputPatch(0, KOutputNone, QString(), 0, true));
    QVERIFY(!ss->startLampTest(0, &why));
    QVERIFY2(why.contains("no feedback line"), qPrintable(why));
    ss->setLampTestStepMs(250);
}

void SurfaceSim_Test::aBoardWithParenthesesKeepsItsRoles()
{
    QSettings().remove(DeskRecord::settingsGroup());
    // As the MIDI plugin: its identity is "midi:<port>".
    m_stub->m_nameOverride = QStringLiteral("MIDI");
    const QString name = QStringLiteral("APC64 DAW (APC64)");
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("Akai-APC64.qxi")) != nullptr);
    {
        SimMidiDevice board(m_doc, m_stub, name, 0, QStringLiteral("Akai APC64"));
        QVERIFY2(board.plug(), qPrintable(board.lastError()));
        DeskRecord *desk = m_doc->deskRecord();
        QTRY_VERIFY(desk->isSet(DeskRecord::Stop));
        QCOMPARE(desk->binding(DeskRecord::Stop).identity, QString("midi:APC64 DAW (APC64)"));
        QVERIFY(desk->isLive(DeskRecord::Stop));
        QVERIFY(desk->seededDevices().contains("midi:APC64 DAW (APC64)"));
        // Kept on disk percent-encoded (a settings key holds no '/'), as on dink.
        QSettings s;
        const QString stored = s.value(DeskRecord::settingsGroup() + "/stop/identity").toString();
        QVERIFY2(stored.isEmpty() || stored == "midi:APC64 DAW %28APC64%29", qPrintable(stored));
    }
    // A restart: read back raw, and the board's roles are live on it again.
    delete m_doc;
    m_doc = new Doc(this);
    QDir dir(QStringLiteral("../iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    m_stub->m_nameOverride = QStringLiteral("MIDI");
    // Another device first: the board on line 1.
    m_stub->m_lineNames = QStringList() << QStringLiteral("Other Box");
    QVERIFY(mapProfile(m_doc, profileDir() + QStringLiteral("Akai-APC64.qxi")) != nullptr);
    SimMidiDevice again(m_doc, m_stub, name, 0, QStringLiteral("Akai APC64"));
    QVERIFY2(again.plug(), qPrintable(again.lastError()));
    DeskRecord *desk = m_doc->deskRecord();
    QCOMPARE(desk->binding(DeskRecord::Stop).identity, QString("midi:APC64 DAW (APC64)"));
    QCOMPARE(desk->identityOfUniverse(0), QString("midi:APC64 DAW (APC64)"));
    QVERIFY(desk->isLive(DeskRecord::Stop));

    QCOMPARE(again.line(), 1);
    // Its line gone from under a patch (the list shrank): no identity, never "midi:None".
    m_stub->m_lineNames = QStringList() << QStringLiteral("Other Box");
    QCOMPARE(m_doc->inputOutputMap()->inputPatch(0)->inputName(), KInputNone);
    QCOMPARE(desk->identityOfUniverse(0), QString());
    desk->seedDefaults();
    QVERIFY(!desk->seededDevices().contains("midi:None"));
    QSettings().remove(DeskRecord::settingsGroup());
    m_stub->m_nameOverride.clear();
}

QTEST_MAIN(SurfaceSim_Test)
