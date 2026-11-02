/*
  Q Light Controller Plus
  encoderbank.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QVariant>
#include <QDebug>
#include <QObject>
#include <climits>

#include "inputoutputmap.h"
#include "encoderbank.h"
#include "doc.h"

#define KXMLName QStringLiteral("Name")
#define KXMLPreset QStringLiteral("Preset")
#define KXMLUniverse QStringLiteral("Universe")
#define KXMLEncoding QStringLiteral("Encoding")
#define KXMLInvert QStringLiteral("Invert")
#define KXMLCount QStringLiteral("Encoders")
#define KXMLTurns QStringLiteral("Turn")
#define KXMLPushes QStringLiteral("Push")
#define KXMLRings QStringLiteral("Ring")
#define KXMLNext QStringLiteral("Next")
#define KXMLPrevious QStringLiteral("Previous")
#define KXMLDirect QStringLiteral("Direct")
#define KXMLLamps QStringLiteral("Lamps")

namespace
{
/* The MIDI plugin's channel numbering (plugins/midi/src/common/midiprotocol.h):
   a CC n is n, a note n is 128 + n, and in an Omni patch each MIDI channel
   above the first adds 4096. */
constexpr qint32 CC(int n, int midiChannel = 1) { return qint32((midiChannel - 1) * 4096 + n); }
constexpr qint32 NOTE(int n, int midiChannel = 1) { return qint32((midiChannel - 1) * 4096 + 128 + n); }

struct Preset
{
    const char *key;
    const char *name;
    const char *note;
    EncoderBank::Encoding encoding;
    bool invert;
    int count;
    qint32 encoders, pushes, rings, next, previous, direct, lamps;
};

/* Decided (can change): three generic encodings, and the three controllers
   Branson named. The numbers are each maker's factory layout as documented;
   none has been checked against a unit here, so the panel keeps every one
   editable and the note says what to set on the controller. */
const Preset kPresets[] = {
    { "generic-twos", "Generic, two's complement",
      "Encoders send CC 16-23; clockwise 1-63, counter-clockwise 127-65. Bank buttons: CC 24 (next), CC 25 (previous).",
      EncoderBank::TwosComplement, false, 8, CC(16), -1, -1, CC(24), CC(25), -1, -1 },
    { "generic-offset", "Generic, binary offset",
      "Encoders send CC 16-23 around 64 (65 clockwise, 63 counter-clockwise). Bank buttons: CC 24 (next), CC 25 (previous).",
      EncoderBank::BinaryOffset, false, 8, CC(16), -1, -1, CC(24), CC(25), -1, -1 },
    { "generic-signmag", "Generic, sign-magnitude",
      "Encoders send CC 16-23; bit 6 is the direction, the low bits the speed. Bank buttons: CC 24 (next), CC 25 (previous).",
      EncoderBank::SignedBit, false, 8, CC(16), -1, -1, CC(24), CC(25), -1, -1 },
    { "xtouch-mini", "Behringer X-Touch Mini",
      "Standard mode, layer A: set the encoders to Relative 1 in the X-Touch Editor. Pushes are notes 0-7, the "
      "top row of buttons (notes 8-12) picks a bank and its lamp shows it, notes 13 and 14 step.",
      EncoderBank::BinaryOffset, false, 8, CC(1), NOTE(0), CC(1), NOTE(13), NOTE(14), NOTE(8), NOTE(8) },
    { "twister", "Midi Fighter Twister",
      "Encoders are CC 0-7 on MIDI channel 1 in 3FH/41H mode, their pushes on channel 2 and the side buttons "
      "on channel 4: patch the input in Omni mode.",
      EncoderBank::BinaryOffset, false, 8, CC(0, 1), CC(0, 2), CC(0, 1), CC(13, 4), CC(10, 4), -1, -1 },
    { "apc40", "Akai APC40 (knobs)",
      "The Device Control knobs, CC 16-23, sending their position (they stop at the ends); the rings are CC 24-31. "
      "Device left and right (notes 58 and 59) step the banks.",
      EncoderBank::AbsoluteDelta, false, 8, CC(16), -1, CC(24), NOTE(59), NOTE(58), -1, -1 }
};

const Preset *findPreset(const QString &key)
{
    for (const Preset &p : kPresets)
        if (key == QLatin1String(p.key))
            return &p;
    return nullptr;
}

QString noteName(quint32 note)
{
    static const char *names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return QStringLiteral("%1%2").arg(QLatin1String(names[note % 12])).arg(int(note / 12) - 1);
}

