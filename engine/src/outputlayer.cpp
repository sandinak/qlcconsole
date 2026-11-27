/*
  Q Light Controller Plus
  outputlayer.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "outputlayer.h"
#include "inputoutputmap.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "qlcchannel.h"
#include "doc.h"

OutputLayer::OutputLayer(Doc *doc, Universe::FaderPriority priority, int flags, const QString &name)
    : m_doc(doc)
    , m_priority(priority)
    , m_flags(flags)
    , m_name(name)
{
}

OutputLayer::~OutputLayer()
{
    clear();
}

void OutputLayer::set(const QList<Level> &levels, const std::function<bool(const Level &)> &renewed)
{
    /* Built outside the lock: a FadeChannel looks its fixture up. A level
       that never fades: it is at its value from the first frame. Each goes
       on its universe's fader -- or, for a layer that is the show, on the
       stale one when it was set before this Operate session. */
    QHash<int, QList<FadeChannel>> next;
    QHash<int, quint32> slotStamps;
    Doc *doc = m_doc.data();
    const quint32 epoch = GenericFader::liveEpoch();
    /* What a level set now is stamped with: an operator action's session.
       With @p renewed, only the levels it picks are set by hand now; any
       other new one (a Park loaded with the show, a parked fixture patched
       later: R6-7) is stamped by no session at all -- held until set again
       by hand. */
    const quint32 stampNow = GenericFader::stampEpoch();
    QHash<quint64, Stamp> stamps;
    {
        QMutexLocker locker(&m_mutex);
        for (const Level &l : levels)
        {
            const quint64 key = (quint64(l.fixtureId) << 32) | l.channel;
            Stamp st;
            st.value = l.value;
            const bool byHand = !renewed || renewed(l);
            st.epoch = byHand ? stampNow : 0;
            const auto was = m_stamps.constFind(key);
            if (was != m_stamps.constEnd() && was.value().value == l.value && !(renewed && renewed(l)))
                st.epoch = was.value().epoch;      // untouched: as old as it was
            stamps.insert(key, st);
        }
    }
    if (doc != nullptr)
    {
        for (const Level &l : levels)
        {
            FadeChannel fc(doc, l.fixtureId, l.channel);
            if (fc.isInert() || fc.addressInUniverse() == QLCChannel::invalid())
                continue;
            fc.removeFlag(FadeChannel::CanFade);
            fc.addFlag(m_flags);
            fc.setStart(l.value);
            fc.setCurrent(l.value);
            fc.setTarget(l.value);
            fc.setReady(true);
            const quint64 key = (quint64(l.fixtureId) << 32) | l.channel;
            const quint32 st = stamps.value(key).epoch;
            const bool stale = !m_holdsShowOnly && st < epoch;
            const int slotKey = int(fc.universe()) | (stale ? kStaleSlot : 0);
            next[slotKey].append(fc);
            // A slot's fader is as old as its oldest level (S0: a level set
            // before a channel was made Show only does not count on it).
            auto ms = slotStamps.find(slotKey);
            if (ms == slotStamps.end())
                slotStamps.insert(slotKey, st);
            else if (st < ms.value())
                ms.value() = st;
        }
    }

    /* The universes as they are: never claimed, the MasterTimer holds that
       lock for its whole tick and a show stopping inside it lands here.
       Universes are only added or removed outside a tick. */
    InputOutputMap *ioMap = doc != nullptr ? doc->inputOutputMap() : nullptr;
    const QList<Universe *> universes = ioMap != nullptr ? ioMap->universes() : QList<Universe *>();

    QMutexLocker locker(&m_mutex);
    m_stamps = stamps;

    // Let go where the layer no longer writes, or the universe went away.
    for (auto it = m_slots.begin(); it != m_slots.end(); )
    {
        const int u = it.key() & ~kStaleSlot;
        Universe *live = u < universes.size() ? universes.at(u) : nullptr;
        if (live == it.value().universe && next.contains(it.key()))
        {
            ++it;
            continue;
        }
        // A universe that went away took its faders with it.
        if (live != nullptr && live == it.value().universe)
            live->dismissFader(it.value().fader);
        it = m_slots.erase(it);
    }

    for (auto it = next.constBegin(); it != next.constEnd(); ++it)
    {
        const int u = it.key() & ~kStaleSlot;
        const bool stale = it.key() & kStaleSlot;
        Universe *uni = u >= 0 && u < universes.size() ? universes.at(u) : nullptr;
        if (uni == nullptr)
            continue;

        Slot &slot = m_slots[it.key()];
        // Same address, another universe -- or ours, but it dropped the fader.
        if (slot.fader.isNull() || slot.universe != uni || uni->faders().contains(slot.fader) == false)
        {
            slot.universe = uni;
            // Its own, not the function's whose tick this may run in (a
            // show stopping captures its last look in postRun()).
            GenericFader::CreationScope noFunction(nullptr);
            slot.fader = m_onTop ? uni->requestTopFader(m_priority) : uni->requestFader(m_priority);
            slot.fader->setHoldsShowOnly(m_holdsShowOnly || stale);
            slot.fader->setName(m_name);
            slot.fader->setGate(m_gate);
            slot.fader->setDoc(doc);
        }
        // Park, Mark, a held last look: what was set in this session is the
        // show until the next one starts (set in Design, it holds Show-only
        // fixtures once Operate starts).
        if (!m_holdsShowOnly && !stale)
            slot.fader->setLiveEpoch(slotStamps.value(it.key()));
        slot.fader->replaceAll(it.value());
    }
}

