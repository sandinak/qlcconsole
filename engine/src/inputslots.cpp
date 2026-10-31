/*
  Q Light Controller Plus
  inputslots.cpp

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

/*
 * Controller slots (InputOutputMap): a universe's input bound to an ordered
 * list of devices, the first one connected driving it. Kept apart from
 * inputoutputmap.cpp so the patching code there stays as it was: the slot
 * logic only calls its public setInputPatch() and keeps the offline record
 * the rest of the app already shows ("not connected — kept").
 */

#include <QDebug>
#include <QHash>
#include <QSet>

#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "inputpatch.h"
#include "universe.h"
#include "doc.h"

namespace
{
    /** Controls past this are not a gamepad's (HID: axes, then buttons). */
    const quint32 kMaxRoleChannel = 64;

    bool sameDevice(const Universe::InputSlotDevice &a, const Universe::InputSlotDevice &b)
    {
        return a.plugin == b.plugin && a.anyGamepad == b.anyGamepad
               && (a.anyGamepad || a.uid == b.uid);
    }
}

QStringList InputOutputMap::standardChannelRoles()
{
    /* The stock Xbox and PlayStation controller profiles (xbox.qxhid,
       playstation.qxhid), which every Xbox, DualShock 4 and DualSense is
       read into on a Mac and, for the pads the HID plugin knows, on Linux:
       the six axes, then the buttons, then a PlayStation pad's touchpad. */
    static const QStringList roles = {
        QStringLiteral("lx"), QStringLiteral("ly"), QStringLiteral("lt"),
        QStringLiteral("rx"), QStringLiteral("ry"), QStringLiteral("rt"),
        QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("x"), QStringLiteral("y"),
        QStringLiteral("lb"), QStringLiteral("rb"),
        QStringLiteral("back"), QStringLiteral("start"),
        QStringLiteral("ls"), QStringLiteral("rs"), QStringLiteral("guide"),
        QStringLiteral("dpad_up"), QStringLiteral("dpad_down"),
        QStringLiteral("dpad_left"), QStringLiteral("dpad_right"),
        QString(),      // Xbox Share / PlayStation touchpad click: no common role
        QStringLiteral("touch_x"), QStringLiteral("touch_y"), QStringLiteral("touch_down"),
        QStringLiteral("touch2_x"), QStringLiteral("touch2_y"), QStringLiteral("touch2_down"),
    };
    return roles;
}

QStringList InputOutputMap::channelRoles(QLCIOPlugin *plugin, quint32 line)
{
    QStringList roles;
    if (plugin == nullptr || line == QLCIOPlugin::invalidLine())
        return roles;
    for (quint32 ch = 0; ch < kMaxRoleChannel; ++ch)
        roles << plugin->inputChannelRole(line, ch);
    while (roles.isEmpty() == false && roles.last().isEmpty())
        roles.removeLast();
    return roles;
}

QVector<quint32> InputOutputMap::slotChannelMap(const QStringList &showRoles,
                                                const QStringList &deviceRoles)
{
    if (showRoles.isEmpty() || deviceRoles.isEmpty())
        return QVector<quint32>();

    QHash<QString, int> showAt;
    for (int i = 0; i < showRoles.count(); ++i)
        if (showRoles.at(i).isEmpty() == false && showAt.contains(showRoles.at(i)) == false)
            showAt.insert(showRoles.at(i), i);

    QVector<quint32> map(deviceRoles.count());
    bool moved = false;
    for (int c = 0; c < deviceRoles.count(); ++c)
    {
        int to = c;
        if (deviceRoles.at(c).isEmpty() == false)
        {
            QHash<QString, int>::const_iterator it = showAt.constFind(deviceRoles.at(c));
            if (it != showAt.constEnd())
                to = it.value();
        }
        map[c] = quint32(to);
        moved = moved || to != c;
    }
    return moved ? map : QVector<quint32>();
}

bool InputOutputMap::isGamepadLine(QLCIOPlugin *plugin, quint32 line)
{
    if (plugin == nullptr)
        return false;
    if (plugin->inputDeviceKind(line).isEmpty() == false)
        return true;
    return channelRoles(plugin, line).isEmpty() == false;
}

