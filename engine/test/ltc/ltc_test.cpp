/*
  Q Light Controller Plus
  ltc_test.cpp

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

#include <QtTest>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <vector>

#include "ltcdecoder.h"
#include "ltcreceiver.h"
#include "timecodesource.h"
#include "doc.h"
#include "ltc_test.h"

namespace
{

struct Code
{
    int h, m, s, f;
    bool df;
};

/* The encoder lives here and only here: the 80-bit frame, sent as biphase
   mark (a transition at every bit start, another in the middle of a 1). */
class Encoder
{
public:
    Encoder(int rate, double fps, bool invert = false, int amplitude = 12000)
        : m_rate(rate), m_fps(fps), m_amp(amplitude), m_level(invert ? -1 : 1)
    {
    }

    static void frameBits(const Code &c, int bits[80])
    {
        for (int i = 0; i < 80; i++)
            bits[i] = 0;
        auto put = [&](int start, int count, int value) {
            for (int i = 0; i < count; i++)
                bits[start + i] = (value >> i) & 1;
        };
        put(0, 4, c.f % 10);
        put(8, 2, c.f / 10);
        bits[10] = c.df ? 1 : 0;
        put(16, 4, c.s % 10);
        put(24, 3, c.s / 10);
        put(32, 4, c.m % 10);
        put(40, 3, c.m / 10);
        put(48, 4, c.h % 10);
        put(56, 2, c.h / 10);
        // bit 27: keeps the number of zeros even, so the polarity is the same every frame
        int zeros = 0;
        for (int i = 0; i < 80; i++)
            if (i != 27 && i < 64 && bits[i] == 0)
                zeros++;
        zeros += 2 + 1;        // the sync word's three zeros
        bits[27] = (zeros % 2) ? 0 : 1;
        static const int sync[16] = { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1 };
        for (int i = 0; i < 16; i++)
            bits[64 + i] = sync[i];
    }

    void frame(const Code &c, std::vector<int16_t> &out)
    {
        int bits[80];
        frameBits(c, bits);
        const double bitSamples = double(m_rate) / (m_fps * 80.0);
        for (int b = 0; b < 80; b++)
        {
            const double start = m_t;
            m_level = -m_level;                          // the bit's own transition
            const double mid = start + bitSamples / 2.0;
            const double end = start + bitSamples;
            for (; double(m_n) < end; m_n++)
            {
                if (bits[b] && double(m_n) >= mid && !m_flipped)
                {
                    m_level = -m_level;
                    m_flipped = true;
                }
                out.push_back(int16_t(m_level * m_amp));
            }
            m_flipped = false;
            m_t = end;
        }
    }

    void silence(double seconds, std::vector<int16_t> &out)
    {
        const long n = long(seconds * m_rate);
        for (long i = 0; i < n; i++)
            out.push_back(0);
        m_n += n;
        m_t += double(n);
    }

private:
    int m_rate;
    double m_fps;
    int m_amp;
    int m_level;
    double m_t = 0;
    long m_n = 0;
    bool m_flipped = false;
};

/* the code that follows a frame, stepping the way the rate counts */
Code next(Code c, int fps)
{
    if (++c.f < fps)
        return c;
    c.f = 0;
    if (++c.s < 60)
        return c;
    c.s = 0;
    if (++c.m < 60)
    {
        // drop frame: frames 0 and 1 do not exist at a minute that is not a tenth
        if (c.df && c.m % 10 != 0)
            c.f = 2;
        return c;
    }
    c.m = 0;
    c.h++;
    return c;
}

struct Got
{
    std::vector<LtcDecoder::Frame> frames;
};

void collect(void *context, const LtcDecoder::Frame &f)
{
    static_cast<Got *>(context)->frames.push_back(f);
}

bool same(const LtcDecoder::Frame &f, const Code &c)
{
    return f.hours == c.h && f.minutes == c.m && f.seconds == c.s && f.frames == c.f &&
           f.dropFrame == c.df;
}

QString text(const LtcDecoder::Frame &f)
{
    return QStringLiteral("%1:%2:%3:%4%5").arg(f.hours).arg(f.minutes).arg(f.seconds)
                                          .arg(f.frames).arg(f.dropFrame ? "df" : "");
}

