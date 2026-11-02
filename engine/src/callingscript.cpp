/*
  Q Light Controller Plus
  callingscript.cpp

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

#include <QDateTime>
#include <QLocale>
#include <QObject>
#include <QSet>

#include "showtreecontroller.h"
#include "callingscript.h"
#include "cuesheet.h"
#include "doc.h"

using namespace ShowTree;

CallingScriptSheet CallingScriptSheet::build(Doc *doc, quint32 productionId)
{
    CallingScriptSheet out;
    ShowTreeController *st = doc == nullptr ? nullptr : doc->showTree();
    const Production *prod = st == nullptr ? nullptr : st->production(productionId);
    if (prod == nullptr)
        return out;
    const CallingScript script = st->callingScript(productionId);
    out.m_title = script.title;
    if (script.text.isEmpty())
        return out;

    // The song sections' names, which a script may carry as headings.
    QSet<QString> headings;
    const quint32 mainId = st->mainList(productionId);
    for (const Section &s : st->sections(mainId))
    {
        const QString t = sectionTitle(s).trimmed().toLower();
        if (!t.isEmpty())
            headings.insert(t);
        if (!s.name.trimmed().isEmpty())
            headings.insert(s.name.trimmed().toLower());
    }

    int pos = 0;
    const QString &text = script.text;
    for (;;)
    {
        const int nl = text.indexOf(QLatin1Char('\n'), pos);
        Line l;
        l.start = pos;
        l.text = nl < 0 ? text.mid(pos) : text.mid(pos, nl - pos);
        const QString t = l.text.trimmed();
        l.heading = t.startsWith(QLatin1Char('#')) || (!t.isEmpty() && headings.contains(t.toLower()));
        out.m_lines << l;
        if (nl < 0)
            break;
        pos = nl + 1;
    }

    // Standby markers before GO ones, in the order of the cues placed.
    for (int pass = 0; pass < 2; ++pass)
        for (const ScriptAnchor &a : script.anchors)
        {
            const bool go = pass == 1;
            const int offset = go ? a.go : a.standby;
            if (offset < 0)
                continue;
            const CueList *list = st->listOfCue(a.cue);
            const int idx = list == nullptr ? -1 : list->indexOf(a.cue);
            if (idx < 0)
                continue;
            const int li = out.lineOfOffset(offset);
            if (li < 0)
                continue;
            Marker m;
            m.go = go;
            m.cueId = a.cue;
            m.number = st->cueNumber(*list, idx);
            m.name = list->cues.at(idx).name;
            m.note = list->cues.at(idx).note;
            m.offset = offset;
            out.m_lines[li].markers << m;
        }
    return out;
}

int CallingScriptSheet::markerCount() const
{
    QSet<quint32> cues;
    for (const Line &l : m_lines)
        for (const Marker &m : l.markers)
            cues.insert(m.cueId);
    return cues.count();
}

int CallingScriptSheet::lineOfOffset(int offset) const
{
    if (offset < 0)
        return -1;
    for (int i = m_lines.count() - 1; i >= 0; --i)
        if (offset >= m_lines.at(i).start)
            return offset <= m_lines.at(i).start + m_lines.at(i).text.length() ? i : -1;
    return -1;
}

int CallingScriptSheet::lineOfCue(quint32 cueId, bool go) const
{
    for (int i = 0; i < m_lines.count(); ++i)
        for (const Marker &m : m_lines.at(i).markers)
            if (m.cueId == cueId && m.go == go)
                return i;
    return -1;
}

QString CallingScriptSheet::toHtml(const QString &heading) const
{
    QString h = QStringLiteral("<html><head><style>"
                               "body { font-family: Helvetica, Arial, sans-serif; font-size: 10pt; }"
                               "td { vertical-align: top; padding: 1px 4px; }"
                               "td.margin { width: 28%; color: #000; }"
                               "td.line { border-left: 1px solid #888; }"
                               "td.head { font-weight: bold; font-size: 12pt; border-left: 1px solid #888; }"
                               ".note { color: #555; font-style: italic; }"
                               "</style></head><body>");
    h += QStringLiteral("<h1>%1</h1>").arg(heading.toHtmlEscaped());
    h += QStringLiteral("<p>%1 · %2</p>").arg(QObject::tr("%n cue(s) placed", nullptr, markerCount()),
                                              QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat));
    h += QStringLiteral("<table cellspacing=\"0\" width=\"100%\">");
    for (const Line &l : m_lines)
    {
        QString margin;
        for (const Marker &m : l.markers)
        {
            margin += QStringLiteral("<b>%1 %2</b> %3").arg(m.go ? QObject::tr("GO") : QObject::tr("STBY"),
                                                          m.number.toHtmlEscaped(), m.name.toHtmlEscaped());
            if (!m.note.isEmpty())
                margin += QStringLiteral(" <span class=\"note\">%1</span>").arg(m.note.toHtmlEscaped());
            margin += QStringLiteral("<br>");
        }
        h += QStringLiteral("<tr><td class=\"margin\">%1</td><td class=\"%2\">%3</td></tr>")
                 .arg(margin.isEmpty() ? QStringLiteral("&nbsp;") : margin,
                      l.heading ? QStringLiteral("head") : QStringLiteral("line"),
                      l.text.isEmpty() ? QStringLiteral("&nbsp;") : l.text.toHtmlEscaped());
    }
    h += QStringLiteral("</table></body></html>");
    return h;
}

bool CallingScriptSheet::writePdf(const QString &path, const QString &heading, QString *error) const
{
    return CueSheet::writePdf(path, heading, toHtml(heading), error);
}
