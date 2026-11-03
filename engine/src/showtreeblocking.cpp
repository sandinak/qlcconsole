/*
  Q Light Controller Plus
  showtreeblocking.cpp

  A cue's blocking (docs/design/BLOCKING.md, board Q211): which blocking
  set the stage targets stand in from that cue on. It tracks: an unset cue
  takes the cue before's.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "showtreecontroller.h"

using namespace ShowTree;

bool ShowTreeController::setCuesBlocking(const QList<quint32> &cueIds, quint32 set)
{
    const QString label = set == invalidId()
            ? (cueIds.count() == 1 ? tr("Cue blocking tracks") : tr("%1 cues' blocking tracks").arg(cueIds.count()))
            : (cueIds.count() == 1 ? tr("Set cue blocking") : tr("Set blocking of %1 cues").arg(cueIds.count()));
    return edit(label, [&](QList<Production> &ps) {
        bool changed = false;
        for (Production &p : ps)
            for (CueList &l : p.lists)
            {
                if (l.looks)
                    continue;
                for (Cue &c : l.cues)
                    if (cueIds.contains(c.id) && c.blocking != set)
                    {
                        c.blocking = set;
                        changed = true;
                    }
            }
        return changed;
    });
}

quint32 ShowTreeController::cueOwnBlocking(quint32 cueId) const
{
    const Cue *c = cue(cueId);
    return c == nullptr ? invalidId() : c->blocking;
}

quint32 ShowTreeController::cueBlocking(quint32 cueId) const
{
    // Bounded: a sub-cue's cue is in another list; never a loop, but a
    // damaged file should not hang us.
    for (int hops = 0; hops < 8; ++hops)
    {
        const CueList *list = listOfCue(cueId);
        if (list == nullptr || list->looks)
            return invalidId();
        const int at = list->indexOf(cueId);
        for (int i = at; i >= 0; --i)
        {
            const Cue &c = list->cues.at(i);
            // A skipped cue passes nothing on (it still plays its own when jumped to).
            if (i != at && c.skip)
                continue;
            if (c.blocking != invalidId())
                return c.blocking;
        }
        if (!list->isSubCues())
            return 0;
        cueId = list->subOf;
    }
    return 0;
}

bool ShowTreeController::cueChangesBlocking(quint32 cueId) const
{
    const CueList *list = listOfCue(cueId);
    if (list == nullptr || list->looks)
        return false;
    const Cue *c = cue(cueId);
    if (c == nullptr || c->blocking == invalidId())
        return false;
    const int at = list->indexOf(cueId);
    for (int i = at - 1; i >= 0; --i)
        if (!list->cues.at(i).skip)
            return cueBlocking(list->cues.at(i).id) != c->blocking;
    // The first cue: a change when it is not Default (or its cue's, for a sub-cue).
    const quint32 before = list->isSubCues() ? cueBlocking(list->subOf) : 0;
    return before != c->blocking;
}
