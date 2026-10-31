/*
  Q Light Controller Plus
  cuesheet.cpp

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
#include <QHash>

#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "xlsxwriter.h"
#include "qlcpalette.h"
#include "csvfield.h"
#include "function.h"
#include "cuesheet.h"
#include "doc.h"

using namespace ShowTree;

namespace
{

QString timeText(uint ms)
{
    return ms == 0 ? QStringLiteral("0") : Function::speedToString(ms);
}

QString joinNames(const ShowTreeController *st, const QList<quint32> &hitIds)
{
    QStringList names;
    for (quint32 id : hitIds)
        if (const Cue *c = st->cue(id))
            names << c->name;
    return names.join(QStringLiteral(", "));
}

/* What a layer plays, per key, as a person reads it (the cue runner's
   "what changes" wording): Colour and Dimmer bare, the rest by name. */
struct Played
{
    QString key;
    QString text;
    Origin origin;
};

QList<Played> playedBy(const ShowTreeController *st, Doc *doc, const ResolvedLayer &l)
{
    QList<Played> out;
    QList<Look> looks = l.playedLooks();
    for (const QString &k : { KeySpread, KeyGrouping, KeyCross })
        if (l.values.contains(k) && !l.values.value(k).look.none)
            looks << l.values.value(k).look;
    for (const Look &look : looks)
    {
        QStringList names;
        for (quint32 pid : look.palettes)
        {
            const QLCPalette *p = doc->palette(pid);
            names << (p == nullptr ? QStringLiteral("(missing)") : p->name());
        }
        for (quint32 pid : look.own)
            if (const QLCPalette *p = doc->palette(pid))
                names << QStringLiteral("own %1").arg(p->name());
        QString text = names.join(QStringLiteral(" + "));
        if (isLineParamKey(look.key))
            text = st->lineParamText(look.key, look.value);
        if (look.key != KeyColour && look.key != KeyDimmer)
            text = keyLabel(look.key).toLower() + QLatin1Char(' ') + text;
        out << Played{ look.key, text, l.values.value(look.key).origin };
    }
    return out;
}

QString csvRow(const QStringList &fields)
{
    QStringList out;
    for (const QString &f : fields)
        out << CsvField::escape(f);
    return out.join(QLatin1Char(','));
}

QString csvOf(const QList<QStringList> &table)
{
    QStringList out;
    for (const QStringList &line : table)
        out << csvRow(line);
    return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

const char *kStyle =
    "<style>"
    "body { font-family: sans-serif; font-size: 8pt; }"
    "h1 { font-size: 14pt; } h2 { font-size: 11pt; margin-top: 12px; }"
    "h3 { font-size: 9pt; margin-top: 8px; margin-bottom: 2px; }"
    "th { background: #ddd; text-align: left; }"
    "table { border-collapse: collapse; }"
    "td, th { padding: 2px 4px; border: 1px solid #999; }"
    "td.song { background: #eee; font-weight: bold; }"
    "span.tracked { color: #888; }"
    "</style>";

} // namespace

bool CueSheet::Cell::allTracked() const
{
    for (const Value &v : values)
        if (v.changed)
            return false;
    return true;
}

int CueSheet::cueCount() const
{
    int n = 0;
    for (const ListBlock &b : m_lists)
        for (const Row &r : b.rows)
            if (r.kind != Row::Section)
                ++n;
    return n;
}

