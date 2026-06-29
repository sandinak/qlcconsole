/*
  Q Light Controller Plus
  venue.h

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

#ifndef VENUE_H
#define VENUE_H

#include <QStringList>
#include <QString>

class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLVenue        QStringLiteral("Venue")
#define KXMLVenueVersion QStringLiteral("Version")

/**
 * A "venue" is the physically-fixed infrastructure of a location — power
 * distribution today, and (designed to extend into) trusses, platforms, stage
 * geometry and house fixtures. It is portable across shows via a sectioned,
 * versioned `.venue` file: each section just reuses its subsystem's own
 * save/load, so adding a section is additive (no format change).
 *
 * Show-specific data (scenes, programming, and fixture→circuit assignments)
 * stays in the `.qxw` — a venue carries the circuit *infrastructure*, not which
 * of this production's fixtures hang on it.
 */
namespace Venue
{
    /** Bit flags selecting which venue-fixed subsystems to export. */
    enum Section
    {
        Power = 0x01
        // Trusses = 0x02, Platforms = 0x04, MonitorGrid = 0x08, ... (future)
    };
    static const int AllSections = Power;

    /** Write the selected sections of the document's venue-fixed state to a
     *  `.venue` file. Returns false on I/O error. */
    bool exportToFile(Doc *doc, const QString &path, int sections);

    /** Load a `.venue` file and apply its sections to the document (replacing
     *  the corresponding workspace state). Returns false on I/O/parse error. */
    bool importFromFile(Doc *doc, const QString &path);

    /** Human-readable names of the sections present in a `.venue` file (for an
     *  import preview); empty if the file is unreadable or not a venue. */
    QStringList sectionsInFile(const QString &path);
}

/** @} */

#endif
