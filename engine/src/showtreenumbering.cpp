/*
  Q Light Controller Plus
  showtreenumbering.cpp

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

/* Order and numbers (docs/features/show-tree.md, "Reordering and
   numbering"). A cue list's order is always its cues' number order:

   - a typed number moves the cue to its place (a number taken -- by a cue
     of the list or a sub-cue's derived 4.1 -- is refused);
   - a dragged cue takes a number between its new neighbours by D23's rule
     (numberBetween(), the one + Cue uses); nothing else is renumbered;
   - Renumber this level... renumbers a cue's level when asked.

   Everything else refers to a cue by its id (includes, base, sub-cues), or
   reads the number again when it is used (the bake's chaser steps and
   collection names, show control, the Run tab), so it follows. */

#include "showtreecontroller.h"
#include "showcontrol.h"
#include "doc.h"

using namespace ShowTree;

namespace
{

CueList *listOf(QList<Production> &ps, quint32 cueId)
{
    for (Production &p : ps)
        for (CueList &l : p.lists)
            if (l.indexOf(cueId) >= 0)
                return &l;
    return nullptr;
}

const CueList *subsOf(const QList<Production> &ps, quint32 cueId)
{
    for (const Production &p : ps)
        for (const CueList &l : p.lists)
            if (l.isSubCues() && l.subOf == cueId)
                return &l;
    return nullptr;
}

/** The full numbers cue @a cueId's sub-cues have when it is @a number. */
QStringList derivedNumbers(const QList<Production> &ps, quint32 cueId, const QString &number)
{
    QStringList out;
    if (const CueList *subs = subsOf(ps, cueId))
        for (int k = 0; k < subs->cues.count(); ++k)
            out << cueNumberAt(*subs, k, number);
    return out;
}

/** The cue numbered @a number in @a list (not @a except); -1: none. */
int numberedAs(const CueList &list, const QString &number, quint32 except = invalidId())
{
    for (int i = 0; i < list.cues.count(); ++i)
        if (list.cues.at(i).id != except && compareCueNumbers(cueNumberAt(list, i), number) == 0)
            return i;
    return -1;
}

/** The main list a sub-cue list's cue is in, and that cue's number. */
const CueList *parentOf(const QList<Production> &ps, const CueList &subs, QString *number)
{
    for (const Production &p : ps)
        for (const CueList &l : p.lists)
            if (!l.isSubCues() && l.indexOf(subs.subOf) >= 0)
            {
                *number = cueNumberAt(l, l.indexOf(subs.subOf));
                return &l;
            }
    return nullptr;
}

/** As show control knows cue @a index of @a list: "4", or "4.1". */
QString fullNumber(const QList<Production> &ps, const CueList &list, int index)
{
    QString up;
    if (list.isSubCues())
        parentOf(ps, list, &up);
    return cueNumberAt(list, index, up);
}

/** A cue with no number answers to its position: give it that number for
 *  good before cues move, so that what it answers to doesn't change. */
void materialize(CueList &list)
{
    for (int i = 0; i < list.cues.count(); ++i)
        if (list.cues.at(i).number.trimmed().isEmpty())
            list.cues[i].number = QString::number(i + 1);
}

bool inOrder(const CueList &list)
{
    for (int i = 1; i < list.cues.count(); ++i)
        if (compareCueNumbers(cueNumberAt(list, i - 1), cueNumberAt(list, i)) >= 0)
            return false;
    return true;
}

QString cueText(const Cue &c, const QString &number)
{
    return c.name.isEmpty() ? number : QStringLiteral("%1 %2").arg(number, c.name);
}

bool say(QString *why, const QString &text)
{
    if (why != nullptr)
        *why = text;
    return false;
}

/** Whether cue @a cueId of @a list may be numbered @a number. */
bool checkNumber(const QList<Production> &ps, const CueList &list, quint32 cueId,
                 const QString &number, QString *why)
{
    if (number.isEmpty())
        return say(why, ShowTreeController::tr("A cue needs a number."));
    if (!isCueNumber(number))
        return say(why, ShowTreeController::tr("“%1” isn't a cue number: digits and dots, as 4, 2.5 or 0.0.4.")
                        .arg(number));
    const int taken = numberedAs(list, number, cueId);
    if (taken >= 0)
        return say(why, ShowTreeController::tr("%1 is taken: cue %2 has it.")
                        .arg(number, cueText(list.cues.at(taken), cueNumberAt(list, taken))));
    if (list.isSubCues())
    {
        // Its full number (4.1) mustn't be a cue's of its cue's list.
        QString up;
        const CueList *main = parentOf(ps, list, &up);
        const QString full = up + QLatin1Char('.') + number;
        const int hit = main == nullptr ? -1 : numberedAs(*main, full);
        if (hit >= 0)
            return say(why, ShowTreeController::tr("Sub-cue %1 would be cue %2's number: show control "
                                                   "would fire the cue.").arg(full, cueText(main->cues.at(hit), full)));
        return true;
    }
    for (int i = 0; i < list.cues.count(); ++i)
    {
        const Cue &c = list.cues.at(i);
        if (c.id == cueId)
            continue;
        if (derivedNumbers(ps, c.id, cueNumberAt(list, i)).contains(number))
            return say(why, ShowTreeController::tr("%1 is a sub-cue of cue %2: a cue numbered so would "
                                                   "hide it from show control.")
                            .arg(number, cueText(c, cueNumberAt(list, i))));
    }
    for (const QString &d : derivedNumbers(ps, cueId, number))
    {
        const int hit = numberedAs(list, d, cueId);
        if (hit >= 0)
            return say(why, ShowTreeController::tr("Its sub-cue would be %1, cue %2's number.")
                            .arg(d, cueText(list.cues.at(hit), d)));
    }
    return true;
}

/** Number cue @a cueId @a number and move it to its place in number order. */
bool applyNumber(QList<Production> &ps, quint32 cueId, const QString &number, QString *why)
{
    CueList *l = listOf(ps, cueId);
    if (l == nullptr)
        return false;
    if (l->looks)
    {
        // A look's number is only the tree's: no order to keep.
        l->cue(cueId)->number = number;
        return true;
    }
    if (!checkNumber(ps, *l, cueId, number, why))
        return false;
    materialize(*l);
    const int from = l->indexOf(cueId);
    Cue c = l->cues.takeAt(from);
    c.number = number;
    int at = 0;
    while (at < l->cues.count() && compareCueNumbers(l->cues.at(at).number, number) < 0)
        ++at;
    l->cues.insert(at, c);
    return true;
}

/** Move cues @a ids before (@a after: after) cue @a anchor and number them
 *  between their new neighbours; their new numbers in @a numbers. */
bool applyMove(QList<Production> &ps, const QList<quint32> &ids, quint32 anchor, bool after,
               QString *why, QStringList *numbers)
{
    CueList *l = listOf(ps, anchor);
    if (l == nullptr || ids.isEmpty() || ids.contains(anchor))
        return false;
    for (quint32 id : ids)
        if (l->indexOf(id) < 0)
            return say(why, ShowTreeController::tr("Cues move within their own list."));

    const QList<quint32> before = [l]() {
        QList<quint32> out;
        for (const Cue &c : l->cues)
            out << c.id;
        return out;
    }();
    QList<Cue> moved, remaining;
    for (const Cue &c : l->cues)
        (ids.contains(c.id) ? moved : remaining) << c;
    int at = 0;
    while (remaining.at(at).id != anchor)
        ++at;
    if (after)
        ++at;
    QList<quint32> order;
    for (int i = 0; i < remaining.count(); ++i)
    {
        if (i == at)
            for (const Cue &c : moved)
                order << c.id;
        order << remaining.at(i).id;
    }
    if (at == remaining.count())
        for (const Cue &c : moved)
            order << c.id;
    if (order == before)
        return false;                       // where they are already

    if (!l->looks)
    {
        materialize(*l);
        for (Cue &c : moved)
            c.number = l->cue(c.id)->number;
        for (Cue &c : remaining)
            c.number = l->cue(c.id)->number;
        QStringList used;
        for (const Cue &c : remaining)
        {
            used << c.number;
            if (!l->isSubCues())
                used << derivedNumbers(ps, c.id, c.number);
        }
        QString prev = at > 0 ? remaining.at(at - 1).number : QString();
        const QString next = at < remaining.count() ? remaining.at(at).number : QString();
        QString up;
        const CueList *main = l->isSubCues() ? parentOf(ps, *l, &up) : nullptr;
        for (Cue &c : moved)
        {
            const QString n = numberBetween(prev, next, used);
            if (n.isEmpty())
                return say(why, ShowTreeController::tr("No number fits between %1 and %2: "
                                                       "Renumber this level… to make room, then drag again.")
                                .arg(prev.isEmpty() ? ShowTreeController::tr("the top") : prev,
                                     next.isEmpty() ? ShowTreeController::tr("the end") : next));
            // Its sub-cues (4.1...) mustn't land on a cue's number, nor (a
            // sub-cue) its full number on one of its cue's list.
            const QStringList derived = l->isSubCues() ? QStringList() : derivedNumbers(ps, c.id, n);
            for (const QString &d : derived)
                for (const QString &u : used)
                    if (compareCueNumbers(d, u) == 0)
                        return say(why, ShowTreeController::tr("Its sub-cue would be %1, a number in use: "
                                                               "Renumber this level… first.").arg(d));
            if (main != nullptr && numberedAs(*main, up + QLatin1Char('.') + n) >= 0)
                return say(why, ShowTreeController::tr("Sub-cue %1.%2 would be a cue's number: "
                                                       "Renumber this level… first.").arg(up, n));
            c.number = n;
            if (numbers != nullptr)
                *numbers << n;
            used << n << derived;
            prev = n;
        }
    }
    else if (numbers != nullptr)
    {
        for (const Cue &c : moved)
            *numbers << c.number;
    }

    for (int i = 0; i < moved.count(); ++i)
        remaining.insert(at + i, moved.at(i));
    l->cues = remaining;
    return true;
}

QStringList partsOf(const QString &number)
{
    return number.split(QLatin1Char('.'));
}

/** Whether @a parts starts with @a prefix, place by place as numbers. */
bool startsWith(const QStringList &parts, const QStringList &prefix)
{
    if (parts.count() < prefix.count())
        return false;
    for (int i = 0; i < prefix.count(); ++i)
        if (parts.at(i).toDouble() != prefix.at(i).toDouble())
            return false;
    return true;
}

/** After a renumber of @a l: one number each, sub-cues included, and still
 *  in order; @a plan gets every number show control fires that changed.
 *  @a first .. @a last: the song renumbered (-1: a level). */
bool checkAndPlan(const QList<Production> &ps, const CueList &l, const QStringList &before,
                  const QList<QStringList> &subsBefore, bool wasInOrder, int first, int last, QString *why,
                  QList<ShowTreeController::Renumbered> *plan)
{
    const bool song = first >= 0;
    auto inSong = [&](int i) { return i >= first && i <= last; };
    for (int i = 0; i < l.cues.count(); ++i)
    {
        const int twin = numberedAs(l, l.cues.at(i).number, l.cues.at(i).id);
        if (twin >= 0 && song && inSong(i) != inSong(twin))
            return say(why, ShowTreeController::tr("%1 would be the number of a cue outside this song: "
                                                   "renumber with another start or step.")
                            .arg(l.cues.at(i).number));
        if (twin >= 0)
            return say(why, ShowTreeController::tr("Two cues would be %1: renumber with another start or step.")
                            .arg(l.cues.at(i).number));
        if (!l.isSubCues())
            for (const QString &d : derivedNumbers(ps, l.cues.at(i).id, l.cues.at(i).number))
                if (numberedAs(l, d) >= 0)
                    return say(why, ShowTreeController::tr("Sub-cue %1 would be a cue's number: "
                                                           "renumber with another start or step.").arg(d));
    }
    if (wasInOrder && !inOrder(l))
        return say(why, song ? ShowTreeController::tr("That would put this song out of order with the cues around "
                                                      "it: renumber with another start or step.")
                             : ShowTreeController::tr("That would put this level out of order with the cues around "
                                                      "it: renumber with another start or step."));

    if (plan != nullptr)
        plan->clear();
    int changes = 0;
    for (int i = 0; i < l.cues.count(); ++i)
    {
        const QString to = fullNumber(ps, l, i);
        if (to == before.at(i))
            continue;
        ++changes;
        if (plan == nullptr)
            continue;
        plan->append({ l.cues.at(i).id, before.at(i), to });
        const QStringList subsNow = l.isSubCues() ? QStringList() : derivedNumbers(ps, l.cues.at(i).id, to);
        const CueList *subs = subsOf(ps, l.cues.at(i).id);
        for (int k = 0; k < subsNow.count() && k < subsBefore.at(i).count(); ++k)
            plan->append({ subs->cues.at(k).id, subsBefore.at(i).at(k), subsNow.at(k) });
    }
    if (changes == 0)
        return say(why, song ? ShowTreeController::tr("This song is numbered so already.")
                             : ShowTreeController::tr("This level is numbered so already."));
    return true;
}

bool applyRenumber(QList<Production> &ps, quint32 cueId, int start, int step, QString *why,
                   QList<ShowTreeController::Renumbered> *plan)
{
    CueList *l = listOf(ps, cueId);
    if (l == nullptr)
        return false;
    if (l->looks)
        return say(why, ShowTreeController::tr("Looks have no numbers to renumber."));
    if (start < 0 || step < 1)
        return say(why, ShowTreeController::tr("Start at 0 or more, in steps of 1 or more."));

    const bool wasInOrder = inOrder(*l);
    QStringList before;                     // as show control knows them
    QList<QStringList> subsBefore;
    for (int i = 0; i < l->cues.count(); ++i)
    {
        before << fullNumber(ps, *l, i);
        subsBefore << (l->isSubCues() ? QStringList() : derivedNumbers(ps, l->cues.at(i).id, before.last()));
    }
    materialize(*l);

    const QStringList level = partsOf(l->cue(cueId)->number);
    const int depth = level.count();
    const QStringList prefix = level.mid(0, depth - 1);

    // The level, in list order, and what each becomes.
    QList<QStringList> oldSiblings;
    QStringList newSiblings;
    QStringList renumbered;
    for (const Cue &c : l->cues)
    {
        const QStringList parts = partsOf(c.number);
        if (parts.count() == depth && startsWith(parts, prefix))
        {
            oldSiblings << parts;
            QStringList n = prefix;
            n << QString::number(start + step * (oldSiblings.count() - 1));
            newSiblings << n.join(QLatin1Char('.'));
        }
    }
    for (const Cue &c : l->cues)
    {
        const QStringList parts = partsOf(c.number);
        QString to = c.number;
        for (int s = 0; s < oldSiblings.count(); ++s)
        {
            if (!startsWith(parts, oldSiblings.at(s)))
                continue;
            // The cue itself, or a point cue under it: its own places follow.
            QStringList n = partsOf(newSiblings.at(s));
            n << parts.mid(depth);
            to = n.join(QLatin1Char('.'));
            break;
        }
        renumbered << to;
    }
    for (int i = 0; i < l->cues.count(); ++i)
        l->cues[i].number = renumbered.at(i);

    return checkAndPlan(ps, *l, before, subsBefore, wasInOrder, -1, -1, why, plan);
}

/** What Renumber Song... numbers a cue at: the scheme's own places --
 *  {S}.{P}.{II}: three, the last the iteration; Hundreds: one (215). */
int songDepth(Numbering numbering)
{
    return numbering == Numbering::Hundreds ? 1 : 3;
}

/** "1.00.3" -> "1.0.3": places compared as numbers. */
QString normalized(const QStringList &parts)
{
    QStringList out;
    for (const QString &p : parts)
        out << QString::number(p.toDouble());
    return out.join(QLatin1Char('.'));
}

bool applyRenumberSong(QList<Production> &ps, quint32 listId, const Section &song, int start, int step,
                       bool wholePoints, QString *why, QList<ShowTreeController::Renumbered> *plan)
{
    Production *pr = nullptr;
    CueList *l = nullptr;
    for (Production &p : ps)
        if ((l = p.list(listId)) != nullptr)
        {
            pr = &p;
            break;
        }
    if (l == nullptr || l->looks || l->isSubCues())
        return say(why, ShowTreeController::tr("Only a cue list's songs can be renumbered."));
    if (pr->numbering == Numbering::Manual)
        return say(why, ShowTreeController::tr("Under Manual numbering the numbers say nothing about the song: "
                                               "use Renumber This Level… on its cues."));
    if (pr->numbering == Numbering::None)
        return say(why, ShowTreeController::tr("This show has no songs: choose a numbering scheme first."));
    if (start < 0 || step < 1)
        return say(why, ShowTreeController::tr("Start at 0 or more, in steps of 1 or more."));

    // The song: by its first cue, else (renumbered since) by its key.
    const QList<Section> sections = sectionsOf(*l, pr->numbering);
    const Section *s = nullptr;
    for (const Section &sec : sections)
        if (sec.headCue == song.headCue)
            s = &sec;
    for (int k = 0; k < sections.count() && s == nullptr; ++k)
        if (!song.key.isEmpty() && sections.at(k).key == song.key)
            s = &sections.at(k);
    if (s == nullptr)
        return say(why, ShowTreeController::tr("That song has no cues any more."));

    const bool wasInOrder = inOrder(*l);
    QStringList before;
    QList<QStringList> subsBefore;
    for (int i = 0; i < l->cues.count(); ++i)
    {
        before << fullNumber(ps, *l, i);
        subsBefore << derivedNumbers(ps, l->cues.at(i).id, before.last());
    }
    materialize(*l);

    // Each part of the song ({S}.{P}) numbers its cues start, start +
    // step... in list order, in the last place; S and P stay, so no cue
    // changes song or part. A point cue (1.0.3.5) becomes a cue of its own
    // (wholePoints), or keeps its point under its cue's new number.
    const int depth = songDepth(pr->numbering);
    const qlonglong key = s->key.toLongLong();
    QHash<QString, int> counts;
    QList<QPair<QStringList, QString>> renamed;    // old places -> new number
    for (int i = s->first; i <= s->last; ++i)
    {
        const QStringList parts = partsOf(l->cues.at(i).number);
        if (parts.count() < depth)
            continue;                       // "1", the song's own count-in: as it is
        QString to;
        if (!wholePoints && parts.count() > depth)
            for (const QPair<QStringList, QString> &r : renamed)
                if (startsWith(parts, r.first))
                {
                    QStringList n = partsOf(r.second);
                    n << parts.mid(depth);
                    to = n.join(QLatin1Char('.'));
                    break;
                }
        if (to.isEmpty())
        {
            const QStringList group = parts.mid(0, depth - 1);
            const int value = start + step * counts[normalized(group)]++;
            if (pr->numbering == Numbering::Hundreds)
            {
                if (value > 99)
                    return say(why, ShowTreeController::tr("Song %1 has room for numbers %1 to %2: renumber with "
                                                           "a smaller start or step.")
                                    .arg(s->key, QString::number(key + 99)));
                to = QString::number(key + value);
            }
            else
            {
                QStringList n = group;
                n << QString::number(value);
                to = n.join(QLatin1Char('.'));
            }
            renamed << qMakePair(parts.mid(0, depth), to);
        }
        l->cues[i].number = to;
    }
    return checkAndPlan(ps, *l, before, subsBefore, wasInOrder, s->first, s->last, why, plan);
}

} // namespace

