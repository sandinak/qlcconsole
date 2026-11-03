/*
  Q Light Controller Plus
  blocking.h

  Blocking (docs/design/BLOCKING.md, board Q211): named sets of where the
  stage targets stand, chosen per Show Tree cue. The sets and each target's
  override live on MonitorProperties and StageTarget; this holds what reads
  them in a context -- the thread's scope -- and what a change of the live
  blocking does to the scenes on stage.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef BLOCKING_H
#define BLOCKING_H

#include <QtGlobal>
#include "qlcengine_export.h"

class Doc;

/** While one is open, every target reader on THIS thread
 *  (MonitorProperties::targetPosition() and all that goes through it) reads
 *  blocking @p set instead of the live one: Blind, the Studio's picker, a
 *  plan view of the cue in edit, Show Review. Scopes nest; the MasterTimer
 *  thread never opens one, so live output only follows the cue that went. */
class QLC_ENGINE_EXPORT BlockingScope
{
public:
    explicit BlockingScope(quint32 set);
    ~BlockingScope();
    BlockingScope(const BlockingScope &) = delete;
    BlockingScope &operator=(const BlockingScope &) = delete;

    static bool active();
    static quint32 current();

    /** The blocking THIS thread reads when no scope is open: the UI sets it
     *  on the main thread to the blocking its views show (the cue in edit,
     *  or a set picked in the Lighting Studio). Unset (every other thread,
     *  and the main thread of a test): the live blocking. */
    static void setThreadDefault(quint32 set);
    static void clearThreadDefault();

private:
    bool m_prevActive;
    quint32 m_prev;
};

namespace Blocking
{
    /** The live blocking went from @p from to @p to: every running scene
     *  with an Aim look at a target that stands somewhere else in one of
     *  them re-aims, over @p fadeMs (the cue's fade in). Main thread. */
    QLC_ENGINE_EXPORT int reaimRunning(Doc *doc, quint32 from, quint32 to, uint fadeMs);

    /** Whether target @p targetId stands somewhere else in @p a than in @p b. */
    QLC_ENGINE_EXPORT bool targetDiffers(Doc *doc, quint32 targetId, quint32 a, quint32 b);
}

#endif // BLOCKING_H
