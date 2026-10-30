/*
  Q Light Controller Plus
  showtreesections.cpp

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

/* Song sections (Q190): a show is ONE cue list, its songs sections of it.
   A production's numbering scheme says what a section is -- the first level
   of {S}.{P}.{II}, the hundred of Eos / MA numbers, or a cue marked by hand
   (QLab) -- so sections follow the cue numbers rather than being kept by
   hand. Merge lists into one… turns a show built as a list per song into
   that one list, every cue playing as it did.

   Sections are how the list reads and numbers. The one thing they do
   play is tonight's running order (playOrder()): the chaser's steps come
   in that order, a song left out tonight is skipped, and no cue is moved
   or renumbered. "Block at
   section start" is the first cue's own mode. */

#include <QCoreApplication>
#include <QSet>

#include "showtreecontroller.h"
#include "doc.h"

using namespace ShowTree;

/****************************************************************************
 * The scheme
 ****************************************************************************/

QString ShowTree::numberingToString(Numbering numbering)
{
    switch (numbering)
    {
        case Numbering::ShowPartIteration: return QStringLiteral("ShowPartIteration");
        case Numbering::Hundreds: return QStringLiteral("Hundreds");
        case Numbering::Manual: return QStringLiteral("Manual");
        case Numbering::None: break;
    }
    return QStringLiteral("None");
}

Numbering ShowTree::stringToNumbering(const QString &str)
{
    if (str.compare(QLatin1String("ShowPartIteration"), Qt::CaseInsensitive) == 0)
        return Numbering::ShowPartIteration;
    if (str.compare(QLatin1String("Hundreds"), Qt::CaseInsensitive) == 0)
        return Numbering::Hundreds;
    if (str.compare(QLatin1String("Manual"), Qt::CaseInsensitive) == 0)
        return Numbering::Manual;
    return Numbering::None;
}

static bool derived(Numbering numbering)
{
    return numbering == Numbering::ShowPartIteration || numbering == Numbering::Hundreds;
}

QString ShowTree::sectionKey(Numbering numbering, const QString &number)
{
    if (!derived(numbering) || !isCueNumber(number))
        return QString();
    bool ok = false;
    const qlonglong first = number.section(QLatin1Char('.'), 0, 0).toLongLong(&ok);
    if (!ok)
        return QString();
    if (numbering == Numbering::Hundreds)
        return QString::number((first / 100) * 100);
    return QString::number(first);
}

/* The first number past section @a key: where the next one begins. */
static QString sectionEnd(Numbering numbering, const QString &key)
{
    const qlonglong k = key.toLongLong();
    return QString::number(numbering == Numbering::Hundreds ? k + 100 : k + 1);
}

/****************************************************************************
 * Sections of a list
 ****************************************************************************/

QList<Section> ShowTree::sectionsOf(const CueList &list, Numbering numbering)
{
    QList<Section> out;
    if (numbering == Numbering::None || list.looks || list.isSubCues())
        return out;

    if (numbering == Numbering::Manual)
    {
        QHash<quint32, QString> heads;
        for (const SectionHead &h : list.sections)
            if (h.cue != invalidId())
                heads.insert(h.cue, h.name);
        for (int i = 0; i < list.cues.count(); ++i)
        {
            const quint32 id = list.cues.at(i).id;
            if (heads.contains(id))
            {
                Section s;
                s.headCue = id;
                s.name = heads.value(id);
                s.first = s.last = i;
                out << s;
            }
            else if (!out.isEmpty())
                out.last().last = i;
        }
        return out;
    }

    QHash<QString, QString> names;
    for (const SectionHead &h : list.sections)
        if (!h.key.isEmpty() && !names.contains(h.key))
            names.insert(h.key, h.name);
    for (int i = 0; i < list.cues.count(); ++i)
    {
        const QString key = sectionKey(numbering, cueNumberAt(list, i));
        if (out.isEmpty() || out.last().key != key)
        {
            Section s;
            s.key = key;
            s.headCue = list.cues.at(i).id;
            s.name = names.value(key);
            s.first = s.last = i;
            out << s;
        }
        else
            out.last().last = i;
    }
    return out;
}

int ShowTree::sectionOfIndex(const QList<Section> &sections, int index)
{
    for (int i = 0; i < sections.count(); ++i)
        if (index >= sections.at(i).first && index <= sections.at(i).last)
            return i;
    return -1;
}

