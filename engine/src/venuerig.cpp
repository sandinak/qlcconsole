/*
  Q Light Controller Plus
  venuerig.cpp

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
#include <algorithm>

#include "monitorproperties.h"
#include "inputoutputmap.h"
#include "fixturegroup.h"
#include "grouphead.h"
#include "qlcfile.h"
#include "venuerig.h"
#include "rigowner.h"
#include "fixture.h"
#include "doc.h"

namespace
{
const QString kHouse = QStringLiteral("House");
const QString kFill = QStringLiteral("Fill");
const QString kHead = QStringLiteral("Head");
const QString kPatch = QStringLiteral("Patch");
const QString kNotOurs = QStringLiteral("NotOurs");

/* The element the reader is at, whole, as its own little document; the
   reader is left at its end element. */
QByteArray captureElement(QXmlStreamReader &r)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    int depth = 0;
    while (!r.atEnd())
    {
        if (r.isStartElement())
            ++depth;
        if (!(r.isCharacters() && r.isWhitespace()))
            w.writeCurrentToken(r);
        if (r.isEndElement() && --depth == 0)
            break;
        r.readNext();
    }
    return out;
}

/* captureElement()'s bytes written into @p w where it is. */
void writeElement(QXmlStreamWriter &w, const QByteArray &xml)
{
    QXmlStreamReader r(xml);
    while (!r.atEnd())
    {
        r.readNext();
        if (r.isStartDocument() || r.isEndDocument() || (r.isCharacters() && r.isWhitespace()))
            continue;
        if (r.tokenType() == QXmlStreamReader::Invalid)
            break;
        w.writeCurrentToken(r);
    }
}

/* What the clash check and the venue page read of a <Fixture>. */
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

QString paramsKey(const QMap<QString, QVariant> &params)
{
    QStringList out;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        out << it.key() + QLatin1Char('=') + it.value().toString();
    return out.join(QLatin1Char('\n'));
}

QString range(quint32 universe, quint32 address, quint32 channels)
{
    if (channels <= 1)
        return QObject::tr("U%1 %2").arg(universe + 1).arg(address + 1);
    return QObject::tr("U%1 %2\342\200\223%3").arg(universe + 1).arg(address + 1).arg(address + channels);
}
}

VenueRig::VenueRig(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    if (m_doc == nullptr)
        return;
    /* While a venue is applied, owners are followed: a fixture set to House
       moves into the venue, one set back to Tour into the show. */
    connect(m_doc, &Doc::fixtureAdded, this, &VenueRig::sync);
    connect(m_doc, &Doc::fixtureRemoved, this, &VenueRig::sync);
    connect(m_doc, &Doc::fixtureChanged, this, &VenueRig::sync);
    connect(m_doc, &Doc::fixtureGroupAdded, this, &VenueRig::sync);
    connect(m_doc, &Doc::fixtureGroupRemoved, this, &VenueRig::sync);
    connect(m_doc, &Doc::fixtureGroupChanged, this, &VenueRig::sync);
    if (m_doc->monitorProperties() != nullptr)
        connect(m_doc->monitorProperties(), &MonitorProperties::rigPropsChanged, this, [this](quint32 fid) {
            if (m_applied && !m_writing && m_ids.contains(fid))
                emit rigChanged(true);
        });
}

VenueRig::~VenueRig()
{
}

QSet<quint32> VenueRig::Data::fixtureIds() const
{
    QSet<quint32> out;
    for (const HouseFixture &hf : fixtures)
        out.insert(hf.id);
    return out;
}

/*****************************************************************************
 * The sidecar
 *****************************************************************************/

QByteArray VenueRig::keepElement(QXmlStreamReader &r)
{
    return captureElement(r);
}

void VenueRig::writeKept(QXmlStreamWriter &w, const QList<QByteArray> &kept)
{
    for (const QByteArray &xml : kept)
        writeElement(w, xml);
}

