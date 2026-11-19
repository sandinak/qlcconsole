/*
  Q Light Controller Plus
  venueimport.cpp

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
#include <QRegularExpression>
#include <QFileInfo>
#include <QFile>
#include <algorithm>

#include "monitorproperties.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "qlcfile.h"
#include "inputoutputmap.h"
#include "fixturegroup.h"
#include "venueimport.h"
#include "rigowner.h"
#include "fixture.h"
#include "truss.h"
#include "doc.h"

namespace
{
/* The element the reader is at, whole, without its namespace (a show's
   elements are in the workspace's; the venue file's are in none); the
   reader is left at its end. */
QByteArray copyElement(QXmlStreamReader &r)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    int depth = 0;
    while (!r.atEnd())
    {
        if (r.isStartElement())
        {
            ++depth;
            w.writeStartElement(r.name().toString());
            for (const QXmlStreamAttribute &a : r.attributes())
                w.writeAttribute(a.qualifiedName().toString(), a.value().toString());
        }
        else if (r.isEndElement())
        {
            w.writeEndElement();
            if (--depth == 0)
                break;
        }
        else if (r.isCharacters() && !r.isWhitespace())
        {
            if (r.isCDATA())
                w.writeCDATA(r.text().toString());
            else
                w.writeCharacters(r.text().toString());
        }
        r.readNext();
    }
    return out;
}

/* A <Fixture> with its id, universe and owner set: everything else as it
   was. */
QByteArray rewriteFixture(const QByteArray &xml, quint32 id, quint32 universe)
{
    QXmlStreamReader r(xml);
    QByteArray out;
    QXmlStreamWriter w(&out);
    int depth = 0;
    bool owner = false;
    while (!r.atEnd())
    {
        r.readNext();
        if (r.isStartElement())
        {
            ++depth;
            const QString n = r.name().toString();
            if (depth == 2 && (n == KXMLFixtureID || n == KXMLFixtureUniverse || n == KXMLFixtureOwner))
            {
                r.readElementText();
                --depth;
                if (n == KXMLFixtureID)
                    w.writeTextElement(n, QString::number(id));
                else if (n == KXMLFixtureUniverse)
                    w.writeTextElement(n, QString::number(universe));
                else
                {
                    w.writeTextElement(n, RigOwner::xmlName(RigOwner::House));
                    owner = true;
                }
                continue;
            }
            w.writeStartElement(n);
            for (const QXmlStreamAttribute &a : r.attributes())
                w.writeAttribute(a.qualifiedName().toString(), a.value().toString());
        }
        else if (r.isEndElement())
        {
            if (depth == 1 && !owner)
                w.writeTextElement(KXMLFixtureOwner, RigOwner::xmlName(RigOwner::House));
            w.writeEndElement();
            --depth;
        }
        else if (r.isCharacters() && !r.isWhitespace())
        {
            w.writeCharacters(r.text().toString());
        }
    }
    return out;
}

/* What the venue keeps of a <Fixture> without loading it. */
void describe(VenueRig::HouseFixture &hf)
{
    QXmlStreamReader r(hf.xml);
    if (!r.readNextStartElement())
        return;
    while (r.readNextStartElement())
    {
        const QStringView n = r.name();
        if (n == KXMLFixtureID)
            hf.id = r.readElementText().toUInt();
        else if (n == KXMLFixtureName)
            hf.name = r.readElementText();
        else if (n == KXMLFixtureUniverse)
            hf.universe = r.readElementText().toUInt();
        else if (n == KXMLFixtureAddress)
            hf.address = r.readElementText().toUInt();
        else if (n == KXMLFixtureChannels)
            hf.channels = r.readElementText().toUInt();
        else if (n == KXMLFixturePatched)
            hf.patched = r.readElementText() != KXMLQLCFalse;
        else
            r.skipCurrentElement();
    }
}

/* Manufacturer and model of a <Fixture>, for a warning. */
QPair<QString, QString> makeModel(const QByteArray &xml)
{
    QXmlStreamReader r(xml);
    QPair<QString, QString> out;
    if (!r.readNextStartElement())
        return out;
    while (r.readNextStartElement())
    {
        if (r.name() == KXMLQLCFixtureDefManufacturer)
            out.first = r.readElementText();
        else if (r.name() == KXMLQLCFixtureDefModel)
            out.second = r.readElementText();
        else
            r.skipCurrentElement();
    }
    return out;
}

