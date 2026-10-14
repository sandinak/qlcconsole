/*
  Q Light Controller
  midiinputdevice.h

  Copyright (c) Heikki Junnila

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

#ifndef MIDIINPUTDEVICE_H
#define MIDIINPUTDEVICE_H

#include "mididevice.h"
#include "midimtcdecoder.h"
#include "midisysexassembler.h"

class MidiInputDevice : public MidiDevice
{
    Q_OBJECT

public:
    MidiInputDevice(const QVariant& uid, const QString& name, QObject* parent = 0);
    virtual ~MidiInputDevice();

    void emitValueChanged(uint channel, uchar value);

    /** Feed a MIDI Time Code quarter-frame data byte (the byte after 0xF1).
     *  When a full SMPTE time is assembled, emits mtcTimeChanged(). */
    void feedMTCQuarterFrame(uchar data);

    /** Feed a full-frame MTC SysEx payload (F0 7F .. F7). Emits
     *  mtcTimeChanged() if it is a valid full-frame message. */
    void feedMTCFullFrame(const QByteArray &sysex);

    /** Feed SysEx bytes as the driver hands them over (a message may come
     *  in pieces). A full-frame MTC message goes to the MTC decoder; any
     *  other whole message is emitted as sysExReceived(). */
    void feedSysEx(const uchar *data, int length);

    /** A SysEx message has started and not finished: the driver's next
     *  bytes belong to it. */
    bool inSysEx() const { return m_sysEx.inProgress(); }

signals:
    void valueChanged(const QVariant& uid, ushort channel, uchar value);

    /** Emitted with an absolute timeline position (ms) and nominal fps
     *  whenever incoming MTC assembles a complete SMPTE time. */
    void mtcTimeChanged(const QVariant& uid, quint32 msPosition, uchar fps);

    /** A whole SysEx message (F0 ... F7) other than MTC -- MIDI Show Control. */
    void sysExReceived(const QVariant& uid, const QByteArray &sysEx);

private:
    MidiMtcDecoder m_mtcDecoder;
    MidiSysExAssembler m_sysEx;
};

#endif
