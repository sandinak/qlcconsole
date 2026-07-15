/*
  Q Light Controller Plus
  midi_test.cpp

  Copyright (c) Jano Svitok

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

#include <QTest>

#define private public
#include "midi_test.h"
#include "midiprotocol.h"
#include "midimtcdecoder.h"

#undef private

/** Build the 8 quarter-frame data bytes encoding hh:mm:ss:ff at rateCode. */
static void buildQuarterFrames(int hh, int mm, int ss, int ff, int rateCode, uchar out[8])
{
    out[0] = (0 << 4) | (ff & 0x0F);
    out[1] = (1 << 4) | ((ff >> 4) & 0x01);
    out[2] = (2 << 4) | (ss & 0x0F);
    out[3] = (3 << 4) | ((ss >> 4) & 0x03);
    out[4] = (4 << 4) | (mm & 0x0F);
    out[5] = (5 << 4) | ((mm >> 4) & 0x03);
    out[6] = (6 << 4) | (hh & 0x0F);
    out[7] = (7 << 4) | (((rateCode & 0x03) << 1) | ((hh >> 4) & 0x01));
}

/****************************************************************************
 * MIDI tests
 ****************************************************************************/

void Midi_Test::midiToInput()
{
    quint32 channel = 0;
    uchar value = 0;

    uchar midiChannel = 7;
    uchar cmd = MIDI_NOTE_ON | midiChannel;
    uchar data1 = 10;
    uchar data2 = 127;

    QLCMIDIProtocol::midiToInput(cmd, data1, data2, midiChannel, &channel, &value);

    QCOMPARE(channel, 138U);
    QCOMPARE(value, uchar(255U));
}

void Midi_Test::mtcQuarterFrame()
{
    MidiMtcDecoder dec;
    uchar qf[8];
    // 01:02:03:04 @ 25 fps (rate code 1)
    buildQuarterFrames(1, 2, 3, 4, 1, qf);

    // Only the final piece (7) completes a time.
    for (int i = 0; i < 7; i++)
        QCOMPARE(dec.feedQuarterFrame(qf[i]), false);
    QCOMPARE(dec.feedQuarterFrame(qf[7]), true);

    QCOMPARE(dec.hours(), 1);
    QCOMPARE(dec.minutes(), 2);
    QCOMPARE(dec.seconds(), 3);
    QCOMPARE(dec.frames(), 4);
    QCOMPARE(dec.fps(), 25);
    // (3723 s * 25 + 4 frames + 2 assembly frames) / 25 * 1000 = 3,723,240 ms
    QCOMPARE(dec.milliseconds(), quint32(3723240));
}

void Midi_Test::mtcFullFrame()
{
    MidiMtcDecoder dec;
    // Full-frame SysEx for 02:00:00:00 @ 30 fps (rate code 3).
    QByteArray sysex;
    sysex.append(char(0xF0));
    sysex.append(char(0x7F));
    sysex.append(char(0x7F)); // device id
    sysex.append(char(0x01)); // sub-id 1
    sysex.append(char(0x01)); // sub-id 2 (full-frame)
    sysex.append(char(((3 & 0x03) << 5) | 2)); // rate<<5 | hours
    sysex.append(char(0x00)); // minutes
    sysex.append(char(0x00)); // seconds
    sysex.append(char(0x00)); // frames
    sysex.append(char(0xF7));

    QCOMPARE(dec.feedFullFrame(sysex), true);
    QCOMPARE(dec.fps(), 30);
    // Exact locate, no assembly offset: 2 h = 7,200,000 ms
    QCOMPARE(dec.milliseconds(), quint32(7200000));
}

void Midi_Test::mtcPartialIgnored()
{
    MidiMtcDecoder dec;
    uchar qf[8];
    buildQuarterFrames(0, 0, 10, 0, 3, qf);
    // A stream that begins mid-group (piece 7 before piece 0) must not
    // assemble a bogus time.
    QCOMPARE(dec.feedQuarterFrame(qf[7]), false);
    QCOMPARE(dec.feedQuarterFrame(qf[3]), false);
    // A full clean group then completes.
    for (int i = 0; i < 7; i++)
        dec.feedQuarterFrame(qf[i]);
    QCOMPARE(dec.feedQuarterFrame(qf[7]), true);
    QCOMPARE(dec.seconds(), 10);
}

QTEST_MAIN(Midi_Test)
