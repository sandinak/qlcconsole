/*
  Q Light Controller Plus
  cuelookahead.h

  The shared "what fires next" abstraction for the move-in-black planner. Whatever
  is currently driving cues — a running Show timeline or a standalone Chaser —
  reduces to the same question: which Function fires next, and in how many ms?
  The planner uses that to peek ahead and pre-position dark movers.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef CUELOOKAHEAD_H
#define CUELOOKAHEAD_H

#include <QList>

class Doc;

class CueLookahead
{
public:
    struct UpcomingCue
    {
        quint32 functionId; //!< the Function (Scene/Collection) that fires next
        int     fireInMs;   //!< ms until it fires; INT_MAX = holds for a manual GO
    };

    /** Upcoming cues within @p horizonMs for whatever is driving the show right
     *  now: a running Show timeline takes precedence, else a standalone (not
     *  show-clocked) running Chaser. Empty if nothing is driving cues. Ordered
     *  nearest-first. Currently one cue ahead; the list leaves room for more. */
    static QList<UpcomingCue> upcoming(Doc *doc, int horizonMs = 60000);
};

#endif // CUELOOKAHEAD_H