void VenueRig::writeXml(QXmlStreamWriter &w, const Data &d)
{
    if (!d.fixtures.isEmpty() || !d.fills.isEmpty() || !d.houseUnknown.isEmpty())
    {
        w.writeStartElement(kHouse);
        for (const HouseFixture &hf : d.fixtures)
            writeElement(w, hf.xml);
        if (!d.monitorXml.isEmpty())
            writeElement(w, d.monitorXml);
        for (const Fill &f : d.fills)
        {
            w.writeStartElement(kFill);
            w.writeAttribute(QStringLiteral("Group"), QString::number(f.group));
            w.writeAttribute(QStringLiteral("Name"), f.groupName);
            for (const FillHead &h : f.heads)
            {
                w.writeStartElement(kHead);
                w.writeAttribute(QStringLiteral("Fixture"), QString::number(h.fixture));
                w.writeAttribute(QStringLiteral("Head"), QString::number(h.head));
                w.writeAttribute(QStringLiteral("X"), QString::number(h.x));
                w.writeAttribute(QStringLiteral("Y"), QString::number(h.y));
                w.writeEndElement();
            }
            w.writeEndElement();
        }
        writeKept(w, d.houseUnknown);
        w.writeEndElement();
    }
    for (auto it = d.patch.constBegin(); it != d.patch.constEnd(); ++it)
    {
        w.writeStartElement(kPatch);
        w.writeAttribute(QStringLiteral("Universe"), QString::number(it.key()));
        for (const Universe::OfflinePatch &p : it.value())
        {
            w.writeStartElement(KXMLQLCUniverseOutputPatch);
            w.writeAttribute(KXMLQLCUniversePlugin, p.plugin);
            w.writeAttribute(KXMLQLCUniverseLineUID, p.uid);
            w.writeAttribute(KXMLQLCUniverseLine, QString::number(p.line));
            if (!p.network.isEmpty())
                w.writeAttribute(KXMLQLCUniverseNetwork, p.network);
            if (!p.params.isEmpty())
            {
                w.writeStartElement(KXMLQLCUniversePluginParameters);
                for (auto pit = p.params.constBegin(); pit != p.params.constEnd(); ++pit)
                    w.writeAttribute(pit.key(), pit.value().toString());
                w.writeEndElement();
            }
            w.writeEndElement();
        }
        w.writeEndElement();
    }
    for (const NotOurs &n : d.notOurs)
    {
        w.writeStartElement(kNotOurs);
        w.writeAttribute(QStringLiteral("Universe"), QString::number(n.universe + 1));
        if (!n.whole())
        {
            w.writeAttribute(QStringLiteral("From"), QString::number(n.from + 1));
            w.writeAttribute(QStringLiteral("To"), QString::number(n.to + 1));
        }
        w.writeEndElement();
    }
}