const int kRefuseGapMs = 2000;
}

EncoderBank::EncoderBank(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
    m_timer.setInterval(25);
    connect(&m_timer, &QTimer::timeout, this, &EncoderBank::slotTick);
}

EncoderBank::~EncoderBank()
{
}

/*****************************************************************************
 * Banks and slots
 *****************************************************************************/

QString EncoderBank::bankName(int bank)
{
    switch (bank)
    {
    case IntensityBank: return QObject::tr("Intensity");
    case ColourBank: return QObject::tr("Colour");
    case PositionBank: return QObject::tr("Position");
    case BeamBank: return QObject::tr("Beam");
    case GoboBank: return QObject::tr("Gobo / prism");
    default: return QString();
    }
}

int EncoderBank::slotCount(int bank)
{
    switch (bank)
    {
    case IntensityBank: return 1;
    case ColourBank: return 8;
    case PositionBank: return 2;
    case BeamBank: return 6;
    case GoboBank: return 4;
    default: return 0;
    }
}

QString EncoderBank::slotName(int bank, int slot)
{
    if (slot < 0 || slot >= slotCount(bank))
        return QString();
    switch (bank)
    {
    case IntensityBank:
        return QObject::tr("Intensity");
    case ColourBank:
    {
        const QStringList names = { QObject::tr("Hue"), QObject::tr("Saturation"), QObject::tr("Red"),
                                    QObject::tr("Green"), QObject::tr("Blue"), QObject::tr("White"),
                                    QObject::tr("Amber"), QObject::tr("UV") };
        return names.at(slot);
    }
    case PositionBank:
        return slot == 0 ? QObject::tr("Pan") : QObject::tr("Tilt");
    case BeamBank:
    {
        const QStringList names = { QObject::tr("Zoom"), QObject::tr("Focus"), QObject::tr("Iris"),
                                    QObject::tr("Frost"), QObject::tr("Strobe"), QObject::tr("Shutter") };
        return names.at(slot);
    }
    case GoboBank:
    {
        const QStringList names = { QObject::tr("Gobo"), QObject::tr("Gobo rotation"), QObject::tr("Prism"),
                                    QObject::tr("Prism rotation") };
        return names.at(slot);
    }
    }
    return QString();
}

/*****************************************************************************
 * The device
 *****************************************************************************/

QStringList EncoderBank::presetKeys()
{
    QStringList keys;
    for (const Preset &p : kPresets)
        keys << QLatin1String(p.key);
    return keys;
}

QString EncoderBank::presetName(const QString &key)
{
    const Preset *p = findPreset(key);
    return p != nullptr ? QObject::tr(p->name) : QString();
}

QString EncoderBank::presetNote(const QString &key)
{
    const Preset *p = findPreset(key);
    return p != nullptr ? QObject::tr(p->note) : QString();
}

bool EncoderBank::preset(const QString &key, Device *device)
{
    const Preset *p = findPreset(key);
    if (p == nullptr || device == nullptr)
        return false;
    const quint32 universe = device->universe;
    *device = Device();
    device->universe = universe;
    device->name = QObject::tr(p->name);
    device->encoding = p->encoding;
    device->invert = p->invert;
    device->encoderCount = p->count;
    device->encoders = p->encoders;
    device->pushes = p->pushes;
    device->rings = p->rings;
    device->bankNext = p->next;
    device->bankPrevious = p->previous;
    device->bankDirect = p->direct;
    device->bankLamps = p->lamps;
    return true;
}

QString EncoderBank::encodingName(Encoding encoding)
{
    switch (encoding)
    {
    case TwosComplement: return QObject::tr("Two's complement");
    case SignedBit: return QObject::tr("Sign-magnitude");
    case BinaryOffset: return QObject::tr("Binary offset");
    case AbsoluteDelta: return QObject::tr("Absolute (position)");
    }
    return QString();
}

QString EncoderBank::encodingToString(Encoding encoding)
{
    switch (encoding)
    {
    case TwosComplement: return QStringLiteral("TwosComplement");
    case SignedBit: return QStringLiteral("SignedBit");
    case BinaryOffset: return QStringLiteral("BinaryOffset");
    case AbsoluteDelta: return QStringLiteral("Absolute");
    }
    return QStringLiteral("TwosComplement");
}

