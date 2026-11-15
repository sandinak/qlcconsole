/*
  Q Light Controller Plus
  showtreescript.cpp

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

/* The calling script (0.5): a production keeps its script or lyrics as
   plain text, and cues placed on it by id. An id survives a renumber and a
   move; a cue that is deleted just stops being found, so its marker is
   gone (and an undo of the delete brings it back). Every edit is a tree
   step like any other. */

#include <algorithm>

#include <QRegularExpression>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFile>

#include "showtreecontroller.h"

using namespace ShowTree;

static bool cueInProduction(const Production &p, quint32 cueId)
{
    for (const CueList &l : p.lists)
        if (l.indexOf(cueId) >= 0)
            return true;
    return false;
}

CallingScript ShowTreeController::callingScript(quint32 productionId) const
{
    const Production *p = production(productionId);
    if (p == nullptr)
        return CallingScript();
    CallingScript out = p->script;
    out.anchors.clear();
    for (const ScriptAnchor &a : p->script.anchors)
        if (cueInProduction(*p, a.cue))
            out.anchors << a;
    return out;
}

static int shifted(int offset, int prefix, int oldLen, int newLen, int suffix)
{
    if (offset < 0)
        return offset;
    if (offset <= prefix)
        return offset;
    if (offset >= oldLen - suffix)
        return qMin(newLen, offset + (newLen - oldLen));
    return prefix;                      // inside what was replaced
}

bool ShowTreeController::setScriptText(quint32 productionId, const QString &textIn, const QString &title)
{
    const Production *have = production(productionId);
    if (have == nullptr)
        return false;
    QString text = textIn;
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QString newTitle = title.isNull() ? have->script.title : title;
    if (text == have->script.text && newTitle == have->script.title)
        return false;

    const QString old = have->script.text;
    int prefix = 0;
    const int minLen = qMin(old.length(), text.length());
    while (prefix < minLen && old.at(prefix) == text.at(prefix))
        ++prefix;
    int suffix = 0;
    while (suffix < minLen - prefix
           && old.at(old.length() - 1 - suffix) == text.at(text.length() - 1 - suffix))
        ++suffix;

    return edit(tr("Calling script"), [&](QList<Production> &ps) {
        for (Production &p : ps)
            if (p.id == productionId)
            {
                QList<ScriptAnchor> kept;
                for (ScriptAnchor a : p.script.anchors)
                {
                    if (!cueInProduction(p, a.cue))
                        continue;
                    a.go = shifted(a.go, prefix, old.length(), text.length(), suffix);
                    a.standby = shifted(a.standby, prefix, old.length(), text.length(), suffix);
                    kept << a;
                }
                p.script.anchors = kept;
                p.script.text = text;
                p.script.title = newTitle;
                return true;
            }
        return false;
    });
}

bool ShowTreeController::importScript(quint32 productionId, const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
            *error = tr("Cannot read %1: %2").arg(path, file.errorString());
        return false;
    }
    const QString text = QString::fromUtf8(file.readAll());
    if (production(productionId) == nullptr)
    {
        if (error != nullptr)
            *error = tr("No such production.");
        return false;
    }
    const QString title = QFileInfo(path).fileName();
    if (setScriptText(productionId, text, title))
        return true;
    return production(productionId)->script.text == text;       // the same script again
}

bool ShowTreeController::anchorCue(quint32 productionId, quint32 cueId, int offset, ScriptMark mark)
{
    const Production *have = production(productionId);
    if (have == nullptr || offset < 0 || offset > have->script.text.length()
        || !cueInProduction(*have, cueId))
        return false;
    const ScriptAnchor *now = have->script.anchor(cueId);
    if (now != nullptr && (mark == ScriptMark::Go ? now->go : now->standby) == offset)
        return false;
    return edit(tr("Place cue on the script"), [&](QList<Production> &ps) {
        for (Production &p : ps)
            if (p.id == productionId)
            {
                for (ScriptAnchor &a : p.script.anchors)
                    if (a.cue == cueId)
                    {
                        (mark == ScriptMark::Go ? a.go : a.standby) = offset;
                        return true;
                    }
                ScriptAnchor a;
                a.cue = cueId;
                (mark == ScriptMark::Go ? a.go : a.standby) = offset;
                p.script.anchors << a;
                return true;
            }
        return false;
    });
}

