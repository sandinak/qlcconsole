/*
  Q Light Controller Plus - qlcconsole
  showreviewcuechecks.cpp

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

/* Show Review's checks over the Show Tree (docs/design/SHOW_REVIEW.md
   sections 3.2-3.4): palettes in cues, tracking across songs, cue numbers
   and skips, includes, and hits that fight in one place. They read the
   tree's model and its resolve (ShowReview's context resolves each list
   once a run). */

#include <QCoreApplication>
#include <QSet>
#include <algorithm>

#include "showreviewcontext.h"
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "lookstackcheck.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

static QString tr(const char *text)
{
    return QCoreApplication::translate("ShowReview", text);
}

static QString trn(const char *text, int n)
{
    return QCoreApplication::translate("ShowReview", text, nullptr, n);
}

static QString cueKey(const CueList &list, quint32 cueId)
{
    return QStringLiteral("list:%1/cue:%2").arg(list.id).arg(cueId);
}

static QString listKey(const CueList &list)
{
    return QStringLiteral("list:%1").arg(list.id);
}

/** A layer target as a key: its signature without the separators a target
 *  list uses. */
static QString targetKey(const Target &target)
{
    QString sig = target.signature();
    sig.replace(QLatin1Char(','), QLatin1Char('.'));
    return QStringLiteral("layer:") + sig;
}

/** The plain cue lists (not a Looks section, not sub-cues), with their production. */
static QList<QPair<const Production *, const CueList *>> plainLists(Doc *doc)
{
    QList<QPair<const Production *, const CueList *>> lists;
    for (const Production &p : doc->showTree()->productions())
        for (const CueList &l : p.lists)
            if (!l.looks && !l.isSubCues())
                lists << qMakePair(&p, &l);
    return lists;
}

/** What a look sets, for comparing: its palettes, its value, or "off". */
static QString lookValue(const Look &look)
{
    if (look.none)
        return QStringLiteral("none");
    QStringList ids;
    for (quint32 id : look.palettes)
        ids << QString::number(id);
    return ids.join(QLatin1Char('+')) + QLatin1Char('|') + look.value;
}

static QString lookName(ReviewContext &ctx, const Look &look)
{
    if (look.none)
        return tr("off");
    QStringList names;
    for (quint32 id : look.palettes)
        names << ctx.paletteName(id);
    if (names.isEmpty())
        return look.value;
    return names.join(QStringLiteral(" + "));
}

static QString fixtureNames(ReviewContext &ctx, const QList<quint32> &ids, int max = 3)
{
    QStringList names;
    for (int i = 0; i < ids.count() && i < max; ++i)
        names << ctx.fixtureName(ids.at(i));
    QString s = names.join(QStringLiteral(", "));
    if (ids.count() > max)
        s = trn("%1 and %n more", ids.count() - max).arg(s);
    return s;
}

/****************************************************************************
 * Palettes and looks (PAL)
 ****************************************************************************/

static void pal01(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    ShowTreeController *st = ctx.doc()->showTree();
    for (const Production &p : st->productions())
    {
        for (const CueList &list : p.lists)
        {
            for (int ci = 0; ci < list.cues.count(); ++ci)
            {
                const Cue &cue = list.cues.at(ci);
                if (cue.mixOverlap)
                    continue;                   // mixes as its chaser did, on purpose
                QList<const Layer *> top;
                QList<QSet<quint32>> fixtures;
                for (const Layer &l : cue.layers)
                {
                    if (l.parent != invalidId() || l.target.isOpaque())
                        continue;               // a child layer is a deliberate exception
                    top << &l;
                    const QList<quint32> fx = ctx.targetFixtures(l.target);
                    fixtures << QSet<quint32>(fx.begin(), fx.end());
                }
                for (int a = 0; a < top.count(); ++a)
                {
                    for (int b = a + 1; b < top.count(); ++b)
                    {
                        QSet<quint32> both = fixtures.at(a);
                        both.intersect(fixtures.at(b));
                        if (both.isEmpty())
                            continue;
                        for (const Look &la : top.at(a)->looks)
                        {
                            if (la.key == KeyDimmer || isLineParamKey(la.key) || la.key == KeyFormation)
                                continue;       // intensity mixes (HTP)
                            const Look *lb = top.at(b)->look(la.key);
                            if (lb == nullptr || lookValue(la) == lookValue(*lb))
                                continue;
                            QList<quint32> ids = both.values();
                            std::sort(ids.begin(), ids.end());
                            const QString nameA = top.at(a)->name.isEmpty() ? st->targetLabel(top.at(a)->target) : top.at(a)->name;
                            const QString nameB = top.at(b)->name.isEmpty() ? st->targetLabel(top.at(b)->target) : top.at(b)->name;
                            QByteArray fp = la.key.toUtf8() + ':' + lookValue(la).toUtf8() + '/'
                                            + lookValue(*lb).toUtf8() + ':';
                            for (quint32 id : ids)
                                fp += QByteArray::number(id) + ',';
                            out << ReviewContext::make(QStringLiteral("PAL-01"), ReviewFinding::Warning,
                                tr("%1: %2 is in %3 (%4) and %5 (%6) — %7. %6 wins because it's lower in "
                                   "the cue; change the order and the look changes.")
                                    .arg(ctx.cueText(list, ci), fixtureNames(ctx, ids), nameA,
                                         lookName(ctx, la), nameB, lookName(ctx, *lb),
                                         keyLabel(la.key).toLower()),
                                { cueKey(list, cue.id), QStringLiteral("lyr:%1").arg(top.at(a)->id),
                                  QStringLiteral("lyr:%1").arg(top.at(b)->id) },
                                fp, QStringLiteral("cue"), QVariantList() << cue.id);
                        }
                    }
                }
            }
        }
    }
}

