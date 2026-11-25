/*
  Q Light Controller Plus
  opendecklink.cpp

  Copyright (C) Branson Matheson

  Ported from qlcplus-midi-profiler (qlcprofiler/opendeck.py, Apache-2.0).

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

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QEventLoop>
#include <QSaveFile>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QFileInfo>

#include "opendecklink.h"

namespace
{
const char kId[] = { 0x00, 0x53, 0x43 };
// F0 + the id + status: the reply's status byte, then the echoed request.
const int kStatusAt = 4;
const int kValuesAt = 14;
const int kPage = 32;
}

OpenDeckLink::OpenDeckLink(Send send, int timeoutMs)
    : m_send(std::move(send))
    , m_timeoutMs(timeoutMs)
{
}

OpenDeckLink::~OpenDeckLink()
{
    if (m_loop != nullptr)
        m_loop->quit();
}

/****************************************************************************
 * The wire format
 ****************************************************************************/

bool OpenDeckLink::isOpenDeck(const QByteArray &sysEx)
{
    return sysEx.size() > kStatusAt && uchar(sysEx.at(0)) == 0xF0
        && sysEx.at(1) == kId[0] && sysEx.at(2) == kId[1] && sysEx.at(3) == kId[2];
}

QByteArray OpenDeckLink::request(int part, int wish, int amount, int block, int section,
                                 int index, int value)
{
    QByteArray m;
    m.append(char(0xF0));
    m.append(kId, 3);
    m.append(char(StatusRequest));
    m.append(char(part & 0x7F));
    m.append(char(wish & 0x7F));
    m.append(char(amount & 0x7F));
    m.append(char(block & 0x7F));
    m.append(char(section & 0x7F));
    m.append(char((index >> 7) & 0x7F));
    m.append(char(index & 0x7F));
    m.append(char((value >> 7) & 0x7F));
    m.append(char(value & 0x7F));
    m.append(char(0xF7));
    return m;
}

QByteArray OpenDeckLink::special(int wish)
{
    QByteArray m;
    m.append(char(0xF0));
    m.append(kId, 3);
    m.append(char(StatusRequest));
    m.append(char(0x00));
    m.append(char(wish & 0x7F));
    m.append(char(0xF7));
    return m;
}

int OpenDeckLink::replyStatus(const QByteArray &sysEx)
{
    return isOpenDeck(sysEx) ? int(uchar(sysEx.at(kStatusAt))) : -1;
}

QList<int> OpenDeckLink::replyValues(const QByteArray &sysEx)
{
    QList<int> out;
    int end = sysEx.size();
    if (end > 0 && uchar(sysEx.at(end - 1)) == 0xF7)
        --end;
    for (int i = kValuesAt; i + 1 < end; i += 2)
        out << ((int(uchar(sysEx.at(i))) << 7) | int(uchar(sysEx.at(i + 1))));
    return out;
}

QString OpenDeckLink::statusName(int status)
{
    switch (status)
    {
        case -1: return QStringLiteral("no answer");
        case 0x01: return QStringLiteral("ack");
        case 0x02: return QStringLiteral("status error");
        case 0x03: return QStringLiteral("handshake error");
        case 0x04: return QStringLiteral("wish not supported");
        case 0x05: return QStringLiteral("amount not supported");
        case 0x06: return QStringLiteral("block not supported");
        case 0x07: return QStringLiteral("section not supported");
        case 0x08: return QStringLiteral("part not supported");
        case 0x09: return QStringLiteral("index out of range");
        case 0x0A: return QStringLiteral("value out of range");
        case 0x0B: return QStringLiteral("message length error");
        case 0x0C: return QStringLiteral("write error");
        case 0x0D: return QStringLiteral("not supported");
        case 0x0E: return QStringLiteral("read error");
        default: return QStringLiteral("status %1").arg(status);
    }
}

/****************************************************************************
 * The session
 ****************************************************************************/

void OpenDeckLink::deliver(const QByteArray &sysEx)
{
    if (!m_waiting || !isOpenDeck(sysEx) || sysEx.at(kStatusAt) == char(StatusRequest))
        return;
    // A config reply echoes the request (part, wish, amount, block, section).
    if (m_expect.size() > 10 && sysEx.size() > 10 && sysEx.mid(5, 5) != m_expect.mid(5, 5))
        return;
    m_reply = sysEx;
    m_waiting = false;
    if (m_loop != nullptr)
        m_loop->quit();
}

QByteArray OpenDeckLink::exchange(const QByteArray &msg)
{
    m_reply.clear();
    m_expect = msg;
    m_waiting = true;
    ++m_requests;
    if (!m_send || !m_send(msg))
    {
        m_waiting = false;
        m_error = QStringLiteral("the board's output could not be written");
        return QByteArray();
    }
    // A board on the same thread (the simulator) has answered already.
    if (m_waiting)
    {
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        m_loop = &loop;
        timer.start(m_timeoutMs);
        loop.exec(QEventLoop::ExcludeUserInputEvents);
        m_loop = nullptr;
    }
    if (m_waiting)
    {
        m_waiting = false;
        m_error = QStringLiteral("no answer");
        return QByteArray();
    }
    return m_reply;
}

