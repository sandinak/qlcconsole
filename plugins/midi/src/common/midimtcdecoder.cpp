/*
  Q Light Controller Plus
  midimtcdecoder.cpp

  Copyright (c) Massimo Callegari

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

#include <QtMath>

#include "midimtcdecoder.h"

MidiMtcDecoder::MidiMtcDecoder()
{
    reset();
}

void MidiMtcDecoder::reset()
{
    for (int i = 0; i < 8; i++)
        m_pieces[i] = 0;
    m_seenMask = 0;
    m_hours = 0;
    m_minutes = 0;
    m_seconds = 0;
    m_frames = 0;
    m_rateCode = 3; // default 30 fps
    m_frameOffset = 0;
}

double MidiMtcDecoder::fpsForRateCode(int code)
{
    switch (code & 0x03)
    {
        case 0: return 24.0;
        case 1: return 25.0;
        case 2: return 30000.0 / 1001.0; // 29.97 drop-frame
        default: return 30.0;
    }
}

bool MidiMtcDecoder::feedQuarterFrame(uchar data)
{
    // data byte: 0nnn dddd  -> nnn = piece index (0..7), dddd = 4 data bits
    int piece = (data >> 4) & 0x07;
    uchar nibble = data & 0x0F;

    m_pieces[piece] = nibble;
    m_seenMask |= (1 << piece);

    // A group is complete when the last piece (7, hours MS + rate) arrives.
    // Require that we have also seen piece 0 since the last completion so we
    // never assemble a half-populated time after a mid-stream port open.
    if (piece == 7 && (m_seenMask & 0x01))
    {
        recomputeFromPieces();
        m_seenMask = 0;
        return true;
    }

    return false;
}

void MidiMtcDecoder::recomputeFromPieces()
{
    m_frames  =  m_pieces[0]        | ((m_pieces[1] & 0x01) << 4);
    m_seconds =  m_pieces[2]        | ((m_pieces[3] & 0x03) << 4);
    m_minutes =  m_pieces[4]        | ((m_pieces[5] & 0x03) << 4);
    m_hours   =  m_pieces[6]        | ((m_pieces[7] & 0x01) << 4);
    m_rateCode = (m_pieces[7] >> 1) & 0x03;
    m_frameOffset = 2;
}

bool MidiMtcDecoder::feedFullFrame(const QByteArray &sysex)
{
    // Expected: F0 7F <chan> 01 01 hh mm ss ff F7  (10 bytes)
    if (sysex.size() < 10)
        return false;

    const uchar *d = reinterpret_cast<const uchar *>(sysex.constData());
    if (d[0] != 0xF0 || d[1] != 0x7F || d[3] != 0x01 || d[4] != 0x01)
        return false;

    uchar hh = d[5];
    m_rateCode = (hh >> 5) & 0x03;
    m_hours    = hh & 0x1F;
    m_minutes  = d[6] & 0x3F;
    m_seconds  = d[7] & 0x3F;
    m_frames   = d[8] & 0x1F;
    // Full-frame carries the exact locate position; no 2-frame offset.
    m_frameOffset = 0;
    m_seenMask = 0;
    return true;
}

quint32 MidiMtcDecoder::milliseconds() const
{
    double fps = fpsForRateCode(m_rateCode);
    // Quarter-frame assembly completes two frames after the encoded time, so
    // compensate forward by two frames to report the current position.
    double totalFrames = ((m_hours * 3600.0) + (m_minutes * 60.0) + m_seconds) * fps
                         + m_frames + m_frameOffset;
    return quint32(qRound((totalFrames / fps) * 1000.0));
}

int MidiMtcDecoder::fps() const
{
    return int(qRound(fpsForRateCode(m_rateCode)));
}
