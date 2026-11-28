/*
  Q Light Controller Plus
  deskrecord.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QSettings>
#include <QTimer>
#include <QFile>
#include <QUrl>
#include <QRegularExpression>
#include <climits>

#include "controllerkeepalive.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"
#include "inputoutputmap.h"
#include "controllerjobs.h"
#include "ioplugincache.h"
#include "showoperate.h"
#include "showcontrol.h"
#include "controlmap.h"
#include "deskrecord.h"
#include "surfacesetup.h"
#include "surfaceboardlog.h"
#include "inputpatch.h"
#include "qlcioplugin.h"
#include "universe.h"
#include "surfacelayout.h"
#include "outputrulekeeper.h"
#include "doc.h"

namespace
{
const quint32 kInvalid = 0xFFFFFFFF;

quint64 controlKey(quint32 universe, quint32 channel)
{
    return (quint64(universe) << 32) | channel;
}

/* A settings key holds no '/' (QSettings' separator): as ControllerJobs. */
QString toKey(const QString &text)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(text, ":-._ #@"));
}

QString fromKey(const QString &key)
{
    return QUrl::fromPercentEncoding(key.toLatin1());
}

const QLatin1String kFileRoot("ThisComputersDesk");
/* The boards seedDefaults() has seeded, by identity (beside the roles in
   the group, kept by save()). */
const QLatin1String kSeeded("seeded");
/* Roles a board waited on because another board held them ("identity|role"). */
const QLatin1String kDeferred("deferred");
}

DeskRecord::DeskRecord(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    for (int r = 0; r < RoleCount; ++r)
    {
        m_universes[r] = kInvalid;
        m_lastUniverses[r] = kInvalid;
        m_lastState[r] = NotSet;
        m_lastPassthrough[r] = false;
    }
    m_elapsed.start();
    m_learnTimer = new QTimer(this);
    m_learnTimer->setSingleShot(true);
    connect(m_learnTimer, &QTimer::timeout, this, [this]() {
        if (m_learning)
            endLearn(tr("Learn stopped: nothing was pressed in %1 seconds.")
                         .arg(qMax(1, m_learnTimeoutMs / 1000)));
    });
    load();

    if (InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr)
    {
        /* Where each role is follows the patch and the devices: a repatch
           moves it with its device (by identity, never by universe). */
        /* Q316: a port (re)opened or rebound -- a patch, a slot, Patch It,
           a show's IOMap -- may dump where its controls sit: the Grand
           Master's own one waits out the dump and picks up (portChanged()). */
        connect(iom, &InputOutputMap::inputPatchChanged, this, [this](quint32 u) { portChanged(u); });
        connect(iom, &InputOutputMap::inputSlotChanged, this, [this](quint32 u) { portChanged(u); });
        // A board given its profile: its type's default roles (seedDefaults()).
        connect(iom, &InputOutputMap::profileChanged, this, [this](quint32 u) { portChanged(u); });
        connect(iom, &InputOutputMap::offlinePatchesChanged, this, [this]() { portChanged(kInvalid, true); });
        connect(iom, &InputOutputMap::pluginConfigurationChanged, this, [this]() { portChanged(kInvalid, true); });
        connect(iom, &InputOutputMap::inputDeviceDisconnected, this, [this](quint32 universe) {
            /* The spotter's device gone: every hazard safe and disarmed at
               once, before anything else (Q352/Q353). */
            for (Role r : rolesOn(universe))
                if (isHazardRole(r))
                {
                    if (OutputRuleKeeper *k = m_doc->outputRuleKeeper())
                        k->disarmHazards(tr("%1 (the spotter's %2) was lost")
                                             .arg(m_bindings[r].device, roleName(r)));
                    break;
                }
            forgetHeld(universe);
            rebind();
        });
        /* A universe removed takes its patch with it (removeUniverse() says
           only universeRemoved): a role there is no longer live. */
        connect(iom, &InputOutputMap::universeAdded, this, [this]() { portChanged(kInvalid); });
        connect(iom, &InputOutputMap::universeRemoved, this, [this]() { portChanged(kInvalid); });
        // Passthrough on: the desk can't hear the universe (its input goes to DMX).
        connect(iom, &InputOutputMap::universePassthroughChanged, this, [this]() { portChanged(kInvalid); });
        /* A replug: found again. Q316: the Grand Master waits to pick up
           only when its own device came back, never for a pad or another
           box on another universe (that would freeze a fade mid-show). */
        connect(iom, &InputOutputMap::inputDeviceConnected, this, [this](quint32 universe) {
            forgetHeld(universe);
            seedFor(universe, false);       // a board found: its roles, or the ones its predecessor held
            rebind();
            if (isLive(GrandMaster) && m_universes[GrandMaster] == universe)
                rearmGrandMaster();
        });
        /* The Grand Master's board given its feedback line (an auto-patch
           pairs it just after the input, when the motor was sent nowhere):
           its motor fader is sent the master now (the virtual bench,
           2026-11-27: the FaderPort plugged in was never sent it). */
        connect(iom, &InputOutputMap::feedbackPatchChanged, this, [this](quint32 universe) {
            if (isLive(GrandMaster) && m_universes[GrandMaster] == universe)
                sendGrandMasterToMotor();
        });
        /* A motor fader as the Grand Master follows the master when
           something else moves it (the footer, a VC slider, a fade). */
        connect(iom, &InputOutputMap::grandMasterValueChanged, this, [this](uchar) {
            if (!m_gmFromFader)
                sendGrandMasterToMotor();
        });
    }
    if (m_doc != nullptr)
    {
        /* A load opens the show's ports while it reads the file (the IOMap
           first), and the app paints meanwhile: the Grand Master hears
           nothing of the desk until it ends (grandMaster()), then waits. */
        connect(m_doc, &Doc::loading, this, [this]() {
            m_unlocked = false;
            rearmGrandMaster();
        });
        // A load: another patch, and the Grand Master waits to pick up.
        connect(m_doc, &Doc::loaded, this, [this]() {
            m_unlocked = false;
            if (m_learning)
                endLearn(tr("Learn stopped: another show was loaded."));
            forgetPatchUndo();
            // Another show's patch: where a role was in the last one says nothing here.
            for (int r = 0; r < RoleCount; ++r)
                m_lastUniverses[r] = kInvalid;
            seedDefaults();
            rebind();
            rearmGrandMaster();
        });
        /* Patch It's Undo is of the show it patched: never run against
           another one (it would clear that show's input, or remove its
           last universe). */
        connect(m_doc, &Doc::cleared, this, &DeskRecord::forgetPatchUndo);
        /* A learn open while the operator goes to Operate (or back) would
           take the next control touched: it ends with the mode. */
        connect(m_doc, &Doc::modeChanged, this, [this](Doc::Mode mode) {
            if (m_learning)
                endLearn(mode == Doc::Operate ? tr("Learn stopped: the show went to Operate.")
                                              : tr("Learn stopped: the mode changed."));
        });
        // Q319: a PIN set, changed or removed asks again for the next change.
        if (ShowOperate *op = m_doc->operateSettings())
            connect(op, &ShowOperate::settingsChanged, this, &DeskRecord::relock);
        if (ControlMap *cm = m_doc->controlMap())
            connect(cm, &ControlMap::runTabListChanged, this, [this]() { emit goTargetChanged(goTargetText()); });
        // A wake: the devices are found again; the Grand Master waits too.
        if (ProgrammerController *pc = m_doc->programmer())
            if (ControllerKeepAlive *ka = pc->controllerKeepAlive())
                connect(ka, &ControllerKeepAlive::systemStateChanged, this,
                        [this](ControllerKeepAlive::SystemState s) {
                            if (s == ControllerKeepAlive::Waking)
                                rearmGrandMaster();
                        });
    }
    seedDefaults();
    rebind();
    rearmGrandMaster();     // the app starting is a load
}

DeskRecord::~DeskRecord()
{
}

QList<DeskRecord::Role> DeskRecord::roles()
{
    return { Stop, GrandMaster, Go, Back, HazardHold, HazardArm, HazardKill };
}

QString DeskRecord::roleName(Role role)
{
    switch (role)
    {
        case Stop:          return tr("STOP");
        case GrandMaster:   return tr("Grand Master");
        case Go:            return tr("GO");
        case Back:          return tr("Back");
        case HazardHold:    return tr("Hazard hold");
        case HazardArm:     return tr("Hazard arm");
        case HazardKill:    return tr("Hazard kill");
    }
    return QString();
}

QString DeskRecord::roleKey(Role role)
{
    switch (role)
    {
        case Stop:          return QStringLiteral("stop");
        case GrandMaster:   return QStringLiteral("gm");
        case Go:            return QStringLiteral("go");
        case Back:          return QStringLiteral("back");
        case HazardHold:    return QStringLiteral("hazardhold");
        case HazardArm:     return QStringLiteral("hazardarm");
        case HazardKill:    return QStringLiteral("hazardkill");
    }
    return QString();
}

