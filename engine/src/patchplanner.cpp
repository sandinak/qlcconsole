/*
  Q Light Controller Plus
  patchplanner.cpp

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

#include <QBitArray>
#include <QPointF>
#include <cmath>
#include <QCollator>
#include <QHash>
#include <QSet>
#include <algorithm>

#include "patchplanner.h"
#include "qlcioplugin.h"
#include "ioplugincache.h"
#include "monitorproperties.h"
#include "networktopology.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "fixture.h"
#include "doc.h"

#define UNI_SIZE 512

namespace
{
    bool isNetworkPlugin(const QString &name)
    {
        return name == QLatin1String("ArtNet") || name == QLatin1String("E1.31");
    }

    /* First run of @p n (1..UNI_SIZE) free channels in @p used, or -1. */
    int firstFree(const QBitArray &used, quint32 n)
    {
        quint32 run = 0;
        for (int i = 0; i < UNI_SIZE; i++)
        {
            run = used.testBit(i) ? 0 : run + 1;
            if (run == n)
                return i - int(n) + 1;
        }
        return -1;
    }

    void mark(QBitArray &used, quint32 address, quint32 n)
    {
        for (quint32 i = address; i < address + n && i < UNI_SIZE; i++)
            used.setBit(int(i));
    }
}

PatchPlanner::PatchPlanner(Doc *doc)
    : m_doc(doc)
    , m_useProximity(true)
{
}

QList<PatchPlanner::Assignment> PatchPlanner::plan(const QList<quint32> &fixtureIds) const
{
    return plan(fixtureIds, QHash<quint32, quint32>());
}