CueSheet CueSheet::build(Doc *doc, quint32 productionId)
{
    CueSheet sheet;
    ShowTreeController *st = doc == nullptr ? nullptr : doc->showTree();
    const Production *prod = st == nullptr ? nullptr : st->production(productionId);
    if (prod == nullptr)
        return sheet;
    sheet.m_production = prod->name;
    const Numbering numbering = prod->numbering;

    for (quint32 listId : st->playableLists(productionId))
    {
        const CueList *list = st->cueList(listId);
        if (list == nullptr)
            continue;

        /******************** the cue sheet: running order ***************/
        ListBlock block;
        block.listId = listId;
        block.name = list->name;
        QStringList always;
        for (quint32 id : st->hitsAt(HitScope::List, listId))
            always << joinNames(st, { id });
        for (quint32 id : st->hitsAt(HitScope::Production, productionId))
            always << joinNames(st, { id });
        for (quint32 id : st->hitsAt(HitScope::Global, productionId))
            always << joinNames(st, { id });
        always.removeAll(QString());
        block.hitsThroughout = always;

        QList<bool> songSkipped;
        const QList<int> order = st->playOrderOf(listId, &songSkipped);
        const QList<Section> sections = sectionsOf(*list, numbering);
        int lastSection = -2;
        QString sectionName;
        for (int pos = 0; pos < order.count(); ++pos)
        {
            const int i = order.at(pos);
            const Cue &cue = list->cues.at(i);
            const int sec = sectionOfIndex(sections, i);
            if (sec != lastSection)
            {
                lastSection = sec;
                sectionName = sec < 0 ? QString() : sectionTitle(sections.at(sec));
                if (sec >= 0)
                {
                    Row head;
                    head.kind = Row::Section;
                    head.section = sectionName;
                    head.name = sectionName;
                    head.skipped = i < songSkipped.count() && songSkipped.at(i);
                    block.rows << head;
                }
            }
            Row r;
            r.kind = Row::Cue;
            r.cueId = cue.id;
            r.section = sectionName;
            r.number = st->cueNumber(*list, i);
            r.name = cue.name;
            r.fadeIn = timeText(cue.fadeIn);
            r.fadeOut = timeText(cue.fadeOut);
            r.follow = cue.hold == Function::infiniteSpeed() ? QObject::tr("GO")
                                                              : QObject::tr("auto after %1").arg(timeText(cue.hold));
            r.note = cue.note;
            r.hits = joinNames(st, st->hitsAt(HitScope::Cue, cue.id));
            r.skipped = cue.skip || (i < songSkipped.count() && songSkipped.at(i));
            block.rows << r;

            const CueList *subs = st->cueList(st->subCueList(cue.id));
            if (subs != nullptr)
                for (int s = 0; s < subs->cues.count(); ++s)
                {
                    const Cue &sub = subs->cues.at(s);
                    Row sr;
                    sr.kind = Row::SubCue;
                    sr.cueId = sub.id;
                    sr.section = sectionName;
                    sr.number = st->cueNumber(*subs, s);
                    sr.name = sub.name;
                    sr.fadeIn = timeText(sub.fadeIn);
                    sr.fadeOut = timeText(sub.fadeOut);
                    sr.follow = sub.follow ? QObject::tr("auto after %1").arg(timeText(sub.wait))
                                           : QObject::tr("Sub GO");
                    sr.note = sub.note;
                    sr.hits = joinNames(st, st->hitsAt(HitScope::Cue, sub.id));
                    sr.skipped = r.skipped || sub.skip;
                    block.rows << sr;
                }
        }
        sheet.m_lists << block;

        /******************** the tracking sheet: list order ****************/
        TrackingBlock tb;
        tb.listId = listId;
        tb.name = list->name;
        const QList<ResolvedCue> resolved = st->resolved(*list);
        // The rows: every layer in the list, in the order they first appear.
        QList<QString> keys;
        QHash<QString, int> rowOf;
        for (const ResolvedCue &rc : resolved)
            for (const ResolvedLayer &l : rc.layers)
            {
                const QString sig = l.target.signature();
                if (rowOf.contains(sig))
                    continue;
                rowOf.insert(sig, tb.rows.count());
                keys << sig;
                TrackingRow tr;
                const QString label = l.name.isEmpty() ? st->targetLabel(l.target) : l.name;
                tr.label = QString(l.depth * 2, QLatin1Char(' ')) + label;
                tb.rows << tr;
            }
        for (int i = 0; i < list->cues.count() && i < resolved.count(); ++i)
        {
            const Cue &cue = list->cues.at(i);
            TrackingColumn col;
            col.cueId = cue.id;
            col.number = st->cueNumber(*list, i);
            col.name = cue.name;
            col.skipped = cue.skip;
            tb.columns << col;
            const int before = playedBefore(*list, i);
            const ResolvedCue *prev = before < 0 || before >= resolved.count() ? nullptr : &resolved.at(before);
            for (int r = 0; r < tb.rows.count(); ++r)
            {
                Cell cell;
                const ResolvedLayer *now = nullptr;
                const ResolvedLayer *was = nullptr;
                for (const ResolvedLayer &l : resolved.at(i).layers)
                    if (l.target.signature() == keys.at(r))
                        now = &l;
                if (prev != nullptr)
                    for (const ResolvedLayer &l : prev->layers)
                        if (l.target.signature() == keys.at(r))
                            was = &l;
                const QList<Played> p = now == nullptr ? QList<Played>() : playedBy(st, doc, *now);
                const QList<Played> q = was == nullptr ? QList<Played>() : playedBy(st, doc, *was);
                QHash<QString, QString> before2;
                for (const Played &x : q)
                    before2.insert(x.key, x.text);
                for (const Played &x : p)
                {
                    const bool same = before2.value(x.key) == x.text && before2.contains(x.key);
                    cell.values << Value{ x.text, x.origin != Origin::Tracked && !same };
                }
                if (p.isEmpty() && !q.isEmpty())
                    cell.values << Value{ QObject::tr("out"), true };
                else if (!p.isEmpty())
                    for (const Played &x : q)
                    {
                        bool still = false;
                        for (const Played &y : p)
                            still = still || y.key == x.key;
                        if (!still)
                            cell.values << Value{ QObject::tr("no %1").arg(keyLabel(x.key).toLower()), true };
                    }
                // Released here: it is still on stage for this cue, gone after.
                if (now != nullptr && now->release)
                    cell.values << Value{ QObject::tr("release"), true };
                tb.rows[r].cells << cell;
            }
        }
        sheet.m_tracking << tb;
    }
    return sheet;
}

