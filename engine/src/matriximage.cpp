/*
  Q Light Controller Plus
  matriximage.cpp

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

#include <QCryptographicHash>
#include <QMimeDatabase>
#include <QImageReader>
#include <QFileInfo>
#include <QBuffer>
#include <QMutex>
#include <QObject>
#include <QImage>
#include <QJSEngine>
#include <QJSValue>
#include <QFile>
#include <QHash>
#include <QUrl>

#include "matriximage.h"

namespace
{
    const QLatin1String kPrefix("data:");
    const QLatin1String kBase64(";base64,");

    QMutex s_mutex;
    QHash<QString, QSharedPointer<const MatrixImage::Frames>> s_cache;   // by key
    QList<QString> s_recent;                                             // most recent last
    const int kCacheSize = 8;

    QString keyOf(const QString &value)
    {
        return QString::fromLatin1(
            QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha1).toHex().left(16));
    }

    QSharedPointer<MatrixImage::Frames> decodeBytes(const QByteArray &bytes, const QString &name)
    {
        QByteArray data = bytes;
        QBuffer buf(&data);
        buf.open(QIODevice::ReadOnly);
        QImageReader reader(&buf);
        reader.setAutoTransform(true);
        if (!reader.canRead())
            return QSharedPointer<MatrixImage::Frames>();

        QSharedPointer<MatrixImage::Frames> out(new MatrixImage::Frames);
        out->name = name;
        QSize target;
        while (out->rgba.size() < MatrixImage::kMaxFrames)
        {
            QImage img = reader.read();
            if (img.isNull())
                break;
            const int delay = reader.nextImageDelay();
            if (!target.isValid())
            {
                // Every frame the size of the first, inside kMaxSide.
                target = img.size();
                if (target.width() > MatrixImage::kMaxSide || target.height() > MatrixImage::kMaxSide)
                    target.scale(MatrixImage::kMaxSide, MatrixImage::kMaxSide, Qt::KeepAspectRatio);
                target = target.expandedTo(QSize(1, 1));
            }
            if (img.size() != target)
                img = img.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            // Premultiplied: a transparent pixel is black, as it would be on
            // the rig.
            img = img.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            QByteArray px;
            px.reserve(target.width() * target.height() * 4);
            for (int y = 0; y < img.height(); ++y)
                px.append(reinterpret_cast<const char *>(img.constScanLine(y)), img.width() * 4);
            out->rgba.append(px);
            out->delaysMs.append(qMax(0, delay));
            if (!reader.supportsAnimation() || reader.imageCount() == 1)
                break;
        }
        if (out->rgba.isEmpty())
            return QSharedPointer<MatrixImage::Frames>();
        out->width = target.width();
        out->height = target.height();
        if (out->rgba.size() == 1)
            out->delaysMs[0] = 0;
        else
        {
            // A frame with no delay (or the 10ms browsers clamp up) shows
            // for a tenth, as a browser shows it.
            for (int &d : out->delaysMs)
                if (d <= 10)
                    d = 100;
        }
        out->totalMs = 0;
        for (int d : out->delaysMs)
            out->totalMs += d;
        return out;
    }
}

bool MatrixImage::isImage(const QString &value)
{
    return value.startsWith(kPrefix) && value.indexOf(kBase64) > 0;
}

bool MatrixImage::unpack(const QString &value, QByteArray *bytes, QString *name, QString *mimeType)
{
    if (!isImage(value))
        return false;
    const int at = value.indexOf(kBase64);
    const QStringList head = value.mid(kPrefix.size(), at - kPrefix.size()).split(QLatin1Char(';'));
    if (mimeType != nullptr)
        *mimeType = head.value(0);
    if (name != nullptr)
    {
        name->clear();
        for (const QString &h : head)
            if (h.startsWith(QLatin1String("name=")))
                *name = QUrl::fromPercentEncoding(h.mid(5).toLatin1());
    }
    if (bytes != nullptr)
        *bytes = QByteArray::fromBase64(QStringView(value).mid(at + kBase64.size()).toLatin1());
    return true;
}

QString MatrixImage::fromBytes(const QByteArray &bytes, const QString &name, QString *error)
{
    auto fail = [error](const QString &why) {
        if (error != nullptr)
            *error = why;
        return QString();
    };
    if (bytes.isEmpty())
        return fail(QObject::tr("The file is empty."));
    if (bytes.size() > kMaxFileBytes)
        return fail(QObject::tr("The file is %1 KB; a show takes pictures up to %2 KB. "
                                "Make it smaller (a box rig needs only a few hundred pixels).")
                        .arg(bytes.size() / 1024).arg(kMaxFileBytes / 1024));
    if (decodeBytes(bytes, name).isNull())
        return fail(QObject::tr("This isn't a picture this desk can read."));
    QString mime = QMimeDatabase().mimeTypeForData(bytes).name();
    if (!mime.startsWith(QLatin1String("image/")))
        mime = QStringLiteral("image/") + QFileInfo(name).suffix().toLower();
    return kPrefix + mime + QLatin1String(";name=")
           + QString::fromLatin1(QUrl::toPercentEncoding(name, QByteArray(" .-_()")))
           + kBase64 + QString::fromLatin1(bytes.toBase64());
}

QString MatrixImage::fromFile(const QString &path, QString *error)
{
    QFile f(path);
    if (f.size() > kMaxFileBytes)
    {
        if (error != nullptr)
            *error = QObject::tr("The file is %1 KB; a show takes pictures up to %2 KB. "
                                 "Make it smaller (a box rig needs only a few hundred pixels).")
                         .arg(f.size() / 1024).arg(kMaxFileBytes / 1024);
        return QString();
    }
    if (!f.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
            *error = QObject::tr("Can't open %1.").arg(QFileInfo(path).fileName());
        return QString();
    }
    return fromBytes(f.readAll(), QFileInfo(path).fileName(), error);
}

QSharedPointer<const MatrixImage::Frames> MatrixImage::decode(const QString &value)
{
    if (!isImage(value))
        return QSharedPointer<const Frames>();
    const QString key = keyOf(value);
    {
        QMutexLocker lock(&s_mutex);
        auto it = s_cache.constFind(key);
        if (it != s_cache.constEnd())
        {
            s_recent.removeOne(key);
            s_recent.append(key);
            return it.value();
        }
    }
    // Decoded outside the lock: another thread may decode the same picture
    // at the same moment, harmlessly -- the first one in is kept.
    QByteArray bytes;
    QString name;
    unpack(value, &bytes, &name);
    QSharedPointer<Frames> frames = decodeBytes(bytes, name);
    if (frames.isNull())
        return QSharedPointer<const Frames>();
    frames->key = key;

    QMutexLocker lock(&s_mutex);
    auto it = s_cache.constFind(key);
    if (it != s_cache.constEnd())
        return it.value();
    s_cache.insert(key, frames);
    s_recent.append(key);
    while (s_recent.size() > kCacheSize)
        s_cache.remove(s_recent.takeFirst());
    return frames;
}

QJSValue MatrixImage::toScript(QJSEngine *engine, const Frames &f)
{
    QJSValue o = engine->newObject();
    o.setProperty(QStringLiteral("name"), f.name);
    o.setProperty(QStringLiteral("width"), f.width);
    o.setProperty(QStringLiteral("height"), f.height);
    o.setProperty(QStringLiteral("totalMs"), f.totalMs);
    o.setProperty(QStringLiteral("key"), f.key);
    QJSValue frames = engine->newArray(uint(f.rgba.size()));
    QJSValue delays = engine->newArray(uint(f.rgba.size()));
    for (int i = 0; i < f.rgba.size(); ++i)
    {
        frames.setProperty(uint(i), engine->toScriptValue(f.rgba.at(i)));   // an ArrayBuffer
        delays.setProperty(uint(i), f.delaysMs.at(i));
    }
    o.setProperty(QStringLiteral("frames"), frames);
    o.setProperty(QStringLiteral("delays"), delays);
    return o;
}

QString MatrixImage::describe(const QString &value)
{
    QByteArray bytes;
    QString name;
    if (!unpack(value, &bytes, &name))
        return QString();
    QSharedPointer<const Frames> f = decode(value);
    if (f.isNull())
        return QObject::tr("%1 (can't be read)").arg(name);
    QString line = QString::fromUtf8("%1 · %2 × %3").arg(name).arg(f->width).arg(f->height);
    if (f->rgba.size() > 1)
        line += QString::fromUtf8(" · ") + QObject::tr("%1 frames").arg(f->rgba.size());
    line += QString::fromUtf8(" · ") + QObject::tr("%1 KB").arg(qMax(1, int(bytes.size() / 1024)));
    return line;
}

void MatrixImage::clearCache()
{
    QMutexLocker lock(&s_mutex);
    s_cache.clear();
    s_recent.clear();
}
