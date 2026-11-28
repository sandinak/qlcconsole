/*
  Q Light Controller Plus
  surfacesetup.cpp

  Copyright (C) Branson Matheson

  Ported in part from qlcplus-midi-profiler (Apache-2.0): see surfacesetup.h.

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

#include <QStandardPaths>
#include <QDateTime>
#include <QSettings>
#include <QTimer>
#include <QFile>
#include <QSet>
#include <climits>

#include "surfacesetup.h"
#include "surfaceboardlog.h"
#include "surfaceautopatch.h"
#include "surfacelayout.h"
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"
#include "controllerkeepalive.h"
#include "programmercontroller.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "qlcioplugin.h"
#include "doc.h"

namespace
{
QString s_backupDir;

QByteArray hex(const char *text)
{
    return QByteArray::fromHex(QByteArray(text).replace(' ', ""));
}

/* The PMJ Black 1's lamps and the buttons over them (LED index, note; every
   button sends on MIDI channel 9), as qlcplus-midi-profiler's `opendeck
   identify` observed them on the board (maps/pmj-black-1.json, "button").
   Its factory config has LED i on note i, channel 1: no lamp under its key. */
const int kPmjPairs[][2] = {
    {0, 23}, {2, 2}, {3, 4}, {4, 6}, {5, 8}, {6, 10}, {8, 36}, {10, 13}, {11, 15},
    {12, 17}, {13, 19}, {14, 21}, {16, 49}, {17, 0}, {18, 26}, {19, 28}, {20, 30},
    {21, 32}, {22, 34}, {24, 52}, {25, 11}, {26, 39}, {27, 41}, {28, 43}, {29, 45},
    {30, 47}, {32, 22}, {33, 24}, {34, 1}, {35, 3}, {36, 5}, {37, 7}, {38, 9},
    {40, 35}, {41, 37}, {42, 12}, {43, 14}, {44, 16}, {45, 18}, {46, 20}, {48, 48},
    {49, 50}, {50, 25}, {51, 27}, {52, 29}, {53, 31}, {54, 33}, {56, 51}, {57, 53},
    {58, 38}, {59, 40}, {60, 42}, {61, 44}, {62, 46},
};

QList<SurfaceSetup::Spec> buildSpecs()
{
    QList<SurfaceSetup::Spec> list;
    {
        SurfaceSetup::Spec s;
        s.profile = QStringLiteral("PMJ Black 1");
        s.openDeck = true;
        for (const auto &p : kPmjPairs)
        {
            SurfaceSetup::LampPair lp;
            lp.led = p[0];
            lp.channel = 9;
            lp.note = p[1];
            s.pairing << lp;
        }
        list << s;
    }
    {
        // resources/miditemplates/APC40MK2.qxm: its pads light only from the app in this mode.
        SurfaceSetup::Spec s;
        s.profile = QStringLiteral("Akai APC40 mkII");
        s.init << hex("F0 47 7F 29 60 00 04 41 09 00 05 F7");
        s.initText = QObject::tr("Ableton mode");
        list << s;
    }
    {
        /* The APC64 (identity model 0x53) on its "APC64 DAW" port: its
           display taken and named, given back on exit (bench-confirmed on
           his APC64, 2026-11-25). Session mode has no known message (Live's
           mode id 0x19, values unknown): a pad press on channel 7 shows it. */
        SurfaceSetup::Spec s;
        s.profile = QStringLiteral("Akai APC64");
        s.init << hex("F0 47 00 53 1C 00 01 01 F7")
               << SurfaceSetup::apc64Text(0, QStringLiteral("qlc"))
               << SurfaceSetup::apc64Text(1, QStringLiteral("console"))
               << SurfaceSetup::apc64Text(2, QString());
        s.release << hex("F0 47 00 53 1C 00 01 00 F7");
        s.initText = QObject::tr("Its display");
        s.modeChannel = 7;
        s.modeName = QObject::tr("Session");
        s.modeHint = QObject::tr("Press SESSION on the APC64");
        list << s;
    }
    {
        // Novation-LaunchPadMiniMK3.qxm's first message: the pads as notes 11-88.
        SurfaceSetup::Spec s;
        s.profile = QStringLiteral("Novation Launchpad Mini MK3");
        s.init << hex("F0 00 20 29 02 0D 0E 01 F7");
        s.initText = QObject::tr("programmer mode");
        list << s;
    }
    return list;
}