bool VenueRig::readXml(QXmlStreamReader &r, Data &d)
{
    if (r.name() == kHouse)
    {
        while (r.readNextStartElement())
        {
            if (r.name() == KXMLFixture)
            {
                HouseFixture hf;
                hf.xml = captureElement(r);
                describe(hf);
                if (hf.id != UINT_MAX)
                    d.fixtures << hf;
            }
            else if (r.name() == KXMLQLCMonitorProperties)
            {
                d.monitorXml = captureElement(r);
            }
            else if (r.name() == kFill)
            {
                Fill f;
                f.group = r.attributes().value(QStringLiteral("Group")).toUInt();
                f.groupName = r.attributes().value(QStringLiteral("Name")).toString();
                while (r.readNextStartElement())
                {
                    if (r.name() == kHead)
                    {
                        const QXmlStreamAttributes a = r.attributes();
                        FillHead h;
                        h.fixture = a.value(QStringLiteral("Fixture")).toUInt();
                        h.head = a.value(QStringLiteral("Head")).toInt();
                        h.x = a.value(QStringLiteral("X")).toInt();
                        h.y = a.value(QStringLiteral("Y")).toInt();
                        f.heads << h;
                    }
                    r.skipCurrentElement();
                }
                d.fills << f;
            }
            else
            {
                d.houseUnknown << captureElement(r);     // a newer build's: kept
            }
        }
        return true;
    }
    if (r.name() == kPatch)
    {
        bool ok = false;
        const quint32 uni = r.attributes().value(QStringLiteral("Universe")).toUInt(&ok);
        QList<Universe::OfflinePatch> list;
        while (r.readNextStartElement())
        {
            if (r.name() != KXMLQLCUniverseOutputPatch)
            {
                r.skipCurrentElement();
                continue;
            }
            const QXmlStreamAttributes a = r.attributes();
            Universe::OfflinePatch p;
            p.tag = Universe::OutputPatchTag;
            p.plugin = a.value(KXMLQLCUniversePlugin).toString();
            p.uid = a.value(KXMLQLCUniverseLineUID).toString();
            p.line = a.hasAttribute(KXMLQLCUniverseLine) ? a.value(KXMLQLCUniverseLine).toUInt() : UINT_MAX;
            p.network = a.value(KXMLQLCUniverseNetwork).toString();
            p.profile = QString();
            while (r.readNextStartElement())
            {
                if (r.name() == KXMLQLCUniversePluginParameters)
                    for (const QXmlStreamAttribute &pa : r.attributes())
                        p.params.insert(pa.name().toString(), pa.value().toString());
                r.skipCurrentElement();
            }
            list << p;
        }
        if (ok)
            d.patch.insert(uni, list);
        return true;
    }
    if (r.name() == kNotOurs)
    {
        const QXmlStreamAttributes a = r.attributes();
        bool ok = false;
        NotOurs n;
        const int u = a.value(QStringLiteral("Universe")).toInt(&ok);
        if (ok && u >= 1)
        {
            n.universe = quint32(u - 1);
            if (a.hasAttribute(QStringLiteral("From")))
            {
                n.from = qBound(0, a.value(QStringLiteral("From")).toInt() - 1, 511);
                n.to = qBound(n.from, a.value(QStringLiteral("To")).toInt() - 1, 511);
            }
            d.notOurs << n;
        }
        r.skipCurrentElement();
        return true;
    }
    return false;
}

/*****************************************************************************
 * On and off the rig
 *****************************************************************************/

void VenueRig::keepUnmodified(bool was)
{
    // Putting a venue on, or taking it off, is not an edit to the show.
    if (!was && m_doc->isModified())
        m_doc->resetModified();
}

quint32 VenueRig::freeId(const QSet<quint32> &taken) const
{
    quint32 id = 0;
    for (const Fixture *fx : m_doc->fixtures())
        id = qMax(id, fx->id() + 1);
    for (quint32 r : m_reserved)
        if (r != UINT_MAX)
            id = qMax(id, r + 1);
    for (quint32 t : taken)
        if (t != UINT_MAX)
            id = qMax(id, t + 1);
    while (m_doc->fixture(id) != nullptr || taken.contains(id) || id == Fixture::invalidId())
        ++id;
    return id;
}

bool VenueRig::clashesOnPut(const Fixture *fx, const Data &d) const
{
    if (fx == nullptr || !fx->isPatched())
        return false;
    const quint32 from = fx->address();
    const quint32 to = fx->address() + qMax(1u, fx->channels()) - 1;
    for (const NotOurs &n : d.notOurs)
    {
        if (n.universe != fx->universe())
            continue;
        if (n.whole() || (from <= quint32(n.to) && to >= quint32(n.from)))
            return true;
    }
    const QList<Universe *> unis = m_doc->inputOutputMap()->universes();
    bool anyHouseSlot = false;
    for (const Universe *u : unis)
        anyHouseSlot = anyHouseSlot || u->slotOwner() == RigOwner::House;
    return anyHouseSlot && fx->universe() < quint32(unis.size())
           && unis.at(int(fx->universe()))->slotOwner() != RigOwner::House;
}

