/*
  Q Light Controller Plus - Unit test
  waveformpeaks_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtTest>
#include <QTemporaryDir>
#include <QImage>
#include <QPainter>

#include <atomic>
#include <cstring>

#include "waveformpeaks.h"
#include "waveformpeaks_test.h"

/** A decoder that makes its audio up: @p secs seconds, 44100 Hz 16 bit, the
 *  left channel at +-16384 for the first second then silent, the right
 *  channel silent. Counts how many were made. */
class FakeDecoder : public AudioDecoder
{
public:
    FakeDecoder(int secs, int channels = 2) : m_secs(secs), m_ch(channels), m_pos(0)
    {
        configure(44100, channels, PCM_S16LE);
    }
    AudioDecoder *createCopy() override { return nullptr; }
    int priority() const override { return 0; }
    QStringList supportedFormats() override { return QStringList(); }
    bool initialize(const QString &) override { return true; }
    qint64 totalTime() override { return qint64(m_secs) * 1000; }
    void seek(qint64) override { m_pos = 0; }
    int bitrate() override { return 0; }
    qint64 read(char *data, qint64 maxSize) override
    {
        const qint64 total = qint64(m_secs) * 44100 * m_ch * 2;
        qint64 n = qMin(maxSize, total - m_pos);
        for (qint64 i = 0; i + 1 < n; i += 2)
        {
            const qint64 frame = (m_pos + i) / (m_ch * 2);
            const int c = int(((m_pos + i) / 2) % m_ch);
            qint16 v = 0;
            if (c == 0 && frame < 44100)
                v = (frame / 20) % 2 ? 16384 : -16384;
            std::memcpy(data + i, &v, 2);
        }
        m_pos += n;
        return n;
    }

private:
    int m_secs, m_ch;
    qint64 m_pos;
};

static QString makeFile(const QTemporaryDir &dir, const QString &name, int bytes)
{
    const QString p = dir.filePath(name);
    QFile f(p);
    f.open(QIODevice::WriteOnly);
    f.write(QByteArray(bytes, 'x'));
    return p;
}

void WaveformPeaks_Test::peaksFollowTheSamples()
{
    FakeDecoder d(3);
    const WaveformPeaks p = WaveformPeaks::compute(&d);
    QCOMPARE(p.channels(), 2);
    QCOMPARE(p.bucketCount(), 300);                 // 100 per second
    QCOMPARE(p.durationMs(), quint32(3000));
    QCOMPARE(p.maxAt(0, 50), 64);                   // 16384 >> 8
    QCOMPARE(p.minAt(0, 50), -64);
    QCOMPARE(p.maxAt(0, 150), 0);                   // silent after the first second
    QCOMPARE(p.maxAt(1, 50), 0);                    // right channel is silent

    FakeDecoder mono(1, 1);
    const WaveformPeaks m = WaveformPeaks::compute(&mono);
    QCOMPARE(m.channels(), 1);
    QCOMPARE(m.maxAt(1, 10), m.maxAt(0, 10));       // a mono file fills both
}

void WaveformPeaks_Test::aCancelledBuildIsEmpty()
{
    FakeDecoder d(3);
    std::atomic_bool cancel(true);
    QVERIFY(WaveformPeaks::compute(&d, &cancel).isEmpty());
    QVERIFY(WaveformPeaks::compute(nullptr).isEmpty());
}

