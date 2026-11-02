/*
  Q Light Controller Plus
  showtreetimecode.cpp

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

/* Timecode anchors (Q198). A song section has a start on the incoming
   timecode, and a cue's time counts from it (Cue::tcFromSection, tcOffset):
   cut a verse for time and the sections after move by one number each, not
   every cue. Any cue can be an anchor (Cue::tcAnchor): the cues after it in
   its section count from it instead.

   Cue::timecode stays the absolute time a cue goes at -- resolved here
   after every edit -- so TimecodeCues, the cue page, the Run tab and the
   cue sheet read what they always read, and nothing is worked out per
   timecode frame. A show saved by this build plays the same in an older
   one (Timecode= is the resolved time). An older show keeps its absolute
   times until it is converted (convertToSectionTimes(), offered, never
   forced). */

#include <QCoreApplication>
#include <QSet>

#include "showtreecontroller.h"
#include "doc.h"

using namespace ShowTree;

/****************************************************************************
 * The walk: which base each cue counts from
 ****************************************************************************/

namespace
{

/* Per cue index: the section it is in (-1: none). */
QVector<int> sectionIndexes(const QList<Section> &secs, int count)
{
    QVector<int> out(count, -1);
    for (int s = 0; s < secs.count(); ++s)
        for (int i = qMax(0, secs.at(s).first); i <= secs.at(s).last && i < count; ++i)
            out[i] = s;
    return out;
}

QVector<qint32> startsOf(const QList<Section> &secs)
{
    QVector<qint32> out;
    out.reserve(secs.count());
    for (const Section &s : secs)
        out << s.start;
    return out;
}

QVector<qint32> timesOf(const CueList &list)
{
    QVector<qint32> out;
    out.reserve(list.cues.count());
    for (const Cue &c : list.cues)
        out << c.timecode;
    return out;
}

qint32 clampMs(qint64 ms)
{
    return ms < 0 ? 0 : (ms > 0x7FFFFFFF ? 0x7FFFFFFF : qint32(ms));
}

/* The head that holds @a section's start; made when it has none yet. */
SectionHead &headFor(CueList &list, const Section &section)
{
    for (SectionHead &h : list.sections)
        if (section.key.isEmpty() ? h.cue == section.headCue : h.key == section.key)
            return h;
    SectionHead h;
    h.key = section.key;
    h.cue = section.key.isEmpty() ? section.headCue : invalidId();
    list.sections << h;
    return list.sections.last();
}

/* Write @a starts into the heads of @a secs (a scheme's section with no
   name and no start needs no head). */
void writeStarts(CueList &list, const QList<Section> &secs, const QVector<qint32> &starts)
{
    for (int s = 0; s < secs.count(); ++s)
    {
        if (secs.at(s).start == starts.at(s))
            continue;
        headFor(list, secs.at(s)).start = starts.at(s);
    }
    for (int i = 0; i < list.sections.count(); ++i)
    {
        const SectionHead &h = list.sections.at(i);
        if (!h.key.isEmpty() && h.name.isEmpty() && h.start < 0)
            list.sections.removeAt(i--);
    }
}

/* Every cue at time @a want (-1: none), counting from where it counts from
   now: a cue timed from its section whose base has no time keeps its time
   as a time of its own. */
void encode(CueList &list, const QVector<int> &secOf, const QVector<qint32> &starts, const QVector<qint32> &want)
{
    qint32 base = -1;
    int cur = -2;
    for (int i = 0; i < list.cues.count(); ++i)
    {
        if (secOf.at(i) != cur)
        {
            cur = secOf.at(i);
            base = cur >= 0 ? starts.at(cur) : -1;
        }
        Cue &c = list.cues[i];
        const qint32 w = want.at(i);
        if (w < 0)
        {
            c.timecode = -1;
            c.tcFromSection = false;
            c.tcOffset = 0;
        }
        else
        {
            c.timecode = w;
            if (c.tcFromSection && base >= 0)
                c.tcOffset = w - base;
            else
            {
                c.tcFromSection = false;
                c.tcOffset = 0;
            }
        }
        if (c.tcAnchor && c.timecode >= 0)
            base = c.timecode;
    }
}

bool sameTimes(const CueList &a, const CueList &b)
{
    if (a.cues.count() != b.cues.count() || a.sections.count() != b.sections.count())
        return false;
    for (int i = 0; i < a.cues.count(); ++i)
    {
        const Cue &x = a.cues.at(i), &y = b.cues.at(i);
        if (x.timecode != y.timecode || x.tcFromSection != y.tcFromSection || x.tcOffset != y.tcOffset)
            return false;
    }
    for (int i = 0; i < a.sections.count(); ++i)
        if (a.sections.at(i).start != b.sections.at(i).start || a.sections.at(i).key != b.sections.at(i).key
            || a.sections.at(i).cue != b.sections.at(i).cue)
            return false;
    return true;
}

} // namespace

