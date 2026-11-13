/*
  Q Light Controller Plus - qlcconsole
  universepatcher.cpp

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

#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QWidget>
#include <QDebug>
#include <QSet>
#include <QMap>
#include <QHostAddress>
#include <QRegularExpression>
#include <QVariant>

#include "universepatcher.h"
#include "wleddevicesdialog.h"
#include "networkroute.h"
#include "qlcioplugin.h"
#include "ioplugincache.h"
#include "inputoutputmap.h"
#include "patchundo.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "universe.h"
#include "doc.h"

UniversePatcher::UniversePatcher(Doc *doc, QWidget *dialogParent)
    : QObject(dialogParent)
    , m_doc(doc)
    , m_parent(dialogParent)
{
    Q_ASSERT(doc != NULL);
}

/*********************************************************************
 * Words
 *********************************************************************/

QString UniversePatcher::portAddressText(quint32 pa)
{
    return QString("%1:%2:%3").arg((pa >> 8) & 0x7F).arg((pa >> 4) & 0x0F).arg(pa & 0x0F);
}

QString UniversePatcher::patchSelectionText(int count)
{
    return tr("Patch %1 Selected Universes to…").arg(count);
}

QString UniversePatcher::unpatchSelectionText(int patchedCount)
{
    return patchedCount == 1
        ? tr("Unpatch the 1 Patched Universe of the Selection")
        : tr("Unpatch %1 Selected Universes").arg(patchedCount);
}

QString UniversePatcher::retargetText(const QString &pluginName)
{
    return pluginName == QLatin1String("WLED") ? tr("WLED Devices / Backup Lanterns…")
                                               : tr("Change Interface / Target…");
}

bool UniversePatcher::supportsTargets(const QString &pluginName) const
{
    if (m_doc->ioPluginCache() == NULL)
        return false;
    QLCIOPlugin *p = m_doc->ioPluginCache()->plugin(pluginName);
    return p != NULL && p->supportsOutputTargets();
}

QString UniversePatcher::outputLegText(OutputPatch *op,
                                       QHash<QString, QList<TargetChoice> > *cache) const
{
    if (op == NULL)
        return QString();
    InputOutputMap *iomap = m_doc->inputOutputMap();
    QHash<QString, QList<TargetChoice> > local;
    QHash<QString, QList<TargetChoice> > &targetsByLine = cache != nullptr ? *cache : local;

    /* "Art-Net → CS1 (172.18.2.211) · port 1 · universe 0:0:0". "port" is
       the socket on the node, named only when the node has said (or was
       declared with) which one carries that universe; "universe" is the
       Art-Net port-address, as the device rows and the plugin's own
       "ArtNet Universe" have it. */
    const QString plugin = op->plugin() != NULL ? op->plugin()->name() : op->pluginName();
    const QMap<QString, QVariant> pp = op->getPluginParameters();
    const QString ip = pp.value(QStringLiteral("outputIP")).toString();
    QString where;
    if (!ip.isEmpty())
    {
        /* The device by the same name its row has in the device view -- a
           node that answered goes by the name it gave, not by its bare
           address, which made a listed node look missing from one view or
           the other. */
        const QString key = QString("%1|%2").arg(plugin).arg(op->output());
        if (op->plugin() != NULL && targetsByLine.contains(key) == false)
            targetsByLine.insert(key, visibleTargets(plugin, op->output()));
        const QString alias = iomap != NULL ? iomap->targetAlias(ip) : QString();
        where = alias.isEmpty() ? ip : QStringLiteral("%1 (%2)").arg(alias, ip);
        int number = 0;
        const quint32 pa = pp.value(QStringLiteral("outputUni")).toUInt();
        foreach (const TargetChoice &t, targetsByLine.value(key))
        {
            if (t.address != ip)
                continue;
            where = t.label;
            if (t.ports.contains(pa))
                number = t.numbers.value(t.ports.indexOf(pa));
        }
        if (pp.contains(QStringLiteral("outputUni")))
            where += QStringLiteral(" · ") + portWhereText(number, pa);
        /* "WLED → 172.18.2.50 + 1 backup": the lanterns mirroring it. */
        if (plugin == QLatin1String("WLED"))
        {
            const int backups = WledDevicesDialog::fromParameters(pp).count() - 1;
            if (backups > 0)
                where += backups == 1 ? tr(" + 1 backup") : tr(" + %1 backups").arg(backups);
        }

        /* And which interface it leaves by, when there is a choice: the
           device view files it under that interface, so a node fed from two
           NICs is listed under each. */
        if (op->isPending())
        {
            /* Kept offline: nothing here reaches its network (yet). */
            where += tr(" · no route (%1 not present) — kept, will reconnect")
                         .arg(op->outputName());
        }
        else if (op->plugin() != NULL && op->plugin()->outputs().count() > 1 && iomap != NULL)
        {
            const QString lname = op->plugin()->outputs().value(int(op->output()));
            const QString lalias = iomap->lineAlias(plugin, lname);
            where += tr(" · via %1").arg(lalias.isEmpty() ? NetworkRoute::describeLine(lname)
                                                          : lalias);
        }
    }
    else
    {
        where = op->outputName();
        if (op->isPending())
            where += tr(" — not present, kept, will reconnect");
    }
    return where.isEmpty() ? plugin : QStringLiteral("%1 → %2").arg(plugin, where);
}

QStringList UniversePatcher::outputText(quint32 universe) const
{
    QStringList legs;
    InputOutputMap *iomap = m_doc->inputOutputMap();
    Universe *u = iomap != NULL ? iomap->universe(universe) : NULL;
    if (u == NULL)
        return legs;
    QHash<QString, QList<TargetChoice> > cache;
    for (int i = 0; i < u->outputPatchesCount(); i++)
        if (OutputPatch *op = u->outputPatch(i))
            legs << outputLegText(op, &cache);
    return legs;
}