bool OpenDeckLink::handshake()
{
    const QByteArray r = exchange(special(SpecialConnect));
    if (replyStatus(r) != StatusAck)
    {
        if (!r.isEmpty())
            m_error = statusName(replyStatus(r));
        return false;
    }
    return true;
}

QList<int> OpenDeckLink::readSection(int block, int section)
{
    QList<int> values;
    for (int part = 0; part < 32; ++part)
    {
        const QByteArray r = exchange(request(part, WishGet, AmountAll, block, section));
        if (replyStatus(r) != StatusAck)
            break;
        const QList<int> page = replyValues(r);
        if (page.isEmpty())
            break;
        values << page;
        if (page.size() < kPage)     // a short page is the last
            break;
    }
    return values;
}

int OpenDeckLink::writeSingle(int block, int section, int index, int value)
{
    if (value < 0 || value > 0x3FFF)
        return 0x0A;
    const QByteArray r = exchange(request(0, WishSet, AmountSingle, block, section, index, value));
    return replyStatus(r);
}

bool OpenDeckLink::writeChecked(int block, int section, int index, int value)
{
    const int status = writeSingle(block, section, index, value);
    if (status == StatusAck)
        return true;
    m_error = QStringLiteral("block %1 section %2 index %3 = %4: %5")
                  .arg(block).arg(section).arg(index).arg(value).arg(statusName(status));
    return false;
}

QList<OpenDeckLink::Led> OpenDeckLink::readLeds()
{
    QList<Led> leds;
    const QList<int> ids = readSection(LedBlock, LedActivationId);
    if (ids.isEmpty())
        return leds;
    const QList<int> channels = readSection(LedBlock, LedChannel);
    const QList<int> types = readSection(LedBlock, LedControlType);
    const QList<int> values = readSection(LedBlock, LedActivationValue);
    for (int i = 0; i < ids.size(); ++i)
    {
        Led l;
        l.index = i;
        l.note = ids.at(i);
        l.channel = channels.value(i, 1);
        l.controlType = types.value(i, 0);
        l.activationValue = values.value(i, 0);
        leds << l;
    }
    return leds;
}

bool OpenDeckLink::alignLed(int led, int note, int channel)
{
    // As the profiler's align: type, note, channel, full activation value.
    return writeChecked(LedBlock, LedControlType, led, MidiInNoteMultiVal)
        && writeChecked(LedBlock, LedActivationId, led, note)
        && writeChecked(LedBlock, LedChannel, led, channel)
        && writeChecked(LedBlock, LedActivationValue, led, 127);
}

OpenDeckLink::Tables OpenDeckLink::dumpAll(int maxBlock, int maxSection)
{
    Tables tables;
    for (int block = 0; block <= maxBlock; ++block)
        for (int section = 0; section <= maxSection; ++section)
        {
            const QList<int> values = readSection(block, section);
            if (!values.isEmpty())
                tables.insert(QStringLiteral("%1/%2").arg(block).arg(section), values);
        }
    return tables;
}

OpenDeckLink::RestoreResult OpenDeckLink::restoreAll(const Tables &tables)
{
    RestoreResult res;
    for (auto it = tables.constBegin(); it != tables.constEnd(); ++it)
    {
        const QStringList bs = it.key().split(QLatin1Char('/'));
        if (bs.size() != 2)
            continue;
        const int block = bs.at(0).toInt(), section = bs.at(1).toInt();
        const QList<int> now = readSection(block, section);
        if (now.isEmpty())
        {
            res.failed << QStringLiteral("%1: not readable").arg(it.key());
            continue;
        }
        const QList<int> &want = it.value();
        for (int i = 0; i < want.size(); ++i)
        {
            if (i < now.size() && now.at(i) == want.at(i))
            {
                ++res.unchanged;
                continue;
            }
            const int status = writeSingle(block, section, i, want.at(i));
            if (status == StatusAck)
                ++res.changed;
            else
                res.failed << QStringLiteral("%1 index %2: %3").arg(it.key()).arg(i).arg(statusName(status));
        }
    }
    return res;
}

/****************************************************************************
 * Backups
 ****************************************************************************/

bool OpenDeckLink::saveBackup(const QString &path, const QString &device, const Tables &tables,
                              QString *error)
{
    QJsonObject t;
    for (auto it = tables.constBegin(); it != tables.constEnd(); ++it)
    {
        QJsonArray a;
        for (int v : it.value())
            a.append(v);
        t.insert(it.key(), a);
    }
    QJsonObject root;
    root.insert(QStringLiteral("device"), device);
    root.insert(QStringLiteral("created"),
                QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    root.insert(QStringLiteral("tables"), t);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
    {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!f.commit())
    {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

bool OpenDeckLink::loadBackup(const QString &path, Tables *tables, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (error) *error = f.errorString();
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    const QJsonObject t = doc.object().value(QStringLiteral("tables")).toObject();
    if (pe.error != QJsonParseError::NoError || t.isEmpty())
    {
        if (error) *error = QStringLiteral("not a controller backup");
        return false;
    }
    tables->clear();
    for (auto it = t.constBegin(); it != t.constEnd(); ++it)
    {
        QList<int> values;
        for (const QJsonValue &v : it.value().toArray())
            values << v.toInt();
        tables->insert(it.key(), values);
    }
    return true;
}