bool ShowTree::resolveTimecodes(CueList &list, Numbering numbering)
{
    bool any = false;
    for (const Cue &c : list.cues)
        if (c.tcFromSection)
        {
            any = true;
            break;
        }
    if (!any)
        return false;
    const QList<Section> secs = sectionsOf(list, numbering);
    const QVector<int> secOf = sectionIndexes(secs, int(list.cues.count()));
    bool changed = false;
    qint32 base = -1;
    int cur = -2;
    for (int i = 0; i < list.cues.count(); ++i)
    {
        if (secOf.at(i) != cur)
        {
            cur = secOf.at(i);
            base = cur >= 0 ? secs.at(cur).start : -1;
        }
        Cue &c = list.cues[i];
        if (c.tcFromSection && base >= 0)
        {
            const qint32 t = resolveTimecode(base, c.tcOffset);
            if (t != c.timecode)
            {
                c.timecode = t;
                changed = true;
            }
        }
        if (c.tcAnchor && c.timecode >= 0)
            base = c.timecode;
    }
    return changed;
}

bool ShowTree::setCueTimecodeAt(CueList &list, Numbering numbering, int index, qint32 ms)
{
    if (index < 0 || index >= list.cues.count())
        return false;
    ms = ms < 0 ? -1 : ms;
    Cue &c = list.cues[index];
    if (c.timecode == ms)
        return false;
    const QList<Section> secs = sectionsOf(list, numbering);
    const QVector<int> secOf = sectionIndexes(secs, int(list.cues.count()));
    if (ms < 0)
    {
        // Cleared: everything else stays where it is.
        QVector<qint32> want = timesOf(list);
        want[index] = -1;
        encode(list, secOf, startsOf(secs), want);
        return true;
    }
    // Its base: the section's start, or the last anchor before it in there.
    const int s = secOf.at(index);
    qint32 base = s >= 0 ? secs.at(s).start : -1;
    for (int i = s >= 0 ? secs.at(s).first : 0; i < index; ++i)
        if (secOf.at(i) == s && list.cues.at(i).tcAnchor && list.cues.at(i).timecode >= 0)
            base = list.cues.at(i).timecode;
    if (c.timecode < 0)
        c.tcFromSection = base >= 0;
    if (c.tcFromSection && base >= 0)
        c.tcOffset = ms - base;
    else
    {
        c.tcFromSection = false;
        c.tcOffset = 0;
    }
    const bool wasAnchor = c.tcAnchor && c.timecode >= 0;
    c.timecode = ms;
    if (!wasAnchor && c.tcAnchor)
    {
        // A first time on an anchor: the cues after keep their times.
        encode(list, secOf, startsOf(secs), timesOf(list));
        return true;
    }
    resolveTimecodes(list, numbering);      // the cues counting from it follow
    return true;
}

