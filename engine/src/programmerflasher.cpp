/*
  Q Light Controller Plus
  programmerflasher.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "programmerflasher.h"
#include "doc.h"
#include "fixture.h"
#include "mastertimer.h"
#include "qlcchannel.h"
#include "universe.h"

ProgrammerFlasher::ProgrammerFlasher(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
}

ProgrammerFlasher::~ProgrammerFlasher()
{
    if (m_registered && m_doc != nullptr && m_doc->masterTimer() != nullptr)
        m_doc->masterTimer()->unregisterDMXSource(this);
}

void ProgrammerFlasher::flashFixture(quint32 fixtureId, int durationMs)
{
    if (m_doc == nullptr)
        return;
    Fixture *fxi = m_doc->fixture(fixtureId);
    if (fxi == nullptr)
        return;

    // Resolve intensity channels. Prefer the fixture's master intensity
    // channel if it has one (for moving heads etc.); otherwise walk
    // each head looking for an Intensity-grouped channel.
    QList<int> addrs;
    quint32 mi = fxi->masterIntensityChannel();
    if (mi != QLCChannel::invalid())
    {
        addrs.append(int(fxi->address()) + int(mi));
    }
    else
    {
        const int headCount = fxi->heads();
        for (int h = 0; h < headCount; ++h)
        {
            quint32 ch = fxi->channelNumber(QLCChannel::Intensity,
                                            QLCChannel::MSB, h);
            if (ch != QLCChannel::invalid())
                addrs.append(int(fxi->address()) + int(ch));
        }
    }
    if (addrs.isEmpty())
        return;

    Entry e;
    e.fixtureId = fixtureId;
    e.durationMs = qMax(60, durationMs);
    e.dmxAddresses = addrs;
    e.universeId = int(fxi->universe());
    e.elapsed.start();

    {
        QMutexLocker locker(&m_mutex);
        m_entries.append(e);
    }

    if (!m_registered && m_doc->masterTimer() != nullptr)
    {
        m_doc->masterTimer()->registerDMXSource(this);
        m_registered = true;
    }
}

void ProgrammerFlasher::writeDMX(MasterTimer *timer, QList<Universe*> universes)
{
    Q_UNUSED(timer)
    QMutexLocker locker(&m_mutex);
    if (m_entries.isEmpty())
        return;

    QMutableListIterator<Entry> it(m_entries);
    while (it.hasNext())
    {
        Entry &e = it.next();
        if (e.universeId < 0 || e.universeId >= universes.size())
        {
            it.remove();
            continue;
        }
        Universe *uni = universes.at(e.universeId);
        if (uni == nullptr)
        {
            it.remove();
            continue;
        }

        // First tick: sample the current pre-GM value to decide
        // whether we should flash UP (currently dim) or DOWN
        // (currently bright). Sampling at-start instead of fresh each
        // tick keeps the flash visually consistent end-to-end.
        if (!e.sampled)
        {
            int avg = 0;
            int n = 0;
            for (int a : e.dmxAddresses)
            {
                avg += int(uni->preGMValue(a));
                ++n;
            }
            const int curr = n > 0 ? (avg / n) : 0;
            e.target = (curr < 128) ? uchar(255) : uchar(0);
            e.sampled = true;
        }

        if (e.elapsed.elapsed() >= e.durationMs)
        {
            it.remove();
            continue;
        }

        // Force-LTP write so the flash value beats running scenes for
        // the duration. After the entry is removed, the running source
        // takes back over on the next tick.
        for (int a : e.dmxAddresses)
            uni->write(a, e.target, /*forceLTP=*/true);
    }

    // Auto-unregister when there's nothing left to do — keeps the
    // master-timer DMX-source list clean during normal idle.
    if (m_entries.isEmpty() && m_registered && m_doc != nullptr
        && m_doc->masterTimer() != nullptr)
    {
        m_doc->masterTimer()->unregisterDMXSource(this);
        m_registered = false;
    }
}
