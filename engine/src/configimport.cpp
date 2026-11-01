/*
  Q Light Controller Plus - qlcconsole
  configimport.cpp

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
#include <QJsonArray>
#include <QFileInfo>
#include <QBuffer>
#include <QFile>
#include <QSet>
#include <climits>
#include <memory>

#include "configimport.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "networkroute.h"
#include "qlcinputprofile.h"
#include "qlcioplugin.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "universe.h"
#include "doc.h"

namespace
{
const QString kUniverses = QStringLiteral("universes");
const QString kOutputs = QStringLiteral("outputs");
const QString kInputs = QStringLiteral("inputs");
const QString kFeedback = QStringLiteral("feedback");

bool isNone(const QString &plugin)
{
    return plugin.isEmpty() || plugin == KOutputNone || plugin == KInputNone;
}

/** "Art-Net 2.0.0.50 -> 2.0.0.255 [2.0.0.0/8]" */
QString patchText(const Universe::OfflinePatch &p)
{
    QString line = p.plugin;
    const QString where = !p.uid.isEmpty() && p.uid != KOutputNone ? p.uid : QString();
    if (!where.isEmpty())
        line += QLatin1Char(' ') + where;
    else if (p.line != UINT_MAX)
        line += ConfigImport::tr(" line %1").arg(p.line + 1);
    const QString target = NetworkRoute::targetFromParameters(p.params);
    if (!target.isEmpty())
        line += QStringLiteral(" → ") + target;
    if (!p.network.isEmpty())
        line += QStringLiteral(" [%1]").arg(p.network);
    if (p.tag == Universe::InputPatchTag && !p.profile.isEmpty() && p.profile != KInputNone)
        line += ConfigImport::tr(", profile %1").arg(p.profile);
    return line;
}

QString listText(const QList<Universe::OfflinePatch> &patches)
{
    if (patches.isEmpty())
        return ConfigImport::tr("none");
    QStringList parts;
    for (const Universe::OfflinePatch &p : patches)
        parts << patchText(p);
    return parts.join(QStringLiteral("; "));
}

/** What is patched on universe @a idx now, of @a tag. */
QList<Universe::OfflinePatch> currentPatches(Doc *doc, int idx, Universe::PatchTagType tag)
{
    InputOutputMap *io = doc->inputOutputMap();
    Universe *u = io->universe(io->getUniverseID(idx));
    QList<Universe::OfflinePatch> out;
    if (u == nullptr)
        return out;
    if (tag == Universe::OutputPatchTag)
        return u->outputPatchDescriptions();
    if (tag == Universe::InputPatchTag)
    {
        if (InputPatch *ip = u->inputPatch())
            if (ip->isPatched())
            {
                Universe::OfflinePatch p;
                p.tag = tag;
                p.plugin = ip->pluginName();
                p.uid = ip->inputName();
                p.line = ip->input();
                p.profile = ip->profileName();
                p.params = ip->getPluginParameters();
                p.network = NetworkRoute::networkForLine(ip->inputName());
                out << p;
            }
    }
    else if (OutputPatch *fb = u->feedbackPatch())
    {
        if (fb->isPatched())
        {
            Universe::OfflinePatch p;
            p.tag = tag;
            p.plugin = fb->pluginName();
            p.uid = fb->outputName();
            p.line = fb->output();
            p.params = fb->getPluginParameters();
            p.network = NetworkRoute::networkForLine(fb->outputName());
            out << p;
        }
    }
    for (const Universe::OfflinePatch &p : u->offlinePatches())
        if (p.tag == tag)
            out << p;
    return out;
}

