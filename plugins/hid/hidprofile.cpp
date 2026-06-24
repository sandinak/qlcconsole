/*
  Q Light Controller Plus
  hidprofile.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "hidprofile.h"

#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QFile>
#include <QDebug>

bool HIDProfile::matchesDevice(unsigned short vid, unsigned short pid) const
{
    if (m_vendorId != 0 && m_vendorId != vid)
        return false;
    if (m_productIds.isEmpty())
        return true; // VID-only match
    return m_productIds.contains(pid);
}

int HIDProfile::axisForSlot(const QString &slot) const
{
    QHashIterator<int, QString> it(m_axisSlots);
    while (it.hasNext())
    {
        it.next();
        if (it.value() == slot)
            return it.key();
    }
    return -1;
}

HIDProfile *HIDProfile::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "[HIDProfile] cannot open" << path;
        return nullptr;
    }

    QXmlStreamReader xml(&file);
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("HIDProfile"))
    {
        qWarning() << "[HIDProfile] not a HIDProfile file:" << path;
        return nullptr;
    }

    HIDProfile *p = new HIDProfile;

    while (xml.readNextStartElement())
    {
        if (xml.name() == QLatin1String("Name"))
        {
            p->m_name = xml.readElementText();
        }
        else if (xml.name() == QLatin1String("Vendor"))
        {
            bool ok = false;
            p->m_vendorId = (unsigned short)xml.attributes()
                                .value(QLatin1String("id")).toUInt(&ok, 16);
            xml.skipCurrentElement();
        }
        else if (xml.name() == QLatin1String("Products"))
        {
            while (xml.readNextStartElement())
            {
                if (xml.name() == QLatin1String("Product"))
                {
                    bool ok = false;
                    unsigned short pid = (unsigned short)xml.attributes()
                                             .value(QLatin1String("id")).toUInt(&ok, 16);
                    if (ok)
                        p->m_productIds.append(pid);
                }
                xml.skipCurrentElement();
            }
        }
        else if (xml.name() == QLatin1String("Settings"))
        {
            bool ok = false;
            float s = xml.attributes().value(QLatin1String("sensitivity")).toFloat(&ok);
            if (ok) p->m_sensitivity = qBound(0.1f, s, 3.0f);
            float d = xml.attributes().value(QLatin1String("deadzone")).toFloat(&ok);
            if (ok) p->m_deadzone = qBound(0.0f, d, 0.5f);
            xml.skipCurrentElement();
        }
        else if (xml.name() == QLatin1String("Axes"))
        {
            while (xml.readNextStartElement())
            {
                if (xml.name() == QLatin1String("Axis"))
                {
                    bool ok = false;
                    int idx = xml.attributes()
                                  .value(QLatin1String("index")).toInt(&ok);
                    const QString slot = xml.attributes()
                                             .value(QLatin1String("slot")).toString();
                    const QString label = xml.readElementText();
                    if (ok && !label.isEmpty())
                    {
                        p->m_axisNames.insert(idx, label);
                        if (!slot.isEmpty())
                            p->m_axisSlots.insert(idx, slot);
                    }
                }
                else
                {
                    xml.skipCurrentElement();
                }
            }
        }
        else if (xml.name() == QLatin1String("Buttons"))
        {
            while (xml.readNextStartElement())
            {
                if (xml.name() == QLatin1String("Button"))
                {
                    bool ok = false;
                    int idx = xml.attributes()
                                  .value(QLatin1String("index")).toInt(&ok);
                    const QString action = xml.attributes()
                                               .value(QLatin1String("action")).toString();
                    const QString label = xml.readElementText();
                    if (ok && !label.isEmpty())
                    {
                        p->m_buttonNames.insert(idx, label);
                        if (!action.isEmpty())
                            p->m_buttonActions.insert(idx, action);
                    }
                }
                else
                {
                    xml.skipCurrentElement();
                }
            }
        }
        else
        {
            xml.skipCurrentElement();
        }
    }

    if (p->m_name.isEmpty())
    {
        qWarning() << "[HIDProfile] profile has no name:" << path;
        delete p;
        return nullptr;
    }

    p->m_path = path;
    qDebug() << "[HIDProfile] loaded:" << p->m_name
             << "VID" << Qt::hex << p->m_vendorId
             << "PIDs" << p->m_productIds.size();
    return p;
}

bool HIDProfile::save() const
{
    if (m_path.isEmpty())
        return false;

    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qWarning() << "[HIDProfile] cannot write" << m_path;
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeDTD(QLatin1String("<!DOCTYPE HIDProfile>"));
    xml.writeStartElement(QLatin1String("HIDProfile"));

    xml.writeTextElement(QLatin1String("Name"), m_name);

    xml.writeStartElement(QLatin1String("Vendor"));
    xml.writeAttribute(QLatin1String("id"),
                       QString("0x%1").arg(m_vendorId, 4, 16, QLatin1Char('0')).toUpper());
    xml.writeEndElement();

    xml.writeStartElement(QLatin1String("Settings"));
    xml.writeAttribute(QLatin1String("sensitivity"), QString::number(double(m_sensitivity), 'f', 2));
    xml.writeAttribute(QLatin1String("deadzone"),    QString::number(double(m_deadzone),    'f', 3));
    xml.writeEndElement();

    if (!m_productIds.isEmpty())
    {
        xml.writeStartElement(QLatin1String("Products"));
        for (unsigned short pid : m_productIds)
        {
            xml.writeStartElement(QLatin1String("Product"));
            xml.writeAttribute(QLatin1String("id"),
                               QString("0x%1").arg(pid, 4, 16, QLatin1Char('0')).toUpper());
            xml.writeEndElement();
        }
        xml.writeEndElement();
    }

    xml.writeStartElement(QLatin1String("Axes"));
    const QList<int> axisKeys = m_axisNames.keys();
    QList<int> sortedAxisKeys = axisKeys;
    std::sort(sortedAxisKeys.begin(), sortedAxisKeys.end());
    for (int idx : sortedAxisKeys)
    {
        xml.writeStartElement(QLatin1String("Axis"));
        xml.writeAttribute(QLatin1String("index"), QString::number(idx));
        const QString slot = m_axisSlots.value(idx);
        if (!slot.isEmpty())
            xml.writeAttribute(QLatin1String("slot"), slot);
        xml.writeCharacters(m_axisNames.value(idx));
        xml.writeEndElement();
    }
    xml.writeEndElement();

    xml.writeStartElement(QLatin1String("Buttons"));
    const QList<int> btnKeys = m_buttonNames.keys();
    QList<int> sortedBtnKeys = btnKeys;
    std::sort(sortedBtnKeys.begin(), sortedBtnKeys.end());
    for (int idx : sortedBtnKeys)
    {
        xml.writeStartElement(QLatin1String("Button"));
        xml.writeAttribute(QLatin1String("index"), QString::number(idx));
        const QString action = m_buttonActions.value(idx);
        if (!action.isEmpty())
            xml.writeAttribute(QLatin1String("action"), action);
        xml.writeCharacters(m_buttonNames.value(idx));
        xml.writeEndElement();
    }
    xml.writeEndElement();

    xml.writeEndElement(); // HIDProfile
    xml.writeEndDocument();

    qDebug() << "[HIDProfile] saved:" << m_path;
    return true;
}
