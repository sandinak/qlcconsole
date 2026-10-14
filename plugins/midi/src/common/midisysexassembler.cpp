/*
  Q Light Controller Plus
  midisysexassembler.cpp

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

#include "midisysexassembler.h"

QList<QByteArray> MidiSysExAssembler::feed(const uchar *data, int length)
{
    QList<QByteArray> done;
    for (int i = 0; i < length; ++i)
    {
        const uchar b = data[i];
        if (b == 0xF0)
        {
            m_buffer = QByteArray(1, char(b));
            m_inProgress = true;
            continue;
        }
        if (!m_inProgress)
            continue;
        if (b >= 0xF8)
            continue;                   // real-time: allowed inside a message
        if (b == 0xF7)
        {
            m_buffer.append(char(b));
            done << m_buffer;
            reset();
            continue;
        }
        if (b & 0x80)
        {
            reset();                    // another status: it never finished
            continue;
        }
        m_buffer.append(char(b));
        if (m_buffer.size() > MaxLength)
            reset();
    }
    return done;
}

void MidiSysExAssembler::reset()
{
    m_buffer.clear();
    m_inProgress = false;
}