bool hasContent(Doc *doc, int idx, const QStringList &sections)
{
    InputOutputMap *io = doc->inputOutputMap();
    if (sections.contains(kUniverses))
    {
        const QString name = io->getUniverseNameByIndex(idx).trimmed();
        if (!name.isEmpty() && name != QStringLiteral("Universe %1").arg(idx + 1))
            return true;
    }
    if (sections.contains(kOutputs) && !currentPatches(doc, idx, Universe::OutputPatchTag).isEmpty())
        return true;
    if (sections.contains(kInputs) && !currentPatches(doc, idx, Universe::InputPatchTag).isEmpty())
        return true;
    if (sections.contains(kFeedback) && !currentPatches(doc, idx, Universe::FeedbackPatchTag).isEmpty())
        return true;
    return false;
}

QByteArray captureUniverse(Doc *doc, int idx)
{
    InputOutputMap *io = doc->inputOutputMap();
    Universe *u = io->universe(io->getUniverseID(idx));
    QByteArray xml;
    if (u == nullptr)
        return xml;
    QXmlStreamWriter w(&xml);
    u->saveXML(&w);
    return xml;
}

void clearUniverse(Doc *doc, int idx)
{
    InputOutputMap *io = doc->inputOutputMap();
    Universe *u = io->universe(io->getUniverseID(idx));
    if (u == nullptr)
        return;
    io->setInputSlot(idx, QList<Universe::InputSlotDevice>());
    io->setInputPatch(idx, KInputNone, QString(), QLCIOPlugin::invalidLine(), QString());
    io->setOutputPatch(idx, KOutputNone, QString(), QLCIOPlugin::invalidLine(), true);
    io->replaceOutputPatches(idx, QList<Universe::OfflinePatch>());
    u->clearOfflinePatches(Universe::InputPatchTag);
    u->clearOfflinePatches(Universe::OutputPatchTag);
    u->clearOfflinePatches(Universe::FeedbackPatchTag);
}

void restoreUniverse(Doc *doc, int idx, const QByteArray &xml)
{
    InputOutputMap *io = doc->inputOutputMap();
    Universe *u = io->universe(io->getUniverseID(idx));
    if (u == nullptr)
        return;
    clearUniverse(doc, idx);
    QXmlStreamReader r(xml);
    if (r.readNextStartElement())
        u->loadXML(r, idx, io);
}

QString mapped(const QMap<QString, QString> &map, const QString &value)
{
    return map.contains(value) ? map.value(value) : value;
}

QString intText(const QJsonValue &v)
{
    if (v.isDouble())
        return QString::number(int(v.toDouble()));
    return v.toString().trimmed();
}
} // namespace

QStringList ConfigImport::allSections()
{
    return QStringList { kUniverses, kOutputs, kInputs, kFeedback };
}

/****************************************************************************
 * The source file
 ****************************************************************************/