QString pairsToText(const QList<SurfaceSetup::LampPair> &pairs)
{
    QStringList out;
    for (const SurfaceSetup::LampPair &p : pairs)
        out << QStringLiteral("%1:%2:%3").arg(p.led).arg(p.channel).arg(p.note);
    return out.join(QLatin1Char(' '));
}

QList<SurfaceSetup::LampPair> pairsFromText(const QString &text)
{
    QList<SurfaceSetup::LampPair> out;
    for (const QString &item : text.split(QLatin1Char(' '), Qt::SkipEmptyParts))
    {
        const QStringList f = item.split(QLatin1Char(':'));
        if (f.size() != 3)
            continue;
        SurfaceSetup::LampPair p;
        p.led = f.at(0).toInt();
        p.channel = f.at(1).toInt();
        p.note = f.at(2).toInt();
        out << p;
    }
    return out;
}
}

SurfaceSetup::SurfaceSetup(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    m_boardLog = new SurfaceBoardLog(m_doc, this);
    m_autoPatch = new SurfaceAutoPatch(m_doc, this);
    connect(iom, &InputOutputMap::inputSysExReceived, this, &SurfaceSetup::slotSysEx);
    // Patched, its profile set, or found again after a replug: its init.
    connect(iom, &InputOutputMap::inputPatchChanged, this, &SurfaceSetup::slotPatchChanged);
    connect(iom, &InputOutputMap::profileChanged, this,
            [this](quint32 universe, const QString &) { slotPatchChanged(universe); });
    connect(iom, &InputOutputMap::inputDeviceConnected, this,
            [this](quint32 universe, const QString &, const QString &) { slotPatchChanged(universe); });
    connect(iom, &InputOutputMap::universeRemoved, this, [this](quint32 universe) {
        m_checks.remove(universe);
    });
    connect(iom, &InputOutputMap::inputValueChanged, this,
            [this](quint32 u, quint32 ch, uchar v, const QString &) { slotInput(u, ch, v); });
    if (ProgrammerController *pc = m_doc->programmer())
        if (ControllerKeepAlive *ka = pc->controllerKeepAlive())
            connect(ka, &ControllerKeepAlive::woke, this, &SurfaceSetup::slotWoke);
}

SurfaceSetup::~SurfaceSetup()
{
    // The app lets go: a board whose display it took gets it back.
    for (quint32 u : universes())
        if (const Spec *sp = specOf(u))
            for (const QByteArray &m : sp->release)
                sendTo(u, m);
    // Let go without asking the lamp owners to repaint: the show is
    // going too (Doc's destructor deletes this first).
    for (quint32 u : std::as_const(m_holds))
        if (m_doc->inputOutputMap()->isSetupHeld(u))
            m_doc->inputOutputMap()->setSetupListening(UINT_MAX, false);
    qDeleteAll(m_links);
}

QByteArray SurfaceSetup::apc64Text(int line, const QString &text)
{
    // F0 47 00 53 10 <len hi> <len lo> <line> <ASCII, up to 8> 00 F7; len = 1 + text + 1.
    const QByteArray ascii = text.toLatin1().left(8);
    const int len = 1 + int(ascii.size()) + 1;
    QByteArray m = hex("F0 47 00 53 10");
    m.append(char((len >> 7) & 0x7F));
    m.append(char(len & 0x7F));
    m.append(char(line & 0x7F));
    for (char c : ascii)
        m.append(char(c & 0x7F));
    m.append(char(0x00));
    m.append(char(0xF7));
    return m;
}

void SurfaceSetup::holdPresses(quint32 universe, bool on)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    if (on)
    {
        m_holds.insert(universe);
        iom->setSetupListening(universe);
    }
    else
    {
        m_holds.remove(universe);
        if (iom->isSetupHeld(universe))
            iom->setSetupListening(UINT_MAX);
    }
}