QList<UniversePatcher::UnpatchLeg> UniversePatcher::outputLegs(const QList<quint32> &universes) const
{
    QList<UnpatchLeg> legs;
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL)
        return legs;
    foreach (quint32 id, universes)
    {
        Universe *u = iomap->universe(id);
        if (u == NULL)
            continue;
        bool pending = false;
        for (int i = 0; i < u->outputPatchesCount(); i++)
        {
            OutputPatch *op = u->outputPatch(i);
            if (op == NULL)
                continue;
            if (op->isPending())
            {
                pending = true;
                continue;
            }
            if (op->plugin() == NULL)
                continue;
            UnpatchLeg leg;
            leg.universe = id;
            leg.pluginName = op->plugin()->name();
            leg.line = op->output();
            leg.output = true;
            legs << leg;
        }
        // Input/feedback (or plugin-less) patches kept offline count too.
        if (u->offlinePatches().isEmpty() == false)
            pending = true;
        if (pending)
        {
            UnpatchLeg leg;
            leg.universe = id;
            leg.pending = true;
            legs << leg;
        }
    }
    return legs;
}

/*********************************************************************
 * Shared with the Connections tab
 *********************************************************************/

bool UniversePatcher::pickPluginLine(QLCIOPlugin *p, bool output, quint32 &lineOut,
                                     int current)
{
    if (p == NULL)
        return false;

    const QStringList lines = output ? p->outputs() : p->inputs();
    if (lines.isEmpty())
        return false;

    if (lines.count() == 1)
    {
        lineOut = 0;
        return true;
    }

    /* Same "operator name wins over NIC name" labelling as every line row in
       the tree itself, so the picker reads exactly like what it is standing
       in for. */
    QStringList labels;
    for (int i = 0; i < lines.count(); i++)
    {
        const QString ifname = p->lineDescription(quint32(i), output);
        const QString alias = m_doc->inputOutputMap()
            ? m_doc->inputOutputMap()->lineAlias(p->name(), lines.at(i)) : QString();
        const QString shown = alias.isEmpty() ? ifname : alias;
        labels << (shown.isEmpty() ? lines.at(i)
                   : tr("%1 (%2)").arg(shown).arg(lines.at(i)));
    }

    bool ok = false;
    const QString picked = QInputDialog::getItem(
        m_parent, tr("Choose an interface"),
        tr("Which interface should this go out on?"), labels,
        current >= 0 && current < labels.count() ? current : 0, false, &ok);
    if (ok == false)
        return false;

    const int idx = labels.indexOf(picked);
    if (idx < 0)
        return false;
    lineOut = quint32(idx);
    return true;
}

bool UniversePatcher::patchTarget(quint32 universe, const QString &pluginName,
                                  quint32 line, QString &address,
                                  quint32 &portAddress) const
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    Universe *uni = iomap ? iomap->universe(universe) : NULL;
    if (uni == NULL)
        return false;

    for (int i = 0; i < uni->outputPatchesCount(); i++)
    {
        OutputPatch *op = uni->outputPatch(i);
        if (op == NULL || op->plugin() == NULL)
            continue;
        if (op->plugin()->name() != pluginName || op->output() != line)
            continue;

        /* Where a patch actually lands is protocol-specific and lives in the
           plugin parameters, not in the patch itself. For Art-Net that is the
           node IP plus a 15-bit port address encoded Net<<8 | Sub<<4 | Universe
           -- the same encoding ArtPollReply reports per port, which is what
           makes the two correlatable. A patch with neither is untargeted
           (broadcast, or simply never configured). */
        const QMap<QString, QVariant> params =
            const_cast<OutputPatch *>(op)->getPluginParameters();

        /* Absent parameters are not zero -- they are the plugin's defaults, and
           guessing wrong makes this view assert things that are false.
           ArtNetController::addUniverse defaults outputAddress to the BROADCAST
           address and outputUniverse to the universe's own id. Defaulting the
           port address to 0 instead made every patch that had an IP but no
           explicit universe collapse onto port 0, which showed up as several
           universes apparently bound to one physical port -- a configuration
           that would be a real fault if it were true. */
        if (params.contains("outputIP") == false)
            return false;   // broadcast: no single node to sit under
        address = params.value("outputIP").toString();
        portAddress = params.value("outputUni", universe).toUInt();
        return address.isEmpty() == false;
    }
    return false;
}


/** Write outputIP / outputUni onto one universe's patch on one line.
 *
 *  Shared by the create path (patchUniverseToPort) and the edit path
 *  (retargetPatch) so a patch made by one and changed by the other cannot end
 *  up describing itself differently.
 */
quint32 UniversePatcher::universeOnTarget(const QString &pluginName, quint32 line,
                                          const QString &address, quint32 portAddress,
                                          quint32 exceptUniverse) const
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL || address.isEmpty())
        return InputOutputMap::invalidUniverse();

    for (quint32 u = 0; u < iomap->universesCount(); u++)
    {
        if (u == exceptUniverse)
            continue;
        QString a;
        quint32 p = 0;
        if (patchTarget(u, pluginName, line, a, p) == false)
            continue;
        if (a == address && p == portAddress)
            return u;
    }
    return InputOutputMap::invalidUniverse();
}

bool UniversePatcher::applyTarget(quint32 universe, const QString &pluginName,
                                  quint32 line, const QString &address,
                                  quint32 portAddress)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    Universe *uni = iomap ? iomap->universe(universe) : NULL;
    if (uni == NULL)
        return false;

    for (int i = 0; i < uni->outputPatchesCount(); i++)
    {
        OutputPatch *op = uni->outputPatch(i);
        if (op == NULL || op->plugin() == NULL)
            continue;
        if (op->plugin()->name() != pluginName || op->output() != line)
            continue;

        /* One node port, one universe.
         *
           Nothing used to stop two universes being pointed at the same node
           and port. Both then write it every tick and the fixture shows
           whichever arrived last -- a patch that reads as correct everywhere on
           screen and is wrong on the floor, which is the hardest kind of fault
           to find in a dark room.

           Keyed on the DESTINATION, deliberately, not on the plugin line: a
           line here is a network interface, and universes sharing one NIC is
           how Art-Net normally works. Refusing per line would have made the
           second universe on a node unpatchable. */
        const quint32 owner = universeOnTarget(pluginName, line, address,
                                               portAddress, universe);
        if (owner != InputOutputMap::invalidUniverse())
        {
            qWarning() << "[Connections] refusing to target universe" << universe
                       << "at" << address << "port" << portAddress
                       << "-- universe" << owner << "already sends there";
            return false;
        }

        op->setPluginParameter("outputIP", address);
        op->setPluginParameter("outputUni", portAddress);
        return true;
    }
    return false;
}