EncoderBank::Encoding EncoderBank::stringToEncoding(const QString &str)
{
    if (str == QLatin1String("SignedBit"))
        return SignedBit;
    if (str == QLatin1String("BinaryOffset"))
        return BinaryOffset;
    if (str == QLatin1String("Absolute"))
        return AbsoluteDelta;
    return TwosComplement;
}

int EncoderBank::decode(Encoding encoding, uchar value)
{
    // The MIDI plugin scaled the 0-127 data byte to 0-255 (v << 1, 127 as
    // 255): the byte as the controller sent it is value >> 1.
    const int v = value >> 1;
    switch (encoding)
    {
    case TwosComplement:
        // 1..63 = +1..+63, 65..127 = -63..-1 (0 and 64: no movement)
        return v < 64 ? v : v - 128;
    case SignedBit:
        // bit 6 = clockwise, the low six bits the speed
        return (v & 0x40) ? (v & 0x3F) : -(v & 0x3F);
    case BinaryOffset:
        // centred on 64
        return v - 64;
    case AbsoluteDelta:
        break;
    }
    return 0;
}

QString EncoderBank::controlName(qint32 channel)
{
    if (channel < 0)
        return QString();
    const quint32 ch = quint32(channel);
    const quint32 midi = ch / 4096 + 1;
    const quint32 n = ch % 4096;
    QString what;
    if (n < 128)
        what = QObject::tr("CC %1").arg(n);
    else if (n < 256)
        what = QObject::tr("Note %1 (%2)").arg(n - 128).arg(noteName(n - 128));
    else
        what = QObject::tr("Channel %1").arg(n);
    return midi > 1 ? QObject::tr("%1, MIDI ch %2").arg(what).arg(midi) : what;
}

void EncoderBank::setDevice(const Device &device, const QString &presetKey)
{
    m_device = device;
    m_device.encoderCount = qBound(1, m_device.encoderCount, slotsPerBank());
    m_presetKey = presetKey;
    m_enabled = m_device.encoders >= 0;
    m_pushDown = m_turned = 0;
    for (int i = 0; i < slotsPerBank(); ++i)
    {
        m_pending[i] = 0;
        m_last[i] = -1;
        m_ring[i] = -1;
    }
    updateConnection();
    m_doc->setModified();
    emit deviceChanged();
    if (m_enabled)
        resendFeedback();
}

void EncoderBank::setEnabled(bool enabled)
{
    enabled = enabled && m_device.encoders >= 0;
    if (enabled == m_enabled)
        return;
    m_enabled = enabled;
    m_pushDown = m_turned = 0;
    updateConnection();
    m_doc->setModified();
    emit deviceChanged();
    if (m_enabled)
        resendFeedback();
}

void EncoderBank::reset()
{
    const bool had = m_enabled || m_device.encoders >= 0;
    m_device = Device();
    m_presetKey.clear();
    m_enabled = false;
    m_bank = IntensityBank;
    m_pushDown = m_turned = 0;
    m_timer.stop();
    for (int i = 0; i < slotsPerBank(); ++i)
    {
        m_pending[i] = 0;
        m_last[i] = -1;
        m_ring[i] = -1;
    }
    if (had)
    {
        updateConnection();
        emit deviceChanged();
    }
}

void EncoderBank::updateConnection()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    if (iom == nullptr || m_enabled == m_connected)
        return;
    if (m_enabled)
    {
        connect(iom, &InputOutputMap::inputValueChanged, this, &EncoderBank::handleInput);
        connect(iom, &InputOutputMap::inputDeviceConnected, this, &EncoderBank::slotInputDeviceConnected);
        connect(iom, &InputOutputMap::profileChanged, this, &EncoderBank::slotInputDeviceConnected);
    }
    else
    {
        disconnect(iom, &InputOutputMap::inputValueChanged, this, &EncoderBank::handleInput);
        disconnect(iom, &InputOutputMap::inputDeviceConnected, this, &EncoderBank::slotInputDeviceConnected);
        disconnect(iom, &InputOutputMap::profileChanged, this, &EncoderBank::slotInputDeviceConnected);
    }
    m_connected = m_enabled;
}

void EncoderBank::slotInputDeviceConnected()
{
    resendFeedback();
}

/*****************************************************************************
 * Banks
 *****************************************************************************/

