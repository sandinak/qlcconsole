/*
  Q Light Controller Plus
  waveformpeaks.cpp

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
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QDataStream>
#include <QThreadPool>
#include <QFileInfo>
#include <QSaveFile>
#include <QPainter>
#include <QDir>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>

#include "waveformpeaks.h"

static const quint32 kMagic = 0x51575031; // "QWP1"
static QString s_cacheDir;

WaveformPeaks::WaveformPeaks()
    : m_count(0)
    , m_channels(1)
{
}

int WaveformPeaks::minAt(int channel, int bucket) const
{
    if (channel < 0 || channel > 1 || bucket < 0 || bucket >= m_count)
        return 0;
    return m_min[channel].at(bucket);
}

int WaveformPeaks::maxAt(int channel, int bucket) const
{
    if (channel < 0 || channel > 1 || bucket < 0 || bucket >= m_count)
        return 0;
    return m_max[channel].at(bucket);
}

/** One sample as a signed 16 bit value */
static int sample16(const char *p, AudioFormat fmt)
{
    switch (fmt)
    {
        case PCM_S8:
            return int(*reinterpret_cast<const signed char *>(p)) * 256;
        case PCM_S24LE:
        {
            qint32 v;
            std::memcpy(&v, p, 4);
            return int((v << 8) >> 16);     // low three bytes, sign extended
        }
        case PCM_S32LE:
        {
            qint32 v;
            std::memcpy(&v, p, 4);
            return int(v >> 16);
        }
        default:
        {
            qint16 v;
            std::memcpy(&v, p, 2);
            return int(v);
        }
    }
}

WaveformPeaks WaveformPeaks::compute(AudioDecoder *decoder, const std::atomic_bool *cancel)
{
    WaveformPeaks out;
    if (decoder == nullptr)
        return out;

    const AudioParameters ap = decoder->audioParameters();
    const int ch = ap.channels();
    const int ss = ap.sampleSize();
    const quint32 rate = ap.sampleRate();
    if (ch < 1 || ss < 1 || rate < quint32(BucketsPerSecond))
        return out;

    const int frameBytes = ch * ss;
    const int framesPerBucket = int(rate / quint32(BucketsPerSecond));
    out.m_channels = (ch >= 2) ? 2 : 1;

    decoder->seek(0);

    QByteArray buf(frameBytes * 4096, 0);
    int have = 0;
    int inBucket = 0;
    int curMin[2] = { INT_MAX, INT_MAX };
    int curMax[2] = { INT_MIN, INT_MIN };

    auto flush = [&]()
    {
        for (int k = 0; k < 2; k++)
        {
            const int src = (out.m_channels == 2) ? k : 0;
            out.m_min[k].append(qint8(std::max(-127, curMin[src] >> 8)));
            out.m_max[k].append(qint8(std::min(127, curMax[src] >> 8)));
        }
        curMin[0] = curMin[1] = INT_MAX;
        curMax[0] = curMax[1] = INT_MIN;
        inBucket = 0;
    };

    for (;;)
    {
        if (cancel != nullptr && cancel->load())
            return WaveformPeaks();

        const qint64 n = decoder->read(buf.data() + have, buf.size() - have);
        if (n <= 0)
            break;
        have += int(n);

        const int frames = have / frameBytes;
        const char *p = buf.constData();
        for (int f = 0; f < frames; f++)
        {
            for (int c = 0; c < ch; c++)
            {
                const int k = (c == 0) ? 0 : 1;
                const int v = sample16(p, ap.format());
                p += ss;
                if (out.m_channels == 1 && c > 0)
                    continue;
                curMin[k] = std::min(curMin[k], v);
                curMax[k] = std::max(curMax[k], v);
            }
            if (++inBucket == framesPerBucket)
                flush();
        }
        const int used = frames * frameBytes;
        std::memmove(buf.data(), buf.constData() + used, size_t(have - used));
        have -= used;
    }
    if (inBucket > 0)
        flush();

    out.m_count = out.m_min[0].size();
    return out;
}