void OutputLayer::setHoldsShowOnly(bool hold)
{
    QMutexLocker locker(&m_mutex);
    m_holdsShowOnly = hold;
    for (auto it = m_slots.begin(); it != m_slots.end(); ++it)
        if (!it.value().fader.isNull())
            it.value().fader->setHoldsShowOnly(hold || (it.key() & kStaleSlot));
}

void OutputLayer::clear()
{
    set(QList<Level>());
}

void OutputLayer::glideOut(uint ms)
{
    if (ms == 0)
    {
        clear();
        return;
    }
    QMutexLocker locker(&m_mutex);
    m_stamps.clear();
    // What finished travelling (its fader deleted) is no longer kept.
    for (auto it = m_gliding.begin(); it != m_gliding.end();)
    {
        QSharedPointer<GenericFader> f = it->toStrongRef();
        if (f.isNull() || f->deleteRequested())
            it = m_gliding.erase(it);
        else
            ++it;
    }
    for (auto it = m_slots.begin(); it != m_slots.end(); ++it)
    {
        Slot &slot = it.value();
        // Its universe deletes it once it is all the way out; a set() after
        // this takes a new one.
        if (!slot.fader.isNull() && slot.universe != nullptr && slot.universe->faders().contains(slot.fader))
        {
            slot.fader->glideOut(ms);
            m_gliding << slot.fader.toWeakRef();
        }
    }
    m_slots.clear();
}

void OutputLayer::dropGlides()
{
    QMutexLocker locker(&m_mutex);
    for (const QWeakPointer<GenericFader> &w : std::as_const(m_gliding))
        if (QSharedPointer<GenericFader> f = w.toStrongRef())
            f->requestDelete();
    m_gliding.clear();
}

int OutputLayer::glidingCount() const
{
    QMutexLocker locker(&m_mutex);
    int n = 0;
    for (const QWeakPointer<GenericFader> &w : m_gliding)
        if (QSharedPointer<GenericFader> f = w.toStrongRef())
            n += f->deleteRequested() ? 0 : 1;
    return n;
}

QHash<int, int> OutputLayer::channelCounts() const
{
    QMutexLocker locker(&m_mutex);
    QHash<int, int> counts;
    for (auto it = m_slots.constBegin(); it != m_slots.constEnd(); ++it)
        counts[it.key() & ~kStaleSlot] += it.value().fader.isNull() ? 0 : it.value().fader->channelsCount();
    return counts;
}

Doc *OutputLayer::doc() const
{
    return m_doc.data();
}
