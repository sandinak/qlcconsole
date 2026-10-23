/*
  Q Light Controller Plus
  programmerflasher.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QDebug>

#include "programmerflasher.h"
#include "fadechannel.h"
#include "doc.h"
#include "fixture.h"
#include "mastertimer.h"
#include "qlcchannel.h"
#include "universe.h"

ProgrammerFlasher::ProgrammerFlasher(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
    , m_layer(doc, Universe::Flash, FadeChannel::Override, QStringLiteral("Flash"))
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
    if (fxi == nullptr || !fxi->isPatched())
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
    // Drop any address that runs past the end of the universe: a fixture
    // patched close enough to 511 that its channels overrun.
    for (int i = addrs.count() - 1; i >= 0; --i)
    {
        if (addrs.at(i) < 0 || addrs.at(i) >= UNIVERSE_SIZE)
        {
            qWarning() << "Fixture" << fixtureId << "has an intensity channel at DMX address"
                       << addrs.at(i) << "which is outside the universe — not flashing it.";
            addrs.removeAt(i);
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

    bool changed = false;
    QMutableListIterator<Entry> it(m_entries);
    while (it.hasNext())
    {
        Entry &e = it.next();
        if (e.universeId < 0 || e.universeId >= universes.size()
            || universes.at(e.universeId) == nullptr)
        {
            it.remove();
            changed = true;
            continue;
        }
        Universe *uni = universes.at(e.universeId);

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
            changed = true;
        }

        if (e.elapsed.elapsed() >= e.durationMs)
        {
            it.remove();
            changed = true;
        }
    }

    // The levels only change when a flash starts or ends: the fader holds
    // them in between, over the running scenes (Override).
    if (changed)
    {
        QList<OutputLayer::Level> levels;
        for (const Entry &e : m_entries)
            for (int a : e.dmxAddresses)
                levels.append({ Fixture::invalidId(),
                                quint32(e.universeId) * UNIVERSE_SIZE + quint32(a), e.target });
        m_layer.set(levels);
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
