/*
  Q Light Controller Plus
  markerimport.cpp

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

#include <QStringList>
#include <QColor>
#include <QRegularExpression>

#include <algorithm>
#include <climits>
#include <cmath>

#include "markerimport.h"
#include "show.h"

qint64 MarkerImport::parseTime(const QString &cell, double fps)
{
    QString s = cell.trimmed();
    if (s.isEmpty())
        return -1;
    if (fps <= 0.0)
        fps = 25.0;

    // plain seconds: 12 or 12.5
    static const QRegularExpression secs(QStringLiteral("^\\d+(\\.\\d+)?$"));
    if (secs.match(s).hasMatch())
    {
        // a bare integer is more likely a marker number than a time; take
        // it as seconds only with a decimal point
        if (s.contains('.') == false)
            return -1;
        return qint64(std::llround(s.toDouble() * 1000.0));
    }

    // ignore a Logic sub-frame tail: 01:00:10:12.00
    static const QRegularExpression sub(QStringLiteral("^(\\d+:\\d+:\\d+:\\d+)\\.\\d+$"));
    const QRegularExpressionMatch sm = sub.match(s);
    if (sm.hasMatch())
        s = sm.captured(1);

    static const QRegularExpression tc(QStringLiteral("^\\d+(:\\d+){1,3}(\\.\\d+)?$"));
    if (tc.match(s).hasMatch() == false)
        return -1;

    QString frac;
    const int dot = s.indexOf('.');
    if (dot >= 0)
    {
        frac = s.mid(dot);
        s = s.left(dot);
    }
    const QStringList p = s.split(':');
    double ms = 0;
    if (p.size() == 2)                      // mm:ss
        ms = (p[0].toInt() * 60 + p[1].toInt()) * 1000.0;
    else if (p.size() == 3)                 // hh:mm:ss(.mmm)
        ms = (p[0].toInt() * 3600 + p[1].toInt() * 60 + p[2].toInt()) * 1000.0;
    else                                    // hh:mm:ss:ff
        ms = (p[0].toInt() * 3600 + p[1].toInt() * 60 + p[2].toInt()) * 1000.0
             + p[3].toInt() * 1000.0 / fps;
    if (frac.isEmpty() == false)
        ms += frac.toDouble() * 1000.0;
    return qint64(std::llround(ms));
}

static QStringList splitLine(const QString &line)
{
    if (line.contains('\t'))
        return line.split('\t');

    // comma separated, with "quoted, names"
    QStringList cells;
    QString cur;
    bool quoted = false;
    for (int i = 0; i < line.size(); i++)
    {
        const QChar ch = line.at(i);
        if (ch == '"')
        {
            if (quoted && i + 1 < line.size() && line.at(i + 1) == '"')
            {
                cur += '"';
                i++;
            }
            else
                quoted = !quoted;
        }
        else if (ch == ',' && quoted == false)
        {
            cells << cur;
            cur.clear();
        }
        else
            cur += ch;
    }
    cells << cur;
    return cells;
}

MarkerImport::Result MarkerImport::parse(const QString &text, double fps)
{
    Result r;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("\r\n|\r|\n")));
    foreach (QString line, lines)
    {
        if (line.trimmed().isEmpty())
            continue;
        const QStringList cells = splitLine(line);

        qint64 t = -1;
        int timeCell = -1;
        for (int i = 0; i < cells.size() && t < 0; i++)
        {
            t = parseTime(cells.at(i), fps);
            if (t >= 0)
                timeCell = i;
        }
        if (t < 0)
        {
            r.skipped++;
            continue;
        }

        // the name: the first other cell that isn't empty, a time or a bare number
        QString label;
        for (int i = 0; i < cells.size() && label.isEmpty(); i++)
        {
            const QString c = cells.at(i).trimmed();
            bool isNum = false;
            c.toDouble(&isNum);
            if (i == timeCell || c.isEmpty() || isNum || parseTime(c, fps) >= 0)
                continue;
            label = c;
        }
        if (label.isEmpty())
            label = QObject::tr("Marker %1").arg(r.markers.size() + 1);

        Entry e;
        e.start = quint32(t);
        e.label = label;
        r.markers << e;
    }

    std::stable_sort(r.markers.begin(), r.markers.end(),
                     [](const Entry &a, const Entry &b) { return a.start < b.start; });

    // DAW time starts at 01:00:00:00: shift it down when all markers are past it
    if (r.markers.isEmpty() == false && r.markers.first().start >= 3600000u)
    {
        r.offsetMs = 3600000u;
        for (int i = 0; i < r.markers.size(); i++)
            r.markers[i].start -= r.offsetMs;
    }
    return r;
}

int MarkerImport::apply(Show *show, const Result &result)
{
    if (show == NULL)
        return 0;

    static const char *palette[] = { "#e0a820", "#4a90d9", "#5aa469", "#c0504d",
                                     "#8e6fb0", "#3fa8a0", "#d07030" };
    int added = 0;
    for (int i = 0; i < result.markers.size(); i++)
    {
        const Entry &e = result.markers.at(i);
        const quint32 end = (i + 1 < result.markers.size() && result.markers.at(i + 1).start > e.start)
                            ? result.markers.at(i + 1).start : e.start + 15000;
        show->setMarker(e.start, end, e.label, QColor(palette[(show->markers().count()) % 7]));
        added++;
    }
    return added;
}