ConfigImport::Source ConfigImport::readSource(const QString &path)
{
    Source src;
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly))
    {
        src.error = tr("%1 can't be opened").arg(path);
        return src;
    }
    if (!path.endsWith(QLatin1String(".qxw"), Qt::CaseInsensitive)
        && !path.endsWith(QLatin1String(".qlcc"), Qt::CaseInsensitive))
    {
        src.error = tr("%1 isn't a show file (.qxw)").arg(QFileInfo(path).fileName());
        return src;
    }
    QXmlStreamReader r(&file);
    bool engine = false;
    while (!r.atEnd() && !r.hasError())
    {
        if (!r.readNextStartElement())
            continue;
        if (r.name() == QLatin1String("Workspace"))
            continue;       // descend
        if (r.name() != QLatin1String("Engine"))
        {
            r.skipCurrentElement();
            continue;
        }
        engine = true;
        while (r.readNextStartElement())
        {
            if (r.name() == QLatin1String("Fixture"))
            {
                ++src.fixtures;
                r.skipCurrentElement();
            }
            else if (r.name() == QLatin1String("FixtureGroup"))
            {
                ++src.groups;
                r.skipCurrentElement();
            }
            else if (r.name() == QLatin1String("Palette"))
            {
                ++src.palettes;
                r.skipCurrentElement();
            }
            else if (r.name() == QLatin1String("InputOutputMap"))
            {
                while (r.readNextStartElement())
                {
                    if (r.name() != KXMLQLCUniverse)
                    {
                        r.skipCurrentElement();
                        continue;
                    }
                    SourceUniverse u;
                    u.number = src.universes.count() + 1;
                    const QXmlStreamAttributes a = r.attributes();
                    u.name = a.value(KXMLQLCUniverseName).toString();
                    u.passthrough = a.value(KXMLQLCUniversePassthrough).toString() == QLatin1String("True")
                                    || a.value(KXMLQLCUniversePassthrough).toString() == QLatin1String("1");
                    while (r.readNextStartElement())
                    {
                        if (r.name() == KXMLQLCUniverseInputSlot)
                        {
                            u.hasSlot = true;
                            r.skipCurrentElement();
                            continue;
                        }
                        Universe::OfflinePatch p;
                        if (r.name() == KXMLQLCUniverseInputPatch)
                            p.tag = Universe::InputPatchTag;
                        else if (r.name() == KXMLQLCUniverseOutputPatch)
                            p.tag = Universe::OutputPatchTag;
                        else if (r.name() == KXMLQLCUniverseFeedbackPatch)
                            p.tag = Universe::FeedbackPatchTag;
                        else
                        {
                            r.skipCurrentElement();
                            continue;
                        }
                        const QXmlStreamAttributes pa = r.attributes();
                        p.plugin = pa.value(KXMLQLCUniversePlugin).toString();
                        p.uid = pa.value(KXMLQLCUniverseLineUID).toString();
                        if (pa.hasAttribute(KXMLQLCUniverseLine))
                            p.line = pa.value(KXMLQLCUniverseLine).toString().toUInt();
                        p.network = pa.value(KXMLQLCUniverseNetwork).toString();
                        p.profile = pa.value(KXMLQLCUniverseProfileName).toString();
                        p.hidProfile = pa.value(KXMLQLCUniverseHIDProfileName).toString();
                        while (r.readNextStartElement())
                        {
                            if (r.name() == KXMLQLCUniversePluginParameters)
                            {
                                const QXmlStreamAttributes pp = r.attributes();
                                for (int i = 0; i < pp.count(); ++i)
                                    p.params[pp.at(i).name().toString()] = pp.at(i).value().toString();
                            }
                            r.skipCurrentElement();
                        }
                        u.patches << p;
                    }
                    src.universes << u;
                }
            }
            else
            {
                r.skipCurrentElement();
            }
        }
        break;
    }
    if (!engine)
    {
        src.error = tr("%1 isn't a show file").arg(QFileInfo(path).fileName());
        return src;
    }
    if (r.hasError() && src.universes.isEmpty())
    {
        src.error = tr("%1 can't be read: %2").arg(QFileInfo(path).fileName(), r.errorString());
        return src;
    }
    src.ok = true;
    return src;
}

QString ConfigImport::sourceText(const Source &source)
{
    if (!source.ok)
        return source.error;
    QStringList lines;
    lines << tr("%n universe(s) in the file", "", source.universes.count());
    for (const SourceUniverse &u : source.universes)
    {
        lines << tr("Universe %1 \"%2\"%3").arg(u.number).arg(u.name, u.passthrough ? tr(" (passthrough)") : QString());
        bool any = false;
        for (const Universe::OfflinePatch &p : u.patches)
        {
            if (isNone(p.plugin))
                continue;
            any = true;
            const QString kind = p.tag == Universe::OutputPatchTag ? tr("output")
                               : p.tag == Universe::InputPatchTag ? tr("input") : tr("feedback");
            lines << QStringLiteral("  %1: %2").arg(kind, patchText(p));
        }
        if (!any)
            lines << tr("  no patches");
        if (u.hasSlot)
            lines << tr("  (a controller slot: not imported)");
    }
    lines << tr("Also in the file: %1 fixture(s), %2 group(s), %3 palette(s). File ▸ Import ▸ "
                "Workspace brings those in; this import does not.")
                 .arg(source.fixtures).arg(source.groups).arg(source.palettes);
    return lines.join(QLatin1Char('\n'));
}