bool ShowTreeController::unanchorCue(quint32 productionId, quint32 cueId, ScriptMark mark, bool both)
{
    const Production *have = production(productionId);
    const ScriptAnchor *now = have == nullptr ? nullptr : have->script.anchor(cueId);
    if (now == nullptr)
        return false;
    return edit(tr("Take cue off the script"), [&](QList<Production> &ps) {
        for (Production &p : ps)
            if (p.id == productionId)
                for (int i = 0; i < p.script.anchors.count(); ++i)
                    if (p.script.anchors.at(i).cue == cueId)
                    {
                        ScriptAnchor &a = p.script.anchors[i];
                        if (both)
                            a.go = a.standby = -1;
                        else
                            (mark == ScriptMark::Go ? a.go : a.standby) = -1;
                        if (a.go < 0 && a.standby < 0)
                            p.script.anchors.removeAt(i);
                        return true;
                    }
        return false;
    });
}

bool ShowTreeController::clearScript(quint32 productionId)
{
    const Production *have = production(productionId);
    if (have == nullptr || have->script.isEmpty())
        return false;
    return edit(tr("Clear the calling script"), [&](QList<Production> &ps) {
        for (Production &p : ps)
            if (p.id == productionId)
            {
                p.script = CallingScript();
                return true;
            }
        return false;
    });
}

/****************************************************************************
 * Cues from the script (0.4.9, board Q237)
 ****************************************************************************/

/* The invisible characters a Google Docs or PDF export leaves in the text
   (a zero-width space after "SCENE:"), and no-break spaces as spaces. */
static QString visible(const QString &in)
{
    QString s = in;
    for (const QChar c : { QChar(0x200B), QChar(0x200C), QChar(0x200D), QChar(0x2060), QChar(0xFEFF) })
        s.remove(c);
    s.replace(QChar(0x00A0), QLatin1Char(' '));
    return s;
}

/* Letters and digits only, lower case: "LET'S DANCE" and "Lets Dance" match. */
static QString bare(const QString &in)
{
    QString out;
    for (const QChar c : in)
        if (c.isLetterOrNumber())
            out += c.toLower();
    return out;
}

QList<ShowTreeController::ScriptDirection> ShowTreeController::scriptDirections(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("^\\s*(SCENE|LIGHTING|LIGHTS|LIGHT)\\s*:(.*)$"),
                                       QRegularExpression::CaseInsensitiveOption);
    QList<ScriptDirection> out;
    int pos = 0;
    for (int line = 0;; ++line)
    {
        const int nl = text.indexOf(QLatin1Char('\n'), pos);
        const QString raw = nl < 0 ? text.mid(pos) : text.mid(pos, nl - pos);
        const QRegularExpressionMatch m = re.match(visible(raw));
        if (m.hasMatch())
        {
            const QString body = m.captured(2).simplified();
            if (!body.isEmpty())
            {
                ScriptDirection d;
                d.line = line;
                d.kind = m.captured(1).toUpper();
                d.text = body;
                int k = 0;
                while (k < raw.length() && !raw.at(k).isLetter())
                    ++k;
                d.offset = pos + k;
                out << d;
            }
        }
        if (nl < 0)
            break;
        pos = nl + 1;
    }
    return out;
}

QString ShowTreeController::scriptTitleLine(const QString &line)
{
    QString t = visible(line).trimmed();
    if (t.startsWith(QLatin1Char('#')))
    {
        while (t.startsWith(QLatin1Char('#')))
            t.remove(0, 1);
        return t.trimmed();
    }
    if (t.contains(QLatin1Char(':')))
        return QString();
    int letters = 0;
    for (const QChar c : t)
    {
        if (c.isLower())
            return QString();
        if (c.isLetter())
            ++letters;
    }
    return letters >= 2 ? t : QString();
}

QString ShowTreeController::scriptCueName(const QString &direction)
{
    const QString all = visible(direction).simplified();
    // The first phrase: up to a comma, a stop, an ellipsis, a dash, a bracket.
    static const QRegularExpression stop(QStringLiteral("\\s*(?:[,;.!?()\\x{2026}\\x{2014}\\x{2013}:]|\\s-\\s)"));
    QString t = all;
    const QRegularExpressionMatch m = stop.match(all);
    if (m.hasMatch() && m.capturedStart() > 0)
        t = all.left(m.capturedStart());
    t = t.trimmed();
    const int most = 32;
    if (t.length() > most)
    {
        const int cut = t.lastIndexOf(QLatin1Char(' '), most);
        t = t.left(cut > 0 ? cut : most).trimmed();
    }
    return t;
}

