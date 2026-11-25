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
#include <QFile>
#include <QUrl>

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
#include "inputpatch.h"
#include "qlcioplugin.h"
#include "universe.h"
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
}

DeskRecord::DeskRecord(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    for (int r = 0; r < RoleCount; ++r)
        m_universes[r] = kInvalid;
    m_elapsed.start();
    load();

    if (InputOutputMap *iom = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr)
    {
        /* Where each role is follows the patch and the devices: a repatch
           moves it with its device (by identity, never by universe). */
        connect(iom, &InputOutputMap::inputPatchChanged, this, &DeskRecord::rebind);
        connect(iom, &InputOutputMap::inputSlotChanged, this, &DeskRecord::rebind);
        connect(iom, &InputOutputMap::offlinePatchesChanged, this, &DeskRecord::rebind);
        connect(iom, &InputOutputMap::pluginConfigurationChanged, this, &DeskRecord::rebind);
        connect(iom, &InputOutputMap::inputDeviceDisconnected, this, &DeskRecord::rebind);
        // A replug: found again, and the Grand Master waits to pick up (Q316).
        connect(iom, &InputOutputMap::inputDeviceConnected, this, [this]() {
            rebind();
            rearmGrandMaster();
        });
    }
    if (m_doc != nullptr)
    {
        // A load: another patch, and the Grand Master waits to pick up.
        connect(m_doc, &Doc::loaded, this, [this]() {
            m_unlocked = false;
            rebind();
            rearmGrandMaster();
        });
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
    rearmGrandMaster();     // the app starting is a load
    rebind();
}

DeskRecord::~DeskRecord()
{
}

QList<DeskRecord::Role> DeskRecord::roles()
{
    return { Stop, GrandMaster, Go, Back };
}

QString DeskRecord::roleName(Role role)
{
    switch (role)
    {
        case Stop:          return tr("STOP");
        case GrandMaster:   return tr("Grand Master");
        case Go:            return tr("GO");
        case Back:          return tr("Back");
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
    s.remove(settingsGroup());
    s.beginGroup(settingsGroup());
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
    if (!isLocked())
        return true;
    emit refused(lockedText());
    return false;
}

bool DeskRecord::setBinding(Role role, const Binding &binding)
{
    if (!changeAllowed() || m_bindings[role] == binding)
        return false;
    m_bindings[role] = binding.isSet() ? binding : Binding();
    save();
    rebind();
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
            // The first universe with its device (two of one name: the first, and warned).
            for (quint32 u = 0; u < iom->universesCount(); ++u)
            {
                if (ids.value(u) == b.identity)
                {
                    m_universes[r] = u;
                    break;
                }
            }
        }
    }
    applyStatic();
    updateConnections();
    bool moved = false;
    for (int r = 0; r < RoleCount; ++r)
        moved = moved || was[r] != m_universes[r];
    if (moved)
        emit changed();
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

QStringList DeskRecord::missingDevices() const
{
    QStringList out;
    for (Role r : roles())
        if (state(r) == NotPatched && !out.contains(m_bindings[r].device))
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
        if (state(r) != NotPatched)
            continue;
        const QList<QPair<QLCIOPlugin *, quint32>> lines = linesOf(m_bindings[r].identity);
        if (lines.isEmpty())
            continue;
        // A universe whose input is free: no patch, no controller slot, nothing kept offline.
        quint32 target = kInvalid;
        const QList<Universe *> unis = iom->universes();
        for (int u = 0; u < unis.size() && target == kInvalid; ++u)
        {
            Universe *uni = unis.at(u);
            if (uni->inputPatch() != nullptr || uni->hasInputSlot())
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
}

void DeskRecord::grandMaster(uchar value)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    const int current = iom->grandMasterValue();
    if (m_gmPickup.accept(value, current, now()) && value != current)
        iom->setGrandMasterValue(value);
    if (grandMasterWaiting() != m_gmWasWaiting)
    {
        m_gmWasWaiting = grandMasterWaiting();
        emit grandMasterWaitingChanged(m_gmWasWaiting);
    }
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
    for (Role r : roles())
    {
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
    m_learning = true;
    m_learnRole = role;
    m_pendingKey = ~quint64(0);
    m_pendingPresses = 0;
    m_pendingReleased = false;
    m_faderSeen.clear();
    updateConnections();
    return true;
}

void DeskRecord::cancelLearn()
{
    if (!m_learning)
        return;
    m_learning = false;
    m_faderSeen.clear();
    m_pendingKey = ~quint64(0);
    updateConnections();
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

void DeskRecord::refuseLearn(const QString &why)
{
    m_pendingKey = ~quint64(0);
    m_pendingPresses = 0;
    emit learnRefused(why);
}

void DeskRecord::finishLearn(quint32 universe, quint32 channel)
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
    m_learning = false;
    m_faderSeen.clear();
    m_pendingKey = ~quint64(0);
    // A role learned on a control another role had: that one gives it up.
    for (Role r : roles())
        if (r != role && m_bindings[r].isSet() && m_bindings[r].identity == b.identity
            && m_bindings[r].channel == channel)
            m_bindings[r] = Binding();
    m_bindings[role] = b;
    save();
    rebind();
    emit learned(role);
    emit changed();
}

void DeskRecord::learnHeard(quint32 universe, quint32 channel, uchar value)
{
    const quint64 k = controlKey(universe, channel);
    const QString name = controlName(universe, channel);
    if (identityOfUniverse(universe).isEmpty())
        return;
    bool known = false;
    const bool fader = isFaderOnProfile(universe, channel, &known);

    if (takesFader(m_learnRole))
    {
        if (known && !fader)
        {
            if (value > 0)
                refuseLearn(tr("%1 is a button. The Grand Master takes a fader.").arg(name));
            return;
        }
        if (known && fader)
        {
            finishLearn(universe, channel);
            return;
        }
        /* No profile: a fader is told by moving. Two values above 0 with no
           0 between are a fader; a press and a release with one value are
           a button. */
        QSet<int> &seen = m_faderSeen[k];
        if (value == 0)
        {
            if (seen.count() == 1)
                refuseLearn(tr("%1 is a button. The Grand Master takes a fader.").arg(name));
            seen.clear();
            return;
        }
        seen.insert(value);
        if (seen.count() >= 2)
            finishLearn(universe, channel);
        return;
    }

    // A button: STOP, GO, Back.
    if (known && fader)
    {
        if (value > 0)
            refuseLearn(tr("%1 is a fader, knob or encoder. %2 takes a button.").arg(name, roleName(m_learnRole)));
        return;
    }
    if (value == 0)
    {
        if (k != m_pendingKey)
            return;
        m_pendingReleased = true;
        // GO and Back: one press and its release.
        if (m_learnRole != Stop)
            finishLearn(universe, channel);
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
        return;
    }
    if (!m_pendingReleased)
    {
        // No release between: a held button repeats its value, a fader does not.
        if (value != m_pendingValue)
            refuseLearn(tr("%1 sends a range of values: it is a fader or knob, not a button. "
                           "%2 takes a button.").arg(name, roleName(m_learnRole)));
        return;
    }
    // STOP: pressed, released, pressed again. The second press is the learn's.
    m_swallow.insert(k);
    finishLearn(universe, channel);
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
        return;
    const quint64 k = controlKey(universe, channel);
    if (value == 0)
    {
        auto h = m_stopHold.find(k);
        if (h != m_stopHold.end())
            h->released = true;
    }

    Role role = Stop;
    const bool desk = isDeskControl(universe, channel, &role);

    // The desk's STOP comes before anything, a learn included.
    if (desk && role == Stop && value > 0 && !m_swallow.contains(k))
    {
        stopPressed(k, value);
        return;
    }
    // The press that taught a role is not a press of it.
    if (m_swallow.contains(k))
    {
        if (value == 0)
            m_swallow.remove(k);
        return;
    }

    if (m_learning)
    {
        // Another role's control is not a new choice (re-learning a role's own is).
        if (desk && role != m_learnRole && value > 0)
        {
            emit learnRefused(tr("%1 is already this computer's %2. Choose another control.")
                                  .arg(controlName(universe, channel), roleName(role)));
            return;
        }
        learnHeard(universe, channel, value);
        return;
    }

    if (!desk)
        return;
    switch (role)
    {
        case GrandMaster:
            grandMaster(value);
            break;
        case Go:
        case Back:
            // One press, the edge from 0 (a held button resending is the one press).
            if (value == 0)
            {
                m_down.remove(k);
                break;
            }
            if (m_down.contains(k))
                break;
            m_down.insert(k);
            goPressed(role == Back);
            break;
        case Stop:
            break;
    }
}