QList<PatchPlanner::Assignment> PatchPlanner::plan(const QList<quint32> &fixtureIds,
                                                   const QHash<quint32, quint32> &channelOverride) const
{
    QList<Assignment> out;

    QHash<quint32, QBitArray> used;         // universe -> occupied channels
    QSet<quint32> networkUnis;              // universes that carry network fixtures
    QHash<QString, quint32> uniForIp;       // unicast IP -> its universe

    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == NULL || !fx->isPatched())
            continue;
        const quint32 uni = fx->universe();
        if (!used.contains(uni))
            used[uni] = QBitArray(UNI_SIZE);
        mark(used[uni], fx->address(), fx->channels());
        if (fx->connection() == Fixture::Network)
        {
            networkUnis.insert(uni);
            if (!fx->ipAddress().isEmpty() && !uniForIp.contains(fx->ipAddress()))
                uniForIp.insert(fx->ipAddress(), uni);
        }
    }

    quint32 nextNew = m_doc->inputOutputMap()->universesCount();
    const quint32 existing = nextNew;

    // Patch in inventory order -- manufacturer, model, name -- rather than the
    // order they happened to be added, so identical fixtures land in contiguous
    // addresses and the plan reads the way the inventory tree does. Numbers in
    // names compare as numbers ("Pointe 2" before "Pointe 10").
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    QList<Fixture *> ordered;
    foreach (quint32 fid, fixtureIds)
    {
        Fixture *f = m_doc->fixture(fid);
        if (f != NULL && !f->isPatched())
            ordered << f;
    }
    std::sort(ordered.begin(), ordered.end(), [&collator](Fixture *x, Fixture *y) {
        int c = collator.compare(x->groupingManufacturer(), y->groupingManufacturer());
        if (c == 0)
            c = collator.compare(x->groupingModel(), y->groupingModel());
        if (c == 0)
            c = collator.compare(x->name(), y->name());
        return c != 0 ? c < 0 : x->id() < y->id();
    });
    /* Proximity: where each placed fixture is, and where each node port's
       universe lands. A port claimed during this plan (a free port given a
       new universe) is remembered, so the next fixture near that node joins it
       instead of claiming another. */
    struct PortRef { int node; int port; double x; double y; };
    QList<PortRef> ports;
    QHash<QPair<int, int>, quint32> portUni;      // (node, port) -> universe, incl. planned
    QHash<quint32, QPointF> placedAt;             // fixture -> plan position, metres
    const NetworkTopology *nt = m_doc->networkTopology();
    MonitorProperties *props = m_doc->monitorProperties();
    if (m_useProximity && props != NULL)
    {
        for (int ni = 0; ni < nt->nodes().size(); ni++)
        {
            const NetworkNode &node = nt->nodes().at(ni);
            for (int pi = 0; pi < node.ports.size(); pi++)
            {
                double x = 0, y = 0;
                if (!NetworkTopology::proximityAnchor(node, node.ports.at(pi), x, y))
                    continue;
                ports << PortRef{ ni, pi, x, y };
                if (node.ports.at(pi).universe != InputOutputMap::invalidUniverse())
                    portUni.insert(qMakePair(ni, pi), node.ports.at(pi).universe);
            }
        }
        if (!ports.isEmpty())
        {
            foreach (Fixture *fx, ordered)
            {
                if (fx->connection() != Fixture::Dmx || !props->containsFixture(fx->id()))
                    continue;
                const QVector3D w = props->fixtureRigPosition(fx->id());
                placedAt.insert(fx->id(), QPointF(double(w.x()), double(w.y())));
            }
        }
    }
    auto loadOf = [&used](quint32 uni) {
        return used.contains(uni) ? int(used.value(uni).count(true)) : 0;
    };

    // Placed fixtures first (see the class comment), output in inventory order.
    QList<Fixture *> passOrder;
    foreach (Fixture *fx, ordered)
        if (placedAt.contains(fx->id()))
            passOrder << fx;
    foreach (Fixture *fx, ordered)
        if (!placedAt.contains(fx->id()))
            passOrder << fx;

    QHash<quint32, Assignment> planned;
    foreach (Fixture *fx, passOrder)
    {
        const quint32 fid = fx->id();
        const quint32 n = channelOverride.value(fid, fx->channels());
        Assignment a;
        a.fixtureId = fid;
        a.universe = 0;
        a.address = 0;
        a.newUniverse = false;

        if (n == 0 || n > UNI_SIZE)
        {
            a.note = QStringLiteral("%1 channels do not fit in a universe").arg(n);
            planned.insert(fid, a);
            continue;
        }

        bool placed = false;

        if (fx->connection() == Fixture::Network)
        {
            const QString ip = fx->ipAddress();
            if (!ip.isEmpty() && uniForIp.contains(ip))
            {
                const quint32 uni = uniForIp.value(ip);
                const int addr = firstFree(used[uni], n);
                if (addr >= 0)
                {
                    a.universe = uni;
                    a.address = quint32(addr);
                    a.newUniverse = (uni >= existing);
                    mark(used[uni], a.address, n);
                    placed = true;
                }
            }
            if (!placed)
            {
                const quint32 uni = nextNew++;
                used[uni] = QBitArray(UNI_SIZE);
                mark(used[uni], 0, n);
                networkUnis.insert(uni);
                if (!ip.isEmpty())
                    uniForIp.insert(ip, uni);
                a.universe = uni;
                a.address = 0;
                a.newUniverse = true;
                placed = true;
            }
            if (ip.isEmpty())
                a.note = QStringLiteral("network fixture has no IP set; its universe cannot be "
                                        "pointed at it until one is");
        }
        else if (placedAt.contains(fid))
        {
            // Nearest port first; equally near ports least-loaded first.
            const QPointF at = placedAt.value(fid);
            QList<PortRef> byDistance = ports;
            std::stable_sort(byDistance.begin(), byDistance.end(),
                             [&](const PortRef &p1, const PortRef &p2) {
                const double d1 = std::hypot(p1.x - at.x(), p1.y - at.y());
                const double d2 = std::hypot(p2.x - at.x(), p2.y - at.y());
                if (qAbs(d1 - d2) > 0.01)
                    return d1 < d2;
                const bool f1 = !portUni.contains(qMakePair(p1.node, p1.port));
                const bool f2 = !portUni.contains(qMakePair(p2.node, p2.port));
                if (f1 != f2)
                    return f2;                  // a port with a universe before a free one
                if (f1)
                    return false;
                return loadOf(portUni.value(qMakePair(p1.node, p1.port)))
                     < loadOf(portUni.value(qMakePair(p2.node, p2.port)));
            });
            foreach (const PortRef &pr, byDistance)
            {
                const QPair<int, int> key(pr.node, pr.port);
                quint32 uni;
                if (portUni.contains(key))
                {
                    uni = portUni.value(key);
                    if (networkUnis.contains(uni))
                        continue;               // one output, one destination
                    if (!used.contains(uni))
                        used[uni] = QBitArray(UNI_SIZE);
                    const int addr = firstFree(used[uni], n);
                    if (addr < 0)
                        continue;
                    a.address = quint32(addr);
                }
                else
                {
                    // A free port: it takes a universe of its own.
                    uni = nextNew++;
                    used[uni] = QBitArray(UNI_SIZE);
                    portUni.insert(key, uni);
                    a.claimsPort = true;
                    a.address = 0;
                }
                a.universe = uni;
                a.newUniverse = (uni >= existing);
                a.nodeIndex = pr.node;
                a.portIndex = pr.port;
                a.distanceMetres = std::hypot(pr.x - at.x(), pr.y - at.y());
                mark(used[uni], a.address, n);
                placed = true;
                break;
            }
            if (!placed)
                a.note = QStringLiteral("no node near it has room; placed first-fit");
        }
        if (!placed && fx->connection() != Fixture::Network)
        {
            for (quint32 uni = 0; uni < nextNew && !placed; uni++)
            {
                if (networkUnis.contains(uni))
                    continue;           // one output, one destination
                if (!used.contains(uni))
                    used[uni] = QBitArray(UNI_SIZE);
                const int addr = firstFree(used[uni], n);
                if (addr < 0)
                    continue;
                a.universe = uni;
                a.address = quint32(addr);
                a.newUniverse = (uni >= existing);
                mark(used[uni], a.address, n);
                placed = true;
            }
            if (!placed)
            {
                const quint32 uni = nextNew++;
                used[uni] = QBitArray(UNI_SIZE);
                mark(used[uni], 0, n);
                a.universe = uni;
                a.address = 0;
                a.newUniverse = true;
            }
        }
        planned.insert(fid, a);
    }
    foreach (Fixture *fx, ordered)
        out << planned.value(fx->id());
    return out;
}

