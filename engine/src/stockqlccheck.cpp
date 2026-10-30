/*
  Q Light Controller Plus
  stockqlccheck.cpp

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

#include <QCoreApplication>
#include <QXmlStreamReader>
#include <QHash>
#include <QMap>
#include <QFile>
#include <QSet>

#include "stockqlccheck.h"

namespace
{

QString tr(const char *text)
{
    return QCoreApplication::translate("StockQlcCheck", text);
}

/* The <Engine> children QLC+ 4.14's Doc::loadXML reads. */
const QSet<QString> &stockEngine()
{
    static const QSet<QString> s = {
        QStringLiteral("Fixture"), QStringLiteral("FixtureGroup"), QStringLiteral("ChannelsGroup"),
        QStringLiteral("Palette"), QStringLiteral("Function"), QStringLiteral("Bus"),
        QStringLiteral("InputOutputMap"), QStringLiteral("Monitor") };
    return s;
}

/* Settings QLC+ 4 has no idea of but that change nothing it shows: not
   worth a word. */
const QSet<QString> &quietEngine()
{
    static const QSet<QString> s = { QStringLiteral("ShutterAutoOpen"), QStringLiteral("MoveInBlack") };
    return s;
}

/* The Lighting Studio's rigging, in <Monitor>: QLC+ 4 keeps its fixtures'
   places and drops these. */
const QSet<QString> &forkMonitor()
{
    static const QSet<QString> s = {
        QStringLiteral("Scenery"), QStringLiteral("Truss"),
        QStringLiteral("Tower"), QStringLiteral("Boom"), QStringLiteral("Hanger"),
        QStringLiteral("Stand"), QStringLiteral("StagePlatform"), QStringLiteral("StageTarget"),
        QStringLiteral("MonitorImage"), QStringLiteral("FixtureRig") };
    return s;
}

/* Virtual Console widgets QLC+ 4.14's frames don't know. */
const QSet<QString> &forkVcWidgets()
{
    static const QSet<QString> s = { QStringLiteral("CueRunner"), QStringLiteral("ShowControl"),
        QStringLiteral("EffectControl") };
    return s;
}

/* What was met, in the order met: a kind of content and how many of it. */
struct Found
{
    QStringList order;
    QMap<QString, int> count;

    void add(const QString &kind, int n)
    {
        if (!count.contains(kind))
            order.append(kind);
        count[kind] += n;
    }
};

/* Every element under (and including) the current one, by name; the
   current element is consumed. */
void tally(QXmlStreamReader &xml, QHash<QString, int> *seen)
{
    (*seen)[xml.name().toString()] += 1;
    while (xml.readNextStartElement())
        tally(xml, seen);
}

QString plural(int n, const char *one, const char *many)
{
    return QCoreApplication::translate("StockQlcCheck", n == 1 ? one : many).arg(n);
}

/* How much of a kind, said after its name: "(3 cue lists, 12 cues)". */
QString countsOf(const QString &kind, const QHash<QString, int> &seen, int n)
{
    QStringList parts;
    if (kind == QLatin1String("ShowTree"))
    {
        const int lists = seen.value(QStringLiteral("CueList")) + seen.value(QStringLiteral("Looks"));
        const int cues = seen.value(QStringLiteral("Cue"));
        if (lists > 0)
            parts << plural(lists, "%1 cue list", "%1 cue lists");
        if (cues > 0)
            parts << plural(cues, "%1 cue", "%1 cues");
        if (seen.value(QStringLiteral("Look")) > 0)
            parts << plural(seen.value(QStringLiteral("Look")), "%1 look", "%1 looks");
    }
    else if (kind == QLatin1String("SpeedMasters") && seen.value(QStringLiteral("Master")) > 0)
        parts << plural(seen.value(QStringLiteral("Master")), "%1 master", "%1 masters");
    else if (kind == QLatin1String("FocusCalls") && seen.value(QStringLiteral("Call")) > 0)
        parts << plural(seen.value(QStringLiteral("Call")), "%1 call", "%1 calls");
    else if (kind == QLatin1String("ControlMap") && seen.value(QStringLiteral("Trigger")) > 0)
        parts << plural(seen.value(QStringLiteral("Trigger")), "%1 trigger", "%1 triggers");
    else if (kind == QLatin1String("Rigging") || kind == QLatin1String("Widgets"))
        parts << (kind == QLatin1String("Rigging") ? plural(n, "%1 piece", "%1 pieces")
                                                   : plural(n, "%1 widget", "%1 widgets"));
    else if (kind == QLatin1String("StagedFixtures") && seen.value(QStringLiteral("Fixture")) > 0)
        parts << plural(seen.value(QStringLiteral("Fixture")), "%1 fixture", "%1 fixtures");
    return parts.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(parts.join(QStringLiteral(", ")));
}

