/*
  Q Light Controller Plus
  markplanner.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "markplanner.h"

#include <algorithm>

#include <QHash>

#include "doc.h"
#include "fixture.h"
#include "qlcchannel.h"
#include "inputoutputmap.h"
#include "universe.h"
#include "markeffect.h"
#include "cueoutput.h"
#include "cuelookahead.h"

MarkPlanner::MarkPlanner(Doc *doc, MarkEffect *mark, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_mark(mark)
{
    m_timer.setInterval(TICK_MS);
    connect(&m_timer, &QTimer::timeout, this, &MarkPlanner::evaluate);
    // Runs regardless of isEnabled(): dangle detection (below) has to see
    // manual marks too, not just the auto-MIB pre-sets this class creates.
    m_timer.start();
}

void MarkPlanner::setEnabled(bool enable)
{
    if (enable == m_enabled)
        return;
    m_enabled = enable;
    if (enable)
    {
        evaluate();          // act at once, don't wait a tick
    }
    else
    {
        clearPlanned();      // release our pre-sets; manual marks stay
    }
    emit enabledChanged(enable);
}

void MarkPlanner::clearPlanned()
{
    for (quint32 fid : m_planned)
        if (m_mark != nullptr && m_mark->isMarked(fid))
            m_mark->unmarkFixture(fid);
    m_planned.clear();
}

uchar MarkPlanner::liveValue(const QList<Universe*> &unis, int uni, int absAddr) const
{
    if (uni < 0 || uni >= unis.size() || unis.at(uni) == nullptr)
        return 0;
    if (absAddr < 0 || absAddr >= UNIVERSE_SIZE)
        return 0;
    return unis.at(uni)->preGMValue(absAddr);
}

QList<quint32> MarkPlanner::dangleFixtures() const
{
    if (m_doc == nullptr || m_mark == nullptr)
        return QList<quint32>();

    const QList<quint32> marked = m_mark->markedFixtures();
    if (marked.isEmpty())
        return QList<quint32>();

    // Union of every fixture lit by any cue within the lookahead horizon —
    // not just the nearest one, so a mark aimed two cues out still counts.
    const QList<CueLookahead::UpcomingCue> cues =
        CueLookahead::upcoming(m_doc, HORIZON_MS);

    QSet<quint32> litSoon;
    for (const CueLookahead::UpcomingCue &cue : cues)
    {
        const QHash<quint32, CueOutput::FixtureCue> cueOut =
            CueOutput::computeCues(m_doc, cue.functionId);
        for (auto it = cueOut.constBegin(); it != cueOut.constEnd(); ++it)
            if (it.value().lit)
                litSoon.insert(it.key());
    }

    QList<quint32> dangling;
    for (quint32 fid : marked)
        if (!litSoon.contains(fid))
            dangling << fid;
    std::sort(dangling.begin(), dangling.end());
    return dangling;
}

void MarkPlanner::updateDangleDetection()
{
    const QList<quint32> current = dangleFixtures();
    if (current == m_lastDangling)
        return;
    m_lastDangling = current;
    emit dangleFixturesChanged(current);
}

void MarkPlanner::evaluate()
{
    updateDangleDetection();

    if (!m_enabled || m_doc == nullptr || m_mark == nullptr)
        return;

    // A mark auto-releases on reveal; prune those from our owned set first.
    const QList<quint32> stillMarked = m_mark->markedFixtures();
    m_planned.intersect(QSet<quint32>(stillMarked.begin(), stillMarked.end()));

    const QList<CueLookahead::UpcomingCue> cues =
        CueLookahead::upcoming(m_doc, HORIZON_MS);
    if (cues.isEmpty())
    {
        clearPlanned();      // nothing driving cues → no pre-sets
        return;
    }
    const CueLookahead::UpcomingCue cue = cues.first();
    // Too soon to move in black? Leave existing pre-sets be, add nothing new.
    if (cue.fireInMs < m_darkGapMs)
        return;

    const QHash<quint32, CueOutput::FixtureCue> cueOut =
        CueOutput::computeCues(m_doc, cue.functionId);

    QList<Universe*> unis = m_doc->inputOutputMap()->claimUniverses();

    QSet<quint32> desired;
    QHash<quint32, QHash<quint32, uchar>> holdValues;

    for (auto it = cueOut.constBegin(); it != cueOut.constEnd(); ++it)
    {
        const quint32 fid = it.key();
        const CueOutput::FixtureCue &fc = it.value();
        if (!fc.lit)
            continue;                        // cue doesn't light it → nothing to pre-empt

        Fixture *fxi = m_doc->fixture(fid);
        if (fxi == nullptr)
            continue;
        const quint32 master = fxi->masterIntensityChannel();
        if (master == QLCChannel::invalid())
            continue;                        // need a dimmer to be dark-but-preset

        // Already ours → keep it (and refresh values for the current next-cue).
        // Skip the "would move" test: the mark itself now drives the non-intensity
        // channels, so they'd read as matching and it would oscillate.
        if (m_planned.contains(fid))
        {
            desired.insert(fid);
            holdValues.insert(fid, fc.nonIntensity);
            continue;
        }

        const int uni = int(fxi->universe());
        // Dark right now?
        if (liveValue(unis, uni, int(fxi->address()) + int(master)) > DARK_LEVEL)
            continue;
        // Would it actually move? (else pre-setting is pointless)
        bool moves = false;
        for (auto vit = fc.nonIntensity.constBegin(); vit != fc.nonIntensity.constEnd(); ++vit)
        {
            if (liveValue(unis, uni, int(fxi->address()) + int(vit.key())) != vit.value())
            { moves = true; break; }
        }
        if (!moves)
            continue;
        // A manual mark already owns it — don't fight the operator.
        if (m_mark->isMarked(fid))
            continue;

        desired.insert(fid);
        holdValues.insert(fid, fc.nonIntensity);
    }

    m_doc->inputOutputMap()->releaseUniverses(false);

    // Drop the planner's marks that are no longer wanted (cue advanced / changed).
    QSet<quint32> toRelease = m_planned;
    toRelease.subtract(desired);
    for (quint32 fid : toRelease)
        if (m_mark->isMarked(fid))
            m_mark->unmarkFixture(fid);

    // Apply / refresh the desired pre-sets.
    for (quint32 fid : desired)
        m_mark->markFixture(fid, holdValues.value(fid));

    m_planned = desired;
}
