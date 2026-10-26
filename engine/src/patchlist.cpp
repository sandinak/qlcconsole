/*
  Q Light Controller Plus
  patchlist.cpp

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

#include <QRegularExpression>
#include <QTextDocument>
#include <QCollator>
#include <QPageLayout>
#include <QPdfWriter>
#include <QPageSize>
#include <QDateTime>
#include <QLocale>
#include <QFile>

#include <algorithm>

#include "monitorproperties.h"
#include "qlcioplugin.h"
#include "inputoutputmap.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "outputpatch.h"
#include "patchlist.h"
#include "rigowner.h"
#include "universe.h"
#include "fixture.h"
#include "xlsxwriter.h"
#include "truss.h"
#include "doc.h"

QString PatchList::Row::patchText() const
{
    const QString range = channels > 1
        ? QStringLiteral("%1–%2").arg(firstChannel()).arg(lastChannel())
        : QString::number(firstChannel());
    const QString uni = universeName.isEmpty()
        ? QString::number(universe + 1)
        : QStringLiteral("%1: %2").arg(universe + 1).arg(universeName);
    if (clash)
        return QStringLiteral("clash · %1 · %2").arg(uni, range);
    if (!patched)
        return QStringLiteral("not patched");
    return QStringLiteral("%1 · %2").arg(uni, range);
}

QString PatchList::Row::modeText() const
{
    /* "18 Channel", "165ch", "15-Channel-Chinese" say it already: the
       count as a number of its own in the name. */
    const QRegularExpression count(QStringLiteral("(^|\\D)%1(\\D|$)").arg(channels));
    if (mode.isEmpty())
        return channels > 0 ? QStringLiteral("%1 ch").arg(channels) : QString();
    if (channels == 0 || mode.contains(count))
        return mode;
    return QStringLiteral("%1 (%2 ch)").arg(mode).arg(channels);
}

QString PatchList::Row::makeModel() const
{
    if (model.isEmpty() || model == manufacturer)
        return manufacturer;
    if (manufacturer.isEmpty())
        return model;
    return QStringLiteral("%1 %2").arg(manufacturer, model);
}

QString PatchList::universeOutput(Doc *doc, quint32 universe)
{
    InputOutputMap *iomap = doc->inputOutputMap();
    foreach (Universe *u, iomap->universes())
    {
        if (u == nullptr || u->id() != universe)
            continue;
        QStringList legs;
        for (int i = 0; i < u->outputPatchesCount(); ++i)
        {
            OutputPatch *op = u->outputPatch(i);
            if (op == nullptr)
                continue;
            const QString plugin = op->plugin() != nullptr ? op->plugin()->name() : op->pluginName();
            const QMap<QString, QVariant> pp = op->getPluginParameters();
            const QString ip = pp.value(QStringLiteral("outputIP")).toString();
            QString where;
            if (!ip.isEmpty())
            {
                const QString alias = iomap->targetAlias(ip);
                where = alias.isEmpty() ? ip : QStringLiteral("%1 (%2)").arg(alias, ip);
                /* Same words as the Connections tab: "port N" is the socket
                   on the node, only when the node says (or was declared
                   with) which one; "universe n:s:u" is the Art-Net
                   port-address. This said "port 1" for port-address 0. */
                if (pp.contains(QStringLiteral("outputUni")))
                {
                    const quint32 pa = pp.value(QStringLiteral("outputUni")).toUInt();
                    int number = 0;
                    if (op->plugin() != nullptr)
                    {
                        foreach (const QLCIOPlugin::Device &d, op->plugin()->discoveredDevices())
                        {
                            if (number == 0 && d.line == op->output() && d.address == ip
                                    && d.portUniverses.contains(pa))
                            {
                                const int k = d.portUniverses.indexOf(pa);
                                number = d.portNumbers.value(k, k + 1);
                            }
                        }
                    }
                    if (number == 0)
                    {
                        foreach (const InputOutputMap::ManualTarget &mt, iomap->manualTargets())
                        {
                            if (mt.address == ip && mt.ports.contains(pa))
                                number = mt.ports.indexOf(pa) + 1;
                        }
                    }
                    const QString uni = QStringLiteral("%1:%2:%3").arg((pa >> 8) & 0x7F)
                                            .arg((pa >> 4) & 0x0F).arg(pa & 0x0F);
                    where += number > 0
                        ? QObject::tr(" · port %1 · universe %2").arg(number).arg(uni)
                        : QObject::tr(" · universe %1").arg(uni);
                }
            }
            else
            {
                where = op->outputName();
            }
            legs << (where.isEmpty() ? plugin : QStringLiteral("%1 → %2").arg(plugin, where));
        }
        return legs.join(QStringLiteral(" · "));
    }
    return QString();
}