QString DeskRecord::settingsGroup()
{
    return QStringLiteral("thisComputersDesk");
}

/*****************************************************************************
 * Settings (this computer's; never the show)
 *****************************************************************************/

void DeskRecord::load()
{
    QSettings s;
    s.beginGroup(settingsGroup());
    for (Role r : roles())
    {
        s.beginGroup(roleKey(r));
        Binding b;
        b.identity = fromKey(s.value(QStringLiteral("identity")).toString());
        b.device = s.value(QStringLiteral("device")).toString();
        b.plugin = s.value(QStringLiteral("plugin")).toString();
        bool ok = false;
        const quint32 ch = s.value(QStringLiteral("channel")).toUInt(&ok);
        b.channel = ok ? ch : kInvalid;
        b.control = s.value(QStringLiteral("control")).toString();
        b.on = s.value(QStringLiteral("on"), true).toBool();
        m_bindings[r] = b.isSet() ? b : Binding();
        s.endGroup();
    }
    s.endGroup();
}

void DeskRecord::save() const
{
    QSettings s;
    const QVariant seeded = s.value(settingsGroup() + QLatin1Char('/') + kSeeded);
    const QVariant deferred = s.value(settingsGroup() + QLatin1Char('/') + kDeferred);
    s.remove(settingsGroup());
    s.beginGroup(settingsGroup());
    if (seeded.isValid())
        s.setValue(kSeeded, seeded);
    if (deferred.isValid())
        s.setValue(kDeferred, deferred);
    for (Role r : roles())
    {
        const Binding &b = m_bindings[r];
        if (!b.isSet())
            continue;
        s.beginGroup(roleKey(r));
        s.setValue(QStringLiteral("identity"), toKey(b.identity));
        s.setValue(QStringLiteral("device"), b.device);
        s.setValue(QStringLiteral("plugin"), b.plugin);
        s.setValue(QStringLiteral("channel"), b.channel);
        s.setValue(QStringLiteral("control"), b.control);
        s.setValue(QStringLiteral("on"), b.on);
        s.endGroup();
    }
    s.endGroup();
}

/*****************************************************************************
 * The record
 *****************************************************************************/

bool DeskRecord::anySet() const
{
    for (const Binding &b : m_bindings)
        if (b.isSet())
            return true;
    return false;
}

DeskRecord::State DeskRecord::state(Role role) const
{
    const Binding &b = m_bindings[role];
    if (!b.isSet())
        return NotSet;
    if (!b.on)
        return Off;
    if (m_universes[role] != kInvalid)
        return Live;
    return linesOf(b.identity).isEmpty() ? NotConnected : NotPatched;
}

quint32 DeskRecord::universeOf(Role role) const
{
    return state(role) == Live ? m_universes[role] : kInvalid;
}

QList<DeskRecord::Role> DeskRecord::rolesOn(quint32 universe) const
{
    QList<Role> out;
    const QString here = identityOfUniverse(universe);
    QString waitsFor;
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (here.isEmpty() && iom != nullptr)
        if (Universe *uni = iom->universe(universe))
            for (const Universe::OfflinePatch &rec : uni->offlinePatches())
                if (rec.tag == Universe::InputPatchTag && !rec.uid.isEmpty())
                    waitsFor = rec.uid;
    for (Role r : roles())
    {
        const Binding &b = m_bindings[r];
        if (!b.isSet() || !b.on)
            continue;
        if (m_universes[r] == universe)
        {
            out << r;
            continue;
        }
        /* Not live: its device gone, the universe waiting for it (no device
           of its own now). Its universe was this one, or the universe waits
           for a device of its name. */
        if (m_universes[r] == kInvalid && here.isEmpty()
            && (m_lastUniverses[r] == universe || (!waitsFor.isEmpty() && waitsFor == b.device)))
            out << r;
    }
    return out;
}

bool DeskRecord::isDeskControl(quint32 universe, quint32 channel, Role *role) const
{
    for (Role r : roles())
    {
        if (m_universes[r] == universe && m_bindings[r].channel == channel && m_bindings[r].isSet()
            && m_bindings[r].on)
        {
            if (role != nullptr)
                *role = r;
            return true;
        }
    }
    return false;
}

QString DeskRecord::stateText(Role role) const
{
    const Binding &b = m_bindings[role];
    switch (state(role))
    {
        case NotSet:
            return tr("not set");
        case Off:
            return tr("%1 · %2 · off on this computer").arg(b.device, b.control);
        case Live:
        {
            QString uni;
            if (InputOutputMap *iom = m_doc->inputOutputMap())
                uni = iom->getUniverseNameByIndex(int(m_universes[role]));
            if (uni.isEmpty())
                uni = tr("Universe %1").arg(m_universes[role] + 1);
            return tr("%1 · %2 · on %3").arg(b.device, b.control, uni);
        }
        case NotPatched:
            if (isOnPassthrough(role))
                return tr("%1 is on a universe that passes its input through to DMX: "
                          "turn passthrough off there").arg(b.device);
            return tr("%1 is not patched in this show").arg(b.device);
        case NotConnected:
            return tr("%1 is not connected").arg(b.device);
    }
    return QString();
}

bool DeskRecord::isLocked() const
{
    const ShowOperate *op = m_doc != nullptr ? m_doc->operateSettings() : nullptr;
    return op != nullptr && op->hasPin() && !m_unlocked;
}

bool DeskRecord::unlock(const QString &pin)
{
    const ShowOperate *op = m_doc != nullptr ? m_doc->operateSettings() : nullptr;
    if (op == nullptr || !op->hasPin())
        return true;
    if (!op->checkPin(pin))
        return false;
    m_unlocked = true;
    return true;
}

void DeskRecord::relock()
{
    m_unlocked = false;
}

QString DeskRecord::lockedText() const
{
    return tr("This computer's desk is locked by the show's PIN: enter it to change the desk.");
}

bool DeskRecord::changeAllowed()
{
    if (isLocked())
    {
        emit refused(lockedText());
        return false;
    }
    // Q319: one PIN, one change; the next one asks again.
    m_unlocked = false;
    return true;
}

bool DeskRecord::setBinding(Role role, const Binding &binding)
{
    if (!changeAllowed() || m_bindings[role] == binding)
        return false;
    if (m_bindings[role].identity != binding.identity)
        m_lastUniverses[role] = kInvalid;
    m_bindings[role] = binding.isSet() ? binding : Binding();
    save();
    rebind();
    emit changed();
    return true;
}

bool DeskRecord::assignRole(Role role, quint32 universe, quint32 channel, QString *why)
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    InputPatch *ip = iom != nullptr ? iom->inputPatch(universe) : nullptr;
    const QString id = identityOfUniverse(universe);
    auto refuse = [why](const QString &text) {
        if (why != nullptr)
            *why = text;
        return false;
    };
    if (ip == nullptr || id.isEmpty())
        return refuse(tr("That board is not patched here."));
    if (isLocked())
    {
        emit refused(lockedText());
        return refuse(lockedText());
    }
    bool known = false;
    const bool fader = isFaderOnProfile(universe, channel, &known);
    if (known && fader != takesFader(role))
        return refuse(takesFader(role) ? tr("The Grand Master takes a fader.") : tr("%1 takes a button.").arg(roleName(role)));
    if (role != Stop && isShowStop(universe, channel))
        return refuse(tr("That control is the show's emergency stop."));
    Binding b;
    b.identity = id;
    b.plugin = ip->plugin() != nullptr ? ip->plugin()->name() : QString();
    b.device = ControllerJobs::midiModelName(ip->inputName());
    b.channel = channel;
    b.control = controlName(universe, channel);
    b.on = true;
    // Another role on this control gives it up.
    for (Role r : roles())
        if (r != role && m_bindings[r].isSet() && m_bindings[r].identity == id && m_bindings[r].channel == channel)
        {
            m_bindings[r] = Binding();
            m_lastUniverses[r] = kInvalid;
        }
    m_unlocked = false;
    if (m_bindings[role] == b)
    {
        save();
        rebind();
        emit changed();
        return true;
    }
    if (m_bindings[role].identity != b.identity)
        m_lastUniverses[role] = kInvalid;
    m_bindings[role] = b;
    save();
    rebind();
    if (role == GrandMaster)
        rearmGrandMaster();     // it picks up before it moves the master
    emit changed();
    return true;
}

bool DeskRecord::clear(Role role)
{
    return setBinding(role, Binding());
}

