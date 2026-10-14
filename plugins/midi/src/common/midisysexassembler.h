/*
  Q Light Controller Plus
  midisysexassembler.h

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

#ifndef MIDISYSEXASSEMBLER_H
#define MIDISYSEXASSEMBLER_H

#include <QByteArray>
#include <QList>

/**
 * Puts System Exclusive messages back together from the bytes a MIDI driver
 * hands over -- one message can come in several packets (CoreMIDI) or
 * chunks (ALSA). Real-time bytes inside a message are let through, as MIDI
 * allows; any other status byte ends a message that never finished.
 */
class MidiSysExAssembler final
{
public:
    /** Feeds @a length bytes; returns every message they complete, F0 to F7. */
    QList<QByteArray> feed(const uchar *data, int length);

    /** A message has started and not finished. */
    bool inProgress() const { return m_inProgress; }

    void reset();

    /** A message longer than this is dropped (no show-control message is). */
    static const int MaxLength = 4096;

private:
    QByteArray m_buffer;
    bool m_inProgress = false;
};

#endif