QList<PatchPlanner::Assignment> PatchPlanner::place(const QList<quint32> &fixtureIds,
                                                    quint32 universe, quint32 address,
                                                    bool keepSpacing,
                                                    const QHash<quint32, quint32> &channelOverride,
                                                    const QHash<quint32, QPair<quint32, quint32> > &elsewhere) const
{
    QList<Assignment> out;
    QList<Fixture *> fixtures;
    QSet<quint32> moving;
    foreach (quint32 fid, fixtureIds)
    {
        Fixture *fx = m_doc->fixture(fid);
        if (fx != NULL && !moving.contains(fid))
        {
            fixtures << fx;
            moving.insert(fid);
        }
    }

    // Everything that is staying put, and where the rest of the plan goes.
    QHash<quint32, QBitArray> used;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == NULL || !fx->isPatched() || moving.contains(fx->id())
            || elsewhere.contains(fx->id()))
            continue;
        if (!used.contains(fx->universe()))
            used[fx->universe()] = QBitArray(UNI_SIZE);
        mark(used[fx->universe()], fx->address(), fx->channels());
    }
    QHashIterator<quint32, QPair<quint32, quint32> > ew(elsewhere);
    while (ew.hasNext())
    {
        ew.next();
        if (moving.contains(ew.key()))
            continue;
        const quint32 uni = ew.value().first >> 9;
        if (!used.contains(uni))
            used[uni] = QBitArray(UNI_SIZE);
        mark(used[uni], ew.value().first & 0x1FF, ew.value().second);
    }
    auto usedIn = [&used](quint32 uni) -> QBitArray & {
        if (!used.contains(uni))
            used[uni] = QBitArray(UNI_SIZE);
        return used[uni];
    };

    const quint32 existing = m_doc->inputOutputMap()->universesCount();
    auto chans = [&channelOverride](Fixture *fx) {
        return channelOverride.value(fx->id(), fx->channels());
    };
    QHash<quint32, Assignment> planned;

    // The block: patched fixtures keep their offsets from the lowest of them.
    quint32 nextAbs = (universe << 9) | address;   // where packing continues
    if (keepSpacing)
    {
        quint32 lowest = UINT_MAX;
        foreach (Fixture *fx, fixtures)
            if (fx->isPatched())
                lowest = qMin(lowest, fx->universeAddress());
        foreach (Fixture *fx, fixtures)
        {
            if (!fx->isPatched())
                continue;
            const quint32 n = chans(fx);
            const quint32 abs = ((universe << 9) | address) + (fx->universeAddress() - lowest);
            Assignment a;
            a.fixtureId = fx->id();
            a.universe = abs >> 9;
            a.address = abs & 0x1FF;
            a.newUniverse = (a.universe >= existing);
            if (a.address + n > UNI_SIZE)
                a.note = QStringLiteral("runs past the end of universe %1").arg(a.universe + 1);
            else
                mark(usedIn(a.universe), a.address, n);
            nextAbs = qMax(nextAbs, abs + n);
            planned.insert(fx->id(), a);
        }
    }

    // Everything else, packed from where the block (or the start) leaves off.
    quint32 uni = nextAbs >> 9;
    quint32 from = nextAbs & 0x1FF;
    foreach (Fixture *fx, fixtures)
    {
        if (planned.contains(fx->id()))
            continue;
        const quint32 n = chans(fx);
        Assignment a;
        a.fixtureId = fx->id();
        if (n == 0 || n > UNI_SIZE)
        {
            a.note = QStringLiteral("%1 channels do not fit in a universe").arg(n);
            planned.insert(fx->id(), a);
            continue;
        }
        for (int guard = 0; guard < 4096; guard++)
        {
            QBitArray probe = usedIn(uni);
            for (quint32 i = 0; i < from && i < UNI_SIZE; i++)
                probe.setBit(int(i));           // only at or after the cursor
            const int addr = firstFree(probe, n);
            if (addr >= 0)
            {
                a.universe = uni;
                a.address = quint32(addr);
                break;
            }
            uni++;                              // full: on to the next one
            from = 0;
        }
        a.newUniverse = (a.universe >= existing);
        mark(usedIn(a.universe), a.address, n);
        from = a.address + n;
        planned.insert(fx->id(), a);
    }

    foreach (Fixture *fx, fixtures)
        out << planned.value(fx->id());
    return out;
}