static void pal02(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    Doc *doc = ctx.doc();
    for (Function *f : doc->functionsByType(Function::SceneType))
    {
        Scene *scene = qobject_cast<Scene *>(f);
        // The tree's own scenes: PAL-01 / PAL-03 say it in the cue's words.
        if (scene == nullptr || doc->showTree()->isGenerated(scene->id()))
            continue;
        for (const LookStackCheck::Issue &issue : LookStackCheck::check(doc, scene))
        {
            QStringList keys { QStringLiteral("fn:%1").arg(scene->id()) };
            if (issue.paletteId != QLCPalette::invalidId())
                keys << QStringLiteral("pal:%1").arg(issue.paletteId);
            out << ReviewContext::make(QStringLiteral("PAL-02"),
                issue.level == LookStackCheck::Issue::Warning ? ReviewFinding::Warning : ReviewFinding::Note,
                tr("Scene '%1': %2").arg(scene->name(), issue.text), keys, issue.text.toUtf8(),
                QStringLiteral("function"), QVariantList() << scene->id());
        }
    }
}

static void pal03(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    ShowTreeController *st = ctx.doc()->showTree();
    for (const Production &p : st->productions())
    {
        for (const CueList &list : p.lists)
        {
            for (int ci = 0; ci < list.cues.count(); ++ci)
            {
                const Cue &cue = list.cues.at(ci);
                for (const Layer &l : cue.layers)
                {
                    if (l.target.isOpaque())
                        continue;
                    const int total = ctx.targetFixtures(l.target).count();
                    if (total == 0)
                        continue;
                    for (const Look &look : l.looks)
                    {
                        if (look.none || look.palettes.isEmpty() || isLineParamKey(look.key) || look.key == KeyFormation)
                            continue;
                        const QList<quint32> misses = st->lookMisses(l.target, look);
                        if (misses.isEmpty())
                            continue;
                        const bool none = misses.count() >= total;
                        const QString target = l.name.isEmpty() ? st->targetLabel(l.target) : l.name;
                        const QString msg = none
                            ? tr("%1 puts %2 on %3, which can't play it: no %4 there.")
                                  .arg(ctx.cueText(list, ci), lookName(ctx, look), target,
                                       keyLabel(look.key).toLower())
                            : tr("%1 puts %2 on %3: it plays on %4 of %5 (not on %6).")
                                  .arg(ctx.cueText(list, ci), lookName(ctx, look), target)
                                  .arg(total - misses.count()).arg(total).arg(fixtureNames(ctx, misses));
                        QByteArray fp = lookValue(look).toUtf8() + ':' + QByteArray::number(total) + ':';
                        for (quint32 id : misses)
                            fp += QByteArray::number(id) + ',';
                        out << ReviewContext::make(QStringLiteral("PAL-03"),
                            none ? ReviewFinding::Warning : ReviewFinding::Note, msg,
                            { cueKey(list, cue.id), QStringLiteral("lyr:%1").arg(l.id),
                              QStringLiteral("key:%1").arg(look.key) },
                            fp, QStringLiteral("cue"), QVariantList() << cue.id);
                    }
                }
            }
        }
    }
}