QString ShowTree::sectionTitle(const Section &section)
{
    if (section.key.isEmpty())
        return section.name.isEmpty() ? QCoreApplication::translate("ShowTree", "Section") : section.name;
    return section.name.isEmpty() ? section.key
                                  : QStringLiteral("%1 — %2").arg(section.key, section.name);
}

QString ShowTree::sectionId(const Section &section)
{
    if (!section.key.isEmpty())
        return section.key;
    return section.headCue == invalidId() ? QString() : QStringLiteral("#%1").arg(section.headCue);
}

QList<RunningSong> ShowTree::resolveRunningOrder(const QList<Section> &sections, const QList<RunningSong> &order)
{
    QStringList natural;                // each id once, in number order
    for (const Section &s : sections)
        if (!natural.contains(sectionId(s)))
            natural << sectionId(s);

    QList<RunningSong> out;
    QSet<QString> seen;
    for (const RunningSong &r : order)
        if (natural.contains(r.id) && !seen.contains(r.id))
        {
            out << r;
            seen.insert(r.id);
        }
    // A section the order doesn't know (a new song): after the one before it.
    for (int i = 0; i < natural.count(); ++i)
    {
        if (seen.contains(natural.at(i)))
            continue;
        int at = 0;
        for (int k = i - 1; k >= 0; --k)
            if (seen.contains(natural.at(k)))
            {
                for (int j = 0; j < out.count(); ++j)
                    if (out.at(j).id == natural.at(k))
                        at = j + 1;
                break;
            }
        RunningSong r;
        r.id = natural.at(i);
        out.insert(at, r);
        seen.insert(r.id);
    }
    return out;
}

QList<int> ShowTree::playOrder(const CueList &list, Numbering numbering, const QList<RunningSong> &order,
                               QList<bool> *skipped)
{
    QList<int> out;
    for (int i = 0; i < list.cues.count(); ++i)
        out << i;
    if (skipped != nullptr)
        *skipped = QList<bool>(list.cues.count(), false);
    if (order.isEmpty())
        return out;
    const QList<Section> sections = sectionsOf(list, numbering);
    if (sections.isEmpty())
        return out;

    // Cues of no section stay in front, then each song's cues.
    QHash<QString, QList<int>> byId;
    for (int i = 0; i < list.cues.count(); ++i)
    {
        const int si = sectionOfIndex(sections, i);
        byId[si < 0 ? QString() : sectionId(sections.at(si))] << i;
    }
    // Manual: sectionOfIndex covers a range, so a run of one id may be several.
    const QList<RunningSong> resolved = resolveRunningOrder(sections, order);
    QList<int> result = byId.value(QString());
    for (const RunningSong &r : resolved)
    {
        const QList<int> cues = byId.value(r.id);
        result << cues;
        if (skipped != nullptr && r.skip)
            for (int i : cues)
                (*skipped)[i] = true;
    }
    if (result.count() != out.count())
        return out;                     // never lose a cue
    return result;
}

Numbering ShowTree::suggestNumbering(const Production &production)
{
    int total = 0, deep = 0, hundreds = 0;
    QSet<qlonglong> bands;
    for (const CueList &list : production.lists)
    {
        if (list.looks || list.isSubCues())
            continue;
        for (const Cue &c : list.cues)
        {
            if (!isCueNumber(c.number))
                continue;
            ++total;
            const QStringList parts = c.number.split(QLatin1Char('.'));
            if (parts.count() >= 3)
                ++deep;
            const qlonglong whole = parts.first().toLongLong();
            if (whole >= 100)
            {
                ++hundreds;
                bands.insert(whole / 100);
            }
        }
    }
    if (total < 2)
        return Numbering::None;
    if (deep * 2 >= total)
        return Numbering::ShowPartIteration;
    if (hundreds * 2 >= total && bands.count() >= 2)
        return Numbering::Hundreds;
    return Numbering::None;
}

/****************************************************************************
 * Numbering into a section
 ****************************************************************************/