bool SurfaceSetup::beginModeCheck(quint32 universe)
{
    const Spec *s = specOf(universe);
    if (s == nullptr || s->modeChannel == 0)
        return false;
    m_modeWait.insert(universe);
    holdPresses(universe, true);
    Check c;
    c.state = Check::WaitingForPress;
    c.text = tr("%1, then press any pad.").arg(s->modeHint);
    m_checks.insert(universe, c);
    emit changed(universe);
    return true;
}

void SurfaceSetup::cancelModeCheck(quint32 universe)
{
    if (!m_modeWait.remove(universe))
        return;
    holdPresses(universe, false);
    m_checks.remove(universe);
    emit changed(universe);
}

void SurfaceSetup::slotInput(quint32 universe, quint32 channel, uchar value)
{
    if (value == 0 || !m_modeWait.contains(universe))
        return;
    const Spec *s = specOf(universe);
    const quint32 c = channel & 0x0FFF;
    // A pad is a note; a fader or knob moved meanwhile says nothing of the mode.
    if (s == nullptr || c < 128 || c > 255)
        return;
    m_modeWait.remove(universe);
    /* Let go once every listener has had this press (a trigger connected
       after this one must still find it held), unless another check began. */
    QTimer::singleShot(0, this, [this, universe]() {
        if (!m_modeWait.contains(universe))
            holdPresses(universe, false);
    });
    const int midiChannel = int(channel >> 12) + 1;
    Check r;
    if (midiChannel == s->modeChannel)
    {
        r.state = Check::InMode;
        r.text = tr("It is in %1 mode: its pads are what its profile expects.").arg(s->modeName);
    }
    else
    {
        r.state = Check::WrongMode;
        r.text = tr("That pad came on MIDI channel %1, so it is not in %2 mode: its pads won't "
                    "match. %3, then check again.").arg(midiChannel).arg(s->modeName, s->modeHint);
    }
    m_checks.insert(universe, r);
    emit changed(universe);
}

const QList<SurfaceSetup::Spec> &SurfaceSetup::specs()
{
    static const QList<Spec> list = buildSpecs();
    return list;
}

const SurfaceSetup::Spec *SurfaceSetup::spec(const QString &profileName)
{
    for (const Spec &s : specs())
        if (s.profile.compare(profileName, Qt::CaseInsensitive) == 0)
            return &s;
    return nullptr;
}

const SurfaceSetup::Spec *SurfaceSetup::specOf(quint32 universe) const
{
    InputPatch *ip = m_doc->inputOutputMap()->inputPatch(universe);
    if (ip == nullptr || ip->plugin() == nullptr)
        return nullptr;
    return spec(ip->profileName());
}

QList<quint32> SurfaceSetup::universes() const
{
    QList<quint32> out;
    for (quint32 u = 0; u < m_doc->inputOutputMap()->universesCount(); ++u)
        if (specOf(u) != nullptr)
            out << u;
    return out;
}

QString SurfaceSetup::title(quint32 universe) const
{
    const Spec *s = specOf(universe);
    return s == nullptr ? QString()
                        : tr("%1 (universe %2)").arg(s->profile).arg(universe + 1);
}

QString SurfaceSetup::deviceKey(quint32 universe) const
{
    InputPatch *ip = m_doc->inputOutputMap()->inputPatch(universe);
    QString name = ip != nullptr ? ip->inputName() : QString();
    name.replace(QLatin1Char('/'), QLatin1Char('_'));
    const Spec *s = specOf(universe);
    return (s != nullptr ? s->profile : QString()) + QStringLiteral(" - ") + name;
}

/****************************************************************************
 * Out
 ****************************************************************************/