bool PatchList::isPixelFixture(const Fixture *fixture)
{
    if (fixture == nullptr)
        return false;
    if (fixture->type() == QLCFixtureDef::LEDBarPixels)
        return true;
    if (fixture->fixtureMode() == nullptr)
        return false;
    int coloured = 0;
    for (int h = 0; h < fixture->heads(); ++h)
    {
        if (fixture->rgbChannels(h).size() >= 3)
            coloured++;
    }
    return coloured >= 2;
}

/* What @p rp hangs on, without where along it: "Truss FOH", "Tower SL";
   a hanger is on its truss or tower. Empty when it is on nothing. */
static QString hangsOn(MonitorProperties *mp, const FixtureRigProps &rp)
{
    if (rp.onHanger())
    {
        Hanger *h = mp->hanger(rp.hangerId);
        if (h == nullptr)
            return QString();
        if (h->parentKind() == Hanger::OnTruss)
        {
            Truss *t = mp->truss(h->parentId());
            return t != nullptr ? QObject::tr("Truss %1").arg(t->name()) : QString();
        }
        Tower *tw = mp->tower(h->parentId());
        return tw != nullptr ? QObject::tr("Tower %1").arg(tw->name()) : QString();
    }
    if (rp.trussId != Truss::invalidId())
    {
        Truss *t = mp->truss(rp.trussId);
        return t != nullptr ? QObject::tr("Truss %1").arg(t->name()) : QString();
    }
    if (rp.pipeId != Pipe::invalidId())
    {
        Pipe *p = mp->pipe(rp.pipeId);
        return p != nullptr ? QObject::tr("Bar %1").arg(p->name()) : QString();
    }
    if (rp.towerId != Tower::invalidId())
    {
        Tower *t = mp->tower(rp.towerId);
        return t != nullptr ? QObject::tr("Tower %1").arg(t->name()) : QString();
    }
    if (rp.riserPlatformId != FixtureRigProps::invalidPlatformId())
    {
        StagePlatform *pl = mp->platform(rp.riserPlatformId);
        return pl != nullptr ? QObject::tr("Riser %1").arg(pl->name()) : QString();
    }
    if (rp.deckPlatformId != FixtureRigProps::invalidPlatformId())
    {
        StagePlatform *pl = mp->platform(rp.deckPlatformId);
        return pl != nullptr ? QObject::tr("Deck %1").arg(pl->name()) : QString();
    }
    return QString();
}