/** Repoint an existing patch at a different node and port.
 *
 *  Until now the only way to change where a patch landed was to unpatch it and
 *  patch it again, because the address and port were only ever asked for while
 *  CREATING one. A node swapped for a spare, or renumbered on its front panel,
 *  is an ordinary evening -- it should not cost the patch and everything else
 *  hanging off it. Prefilled with the current values so the common edit is one
 *  character.
 */
void UniversePatcher::retargetPatch(quint32 universe, const QString &pluginName,
                                    quint32 line)
{
    /* A WLED universe is aimed at devices, not at a node's port: its own
       editor -- the primary, and any backup lanterns mirroring it. */
    if (pluginName == QLatin1String("WLED"))
    {
        if (WledDevicesDialog::edit(m_doc, universe, line, m_parent))
            emit patchChanged();
        return;
    }

    QString curAddr;
    quint32 curPort = 0;
    const bool targeted = patchTarget(universe, pluginName, line, curAddr, curPort);

    /* Captured before anything is asked, not after the dialogs are answered:
       an operator who changes their mind halfway leaves the held step
       describing a state that is still current, which is harmless, whereas
       capturing late can miss the change entirely. */
    InputOutputMap *undoMap = m_doc->inputOutputMap();
    if (undoMap != NULL && undoMap->patchUndo() != NULL)
        undoMap->patchUndo()->capture(QList<quint32>() << universe,
                                      tr("retarget universe %1").arg(universe + 1));

    /* "Re-patching a universe that's already patched must ask for the
       network interface again": it kept the old one, so a universe on the
       wrong NIC could only be moved by unpatching it and starting over.
       Asked first, the current one preselected -- and only when there is
       more than one to choose. */
    quint32 newLine = line;
    QLCIOPlugin *p = m_doc->ioPluginCache() != NULL
        ? m_doc->ioPluginCache()->plugin(pluginName) : NULL;
    if (p != NULL && pickPluginLine(p, true, newLine, int(line)) == false)
        return;

    QString addr = (targeted && newLine == line) ? curAddr : QString();
    quint32 wanted = curPort;
    if (askTargetPort(tr("Retarget patch"), pluginName, newLine, universe, true,
                      addr, wanted) == false)
        return;

    /* Both answered: now move it, keeping the patch's own settings. */
    if (newLine != line)
    {
        InputOutputMap *iomap = m_doc->inputOutputMap();
        Universe *uni = iomap ? iomap->universe(universe) : NULL;
        bool moved = false;
        for (int i = 0; uni != NULL && i < uni->outputPatchesCount(); i++)
        {
            OutputPatch *op = uni->outputPatch(i);
            if (op != NULL && op->plugin() != NULL
                    && op->plugin()->name() == pluginName && op->output() == line)
            {
                moved = iomap->setOutputPatch(universe, pluginName, QString(),
                                              newLine, false, i);
                break;
            }
        }
        if (moved == false)
        {
            QMessageBox::warning(m_parent, tr("Retarget patch"),
                                 tr("That universe is no longer patched to %1.")
                                     .arg(pluginName));
            return;
        }
        line = newLine;
    }

    /* An empty address means broadcast, which is a real and deliberate choice
       -- it is how you drive every node on a segment at once -- so clear the
       parameters rather than refusing. */
    if (addr.isEmpty())
    {
        InputOutputMap *iomap = m_doc->inputOutputMap();
        Universe *uni = iomap ? iomap->universe(universe) : NULL;
        if (uni != NULL)
        {
            for (int i = 0; i < uni->outputPatchesCount(); i++)
            {
                OutputPatch *op = uni->outputPatch(i);
                if (op == NULL || op->plugin() == NULL)
                    continue;
                if (op->plugin()->name() != pluginName || op->output() != line)
                    continue;
                op->unSetPluginParameter("outputIP");
                op->unSetPluginParameter("outputUni");
                break;
            }
        }
        m_doc->setModified();
        emit patchChanged();
        return;
    }

    /* askTargetPort() has already refused a port another universe holds, so
       failing here means the patch this was invoked on has gone. */
    if (applyTarget(universe, pluginName, line, addr, wanted) == false)
    {
        QMessageBox::warning(m_parent, tr("Retarget patch"),
                             tr("That universe is no longer patched to %1.")
                                 .arg(pluginName));
        return;
    }
    m_doc->setModified();
    emit patchChanged();
}

/*********************************************************************
 * Multi-universe patch / unpatch
 *********************************************************************/

bool UniversePatcher::parsePortAddress(const QString &txt, quint32 &pa)
{
    const QString t = txt.trimmed();
    /* A bare number is the port address itself: what a node's own panel
       and most consoles call "universe 15" is 0:0:15. */
    if (t.isEmpty() == false && t.contains(':') == false)
    {
        bool ok = false;
        const uint n = t.toUInt(&ok);
        if (ok == false || n > 0x7FFF)
            return false;
        pa = quint32(n);
        return true;
    }
    const QStringList parts = t.split(':');
    bool okNet = false, okSub = false, okUni = false;
    const uint net = parts.value(0).toUInt(&okNet);
    const uint sub = parts.value(1).toUInt(&okSub);
    const uint uni = parts.value(2).toUInt(&okUni);
    if (parts.count() != 3 || !okNet || !okSub || !okUni
            || net > 127 || sub > 15 || uni > 15)
        return false;
    pa = quint32((net << 8) | (sub << 4) | uni);
    return true;
}