void EncoderBank::setBank(int bank)
{
    bank = qBound(0, bank, bankCount() - 1);
    flush();
    if (bank == m_bank)
    {
        sendBankLamps();
        return;
    }
    m_bank = bank;
    for (int i = 0; i < slotsPerBank(); ++i)
        m_ring[i] = -1;         // the next bank's values are lit afresh
    sendBankLamps();
    emit bankChanged(m_bank);
}

void EncoderBank::stepBank(int direction)
{
    setBank((m_bank + (direction < 0 ? bankCount() - 1 : 1)) % bankCount());
}

/*****************************************************************************
 * Input
 *****************************************************************************/

void EncoderBank::setCoalesceMs(int ms)
{
    m_timer.setInterval(qMax(0, ms));
}

void EncoderBank::handleInput(quint32 universe, quint32 channel, uchar value)
{
    if (!m_enabled || universe != m_device.universe)
        return;

    int i = indexIn(channel, m_device.encoders, m_device.encoderCount);
    if (i >= 0)
    {
        turn(i, value);
        return;
    }
    i = indexIn(channel, m_device.pushes, m_device.encoderCount);
    if (i >= 0)
    {
        push(i, value > 0);
        return;
    }
    if (value == 0)
        return;     // the buttons act on the press
    if (qint32(channel) == m_device.bankNext)
        stepBank(1);
    else if (qint32(channel) == m_device.bankPrevious)
        stepBank(-1);
    else if ((i = indexIn(channel, m_device.bankDirect, bankCount())) >= 0)
        setBank(i);
}

void EncoderBank::turn(int slot, uchar value)
{
    int detents;
    if (m_device.encoding == AbsoluteDelta)
    {
        const int v = value >> 1;
        const int last = m_last[slot];
        m_last[slot] = v;
        if (last < 0)
            return;     // the first reading is only where the knob is
        detents = v - last;
    }
    else
    {
        detents = decode(m_device.encoding, value);
    }
    if (m_device.invert)
        detents = -detents;
    if (detents == 0 || slot >= slotCount(m_bank))
        return;

    // Turning while a push is held is the fine turn, and not a tap.
    m_turned |= m_pushDown | (1u << slot);
    if (operateRefuses())
        return;
    queue(slot, detents, isFine(slot));
}

void EncoderBank::push(int slot, bool down)
{
    const quint32 bit = 1u << slot;
    if (down)
    {
        m_pushDown |= bit;
        m_turned &= ~bit;
        return;
    }
    if ((m_pushDown & bit) == 0)
        return;
    m_pushDown &= ~bit;
    const bool turned = (m_turned & bit) != 0;
    m_turned &= ~bit;
    if (turned || slot >= slotCount(m_bank))
        return;
    flush();
    if (operateRefuses())
        return;
    emit homed(m_bank, slot);
}

bool EncoderBank::operateRefuses()
{
    /* Decided (can change): in Operate the encoders do not edit. The show is
       running and a look about to be GOne is not a place for a knob that
       was brushed; Refine's quiet rule is for the one cue that runs, and an
       encoder has no cue to hold to. The banks still step. */
    if (m_doc->mode() != Doc::Operate)
        return false;
    if (!m_refusedAt.isValid() || m_refusedAt.elapsed() >= kRefuseGapMs)
    {
        m_refusedAt.start();
        emit refused(QObject::tr("Encoders are off in Operate -- switch to Design to edit."));
    }
    return true;
}

void EncoderBank::queue(int slot, int detents, bool fine)
{
    if (m_pending[slot] != 0 && m_pendingFine[slot] != fine)
        flush();
    m_pendingFine[slot] = fine;
    m_pending[slot] = qBound(-127, m_pending[slot] + detents, 127);
    if (m_timer.interval() <= 0)
        flush();
    else if (!m_timer.isActive())
    {
        m_idleTicks = 0;
        m_timer.start();
    }
}

void EncoderBank::slotTick()
{
    bool any = false;
    for (int i = 0; i < slotsPerBank(); ++i)
        any |= m_pending[i] != 0;
    if (any)
    {
        m_idleTicks = 0;
        flush();
    }
    else if (++m_idleTicks >= 10)
    {
        m_timer.stop();
    }
}

void EncoderBank::flush()
{
    for (int i = 0; i < slotsPerBank(); ++i)
    {
        const int d = m_pending[i];
        if (d == 0)
            continue;
        m_pending[i] = 0;
        if (d != 0)
            emit adjusted(m_bank, i, d, m_pendingFine[i]);
    }
}

/*****************************************************************************
 * Feedback
 *****************************************************************************/

