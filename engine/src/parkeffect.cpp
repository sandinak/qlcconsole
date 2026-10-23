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
#include "fadechannel.h"

ParkEffect::ParkEffect(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
    , m_layer(doc, Universe::Park, FadeChannel::Override | FadeChannel::PastGM,
              QStringLiteral("Park"))
{
}

ParkEffect::~ParkEffect()
{
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
    QList<OutputLayer::Level> levels;

    for (QHash<quint32, QHash<quint32, uchar>>::const_iterator it = m_parked.constBegin();
         it != m_parked.constEnd(); ++it)
    {
        Fixture *fxi = m_doc->fixture(it.key());
        if (fxi == NULL || !fxi->isPatched())
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
            // Guard against an over-addressed fixture (patched so its
            // channels run past 511): nothing to write there.
            if (absAddr < 0 || absAddr >= UNIVERSE_SIZE)
                continue;
            entry.writes.append(qMakePair(absAddr, vit.value()));
            levels.append({ it.key(), vit.key(), vit.value() });
        }

        if (!entry.writes.isEmpty())
            entries.append(entry);
    }

    m_entries = entries;
    m_layer.set(levels);
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
