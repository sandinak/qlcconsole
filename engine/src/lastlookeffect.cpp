/*
  Q Light Controller Plus
  lastlookeffect.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QDebug>
#include <QSet>

#include "lastlookeffect.h"
#include "mastertimer.h"
#include "universe.h"
#include "doc.h"

LastLookEffect::LastLookEffect(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
}

LastLookEffect::~LastLookEffect()
{
    if (m_registered && m_doc && m_doc->masterTimer())
        m_doc->masterTimer()->unregisterDMXSource(this);
}

void LastLookEffect::hold(const QList<ChannelHold> &channels)
{
    {
        QMutexLocker locker(&m_mutex);
        m_entries = channels;
    }

    const bool hasWork = !channels.isEmpty();
    if (hasWork && !m_registered && m_doc && m_doc->masterTimer())
    {
        m_doc->masterTimer()->registerDMXSource(this);
        m_registered = true;
    }
    else if (!hasWork && m_registered && m_doc && m_doc->masterTimer())
    {
        m_doc->masterTimer()->unregisterDMXSource(this);
        m_registered = false;
    }
}

void LastLookEffect::addHold(const QList<ChannelHold> &channels)
{
    if (channels.isEmpty())
        return;

    // Fixtures present in the new batch replace their old entries (fresh look).
    QSet<quint32> incoming;
    for (const ChannelHold &c : channels)
        incoming.insert(c.fixtureId);

    {
        QMutexLocker locker(&m_mutex);
        QList<ChannelHold> merged;
        merged.reserve(m_entries.size() + channels.size());
        for (const ChannelHold &e : m_entries)
            if (incoming.contains(e.fixtureId) == false)
                merged.append(e);
        merged.append(channels);
        m_entries = merged;
    }

    if (!m_registered && m_doc && m_doc->masterTimer())
    {
        m_doc->masterTimer()->registerDMXSource(this);
        m_registered = true;
    }
}

void LastLookEffect::releaseFixtures(const QList<quint32> &fixtureIds)
{
    if (fixtureIds.isEmpty())
        return;

    const QSet<quint32> drop = QSet<quint32>(fixtureIds.begin(), fixtureIds.end());
    bool nowEmpty = false;
    {
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
        nowEmpty = m_entries.isEmpty();
    }

    if (nowEmpty && m_registered && m_doc && m_doc->masterTimer())
    {
        m_doc->masterTimer()->unregisterDMXSource(this);
        m_registered = false;
    }
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

void LastLookEffect::writeDMX(MasterTimer *timer, QList<Universe*> universes)
{
    Q_UNUSED(timer)
    QMutexLocker locker(&m_mutex);

    for (const ChannelHold &e : m_entries)
    {
        if (e.universeId < 0 || e.universeId >= universes.size())
            continue;
        Universe *uni = universes.at(e.universeId);
        if (uni == NULL)
            continue;
        // forceLTP so the held value survives zeroIntensityChannels() each tick.
        uni->write(e.address, e.value, /*forceLTP=*/true);
    }
}
