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
#include <QRegularExpression>
#include <algorithm>
#include <climits>
#include <memory>

#include "naturalcompare.h"
#include "patchplanner.h"
#include "qlcioplugin.h"
#include "ioplugincache.h"
#include "monitorproperties.h"
#include "networktopology.h"
#include "networkroute.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "patchundo.h"
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
            // A WLED driven by its own API is sent to through the WLED
            // output, not Art-Net: it has its own map (below).
            if (fx->transport() != Fixture::WledTransport
                && !fx->ipAddress().isEmpty() && !uniForIp.contains(fx->ipAddress()))
                uniForIp.insert(fx->ipAddress(), uni);
        }
    }

    /* WLED by its own API: the universe already sent through the WLED output
       to that IP, and carrying nothing else -- never one that goes out as
       Art-Net or sACN, which the WLED would never hear. */
    QHash<QString, quint32> wledUniForIp;
    {
        InputOutputMap *io = m_doc->inputOutputMap();
        QHash<quint32, QSet<QString> > wledIpsOn;   // universe -> WLED IPs on it
        QSet<quint32> otherOn;                      // universes carrying anything else
        foreach (Fixture *fx, m_doc->fixtures())
        {
            if (fx == NULL || !fx->isPatched())
                continue;
            if (fx->connection() == Fixture::Network && fx->transport() == Fixture::WledTransport)
                wledIpsOn[fx->universe()].insert(fx->ipAddress());
            else
                otherOn.insert(fx->universe());
        }
        for (quint32 u = 0; u < io->universesCount(); u++)
        {
            OutputPatch *op = io->outputPatch(u);
            if (op == NULL || op->pluginName() != QLatin1String("WLED") || otherOn.contains(u))
                continue;
            const QString ip = op->getPluginParameters().value(QStringLiteral("outputIP")).toString();
            if (ip.isEmpty() || wledUniForIp.contains(ip))
                continue;
            const QSet<QString> on = wledIpsOn.value(u);
            if (on.isEmpty() || (on.size() == 1 && on.contains(ip)))
                wledUniForIp.insert(ip, u);
        }
    }
    auto ipMapFor = [&uniForIp, &wledUniForIp](const Fixture *f) -> QHash<QString, quint32> & {
        return f->transport() == Fixture::WledTransport ? wledUniForIp : uniForIp;
    };

    quint32 nextNew = m_doc->inputOutputMap()->universesCount();
    const quint32 existing = nextNew;

    // Patch in inventory order -- manufacturer, model, name -- rather than the
    // order they happened to be added, so identical fixtures land in contiguous
    // addresses and the plan reads the way the inventory tree does. Numbers in
    // names compare as numbers ("Pointe 2" before "Pointe 10").
    QList<Fixture *> ordered;
    foreach (quint32 fid, fixtureIds)
    {
        Fixture *f = m_doc->fixture(fid);
        if (f != NULL && !f->isPatched())
            ordered << f;
    }
    std::sort(ordered.begin(), ordered.end(), [](Fixture *x, Fixture *y) {
        int c = naturalCompare(x->groupingManufacturer(), y->groupingManufacturer());
        if (c == 0)
            c = naturalCompare(x->groupingModel(), y->groupingModel());
        if (c == 0)
            c = naturalCompare(x->name(), y->name());
        return c != 0 ? c < 0 : x->id() < y->id();
    });
    /* A universe of its own -- for a lantern's IP, a free node port, a DMX
       overflow -- is taken from the free ones first ("we can use a block
       that's free when it's free"): no fixture patched on it, nothing patched
       to its output, no node port carrying it. A run of free universes long
       enough for all of this plan's new IPs is preferred, so they land
       together; otherwise the free ones in order; then new ones after the
       last. */
    QList<quint32> freeUnis;
    {
        InputOutputMap *io = m_doc->inputOutputMap();
        QSet<quint32> portUnis;
        foreach (const NetworkNode &node, m_doc->networkTopology()->nodes())
            foreach (const NetworkPort &port, node.ports)
                if (port.universe != InputOutputMap::invalidUniverse())
                    portUnis.insert(port.universe);
        QList<quint32> all;
        for (quint32 u = 0; u < existing; ++u)
            if (!used.contains(u) && io->outputPatch(u) == NULL && !portUnis.contains(u))
                all << u;
        QSet<QString> newIps;
        int needed = 0;
        foreach (Fixture *f, ordered)
        {
            if (f->connection() != Fixture::Network)
                continue;
            const QString key = (f->transport() == Fixture::WledTransport ? QStringLiteral("wled:")
                                                                           : QString()) + f->ipAddress();
            if (f->ipAddress().isEmpty())
                ++needed;
            else if (!ipMapFor(f).contains(f->ipAddress()) && !newIps.contains(key))
            {
                newIps.insert(key);
                ++needed;
            }
        }
        // The first run of consecutive free universes long enough, if any.
        int runStart = -1;
        for (int i = 0; i < all.size() && runStart < 0 && needed > 0; ++i)
        {
            int j = i;
            while (j + 1 < all.size() && all.at(j + 1) == all.at(j) + 1)
                ++j;
            if (j - i + 1 >= needed)
                runStart = i;
            i = j;
        }
        if (runStart >= 0)
            freeUnis = all.mid(runStart) + all.mid(0, runStart);
        else
            freeUnis = all;
    }
    auto takeUniverse = [&freeUnis, &nextNew]() {
        return freeUnis.isEmpty() ? nextNew++ : freeUnis.takeFirst();
    };

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
    // Network fixtures last: each takes a universe of its own, and a free one
    // a DMX fixture would pack into is the DMX fixture's.
    QList<Fixture *> passOrder;
    foreach (Fixture *fx, ordered)
        if (placedAt.contains(fx->id()))
            passOrder << fx;
    foreach (Fixture *fx, ordered)
        if (!placedAt.contains(fx->id()) && fx->connection() != Fixture::Network)
            passOrder << fx;
    foreach (Fixture *fx, ordered)
        if (!placedAt.contains(fx->id()) && fx->connection() == Fixture::Network)
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
            QHash<QString, quint32> &forIp = ipMapFor(fx);
            if (!ip.isEmpty() && forIp.contains(ip))
            {
                const quint32 uni = forIp.value(ip);
                if (!used.contains(uni))
                    used[uni] = QBitArray(UNI_SIZE);
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
                const quint32 uni = takeUniverse();
                used[uni] = QBitArray(UNI_SIZE);
                mark(used[uni], 0, n);
                networkUnis.insert(uni);
                if (!ip.isEmpty())
                    forIp.insert(ip, uni);
                a.universe = uni;
                a.address = 0;
                a.newUniverse = (a.universe >= existing);
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
                    uni = takeUniverse();
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
                freeUnis.removeAll(uni);    // not free any more
                placed = true;
            }
            if (!placed)
            {
                const quint32 uni = takeUniverse();
                used[uni] = QBitArray(UNI_SIZE);
                mark(used[uni], 0, n);
                a.universe = uni;
                a.address = 0;
                a.newUniverse = (a.universe >= existing);
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

QList<quint32> PatchPlanner::ordered(Doc *doc, const QList<quint32> &fixtureIds, AddressOrder order)
{
    QList<Fixture *> fixtures;
    foreach (quint32 fid, fixtureIds)
        if (Fixture *fx = doc->fixture(fid))
            fixtures << fx;

    auto kindRank = [](Fixture *fx) -> int {
        if (fx->fixtureDef() == NULL)
            return 6;                                   // a generic dimmer
        switch (fx->fixtureDef()->type())
        {
            case QLCFixtureDef::MovingHead:   return 0;
            case QLCFixtureDef::Scanner:      return 1;
            case QLCFixtureDef::LEDBarPixels: return 2;
            case QLCFixtureDef::LEDBarBeams:  return 3;
            case QLCFixtureDef::ColorChanger: return 4;
            case QLCFixtureDef::Strobe:       return 5;
            case QLCFixtureDef::Dimmer:       return 6;
            case QLCFixtureDef::Laser:
            case QLCFixtureDef::Effect:
            case QLCFixtureDef::Flower:       return 7;
            case QLCFixtureDef::Fan:
            case QLCFixtureDef::Hazer:
            case QLCFixtureDef::Smoke:        return 8;
            default:                          return 9;
        }
    };
    // As they hang: each structure's place in name order, and each
    // fixture's place on it.
    QHash<quint32, QPair<int, int> > hung;
    if (order == AsTheyHang)
    {
        QStringList keys;
        foreach (Fixture *fx, fixtures)
        {
            const QString key = mountKey(doc, fx->id());
            if (!key.isEmpty() && !keys.contains(key))
                keys << key;
        }
        std::stable_sort(keys.begin(), keys.end(), [doc](const QString &a, const QString &b) {
            return naturalCompare(structureName(doc, a), structureName(doc, b)) < 0;
        });
        for (int k = 0; k < keys.size(); k++)
        {
            const QList<quint32> on = fixturesOn(doc, keys.at(k));
            for (int i = 0; i < on.size(); i++)
                hung.insert(on.at(i), qMakePair(k, i));
        }
    }

    std::stable_sort(fixtures.begin(), fixtures.end(), [&](Fixture *a, Fixture *b) {
        if (order == ByKind && kindRank(a) != kindRank(b))
            return kindRank(a) < kindRank(b);
        if (order == AsTheyHang)
        {
            const QPair<int, int> ha = hung.value(a->id(), qMakePair(INT_MAX, 0));
            const QPair<int, int> hb = hung.value(b->id(), qMakePair(INT_MAX, 0));
            if (ha != hb)
                return ha < hb;                 // unmounted ones after, by name
        }
        return naturalCompare(a->name(), b->name()) < 0;
    });
    QList<quint32> out;
    foreach (Fixture *fx, fixtures)
        out << fx->id();
    return out;
}

QList<PatchPlanner::Assignment> PatchPlanner::fit(const QList<quint32> &fixtureIds,
                                                  quint32 universe, quint32 address, bool carryOn,
                                                  const QHash<quint32, quint32> &channelOverride,
                                                  const QHash<quint32, QPair<quint32, quint32> > &elsewhere) const
{
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
    auto chans = [&channelOverride](Fixture *fx) {
        return channelOverride.value(fx->id(), fx->channels());
    };

    // What stays put, and where the rest of the plan is going.
    QHash<quint32, QBitArray> base;
    auto usedIn = [&base](quint32 uni) -> QBitArray & {
        if (!base.contains(uni))
            base[uni] = QBitArray(UNI_SIZE);
        return base[uni];
    };
    foreach (Fixture *fx, m_doc->fixtures())
        if (fx != NULL && fx->isPatched() && !moving.contains(fx->id()) && !elsewhere.contains(fx->id()))
            mark(usedIn(fx->universe()), fx->address(), fx->channels());
    QHashIterator<quint32, QPair<quint32, quint32> > ew(elsewhere);
    while (ew.hasNext())
    {
        ew.next();
        if (!moving.contains(ew.key()))
            mark(usedIn(ew.value().first >> 9), ew.value().first & 0x1FF, ew.value().second);
    }

    /* In the order given, one after another: each takes the first free run
       at or after the end of the one before it, around everything else. The
       same fixtures in the same order always land the same way ("still not
       patching the same all the time"), at addresses that read in order. */
    QHash<quint32, Assignment> planned;
    const quint32 existing = m_doc->inputOutputMap()->universesCount();
    quint32 uni = universe, from = address;
    bool spilled = false;
    foreach (Fixture *fx, fixtures)
    {
        const quint32 n = chans(fx);
        Assignment a;
        a.fixtureId = fx->id();
        int at = -1;
        if (n > 0 && n <= UNI_SIZE)
        {
            for (int guard = 0; guard < 4096; guard++)
            {
                QBitArray probe = usedIn(uni);
                for (quint32 i = 0; i < from && i < UNI_SIZE; i++)
                    probe.setBit(int(i));       // only at or after the cursor
                at = firstFree(probe, n);
                if (at >= 0 || !carryOn)
                    break;
                uni++;                          // full: on to the next one
                from = 0;
                spilled = true;
            }
        }
        if (at < 0)
        {
            // No room after the ones before it: flagged, left where it was.
            int freeCount = 0, widest = 0, run = 0;
            const QBitArray &room = usedIn(uni);
            for (int i = int(spilled ? 0 : address); i < UNI_SIZE; i++)
            {
                run = room.testBit(i) ? 0 : run + 1;
                freeCount += room.testBit(i) ? 0 : 1;
                widest = qMax(widest, run);
            }
            a.unfitted = true;
            a.universe = fx->isPatched() ? fx->universe() : 0;
            a.address = fx->isPatched() ? fx->address() : 0;
            a.note = QStringLiteral("no room in universe %1: needs %2 channels, the widest gap is %3 "
                                    "(%4 free)").arg(uni + 1).arg(n).arg(widest).arg(freeCount);
            planned.insert(fx->id(), a);
            continue;
        }
        a.universe = uni;
        a.address = quint32(at);
        mark(usedIn(uni), a.address, n);
        from = a.address + n;
        planned.insert(fx->id(), a);
    }

    QList<Assignment> out;
    foreach (Fixture *fx, fixtures)
    {
        Assignment a = planned.value(fx->id());
        a.newUniverse = (a.universe >= existing);
        out << a;
    }
    return out;
}

QHash<quint32, QLCFixtureMode *> PatchPlanner::modesToFit(const QList<quint32> &fixtureIds, quint32 universe,
                                                          quint32 address,
                                                          const QHash<quint32, QLCFixtureMode *> &current,
                                                          const QHash<quint32, quint32> &channelOverride,
                                                          const QHash<quint32, QPair<quint32, quint32> > &elsewhere,
                                                          bool *allFit) const
{
    QHash<quint32, QLCFixtureMode *> chosen = current;
    QHash<quint32, quint32> chans = channelOverride;
    QHash<quint32, QLCFixtureMode *> changed;
    for (int step = 0; step < 1000; step++)
    {
        int without = 0;
        foreach (const Assignment &a, fit(fixtureIds, universe, address, false, chans, elsewhere))
            without += a.unfitted ? 1 : 0;
        if (without == 0)
        {
            if (allFit != NULL) *allFit = true;
            return changed;
        }
        // The biggest that has a smaller mode it may take steps down one.
        Fixture *pick = NULL;
        QLCFixtureMode *next = NULL;
        quint32 pickN = 0;
        foreach (quint32 fid, fixtureIds)
        {
            Fixture *fx = m_doc->fixture(fid);
            if (fx == NULL || fx->fixtureDef() == NULL)
                continue;
            QLCFixtureMode *now = chosen.value(fid, fx->fixtureMode());
            const quint32 n = chans.value(fid, now != NULL ? quint32(now->channels().size()) : fx->channels());
            QLCFixtureMode *smaller = NULL;
            foreach (QLCFixtureMode *m, fx->fixtureDef()->modes())
            {
                const quint32 mn = quint32(m->channels().size());
                if (mn == 0 || mn >= n || (smaller != NULL && mn <= quint32(smaller->channels().size())))
                    continue;
                if (m != fx->fixtureMode() && !m_doc->canChangeFixtureMode(fid, m))
                    continue;
                smaller = m;                    // the largest mode smaller than now
            }
            if (smaller != NULL && n > pickN)
            {
                pick = fx;
                next = smaller;
                pickN = n;
            }
        }
        if (pick == NULL)
            break;                              // nothing left to shrink
        chosen.insert(pick->id(), next);
        chans.insert(pick->id(), quint32(next->channels().size()));
        if (next == pick->fixtureMode())
            changed.remove(pick->id());
        else
            changed.insert(pick->id(), next);
    }
    if (allFit != NULL) *allFit = false;
    return changed;
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
            {
                /* The fixture's IP is the primary. Backup lanterns on the
                   patch ("outputIP2"...) stay -- and one that was made the
                   primary by hand trades places back rather than being
                   sent to twice. */
                const QMap<QString, QVariant> pp = wop->getPluginParameters();
                const QString was = pp.value(QStringLiteral("outputIP")).toString();
                wop->setPluginParameter(QStringLiteral("outputIP"), fx->ipAddress());
                for (auto it = pp.constBegin(); it != pp.constEnd(); ++it)
                {
                    if (!it.key().startsWith(QLatin1String("outputIP"))
                        || it.key() == QLatin1String("outputIP")
                        || it.value().toString() != fx->ipAddress())
                        continue;
                    if (!was.isEmpty() && was != fx->ipAddress())
                        wop->setPluginParameter(it.key(), was);
                    else
                        wop->unSetPluginParameter(it.key());
                }
            }
            else if (problems != NULL)
                *problems << QStringLiteral("WLED fixture %1 has no IP address").arg(name);
            wop->setPluginParameter(QStringLiteral("wledAddress"), fx->address() + 1);
            wop->setPluginParameter(QStringLiteral("wledChannels"), fx->channels());
            continue;
        }

        // Point the universe's output at this fixture's IP.
        if (!aimUniverseAt(a.universe, fx->ipAddress()) && problems != NULL)
            *problems << QStringLiteral("Universe %1 carries network fixture %2 but has no ArtNet/sACN "
                                        "output; set one in Connections%3")
                             .arg(a.universe + 1).arg(name)
                             .arg(fx->ipAddress().isEmpty() ? QString()
                                                            : QStringLiteral(" (unicast to %1)").arg(fx->ipAddress()));
    }

    /* A universe patched onto that is still "Universe N" is named the way
       the show names its universes ("when we patch we should also name the
       universes in ways that .. follow the scheme of the rest"). */
    QSet<quint32> named;
    foreach (const Assignment &a, todo)
    {
        if (named.contains(a.universe))
            continue;
        named.insert(a.universe);
        const QString now = io->getUniverseNameByID(a.universe);
        if (!now.isEmpty() && now != QStringLiteral("Universe %1").arg(a.universe + 1))
            continue;                               // somebody named it
        const QString want = universeNameFor(m_doc, a.universe);
        if (!want.isEmpty())
            io->setUniverseName(int(a.universe), want);
    }
    return todo.size();
}