VenueImport::Group readGroup(QXmlStreamReader &r)
{
    VenueImport::Group g;
    while (r.readNextStartElement())
    {
        if (r.name() == KXMLQLCFixtureGroupName)
        {
            g.name = r.readElementText();
        }
        else if (r.name() == KXMLQLCFixtureGroupHead)
        {
            const QXmlStreamAttributes a = r.attributes();
            bool xok = false, yok = false, fok = false, hok = false;
            VenueRig::FillHead h;
            h.x = a.value(QStringLiteral("X")).toInt(&xok);
            h.y = a.value(QStringLiteral("Y")).toInt(&yok);
            h.fixture = a.value(QStringLiteral("Fixture")).toUInt(&fok);
            h.head = r.readElementText().toInt(&hok);
            if (xok && yok && fok && hok)
                g.heads << h;
        }
        else
        {
            r.skipCurrentElement();
        }
    }
    return g;
}

void readOutputs(QXmlStreamReader &r, VenueImport::Source &out)
{
    while (r.readNextStartElement())
    {
        if (r.name() != KXMLQLCUniverse)
        {
            r.skipCurrentElement();
            continue;
        }
        bool ok = false;
        const quint32 id = r.attributes().value(QStringLiteral("ID")).toUInt(&ok);
        QList<Universe::OfflinePatch> list;
        while (r.readNextStartElement())
        {
            if (r.name() == KXMLQLCUniverseOutputPatch)
                list << VenueRig::readOutput(r);
            else
                r.skipCurrentElement();
        }
        if (ok && !list.isEmpty())
            out.outputs.insert(id, list);
    }
}

QString rangeText(quint32 universe, quint32 first, quint32 last)
{
    if (first == last)
        return VenueImport::tr("U%1 %2").arg(universe + 1).arg(first + 1);
    return VenueImport::tr("U%1 %2\342\200\223%3").arg(universe + 1).arg(first + 1).arg(last + 1);
}
} // namespace

/*****************************************************************************
 * Reading the other show
 *****************************************************************************/

QString VenueImport::Source::fileName() const
{
    return QFileInfo(file).fileName();
}

QList<quint32> VenueImport::Source::universes() const
{
    QList<quint32> out;
    for (const VenueRig::HouseFixture &hf : fixtures)
        if (!out.contains(hf.universe))
            out << hf.universe;
    std::sort(out.begin(), out.end());
    return out;
}

QList<Universe::OfflinePatch> VenueImport::Source::fixtureOutputs() const
{
    QList<Universe::OfflinePatch> out;
    for (quint32 u : universes())
        out += outputs.value(u);
    return out;
}

bool VenueImport::read(const QString &path, Source &out, QString *why)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (why != nullptr)
            *why = tr("%1 can't be opened: %2").arg(QFileInfo(path).fileName(), f.errorString());
        return false;
    }
    const bool ok = readXml(f.readAll(), out, why);
    out.file = path;
    return ok;
}

bool VenueImport::readXml(const QByteArray &xml, Source &out, QString *why)
{
    out = Source();
    QXmlStreamReader r(xml);
    if (!r.readNextStartElement() || r.name() != QLatin1String("Workspace"))
    {
        if (why != nullptr)
            *why = tr("This is not a show file (a .qlcc or .qxw).");
        return false;
    }
    while (r.readNextStartElement())
    {
        if (r.name() != QLatin1String("Engine"))
        {
            r.skipCurrentElement();
            continue;
        }
        while (r.readNextStartElement())
        {
            const QStringView n = r.name();
            if (n == KXMLFixture)
            {
                VenueRig::HouseFixture hf;
                hf.xml = copyElement(r);
                describe(hf);
                if (hf.id != UINT_MAX)
                    out.fixtures << hf;
            }
            else if (n == KXMLQLCFixtureGroup)
            {
                const Group g = readGroup(r);
                if (!g.name.isEmpty())
                    out.groups << g;
            }
            else if (n == KXMLQLCMonitorProperties)
            {
                out.monitorXml = copyElement(r);
            }
            else if (n == KXMLIOMap)
            {
                readOutputs(r, out);
            }
            else
            {
                r.skipCurrentElement();
            }
        }
    }
    if (r.hasError())
    {
        if (why != nullptr)
            *why = tr("The show file can't be read: %1 (line %2).").arg(r.errorString()).arg(r.lineNumber());
        return false;
    }
    if (out.fixtures.isEmpty())
    {
        if (why != nullptr)
            *why = tr("That show has no fixtures: there is no rig to take from it.");
        return false;
    }
    return true;
}

/*****************************************************************************
 * Groups
 *****************************************************************************/

QString VenueImport::simplified(const QString &name)
{
    static const QRegularExpression andWord(QStringLiteral("\\band\\b"));
    static const QRegularExpression other(QStringLiteral("[^\\p{L}\\p{N}]"));
    QString s = name.toLower();
    s.replace(QLatin1Char('&'), QLatin1Char(' '));
    s.replace(andWord, QStringLiteral(" "));
    s.remove(other);
    return s;
}