/* the frame is decoded in blocks the size of the audio capture's */
void feed(LtcDecoder &d, const std::vector<int16_t> &samples)
{
    const int block = 2048;
    for (size_t at = 0; at < samples.size(); at += block)
        d.process(samples.data() + at, int(qMin(size_t(block), samples.size() - at)));
}

}

void Ltc_Test::rates_data()
{
    QTest::addColumn<int>("rate");
    QTest::addColumn<double>("exact");
    QTest::addColumn<int>("fps");
    QTest::addColumn<bool>("df");

    for (int rate : { 44100, 48000 })
    {
        QTest::newRow(qPrintable(QString("24@%1").arg(rate))) << rate << 24.0 << 24 << false;
        QTest::newRow(qPrintable(QString("25@%1").arg(rate))) << rate << 25.0 << 25 << false;
        QTest::newRow(qPrintable(QString("29.97df@%1").arg(rate))) << rate << 30000.0 / 1001.0 << 30 << true;
        QTest::newRow(qPrintable(QString("30@%1").arg(rate))) << rate << 30.0 << 30 << false;
    }
}

void Ltc_Test::rates()
{
    QFETCH(int, rate);
    QFETCH(double, exact);
    QFETCH(int, fps);
    QFETCH(bool, df);

    Got got;
    LtcDecoder d;
    d.setSampleRate(rate);
    d.setCallback(collect, &got);

    // Across a minute boundary (a drop at 29.97) and a second, with the frames in order
    Code c = { 1, 2, 59, fps - 12, df };
    std::vector<Code> sent;
    std::vector<int16_t> audio;
    Encoder enc(rate, exact);
    for (int i = 0; i < 60; i++)
    {
        sent.push_back(c);
        enc.frame(c, audio);
        c = next(c, fps);
    }
    feed(d, audio);

    // The first frames go to find the sync and learn the rate (and the last
    // has no edge after it to close it); every one between is read, in order.
    QVERIFY2(got.frames.size() >= 56, qPrintable(QString::number(got.frames.size())));
    size_t first = 0;
    while (first < sent.size() && !same(got.frames[0], sent[first]))
        first++;
    QVERIFY2(first <= 3, qPrintable(text(got.frames[0])));
    for (size_t i = 0; i < got.frames.size(); i++)
    {
        QVERIFY2(same(got.frames[i], sent[i + first]),
                 qPrintable(QString("%1 read as %2").arg(i).arg(text(got.frames[i]))));
        QCOMPARE(got.frames[i].fps, fps);
    }
    if (df)
    {
        // 01:03:00:02 is the frame after 01:02:59:29
        bool crossed = false;
        for (const LtcDecoder::Frame &f : got.frames)
            crossed = crossed || (f.minutes == 3 && f.seconds == 0 && f.frames == 2);
        QVERIFY(crossed);
    }
}

void Ltc_Test::polarity_data()
{
    QTest::addColumn<bool>("invert");
    QTest::addColumn<int>("amplitude");
    QTest::newRow("normal") << false << 12000;
    QTest::newRow("inverted") << true << 12000;
    QTest::newRow("quiet inverted") << true << 700;
    QTest::newRow("hot") << false << 32000;
}

void Ltc_Test::polarity()
{
    QFETCH(bool, invert);
    QFETCH(int, amplitude);

    Got got;
    LtcDecoder d;
    d.setSampleRate(48000);
    d.setCallback(collect, &got);
    Encoder enc(48000, 25.0, invert, amplitude);
    std::vector<int16_t> audio;
    Code c = { 10, 0, 0, 0, false };
    for (int i = 0; i < 30; i++)
    {
        enc.frame(c, audio);
        c = next(c, 25);
    }
    feed(d, audio);
    QVERIFY(got.frames.size() >= 26);
    QVERIFY2(same(got.frames.back(), { 10, 0, 1, 3, false }), qPrintable(text(got.frames.back())));
}

