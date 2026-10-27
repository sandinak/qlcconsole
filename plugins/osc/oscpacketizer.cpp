/*
  Q Light Controller Plus
  oscpacketizer.cpp

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

#include "oscpacketizer.h"

#include <QStringList>
#include <QDebug>

#include <cmath>
#include <cstring>

OSCPacketizer::OSCPacketizer()
{
}

OSCPacketizer::~OSCPacketizer()
{
}

/*********************************************************************
 * Sender functions
 *********************************************************************/

void OSCPacketizer::setupOSCDmx(QByteArray &data, quint32 universe, quint32 channel, uchar value)
{
    data.clear();
    QString path = QString("/%1/dmx/%2").arg(universe).arg(channel);
    data.append(path.toUtf8());

    // add trailing zeros to reach a multiple of 4
    int zeroNumber = 4 - (path.length() % 4);
    if (zeroNumber > 0)
        data.append(QByteArray(zeroNumber, 0x00));

    data.append(",f");
    data.append((char)0x00);
    data.append((char)0x00);
    // Output value
    float fVal = (float)value / 255.f;
    data.append(*(((char *)&fVal) + 3));
    data.append(*(((char *)&fVal) + 2));
    data.append(*(((char *)&fVal) + 1));
    data.append(*(((char *)&fVal) + 0));
}

void OSCPacketizer::setupOSCGeneric(QByteArray &data, QString &path, QString types, QByteArray &values)
{
    data.clear();
    if (path.isEmpty())
    {
        qDebug() << Q_FUNC_INFO << "Empty path, can't create packet";
        return;
    }

    data.append(path.toUtf8());
    // add trailing zeros to reach a multiple of 4
    int zeroNumber = 4 - (path.length() % 4);
    if (zeroNumber > 0)
        data.append(QByteArray(zeroNumber, 0x00));

    data.append(",");
    data.append(types.toUtf8());

    zeroNumber = 4 - ((types.length() + 1) % 4);
    if (zeroNumber > 0)
        data.append(QByteArray(zeroNumber, 0x00));

    for (int i = 0; i < types.length() && i < values.length(); i++)
    {
        if (types.at(i) == 'f')
        {
            uchar val = (uchar)values.at(i);
            float fVal = (float)val / 255.f;
            data.append(*(((char *)&fVal) + 3));
            data.append(*(((char *)&fVal) + 2));
            data.append(*(((char *)&fVal) + 1));
            data.append(*(((char *)&fVal) + 0));
        }
    }
}

/*********************************************************************
 * Receiver functions
 *********************************************************************/
namespace
{
    /* Everything below reads from a datagram a stranger on the network sent:
       every offset is checked against its size before a byte is read, and
       sizes are unsigned (a "negative" bundle size used to step the parser
       backwards forever). */

    quint32 readBE32(const QByteArray &data, int pos)
    {
        return (quint32(uchar(data.at(pos))) << 24) | (quint32(uchar(data.at(pos + 1))) << 16) |
               (quint32(uchar(data.at(pos + 2))) << 8) | quint32(uchar(data.at(pos + 3)));
    }

    quint64 readBE64(const QByteArray &data, int pos)
    {
        return (quint64(readBE32(data, pos)) << 32) | quint64(readBE32(data, pos + 4));
    }

    /* 0.0..1.0 to a DMX byte; NaN, infinities and anything out of range are
       clamped (a plain (char)(255 * f) of those is undefined behaviour). */
    char normalToByte(double v)
    {
        if (std::isnan(v))
            return 0;
        const double scaled = 255.0 * v;
        if (scaled <= 0.0)
            return 0;
        if (scaled >= 255.0)
            return char(255);
        return char(uchar(scaled));
    }

    /* The end of an OSC string that starts at @p pos: the position after its
       NUL and padding, or -1 when it has no NUL inside the data. */
    int skipOSCString(const QByteArray &data, int pos)
    {
        const int nul = data.indexOf('\0', pos);
        if (nul < 0)
            return -1;
        return (nul + 4) & ~3;
    }

    /* A bundle nested inside a bundle inside...: a sane sender nests a few. */
    const int kMaxBundleDepth = 8;
}