bool DeskRecord::setOn(Role role, bool on)
{
    if (!m_bindings[role].isSet() || m_bindings[role].on == on)
        return false;
    Binding b = m_bindings[role];
    b.on = on;
    return setBinding(role, b);
}

/*****************************************************************************
 * Devices and the show's patch
 *****************************************************************************/

QString DeskRecord::identityOfUniverse(quint32 universe) const
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (iom == nullptr || universe >= iom->universesCount())
        return QString();
    InputPatch *ip = iom->inputPatch(universe);
    if (ip == nullptr || ip->plugin() == nullptr)
        return QString();
    /* A patch whose line is not there now (the device's list shrank under
       it) names no device: its inputName() is "None", and "midi:None" was
       seeded as a board on the bench (2026-11-27). */
    if (ip->input() == QLCIOPlugin::invalidLine() || ip->input() >= quint32(ip->plugin()->inputs().size()))
        return QString();
    return ControllerJobs::deskIdentityFor(ip->plugin()->name(), ip->inputName(),
                                           ip->plugin()->inputDeviceLocation(ip->input()));
}

QList<QPair<QLCIOPlugin *, quint32>> DeskRecord::linesOf(const QString &identity) const
{
    QList<QPair<QLCIOPlugin *, quint32>> out;
    if (identity.isEmpty() || m_doc == nullptr || m_doc->ioPluginCache() == nullptr)
        return out;
    for (QLCIOPlugin *plugin : m_doc->ioPluginCache()->plugins())
    {
        if (plugin == nullptr || (plugin->capabilities() & QLCIOPlugin::Input) == 0)
            continue;
        const QStringList lines = plugin->inputs();
        for (int l = 0; l < lines.size(); ++l)
            if (ControllerJobs::deskIdentityFor(plugin->name(), lines.at(l),
                                                plugin->inputDeviceLocation(quint32(l))) == identity)
                out << qMakePair(plugin, quint32(l));
    }
    return out;
}

void DeskRecord::rebind()
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    quint32 was[RoleCount];
    for (int r = 0; r < RoleCount; ++r)
    {
        was[r] = m_universes[r];
        m_universes[r] = kInvalid;
    }
    if (iom != nullptr && anySet())
    {
        // The identity on each universe's input, worked out once a pass.
        QHash<quint32, QString> ids;
        for (quint32 u = 0; u < iom->universesCount(); ++u)
            ids.insert(u, identityOfUniverse(u));
        for (Role r : roles())
        {
            const Binding &b = m_bindings[r];
            if (!b.isSet())
                continue;
            /* The first universe with its device (two of one name: the
               first, and warned). A universe passing its input through to
               DMX never says inputValueChanged: the desk can't hear it there. */
            for (quint32 u = 0; u < iom->universesCount(); ++u)
            {
                if (ids.value(u) == b.identity && !iom->getUniversePassthrough(int(u)))
                {
                    m_universes[r] = u;
                    break;
                }
            }
        }
    }
    for (int r = 0; r < RoleCount; ++r)
        if (m_universes[r] != kInvalid)
            m_lastUniverses[r] = m_universes[r];
    applyStatic();
    updateConnections();

    /* Q316: a Grand Master that becomes live here (an import, the On tick,
       a repatch, another fader) waits to pick up, as a learned one does:
       it never jumps the stage from where its fader sits. */
    QString gmKey;
    if (isLive(GrandMaster))
        gmKey = QStringLiteral("%1|%2|%3").arg(m_bindings[GrandMaster].identity)
                    .arg(m_universes[GrandMaster]).arg(m_bindings[GrandMaster].channel);
    if (!gmKey.isEmpty() && gmKey != m_gmLiveKey)
    {
        m_gmPickup.rearmFromHand(-1);
        notifyGrandMasterWaiting();
    }
    m_gmLiveKey = gmKey;

    /* Moved, or plugged in / unplugged with no universe of its own
       (NotPatched <-> NotConnected): state() reads the devices live, so it
       is compared with what the last pass said. */
    bool moved = false;
    for (int r = 0; r < RoleCount; ++r)
    {
        const State st = state(Role(r));
        /* Not patched, then on a passthrough universe (or off it again):
           one state, but what it says (and the chip's colour) changed. */
        const bool pass = isOnPassthrough(Role(r));
        moved = moved || was[r] != m_universes[r] || m_lastState[r] != st || m_lastPassthrough[r] != pass;
        m_lastState[r] = st;
        m_lastPassthrough[r] = pass;
    }
    checkHazardRolesLost();
    // The Hold let go and the Kill reach the output from the input thread.
    if (OutputRuleKeeper *k = m_doc != nullptr ? m_doc->outputRuleKeeper() : nullptr)
        k->setHazardControls(isLive(HazardHold) ? m_universes[HazardHold] : UINT_MAX,
                             m_bindings[HazardHold].channel,
                             isLive(HazardKill) ? m_universes[HazardKill] : UINT_MAX,
                             m_bindings[HazardKill].channel);
    if (moved)
        emit changed();
}

void DeskRecord::checkHazardRolesLost()
{
    QString lost;
    for (Role r : { HazardHold, HazardArm, HazardKill })
    {
        const bool live = isLive(r);
        if (m_hazardWasLive[r] && !live && lost.isEmpty())
            lost = m_bindings[r].isSet() ? tr("%1 (the spotter's %2) is no longer reachable")
                                               .arg(m_bindings[r].device, roleName(r))
                                         : tr("the spotter's %1 was cleared").arg(roleName(r));
        m_hazardWasLive[r] = live;
    }
    if (!lost.isEmpty() && m_doc != nullptr && m_doc->outputRuleKeeper() != nullptr)
        m_doc->outputRuleKeeper()->disarmHazards(lost);
}

QString DeskRecord::learnRefusal(Role role, quint32 universe, quint32 channel) const
{
    if (universe == ControlMap::KeyboardUniverse)
        return isHazardRole(role)
                   ? tr("The computer keyboard can't be the %1: anyone at the desk can press a key, and a "
                        "held key is lost when another window takes the keyboard. Learn it on a button of "
                        "the spotter's own device.").arg(roleName(role))
                   : QString();
    const QString id = identityOfUniverse(universe);
    const QString name = m_doc != nullptr ? controlName(universe, channel) : QString();
    for (Role r : roles())
    {
        if (r == role || !m_bindings[r].isSet() || m_bindings[r].identity != id || m_bindings[r].channel != channel)
            continue;
        // A hazard role shares no control: not GO's, Back's, a STOP's nor
        // another hazard role's (Hold on Kill's would lose the Kill).
        const bool clash = isHazardRole(role) || isHazardRole(r);
        if (clash)
            return tr("%1 is this computer's %2: the %3 can't share a control with it. Choose another control.")
                .arg(name, roleName(r), isHazardRole(role) ? roleName(role) : roleName(r));
    }
    if (!isHazardRole(role))
        return QString();
    /* Hold and Arm only from a device on a cable (second review, 5): a
       network input (OSC, Art-Net, sACN, the web) never says it went
       away, so a lost Hold would never let go. A Kill may come from
       anything. */
    if (role != HazardKill && m_doc != nullptr && m_doc->inputOutputMap() != nullptr)
        if (InputPatch *ip = m_doc->inputOutputMap()->inputPatch(universe))
            if (ip->plugin() != nullptr)
            {
                /* Over the air too (third review N3): an ENTTEC Wing is UDP
                   whatever its lines say; a CoreMIDI network session or a
                   Bluetooth MIDI device is MIDI over a link that can drop. */
                const QString line = ip->inputName().toLower();
                const bool wireless = ip->plugin()->name() == QLatin1String("ENTTEC Wing")
                                      || line.contains(QLatin1String("network session"))
                                      || line.contains(QLatin1String("network midi"))
                                      || line.contains(QLatin1String("rtp"))
                                      || line.contains(QLatin1String("bluetooth"))
                                      || line.contains(QRegularExpression(QStringLiteral("\\bble\\b")));
                if (!ip->plugin()->linesAreHardware() || wireless)
                    return tr("%1 comes over the network or the air (%2): a link that drops never says "
                              "the button was let go, so it can't hold or arm a hazard. Use a button on a "
                              "device plugged into this computer (USB, MIDI cable). The Hazard kill may be "
                              "anything.").arg(name, ip->plugin()->name());
            }
    // The show's own GO, Back or stop on this control.
    if (ControlMap *cm = m_doc != nullptr ? m_doc->controlMap() : nullptr)
        for (const ControlMap::Binding &b : cm->bindingsOn(universe, channel))
            switch (b.target.type)
            {
                case ControlMap::CueListTarget:
                case ControlMap::CueTarget:
                case ControlMap::CueListBackTarget:
                case ControlMap::EmergencyStopTarget:
                    return tr("%1 is this show's %2: the %3 can't share a control with a GO, a Back or a "
                              "stop. Choose another control.").arg(name, cm->targetName(b.target), roleName(role));
                default:
                    break;
            }
    return QString();
}