/****************************************************************************
 * Options
 ****************************************************************************/

bool ConfigImport::optionsFrom(const QJsonObject &args, Options *options, QString *why)
{
    Options o;
    o.file = args.value(QLatin1String("file")).toString().trimmed();
    for (const QJsonValue &v : args.value(QLatin1String("sections")).toArray())
        o.sections << v.toString().trimmed().toLower();
    for (const QJsonValue &v : args.value(QLatin1String("universes")).toArray())
        o.universes << int(v.toDouble());
    const QJsonObject um = args.value(QLatin1String("universeMap")).toObject();
    for (auto it = um.constBegin(); it != um.constEnd(); ++it)
    {
        bool ok1 = false, ok2 = false;
        const int from = it.key().trimmed().toInt(&ok1);
        const int to = intText(it.value()).toInt(&ok2);
        if (!ok1 || !ok2)
        {
            *why = tr("universeMap: \"%1\" to \"%2\" isn't universe numbers").arg(it.key(), intText(it.value()));
            return false;
        }
        o.universeMap.insert(from, to);
    }
    const QJsonObject am = args.value(QLatin1String("addressMap")).toObject();
    for (auto it = am.constBegin(); it != am.constEnd(); ++it)
        o.addressMap.insert(it.key().trimmed(), it.value().toString().trimmed());
    const QJsonObject pm = args.value(QLatin1String("pluginMap")).toObject();
    for (auto it = pm.constBegin(); it != pm.constEnd(); ++it)
        o.pluginMap.insert(it.key().trimmed(), it.value().toString().trimmed());
    o.onCollision = args.value(QLatin1String("onCollision")).toString().trimmed();
    o.onMissingPlugin = args.value(QLatin1String("onMissingPlugin")).toString().trimmed();
    *options = o;
    return true;
}

QJsonObject ConfigImport::toArgs(const Options &o)
{
    QJsonObject a;
    a.insert(QStringLiteral("file"), o.file);
    if (!o.sections.isEmpty())
        a.insert(QStringLiteral("sections"), QJsonArray::fromStringList(o.sections));
    if (!o.universes.isEmpty())
    {
        QJsonArray u;
        for (int n : o.universes)
            u << n;
        a.insert(QStringLiteral("universes"), u);
    }
    auto putMap = [&a](const char *key, const QMap<QString, QString> &m) {
        if (m.isEmpty())
            return;
        QJsonObject obj;
        for (auto it = m.cbegin(); it != m.cend(); ++it)
            obj.insert(it.key(), it.value());
        a.insert(QLatin1String(key), obj);
    };
    if (!o.universeMap.isEmpty())
    {
        QJsonObject obj;
        for (auto it = o.universeMap.cbegin(); it != o.universeMap.cend(); ++it)
            obj.insert(QString::number(it.key()), QString::number(it.value()));
        a.insert(QStringLiteral("universeMap"), obj);
    }
    putMap("addressMap", o.addressMap);
    putMap("pluginMap", o.pluginMap);
    if (!o.onCollision.isEmpty())
        a.insert(QStringLiteral("onCollision"), o.onCollision);
    if (!o.onMissingPlugin.isEmpty())
        a.insert(QStringLiteral("onMissingPlugin"), o.onMissingPlugin);
    return a;
}

/****************************************************************************
 * Plan
 ****************************************************************************/

