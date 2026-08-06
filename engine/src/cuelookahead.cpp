/*
  Q Light Controller Plus
  cuelookahead.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "cuelookahead.h"

#include <QtGlobal>
#include <climits>

#include "doc.h"
#include "function.h"
#include "chaser.h"
#include "chaserstep.h"
#include "chaserrunner.h"
#include "show.h"
#include "track.h"
#include "showfunction.h"

// --- Chaser: next step + time left in the current step -----------------------
static QList<CueLookahead::UpcomingCue> chaserUpcoming(Chaser *chaser, int horizonMs)
{
    QList<CueLookahead::UpcomingCue> out;

    const int cur = chaser->currentStepIndex();
    if (cur < 0)
        return out;
    const int next = chaser->computeNextStep(cur);
    if (next < 0)
        return out;
    ChaserStep *ns = chaser->stepAt(next);
    if (ns == NULL)
        return out;

    const uint dur = chaser->currentStepDuration();
    int fireInMs;
    if (dur == 0)
    {
        // Holds until a manual GO — unknown, but there's ample dark time.
        fireInMs = INT_MAX;
    }
    else
    {
        const quint32 elapsed = chaser->currentRunningStep().m_elapsed;
        fireInMs = int(dur) - int(elapsed);
        if (fireInMs < 0)
            fireInMs = 0;
    }

    if (fireInMs <= horizonMs || fireInMs == INT_MAX)
        out.append({ ns->fid, fireInMs });
    return out;
}

// --- Show: nearest ShowFunction after the playhead ---------------------------
static QList<CueLookahead::UpcomingCue> showUpcoming(Show *show, int horizonMs)
{
    QList<CueLookahead::UpcomingCue> out;

    const quint32 pos = show->runningTime();
    quint32 bestStart = 0;
    quint32 bestFid = Function::invalidId();
    bool found = false;

    foreach (Track *tr, show->tracks())
    {
        if (tr == NULL || tr->isMute())
            continue;
        foreach (ShowFunction *sf, tr->showFunctions())
        {
            if (sf == NULL || sf->functionID() == Function::invalidId())
                continue;
            if (sf->startTime() <= pos)
                continue;                       // already fired / running
            if (!found || sf->startTime() < bestStart)
            {
                bestStart = sf->startTime();
                bestFid   = sf->functionID();
                found = true;
            }
        }
    }

    if (found)
    {
        const int fireInMs = int(bestStart - pos);
        if (fireInMs <= horizonMs)
            out.append({ bestFid, fireInMs });
    }
    return out;
}

QList<CueLookahead::UpcomingCue> CueLookahead::upcoming(Doc *doc, int horizonMs)
{
    if (doc == NULL)
        return {};

    // A running Show timeline is the master driver.
    foreach (Function *f, doc->functions())
    {
        if (f != NULL && f->type() == Function::ShowType && f->isRunning())
            return showUpcoming(qobject_cast<Show*>(f), horizonMs);
    }
    // Else a standalone running Chaser (one that a Show isn't already stepping).
    foreach (Function *f, doc->functions())
    {
        if (f != NULL && f->type() == Function::ChaserType && f->isRunning())
        {
            Chaser *ch = qobject_cast<Chaser*>(f);
            if (ch != NULL && !ch->isShowClocked())
                return chaserUpcoming(ch, horizonMs);
        }
    }
    return {};
}