static void pal05(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    Doc *doc = ctx.doc();
    ShowTreeController *st = doc->showTree();
    // Cue layers: a palette, group or fixture that is gone, or a group now empty.
    for (const Production &p : st->productions())
    {
        for (const CueList &list : p.lists)
        {
            for (int ci = 0; ci < list.cues.count(); ++ci)
            {
                const Cue &cue = list.cues.at(ci);
                QStringList gone;
                QByteArray fp;
                auto lookGone = [&](const QList<Look> &looks) {
                    for (const Look &look : looks)
                        for (quint32 pid : look.palettes + look.own)
                            if (doc->palette(pid) == nullptr)
                            {
                                gone << tr("palette %1 (deleted)").arg(pid);
                                fp += "p" + QByteArray::number(pid) + ',';
                            }
                };
                lookGone(cue.looks);
                for (const Layer &l : cue.layers)
                {
                    lookGone(l.looks);
                    for (quint32 gid : l.target.groups)
                    {
                        FixtureGroup *g = doc->fixtureGroup(gid);
                        if (g == nullptr)
                        {
                            gone << tr("group %1 (deleted)").arg(gid);
                            fp += "g" + QByteArray::number(gid) + ',';
                        }
                        else if (g->fixtureList().isEmpty())
                        {
                            gone << tr("group '%1' (empty)").arg(g->name());
                            fp += "e" + QByteArray::number(gid) + ',';
                        }
                    }
                    for (quint32 fid : l.target.fixtures)
                        if (doc->fixture(fid) == nullptr)
                        {
                            gone << tr("fixture %1 (deleted)").arg(fid);
                            fp += "f" + QByteArray::number(fid) + ',';
                        }
                }
                if (gone.isEmpty())
                    continue;
                gone.removeDuplicates();
                out << ReviewContext::make(QStringLiteral("PAL-05"), ReviewFinding::Warning,
                    tr("%1 points at what isn't there: %2. It plays on nothing there.")
                        .arg(ctx.cueText(list, ci), gone.join(QStringLiteral(", "))),
                    { cueKey(list, cue.id) }, fp, QStringLiteral("cue"), QVariantList() << cue.id);
            }
        }
    }
    // Scenes playing a deleted palette.
    for (Function *f : doc->functionsByType(Function::SceneType))
    {
        Scene *scene = qobject_cast<Scene *>(f);
        if (scene == nullptr || st->isGenerated(scene->id()))
            continue;
        QList<quint32> missing;
        for (quint32 pid : scene->palettes())
            if (doc->palette(pid) == nullptr)
                missing << pid;
        if (missing.isEmpty())
            continue;
        QByteArray fp;
        for (quint32 pid : missing)
            fp += QByteArray::number(pid) + ',';
        out << ReviewContext::make(QStringLiteral("PAL-05"), ReviewFinding::Warning,
            trn("Scene '%1' plays %n palette(s) that were deleted.", missing.count()).arg(scene->name()),
            { QStringLiteral("fn:%1").arg(scene->id()) }, fp, QStringLiteral("function"),
            QVariantList() << scene->id());
    }
    // Aim palettes at a stage target that was deleted.
    MonitorProperties *mp = doc->monitorProperties();
    for (QLCPalette *pal : doc->palettes())
    {
        if (pal == nullptr || pal->type() != QLCPalette::Aim || pal->stageTargetId() == QLCPalette::invalidId())
            continue;
        if (mp != nullptr && mp->stageTarget(pal->stageTargetId()) != nullptr)
            continue;
        out << ReviewContext::make(QStringLiteral("PAL-05"), ReviewFinding::Warning,
            tr("Position palette '%1' aims at a stage target that was deleted.").arg(pal->name()),
            { QStringLiteral("pal:%1").arg(pal->id()) }, QByteArray::number(pal->stageTargetId()),
            QStringLiteral("palette"), QVariantList() << pal->id());
    }
}

/****************************************************************************
 * Tracking and cues (CUE)
 ****************************************************************************/