QString ShowTree::numberInSection(const CueList &list, Numbering numbering, int afterIndex,
                                  const QString &key, const QStringList &taken)
{
    if (!derived(numbering) || key.isEmpty())
        return QString();
    QStringList used = taken;
    for (int i = 0; i < list.cues.count(); ++i)
        used << cueNumberAt(list, i);

    const int count = int(list.cues.count());
    const QString prev = afterIndex >= 0 && afterIndex < count ? cueNumberAt(list, afterIndex) : QString();
    const QString next = afterIndex + 1 >= 0 && afterIndex + 1 < count ? cueNumberAt(list, afterIndex + 1)
                                                                     : QString();
    auto fits = [&](const QString &c) {
        if (c.isEmpty() || sectionKey(numbering, c) != key)
            return false;
        if (!prev.isEmpty() && compareCueNumbers(c, prev) <= 0)
            return false;
        if (!next.isEmpty() && compareCueNumbers(c, next) >= 0)
            return false;
        for (const QString &u : used)
            if (compareCueNumbers(u, c) == 0)
                return false;
        return true;
    };
    const QString hi = sectionEnd(numbering, key);
    const QString top = prev.isEmpty() || sectionKey(numbering, prev) != key ? QString() : prev;
    const QString bound = !next.isEmpty() && sectionKey(numbering, next) == key ? next : hi;

    QStringList candidates;
    if (!top.isEmpty())
        candidates << numberBetween(top, bound, used);
    else if (bound == hi)
    {
        // An empty section: its first cue, {S}.0.1 (music starts) or the
        // hundred itself.
        candidates << (numbering == Numbering::ShowPartIteration ? key + QStringLiteral(".0.1") : key)
                   << key << numberBetween(key, hi, used);
    }
    else
        candidates << numberBetween(QString(), bound, used) << key << numberBetween(key, bound, used);
    for (const QString &c : candidates)
        if (fits(c))
            return c;
    return QString();
}

QString ShowTree::numberForInsertIn(const CueList &list, Numbering numbering, int afterIndex,
                                    const QStringList &taken)
{
    const QString plain = numberForInsert(list, afterIndex, taken);
    if (!derived(numbering) || list.looks || list.isSubCues() || list.cues.isEmpty())
        return plain;
    const int at = qBound(0, afterIndex, int(list.cues.count()) - 1);
    const QString key = sectionKey(numbering, cueNumberAt(list, at));
    if (key.isEmpty() || sectionKey(numbering, plain) == key)
        return plain;
    const QString in = numberInSection(list, numbering, qMin(afterIndex, int(list.cues.count()) - 1), key, taken);
    return in.isEmpty() ? plain : in;
}

bool ShowTree::nextSection(const CueList &list, Numbering numbering, QString *key, QString *number)
{
    if (!derived(numbering))
        return false;
    qlonglong high = -1;
    for (int i = 0; i < list.cues.count(); ++i)
    {
        const QString k = sectionKey(numbering, cueNumberAt(list, i));
        if (!k.isEmpty())
            high = qMax(high, k.toLongLong());
    }
    QString k;
    if (numbering == Numbering::ShowPartIteration)
        k = QString::number(high + 1);      // an empty list starts at 0: setup, preshow
    else
        k = QString::number(high < 0 ? 100 : high + 100);
    if (key != nullptr)
        *key = k;
    if (number != nullptr)
        *number = numberInSection(list, numbering, int(list.cues.count()) - 1, k);
    return true;
}

/****************************************************************************
 * Merge lists into one
 ****************************************************************************/

/* The full numbers of cue @a cueId's sub-cues, when it is numbered @a number. */
static QStringList subNumbers(const Production &p, quint32 cueId, const QString &number)
{
    QStringList out;
    for (const CueList &s : p.lists)
        if (s.subOf == cueId)
            for (int k = 0; k < s.cues.count(); ++k)
                out << cueNumberAt(s, k, number);
    return out;
}

static bool numberUsed(const QStringList &used, const QString &number)
{
    for (const QString &u : used)
        if (compareCueNumbers(u, number) == 0)
            return true;
    return false;
}

