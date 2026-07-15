/*
  Q Light Controller Plus
  parkeffect.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

#include "parkeffect.h"
#include "doc.h"
#include "fixture.h"
#include "mastertimer.h"
#include "universe.h"

ParkEffect::ParkEffect(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
}

ParkEffect::~ParkEffect()
{
    if (m_registered && m_doc && m_doc->masterTimer())
        m_doc->masterTimer()->unregisterDMXSource(this);
}

void ParkEffect::parkFixture(quint32 fixtureId, const QHash<quint32, uchar> &values)
{
    if (values.isEmpty())
    {
        unparkFixture(fixtureId);
        return;
    }
    m_parked.insert(fixtureId, values);
    rebuild();
}

void ParkEffect::unparkFixture(quint32 fixtureId)
{
    if (m_parked.remove(fixtureId) > 0)
        rebuild();
}

void ParkEffect::unparkAll()
{
    if (m_parked.isEmpty())
        return;
    m_parked.clear();
    rebuild();
}

bool ParkEffect::isParked(quint32 fixtureId) const
{
    return m_parked.contains(fixtureId);
}

QList<quint32> ParkEffect::parkedFixtures() const
{
    return m_parked.keys();
}

bool ParkEffect::isEmpty() const
{
    return m_parked.isEmpty();
}

void ParkEffect::rebuild()
{
    QList<FixEntry> entries;

    for (QHash<quint32, QHash<quint32, uchar>>::const_iterator it = m_parked.constBegin();
         it != m_parked.constEnd(); ++it)
    {
        Fixture *fxi = m_doc->fixture(it.key());
        if (fxi == NULL)
            continue;

        FixEntry entry;
        entry.universeId = (int)fxi->universe();

        const QHash<quint32, uchar> &vals = it.value();
        for (QHash<quint32, uchar>::const_iterator vit = vals.constBegin();
             vit != vals.constEnd(); ++vit)
        {
            if (vit.key() >= fxi->channels())
                continue;
            int absAddr = (int)fxi->address() + (int)vit.key();
            // Universe::write() only Q_ASSERTs the bound (compiled out in
            // release), so guard against an over-addressed fixture here.
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
    }

    bool hasWork = !m_parked.isEmpty();
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

void ParkEffect::writeDMX(MasterTimer *timer, QList<Universe*> universes)
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

bool ParkEffect::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != NULL);

    if (m_parked.isEmpty())
        return true;

    doc->writeStartElement(KXMLQLCPark);

    for (QHash<quint32, QHash<quint32, uchar>>::const_iterator it = m_parked.constBegin();
         it != m_parked.constEnd(); ++it)
    {
        doc->writeStartElement(KXMLQLCParkFixture);
        doc->writeAttribute(KXMLQLCParkFixtureID, QString::number(it.key()));

        const QHash<quint32, uchar> &vals = it.value();
        for (QHash<quint32, uchar>::const_iterator vit = vals.constBegin();
             vit != vals.constEnd(); ++vit)
        {
            doc->writeStartElement(KXMLQLCParkChannel);
            doc->writeAttribute(KXMLQLCParkChannelNum, QString::number(vit.key()));
            doc->writeCharacters(QString::number(vit.value()));
            doc->writeEndElement();
        }
        doc->writeEndElement();
    }

    doc->writeEndElement();
    return true;
}

bool ParkEffect::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCPark)
    {
        qWarning() << Q_FUNC_INFO << "Park node not found";
        return false;
    }

    m_parked.clear();

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCParkFixture)
        {
            bool ok = false;
            quint32 fid = root.attributes().value(KXMLQLCParkFixtureID).toString().toUInt(&ok);
            QHash<quint32, uchar> vals;

            while (root.readNextStartElement())
            {
                if (root.name() == KXMLQLCParkChannel)
                {
                    quint32 ch = root.attributes().value(KXMLQLCParkChannelNum).toString().toUInt();
                    uchar v = (uchar)root.readElementText().toUInt();
                    vals.insert(ch, v);
                }
                else
                {
                    root.skipCurrentElement();
                }
            }

            if (ok && !vals.isEmpty())
                m_parked.insert(fid, vals);
        }
        else
        {
            root.skipCurrentElement();
        }
    }

    rebuild();
    return true;
}