PatchList PatchList::build(Doc *doc, const QList<quint32> &ids)
{
    PatchList list;
    MonitorProperties *mp = doc->monitorProperties();
    list.m_feet = mp->gridUnits() == MonitorProperties::Feet;
    const QList<quint32> plotted = mp->fixtureItemsID();

    /* A fixture's group: the first (lowest id) that holds it. */
    QHash<quint32, QString> groupOf;
    QList<FixtureGroup *> groups = doc->fixtureGroups();
    std::sort(groups.begin(), groups.end(), [](FixtureGroup *a, FixtureGroup *b) {
        return a->id() < b->id();
    });
    foreach (FixtureGroup *g, groups)
    {
        foreach (quint32 fid, g->fixtureList())
        {
            if (!groupOf.contains(fid))
                groupOf.insert(fid, g->name());
        }
    }
    const QHash<quint32, RigOwner::Owner> owners = RigOwner::effectiveAll(doc);

    /* Universes in use, over the whole show (a label's address says which
       when there is more than one). */
    QSet<quint32> universes;
    foreach (Fixture *f, doc->fixtures())
    {
        if (f != nullptr && f->isPatched())
            universes.insert(f->universe());
    }
    list.m_multiUniverse = universes.size() > 1;

    QHash<quint32, QString> outputs;      // one look-up per universe
    foreach (Fixture *f, doc->fixtures())
    {
        if (f == nullptr || (!ids.isEmpty() && !ids.contains(f->id())))
            continue;
        Row r;
        r.id = f->id();
        r.name = f->name();
        if (QLCFixtureDef *def = f->fixtureDef())
        {
            r.manufacturer = def->manufacturer();
            r.model = def->model();
        }
        if (QLCFixtureMode *mode = f->fixtureMode())
            r.mode = mode->name();
        r.channels = f->channels();

        r.patched = f->isPatched();
        r.clash = f->addressClash();
        if (r.patched || r.clash)
        {
            r.universe = f->universe();
            r.address = f->address();
            r.universeName = doc->inputOutputMap()->getUniverseNameByID(r.universe);
            if (r.universeName == QStringLiteral("Universe %1").arg(r.universe + 1))
                r.universeName.clear();     // the default says nothing the number doesn't
            if (!outputs.contains(r.universe))
                outputs.insert(r.universe, universeOutput(doc, r.universe));
            r.output = outputs.value(r.universe);
        }

        r.network = f->connection() == Fixture::Network;
        if (r.network)
        {
            r.ip = f->ipAddress();
            if (!r.ip.isEmpty())
                r.host = doc->inputOutputMap()->targetAlias(r.ip);
            r.transport = f->transport() == Fixture::WledTransport ? QStringLiteral("WLED")
                                                                   : QStringLiteral("Art-Net");
        }

        r.mountedOn = mp->mountDescription(r.id);
        r.group = groupOf.value(r.id);
        r.location = hangsOn(mp, mp->fixtureRigProps(r.id));
        if (r.location.isEmpty())
            r.location = r.group;
        r.type = f->typeString();
        r.pixel = isPixelFixture(f);
        const RigOwner::Owner owner = owners.value(r.id, RigOwner::Tour);
        r.ownerKind = owner;
        if (owner != RigOwner::Tour)
        {
            r.owner = RigOwner::displayName(owner);
            list.m_owners = true;
        }
        r.onPlot = plotted.contains(r.id) || mp->hasFixtureRigProps(r.id);
        if (r.onPlot)
            r.position = mp->fixtureRigPosition(r.id);
        list.m_rows << r;
    }

    /* The crew's order: universe, then address; clashes where they asked to
       be (they are patched there in the file); the inventory last, by name. */
    std::stable_sort(list.m_rows.begin(), list.m_rows.end(), [](const Row &a, const Row &b) {
        const bool pa = a.patched || a.clash, pb = b.patched || b.clash;
        if (pa != pb)
            return pa;
        if (!pa)
            return a.name.localeAwareCompare(b.name) < 0;
        if (a.universe != b.universe)
            return a.universe < b.universe;
        if (a.address != b.address)
            return a.address < b.address;
        return a.id < b.id;
    });
    return list;
}

/* One CSV field: quoted when it holds a comma, quote or line break. */
static QString csv(const QString &field)
{
    if (!field.contains(QLatin1Char(',')) && !field.contains(QLatin1Char('"'))
        && !field.contains(QLatin1Char('\n')))
        return field;
    QString f = field;
    f.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(f);
}

static QString csvRow(const QStringList &fields)
{
    QStringList out;
    foreach (const QString &f, fields)
        out << csv(f);
    return out.join(QLatin1Char(','));
}

/* A plot coordinate in the plot's units, one decimal. */
static QString coord(float metres, bool feet)
{
    const double m = double(metres);
    return QString::number(feet ? m * 3.28084 : m, 'f', 1);
}