QStringList CueSheet::header()
{
    return { QObject::tr("Cue list"), QObject::tr("Song"), QObject::tr("Cue"), QObject::tr("Name"),
             QObject::tr("Fade in"), QObject::tr("Fade out"), QObject::tr("Follow / wait"),
             QObject::tr("Notes"), QObject::tr("Hits armed here") };
}

QList<QStringList> CueSheet::cueTable() const
{
    QList<QStringList> t;
    t << header();
    for (const ListBlock &b : m_lists)
        for (const Row &r : b.rows)
        {
            if (r.kind == Row::Section)
                continue;
            QString note = r.note;
            if (r.skipped)
                note = note.isEmpty() ? QObject::tr("SKIPPED") : QObject::tr("SKIPPED. %1").arg(note);
            t << QStringList{ b.name, r.section, r.number, r.name, r.fadeIn, r.fadeOut, r.follow, note,
                              r.hits };
        }
    return t;
}

QString CueSheet::cellText(const Cell &cell)
{
    QStringList parts;
    for (const Value &v : cell.values)
        parts << (v.changed ? v.text : QStringLiteral("(%1)").arg(v.text));
    return parts.join(QStringLiteral(" · "));
}

QList<QStringList> CueSheet::trackingTable() const
{
    QList<QStringList> t;
    for (const TrackingBlock &b : m_tracking)
    {
        t << QStringList{ b.name };
        QStringList head{ QObject::tr("Layer") };
        for (const TrackingColumn &c : b.columns)
            head << (c.skipped ? c.number + QStringLiteral(" (skipped)") : c.number);
        t << head;
        QStringList names{ QObject::tr("Cue name") };
        for (const TrackingColumn &c : b.columns)
            names << c.name;
        t << names;
        for (const TrackingRow &r : b.rows)
        {
            QStringList line{ r.label };
            for (const Cell &c : r.cells)
                line << cellText(c);
            t << line;
        }
        t << QStringList();
    }
    return t;
}

QString CueSheet::cueCsv() const { return csvOf(cueTable()); }
QString CueSheet::trackingCsv() const { return csvOf(trackingTable()); }

QByteArray CueSheet::cueXlsx() const
{
    return XlsxWriter::workbook(QObject::tr("Cue sheet"), cueTable());
}

QByteArray CueSheet::trackingXlsx() const
{
    return XlsxWriter::workbook(QObject::tr("Tracking sheet"), trackingTable());
}

bool CueSheet::writeFile(const QString &path, const QByteArray &data, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(data) != data.size())
    {
        if (error != nullptr)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool CueSheet::writeCueXlsx(const QString &path, QString *error) const
{
    return writeFile(path, cueXlsx(), error);
}

bool CueSheet::writeTrackingXlsx(const QString &path, QString *error) const
{
    return writeFile(path, trackingXlsx(), error);
}

QString CueSheet::cueHtml(const QString &title) const
{
    QString h = QStringLiteral("<html><head>") + QLatin1String(kStyle) + QStringLiteral("</head><body>");
    h += QStringLiteral("<h1>%1</h1>").arg(title.toHtmlEscaped());
    h += QStringLiteral("<p>%1 · %2</p>").arg(QObject::tr("%n cue(s)", nullptr, cueCount()),
                                              QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat));
    for (const ListBlock &b : m_lists)
    {
        h += QStringLiteral("<h2>%1</h2>").arg(b.name.toHtmlEscaped());
        if (!b.hitsThroughout.isEmpty())
            h += QStringLiteral("<p>%1 %2</p>").arg(QObject::tr("Hits armed throughout:").toHtmlEscaped(),
                                                   b.hitsThroughout.join(QStringLiteral(", ")).toHtmlEscaped());
        h += QStringLiteral("<table cellspacing=\"0\" width=\"100%\"><thead><tr>");
        const QStringList head = header();
        for (int c = 2; c < head.count(); ++c)
            h += QStringLiteral("<th>%1</th>").arg(head.at(c).toHtmlEscaped());
        h += QStringLiteral("</tr></thead>");
        for (const Row &r : b.rows)
        {
            if (r.kind == Row::Section)
            {
                h += QStringLiteral("<tr><td class=\"song\" colspan=\"%1\">%2%3</td></tr>")
                         .arg(head.count() - 2).arg(r.name.toHtmlEscaped(),
                                                    r.skipped ? QObject::tr(" (left out tonight)").toHtmlEscaped()
                                                              : QString());
                continue;
            }
            QString note = r.note.toHtmlEscaped();
            if (r.skipped)
                note = QStringLiteral("<b>%1</b> %2").arg(QObject::tr("SKIPPED").toHtmlEscaped(), note);
            const QString indent = r.kind == Row::SubCue ? QStringLiteral("&nbsp;&nbsp;") : QString();
            h += QStringLiteral("<tr><td>%1%2</td><td>%1%3</td><td>%4</td><td>%5</td><td>%6</td><td>%7</td>"
                                "<td>%8</td></tr>")
                     .arg(indent, r.number.toHtmlEscaped(), r.name.toHtmlEscaped(), r.fadeIn.toHtmlEscaped(),
                          r.fadeOut.toHtmlEscaped(), r.follow.toHtmlEscaped(), note, r.hits.toHtmlEscaped());
        }
        h += QStringLiteral("</table>");
    }
    h += QStringLiteral("</body></html>");
    return h;
}