bool ShowTree::mergeLists(Production &production, const QList<quint32> &listIds, MergeReport *report,
                          QString *why)
{
    auto no = [why](const QString &text) {
        if (why != nullptr)
            *why = text;
        return false;
    };
    if (listIds.count() < 2)
        return no(QCoreApplication::translate("ShowTree", "Pick two cue lists or more to merge."));
    QList<const CueList *> lists;
    for (quint32 id : listIds)
    {
        const CueList *l = production.list(id);
        if (l == nullptr || l->looks || l->isSubCues())
            return no(QCoreApplication::translate("ShowTree", "Only cue lists of this production merge."));
        if (lists.contains(l))
            return no(QCoreApplication::translate("ShowTree", "A cue list is picked twice."));
        lists << l;
    }

    MergeReport r;
    Numbering scheme = production.numbering;
    if (scheme == Numbering::None)
    {
        scheme = suggestNumbering(production);
        if (scheme == Numbering::None)
            scheme = Numbering::Manual;
    }

    // What every cue puts on stage now, to check the merge by.
    QHash<quint32, Stage> before;
    QHash<quint32, QString> labels;
    for (const CueList *l : lists)
    {
        const QList<ResolvedCue> res = resolve(*l);
        for (int i = 0; i < res.count(); ++i)
        {
            before.insert(res.at(i).cueId, stageOf(res.at(i)));
            labels.insert(l->cues.at(i).id, QStringLiteral("%1 %2").arg(l->name, cueNumberAt(*l, i)));
        }
    }

    CueList merged = *lists.first();
    merged.cues.clear();
    merged.sections.clear();
    // The sections the lists already had stay (the first of a key wins).
    for (const CueList *l : lists)
        for (const SectionHead &h : l->sections)
        {
            if ((scheme == Numbering::Manual) != (h.cue != invalidId()))
                continue;
            bool have = false;
            for (const SectionHead &s : merged.sections)
                have = have || (h.cue != invalidId() ? s.cue == h.cue : s.key == h.key);
            if (!have)
                merged.sections << h;
        }

    QStringList used;
    bool mibOnNoted = false;
    for (int li = 0; li < lists.count(); ++li)
    {
        const CueList &l = *lists.at(li);
        bool started = li == 0;
        for (int i = 0; i < l.cues.count(); ++i)
        {
            Cue c = l.cues.at(i);
            // A cue with no number answered to its position: keep that number.
            c.number = cueNumberAt(l, i);
            const CueMode was = effectiveMode(l, l.cues.at(i));
            if (c.mode == CueMode::ListDefault && effectiveMode(merged, c) != was)
                c.mode = was;
            // The list's start: from nothing, as it did on its own -- up to
            // the first cue that plays and passes something on.
            if (!started)
            {
                if (was != CueMode::Block)
                {
                    c.mode = CueMode::Block;
                    r.blocked << c.id;
                }
                if (!c.skip && was != CueMode::CueOnly)
                    started = true;
            }
            if (c.colour.isEmpty() && !l.colour.isEmpty() && l.colour != merged.colour)
                c.colour = l.colour;
            if (li > 0 && l.moveInBlack == MoveInBlack::Off && merged.moveInBlack != MoveInBlack::Off)
                c.noMoveInBlack = true;
            if (li > 0 && l.moveInBlack == MoveInBlack::On && merged.moveInBlack != MoveInBlack::On
                && !mibOnNoted)
            {
                mibOnNoted = true;
                r.notes << QCoreApplication::translate("ShowTree", "%1 had move in black On: the merged list "
                                                                   "follows %2's setting.").arg(l.name, merged.name);
            }

            // Its number, unless it clashes (with a cue or a sub-cue).
            QStringList mine = QStringList(c.number) << subNumbers(production, c.id, c.number);
            bool clash = false;
            for (const QString &n : mine)
                clash = clash || numberUsed(used, n);
            if (clash)
            {
                const QString prev = merged.cues.isEmpty() ? QString() : merged.cues.last().number;
                const QString next = i + 1 < l.cues.count() ? cueNumberAt(l, i + 1) : QString();
                QString n;
                for (const QString &upTo : { next, QString() })
                {
                    if (!n.isEmpty())
                        break;
                    if (!upTo.isEmpty() && !prev.isEmpty() && compareCueNumbers(upTo, prev) <= 0)
                        continue;
                    QStringList avoid = used;
                    for (int k = i + 1; k < l.cues.count(); ++k)
                        avoid << cueNumberAt(l, k);
                    n = numberBetween(prev, upTo, avoid);
                    if (!n.isEmpty() && numberUsed(used, n))
                        n.clear();
                }
                if (n.isEmpty())
                    n = (prev.isEmpty() ? c.number : prev) + QStringLiteral(".5.5");
                r.renumbered << QCoreApplication::translate("ShowTree", "%1 %2 → %3").arg(l.name, c.number, n);
                c.number = n;
                mine = QStringList(c.number) << subNumbers(production, c.id, c.number);
            }
            used << mine;
            merged.cues << c;
        }

        // The list becomes a section named after it.
        if (!l.cues.isEmpty())
        {
            const Cue &head = merged.cues.at(merged.cues.count() - l.cues.count());
            SectionHead h;
            h.name = l.name;
            if (scheme == Numbering::Manual)
            {
                h.cue = head.id;
                bool have = false;
                for (const SectionHead &s : merged.sections)
                    have = have || s.cue == h.cue;
                if (!have)
                    merged.sections << h;
            }
            else
            {
                h.key = sectionKey(scheme, head.number);
                bool have = false;
                for (const SectionHead &s : merged.sections)
                    have = have || s.key == h.key;
                if (!have && !h.key.isEmpty())
                    merged.sections << h;
            }
        }

        if (li > 0 && l.baseCue != invalidId() && l.baseCue != merged.baseCue)
            r.notes << QCoreApplication::translate("ShowTree", "%1's base cue: layers released in it now go back "
                                                               "to %2.")
                       .arg(l.name, merged.baseCue == invalidId()
                                    ? QCoreApplication::translate("ShowTree", "dark (the list has no base cue)")
                                    : QCoreApplication::translate("ShowTree", "the merged list's base cue"));
    }
    // One list for the show: named after it.
    if (!production.name.isEmpty())
        merged.name = production.name;

    // Check: every cue plays as it did.
    QStringList differ;
    const QList<ResolvedCue> after = resolve(merged);
    for (const ResolvedCue &rc : after)
        if (before.contains(rc.cueId) && stageOf(rc) != before.value(rc.cueId))
            differ << labels.value(rc.cueId);
    if (!differ.isEmpty())
        r.notes << QCoreApplication::translate("ShowTree", "%n cue(s) play differently: %1", "", int(differ.count()))
                   .arg(differ.mid(0, 6).join(QStringLiteral(", ")));

    // Into the production: the first list's place, the others gone.
    const quint32 keep = listIds.first();
    for (int i = int(production.lists.count()) - 1; i >= 0; --i)
    {
        const quint32 id = production.lists.at(i).id;
        if (id == keep)
            production.lists[i] = merged;
        else if (listIds.contains(id))
            production.lists.removeAt(i);
    }
    if (production.mainList == invalidId() || listIds.contains(production.mainList))
        production.mainList = keep;
    production.numbering = scheme;
    if (report != nullptr)
        *report = r;
    return true;
}