/*********************************************************************
 * Filtering and grouping
 *********************************************************************/

bool PatchList::Filter::accepts(const Row &row) const
{
    if (kind == Discrete && row.pixel)
        return false;
    if (kind == Pixel && !row.pixel)
        return false;
    return owners.isEmpty() || owners.contains(row.ownerKind);
}

PatchList PatchList::filtered(const Filter &filter) const
{
    PatchList out = *this;
    out.m_rows.clear();
    foreach (const Row &r, m_rows)
    {
        if (filter.accepts(r))
            out.m_rows << r;
    }
    return out;
}

QString PatchList::groupByName(GroupBy by)
{
    switch (by)
    {
        case ByLocation: return QObject::tr("Location");
        case ByUniverse: return QObject::tr("Universe");
        case ByType:     return QObject::tr("Type");
        case ByNone:     break;
    }
    return QString();
}

/* The headings that go last: nowhere to hang, nothing patched. */
static QString notPlaced()   { return QObject::tr("Not placed"); }
static QString notPatched()  { return QObject::tr("Not patched (in the inventory)"); }

QString PatchList::sectionTitle(const Row &row, GroupBy by)
{
    switch (by)
    {
        case ByLocation:
            return row.location.isEmpty() ? notPlaced() : row.location;
        case ByUniverse:
        {
            if (!row.patched && !row.clash)
                return notPatched();
            QString head = QObject::tr("Universe %1").arg(row.universe + 1);
            if (!row.universeName.isEmpty())
                head += QStringLiteral(": ") + row.universeName;
            return head;
        }
        case ByType:
            return row.type.isEmpty() ? QObject::tr("Other") : row.type;
        case ByNone:
            break;
    }
    return QString();
}

QList<PatchList::Section> PatchList::sections(GroupBy by) const
{
    QList<Section> out;
    if (m_rows.isEmpty())
        return out;
    if (by == ByNone)
    {
        Section all;
        all.rows = m_rows;
        out << all;
        return out;
    }

    /* The rows are in patch order already; a section keeps it. */
    QHash<QString, int> at;
    QHash<QString, quint32> firstUniverse;
    foreach (const Row &r, m_rows)
    {
        const QString t = sectionTitle(r, by);
        if (!at.contains(t))
        {
            at.insert(t, out.size());
            Section s;
            s.title = t;
            out << s;
            firstUniverse.insert(t, (r.patched || r.clash) ? r.universe : UINT_MAX);
        }
        out[at.value(t)].rows << r;
    }

    const QString last = by == ByLocation ? notPlaced()
                       : (by == ByUniverse ? notPatched() : QString());
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(out.begin(), out.end(), [&](const Section &a, const Section &b) {
        const bool la = a.title == last, lb = b.title == last;
        if (la != lb)
            return lb;
        if (by == ByUniverse)
            return firstUniverse.value(a.title) < firstUniverse.value(b.title);
        return collator.compare(a.title, b.title) < 0;
    });
    return out;
}

QList<PatchList::Row> PatchList::arranged(GroupBy by) const
{
    QList<Row> out;
    foreach (const Section &s, sections(by))
        out << s.rows;
    return out;
}

/*********************************************************************
 * The spreadsheet
 *********************************************************************/