int PatchPlanner::apply(const QList<Assignment> &assignments, QStringList *problems)
{
    InputOutputMap *io = m_doc->inputOutputMap();

    /* Decide everything before changing anything: which mode changes are
       allowed, and whether the whole set of addresses fits. */
    QList<Assignment> todo;
    QList<Doc::FixturePatch> patches;
    QHash<quint32, quint32> footprint;
    foreach (const Assignment &a, assignments)
    {
        Fixture *fx = m_doc->fixture(a.fixtureId);
        if (fx == NULL)
            continue;
        const QString name = fx->name();
        quint32 n = fx->channels();
        if (a.mode != NULL && a.mode != fx->fixtureMode())
        {
            QString why;
            if (!m_doc->canChangeFixtureMode(a.fixtureId, a.mode, &why))
            {
                if (problems != NULL)
                    *problems << QStringLiteral("%1: mode not changed (%2)").arg(name, why);
                continue;
            }
            n = quint32(a.mode->channels().size());
        }
        else if (a.dimmerChannels > 0 && a.dimmerChannels != fx->channels())
        {
            QString why;
            if (!m_doc->canChangeFixtureMode(a.fixtureId, NULL, &why))
            {
                if (problems != NULL)
                    *problems << QStringLiteral("%1: channel count not changed (%2)").arg(name, why);
                continue;
            }
            n = a.dimmerChannels;
        }
        // Already there, same footprint: nothing to do.
        if (fx->isPatched() && n == fx->channels()
            && fx->universe() == a.universe && fx->address() == a.address)
            continue;
        todo << a;
        patches << Doc::FixturePatch{ a.fixtureId, a.universe, a.address };
        footprint.insert(a.fixtureId, n);
    }

    QString why;
    if (!m_doc->canPatchFixtures(patches, &why, footprint))
    {
        if (problems != NULL)
            *problems << QStringLiteral("Nothing was patched: %1").arg(why);
        return 0;
    }

    // Modes first, so the ranges patched below are the ones checked above.
    // Signals held: a patched fixture's new footprint must not be claimed at
    // its OLD address on the way through -- patchFixtures() claims it.
    foreach (const Assignment &a, todo)
    {
        Fixture *fx = m_doc->fixture(a.fixtureId);
        const bool held = fx->signalsBlocked();
        if (a.mode != NULL && a.mode != fx->fixtureMode())
        {
            fx->blockSignals(true);
            fx->setFixtureDefinition(fx->fixtureDef(), a.mode);
            fx->blockSignals(held);
        }
        else if (a.dimmerChannels > 0 && a.dimmerChannels != fx->channels())
        {
            QLCFixtureDef *def = fx->genericDimmerDef(int(a.dimmerChannels));
            fx->blockSignals(true);
            fx->setFixtureDefinition(def, fx->genericDimmerMode(def, int(a.dimmerChannels)));
            fx->blockSignals(held);
        }
    }

    if (!m_doc->patchFixtures(patches, &why))
    {
        if (problems != NULL)
            *problems << QStringLiteral("Nothing was patched: %1").arg(why);
        return 0;
    }

    foreach (const Assignment &a, todo)
    {
        Fixture *fx = m_doc->fixture(a.fixtureId);
        const QString name = fx->name();

        if (a.claimsPort)
        {
            NetworkTopology *nt = m_doc->networkTopology();
            if (a.nodeIndex >= 0 && a.nodeIndex < nt->nodes().size()
                && a.portIndex >= 0 && a.portIndex < nt->nodes().at(a.nodeIndex).ports.size())
            {
                NetworkPort &port = nt->nodes()[a.nodeIndex].ports[a.portIndex];
                if (port.universe == InputOutputMap::invalidUniverse())
                    port.universe = a.universe;
            }
        }

        if (fx->connection() != Fixture::Network)
            continue;

        /* WLED by its own API: the universe goes out through the WLED plugin,
           aimed at the lantern, reading its channels where they are. */
        if (fx->transport() == Fixture::WledTransport)
        {
            OutputPatch *wop = io->outputPatch(a.universe);
            QLCIOPlugin *wled = m_doc->ioPluginCache()->plugin(QStringLiteral("WLED"));
            if ((wop == NULL || wop->pluginName() != QLatin1String("WLED"))
                && wled != NULL && !wled->outputs().isEmpty())
            {
                io->setOutputPatch(a.universe, QStringLiteral("WLED"), wled->outputs().first(), 0);
                wop = io->outputPatch(a.universe);
            }
            if (wop == NULL || wop->pluginName() != QLatin1String("WLED"))
            {
                if (problems != NULL)
                    *problems << QStringLiteral("Universe %1 carries WLED fixture %2 but the WLED output "
                                                "is not available here").arg(a.universe + 1).arg(name);
                continue;
            }
            if (!fx->ipAddress().isEmpty())
                wop->setPluginParameter(QStringLiteral("outputIP"), fx->ipAddress());
            else if (problems != NULL)
                *problems << QStringLiteral("WLED fixture %1 has no IP address").arg(name);
            wop->setPluginParameter(QStringLiteral("wledAddress"), fx->address() + 1);
            wop->setPluginParameter(QStringLiteral("wledChannels"), fx->channels());
            continue;
        }

        // Point the universe's output at this fixture's IP.
        OutputPatch *op = io->outputPatch(a.universe);
        if (op == NULL || !isNetworkPlugin(op->pluginName()))
        {
            // Borrow the plugin and interface of an existing network universe.
            for (quint32 u = 0; u < io->universesCount(); u++)
            {
                OutputPatch *ref = io->outputPatch(u);
                if (u != a.universe && ref != NULL && isNetworkPlugin(ref->pluginName()))
                {
                    io->setOutputPatch(a.universe, ref->pluginName(), ref->outputName(), ref->output());
                    break;
                }
            }
            op = io->outputPatch(a.universe);
        }
        if (op == NULL || !isNetworkPlugin(op->pluginName()))
        {
            if (problems != NULL)
                *problems << QStringLiteral("Universe %1 carries network fixture %2 but has no ArtNet/sACN "
                                            "output; set one in Connections%3")
                                 .arg(a.universe + 1).arg(name)
                                 .arg(fx->ipAddress().isEmpty() ? QString()
                                                                : QStringLiteral(" (unicast to %1)").arg(fx->ipAddress()));
        }
        else if (!fx->ipAddress().isEmpty())
        {
            op->setPluginParameter(QStringLiteral("outputIP"), fx->ipAddress());
        }
    }
    return todo.size();
}