/****************************************************************************
 * The controller
 ****************************************************************************/

static Production *productionOfList(QList<Production> &ps, quint32 listId)
{
    for (Production &p : ps)
        if (p.list(listId) != nullptr)
            return &p;
    return nullptr;
}

Numbering ShowTreeController::numbering(quint32 productionId) const
{
    const Production *p = production(productionId);
    return p == nullptr ? Numbering::None : p->numbering;
}

bool ShowTreeController::setNumbering(quint32 productionId, Numbering numbering)
{
    const Production *have = production(productionId);
    if (have == nullptr || have->numbering == numbering)
        return false;
    return edit(tr("Numbering scheme"), [&](QList<Production> &ps) {
        for (Production &p : ps)
            if (p.id == productionId)
            {
                p.numbering = numbering;
                return true;
            }
        return false;
    });
}

QList<RunningSong> ShowTreeController::runningOrder(quint32 productionId, QList<Section> *sectionsOut) const
{
    const Production *p = production(productionId);
    const quint32 lid = mainList(productionId);
    const CueList *l = p == nullptr ? nullptr : cueList(lid);
    if (l == nullptr)
        return QList<RunningSong>();
    const QList<Section> secs = sectionsOf(*l, p->numbering);
    QList<Section> unique;
    QStringList ids;
    for (const Section &s : secs)
        if (!ids.contains(sectionId(s)))
        {
            ids << sectionId(s);
            unique << s;
        }
    const QList<RunningSong> out = resolveRunningOrder(secs, p->runningOrder);
    if (sectionsOut != nullptr)
    {
        sectionsOut->clear();
        for (const RunningSong &r : out)
            *sectionsOut << unique.at(ids.indexOf(r.id));
    }
    return out;
}