QString DeskRecord::hazardDeviceNote() const
{
    if (!m_bindings[Go].isSet())
        return QString();
    QStringList shared;
    for (Role r : { HazardHold, HazardArm, HazardKill })
        if (m_bindings[r].isSet() && m_bindings[r].identity == m_bindings[Go].identity)
            shared << roleName(r);
    if (shared.isEmpty())
        return QString();
    return tr("%1 on %2, the same device as GO. That works, but the spotter's controls are safer on a "
              "device of their own, away from the operator's hands.")
        .arg(shared.join(QStringLiteral(", ")), m_bindings[Go].device);
}

void DeskRecord::hazardInput(Role role, quint64 k, quint32 universe, quint32 channel, uchar value)
{
    OutputRuleKeeper *keeper = m_doc != nullptr ? m_doc->outputRuleKeeper() : nullptr;
    if (keeper == nullptr)
        return;
    if (role == HazardKill)
    {
        // Always, as STOP: a learn, Identify or Assign to Control open too.
        if (value > 0)
        {
            keeper->killHazards();
            emit fired(tr("Hazards killed"));
        }
        return;
    }
    // A release always lets go: back to safe at once.
    if (value == 0)
    {
        m_down.remove(k);
        if (role == HazardHold)
            keeper->setHazardHold(false);
        return;
    }
    /* The device's dump after a replug is no press, and a control held
       across it counts only after a release then a press (as GO). */
    ControlMap *cm = m_doc->controlMap();
    if (cm != nullptr && cm->isTriggerDump(universe, channel, value, tr("this computer's %1").arg(roleName(role))))
    {
        m_down.insert(k);
        return;
    }
    if (m_down.contains(k))
        return;
    if (m_doc->inputOutputMap() != nullptr && m_doc->inputOutputMap()->assignListening())
        return;
    m_down.insert(k);
    if (role == HazardHold)
        keeper->setHazardHold(true);
    else
    {
        keeper->armHazards();
        emit fired(tr("Hazards armed"));
    }
}

void DeskRecord::portChanged(quint32 universe, bool anyComesOnline)
{
    const QString was = m_gmLiveKey;
    seedFor(universe, anyComesOnline);
    rebind();
    /* The Grand Master's port (re)opened, or it moved to another: the
       device may dump its fader where it sits (a parked 0 would black the
       stage out), so it waits out the dump and picks up. Another
       universe's port leaves it alone (a pad replugged mid-fade). */
    if (isLive(GrandMaster)
        && (m_gmLiveKey != was || (universe != kInvalid && universe == m_universes[GrandMaster])))
        rearmGrandMaster();
}

bool DeskRecord::isOnPassthrough(Role role) const
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    const Binding &b = m_bindings[role];
    if (iom == nullptr || !b.isSet() || state(role) != NotPatched)
        return false;
    for (quint32 u = 0; u < iom->universesCount(); ++u)
        if (iom->getUniversePassthrough(int(u)) && identityOfUniverse(u) == b.identity)
            return true;
    return false;
}

void DeskRecord::forgetPatchUndo()
{
    if (m_patchedUniverse == kInvalid && !m_patchAddedUniverse)
        return;
    m_patchedUniverse = kInvalid;
    m_patchAddedUniverse = false;
    m_patchedDevice.clear();
    emit changed();
}

void DeskRecord::forgetHeld(quint32 universe)
{
    // The device went away or came back: a press it held never lets go.
    for (const quint64 k : m_swallow.values())
        if (quint32(k >> 32) == universe)
            endSwallow(k);
    for (auto it = m_stopHold.begin(); it != m_stopHold.end(); ++it)
        if (quint32(it.key() >> 32) == universe)
            it->released = true;
    for (const quint64 k : m_down.values())
        if (quint32(k >> 32) == universe)
            m_down.remove(k);
    m_shiftDown.remove(universe);
    if (isLive(GrandMaster) && m_universes[GrandMaster] == universe)
        m_gmTouched = false;
}

void DeskRecord::endSwallow(quint64 k)
{
    m_swallow.remove(k);
    m_swallowAt.remove(k);
    m_swallowSeen.remove(k);
}

void DeskRecord::notifyGrandMasterWaiting()
{
    if (grandMasterWaiting() != m_gmWasWaiting)
    {
        m_gmWasWaiting = grandMasterWaiting();
        emit grandMasterWaitingChanged(m_gmWasWaiting);
    }
}

void DeskRecord::setLearning(bool on)
{
    m_learning = on;
    /* A press heard by a learn is not a press of anything else: no show
       listener (a Virtual Console widget, a trigger, a speed master) hears
       it while the desk listens, as the Control Map's own Learn fires
       nothing. Releases still go through, so nothing held is left on. */
    if (InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr)
        iom->setDeskLearning(on);
}

void DeskRecord::applyStatic()
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (iom == nullptr)
        return;
    QSet<quint64> controls;
    for (Role r : roles())
        if (state(r) == Live)
            controls.insert(controlKey(m_universes[r], m_bindings[r].channel));
    iom->setStaticControls(controls);
    // Q314: the show's own Grand Master input is a fallback, armed only while this has none.
    iom->setDeskGrandMaster(state(GrandMaster) == Live);
}

QString DeskRecord::duplicateWarning() const
{
    QStringList lines;
    QSet<QString> said;
    for (Role r : roles())
    {
        const Binding &b = m_bindings[r];
        if (!b.isSet() || said.contains(b.identity))
            continue;
        if (linesOf(b.identity).count() > 1)
        {
            said.insert(b.identity);
            lines << tr("Two devices are called %1: this computer's desk can't tell them apart, so it "
                        "uses the first one patched. Unplug one, or plug it into another port.").arg(b.device);
        }
    }
    return lines.join(QLatin1Char('\n'));
}

QStringList DeskRecord::seededDevices() const
{
    QStringList out = QSettings().value(settingsGroup() + QLatin1Char('/') + kSeeded).toStringList();
    // An entry an earlier build seeded for a patch with no device on its line.
    out.removeAll(QStringLiteral("midi:None"));
    return out;
}

QStringList DeskRecord::deferredRoles() const
{
    return QSettings().value(settingsGroup() + QLatin1Char('/') + kDeferred).toStringList();
}

bool DeskRecord::seedDefaults()
{
    return seedFor(kInvalid, true);
}

bool DeskRecord::seedFor(quint32 cutOverOn, bool cutOverAll)
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (iom == nullptr || isLocked())
        return false;
    QStringList seeded = seededDevices();
    QStringList deferred = deferredRoles();
    bool marked = false;
    bool learnedAny = false;
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        if (ip == nullptr || ip->plugin() == nullptr)
            continue;
        const SurfaceLayout layout = SurfaceLayout::forProfile(ip->profileName());
        if (!layout.isValid())
            continue;
        const QString id = identityOfUniverse(u);
        if (id.isEmpty())
            continue;
        const bool seen = seeded.contains(id);
        if (!seen)
        {
            seeded << id;
            marked = true;
        }
        const bool mayCut = cutOverAll || u == cutOverOn;
        CutOver cut;
        for (const SurfaceLayout::Role &lr : layout.roles())
        {
            if (!lr.seed)
                continue;
            Role role = Stop;
            bool known = false;
            for (Role r : roles())
                if (roleKey(r) == lr.role)
                {
                    role = r;
                    known = true;
                }
            if (!known)
                continue;
            const QString dk = id + QLatin1Char('|') + roleKey(role);
            const bool held = m_bindings[role].isSet();
            bool cutting = false;
            if (held)
            {
                if (m_bindings[role].identity == id)
                    continue;
                /* Another board's: it keeps it while it is here. Gone, and
                   this one just came online: it hands the role over. */
                if (state(role) != NotConnected || !mayCut
                    || m_cutRefused.contains(id + QLatin1Char('|') + m_bindings[role].identity))
                {
                    if (!deferred.contains(dk))
                        deferred << dk;
                    marked = true;
                    continue;
                }
                cutting = true;
            }
            // A cleared default stays cleared, unless it waited for the board that held it.
            else if (seen && !deferred.contains(dk))
                continue;
            bool taken = false;
            for (Role r : roles())
                if (m_bindings[r].isSet() && m_bindings[r].identity == id && m_bindings[r].channel == lr.channel)
                    taken = true;
            /* A control the open show already uses is left to it: the desk
               would win on it (Q309) and silently block the show's trigger
               (the show's stop included, Q315). The role stays free to learn. */
            ControlMap *cm = m_doc->controlMap();
            if (taken || (cm != nullptr && !cm->bindingsOn(u, lr.channel).isEmpty()))
                continue;
            Binding b;
            b.identity = id;
            b.plugin = ip->plugin()->name();
            b.device = ControllerJobs::midiModelName(ip->inputName());
            b.channel = lr.channel;
            b.control = controlName(u, lr.channel);
            b.on = true;
            if (cutting)
            {
                cut.roles << role;
                cut.was << m_bindings[role];
            }
            m_bindings[role] = b;
            m_lastUniverses[role] = kInvalid;
            if (deferred.removeAll(dk) > 0)
                marked = true;
            learnedAny = true;
        }
        if (!cut.roles.isEmpty())
        {
            QStringList names;
            for (Role r : std::as_const(cut.roles))
                names << roleName(r);
            QString list = names.value(0);
            if (names.size() > 1)
                list = names.mid(0, names.size() - 1).join(QStringLiteral(", ")) + tr(" and ") + names.last();
            const QString from = cut.was.first().device;
            cut.newIdentity = id;
            cut.text = tr("%1 moved to %2 from %3.").arg(list, layout.shortName(), from);
            m_cut = cut;
        }
    }
    if (learnedAny)
        save();
    if (marked)
    {
        QSettings().setValue(settingsGroup() + QLatin1Char('/') + kSeeded, seeded);
        QSettings().setValue(settingsGroup() + QLatin1Char('/') + kDeferred, deferred);
    }
    if (learnedAny)
    {
        rebind();
        emit changed();
        if (!m_cut.text.isEmpty() && m_cut.roles.size() > 0 && m_cutAnnounced != m_cut.text + m_cut.newIdentity)
        {
            m_cutAnnounced = m_cut.text + m_cut.newIdentity;
            emit cutOver(m_cut.text);
        }
    }
    return learnedAny;
}

