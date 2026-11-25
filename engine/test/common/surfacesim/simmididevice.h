/*
  Q Light Controller Plus
  simmididevice.h

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

#ifndef SIMMIDIDEVICE_H
#define SIMMIDIDEVICE_H

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

#include "simsurface.h"
#include "midiprotocol.h"
#include "iopluginstub.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include "qlcinputchannel.h"
#include "qlcinputprofile.h"
#include "qlcioplugin.h"
#include "doc.h"

/**
 * A MIDI control surface that is not there (PMJ Black 1, APC mini...).
 *
 * In: the bytes the board sends (note on/off, CC, velocity, its real MIDI
 * channel) go through the MIDI plugin's own decoder,
 * QLCMIDIProtocol::midiToInput(), in OMNI mode as the plugin's line is for
 * a profile whose channels carry the MIDI channel -- and leave the plugin
 * as MidiPlugin::slotValueChanged() sends them: valueChanged(UINT_MAX,
 * line, channel, value) of a QLCIOPlugin (the I/O plugin stub, its lines
 * named as the system names the device, "hardware" lines as a MIDI
 * plugin's are). Everything after that -- the input patch, its buffer and
 * flush, the profile, every listener -- is the app's own.
 *
 * Out: what the app sends to the board's feedback line
 * (QLCIOPlugin::sendFeedBack(), as MidiPlugin gets it) is turned into the
 * MIDI bytes MidiPlugin would write, through
 * QLCMIDIProtocol::feedbackToMidi(); lamp() is a control's last velocity.
 *
 * Controls are named as the profile names them ("Go", "Master"); a number
 * ("#32896") names a channel the profile does not.
 */
class SimMidiDevice final : public SimSurface
{
public:
    static constexpr uchar kOmni = MAX_MIDI_CHANNELS;

    /** One MIDI message the app sent the board. */
    struct Out
    {
        quint32 channel = 0;    ///< the QLC+ channel it was sent for
        uchar value = 0;        ///< as the app sent it (0..255)
        QByteArray midi;        ///< the bytes MidiPlugin writes for it
    };

    /** @p profile: the input profile's name ("PMJ Black 1"), loaded into
     *  the map from @p profileFile if the map has none of that name. */
    /** @p nth: the second of two identical boards (one name) is 1. */
    SimMidiDevice(Doc *doc, IOPluginStub *stub, const QString &name, quint32 universe,
                  const QString &profile = QString(), const QString &profileFile = QString(),
                  int nth = 0)
        : m_doc(doc), m_stub(stub), m_name(name), m_universe(universe), m_profileName(profile), m_nth(nth)
    {
        InputOutputMap *iom = m_doc->inputOutputMap();
        if (!profile.isEmpty() && iom->profile(profile) == nullptr && !profileFile.isEmpty())
        {
            QLCInputProfile *p = QLCInputProfile::loader(profileFile);
            if (p != nullptr && !iom->addProfile(p))
                delete p;
        }
    }

    QString deviceName() const override { return m_name; }
    quint32 universe() const { return m_universe; }
    /** The plugin line the system lists it on now (-1: not listed). */
    int line() const
    {
        int seen = 0;
        for (int i = 0; i < m_stub->m_lineNames.size(); ++i)
            if (m_stub->m_lineNames.at(i) == m_name && seen++ == m_nth)
                return i;
        return -1;
    }
    QLCInputProfile *profile() const
    {
        return m_profileName.isEmpty() ? nullptr : m_doc->inputOutputMap()->profile(m_profileName);
    }

