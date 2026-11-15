/*
  Q Light Controller Plus
  lastlookeffect.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QDebug>
#include <QSet>

#include "lastlookeffect.h"
#include "universe.h"
#include "inputoutputmap.h"
#include "outputrulekeeper.h"
#include "fixture.h"
#include "doc.h"

LastLookEffect::LastLookEffect(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
    , m_layer(doc, Universe::Hold, 0, QStringLiteral("Last look"))
{
}

LastLookEffect::~LastLookEffect()
{
}

void LastLookEffect::apply(const QSet<quint32> &renewAddresses, bool renewAll)
{
    // By address: the fixture a held channel is on is looked up again, so
    // the fader treats it as that fixture's channel.
    QList<OutputLayer::Level> levels;
    levels.reserve(m_entries.size());
    for (const ChannelHold &e : m_entries)
    {
        if (e.universeId < 0 || e.address < 0 || e.address >= UNIVERSE_SIZE)
            continue;
        levels.append({ Fixture::invalidId(),
                        quint32(e.universeId) * UNIVERSE_SIZE + quint32(e.address), e.value });
    }
    /* Show only: a look captured now is set now (the show, in Operate);
       what was held from before -- a capture in Design -- keeps its
       session, so a GO that lets some fixtures go does not wake the rest. */
    m_layer.set(levels, [&renewAddresses, renewAll](const OutputLayer::Level &l) {
        return renewAll || renewAddresses.contains(l.channel);
    });
}

QList<LastLookEffect::ChannelHold> LastLookEffect::withoutShowOnly(const QList<ChannelHold> &channels,
                                                                  bool always) const
{
    /* Show only, the emergency stop: a look captured while it holds (a
       show stopped by it, its postRun on the next tick, reads the values
       from before the stop) never holds a Show-only channel. By the
       universe's own bits: no fixture lookup, safe on the timer thread. */
    const OutputRuleKeeper *rules = m_doc->outputRuleKeeper();
    if (!always && (rules == nullptr || !rules->emergencyStopped()))
        return channels;
    const QList<Universe *> ua = m_doc->inputOutputMap()->universes();
    QList<ChannelHold> out;
    out.reserve(channels.size());
    for (const ChannelHold &c : channels)
    {
        Universe *u = c.universeId >= 0 && c.universeId < ua.size() ? ua.at(c.universeId) : nullptr;
        if (u != nullptr && c.address >= 0 && c.address < UNIVERSE_SIZE
            && (u->channelCapabilities(ushort(c.address)) & Universe::ShowOnlyOutput))
            continue;
        out.append(c);
    }
    return out;
}

void LastLookEffect::dropShowOnly()
{
    QMutexLocker locker(&m_mutex);
    const QList<ChannelHold> kept = withoutShowOnly(m_entries, true);
    if (kept.size() == m_entries.size())
        return;
    m_entries = kept;
    apply();
}

void LastLookEffect::hold(const QList<ChannelHold> &channels)
{
    QMutexLocker locker(&m_mutex);
    m_entries = withoutShowOnly(channels, false);
    apply(QSet<quint32>(), true);
}

void LastLookEffect::addHold(const QList<ChannelHold> &channels)
{
    if (channels.isEmpty())
        return;

    // Fixtures present in the new batch replace their old entries (fresh look).
    QSet<quint32> incoming;
    for (const ChannelHold &c : channels)
        incoming.insert(c.fixtureId);
    const QList<ChannelHold> fresh0 = withoutShowOnly(channels, false);

    QMutexLocker locker(&m_mutex);
    QList<ChannelHold> merged;
    merged.reserve(m_entries.size() + channels.size());
    for (const ChannelHold &e : m_entries)
        if (incoming.contains(e.fixtureId) == false)
            merged.append(e);
    merged.append(fresh0);
    m_entries = merged;
    QSet<quint32> fresh;
    for (const ChannelHold &c : fresh0)
        if (c.universeId >= 0 && c.address >= 0 && c.address < UNIVERSE_SIZE)
            fresh.insert(quint32(c.universeId) * UNIVERSE_SIZE + quint32(c.address));
    apply(fresh, false);
}

void LastLookEffect::releaseFixtures(const QList<quint32> &fixtureIds)
{
    if (fixtureIds.isEmpty())
        return;

    const QSet<quint32> drop = QSet<quint32>(fixtureIds.begin(), fixtureIds.end());
    QMutexLocker locker(&m_mutex);
    if (m_entries.isEmpty())
        return;
    QList<ChannelHold> kept;
    kept.reserve(m_entries.size());
    for (const ChannelHold &e : m_entries)
        if (drop.contains(e.fixtureId) == false)
            kept.append(e);
    if (kept.size() == m_entries.size())
        return; // nothing dropped — the started cue touched no held fixture
    m_entries = kept;
    apply();
}

void LastLookEffect::clear()
{
    hold(QList<ChannelHold>());
}

bool LastLookEffect::isActive() const
{
    QMutexLocker locker(&m_mutex);
    return !m_entries.isEmpty();
}