namespace
{
bool isWledByApi(const Fixture *fx)
{
    return fx != NULL && fx->connection() == Fixture::Network
           && fx->transport() == Fixture::WledTransport && !fx->ipAddress().isEmpty();
}

/* Everything patched on @p fx's universe is a WLED by its API at its IP:
   the universe is that WLED's own. */
bool ownsUniverse(Doc *doc, const Fixture *fx)
{
    foreach (Fixture *other, doc->fixtures())
    {
        if (other == NULL || !other->isPatched() || other->universe() != fx->universe())
            continue;
        if (!isWledByApi(other) || other->ipAddress() != fx->ipAddress())
            return false;
    }
    return true;
}
}

PatchPlanner::WledRoute PatchPlanner::wledRoute(Doc *doc, const Fixture *fx, QString *detail)
{
    if (doc == NULL || !isWledByApi(fx))
        return WledRouted;
    if (!fx->isPatched())
        return WledNotPatched;

    InputOutputMap *io = doc->inputOutputMap();
    const int count = io->outputPatchesCount(fx->universe());
    if (count == 0 || io->outputPatch(fx->universe()) == NULL)
        return WledNoOutput;

    OutputPatch *wled = NULL;
    QString otherPlugin, otherIp;
    for (int i = 0; i < count; i++)
    {
        OutputPatch *op = io->outputPatch(fx->universe(), i);
        if (op == NULL)
            continue;
        if (op->pluginName() != QLatin1String("WLED"))
        {
            if (otherPlugin.isEmpty())
                otherPlugin = op->pluginName();
            continue;
        }
        // Its IP as the primary or as a backup lantern ("outputIP2"...).
        const QMap<QString, QVariant> pp = op->getPluginParameters();
        for (auto it = pp.constBegin(); it != pp.constEnd() && wled == NULL; ++it)
            if (it.key().startsWith(QLatin1String("outputIP")) && it.value().toString() == fx->ipAddress())
                wled = op;
        if (wled == NULL && otherIp.isEmpty())
            otherIp = pp.value(QStringLiteral("outputIP")).toString();
    }
    if (wled == NULL)
    {
        if (!otherIp.isEmpty() || otherPlugin.isEmpty())
        {
            if (detail != NULL)
                *detail = otherIp;
            return WledOtherIp;
        }
        if (detail != NULL)
            *detail = otherPlugin;
        return WledOtherOutput;
    }

    /* Alone on it: the output reads its channels where they are now (a mode
       or address changed since patching moves them). Several on one IP share
       the universe, and the output's window is the patch's to say. */
    int onIt = 0;
    foreach (Fixture *other, doc->fixtures())
        if (other != NULL && other->isPatched() && other->universe() == fx->universe())
            ++onIt;
    if (onIt == 1)
    {
        const QMap<QString, QVariant> pp = wled->getPluginParameters();
        const QVariant addr = pp.value(QStringLiteral("wledAddress"));
        const QVariant chans = pp.value(QStringLiteral("wledChannels"));
        if ((addr.isValid() && addr.toUInt() != fx->address() + 1)
            || (chans.isValid() && chans.toUInt() != fx->channels()))
            return WledStale;
    }
    return WledRouted;
}