bool DeskRecord::undoCutOver()
{
    if (m_cut.roles.isEmpty() || isLocked())
        return false;
    for (int i = 0; i < m_cut.roles.size(); ++i)
    {
        const Role r = m_cut.roles.at(i);
        if (m_bindings[r].identity == m_cut.newIdentity)
        {
            m_cutRefused.insert(m_cut.newIdentity + QLatin1Char('|') + m_cut.was.at(i).identity);
            m_bindings[r] = m_cut.was.at(i);
            m_lastUniverses[r] = kInvalid;
        }
    }
    m_cut = CutOver();
    m_cutAnnounced.clear();
    save();
    rebind();
    emit changed();
    return true;
}

QStringList DeskRecord::missingDevices() const
{
    QStringList out;
    for (Role r : roles())
        if (state(r) == NotPatched && !isOnPassthrough(r) && !out.contains(m_bindings[r].device))
            out << m_bindings[r].device;
    return out;
}

quint32 DeskRecord::patchIt(QString *what)
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (iom == nullptr)
        return kInvalid;
    for (Role r : roles())
    {
        if (state(r) != NotPatched || isOnPassthrough(r))
            continue;
        const QList<QPair<QLCIOPlugin *, quint32>> lines = linesOf(m_bindings[r].identity);
        if (lines.isEmpty())
            continue;
        /* A universe whose input is free: no patch, no controller slot,
           nothing kept offline, and no passthrough (it would send the desk's
           presses to DMX, and the desk would never hear them). */
        quint32 target = kInvalid;
        const QList<Universe *> unis = iom->universes();
        for (int u = 0; u < unis.size() && target == kInvalid; ++u)
        {
            Universe *uni = unis.at(u);
            if (uni->inputPatch() != nullptr || uni->hasInputSlot() || uni->passthrough())
                continue;
            bool kept = false;
            for (const Universe::OfflinePatch &p : uni->offlinePatches())
                kept = kept || p.tag == Universe::InputPatchTag;
            if (!kept)
                target = quint32(u);
        }
        bool added = false;
        if (target == kInvalid)
        {
            if (!iom->addUniverse())
                return kInvalid;
            target = iom->universesCount() - 1;
            added = true;
        }
        const QPair<QLCIOPlugin *, quint32> line = lines.first();
        if (!iom->setInputPatch(target, line.first->name(), line.first->inputs().value(int(line.second)),
                                line.second))
        {
            if (added)
                iom->removeUniverse(int(target));
            return kInvalid;
        }
        m_patchedUniverse = target;
        m_patchAddedUniverse = added;
        m_patchedDevice = m_bindings[r].device;
        m_doc->setModified();
        rebind();
        QString uni = iom->getUniverseNameByIndex(int(target));
        if (uni.isEmpty())
            uni = tr("Universe %1").arg(target + 1);
        if (what != nullptr)
            *what = tr("Patched %1 to %2's input.").arg(m_patchedDevice, uni);
        emit changed();
        return target;
    }
    return kInvalid;
}

bool DeskRecord::undoPatchIt(QString *what)
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (iom == nullptr || m_patchedUniverse == kInvalid || m_patchedUniverse >= iom->universesCount())
    {
        m_patchedUniverse = kInvalid;
        return false;
    }
    const quint32 u = m_patchedUniverse;
    iom->setInputPatch(u, KInputNone, QString(), QLCIOPlugin::invalidLine());
    if (m_patchAddedUniverse && u == iom->universesCount() - 1)
        iom->removeUniverse(int(u));
    if (what != nullptr)
        *what = tr("Took %1 off the show's patch again.").arg(m_patchedDevice);
    m_patchedUniverse = kInvalid;
    m_patchAddedUniverse = false;
    m_doc->setModified();
    rebind();
    emit changed();
    return true;
}

/*****************************************************************************
 * GO, and the Grand Master's pick-up
 *****************************************************************************/

QString DeskRecord::goTargetText() const
{
    ControlMap *cm = m_doc != nullptr ? m_doc->controlMap() : nullptr;
    ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    const ShowTree::CueList *list = cm != nullptr && st != nullptr ? st->cueList(cm->runList()) : nullptr;
    if (list == nullptr)
        return tr("GO: the Run tab has no cue list");
    return tr("GO → %1").arg(list->name);
}

QString DeskRecord::grandMasterHint() const
{
    if (!grandMasterWaiting() || m_doc == nullptr || m_doc->inputOutputMap() == nullptr)
        return QString();
    const int current = m_doc->inputOutputMap()->grandMasterValue();
    const int percent = qRound(current * 100.0 / 255.0);
    return tr("GM waiting: bring the fader to %1 %").arg(percent);
}

void DeskRecord::rearmGrandMaster()
{
    m_gmPickup.rearm(now());
    notifyGrandMasterWaiting();
    // A motor fader is sent to the master: it is picked up when it arrives.
    sendGrandMasterToMotor();
}

void DeskRecord::sendGrandMasterToMotor()
{
    if (m_gmFromFader || m_gmTouched || m_doc == nullptr || !isLive(GrandMaster))
        return;
    /* A hand about to let go settles first: handReleased() sends it. */
    if (m_gmSettle != nullptr && m_gmSettle->isActive())
        return;
    m_gmMoved = false;
    InputOutputMap *iom = m_doc->inputOutputMap();
    if (iom == nullptr)
        return;
    const quint32 universe = m_universes[GrandMaster];
    const quint32 channel = m_bindings[GrandMaster].channel;
    if (iom->faderFact(universe, channel) != FaderFact::Motorised)
        return;
    const int gm = iom->grandMasterValue();
    if (SurfaceSetup *ss = m_doc->surfaceSetup())
        ss->boardLog()->note(universe, tr("Motor fader sent to the Grand Master, %1 % (%2)")
                                           .arg(qRound(gm * 100.0 / 255.0)).arg(gm));
    iom->sendFeedBack(universe, channel, gm, QVariant());
}

SurfaceLayout DeskRecord::layoutOf(quint32 universe) const
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    InputPatch *ip = iom != nullptr ? iom->inputPatch(universe) : nullptr;
    return ip != nullptr ? SurfaceLayout::forProfile(ip->profileName()) : SurfaceLayout();
}

bool DeskRecord::hasShift(quint32 universe) const
{
    return layoutOf(universe).shiftChannel() != kInvalid;
}

bool DeskRecord::shiftGuardsStop(quint32 universe) const
{
    const SurfaceLayout l = layoutOf(universe);
    if (l.shiftChannel() == kInvalid)
        return false;
    const ControlMap *cm = m_doc->controlMap();
    const int set = cm != nullptr ? cm->shiftGuardStopSetting() : int(ControlMap::GuardAuto);
    return set == ControlMap::GuardAuto ? l.shiftGuardsStop() : set == ControlMap::GuardOn;
}

