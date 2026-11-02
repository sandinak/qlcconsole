/*
  Q Light Controller Plus
  markerimport.h

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

#ifndef MARKERIMPORT_H
#define MARKERIMPORT_H

#include <QString>
#include <QList>

class Show;

/**
 * Section markers from a DAW's marker list: the text export of Logic's
 * Marker List, an Audition / Reaper / Pro Tools style marker CSV, or any
 * tab- or comma-separated list with a time and a name on each line.
 *
 *   Verse 1,00:00:12.500          name then time, or
 *   00:01:03:12<TAB>Chorus        time then name
 *
 * A time is hh:mm:ss.mmm, hh:mm:ss:ff (frames at @c fps; a trailing
 * ".subframe" is ignored), mm:ss or plain seconds. Lines with no time (a
 * header, a bar position such as "17 1 1 1") are skipped and counted.
 */
class MarkerImport
{
public:
    struct Entry
    {
        quint32 start;      //!< ms from the first marker's origin
        QString label;
    };

    struct Result
    {
        QList<Entry> markers;   //!< sorted by start
        int skipped = 0;        //!< non-empty lines that held no time
        quint32 offsetMs = 0;   //!< removed from every time (see below)
    };

    /** Parse @p text. DAW projects usually start at 01:00:00:00: when every
     *  marker is at or past one hour that hour is taken off, so the first
     *  song section lands near the start of the show's timeline. */
    static Result parse(const QString &text, double fps = 25.0);

    /** One time cell as ms, or -1 when it isn't a time */
    static qint64 parseTime(const QString &cell, double fps);

    /** Put @p result into @p show as section markers: each runs to the next
     *  one (the last for 15 s), replacing a marker already at that start.
     *  Returns how many were added. Colours cycle the same palette as
     *  "Add marker here". */
    static int apply(Show *show, const Result &result);
};

#endif