VenueImport::Match VenueImport::suggest(const QString &name, const Doc *doc)
{
    Match m;
    if (doc == nullptr)
        return m;
    QList<FixtureGroup *> groups = doc->fixtureGroups();
    std::sort(groups.begin(), groups.end(),
              [](const FixtureGroup *a, const FixtureGroup *b) { return a->id() < b->id(); });
    const QString trimmed = name.trimmed();
    for (const FixtureGroup *g : groups)
        if (g->name().trimmed().compare(trimmed, Qt::CaseInsensitive) == 0)
        {
            m.group = g->id();
            m.kind = SameName;
            return m;
        }
    const QString simple = simplified(name);
    if (simple.isEmpty())
        return m;
    for (const FixtureGroup *g : groups)
        if (simplified(g->name()) == simple)
        {
            m.group = g->id();
            m.kind = SimilarName;
            return m;
        }
    return m;
}

QList<VenueImport::Match> VenueImport::suggest(const Source &src, const Doc *doc)
{
    QList<Match> out;
    for (const Group &g : src.groups)
        out << suggest(g.name, doc);
    return out;
}

int VenueImport::unconfirmed(const QList<Match> &matches)
{
    int n = 0;
    for (const Match &m : matches)
        if (m.group != UINT_MAX && !m.confirmed)
            ++n;
    return n;
}

/*****************************************************************************
 * Building the venue
 *****************************************************************************/

quint32 VenueImport::suggestUniverse(const Doc *doc, const QSet<quint32> &venueFixtures)
{
    if (doc == nullptr)
        return UINT_MAX;
    const QList<Universe *> unis = doc->inputOutputMap()->universes();
    QSet<quint32> used, tourUsed;
    const QHash<quint32, RigOwner::Owner> owners = RigOwner::effectiveAll(doc);
    for (const Fixture *fx : doc->fixtures())
    {
        if (venueFixtures.contains(fx->id()))
            continue;
        used.insert(fx->universe());
        if (owners.value(fx->id(), RigOwner::Tour) != RigOwner::House)
            tourUsed.insert(fx->universe());
    }
    for (int i = 0; i < unis.size(); ++i)
        if (unis.at(i)->slotOwner() == RigOwner::House && !tourUsed.contains(quint32(i)))
            return quint32(i);
    for (int i = 0; i < unis.size(); ++i)
        if (!used.contains(quint32(i)))
            return quint32(i);
    return UINT_MAX;
}