QList<TimecodeAnchor> ShowTree::anchorsOf(const CueList &list, Numbering numbering)
{
    QList<TimecodeAnchor> out;
    if (list.looks || list.isSubCues())
        return out;
    const QList<Section> secs = sectionsOf(list, numbering);
    const QVector<int> secOf = sectionIndexes(secs, int(list.cues.count()));
    int cur = -2;
    int baseAt = -1;            // index in out of the base cues count from
    for (int i = 0; i < list.cues.count(); ++i)
    {
        if (secOf.at(i) != cur)
        {
            cur = secOf.at(i);
            baseAt = -1;
            if (cur >= 0 && secs.at(cur).start >= 0)
            {
                TimecodeAnchor a;
                a.kind = TimecodeAnchor::SectionStart;
                a.section = cur;
                a.cue = secs.at(cur).headCue;
                a.name = sectionTitle(secs.at(cur));
                a.ms = secs.at(cur).start;
                out << a;
                baseAt = int(out.count()) - 1;
            }
        }
        const Cue &c = list.cues.at(i);
        if (c.tcFromSection && baseAt >= 0)
            ++out[baseAt].follow;
        if (c.tcAnchor && c.timecode >= 0)
        {
            TimecodeAnchor a;
            a.kind = TimecodeAnchor::CueAnchor;
            a.section = cur;
            a.cue = c.id;
            a.name = cueNumberAt(list, i);
            a.ms = c.timecode;
            out << a;
            baseAt = int(out.count()) - 1;
        }
    }
    return out;
}

bool ShowTree::setSectionStart(CueList &list, Numbering numbering, int section, qint32 ms, bool laterFollow)
{
    const QList<Section> secs = sectionsOf(list, numbering);
    if (section < 0 || section >= secs.count())
        return false;
    ms = ms < 0 ? -1 : ms;
    const qint32 old = secs.at(section).start;
    if (old == ms)
        return false;
    const CueList before = list;
    QVector<qint32> starts = startsOf(secs);
    starts[section] = ms;
    const bool follow = laterFollow && old >= 0 && ms >= 0;
    const qint64 delta = qint64(ms) - qint64(old);
    if (follow)
    {
        for (int s = section + 1; s < secs.count(); ++s)
            if (starts.at(s) >= 0)
                starts[s] = clampMs(starts.at(s) + delta);
        // Later cues with a time of their own move with them.
        for (int i = secs.at(section).last + 1; i < list.cues.count(); ++i)
        {
            Cue &c = list.cues[i];
            if (c.timecode >= 0 && !c.tcFromSection)
                c.timecode = clampMs(c.timecode + delta);
        }
    }
    writeStarts(list, secs, starts);
    resolveTimecodes(list, numbering);
    // A section that lost its start: its cues keep their times as their own.
    const QList<Section> now = sectionsOf(list, numbering);
    encode(list, sectionIndexes(now, int(list.cues.count())), startsOf(now), timesOf(list));
    return !sameTimes(before, list);
}

int ShowTree::absoluteTimedCues(const CueList &list, Numbering numbering)
{
    const QList<Section> secs = sectionsOf(list, numbering);
    const QVector<int> secOf = sectionIndexes(secs, int(list.cues.count()));
    int n = 0;
    for (int i = 0; i < list.cues.count(); ++i)
        if (secOf.at(i) >= 0 && list.cues.at(i).timecode >= 0 && !list.cues.at(i).tcFromSection)
            ++n;
    return n;
}

bool ShowTree::convertToSectionTimes(CueList &list, Numbering numbering, int *converted)
{
    const QList<Section> secs = sectionsOf(list, numbering);
    const QVector<int> secOf = sectionIndexes(secs, int(list.cues.count()));
    QVector<qint32> starts = startsOf(secs);
    const QVector<qint32> want = timesOf(list);
    for (int s = 0; s < secs.count(); ++s)
    {
        if (starts.at(s) >= 0)
            continue;
        qint32 first = -1;
        for (int i = secs.at(s).first; i <= secs.at(s).last; ++i)
            if (want.at(i) >= 0 && (first < 0 || want.at(i) < first))
                first = want.at(i);
        starts[s] = first;
    }
    int n = 0;
    for (int i = 0; i < list.cues.count(); ++i)
    {
        Cue &c = list.cues[i];
        if (secOf.at(i) < 0 || c.timecode < 0 || c.tcFromSection || starts.at(secOf.at(i)) < 0)
            continue;
        c.tcFromSection = true;
        ++n;
    }
    if (converted != nullptr)
        *converted = n;
    if (n == 0)
        return false;
    writeStarts(list, secs, starts);
    encode(list, secOf, starts, want);
    return true;
}

