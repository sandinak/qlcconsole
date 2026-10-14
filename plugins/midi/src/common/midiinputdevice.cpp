/*
  Q Light Controller
  midiinputdevice.cpp

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

#include <QDebug>
#include "midiinputdevice.h"

MidiInputDevice::MidiInputDevice(const QVariant& uid, const QString& name, QObject* parent)
    : MidiDevice(uid, name, Input, parent)
{
    //qDebug() << Q_FUNC_INFO;
}

MidiInputDevice::~MidiInputDevice()
{
    //qDebug() << Q_FUNC_INFO;
}

void MidiInputDevice::emitValueChanged(uint channel, uchar value)
{
    emit valueChanged(uid(), channel, value);
}

void MidiInputDevice::feedMTCQuarterFrame(uchar data)
{
    if (m_mtcDecoder.feedQuarterFrame(data))
        emit mtcTimeChanged(uid(), m_mtcDecoder.milliseconds(), uchar(m_mtcDecoder.fps()));
}

void MidiInputDevice::feedMTCFullFrame(const QByteArray &sysex)
{
    if (m_mtcDecoder.feedFullFrame(sysex))
        emit mtcTimeChanged(uid(), m_mtcDecoder.milliseconds(), uchar(m_mtcDecoder.fps()));
}

void MidiInputDevice::feedSysEx(const uchar *data, int length)
{
    for (const QByteArray &message : m_sysEx.feed(data, length))
    {
        // F0 7F <device> 01 01 ...: a full-frame MTC position.
        if (message.size() >= 10 && uchar(message.at(1)) == 0x7F
                && uchar(message.at(3)) == 0x01 && uchar(message.at(4)) == 0x01)
            feedMTCFullFrame(message);
        else
            emit sysExReceived(uid(), message);
    }
}