bool SurfaceSetup::sendTo(quint32 universe, const QByteArray &msg, const QString &why)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    // Its feedback line first: the port the app already writes its lamps to.
    OutputPatch *fb = iom->feedbackPatch(universe);
    if (fb != nullptr && fb->plugin() != nullptr && fb->output() != QLCIOPlugin::invalidLine())
    {
        const bool ok = fb->plugin()->sendShowControl(fb->output(), msg, QString(), 0);
        m_boardLog->logSysExOut(universe, msg, ok,
                                ok ? tr("%1, to %2").arg(why, fb->outputName())
                                   : tr("%1: %2 refused it").arg(why, fb->outputName()));
        return ok;
    }
    // Else the output of the same name as its input (a MIDI box's two halves).
    InputPatch *ip = iom->inputPatch(universe);
    if (ip == nullptr || ip->plugin() == nullptr)
    {
        m_boardLog->logSysExOut(universe, msg, false, tr("%1: no input patched").arg(why));
        return false;
    }
    const int line = int(ip->plugin()->outputs().indexOf(ip->inputName()));
    if (line < 0)
    {
        m_boardLog->logSysExOut(universe, msg, false,
                                tr("%1: no feedback line, and no output named %2").arg(why, ip->inputName()));
        return false;
    }
    const bool ok = ip->plugin()->sendShowControl(quint32(line), msg, QString(), 0);
    m_boardLog->logSysExOut(universe, msg, ok,
                            ok ? tr("%1, to %2 (no feedback line: its output of that name)").arg(why, ip->inputName())
                               : tr("%1: %2 refused it").arg(why, ip->inputName()));
    return ok;
}

bool SurfaceSetup::sendInit(quint32 universe, const QString &why)
{
    const Spec *s = specOf(universe);
    if (s == nullptr || s->init.isEmpty())
        return false;
    const QString what = tr("Init (%1), %2").arg(s->initText, why.isEmpty() ? tr("asked") : why);
    bool ok = true;
    for (const QByteArray &m : s->init)
        ok = sendTo(universe, m, what) && ok;
    if (ok)
        ++m_initsSent;
    return ok;
}

void SurfaceSetup::slotPatchChanged(quint32 universe)
{
    // Another device or profile there: what was read of the last one says nothing.
    if (m_modeWait.remove(universe))
        holdPresses(universe, false);
    m_checks.remove(universe);
    /* One patch says both "patched" and "its profile" (setInputPatch()):
       the init that went for the first goes again when the port has
       settled, never twice back to back (the virtual bench, 2026-11-27:
       the APC40 mkII was sent Ableton mode four times on a plug). */
    if (m_initAgain.contains(universe))
    {
        emit changed(universe);
        return;
    }
    if (sendInit(universe, tr("on patch")))
    {
        /* Once more when the port has settled: a board just plugged in may
           not take a message the moment its port appears (the APC40 mkII
           stayed in its generic mode on the bench, 2026-11-27). */
        m_initAgain.insert(universe);
        QTimer::singleShot(m_initAgainMs, this, [this, universe]() {
            if (m_initAgain.remove(universe))
                sendInit(universe, tr("again, after its port opened"));
        });
    }
    emit changed(universe);
}

/****************************************************************************
 * Lamp test
 ****************************************************************************/

QList<quint32> SurfaceSetup::lampChannels(quint32 universe) const
{
    QList<quint32> out;
    InputPatch *ip = m_doc->inputOutputMap()->inputPatch(universe);
    if (ip == nullptr || ip->profile() == nullptr)
        return out;
    const SurfaceLayout layout = SurfaceLayout::forProfile(ip->profileName());
    if (layout.isValid())
    {
        for (const SurfaceLayout::Control &c : layout.controls())
            if (c.lamp != SurfaceLayout::NoLamp)
                out << c.channel;
        if (!out.isEmpty())
            return out;
    }
    const QMap<quint32, QLCInputChannel *> chans = ip->profile()->channels();
    for (auto it = chans.constBegin(); it != chans.constEnd(); ++it)
        if (it.value()->type() == QLCInputChannel::Button)
            out << it.key();
    return out;
}

bool SurfaceSetup::startLampTest(quint32 universe, QString *why)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    OutputPatch *fb = iom->feedbackPatch(universe);
    if (fb == nullptr || !fb->isPatched())
    {
        const QString w = tr("It has no feedback line: its lamps can't be lit. Choose Feedback ▸ Pair first.");
        m_boardLog->note(universe, tr("Lamp test refused: no feedback line"));
        if (why != nullptr)
            *why = w;
        return false;
    }
    const QList<quint32> chans = lampChannels(universe);
    if (chans.isEmpty())
    {
        if (why != nullptr)
            *why = tr("Its profile has no lamp.");
        return false;
    }
    stopLampTest();
    m_lampTestUniverse = universe;
    m_lampTestChannels = chans;
    m_lampTestIndex = 0;
    // The test is the one writer of its lamps until it ends (as Identify).
    iom->setSetupListening(universe);
    m_boardLog->note(universe, tr("Lamp test: %n lamp(s), out of %1", "", int(chans.size())).arg(fb->outputName()));
    if (m_lampTimer == nullptr)
    {
        m_lampTimer = new QTimer(this);
        connect(m_lampTimer, &QTimer::timeout, this, &SurfaceSetup::lampTestStep);
    }
    m_lampTimer->start(m_lampStepMs);
    lampTestStep();
    return true;
}