QString UniversePatcher::portAddressError(const QString &txt)
{
    quint32 pa = 0;
    if (parsePortAddress(txt, pa))
        return QString();
    const QString t = txt.trimmed();
    bool ok = false;
    const uint n = t.toUInt(&ok);
    if (ok && n > 0x7FFF)
        return tr("%1 is past the last Art-Net universe: 32767 (127:15:15) is the highest.").arg(t);
    return tr("\"%1\" is not an Art-Net universe. Type a number from 0 to 32767, or "
              "net:subnet:universe with net 0-127 and subnet and universe 0-15.").arg(t);
}

/*********************************************************************
 * Which interface: from where it is sent
 *********************************************************************/

namespace
{
    QHostAddress v4(const QString &text)
    {
        const QHostAddress a(text.trimmed());
        if (a.isNull() || a.protocol() != QAbstractSocket::IPv4Protocol)
            return QHostAddress();
        return a;
    }

    /* The local address a line is named after, if this machine has it. */
    bool localOf(const QString &lineName, NetworkRoute::LocalAddress &out)
    {
        const QHostAddress ip = v4(lineName);
        if (ip.isNull())
            return false;
        foreach (const NetworkRoute::LocalAddress &la, NetworkRoute::localAddresses())
            if (la.ip == ip)
            {
                out = la;
                return true;
            }
        return false;
    }

    int prefixOf(const NetworkRoute::LocalAddress &la)
    {
        return la.prefix > 0 ? la.prefix : 24;  // as NetworkRoute assumes
    }

    quint32 broadcastOf(const NetworkRoute::LocalAddress &la)
    {
        const int p = prefixOf(la);
        const quint32 mask = p == 0 ? 0 : (0xFFFFFFFFu << (32 - p));
        return (la.ip.toIPv4Address() & mask) | ~mask;
    }
}

bool UniversePatcher::linesAreAddresses(const QLCIOPlugin *p)
{
    if (p == nullptr)
        return false;
    const QStringList lines = const_cast<QLCIOPlugin *>(p)->outputs();
    if (lines.isEmpty())
        return false;
    foreach (const QString &l, lines)
        if (NetworkRoute::isAddressIdentity(l) == false)
            return false;
    return true;
}

UniversePatcher::Destination UniversePatcher::readDestination(const QString &text)
{
    Destination d;
    const QString t = text.trimmed();
    if (t.isEmpty() || t.compare(tr("Broadcast"), Qt::CaseInsensitive) == 0
            || t.compare(QLatin1String("Broadcast"), Qt::CaseInsensitive) == 0)
        return d;

    const QHostAddress ip = v4(t);
    if (ip.isNull() == false && t.count('.') == 3)
    {
        if (ip == QHostAddress(QHostAddress::Broadcast))
            return d;
        /* A local network's broadcast address: broadcast, on that one. */
        foreach (const NetworkRoute::LocalAddress &la, NetworkRoute::localAddresses())
            if (ip.isLoopback() == false && ip.toIPv4Address() == broadcastOf(la))
            {
                d.network = NetworkRoute::networkOf(la.ip, prefixOf(la));
                return d;
            }
        d.kind = Destination::Unicast;
        d.address = ip.toString();
        return d;
    }

    /* Digits and dots still being typed: not wrong yet. */
    static const QRegularExpression partial(QStringLiteral("^\\d{1,3}(\\.\\d{0,3}){0,3}$"));
    if (partial.match(t).hasMatch())
    {
        bool fits = true;
        foreach (const QString &part, t.split('.'))
            if (part.isEmpty() == false && part.toUInt() > 255)
                fits = false;
        if (fits)
        {
            d.kind = Destination::Partial;
            return d;
        }
    }
    d.kind = Destination::Invalid;
    d.error = tr("\"%1\" is not an IP address. Type the node's address, like 172.18.2.50, "
                 "or pick it from the list.").arg(t);
    return d;
}

QString UniversePatcher::lineVia(const QString &lineName)
{
    NetworkRoute::LocalAddress la;
    if (localOf(lineName, la) == false || la.iface.isEmpty())
        return lineName;
    return tr("%1 · %2").arg(la.iface, la.ip.toString());
}

UniversePatcher::AutoLine UniversePatcher::autoLine(const QStringList &lines,
                                                    const Destination &destination,
                                                    int fallback)
{
    AutoLine out;
    if (lines.isEmpty())
        return out;
    if (fallback < 0 || fallback >= lines.count())
    {
        fallback = 0;
        for (int i = 0; i < lines.count(); i++)
            if (v4(lines.at(i)).isLoopback() == false)
            {
                fallback = i;
                break;
            }
    }
    auto finish = [&](int line) {
        out.line = line;
        out.via = lineVia(lines.at(line));
        return out;
    };

    if (destination.kind == Destination::Unicast)
    {
        const NetworkRoute::Resolution r =
            NetworkRoute::resolve(lines, QString(), QString(), destination.address);
        if (r.line >= 0)
        {
            NetworkRoute::LocalAddress la;
            const bool attached = localOf(lines.at(r.line), la)
                && QHostAddress(destination.address).isInSubnet(la.ip, prefixOf(la));
            out.why = attached
                ? tr("%1 is on its network (%2).").arg(destination.address, r.network)
                : tr("The system routes %1 through it.").arg(destination.address);
            return finish(r.line);
        }
        out.fallback = true;
        out.why = tr("Nothing on this computer reaches %1 (no network of its own, only the "
                     "default route), so it keeps this one. Connect the show network, or "
                     "choose the interface.").arg(destination.address);
        return finish(fallback);
    }
    if (destination.kind == Destination::Broadcast && destination.network.isEmpty() == false)
    {
        const QPair<QHostAddress, int> net = QHostAddress::parseSubnet(destination.network);
        for (int i = 0; i < lines.count(); i++)
        {
            const QHostAddress ip = v4(lines.at(i));
            if (ip.isNull() == false && ip.isInSubnet(net))
            {
                out.why = tr("It broadcasts on %1.").arg(destination.network);
                return finish(i);
            }
        }
    }
    if (destination.kind == Destination::Broadcast)
        out.why = tr("Broadcast leaves by one interface. To pick it by network, type that "
                     "network's broadcast address (like 172.18.2.255).");
    return finish(fallback);
}

