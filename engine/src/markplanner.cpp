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
#include "showtreecontroller.h"
#include "shutterautoopen.h"

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
        const bool had = !m_planned.isEmpty();
        clearPlanned();      // release our pre-sets; manual marks stay
        if (had)
            emit plannedChanged(0);
    }
    emit enabledChanged(enable);
}

void MarkPlanner::clearPlanned()
{
    for (quint32 fid : m_planned)
        if (m_mark != nullptr && m_mark->isMarked(fid))
        {
            m_mark->unmarkFixture(fid);
            putBack(fid);
        }
    m_planned.clear();
    m_shutterHeld.clear();
    m_before.clear();
    m_plannedFor.clear();
}

QHash<quint32, uchar> MarkPlanner::withoutShutters(const Fixture *fxi,
                                                   const QHash<quint32, uchar> &values)
{
    QHash<quint32, uchar> out = values;
    for (auto it = out.begin(); it != out.end(); )
    {
        const QLCChannel *ch = fxi->channel(it.key());
        if (ch != nullptr && ch->group() == QLCChannel::Shutter)
            it = out.erase(it);
        else
            ++it;
    }
    return out;
}

void MarkPlanner::putBack(quint32 fid)
{
    const QHash<quint32, uchar> before = m_before.take(fid);
    m_plannedFor.remove(fid);
    Fixture *fxi = m_doc != nullptr ? m_doc->fixture(fid) : nullptr;
    if (fxi == nullptr || !fxi->isPatched() || before.isEmpty())
        return;
    /* The mark's fader no longer has them (unmarkFixture() takes them out
       at once), so nothing writes them back: written here, between two
       frames, as a patch writes a channel's default. */
    const QList<Universe *> unis = m_doc->inputOutputMap()->universes();
    for (auto it = before.constBegin(); it != before.constEnd(); ++it)
    {
        const quint32 abs = fxi->universeAddress() + it.key();
        const int uni = int(abs >> 9);
        if (uni < unis.size() && unis.at(uni) != nullptr)
            unis.at(uni)->setChannelDefaultValue(ushort(abs & 511), it.value());
    }
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
            CueOutput::computeCues(m_doc, cue.functionId, &m_wledLooks);
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
    const QSet<quint32> before = m_planned;
    m_wledLooks.beginPass();
    evaluatePlan();
    m_wledLooks.endPass();
    if (m_planned != before)
        emit plannedChanged(int(m_planned.size()));
}

bool MarkPlanner::wantsMoveInBlack(const CueLookahead::UpcomingCue &cue) const
{
    const ShowTreeController *st = m_doc != nullptr ? m_doc->showTree() : nullptr;
    if (st == nullptr)
        return m_enabled;
    return st->moveInBlackForNext(cue.chaserId, cue.step, cue.functionId, m_enabled);
}