int PatchPlanner::routeWled(const QList<quint32> &fixtureIds, QStringList *problems,
                            std::function<void()> *undo)
{
    InputOutputMap *io = m_doc->inputOutputMap();

    QList<Fixture *> todo;
    foreach (quint32 fid, fixtureIds)
    {
        Fixture *fx = m_doc->fixture(fid);
        if (!isWledByApi(fx) || todo.contains(fx) || wledRoute(m_doc, fx) == WledRouted)
            continue;
        todo << fx;
    }
    if (todo.isEmpty())
        return 0;

    const quint32 universesBefore = io->universesCount();
    QHash<quint32, QPair<quint32, quint32> > was;   // patched ones: where they were
    QList<Assignment> inPlace;
    QList<quint32> toPlan;
    foreach (Fixture *fx, todo)
    {
        if (!fx->isPatched())
        {
            toPlan << fx->id();
            continue;
        }
        was.insert(fx->id(), qMakePair(fx->universe(), fx->address()));
        const WledRoute r = wledRoute(m_doc, fx);
        OutputPatch *op = io->outputPatch(fx->universe());
        if ((r == WledOtherIp || r == WledStale) && op != NULL
            && op->pluginName() == QLatin1String("WLED") && ownsUniverse(m_doc, fx))
        {
            // Its own universe already: only the output is re-aimed.
            Assignment a;
            a.fixtureId = fx->id();
            a.universe = fx->universe();
            a.address = fx->address();
            inPlace << a;
        }
        else
            toPlan << fx->id();
    }

    // Off where they are (inventory for a moment), so plan() places them.
    for (auto it = was.constBegin(); it != was.constEnd(); ++it)
        m_doc->unpatchFixture(it.key());

    const QList<Assignment> assignments = inPlace + plan(toPlan);

    // What the change touches, as it is now: the universes it lands on.
    QList<quint32> touched;
    QHash<quint32, QString> names;
    foreach (const Assignment &a, assignments)
    {
        if (a.universe < universesBefore && !touched.contains(a.universe))
        {
            touched << a.universe;
            names.insert(a.universe, io->getUniverseNameByID(a.universe));
        }
    }
    std::shared_ptr<PatchUndo> patchBefore = std::make_shared<PatchUndo>(io);
    patchBefore->capture(touched, QString());

    const int done = apply(assignments, problems);

    auto putBack = [doc = m_doc, io, was, universesBefore, names, patchBefore]() {
        foreach (quint32 fid, was.keys())
            if (doc->fixture(fid) != NULL && doc->fixture(fid)->isPatched())
                doc->unpatchFixture(fid);
        if (patchBefore->canUndo())
            patchBefore->undo();
        for (auto it = names.constBegin(); it != names.constEnd(); ++it)
            if (it.key() < io->universesCount())
                io->setUniverseName(int(it.key()), it.value());
        /* The universes it made, from the end, while nothing is on them and
           they go nowhere or to a WLED. */
        while (io->universesCount() > universesBefore)
        {
            const quint32 last = io->universesCount() - 1;
            bool carries = false;
            foreach (Fixture *fx, doc->fixtures())
                carries = carries || (fx != NULL && fx->isPatched() && fx->universe() == last);
            OutputPatch *op = io->outputPatch(last);
            if (carries || (op != NULL && op->pluginName() != QLatin1String("WLED"))
                || !io->removeUniverse(int(last)))
                break;
        }
        QList<Doc::FixturePatch> back;
        for (auto it = was.constBegin(); it != was.constEnd(); ++it)
            if (doc->fixture(it.key()) != NULL)
                back << Doc::FixturePatch{ it.key(), it.value().first, it.value().second };
        if (!back.isEmpty())
            doc->patchFixtures(back);
    };

    if (done == 0)
    {
        // Nothing changed (apply() is all or none): back where they were.
        QList<Doc::FixturePatch> back;
        for (auto it = was.constBegin(); it != was.constEnd(); ++it)
            back << Doc::FixturePatch{ it.key(), it.value().first, it.value().second };
        if (!back.isEmpty())
            m_doc->patchFixtures(back);
        return 0;
    }
    if (undo != NULL)
        *undo = putBack;
    return done;
}

