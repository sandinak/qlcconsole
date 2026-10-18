/*
  Q Light Controller Plus
  showtreelooks.cpp

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

#include <algorithm>

#include "showtreecontroller.h"
#include "function.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

/* The Looks section (PROGRAMMING_ROLE_DESIGN.md §4.1, P-D5..P-D7, R3).
   A production's busking looks are the cues of one more list, flagged
   CueList::looks: so every operation on a cue -- layers, attribute rows,
   the cascade, group sets, child layers, effects, line aims, reusable
   layers, copy/paste, rename, delete, undo, the Design preview, the command
   line -- works on a look unchanged. What makes a look a look:

   - it never tracks: effectiveMode() is Block, and edit() keeps every look
     without a mode, includes or base (normalizeLooks);
   - it bakes to a Collection of its own, never a chaser step: shown (the
     function pickers list it by the look's name), filed under
     Productions/<production>/Looks/<folder>, locked (D13). Its id stays as
     the look is edited (the bake changes its members), so a VC button keeps
     firing it;
   - on stage it mixes with a running cue list as any function does (P-D6):
     intensity HTP, the rest LTP, last started wins. */

static CueList *listIn(QList<Production> &productions, quint32 listId)
{
    for (Production &p : productions)
        if (CueList *l = p.list(listId))
            return l;
    return nullptr;
}

static QString cleanFolder(const QString &folder)
{
    QStringList parts = folder.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (QString &p : parts)
        p = p.trimmed();
    parts.removeAll(QString());
    return parts.join(QLatin1Char('/'));
}

/** Whether @a path is @a folder or under it. */
static bool inFolder(const QString &path, const QString &folder)
{
    return folder.isEmpty() || path == folder || path.startsWith(folder + QLatin1Char('/'));
}

quint32 ShowTreeController::looksSection(quint32 productionId) const
{
    const Production *p = production(productionId);
    const CueList *l = p == nullptr ? nullptr : p->looks();
    return l == nullptr ? invalidId() : l->id;
}

bool ShowTreeController::isLooks(quint32 listId) const
{
    const CueList *l = cueList(listId);
    return l != nullptr && l->looks;
}

bool ShowTreeController::isLook(quint32 cueId) const
{
    const CueList *l = listOfCue(cueId);
    return l != nullptr && l->looks;
}

quint32 ShowTreeController::addLooksSection(quint32 productionId)
{
    const quint32 have = looksSection(productionId);
    if (have != invalidId())
        return have;
    const quint32 id = newId();
    const bool ok = edit(tr("New Looks section"), [&](QList<Production> &ps) {
        for (Production &p : ps)
        {
            if (p.id != productionId || p.looks() != nullptr)
                continue;
            CueList l;
            l.id = id;
            l.name = tr("Looks");
            l.looks = true;
            l.mode = ListMode::NonTracking;
            p.lists << l;
            return true;
        }
        return false;
    });
    return ok ? id : invalidId();
}

quint32 ShowTreeController::addLook(quint32 productionId, const QString &name, const QString &folder,
                                    quint32 afterId)
{
    if (production(productionId) == nullptr)
        return invalidId();
    const quint32 sectionId = looksSection(productionId) == invalidId() ? newId() : invalidId();
    const quint32 id = newId();
    const QString f = cleanFolder(folder);
    const bool ok = edit(tr("New look"), [&](QList<Production> &ps) {
        for (Production &p : ps)
        {
            if (p.id != productionId)
                continue;
            CueList *l = p.looks();
            if (l == nullptr)
            {
                // The first look makes the section, in the same step.
                CueList fresh;
                fresh.id = sectionId;
                fresh.name = tr("Looks");
                fresh.looks = true;
                p.lists << fresh;
                l = &p.lists.last();
            }
            const int after = afterId == invalidId() ? int(l->cues.count()) - 1 : l->indexOf(afterId);
            Cue c;
            c.id = id;
            c.name = name;
            c.folder = f;
            c.number = numberForInsert(*l, after < 0 ? int(l->cues.count()) - 1 : after);
            l->cues.insert(after < 0 ? l->cues.count() : after + 1, c);
            l->folders.removeAll(f);
            return true;
        }
        return false;
    });
    return ok ? id : invalidId();
}

bool ShowTreeController::setLookFolder(const QList<quint32> &lookIds, const QString &folder)
{
    const QString f = cleanFolder(folder);
    return edit(f.isEmpty() ? tr("Out of folder") : tr("Move to %1").arg(f), [&](QList<Production> &ps) {
        bool any = false;
        for (Production &p : ps)
            for (CueList &l : p.lists)
            {
                if (!l.looks)
                    continue;
                for (Cue &c : l.cues)
                    if (lookIds.contains(c.id) && c.folder != f)
                    {
                        c.folder = f;
                        any = true;
                    }
                if (any)
                    l.folders.removeAll(f);
            }
        return any;
    });
}

bool ShowTreeController::addLookFolder(quint32 listId, const QString &folder)
{
    const QString f = cleanFolder(folder);
    if (f.isEmpty() || lookFolders(listId).contains(f))
        return false;
    return edit(tr("New folder %1").arg(f), [&](QList<Production> &ps) {
        CueList *l = listIn(ps, listId);
        if (l == nullptr || !l->looks)
            return false;
        l->folders << f;
        return true;
    });
}