/* A song's name from the script's title line: "LIFE AND DEATH REPRISE" ->
   "Life And Death Reprise"; one with lower case stays as it is. */
static QString songName(const QString &title)
{
    for (const QChar c : title)
        if (c.isLower())
            return title;
    QString out = title.toLower();
    bool start = true;
    for (QChar &c : out)
    {
        if (start && c.isLetter())
            c = c.toUpper();
        start = !c.isLetterOrNumber() && c != QLatin1Char('\'');
    }
    return out;
}

QList<ShowTreeController::ScriptCue> ShowTreeController::proposeScriptCues(quint32 productionId, quint32 listId,
                                                                         const QSet<int> &leaveOut) const
{
    QList<ScriptCue> out;
    const Production *prod = production(productionId);
    const CueList *list = prod == nullptr ? nullptr : prod->list(listId);
    if (list == nullptr || list->looks || list->isSubCues())
        return out;
    const CallingScript script = callingScript(productionId);
    const QList<ScriptDirection> dirs = scriptDirections(script.text);
    if (dirs.isEmpty())
        return out;
    const Numbering scheme = prod->numbering;
    const bool songs = scheme == Numbering::ShowPartIteration || scheme == Numbering::Hundreds;
    const qlonglong songStep = scheme == Numbering::Hundreds ? 100 : 1;

    const QStringList lines = script.text.split(QLatin1Char('\n'));
    QList<int> starts;
    for (int i = 0, at = 0; i < lines.count(); ++i)
    {
        starts << at;
        at += int(lines.at(i).length()) + 1;
    }
    auto lineOf = [&](int offset) {
        int li = 0;
        while (li + 1 < starts.count() && starts.at(li + 1) <= offset)
            ++li;
        return li;
    };

    // The cues already called (their GO) on each line; this list's numbers.
    QHash<int, QList<quint32>> called;
    for (const ScriptAnchor &a : script.anchors)
        if (a.go >= 0)
            called[lineOf(a.go)] << a.cue;
    auto numberOf = [&](quint32 cueId) {
        const int at = list->indexOf(cueId);
        return at < 0 ? QString() : cueNumberAt(*list, at);
    };

    // The list's songs by name ("Everybody Scream", "1 — Everybody Scream").
    QHash<QString, QString> songByName;
    QHash<QString, QString> nameOfSong;
    if (songs)
    {
        for (const SectionHead &h : list->sections)
            if (!h.key.isEmpty() && !h.name.trimmed().isEmpty())
                nameOfSong.insert(h.key, h.name.trimmed());
        for (auto it = nameOfSong.cbegin(); it != nameOfSong.cend(); ++it)
        {
            songByName.insert(bare(it.value()), it.key());
            Section s;
            s.key = it.key();
            s.name = it.value();
            songByName.insert(bare(sectionTitle(s)), it.key());
        }
    }

    // This list's cues called on the script, by song, in script order: a
    // song's first direction before them goes in front of the first.
    QHash<QString, QList<QPair<int, QString>>> calledInSong;
    if (songs)
        for (auto it = called.cbegin(); it != called.cend(); ++it)
            for (quint32 id : it.value())
            {
                const QString n = numberOf(id);
                const QString k = sectionKey(scheme, n);
                if (!k.isEmpty())
                    calledInSong[k] << qMakePair(it.key(), n);
            }
    for (auto &v : calledInSong)
        std::sort(v.begin(), v.end());

    auto nextIsLighting = [&](int li) {
        for (int i = li + 1; i < lines.count(); ++i)
        {
            const QString t = visible(lines.at(i)).trimmed();
            if (t.isEmpty())
                continue;
            static const QRegularExpression re(QStringLiteral("^LIGHT(?:S|ING)?\\s*:"),
                                               QRegularExpression::CaseInsensitiveOption);
            return re.match(t).hasMatch();
        }
        return false;
    };

    // The cues of this list on the script; one that isn't is taken to come
    // right after the cue before it in number order.
    QSet<quint32> placed;
    for (const QList<quint32> &ids : called)
        for (quint32 id : ids)
            placed.insert(id);
    CueList work = *list;               // with the proposed cues put in, as they are numbered
    auto pastUnplaced = [&](int after, const QString &song) {
        while (after >= 0 && after + 1 < work.cues.count())
        {
            const Cue &n = work.cues.at(after + 1);
            if (n.id == invalidId() || placed.contains(n.id)
                || (!song.isEmpty() && sectionKey(scheme, cueNumberAt(work, after + 1)) != song))
                break;
            ++after;
        }
        return after;
    };
    const QStringList taken = takenNumbers(m_productions, *list);
    QHash<QString, QString> newSongs;   // key -> the script's title
    QHash<QString, QString> lastInSong;
    QString lastAny;
    QString key;
    if (scheme == Numbering::ShowPartIteration)
        key = QStringLiteral("0");      // before the first song: setup, preshow
    else if (scheme == Numbering::Hundreds)
    {
        const QList<Section> secs = sectionsOf(*list, scheme);
        key = secs.isEmpty() ? QStringLiteral("100") : secs.first().key;
    }

    int di = 0;
    for (int li = 0; li < lines.count() && di < dirs.count(); ++li)
    {
        // A song begins here.
        if (songs)
        {
            const QString whole = bare(lines.at(li));
            QString k = whole.isEmpty() ? QString() : songByName.value(whole);
            const QString title = scriptTitleLine(lines.at(li));
            if (k.isEmpty() && !bare(title).isEmpty())
                k = songByName.value(bare(title));
            if (k.isEmpty() && !title.isEmpty() && nextIsLighting(li))
            {
                qlonglong n = key.toLongLong() + songStep;
                while (nameOfSong.contains(QString::number(n)) || newSongs.contains(QString::number(n)))
                    n += songStep;
                k = QString::number(n);
                newSongs.insert(k, songName(title));
            }
            // A song's name again further on (a lyric, "Everybody scream")
            // never takes the script back to an earlier song.
            if (!k.isEmpty() && k.toLongLong() >= key.toLongLong())
                key = k;
        }
        // Cues already called here: the script is in their song from here.
        for (quint32 id : called.value(li))
        {
            const QString n = numberOf(id);
            if (n.isEmpty())
                continue;
            lastAny = n;
            const QString k = sectionKey(scheme, n);
            if (songs && !k.isEmpty())
            {
                key = k;
                lastInSong.insert(k, n);
            }
        }
        if (dirs.at(di).line != li)
            continue;

        ScriptCue sc;
        sc.direction = dirs.at(di++);
        sc.name = scriptCueName(sc.direction.text);
        sc.song = key;
        if (songs)
        {
            if (nameOfSong.contains(key))
                sc.songTitle = nameOfSong.value(key);
            else if (newSongs.contains(key))
            {
                sc.songTitle = newSongs.value(key);
                sc.newSong = true;
            }
        }
        if (!called.value(li).isEmpty())
        {
            sc.cuedBy = called.value(li).first();
            const CueList *cl = listOfCue(sc.cuedBy);
            const int at = cl == nullptr ? -1 : cl->indexOf(sc.cuedBy);
            if (at >= 0)
                sc.cuedAs = QStringLiteral("%1 %2").arg(cueNumber(*cl, at), cl->cues.at(at).name).trimmed();
            out << sc;
            continue;
        }
        if (leaveOut.contains(li))
        {
            sc.leftOut = true;
            out << sc;
            continue;
        }

        // Where it goes in the list, and so its number.
        int after = int(work.cues.count()) - 1;
        QString number;
        if (songs)
        {
            if (lastInSong.contains(key))
                after = pastUnplaced(indexOfNumber(work, lastInSong.value(key)), key);
            else
            {
                QString before;
                for (const auto &c : calledInSong.value(key))
                    if (c.first > li)
                    {
                        before = c.second;
                        break;
                    }
                if (!before.isEmpty())
                    after = indexOfNumber(work, before) - 1;
                else
                {
                    // After the song's last cue; in an empty song, where it goes.
                    after = -1;
                    for (int i = 0; i < work.cues.count(); ++i)
                    {
                        const QString n = cueNumberAt(work, i);
                        if (sectionKey(scheme, n) == key || compareCueNumbers(n, key) < 0)
                            after = i;
                    }
                }
            }
            number = numberInSection(work, scheme, after, key, taken);
        }
        else
        {
            const int at = lastAny.isEmpty() ? -1 : indexOfNumber(work, lastAny);
            if (at >= 0)
                after = pastUnplaced(at, QString());
            number = numberForInsert(work, after, taken);
        }
        if (number.isEmpty())
        {
            sc.noRoom = true;
            out << sc;
            continue;
        }
        sc.number = number;
        Cue placeholder;
        placeholder.number = number;
        work.cues.insert(qBound(0, after + 1, int(work.cues.count())), placeholder);
        lastAny = number;
        if (songs)
            lastInSong.insert(key, number);
        out << sc;
    }
    return out;
}