QList<UniversePatcher::TargetChoice>
UniversePatcher::visibleTargets(const QString &pluginName, quint32 line) const
{
    QList<TargetChoice> choices;
    InputOutputMap *iomap = m_doc->inputOutputMap();
    QLCIOPlugin *p = m_doc->ioPluginCache() != NULL
        ? m_doc->ioPluginCache()->plugin(pluginName) : NULL;
    if (iomap == NULL || p == NULL)
        return choices;

    /* Built the way the Connections tab builds the device rows -- heard from first,
       then declared by hand, then only named by a patch -- so a choice reads
       like the tree it was picked from. */
    QSet<QString> seen;
    const QString self = p->outputs().value(int(line));
    foreach (const QLCIOPlugin::Device &dev, p->discoveredDevices())
    {
        if (dev.line != line || dev.address == self || seen.contains(dev.address))
            continue;
        seen.insert(dev.address);
        TargetChoice c;
        c.address = dev.address;
        c.heard = true;
        const QString alias = iomap->targetAlias(dev.address);
        const QString shown = alias.isEmpty()
            ? (dev.name.isEmpty() ? dev.address : dev.name) : alias;
        c.label = shown == dev.address ? shown : tr("%1 (%2)").arg(shown).arg(dev.address);
        /* Ports sharing a port address are one destination on the wire
           (see ConnectionsTree::refresh()), so they count once. */
        for (int i = 0; i < dev.portUniverses.count(); i++)
        {
            const quint32 pa = dev.portUniverses.at(i);
            if (c.ports.contains(pa))
                continue;
            c.ports << pa;
            c.numbers << dev.portNumbers.value(i, i + 1);
        }
        choices << c;
    }
    const int lineCount = p->outputs().count();
    foreach (const InputOutputMap::ManualTarget &mt, iomap->manualTargets())
    {
        const quint32 mline = mt.line < quint32(lineCount) ? mt.line : 0;
        if (mt.plugin != pluginName || mline != line || seen.contains(mt.address))
            continue;
        seen.insert(mt.address);
        TargetChoice c;
        c.address = mt.address;
        const QString alias = iomap->targetAlias(mt.address);
        c.label = alias.isEmpty() ? mt.address : tr("%1 (%2)").arg(alias).arg(mt.address);
        for (int i = 0; i < mt.ports.count(); i++)
        {
            if (c.ports.contains(mt.ports.at(i)))
                continue;
            c.ports << mt.ports.at(i);
            c.numbers << i + 1;
        }
        choices << c;
    }
    /* A node nothing has answered from, but that a patch on this interface
       already sends to: the tree lists it, so the picker does too, with the
       port addresses those patches use (its physical ports are unknown). */
    for (quint32 u = 0; u < iomap->universesCount(); u++)
    {
        QString a;
        quint32 pa = 0;
        if (patchTarget(u, pluginName, line, a, pa) == false || a.isEmpty())
            continue;
        int at = -1;
        for (int i = 0; i < choices.count(); i++)
            if (choices.at(i).address == a)
                at = i;
        if (at < 0)
        {
            if (seen.contains(a))
                continue;
            seen.insert(a);
            TargetChoice c;
            c.address = a;
            const QString alias = iomap->targetAlias(a);
            c.label = alias.isEmpty() ? a : tr("%1 (%2)").arg(alias).arg(a);
            choices << c;
            at = choices.count() - 1;
        }
        TargetChoice &c = choices[at];
        if (c.heard == false && c.ports.contains(pa) == false)
        {
            c.ports << pa;
            c.numbers << 0;
        }
    }
    return choices;
}

/* How a port reads everywhere: its physical number when the device has told
   us (or was declared with) one, then its Art-Net port-address as
   net:subnet:universe. */
QString UniversePatcher::portWhereText(int number, quint32 pa)
{
    return number > 0
        ? QObject::tr("port %1 · universe %2").arg(number).arg(portAddressText(pa))
        : QObject::tr("universe %1").arg(portAddressText(pa));
}

QList<UniversePatcher::TargetPortChoice>
UniversePatcher::targetPortChoices(const QString &pluginName, quint32 line,
                                   quint32 universe) const
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    QList<TargetPortChoice> freePorts, usedPorts;
    foreach (const TargetChoice &t, visibleTargets(pluginName, line))
    {
        for (int i = 0; i < t.ports.count(); i++)
        {
            TargetPortChoice c;
            c.address = t.address;
            c.port = t.ports.at(i);
            c.owner = universeOnTarget(pluginName, line, t.address, c.port, universe);
            const QString where = QString("%1 — %2").arg(t.label)
                                      .arg(portWhereText(t.numbers.value(i), c.port));
            if (c.owner == InputOutputMap::invalidUniverse())
            {
                c.text = tr("%1 — free").arg(where);
                freePorts << c;
            }
            else
            {
                Universe *o = iomap != NULL ? iomap->universe(c.owner) : NULL;
                c.text = tr("%1 — used by %2").arg(where)
                    .arg(o != NULL ? tr("%1: %2").arg(o->id() + 1).arg(o->name())
                                   : tr("universe %1").arg(c.owner + 1));
                usedPorts << c;
            }
        }
    }
    return freePorts + usedPorts;
}