bool ShowTreeController::keepingStandby(quint32 cueId, const std::function<bool()> &op)
{
    const CueList *l = listOfCue(cueId);
    ShowControl *sc = m_doc->showControl();
    if (l == nullptr || l->isSubCues() || l->looks || sc == nullptr || l->chaser == invalidId())
        return op();
    const quint32 listId = l->id;
    const quint32 chaser = l->chaser;
    const int step = sc->loadedStep(chaser);
    const quint32 loaded = step >= 0 && step < l->cues.count() ? l->cues.at(step).id : invalidId();
    const bool ok = op();
    if (ok && loaded != invalidId())
        if (const CueList *now = cueList(listId))
            sc->moveLoaded(chaser, now->indexOf(loaded));
    return ok;
}

bool ShowTreeController::moveCue(quint32 cueId, int toIndex)
{
    const CueList *l = listOfCue(cueId);
    if (l == nullptr || l->cues.isEmpty())
        return false;
    const int from = l->indexOf(cueId);
    toIndex = qBound(0, toIndex, int(l->cues.count()) - 1);
    if (from == toIndex)
        return false;
    return moveCues({ cueId }, l->cues.at(toIndex).id, from < toIndex);
}

bool ShowTreeController::setCue(const Cue &cue, QString *why)
{
    const Cue *now = this->cue(cue.id);
    if (now == nullptr)
        return false;
    const QString number = cue.number.trimmed();
    const bool renumber = number != now->number;
    QString err;
    auto op = [&]() {
        return edit(renumber ? tr("Renumber cue") : tr("Edit cue"), [&](QList<Production> &ps) {
            CueList *l = listOf(ps, cue.id);
            if (l == nullptr)
                return false;
            Cue *c = l->cue(cue.id);
            const quint32 coll = c->collection;
            const QString was = c->number;
            *c = cue;
            c->collection = coll;
            c->number = was;
            // Includes as given, less any that are gone or would loop (D8).
            const QList<quint32> wanted = c->includes;
            c->includes.clear();
            for (quint32 inc : wanted)
                if (canInclude(*l, cue.id, inc))
                    c->includes << inc;
            // Last: a new number moves the cue to its place.
            return !renumber || applyNumber(ps, cue.id, number, &err);
        });
    };
    const bool ok = renumber ? keepingStandby(cue.id, op) : op();
    if (!ok && why != nullptr)
        *why = err;
    return ok;
}