VenueImport::Result VenueImport::build(const Source &src, const Doc *doc, const Options &opt,
                                       const QSet<quint32> &reserved)
{
    Result res;
    if (doc == nullptr)
        return res;
    VenueRig::Data &d = res.data;

    // New ids, after the show's and every venue's: none is ever renumbered.
    quint32 next = 0;
    for (const Fixture *fx : doc->fixtures())
        next = qMax(next, fx->id() + 1);
    for (quint32 r : reserved)
        if (r != UINT_MAX)
            next = qMax(next, r + 1);
    QMap<quint32, quint32> remap;

    // The fixtures: House, on the room's universe, each address as it was.
    QMap<quint32, QList<QString>> byAddress;
    QSet<QString> missing;
    bool any = false;
    for (const VenueRig::HouseFixture &from : src.fixtures)
    {
        while (doc->fixture(next) != nullptr || reserved.contains(next) || next == Fixture::invalidId())
            ++next;
        const quint32 id = next++;
        remap.insert(from.id, id);
        VenueRig::HouseFixture hf;
        hf.xml = rewriteFixture(from.xml, id, opt.universe);
        describe(hf);
        d.fixtures << hf;
        ++res.fixtures;

        const QPair<QString, QString> mm = makeModel(from.xml);
        if (!mm.second.isEmpty() && mm.first != KXMLFixtureGeneric
            && doc->fixtureDefCache()->fixtureDef(mm.first, mm.second) == nullptr)
            missing.insert(QStringLiteral("%1 %2").arg(mm.first, mm.second));

        if (hf.patched)
        {
            const quint32 last = hf.address + qMax(1u, hf.channels) - 1;
            res.firstAddress = any ? qMin(res.firstAddress, hf.address) : hf.address;
            res.lastAddress = any ? qMax(res.lastAddress, last) : last;
            any = true;
        }
    }
    for (const QString &m : missing)
        res.warnings << tr("%1: no fixture definition on this computer. Its fixtures are kept in the "
                           "venue and come onto the rig once the definition is installed.").arg(m);
    const QList<quint32> unis = src.universes();
    if (unis.size() > 1)
    {
        QStringList names;
        for (quint32 u : unis)
            names << tr("U%1").arg(u + 1);
        res.warnings << tr("%1 has fixtures in %2 universes (%3); the venue has one, so they all go on U%4 "
                           "at their own addresses. Where two meet, the clash check lists them and the "
                           "second goes in unpatched.")
                            .arg(src.fileName().isEmpty() ? tr("The other show") : src.fileName())
                            .arg(unis.size())
                            .arg(names.join(QStringLiteral(", ")))
                            .arg(opt.universe + 1);
    }
    if (opt.universe >= doc->inputOutputMap()->universesCount())
        res.warnings << tr("The show has no universe %1: add it on the Connections tab, or the house rig "
                           "goes nowhere.").arg(opt.universe + 1);

    /* The plot: each where it was in the other show. That show's structure
       doesn't come (a house fixture hangs on this show's), so a fixture on
       one of its trusses is placed free where it hung, aim and all; its 2D
       layer and plot group were that show's. */
    if (!src.monitorXml.isEmpty())
    {
        MonitorProperties theirs;
        QXmlStreamReader r(src.monitorXml);
        if (r.readNextStartElement())
            theirs.loadXML(r, doc);
        MonitorProperties house;
        int unhung = 0;
        for (auto it = remap.constBegin(); it != remap.constEnd(); ++it)
        {
            const quint32 sid = it.key(), nid = it.value();
            const bool item = theirs.containsFixture(sid);
            const bool rig = theirs.hasFixtureRigProps(sid);
            FixtureRigProps rp = theirs.fixtureRigProps(sid);
            const bool mounted = rig && (rp.trussId != Truss::invalidId() || rp.onPipe() || rp.onTower()
                                         || rp.onDeck() || rp.onRiser() || rp.onHanger() || rp.isHeld());
            if (!item && !mounted)
                continue;
            FixturePreviewItem fp = item ? theirs.fixtureProperties(sid) : FixturePreviewItem();
            const QVector3D world = theirs.fixtureRigPosition(sid);    // metres
            fp.m_baseItem.m_position = QVector3D(world.x() * 1000.0f, world.y() * 1000.0f, world.z());
            fp.m_baseItem.m_layerId = 0;
            fp.m_baseItem.m_groupId = 0;
            house.setFixtureProperties(nid, fp);
            if (rig)
            {
                if (mounted)
                    ++unhung;
                rp.clearMounts();
                rp.heldBy = UINT_MAX;
                house.setFixtureRigProps(nid, rp);
            }
        }
        if (!house.fixtureItemsID().isEmpty() || !house.fixtureRigPropsMap().isEmpty())
        {
            QXmlStreamWriter w(&d.monitorXml);
            house.saveXML(&w, doc);
        }
        if (unhung > 0)
            res.warnings << (unhung == 1
                ? tr("1 fixture hung on the other show's structure is placed where it hung, free of any "
                     "truss: this show's structure is the one it hangs on.")
                : tr("%1 fixtures hung on the other show's structure are placed where they hung, free of "
                     "any truss: this show's structure is the one they hang on.").arg(unhung));
    }

    // The groups: a confirmed match fills the show's group; the rest wait.
    for (int i = 0; i < src.groups.size(); ++i)
    {
        const Group &g = src.groups.at(i);
        const Match m = opt.matches.value(i);
        VenueRig::Fill f;
        for (const VenueRig::FillHead &h : g.heads)
        {
            if (!remap.contains(h.fixture))
                continue;
            VenueRig::FillHead nh = h;
            nh.fixture = remap.value(h.fixture);
            f.heads << nh;
        }
        const FixtureGroup *grp = m.confirmed && m.group != UINT_MAX ? doc->fixtureGroup(m.group) : nullptr;
        if (grp != nullptr)
        {
            f.group = grp->id();
            f.groupName = grp->name();
            d.fills << f;
            ++res.filled;
        }
        else
        {
            f.groupName = g.name;
            d.room.groups << f;
            ++res.held;
        }
    }

    // The room: its universe, where it came from, and its output.
    d.room.universe = opt.universe;
    d.room.from = src.fileName();
    d.room.imported = src.fixtureOutputs();
    d.patch.insert(opt.universe, opt.output);
    return res;
}

QString VenueImport::summary(const Result &r, quint32 universe)
{
    if (r.fixtures == 0)
        return tr("No fixtures.");
    const QString where = rangeText(universe, r.firstAddress, r.lastAddress);
    return r.fixtures == 1 ? tr("1 fixture on %1").arg(where) : tr("%1 fixtures on %2").arg(r.fixtures).arg(where);
}
