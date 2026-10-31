/*
  Q Light Controller Plus
  timecodecues.cpp

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
#include <QStringList>

#include "timecodecues.h"
#include "timecodesource.h"
#include "showtreecontroller.h"
#include "showcontrol.h"
#include "doc.h"

using namespace ShowTree;

TimecodeCues::TimecodeCues(Doc *doc, TimecodeSource *source)
    : QObject(doc)
    , m_doc(doc)
    , m_source(source)
{
    connect(source, &TimecodeSource::timeChanged, this, &TimecodeCues::slotTime);
    if (doc->showTree() != nullptr)
        connect(doc->showTree(), &ShowTreeController::changed, this, &TimecodeCues::slotTreeChanged);
}

QString TimecodeCues::format(qint32 ms, int fps)
{
    if (fps <= 0)
        fps = 30;
    const quint32 u = quint32(qMax<qint32>(0, ms));
    const quint32 sec = u / 1000;
    return QStringLiteral("%1:%2:%3:%4")
        .arg(sec / 3600, 2, 10, QChar('0'))
        .arg((sec % 3600) / 60, 2, 10, QChar('0'))
        .arg(sec % 60, 2, 10, QChar('0'))
        .arg(int((u % 1000) * quint32(fps) / 1000), 2, 10, QChar('0'));
}

bool TimecodeCues::parse(const QString &text, int fps, qint32 *ms)
{
    if (fps <= 0)
        fps = 30;
    const QStringList parts = text.trimmed().split(QRegularExpression(QStringLiteral("[:;.]")));
    if (parts.size() < 2 || parts.size() > 4)
        return false;
    qint64 v[4] = { 0, 0, 0, 0 };
    const int skip = 4 - parts.size();
    for (int i = 0; i < parts.size(); ++i)
    {
        bool ok = false;
        const qint64 n = parts.at(i).trimmed().toLongLong(&ok);
        if (!ok || n < 0)
            return false;
        v[skip + i] = n;
    }
    if (v[1] > 59 || v[2] > 59 || v[3] >= fps)
        return false;
    const qint64 frameMs = (v[3] * 1000 + fps - 1) / fps;       // up: reads back the same
    const qint64 total = ((v[0] * 60 + v[1]) * 60 + v[2]) * 1000 + frameMs;
    if (total > 0x7FFFFFFF)
        return false;
    *ms = qint32(total);
    return true;
}

void TimecodeCues::rebuild() const
{
    m_entries.clear();
    m_any = false;
    m_dirty = false;
    const ShowTreeController *st = m_doc->showTree();
    if (st == nullptr)
        return;
    const quint32 current = st->currentProduction();
    int order = 0;
    for (const Production &p : st->productions())
    {
        if (current != 0xFFFFFFFF && p.id != current)
            continue;
        for (const CueList &l : p.lists)
        {
            if (l.looks || l.isSubCues() || !l.followTimecode)
                continue;
            m_any = true;
            for (int i = 0; i < l.cues.size(); ++i, ++order)
            {
                const Cue &c = l.cues.at(i);
                if (c.timecode < 0 || c.skip || l.chaser == 0xFFFFFFFF)
                    continue;
                m_entries << Entry{ c.timecode, l.chaser, st->cueNumber(l, i), order };
            }
        }
    }
}

bool TimecodeCues::anyFollowing() const
{
    if (m_dirty)
        rebuild();
    return m_any;
}

bool TimecodeCues::next(quint32 chaserId, QString *cue, qint32 *ms) const
{
    if (m_dirty)
        rebuild();
    const qint64 now = qint64(m_source->positionMs());
    const Entry *best = nullptr;
    for (const Entry &e : m_entries)
        if (e.chaser == chaserId && e.ms > now && (best == nullptr || e.ms < best->ms))
            best = &e;
    if (best == nullptr)
        return false;
    if (cue != nullptr)
        *cue = best->cue;
    if (ms != nullptr)
        *ms = best->ms;
    return true;
}

void TimecodeCues::slotTime(quint32 now)
{
    qint64 prev = m_last;
    m_last = now;
    if (prev < 0)
        prev = qint64(now) - 1;                 // joining: only a cue exactly here
    else if (qint64(now) < prev)
    {
        if (prev - qint64(now) <= kJitterMs)
            m_last = prev;                      // jitter: not a jump back
        return;                                 // a jump back: re-armed, nothing fires
    }
    if (qint64(now) == prev)
        return;
    if (m_dirty)
        rebuild();

    /* Of the cues passed, the latest one of each list (a jump forward
       fires only that). Collected first: run() may edit the tree. */
    QList<Entry> go;
    for (const Entry &e : m_entries)
    {
        if (e.ms <= prev || e.ms > qint64(now))
            continue;
        bool replaced = false;
        for (Entry &g : go)
            if (g.chaser == e.chaser)
            {
                if (e.ms > g.ms || (e.ms == g.ms && e.order > g.order))
                    g = e;
                replaced = true;
                break;
            }
        if (!replaced)
            go << e;
    }
    ShowControl *sc = m_doc->showControl();
    if (sc == nullptr)
        return;
    for (const Entry &e : go)
    {
        ShowControl::Request r;
        r.command = ShowControl::Go;
        r.cue = e.cue;
        r.chaser = e.chaser;
        sc->run(r, ShowControl::RouteTimecode);
        emit fired(e.chaser, e.cue);
    }
}
