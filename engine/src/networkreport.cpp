/*
  Q Light Controller Plus
  networkreport.cpp

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

#include <algorithm>
#include <QHash>
#include <QSet>

#include "csvfield.h"
#include "networkreport.h"
#include "networktopology.h"
#include "monitorproperties.h"
#include "inputoutputmap.h"
#include "fixture.h"
#include "doc.h"

#define FEET_PER_METRE 3.28084

NetworkReport NetworkReport::build(Doc *doc)
{
    NetworkReport r;
    if (doc == NULL)
        return r;

    const NetworkTopology *nt = doc->networkTopology();

    // Where the plotted fixtures are, the same positions the Network overlay
    // measures its runs with.
    MonitorProperties *props = doc->monitorProperties();
    QHash<quint32, QVector3D> positions;
    if (props != NULL)
    {
        foreach (quint32 fid, props->fixtureItemsID())
            positions.insert(fid, props->fixtureRigPosition(fid));
    }

    // runs() has one entry per port that carries a universe.
    QHash<QPair<int, int>, NetworkRun> runs;
    foreach (const NetworkRun &run, nt->runs(doc, positions))
        runs.insert(qMakePair(run.nodeIndex, run.portIndex), run);

    for (int n = 0; n < nt->nodes().size(); n++)
    {
        const NetworkNode &nn = nt->nodes().at(n);
        Node node;
        node.name = nn.name;
        node.transport = NetworkNode::transportName(nn.transport);
        node.model = nn.model;
        node.ip = nn.ipHint;
        node.placed = nn.placed;
        node.ports = nn.ports.size();
        for (int p = 0; p < nn.ports.size(); p++)
        {
            Port port;
            port.nodeIndex = n;
            port.portIndex = p;
            const QPair<int, int> key(n, p);
            if (runs.contains(key))
            {
                const NetworkRun &run = runs.value(key);
                port.used = true;
                port.universe = run.universe;
                port.receiver = run.receiverName;
                port.fixtures = run.fixtures.size();
                port.channels = run.channels;
                port.anchored = run.anchored;
                port.lengthMetres = run.lengthMetres;
                port.unplotted = run.fixtures.size() - run.plotted.size();
                node.portsUsed++;
                node.universes << run.universe;
                node.fixtures += port.fixtures;
                node.channels += port.channels;
            }
            r.ports << port;
        }
        r.nodes << node;
    }

    r.unserved = nt->unservedFixtures(doc);
    foreach (quint32 fid, r.unserved)
        if (Fixture *fx = doc->fixture(fid))
            r.unservedNames << fx->name();

    foreach (Fixture *fx, doc->fixtures())
    {
        if (fx == NULL || fx->connection() != Fixture::Network)
            continue;
        NetworkFixture nf;
        nf.id = fx->id();
        nf.name = fx->name();
        nf.ip = fx->ipAddress();
        nf.patched = fx->isPatched();
        nf.universe = fx->universe();
        r.networkFixtures << nf;
    }
    std::sort(r.networkFixtures.begin(), r.networkFixtures.end(),
              [](const NetworkFixture &a, const NetworkFixture &b) {
                  return a.ip != b.ip ? a.ip < b.ip : a.name < b.name;
              });

    r.warnings = nt->warnings(doc);
    return r;
}

int NetworkReport::portsUsed() const
{
    int n = 0;
    foreach (const Port &p, ports)
        if (p.used)
            n++;
    return n;
}

int NetworkReport::universesCarried() const
{
    QSet<quint32> unis;
    foreach (const Port &p, ports)
        if (p.used)
            unis.insert(p.universe);
    return unis.size();
}

int NetworkReport::longestRun() const
{
    int best = -1;
    for (int i = 0; i < ports.size(); i++)
        if (ports.at(i).hasRun() && (best < 0 || ports.at(i).lengthMetres > ports.at(best).lengthMetres))
            best = i;
    return best;
}

static QString lengthText(double metres, bool feet)
{
    return feet ? QStringLiteral("%1 ft").arg(metres * FEET_PER_METRE, 0, 'f', 1)
                : QStringLiteral("%1 m").arg(metres, 0, 'f', 1);
}

QString NetworkReport::toText(bool feet) const
{
    QStringList out;
    out << QStringLiteral("NETWORK SUMMARY");
    out << QStringLiteral("Nodes: %1   Ports: %2 (%3 used, %4 free)   Universes carried: %5")
               .arg(nodes.size()).arg(portsTotal()).arg(portsUsed()).arg(portsFree())
               .arg(universesCarried());
    const int longest = longestRun();
    if (longest >= 0)
    {
        const Port &p = ports.at(longest);
        out << QStringLiteral("Longest run: %1, port %2 (universe %3), ~%4")
                   .arg(nodes.at(p.nodeIndex).name).arg(p.portIndex + 1).arg(p.universe + 1)
                   .arg(lengthText(p.lengthMetres, feet));
    }

    for (int n = 0; n < nodes.size(); n++)
    {
        const Node &node = nodes.at(n);
        out << QString();
        QString head = QStringLiteral("%1 -- %2").arg(node.name, node.transport);
        if (!node.model.isEmpty())
            head += QStringLiteral(" (%1)").arg(node.model);
        if (!node.ip.isEmpty())
            head += QStringLiteral(" @ %1").arg(node.ip);
        if (!node.placed)
            head += QStringLiteral(" (not on the plot)");
        out << head;
        out << QStringLiteral("  %1 port(s), %2 used; %3 fixture(s), %4 channel(s)")
                   .arg(node.ports).arg(node.portsUsed).arg(node.fixtures).arg(node.channels);
        foreach (const Port &p, ports)
        {
            if (p.nodeIndex != n)
                continue;
            if (!p.used)
            {
                out << QStringLiteral("  Port %1: free").arg(p.portIndex + 1);
                continue;
            }
            QString line = QStringLiteral("  Port %1: universe %2, %3 fixture(s), %4/512 ch")
                               .arg(p.portIndex + 1).arg(p.universe + 1).arg(p.fixtures).arg(p.channels);
            if (!p.receiver.isEmpty())
                line += QStringLiteral(", via %1").arg(p.receiver);
            if (p.hasRun())
                line += QStringLiteral(", run ~%1").arg(lengthText(p.lengthMetres, feet));
            if (p.unplotted > 0)
                line += QStringLiteral(" (%1 not on the plot)").arg(p.unplotted);
            out << line;
        }
    }

    if (!networkFixtures.isEmpty())
    {
        out << QString() << QStringLiteral("NETWORK FIXTURES (their own endpoint)");
        foreach (const NetworkFixture &f, networkFixtures)
            out << QStringLiteral("  %1: %2, %3")
                       .arg(f.ip.isEmpty() ? QStringLiteral("(no IP)") : f.ip, f.name,
                            f.patched ? QStringLiteral("universe %1").arg(f.universe + 1)
                                      : QStringLiteral("not patched"));
    }

    if (!unserved.isEmpty())
    {
        out << QString() << QStringLiteral("UNSERVED (patched, but no port carries their universe)");
        foreach (const QString &name, unservedNames)
            out << QStringLiteral("  %1").arg(name);
    }

    if (!warnings.isEmpty())
    {
        out << QString() << QStringLiteral("WARNINGS");
        foreach (const QString &w, warnings)
            out << QStringLiteral("  %1").arg(w);
    }
    return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

/* One CSV field: a formula defused, quoted when it holds a comma, quote or
   line break (CsvField). */