QString InputOutputMap::stockInputProfile(const QString &kind) const
{
    QString name;
    if (kind == QLatin1String("xbox"))
        name = QStringLiteral("Microsoft Xbox Controller");
    else if (kind == QLatin1String("dualshock4"))
        name = QStringLiteral("Sony DualShock 4");
    else if (kind == QLatin1String("dualsense"))
        name = QStringLiteral("Sony DualSense");
    return name.isEmpty() == false && const_cast<InputOutputMap *>(this)->profile(name) != nullptr
           ? name : QString();
}

QList<Universe::InputSlotDevice> InputOutputMap::inputSlot(quint32 universe) const
{
    if (universe >= universesCount())
        return QList<Universe::InputSlotDevice>();
    return m_universeArray.at(universe)->inputSlot();
}

int InputOutputMap::inputSlotLive(quint32 universe) const
{
    if (universe >= universesCount())
        return -1;
    return m_universeArray.at(universe)->inputSlotLive();
}

int InputOutputMap::slotDeviceLine(quint32 universe, const Universe::InputSlotDevice &dev,
                                   QLCIOPlugin **pluginOut) const
{
    QLCIOPlugin *plugin = m_doc->ioPluginCache()->plugin(dev.plugin);
    if (pluginOut != nullptr)
        *pluginOut = plugin;
    if (plugin == nullptr)
        return -1;
    const QStringList lines = plugin->inputs();
    if (dev.anyGamepad == false)
        return lines.indexOf(dev.uid);

    /* Any game controller: the one it has, while it is still here (two
       pads on must not swap it to and fro), else the first one free. */
    InputPatch *ip = m_universeArray.at(universe)->inputPatch();
    if (ip != nullptr && ip->plugin() == plugin && ip->isPatched()
        && int(ip->input()) < lines.count() && lines.at(int(ip->input())) == ip->inputUID()
        && isGamepadLine(plugin, ip->input()))
        return int(ip->input());

    for (int l = 0; l < lines.count(); ++l)
    {
        bool taken = false;
        for (quint32 u = 0; u < universesCount() && taken == false; ++u)
        {
            if (u == universe)
                continue;
            InputPatch *other = m_universeArray.at(u)->inputPatch();
            if (other != nullptr && other->plugin() == plugin && int(other->input()) == l)
                taken = true;
            foreach (const Universe::InputSlotDevice &d, m_universeArray.at(u)->inputSlot())
                if (d.anyGamepad == false && d.plugin == dev.plugin && d.uid == lines.at(l))
                    taken = true;
        }
        if (taken == false && isGamepadLine(plugin, quint32(l)))
            return l;
    }
    return -1;
}

void InputOutputMap::setInputSlot(quint32 universe, const QList<Universe::InputSlotDevice> &devices,
                                  const QStringList &roles)
{
    if (universe >= universesCount())
        return;
    Universe *uni = m_universeArray.at(universe);
    InputPatch *ip = uni->inputPatch();

    if (devices.isEmpty())
    {
        uni->setInputSlot(devices);
        uni->setInputSlotLive(-1);
        uni->setInputSlotRoles(QStringList());
        if (ip != nullptr)
            ip->setChannelMap(QVector<quint32>());
        emit inputSlotChanged(universe);
        return;
    }

    uni->setInputSlot(devices);

    /* Which entry the present patch is, if any. */
    int live = -1;
    if (ip != nullptr && ip->isPatched() && ip->plugin() != nullptr)
    {
        for (int k = 0; k < devices.count() && live < 0; ++k)
        {
            const Universe::InputSlotDevice &d = devices.at(k);
            if (d.plugin != ip->pluginName())
                continue;
            if ((d.anyGamepad == false && d.uid == ip->inputUID())
                || (d.anyGamepad && isGamepadLine(ip->plugin(), ip->input())))
                live = k;
        }
    }
    uni->setInputSlotLive(live);

    QStringList showRoles = roles;
    if (showRoles.isEmpty())
    {
        /* The show's channels are the first device's: read off it when it
           is here, else the standard order the stock profiles give. */
        if (live == 0)
            showRoles = channelRoles(ip->plugin(), ip->input());
        else
            showRoles = standardChannelRoles();
    }
    uni->setInputSlotRoles(showRoles);
    if (live >= 0)
        ip->setChannelMap(slotChannelMap(showRoles, channelRoles(ip->plugin(), ip->input())));

    applyInputSlot(universe);
    emit inputSlotChanged(universe);
}