bool DeskRecord::shiftGuardsRelease(quint32 universe) const
{
    const SurfaceLayout l = layoutOf(universe);
    if (l.shiftChannel() == kInvalid)
        return false;
    const ControlMap *cm = m_doc->controlMap();
    const int set = cm != nullptr ? cm->shiftGuardReleaseSetting() : int(ControlMap::GuardAuto);
    return set == ControlMap::GuardAuto ? l.shiftGuardsRelease() : set == ControlMap::GuardOn;
}

void DeskRecord::handReleased()
{
    if (m_gmSettle != nullptr)
        m_gmSettle->stop();
    m_gmTouched = false;
    if (!m_gmMoved)
        return;
    /* The motor fader holds the last position the host sent it and drives
       back there when let go: send it where the master is now, so it stays
       (a hand that moved it is the master's value; a move still waiting to
       pick up is NOT echoed as taken: the master has not moved to it). */
    sendGrandMasterToMotor();
}

void DeskRecord::grandMaster(uchar value)
{
    /* Q316: while a show loads its ports open and dump (the app paints
       between items): none of it is a hand on the fader. Picked up after. */
    if (m_doc->loadStatus() == Doc::Loading)
        return;
    InputOutputMap *iom = m_doc->inputOutputMap();
    const int current = iom->grandMasterValue();
    const bool taken = m_gmPickup.accept(value, current, now());
    if (!taken && m_universes[GrandMaster] != kInvalid && now() - m_gmRefusedNoteAt > 1000)
        if (SurfaceSetup *ss = m_doc->surfaceSetup())
        {
            // The drive-back's why, on the bench: the move was not the master's.
            m_gmRefusedNoteAt = now();
            ss->boardLog()->note(m_universes[GrandMaster],
                                 tr("Fader at %1 not taken: the Grand Master (%2) waits for it to come "
                                    "to it (pick-up)").arg(value).arg(current));
        }
    if (taken && value != current)
    {
        // The fader's own move: never sent back to it (a motor would fight the hand)
        m_gmFromFader = true;
        iom->setGrandMasterValue(value);
        m_gmFromFader = false;
    }
    /* A hand moved the fader: when it lets go (touch off), or has not moved
       for a moment on a board with no touch sensor, the motor is sent the
       master, so it does not drive back to where the host last sent it. */
    m_gmMoved = true;
    if (!m_gmTouched)
    {
        if (m_gmSettle == nullptr)
        {
            m_gmSettle = new QTimer(this);
            m_gmSettle->setSingleShot(true);
            connect(m_gmSettle, &QTimer::timeout, this, &DeskRecord::handReleased);
        }
        m_gmSettle->start(m_gmSettleMs);
    }
    notifyGrandMasterWaiting();
}

bool DeskRecord::sayRoleElsewhere(quint32 universe, quint32 channel, uchar value, quint64 k)
{
    /* The virtual bench (2026-11-27): STOP and the Grand Master learned on
       the APC64, the FaderPort plugged in beside it -- its Shift+Stop and
       its fader did nothing, and nothing said why. One STOP and one Grand
       Master per computer stays the rule (the old board is here, so
       nothing moves); the board says where they are. */
    if (value == 0)
    {
        m_down.remove(k);
        return false;
    }
    if (m_down.contains(k))
        return false;
    const SurfaceLayout lay = layoutOf(universe);
    if (!lay.isValid())
        return false;
    for (const SurfaceLayout::Role &r : lay.roles())
    {
        const Role role = r.role == roleKey(Stop) ? Stop
                        : r.role == roleKey(GrandMaster) ? GrandMaster : Go;
        if (role == Go || r.channel != channel || !isSet(role) || universeOf(role) == universe)
            continue;
        if (role == Stop)
            m_down.insert(k);       // once a press
        else if (now() - m_elsewhereSaidAt < 2000)
            return false;           // a fader: once in a while, not on every value
        m_elsewhereSaidAt = now();
        const QString where = m_bindings[role].device.isEmpty() ? m_bindings[role].identity
                                                                : m_bindings[role].device;
        if (SurfaceSetup *ss = m_doc->surfaceSetup())
            ss->boardLog()->note(universe, tr("%1 here does nothing: this computer's %1 is on %2")
                                               .arg(roleName(role), where));
        emit refused(tr("%1: this computer's %2 is on %3. Click it on this board's map to move it here.")
                         .arg(lay.shortName(), roleName(role), where));
        return true;
    }
    return false;
}

void DeskRecord::goPressed(bool back)
{
    ControlMap *cm = m_doc->controlMap();
    ShowTreeController *st = m_doc->showTree();
    const ShowTree::CueList *list = cm != nullptr && st != nullptr ? st->cueList(cm->runList()) : nullptr;
    if (list == nullptr || list->chaser == ShowTree::invalidId())
    {
        emit refused(tr("%1: the Run tab has no cue list.").arg(roleName(back ? Back : Go)));
        return;
    }
    ShowControl::Request r;
    r.chaser = list->chaser;
    r.command = back ? ShowControl::Back : ShowControl::Go;
    // The one run path, with its double-GO guard (Q317: one press, no arm).
    if (m_doc->showControl()->run(r, ShowControl::RouteControlMap))
        emit fired(back ? tr("Back: %1").arg(list->name) : tr("GO %1").arg(list->name));
}

void DeskRecord::stopPressed(quint64 k, uchar value)
{
    Q_UNUSED(value);
    // A press after a release fires; a held press that resends, once a second.
    StopHold &h = m_stopHold[k];
    const bool fire = h.released || !h.lastFired.isValid() || h.lastFired.elapsed() >= 1000;
    h.released = false;
    if (!fire)
        return;
    if (m_doc->showControl()->allOff(ShowControl::RouteControlMap))
    {
        h.lastFired.start();
        emit fired(tr("ALL OFF"));
    }
}

/*****************************************************************************
 * Show Review's "This computer" section
 *****************************************************************************/

QStringList DeskRecord::reviewLines() const
{
    QStringList out;
    // The spotter's roles: said when set, or when the show has a hazard.
    const OutputRuleKeeper *keeper = m_doc != nullptr ? m_doc->outputRuleKeeper() : nullptr;
    const bool hazards = keeper != nullptr && !keeper->hazards().isEmpty();
    for (Role r : roles())
    {
        if (isHazardRole(r) && !m_bindings[r].isSet() && !hazards)
            continue;
        QString text;
        switch (state(r))
        {
            case NotSet:
                text = r == Stop ? tr("not set: the show's own stop and the keyboard's are the stops")
                                 : tr("not set");
                break;
            case Live:
                text = tr("%1 · reachable now").arg(stateText(r));
                break;
            default:
                text = stateText(r);
                break;
        }
        out << tr("%1: %2").arg(roleName(r), text);
    }
    if (isLive(Go) || isLive(Back))
        out << goTargetText();
    const QString dup = duplicateWarning();
    if (!dup.isEmpty())
        out << dup.split(QLatin1Char('\n'));
    return out;
}

bool DeskRecord::reviewNeedsAttention() const
{
    for (Role r : roles())
        if (state(r) == NotPatched || state(r) == NotConnected)
            return true;
    return !duplicateWarning().isEmpty();
}

/*****************************************************************************
 * Export / import
 *****************************************************************************/

bool DeskRecord::exportTo(const QString &path, QString *error) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (error != nullptr)
            *error = file.errorString();
        return false;
    }
    QXmlStreamWriter w(&file);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement(kFileRoot);
    w.writeAttribute(QStringLiteral("Version"), QStringLiteral("1"));
    for (Role r : roles())
    {
        const Binding &b = m_bindings[r];
        if (!b.isSet())
            continue;
        w.writeStartElement(QStringLiteral("Role"));
        w.writeAttribute(QStringLiteral("Key"), roleKey(r));
        w.writeAttribute(QStringLiteral("Identity"), b.identity);
        w.writeAttribute(QStringLiteral("Device"), b.device);
        w.writeAttribute(QStringLiteral("Plugin"), b.plugin);
        w.writeAttribute(QStringLiteral("Channel"), QString::number(b.channel));
        w.writeAttribute(QStringLiteral("Control"), b.control);
        w.writeAttribute(QStringLiteral("On"), b.on ? QStringLiteral("1") : QStringLiteral("0"));
        w.writeEndElement();
    }
    w.writeEndElement();
    w.writeEndDocument();
    file.close();
    if (file.error() != QFile::NoError)
    {
        if (error != nullptr)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool DeskRecord::importFrom(const QString &path, QString *error)
{
    if (isLocked())
    {
        if (error != nullptr)
            *error = lockedText();
        emit refused(lockedText());
        return false;
    }
    m_unlocked = false;     // Q319: one PIN, one change
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
            *error = file.errorString();
        return false;
    }
    QXmlStreamReader xml(&file);
    if (!xml.readNextStartElement() || xml.name() != kFileRoot)
    {
        if (error != nullptr)
            *error = tr("This is not a desk file.");
        return false;
    }
    Binding read[RoleCount];
    while (xml.readNextStartElement())
    {
        if (xml.name() != QLatin1String("Role"))
        {
            xml.skipCurrentElement();
            continue;
        }
        const QXmlStreamAttributes a = xml.attributes();
        const QString key = a.value(QStringLiteral("Key")).toString();
        for (Role r : roles())
        {
            if (roleKey(r) != key)
                continue;
            Binding b;
            b.identity = a.value(QStringLiteral("Identity")).toString();
            b.device = a.value(QStringLiteral("Device")).toString();
            b.plugin = a.value(QStringLiteral("Plugin")).toString();
            bool ok = false;
            const quint32 ch = a.value(QStringLiteral("Channel")).toUInt(&ok);
            b.channel = ok ? ch : kInvalid;
            b.control = a.value(QStringLiteral("Control")).toString();
            b.on = a.value(QStringLiteral("On")).toString() != QLatin1String("0");
            read[r] = b.isSet() ? b : Binding();
        }
        xml.skipCurrentElement();
    }
    if (xml.hasError())
    {
        if (error != nullptr)
            *error = tr("This is not a desk file: %1").arg(xml.errorString());
        return false;
    }
    for (int r = 0; r < RoleCount; ++r)
        m_bindings[r] = read[r];
    save();
    rebind();
    emit changed();
    return true;
}

