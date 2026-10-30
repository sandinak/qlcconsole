/*
  Q Light Controller Plus
  teachingsettings.cpp

  Copyright (C) Branson Matheson

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "teachingsettings.h"
#include "inputoutputmap.h"
#include "doc.h"

#define KXMLTeachingEnabled     QStringLiteral("Enabled")
#define KXMLTeachingAllowOutput QStringLiteral("AllowOutput")
#define KXMLTeachingLesson      QStringLiteral("Lesson")
#define KXMLTeachingLink        QStringLiteral("Link")
#define KXMLTeachingHandedOut   QStringLiteral("HandedOut")

TeachingSettings::TeachingSettings(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
}

void TeachingSettings::applyOutputHold()
{
    if (m_doc != nullptr && m_doc->inputOutputMap() != nullptr)
        m_doc->inputOutputMap()->setTeachingHold(holdsOutput());
}

void TeachingSettings::changed()
{
    applyOutputHold();
    if (m_doc != nullptr)
        m_doc->setModified();
    emit teachingChanged();
}

void TeachingSettings::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    changed();
}

void TeachingSettings::setAllowOutput(bool on)
{
    if (on == m_allowOutput)
        return;
    m_allowOutput = on;
    changed();
}

void TeachingSettings::setLesson(const QString &lesson, const QString &link)
{
    if (lesson == m_lesson && link == m_link)
        return;
    m_lesson = lesson;
    m_link = link;
    changed();
}

void TeachingSettings::setHandedOut(const QString &when)
{
    if (when == m_handedOut)
        return;
    m_handedOut = when;
    changed();
}

void TeachingSettings::reset()
{
    if (!m_enabled && !m_allowOutput && m_lesson.isEmpty() && m_link.isEmpty()
        && m_handedOut.isEmpty() && m_unknown.isEmpty())
        return;
    m_enabled = false;
    m_allowOutput = false;
    m_lesson.clear();
    m_link.clear();
    m_handedOut.clear();
    m_unknown.clear();
    applyOutputHold();
    emit teachingChanged();
}

void TeachingSettings::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != nullptr);
    const bool enabled = m_saveOverride != nullptr ? m_saveOverride->enabled : m_enabled;
    const bool allow = m_saveOverride != nullptr ? m_saveOverride->allowOutput : m_allowOutput;
    const QString lesson = m_saveOverride != nullptr ? m_saveOverride->lesson : m_lesson;
    const QString link = m_saveOverride != nullptr ? m_saveOverride->link : m_link;
    const QString handedOut = m_saveOverride != nullptr ? m_saveOverride->handedOut : m_handedOut;

    // Off: nothing written, so a non-Teaching show's file is what it was.
    if (!enabled)
        return;

    doc->writeStartElement(KXMLQLCTeaching);
    doc->writeAttribute(KXMLTeachingEnabled, QStringLiteral("1"));
    if (allow)
        doc->writeAttribute(KXMLTeachingAllowOutput, QStringLiteral("1"));
    if (!lesson.isEmpty())
        doc->writeAttribute(KXMLTeachingLesson, lesson);
    if (!link.isEmpty())
        doc->writeAttribute(KXMLTeachingLink, link);
    if (!handedOut.isEmpty())
        doc->writeAttribute(KXMLTeachingHandedOut, handedOut);
    for (const QXmlStreamAttribute &a : m_unknown)
        doc->writeAttribute(a.qualifiedName().toString(), a.value().toString());
    doc->writeEndElement();
}

void TeachingSettings::loadXML(QXmlStreamReader &root)
{
    const QXmlStreamAttributes attrs = root.attributes();
    m_enabled = attrs.value(KXMLTeachingEnabled) == QLatin1String("1");
    m_allowOutput = attrs.value(KXMLTeachingAllowOutput) == QLatin1String("1");
    m_lesson = attrs.value(KXMLTeachingLesson).toString();
    m_link = attrs.value(KXMLTeachingLink).toString();
    m_handedOut = attrs.value(KXMLTeachingHandedOut).toString();
    m_unknown.clear();
    for (const QXmlStreamAttribute &a : attrs)
    {
        const QString n = a.qualifiedName().toString();
        if (n != KXMLTeachingEnabled && n != KXMLTeachingAllowOutput && n != KXMLTeachingLesson
            && n != KXMLTeachingLink && n != KXMLTeachingHandedOut)
            m_unknown.append(a);
    }
    root.skipCurrentElement();

    // Loading doesn't modify the show.
    applyOutputHold();
    emit teachingChanged();
}