    /** Listed by the system and patched as a person patches it: input
     *  with its profile, and its feedback line. */
    bool plug()
    {
        m_stub->m_linesAreHardware = true;
        m_stub->m_networkDmx = false;
        m_stub->m_lineNames.removeAll(QStringLiteral("(no device)"));
        while (line() < 0)
            m_stub->m_lineNames << m_name;
        InputOutputMap *iom = m_doc->inputOutputMap();
        while (iom->universesCount() <= m_universe)
            if (!iom->addUniverse())
                return fail(QStringLiteral("no universe %1").arg(m_universe + 1));
        if (!m_profileName.isEmpty() && iom->profile(m_profileName) == nullptr)
            return fail(QStringLiteral("no input profile '%1'").arg(m_profileName));
        if (!iom->setInputPatch(m_universe, m_stub->name(), m_name, quint32(line()), m_profileName))
            return fail(QStringLiteral("its input did not patch"));
        if (!iom->setOutputPatch(m_universe, m_stub->name(), m_name, quint32(line()), true))
            return fail(QStringLiteral("its feedback line did not patch"));
        m_seen = int(m_stub->m_feedback.size());
        settle();
        return true;
    }

    /* ---- in ---- */

    /** @p midiChannel 1..16, as the board's manual counts them. */
    bool noteOn(int midiChannel, int note, int velocity)
    {
        return raw(bytes(0x90, midiChannel, note, velocity));
    }
    bool noteOff(int midiChannel, int note, int velocity = 0)
    {
        return raw(bytes(0x80, midiChannel, note, velocity));
    }
    bool cc(int midiChannel, int controller, int value)
    {
        return raw(bytes(0xB0, midiChannel, controller, value));
    }

    /** MIDI messages exactly as the board sends them (no running status:
     *  these boards send each status byte). */
    bool raw(const QByteArray &midi) override
    {
        if (line() < 0)
            return fail(QStringLiteral("sent while unplugged"));
        int i = 0;
        while (i < midi.size())
        {
            const uchar cmd = uchar(midi.at(i));
            if (!MIDI_IS_CMD(cmd))
                return fail(QStringLiteral("byte %1 (0x%2) is not a status byte").arg(i).arg(cmd, 2, 16, QLatin1Char('0')));
            const int len = messageLength(cmd);
            if (i + len > midi.size())
                return fail(QStringLiteral("a short message at byte %1").arg(i));
            const uchar d1 = len > 1 ? uchar(midi.at(i + 1)) : 0;
            const uchar d2 = len > 2 ? uchar(midi.at(i + 2)) : 0;
            quint32 channel = 0;
            uchar value = 0;
            // The plugin's own decoder, as CoreMIDI / ALSA / WinMM's readers call it.
            if (QLCMIDIProtocol::midiToInput(cmd, d1, d2, kOmni, &channel, &value))
            {
                m_sent << qMakePair(channel, value);
                // As MidiPlugin::slotValueChanged(): any universe, this line.
                m_stub->emitValueChanged(UINT_MAX, quint32(line()), channel, value);
            }
            i += len;
        }
        settle();
        return true;
    }

    /** @p control at @p value (0..127), as the board sends that control:
     *  a note on (0: note off) or a CC on its own MIDI channel. */
    bool set(const QString &control, int value) override
    {
        bool ok = false;
        const quint32 ch = channelOf(control, &ok);
        if (!ok)
            return fail(QStringLiteral("no control '%1'").arg(control));
        return raw(bytesFor(ch, value));
    }
    int top(const QString &control) const override { Q_UNUSED(control) return 127; }

