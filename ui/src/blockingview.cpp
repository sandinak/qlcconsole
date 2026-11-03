/*
  Q Light Controller Plus
  blockingview.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "blockingview.h"
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "blocking.h"
#include "doc.h"

BlockingView *BlockingView::of(Doc *doc)
{
    if (doc == nullptr)
        return nullptr;
    if (BlockingView *v = doc->findChild<BlockingView *>(QString(), Qt::FindDirectChildrenOnly))
        return v;
    return new BlockingView(doc);
}

BlockingView::BlockingView(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
    MonitorProperties *mp = doc->monitorProperties();
    connect(mp, &MonitorProperties::blockingSetsChanged, this, &BlockingView::refresh);
    connect(mp, &MonitorProperties::liveBlockingChanged, this, &BlockingView::refresh, Qt::QueuedConnection);
    connect(doc, &Doc::modeChanged, this, &BlockingView::refresh);
    if (ShowTreeController *st = doc->showTree())
    {
        connect(st, &ShowTreeController::editedCueChanged, this, &BlockingView::refresh);
        connect(st, &ShowTreeController::changed, this, &BlockingView::refresh);
    }
    refresh();
}

BlockingView::~BlockingView()
{
    // The views it set the main thread's blocking for are going.
    BlockingScope::clearThreadDefault();
}

void BlockingView::setPick(quint32 pick)
{
    if (pick != followCue() && pick != 0 && !m_doc->monitorProperties()->hasBlockingSet(pick))
        pick = followCue();
    if (pick == m_pick)
        return;
    m_pick = pick;
    refresh();
}

void BlockingView::refresh()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    if (m_pick != followCue() && m_pick != 0 && !mp->hasBlockingSet(m_pick))
        m_pick = followCue();       // the set was removed
    quint32 b = m_pick;
    if (b == followCue())
    {
        b = mp->liveBlocking();
        ShowTreeController *st = m_doc->showTree();
        if (m_doc->mode() == Doc::Design && st != nullptr && st->editedCue() != ShowTree::invalidId())
        {
            const quint32 c = st->cueBlocking(st->editedCue());
            if (c != ShowTree::invalidId())
                b = c;
        }
    }
    if (b != 0 && !mp->hasBlockingSet(b))
        b = 0;
    BlockingScope::setThreadDefault(b);
    const QString name = mp->blockingName(b);
    if (b == m_blocking && name == m_name)
        return;
    const bool moved = b != m_blocking;
    m_blocking = b;
    m_name = name;
    emit changed();
    // Everything that draws or checks targets (the rig view, the look
    // panel's reach) redraws them where they now stand.
    if (moved)
        mp->notifyStageTargetsChanged();
}

QString BlockingView::blockingName() const
{
    return m_name;
}

QString BlockingView::bannerText() const
{
    return editsSet() ? tr("Moving targets in: %1").arg(m_name) : QString();
}
