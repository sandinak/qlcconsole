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
    /* Holds until a manual GO -- unknown, but there's ample dark time. A
       Show Tree cue that waits for GO holds for Function::infiniteSpeed()
       (UINT_MAX - 1): as an int that is -2, which used to read as "fires
       now" and so never moved anything in black (0.4.5). Any sentinel at
       or above it (defaultSpeed() too) is no timed wait. */
    if (dur == 0 || dur >= Function::infiniteSpeed())
    {
        fireInMs = INT_MAX;
    }
    else
    {
        // In 64 bits: a long finite hold (over INT_MAX ms) must not wrap.
        const quint32 elapsed = chaser->currentRunningStep().m_elapsed;
        const qint64 left = qint64(dur) - qint64(elapsed);
        fireInMs = int(qBound(qint64(0), left, qint64(INT_MAX) - 1));
    }

    if (fireInMs <= horizonMs || fireInMs == INT_MAX)
        out.append({ ns->fid, fireInMs, chaser->id(), next });
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