void SurfaceSetup::lampTestStep()
{
    const quint32 u = m_lampTestUniverse;
    if (u == UINT_MAX)
        return;
    InputOutputMap *iom = m_doc->inputOutputMap();
    OutputPatch *fb = iom->feedbackPatch(u);
    InputPatch *ip = iom->inputPatch(u);
    if (fb == nullptr || !fb->isPatched() || ip == nullptr || ip->profile() == nullptr)
    {
        stopLampTest();
        return;
    }
    auto send = [this, u, fb, ip](quint32 ch, bool on) {
        QLCInputChannel *c = ip->profile()->channel(ch);
        uchar v = on ? (c != nullptr ? c->upperValue() : 255) : (c != nullptr ? c->lowerValue() : 0);
        // A MIDI profile's levels are velocities: the plugin halves what it is sent.
        if (c != nullptr && ip->profile()->type() == QLCInputProfile::MIDI)
            v = v >= 127 ? UCHAR_MAX : uchar(v * 2);
        const int midiCh = c != nullptr ? c->lowerChannel() : -1;
        const QVariant params = midiCh >= 0 ? QVariant(midiCh) : QVariant();
        fb->plugin()->sendFeedBack(u, fb->output(), ch, v, params);
        const QByteArray b = SurfaceBoardLog::midiBytes(ch, v, midiCh);
        m_boardLog->logSysExOut(u, b, true, tr("lamp test%1").arg(c != nullptr ? QStringLiteral(": ") + c->name() : QString()));
    };
    const int n = int(m_lampTestChannels.size());
    QString step;
    if (m_lampTestIndex < n)
    {
        if (m_lampTestIndex > 0)
            send(m_lampTestChannels.at(m_lampTestIndex - 1), false);
        const quint32 ch = m_lampTestChannels.at(m_lampTestIndex);
        send(ch, true);
        QLCInputChannel *c = ip->profile()->channel(ch);
        step = tr("Lamp %1 of %2: %3").arg(m_lampTestIndex + 1).arg(n).arg(c != nullptr ? c->name() : QString::number(ch));
    }
    else if (m_lampTestIndex == n)
    {
        send(m_lampTestChannels.last(), false);
        for (quint32 ch : std::as_const(m_lampTestChannels))
            send(ch, true);
        step = tr("All %n lamp(s) on", "", n);
    }
    else
    {
        for (quint32 ch : std::as_const(m_lampTestChannels))
            send(ch, false);
        stopLampTest();
        return;
    }
    ++m_lampTestIndex;
    emit lampTestStepped(u, step);
}

void SurfaceSetup::stopLampTest()
{
    if (m_lampTimer != nullptr)
        m_lampTimer->stop();
    const quint32 u = m_lampTestUniverse;
    if (u == UINT_MAX)
        return;
    m_lampTestUniverse = UINT_MAX;
    m_lampTestChannels.clear();
    // The show's lamps again (boardLampsReleased: each owner repaints).
    if (m_doc->inputOutputMap()->isSetupHeld(u))
        m_doc->inputOutputMap()->setSetupListening(UINT_MAX);
    m_boardLog->note(u, tr("Lamp test over: the show's lamps back"));
    emit lampTestStepped(u, QString());
}

void SurfaceSetup::slotWoke()
{
    // The devices stayed plugged in but may have lost their mode with the
    // power of the port: once they are found again, each gets its init.
    QTimer::singleShot(2000, this, [this]() {
        for (quint32 u : universes())
            sendInit(u, tr("after a wake"));
    });
}

/****************************************************************************
 * In
 ****************************************************************************/

void SurfaceSetup::slotSysEx(quint32 universe, const QByteArray &sysEx)
{
    if (OpenDeckLink *l = m_links.value(universe))
        l->deliver(sysEx);
}