/*****************************************************************************
 * Learning
 *****************************************************************************/

bool DeskRecord::learn(Role role)
{
    if (!changeAllowed())
        return false;
    setLearning(true);
    m_learnRole = role;
    m_pendingKey = ~quint64(0);
    m_pendingPresses = 0;
    m_pendingReleased = false;
    m_faderSeen.clear();
    m_learnRefused.clear();
    // Never open for good: an open learn takes the next control touched.
    m_learnTimer->start(qMax(1, m_learnTimeoutMs));
    updateConnections();
    return true;
}

void DeskRecord::cancelLearn()
{
    if (m_learning)
        endLearn(QString());
}

void DeskRecord::endLearn(const QString &why)
{
    setLearning(false);
    m_learnTimer->stop();
    m_faderSeen.clear();
    m_learnRefused.clear();
    m_pendingKey = ~quint64(0);
    updateConnections();
    emit learnCancelled(why);
}

bool DeskRecord::isShowStop(quint32 universe, quint32 channel) const
{
    ControlMap *cm = m_doc != nullptr ? m_doc->controlMap() : nullptr;
    if (cm == nullptr)
        return false;
    for (const ControlMap::Binding &b : cm->bindingsOn(universe, channel))
        if (b.target.type == ControlMap::EmergencyStopTarget)
            return true;
    return false;
}

bool DeskRecord::isFaderOnProfile(quint32 universe, quint32 channel, bool *known) const
{
    *known = false;
    InputOutputMap *iom = m_doc->inputOutputMap();
    InputPatch *ip = iom == nullptr ? nullptr : iom->inputPatch(universe);
    if (ip == nullptr || ip->profile() == nullptr)
        return false;
    const QLCInputChannel *ich = ip->profile()->channel(channel);
    if (ich == nullptr)
        return false;
    *known = true;
    switch (ich->type())
    {
        case QLCInputChannel::Slider:
        case QLCInputChannel::Knob:
        case QLCInputChannel::Encoder:
            return true;
        default:
            return false;
    }
}

QString DeskRecord::controlName(quint32 universe, quint32 channel) const
{
    ControlMap *cm = m_doc->controlMap();
    QString name = cm != nullptr ? cm->controlName(universe, channel) : QString();
    // "Universe 1 · Note 82, MIDI ch 1": the desk names its device itself.
    const int dot = name.indexOf(QStringLiteral(" · "));
    return dot >= 0 ? name.mid(dot + 3) : name;
}

void DeskRecord::refuseLearn(quint64 k, const QString &why)
{
    /* Refused once, refused for this learn: a fader's next values (down to
       0) must not start a press of their own and be taken as a button. */
    m_learnRefused.insert(k);
    m_faderSeen.remove(k);
    m_pendingKey = ~quint64(0);
    m_pendingPresses = 0;
    emit learnRefused(why);
}

void DeskRecord::finishLearn(quint32 universe, quint32 channel, uchar value)
{
    InputPatch *ip = m_doc->inputOutputMap()->inputPatch(universe);
    Binding b;
    b.identity = identityOfUniverse(universe);
    b.plugin = ip != nullptr && ip->plugin() != nullptr ? ip->plugin()->name() : QString();
    b.device = ip != nullptr ? ControllerJobs::midiModelName(ip->inputName()) : QString();
    b.channel = channel;
    b.control = controlName(universe, channel);
    b.on = true;
    const Role role = m_learnRole;
    setLearning(false);
    m_learnTimer->stop();
    m_faderSeen.clear();
    m_learnRefused.clear();
    m_pendingKey = ~quint64(0);
    // A role learned on a control another role had: that one gives it up.
    for (Role r : roles())
        if (r != role && m_bindings[r].isSet() && m_bindings[r].identity == b.identity
            && m_bindings[r].channel == channel)
            m_bindings[r] = Binding();
    m_bindings[role] = b;
    save();
    rebind();
    /* Q316: a Grand Master learned on another fader picks up like one
       found after a load: it waits until it reaches the master, from
       where the learn left it, never jumping the stage there. */
    if (role == GrandMaster)
    {
        m_gmPickup.rearmFromHand(value);
        notifyGrandMasterWaiting();
    }
    emit learned(role);
    emit changed();
}

void DeskRecord::learnHeard(quint32 universe, quint32 channel, uchar value)
{
    const quint64 k = controlKey(universe, channel);
    const QString name = controlName(universe, channel);
    if (identityOfUniverse(universe).isEmpty() || m_learnRefused.contains(k))
        return;
    // A hazard role never on GO's, Back's or a STOP's control (Q352/Q353).
    const QString refusal = learnRefusal(m_learnRole, universe, channel);
    if (!refusal.isEmpty())
    {
        if (value > 0)
            refuseLearn(k, refusal);
        return;
    }
    /* Q315: learned here, the show's emergency stop on this control would
       be blocked, and the show's STOP would GO or move the master. */
    if (m_learnRole != Stop && isShowStop(universe, channel))
    {
        if (value > 0)
            refuseLearn(k, tr("%1 is the show's emergency stop. Choose another control for %2.")
                               .arg(name, roleName(m_learnRole)));
        return;
    }
    bool known = false;
    const bool fader = isFaderOnProfile(universe, channel, &known);

    if (takesFader(m_learnRole))
    {
        if (known && !fader)
        {
            if (value > 0)
                refuseLearn(k, tr("%1 is a button. The Grand Master takes a fader.").arg(name));
            return;
        }
        if (known && fader)
        {
            finishLearn(universe, channel, value);
            return;
        }
        /* No profile: a fader is told by moving. Two values above 0 with no
           0 between are a fader; a press and a release with one value are
           a button. */
        QSet<int> &seen = m_faderSeen[k];
        if (value == 0)
        {
            if (seen.count() == 1)
                refuseLearn(k, tr("%1 is a button. The Grand Master takes a fader.").arg(name));
            else
                seen.clear();
            return;
        }
        seen.insert(value);
        if (seen.count() >= 2)
            finishLearn(universe, channel, value);
        return;
    }

    // A button: STOP, GO, Back.
    if (known && fader)
    {
        if (value > 0)
            refuseLearn(k, tr("%1 is a fader, knob or encoder. %2 takes a button.").arg(name, roleName(m_learnRole)));
        return;
    }
    if (value == 0)
    {
        if (k != m_pendingKey)
            return;
        m_pendingReleased = true;
        if (m_holdLearnTimer != nullptr)
            m_holdLearnTimer->stop();
        /* The Hold (second review, 9): pressed, held about a second, let
           go -- a button that lets go when the hand does. */
        if (m_learnRole == HazardHold && m_pendingAt.elapsed() < kHoldLearnMs)
        {
            refuseLearn(k, tr("Hold %1 down about a second, then let go: the Hazard hold is learned "
                              "from a press held, so it is a button that lets go with the hand.").arg(name));
            m_learnRefused.remove(k);       // the same button, held this time
            return;
        }
        // GO and Back: one press and its release.
        if (m_learnRole != Stop)
            finishLearn(universe, channel, value);
        return;
    }
    const bool fresh = k != m_pendingKey || !m_pendingAt.isValid() || m_pendingAt.elapsed() > m_confirmMs;
    if (fresh)
    {
        m_pendingKey = k;
        m_pendingValue = value;
        m_pendingReleased = false;
        m_pendingPresses = 1;
        m_pendingAt.start();
        if (m_learnRole == Stop)
            emit learnConfirming(Stop);
        // The Hold: a release must come (a latching button never lets go).
        if (m_learnRole == HazardHold)
        {
            if (m_holdLearnTimer == nullptr)
            {
                m_holdLearnTimer = new QTimer(this);
                m_holdLearnTimer->setSingleShot(true);
                connect(m_holdLearnTimer, &QTimer::timeout, this, [this]() {
                    if (!m_learning || m_learnRole != HazardHold || m_pendingReleased
                        || m_pendingKey == ~quint64(0))
                        return;
                    refuseLearn(m_pendingKey, tr("No release came: a latching or toggle button stays on "
                                                 "when the hand lets go, so it can't be the Hazard hold. "
                                                 "Choose a momentary button."));
                });
            }
            m_holdLearnTimer->start(kHoldReleaseMs);
            emit learnConfirming(HazardHold);
        }
        return;
    }
    if (!m_pendingReleased)
    {
        // No release between: a held button repeats its value, a fader does not.
        if (value != m_pendingValue)
            refuseLearn(k, tr("%1 sends a range of values: it is a fader or knob, not a button. "
                           "%2 takes a button.").arg(name, roleName(m_learnRole)));
        return;
    }
    // STOP: pressed, released, pressed again. The second press is the learn's.
    m_swallow.insert(k);
    m_swallowAt.insert(k, now());
    m_swallowSeen.remove(k);
    finishLearn(universe, channel, value);
}