QStringList ShowTreeController::lookFolders(quint32 listId) const
{
    QStringList out;
    const CueList *l = cueList(listId);
    if (l == nullptr || !l->looks)
        return out;
    auto add = [&out](const QString &path) {
        QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        for (int i = 1; i <= parts.count(); ++i)
        {
            const QString p = parts.mid(0, i).join(QLatin1Char('/'));
            if (!out.contains(p))
                out << p;
        }
    };
    for (const QString &f : l->folders)
        add(f);
    for (const Cue &c : l->cues)
        add(c.folder);
    std::sort(out.begin(), out.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return out;
}

bool ShowTreeController::renameLookFolder(quint32 listId, const QString &folder, const QString &name)
{
    const QString from = cleanFolder(folder);
    const QString leaf = cleanFolder(name);
    if (from.isEmpty() || leaf.isEmpty() || leaf.contains(QLatin1Char('/')))
        return false;
    const int slash = from.lastIndexOf(QLatin1Char('/'));
    const QString to = slash < 0 ? leaf : from.left(slash + 1) + leaf;
    if (to == from)
        return false;
    auto moved = [&](const QString &path) {
        return path == from ? to : to + path.mid(from.length());
    };
    return edit(tr("Rename folder %1").arg(from), [&](QList<Production> &ps) {
        CueList *l = listIn(ps, listId);
        if (l == nullptr || !l->looks)
            return false;
        bool any = false;
        for (Cue &c : l->cues)
            if (!c.folder.isEmpty() && inFolder(c.folder, from))
            {
                c.folder = moved(c.folder);
                any = true;
            }
        for (QString &f : l->folders)
            if (inFolder(f, from))
            {
                f = moved(f);
                any = true;
            }
        l->folders.removeDuplicates();
        return any;
    });
}

bool ShowTreeController::removeLookFolder(quint32 listId, const QString &folder)
{
    const QString gone = cleanFolder(folder);
    if (gone.isEmpty())
        return false;
    const int slash = gone.lastIndexOf(QLatin1Char('/'));
    const QString up = slash < 0 ? QString() : gone.left(slash);
    // What was in it moves up one level, keeping what is under it.
    auto moved = [&](const QString &path) {
        const QString rest = path.mid(gone.length() + 1);
        return path == gone ? up : up.isEmpty() ? rest : up + QLatin1Char('/') + rest;
    };
    return edit(tr("Delete folder %1").arg(gone), [&](QList<Production> &ps) {
        CueList *l = listIn(ps, listId);
        if (l == nullptr || !l->looks)
            return false;
        bool any = false;
        for (Cue &c : l->cues)
            if (!c.folder.isEmpty() && inFolder(c.folder, gone))
            {
                c.folder = moved(c.folder);
                any = true;
            }
        QStringList keep;
        for (const QString &f : l->folders)
        {
            if (!inFolder(f, gone))
                keep << f;
            else if (f != gone)
                keep << moved(f);
            any = true;
        }
        keep.removeAll(QString());
        keep.removeDuplicates();
        l->folders = keep;
        return any;
    });
}

QList<quint32> ShowTreeController::looksIn(quint32 listId, const QString &folder) const
{
    QList<quint32> out;
    const CueList *l = cueList(listId);
    if (l == nullptr || !l->looks)
        return out;
    const QString f = cleanFolder(folder);
    for (const Cue &c : l->cues)
        if (inFolder(c.folder, f))
            out << c.id;
    return out;
}

quint32 ShowTreeController::lookFunction(quint32 lookId) const
{
    const CueList *l = listOfCue(lookId);
    if (l == nullptr || !l->looks)
        return invalidId();
    const Cue *c = l->cue(lookId);
    return c == nullptr || m_doc->function(c->collection) == nullptr ? invalidId() : c->collection;
}

quint32 ShowTreeController::lookOfFunction(quint32 functionId) const
{
    if (functionId == invalidId())
        return invalidId();
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            if (l.looks)
                for (const Cue &c : l.cues)
                    if (c.collection == functionId)
                        return c.id;
    return invalidId();
}

quint32 ShowTreeController::adoptAsLook(quint32 productionId, quint32 functionId, QString *why)
{
    auto no = [why](const QString &text) {
        if (why != nullptr)
            *why = text;
        return invalidId();
    };
    const Function *f = m_doc->function(functionId);
    if (f == nullptr)
        return no(tr("No such function."));
    if (isGenerated(functionId))
        return no(tr("It is the Show Tree's already."));
    if (production(productionId) == nullptr)
        return no(tr("Select a production first."));

    // Read as an import reads a step's scene: targets and palettes become a
    // layer when that plays exactly the same; anything else is played as it
    // is, untouched (an opaque layer) -- the safety net (§6).
    Layer layer;
    QString reason;
    const Scene *scene = qobject_cast<const Scene *>(f);
    if (scene == nullptr || !sceneToLayer(scene, &layer, &reason))
    {
        layer = Layer();
        layer.target.function = functionId;
    }
    layer.name.clear();

    const quint32 sectionId = looksSection(productionId) == invalidId() ? newId() : invalidId();
    const quint32 lookId = newId();
    layer.id = newId();
    const bool ok = edit(tr("Adopt %1 as look").arg(f->name()), [&](QList<Production> &ps) {
        for (Production &p : ps)
        {
            if (p.id != productionId)
                continue;
            CueList *l = p.looks();
            if (l == nullptr)
            {
                CueList fresh;
                fresh.id = sectionId;
                fresh.name = tr("Looks");
                fresh.looks = true;
                p.lists << fresh;
                l = &p.lists.last();
            }
            Cue c;
            c.id = lookId;
            c.name = f->name();
            c.number = numberForInsert(*l, int(l->cues.count()) - 1);
            c.layers << layer;
            l->cues << c;
            return true;
        }
        return false;
    });
    if (!ok)
        return no(tr("Couldn't adopt it."));
    if (why != nullptr)
        *why = reason;      // read as it is, and why (empty: as a layer)
    return lookId;
}