bool ShowTreeController::hasRunningOrder(quint32 productionId) const
{
    const Production *p = production(productionId);
    return p != nullptr && !p->runningOrder.isEmpty();
}

bool ShowTreeController::setRunningOrder(quint32 productionId, const QList<RunningSong> &order)
{
    const Production *have = production(productionId);
    if (have == nullptr)
        return false;
    // Number order with every song in is no order at all.
    const CueList *main = cueList(mainList(productionId));
    if (main == nullptr)
        return false;
    const QList<Section> secs = sectionsOf(*main, have->numbering);
    QList<RunningSong> natural;
    for (const Section &s : secs)
    {
        RunningSong r;
        r.id = sectionId(s);
        if (!natural.contains(r))
            natural << r;
    }
    QList<RunningSong> value = order;
    if (value == natural)
        value.clear();
    if (value == have->runningOrder)
        return false;
    return edit(tr("Running order"), [&](QList<Production> &ps) {
        for (Production &p : ps)
            if (p.id == productionId)
            {
                p.runningOrder = value;
                return true;
            }
        return false;
    });
}

bool ShowTreeController::resetRunningOrder(quint32 productionId)
{
    return setRunningOrder(productionId, QList<RunningSong>());
}

QList<int> ShowTreeController::playOrderOf(quint32 listId, QList<bool> *skipped) const
{
    quint32 pid = invalidId();
    const CueList *l = cueList(listId, &pid);
    const Production *p = l == nullptr ? nullptr : production(pid);
    if (l == nullptr || p == nullptr || p->runningOrder.isEmpty())
    {
        QList<int> out;
        const int n = l == nullptr ? 0 : l->cues.count();
        for (int i = 0; i < n; ++i)
            out << i;
        if (skipped != nullptr)
            *skipped = QList<bool>(n, false);
        return out;
    }
    return playOrder(*l, listNumbering(listId), p->runningOrder, skipped);
}

Numbering ShowTreeController::listNumbering(quint32 listId) const
{
    quint32 pid = invalidId();
    const CueList *l = cueList(listId, &pid);
    if (l == nullptr || l->looks || l->isSubCues())
        return Numbering::None;
    return numbering(pid);
}

QList<Section> ShowTreeController::sections(quint32 listId) const
{
    const CueList *l = cueList(listId);
    return l == nullptr ? QList<Section>() : sectionsOf(*l, listNumbering(listId));
}

bool ShowTreeController::sectionOfCue(quint32 cueId, Section *out) const
{
    const CueList *l = listOfCue(cueId);
    if (l == nullptr)
        return false;
    const QList<Section> all = sections(l->id);
    const int at = sectionOfIndex(all, l->indexOf(cueId));
    if (at < 0)
        return false;
    if (out != nullptr)
        *out = all.at(at);
    return true;
}

/* The index of the head in @a list that names @a section (-1: none yet). */
static int headOf(const CueList &list, const Section &section)
{
    for (int i = 0; i < list.sections.count(); ++i)
    {
        const SectionHead &h = list.sections.at(i);
        if (section.key.isEmpty() ? h.cue == section.headCue : h.key == section.key)
            return i;
    }
    return -1;
}

bool ShowTreeController::setSectionName(quint32 listId, const Section &section, const QString &name)
{
    const QString n = name.trimmed();
    if (section.name == n)
        return false;
    return edit(tr("Name section"), [&](QList<Production> &ps) {
        Production *p = productionOfList(ps, listId);
        CueList *l = p == nullptr ? nullptr : p->list(listId);
        if (l == nullptr)
            return false;
        const int at = headOf(*l, section);
        if (at >= 0)
        {
            // A scheme's section with no name needs no entry.
            if (n.isEmpty() && !section.key.isEmpty())
                l->sections.removeAt(at);
            else
                l->sections[at].name = n;
            return true;
        }
        SectionHead h;
        h.key = section.key;
        h.cue = section.key.isEmpty() ? section.headCue : invalidId();
        h.name = n;
        l->sections << h;
        return true;
    });
}