bool InputOutputMap::applyInputSlot(quint32 universe)
{
    if (universe >= universesCount())
        return false;
    Universe *uni = m_universeArray.at(universe);
    if (uni->hasInputSlot() == false)
        return false;

    QList<Universe::InputSlotDevice> devs = uni->inputSlot();
    const int live = uni->inputSlotLive();
    InputPatch *ip = uni->inputPatch();
    const bool patched = ip != nullptr && ip->isPatched() && ip->plugin() != nullptr;

    /* The live entry keeps what was set on it meanwhile (a profile chosen
       in the patch editor) for when it is next picked. */
    if (live >= 0 && live < devs.count() && patched)
    {
        Universe::InputSlotDevice &d = devs[live];
        d.profile = ip->profileName() == KInputNone ? QString() : ip->profileName();
        d.hidProfile = ip->hidProfileName();
        d.params = ip->getPluginParameters();
        d.line = ip->input();
        uni->setInputSlot(devs);
    }

    int winner = -1;
    int line = -1;
    QLCIOPlugin *plugin = nullptr;
    for (int k = 0; k < devs.count() && winner < 0; ++k)
    {
        QLCIOPlugin *p = nullptr;
        const int l = slotDeviceLine(universe, devs.at(k), &p);
        if (l >= 0)
        {
            winner = k;
            line = l;
            plugin = p;
        }
    }

    if (winner >= 0)
    {
        const QString uid = plugin->inputs().value(line);
        if (winner == live && patched && ip->plugin() == plugin && ip->inputUID() == uid)
        {
            if (int(ip->input()) != line)
            {
                // Still the one, on another line (a device before it went).
                QMutexLocker locker(&m_universeMutex);
                uni->setInputPatch(plugin, quint32(line), ip->profile());
            }
            return false;
        }

        const Universe::InputSlotDevice d = devs.at(winner);
        qDebug() << "[IOMAP] universe" << universe + 1 << "controller slot: using" << uid
                 << "(entry" << winner + 1 << "of" << devs.count() << ")";
        m_applyingSlot = true;
        setInputPatch(universe, d.plugin, QString(), quint32(line), d.profile);
        m_applyingSlot = false;
        if (InputPatch *nip = uni->inputPatch())
        {
            nip->setHidProfileName(d.hidProfile);
            /* The plugin keeps a universe's parameters whatever is patched
               to it: the last device's must not stay on this one. */
            foreach (const QString &key, nip->getPluginParameters().keys())
                if (d.params.contains(key) == false)
                    nip->unsetPluginParameter(key);
            QMapIterator<QString, QVariant> it(d.params);
            while (it.hasNext())
            {
                it.next();
                nip->setPluginParameter(it.key(), it.value());
            }
            nip->setChannelMap(slotChannelMap(uni->inputSlotRoles(),
                                              channelRoles(plugin, quint32(line))));
        }
        if (d.anyGamepad == false)
        {
            devs[winner].line = quint32(line);
            uni->setInputSlot(devs);
        }
        uni->setInputSlotLive(winner);
        emit inputDeviceConnected(universe, d.plugin, uid);
        emit inputSlotChanged(universe);
        emit offlinePatchesChanged();
        return true;
    }

    /* None of them is here. */
    bool changed = false;
    QString gonePlugin, goneDevice;
    if (patched)
    {
        gonePlugin = ip->pluginName();
        goneDevice = ip->inputUID().isEmpty() ? ip->inputName() : ip->inputUID();
        m_applyingSlot = true;
        setInputPatch(universe, KInputNone, QString(), QLCIOPlugin::invalidLine());
        m_applyingSlot = false;
        changed = true;
    }
    uni->setInputSlotLive(-1);

    /* Kept as its first device's patch, the way an absent device's input
       always is: shown greyed in Connections, and what older builds open. */
    bool kept = false;
    foreach (const Universe::OfflinePatch &rec, uni->offlinePatches())
        if (rec.tag == Universe::InputPatchTag)
            kept = true;
    if (kept == false)
    {
        foreach (const Universe::InputSlotDevice &d, devs)
        {
            if (d.anyGamepad)
                continue;
            Universe::OfflinePatch rec;
            rec.tag = Universe::InputPatchTag;
            rec.plugin = d.plugin;
            rec.uid = d.uid;
            rec.line = d.line;
            rec.profile = d.profile;
            rec.hidProfile = d.hidProfile;
            rec.params = d.params;
            uni->addOfflinePatch(rec);
            changed = true;
            break;
        }
    }

    if (goneDevice.isEmpty() == false)
        emit inputDeviceDisconnected(universe, gonePlugin, goneDevice);
    if (changed)
    {
        emit inputSlotChanged(universe);
        emit offlinePatchesChanged();
    }
    return changed;
}

