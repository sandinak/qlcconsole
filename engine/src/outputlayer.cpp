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

void OutputLayer::set(const QList<Level> &levels)
{
    /* Built outside the lock: a FadeChannel looks its fixture up. A level
       that never fades: it is at its value from the first frame. */
    QHash<int, QList<FadeChannel>> next;
    if (m_doc != nullptr)
    {
        for (const Level &l : levels)
        {
            FadeChannel fc(m_doc, l.fixtureId, l.channel);
            if (fc.isInert() || fc.addressInUniverse() == QLCChannel::invalid())
                continue;
            fc.removeFlag(FadeChannel::CanFade);
            fc.addFlag(m_flags);
            fc.setStart(l.value);
            fc.setCurrent(l.value);
            fc.setTarget(l.value);
            fc.setReady(true);
            next[int(fc.universe())].append(fc);
        }
    }

    /* The universes as they are: never claimed, the MasterTimer holds that
       lock for its whole tick and a show stopping inside it lands here.
       Universes are only added or removed outside a tick. */
    InputOutputMap *ioMap = m_doc != nullptr ? m_doc->inputOutputMap() : nullptr;
    const QList<Universe *> universes = ioMap != nullptr ? ioMap->universes() : QList<Universe *>();

    QMutexLocker locker(&m_mutex);

    // Let go where the layer no longer writes, or the universe went away.
    for (auto it = m_slots.begin(); it != m_slots.end(); )
    {
        Universe *live = it.key() < universes.size() ? universes.at(it.key()) : nullptr;
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
        Universe *uni = it.key() >= 0 && it.key() < universes.size() ? universes.at(it.key()) : nullptr;
        if (uni == nullptr)
            continue;

        Slot &slot = m_slots[it.key()];
        // Same address, another universe -- or ours, but it dropped the fader.
        if (slot.fader.isNull() || slot.universe != uni || uni->faders().contains(slot.fader) == false)
        {
            slot.universe = uni;
            slot.fader = uni->requestFader(m_priority);
            slot.fader->setName(m_name);
            slot.fader->setDoc(m_doc);
        }
        slot.fader->replaceAll(it.value());
    }
}

void OutputLayer::clear()
{
    set(QList<Level>());
}

QHash<int, int> OutputLayer::channelCounts() const
{
    QMutexLocker locker(&m_mutex);
    QHash<int, int> counts;
    for (auto it = m_slots.constBegin(); it != m_slots.constEnd(); ++it)
        counts.insert(it.key(), it.value().fader.isNull() ? 0 : it.value().fader->channelsCount());
    return counts;
}