void EncoderBank::send(quint32 channel, uchar value)
{
    emit feedbackSent(m_device.universe, channel, value);
    if (InputOutputMap *iom = m_doc->inputOutputMap())
        iom->sendFeedBack(m_device.universe, channel, value, QVariant());
}

void EncoderBank::setRing(int slot, float norm)
{
    if (!m_enabled || m_device.rings < 0 || slot < 0 || slot >= m_device.encoderCount)
        return;
    const int value = norm < 0.0f ? 0 : qBound(0, int(norm * 255.0f + 0.5f), 255);
    if (m_ring[slot] == value)
        return;
    m_ring[slot] = value;
    send(quint32(m_device.rings + slot), uchar(value));
}

void EncoderBank::clearRings()
{
    for (int i = 0; i < m_device.encoderCount; ++i)
        setRing(i, -1.0f);
}

void EncoderBank::sendBankLamps()
{
    if (!m_enabled || m_device.bankLamps < 0)
        return;
    for (int b = 0; b < bankCount(); ++b)
        send(quint32(m_device.bankLamps + b), b == m_bank ? UCHAR_MAX : 0);
}

void EncoderBank::resendFeedback()
{
    for (int i = 0; i < slotsPerBank(); ++i)
        m_ring[i] = -1;
    sendBankLamps();
    emit ringsWanted();
}

/*****************************************************************************
 * Load & Save
 *****************************************************************************/

bool EncoderBank::saveXML(QXmlStreamWriter *doc) const
{
    if (doc == nullptr)
        return false;
    if (m_device.encoders < 0)
        return true;    // never set up: a show without one saves as before
    doc->writeStartElement(KXMLQLCEncoderBank);
    doc->writeAttribute(KXMLName, m_device.name);
    if (!m_presetKey.isEmpty())
        doc->writeAttribute(KXMLPreset, m_presetKey);
    doc->writeAttribute(KXMLUniverse, QString::number(m_device.universe));
    doc->writeAttribute(KXMLEncoding, encodingToString(m_device.encoding));
    if (m_device.invert)
        doc->writeAttribute(KXMLInvert, QStringLiteral("1"));
    if (!m_enabled)
        doc->writeAttribute(QStringLiteral("Off"), QStringLiteral("1"));
    doc->writeAttribute(KXMLCount, QString::number(m_device.encoderCount));
    auto put = [doc](const QString &name, qint32 channel) {
        if (channel >= 0)
            doc->writeAttribute(name, QString::number(channel));
    };
    put(KXMLTurns, m_device.encoders);
    put(KXMLPushes, m_device.pushes);
    put(KXMLRings, m_device.rings);
    put(KXMLNext, m_device.bankNext);
    put(KXMLPrevious, m_device.bankPrevious);
    put(KXMLDirect, m_device.bankDirect);
    put(KXMLLamps, m_device.bankLamps);
    doc->writeEndElement();
    return true;
}

bool EncoderBank::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCEncoderBank)
    {
        qWarning() << Q_FUNC_INFO << "Encoder bank node not found";
        return false;
    }
    const QXmlStreamAttributes attrs = root.attributes();
    auto get = [&attrs](const QString &name) -> qint32 {
        bool ok = false;
        const qint32 v = attrs.value(name).toInt(&ok);
        return ok && v >= 0 ? v : -1;
    };
    Device d;
    d.name = attrs.value(KXMLName).toString();
    d.universe = attrs.value(KXMLUniverse).toUInt();
    d.encoding = stringToEncoding(attrs.value(KXMLEncoding).toString());
    d.invert = attrs.value(KXMLInvert) == QLatin1String("1");
    d.encoderCount = qBound(1, attrs.value(KXMLCount).toInt(), slotsPerBank());
    d.encoders = get(KXMLTurns);
    d.pushes = get(KXMLPushes);
    d.rings = get(KXMLRings);
    d.bankNext = get(KXMLNext);
    d.bankPrevious = get(KXMLPrevious);
    d.bankDirect = get(KXMLDirect);
    d.bankLamps = get(KXMLLamps);
    const bool off = attrs.value(QStringLiteral("Off")) == QLatin1String("1");
    const QString key = attrs.value(KXMLPreset).toString();
    root.skipCurrentElement();

    reset();
    m_device = d;
    m_presetKey = key;
    m_enabled = d.encoders >= 0 && !off;
    updateConnection();
    emit deviceChanged();
    if (m_enabled)
        resendFeedback();
    return true;
}
