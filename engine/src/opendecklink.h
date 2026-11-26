/*
  Q Light Controller Plus
  opendecklink.h

  Copyright (C) Branson Matheson

  The OpenDeck SysEx subset here (requests, pagination, the LED block's
  sections, the full-config backup and the diffing restore) is ported from
  qlcplus-midi-profiler (qlcprofiler/opendeck.py, Apache-2.0, by Branson
  Matheson), which was verified against OpenDeck 2.x hardware (a PMJ Black 1).

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

#ifndef OPENDECKLINK_H
#define OPENDECKLINK_H

#include <QByteArray>
#include <QString>
#include <QList>
#include <QMap>

#include <functional>

#include "qlcengine_export.h"

class QEventLoop;

/** @addtogroup engine Engine
 * @{
 */

/**
 * A SysEx session with an OpenDeck board (shanteacontrols/OpenDeck: the PMJ
 * Black 1 runs it): its configuration database read and written over SysEx.
 *
 * Request: F0 00 53 43 <status> <part> <wish> <amount> <block> <section>
 *          <index MSB> <index LSB> <value MSB> <value LSB> F7
 * The reply echoes the request with the status set (1 = ack) and, for a
 * read, the values after it as 7-bit MSB/LSB pairs. A section bigger than
 * one reply comes in "parts" of 32: every part is read until a short one or
 * a refusal (asking for part 0 only makes a 128-LED board look like 32).
 *
 * The link sends through @a send (the board's MIDI output) and is handed
 * each SysEx that comes back with deliver(); a request waits for its reply
 * up to the timeout, with user input held back meanwhile.
 */
class QLC_ENGINE_EXPORT OpenDeckLink
{
public:
    typedef std::function<bool(const QByteArray &)> Send;

    enum Block { GlobalBlock = 0, ButtonBlock = 1, EncoderBlock = 2, AnalogBlock = 3, LedBlock = 4 };
    enum LedSection
    {
        LedLevel = 0,
        LedActivationId = 3,
        LedRgb = 4,
        LedControlType = 5,
        LedActivationValue = 6,
        LedChannel = 7          ///< 1-based, as OpenDeck stores it
    };
    /** io/outputs/shared/common.h ControlType: an LED driven by incoming notes. */
    enum { MidiInNoteSingleVal = 0, MidiInNoteMultiVal = 6 };

    enum Status
    {
        StatusRequest = 0x00,
        StatusAck = 0x01,
        StatusHandshakeError = 0x03,
        StatusPartNotSupported = 0x08,
        StatusIndexOutOfRange = 0x09
    };
    enum { WishGet = 0, WishSet = 1, AmountSingle = 0, AmountAll = 1, SpecialConnect = 1 };

    /** One LED's configuration. */
    struct Led
    {
        int index = 0;
        int note = 0;           ///< activation id
        int channel = 1;        ///< MIDI channel 1-16
        int controlType = 0;
        int activationValue = 0;
        /** It lights from an incoming note (not static, not local). */
        bool hearsNotes() const
        { return controlType == MidiInNoteSingleVal || controlType == MidiInNoteMultiVal; }
    };

    /** Every section a board answers, keyed "block/section". */
    typedef QMap<QString, QList<int>> Tables;

    struct RestoreResult
    {
        int changed = 0;
        int unchanged = 0;
        QStringList failed;
    };

    explicit OpenDeckLink(Send send, int timeoutMs = 1000);
    ~OpenDeckLink();

    /** A SysEx (F0 ... F7) that arrived from the board. */
    void deliver(const QByteArray &sysEx);

    /** The handshake every session starts with. */
    bool handshake();
    /** Every value of @p section in @p block; empty when the board has no such section. */
    QList<int> readSection(int block, int section);
    /** One value written; the board's status byte (StatusAck = done, -1: no answer). */
    int writeSingle(int block, int section, int index, int value);
    bool writeChecked(int block, int section, int index, int value);

    /** The LED table (activation note, channel, type, value of each LED). */
    QList<Led> readLeds();
    /** Point LED @p led at @p note on MIDI channel @p channel (1-16): a
     *  note-driven LED whose velocity is its brightness. */
    bool alignLed(int led, int note, int channel);

    /** Every section the board will hand over: a full backup. */
    Tables dumpAll(int maxBlock = 6, int maxSection = 15);
    /** @p tables written back, only the values that differ from the board's now. */
    RestoreResult restoreAll(const Tables &tables);

    /** What went wrong last ("no answer", "index out of range"). */
    QString lastError() const { return m_error; }
    /** Requests sent so far (a test counts them). */
    int requests() const { return m_requests; }

    /* ---- the wire format (pure) ---- */
    static bool isOpenDeck(const QByteArray &sysEx);
    static QByteArray request(int part, int wish, int amount, int block, int section,
                              int index = 0, int value = 0);
    static QByteArray special(int wish);
    /** The status byte of a reply (-1: not an OpenDeck reply). */
    static int replyStatus(const QByteArray &sysEx);
    /** The values a read's reply carries. */
    static QList<int> replyValues(const QByteArray &sysEx);
    static QString statusName(int status);

    /* ---- backups (the profiler's JSON, so either tool restores the other's) ---- */
    static bool saveBackup(const QString &path, const QString &device, const Tables &tables,
                           QString *error = nullptr);
    static bool loadBackup(const QString &path, Tables *tables, QString *error = nullptr);

private:
    /** Send @p msg and wait for the board's answer to it (empty: none). */
    QByteArray exchange(const QByteArray &msg);

    Send m_send;
    int m_timeoutMs;
    QByteArray m_expect;        ///< the request whose answer is awaited
    QByteArray m_reply;
    bool m_waiting = false;
    QEventLoop *m_loop = nullptr;
    QString m_error;
    int m_requests = 0;
};

/** @} */

#endif