/*********************************************************************
 * Art-Net / sACN network fixtures
 *********************************************************************/

namespace
{
/* A network fixture sent Art-Net or sACN DMX at its own IP. */
bool isNetByDmx(const Fixture *fx)
{
    return fx != NULL && fx->connection() == Fixture::Network
           && fx->transport() != Fixture::WledTransport && !fx->ipAddress().trimmed().isEmpty();
}

/* A broadcast target ("172.18.255.255", "255.255.255.255") as the network
   it covers ("172.18.0.0/16"; empty for every network). False when
   @p target is an ordinary address. */
bool broadcastNetwork(const QString &target, QString *net)
{
    const QHostAddress a(target);
    if (a.protocol() != QAbstractSocket::IPv4Protocol)
        return false;
    const quint32 v = a.toIPv4Address();
    if (v == 0xFFFFFFFFu)
    {
        net->clear();
        return true;
    }
    int octets = 0;
    while (octets < 3 && ((v >> (8 * octets)) & 0xFF) == 0xFF)
        ++octets;
    if (octets == 0)
        return false;
    *net = NetworkRoute::networkOf(a, 32 - 8 * octets);
    return true;
}

/* @p ip is on @p net ("a.b.c.d/p"); an empty (unknown) network counts, so
   a line this computer can't place is never called wrong. */
bool onNetwork(const QString &ip, const QString &net)
{
    if (net.isEmpty())
        return true;
    return QHostAddress(ip).isInSubnet(QHostAddress::parseSubnet(net));
}

/* The Art-Net Port-Address an output of @p universe goes out on. */
quint32 artNetPortAddress(const QMap<QString, QVariant> &params, quint32 universe)
{
    return params.value(QStringLiteral("outputUni"), universe).toUInt();
}

/* The output Port-Addresses of the Art-Net node(s) heard at @p ip, and its
   name; empty when nothing has answered from there. */
QList<quint32> nodePorts(Doc *doc, const QString &ip, QString *name = NULL)
{
    QList<quint32> ports;
    QLCIOPlugin *art = doc->ioPluginCache() != NULL
                     ? doc->ioPluginCache()->plugin(QStringLiteral("ArtNet")) : NULL;
    if (art == NULL)
        return ports;
    foreach (const QLCIOPlugin::Device &d, art->discoveredDevices())
    {
        if (d.address != ip)
            continue;
        if (name != NULL && name->isEmpty())
            *name = d.name;
        foreach (quint32 pa, d.portUniverses)
            if (!ports.contains(pa))
                ports << pa;
    }
    return ports;
}

/* The Port-Addresses other universes than @p universe unicast to @p ip. */
QSet<quint32> portsSentTo(Doc *doc, const QString &ip, quint32 universe)
{
    QSet<quint32> out;
    InputOutputMap *io = doc->inputOutputMap();
    for (quint32 u = 0; u < io->universesCount(); u++)
    {
        if (u == universe)
            continue;
        for (int i = 0; i < io->outputPatchesCount(u); i++)
        {
            OutputPatch *op = io->outputPatch(u, i);
            if (op == NULL || op->pluginName() != QLatin1String("ArtNet"))
                continue;
            const QMap<QString, QVariant> pp = op->getPluginParameters();
            if (pp.value(QStringLiteral("outputIP")).toString().trimmed() == ip)
                out.insert(artNetPortAddress(pp, u));
        }
    }
    return out;
}

/* Put back what a route change did: the moved fixtures where they were,
   the touched universes' patches and names, and the universes it made
   (from the end, while nothing is on them). */
std::function<void()> putBackRoute(Doc *doc, const QHash<quint32, QPair<quint32, quint32> > &was,
                                   const QList<quint32> &unpatched, quint32 universesBefore,
                                   const QHash<quint32, QString> &names,
                                   std::shared_ptr<PatchUndo> patchBefore)
{
    InputOutputMap *io = doc->inputOutputMap();
    return [doc, io, was, unpatched, universesBefore, names, patchBefore]() {
        foreach (quint32 fid, was.keys() + unpatched)
            if (doc->fixture(fid) != NULL && doc->fixture(fid)->isPatched())
                doc->unpatchFixture(fid);
        if (patchBefore->canUndo())
            patchBefore->undo();
        for (auto it = names.constBegin(); it != names.constEnd(); ++it)
            if (it.key() < io->universesCount())
                io->setUniverseName(int(it.key()), it.value());
        while (io->universesCount() > universesBefore)
        {
            const quint32 last = io->universesCount() - 1;
            bool carries = false;
            foreach (Fixture *fx, doc->fixtures())
                carries = carries || (fx != NULL && fx->isPatched() && fx->universe() == last);
            if (carries || !io->removeUniverse(int(last)))
                break;
        }
        QList<Doc::FixturePatch> back;
        for (auto it = was.constBegin(); it != was.constEnd(); ++it)
            if (doc->fixture(it.key()) != NULL)
                back << Doc::FixturePatch{ it.key(), it.value().first, it.value().second };
        if (!back.isEmpty())
            doc->patchFixtures(back);
    };
}
}