static QString csv(const QString &field)
{
    return CsvField::escape(field);
}

static QString csvRow(const QStringList &fields)
{
    QStringList out;
    foreach (const QString &f, fields)
        out << csv(f);
    return out.join(QLatin1Char(','));
}

QString NetworkReport::toCsv(bool feet) const
{
    QStringList out;
    out << csvRow({ QStringLiteral("Node"), QStringLiteral("Transport"), QStringLiteral("IP"),
                    QStringLiteral("Port"), QStringLiteral("Universe"), QStringLiteral("Receiver"),
                    QStringLiteral("Fixtures"), QStringLiteral("Channels"),
                    feet ? QStringLiteral("Run (ft)") : QStringLiteral("Run (m)"),
                    QStringLiteral("Not on plot") });
    foreach (const Port &p, ports)
    {
        const Node &node = nodes.at(p.nodeIndex);
        out << csvRow({ node.name, node.transport, node.ip, QString::number(p.portIndex + 1),
                        p.used ? QString::number(p.universe + 1) : QString(), p.receiver,
                        QString::number(p.fixtures), QString::number(p.channels),
                        p.hasRun() ? QString::number(p.lengthMetres * (feet ? FEET_PER_METRE : 1.0), 'f', 1)
                               : QString(),
                        QString::number(p.unplotted) });
    }

    out << QString();
    out << csvRow({ QStringLiteral("Network fixture"), QStringLiteral("IP"), QStringLiteral("Universe") });
    foreach (const NetworkFixture &f, networkFixtures)
        out << csvRow({ f.name, f.ip, f.patched ? QString::number(f.universe + 1) : QString() });
    return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}