/*****************************************************************************
 * Input
 *****************************************************************************/

void DeskRecord::updateConnections()
{
    InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    if (iom == nullptr)
        return;
    const bool want = m_learning || anySet() || !m_down.isEmpty();
    if (want == m_inputConnected)
        return;
    if (want)
        connect(iom, &InputOutputMap::inputValueChanged, this, &DeskRecord::handleInput);
    else
        disconnect(iom, &InputOutputMap::inputValueChanged, this, &DeskRecord::handleInput);
    m_inputConnected = want;
}

void DeskRecord::handleInput(quint32 universe, quint32 channel, uchar value)
{
    if (universe == ControlMap::KeyboardUniverse)
    {
        // A key while a hazard role learns: refused, and said why.
        if (m_learning && value > 0)
        {
            const QString why = learnRefusal(m_learnRole, universe, channel);
            if (!why.isEmpty())
                emit learnRefused(why);
        }
        return;
    }
    const quint64 k = controlKey(universe, channel);
    if (value == 0)
    {
        auto h = m_stopHold.find(k);
        if (h != m_stopHold.end())
            h->released = true;
    }

    Role role = Stop;
    const bool desk = isDeskControl(universe, channel, &role);

    /* The board's Shift key and its fader's touch sensor: not roles, but
       the guards and the motor read them. */
    if (anySet())
    {
        const SurfaceLayout lay = layoutOf(universe);
        if (lay.isValid() && channel == lay.shiftChannel())
        {
            if (value > 0)
                m_shiftDown.insert(universe);
            else
                m_shiftDown.remove(universe);
        }
        if (lay.isValid() && lay.gmTouchChannel() != kInvalid && channel == lay.gmTouchChannel()
            && isLive(GrandMaster) && m_universes[GrandMaster] == universe)
        {
            if (value > 0)
            {
                m_gmTouched = true;
                if (m_gmSettle != nullptr)
                    m_gmSettle->stop();
            }
            else
            {
                m_gmMoved = true;   // the master may have moved meanwhile (a fade)
                handReleased();
            }
        }
    }

    /* The press that taught a role is not a press of it, until its release.
       A release that was lost (a cable pulled, a toggle-mode button) must
       not swallow the first real press for long: the first value after it
       may be the held press repeating; a second one, or a second of quiet,
       is a real press (as ControlMap's emergency stop). */
    if (m_swallow.contains(k))
    {
        if (value == 0)
        {
            endSwallow(k);
            return;
        }
        if (m_swallowSeen.contains(k) || now() - m_swallowAt.value(k, now()) > 1000)
        {
            endSwallow(k);
        }
        else
        {
            m_swallowSeen.insert(k);
            return;
        }
    }
    /* A device that came back dumps its state (phase R): the Control Map's
       one rule for each, never a copy. The STOP: at most one value ignored
       per replug (an extra ALL OFF beats a missed one); GO and Back below:
       its burst, then a release before a press. */
    ControlMap *cm = m_doc->controlMap();
    if (desk && role == Stop && cm != nullptr && cm->isStateDump(universe, channel, value))
        return;
    // The desk's STOP comes before anything, a learn included.
    if (desk && role == Stop && value > 0)
    {
        /* A board that asks for Shift: a bare STOP does nothing, and says so.
           Held, it says it once. */
        if (shiftGuardsStop(universe) && !shiftHeld(universe))
        {
            if (m_down.contains(k))
                return;
            m_down.insert(k);
            if (SurfaceSetup *ss = m_doc->surfaceSetup())
                ss->boardLog()->note(universe, tr("STOP refused: Shift (channel %1) is not held")
                                                   .arg(layoutOf(universe).shiftChannel()));
            const SurfaceLayout lay = layoutOf(universe);
            emit refused(lay.guardedStopWords().isEmpty()
                             ? tr("%1: Shift+Stop stops.").arg(lay.shortName())
                             : tr("%1: %2 stops.").arg(lay.shortName(), lay.guardedStopWords()));
            return;
        }
        if (SurfaceSetup *ss = m_doc->surfaceSetup())
            ss->boardLog()->note(universe, tr("STOP: the desk stops"));
        stopPressed(k, value);
        return;
    }
    // So do the spotter's Kill and their Hold let go of: back to safe.
    if (desk && ((role == HazardKill && value > 0) || (role == HazardHold && value == 0)))
    {
        hazardInput(role, k, universe, channel, value);
        if (role == HazardKill)
            return;
    }
    if (desk && role == Stop && value == 0)
        m_down.remove(k);

    if (m_learning)
    {
        /* Another role's control is not a new choice (re-learning a role's
           own is), and it still does its job: a learn left open never
           takes the desk's GO, Back or Grand Master away. */
        if (!desk || role == m_learnRole)
        {
            learnHeard(universe, channel, value);
            return;
        }
        if (value > 0 && !m_learnRefused.contains(k))
        {
            m_learnRefused.insert(k);   // said once, not on every value of a fader
            emit learnRefused(tr("%1 is already this computer's %2. Choose another control.")
                                  .arg(controlName(universe, channel), roleName(role)));
        }
    }

    if (!desk)
    {
        sayRoleElsewhere(universe, channel, value, k);
        return;
    }
    /* A controller's setup is listening to this board (Identify Lamps):
       its presses pair lamps, they do no job. The STOP went above. */
    if (value > 0 && m_doc != nullptr && m_doc->inputOutputMap()->isSetupHeld(universe))
        return;
    switch (role)
    {
        case GrandMaster:
            grandMaster(value);
            break;
        case Go:
        case Back:
            /* The device's dump after a replug is no press (an unwanted GO is
               worse than a missed one), and the key counts as held: one
               really held across the replug resending (aftertouch, an axis)
               GOes only after a release then a press, as the PMJ's Go. */
            if (cm != nullptr
                && cm->isTriggerDump(universe, channel, value, tr("this computer's %1").arg(roleName(role))))
            {
                m_down.insert(k);
                break;
            }
            // One press, the edge from 0 (a held button resending is the one press).
            if (value == 0)
            {
                m_down.remove(k);
                break;
            }
            if (m_down.contains(k))
                break;
            /* Assign to Control… is open: a press finds the control there
               and fires nothing (phase A). The STOP above still stops; the
               Grand Master still follows its fader. */
            if (m_doc->inputOutputMap() != nullptr && m_doc->inputOutputMap()->assignListening())
                break;
            /* After an emergency stop, the GO that would release it needs
               Shift on a board that asks for it: Shift+GO. Back, and GO with
               no stop to release, are as ever. */
            if (role == Go && shiftGuardsRelease(universe) && !shiftHeld(universe)
                && m_doc->outputRuleKeeper() != nullptr && m_doc->outputRuleKeeper()->emergencyStopped())
            {
                m_down.insert(k);
                const SurfaceLayout lay = layoutOf(universe);
                emit refused(lay.guardedReleaseWords().isEmpty()
                                 ? tr("%1: stopped. Shift+Play releases the stop.").arg(lay.shortName())
                                 : tr("%1: stopped. %2 releases the stop.").arg(lay.shortName(), lay.guardedReleaseWords()));
                break;
            }
            m_down.insert(k);
            goPressed(role == Back);
            break;
        case HazardHold:
        case HazardArm:
        case HazardKill:
            hazardInput(role, k, universe, channel, value);
            break;
        case Stop:
            break;
    }
}
