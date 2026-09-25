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
#include <QCollator>
#include <QHash>
#include <QSet>
#include <algorithm>

#include "patchplanner.h"
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
    foreach (Fixture *fx, ordered)
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
            out << a;
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
        else
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
        out << a;
    }
    return out;
}

int PatchPlanner::apply(const QList<Assignment> &assignments, QStringList *problems)
{
    InputOutputMap *io = m_doc->inputOutputMap();
    int done = 0;

    foreach (const Assignment &a, assignments)
    {
        Fixture *fx = m_doc->fixture(a.fixtureId);
        if (fx == NULL)
            continue;
        const QString name = fx->name();

        // The mode was chosen for its footprint: apply it first, so the range
        // patched below is the one the plan was made for.
        if (a.mode != NULL && a.mode != fx->fixtureMode())
        {
            QString why;
            if (!m_doc->canChangeFixtureMode(a.fixtureId, a.mode, &why))
            {
                if (problems != NULL)
                    *problems << QStringLiteral("%1: mode not changed (%2)").arg(name, why);
                continue;
            }
            fx->setFixtureDefinition(fx->fixtureDef(), a.mode);
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
            QLCFixtureDef *def = fx->genericDimmerDef(int(a.dimmerChannels));
            fx->setFixtureDefinition(def, fx->genericDimmerMode(def, int(a.dimmerChannels)));
        }

        if (!m_doc->patchFixture(a.fixtureId, a.universe, a.address))
        {
            if (problems != NULL)
                *problems << QStringLiteral("%1: universe %2, address %3 is taken or does not fit")
                                 .arg(name).arg(a.universe + 1).arg(a.address + 1);
            continue;
        }
        done++;

        if (fx->connection() != Fixture::Network)
            continue;

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
    return done;
}
