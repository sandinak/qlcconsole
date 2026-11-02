/*
  Q Light Controller Plus
  waveformpeaks.h

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

#ifndef WAVEFORMPEAKS_H
#define WAVEFORMPEAKS_H

#include <QSharedPointer>
#include <QVector>
#include <QObject>
#include <QString>
#include <QRectF>
#include <QHash>

#include <atomic>
#include <functional>

#include "audiodecoder.h"

class QPainter;

/** @addtogroup ui_functions
 * @{
 */

/**
 * The min/max envelope of an audio file, one bucket per 10 ms, per channel
 * (at most two; a mono file fills both). Built once from the app's existing
 * audio decoders (no new decoder, no playback), kept in memory by the
 * WaveformService and on disk in the user cache keyed by the file's path,
 * size and mtime, so a 5-minute song costs about 60 KB and is drawn from the
 * envelope at any zoom.
 */
class WaveformPeaks
{
public:
    /** Buckets per second of audio */
    static const int BucketsPerSecond = 100;

    /** How the envelope is drawn */
    enum Mode { Mixed = 0, Left, Right, Stereo };

    WaveformPeaks();

    bool isEmpty() const { return m_count == 0; }
    int bucketCount() const { return m_count; }
    int channels() const { return m_channels; }
    quint32 durationMs() const { return quint32((qint64(m_count) * 1000) / BucketsPerSecond); }

    /** Min / max of a bucket (-127..127) for a channel (0 or 1) */
    int minAt(int channel, int bucket) const;
    int maxAt(int channel, int bucket) const;

    /** Decode a whole file with @p decoder (consumed from its start). A set
     *  @p cancel stops early and returns an empty envelope. Runs anywhere. */
    static WaveformPeaks compute(AudioDecoder *decoder, const std::atomic_bool *cancel = nullptr);

    /** Draw the envelope into @p target (the clip's rectangle; its left edge is
     *  the file's time 0 and one pixel is @p msPerPx), only the columns inside
     *  @p clip. One vertical line per pixel column; cost follows the visible
     *  width, not the song length. */
    void paint(QPainter *painter, const QRectF &target, double msPerPx,
               const QRectF &clip, Mode mode) const;

    /** Cache file for @p path under cacheDir() (fixed name per path) */
    static QString cacheFileFor(const QString &path);

    /** Folder of the cache files: the user cache's "waveforms" folder, or the
     *  folder set by setCacheDir() (tests) */
    static QString cacheDir();
    static void setCacheDir(const QString &dir);

    /** Read / write a cache file. load() accepts it only if it was written for
     *  this path with this size and modification time (so an edited or
     *  replaced file is never drawn from its old envelope). */
    static bool load(const QString &path, WaveformPeaks &out);
    bool save(const QString &path) const;

private:
    QVector<qint8> m_min[2];
    QVector<qint8> m_max[2];
    int m_count;
    int m_channels;
};

/**
 * Hands out the peaks of audio files and builds missing ones on the global
 * thread pool, never on the GUI thread. One per application; ready() is
 * emitted (on the GUI thread) when a file's envelope became available.
 */
class WaveformService : public QObject
{
    Q_OBJECT

public:
    typedef QSharedPointer<const WaveformPeaks> PeaksPtr;
    /** Called on the GUI thread, only when the envelope isn't cached yet */
    typedef std::function<AudioDecoder *()> DecoderFactory;

    static WaveformService *instance();
    ~WaveformService();

    /** The ready envelope of @p path, else null and the build is started (or
     *  already running). A changed file (size / mtime) drops what was kept. */
    PeaksPtr request(const QString &path, const DecoderFactory &makeDecoder);

    /** The envelope of @p path if it is in memory, else null; never starts
     *  anything and never touches the disk (safe from paint()) */
    PeaksPtr lookup(const QString &path) const;

    /** How many decoders were built for envelopes so far (tests) */
    int builds() const { return m_builds; }

signals:
    void ready(const QString &path);

private slots:
    void slotDone(const QString &path, qint64 size, qint64 mtime, const QSharedPointer<WaveformPeaks> &peaks);

private:
    explicit WaveformService(QObject *parent = nullptr);

    struct Entry
    {
        PeaksPtr peaks;
        qint64 size = -1;
        qint64 mtime = -1;
        bool pending = false;
    };
    QHash<QString, Entry> m_entries;
    std::atomic_bool m_cancel;
    int m_builds;
};

Q_DECLARE_METATYPE(QSharedPointer<WaveformPeaks>)

/** @} */

#endif