OpenDeckLink *SurfaceSetup::link(quint32 universe)
{
    const Spec *s = specOf(universe);
    if (s == nullptr || !s->openDeck)
        return nullptr;
    OpenDeckLink *l = m_links.value(universe);
    if (l == nullptr)
    {
        l = new OpenDeckLink([this, universe](const QByteArray &m) { return sendTo(universe, m); },
                             m_timeoutMs);
        m_links.insert(universe, l);
    }
    return l;
}

/****************************************************************************
 * Check
 ****************************************************************************/

QList<SurfaceSetup::LampPair> SurfaceSetup::pairing(quint32 universe) const
{
    const Spec *s = specOf(universe);
    if (s == nullptr)
        return {};
    QSettings settings;
    const QString learned = settings.value(QStringLiteral("surfacesetup/pairing/") + s->profile).toString();
    if (!learned.isEmpty())
        return pairsFromText(learned);
    return s->pairing;
}

SurfaceSetup::Check SurfaceSetup::lastCheck(quint32 universe) const
{
    if (m_checks.contains(universe))
        return m_checks.value(universe);
    Check c;
    const Spec *s = specOf(universe);
    if (s != nullptr && !s->openDeck)
    {
        c.state = Check::InitOnly;
        c.text = tr("%1 is sent when it connects, after a replug and a wake.").arg(s->initText);
        if (s->modeChannel != 0)
            c.text += QLatin1Char(' ')
                    + tr("%1 mode: not checked yet (Check Mode asks for a pad press).").arg(s->modeName);
    }
    else
        c.text = tr("Not checked yet.");
    return c;
}

SurfaceSetup::Check SurfaceSetup::check(quint32 universe)
{
    Check c;
    const Spec *s = specOf(universe);
    if (s == nullptr)
    {
        c.state = Check::NoInit;
        c.text = tr("No controller this app knows is patched here.");
        return c;
    }
    if (!s->openDeck)
    {
        c = lastCheck(universe);
        m_checks.insert(universe, c);
        return c;
    }
    OpenDeckLink *l = link(universe);
    QList<OpenDeckLink::Led> leds;
    if (l->handshake())
        leds = l->readLeds();
    if (leds.isEmpty())
    {
        c.state = Check::NoAnswer;
        c.text = tr("The board did not answer (%1). Check that its output is patched as the "
                    "feedback line and that it is the board, not another port.").arg(l->lastError());
        m_checks.insert(universe, c);
        emit changed(universe);
        return c;
    }
    m_lampCount.insert(universe, int(leds.size()));
    const QList<LampPair> pairs = pairing(universe);
    if (pairs.isEmpty())
    {
        c.state = Check::NoPairing;
        c.text = tr("This board's %1 lamps are not paired with its buttons yet: Identify Lamps "
                    "flashes each one for you to press the button under it.").arg(leds.size());
        m_checks.insert(universe, c);
        emit changed(universe);
        return c;
    }
    for (const LampPair &p : pairs)
    {
        ++c.lamps;
        if (p.led >= leds.size())
        {
            ++c.misplaced;
            continue;
        }
        const OpenDeckLink::Led &led = leds.at(p.led);
        if (!led.hearsNotes() || led.note != p.note || led.channel != p.channel)
            ++c.misplaced;
    }
    if (c.misplaced == 0)
    {
        c.state = Check::Aligned;
        c.text = tr("All %1 lamps are under their buttons.").arg(c.lamps);
    }
    else
    {
        c.state = Check::Misplaced;
        c.text = tr("%1 of %2 lamps aren't under their buttons: a lamp the app lights is on "
                    "another key. Align Lamps sets each one to its own button.")
                     .arg(c.misplaced).arg(c.lamps);
    }
    m_checks.insert(universe, c);
    emit changed(universe);
    return c;
}

/****************************************************************************
 * Writes
 ****************************************************************************/

QString SurfaceSetup::writeRefusal(bool confirmed) const
{
    if (m_doc->mode() == Doc::Operate && !confirmed)
        return tr("The show is in Operate: the board's buttons and lamps stop for a few seconds "
                  "while its memory is written. Confirm to write anyway.");
    return QString();
}

QString SurfaceSetup::backupDir()
{
    if (!s_backupDir.isEmpty())
        return s_backupDir;
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/Controller backups");
}