    /** The MIDI message the board sends for QLC+ channel @p channel at
     *  @p value (0..127): the inverse of the plugin's decoder (a note at 0
     *  is a note off, as these boards send it). */
    static QByteArray bytesFor(quint32 channel, int value)
    {
        const int midiCh = int(channel >> 12) & 0x0F;     // OMNI: the board's own channel
        const quint32 c = channel & 0x0FFF;
        const char v = char(qBound(0, value, 127));
        QByteArray out;
        // The MIDI clock's messages carry no channel.
        if (c == CHANNEL_OFFSET_MBC_BEAT)
            return QByteArray(1, char(MIDI_BEAT_CLOCK));
        if (c == CHANNEL_OFFSET_MBC_PLAYBACK)
            return QByteArray(1, char(MIDI_BEAT_START));
        if (c == CHANNEL_OFFSET_MBC_STOP)
            return QByteArray(1, char(MIDI_BEAT_STOP));
        uchar cmd = 0, d1 = 0, d2 = 0;
        if (!QLCMIDIProtocol::feedbackToMidi(c, 0, uchar(midiCh), true, &cmd, &d1, &d2))
            return QByteArray();
        out.append(char(cmd));
        switch (MIDI_CMD(cmd))
        {
            case MIDI_PROGRAM_CHANGE:       // the program is the channel
                out.append(char(c - CHANNEL_OFFSET_PROGRAM_CHANGE));
                break;
            case MIDI_CHANNEL_AFTERTOUCH:   // the pressure is the value
                out.append(v);
                break;
            case MIDI_PITCH_WHEEL:          // LSB, MSB
                out.append(char(0));
                out.append(v);
                break;
            case MIDI_NOTE_OFF:
                if (value > 0)
                    out[0] = char(MIDI_NOTE_ON | midiCh);
                out.append(char(d1));
                out.append(v);
                break;
            default:
                out.append(char(d1));
                out.append(v);
                break;
        }
        return out;
    }

    /** The QLC+ channel of @p control: the profile's channel of that name,
     *  or "#<number>". */
    quint32 channelOf(const QString &control, bool *ok = nullptr) const
    {
        if (ok) *ok = false;
        if (control.startsWith(QLatin1Char('#')))
        {
            bool isNum = false;
            const quint32 n = control.mid(1).toUInt(&isNum);
            if (ok) *ok = isNum;
            return n;
        }
        QLCInputProfile *p = profile();
        if (p == nullptr)
            return 0;
        const QMap<quint32, QLCInputChannel *> chans = p->channels();
        for (auto it = chans.constBegin(); it != chans.constEnd(); ++it)
            if (it.value()->name().compare(control, Qt::CaseInsensitive) == 0)
            {
                if (ok) *ok = true;
                return it.key();
            }
        return 0;
    }
    QString nameOf(quint32 channel) const
    {
        QLCInputProfile *p = profile();
        QLCInputChannel *c = p == nullptr ? nullptr : p->channel(channel);
        return c == nullptr ? QStringLiteral("#%1").arg(channel) : c->name();
    }

    /** Every (channel, value) the plugin's decoder made of what was sent. */
    QList<QPair<quint32, uchar>> decoded() const { return m_sent; }

    /* ---- out ---- */

    /** The MIDI channel the feedback line is set to (0..15, or kOmni), as
     *  the MIDI plugin's Configure dialog keeps it per device (a Mac with
     *  no saved setting: channel 1, MidiDevice::loadSettings()). */
    void setFeedbackLineChannel(int channel) { m_lineChannel = channel; }