bool ShowTree::rippleTimecodes(CueList &list, Numbering numbering, int index, qint32 deltaMs)
{
    if (index < 0 || index >= list.cues.count() || deltaMs == 0)
        return false;
    const CueList before = list;
    const QList<Section> secs = sectionsOf(list, numbering);
    const QVector<int> secOf = sectionIndexes(secs, int(list.cues.count()));
    QVector<qint32> starts = startsOf(secs);
    QVector<qint32> want = timesOf(list);
    for (int i = index; i < want.count(); ++i)
        if (want.at(i) >= 0)
            want[i] = clampMs(qint64(want.at(i)) + deltaMs);
    for (int s = 0; s < secs.count(); ++s)
        if (secs.at(s).first >= index && starts.at(s) >= 0)
            starts[s] = clampMs(qint64(starts.at(s)) + deltaMs);
    writeStarts(list, secs, starts);
    encode(list, secOf, starts, want);
    return !sameTimes(before, list);
}

qint32 ShowTree::sectionLength(const CueList &list, Numbering numbering, int section)
{
    const QList<Section> secs = sectionsOf(list, numbering);
    if (section < 0 || section >= secs.count())
        return -1;
    const Section &s = secs.at(section);
    qint32 from = s.start, last = -1;
    for (int i = s.first; i <= s.last; ++i)
    {
        const qint32 t = list.cues.at(i).timecode;
        if (t < 0)
            continue;
        if (s.start < 0 && (from < 0 || t < from))
            from = t;
        last = qMax(last, t);
    }
    if (from < 0)
        return -1;
    qint32 end = -1;
    for (int n = section + 1; n < secs.count() && end < 0; ++n)
        end = secs.at(n).start;
    if (end < 0)
        end = last;
    return end > from ? end - from : -1;
}

bool ShowTree::stretchSection(CueList &list, Numbering numbering, int section, qint32 lengthMs, QString *why)
{
    const QList<Section> secs = sectionsOf(list, numbering);
    if (section < 0 || section >= secs.count())
    {
        if (why != nullptr)
            *why = QCoreApplication::translate("ShowTree", "No such section");
        return false;
    }
    const qint32 length = sectionLength(list, numbering, section);
    if (length <= 0)
    {
        if (why != nullptr)
            *why = QCoreApplication::translate("ShowTree", "It has no length to stretch: give it a start and a "
                                                           "later cue or section a time");
        return false;
    }
    if (lengthMs <= 0)
    {
        if (why != nullptr)
            *why = QCoreApplication::translate("ShowTree", "A section can't be stretched to nothing");
        return false;
    }
    const Section &s = secs.at(section);
    const QVector<int> secOf = sectionIndexes(secs, int(list.cues.count()));
    QVector<qint32> starts = startsOf(secs);
    QVector<qint32> want = timesOf(list);
    qint32 from = s.start;
    if (from < 0)
        for (int i = s.first; i <= s.last; ++i)
            if (want.at(i) >= 0 && (from < 0 || want.at(i) < from))
                from = want.at(i);
    for (int i = s.first; i <= s.last; ++i)
        if (want.at(i) >= 0)
            want[i] = clampMs(qint64(from) + (qint64(want.at(i) - from) * lengthMs + length / 2) / length);
    const qint64 diff = qint64(lengthMs) - length;
    for (int i = s.last + 1; i < want.count(); ++i)
        if (want.at(i) >= 0)
            want[i] = clampMs(want.at(i) + diff);
    for (int n = section + 1; n < secs.count(); ++n)
        if (starts.at(n) >= 0)
            starts[n] = clampMs(starts.at(n) + diff);
    const CueList before = list;
    writeStarts(list, secs, starts);
    encode(list, secOf, starts, want);
    if (sameTimes(before, list))
    {
        if (why != nullptr)
            *why = QCoreApplication::translate("ShowTree", "It is that long already");
        return false;
    }
    return true;
}

/****************************************************************************
 * ShowTreeController
 ****************************************************************************/

static Production *productionOfListT(QList<Production> &ps, quint32 listId)
{
    for (Production &p : ps)
        if (p.list(listId) != nullptr)
            return &p;
    return nullptr;
}

