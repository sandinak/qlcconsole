/*
  Q Light Controller Plus
  virtualmidi.cpp

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

#include <QMutexLocker>
#include <QDebug>

#include <CoreFoundation/CoreFoundation.h>

#include "virtualmidi.h"

// The C API CoreMIDI has had since 10.0; the newer protocol calls add
// nothing a MIDI 1.0 board needs.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace
{
CFStringRef cf(const QString &s)
{
    return CFStringCreateWithCharacters(kCFAllocatorDefault,
                                        reinterpret_cast<const UniChar *>(s.unicode()), s.length());
}

QString qs(CFStringRef s)
{
    if (s == nullptr)
        return QString();
    const CFIndex len = CFStringGetLength(s);
    QString out;
    out.resize(qsizetype(len));
    CFStringGetCharacters(s, CFRangeMake(0, len), reinterpret_cast<UniChar *>(out.data()));
    return out;
}

QString nameOf(MIDIEndpointRef e)
{
    CFStringRef s = nullptr;
    if (MIDIObjectGetStringProperty(e, kMIDIPropertyDisplayName, &s) != noErr || s == nullptr)
        if (MIDIObjectGetStringProperty(e, kMIDIPropertyName, &s) != noErr || s == nullptr)
            return QString();
    const QString out = qs(s);
    CFRelease(s);
    return out;
}

/* A channel message's length by its status byte (0 for a system one). */
int messageLength(uchar status)
{
    switch (status & 0xF0)
    {
        case 0x80: case 0x90: case 0xA0: case 0xB0: case 0xE0: return 3;
        case 0xC0: case 0xD0: return 2;
        default: break;
    }
    switch (status)
    {
        case 0xF1: case 0xF3: return 2;
        case 0xF2: return 3;
        default: return 1;
    }
}

void readProc(const MIDIPacketList *list, void *portRef, void *)
{
    static_cast<VirtualMidiPort *>(portRef)->take(list);
}
}

/****************************************************************************
 * VirtualMidiPort
 ****************************************************************************/

VirtualMidiPort::VirtualMidiPort(MIDIClientRef client, const QString &name, bool source, bool destination)
    : m_name(name)
{
    CFStringRef n = cf(name);
    m_valid = true;
    if (source)
    {
        m_status = MIDISourceCreate(client, n, &m_source);
        m_valid = m_valid && m_status == noErr;
    }
    if (destination && m_valid)
    {
        m_status = MIDIDestinationCreate(client, n, readProc, this, &m_destination);
        m_valid = m_valid && m_status == noErr;
    }
    CFRelease(n);
}

VirtualMidiPort::~VirtualMidiPort()
{
    if (m_source != 0)
        MIDIEndpointDispose(m_source);
    if (m_destination != 0)
        MIDIEndpointDispose(m_destination);
}

bool VirtualMidiPort::send(const QByteArray &bytes)
{
    if (m_source == 0 || bytes.isEmpty())
        return false;
    QByteArray buffer(bytes.size() + 256, 0);
    MIDIPacketList *list = reinterpret_cast<MIDIPacketList *>(buffer.data());
    MIDIPacket *packet = MIDIPacketListInit(list);
    packet = MIDIPacketListAdd(list, ByteCount(buffer.size()), packet, 0, ByteCount(bytes.size()),
                               reinterpret_cast<const Byte *>(bytes.constData()));
    if (packet == nullptr)
        return false;
    return MIDIReceived(m_source, list) == noErr;
}

void VirtualMidiPort::take(const MIDIPacketList *list)
{
    QMutexLocker locker(&m_mutex);
    const MIDIPacket *packet = &list->packet[0];
    for (UInt32 p = 0; p < list->numPackets; ++p)
    {
        const uchar *d = packet->data;
        const int len = int(packet->length);
        int i = 0;
        while (i < len)
        {
            if (!m_partial.isEmpty() || d[i] == 0xF0)
            {
                // A SysEx, maybe over several packets: whole to its F7.
                while (i < len)
                {
                    m_partial.append(char(d[i]));
                    if (d[i++] == 0xF7)
                    {
                        m_received << m_partial;
                        m_partial.clear();
                        break;
                    }
                }
                continue;
            }
            if (d[i] < 0x80)
            {
                ++i;    // a stray data byte (the plugin never uses running status)
                continue;
            }
            const int n = qMin(messageLength(d[i]), len - i);
            m_received << QByteArray(reinterpret_cast<const char *>(d + i), n);
            i += n;
        }
        packet = MIDIPacketNext(packet);
    }
}