void Ltc_Test::softEdges()
{
    // Tape-ish: rounded edges (a moving average), hiss, and a DC offset
    Got got;
    LtcDecoder d;
    d.setSampleRate(44100);
    d.setCallback(collect, &got);
    Encoder enc(44100, 25.0);
    std::vector<int16_t> hard, soft;
    Code c = { 3, 30, 0, 0, false };
    std::vector<Code> sent;
    for (int i = 0; i < 50; i++)
    {
        sent.push_back(c);
        enc.frame(c, hard);
        c = next(c, 25);
    }
    unsigned noise = 1;
    for (size_t i = 0; i < hard.size(); i++)
    {
        int sum = 0;
        for (int k = 0; k < 4; k++)
            sum += hard[i >= size_t(k) ? i - k : 0];
        noise = noise * 1664525u + 1013904223u;
        soft.push_back(int16_t(sum / 4 + int((noise >> 16) % 801) - 400 + 500));
    }
    feed(d, soft);
    QVERIFY(got.frames.size() >= 44);
    QVERIFY2(same(got.frames.back(), sent[48]), qPrintable(text(got.frames.back())));
    // in order, none skipped
    for (size_t i = 1; i < got.frames.size(); i++)
    {
        Code prev = { got.frames[i - 1].hours, got.frames[i - 1].minutes, got.frames[i - 1].seconds,
                      got.frames[i - 1].frames, false };
        QVERIFY(same(got.frames[i], next(prev, 25)));
    }
}

void Ltc_Test::quietSignal()
{
    // Hiss and a low hum are not timecode
    Got got;
    LtcDecoder d;
    d.setSampleRate(48000);
    d.setCallback(collect, &got);
    std::vector<int16_t> audio;
    for (int i = 0; i < 48000; i++)
        audio.push_back(int16_t((i * 7919) % 61 - 30));
    for (int i = 0; i < 48000; i++)
        audio.push_back(int16_t(8000 * std::sin(i * 0.0654)));    // 500 Hz tone
    feed(d, audio);
    QCOMPARE(int(got.frames.size()), 0);
}

void Ltc_Test::dropout()
{
    Got got;
    LtcDecoder d;
    d.setSampleRate(44100);
    d.setCallback(collect, &got);
    Encoder enc(44100, 30.0);
    std::vector<int16_t> audio;
    Code c = { 0, 5, 0, 0, false };
    for (int i = 0; i < 20; i++)
    {
        enc.frame(c, audio);
        c = next(c, 30);
    }
    const size_t before = got.frames.size();
    feed(d, audio);
    const size_t first = got.frames.size();
    QVERIFY(first >= 16);
    QVERIFY(before == 0);

    // Half a second of nothing (the cable out), then it carries on from where it would be
    audio.clear();
    enc.silence(0.5, audio);
    for (int i = 0; i < 15; i++)           // 15 frames elapsed in the dropout
        c = next(c, 30);
    const Code resumed = c;
    for (int i = 0; i < 20; i++)
    {
        enc.frame(c, audio);
        c = next(c, 30);
    }
    feed(d, audio);

    QVERIFY2(got.frames.size() > first + 15, qPrintable(QString::number(got.frames.size())));
    // the first frame after it is read and is the right one
    QVERIFY2(same(got.frames[first], next(resumed, 30)) || same(got.frames[first], resumed) ||
             same(got.frames[first], next(next(resumed, 30), 30)),
             qPrintable(text(got.frames[first])));
    // 20 frames after the gap, the last of them (19) has no closing edge
    Code last = resumed;
    for (int i = 0; i < 18; i++)
        last = next(last, 30);
    QVERIFY2(same(got.frames.back(), last), qPrintable(text(got.frames.back())));
    QCOMPARE(got.frames.back().fps, 30);
}

void Ltc_Test::jump()
{
    Got got;
    LtcDecoder d;
    d.setSampleRate(48000);
    d.setCallback(collect, &got);
    Encoder enc(48000, 24.0);
    std::vector<int16_t> audio;
    Code c = { 0, 0, 10, 0, false };
    for (int i = 0; i < 24; i++)
    {
        enc.frame(c, audio);
        c = next(c, 24);
    }
    // no gap in the audio, the code just goes somewhere else: 00:42:00:00
    c = { 0, 42, 0, 0, false };
    for (int i = 0; i < 24; i++)
    {
        enc.frame(c, audio);
        c = next(c, 24);
    }
    feed(d, audio);

    bool before = false, after = false;
    for (const LtcDecoder::Frame &f : got.frames)
    {
        before = before || same(f, { 0, 0, 10, 20, false });
        after = after || same(f, { 0, 42, 0, 1, false });
    }
    QVERIFY(before);
    QVERIFY(after);
    // and still reads the rate
    QCOMPARE(got.frames.back().fps, 24);
    QVERIFY(same(got.frames.back(), { 0, 42, 0, 22, false }));
}

