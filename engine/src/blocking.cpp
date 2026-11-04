/*
  Q Light Controller Plus
  blocking.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "blocking.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "scene.h"
#include "doc.h"

namespace
{
thread_local bool t_scopeActive = false;
thread_local quint32 t_scopeSet = 0;
thread_local bool t_defaultSet = false;
thread_local quint32 t_default = 0;
}

void BlockingScope::setThreadDefault(quint32 set)
{
    t_defaultSet = true;
    t_default = set;
}

void BlockingScope::clearThreadDefault()
{
    t_defaultSet = false;
    t_default = 0;
}

BlockingScope::BlockingScope(quint32 set)
    : m_prevActive(t_scopeActive)
    , m_prev(t_scopeSet)
{
    t_scopeActive = true;
    t_scopeSet = set;
}

BlockingScope::~BlockingScope()
{
    t_scopeActive = m_prevActive;
    t_scopeSet = m_prev;
}

bool BlockingScope::active()
{
    return t_scopeActive || t_defaultSet;
}

quint32 BlockingScope::current()
{
    return t_scopeActive ? t_scopeSet : t_default;
}

bool Blocking::targetDiffers(Doc *doc, quint32 targetId, quint32 a, quint32 b)
{
    if (a == b || doc == nullptr)
        return false;
    const StageTarget *t = doc->monitorProperties()->stageTarget(targetId);
    return t != nullptr && (t->isBlockedIn(a) || t->isBlockedIn(b));
}

int Blocking::reaimRunning(Doc *doc, quint32 from, quint32 to, uint fadeMs)
{
    if (doc == nullptr || from == to)
        return 0;
    int n = 0;
    for (Function *fn : doc->functions())
    {
        Scene *s = qobject_cast<Scene *>(fn);
        if (s == nullptr || !s->isRunning())
            continue;
        for (quint32 pid : s->runtimePalettes())
        {
            const QLCPalette *pal = doc->palette(pid);
            if (pal != nullptr && pal->type() == QLCPalette::Aim
                && targetDiffers(doc, pal->stageTargetId(), from, to))
            {
                s->requestReaim(fadeMs);
                ++n;
                break;
            }
        }
    }
    return n;
}