QString CueSheet::trackingHtml(const QString &title, int cuesPerPage) const
{
    cuesPerPage = qMax(1, cuesPerPage);
    QString h = QStringLiteral("<html><head>") + QLatin1String(kStyle) + QStringLiteral("</head><body>");
    h += QStringLiteral("<h1>%1</h1>").arg(title.toHtmlEscaped());
    h += QStringLiteral("<p>%1</p>")
             .arg(QObject::tr("Plain: this cue sets it. Grey in brackets: tracked in from an earlier cue, "
                              "nothing changes. %1")
                      .arg(QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat))
                      .toHtmlEscaped());
    for (const TrackingBlock &b : m_tracking)
    {
        h += QStringLiteral("<h2>%1</h2>").arg(b.name.toHtmlEscaped());
        // Wider than a page: the next cues go in a table of their own,
        // the layer names repeated.
        for (int from = 0; from < b.columns.count(); from += cuesPerPage)
        {
            const int to = qMin(from + cuesPerPage, int(b.columns.count()));
            h += QStringLiteral("<table cellspacing=\"0\" width=\"100%\"><thead><tr><th>%1</th>")
                     .arg(QObject::tr("Layer").toHtmlEscaped());
            for (int c = from; c < to; ++c)
            {
                const TrackingColumn &col = b.columns.at(c);
                h += QStringLiteral("<th>%1%2<br/>%3</th>")
                         .arg(col.number.toHtmlEscaped(),
                              col.skipped ? QObject::tr(" (skipped)").toHtmlEscaped() : QString(),
                              col.name.toHtmlEscaped());
            }
            h += QStringLiteral("</tr></thead>");
            for (const TrackingRow &r : b.rows)
            {
                QString label = r.label;
                int depth = 0;
                while (depth < label.size() && label.at(depth) == QLatin1Char(' '))
                    ++depth;
                h += QStringLiteral("<tr><td>%1%2</td>")
                         .arg(QString(depth, QChar(0xA0)).replace(QChar(0xA0), QStringLiteral("&nbsp;")),
                              label.mid(depth).toHtmlEscaped());
                for (int c = from; c < to; ++c)
                {
                    QStringList parts;
                    for (const Value &v : r.cells.at(c).values)
                        parts << (v.changed ? QStringLiteral("<b>%1</b>").arg(v.text.toHtmlEscaped())
                                            : QStringLiteral("<span class=\"tracked\">(%1)</span>")
                                                  .arg(v.text.toHtmlEscaped()));
                    h += QStringLiteral("<td>%1</td>").arg(parts.join(QStringLiteral("<br/>")));
                }
                h += QStringLiteral("</tr>");
            }
            h += QStringLiteral("</table><br/>");
        }
    }
    h += QStringLiteral("</body></html>");
    return h;
}

bool CueSheet::writePdf(const QString &path, const QString &title, const QString &html, QString *error)
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

    // As the patch list does: lay the page out at the writer's printable size.
    QTextDocument doc;
    doc.setDocumentMargin(0);
    doc.setPageSize(writer.pageLayout().paintRectPoints().size());
    doc.setHtml(html);
    doc.print(&writer);
    return true;
}

bool CueSheet::writeCuePdf(const QString &path, const QString &title, QString *error) const
{
    return writePdf(path, title, cueHtml(title), error);
}

bool CueSheet::writeTrackingPdf(const QString &path, const QString &title, QString *error) const
{
    return writePdf(path, title, trackingHtml(title), error);
}
