/*
  Q Light Controller Plus - qlcconsole
  surfaceboardlog.cpp

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

#include <QCoreApplication>
#include <QDateTime>
#include <initializer_list>

#include "surfaceboardlog.h"
#include "inputoutputmap.h"
#include "qlcinputchannel.h"
#include "qlcinputprofile.h"
#include "qlcioplugin.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "doc.h"

namespace
{
// The MIDI plugin's channel numbering (plugins/midi/src/common/midiprotocol.h).
constexpr quint32 kNote = 128;
constexpr quint32 kNoteMax = 255;
constexpr quint32 kAftertouch = 256;
constexpr quint32 kAftertouchMax = 383;
constexpr quint32 kProgram = 384;
constexpr quint32 kProgramMax = 511;
constexpr quint32 kChannelPressure = 512;
constexpr quint32 kPitchBend = 513;

QString hexBytes(const QByteArray &b, int max = 24)
{
    QStringList out;
    for (int i = 0; i < b.size() && i < max; ++i)
        out << QStringLiteral("%1").arg(uchar(b.at(i)), 2, 16, QLatin1Char('0')).toUpper();
    if (b.size() > max)
        out << QStringLiteral("…");
    return out.join(QLatin1Char(' '));
}
}

SurfaceBoardLog::SurfaceBoardLog(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    connect(iom, &InputOutputMap::feedbackSent, this,
            [this](quint32 universe, quint32 channel, uchar value, const QVariant &params) {
                const int profileCh = params.isValid() ? params.toInt() : -1;
                const QByteArray b = midiBytes(channel, value, profileCh >= 0 ? profileCh : -1);
                QString t = b.isEmpty() ? tr("channel %1 = %2").arg(channel).arg(value) : decode(b);
                const QString name = controlName(universe, channel);
                if (!name.isEmpty())
                    t += QStringLiteral(" (%1)").arg(name);
                add(universe, Out, b, t);
            });
    connect(iom, &InputOutputMap::inputSysExReceived, this, [this](quint32 universe, const QByteArray &sysEx) {
        add(universe, In, sysEx, decode(sysEx));
    });
    connect(iom, &InputOutputMap::inputPatchChanged, this, [this](quint32 universe) {
        InputPatch *ip = m_doc->inputOutputMap()->inputPatch(universe);
        if (ip == nullptr || ip->plugin() == nullptr)
            return;
        watchPlugin(ip->plugin());
        note(universe, tr("Input: %1, profile %2").arg(ip->inputName(),
                         ip->profileName().isEmpty() ? tr("none") : ip->profileName()));
    });
    connect(iom, &InputOutputMap::profileChanged, this, [this](quint32 universe, const QString &profile) {
        note(universe, tr("Profile: %1").arg(profile.isEmpty() ? tr("none") : profile));
    });
    connect(iom, &InputOutputMap::inputDeviceConnected, this,
            [this](quint32 universe, const QString &, const QString &device) {
                note(universe, tr("Found again: %1").arg(device));
            });
    connect(iom, &InputOutputMap::inputDeviceDisconnected, this,
            [this](quint32 universe, const QString &, const QString &device) {
                note(universe, tr("Gone (unplugged or asleep): %1").arg(device));
            });
    for (quint32 u = 0; u < iom->universesCount(); ++u)
        if (InputPatch *ip = iom->inputPatch(u))
            if (ip->plugin() != nullptr)
                watchPlugin(ip->plugin());
}

void SurfaceBoardLog::watchPlugin(QLCIOPlugin *plugin)
{
    if (plugin == nullptr || !plugin->linesAreHardware())
        return;
    for (const QPointer<QLCIOPlugin> &p : std::as_const(m_watched))
        if (p == plugin)
            return;
    m_watched << plugin;
    /* From the plugin's own thread (CoreMIDI's): queued here, every value
       as it came, before the input patch gathers them into one per frame. */
    QPointer<QLCIOPlugin> guard(plugin);
    connect(plugin, &QLCIOPlugin::valueChanged, this,
            [this, guard](quint32, quint32 input, quint32 channel, uchar value, const QString &) {
                if (guard != nullptr)
                    slotRawInput(guard, input, channel, value);
            });
}

void SurfaceBoardLog::slotRawInput(QLCIOPlugin *plugin, quint32 input, quint32 channel, uchar value)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        if (ip == nullptr || ip->plugin() != plugin || ip->input() != input)
            continue;
        const QByteArray b = midiBytes(channel, value);
        QString t = b.isEmpty() ? tr("channel %1 = %2").arg(channel).arg(value) : decode(b);
        const QString name = controlName(u, channel);
        if (!name.isEmpty())
            t += QStringLiteral(" (%1)").arg(name);
        add(u, In, b, t);
    }
}

QString SurfaceBoardLog::controlName(quint32 universe, quint32 channel) const
{
    InputPatch *ip = m_doc->inputOutputMap()->inputPatch(universe);
    QLCInputProfile *p = ip != nullptr ? ip->profile() : nullptr;
    if (p == nullptr)
        return QString();
    QLCInputChannel *ch = p->channel(channel);
    if (ch == nullptr && (channel >> 12) != 0)
        ch = p->channel(channel & 0x0FFF);
    return ch != nullptr ? ch->name() : QString();
}