static void cue01and02(ReviewContext &ctx, QList<ReviewFinding> &out, bool values)
{
    for (const auto &pl : plainLists(ctx.doc()))
    {
        const Production &p = *pl.first;
        const CueList &list = *pl.second;
        // decided (SHOW_REVIEW.md Q1, can change): songs are the numbering
        // scheme's song sections (Q190); no separate "song starts here" flag.
        const QList<Section> sections = sectionsOf(list, p.numbering);
        if (sections.count() < 2)
            continue;
        const QList<ResolvedCue> &res = ctx.resolved(list);
        if (res.count() != list.cues.count())
            continue;
        QHash<quint32, int> sectionOfCue;
        for (int i = 0; i < list.cues.count(); ++i)
            sectionOfCue.insert(list.cues.at(i).id, sectionOfIndex(sections, i));
        // CUE-01: a value from one song still on in a later one, said once at
        // the last cue it reaches.
        struct Crossing { int lastIndex = -1; int fromIndex = -1; QString key; const ResolvedLayer *layer = nullptr; };
        QMap<QString, Crossing> crossings;
        for (int i = 0; i < list.cues.count(); ++i)
        {
            const Cue &cue = list.cues.at(i);
            if (cue.skip)
                continue;
            const int here = sectionOfIndex(sections, i);
            if (here < 0)
                continue;
            int trackedIn = 0;
            QByteArray fp;
            for (const ResolvedLayer &layer : res.at(i).layers)
            {
                bool layerIn = false;
                for (auto it = layer.values.constBegin(); it != layer.values.constEnd(); ++it)
                {
                    const ResolvedValue &v = it.value();
                    if (v.origin != Origin::Tracked || v.look.none)
                        continue;
                    const int from = sectionOfCue.value(v.fromCue, -1);
                    if (from < 0 || from == here)
                        continue;
                    layerIn = true;
                    const QString k = layer.target.signature() + QLatin1Char('#') + it.key()
                                      + QLatin1Char('@') + QString::number(v.fromCue);
                    Crossing &c = crossings[k];
                    c.lastIndex = i;
                    c.fromIndex = list.indexOf(v.fromCue);
                    c.key = it.key();
                    c.layer = &layer;
                }
                if (layerIn)
                {
                    ++trackedIn;
                    fp += layer.target.signature().toUtf8() + ';';
                }
            }
            // CUE-02: the top of a song that tracks in from the song before.
            if (!values && trackedIn > 0 && sections.at(here).first == i
                && effectiveMode(list, cue) == CueMode::Track)
            {
                out << ReviewContext::make(QStringLiteral("CUE-02"), ReviewFinding::Warning,
                    trn("%1, the top of %2, tracks %n layer(s) in from the song before: editing the end "
                        "of that song changes this one. Make it a Block cue?", trackedIn)
                        .arg(ctx.cueText(list, i), sectionTitle(sections.at(here))),
                    { cueKey(list, cue.id) }, fp, QStringLiteral("cue"), QVariantList() << cue.id);
            }
        }
        if (!values)
            continue;
        for (auto it = crossings.constBegin(); it != crossings.constEnd(); ++it)
        {
            const Crossing &c = it.value();
            if (c.fromIndex < 0 || c.lastIndex < 0)
                continue;
            const Cue &from = list.cues.at(c.fromIndex);
            const int songs = sectionOfIndex(sections, c.lastIndex) - sectionOfIndex(sections, c.fromIndex);
            const QString what = c.layer->name.isEmpty() ? ctx.doc()->showTree()->targetLabel(c.layer->target)
                                                         : c.layer->name;
            out << ReviewContext::make(QStringLiteral("CUE-01"), ReviewFinding::Warning,
                trn("%1's %2 from %3 is still on in %4 — it tracks across %n song(s).", songs)
                    .arg(what, keyLabel(c.key).toLower(), ctx.cueText(list, c.fromIndex),
                         ctx.doc()->showTree()->cueLabel(list, c.lastIndex)),
                { cueKey(list, from.id), targetKey(c.layer->target), QStringLiteral("key:%1").arg(c.key) },
                QByteArray::number(list.cues.at(c.lastIndex).id), QStringLiteral("cue"),
                QVariantList() << from.id);
        }
    }
}

static void cue01(ReviewContext &ctx, QList<ReviewFinding> &out) { cue01and02(ctx, out, true); }
static void cue02(ReviewContext &ctx, QList<ReviewFinding> &out) { cue01and02(ctx, out, false); }

static void cue03(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    ShowTreeController *st = ctx.doc()->showTree();
    for (const auto &pl : plainLists(ctx.doc()))
    {
        const CueList &list = *pl.second;
        const QList<ResolvedCue> &res = ctx.resolved(list);
        if (res.count() != list.cues.count())
            continue;
        // A cue with sub-cues does something more than its own look.
        QSet<quint32> withSubs;
        for (const Production &prod : st->productions())
            for (const CueList &l : prod.lists)
                if (l.isSubCues())
                    withSubs.insert(l.subOf);
        for (int i = 0; i < list.cues.count(); ++i)
        {
            const Cue &cue = list.cues.at(i);
            const int before = playedBefore(list, i);
            if (cue.skip || before < 0 || cue.focusCall || withSubs.contains(cue.id)
                || res.at(i).layers.isEmpty())              // empty is CUE-04's
                continue;
            const Cue &prev = list.cues.at(before);
            if (prev.focusCall || stageOf(res.at(i)) != stageOf(res.at(before)))
                continue;
            out << ReviewContext::make(QStringLiteral("CUE-03"), ReviewFinding::Note,
                tr("%1 looks exactly like %2: GO on it changes nothing.")
                    .arg(ctx.cueText(list, i), st->cueLabel(list, before)),
                { cueKey(list, cue.id) }, QByteArray::number(prev.id), QStringLiteral("cue"),
                QVariantList() << cue.id);
        }
    }
}

/** "VC button 'Act 1'" */
static QString vcWidgetText(const VcWidget &w)
{
    QString kind = tr("VC widget");
    if (w.type == QLatin1String("Button"))
        kind = tr("VC button");
    else if (w.type == QLatin1String("CueList"))
        kind = tr("VC Cue List");
    else if (w.type == QLatin1String("Slider"))
        kind = tr("VC slider");
    else if (w.type == QLatin1String("Matrix"))
        kind = tr("VC matrix");
    return QStringLiteral("%1 '%2'").arg(kind, w.label());
}

