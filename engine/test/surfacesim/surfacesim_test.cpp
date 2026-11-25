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
#include <QDir>

#include "surfacesim_test.h"
#include "simmididevice.h"
#include "surfacescenarios.h"
#include "surfacescript.h"
#include "gamepadlayout.h"
#include "hidprofile.h"
#include "controlmap.h"
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

void SurfaceSim_Test::init()
{
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
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
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
            iom->flushInputs();
            m_stub->emitValueChanged(UINT_MAX, 0, it.key(), 0);
            iom->flushInputs();
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
    QEXPECT_FAIL("", "Phase R's state-dump guard (feat/053-r-recovery, ControlMap::isStateDump()) is not on main yet: "
                     "the dump's note on is taken as a press", Continue);
    QCOMPARE(stops(), 1);
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
    /* Each universe on its own board's line. Found by this test
       (2026-11-25): setInputPatch() looks the line up by its name and takes
       the first of that name, so the second board's universe opens the
       first board's line and the second board is heard nowhere. */
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->inputPatch(1) != nullptr);
    QEXPECT_FAIL("", "a bug this test found, no fix branch yet: InputOutputMap::setInputPatch() resolves an "
                     "input by name to the first line of that name (two identical boards)", Abort);
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

QTEST_MAIN(SurfaceSim_Test)
