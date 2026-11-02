/*
  Q Light Controller Plus
  midimarkers.cpp

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
#include <QStringDecoder>
#include <QFile>

#include <algorithm>

#include "midimarkers.h"

namespace
{

QString tr(const char *text)
{
    return QCoreApplication::translate("MidiMarkers", text);
}

/* A cursor over the bytes that never reads past @a end. */
struct Reader
{
    const uchar *p;
    const uchar *end;

    bool has(qint64 n) const { return n >= 0 && end - p >= n; }
    bool u8(quint32 *v)
    {
        if (!has(1))
            return false;
        *v = *p++;
        return true;
    }
    bool be(int bytes, quint32 *v)
    {
        if (!has(bytes))
            return false;
        quint32 out = 0;
        for (int i = 0; i < bytes; ++i)
            out = (out << 8) | *p++;
        *v = out;
        return true;
    }
    /* A variable-length quantity: at most four bytes. */
    bool vlq(quint32 *v)
    {
        quint32 out = 0;
        for (int i = 0; i < 4; ++i)
        {
            quint32 b = 0;
            if (!u8(&b))
                return false;
            out = (out << 7) | (b & 0x7F);
            if ((b & 0x80) == 0)
            {
                *v = out;
                return true;
            }
        }
        return false;
    }
};

struct Tempo
{
    quint64 tick;
    quint32 usPerQuarter;
};

struct RawMarker
{
    quint64 tick;
    QString name;
    int order;
};

QString markerText(const uchar *data, int len)
{
    const QByteArray bytes(reinterpret_cast<const char *>(data), len);
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    const QString s = utf8(bytes);
    if (!utf8.hasError())
        return s.trimmed();
    return QString::fromLatin1(bytes).trimmed();    // an older DAW's own code page
}

} // namespace

MidiMarkers::Result MidiMarkers::readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        Result r;
        r.error = tr("Can't open %1").arg(path);
        return r;
    }
    return read(f.readAll());
}

MidiMarkers::Result MidiMarkers::read(const QByteArray &data)
{
    Result r;
    Reader in { reinterpret_cast<const uchar *>(data.constData()),
                reinterpret_cast<const uchar *>(data.constData()) + data.size() };

    quint32 magic = 0, headerLen = 0, format = 0, tracks = 0, division = 0;
    if (!in.be(4, &magic) || magic != 0x4D546864 /* MThd */ || !in.be(4, &headerLen) || headerLen < 6
        || !in.be(2, &format) || !in.be(2, &tracks) || !in.be(2, &division) || !in.has(headerLen - 6))
    {
        r.error = tr("Not a MIDI file");
        return r;
    }
    in.p += headerLen - 6;
    if (format > 1)
    {
        r.error = tr("A type %1 MIDI file: export it as type 1 (or 0)").arg(format);
        return r;
    }
    if (division == 0)
    {
        r.error = tr("The MIDI file has no time division");
        return r;
    }

    QList<Tempo> tempos;
    QList<RawMarker> raw;
    int order = 0;
    for (quint32 t = 0; t < tracks && in.has(8); ++t)
    {
        quint32 id = 0, len = 0;
        in.be(4, &id);
        in.be(4, &len);
        if (!in.has(len))
        {
            r.error = tr("The MIDI file is cut short");
            return r;
        }
        Reader trk { in.p, in.p + len };
        in.p += len;
        if (id != 0x4D54726B /* MTrk */)
            continue;                       // an unknown chunk: stepped over
        quint64 tick = 0;
        quint32 status = 0;
        while (trk.has(1))
        {
            quint32 delta = 0;
            if (!trk.vlq(&delta))
                break;
            tick += delta;
            quint32 b = 0;
            if (!trk.u8(&b))
                break;
            if (b == 0xFF)
            {
                quint32 type = 0, mlen = 0;
                if (!trk.u8(&type) || !trk.vlq(&mlen) || !trk.has(mlen))
                    break;
                const uchar *m = trk.p;
                trk.p += mlen;
                if (type == 0x2F)
                    break;                  // end of track
                if (type == 0x06)
                    raw << RawMarker{ tick, markerText(m, int(mlen)), order++ };
                else if (type == 0x51 && mlen == 3)
                {
                    const quint32 us = (quint32(m[0]) << 16) | (quint32(m[1]) << 8) | m[2];
                    if (us > 0)
                        tempos << Tempo{ tick, us };
                }
                else if (type == 0x54 && mlen == 5 && r.smpteOffsetMs < 0)
                {
                    static const int rates[4] = { 24, 25, 30, 30 };     // 29.97 drop reads as 30
                    const int fps = rates[(m[0] >> 5) & 0x03];
                    const qint64 h = m[0] & 0x1F;
                    r.smpteOffsetMs = ((h * 60 + m[1]) * 60 + m[2]) * 1000 + qint64(m[3]) * 1000 / fps
                                      + qint64(m[4]) * 10 / fps;
                }
                status = 0;                 // a meta event cancels running status
                continue;
            }
            if (b == 0xF0 || b == 0xF7)
            {
                quint32 slen = 0;
                if (!trk.vlq(&slen) || !trk.has(slen))
                    break;
                trk.p += slen;
                status = 0;
                continue;
            }
            int dataBytes = 0;
            if (b & 0x80)
            {
                status = b;
                dataBytes = ((b & 0xF0) == 0xC0 || (b & 0xF0) == 0xD0) ? 1 : 2;
            }
            else
            {
                if (status == 0)
                    break;                  // a data byte with no status: garbage
                --trk.p;                    // running status: this byte is data
                dataBytes = ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) ? 1 : 2;
            }
            if (!trk.has(dataBytes))
                break;
            trk.p += dataBytes;
        }
    }

    // Ticks to ms, through the tempo map.
    std::stable_sort(tempos.begin(), tempos.end(), [](const Tempo &a, const Tempo &b) { return a.tick < b.tick; });
    r.tempoChanges = int(tempos.count());
    const bool smpte = (division & 0x8000) != 0;
    auto toMs = [&](quint64 tick) -> qint64 {
        if (smpte)
        {
            const int fps = -qint8(quint8(division >> 8));
            const int perFrame = int(division & 0xFF);
            if (fps <= 0 || perFrame <= 0)
                return 0;
            return qint64(tick * 1000 / quint64(fps * perFrame));
        }
        const double ppq = double(division);
        double us = 0;
        quint64 at = 0;
        quint32 tempo = 500000;             // 120 bpm until the first tempo event
        for (const Tempo &tm : tempos)
        {
            if (tm.tick >= tick)
                break;
            us += double(tm.tick - at) * tempo / ppq;
            at = tm.tick;
            tempo = tm.usPerQuarter;
        }
        us += double(tick - at) * tempo / ppq;
        return qint64(us / 1000.0 + 0.5);
    };
    std::stable_sort(raw.begin(), raw.end(), [](const RawMarker &a, const RawMarker &b) {
        return a.tick != b.tick ? a.tick < b.tick : a.order < b.order;
    });
    for (const RawMarker &m : raw)
    {
        if (m.name.isEmpty())
            continue;
        Marker mk;
        mk.name = m.name;
        mk.ms = toMs(m.tick);
        r.markers << mk;
    }
    r.ok = true;
    return r;
}
