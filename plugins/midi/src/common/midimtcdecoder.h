/*
  Q Light Controller Plus
  midimtcdecoder.h

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

#ifndef MIDIMTCDECODER_H
#define MIDIMTCDECODER_H

#include <QtGlobal>
#include <QByteArray>

/**
 * @brief Decodes MIDI Time Code (MTC) into an absolute timeline position.
 *
 * MTC is transmitted two ways:
 *  - Quarter-frame messages (0xF1 + one data byte). Eight of them, each
 *    carrying one nibble (piece 0..7), assemble a full SMPTE time
 *    (HH:MM:SS:FF). A complete time therefore takes two frames to transmit,
 *    so the assembled value is compensated by +2 frames (forward playback).
 *  - Full-frame messages (a SysEx: F0 7F cc 01 01 hh mm ss ff F7), sent on
 *    locate/relocate. Some platforms deliver these; others reserve whole
 *    SysEx packets and drop them, in which case only quarter-frames arrive
 *    (which is the normal running case).
 *
 * This class is intentionally free of Qt object machinery so its logic can
 * be reasoned about (and unit-tested) in isolation.
 */
class MidiMtcDecoder
{
public:
    MidiMtcDecoder();

    /** Reset the assembly state (e.g. on port open). */
    void reset();

    /**
     * Feed one quarter-frame data byte (the byte that follows 0xF1).
     * @return true when a complete SMPTE time has just been assembled
     *         (i.e. the final piece of a group arrived). When true, the
     *         milliseconds()/fps()/hours()... getters hold the new value.
     */
    bool feedQuarterFrame(uchar data);

    /**
     * Parse a full-frame MTC SysEx payload.
     * @param sysex the complete message including F0 ... F7
     * @return true if it was a valid MTC full-frame and the getters updated
     */
    bool feedFullFrame(const QByteArray &sysex);

    /** Absolute position of the last assembled time, in milliseconds. */
    quint32 milliseconds() const;

    /** Nominal frame rate (24, 25, 29.97 rounded to 30, or 30). */
    int fps() const;

    int hours() const   { return m_hours; }
    int minutes() const { return m_minutes; }
    int seconds() const { return m_seconds; }
    int frames() const  { return m_frames; }

    /** SMPTE rate code carried in the timecode (0=24, 1=25, 2=29.97, 3=30). */
    int rateCode() const { return m_rateCode; }

private:
    void recomputeFromPieces();
    static double fpsForRateCode(int code);

private:
    /** The eight assembled nibbles, indexed by piece number 0..7. */
    uchar m_pieces[8];

    /** Bitmask of which pieces have been seen since the last completion. */
    quint8 m_seenMask;

    int m_hours;
    int m_minutes;
    int m_seconds;
    int m_frames;
    int m_rateCode;

    /** Frames to add when reporting milliseconds(): +2 for quarter-frame
     *  assembly (spans two frames), 0 for an exact full-frame locate. */
    int m_frameOffset;
};

#endif // MIDIMTCDECODER_H