void InputOutputMap::applyInputSlots()
{
    for (quint32 u = 0; u < universesCount(); ++u)
        if (m_universeArray.at(u)->hasInputSlot())
            applyInputSlot(u);
}

bool InputOutputMap::addInputSlotDevice(quint32 universe, const QString &pluginName,
                                        const QString &device)
{
    if (universe >= universesCount() || pluginName.isEmpty())
        return false;
    Universe *uni = m_universeArray.at(universe);
    QList<Universe::InputSlotDevice> devs = uni->inputSlot();
    const bool fresh = devs.isEmpty();

    if (fresh)
    {
        /* Made from the input it has: the live patch, or the one kept for
           a device that is away. */
        Universe::InputSlotDevice first;
        InputPatch *ip = uni->inputPatch();
        if (ip != nullptr && ip->isPatched() && ip->plugin() != nullptr)
        {
            first.plugin = ip->pluginName();
            first.uid = ip->inputUID().isEmpty() ? ip->inputName() : ip->inputUID();
            first.line = ip->input();
            first.profile = ip->profileName() == KInputNone ? QString() : ip->profileName();
            first.hidProfile = ip->hidProfileName();
            first.params = ip->getPluginParameters();
        }
        else
        {
            foreach (const Universe::OfflinePatch &rec, uni->offlinePatches())
            {
                if (rec.tag != Universe::InputPatchTag || rec.uid.isEmpty())
                    continue;
                first.plugin = rec.plugin;
                first.uid = rec.uid;
                first.line = rec.line;
                first.profile = rec.profile == KInputNone ? QString() : rec.profile;
                first.hidProfile = rec.hidProfile;
                first.params = rec.params;
                break;
            }
        }
        if (first.plugin.isEmpty())
            return false;
        devs << first;
    }

    Universe::InputSlotDevice dev;
    dev.plugin = pluginName;
    dev.uid = device;
    dev.anyGamepad = device.isEmpty();
    foreach (const Universe::InputSlotDevice &d, devs)
        if (sameDevice(d, dev))
            return false;

    if (dev.anyGamepad == false)
    {
        QLCIOPlugin *plugin = m_doc->ioPluginCache()->plugin(pluginName);
        const int line = plugin != nullptr ? plugin->inputs().indexOf(device) : -1;
        if (line >= 0)
        {
            dev.line = quint32(line);
            dev.profile = stockInputProfile(plugin->inputDeviceKind(quint32(line)));
        }
    }
    devs << dev;

    setInputSlot(universe, devs, fresh ? QStringList() : uni->inputSlotRoles());
    return true;
}

bool InputOutputMap::removeInputSlotDevice(quint32 universe, int index)
{
    if (universe >= universesCount())
        return false;
    Universe *uni = m_universeArray.at(universe);
    QList<Universe::InputSlotDevice> devs = uni->inputSlot();
    if (index < 0 || index >= devs.count())
        return false;

    const int live = uni->inputSlotLive();
    devs.removeAt(index);

    if (devs.count() == 1 && devs.first().anyGamepad == false)
    {
        /* One device left: an ordinary patch on it again, as if the show
           had been patched to it alone. */
        const Universe::InputSlotDevice d = devs.first();
        InputPatch *ip = uni->inputPatch();
        const bool itIsLive = ip != nullptr && ip->isPatched() && live >= 0 && live != index;
        setInputSlot(universe, QList<Universe::InputSlotDevice>());
        if (itIsLive == false)
        {
            setInputPatch(universe, KInputNone, QString(), QLCIOPlugin::invalidLine());
            Universe::OfflinePatch rec;
            rec.tag = Universe::InputPatchTag;
            rec.plugin = d.plugin;
            rec.uid = d.uid;
            rec.line = d.line;
            rec.profile = d.profile;
            rec.hidProfile = d.hidProfile;
            rec.params = d.params;
            loadPatch(universe, rec);
        }
        emit offlinePatchesChanged();
        return true;
    }
    if (devs.isEmpty())
    {
        setInputSlot(universe, devs);
        return true;
    }

    uni->setInputSlot(devs);
    uni->setInputSlotLive(live == index ? -1 : (live > index ? live - 1 : live));
    applyInputSlot(universe);
    emit inputSlotChanged(universe);
    return true;
}