bool OSCPacketizer::parseMessage(QByteArray const& data, QString& path, QByteArray& values)
{
    path.clear();
    values.clear();

    QList<TagType> typeArray;

    //qDebug() << "[OSC] data:" << data;

    // first of all look for a comma
    int commaPos = data.indexOf(0x2C);
    if (commaPos == -1)
        return false;

    path = QString(data.mid(0, commaPos));
    // remove possible trailing zeroes used for padding
    path.remove(QChar(0x00));
    //qDebug() << "[OSC] path extracted:" << path;

    // the type tags run from the comma to a NUL, which must be in the data
    const int tagsEnd = data.indexOf('\0', commaPos + 1);
    if (tagsEnd < 0)
        return false;

    for (int i = commaPos + 1; i < tagsEnd; i++)
    {
        switch (data.at(i))
        {
            case 'b': typeArray.append(BlobTag); break;
            case 'f': typeArray.append(FloatTag); break;
            case 'd': typeArray.append(DoubleTag); break;
            case 'i': typeArray.append(IntegerTag); break;
            case 's': typeArray.append(StringTag); break;
            case 't': typeArray.append(TimeTag); break;
            default: break;
        }
    }
    // the arguments start at the next multiple of 4 after the tags' NUL
    int currPos = (tagsEnd + 4) & ~3;

    //qDebug () << "[OSC] Tags found:" << typeArray.count() << "currpos at" << currPos;

    foreach (TagType tag, typeArray)
    {
        if (currPos < 0 || currPos >= data.size())
            break;
        const int left = data.size() - currPos;

        switch (tag)
        {
            case IntegerTag:
            {
                if (left < 4)
                    return true;
                quint32 iVal = readBE32(data, currPos);

                if (iVal < 256)
                    values.append((char)iVal);
                else
                    values.append((char)(iVal / 0xFFFFFF));

                qDebug() << "[OSC] iVal:" << iVal;
                currPos += 4;
            }
            break;
            case FloatTag:
            {
                if (left < 4)
                    return true;
                quint32 bits = readBE32(data, currPos);
                float fVal;
                memcpy(&fVal, &bits, sizeof(fVal));

                values.append(normalToByte(fVal));

                qDebug() << "[OSC] fVal:" << fVal;

                currPos += 4;
            }
            break;
            case DoubleTag:
            {
                if (left < 8)
                    return true;

                quint64 bits = readBE64(data, currPos);
                double dVal;
                memcpy(&dVal, &bits, sizeof(dVal));

                values.append(normalToByte(dVal));

                qDebug() << "[OSC] dVal:" << dVal;
                currPos += 8;
            }
            break;
            case StringTag:
            {
                const int next = skipOSCString(data, currPos);
                if (next < 0)
                    return true;
                qDebug() << "[OSC] string:" << QString(data.mid(currPos, next - currPos));
                currPos = next;
            }
            break;
            case BlobTag:
            {
                if (left < 4)
                    return true;
                const quint32 blobSize = readBE32(data, currPos);
                if (quint64(blobSize) > quint64(left - 4))
                    return true;
                // skip the blob and its padding; its content is not a value
                currPos += 4 + int((blobSize + 3) & ~3u);
            }
            break;
            case TimeTag:
            {
                // A OSC timestamp would be helpful to defer
                // value changes, but since QLC+ plugins don't support
                // such thing, let's skip it.
                currPos += 8;
            }
            break;
            default: break;
        }
    }

    return true;
}

void OSCPacketizer::parseBundle(QByteArray const& data, int depth,
                                QList<QPair<QString, QByteArray> > &messages)
{
    // 8 bytes for '#bundle\0' and 8 bytes for a timestamp that we don't handle
    if (depth > kMaxBundleDepth || data.size() < 16 || !data.startsWith(QByteArray("#bundle", 8)))
    {
        qWarning() << "[OSC] Found an unsupported message type!";
        return;
    }

    // a bundle holds elements, each a 4-byte size followed by that many bytes
    // of message or of another bundle
    int bufPos = 16;
    while (data.size() - bufPos >= 4)
    {
        const quint32 elemSize = readBE32(data, bufPos);
        bufPos += 4;

        // an element past the end of the datagram ends the bundle: what is
        // left cannot be trusted to be anything
        if (elemSize == 0 || quint64(elemSize) > quint64(data.size() - bufPos))
            return;

        const QByteArray elem = data.mid(bufPos, int(elemSize));
        if (elem.startsWith('#'))
        {
            parseBundle(elem, depth + 1, messages);
        }
        else
        {
            QString path;
            QByteArray values;
            if (parseMessage(elem, path, values) == true)
                messages.append(QPair<QString, QByteArray>(path, values));
        }
        // elemSize is >= 1, so this always moves forward
        bufPos += int(elemSize);
    }
}

QList<QPair<QString, QByteArray> > OSCPacketizer::parsePacket(QByteArray const& data)
{
    QList<QPair<QString, QByteArray> > messages;

    if (data.isEmpty())
        return messages;

    // check wether we need to parse a bundle or a single message
    if (data.at(0) == '#')
    {
        parseBundle(data, 0, messages);
    }
    else
    {
        QString path;
        QByteArray values;
        if (parseMessage(data, path, values) == true)
            messages.append(QPair<QString, QByteArray>(path, values));
    }

    return messages;
}