QList<QByteArray> VirtualMidiPort::received() const
{
    QMutexLocker locker(&m_mutex);
    return m_received;
}

int VirtualMidiPort::count(const QByteArray &msg) const
{
    QMutexLocker locker(&m_mutex);
    return int(m_received.count(msg));
}

QList<QByteArray> VirtualMidiPort::receivedOfType(uchar status) const
{
    QMutexLocker locker(&m_mutex);
    QList<QByteArray> out;
    for (const QByteArray &m : m_received)
    {
        const uchar s = uchar(m.at(0));
        if ((status == 0xF0 && s == 0xF0) || (status != 0xF0 && (s & 0xF0) == status))
            out << m;
    }
    return out;
}

void VirtualMidiPort::clear()
{
    QMutexLocker locker(&m_mutex);
    m_received.clear();
    m_partial.clear();
}

/****************************************************************************
 * VirtualMidiBench
 ****************************************************************************/

VirtualMidiBench::VirtualMidiBench()
{
    m_status = MIDIClientCreate(CFSTR("qlcconsole virtual bench"), nullptr, nullptr, &m_client);
    if (m_status != noErr)
        m_client = 0;
}

VirtualMidiBench::~VirtualMidiBench()
{
    unplugAll();
    if (m_client != 0)
        MIDIClientDispose(m_client);
}

VirtualMidiPort *VirtualMidiBench::plug(const QString &name, bool source, bool destination)
{
    if (m_client == 0)
        return nullptr;
    unplug(name);
    VirtualMidiPort *p = new VirtualMidiPort(m_client, name, source, destination);
    if (!p->isValid())
    {
        qWarning() << "[VirtualMidi] could not make" << name << "status" << p->lastStatus();
        delete p;
        return nullptr;
    }
    m_ports.insert(name, p);
    return p;
}

QList<VirtualMidiPort *> VirtualMidiBench::plugBoard(const QString &b)
{
    QList<VirtualMidiPort *> out;
    for (const PortSpec &s : board(b))
        out << plug(s.name, s.source, s.destination);
    return out;
}

void VirtualMidiBench::unplug(const QString &name)
{
    delete m_ports.take(name);
}

void VirtualMidiBench::unplugBoard(const QString &b)
{
    for (const PortSpec &s : board(b))
        unplug(s.name);
}

void VirtualMidiBench::unplugAll()
{
    qDeleteAll(m_ports);
    m_ports.clear();
}

QList<VirtualMidiBench::PortSpec> VirtualMidiBench::board(const QString &b)
{
    // The names CoreMIDI gave his boards on dink, 2026-11-27.
    if (b == QLatin1String("APC40 mkII"))
        return { { "APC40 mkII", true, true } };
    if (b == QLatin1String("APC64"))
        return { { "APC64 DAW (APC64)", true, true }, { "APC64 Notes (APC64)", true, true },
                 { "APC64 MIDI (APC64)", true, true }, { "APC64 Custom (APC64)", true, true } };
    if (b == QLatin1String("APC mini mk2"))
        return { { "APC mini mk2 Control", true, true }, { "APC mini mk2 Notes", true, true } };
    if (b == QLatin1String("FaderPort 2"))
        return { { "PreSonus FP2", true, true } };
    // The MaizeDMX: its Port 2 is an output only.
    if (b == QLatin1String("MaizeDMX"))
        return { { "USB2.0-MIDI Port 1", true, true }, { "USB2.0-MIDI Port 2", false, true } };
    if (b == QLatin1String("PMJ Black 1"))
        return { { "OpenDeck | PMJ_BLACK_1", true, true } };
    if (b == QLatin1String("RTP"))
        return { { "Network RTP Session 1", true, true } };
    return {};
}

QStringList VirtualMidiBench::boards()
{
    return { "APC40 mkII", "APC64", "APC mini mk2", "FaderPort 2", "MaizeDMX", "PMJ Black 1", "RTP" };
}

QStringList VirtualMidiBench::systemSources()
{
    QStringList out;
    for (ItemCount i = 0; i < MIDIGetNumberOfSources(); ++i)
        out << nameOf(MIDIGetSource(i));
    return out;
}

QStringList VirtualMidiBench::systemDestinations()
{
    QStringList out;
    for (ItemCount i = 0; i < MIDIGetNumberOfDestinations(); ++i)
        out << nameOf(MIDIGetDestination(i));
    return out;
}

void VirtualMidiBench::pumpRunLoop()
{
    while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) == kCFRunLoopRunHandledSource)
        ;
}

#pragma clang diagnostic pop
