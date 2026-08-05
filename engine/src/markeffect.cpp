/*
  Q Light Controller Plus
  markeffect.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

#include "markeffect.h"
#include "doc.h"
#include "fixture.h"
#include "qlcfixturemode.h"
#include "mastertimer.h"
#include "universe.h"

MarkEffect::MarkEffect(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
    // Auto-handoff crosses threads: writeDMX (MasterTimer thread) emits, this
    // slot releases the mark on the GUI thread.
    connect(this, SIGNAL(fixtureRevealed(quint32)),
            this, SLOT(slotFixtureRevealed(quint32)), Qt::QueuedConnection);
}

MarkEffect::~MarkEffect()
{
    if (m_registered && m_doc && m_doc->masterTimer())
        m_doc->masterTimer()->unregisterDMXSource(this);
}

void MarkEffect::markFixture(quint32 fixtureId, const QHash<quint32, uchar> &values)
{
    if (values.isEmpty())
    {
        unmarkFixture(fixtureId);
        return;
    }
    m_marked.insert(fixtureId, values);
    rebuild();
}

void MarkEffect::unmarkFixture(quint32 fixtureId)
{
    if (m_marked.remove(fixtureId) > 0)
        rebuild();
}

void MarkEffect::unmarkAll()
{
    if (m_marked.isEmpty())
        return;
    m_marked.clear();
    rebuild();
}

bool MarkEffect::isMarked(quint32 fixtureId) const
{
    return m_marked.contains(fixtureId);
}

QList<quint32> MarkEffect::markedFixtures() const
{
    return m_marked.keys();
}

bool MarkEffect::isEmpty() const
{
    return m_marked.isEmpty();
}

void MarkEffect::slotFixtureRevealed(quint32 fixtureId)
{
    // The cue that lit the fixture now owns it — drop the mark.
    unmarkFixture(fixtureId);
}

void MarkEffect::rebuild()
{
    QList<FixEntry> entries;

    for (QHash<quint32, QHash<quint32, uchar>>::const_iterator it = m_marked.constBegin();
         it != m_marked.constEnd(); ++it)
    {
        Fixture *fxi = m_doc->fixture(it.key());
        if (fxi == NULL)
            continue;

        FixEntry entry;
        entry.fixtureId = it.key();
        entry.universeId = (int)fxi->universe();

        // Master-intensity absolute address (for auto-handoff), if any.
        const quint32 masterCh = fxi->masterIntensityChannel();
        entry.intensityAddr =
            (masterCh != QLCChannel::invalid() && masterCh < fxi->channels())
                ? (int)fxi->address() + (int)masterCh
                : -1;

        const QHash<quint32, uchar> &vals = it.value();
        for (QHash<quint32, uchar>::const_iterator vit = vals.constBegin();
             vit != vals.constEnd(); ++vit)
        {
            if (vit.key() >= fxi->channels())
                continue;
            int absAddr = (int)fxi->address() + (int)vit.key();
            if (absAddr < 0 || absAddr >= UNIVERSE_SIZE)
                continue;
            entry.writes.append(qMakePair(absAddr, vit.value()));
        }

        if (!entry.writes.isEmpty())
            entries.append(entry);
    }

    {
        QMutexLocker locker(&m_mutex);
        m_entries = entries;
        // Forget reveal edges for fixtures no longer (or freshly re-) marked.
        QSet<quint32> live;
        for (const FixEntry &e : entries) live.insert(e.fixtureId);
        m_revealed.intersect(live);
    }

    bool hasWork = !m_marked.isEmpty();
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

void MarkEffect::writeDMX(MasterTimer *timer, QList<Universe*> universes)
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

        // Auto-handoff: a real cue is lighting this fixture → stop forcing its
        // pre-position and release the mark (once). The cue was pre-set to these
        // same values, so the take-over is seamless.
        if (e.intensityAddr >= 0 && uni->preGMValue(e.intensityAddr) > HANDOFF_INTENSITY)
        {
            if (!m_revealed.contains(e.fixtureId))
            {
                m_revealed.insert(e.fixtureId);
                emit fixtureRevealed(e.fixtureId);   // queued → slotFixtureRevealed
            }
            continue;
        }

        for (const auto &w : e.writes)
            uni->write(w.first, w.second, /*forceLTP=*/true);
    }
}

bool MarkEffect::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != NULL);

    if (m_marked.isEmpty())
        return true;

    doc->writeStartElement(KXMLQLCMark);

    for (QHash<quint32, QHash<quint32, uchar>>::const_iterator it = m_marked.constBegin();
         it != m_marked.constEnd(); ++it)
    {
        doc->writeStartElement(KXMLQLCMarkFixture);
        doc->writeAttribute(KXMLQLCMarkFixtureID, QString::number(it.key()));

        const QHash<quint32, uchar> &vals = it.value();
        for (QHash<quint32, uchar>::const_iterator vit = vals.constBegin();
             vit != vals.constEnd(); ++vit)
        {
            doc->writeStartElement(KXMLQLCMarkChannel);
            doc->writeAttribute(KXMLQLCMarkChannelNum, QString::number(vit.key()));
            doc->writeCharacters(QString::number(vit.value()));
            doc->writeEndElement();
        }
        doc->writeEndElement();
    }

    doc->writeEndElement();
    return true;
}

bool MarkEffect::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCMark)
    {
        qWarning() << Q_FUNC_INFO << "Mark node not found";
        return false;
    }

    m_marked.clear();

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCMarkFixture)
        {
            bool ok = false;
            quint32 fid = root.attributes().value(KXMLQLCMarkFixtureID).toString().toUInt(&ok);
            QHash<quint32, uchar> vals;

            while (root.readNextStartElement())
            {
                if (root.name() == KXMLQLCMarkChannel)
                {
                    quint32 ch = root.attributes().value(KXMLQLCMarkChannelNum).toString().toUInt();
                    uchar v = (uchar)root.readElementText().toUInt();
                    vals.insert(ch, v);
                }
                else
                {
                    root.skipCurrentElement();
                }
            }

            if (ok && !vals.isEmpty())
                m_marked.insert(fid, vals);
        }
        else
        {
            root.skipCurrentElement();
        }
    }

    rebuild();
    return true;
}