/* The index of @a section among @a secs (by its head cue, then its key). */
static int indexOfSection(const QList<Section> &secs, const Section &section)
{
    for (int i = 0; i < secs.count(); ++i)
        if (secs.at(i).headCue == section.headCue && secs.at(i).key == section.key)
            return i;
    for (int i = 0; i < secs.count(); ++i)
        if (!section.key.isEmpty() && secs.at(i).key == section.key)
            return i;
    return -1;
}

void ShowTreeController::resolveAllTimecodes(QList<Production> &ps)
{
    for (Production &p : ps)
        for (CueList &l : p.lists)
            if (!l.looks && !l.isSubCues())
                resolveTimecodes(l, p.numbering);
}

QList<TimecodeAnchor> ShowTreeController::timecodeAnchors(quint32 listId) const
{
    const CueList *l = cueList(listId);
    return l == nullptr ? QList<TimecodeAnchor>() : anchorsOf(*l, listNumbering(listId));
}

qint32 ShowTreeController::sectionStart(quint32 listId, const Section &section) const
{
    const QList<Section> secs = sections(listId);
    const int at = indexOfSection(secs, section);
    return at < 0 ? -1 : secs.at(at).start;
}

bool ShowTreeController::setSectionStart(quint32 listId, const Section &section, qint32 ms, bool laterFollow)
{
    const int at = indexOfSection(sections(listId), section);
    if (at < 0)
        return false;
    return edit(ms < 0 ? tr("Clear section start") : tr("Set section start"), [&](QList<Production> &ps) {
        Production *p = productionOfListT(ps, listId);
        CueList *l = p == nullptr ? nullptr : p->list(listId);
        return l != nullptr && ShowTree::setSectionStart(*l, listNumbering(listId), at, ms, laterFollow);
    });
}

bool ShowTreeController::setSectionStarts(quint32 listId, const QList<QPair<int, qint32>> &starts,
                                          const QString &label)
{
    if (starts.isEmpty())
        return false;
    const Numbering scheme = listNumbering(listId);
    return edit(label, [&](QList<Production> &ps) {
        Production *p = productionOfListT(ps, listId);
        CueList *l = p == nullptr ? nullptr : p->list(listId);
        if (l == nullptr)
            return false;
        const CueList before = *l;
        const QList<Section> secs = sectionsOf(*l, scheme);
        QVector<qint32> st = startsOf(secs);
        QSet<int> fresh;
        for (const QPair<int, qint32> &s : starts)
        {
            if (s.first < 0 || s.first >= secs.count() || s.second < 0)
                continue;
            if (st.at(s.first) < 0)
                fresh.insert(s.first);
            st[s.first] = s.second;
        }
        writeStarts(*l, secs, st);
        resolveTimecodes(*l, scheme);               // the cues of a moved section follow
        // A section that gets its first start: its cues count from it now,
        // each where it was.
        const QVector<int> secOf = sectionIndexes(secs, int(l->cues.count()));
        for (int i = 0; i < l->cues.count(); ++i)
            if (fresh.contains(secOf.at(i)) && l->cues.at(i).timecode >= 0)
                l->cues[i].tcFromSection = true;
        encode(*l, secOf, st, timesOf(*l));
        return !sameTimes(before, *l);
    });
}

bool ShowTreeController::setCueTimecodeAnchor(quint32 cueId, bool anchor)
{
    const CueList *have = listOfCue(cueId);
    if (have == nullptr)
        return false;
    const quint32 listId = have->id;
    const Numbering scheme = listNumbering(listId);
    return edit(anchor ? tr("Make cue a timecode anchor") : tr("Cue no longer a timecode anchor"),
                [&](QList<Production> &ps) {
        Production *p = productionOfListT(ps, listId);
        CueList *l = p == nullptr ? nullptr : p->list(listId);
        const int at = l == nullptr ? -1 : l->indexOf(cueId);
        if (at < 0 || l->looks || l->isSubCues() || l->cues.at(at).tcAnchor == anchor)
            return false;
        // Every cue stays where it is; the ones after it count from it now.
        const QVector<qint32> want = timesOf(*l);
        l->cues[at].tcAnchor = anchor;
        const QList<Section> secs = sectionsOf(*l, scheme);
        encode(*l, sectionIndexes(secs, int(l->cues.count())), startsOf(secs), want);
        return true;
    });
}