QList<QStringList> PatchList::table(GroupBy by) const
{
    const QString unit = m_feet ? QStringLiteral("ft") : QStringLiteral("m");
    QList<QStringList> out;
    QStringList header;
    if (by != ByNone)
        header << groupByName(by);
    header << QStringLiteral("Fixture") << QStringLiteral("ID") << QStringLiteral("Manufacturer")
           << QStringLiteral("Model") << QStringLiteral("Mode") << QStringLiteral("Channels")
           << QStringLiteral("Universe") << QStringLiteral("Universe name")
           << QStringLiteral("Address") << QStringLiteral("Last channel")
           << QStringLiteral("Connection") << QStringLiteral("IP") << QStringLiteral("Output")
           << QStringLiteral("Mounted on")
           << QStringLiteral("X (%1)").arg(unit) << QStringLiteral("Y (%1)").arg(unit)
           << QStringLiteral("Z (%1)").arg(unit) << QStringLiteral("Note")
           << QStringLiteral("Location") << QStringLiteral("Fixture group")
           << QStringLiteral("Type") << QStringLiteral("Kind");
    // An Owner column only when some fixture is not the tour's.
    if (m_owners)
        header << QStringLiteral("Owner");
    out << header;

    foreach (const Section &s, sections(by))
    {
        foreach (const Row &r, s.rows)
        {
            const bool at = r.patched || r.clash;
            QStringList line;
            if (by != ByNone)
                line << s.title;
            line << r.name << QString::number(r.id) << r.manufacturer << r.model << r.mode
                 << QString::number(r.channels)
                 << (at ? QString::number(r.universe + 1) : QString())
                 << (at ? r.universeName : QString())
                 << (at ? QString::number(r.firstChannel()) : QString())
                 << (at ? QString::number(r.lastChannel()) : QString())
                 << (r.network ? QStringLiteral("Network (%1)").arg(r.transport) : QStringLiteral("DMX"))
                 << r.ip << r.output << r.mountedOn
                 << (r.onPlot ? coord(r.position.x(), m_feet) : QString())
                 << (r.onPlot ? coord(r.position.y(), m_feet) : QString())
                 << (r.onPlot ? coord(r.position.z(), m_feet) : QString())
                 << (r.clash ? QStringLiteral("address clash: not output")
                             : (r.patched ? QString() : QStringLiteral("in the inventory")))
                 << r.location << r.group << r.type
                 << (r.pixel ? QStringLiteral("Pixel / matrix") : QStringLiteral("Discrete"));
            if (m_owners)
                line << RigOwner::displayName(r.ownerKind);
            out << line;
        }
    }
    return out;
}

QString PatchList::toCsv(GroupBy by) const
{
    QStringList out;
    foreach (const QStringList &line, table(by))
        out << csvRow(line);
    return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QByteArray PatchList::toXlsx(GroupBy by) const
{
    /* The columns that hold numbers, as numbers: a sheet sorts them right. */
    const QList<QStringList> t = table(by);
    QList<int> numeric;
    if (!t.isEmpty())
    {
        const QStringList &h = t.first();
        for (int c = 0; c < h.size(); ++c)
        {
            const QString &n = h.at(c);
            if (n == QLatin1String("ID") || n == QLatin1String("Channels")
                || n == QLatin1String("Universe") || n == QLatin1String("Address")
                || n == QLatin1String("Last channel") || n.startsWith(QLatin1String("X ("))
                || n.startsWith(QLatin1String("Y (")) || n.startsWith(QLatin1String("Z (")))
            {
                // "Universe" grouped by universe is the heading, words: not a number.
                if (!(c == 0 && by != ByNone))
                    numeric << c;
            }
        }
    }
    return XlsxWriter::workbook(QStringLiteral("Patch list"), t, numeric);
}

bool PatchList::writeXlsx(const QString &path, GroupBy by, QString *error) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (error != nullptr)
            *error = file.errorString();
        return false;
    }
    const QByteArray data = toXlsx(by);
    if (file.write(data) != data.size())
    {
        if (error != nullptr)
            *error = file.errorString();
        return false;
    }
    return true;
}