void VenueRig::putFixture(HouseFixture &hf, QMap<quint32, quint32> &remap, const Data &d)
{
    const quint32 id = remap.value(hf.id, hf.id);
    QXmlStreamReader r(hf.xml);
    if (!r.readNextStartElement())
    {
        m_ids.remove(id);
        return;
    }
    Fixture *fx = new Fixture(m_doc);
    if (!fx->loadXML(r, m_doc, m_doc->fixtureDefCache()))
    {
        delete fx;
        m_ids.remove(id);
        m_unloaded.insert(hf.id);
        return;
    }
    /* Any clash the check lists for it -- channels the venue keeps, or a
       Tour slot -- puts it in unpatched, as an overlap does (VENUES.md
       section 6, board Q138): the clash check still lists it, at the
       address it asked for. */
    if (clashesOnPut(fx, d) || !m_doc->addFixture(fx, id, fx->crossUniverse()))
    {
        /* It wants channels a show fixture holds (or the room's, or the
           tour's slot): in, unpatched, keeping the address it asked for --
           as a fixture clashing on load does -- so it never drives the
           tour's channels (VENUES.md section 6). */
        fx->setPatched(false);
        fx->setAddressClash(true);
        if (!m_doc->addFixture(fx, id, fx->crossUniverse()))
        {
            delete fx;
            m_ids.remove(id);
            m_unloaded.insert(hf.id);
            return;
        }
    }
    hf.id = id;
}

void VenueRig::apply(Data &d)
{
    if (m_doc == nullptr)
        return;
    const bool was = m_doc->isModified();
    m_writing = true;
    m_applied = true;
    m_ids.clear();
    m_unloaded.clear();
    MonitorProperties *props = m_doc->monitorProperties();

    /* Ids first, so everything that hears of a fixture being added already
       knows it is the venue's. A show fixture holding one since: the house
       fixture takes a free id, and its plot place and heads follow. */
    QMap<quint32, quint32> remap;
    QSet<quint32> taken;
    for (const HouseFixture &hf : d.fixtures)
        taken.insert(hf.id);
    for (const HouseFixture &hf : d.fixtures)
    {
        quint32 id = hf.id;
        if (m_doc->fixture(id) != nullptr || m_ids.contains(id))
        {
            id = freeId(taken + m_ids);
            taken.insert(id);
        }
        remap.insert(hf.id, id);
        m_ids.insert(id);
    }

    // Their places, before they arrive, so a view drawing them finds them.
    MonitorProperties house;
    if (!d.monitorXml.isEmpty() && props != nullptr)
    {
        QXmlStreamReader r(d.monitorXml);
        if (r.readNextStartElement())
            house.loadXML(r, m_doc);
        for (auto it = remap.constBegin(); it != remap.constEnd(); ++it)
        {
            if (house.containsFixture(it.key()))
                props->setFixtureProperties(it.value(), house.fixtureProperties(it.key()));
            if (house.fixtureRigPropsMap().contains(it.key()))
                props->setFixtureRigProps(it.value(), house.fixtureRigProps(it.key()));
        }
    }
    for (HouseFixture &hf : d.fixtures)
        putFixture(hf, remap, d);

    // The fills: its heads in the show's groups.
    for (Fill &f : d.fills)
    {
        FixtureGroup *grp = m_doc->fixtureGroup(f.group);
        if (grp == nullptr)
            for (FixtureGroup *g : m_doc->fixtureGroups())
                if (g->name() == f.groupName)
                {
                    grp = g;
                    f.group = g->id();
                    break;
                }
        if (grp == nullptr)
            continue;
        bool any = false;
        for (FillHead &h : f.heads)
        {
            h.fixture = remap.value(h.fixture, h.fixture);
            if (!m_ids.contains(h.fixture) || m_doc->fixture(h.fixture) == nullptr)
                continue;
            QLCPoint pt(h.x, h.y);
            const GroupHead gh(h.fixture, h.head);
            if (grp->head(pt).isValid())
                pt = QLCPoint();    // the show put a head there since: the next free cell
            else if (h.x >= grp->size().width() || h.y >= grp->size().height())
                grp->setSize(QSize(qMax(grp->size().width(), h.x + 1), qMax(grp->size().height(), h.y + 1)));
            any = grp->assignHead(pt, gh) || any;
        }
        if (any)
            grp->notifyChanged();   // a scene playing on the group follows
    }

    // The room's network: the show's own patch held for the show file.
    InputOutputMap *iom = m_doc->inputOutputMap();
    const QList<Universe *> unis = iom->universes();
    for (Universe *u : unis)
        if (!u->hasShowOutputBase())
            u->setShowOutputBase(u->outputPatchDescriptions());
    for (auto it = d.patch.constBegin(); it != d.patch.constEnd(); ++it)
        if (it.key() < quint32(unis.size()) && !samePatch(unis.at(int(it.key()))->outputPatchDescriptions(), it.value()))
            iom->replaceOutputPatches(it.key(), it.value());
    applyNotOurs(d.notOurs);
    watchUniverses();

    m_writing = false;
    keepUnmodified(was);
}

