/*
  Q Light Controller Plus
  simopendeck.h

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

#ifndef SIMOPENDECK_H
#define SIMOPENDECK_H

#include <QByteArray>
#include <QString>
#include <QList>
#include <QMap>

#include "simmididevice.h"
#include "iopluginstub.h"

/**
 * The configuration database of an OpenDeck board (the PMJ Black 1), as it
 * answers SysEx: what a SimMidiDevice's output line is sent as one message
 * (QLCIOPlugin::sendShowControl(), as MidiPlugin writes SysEx) is answered
 * on its input line (QLCIOPlugin::sysExReceived(), as the MIDI plugin hands
 * SysEx over), on the same thread, before the send returns.
 *
 * The protocol is the one qlcplus-midi-profiler verified on the real board
 * (qlcprofiler/opendeck.py): a handshake first (anything before it is a
 * handshake error), sections read in parts of 32 values, single values
 * written. Its LEDs start in the factory order the board shipped in: LED i
 * on note i, MIDI channel 1 -- no lamp under the button over it.
 *
 * @a under is which LED sits under which button (note on MIDI channel 9),
 * the physical fact the config cannot say: litBy() says which LED a note
 * lights, so a test sees the lamp land under its key or not.
 */
class SimOpenDeck
{
public:
    enum { Leds = 128, Buttons = 64 };

    SimOpenDeck(IOPluginStub *stub, SimMidiDevice *board)
        : m_stub(stub), m_board(board)
    {
        factory();
        std::function<bool(quint32, const QByteArray &)> previous = m_stub->m_onShowControl;
        m_stub->m_onShowControl = [this, previous](quint32 line, const QByteArray &msg) {
            if (int(line) == m_board->line())
                return answer(msg);
            return previous ? previous(line, msg) : false;
        };
    }

    ~SimOpenDeck() { m_stub->m_onShowControl = nullptr; }

    /** As it left the factory. */
    void factory()
    {
        m_tables.clear();
        QList<int> zeros, ids, ones, full, nine, buttonIds;
        for (int i = 0; i < Leds; ++i)
        {
            zeros << 0;
            ids << i;
            ones << 1;
            full << 127;
        }
        m_tables[key(4, 0)] = zeros;     // level
        m_tables[key(4, 3)] = ids;       // activation id: note i
        m_tables[key(4, 4)] = zeros;     // rgb
        m_tables[key(4, 5)] = zeros;     // control type: MidiInNoteSingleVal
        m_tables[key(4, 6)] = full;      // activation value
        m_tables[key(4, 7)] = ones;      // channel 1
        for (int i = 0; i < Buttons; ++i)
        {
            nine << 9;
            buttonIds << i;
        }
        m_tables[key(1, 0)] = zeros.mid(0, Buttons);
        m_tables[key(1, 1)] = zeros.mid(0, Buttons);
        m_tables[key(1, 2)] = buttonIds;
        m_tables[key(1, 3)] = full.mid(0, Buttons);
        m_tables[key(1, 4)] = nine;
        m_tables[key(0, 0)] = { 0, 1, 0, 0 };
        m_connected = false;
    }

    /** Which LED sits under the button that sends @p note on channel 9. */
    void setUnder(int note, int led) { m_under[note] = led; }
    int ledUnder(int note) const { return m_under.value(note, -1); }

    /** The LED that note @p note on MIDI channel @p channel lights (-1: none). */
    int litBy(int channel, int note) const
    {
        const QList<int> ids = m_tables.value(key(4, 3));
        const QList<int> chs = m_tables.value(key(4, 7));
        const QList<int> types = m_tables.value(key(4, 5));
        for (int i = 0; i < ids.size(); ++i)
            if (ids.at(i) == note && chs.value(i) == channel
                && (types.value(i) == 0 || types.value(i) == 6))
                return i;
        return -1;
    }

    int value(int block, int section, int index) const
    { return m_tables.value(key(block, section)).value(index, -1); }
    void setValue(int block, int section, int index, int v) { m_tables[key(block, section)][index] = v; }
    QMap<QString, QList<int>> tables() const { return m_tables; }

    /** Values written to its memory so far. */
    int writes() const { return m_writes; }
    int requests() const { return m_requests; }
    /** A board that hears nothing (unplugged, or not an OpenDeck): no answers. */
    void setDeaf(bool deaf) { m_deaf = deaf; }

private:
    static QString key(int block, int section) { return QStringLiteral("%1/%2").arg(block).arg(section); }

    bool answer(const QByteArray &msg)
    {
        if (m_board->line() < 0)
            return false;           // unplugged: the port is gone
        if (m_deaf || msg.size() < 5 || uchar(msg.at(0)) != 0xF0
            || msg.at(1) != 0x00 || msg.at(2) != 0x53 || msg.at(3) != 0x43)
            return true;
        ++m_requests;
        QByteArray reply = msg.left(msg.size() - 1);    // without F7
        auto send = [&](int status, const QList<int> &values) {
            reply[4] = char(status);
            for (int v : values)
            {
                reply.append(char((v >> 7) & 0x7F));
                reply.append(char(v & 0x7F));
            }
            reply.append(char(0xF7));
            m_stub->emitSysEx(quint32(m_board->line()), reply);
            return true;
        };
        if (msg.size() == 8)            // special: the handshake
        {
            m_connected = msg.at(6) == 0x01 || m_connected;
            return send(1, {});
        }
        if (msg.size() != 15)
            return send(0x0B, {});
        if (!m_connected)
            return send(0x03, {});
        const int part = msg.at(5), wish = msg.at(6), amount = msg.at(7);
        const int block = msg.at(8), section = msg.at(9);
        const int index = (int(msg.at(10)) << 7) | int(msg.at(11));
        const int val = (int(msg.at(12)) << 7) | int(msg.at(13));
        const QString k = key(block, section);
        if (!m_tables.contains(k))
            return send(0x07, {});
        QList<int> &t = m_tables[k];
        if (wish == 0 && amount == 1)
        {
            const int from = part * 32;
            if (from >= t.size())
                return send(0x08, {});
            return send(1, t.mid(from, 32));
        }
        if (index >= t.size())
            return send(0x09, {});
        if (wish == 0)
            return send(1, { t.at(index) });
        t[index] = val;
        ++m_writes;
        return send(1, {});
    }

    IOPluginStub *m_stub;
    SimMidiDevice *m_board;
    QMap<QString, QList<int>> m_tables;
    QMap<int, int> m_under;
    bool m_connected = false;
    bool m_deaf = false;
    int m_writes = 0;
    int m_requests = 0;
};

#endif
