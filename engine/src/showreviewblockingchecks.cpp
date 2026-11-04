/*
  Q Light Controller Plus
  showreviewblockingchecks.cpp

  Show Review over blocking (docs/design/BLOCKING.md, board Q211): BLK-01,
  a blocking that leaves a target where no light aimed at it can reach. The
  reach is the Aim Solver's (AimSolver::sceneAims, as the look panel and the
  rig view use it), read in each blocking a cue plays with.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QCoreApplication>
#include <QSet>
#include "showreviewcontext.h"
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "collection.h"
#include "aimsolver.h"
#include "blocking.h"
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

/** Heads of @a scene that can't reach their target in blocking @a set
 *  although they can in Default, by target. */
static QHash<quint32, QList<quint32>> unreachedIn(Doc *doc, const Scene *scene, quint32 set)
{
    QHash<quint32, QList<quint32>> out;
    QSet<QPair<quint32, quint32>> fine;
    {
        BlockingScope def(MonitorProperties::defaultBlocking());
        for (const AimSolver::SceneAim &a : AimSolver::sceneAims(doc, scene))
            if (a.solution.reachable())
                fine.insert(qMakePair(a.targetId, a.fixtureId));
    }
    BlockingScope scope(set);
    const MonitorProperties *mp = doc->monitorProperties();
    for (const AimSolver::SceneAim &a : AimSolver::sceneAims(doc, scene))
    {
        const StageTarget *t = mp->stageTarget(a.targetId);
        if (t == nullptr || !t->isBlockedIn(set) || a.solution.reachable()
            || !fine.contains(qMakePair(a.targetId, a.fixtureId)))
            continue;
        if (!out.value(a.targetId).contains(a.fixtureId))
            out[a.targetId] << a.fixtureId;
    }
    return out;
}

static void blk01(ReviewContext &ctx, QList<ReviewFinding> &out)
{
    Doc *doc = ctx.doc();
    MonitorProperties *mp = doc->monitorProperties();
    ShowTreeController *st = doc->showTree();
    if (mp == nullptr || st == nullptr || mp->blockingSets().isEmpty())
        return;
    // Once per blocking, target and scene: the first cue that plays it says so.
    QSet<QString> said;
    for (const Production &p : st->productions())
        for (const CueList &list : p.lists)
        {
            if (list.looks)
                continue;
            for (int ci = 0; ci < list.cues.count(); ++ci)
            {
                const Cue &cue = list.cues.at(ci);
                const quint32 set = st->cueBlocking(cue.id);
                if (set == 0 || set == invalidId() || !mp->hasBlockingSet(set))
                    continue;
                const Collection *coll = qobject_cast<const Collection *>(doc->function(cue.collection));
                if (coll == nullptr)
                    continue;
                for (quint32 fid : coll->functions())
                {
                    const Scene *scene = qobject_cast<const Scene *>(doc->function(fid));
                    if (scene == nullptr)
                        continue;
                    const QHash<quint32, QList<quint32>> missed = unreachedIn(doc, scene, set);
                    for (auto it = missed.constBegin(); it != missed.constEnd(); ++it)
                    {
                        const QString key = QStringLiteral("%1/%2/%3").arg(set).arg(it.key()).arg(fid);
                        if (said.contains(key))
                            continue;
                        said.insert(key);
                        const StageTarget *t = mp->stageTarget(it.key());
                        QStringList names;
                        for (int k = 0; k < it.value().count() && k < 4; ++k)
                            names << ctx.fixtureName(it.value().at(k));
                        QString who = names.join(QStringLiteral(", "));
                        if (it.value().count() > 4)
                            who = trn("%1 and %n more", it.value().count() - 4).arg(who);
                        QStringList keys { QStringLiteral("list:%1/cue:%2").arg(list.id).arg(cue.id),
                                           QStringLiteral("blocking:%1").arg(set),
                                           QStringLiteral("target:%1").arg(it.key()) };
                        out << ReviewContext::make(QStringLiteral("BLK-01"), ReviewFinding::Warning,
                            trn("%1: blocking '%2' puts %3 where %n light(s) aimed at it can't reach (%4). "
                                "Move the mark, or light it from somewhere else.", it.value().count())
                                .arg(ctx.cueText(list, ci), mp->blockingName(set),
                                     t != nullptr ? t->name() : QString(), who),
                            keys, key.toUtf8(), QStringLiteral("cue"), QVariantList() << cue.id);
                    }
                }
            }
        }
}

QList<ShowReview::Check> ShowReviewChecks::blockingChecks()
{
    typedef ShowReview S;
    return {
        { QStringLiteral("BLK-01"), tr("A blocking leaves a target out of reach"), S::Movement,
          tr("A blocking moves the stage targets for the cues that use it: the performers on the "
             "apron for one number. A light that reaches the target where it usually stands may "
             "not reach it there, and the look goes dark or aims short on the night."), blk01 },
    };
}