void MarkPlanner::evaluatePlan()
{
    updateDangleDetection();

    if (m_doc == nullptr || m_mark == nullptr)
        return;
    // Off for the show and no cue list says On: nothing to plan (cheap, as
    // before Q38).
    ShowTreeController *st = m_doc->showTree();
    if (!m_enabled && (st == nullptr || !st->anyListMoveInBlackOn()))
    {
        clearPlanned();
        return;
    }

    /* A mark auto-releases on reveal; prune those from our owned set
       first. Revealed by the cue it was for, that cue has its values now.
       Revealed by another -- a jump straight to a cue that lights it --
       what the pre-set held is not that cue's: put back. */
    const QList<quint32> stillMarkedList = m_mark->markedFixtures();
    const QSet<quint32> stillMarked(stillMarkedList.begin(), stillMarkedList.end());
    QSet<quint32> letGo = m_planned;
    letGo.subtract(stillMarked);
    for (quint32 fid : std::as_const(letGo))
    {
        Function *forCue = m_doc->function(m_plannedFor.value(fid, Function::invalidId()));
        if (forCue != nullptr && forCue->isRunning())
        {
            m_before.remove(fid);
            m_plannedFor.remove(fid);
        }
        else
            putBack(fid);
    }
    m_planned.intersect(stillMarked);
    m_shutterHeld.intersect(m_planned);

    const QList<CueLookahead::UpcomingCue> cues =
        CueLookahead::upcoming(m_doc, HORIZON_MS);
    if (cues.isEmpty())
    {
        clearPlanned();      // nothing driving cues → no pre-sets
        return;
    }
    const CueLookahead::UpcomingCue cue = cues.first();
    // Not into this cue (its list is Off, the cue says don't, or the show
    // is off and the list doesn't say On): release what we pre-set.
    if (!wantsMoveInBlack(cue))
    {
        clearPlanned();
        return;
    }
    // Too soon to move in black? Leave existing pre-sets be, add nothing new.
    if (cue.fireInMs < m_darkGapMs)
        return;

    /* A WLED's next effect, speed, palette and colours are the look's
       (WledLookCache), not the scene's: moved in black too, so the GO is
       its brightness alone (UX-73). */
    const QHash<quint32, CueOutput::FixtureCue> cueOut =
        CueOutput::computeCues(m_doc, cue.functionId, &m_wledLooks);

    QList<Universe*> unis = m_doc->inputOutputMap()->claimUniverses();

    QSet<quint32> desired;
    QHash<quint32, QHash<quint32, uchar>> holdValues;
    /* What each channel a pre-set is about to hold reads now, before it
       does (for putBack()); a channel it already holds keeps its first. */
    auto noteBefore = [&](quint32 fid, Fixture *fxi, const QHash<quint32, uchar> &values) {
        QHash<quint32, uchar> &was = m_before[fid];
        for (auto vit = values.constBegin(); vit != values.constEnd(); ++vit)
        {
            if (was.contains(vit.key()))
                continue;
            const quint32 abs = fxi->universeAddress() + vit.key();
            was.insert(vit.key(), liveValue(unis, int(abs >> 9), int(abs & 511)));
        }
        m_plannedFor.insert(fid, cue.functionId);
    };

    for (auto it = cueOut.constBegin(); it != cueOut.constEnd(); ++it)
    {
        const quint32 fid = it.key();
        const CueOutput::FixtureCue &fc = it.value();
        if (!fc.lit)
            continue;                        // cue doesn't light it → nothing to pre-empt

        Fixture *fxi = m_doc->fixture(fid);
        if (fxi == nullptr || !fxi->isPatched())
            continue;
        const quint32 master = fxi->masterIntensityChannel();
        const QVector<quint32> shutters = ShutterAutoOpen::closingShutters(fxi);
        if (master == QLCChannel::invalid() && shutters.isEmpty())
            continue;                        // nothing that can keep it dark

        // Already ours → keep it (and refresh values for the current next-cue).
        // Skip the "would move" test: the mark itself now drives the non-intensity
        // channels, so they'd read as matching and it would oscillate.
        if (m_planned.contains(fid))
        {
            const QHash<quint32, uchar> hold = m_shutterHeld.contains(fid)
                    ? withoutShutters(fxi, fc.nonIntensity) : fc.nonIntensity;
            desired.insert(fid);
            holdValues.insert(fid, hold);
            noteBefore(fid, fxi, hold);
            continue;
        }

        const int uni = int(fxi->universe());
        /* Dark right now? Its dimmer down, or its shutter closed. A dimmer
           keeps it dark while the shutter is pre-set with the rest (the GO
           brings the level); a closed shutter keeps it dark only while it
           stays closed, so then the shutter is left alone (shutterHeld) and
           the cue opens it. */
        const bool dimmerDark = master != QLCChannel::invalid()
                && liveValue(unis, uni, int(fxi->address()) + int(master)) <= DARK_LEVEL;
        bool shutterDark = false;
        if (!dimmerDark && !shutters.isEmpty())
        {
            shutterDark = true;
            for (quint32 c : shutters)
            {
                // Post-Grand-Master: an auto-opened shutter is open on stage.
                const quint32 abs = fxi->universeAddress() + c;
                const int u = int(abs >> 9);
                const uchar v = (u < unis.size() && unis.at(u) != nullptr)
                        ? unis.at(u)->postGMValue(int(abs & 511)) : 0;
                if (!ShutterAutoOpen::closesAt(fxi->channel(c), v))
                { shutterDark = false; break; }
            }
        }
        if (!dimmerDark && !shutterDark)
            continue;
        const QHash<quint32, uchar> hold = shutterDark
                ? withoutShutters(fxi, fc.nonIntensity) : fc.nonIntensity;
        // Would it actually move? (else pre-setting is pointless)
        bool moves = false;
        for (auto vit = hold.constBegin(); vit != hold.constEnd(); ++vit)
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
        holdValues.insert(fid, hold);
        noteBefore(fid, fxi, hold);
        if (shutterDark)
            m_shutterHeld.insert(fid);
        else
            m_shutterHeld.remove(fid);
    }

    m_doc->inputOutputMap()->releaseUniverses(false);

    // Drop the planner's marks that are no longer wanted (cue advanced / changed).
    QSet<quint32> toRelease = m_planned;
    toRelease.subtract(desired);
    for (quint32 fid : toRelease)
        if (m_mark->isMarked(fid))
        {
            m_mark->unmarkFixture(fid);
            putBack(fid);
        }

    // Apply / refresh the desired pre-sets.
    for (quint32 fid : desired)
        m_mark->markFixture(fid, holdValues.value(fid));

    m_planned = desired;
    m_shutterHeld.intersect(m_planned);
}