static void cue12(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    // decided (can change): only (a) of the design: an original kept for
    // Restore original that a chaser, collection, timeline or Virtual
    // Console widget still plays. An original edited since, or a generated
    // function edited by hand, isn't compared (it needs a rebake compare).
    Doc *doc = ctx.doc();
    ShowTreeController *st = doc->showTree();
    for (const auto &pl : plainLists(doc))
    {
        const CueList &list = *pl.second;
        if (!list.inPlace)
            continue;
        for (const CueList::Original &o : list.originals)
        {
            Function *orig = doc->function(o.function);
            if (orig == nullptr)
                continue;
            QStringList users;
            quint32 firstUser = o.function;
            QStringList keys { QStringLiteral("fn:%1").arg(o.function), listKey(list) };
            for (Function *f : doc->functions())
            {
                if (f == nullptr || f == orig || st->isLocked(f->id()) || !f->components().contains(o.function))
                    continue;
                if (users.isEmpty())
                    firstUser = f->id();
                users << QStringLiteral("'%1'").arg(f->name());
                keys << QStringLiteral("fn:%1").arg(f->id());
            }
            for (const VcWidget &w : ctx.vc().widgets)
            {
                if (w.id == 0xFFFFFFFF || w.type == QLatin1String("CueList") || !w.allFunctions().contains(o.function))
                    continue;
                users << vcWidgetText(w);
                keys << QStringLiteral("vc:%1").arg(w.id);
            }
            if (users.isEmpty())
                continue;
            out << ReviewContext::make(QStringLiteral("CUE-12"), ReviewFinding::Warning,
                tr("%1 still plays the original '%2', not the Show Tree list '%3' made from it.")
                    .arg(users.join(QStringLiteral(", ")), orig->name(), list.name),
                keys, users.join(QLatin1Char(',')).toUtf8(), QStringLiteral("function"),
                QVariantList() << firstUser);
        }
    }
}

static void cue04(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    for (const auto &pl : plainLists(ctx.doc()))
    {
        const CueList &list = *pl.second;
        for (int i = 0; i < list.cues.count(); ++i)
        {
            const Cue &cue = list.cues.at(i);
            if (!cue.layers.isEmpty() || !cue.looks.isEmpty() || !cue.includes.isEmpty()
                || cue.reusable != invalidId() || cue.focusCall)
                continue;
            const bool block = effectiveMode(list, cue) == CueMode::Block;
            // A cue called a blackout says what it is.
            if (block && cue.name.contains(QStringLiteral("black"), Qt::CaseInsensitive))
                continue;
            out << ReviewContext::make(QStringLiteral("CUE-04"), ReviewFinding::Note,
                block ? tr("%1 is empty — a blackout. Name it 'Blackout', or give it a look.")
                            .arg(ctx.cueText(list, i))
                      : tr("%1 is empty: it changes nothing.").arg(ctx.cueText(list, i)),
                { cueKey(list, cue.id) }, QByteArray(block ? "block" : "track"),
                QStringLiteral("cue"), QVariantList() << cue.id);
        }
    }
}

static void cue05(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    for (const auto &pl : plainLists(ctx.doc()))
    {
        const CueList &list = *pl.second;
        for (int i = 0; i < list.cues.count(); ++i)
        {
            const Cue &cue = list.cues.at(i);
            // hold: Function::infiniteSpeed() (0xFFFFFFFE) waits for GO.
            if (cue.skip || cue.hold != 0 || cue.fadeIn != 0 || playedAfter(list, i) < 0)
                continue;
            out << ReviewContext::make(QStringLiteral("CUE-05"), ReviewFinding::Warning,
                tr("%1 follows on at once with no fade and no hold: it's never seen.")
                    .arg(ctx.cueText(list, i)),
                { cueKey(list, cue.id) }, QByteArray("0/0"), QStringLiteral("cue"),
                QVariantList() << cue.id);
        }
    }
}

static void cue06(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    for (const auto &pl : plainLists(ctx.doc()))
    {
        const CueList &list = *pl.second;
        QMap<QString, QList<int>> byNumber;
        for (int i = 0; i < list.cues.count(); ++i)
            if (!list.cues.at(i).number.isEmpty())
                byNumber[list.cues.at(i).number] << i;
        for (auto it = byNumber.constBegin(); it != byNumber.constEnd(); ++it)
        {
            if (it.value().count() < 2)
                continue;
            QStringList keys { listKey(list) };
            for (int i : it.value())
                keys << cueKey(list, list.cues.at(i).id);
            out << ReviewContext::make(QStringLiteral("CUE-06"), ReviewFinding::Warning,
                trn("%n cues are numbered %1 in %2: GO, show control and the SM can't tell them apart.",
                    it.value().count()).arg(it.key(), list.name),
                keys, it.key().toUtf8(), QStringLiteral("cue"),
                QVariantList() << list.cues.at(it.value().first()).id);
        }
    }
}