bool UniversePatcher::askTargetPort(const QString &title, const QString &pluginName,
                                    quint32 line, quint32 universe, bool allowBroadcast,
                                    QString &address, quint32 &port)
{
    const QString curAddr = address;
    const quint32 curPort = port;
    bool ok = false;

    /* "should be able to select an unused Artnet->DMX or other interface if
       they're visible": every port of every device this interface lists,
       the free ones first. Ports another universe holds stay in the list,
       below, saying who -- picking one is refused, never half-done. */
    const QList<TargetPortChoice> ports = targetPortChoices(pluginName, line, universe);
    const QString other = tr("Another address…");
    const QString bcast = tr("Broadcast (no target)");
    bool typeIt = ports.isEmpty();
    if (typeIt == false)
    {
        QStringList labels;
        int current = 0;
        for (int i = 0; i < ports.count(); i++)
        {
            const TargetPortChoice &c = ports.at(i);
            const bool isCurrent = (c.address == curAddr && c.port == curPort);
            if (isCurrent)
                current = i;
            labels << (isCurrent ? tr("%1  (current)").arg(c.text) : c.text);
        }
        labels << other;
        if (allowBroadcast)
            labels << bcast;
        const QString pick = QInputDialog::getItem(m_parent, title,
            tr("Send it to which port?"), labels, current, false, &ok);
        if (ok == false)
            return false;
        const int idx = labels.indexOf(pick);
        if (pick == bcast)
        {
            address.clear();
            return true;
        }
        if (idx >= 0 && idx < ports.count())
        {
            const TargetPortChoice &c = ports.at(idx);
            if (c.owner != InputOutputMap::invalidUniverse())
            {
                QMessageBox::warning(m_parent, title,
                    tr("Universe %1 already sends to %2 %3.\n\nOne port carries "
                       "one universe. Move universe %1 first, or pick another port.")
                        .arg(c.owner + 1).arg(c.address).arg(portAddressText(c.port)));
                return false;
            }
            address = c.address;
            port = c.port;
            return true;
        }
        typeIt = true;
    }

    /* Typed by hand: a node not heard from yet, on the bench or off this
       subnet. Deliberately no reachability check. */
    const QString addr = QInputDialog::getText(m_parent, title,
        allowBroadcast ? tr("Target address (a node IP, or a broadcast address). "
                            "Leave empty to broadcast:")
                       : tr("Target address (a node IP, or a broadcast address):"),
        QLineEdit::Normal, curAddr, &ok).trimmed();
    if (ok == false)
        return false;
    if (addr.isEmpty())
    {
        if (allowBroadcast == false)
            return false;
        address.clear();
        return true;
    }
    const QString txt = QInputDialog::getText(m_parent, title,
        tr("Universe on %1, as net:subnet:universe:").arg(addr), QLineEdit::Normal,
        curAddr.isEmpty() ? QString("0:0:0") : portAddressText(curPort), &ok).trimmed();
    if (ok == false)
        return false;
    quint32 pa = 0;
    if (parsePortAddress(txt, pa) == false)
    {
        QMessageBox::warning(m_parent, title,
            tr("\"%1\" is not a port address. Expected net:subnet:universe, "
               "with net 0-127 and subnet and universe 0-15.").arg(txt));
        return false;
    }
    const quint32 owner = universeOnTarget(pluginName, line, addr, pa, universe);
    if (owner != InputOutputMap::invalidUniverse())
    {
        QMessageBox::warning(m_parent, title,
            tr("Universe %1 already sends to %2 %3.\n\nOne port carries "
               "one universe. Move universe %1 first, or pick another port.")
                .arg(owner + 1).arg(addr).arg(portAddressText(pa)));
        return false;
    }
    address = addr;
    port = pa;
    return true;
}

UniversePatcher::OrderedPatch UniversePatcher::planOrderedPatch(
        const QList<quint32> &universes, const QString &pluginName, quint32 line,
        const QString &address, const QList<quint32> &ports,
        const QStringList &portNames, bool linesAsPorts) const
{
    OrderedPatch plan;
    plan.pluginName = pluginName;
    plan.address = linesAsPorts ? QString() : address;

    InputOutputMap *iomap = m_doc->inputOutputMap();
    QLCIOPlugin *p = m_doc->ioPluginCache() != NULL
                     ? m_doc->ioPluginCache()->plugin(pluginName) : NULL;
    if (iomap == NULL || p == NULL)
    {
        plan.overflow = universes;
        return plan;
    }

    const bool targeted = (linesAsPorts == false && address.isEmpty() == false);
    int capacity = universes.count();               // broadcast: no ports to run out of
    if (linesAsPorts)
        capacity = qMax(0, int(p->outputs().count()) - int(line));
    else if (targeted)
        capacity = ports.count();

    /* Port k gets universe k. What does not fit is left exactly as it was
       -- not wrapped round onto port 1 again, which would put two universes
       on one port, and not dropped without a word. */
    for (int k = 0; k < universes.count(); k++)
    {
        if (k >= capacity)
        {
            plan.overflow << universes.at(k);
            continue;
        }
        OrderedPatchEntry e;
        e.universe = universes.at(k);
        e.line = linesAsPorts ? line + quint32(k) : line;
        e.targeted = targeted;
        e.port = targeted ? ports.at(k) : 0;
        if (k < portNames.count())
            e.portName = portNames.at(k);
        else if (targeted)
            e.portName = tr("universe %1").arg(portAddressText(e.port));
        else
        {
            const QString ln = p->lineDescription(e.line, true);
            e.portName = ln.isEmpty() ? p->outputs().value(int(e.line)) : ln;
        }
        plan.entries << e;
    }

    /* One node port, one universe -- the rule applyTarget() enforces for a
       single patch. A port held by a universe that is ITSELF in this run is
       not a conflict: it is moving off (re-ordering four universes across
       the same four ports is an ordinary thing to want). A port held by
       anything else is, and that universe is skipped rather than patched
       half-way. Repeated until stable, since skipping one can leave it
       holding a port the next one wanted. */
    if (targeted)
    {
        bool changed = true;
        while (changed)
        {
            changed = false;
            QSet<quint32> moving;
            foreach (const OrderedPatchEntry &e, plan.entries)
                moving.insert(e.universe);
            for (int i = 0; i < plan.entries.count() && changed == false; i++)
            {
                OrderedPatchEntry e = plan.entries.at(i);
                for (quint32 u = 0; u < iomap->universesCount(); u++)
                {
                    if (u == e.universe || moving.contains(u))
                        continue;
                    QString a;
                    quint32 pa = 0;
                    if (patchTarget(u, pluginName, e.line, a, pa) == false)
                        continue;
                    if (a == address && pa == e.port)
                    {
                        e.owner = u;
                        plan.conflicts << e;
                        plan.entries.removeAt(i);
                        changed = true;
                        break;
                    }
                }
            }
        }
    }
    return plan;
}