bool ShowTreeController::isCueTimecodeAnchor(quint32 cueId) const
{
    const Cue *c = cue(cueId);
    return c != nullptr && c->tcAnchor;
}

int ShowTreeController::absoluteTimedCues(quint32 listId) const
{
    const CueList *l = cueList(listId);
    return l == nullptr ? 0 : ShowTree::absoluteTimedCues(*l, listNumbering(listId));
}

bool ShowTreeController::convertToSectionTimes(quint32 listId, int *converted)
{
    int n = 0;
    const bool ok = edit(tr("Convert to section times"), [&](QList<Production> &ps) {
        Production *p = productionOfListT(ps, listId);
        CueList *l = p == nullptr ? nullptr : p->list(listId);
        return l != nullptr && ShowTree::convertToSectionTimes(*l, listNumbering(listId), &n);
    });
    if (converted != nullptr)
        *converted = ok ? n : 0;
    return ok;
}

bool ShowTreeController::rippleTimecode(quint32 cueId, qint32 deltaMs)
{
    const CueList *have = listOfCue(cueId);
    if (have == nullptr || deltaMs == 0)
        return false;
    const quint32 listId = have->id;
    return edit(deltaMs > 0 ? tr("Insert time") : tr("Remove time"), [&](QList<Production> &ps) {
        Production *p = productionOfListT(ps, listId);
        CueList *l = p == nullptr ? nullptr : p->list(listId);
        return l != nullptr && rippleTimecodes(*l, listNumbering(listId), l->indexOf(cueId), deltaMs);
    });
}

bool ShowTreeController::stretchSection(quint32 listId, const Section &section, qint32 lengthMs, QString *why)
{
    const int at = indexOfSection(sections(listId), section);
    if (at < 0)
    {
        if (why != nullptr)
            *why = tr("No such section");
        return false;
    }
    QString whyNot;
    const bool ok = edit(tr("Stretch section"), [&](QList<Production> &ps) {
        Production *p = productionOfListT(ps, listId);
        CueList *l = p == nullptr ? nullptr : p->list(listId);
        return l != nullptr && ShowTree::stretchSection(*l, listNumbering(listId), at, lengthMs, &whyNot);
    });
    if (why != nullptr)
        *why = whyNot;
    return ok;
}

bool ShowTreeController::cueTimecodeBase(quint32 cueId, QString *from, qint32 *baseMs) const
{
    const CueList *l = listOfCue(cueId);
    const int at = l == nullptr ? -1 : l->indexOf(cueId);
    if (at < 0 || !l->cues.at(at).tcFromSection)
        return false;
    const QList<Section> secs = sections(l->id);
    const QVector<int> secOf = sectionIndexes(secs, int(l->cues.count()));
    const int s = secOf.at(at);
    if (s < 0 || secs.at(s).start < 0)
        return false;
    QString name = sectionTitle(secs.at(s));
    qint32 base = secs.at(s).start;
    for (int i = secs.at(s).first; i < at; ++i)
        if (l->cues.at(i).tcAnchor && l->cues.at(i).timecode >= 0)
        {
            base = l->cues.at(i).timecode;
            name = tr("cue %1").arg(cueNumber(*l, i));
        }
    if (from != nullptr)
        *from = name;
    if (baseMs != nullptr)
        *baseMs = base;
    return true;
}

void ShowTreeController::checkTimecodeAnchors()
{
    QHash<quint32, QList<TimecodeAnchor>> now;
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            if (!l.looks && !l.isSubCues())
            {
                const QList<TimecodeAnchor> a = anchorsOf(l, p.numbering);
                if (!a.isEmpty())
                    now.insert(l.id, a);
            }
    QList<quint32> changedLists;
    for (auto it = now.cbegin(); it != now.cend(); ++it)
        if (m_anchors.value(it.key()) != it.value())
            changedLists << it.key();
    for (auto it = m_anchors.cbegin(); it != m_anchors.cend(); ++it)
        if (!now.contains(it.key()))
            changedLists << it.key();
    m_anchors = now;
    for (quint32 id : changedLists)
        emit timecodeAnchorsChanged(id);
}