QList<quint32> ShowTreeController::makeScriptCues(quint32 productionId, quint32 listId, const QSet<int> &leaveOut,
                                                  QString *message)
{
    auto say = [message](const QString &text) {
        if (message != nullptr)
            *message = text;
    };
    QList<quint32> ids;
    const Production *prod = production(productionId);
    const CueList *list = prod == nullptr ? nullptr : prod->list(listId);
    if (list == nullptr || list->looks || list->isSubCues())
    {
        say(tr("Pick a cue list of this production."));
        return ids;
    }
    const QString listName = list->name;
    const QList<ScriptCue> plan = proposeScriptCues(productionId, listId, leaveOut);
    int cued = 0, noRoom = 0;
    QHash<QString, QString> names;
    for (const ScriptCue &sc : plan)
    {
        if (sc.made())
        {
            ids << newId();
            if (sc.newSong && !sc.songTitle.isEmpty())
                names.insert(sc.song, sc.songTitle);
        }
        cued += sc.cuedBy != invalidId() ? 1 : 0;
        noRoom += sc.noRoom ? 1 : 0;
    }
    if (ids.isEmpty())
    {
        if (plan.isEmpty())
            say(tr("The script has no SCENE: or LIGHTING: lines."));
        else if (cued == plan.count())
            say(tr("Every SCENE and LIGHTING line already has a cue."));
        else if (noRoom > 0)
            say(tr("No number fits for the lines picked: renumber their song first."));
        else
            say(tr("No lines are picked."));
        return ids;
    }
    const bool ok = edit(tr("Cues from the script"), [&](QList<Production> &ps) {
        for (Production &p : ps)
        {
            if (p.id != productionId)
                continue;
            CueList *l = p.list(listId);
            if (l == nullptr)
                return false;
            int k = 0;
            for (const ScriptCue &sc : plan)
            {
                if (!sc.made())
                    continue;
                Cue c;
                c.id = ids.at(k++);
                c.number = sc.number;
                c.name = sc.name;
                c.note = sc.direction.text;
                int at = 0;
                while (at < l->cues.count() && compareCueNumbers(cueNumberAt(*l, at), c.number) <= 0)
                    ++at;
                l->cues.insert(at, c);
                ScriptAnchor a;
                a.cue = c.id;
                a.go = sc.direction.offset;
                p.script.anchors << a;
            }
            for (auto it = names.cbegin(); it != names.cend(); ++it)
            {
                bool named = false;
                for (SectionHead &h : l->sections)
                    if (h.key == it.key())
                    {
                        if (h.name.trimmed().isEmpty())
                            h.name = it.value();
                        named = true;
                    }
                if (!named)
                {
                    SectionHead h;
                    h.key = it.key();
                    h.name = it.value();
                    l->sections << h;
                }
            }
            return true;
        }
        return false;
    });
    if (!ok)
    {
        say(tr("Nothing was made."));
        return QList<quint32>();
    }
    const int n = int(ids.count());
    QString text = n == 1 ? tr("Made 1 cue in %1, on its line.").arg(listName)
                          : tr("Made %1 cues in %2, each on its line.").arg(n).arg(listName);
    if (cued > 0)
        text += QLatin1Char(' ') + (cued == 1 ? tr("1 line already had a cue.")
                                              : tr("%1 lines already had a cue.").arg(cued));
    if (noRoom > 0)
        text += QLatin1Char(' ') + (noRoom == 1 ? tr("1 line had no number free.")
                                                : tr("%1 lines had no number free.").arg(noRoom));
    say(text);
    return ids;
}