void Ltc_Test::milliseconds()
{
    LtcDecoder::Frame f;
    f.fps = 25;
    f.hours = 0; f.minutes = 1; f.seconds = 2; f.frames = 5;
    QCOMPARE(LtcDecoder::toMilliseconds(f, false), quint32(62000 + 200));
    // the frame is complete at its end: the time now is one frame on
    QCOMPARE(LtcDecoder::toMilliseconds(f), quint32(62000 + 240));

    f.fps = 24; f.frames = 12; f.seconds = 0; f.minutes = 0;
    QCOMPARE(LtcDecoder::toMilliseconds(f, false), quint32(500));

    // 29.97 df: 00:01:00:02 is frame 1800 of the real count, 60060 ms
    f.fps = 30; f.dropFrame = true;
    f.hours = 0; f.minutes = 1; f.seconds = 0; f.frames = 2;
    QCOMPARE(LtcDecoder::toMilliseconds(f, false), quint32(60060));
    // ten minutes in: 17982 frames, 599.999 s
    f.minutes = 10; f.frames = 0;
    QCOMPARE(LtcDecoder::toMilliseconds(f, false), quint32(599999));
}

void Ltc_Test::followPath()
{
    Doc doc(this);
    // This test feeds the audio itself: no capture device is opened.
    TimecodeSource *tc = doc.timecodeSource();
    TimecodeSource source;
    LtcReceiver rx(&doc, &source, false);
    QSignalSpy spy(&source, SIGNAL(timeChanged(quint32)));
    QSignalSpy running(&source, SIGNAL(runningChanged(bool)));

    // Not the picked source: nothing is passed on
    QVERIFY(!rx.isEnabled());
    source.setSourceUniverse(TimecodeSource::LtcUniverse);
    QVERIFY(rx.isEnabled());
    QCOMPARE(source.effectiveUniverse(), TimecodeSource::LtcUniverse);
    QVERIFY(tc->sourceUniverse() == -1);

    // Two channels, the code on the first and music on the second
    Encoder enc(48000, 25.0);
    std::vector<int16_t> mono;
    Code c = { 0, 0, 5, 0, false };
    for (int i = 0; i < 50; i++)
    {
        enc.frame(c, mono);
        c = next(c, 25);
    }
    std::vector<int16_t> stereo;
    for (size_t i = 0; i < mono.size(); i++)
    {
        stereo.push_back(mono[i]);
        stereo.push_back(int16_t(9000 * std::sin(double(i) * 0.05)));
    }
    for (size_t at = 0; at + 4096 <= stereo.size(); at += 4096)
        rx.processBlock(stereo.data() + at, 2048, 2, 48000);

    // queued from "the audio thread" to this one
    QTRY_VERIFY(spy.count() >= 45);
    QCOMPARE(source.lastUniverse(), TimecodeSource::LtcUniverse);
    QCOMPARE(source.fps(), 25);
    QVERIFY(source.isRunning());
    QCOMPARE(running.count(), 1);
    // the last frame read is at 00:00:06:xx, the position is the time now (one frame on)
    const quint32 ms = source.positionMs();
    QVERIFY2(ms > 6800 && ms <= 6960, qPrintable(QString::number(ms)));
    QCOMPARE(ms % 40, quint32(0));
    // its health is measured like any source's
    QVERIFY(source.health() != TimecodeSource::NoSource);

    // another source picked: LTC is let go and no longer passed on
    source.setSourceUniverse(3);
    QVERIFY(!rx.isEnabled());
    const int seen = spy.count();
    rx.processBlock(stereo.data(), 2048, 2, 48000);
    QTest::qWait(30);
    QCOMPARE(spy.count(), seen);
}

void Ltc_Test::sourceIsSaved()
{
    QByteArray bytes;
    {
        TimecodeSource a;
        a.setSourceUniverse(TimecodeSource::LtcUniverse);
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        w.writeStartDocument();
        QVERIFY(a.saveXML(&w));
        w.writeEndDocument();
    }
    TimecodeSource b;
    QXmlStreamReader r(bytes);
    while (!r.atEnd() && r.name() != KXMLQLCTimecode)
        r.readNext();
    QVERIFY(b.loadXML(r));
    QCOMPARE(b.sourceUniverse(), TimecodeSource::LtcUniverse);
}

QTEST_MAIN(Ltc_Test)