static void cue07(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    ShowTreeController *st = ctx.doc()->showTree();
    for (const auto &pl : plainLists(ctx.doc()))
    {
        const CueList &list = *pl.second;
        QStringList numbers, keys { listKey(list) };
        QSet<quint32> included;
        for (const Cue &c : list.cues)
            for (quint32 inc : c.includes)
                included.insert(inc);
        QStringList worse;
        for (int i = 0; i < list.cues.count(); ++i)
        {
            const Cue &cue = list.cues.at(i);
            if (!cue.skip)
                continue;
            numbers << st->cueLabel(list, i).section(QLatin1Char(' '), 0, 0);
            keys << cueKey(list, cue.id);
            if (cue.id == list.baseCue)
                worse << tr("cue %1 is the base cue").arg(numbers.last());
            if (included.contains(cue.id))
                worse << tr("cue %1 is included by another cue").arg(numbers.last());
        }
        if (numbers.isEmpty())
            continue;
        QString msg = trn("%n cue(s) are skipped in %1: %2. Skip is for rehearsal; left on, it's a "
                          "missing cue on the night.", numbers.count())
                          .arg(list.name, numbers.join(QStringLiteral(", ")));
        if (!worse.isEmpty())
            msg += QLatin1Char(' ') + tr("Worse: %1.").arg(worse.join(QStringLiteral("; ")));
        out << ReviewContext::make(QStringLiteral("CUE-07"), ReviewFinding::Warning, msg, keys,
            numbers.join(QLatin1Char(',')).toUtf8(), QStringLiteral("list"), QVariantList() << list.id);
    }
}

static void cue08(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    ShowTreeController *st = ctx.doc()->showTree();
    for (const auto &pl : plainLists(ctx.doc()))
    {
        const CueList &list = *pl.second;
        int firstRelease = -1;
        for (int i = 0; i < list.cues.count() && firstRelease < 0; ++i)
            for (const Layer &l : list.cues.at(i).layers)
                if (l.release && !list.cues.at(i).skip)
                {
                    firstRelease = i;
                    break;
                }
        if (firstRelease < 0)
            continue;
        QString msg;
        QByteArray fp;
        const int baseIdx = list.baseCue == invalidId() ? -1 : list.indexOf(list.baseCue);
        ReviewFinding::Severity severity = ReviewFinding::Warning;
        if (list.baseCue == invalidId())
        {
            // decided (can change): releasing to dark in a list with no base
            // cue is a way of working (the course's), so only a note.
            msg = tr("%1 releases layers, and %2 has no base cue: they go dark. Set a base cue if "
                     "they should go back to a look instead.")
                      .arg(ctx.cueText(list, firstRelease), list.name);
            fp = "none";
            severity = ReviewFinding::Note;
        }
        else if (baseIdx < 0)
        {
            msg = tr("%1 releases layers to the base cue, but %2's base cue was deleted: they go dark.")
                      .arg(ctx.cueText(list, firstRelease), list.name);
            fp = "deleted";
        }
        else if (list.cues.at(baseIdx).skip)
        {
            msg = tr("%2's base cue, %1, is skipped: what's released to it goes dark.")
                      .arg(st->cueLabel(list, baseIdx), list.name);
            fp = "skipped";
        }
        else if (baseIdx > firstRelease)
        {
            msg = tr("%1 releases to the base cue, %2, which comes after it.")
                      .arg(ctx.cueText(list, firstRelease), st->cueLabel(list, baseIdx));
            fp = "after";
        }
        else
            continue;
        out << ReviewContext::make(QStringLiteral("CUE-08"), severity, msg,
            { listKey(list) }, fp, QStringLiteral("list"), QVariantList() << list.id);
    }
}

