/*
  Q Light Controller Plus
  simapc64.h

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

#ifndef SIMAPC64_H
#define SIMAPC64_H

#include <QByteArray>
#include <QStringList>

#include "simmididevice.h"
#include "iopluginstub.h"

/**
 * The SysEx an Akai APC64 takes on its "APC64 DAW" port, as his bench
 * confirmed it (2026-11-25): display ownership
 * (F0 47 00 53 1C 00 01 <1 take | 0 give back> F7) and a line of text
 * (F0 47 00 53 10 <len hi> <len lo> <line 0-2> <ASCII up to 8> 00 F7,
 * len = 1 + text + 1). What the app sends the board's line as one message
 * (QLCIOPlugin::sendShowControl()) lands here; anything malformed is
 * counted, not shown. Its pads (SimMidiDevice) come on MIDI channel 7 in
 * Session mode, on another in the others.
 */
class SimApc64
{
public:
    SimApc64(IOPluginStub *stub, SimMidiDevice *board)
        : m_stub(stub), m_board(board), m_lines(3)
    {
        std::function<bool(quint32, const QByteArray &)> previous = m_stub->m_onShowControl;
        m_stub->m_onShowControl = [this, previous](quint32 line, const QByteArray &msg) {
            if (int(line) == m_board->line())
                return take(msg);
            return previous ? previous(line, msg) : false;
        };
    }
    ~SimApc64() { m_stub->m_onShowControl = nullptr; }

    /** The app has its display. */
    bool owned() const { return m_owned; }
    /** Line @p i of its display ("" blank). */
    QString text(int i) const { return m_lines.value(i); }
    int ownershipMessages() const { return m_ownership; }
    int textMessages() const { return m_texts; }
    int malformed() const { return m_bad; }
    /** As it powers up: its own display, nothing written. */
    void powerCycle()
    {
        m_owned = false;
        m_lines = QStringList() << QString() << QString() << QString();
    }

private:
    bool take(const QByteArray &m)
    {
        if (m_board->line() < 0)
            return false;
        const QByteArray head = QByteArray::fromHex("F0470053");
        if (!m.startsWith(head) || uchar(m.at(m.size() - 1)) != 0xF7)
        {
            ++m_bad;
            return true;
        }
        if (m.size() == 9 && uchar(m.at(4)) == 0x1C && m.at(5) == 0 && m.at(6) == 1)
        {
            m_owned = m.at(7) == 1;
            ++m_ownership;
            return true;
        }
        if (m.size() >= 10 && uchar(m.at(4)) == 0x10)
        {
            const int len = (int(m.at(5)) << 7) | int(m.at(6));
            const int line = m.at(7);
            const QByteArray text = m.mid(8, m.size() - 10);
            // The length counts the line byte, the text and the closing 00.
            if (len != 1 + text.size() + 1 || m.at(m.size() - 2) != 0 || line < 0 || line > 2
                || text.size() > 8)
            {
                ++m_bad;
                return true;
            }
            m_lines[line] = QString::fromLatin1(text);
            ++m_texts;
            return true;
        }
        ++m_bad;
        return true;
    }

    IOPluginStub *m_stub;
    SimMidiDevice *m_board;
    QStringList m_lines;
    bool m_owned = false;
    int m_ownership = 0;
    int m_texts = 0;
    int m_bad = 0;
};

#endif