bool ShowTreeController::checkCueNumber(quint32 cueId, const QString &number, QString *why) const
{
    QList<Production> ps = m_productions;
    const CueList *l = listOf(ps, cueId);
    if (l == nullptr)
        return say(why, tr("No such cue."));
    if (l->looks)
        return true;
    return checkNumber(ps, *l, cueId, number.trimmed(), why);
}

QStringList ShowTreeController::moveNumbers(const QList<quint32> &cueIds, quint32 anchorCueId, bool after,
                                            QString *why) const
{
    QList<Production> ps = m_productions;
    QStringList numbers;
    if (!applyMove(ps, cueIds, anchorCueId, after, why, &numbers))
        return QStringList();
    return numbers;
}

bool ShowTreeController::moveCues(const QList<quint32> &cueIds, quint32 anchorCueId, bool after, QString *why)
{
    if (cueIds.isEmpty())
        return false;
    QString err;
    const bool ok = keepingStandby(anchorCueId, [&]() {
        return edit(cueIds.count() > 1 ? tr("Move cues") : tr("Move cue"), [&](QList<Production> &ps) {
            return applyMove(ps, cueIds, anchorCueId, after, &err, nullptr);
        });
    });
    if (!ok && why != nullptr)
        *why = err;
    return ok;
}

