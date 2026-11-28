/*
  Q Light Controller Plus
  virtualmidi.h

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

#ifndef VIRTUALMIDI_H
#define VIRTUALMIDI_H

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QString>
#include <QStringList>

#include <CoreMIDI/CoreMIDI.h>

/**
 * The virtual bench (macOS): a board as the operating system shows it,
 * through CoreMIDI, so the app's real MIDI plugin finds it the way it finds
 * a board on a USB cable -- by its port names, in the list every MIDI
 * program sees.
 *
 * A port is a CoreMIDI virtual source (what the board says: send()) and a
 * virtual destination (what the app says to it: every message it is sent
 * is kept, received()). A port exists while this object does: creating it
 * is the plug, deleting it the unplug.
 *
 * What differs from hardware (and why it does not matter to the app): a
 * virtual endpoint has no device or entity, so its source and destination
 * have two unique ids where a USB port's share its entity's; the MIDI
 * plugin keys a line by that id and the app pairs a board's input and
 * output by name. And hot-plug notifications come on the run loop of the
 * thread that made the first MIDI client, as they do for hardware: see
 * VirtualMidiBench::pumpRunLoop().
 */
class VirtualMidiPort final
{
public:
    VirtualMidiPort(MIDIClientRef client, const QString &name, bool source, bool destination);
    ~VirtualMidiPort();
    Q_DISABLE_COPY(VirtualMidiPort)

    QString name() const { return m_name; }
    bool isValid() const { return m_valid; }
    OSStatus lastStatus() const { return m_status; }
    bool hasSource() const { return m_source != 0; }
    bool hasDestination() const { return m_destination != 0; }

    /** The board sends @p bytes (one or more whole messages, a SysEx too). */
    bool send(const QByteArray &bytes);
    bool send(const char *hex) { return send(QByteArray::fromHex(QByteArray(hex).replace(' ', ""))); }

    /** Every message the app sent this port, in order, each whole. */
    QList<QByteArray> received() const;
    /** How many of received() equal @p msg. */
    int count(const QByteArray &msg) const;
    /** The received messages whose status byte, channel aside, is @p status
     *  (0x90 note on, 0xB0 CC, 0xE0 pitch bend, 0xF0 SysEx). */
    QList<QByteArray> receivedOfType(uchar status) const;
    void clear();

    /** Called on CoreMIDI's thread for each packet the app sent. */
    void take(const MIDIPacketList *list);

private:
    QString m_name;
    MIDIEndpointRef m_source = 0;
    MIDIEndpointRef m_destination = 0;
    bool m_valid = false;
    OSStatus m_status = 0;

    mutable QMutex m_mutex;
    QList<QByteArray> m_received;
    QByteArray m_partial;           ///< a SysEx still coming
};

/** The boards on the bench, by the port names his boards report (read on
 *  dink 2026-11-27). */
class VirtualMidiBench final
{
public:
    VirtualMidiBench();
    ~VirtualMidiBench();
    Q_DISABLE_COPY(VirtualMidiBench)

    bool isValid() const { return m_client != 0; }
    OSStatus status() const { return m_status; }

    /** Plug a port in (source + destination unless said). */
    VirtualMidiPort *plug(const QString &name, bool source = true, bool destination = true);
    /** Plug a whole board in: its ports, as boards() lists them. */
    QList<VirtualMidiPort *> plugBoard(const QString &board);
    void unplug(const QString &name);
    void unplugBoard(const QString &board);
    void unplugAll();
    VirtualMidiPort *port(const QString &name) const { return m_ports.value(name); }
    QStringList portNames() const { return m_ports.keys(); }

    /** A board's ports: { name, source?, destination? }. */
    struct PortSpec { QString name; bool source; bool destination; };
    static QList<PortSpec> board(const QString &board);
    static QStringList boards();

    /** Every name CoreMIDI lists now (sources, destinations): what any MIDI
     *  program on this computer sees. */
    static QStringList systemSources();
    static QStringList systemDestinations();

    /** Run the main thread's CoreFoundation run loop once, without
     *  waiting: where CoreMIDI delivers its "a device came or went"
     *  notifications. The Cocoa event dispatcher of the real app runs it;
     *  the offscreen platform's (QEventDispatcherUNIX) never does. */
    static void pumpRunLoop();

private:
    MIDIClientRef m_client = 0;
    OSStatus m_status = 0;
    QMap<QString, VirtualMidiPort *> m_ports;
};

#endif