void WaveformPeaks::paint(QPainter *painter, const QRectF &target, double msPerPx,
                          const QRectF &clip, Mode mode) const
{
    if (m_count == 0 || msPerPx <= 0.0 || target.height() < 2.0)
        return;

    const int x0 = int(std::floor(std::max(clip.left(), target.left())));
    const int x1 = int(std::ceil(std::min(clip.right(), target.right())));
    if (x1 <= x0)
        return;

    const bool two = (mode == Stereo && m_channels == 2);
    const int lanes = two ? 2 : 1;
    const double laneH = target.height() / lanes;
    const double pxBuckets = msPerPx / (1000.0 / BucketsPerSecond);

    for (int lane = 0; lane < lanes; lane++)
    {
        const double mid = target.top() + laneH * (lane + 0.5);
        const double half = laneH / 2.0 - 0.5;
        QVector<QLineF> lines;
        lines.reserve(x1 - x0);

        for (int x = x0; x < x1; x++)
        {
            const double pos = (x - target.left()) * pxBuckets;
            const int b0 = int(pos);
            if (b0 >= m_count)
                break;
            const int b1 = std::min(m_count - 1, std::max(b0, int(pos + pxBuckets - 1e-9)));

            int mn = 127;
            int mx = -127;
            for (int b = b0; b <= b1; b++)
            {
                if (two)
                {
                    mn = std::min(mn, int(m_min[lane].at(b)));
                    mx = std::max(mx, int(m_max[lane].at(b)));
                }
                else if (mode == Left || mode == Right)
                {
                    const int c = (mode == Right && m_channels == 2) ? 1 : 0;
                    mn = std::min(mn, int(m_min[c].at(b)));
                    mx = std::max(mx, int(m_max[c].at(b)));
                }
                else
                {
                    mn = std::min(mn, int(std::min(m_min[0].at(b), m_min[1].at(b))));
                    mx = std::max(mx, int(std::max(m_max[0].at(b), m_max[1].at(b))));
                }
            }
            if (mx < mn)
                continue;

            double yTop = mid - (mx / 127.0) * half;
            double yBot = mid - (mn / 127.0) * half;
            if (yBot - yTop < 1.0)
            {
                yTop -= 0.5;
                yBot += 0.5;
            }
            lines.append(QLineF(x + 0.5, yTop, x + 0.5, yBot));
        }
        painter->drawLines(lines);
    }
}

/* ------------------------------------------------------------------------
 * Cache
 * ---------------------------------------------------------------------- */

QString WaveformPeaks::cacheDir()
{
    if (s_cacheDir.isEmpty() == false)
        return s_cacheDir;
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
           + QStringLiteral("/waveforms");
}

void WaveformPeaks::setCacheDir(const QString &dir)
{
    s_cacheDir = dir;
}

QString WaveformPeaks::cacheFileFor(const QString &path)
{
    const QString canon = QFileInfo(path).absoluteFilePath();
    const QByteArray h = QCryptographicHash::hash(canon.toUtf8(), QCryptographicHash::Sha1).toHex();
    return cacheDir() + QLatin1Char('/') + QString::fromLatin1(h) + QStringLiteral(".wfp");
}

bool WaveformPeaks::load(const QString &path, WaveformPeaks &out)
{
    const QFileInfo fi(path);
    if (fi.exists() == false)
        return false;

    QFile f(cacheFileFor(path));
    if (f.open(QIODevice::ReadOnly) == false)
        return false;

    QDataStream in(&f);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 magic = 0;
    QString storedPath;
    qint64 size = 0, mtime = 0;
    qint32 channels = 0, count = 0;
    QByteArray b[4];
    in >> magic >> storedPath >> size >> mtime >> channels >> count;
    for (int i = 0; i < 4; i++)
        in >> b[i];
    if (in.status() != QDataStream::Ok || magic != kMagic
        || storedPath != fi.absoluteFilePath()
        || size != fi.size() || mtime != fi.lastModified().toMSecsSinceEpoch()
        || (channels != 1 && channels != 2) || count <= 0)
        return false;
    for (int i = 0; i < 4; i++)
        if (b[i].size() != count)
            return false;

    WaveformPeaks p;
    p.m_channels = channels;
    p.m_count = count;
    for (int k = 0; k < 2; k++)
    {
        p.m_min[k].resize(count);
        p.m_max[k].resize(count);
        std::memcpy(p.m_min[k].data(), b[k * 2].constData(), size_t(count));
        std::memcpy(p.m_max[k].data(), b[k * 2 + 1].constData(), size_t(count));
    }
    out = p;
    return true;
}