void SurfaceSetup::setBackupDir(const QString &dir)
{
    s_backupDir = dir;
}

QString SurfaceSetup::backup(quint32 universe, const QString &why, QString *error)
{
    OpenDeckLink *l = link(universe);
    if (l == nullptr || !l->handshake())
    {
        if (error) *error = tr("The board did not answer, so nothing was written.");
        return QString();
    }
    const OpenDeckLink::Tables tables = l->dumpAll();
    if (tables.isEmpty())
    {
        if (error) *error = tr("The board's settings could not be read, so nothing was written.");
        return QString();
    }
    QString name = deviceKey(universe);
    for (QChar &ch : name)
        if (!ch.isLetterOrNumber())
            ch = QLatin1Char('-');
    const QString path = backupDir() + QLatin1Char('/') + name.toLower() + QLatin1Char('-') + why
            + QLatin1Char('-') + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))
            + QStringLiteral(".json");
    QString err;
    if (!OpenDeckLink::saveBackup(path, deviceKey(universe), tables, &err))
    {
        if (error) *error = tr("The backup could not be saved (%1), so nothing was written.").arg(err);
        return QString();
    }
    return path;
}

bool SurfaceSetup::align(quint32 universe, bool confirmed, QString *error)
{
    const QString refusal = writeRefusal(confirmed);
    if (!refusal.isEmpty())
    {
        if (error) *error = refusal;
        return false;
    }
    const QList<LampPair> pairs = pairing(universe);
    if (link(universe) == nullptr || pairs.isEmpty())
    {
        if (error) *error = tr("There is no lamp pairing for this board.");
        return false;
    }
    const QString path = backup(universe, QStringLiteral("pre-align"), error);
    if (path.isEmpty())
        return false;
    QSettings().setValue(QStringLiteral("surfacesetup/backup/") + deviceKey(universe), path);
    OpenDeckLink *l = link(universe);
    QStringList failed;
    for (const LampPair &p : pairs)
        if (!l->alignLed(p.led, p.note, p.channel))
            failed << tr("lamp %1: %2").arg(p.led).arg(l->lastError());
    check(universe);
    if (!failed.isEmpty())
    {
        if (error) *error = tr("Some lamps could not be set (%1). Undo Align puts the board back.")
                                .arg(failed.join(QStringLiteral(", ")));
        return false;
    }
    return true;
}

QString SurfaceSetup::lastBackup(quint32 universe) const
{
    const QString path = QSettings().value(QStringLiteral("surfacesetup/backup/") + deviceKey(universe)).toString();
    return QFile::exists(path) ? path : QString();
}

bool SurfaceSetup::undoAlign(quint32 universe, bool confirmed, QString *error)
{
    const QString path = lastBackup(universe);
    if (path.isEmpty())
    {
        if (error) *error = tr("There is no backup of an Align to put back.");
        return false;
    }
    if (!restore(universe, path, confirmed, error))
        return false;
    QSettings().remove(QStringLiteral("surfacesetup/backup/") + deviceKey(universe));
    return true;
}

bool SurfaceSetup::restore(quint32 universe, const QString &path, bool confirmed, QString *error)
{
    const QString refusal = writeRefusal(confirmed);
    if (!refusal.isEmpty())
    {
        if (error) *error = refusal;
        return false;
    }
    OpenDeckLink::Tables tables;
    QString err;
    if (!OpenDeckLink::loadBackup(path, &tables, &err))
    {
        if (error) *error = tr("The backup could not be read (%1).").arg(err);
        return false;
    }
    // Redo: what the board has now is kept too, before it is overwritten.
    if (backup(universe, QStringLiteral("pre-restore"), error).isEmpty())
        return false;
    const OpenDeckLink::RestoreResult res = link(universe)->restoreAll(tables);
    check(universe);
    if (!res.failed.isEmpty())
    {
        if (error) *error = tr("%1 values could not be put back (%2).")
                                .arg(res.failed.size()).arg(res.failed.first());
        return false;
    }
    return true;
}

/****************************************************************************
 * Identify
 ****************************************************************************/

