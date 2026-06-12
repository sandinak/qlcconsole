/*
  Q Light Controller Plus
  highlighteffect.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "highlighteffect.h"
#include "doc.h"
#include "fixture.h"
#include "qlcchannel.h"
#include "qlcfixturemode.h"
#include "mastertimer.h"
#include "universe.h"

HighlightEffect::HighlightEffect(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
}

HighlightEffect::~HighlightEffect()
{
    if (m_registered && m_doc && m_doc->masterTimer())
        m_doc->masterTimer()->unregisterDMXSource(this);
}

void HighlightEffect::setFixtures(const QList<quint32>& fixtureIds)
{
    QList<FixEntry> entries = buildEntries(fixtureIds);

    {
        QMutexLocker locker(&m_mutex);
        m_entries = entries;
    }

    bool hasWork = !entries.isEmpty();
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

QList<HighlightEffect::FixEntry>
HighlightEffect::buildEntries(const QList<quint32>& fixtureIds) const
{
    QList<FixEntry> result;
    for (quint32 fid : fixtureIds)
    {
        Fixture *fxi = m_doc->fixture(fid);
        if (!fxi || !fxi->fixtureMode())
            continue;

        FixEntry entry;
        entry.universeId = (int)fxi->universe();

        quint32 nCh = fxi->channels();
        for (quint32 c = 0; c < nCh; c++)
        {
            QLCChannel *ch = fxi->fixtureMode()->channel(c);
            if (!ch)
                continue;

            int absAddr = (int)fxi->address() + (int)c;
            uchar value = 0;
            bool include = false;

            if (ch->group() == QLCChannel::Intensity)
            {
                value = 255;
                include = true;
            }
            else if (ch->group() == QLCChannel::Colour)
            {
                switch (ch->colour())
                {
                    case QLCChannel::Red:
                    case QLCChannel::Green:
                    case QLCChannel::Blue:
                    case QLCChannel::White:
                    case QLCChannel::Lime:
                    case QLCChannel::Indigo:
                        value = 255;
                        include = true;
                        break;
                    case QLCChannel::Cyan:
                    case QLCChannel::Magenta:
                    case QLCChannel::Yellow:
                    case QLCChannel::Amber:
                    case QLCChannel::UV:
                        value = 0;
                        include = true;
                        break;
                    default:
                        break;
                }
            }

            if (include)
                entry.writes.append(qMakePair(absAddr, value));
        }

        if (!entry.writes.isEmpty())
            result.append(entry);
    }
    return result;
}

void HighlightEffect::writeDMX(MasterTimer *timer, QList<Universe*> universes)
{
    Q_UNUSED(timer)
    QMutexLocker locker(&m_mutex);

    for (const FixEntry &e : m_entries)
    {
        if (e.universeId < 0 || e.universeId >= universes.size())
            continue;
        Universe *uni = universes.at(e.universeId);
        if (!uni)
            continue;
        for (const auto &w : e.writes)
            uni->write(w.first, w.second, /*forceLTP=*/true);
    }
}