void VenueRig::unapply()
{
    if (m_doc == nullptr || !m_applied)
        return;
    const bool was = m_doc->isModified();
    m_writing = true;
    QList<quint32> ids = m_ids.values();
    std::sort(ids.begin(), ids.end());
    for (quint32 id : ids)
        m_doc->deleteFixture(id);
    m_ids.clear();
    m_unloaded.clear();

    InputOutputMap *iom = m_doc->inputOutputMap();
    const QList<Universe *> unis = iom->universes();
    for (int i = 0; i < unis.size(); ++i)
    {
        Universe *u = unis.at(i);
        if (!u->hasShowOutputBase())
            continue;
        const QList<Universe::OfflinePatch> base = u->showOutputBase();
        u->clearShowOutputBase();
        if (!samePatch(u->outputPatchDescriptions(), base))
            iom->replaceOutputPatches(quint32(i), base);
    }
    applyNotOurs(QList<NotOurs>());

    m_applied = false;
    m_writing = false;
    keepUnmodified(was);
}

bool VenueRig::capture(Data &d) const
{
    if (m_doc == nullptr)
        return false;
    Data n;
    QList<quint32> ids = m_ids.values();
    std::sort(ids.begin(), ids.end());
    MonitorProperties house;
    MonitorProperties *props = m_doc->monitorProperties();
    for (quint32 id : ids)
    {
        const Fixture *fx = m_doc->fixture(id);
        if (fx == nullptr)
            continue;
        HouseFixture hf;
        hf.id = id;
        hf.name = fx->name();
        hf.universe = fx->universe();
        hf.address = fx->address();
        hf.channels = fx->channels();
        hf.patched = fx->isPatched() || fx->addressClash();
        QXmlStreamWriter w(&hf.xml);
        fx->saveXML(&w);
        n.fixtures << hf;
        if (props != nullptr)
        {
            if (props->containsFixture(id))
                house.setFixtureProperties(id, props->fixtureProperties(id));
            if (props->fixtureRigPropsMap().contains(id))
                house.setFixtureRigProps(id, props->fixtureRigProps(id));
        }
    }
    /* A house fixture that could not be put in is kept as the venue had
       it -- its entry and its plot data -- never lost by a capture. */
    if (!m_unloaded.isEmpty())
    {
        MonitorProperties before;
        if (!d.monitorXml.isEmpty())
        {
            QXmlStreamReader r(d.monitorXml);
            if (r.readNextStartElement())
                before.loadXML(r, m_doc);
        }
        for (const HouseFixture &hf : d.fixtures)
        {
            if (!m_unloaded.contains(hf.id))
                continue;
            n.fixtures << hf;
            if (before.containsFixture(hf.id))
                house.setFixtureProperties(hf.id, before.fixtureProperties(hf.id));
            if (before.fixtureRigPropsMap().contains(hf.id))
                house.setFixtureRigProps(hf.id, before.fixtureRigProps(hf.id));
        }
    }
    if (!n.fixtures.isEmpty())
    {
        QXmlStreamWriter w(&n.monitorXml);
        house.saveXML(&w, m_doc);
    }
    QSet<quint32> seen;
    for (const FixtureGroup *grp : m_doc->fixtureGroups())
    {
        Fill f;
        f.group = grp->id();
        f.groupName = grp->name();
        const QMap<QLCPoint, GroupHead> heads = grp->headsMap();
        for (auto it = heads.constBegin(); it != heads.constEnd(); ++it)
        {
            if (!m_ids.contains(it.value().fxi))
                continue;
            FillHead h;
            h.fixture = it.value().fxi;
            h.head = it.value().head;
            h.x = it.key().x();
            h.y = it.key().y();
            f.heads << h;
        }
        seen.insert(grp->id());
        if (!f.heads.isEmpty())
            n.fills << f;
    }
    // A fill for a group the show no longer has is kept: it may come back.
    for (const Fill &f : d.fills)
        if (!seen.contains(f.group) && m_doc->fixtureGroup(f.group) == nullptr)
        {
            bool byName = false;
            for (const FixtureGroup *g : m_doc->fixtureGroups())
                byName = byName || g->name() == f.groupName;
            if (!byName)
                n.fills << f;
        }

    bool changed = n.fixtures.size() != d.fixtures.size() || n.monitorXml != d.monitorXml
                   || n.fills.size() != d.fills.size();
    for (int i = 0; !changed && i < n.fixtures.size(); ++i)
        changed = n.fixtures.at(i).xml != d.fixtures.at(i).xml;
    for (int i = 0; !changed && i < n.fills.size(); ++i)
    {
        const Fill &a = n.fills.at(i), &b = d.fills.at(i);
        changed = a.group != b.group || a.groupName != b.groupName || a.heads.size() != b.heads.size();
        for (int k = 0; !changed && k < a.heads.size(); ++k)
            changed = a.heads.at(k).fixture != b.heads.at(k).fixture || a.heads.at(k).head != b.heads.at(k).head
                      || a.heads.at(k).x != b.heads.at(k).x || a.heads.at(k).y != b.heads.at(k).y;
    }
    d.fixtures = n.fixtures;
    d.monitorXml = n.monitorXml;
    d.fills = n.fills;
    return changed;
}