void SurfaceBoardLog::add(quint32 universe, Direction dir, const QByteArray &bytes, const QString &text)
{
    QList<Entry> &list = m_entries[universe];
    Entry e;
    e.msecs = QDateTime::currentMSecsSinceEpoch();
    e.dir = dir;
    e.bytes = bytes;
    e.text = text;
    list << e;
    while (list.size() > kSize)
        list.removeFirst();
    emit logged(universe);
}

void SurfaceBoardLog::note(quint32 universe, const QString &text)
{
    add(universe, Note, QByteArray(), text);
}

void SurfaceBoardLog::logSysExOut(quint32 universe, const QByteArray &message, bool sent, const QString &why)
{
    QString t = decode(message);
    if (!sent)
        t += QStringLiteral(" -- ") + (why.isEmpty() ? tr("NOT SENT") : tr("NOT SENT: %1").arg(why));
    else if (!why.isEmpty())
        t += QStringLiteral(" -- ") + why;
    add(universe, Out, message, t);
}

void SurfaceBoardLog::clear(quint32 universe)
{
    m_entries.remove(universe);
    emit logged(universe);
}

QByteArray SurfaceBoardLog::midiBytes(quint32 channel, uchar value, int midiChannel)
{
    const int mc = (midiChannel >= 0 ? midiChannel : int(channel >> 12)) & 0x0F;
    const quint32 c = channel & 0x0FFF;
    const char v = char(value >> 1);
    QByteArray b;
    auto put = [&b](std::initializer_list<char> l) { for (char x : l) b.append(x); };
    if (c < kNote)
        put({char(0xB0 | mc), char(c), v});
    else if (c <= kNoteMax)
        put({char((value == 0 ? 0x80 : 0x90) | mc), char(c - kNote), v});
    else if (c >= kAftertouch && c <= kAftertouchMax)
        put({char(0xA0 | mc), char(c - kAftertouch), v});
    else if (c >= kProgram && c <= kProgramMax)
        put({char(0xC0 | mc), char(c - kProgram)});
    else if (c == kChannelPressure)
        put({char(0xD0 | mc), v});
    else if (c == kPitchBend)
        put({char(0xE0 | mc), char((value & 0x01) << 6), v});
    return b;
}

QString SurfaceBoardLog::decode(const QByteArray &b)
{
    if (b.isEmpty())
        return QString();
    const uchar s = uchar(b.at(0));
    if (s == 0xF0)
        return tr("SysEx (%n byte(s))", "", int(b.size()));
    const int ch = (s & 0x0F) + 1;
    const int d1 = b.size() > 1 ? uchar(b.at(1)) : 0;
    const int d2 = b.size() > 2 ? uchar(b.at(2)) : 0;
    switch (s & 0xF0)
    {
        case 0x80: return tr("Note %1 off, ch %2").arg(d1).arg(ch);
        case 0x90: return d2 == 0 ? tr("Note %1 off (on at 0), ch %2").arg(d1).arg(ch)
                                  : tr("Note %1 on, ch %2, velocity %3").arg(d1).arg(ch).arg(d2);
        case 0xA0: return tr("Aftertouch note %1, ch %2 = %3").arg(d1).arg(ch).arg(d2);
        case 0xB0: return tr("CC %1, ch %2 = %3").arg(d1).arg(ch).arg(d2);
        case 0xC0: return tr("Program %1, ch %2").arg(d1).arg(ch);
        case 0xD0: return tr("Channel pressure, ch %1 = %2").arg(ch).arg(d1);
        case 0xE0: return tr("Pitch bend, ch %1 = %2 of 16383").arg(ch).arg((d2 << 7) | d1);
        default: return tr("System message");
    }
}

QString SurfaceBoardLog::entryText(const Entry &e)
{
    const QString when = QDateTime::fromMSecsSinceEpoch(e.msecs).toString(QStringLiteral("HH:mm:ss.zzz"));
    const QString dir = e.dir == In ? QStringLiteral("IN  ") : e.dir == Out ? QStringLiteral("OUT ")
                                                                           : QStringLiteral("--  ");
    const QString bytes = hexBytes(e.bytes);
    return QStringLiteral("%1  %2 %3  %4").arg(when, dir, bytes.leftJustified(9), e.text);
}

QString SurfaceBoardLog::text(quint32 universe) const
{
    QStringList out;
    InputOutputMap *iom = m_doc->inputOutputMap();
    InputPatch *ip = iom->inputPatch(universe);
    OutputPatch *fb = iom->feedbackPatch(universe);
    out << tr("Board log, universe %1, %2").arg(universe + 1)
               .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    out << tr("App: %1 %2").arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion());
    out << tr("Input: %1 (%2)").arg(ip != nullptr ? ip->inputName() : tr("none"),
                                    ip != nullptr && ip->plugin() != nullptr ? ip->plugin()->name() : QString());
    out << tr("Profile: %1").arg(ip != nullptr && !ip->profileName().isEmpty() ? ip->profileName() : tr("none"));
    out << tr("Feedback: %1").arg(fb != nullptr && fb->isPatched() ? fb->outputName() : tr("none"));
    out << QString();
    const QList<Entry> list = m_entries.value(universe);
    if (list.isEmpty())
        out << tr("Nothing yet: move or press something on the board.");
    for (const Entry &e : list)
        out << entryText(e);
    return out.join(QLatin1Char('\n'));
}