bool WaveformPeaks::save(const QString &path) const
{
    if (m_count == 0)
        return false;
    const QFileInfo fi(path);
    if (fi.exists() == false)
        return false;
    if (QDir().mkpath(cacheDir()) == false)
        return false;

    QSaveFile f(cacheFileFor(path));
    if (f.open(QIODevice::WriteOnly) == false)
        return false;

    QDataStream out(&f);
    out.setVersion(QDataStream::Qt_6_0);
    out << kMagic << fi.absoluteFilePath() << qint64(fi.size())
        << qint64(fi.lastModified().toMSecsSinceEpoch())
        << qint32(m_channels) << qint32(m_count);
    for (int k = 0; k < 2; k++)
    {
        out << QByteArray(reinterpret_cast<const char *>(m_min[k].constData()), m_count);
        out << QByteArray(reinterpret_cast<const char *>(m_max[k].constData()), m_count);
    }
    return f.commit();
}

/* ------------------------------------------------------------------------
 * Service
 * ---------------------------------------------------------------------- */

WaveformService::WaveformService(QObject *parent)
    : QObject(parent)
    , m_cancel(false)
    , m_builds(0)
{
    qRegisterMetaType<QSharedPointer<WaveformPeaks> >();
}

WaveformService::~WaveformService()
{
    m_cancel.store(true);
    // the global pool is already gone (and has waited for its tasks) when the
    // application object is torn down after main() returns
    if (QThreadPool *pool = QThreadPool::globalInstance())
        pool->waitForDone();
}

WaveformService *WaveformService::instance()
{
    static WaveformService *s = nullptr;
    if (s == nullptr)
        s = new WaveformService(QCoreApplication::instance());
    return s;
}

WaveformService::PeaksPtr WaveformService::lookup(const QString &path) const
{
    const auto it = m_entries.constFind(path);
    if (it == m_entries.constEnd())
        return PeaksPtr();
    return (it->peaks && it->peaks->isEmpty() == false) ? it->peaks : PeaksPtr();
}

WaveformService::PeaksPtr WaveformService::request(const QString &path, const DecoderFactory &makeDecoder)
{
    const QFileInfo fi(path);
    if (path.isEmpty() || fi.exists() == false)
        return PeaksPtr();

    const qint64 size = fi.size();
    const qint64 mtime = fi.lastModified().toMSecsSinceEpoch();

    Entry &e = m_entries[path];
    if (e.size == size && e.mtime == mtime)
        return lookup(path);    // known: ready, still building, or undecodable

    // new, or the file changed since: forget the old envelope
    e = Entry();
    e.size = size;
    e.mtime = mtime;

    WaveformPeaks cached;
    if (WaveformPeaks::load(path, cached))
    {
        e.peaks = PeaksPtr(new WaveformPeaks(cached));
        return e.peaks;
    }

    AudioDecoder *decoder = makeDecoder ? makeDecoder() : nullptr;
    if (decoder == nullptr)
    {
        e.peaks = PeaksPtr(new WaveformPeaks());   // remembered as undecodable
        return PeaksPtr();
    }

    e.pending = true;
    m_builds++;
    QThreadPool::globalInstance()->start(QRunnable::create([this, decoder, path, size, mtime]()
    {
        QSharedPointer<WaveformPeaks> peaks(new WaveformPeaks(WaveformPeaks::compute(decoder, &m_cancel)));
        delete decoder;
        if (m_cancel.load())
            return;
        peaks->save(path);
        QMetaObject::invokeMethod(this, "slotDone", Qt::QueuedConnection,
                                  Q_ARG(QString, path), Q_ARG(qint64, size), Q_ARG(qint64, mtime),
                                  Q_ARG(QSharedPointer<WaveformPeaks>, peaks));
    }));
    return PeaksPtr();
}

void WaveformService::slotDone(const QString &path, qint64 size, qint64 mtime,
                               const QSharedPointer<WaveformPeaks> &peaks)
{
    auto it = m_entries.find(path);
    if (it == m_entries.end() || it->size != size || it->mtime != mtime)
        return;     // the file changed again while this one was building
    it->pending = false;
    it->peaks = peaks;
    if (peaks->isEmpty() == false)
        emit ready(path);
}