PatchPlanner::NetRoute PatchPlanner::outputReaches(const QString &plugin, const QString &lineName,
                                                   const QMap<QString, QVariant> &params,
                                                   const QString &ip, QString *detail)
{
    QString d;
    NetRoute r = NetRouted;
    if (plugin == QLatin1String("ArtNet"))
    {
        const QString target = params.value(QStringLiteral("outputIP")).toString().trimmed();
        QString net;
        if (target.isEmpty())
            net = NetworkRoute::networkForLine(lineName);   // broadcast on its line
        if (target.isEmpty() || broadcastNetwork(target, &net))
        {
            if (!onNetwork(ip, net))
            {
                r = NetOtherNetwork;
                d = net;
            }
        }
        else if (target != ip)
        {
            r = NetOtherIp;
            d = target;
        }
    }
    else if (plugin == QLatin1String("E1.31"))
    {
        if (params.value(QStringLiteral("multicast"), 1).toInt() != 0)
        {
            const QString net = NetworkRoute::networkForLine(lineName);
            if (!onNetwork(ip, net))
            {
                r = NetOtherNetwork;
                d = net;
            }
        }
        else
        {
            const QString target = params.value(QStringLiteral("ucastIP")).toString().trimmed();
            if (target != ip)
            {
                r = NetOtherIp;
                d = target;
            }
        }
    }
    else
    {
        r = NetOtherOutput;
        d = plugin;
    }
    if (detail != NULL)
        *detail = d;
    return r;
}

PatchPlanner::NetRoute PatchPlanner::networkRoute(Doc *doc, const Fixture *fx, QString *detail)
{
    if (doc == NULL || !isNetByDmx(fx))
        return NetRouted;
    if (!fx->isPatched())
        return NetNotPatched;

    return universeReaches(doc, fx->universe(), fx->ipAddress().trimmed(), detail);
}

PatchPlanner::NetRoute PatchPlanner::universeReaches(Doc *doc, quint32 universe, const QString &ip,
                                                     QString *detail)
{
    InputOutputMap *io = doc->inputOutputMap();
    NetRoute first = NetRouted;
    QString firstDetail, otherPlugin;
    for (int i = 0; universe < io->universesCount() && i < io->outputPatchesCount(universe); i++)
    {
        OutputPatch *op = io->outputPatch(universe, i);
        if (op == NULL)
            continue;
        QString d;
        const NetRoute r = outputReaches(op->pluginName(), op->outputName(),
                                         op->getPluginParameters(), ip, &d);
        if (r == NetRouted)
            return NetRouted;
        if (r == NetOtherOutput)
        {
            if (otherPlugin.isEmpty())
                otherPlugin = d;
            continue;
        }
        if (first == NetRouted)
        {
            first = r;
            firstDetail = d;
        }
    }
    if (first == NetRouted)
    {
        first = otherPlugin.isEmpty() ? NetNoOutput : NetOtherOutput;
        firstDetail = otherPlugin;
    }
    if (detail != NULL)
        *detail = firstDetail;
    return first;
}