void WaveformPeaks_Test::drawsWithNoFile()
{
    // the timeline draws before a file is read, or when it can't be
    WaveformPeaks none;
    QImage img(200, 40, QImage::Format_ARGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    none.paint(&p, QRectF(0, 0, 200, 40), 20.0, QRectF(0, 0, 200, 40), WaveformPeaks::Mixed);
    none.paint(&p, QRectF(0, 0, 0, 0), 0.0, QRectF(), WaveformPeaks::Stereo);
    p.end();
    QCOMPARE(img.pixelColor(100, 20), QColor(Qt::black));
    QVERIFY(WaveformService::instance()->request(QString(), nullptr).isNull());
    QVERIFY(WaveformService::instance()->request("/no/such/file.wav", nullptr).isNull());
}

void WaveformPeaks_Test::drawsAtAnyZoom()
{
    FakeDecoder d(300);                             // a 5 minute song
    const WaveformPeaks p = WaveformPeaks::compute(&d);
    QCOMPARE(p.bucketCount(), 30000);

    QElapsedTimer t;
    t.start();
    for (double msPerPx : { 5.0, 20.0, 100.0, 1000.0, 5000.0 })
    {
        QImage img(1600, 76, QImage::Format_ARGB32);
        img.fill(Qt::black);
        QPainter pt(&img);
        pt.setPen(Qt::white);
        const QRectF clip(0, 0, 1600, 76);
        p.paint(&pt, QRectF(0, 0, 300000.0 / msPerPx, 76), msPerPx, clip, WaveformPeaks::Stereo);
        pt.end();
        if (msPerPx <= 100.0)   // the first second is loud: it draws at the start, not later
        {
            bool any = false;
            for (int y = 0; y < 38 && !any; y++)
                any = img.pixelColor(2, y) != QColor(Qt::black);
            QVERIFY2(any, "the loud first second is drawn");
        }
    }
    QVERIFY2(t.elapsed() < 1000, qPrintable(QString("five full redraws took %1 ms").arg(t.elapsed())));
}

void WaveformPeaks_Test::cacheIsKeyedByPathSizeAndMtime()
{
    QTemporaryDir dir;
    WaveformPeaks::setCacheDir(dir.filePath("cache"));
    const QString song = makeFile(dir, "song.wav", 1000);

    FakeDecoder d(2);
    const WaveformPeaks p = WaveformPeaks::compute(&d);
    WaveformPeaks back;
    QVERIFY(!WaveformPeaks::load(song, back));      // nothing yet
    QVERIFY(p.save(song));
    QVERIFY(QFileInfo::exists(WaveformPeaks::cacheFileFor(song)));
    QVERIFY(WaveformPeaks::load(song, back));
    QCOMPARE(back.bucketCount(), p.bucketCount());
    QCOMPARE(back.maxAt(0, 50), 64);

    // another path has another cache file
    const QString other = makeFile(dir, "other.wav", 1000);
    QVERIFY(WaveformPeaks::cacheFileFor(other) != WaveformPeaks::cacheFileFor(song));
    QVERIFY(!WaveformPeaks::load(other, back));

    // same size, later mtime: stale
    QFile f(song);
    QVERIFY(f.open(QIODevice::ReadWrite));
    QVERIFY(f.setFileTime(QDateTime::currentDateTime().addSecs(60), QFileDevice::FileModificationTime));
    f.close();
    QVERIFY(!WaveformPeaks::load(song, back));

    // fresh again once rewritten and saved, then a different size is stale too
    QVERIFY(p.save(song));
    QVERIFY(WaveformPeaks::load(song, back));
    QFile g(song);
    QVERIFY(g.open(QIODevice::Append));
    g.write("more");
    g.close();
    QVERIFY(!WaveformPeaks::load(song, back));
    WaveformPeaks::setCacheDir(QString());
}

void WaveformPeaks_Test::serviceBuildsOnceAndCaches()
{
    QTemporaryDir dir;
    WaveformPeaks::setCacheDir(dir.filePath("cache"));
    const QString song = makeFile(dir, "a.wav", 500);

    WaveformService *s = WaveformService::instance();
    int made = 0;
    auto factory = [&made]() -> AudioDecoder * { made++; return new FakeDecoder(2); };
    const int builds = s->builds();

    QSignalSpy spy(s, &WaveformService::ready);
    QVERIFY(s->request(song, factory).isNull());    // not on the GUI thread: later
    QVERIFY(s->request(song, factory).isNull());    // still building: no second one
    QCOMPARE(made, 1);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 10000);
    QCOMPARE(spy.first().first().toString(), song);

    const WaveformService::PeaksPtr p = s->request(song, factory);
    QVERIFY(!p.isNull());
    QCOMPARE(p->bucketCount(), 200);
    QCOMPARE(made, 1);
    QCOMPARE(s->builds(), builds + 1);
    QVERIFY(QFileInfo::exists(WaveformPeaks::cacheFileFor(song)));  // kept for the next session
    WaveformPeaks::setCacheDir(QString());
}

void WaveformPeaks_Test::serviceRebuildsWhenTheFileChanges()
{
    QTemporaryDir dir;
    WaveformPeaks::setCacheDir(dir.filePath("cache"));
    const QString song = makeFile(dir, "b.wav", 500);

    WaveformService *s = WaveformService::instance();
    int secs = 2;
    auto factory = [&secs]() -> AudioDecoder * { return new FakeDecoder(secs); };
    QSignalSpy spy(s, &WaveformService::ready);
    s->request(song, factory);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 10000);
    QCOMPARE(s->request(song, factory)->bucketCount(), 200);

    // the file is replaced by a longer one
    secs = 4;
    QFile f(song);
    QVERIFY(f.open(QIODevice::Append));
    f.write("longer");
    f.close();
    QVERIFY(s->request(song, factory).isNull());    // the old envelope is gone
    QVERIFY(s->lookup(song).isNull());
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 10000);
    QCOMPARE(s->request(song, factory)->bucketCount(), 400);
    WaveformPeaks::setCacheDir(QString());
}

void WaveformPeaks_Test::unreadableFileIsRememberedNotRetried()
{
    QTemporaryDir dir;
    WaveformPeaks::setCacheDir(dir.filePath("cache"));
    const QString junk = makeFile(dir, "notaudio.wav", 100);
    WaveformService *s = WaveformService::instance();
    int asked = 0;
    auto factory = [&asked]() -> AudioDecoder * { asked++; return nullptr; };
    QVERIFY(s->request(junk, factory).isNull());
    QVERIFY(s->request(junk, factory).isNull());
    QCOMPARE(asked, 1);
    WaveformPeaks::setCacheDir(QString());
}

QTEST_MAIN(WaveformPeaks_Test)