int UniversePatcher::applyOrderedPatch(const OrderedPatch &plan)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL || plan.entries.isEmpty())
        return 0;

    QList<quint32> unis;
    foreach (const OrderedPatchEntry &e, plan.entries)
        unis << e.universe;

    /* One step for the whole set, like the bulk retarget: a wrong device
       costs one press of Undo Patch Change, not N corrections. */
    if (iomap->patchUndo() != NULL)
        iomap->patchUndo()->capture(unis,
            unis.count() == 1
                ? tr("patch universe %1 to %2").arg(unis.first() + 1)
                      .arg(plan.address.isEmpty() ? plan.pluginName : plan.address)
                : tr("patch %1 universes to %2").arg(unis.count())
                      .arg(plan.address.isEmpty() ? plan.pluginName : plan.address));

    /* Two passes. First every universe goes onto its line with whatever
       target it had dropped, so one moving off a port frees it before the
       next one in the run is aimed there; then the targets. Replacing patch
       0 rather than appending: a universe sends to one line (see
       ConnectionsTree::patchUniverseTo()). */
    QList<OrderedPatchEntry> placed;
    foreach (const OrderedPatchEntry &e, plan.entries)
    {
        if (iomap->setOutputPatch(e.universe, plan.pluginName, QString(), e.line,
                                  false, 0) == false)
            continue;
        Universe *u = iomap->universe(e.universe);
        OutputPatch *op = (u != NULL && u->outputPatchesCount() > 0) ? u->outputPatch(0) : NULL;
        if (op != NULL)
        {
            op->unSetPluginParameter("outputIP");
            op->unSetPluginParameter("outputUni");
        }
        placed << e;
    }

    int applied = 0;
    foreach (const OrderedPatchEntry &e, placed)
    {
        if (e.targeted == false
                || applyTarget(e.universe, plan.pluginName, e.line, plan.address, e.port))
            applied++;
    }

    m_doc->setModified();
    emit patchChanged();
    return applied;
}