QString engineLabel(const QString &kind)
{
    // Which cue list GO runs is the Show Tree's too.
    if (kind == QLatin1String("ShowTree"))
        return tr("the Show Tree");
    if (kind == QLatin1String("SpeedMasters"))
        return tr("the speed masters");
    if (kind == QLatin1String("PowerDistribution"))
        return tr("the power plan");
    if (kind == QLatin1String("NetworkTopology"))
        return tr("the network map");
    if (kind == QLatin1String("Timecode"))
        return tr("the timecode source");
    if (kind == QLatin1String("Park") || kind == QLatin1String("Mark"))
        return tr("parked and marked channels");
    if (kind == QLatin1String("FocusCalls"))
        return tr("the focus calls");
    if (kind == QLatin1String("ControlMap"))
        return tr("the Control Map");
    if (kind == QLatin1String("StagedFixtures"))
        return tr("the Inventory's fixtures (not patched)");
    if (kind == QLatin1String("OutputRuleSettings"))
        return tr("when \"on when not in use\" lights come on");
    if (kind == QLatin1String("Teaching"))
        return tr("the Teaching settings");
    if (kind == QLatin1String("Operate"))
        return tr("the Operate settings");
    if (kind == QLatin1String("WebAccess"))
        return tr("the web access settings");
    if (kind == QLatin1String("Rigging"))
        return tr("the Lighting Studio's rigging");
    if (kind == QLatin1String("Widgets"))
        return tr("Cue Runner, Show Control and Effect Control widgets on the Virtual Console");
    return tr("other qlcconsole settings");
}

/* Down a VC frame, for the widgets QLC+ 4 skips. */
void scanVc(QXmlStreamReader &xml, Found *out, QMap<QString, QHash<QString, int>> *seen)
{
    while (xml.readNextStartElement())
    {
        if (forkVcWidgets().contains(xml.name().toString()))
        {
            out->add(QStringLiteral("Widgets"), 1);
            xml.skipCurrentElement();
        }
        else
            scanVc(xml, out, seen);
    }
}

}

QStringList StockQlcCheck::losses(QIODevice *device)
{
    QStringList result;
    if (device == nullptr)
        return result;
    Found out;
    QMap<QString, QHash<QString, int>> seen;
    QXmlStreamReader xml(device);
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("Workspace"))
        return result;
    while (xml.readNextStartElement())
    {
        const QString top = xml.name().toString();
        if (top == QLatin1String("Engine"))
        {
            while (xml.readNextStartElement())
            {
                QString tag = xml.name().toString();
                if (tag == QLatin1String("Monitor"))
                {
                    while (xml.readNextStartElement())
                    {
                        if (forkMonitor().contains(xml.name().toString()))
                            out.add(QStringLiteral("Rigging"), 1);
                        xml.skipCurrentElement();
                    }
                    continue;
                }
                if (stockEngine().contains(tag) || quietEngine().contains(tag))
                {
                    xml.skipCurrentElement();
                    continue;
                }
                // One kind under two tags
                if (tag == QLatin1String("ShowControl"))
                    tag = QStringLiteral("ShowTree");
                else if (tag == QLatin1String("Mark"))
                    tag = QStringLiteral("Park");
                const QSet<QString> known = { "ShowTree", "SpeedMasters", "PowerDistribution", "NetworkTopology",
                                              "Timecode", "Park", "FocusCalls", "ControlMap", "StagedFixtures",
                                              "OutputRuleSettings", "Teaching", "Operate", "WebAccess" };
                if (!known.contains(tag))
                    tag = QStringLiteral("Other");
                out.add(tag, 0);
                tally(xml, &seen[tag]);
            }
        }
        else if (top == QLatin1String("VirtualConsole"))
            scanVc(xml, &out, &seen);
        else
            xml.skipCurrentElement();
    }
    foreach (const QString &kind, out.order)
        result << engineLabel(kind) + countsOf(kind, seen.value(kind), out.count.value(kind));
    return result;
}

QStringList StockQlcCheck::lossesInFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QStringList();
    return losses(&file);
}