static void cue11(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    ShowTreeController *st = ctx.doc()->showTree();
    QSet<quint32> reusableLayers, reusableCues;
    for (const ReusableLayer &r : st->reusableLayers())
        reusableLayers.insert(r.id);
    for (const ReusableCue &r : st->reusableCues())
        reusableCues.insert(r.id);
    for (const Production &p : st->productions())
    {
        for (const CueList &list : p.lists)
        {
            if (list.isSubCues())
            {
                bool parent = false;
                for (const CueList &other : p.lists)
                    parent = parent || other.cue(list.subOf) != nullptr;
                if (!parent)
                    out << ReviewContext::make(QStringLiteral("CUE-11"), ReviewFinding::Blocker,
                        tr("The sub-cues '%1' belong to a cue that was deleted.").arg(list.name),
                        { listKey(list) }, QByteArray::number(list.subOf), QStringLiteral("list"),
                        QVariantList() << list.id);
                continue;
            }
            if (list.looks)
                continue;
            const QList<ResolvedCue> &res = ctx.resolved(list);
            for (int i = 0; i < list.cues.count(); ++i)
            {
                const Cue &cue = list.cues.at(i);
                QStringList broken;
                for (quint32 inc : cue.includes)
                    if (list.cue(inc) == nullptr)
                        broken << tr("an included cue that was deleted");
                if (cue.reusable != invalidId() && !reusableCues.contains(cue.reusable))
                    broken << tr("a reusable cue that was deleted");
                for (const Layer &l : cue.layers)
                    if (l.reusable != invalidId() && !reusableLayers.contains(l.reusable))
                        broken << tr("a reusable layer that was deleted");
                if (!broken.isEmpty())
                {
                    broken.removeDuplicates();
                    out << ReviewContext::make(QStringLiteral("CUE-11"), ReviewFinding::Blocker,
                        tr("%1 uses %2.").arg(ctx.cueText(list, i), broken.join(QStringLiteral(", "))),
                        { cueKey(list, cue.id) }, broken.join(QLatin1Char(',')).toUtf8(),
                        QStringLiteral("cue"), QVariantList() << cue.id);
                }
                if (i < res.count() && !res.at(i).skippedIncludes.isEmpty())
                {
                    QStringList loops;
                    QByteArray fp;
                    for (quint32 inc : res.at(i).skippedIncludes)
                    {
                        loops << st->cueLabel(list, list.indexOf(inc));
                        fp += QByteArray::number(inc) + ',';
                    }
                    out << ReviewContext::make(QStringLiteral("CUE-11"), ReviewFinding::Warning,
                        tr("%1 includes %2, which would loop back to it — left out.")
                            .arg(ctx.cueText(list, i), loops.join(QStringLiteral(", "))),
                        { cueKey(list, cue.id) }, fp, QStringLiteral("cue"), QVariantList() << cue.id);
                }
            }
        }
    }
}

/****************************************************************************
 * Hits (VC)
 ****************************************************************************/

/** What a hit puts on stage per fixture and attribute (not intensity, which
 *  mixes): the winning look's value and name. */
QHash<QPair<quint32, QString>, QPair<QString, QString>> ShowReviewChecks::hitStage(ReviewContext &ctx, const ShowTree::ResolvedCue &rc)
{
    QHash<QPair<quint32, QString>, QPair<QString, QString>> stage;
    for (const ResolvedLayer &layer : rc.layers)
    {
        const QList<quint32> fixtures = ctx.targetFixtures(layer.target);
        for (auto it = layer.values.constBegin(); it != layer.values.constEnd(); ++it)
        {
            if (it.key() == KeyDimmer || isLineParamKey(it.key()) || it.key() == KeyFormation || it.value().look.none)
                continue;
            const QPair<QString, QString> v(lookValue(it.value().look), lookName(ctx, it.value().look));
            for (quint32 fid : fixtures)
                stage.insert(qMakePair(fid, it.key()), v);   // later layers win
        }
    }
    return stage;
}

static void vc01(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    ShowTreeController *st = ctx.doc()->showTree();
    for (const Production &p : st->productions())
    {
        const CueList *looks = p.looks();
        if (looks == nullptr || looks->cues.count() < 2)
            continue;
        const QList<ResolvedCue> &res = ctx.resolved(*looks);
        if (res.count() != looks->cues.count())
            continue;
        QList<QHash<QPair<quint32, QString>, QPair<QString, QString>>> stages;
        for (const ResolvedCue &rc : res)
            stages << ShowReviewChecks::hitStage(ctx, rc);
        for (int a = 0; a < looks->cues.count(); ++a)
        {
            for (int b = a + 1; b < looks->cues.count(); ++b)
            {
                const Cue &ha = looks->cues.at(a), &hb = looks->cues.at(b);
                if (!st->sameHitPlace(ha.id, hb.id))
                    continue;
                QMap<QString, QList<quint32>> fights;     // attribute -> fixtures
                for (auto it = stages.at(a).constBegin(); it != stages.at(a).constEnd(); ++it)
                {
                    auto other = stages.at(b).constFind(it.key());
                    if (other != stages.at(b).constEnd() && other.value().first != it.value().first)
                        fights[it.key().second] << it.key().first;
                }
                if (fights.isEmpty())
                    continue;
                QStringList what;
                QList<quint32> all;
                QByteArray fp;
                for (auto it = fights.begin(); it != fights.end(); ++it)
                {
                    std::sort(it.value().begin(), it.value().end());
                    what << keyLabel(it.key()).toLower();
                    for (quint32 fid : it.value())
                    {
                        if (!all.contains(fid))
                            all << fid;
                        fp += it.key().toUtf8() + QByteArray::number(fid) + ',';
                    }
                }
                std::sort(all.begin(), all.end());
                const QString nameA = ha.name.isEmpty() ? st->cueLabel(*looks, a) : ha.name;
                const QString nameB = hb.name.isEmpty() ? st->cueLabel(*looks, b) : hb.name;
                // decided (can change): any two hits in one place can be on
                // together; a button that steps through them is accepted with a note.
                out << ReviewContext::make(QStringLiteral("VC-01"), ReviewFinding::Warning,
                    tr("Hits '%1' and '%2' (%3) both set the %4 of %5 and can be on at once — "
                       "whichever was pressed last wins.")
                        .arg(nameA, nameB, st->hitPlace(ha.id), what.join(tr(" and ")), fixtureNames(ctx, all)),
                    { QStringLiteral("list:%1/cue:%2").arg(looks->id).arg(ha.id),
                      QStringLiteral("list:%1/cue:%2").arg(looks->id).arg(hb.id) },
                    fp, QStringLiteral("cue"), QVariantList() << ha.id);
            }
        }
    }
}