bool PatchPlanner::aimUniverseAt(quint32 universe, const QString &ipIn, QString *problem)
{
    InputOutputMap *io = m_doc->inputOutputMap();
    const QString ip = ipIn.trimmed();
    OutputPatch *op = io->outputPatch(universe);
    if (op == NULL || !isNetworkPlugin(op->pluginName()))
    {
        // Borrow the plugin and interface of an existing network universe.
        bool done = false;
        for (quint32 u = 0; u < io->universesCount() && !done; u++)
        {
            OutputPatch *ref = io->outputPatch(u);
            if (u != universe && ref != NULL && isNetworkPlugin(ref->pluginName()))
                done = io->setOutputPatch(universe, ref->pluginName(), ref->outputName(), ref->output());
        }
        /* None in the show: the Art-Net (else sACN) interface on the
           fixture's network. */
        IOPluginCache *cache = m_doc->ioPluginCache();
        const char *const names[] = { "ArtNet", "E1.31" };
        for (int n = 0; n < 2 && !done && !ip.isEmpty() && cache != NULL; n++)
        {
            QLCIOPlugin *p = cache->plugin(QLatin1String(names[n]));
            const int line = p != NULL ? p->lineOnSameSubnet(ip) : -1;
            if (line >= 0 && line < p->outputs().size())
                done = io->setOutputPatch(universe, p->name(), p->outputs().at(line), quint32(line));
        }
        op = io->outputPatch(universe);
    }
    if (op == NULL || !isNetworkPlugin(op->pluginName()))
    {
        if (problem != NULL)
            *problem = QStringLiteral("Universe %1 has no Art-Net or sACN output to send it with")
                           .arg(universe + 1);
        return false;
    }
    if (ip.isEmpty())
        return true;

    if (op->pluginName() == QLatin1String("E1.31"))
    {
        /* sACN's unicast: E1.31 reads "multicast" and "ucastIP", never
           "outputIP" (which only a warning in the log ever saw). */
        op->setPluginParameter(QStringLiteral("multicast"), 0);
        op->setPluginParameter(QStringLiteral("ucastIP"), ip);
        if (op->getPluginParameters().contains(QStringLiteral("outputIP")))
            op->unSetPluginParameter(QStringLiteral("outputIP"));
        return true;
    }

    op->setPluginParameter(QStringLiteral("outputIP"), ip);
    /* A node heard at that IP listens on its own Port-Addresses: send on one
       of those, not on the universe's number. */
    const QList<quint32> ports = nodePorts(m_doc, ip);
    const quint32 sends = artNetPortAddress(op->getPluginParameters(), universe);
    if (!ports.isEmpty() && !ports.contains(sends))
    {
        const QSet<quint32> taken = portsSentTo(m_doc, ip, universe);
        foreach (quint32 pa, ports)
        {
            if (taken.contains(pa))
                continue;
            op->setPluginParameter(QStringLiteral("outputUni"), pa);
            break;
        }
    }
    return true;
}

int PatchPlanner::routeNetwork(const QList<quint32> &fixtureIds, QStringList *problems,
                               std::function<void()> *undo)
{
    InputOutputMap *io = m_doc->inputOutputMap();

    QList<Fixture *> todo;
    foreach (quint32 fid, fixtureIds)
    {
        Fixture *fx = m_doc->fixture(fid);
        if (!isNetByDmx(fx) || todo.contains(fx))
            continue;
        // Unpatched: just added, waiting at the address it was given.
        if (fx->isPatched() && networkRoute(m_doc, fx) == NetRouted)
            continue;
        todo << fx;
    }
    if (todo.isEmpty())
        return 0;

    const quint32 universesBefore = io->universesCount();
    QSet<quint32> moving;
    foreach (Fixture *fx, todo)
        moving.insert(fx->id());

    /* What stays where it is: channels taken, and what each universe carries. */
    QHash<quint32, QBitArray> used;
    QHash<quint32, QSet<QString> > ipsOn;           // network fixtures' IPs, per universe
    QSet<quint32> otherOn;                          // anything else on it
    QSet<quint32> anyOn;                            // anything at all, moving too
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == NULL || !fx->isPatched())
            continue;
        anyOn.insert(fx->universe());
        if (moving.contains(fx->id()))
            continue;
        if (!used.contains(fx->universe()))
            used[fx->universe()] = QBitArray(UNI_SIZE);
        mark(used[fx->universe()], fx->address(), fx->channels());
        if (isNetByDmx(fx))
            ipsOn[fx->universe()].insert(fx->ipAddress().trimmed());
        else
            otherOn.insert(fx->universe());
    }
    QSet<quint32> portUnis;
    foreach (const NetworkNode &node, m_doc->networkTopology()->nodes())
        foreach (const NetworkPort &port, node.ports)
            if (port.universe != InputOutputMap::invalidUniverse())
                portUnis.insert(port.universe);
    QList<quint32> freeUnis;
    for (quint32 u = 0; u < universesBefore; u++)
        if (!anyOn.contains(u) && io->outputPatchesCount(u) == 0 && !portUnis.contains(u))
            freeUnis << u;
    quint32 nextNew = universesBefore;

    auto fits = [&used](quint32 u, quint32 addr, quint32 n) {
        if (addr + n > UNI_SIZE)
            return false;
        if (!used.contains(u))
            return true;
        for (quint32 i = addr; i < addr + n; i++)
            if (used[u].testBit(int(i)))
                return false;
        return true;
    };
    auto unicastTo = [io](quint32 u, const QString &ip) {
        for (int i = 0; i < io->outputPatchesCount(u); i++)
        {
            OutputPatch *op = io->outputPatch(u, i);
            if (op == NULL || !isNetworkPlugin(op->pluginName()))
                continue;
            const QMap<QString, QVariant> pp = op->getPluginParameters();
            const QString t = op->pluginName() == QLatin1String("E1.31")
                ? (pp.value(QStringLiteral("multicast"), 1).toInt() == 0
                   ? pp.value(QStringLiteral("ucastIP")).toString().trimmed() : QString())
                : pp.value(QStringLiteral("outputIP")).toString().trimmed();
            if (t == ip)
                return true;
        }
        return false;
    };

    QHash<QString, quint32> forIp;                  // decided in this run
    QList<Assignment> assignments;
    foreach (Fixture *fx, todo)
    {
        const QString ip = fx->ipAddress().trimmed();
        const quint32 n = fx->channels();
        const quint32 addr = fx->address();
        quint32 target = InputOutputMap::invalidUniverse();

        /* The universe already sent to its IP, carrying only fixtures at it. */
        QList<quint32> sentThere;
        if (forIp.contains(ip))
            sentThere << forIp.value(ip);
        for (quint32 u = 0; u < universesBefore; u++)
            if (!sentThere.contains(u) && !otherOn.contains(u) && unicastTo(u, ip)
                && (ipsOn.value(u).isEmpty() || ipsOn.value(u) == QSet<QString>{ ip }))
                sentThere << u;
        foreach (quint32 u, sentThere)
        {
            if (fits(u, addr, n))
            {
                target = u;
                break;
            }
        }
        /* Its own universe, when everything else on it is at its IP: only
           the output is re-aimed. */
        if (target == InputOutputMap::invalidUniverse() && fx->isPatched()
            && !otherOn.contains(fx->universe()) && !forIp.values().contains(fx->universe())
            && (ipsOn.value(fx->universe()).isEmpty()
                || ipsOn.value(fx->universe()) == QSet<QString>{ ip })
            && fits(fx->universe(), addr, n))
            target = fx->universe();
        /* Else a universe of its own: a free one first, then a new one. */
        if (target == InputOutputMap::invalidUniverse())
            target = freeUnis.isEmpty() ? nextNew++ : freeUnis.takeFirst();

        if (!used.contains(target))
            used[target] = QBitArray(UNI_SIZE);
        mark(used[target], addr, n);
        ipsOn[target].insert(ip);
        forIp.insert(ip, target);

        Assignment a;
        a.fixtureId = fx->id();
        a.universe = target;
        a.address = addr;
        a.newUniverse = target >= universesBefore;
        assignments << a;
    }

    // What the change touches, as it is now.
    QList<quint32> touched;
    QHash<quint32, QString> names;
    foreach (const Assignment &a, assignments)
    {
        if (a.universe < universesBefore && !touched.contains(a.universe))
        {
            touched << a.universe;
            names.insert(a.universe, io->getUniverseNameByID(a.universe));
        }
    }
    std::shared_ptr<PatchUndo> patchBefore = std::make_shared<PatchUndo>(io);
    patchBefore->capture(touched, QString());

    // Off where they are, so apply() patches each and aims its universe.
    QHash<quint32, QPair<quint32, quint32> > was;
    QList<quint32> unpatched;
    foreach (Fixture *fx, todo)
    {
        if (fx->isPatched())
        {
            was.insert(fx->id(), qMakePair(fx->universe(), fx->address()));
            m_doc->unpatchFixture(fx->id());
        }
        else
            unpatched << fx->id();
    }

    const int done = apply(assignments, problems);
    if (done == 0)
    {
        QList<Doc::FixturePatch> back;
        for (auto it = was.constBegin(); it != was.constEnd(); ++it)
            back << Doc::FixturePatch{ it.key(), it.value().first, it.value().second };
        if (!back.isEmpty())
            m_doc->patchFixtures(back);
        return 0;
    }
    if (undo != NULL)
        *undo = putBackRoute(m_doc, was, unpatched, universesBefore, names, patchBefore);
    return done;
}

