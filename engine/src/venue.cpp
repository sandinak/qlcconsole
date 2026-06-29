/*
  Q Light Controller Plus
  venue.cpp

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

#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QFile>
#include <QDebug>

#include "venue.h"
#include "powerdistribution.h"
#include "doc.h"

bool Venue::exportToFile(Doc *doc, const QString &path, int sections)
{
    if (doc == NULL)
        return false;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text) == false)
    {
        qWarning() << Q_FUNC_INFO << "cannot write venue file" << path << file.errorString();
        return false;
    }

    QXmlStreamWriter w(&file);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement(KXMLVenue);
    w.writeAttribute(KXMLVenueVersion, QStringLiteral("1"));

    // Power: circuit infrastructure WITHOUT show-specific fixture assignments.
    if (sections & Power)
        doc->powerDistribution()->saveXML(&w, /*includeFixtures=*/false);

    w.writeEndElement();    // Venue
    w.writeEndDocument();
    file.close();
    return true;
}

bool Venue::importFromFile(Doc *doc, const QString &path)
{
    if (doc == NULL)
        return false;

    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text) == false)
    {
        qWarning() << Q_FUNC_INFO << "cannot read venue file" << path << file.errorString();
        return false;
    }

    QXmlStreamReader r(&file);
    bool applied = false;

    if (r.readNextStartElement() && r.name() == KXMLVenue)
    {
        while (r.readNextStartElement())
        {
            if (r.name() == KXMLQLCPowerDistribution)
            {
                doc->powerDistribution()->loadXML(r, doc);
                applied = true;
            }
            else
            {
                r.skipCurrentElement();
            }
        }
    }
    else
    {
        qWarning() << Q_FUNC_INFO << path << "is not a venue file";
    }

    file.close();
    if (applied)
        doc->setModified();
    return applied;
}

QStringList Venue::sectionsInFile(const QString &path)
{
    QStringList names;
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text) == false)
        return names;

    QXmlStreamReader r(&file);
    if (r.readNextStartElement() && r.name() == KXMLVenue)
    {
        while (r.readNextStartElement())
        {
            if (r.name() == KXMLQLCPowerDistribution)
                names << QStringLiteral("Power distribution");
            r.skipCurrentElement();
        }
    }
    file.close();
    return names;
}