    /** What the app sent the board since the last take, in order. */
    QList<Out> takeOutput()
    {
        settle();
        QList<Out> out;
        const int n = int(m_stub->m_feedback.size());
        for (int i = m_seen; i < n; ++i)
        {
            if (int(m_stub->m_feedbackLines.value(i)) != line())
                continue;
            Out o;
            o.channel = m_stub->m_feedback.at(i).first;
            o.value = m_stub->m_feedback.at(i).second;
            // As MidiPlugin::sendFeedBack(): the line's MIDI channel (OMNI:
            // the one the channel carries), note off for 0 when the profile says.
            uchar cmd = 0, d1 = 0, d2 = 0;
            const bool noteOff = profile() == nullptr || profile()->midiSendNoteOff();
            const QVariant params = m_stub->m_feedbackParams.value(i);
            /* As MidiPlugin::sendFeedBack(), by its own rule: the feedback
               line's MIDI channel (setFeedbackLineChannel(): a Mac's MIDI
               output with no saved setting is channel 1), unless the
               control names its own. */
            const int midiChannel = QLCMIDIProtocol::feedbackMidiChannel(
                o.channel, m_lineChannel, params.isValid() ? params.toInt() : -1);
            if (QLCMIDIProtocol::feedbackToMidi(o.channel, o.value, uchar(midiChannel), noteOff, &cmd, &d1, &d2))
            {
                o.midi.append(char(cmd));
                o.midi.append(char(d1));
                if (messageLength(cmd) > 2)
                    o.midi.append(char(d2));
            }
            boardReceives(o.midi);
            out << o;
            m_lamps.insert(o.channel, o.midi.size() > 2 ? int(uchar(o.midi.at(2))) : -1);
            m_lastMidi.insert(o.channel, o.midi);
            m_lampCount[o.channel]++;
        }
        m_seen = n;
        return out;
    }
    /** @p control's lamp as the board shows it: the velocity last sent
     *  (0..127), -1 if the app never sent it one. */
    int lamp(const QString &control)
    {
        bool ok = false;
        const quint32 ch = channelOf(control, &ok);
        return ok ? lamp(ch) : -1;
    }
    int lamp(quint32 channel)
    {
        takeOutput();
        return m_lamps.value(channel, -1);
    }
    /** Every lamp sent so far: channel -> its last velocity. */
    QMap<quint32, int> lamps()
    {
        takeOutput();
        return m_lamps;
    }
    /** How many messages @p channel's lamp got (a lamp sent dark and lit
     *  again on every refresh flickers). */
    int lampMessages(quint32 channel)
    {
        takeOutput();
        return m_lampCount.value(channel);
    }
    /** The MIDI bytes last sent for @p channel's lamp (empty: none). */
    QByteArray lastMidi(quint32 channel)
    {
        takeOutput();
        return m_lastMidi.value(channel);
    }
    void forgetLamps()
    {
        takeOutput();
        m_lastMidi.clear();
        m_lamps.clear();
        m_lampCount.clear();
    }

    /* ---- the board's own LEDs ---- */

