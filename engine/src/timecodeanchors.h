/*
  Q Light Controller Plus
  timecodeanchors.h

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

#ifndef TIMECODEANCHORS_H
#define TIMECODEANCHORS_H

#include <QStringList>
#include <QString>
#include <QList>

#include "qlcengine_export.h"
#include "midimarkers.h"

class ShowTreeController;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Logic markers onto song sections (Q198): an exported MIDI file's markers
 * (MidiMarkers) matched to a cue list's sections by name, previewed, then
 * applied as one undo step (ShowTreeController::setSectionStarts()).
 */
class QLC_ENGINE_EXPORT MarkerPlan final
{
public:
    struct Move
    {
        int section = -1;           //!< index in ShowTreeController::sections()
        QString title;              //!< the section's title
        QString marker;             //!< the marker's name
        qint32 from = -1;           //!< its start now (-1: none)
        qint32 to = -1;             //!< the marker's time on the timecode
        int follow = 0;             //!< cues that move with it / now count from it
    };

    QList<Move> moves;              //!< matched, in section order
    QStringList unmatched;          //!< markers no section is called ("Outro at 00:03:10")
    QStringList missing;            //!< sections no marker is called
    /** Added to every marker: the file's SMPTE offset, else (decided, can
     *  change) one hour when the show's starts sit past 01:00:00:00 and the
     *  markers don't -- Logic's usual session start. */
    qint64 offsetMs = 0;

    /** Whether applying changes anything. */
    bool changes() const;
    /** The preview, a line each: "Chorus 2 moved +6.0 s, 14 cues follow". */
    QStringList lines() const;

    /** Match @a markers to list @a listId's sections. */
    static MarkerPlan make(const ShowTreeController *st, quint32 listId, const MidiMarkers::Result &markers);
    /** Apply @a plan to list @a listId: one undo step. */
    static bool apply(ShowTreeController *st, quint32 listId, const MarkerPlan &plan);

    /** A name as markers and sections are matched: no case, punctuation or
     *  extra spaces ("CHORUS-2 " == "chorus 2"). */
    static QString matchKey(const QString &name);
    /** "+6.0 s", "-1.5 s". */
    static QString formatDelta(qint64 ms);
    /** "0:42.5", "1:02:03.0": a time for a message. */
    static QString formatTime(qint64 ms);
};

/**
 * Re-learn (Q198): play the new export with timecode coming in and tap at
 * each section start -- each tap sets the next section's start to the
 * timecode now (one undo step each) and moves on to the one after.
 */
class QLC_ENGINE_EXPORT SectionLearn final
{
public:
    /** Learning list @a listId from section @a fromSection on; a tap before
     *  @a after (the time tapped last, -1: none) is refused. */
    SectionLearn(ShowTreeController *st, quint32 listId, int fromSection = 0, qint32 after = -1);

    /** The section the next tap sets (-1: done, every section learned). */
    int section() const { return m_section; }
    /** Its title ("2 — Chorus 2"); empty when done. */
    QString sectionTitle() const;
    bool done() const { return m_section < 0; }
    /** Set the current section's start to @a ms and move on. False (and
     *  nothing set) when done, or @a ms is before the section learned last. */
    bool tap(qint32 ms);
    /** How many taps set a start. */
    int learned() const { return m_learned; }

private:
    ShowTreeController *m_st;
    quint32 m_list;
    int m_section;
    int m_learned = 0;
    qint32 m_last = -1;
};

/** @} */

#endif
