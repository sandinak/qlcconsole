/*
  Q Light Controller Plus
  midimarkers.h

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

#ifndef MIDIMARKERS_H
#define MIDIMARKERS_H

#include <QByteArray>
#include <QString>
#include <QList>

#include "qlcengine_export.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * The markers of a Standard MIDI File (Q198): what Logic Pro writes when its
 * project is exported as MIDI (File > Export > Selection as MIDI File, with
 * the markers in it). Only what the markers need is read: marker meta
 * events (FF 06), the tempo map (FF 51) to turn ticks into seconds, and the
 * SMPTE offset (FF 54) where the session starts on the clock. Every other
 * event is stepped over. Formats 0 and 1, PPQ or SMPTE time division.
 */
class QLC_ENGINE_EXPORT MidiMarkers final
{
public:
    struct Marker
    {
        QString name;
        qint64 ms = 0;              //!< from the start of the file (no SMPTE offset)
    };

    struct Result
    {
        bool ok = false;
        QString error;              //!< why not, when !ok
        QList<Marker> markers;      //!< in time order
        /** The SMPTE offset meta event: where the file's start is on the
         *  timecode (ms); -1: the file has none. */
        qint64 smpteOffsetMs = -1;
        int tempoChanges = 0;       //!< tempo events read (0: 120 bpm throughout)
    };

    /** Read the markers of the MIDI file in @a data. */
    static Result read(const QByteArray &data);
    /** Read the MIDI file at @a path. */
    static Result readFile(const QString &path);
};

/** @} */

#endif