    /** One LED of the board: it answers @c note on MIDI channel @c channel
     *  (1..16) when @c midi, and sits under button @c under (a profile
     *  name; empty: under none). */
    struct Led
    {
        int index = 0;
        int channel = 1;
        int note = 0;
        bool midi = true;
        QString under;
    };
    /** The board's LED table, as OpenDeck stores it: which message each
     *  LED answers. Without one (the default) lamp() is what was sent. */
    void setLeds(const QList<Led> &leds)
    {
        m_leds = leds;
        m_ledLevel.clear();
    }
    QList<Led> leds() const { return m_leds; }
    /** An LED table from a JSON file (engine/test/surfacesim/captures/
     *  pmj-leds-*.json: {"leds": [{"led", "ch", "note", "midi", "under"}]}). */
    static QList<Led> loadLeds(const QString &file, QString *error = nullptr)
    {
        QList<Led> out;
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly))
        {
            if (error) *error = QStringLiteral("cannot read %1").arg(file);
            return out;
        }
        const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("leds")).toArray();
        for (const QJsonValue &v : arr)
        {
            const QJsonObject o = v.toObject();
            Led l;
            l.index = o.value(QStringLiteral("led")).toInt();
            l.channel = o.value(QStringLiteral("ch")).toInt(1);
            l.note = o.value(QStringLiteral("note")).toInt();
            l.midi = o.value(QStringLiteral("midi")).toBool(true);
            l.under = o.value(QStringLiteral("under")).toString();
            out << l;
        }
        if (out.isEmpty() && error) *error = QStringLiteral("no LEDs in %1").arg(file);
        return out;
    }
    /** @p leds as `qlc-midi opendeck align` leaves them: each LED under a
     *  button answers that button's own note on its own MIDI channel. */
    QList<Led> aligned(QList<Led> leds) const
    {
        for (Led &l : leds)
        {
            if (l.under.isEmpty())
                continue;
            bool ok = false;
            const quint32 ch = channelOf(l.under, &ok);
            if (!ok || (ch & 0x0FFF) < CHANNEL_OFFSET_NOTE || (ch & 0x0FFF) > CHANNEL_OFFSET_NOTE_MAX)
                continue;
            l.channel = int(ch >> 12) + 1;
            l.note = int((ch & 0x0FFF) - CHANNEL_OFFSET_NOTE);
            l.midi = true;
        }
        return leds;
    }
    /** MIDI the board receives (what the app sent, or a test's own): each
     *  LED that answers a note takes its velocity (a note off: 0). */
    void boardReceives(const QByteArray &midi)
    {
        if (m_leds.isEmpty() || midi.size() < 3)
            return;
        const uchar cmd = uchar(midi.at(0));
        if (MIDI_CMD(cmd) != MIDI_NOTE_ON && MIDI_CMD(cmd) != MIDI_NOTE_OFF)
            return;
        const int ch = MIDI_CH(cmd) + 1;
        const int note = uchar(midi.at(1));
        const int vel = MIDI_CMD(cmd) == MIDI_NOTE_OFF ? 0 : uchar(midi.at(2));
        for (const Led &l : std::as_const(m_leds))
            if (l.midi && l.channel == ch && l.note == note)
                m_ledLevel.insert(l.index, vel);
    }
    /** The LED under @p button as the board shows it: its last velocity, 0
     *  if never sent one, -1 if no LED is under that button. */
    int lampUnder(const QString &button)
    {
        takeOutput();
        for (const Led &l : std::as_const(m_leds))
            if (l.under.compare(button, Qt::CaseInsensitive) == 0)
                return m_ledLevel.value(l.index, 0);
        return -1;
    }

    /* ---- the cable ---- */

    bool unplug() override
    {
        const int l = line();
        if (l < 0)
            return fail(QStringLiteral("unplugged while not listed"));
        m_stub->m_lineNames.removeAt(l);
        if (m_stub->m_lineNames.isEmpty())
            m_stub->m_lineNames << QStringLiteral("(no device)");     // none: not the stub's 4 lines
        takeOutput();
        emit m_stub->configurationChanged();
        settle();
        return true;
    }
    bool replug(const QList<QPair<QString, int>> &dump = {}) override
    {
        m_stub->m_lineNames.removeAll(QStringLiteral("(no device)"));
        while (line() < 0)
            m_stub->m_lineNames << m_name;
        m_seen = int(m_stub->m_feedback.size());
        emit m_stub->configurationChanged();
        settle();
        // The state it sends the moment it is back, before anyone touches it.
        for (const auto &d : dump)
            if (!set(d.first, d.second))
                return false;
        return true;
    }

    void settle() override
    {
        simFlushInputs(m_doc->inputOutputMap());
    }

    static int messageLength(uchar cmd)
    {
        switch (MIDI_CMD(cmd))
        {
            case MIDI_PROGRAM_CHANGE:
            case MIDI_CHANNEL_AFTERTOUCH:
                return 2;
            default:
                break;
        }
        if (MIDI_IS_SYSCOMMON(cmd))
            return 1;
        return 3;
    }

private:
    static QByteArray bytes(uchar status, int midiChannel, int d1, int d2)
    {
        QByteArray b;
        b.append(char(status | uchar(qBound(1, midiChannel, 16) - 1)));
        b.append(char(d1 & 0x7F));
        b.append(char(d2 & 0x7F));
        return b;
    }

    Doc *m_doc;
    IOPluginStub *m_stub;
    QString m_name;
    quint32 m_universe;
    QString m_profileName;
    int m_nth = 0;
    int m_seen = 0;
    QList<QPair<quint32, uchar>> m_sent;
    QMap<quint32, int> m_lamps;
    QMap<quint32, int> m_lampCount;
    QMap<quint32, QByteArray> m_lastMidi;
    int m_lineChannel = kOmni;
    QList<Led> m_leds;
    QMap<int, int> m_ledLevel;
};

#endif