void UniversePatcher::patchSelectionTo(const QList<quint32> &universes)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL || m_doc->ioPluginCache() == NULL || universes.count() < 2)
        return;

    const QString title = tr("Patch %1 universes").arg(universes.count());
    auto uniName = [iomap](quint32 id) {
        Universe *u = iomap->universe(id);
        return u != NULL ? tr("%1: %2").arg(id + 1).arg(u->name())
                         : tr("universe %1").arg(id + 1);
    };

    /* Protocol, then interface: the same two questions "Patch to…" asks of a
       single unpatched universe. */
    QStringList pluginNames;
    foreach (QLCIOPlugin *cand, m_doc->ioPluginCache()->plugins())
        if (cand != NULL && (cand->capabilities() & QLCIOPlugin::Output)
                && cand->outputs().isEmpty() == false)
            pluginNames << cand->name();
    if (pluginNames.isEmpty())
        return;

    bool ok = false;
    QString pluginName = pluginNames.first();
    if (pluginNames.count() > 1)
    {
        pluginName = QInputDialog::getItem(m_parent, title, tr("Which protocol?"),
                                           pluginNames, 0, false, &ok);
        if (ok == false)
            return;
    }
    QLCIOPlugin *p = m_doc->ioPluginCache()->plugin(pluginName);
    if (p == NULL)
        return;

    /* Asked every time, the interface the selection is on now preselected:
       re-patching must be able to move them to another one. */
    int current = -1;
    foreach (quint32 u, universes)
    {
        Universe *uni = iomap->universe(u);
        for (int i = 0; current < 0 && uni != NULL && i < uni->outputPatchesCount(); i++)
        {
            OutputPatch *op = uni->outputPatch(i);
            if (op != NULL && op->plugin() == p)
                current = int(op->output());
        }
    }
    quint32 line = 0;
    if (pickPluginLine(p, true, line, current) == false)
        return;

    QString address;
    QString targetName;
    QList<quint32> ports;
    QStringList portNames;
    const bool linesAsPorts = (p->supportsOutputTargets() == false);

    if (linesAsPorts)
    {
        /* No node to name: each line IS a port (a multi-universe USB
           widget), so they follow on consecutive lines. */
        targetName = pluginName;
    }
    else
    {
        /* The devices known on this interface -- heard from, declared by
           hand, or only aimed at by a patch -- each with the ports it has,
           which is what "patch these to that 4-port node" needs to know. */
        struct Choice { QString address; QString label; QList<quint32> ports; QStringList names; };
        QList<Choice> choices;
        foreach (const TargetChoice &t, visibleTargets(pluginName, line))
        {
            Choice c;
            c.address = t.address;
            c.label = t.label;
            /* A node only a patch names has told us none of its ports: the
               ones in use are not the list to fill, so number up from a
               port address instead, as for an address typed in. */
            if (t.numbers.contains(0) == false)
            {
                c.ports = t.ports;
                for (int i = 0; i < t.ports.count(); i++)
                    c.names << portWhereText(t.numbers.value(i), t.ports.at(i));
            }
            choices << c;
        }

        QStringList labels;
        foreach (const Choice &c, choices)
            labels << (c.ports.isEmpty() ? c.label
                       : tr("%1 — %2 port(s)").arg(c.label).arg(c.ports.count()));
        const QString other = tr("Another address…");
        const QString bcast = tr("Broadcast (no target)");
        labels << other << bcast;

        const QString pick = QInputDialog::getItem(m_parent, title,
            tr("Send them to which device? They go onto its ports in the order "
               "you selected them."), labels, 0, false, &ok);
        if (ok == false)
            return;
        const int idx = labels.indexOf(pick);

        if (pick == bcast)
        {
            targetName = tr("%1, broadcast").arg(pluginName);
        }
        else
        {
            bool askFirstPort = true;
            if (idx >= 0 && idx < choices.count())
            {
                const Choice &c = choices.at(idx);
                address = c.address;
                targetName = c.label;
                if (c.ports.isEmpty() == false)
                {
                    askFirstPort = false;
                    int from = 0;
                    if (c.ports.count() > 1)
                    {
                        const QString first = QInputDialog::getItem(m_parent, title,
                            tr("Starting from which port of %1?").arg(c.label),
                            c.names, 0, false, &ok);
                        if (ok == false)
                            return;
                        from = qMax(0, c.names.indexOf(first));
                    }
                    ports = c.ports.mid(from);
                    portNames = c.names.mid(from);
                }
            }
            else
            {
                address = QInputDialog::getText(m_parent, title,
                    tr("Send them to which address?"), QLineEdit::Normal,
                    QString(), &ok).trimmed();
                if (ok == false || address.isEmpty())
                    return;
                targetName = address;
            }

            /* A device that has told us no ports -- typed in, or declared
               without any -- numbers up from a port address, as the bulk
               retarget does. */
            if (askFirstPort)
            {
                const QString txt = QInputDialog::getText(m_parent, title,
                    tr("Art-Net universe for the FIRST of them on %1: a number "
                       "(15) or net:subnet:universe (0:0:15).\nThe rest follow on "
                       "consecutive ports.")
                        .arg(targetName),
                    QLineEdit::Normal, QString("0:0:0"), &ok).trimmed();
                if (ok == false)
                    return;
                quint32 first = 0;
                if (parsePortAddress(txt, first) == false)
                {
                    QMessageBox::warning(m_parent, title, portAddressError(txt));
                    return;
                }
                for (int k = 0; k < universes.count() && first + quint32(k) <= 0x7FFF; k++)
                    ports << first + quint32(k);
            }
        }
    }

    const OrderedPatch plan = planOrderedPatch(universes, pluginName, line, address,
                                               ports, portNames, linesAsPorts);

    /* The order is the whole point and it is invisible in the selection, so
       show it -- and what will NOT happen -- before doing anything. */
    QStringList lines;
    foreach (const OrderedPatchEntry &e, plan.entries)
    {
        Universe *u = iomap->universe(e.universe);
        const bool replaces = (u != NULL && u->outputPatchesCount() > 0);
        lines << tr("  %1  →  %2%3").arg(uniName(e.universe)).arg(e.portName)
                     .arg(replaces ? tr("  (replaces its current patch)") : QString());
    }
    QString text = plan.entries.isEmpty()
        ? tr("Nothing can be patched to %1.").arg(targetName)
        : tr("Patch to %1, in the order you selected them:\n\n%2")
              .arg(targetName).arg(lines.join("\n"));
    if (plan.overflow.isEmpty() == false)
    {
        QStringList names;
        foreach (quint32 u, plan.overflow)
            names << uniName(u);
        text += tr("\n\nNot enough ports for %1 of them -- left as they are:\n  %2")
                    .arg(plan.overflow.count()).arg(names.join(", "));
    }
    if (plan.conflicts.isEmpty() == false)
    {
        QStringList names;
        foreach (const OrderedPatchEntry &e, plan.conflicts)
            names << tr("%1 (%2 is already used by %3)").arg(uniName(e.universe))
                         .arg(e.portName).arg(uniName(e.owner));
        text += tr("\n\nSkipped, one port carries one universe -- left as they are:\n  %1")
                    .arg(names.join("\n  "));
    }

    if (plan.entries.isEmpty())
    {
        QMessageBox::warning(m_parent, title, text);
        return;
    }
    if (QMessageBox::question(m_parent, title, text, QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::Yes) != QMessageBox::Yes)
        return;

    const int applied = applyOrderedPatch(plan);
    if (applied != plan.entries.count())
        QMessageBox::warning(m_parent, title,
            tr("%1 of %2 were patched; the rest could not be patched to %3.")
                .arg(applied).arg(plan.entries.count()).arg(pluginName));
}

void UniversePatcher::unpatchLegs(const QList<UnpatchLeg> &legs)
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    if (iomap == NULL || legs.isEmpty())
        return;

    QList<quint32> unis;
    foreach (const UnpatchLeg &l, legs)
        if (unis.contains(l.universe) == false)
            unis << l.universe;

    /* All of them at once: one undo step, one rebuild -- not N of each. */
    if (iomap->patchUndo() != NULL)
        iomap->patchUndo()->capture(unis, tr("unpatch %1 universes").arg(unis.count()));

    foreach (const UnpatchLeg &l, legs)
    {
        Universe *u = iomap->universe(l.universe);
        if (u == NULL)
            continue;
        if (l.pending)
        {
            // Every kept offline patch on it: pending outputs, and any
            // input/feedback/plugin-less patch kept for its network.
            iomap->forgetOfflinePatches(l.universe);
        }
        else if (l.output)
        {
            /* Looked up afresh: removing one leg of a fanned-out universe
               shifts the index of the next. */
            for (int i = 0; i < u->outputPatchesCount(); i++)
            {
                OutputPatch *op = u->outputPatch(i);
                if (op != NULL && op->plugin() != NULL
                        && op->plugin()->name() == l.pluginName && op->output() == l.line)
                {
                    iomap->setOutputPatch(l.universe, QString(), QString(), 0, false, i);
                    break;
                }
            }
        }
        else
        {
            iomap->setInputPatch(l.universe, QString(), QString(), 0);
        }
    }

    m_doc->setModified();
    emit patchChanged();
}
