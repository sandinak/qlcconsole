/*
  Q Light Controller Plus - test support
  hostspeed.h

  How much slower this machine is, right now, than the one a wall-clock
  budget was measured on (the pool's newaudio: Apple silicon, 8 performance
  cores, Qt 6.12.0, Release, 2026-11-27). The gate runs on whichever pool
  host is free -- val, an Intel i7 iMac often busy with its owner's work,
  took four to five times as long for every part of the size check -- and
  so each budget is scaled by the median of five runs of a fixed piece of
  C++ (a fade over 26,000 channels) and of JavaScript (what an RGB matrix's
  script runs on), each against newaudio's time, the larger of the two, at
  least 1 and at most kMaxFactor. A machine slower than that fails its
  budgets: it is not one a show of that size should run on.
  Used by ui/test/showscale and ui/test/renderperf (board Q360). Needs Qt Qml.

  Licensed under the Apache License, Version 2.0
*/

#ifndef QLC_TEST_HOSTSPEED_H
#define QLC_TEST_HOSTSPEED_H

#include <QElapsedTimer>
#include <QJSEngine>
#include <QString>
#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace HostSpeed
{
const double kRefCppMs = 23.0;  // newaudio, Release, 2026-11-27 (2.3 ms at a tenth of the passes)
const double kRefJsMs = 32.9;   // newaudio, Release, 2026-11-27
const double kMaxFactor = 6.0;

/** The value at fraction @a q (0.95: p95) of @a v */
inline double quantile(std::vector<double> v, double q)
{
    if (v.empty())
        return 0;
    std::sort(v.begin(), v.end());
    const size_t i = std::min(v.size() - 1, size_t(q * double(v.size() - 1) + 0.5));
    return v[i];
}

inline double medianMs(const std::function<void()> &op)
{
    std::vector<double> ms;
    for (int i = 0; i < 5; ++i)
    {
        QElapsedTimer t;
        t.start();
        op();
        ms.push_back(t.nsecsElapsed() / 1e6);
    }
    return quantile(ms, 0.5);
}

/** The budgets' factor (1 on newaudio or faster); @a text says how it was found */
inline double factor(QString *text = nullptr)
{
    static volatile unsigned sink = 0;
    const double cppMs = medianMs([]() {
        std::vector<unsigned char> from(26400), to(26400), out(26400);
        for (size_t i = 0; i < from.size(); ++i)
        {
            from[i] = (unsigned char)(i * 7);
            to[i] = (unsigned char)(i * 13 + 5);
        }
        unsigned sum = 0;
        for (int pass = 0; pass < 4000; ++pass)
        {
            const unsigned k = unsigned(pass % 256);
            for (size_t i = 0; i < out.size(); ++i)
                out[i] = (unsigned char)((from[i] * (255 - k) + to[i] * k) / 255);
            sum += out[size_t(pass) % out.size()];
            std::swap(from, out);
        }
        sink = sink + sum;
    });
    QJSEngine js;
    const double jsMs = medianMs([&js]() {
        js.evaluate(QStringLiteral("var s = 0; for (var i = 0; i < 300000; i++) "
                                   "s += Math.floor(Math.sin(i) * 255) & 255; s"));
    });
    const double f = std::min(kMaxFactor, std::max(1.0, std::max(cppMs / kRefCppMs, jsMs / kRefJsMs)));
    if (text != nullptr)
        *text = QString::asprintf("host: C++ %.1f ms, JS %.1f ms (newaudio %.1f, %.1f): budgets x%.2f",
                                  cppMs, jsMs, kRefCppMs, kRefJsMs, f);
    return f;
}
}

#endif
