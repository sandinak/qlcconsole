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

#include <QTextDocument>
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
#include "qlcfixturedef.h"
#include "outputpatch.h"
#include "patchlist.h"
#include "universe.h"
#include "fixture.h"
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
                if (pp.contains(QStringLiteral("outputUni")))
                    where += QStringLiteral(" port %1").arg(pp.value(QStringLiteral("outputUni")).toInt() + 1);
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

PatchList PatchList::build(Doc *doc, const QList<quint32> &ids)
{
    PatchList list;
    MonitorProperties *mp = doc->monitorProperties();
    list.m_feet = mp->gridUnits() == MonitorProperties::Feet;
    const QList<quint32> plotted = mp->fixtureItemsID();

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
            r.transport = f->transport() == Fixture::WledTransport ? QStringLiteral("WLED")
                                                                   : QStringLiteral("Art-Net");
        }

        r.mountedOn = mp->mountDescription(r.id);
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

QString PatchList::toCsv() const
{
    const QString unit = m_feet ? QStringLiteral("ft") : QStringLiteral("m");
    QStringList out;
    out << csvRow({ QStringLiteral("Fixture"), QStringLiteral("ID"), QStringLiteral("Manufacturer"),
                    QStringLiteral("Model"), QStringLiteral("Mode"), QStringLiteral("Channels"),
                    QStringLiteral("Universe"), QStringLiteral("Universe name"),
                    QStringLiteral("Address"), QStringLiteral("Last channel"),
                    QStringLiteral("Connection"), QStringLiteral("IP"), QStringLiteral("Output"),
                    QStringLiteral("Mounted on"),
                    QStringLiteral("X (%1)").arg(unit), QStringLiteral("Y (%1)").arg(unit),
                    QStringLiteral("Z (%1)").arg(unit), QStringLiteral("Note") });
    foreach (const Row &r, m_rows)
    {
        const bool at = r.patched || r.clash;
        out << csvRow({ r.name, QString::number(r.id), r.manufacturer, r.model, r.mode,
                        QString::number(r.channels),
                        at ? QString::number(r.universe + 1) : QString(),
                        at ? r.universeName : QString(),
                        at ? QString::number(r.firstChannel()) : QString(),
                        at ? QString::number(r.lastChannel()) : QString(),
                        r.network ? QStringLiteral("Network (%1)").arg(r.transport) : QStringLiteral("DMX"),
                        r.ip, r.output, r.mountedOn,
                        r.onPlot ? coord(r.position.x(), m_feet) : QString(),
                        r.onPlot ? coord(r.position.y(), m_feet) : QString(),
                        r.onPlot ? coord(r.position.z(), m_feet) : QString(),
                        r.clash ? QStringLiteral("address clash: not output")
                                : (r.patched ? QString() : QStringLiteral("in the inventory")) });
    }
    return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QString PatchList::toHtml(const QString &title) const
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
        if (r.patched)
            patched++;
    h += QStringLiteral("<p>%1 fixtures, %2 patched · %3</p>").arg(m_rows.size()).arg(patched)
             .arg(QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat));

    const QString header = QStringLiteral(
        "<table cellspacing=\"0\" width=\"100%\"><thead><tr>"
        "<th>Address</th><th>Fixture</th><th>Make / model</th><th>Mode</th><th>Ch</th>"
        "<th>Connection</th><th>Mounted on</th><th>Position</th></tr></thead>");

    bool open = false;
    int section = -2;                     // -1: the inventory
    foreach (const Row &r, m_rows)
    {
        const bool at = r.patched || r.clash;
        const int s = at ? int(r.universe) : -1;
        if (s != section)
        {
            if (open)
                h += QStringLiteral("</table>");
            section = s;
            if (at)
            {
                QString head = QStringLiteral("Universe %1").arg(r.universe + 1);
                if (!r.universeName.isEmpty())
                    head += QStringLiteral(": ") + r.universeName.toHtmlEscaped();
                h += QStringLiteral("<h2>%1</h2>").arg(head);
                h += QStringLiteral("<p>Goes to: %1</p>")
                         .arg(r.output.isEmpty() ? QStringLiteral("<b>nowhere</b> (not patched to an output)")
                                                 : r.output.toHtmlEscaped());
            }
            else
            {
                h += QStringLiteral("<h2>Not patched (in the inventory)</h2>");
            }
            h += header;
            open = true;
        }

        QString addr;
        if (at)
            addr = r.channels > 1 ? QStringLiteral("%1–%2").arg(r.firstChannel()).arg(r.lastChannel())
                                  : QString::number(r.firstChannel());
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
        h += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td>"
                            "<td>%6</td><td>%7</td><td>%8</td></tr>")
                 .arg(addr, r.name.toHtmlEscaped(), make.toHtmlEscaped(), r.mode.toHtmlEscaped(),
                      QString::number(r.channels), conn.toHtmlEscaped(),
                      r.mountedOn.toHtmlEscaped(), pos);
    }
    if (open)
        h += QStringLiteral("</table>");
    h += QStringLiteral("</body></html>");
    return h;
}

bool PatchList::writePdf(const QString &path, const QString &title, QString *error) const
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
    doc.setHtml(toHtml(title));
    doc.print(&writer);
    return true;
}