quint32 ShowTreeController::addSection(quint32 listId, const QString &name, quint32 atCue)
{
    const Numbering scheme = listNumbering(listId);
    const CueList *have = cueList(listId);
    if (have == nullptr || scheme == Numbering::None)
        return invalidId();
    if (scheme == Numbering::Manual && atCue != invalidId() && have->indexOf(atCue) < 0)
        return invalidId();
    const quint32 cueId = atCue != invalidId() && scheme == Numbering::Manual ? atCue : newId();
    const bool ok = edit(tr("New section"), [&](QList<Production> &ps) {
        Production *p = productionOfList(ps, listId);
        CueList *l = p == nullptr ? nullptr : p->list(listId);
        if (l == nullptr)
            return false;
        SectionHead h;
        h.name = name.trimmed();
        if (cueId != atCue)
        {
            Cue c;
            c.id = cueId;
            if (scheme == Numbering::Manual)
                c.number = numberForInsert(*l, int(l->cues.count()) - 1, takenNumbers(ps, *l));
            else if (!nextSection(*l, scheme, &h.key, &c.number) || c.number.isEmpty())
                return false;
            l->cues << c;
        }
        if (scheme == Numbering::Manual)
        {
            h.cue = cueId;
            for (const SectionHead &s : l->sections)
                if (s.cue == cueId)
                    return false;
        }
        else
        {
            for (int i = 0; i < l->sections.count(); ++i)
                if (l->sections.at(i).key == h.key)
                    l->sections.removeAt(i--);
        }
        if (scheme == Numbering::Manual || !h.name.isEmpty())
            l->sections << h;
        return true;
    });
    return ok ? cueId : invalidId();
}

bool ShowTreeController::removeSection(quint32 listId, quint32 headCue)
{
    return edit(tr("Remove section"), [&](QList<Production> &ps) {
        Production *p = productionOfList(ps, listId);
        CueList *l = p == nullptr ? nullptr : p->list(listId);
        if (l == nullptr)
            return false;
        for (int i = 0; i < l->sections.count(); ++i)
            if (l->sections.at(i).cue == headCue)
            {
                l->sections.removeAt(i);
                return true;
            }
        return false;
    });
}

quint32 ShowTreeController::addCueInSection(quint32 listId, const Section &section, const QString &name)
{
    const Numbering scheme = listNumbering(listId);
    const CueList *have = cueList(listId);
    if (have == nullptr || section.first < 0)
        return invalidId();
    const quint32 id = newId();
    const bool ok = edit(tr("New cue"), [&](QList<Production> &ps) {
        Production *p = productionOfList(ps, listId);
        CueList *l = p == nullptr ? nullptr : p->list(listId);
        if (l == nullptr)
            return false;
        Cue c;
        c.id = id;
        c.name = name;
        c.number = numberInSection(*l, scheme, section.first - 1, section.key, takenNumbers(ps, *l));
        if (c.number.isEmpty())
            c.number = numberForInsert(*l, section.first - 1, takenNumbers(ps, *l));
        l->cues.insert(qBound(0, section.first, int(l->cues.count())), c);
        // Manual: the new cue at the top begins the section in its head's place.
        if (scheme == Numbering::Manual)
            for (SectionHead &h : l->sections)
                if (h.cue == section.headCue)
                    h.cue = id;
        return true;
    });
    return ok ? id : invalidId();
}

bool ShowTreeController::sectionBlocks(quint32 listId, const Section &section) const
{
    const CueList *l = cueList(listId);
    const Cue *c = l == nullptr ? nullptr : l->cue(section.headCue);
    return c != nullptr && effectiveMode(*l, *c) == CueMode::Block;
}

bool ShowTreeController::setSectionBlock(quint32 listId, const Section &section, bool block)
{
    const CueList *l = cueList(listId);
    const Cue *c = l == nullptr ? nullptr : l->cue(section.headCue);
    if (c == nullptr || sectionBlocks(listId, section) == block)
        return false;
    return setCueMode(c->id, block ? CueMode::Block : CueMode::ListDefault);
}

bool ShowTreeController::mergeLists(quint32 productionId, const QList<quint32> &listIds, MergeReport *report,
                                    QString *why)
{
    MergeReport r;
    QString whyNot;
    const bool ok = edit(tr("Merge lists into one"), [&](QList<Production> &ps) {
        for (Production &p : ps)
            if (p.id == productionId)
                return ShowTree::mergeLists(p, listIds, &r, &whyNot);
        whyNot = tr("No such production.");
        return false;
    });
    if (report != nullptr)
        *report = r;
    if (why != nullptr)
        *why = whyNot;
    return ok;
}