QString PatchPlanner::portAddressText(quint32 pa)
{
    return QStringLiteral("%1:%2:%3").arg((pa >> 8) & 0x7f).arg((pa >> 4) & 0x0f).arg(pa & 0x0f);
}

QList<PatchPlanner::PortMismatch> PatchPlanner::artNetPortMismatches(Doc *doc)
{
    QList<PortMismatch> out;
    if (doc == NULL || doc->ioPluginCache() == NULL
        || doc->ioPluginCache()->plugin(QStringLiteral("ArtNet")) == NULL)
        return out;
    InputOutputMap *io = doc->inputOutputMap();
    for (quint32 u = 0; u < io->universesCount(); u++)
    {
        for (int i = 0; i < io->outputPatchesCount(u); i++)
        {
            OutputPatch *op = io->outputPatch(u, i);
            if (op == NULL || op->pluginName() != QLatin1String("ArtNet"))
                continue;
            const QMap<QString, QVariant> pp = op->getPluginParameters();
            const QString ip = pp.value(QStringLiteral("outputIP")).toString().trimmed();
            QString net;
            if (ip.isEmpty() || broadcastNetwork(ip, &net))
                continue;           // broadcast: every node takes what it listens to
            PortMismatch m;
            m.listens = nodePorts(doc, ip, &m.nodeName);
            m.sends = artNetPortAddress(pp, u);
            if (m.listens.isEmpty() || m.listens.contains(m.sends))
                continue;
            m.universe = u;
            m.nodeIp = ip;
            const QSet<quint32> taken = portsSentTo(doc, ip, u);
            foreach (quint32 pa, m.listens)
            {
                if (!taken.contains(pa))
                {
                    m.suggest = int(pa);
                    break;
                }
            }
            out << m;
        }
    }
    return out;
}

namespace
{
/* "Lantern #1" -> "Lantern-1": the parts of a name, joined the way the
   scheme joins its parts. */
QString compactName(const QString &name)
{
    QString n = name;
    n.replace(QLatin1Char('#'), QLatin1Char(' '));
    const QStringList parts = n.split(QRegularExpression(QStringLiteral("[\\s_/]+")),
                                      Qt::SkipEmptyParts);
    return parts.join(QLatin1Char('-'));
}
}

QString PatchPlanner::universeNameFor(Doc *doc, quint32 universe)
{
    if (doc == NULL)
        return QString();

    // What is on it: one fixture's name, or what several names share.
    QStringList names;
    QString model;
    foreach (Fixture *fx, doc->fixtures())
    {
        if (fx == NULL || !fx->isPatched() || fx->universe() != universe)
            continue;
        names << compactName(fx->name());
        if (model.isEmpty() && fx->fixtureDef() != NULL)
            model = compactName(fx->fixtureDef()->model());
    }
    if (names.isEmpty())
        return QString();
    names.sort();
    QString label = names.first();
    if (names.size() > 1)
    {
        QString common = names.first();
        foreach (const QString &n, names)
        {
            int k = 0;
            while (k < common.size() && k < n.size() && common.at(k) == n.at(k))
                ++k;
            common.truncate(k);
        }
        // "Par-1", "Par-2" share "Par-": a whole word, not a stray digit.
        while (!common.isEmpty() && (common.endsWith(QLatin1Char('-')) || common.back().isDigit()))
            common.chop(1);
        label = common.isEmpty() ? model : common;
    }
    if (label.isEmpty())
        return QString();

    /* The show's scheme: names that start with a few letters, then the
       universe's own number, then a separator -- "U17-US1-1". Most of the
       named ones have to agree before it counts. */
    InputOutputMap *io = doc->inputOutputMap();
    static const QRegularExpression re(QStringLiteral("^([A-Za-z]{1,4})(\\d+)([-_ .])\\S"));
    QHash<QString, int> prefixes, separators;
    QHash<int, int> widths;
    int named = 0, matched = 0;
    for (quint32 u = 0; u < io->universesCount(); ++u)
    {
        if (u == universe)
            continue;
        const QString n = io->getUniverseNameByID(u);
        if (n.isEmpty() || n == QStringLiteral("Universe %1").arg(u + 1))
            continue;
        ++named;
        const QRegularExpressionMatch m = re.match(n);
        if (!m.hasMatch() || m.captured(2).toUInt() != u + 1)
            continue;
        ++matched;
        prefixes[m.captured(1)]++;
        separators[m.captured(3)]++;
        widths[m.captured(2).size()]++;
    }
    auto mostOf = [](const auto &counts) {
        auto best = counts.constBegin();
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
            if (it.value() > best.value())
                best = it;
        return best.key();
    };
    QString name = label;
    if (matched >= 2 && matched * 2 >= named)
    {
        const int width = mostOf(widths);
        name = QStringLiteral("%1%2%3%4").arg(mostOf(prefixes))
                   .arg(universe + 1, width, 10, QLatin1Char('0'))
                   .arg(mostOf(separators), label);
    }

    // Not one another universe already has.
    QSet<QString> taken;
    for (quint32 u = 0; u < io->universesCount(); ++u)
        if (u != universe)
            taken.insert(io->getUniverseNameByID(u));
    QString unique = name;
    for (int i = 2; taken.contains(unique); ++i)
        unique = QStringLiteral("%1-%2").arg(name).arg(i);
    return unique;
}

/****************************************************************************
 * Like structures, patched alike
 ****************************************************************************/

QString PatchPlanner::mountKey(Doc *doc, quint32 fixtureId)
{
    const FixtureRigProps rp = doc->monitorProperties()->fixtureRigProps(fixtureId);
    switch (rp.primaryMount())
    {
        case FixtureRigProps::TrussMount: return QStringLiteral("truss:%1").arg(rp.trussId);
        case FixtureRigProps::PipeMount:  return QStringLiteral("pipe:%1").arg(rp.pipeId);
        case FixtureRigProps::TowerMount: return QStringLiteral("tower:%1").arg(rp.towerId);
        case FixtureRigProps::DeckMount:  return QStringLiteral("platform:%1").arg(rp.deckPlatformId);
        case FixtureRigProps::RiserMount: return QStringLiteral("platform:%1").arg(rp.riserPlatformId);
        default: return QString();
    }
}