bool InputOutputMap::moveInputSlotDevice(quint32 universe, int from, int to)
{
    if (universe >= universesCount())
        return false;
    Universe *uni = m_universeArray.at(universe);
    QList<Universe::InputSlotDevice> devs = uni->inputSlot();
    if (from < 0 || from >= devs.count() || to < 0 || to >= devs.count() || from == to)
        return false;

    const int live = uni->inputSlotLive();
    devs.move(from, to);
    int newLive = live;
    if (live == from)
        newLive = to;
    else if (live >= 0 && from < live && to >= live)
        newLive = live - 1;
    else if (live >= 0 && from > live && to <= live)
        newLive = live + 1;
    uni->setInputSlot(devs);
    uni->setInputSlotLive(newLive);
    applyInputSlot(universe);       // the one now higher may take over
    emit inputSlotChanged(universe);
    return true;
}

QList<InputOutputMap::InputSlotOffer> InputOutputMap::inputSlotOffers() const
{
    QList<InputSlotOffer> offers;

    /* Universes waiting: no live input, and a device input kept for them
       (alone, or a slot none of whose devices is here). */
    QList<QPair<quint32, QString> > waiting;    // universe, plugin
    QSet<QString> known;                        // plugin + device anyone names
    for (quint32 u = 0; u < universesCount(); ++u)
    {
        Universe *uni = m_universeArray.at(u);
        InputPatch *ip = uni->inputPatch();
        if (ip != nullptr && ip->isPatched() && ip->plugin() != nullptr)
            known.insert(ip->pluginName() + QLatin1Char('\n') + ip->inputUID());
        foreach (const Universe::InputSlotDevice &d, uni->inputSlot())
            if (d.anyGamepad == false)
                known.insert(d.plugin + QLatin1Char('\n') + d.uid);
        QString waitPlugin;
        foreach (const Universe::OfflinePatch &rec, uni->offlinePatches())
        {
            if (rec.tag != Universe::InputPatchTag || rec.uid.isEmpty())
                continue;
            known.insert(rec.plugin + QLatin1Char('\n') + rec.uid);
            if (waitPlugin.isEmpty())
                waitPlugin = rec.plugin;
        }
        const bool live = ip != nullptr && ip->isPatched();
        if (live == false && waitPlugin.isEmpty() == false)
            waiting << qMakePair(u, waitPlugin);
    }
    if (waiting.isEmpty())
        return offers;

    QSet<quint32> offered;
    foreach (QLCIOPlugin *plugin, m_doc->ioPluginCache()->plugins())
    {
        if (plugin == nullptr || plugin->linesAreHardware() == false)
            continue;
        const QStringList lines = plugin->inputs();
        for (int l = 0; l < lines.count(); ++l)
        {
            if (known.contains(plugin->name() + QLatin1Char('\n') + lines.at(l)))
                continue;
            if (isGamepadLine(plugin, quint32(l)) == false)
                continue;
            for (int w = 0; w < waiting.count(); ++w)
            {
                if (waiting.at(w).second != plugin->name() || offered.contains(waiting.at(w).first))
                    continue;
                InputSlotOffer o;
                o.universe = waiting.at(w).first;
                o.plugin = plugin->name();
                o.device = lines.at(l);
                offers << o;
                offered.insert(o.universe);
                break;
            }
        }
    }
    return offers;
}

void InputOutputMap::keepOrDropInputSlot(quint32 universe, const QString &pluginName,
                                         const QString &inputUID, quint32 input)
{
    Universe *uni = m_universeArray.at(universe);
    QLCIOPlugin *plugin = m_doc->ioPluginCache()->plugin(pluginName);
    QString device = inputUID;
    if (device.isEmpty() && plugin != nullptr)
        device = plugin->inputs().value(int(input));
    const QList<Universe::InputSlotDevice> devs = uni->inputSlot();
    if (plugin != nullptr && device.isEmpty() == false)
    {
        for (int k = 0; k < devs.count(); ++k)
        {
            if (devs.at(k).plugin == pluginName && devs.at(k).anyGamepad == false
                && devs.at(k).uid == device)
            {
                uni->setInputSlotLive(k);
                return;
            }
        }
    }
    qDebug() << "[IOMAP] universe" << universe + 1 << "patched by hand: controller slot dropped";
    uni->setInputSlot(QList<Universe::InputSlotDevice>());
    uni->setInputSlotLive(-1);
    uni->setInputSlotRoles(QStringList());
    emit inputSlotChanged(universe);
}