QString ConfigImport::Plan::text() const
{
    QStringList out;
    if (!problems.isEmpty())
    {
        out << ConfigImport::tr("Problems (nothing is imported until these are settled):");
        for (const QString &p : problems)
            out << QStringLiteral("  - ") + p;
    }
    if (!warnings.isEmpty())
    {
        out << ConfigImport::tr("Warnings (they don't stop the import):");
        for (const QString &w : warnings)
            out << QStringLiteral("  - ") + w;
    }
    if (!diff.isEmpty())
    {
        out << ConfigImport::tr("Changes:");
        out << diff;
    }
    if (out.isEmpty())
        out << ConfigImport::tr("Nothing to import");
    return out.join(QLatin1Char('\n'));
}

ConfigImport::Plan ConfigImport::plan(Doc *doc, const Options &opt)
{
    Plan p;
    p.options = opt;
    if (doc == nullptr)
    {
        p.problems << tr("No show is open");
        return p;
    }
    if (doc->mode() == Doc::Operate)
    {
        p.problems << tr("Configuration import is unavailable in Operate mode");
        return p;
    }
    p.source = readSource(opt.file);
    if (!p.source.ok)
    {
        p.problems << p.source.error;
        return p;
    }
    InputOutputMap *io = doc->inputOutputMap();

    QStringList sections = opt.sections.isEmpty() ? allSections() : opt.sections;
    for (const QString &s : sections)
        if (!allSections().contains(s))
            p.problems << tr("\"%1\" isn't a section (%2)").arg(s, allSections().join(QStringLiteral(", ")));
    if (opt.onCollision.isEmpty() == false && opt.onCollision != QLatin1String("skip")
        && opt.onCollision != QLatin1String("replace") && opt.onCollision != QLatin1String("renumber"))
        p.problems << tr("onCollision is skip, replace or renumber, not \"%1\"").arg(opt.onCollision);
    if (opt.onMissingPlugin.isEmpty() == false && opt.onMissingPlugin != QLatin1String("skip")
        && opt.onMissingPlugin != QLatin1String("keep"))
        p.problems << tr("onMissingPlugin is skip or keep, not \"%1\"").arg(opt.onMissingPlugin);
    if (!p.problems.isEmpty())
        return p;

    // Which source universes.
    QList<const SourceUniverse *> picked;
    if (opt.universes.isEmpty())
    {
        for (const SourceUniverse &u : p.source.universes)
            picked << &u;
    }
    else
    {
        for (int n : opt.universes)
        {
            if (n < 1 || n > p.source.universes.count())
                p.problems << tr("The file has no universe %1 (it has %2)").arg(n).arg(p.source.universes.count());
            else if (!picked.contains(&p.source.universes.at(n - 1)))
                picked << &p.source.universes.at(n - 1);
        }
    }
    if (picked.isEmpty() && p.problems.isEmpty())
        p.problems << tr("The file has no universes");
    for (auto it = opt.universeMap.cbegin(); it != opt.universeMap.cend(); ++it)
    {
        bool used = false;
        for (const SourceUniverse *u : picked)
            used = used || u->number == it.key();
        if (!used)
            p.problems << tr("universeMap names universe %1, which isn't being imported").arg(it.key());
    }
    if (!p.problems.isEmpty())
        return p;

    // Where each lands.
    const int count = int(io->universesCount());
    QSet<int> targets;
    int highest = count - 1;
    for (const SourceUniverse *u : picked)
    {
        Step s;
        s.sourceNumber = u->number;
        s.sourceName = u->name;
        const int to = opt.universeMap.contains(u->number) ? opt.universeMap.value(u->number) : u->number;
        if (to < 1 || to > int(InputOutputMap::maxUniverses()))
        {
            p.problems << tr("Universe %1 can't go to universe %2 (1 to %3)").arg(u->number).arg(to)
                              .arg(InputOutputMap::maxUniverses());
            continue;
        }
        s.target = to - 1;
        if (targets.contains(s.target))
        {
            p.problems << tr("Two universes are being put on universe %1").arg(to);
            continue;
        }
        targets.insert(s.target);
        highest = qMax(highest, s.target);
        p.steps << s;
    }
    if (!p.problems.isEmpty())
        return p;

    int nextFree = highest + 1;
    for (Step &s : p.steps)
    {
        if (s.target < count && hasContent(doc, s.target, sections))
        {
            s.collision = true;
            if (opt.onCollision.isEmpty())
            {
                p.problems << tr("Universe %1 here already has a setup (%2). Choose what happens to it: "
                                 "skip (keep it, leave out file universe %3), replace it, or renumber "
                                 "(put file universe %3 on a new universe)")
                                  .arg(s.target + 1)
                                  .arg(io->getUniverseNameByIndex(s.target))
                                  .arg(s.sourceNumber);
                continue;
            }
            if (opt.onCollision == QLatin1String("skip"))
                s.skip = true;
            else if (opt.onCollision == QLatin1String("renumber"))
                s.target = nextFree++;
        }
        s.isNew = s.target >= count;
    }
    if (!p.problems.isEmpty())
        return p;

    // The patches, mapped.
    const bool wantOut = sections.contains(kOutputs);
    const bool wantIn = sections.contains(kInputs);
    const bool wantFb = sections.contains(kFeedback);
    QSet<QString> missingPlugins;
    for (Step &s : p.steps)
    {
        const SourceUniverse &u = p.source.universes.at(s.sourceNumber - 1);
        const QString who = tr("Universe %1").arg(s.sourceNumber);
        if (s.skip)
            continue;
        s.setName = sections.contains(kUniverses);
        s.name = u.name;
        s.passthrough = u.passthrough;
        s.setOutputs = wantOut;
        s.setInput = wantIn;
        s.setFeedback = wantFb;
        if (u.hasSlot && wantIn)
            p.warnings << tr("%1: its controller slot isn't imported (the input patch is)").arg(who);
        for (const Universe::OfflinePatch &src : u.patches)
        {
            if (isNone(src.plugin))
                continue;
            const bool wanted = (src.tag == Universe::OutputPatchTag && wantOut)
                                || (src.tag == Universe::InputPatchTag && wantIn)
                                || (src.tag == Universe::FeedbackPatchTag && wantFb);
            if (!wanted)
                continue;
            Universe::OfflinePatch m = src;
            m.plugin = mapped(opt.pluginMap, src.plugin);
            m.uid = mapped(opt.addressMap, src.uid);
            m.network = mapped(opt.addressMap, src.network);
            for (auto it = m.params.begin(); it != m.params.end(); ++it)
                if (it.value().typeId() == QMetaType::QString)
                    it.value() = mapped(opt.addressMap, it.value().toString());

            if (doc->ioPluginCache()->plugin(m.plugin) == nullptr)
            {
                missingPlugins.insert(m.plugin);
                if (opt.onMissingPlugin.isEmpty())
                {
                    p.problems << tr("%1: the plugin \"%2\" isn't available here (a device not attached?). "
                                     "Choose: skip those patches, keep them (offline, waiting for the "
                                     "plugin), or map it to a plugin here (pluginMap)").arg(who, m.plugin);
                    continue;
                }
                if (opt.onMissingPlugin == QLatin1String("skip"))
                {
                    s.skipped << patchText(src);
                    continue;
                }
                p.warnings << tr("%1: plugin \"%2\" isn't available here; the patch is kept, offline")
                                  .arg(who, m.plugin);
            }
            if (src.tag == Universe::InputPatchTag && !m.profile.isEmpty() && m.profile != KInputNone
                && io->profile(m.profile) == nullptr)
                p.warnings << tr("%1: input profile \"%2\" isn't installed here").arg(who, m.profile);

            // Route requirement: warnings only.
            const QString target = NetworkRoute::targetFromParameters(m.params);
            const QString dest = !target.isEmpty() ? target : m.network;
            if (!dest.isEmpty() && target != QLatin1String("255.255.255.255"))
            {
                const NetworkRoute::RouteCheck rc = NetworkRoute::checkRoute(dest);
                if (rc.kind == NetworkRoute::NoRoute)
                    p.warnings << tr("%1: %2 is not currently routable from this machine — expected "
                                     "if the show network isn't connected").arg(who, dest);
                else if (rc.kind == NetworkRoute::DefaultRoute)
                    p.warnings << tr("%1: %2 would leave by the DEFAULT route (%3), not a route to the show "
                                     "network. The show NIC may be down and output would go out the house "
                                     "or Wi-Fi network").arg(who, dest, rc.via);
            }

            if (src.tag == Universe::OutputPatchTag)
                s.outputs << m;
            else if (src.tag == Universe::InputPatchTag)
            {
                if (s.inputs.isEmpty())
                    s.inputs << m;
            }
            else if (s.feedback.isEmpty())
            {
                s.feedback << m;
            }
        }
    }
    if (!p.problems.isEmpty())
        return p;

    // Preview.
    for (const Step &s : p.steps)
    {
        const QString head = tr("Universe %1 \"%2\" → universe %3").arg(s.sourceNumber).arg(s.sourceName).arg(s.target + 1);
        if (s.skip)
        {
            p.diff << QStringLiteral("  %1: %2").arg(head, tr("left out (universe %1 here is kept)")
                                                              .arg(opt.universeMap.contains(s.sourceNumber)
                                                                       ? opt.universeMap.value(s.sourceNumber)
                                                                       : s.sourceNumber));
            continue;
        }
        QString note = s.isNew ? tr("a new universe")
                     : s.collision ? tr("replaces what it has") : tr("an empty universe here");
        p.diff << QStringLiteral("  %1 (%2)").arg(head, note);
        if (s.setName)
            p.diff << tr("      name: \"%1\" → \"%2\"")
                          .arg(s.isNew ? QString() : io->getUniverseNameByIndex(s.target), s.name);
        auto line = [&](const QString &what, Universe::PatchTagType tag, const QList<Universe::OfflinePatch> &now) {
            const QList<Universe::OfflinePatch> was = s.isNew ? QList<Universe::OfflinePatch>()
                                                              : currentPatches(doc, s.target, tag);
            p.diff << QStringLiteral("      %1: %2 → %3").arg(what, listText(was), listText(now));
        };
        if (s.setOutputs)
            line(tr("outputs"), Universe::OutputPatchTag, s.outputs);
        if (s.setInput)
            line(tr("input"), Universe::InputPatchTag, s.inputs);
        if (s.setFeedback)
            line(tr("feedback"), Universe::FeedbackPatchTag, s.feedback);
        for (const QString &sk : s.skipped)
            p.diff << tr("      left out (plugin not available): %1").arg(sk);
    }
    if (int(p.steps.last().target) > count)
    {
        bool gap = false;
        for (const Step &s : p.steps)
            gap = gap || (s.isNew && s.target > count);
        if (gap)
            p.warnings << tr("Universes between %1 and the highest one are made empty so the numbering has no gaps")
                              .arg(count);
    }
    return p;
}

