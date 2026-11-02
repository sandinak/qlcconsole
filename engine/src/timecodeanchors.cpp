/*
  Q Light Controller Plus
  timecodeanchors.cpp

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
#include <QVector>

#include "timecodeanchors.h"
#include "showtreecontroller.h"

using namespace ShowTree;

namespace
{

QString tr(const char *text, const char *disambiguation = nullptr, int n = -1)
{
    return QCoreApplication::translate("MarkerPlan", text, disambiguation, n);
}

const qint64 kHourMs = 3600000;

} // namespace

/****************************************************************************
 * MarkerPlan
 ****************************************************************************/

QString MarkerPlan::matchKey(const QString &name)
{
    QString out;
    out.reserve(name.size());
    for (const QChar c : name)
        out += c.isLetterOrNumber() ? c.toLower() : QChar(' ');
    return out.simplified();
}

QString MarkerPlan::formatDelta(qint64 ms)
{
    return QStringLiteral("%1%2 s").arg(ms < 0 ? QStringLiteral("-") : QStringLiteral("+"))
        .arg(qAbs(ms) / 1000.0, 0, 'f', 1);
}

QString MarkerPlan::formatTime(qint64 ms)
{
    if (ms < 0)
        ms = 0;
    const qint64 tenths = (ms + 50) / 100;
    const qint64 sec = tenths / 10;
    if (sec >= 3600)
        return QStringLiteral("%1:%2:%3.%4").arg(sec / 3600).arg((sec % 3600) / 60, 2, 10, QChar('0'))
            .arg(sec % 60, 2, 10, QChar('0')).arg(tenths % 10);
    return QStringLiteral("%1:%2.%3").arg(sec / 60).arg(sec % 60, 2, 10, QChar('0')).arg(tenths % 10);
}

bool MarkerPlan::changes() const
{
    for (const Move &m : moves)
        if (m.from != m.to)
            return true;
    return false;
}

QStringList MarkerPlan::lines() const
{
    QStringList out;
    for (const Move &m : moves)
    {
        if (m.from < 0)
            out << tr("%1 starts at %2, %n cue(s) count from it now", "", m.follow).arg(m.title, formatTime(m.to));
        else if (m.from == m.to)
            out << tr("%1 stays at %2").arg(m.title, formatTime(m.to));
        else
            out << tr("%1 moved %2, %n cue(s) follow", "", m.follow).arg(m.title, formatDelta(qint64(m.to) - m.from));
    }
    for (const QString &u : unmatched)
        out << tr("Marker not matched: %1").arg(u);
    for (const QString &s : missing)
        out << tr("No marker for %1: it stays where it is").arg(s);
    return out;
}

MarkerPlan MarkerPlan::make(const ShowTreeController *st, quint32 listId, const MidiMarkers::Result &markers)
{
    MarkerPlan plan;
    const CueList *list = st == nullptr ? nullptr : st->cueList(listId);
    if (list == nullptr)
        return plan;
    const QList<Section> secs = st->sections(listId);

    // Where the session starts on the clock.
    if (markers.smpteOffsetMs >= 0)
        plan.offsetMs = markers.smpteOffsetMs;
    else
    {
        bool startsPastHour = false, markersPastHour = false;
        for (const Section &s : secs)
            startsPastHour = startsPastHour || s.start >= kHourMs;
        for (const MidiMarkers::Marker &m : markers.markers)
            markersPastHour = markersPastHour || m.ms >= kHourMs;
        if (startsPastHour && !markersPastHour)
            plan.offsetMs = kHourMs;
    }

    QVector<bool> used(secs.count(), false);
    QVector<int> matchOf(secs.count(), -1);
    QVector<qint32> timeOf(secs.count(), -1);
    for (const MidiMarkers::Marker &m : markers.markers)
    {
        const QString key = matchKey(m.name);
        int hit = -1;
        for (int pass = 0; pass < 3 && hit < 0; ++pass)
            for (int i = 0; i < secs.count() && hit < 0; ++i)
            {
                if (used.at(i))
                    continue;
                const QString against = pass == 0 ? secs.at(i).name
                                        : pass == 1 ? sectionTitle(secs.at(i)) : secs.at(i).key;
                if (!against.isEmpty() && matchKey(against) == key)
                    hit = i;
            }
        const qint64 at = m.ms + plan.offsetMs;
        if (hit < 0)
        {
            plan.unmatched << tr("%1 at %2").arg(m.name, formatTime(at));
            continue;
        }
        used[hit] = true;
        timeOf[hit] = qint32(qBound<qint64>(qint64(0), at, qint64(0x7FFFFFFF)));
    }
    for (int i = 0; i < secs.count(); ++i)
    {
        if (!used.at(i))
        {
            plan.missing << sectionTitle(secs.at(i));
            continue;
        }
        Move mv;
        mv.section = i;
        mv.title = sectionTitle(secs.at(i));
        mv.from = secs.at(i).start;
        mv.to = timeOf.at(i);
        for (int c = secs.at(i).first; c <= secs.at(i).last && c < list->cues.count(); ++c)
        {
            const Cue &cue = list->cues.at(c);
            if (mv.from >= 0 ? cue.tcFromSection : cue.timecode >= 0)
                ++mv.follow;
        }
        for (const MidiMarkers::Marker &m : markers.markers)
            if (matchKey(m.name) == matchKey(secs.at(i).name.isEmpty() ? mv.title : secs.at(i).name))
            {
                mv.marker = m.name;
                break;
            }
        plan.moves << mv;
    }
    return plan;
}

bool MarkerPlan::apply(ShowTreeController *st, quint32 listId, const MarkerPlan &plan)
{
    if (st == nullptr || !plan.changes())
        return false;
    QList<QPair<int, qint32>> starts;
    for (const Move &m : plan.moves)
        if (m.from != m.to)
            starts << qMakePair(m.section, m.to);
    return st->setSectionStarts(listId, starts, QCoreApplication::translate("MarkerPlan", "Import Logic markers"));
}

/****************************************************************************
 * SectionLearn
 ****************************************************************************/

SectionLearn::SectionLearn(ShowTreeController *st, quint32 listId, int fromSection, qint32 after)
    : m_st(st)
    , m_list(listId)
    , m_section(-1)
    , m_last(after)
{
    const int n = st == nullptr ? 0 : int(st->sections(listId).count());
    if (fromSection >= 0 && fromSection < n)
        m_section = fromSection;
}

QString SectionLearn::sectionTitle() const
{
    if (m_section < 0)
        return QString();
    const QList<Section> secs = m_st->sections(m_list);
    return m_section < secs.count() ? ShowTree::sectionTitle(secs.at(m_section)) : QString();
}

bool SectionLearn::tap(qint32 ms)
{
    if (m_section < 0 || ms < 0 || (m_last >= 0 && ms < m_last))
        return false;
    const QList<Section> secs = m_st->sections(m_list);
    if (m_section >= secs.count())
    {
        m_section = -1;
        return false;
    }
    // The same time it has is a tap too: it is learned, on to the next.
    // A section getting its first start keeps its cues where they are,
    // counting from it from now on (as the marker import does).
    if (secs.at(m_section).start != ms)
        m_st->setSectionStarts(m_list, { qMakePair(m_section, ms) },
                               QCoreApplication::translate("SectionLearn", "Learn section start"));
    m_last = ms;
    ++m_learned;
    m_section = m_section + 1 < secs.count() ? m_section + 1 : -1;
    return true;
}