/****************************************************************************
 * The list
 ****************************************************************************/

QList<ShowReview::Check> ShowReviewChecks::treeChecks()
{
    typedef ShowReview S;
    return {
        { QStringLiteral("PAL-01"), tr("Two layers, one attribute"), S::Palettes,
          tr("When two layers of a cue light the same fixture and set the same attribute "
             "differently, the lower one wins. Change the order (or a group) and the look "
             "changes. Make one a child layer, or take the fixture out of one group."), pal01 },
        { QStringLiteral("PAL-02"), tr("A look that never shows"), S::Palettes,
          tr("A look the one below always covers, an effect that matches none of its fixtures, or "
             "a dimmer that reaches only some heads: it's in the scene, but not on stage."), pal02 },
        { QStringLiteral("PAL-03"), tr("A palette its fixtures can't play"), S::Palettes,
          tr("A colour on lights with no colour, a gobo on lights with no wheel, a position on "
             "lights that can't move: they ignore it."), pal03 },
        { QStringLiteral("PAL-05"), tr("Pointing at nothing"), S::Palettes,
          tr("A cue, scene or palette that names a palette, group, fixture or stage target that "
             "was deleted plays on nothing there, silently."), pal05 },
        { QStringLiteral("CUE-01"), tr("Tracks into another song"), S::Cues,
          tr("In a tracking list a value stays until a cue changes it. A special from one song "
             "still up three songs later is the classic tracking surprise."), cue01 },
        { QStringLiteral("CUE-02"), tr("No block at the top of a song"), S::Cues,
          tr("When the first cue of a song tracks, editing the end of the song before changes "
             "it. A Block cue at the top of each song keeps songs apart."), cue02 },
        { QStringLiteral("CUE-03"), tr("A cue that changes nothing"), S::Cues,
          tr("A cue that puts out exactly what the cue before did: the stage manager calls GO and "
             "nothing happens. Usually its content moved to another cue."), cue03 },
        { QStringLiteral("CUE-04"), tr("Empty cue"), S::Cues,
          tr("A cue with nothing in it. In a Block cue that's a blackout, which is fine when it's "
             "called one."), cue04 },
        { QStringLiteral("CUE-05"), tr("Auto-follow with no time"), S::Cues,
          tr("A cue that goes on to the next at once, with no fade and no hold, is never seen."), cue05 },
        { QStringLiteral("CUE-06"), tr("Duplicate cue numbers"), S::Cues,
          tr("Two cues with one number in a list: the stage manager, GO by number and show "
             "control can't tell them apart."), cue06 },
        { QStringLiteral("CUE-07"), tr("Skipped cues left skipped"), S::Cues,
          tr("Skip is a rehearsal tool. A skip left on is a missing cue on opening night; "
             "worst when it's the base cue, or another cue includes it."), cue07 },
        { QStringLiteral("CUE-08"), tr("Base cue problems"), S::Cues,
          tr("A released layer goes back to the base cue. With no base cue, a deleted or "
             "skipped one, it goes dark instead."), cue08 },
        { QStringLiteral("CUE-11"), tr("Include loops and dangling references"), S::Cues,
          tr("An include that would loop is left out; an include, reusable or sub-cue list whose "
             "cue was deleted plays nothing."), cue11 },
        { QStringLiteral("CUE-12"), tr("Originals still played"), S::Cues,
          tr("After Import in place the Show Tree list plays the show and the chaser it came "
             "from is kept to Restore. Something that still plays the old chaser plays a show "
             "that no longer matches the list."), cue12 },
        { QStringLiteral("VC-01"), tr("Hits that fight in one place"), S::Hits,
          tr("Two hits in the same place that set the same attribute on the same lights: the "
             "stage depends on which was pressed last. Give them different places, or different "
             "lights, or accept it when one button steps through them."), vc01 },
    };
}
