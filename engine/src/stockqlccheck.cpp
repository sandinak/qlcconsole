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
    static const QSet<QString> s = { QStringLiteral("CueRunner"), QStringLiteral("ShowControl") };
    return s;
}

QString engineLabel(const QString &tag)
{
    // Which cue list GO runs is the Show Tree's too.
    if (tag == QLatin1String("ShowTree") || tag == QLatin1String("ShowControl"))
        return tr("the Show Tree (its cue lists, cues and hits)");
    if (tag == QLatin1String("SpeedMasters"))
        return tr("the speed masters");
    if (tag == QLatin1String("PowerDistribution"))
        return tr("the power plan");
    if (tag == QLatin1String("NetworkTopology"))
        return tr("the network map");
    if (tag == QLatin1String("Timecode"))
        return tr("the timecode source");
    if (tag == QLatin1String("Park") || tag == QLatin1String("Mark"))
        return tr("parked and marked channels");
    if (tag == QLatin1String("FocusCalls"))
        return tr("the focus calls");
    if (tag == QLatin1String("ControlMap"))
        return tr("the Control Map (MIDI and OSC triggers)");
    if (tag == QLatin1String("StagedFixtures"))
        return tr("the Inventory's fixtures (not patched)");
    if (tag == QLatin1String("OutputRuleSettings"))
        return tr("when \"on when not in use\" lights come on");
    return tr("other qlcconsole settings");
}

void add(QStringList *out, const QString &label)
{
    if (!out->contains(label))
        out->append(label);
}

/* Down a VC frame, for the widgets QLC+ 4 skips. */
void scanVc(QXmlStreamReader &xml, QStringList *out)
{
    while (xml.readNextStartElement())
    {
        if (forkVcWidgets().contains(xml.name().toString()))
        {
            add(out, tr("Cue Runner and Show Control widgets on the Virtual Console"));
            xml.skipCurrentElement();
        }
        else
            scanVc(xml, out);
    }
}

}

QStringList StockQlcCheck::losses(QIODevice *device)
{
    QStringList out;
    if (device == nullptr)
        return out;
    QXmlStreamReader xml(device);
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("Workspace"))
        return out;
    while (xml.readNextStartElement())
    {
        const QString top = xml.name().toString();
        if (top == QLatin1String("Engine"))
        {
            while (xml.readNextStartElement())
            {
                const QString tag = xml.name().toString();
                if (tag == QLatin1String("Monitor"))
                {
                    while (xml.readNextStartElement())
                    {
                        if (forkMonitor().contains(xml.name().toString()))
                            add(&out, tr("the Lighting Studio's rigging (trusses, towers, scenery)"));
                        xml.skipCurrentElement();
                    }
                    continue;
                }
                if (!stockEngine().contains(tag) && !quietEngine().contains(tag))
                    add(&out, engineLabel(tag));
                xml.skipCurrentElement();
            }
        }
        else if (top == QLatin1String("VirtualConsole"))
            scanVc(xml, &out);
        else
            xml.skipCurrentElement();
    }
    return out;
}

QStringList StockQlcCheck::lossesInFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QStringList();
    return losses(&file);
}