QString PatchPlanner::structureName(Doc *doc, const QString &key)
{
    MonitorProperties *props = doc->monitorProperties();
    const QString kind = key.section(':', 0, 0);
    const quint32 id = key.section(':', 1, 1).toUInt();
    QString name;
    if (kind == QLatin1String("truss") && props->truss(id) != nullptr)
        name = props->truss(id)->name();
    else if (kind == QLatin1String("pipe") && props->pipe(id) != nullptr)
        name = props->pipe(id)->name();
    else if (kind == QLatin1String("tower") && props->tower(id) != nullptr)
        name = props->tower(id)->name();
    else if (kind == QLatin1String("platform") && props->platform(id) != nullptr)
        name = props->platform(id)->name();
    return name.isEmpty() ? key : name;
}

/* A fixture's place on what it is mounted on, to put a structure's fixtures
   in order: the same place on two like structures is the same position in
   that order. */
static QList<double> placeOn(Doc *doc, quint32 fid)
{
    MonitorProperties *props = doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    switch (rp.primaryMount())
    {
        case FixtureRigProps::TrussMount:
            return { rp.trussOffset, rp.trussCross, rp.trussCrossY };
        case FixtureRigProps::PipeMount:
            return { rp.pipeOffset };
        case FixtureRigProps::TowerMount:
            return { double(rp.towerMountSide), double(rp.towerShelf), rp.towerU, rp.towerV };
        case FixtureRigProps::RiserMount:
            return { rp.riserU, rp.riserV };
        default:
        {
            const QVector3D p = props->fixtureRigPosition(fid);
            return { p.x(), p.y(), p.z() };
        }
    }
}

QList<quint32> PatchPlanner::fixturesOn(Doc *doc, const QString &key)
{
    QList<quint32> out;
    if (key.isEmpty())
        return out;
    foreach (Fixture *fx, doc->fixtures())
        if (fx != nullptr && mountKey(doc, fx->id()) == key)
            out << fx->id();
    std::sort(out.begin(), out.end(), [doc](quint32 a, quint32 b) {
        const QList<double> pa = placeOn(doc, a), pb = placeOn(doc, b);
        for (int i = 0; i < qMin(pa.size(), pb.size()); ++i)
            if (qAbs(pa.at(i) - pb.at(i)) > 0.005)
                return pa.at(i) < pb.at(i);
        return naturalCompare(doc->fixture(a)->name(), doc->fixture(b)->name()) < 0;
    });
    return out;
}

/* What a fixture is, for "the same fixtures": its model and mode. */
static QString fixtureKind(Fixture *fx)
{
    if (fx == nullptr)
        return QString();
    const QString mode = fx->fixtureMode() != nullptr ? fx->fixtureMode()->name() : QString();
    const QString def = fx->fixtureDef() != nullptr
        ? fx->fixtureDef()->manufacturer() + QLatin1Char('/') + fx->fixtureDef()->model()
        : QString();
    return def + QLatin1Char('/') + mode + QLatin1Char('/') + QString::number(fx->channels());
}

QStringList PatchPlanner::likeStructures(Doc *doc, const QString &key)
{
    QStringList out;
    const QList<quint32> mine = fixturesOn(doc, key);
    if (mine.isEmpty())
        return out;
    QStringList kinds;
    foreach (quint32 fid, mine)
        kinds << fixtureKind(doc->fixture(fid));

    const QString kind = key.section(':', 0, 0);
    QSet<QString> seen;
    foreach (Fixture *fx, doc->fixtures())
    {
        const QString other = fx != nullptr ? mountKey(doc, fx->id()) : QString();
        if (other.isEmpty() || other == key || seen.contains(other)
            || other.section(':', 0, 0) != kind)
            continue;
        seen.insert(other);
        QStringList theirs;
        foreach (quint32 fid, fixturesOn(doc, other))
            theirs << fixtureKind(doc->fixture(fid));
        if (theirs == kinds)
            out << other;
    }
    std::sort(out.begin(), out.end(), [doc](const QString &a, const QString &b) {
        return naturalCompare(structureName(doc, a), structureName(doc, b)) < 0;
    });
    return out;
}

QList<PatchPlanner::Assignment> PatchPlanner::matchLike(const QString &pattern,
                                                        const QStringList &targets,
                                                        QStringList *problems) const
{
    QList<Assignment> out;
    const QList<quint32> model = fixturesOn(m_doc, pattern);
    QList<Fixture *> pat;
    foreach (quint32 fid, model)
        pat << m_doc->fixture(fid);
    // The pattern's own patch, and where its universes start.
    int patBase = -1;
    foreach (Fixture *fx, pat)
        if (fx->isPatched())
        {
            patBase = int(fx->universe());
            break;
        }
    if (patBase < 0)
    {
        if (problems != NULL)
            *problems << QObject::tr("%1 has nothing patched to copy").arg(structureName(m_doc, pattern));
        return out;
    }

    foreach (const QString &target, targets)
    {
        if (target == pattern)
            continue;
        const QString name = structureName(m_doc, target);
        const QList<quint32> theirs = fixturesOn(m_doc, target);
        if (theirs.size() != pat.size() && problems != NULL)
            *problems << QObject::tr("%1 has %2 fixtures, %3 has %4: paired as far as they go")
                         .arg(name).arg(theirs.size())
                         .arg(structureName(m_doc, pattern)).arg(pat.size());

        // The target's universe: where its first patched fixture is, taken
        // back to the pattern's first universe.
        int base = -1;
        for (int i = 0; i < theirs.size() && i < pat.size() && base < 0; ++i)
        {
            Fixture *fx = m_doc->fixture(theirs.at(i));
            if (fx->isPatched() && pat.at(i)->isPatched())
                base = int(fx->universe()) - (int(pat.at(i)->universe()) - patBase);
        }
        if (base < 0)
        {
            if (problems != NULL)
                *problems << QObject::tr("%1 has nothing patched: no universe to keep").arg(name);
            continue;
        }

        for (int i = 0; i < theirs.size() && i < pat.size(); ++i)
        {
            Fixture *fx = m_doc->fixture(theirs.at(i));
            Fixture *like = pat.at(i);
            if (!like->isPatched())
                continue;
            Assignment a;
            a.fixtureId = fx->id();
            a.universe = quint32(base + (int(like->universe()) - patBase));
            a.address = like->address();
            if (fixtureKind(fx) != fixtureKind(like))
            {
                a.note = QObject::tr("not the same fixture as %1 on %2")
                             .arg(like->name(), structureName(m_doc, pattern));
                if (problems != NULL)
                    *problems << QStringLiteral("%1: %2").arg(fx->name(), a.note);
            }
            out << a;
        }
    }
    return out;
}
