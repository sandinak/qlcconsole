/*
  Q Light Controller Plus
  lastlookeffect.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QDebug>
#include <QSet>

#include "lastlookeffect.h"
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

void LastLookEffect::apply()
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
    m_layer.set(levels);
}

void LastLookEffect::hold(const QList<ChannelHold> &channels)
{
    QMutexLocker locker(&m_mutex);
    m_entries = channels;
    apply();
}

void LastLookEffect::addHold(const QList<ChannelHold> &channels)
{
    if (channels.isEmpty())
        return;

    // Fixtures present in the new batch replace their old entries (fresh look).
    QSet<quint32> incoming;
    for (const ChannelHold &c : channels)
        incoming.insert(c.fixtureId);

    QMutexLocker locker(&m_mutex);
    QList<ChannelHold> merged;
    merged.reserve(m_entries.size() + channels.size());
    for (const ChannelHold &e : m_entries)
        if (incoming.contains(e.fixtureId) == false)
            merged.append(e);
    merged.append(channels);
    m_entries = merged;
    apply();
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