void VenueRig::forget()
{
    m_ids.clear();
    m_unloaded.clear();
    m_applied = false;
    if (m_doc == nullptr)
        return;
    for (Universe *u : m_doc->inputOutputMap()->universes())
    {
        u->clearShowOutputBase();
        u->setNotOurs(false, QList<QPair<int, int>>());
    }
}

void VenueRig::sync()
{
    if (!m_applied || m_writing || m_doc == nullptr)
        return;
    const QHash<quint32, RigOwner::Owner> owners = RigOwner::effectiveAll(m_doc);
    for (quint32 id : m_ids.values())
        if (m_doc->fixture(id) == nullptr)
            m_ids.remove(id);
    for (const Fixture *fx : m_doc->fixtures())
    {
        const bool house = owners.value(fx->id()) == RigOwner::House;
        if (house && !m_ids.contains(fx->id()))
        {
            // Into the venue: its calibration is the room's now.
            m_ids.insert(fx->id());
            if (m_doc->monitorProperties() != nullptr)
                m_doc->monitorProperties()->clearCalibrationBase(fx->id());
        }
        else if (!house && m_ids.contains(fx->id()))
        {
            // Into the show ("this one comes on tour now").
            m_ids.remove(fx->id());
        }
    }
    emit rigChanged(false);
}

QList<quint32> VenueRig::showHouseFixtures() const
{
    QList<quint32> out;
    if (m_doc == nullptr)
        return out;
    const QHash<quint32, RigOwner::Owner> owners = RigOwner::effectiveAll(m_doc);
    for (const Fixture *fx : m_doc->fixtures())
        if (owners.value(fx->id()) == RigOwner::House && !m_ids.contains(fx->id()))
            out << fx->id();
    return out;
}

int VenueRig::moveShowHouseIn()
{
    if (!m_applied)
        return 0;
    const QList<quint32> ids = showHouseFixtures();
    for (quint32 id : ids)
    {
        m_ids.insert(id);
        if (m_doc->monitorProperties() != nullptr)
            m_doc->monitorProperties()->clearCalibrationBase(id);
    }
    if (!ids.isEmpty())
    {
        m_doc->setModified();   // the show file loses them
        emit rigChanged(false);
    }
    return ids.size();
}

/*****************************************************************************
 * The venue's patch
 *****************************************************************************/

void VenueRig::watchUniverses()
{
    for (Universe *u : m_doc->inputOutputMap()->universes())
    {
        if (m_watched.contains(u))
            continue;
        m_watched.insert(u);
        auto edited = [this]() {
            if (m_applied && !m_writing)
                emit patchEdited();
        };
        connect(u, &Universe::outputPatchChanged, this, edited);
        connect(u, &Universe::outputPatchesCountChanged, this, edited);
        connect(u, &QObject::destroyed, this, [this, u]() { m_watched.remove(u); });
    }
}