QList<ShowTreeController::Renumbered> ShowTreeController::renumberLevelPlan(quint32 cueId, int start, int step,
                                                                            QString *why) const
{
    QList<Production> ps = m_productions;
    QList<Renumbered> plan;
    if (!applyRenumber(ps, cueId, start, step, why, &plan))
        return QList<Renumbered>();
    return plan;
}

bool ShowTreeController::renumberLevel(quint32 cueId, int start, int step, QString *why)
{
    QString err;
    const bool ok = keepingStandby(cueId, [&]() {
        return edit(tr("Renumber this level"), [&](QList<Production> &ps) {
            return applyRenumber(ps, cueId, start, step, &err, nullptr);
        });
    });
    if (!ok && why != nullptr)
        *why = err;
    return ok;
}

QList<ShowTreeController::Renumbered> ShowTreeController::renumberSongPlan(quint32 listId, const Section &song,
                                                                           int start, int step, bool wholePoints,
                                                                           QString *why) const
{
    QList<Production> ps = m_productions;
    QList<Renumbered> plan;
    if (!applyRenumberSong(ps, listId, song, start, step, wholePoints, why, &plan))
        return QList<Renumbered>();
    return plan;
}

bool ShowTreeController::renumberSong(quint32 listId, const Section &song, int start, int step, bool wholePoints,
                                      QString *why)
{
    QString err;
    const CueList *l = cueList(listId);
    if (l == nullptr || l->cues.isEmpty())
        return say(why, tr("Only a cue list's songs can be renumbered."));
    const bool ok = keepingStandby(l->cues.first().id, [&]() {
        return edit(tr("Renumber song"), [&](QList<Production> &ps) {
            return applyRenumberSong(ps, listId, song, start, step, wholePoints, &err, nullptr);
        });
    });
    if (!ok && why != nullptr && !err.isEmpty())
        *why = err;
    return ok;
}