bool SurfaceSetup::beginIdentify(quint32 universe, bool confirmed, QString *error)
{
    const QString refusal = writeRefusal(confirmed);
    if (!refusal.isEmpty())
    {
        if (error) *error = refusal;
        return false;
    }
    if (link(universe) == nullptr)
    {
        if (error) *error = tr("This board's lamps can't be identified from the app.");
        return false;
    }
    const QString path = backup(universe, QStringLiteral("pre-identify"), error);
    if (path.isEmpty())
        return false;
    m_identifyBackup.insert(universe, path);
    // The presses that pair lamps do no job meanwhile.
    holdPresses(universe, true);
    OpenDeckLink *l = link(universe);
    const int n = int(l->readSection(OpenDeckLink::LedBlock, OpenDeckLink::LedActivationId).size());
    m_lampCount.insert(universe, n);
    // Lamp i answers note i on channel 1: every lamp can be found.
    for (int i = 0; i < n; ++i)
        if (!l->alignLed(i, i, 1))
        {
            if (error) *error = l->lastError();
            endIdentify(universe, {});
            return false;
        }
    return true;
}

void SurfaceSetup::flashLamp(quint32 universe, int led, bool on)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    OutputPatch *fb = iom->feedbackPatch(universe);
    QLCIOPlugin *plugin = fb != nullptr ? fb->plugin() : nullptr;
    if (plugin == nullptr || fb->output() == QLCIOPlugin::invalidLine() || led < 0 || led > 127)
        return;
    // Note @p led on MIDI channel 1 (the channel carries no MIDI channel bits).
    plugin->sendFeedBack(universe, fb->output(), quint32(128 + led), on ? 255 : 0, QVariant());
}

bool SurfaceSetup::pairFromChannel(quint32 channel, int led, LampPair *pair)
{
    const quint32 c = channel & 0x0FFF;
    if (c < 128 || c > 255 || pair == nullptr)
        return false;
    pair->led = led;
    pair->note = int(c - 128);
    pair->channel = int(channel >> 12) + 1;
    return pair->channel >= 1 && pair->channel <= 16;
}

bool SurfaceSetup::endIdentify(quint32 universe, const QList<LampPair> &pairs, QString *error)
{
    const QString path = m_identifyBackup.take(universe);
    holdPresses(universe, false);
    bool ok = true;
    if (!path.isEmpty())
    {
        OpenDeckLink::Tables tables;
        ok = OpenDeckLink::loadBackup(path, &tables, error);
        if (ok)
        {
            const OpenDeckLink::RestoreResult res = link(universe)->restoreAll(tables);
            ok = res.failed.isEmpty();
            if (!ok && error)
                *error = res.failed.first();
        }
    }
    const Spec *s = specOf(universe);
    if (s != nullptr && !pairs.isEmpty())
        QSettings().setValue(QStringLiteral("surfacesetup/pairing/") + s->profile, pairsToText(pairs));
    check(universe);
    return ok;
}

/****************************************************************************
 * Rescan
 ****************************************************************************/

QString SurfaceSetup::rescanControllers(Doc *doc, QStringList *found)
{
    IOPluginCache *cache = doc->ioPluginCache();
    if (cache == nullptr)
        return QString();
    auto lines = [cache]() {
        QSet<QString> out;
        for (QLCIOPlugin *p : cache->plugins())
            if (p != nullptr && p->linesAreHardware() && (p->capabilities() & QLCIOPlugin::Input))
                for (const QString &l : p->inputs())
                    out.insert(p->name() + QLatin1Char('\n') + l);
        return out;
    };
    const QSet<QString> before = lines();
    for (QLCIOPlugin *p : cache->plugins())
        if (p != nullptr)
            p->rescan();
    const QSet<QString> after = lines();
    QStringList added;
    for (const QString &k : after)
        if (!before.contains(k))
            added << k.section(QLatin1Char('\n'), 1);
    added.sort();
    if (found != nullptr)
        *found = added;
    if (!added.isEmpty())
        return added.size() == 1 ? tr("Found %1.").arg(added.first())
                                 : tr("Found %1.").arg(added.join(QStringLiteral(", ")));
    return tr("No new controller found. The computer itself doesn't see one: check its power "
              "(the APC40 mk2 and many pads need their own adapter), the cable (charge-only "
              "cables carry no data), and the port.");
}