QMap<quint32, QList<Universe::OfflinePatch>> VenueRig::currentPatch() const
{
    QMap<quint32, QList<Universe::OfflinePatch>> out;
    const QList<Universe *> unis = m_doc->inputOutputMap()->universes();
    for (int i = 0; i < unis.size(); ++i)
        out.insert(quint32(i), unis.at(i)->outputPatchDescriptions());
    return out;
}

QMap<quint32, QList<Universe::OfflinePatch>> VenueRig::showPatch() const
{
    QMap<quint32, QList<Universe::OfflinePatch>> out;
    const QList<Universe *> unis = m_doc->inputOutputMap()->universes();
    for (int i = 0; i < unis.size(); ++i)
        out.insert(quint32(i), unis.at(i)->hasShowOutputBase() ? unis.at(i)->showOutputBase()
                                                                : unis.at(i)->outputPatchDescriptions());
    return out;
}

bool VenueRig::patchChanged(const Data &d) const
{
    const QMap<quint32, QList<Universe::OfflinePatch>> now = currentPatch();
    const QMap<quint32, QList<Universe::OfflinePatch>> show = showPatch();
    for (auto it = now.constBegin(); it != now.constEnd(); ++it)
    {
        const QList<Universe::OfflinePatch> expected = d.patch.contains(it.key()) ? d.patch.value(it.key())
                                                                                  : show.value(it.key());
        if (!samePatch(it.value(), expected))
            return true;
    }
    return false;
}

void VenueRig::restoreShowPatch()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    const QList<Universe *> unis = iom->universes();
    const bool was = m_writing;
    m_writing = true;
    for (int i = 0; i < unis.size(); ++i)
    {
        Universe *u = unis.at(i);
        if (u->hasShowOutputBase() && !samePatch(u->outputPatchDescriptions(), u->showOutputBase()))
            iom->replaceOutputPatches(quint32(i), u->showOutputBase());
    }
    m_writing = was;
}

bool VenueRig::samePatch(const QList<Universe::OfflinePatch> &a, const QList<Universe::OfflinePatch> &b)
{
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i)
    {
        const Universe::OfflinePatch &x = a.at(i), &y = b.at(i);
        if (x.plugin != y.plugin || x.uid != y.uid || x.line != y.line || x.network != y.network
            || paramsKey(x.params) != paramsKey(y.params))
            return false;
    }
    return true;
}

void VenueRig::applyNotOurs(const QList<NotOurs> &notOurs)
{
    const QList<Universe *> unis = m_doc->inputOutputMap()->universes();
    for (int i = 0; i < unis.size(); ++i)
    {
        bool whole = false;
        QList<QPair<int, int>> ranges;
        for (const NotOurs &n : notOurs)
        {
            if (n.universe != quint32(i))
                continue;
            if (n.whole())
                whole = true;
            else
                ranges << qMakePair(n.from, n.to);
        }
        unis.at(i)->setNotOurs(whole, ranges);
    }
}

QString VenueRig::notOursText(const NotOurs &n)
{
    if (n.whole())
        return tr("U%1 (all of it)").arg(n.universe + 1);
    return range(n.universe, quint32(n.from), quint32(n.to - n.from + 1));
}

bool VenueRig::parseRange(const QString &text, int &from, int &to)
{
    const QString t = text.trimmed();
    if (t.isEmpty())
    {
        from = to = -1;
        return true;
    }
    static const QRegularExpression re(QStringLiteral("^(\\d+)\\s*(?:[-–]\\s*(\\d+))?$"));
    const QRegularExpressionMatch m = re.match(t);
    if (!m.hasMatch())
        return false;
    const int a = m.captured(1).toInt();
    const int b = m.captured(2).isEmpty() ? a : m.captured(2).toInt();
    if (a < 1 || b < a || b > 512)
        return false;
    from = a - 1;
    to = b - 1;
    return true;
}

/*****************************************************************************
 * The clash check
 *****************************************************************************/

QString VenueRig::kindName(Clash::Kind kind)
{
    switch (kind)
    {
        case Clash::Overlap:     return tr("Overlap");
        case Clash::NotOursKind: return tr("Not ours");
        case Clash::Slot:        return tr("Slot");
    }
    return QString();
}

