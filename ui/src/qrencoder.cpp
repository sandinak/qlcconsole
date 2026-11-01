/*
  Q Light Controller Plus
  qrencoder.cpp

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

#include <QtGlobal>
#include <algorithm>
#include <climits>

#include "qrencoder.h"

namespace {

const int kMaxVersion = 10;
// Error correction level M, by version (index 0 unused).
const int kEccPerBlock[kMaxVersion + 1] = { -1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26 };
const int kBlocks[kMaxVersion + 1] = { -1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5 };
const int kAlign[kMaxVersion + 1][4] = {
    { 0 }, { 0 }, { 6, 18, 0 }, { 6, 22, 0 }, { 6, 26, 0 }, { 6, 30, 0 }, { 6, 34, 0 },
    { 6, 22, 38, 0 }, { 6, 24, 42, 0 }, { 6, 26, 46, 0 }, { 6, 28, 50, 0 } };

int rawModules(int ver)
{
    int result = (16 * ver + 128) * ver + 64;
    if (ver >= 2)
    {
        const int na = ver / 7 + 2;
        result -= (25 * na - 10) * na - 55;
        if (ver >= 7)
            result -= 36;
    }
    return result;
}

int rawCodewords(int ver)
{
    return rawModules(ver) / 8;
}

int dataCodewords(int ver)
{
    return rawCodewords(ver) - kEccPerBlock[ver] * kBlocks[ver];
}

quint8 gfMul(quint8 x, quint8 y)
{
    int z = 0;
    for (int i = 7; i >= 0; --i)
    {
        z = (z << 1) ^ ((z >> 7) * 0x11D);
        z ^= ((y >> i) & 1) * x;
    }
    return quint8(z);
}

QByteArray rsDivisor(int degree)
{
    QByteArray result(degree, 0);
    result[degree - 1] = 1;
    quint8 root = 1;
    for (int i = 0; i < degree; ++i)
    {
        for (int j = 0; j < degree; ++j)
        {
            result[j] = char(gfMul(quint8(result[j]), root));
            if (j + 1 < degree)
                result[j] = char(quint8(result[j]) ^ quint8(result[j + 1]));
        }
        root = gfMul(root, 0x02);
    }
    return result;
}

QByteArray rsRemainder(const QByteArray &data, const QByteArray &divisor)
{
    QByteArray result(divisor.size(), 0);
    for (char b : data)
    {
        const quint8 factor = quint8(b) ^ quint8(result[0]);
        result.remove(0, 1);
        result.append(char(0));
        for (int i = 0; i < divisor.size(); ++i)
            result[i] = char(quint8(result[i]) ^ gfMul(quint8(divisor[i]), factor));
    }
    return result;
}

struct Grid
{
    int size;
    QVector<quint8> dark;
    QVector<quint8> fixed;      // function modules: never masked, never data

    explicit Grid(int s) : size(s), dark(s * s, 0), fixed(s * s, 0) {}
    bool in(int x, int y) const { return x >= 0 && y >= 0 && x < size && y < size; }
    void setFunction(int x, int y, bool on)
    {
        dark[y * size + x] = on ? 1 : 0;
        fixed[y * size + x] = 1;
    }
    bool at(int x, int y) const { return dark[y * size + x] != 0; }
};

void drawFormat(Grid &g, int mask)
{
    // Level M is 00.
    const int data = (0 << 3) | mask;
    int rem = data;
    for (int i = 0; i < 10; ++i)
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    const int bits = ((data << 10) | rem) ^ 0x5412;
    auto bit = [bits](int i) { return ((bits >> i) & 1) != 0; };
    for (int i = 0; i <= 5; ++i)
        g.setFunction(8, i, bit(i));
    g.setFunction(8, 7, bit(6));
    g.setFunction(8, 8, bit(7));
    g.setFunction(7, 8, bit(8));
    for (int i = 9; i < 15; ++i)
        g.setFunction(14 - i, 8, bit(i));
    for (int i = 0; i < 8; ++i)
        g.setFunction(g.size - 1 - i, 8, bit(i));
    for (int i = 8; i < 15; ++i)
        g.setFunction(8, g.size - 15 + i, bit(i));
    g.setFunction(8, g.size - 8, true);
}

void drawVersion(Grid &g, int ver)
{
    if (ver < 7)
        return;
    int rem = ver;
    for (int i = 0; i < 12; ++i)
        rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
    const int bits = (ver << 12) | rem;
    for (int i = 0; i < 18; ++i)
    {
        const bool on = ((bits >> i) & 1) != 0;
        const int a = g.size - 11 + i % 3;
        const int b = i / 3;
        g.setFunction(a, b, on);
        g.setFunction(b, a, on);
    }
}

void drawFinder(Grid &g, int cx, int cy)
{
    for (int dy = -4; dy <= 4; ++dy)
        for (int dx = -4; dx <= 4; ++dx)
        {
            const int dist = qMax(qAbs(dx), qAbs(dy));
            if (g.in(cx + dx, cy + dy))
                g.setFunction(cx + dx, cy + dy, dist != 2 && dist != 4);
        }
}

void drawAlignment(Grid &g, int cx, int cy)
{
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx)
            g.setFunction(cx + dx, cy + dy, qMax(qAbs(dx), qAbs(dy)) != 1);
}

Grid functionPatterns(int ver)
{
    Grid g(ver * 4 + 17);
    for (int i = 0; i < g.size; ++i)
    {
        g.setFunction(6, i, i % 2 == 0);
        g.setFunction(i, 6, i % 2 == 0);
    }
    drawFinder(g, 3, 3);
    drawFinder(g, g.size - 4, 3);
    drawFinder(g, 3, g.size - 4);
    int n = 0;
    while (kAlign[ver][n] != 0)
        ++n;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            if ((i == 0 && j == 0) || (i == 0 && j == n - 1) || (i == n - 1 && j == 0))
                continue;
            drawAlignment(g, kAlign[ver][i], kAlign[ver][j]);
        }
    drawFormat(g, 0);       // placeholder: reserves the modules
    drawVersion(g, ver);
    return g;
}

bool maskBit(int mask, int x, int y)
{
    switch (mask)
    {
        case 0: return (x + y) % 2 == 0;
        case 1: return y % 2 == 0;
        case 2: return x % 3 == 0;
        case 3: return (x + y) % 3 == 0;
        case 4: return (x / 3 + y / 2) % 2 == 0;
        case 5: return x * y % 2 + x * y % 3 == 0;
        case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
        default: return ((x + y) % 2 + x * y % 3) % 2 == 0;
    }
}

void applyMask(Grid &g, int mask)
{
    for (int y = 0; y < g.size; ++y)
        for (int x = 0; x < g.size; ++x)
            if (!g.fixed[y * g.size + x] && maskBit(mask, x, y))
                g.dark[y * g.size + x] ^= 1;
}

int penalty(const Grid &g)
{
    int score = 0;
    const int n = g.size;
    // Runs of five or more, in rows and columns; the finder-like 1:1:3:1:1.
    for (int dir = 0; dir < 2; ++dir)
    {
        for (int a = 0; a < n; ++a)
        {
            QString line;
            int run = 0;
            bool last = false;
            for (int b = 0; b < n; ++b)
            {
                const bool v = dir == 0 ? g.at(b, a) : g.at(a, b);
                line += v ? QLatin1Char('1') : QLatin1Char('0');
                if (b > 0 && v == last)
                {
                    ++run;
                    if (run == 5)
                        score += 3;
                    else if (run > 5)
                        ++score;
                }
                else
                {
                    run = 1;
                }
                last = v;
            }
            for (const char *pat : { "10111010000", "00001011101" })
            {
                int from = 0;
                while ((from = line.indexOf(QLatin1String(pat), from)) >= 0)
                {
                    score += 40;
                    ++from;
                }
            }
        }
    }
    for (int y = 0; y + 1 < n; ++y)
        for (int x = 0; x + 1 < n; ++x)
        {
            const bool c = g.at(x, y);
            if (c == g.at(x + 1, y) && c == g.at(x, y + 1) && c == g.at(x + 1, y + 1))
                score += 3;
        }
    int dark = 0;
    for (quint8 d : g.dark)
        dark += d;
    const int total = n * n;
    const int k = (qAbs(dark * 20 - total * 10) + total - 1) / total - 1;
    score += qMax(0, k) * 10;
    return score;
}

} // namespace

QVector<quint8> QrEncoder::encode(const QString &text, int *size, int mask, int *version)
{
    const QByteArray bytes = text.toUtf8();
    if (size != nullptr)
        *size = 0;
    if (bytes.isEmpty() || bytes.size() > MaxBytes)
        return QVector<quint8>();

    // Smallest version that holds mode (4) + count (8 / 16) + bytes.
    int ver = 1;
    for (; ver <= kMaxVersion; ++ver)
    {
        const int countBits = ver < 10 ? 8 : 16;
        if (4 + countBits + bytes.size() * 8 <= dataCodewords(ver) * 8)
            break;
    }
    if (ver > kMaxVersion)
        return QVector<quint8>();
    if (version != nullptr)
        *version = ver;

    // The bit stream: byte mode, count, data, terminator, padding.
    QVector<bool> bits;
    auto put = [&bits](quint32 value, int count) {
        for (int i = count - 1; i >= 0; --i)
            bits << (((value >> i) & 1) != 0);
    };
    put(0x4, 4);
    put(quint32(bytes.size()), ver < 10 ? 8 : 16);
    for (char b : bytes)
        put(quint8(b), 8);
    const int capacity = dataCodewords(ver) * 8;
    put(0, qMin(4, capacity - int(bits.size())));
    while (bits.size() % 8 != 0)
        bits << false;
    QByteArray data;
    for (int i = 0; i < bits.size(); i += 8)
    {
        int v = 0;
        for (int j = 0; j < 8; ++j)
            v = (v << 1) | (bits[i + j] ? 1 : 0);
        data.append(char(v));
    }
    for (quint8 pad = 0xEC; data.size() < dataCodewords(ver); pad ^= 0xEC ^ 0x11)
        data.append(char(pad));

    // Blocks, each with its error correction, interleaved.
    const int blocks = kBlocks[ver];
    const int ecc = kEccPerBlock[ver];
    const int raw = rawCodewords(ver);
    const int shortBlocks = blocks - raw % blocks;
    const int shortLen = raw / blocks;
    const QByteArray divisor = rsDivisor(ecc);
    QVector<QByteArray> dat, eccs;
    for (int i = 0, k = 0; i < blocks; ++i)
    {
        const int len = shortLen - ecc + (i < shortBlocks ? 0 : 1);
        const QByteArray d = data.mid(k, len);
        k += len;
        dat << d;
        eccs << rsRemainder(d, divisor);
    }
    QByteArray all;
    for (int i = 0; i < shortLen - ecc + 1; ++i)
        for (int j = 0; j < blocks; ++j)
            if (i < dat[j].size())
                all.append(dat[j][i]);
    for (int i = 0; i < ecc; ++i)
        for (int j = 0; j < blocks; ++j)
            all.append(eccs[j][i]);

    // Place the codewords in the zigzag, then mask.
    Grid base = functionPatterns(ver);
    int bit = 0;
    for (int right = base.size - 1; right >= 1; right -= 2)
    {
        if (right == 6)
            right = 5;
        for (int vert = 0; vert < base.size; ++vert)
            for (int j = 0; j < 2; ++j)
            {
                const int x = right - j;
                const bool upward = ((right + 1) & 2) == 0;
                const int y = upward ? base.size - 1 - vert : vert;
                if (!base.fixed[y * base.size + x] && bit < all.size() * 8)
                {
                    base.dark[y * base.size + x] = (quint8(all[bit >> 3]) >> (7 - (bit & 7))) & 1;
                    ++bit;
                }
            }
    }

    int best = mask;
    if (best < 0 || best > 7)
    {
        int bestScore = INT_MAX;
        for (int m = 0; m < 8; ++m)
        {
            Grid g = base;
            applyMask(g, m);
            drawFormat(g, m);
            const int s = penalty(g);
            if (s < bestScore)
            {
                bestScore = s;
                best = m;
            }
        }
    }
    applyMask(base, best);
    drawFormat(base, best);
    if (size != nullptr)
        *size = base.size;
    return base.dark;
}

QImage QrEncoder::image(const QString &text, int scale)
{
    int n = 0;
    const QVector<quint8> m = encode(text, &n);
    if (m.isEmpty())
        return QImage();
    const int quiet = 4;
    const int px = (n + 2 * quiet) * scale;
    QImage img(px, px, QImage::Format_RGB32);
    img.fill(Qt::white);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            if (m[y * n + x])
                for (int dy = 0; dy < scale; ++dy)
                    for (int dx = 0; dx < scale; ++dx)
                        img.setPixel((x + quiet) * scale + dx, (y + quiet) * scale + dy, qRgb(0, 0, 0));
    return img;
}