/****************************************************************************
 * Apply
 ****************************************************************************/

ConfigImport::Applied ConfigImport::apply(Doc *doc, const Options &opt)
{
    Applied out;
    const Plan p = plan(doc, opt);
    if (!p.ok())
    {
        out.message = p.problems.first();
        out.report = p.problems;
        return out;
    }
    InputOutputMap *io = doc->inputOutputMap();
    const int before = int(io->universesCount());

    QList<const Step *> run;
    for (const Step &s : p.steps)
        if (!s.skip)
            run << &s;

    // What the universes were, for undo.
    auto beforeXml = std::make_shared<QMap<int, QByteArray>>();
    for (const Step *s : run)
        if (s->target < before && !beforeXml->contains(s->target))
            beforeXml->insert(s->target, captureUniverse(doc, s->target));

    QStringList report;
    for (const Step *s : run)
    {
        while (int(io->universesCount()) <= s->target)
            if (!io->addUniverse())
            {
                // Out of room: put back what was done.
                for (int i = int(io->universesCount()) - 1; i >= before; --i)
                    io->removeUniverse(i);
                for (auto it = beforeXml->cbegin(); it != beforeXml->cend(); ++it)
                    restoreUniverse(doc, it.key(), it.value());
                out.message = tr("No room for universe %1").arg(s->target + 1);
                return out;
            }
        const int t = s->target;
        if (s->collision)
        {
            // Replace: the selected sections are cleared first.
            Universe *u = io->universe(io->getUniverseID(t));
            if (s->setOutputs)
                io->replaceOutputPatches(t, QList<Universe::OfflinePatch>());
            if (s->setInput)
            {
                io->setInputSlot(t, QList<Universe::InputSlotDevice>());
                io->setInputPatch(t, KInputNone, QString(), QLCIOPlugin::invalidLine(), QString());
            }
            if (s->setFeedback)
                io->setOutputPatch(t, KOutputNone, QString(), QLCIOPlugin::invalidLine(), true);
            if (u != nullptr)
            {
                if (s->setOutputs)
                    u->clearOfflinePatches(Universe::OutputPatchTag);
                if (s->setInput)
                    u->clearOfflinePatches(Universe::InputPatchTag);
                if (s->setFeedback)
                    u->clearOfflinePatches(Universe::FeedbackPatchTag);
            }
        }
        if (s->setName)
        {
            io->setUniverseName(t, s->name);
            io->setUniversePassthrough(t, s->passthrough);
        }
        if (s->setOutputs && !s->outputs.isEmpty())
            io->replaceOutputPatches(t, s->outputs);
        for (const Universe::OfflinePatch &ip : s->inputs)
            io->loadPatch(t, ip);
        for (const Universe::OfflinePatch &fb : s->feedback)
            io->loadPatch(t, fb);

        QStringList what;
        if (s->setName)
            what << tr("name \"%1\"").arg(s->name);
        if (s->setOutputs)
            what << tr("outputs: %1").arg(listText(s->outputs));
        if (s->setInput)
            what << tr("input: %1").arg(listText(s->inputs));
        if (s->setFeedback)
            what << tr("feedback: %1").arg(listText(s->feedback));
        report << tr("Universe %1 → %2%3: %4").arg(s->sourceNumber).arg(t + 1)
                      .arg(s->isNew ? tr(" (new)") : QString(), what.join(QStringLiteral("; ")));
        for (const QString &sk : s->skipped)
            report << tr("  skipped (plugin not available): %1").arg(sk);
    }
    for (const Step &s : p.steps)
        if (s.skip)
            report << tr("Universe %1 left out (collision: skip)").arg(s.sourceNumber);
    report << p.warnings;

    const int after = int(io->universesCount());
    auto afterXml = std::make_shared<QMap<int, QByteArray>>();
    for (const Step *s : run)
        afterXml->insert(s->target, captureUniverse(doc, s->target));

    out.ok = true;
    out.report = report;
    out.message = tr("Imported %n universe(s) from %1", "", run.count()).arg(QFileInfo(opt.file).fileName());
    out.undo = [doc, before, after, beforeXml, afterXml](bool undone) {
        InputOutputMap *map = doc->inputOutputMap();
        if (undone)
        {
            for (int i = int(map->universesCount()) - 1; i >= before; --i)
                map->removeUniverse(i);
            for (auto it = beforeXml->cbegin(); it != beforeXml->cend(); ++it)
                restoreUniverse(doc, it.key(), it.value());
        }
        else
        {
            while (int(map->universesCount()) < after)
                if (!map->addUniverse())
                    break;
            for (auto it = afterXml->cbegin(); it != afterXml->cend(); ++it)
                restoreUniverse(doc, it.key(), it.value());
        }
    };
    doc->setModified();
    return out;
}