QList<VenueRig::Clash> VenueRig::clashes(const Data &d, bool applied) const
{
    struct Entry
    {
        QString name;
        quint32 universe;
        quint32 address;
        quint32 channels;
        bool house;
        RigOwner::Owner owner;
    };
    QList<Entry> entries;
    const QHash<quint32, RigOwner::Owner> owners = RigOwner::effectiveAll(m_doc);
    for (const Fixture *fx : m_doc->fixtures())
    {
        if (!fx->isPatched() && !fx->addressClash())
            continue;
        const bool venue = m_ids.contains(fx->id());
        // A dry run: the house rig on now is going, the one checked comes.
        if (venue && !applied)
            continue;
        Entry e{ fx->name(), fx->universe(), fx->address(), fx->channels(), venue,
                 owners.value(fx->id(), RigOwner::Tour) };
        entries << e;
    }
    if (!applied)
        for (const HouseFixture &hf : d.fixtures)
        {
            if (!hf.patched)
                continue;
            Entry e{ hf.name, hf.universe, hf.address, qMax(1u, hf.channels), true, RigOwner::House };
            entries << e;
        }

    QList<Clash> out;
    auto ownerWord = [](const Entry &e) {
        return e.house ? tr("house") : RigOwner::displayName(e.owner).toLower();
    };
    // Overlaps that involve a house fixture (two tour fixtures never load
    // overlapping: the show refuses that).
    for (int i = 0; i < entries.size(); ++i)
        for (int j = i + 1; j < entries.size(); ++j)
        {
            const Entry &a = entries.at(i), &b = entries.at(j);
            if (!(a.house || b.house) || a.universe != b.universe)
                continue;
            const quint32 s = qMax(a.address, b.address);
            const quint32 e = qMin(a.address + a.channels, b.address + b.channels);
            if (s >= e)
                continue;
            Clash c;
            c.kind = Clash::Overlap;
            c.universe = a.universe;
            c.address = s;
            c.channels = e - s;
            c.text = tr("%1: %2 (%3) and %4 (%5) on the same addresses")
                         .arg(range(c.universe, s, e - s), a.name, ownerWord(a), b.name, ownerWord(b));
            out << c;
        }
    // Not ours.
    for (const NotOurs &n : d.notOurs)
    {
        const quint32 from = n.whole() ? 0 : quint32(n.from);
        const quint32 to = n.whole() ? 511 : quint32(n.to);
        for (const Entry &e : entries)
        {
            if (e.universe != n.universe || e.address > to || e.address + e.channels <= from)
                continue;
            Clash c;
            c.kind = Clash::NotOursKind;
            c.universe = e.universe;
            c.address = qMax(e.address, from);
            c.channels = qMin(e.address + e.channels - 1, to) - c.address + 1;
            c.text = tr("%1: %2 (%3) is on channels this venue keeps (not ours: %4)")
                         .arg(range(c.universe, c.address, c.channels), e.name, ownerWord(e), notOursText(n));
            out << c;
        }
    }
    // Slots: only when the show has House slots at all.
    bool anyHouseSlot = false;
    const QList<Universe *> unis = m_doc->inputOutputMap()->universes();
    for (const Universe *u : unis)
        anyHouseSlot = anyHouseSlot || u->slotOwner() == RigOwner::House;
    if (anyHouseSlot)
        for (const Entry &e : entries)
        {
            if (e.universe >= quint32(unis.size()))
                continue;
            const bool houseSlot = unis.at(int(e.universe))->slotOwner() == RigOwner::House;
            if (houseSlot == e.house)
                continue;
            Clash c;
            c.kind = Clash::Slot;
            c.universe = e.universe;
            c.address = e.address;
            c.channels = e.channels;
            c.text = houseSlot ? tr("%1: %2 (%3) is in a House slot").arg(range(e.universe, e.address, e.channels),
                                                                        e.name, ownerWord(e))
                               : tr("%1: %2 (house) is in a Tour slot").arg(range(e.universe, e.address, e.channels),
                                                                            e.name);
            out << c;
        }
    return out;
}