QString PatchList::toHtml(const QString &title, GroupBy by) const
{
    const QString unit = m_feet ? QStringLiteral(" ft") : QStringLiteral(" m");
    QString h;
    h += QStringLiteral("<html><head><style>"
                        "body { font-family: sans-serif; font-size: 8pt; }"
                        "h1 { font-size: 14pt; } h2 { font-size: 11pt; margin-top: 12px; }"
                        "th { background: #ddd; text-align: left; }"
                        "table { border-collapse: collapse; }"
                        "td, th { padding: 2px 4px; border: 1px solid #999; }"
                        "</style></head><body>");
    h += QStringLiteral("<h1>%1</h1>").arg(title.toHtmlEscaped());

    int patched = 0;
    foreach (const Row &r, m_rows)
    {
        if (r.patched)
            patched++;
    }
    h += QStringLiteral("<p>%1 fixtures, %2 patched · %3</p>").arg(m_rows.size()).arg(patched)
             .arg(QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat));

    /* Grouped by where they hang, the mount says more than the heading; by
       universe, the heading says the universe. */
    const QString header = QStringLiteral(
        "<table cellspacing=\"0\" width=\"100%\"><thead><tr>"
        "<th>Address</th><th>Fixture</th><th>Make / model</th><th>Mode</th>"
        "<th>Connection</th><th>Mounted on</th><th>Position</th></tr></thead>");

    foreach (const Section &s, sections(by))
    {
        if (!s.title.isEmpty())
            h += QStringLiteral("<h2>%1</h2>").arg(s.title.toHtmlEscaped());
        if (by == ByUniverse && !s.rows.isEmpty() && (s.rows.first().patched || s.rows.first().clash))
        {
            const QString output = s.rows.first().output;
            h += QStringLiteral("<p>Goes to: %1</p>")
                     .arg(output.isEmpty() ? QStringLiteral("<b>nowhere</b> (not patched to an output)")
                                           : output.toHtmlEscaped());
        }
        h += header;
        foreach (const Row &r, s.rows)
        {
            const bool at = r.patched || r.clash;
            QString addr;
            if (at)
            {
                const QString range = r.channels > 1
                    ? QStringLiteral("%1–%2").arg(r.firstChannel()).arg(r.lastChannel())
                    : QString::number(r.firstChannel());
                addr = by == ByUniverse ? range : QStringLiteral("%1 · %2").arg(r.universe + 1).arg(range);
            }
            if (r.clash)
                addr += QStringLiteral(" <b>clash</b>");
            const QString make = r.makeModel();
            QString conn = QStringLiteral("DMX");
            if (r.network)
                conn = QStringLiteral("%1 %2").arg(r.transport,
                                                  r.ip.isEmpty() ? QStringLiteral("(no IP)") : r.ip);
            QString pos;
            if (r.onPlot)
                pos = QStringLiteral("%1, %2, %3%4").arg(coord(r.position.x(), m_feet),
                                                        coord(r.position.y(), m_feet),
                                                        coord(r.position.z(), m_feet), unit);
            h += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td>"
                                "<td>%5</td><td>%6</td><td>%7</td></tr>")
                     .arg(addr, r.owner.isEmpty() ? r.name.toHtmlEscaped()
                                                  : QStringLiteral("%1 <i>(%2)</i>").arg(r.name.toHtmlEscaped(),
                                                                                         r.owner.toHtmlEscaped()),
                          make.toHtmlEscaped(), r.modeText().toHtmlEscaped(), conn.toHtmlEscaped(),
                          r.mountedOn.toHtmlEscaped(), pos);
        }
        h += QStringLiteral("</table>");
    }
    h += QStringLiteral("</body></html>");
    return h;
}

bool PatchList::writePdf(const QString &path, const QString &title, QString *error, GroupBy by) const
{
    {
        // QPdfWriter says nothing when it can't open the file; find out first.
        QFile probe(path);
        if (!probe.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            if (error != nullptr)
                *error = probe.errorString();
            return false;
        }
    }
    QPdfWriter writer(path);
    writer.setTitle(title);
    writer.setCreator(QStringLiteral("qlcconsole"));
    writer.setPageSize(QPageSize(QPageSize::Letter));
    writer.setPageOrientation(QPageLayout::Landscape);
    writer.setPageMargins(QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter);

    /* Lay the page out in points at the writer's printable size. Without a
       page size of its own, QTextDocument::print() adds a 2 cm margin inside
       the writer's, wasting a quarter of a landscape page. */
    QTextDocument doc;
    doc.setDocumentMargin(0);
    doc.setPageSize(writer.pageLayout().paintRectPoints().size());
    doc.setHtml(toHtml(title, by));
    doc.print(&writer);
    return true;
}
