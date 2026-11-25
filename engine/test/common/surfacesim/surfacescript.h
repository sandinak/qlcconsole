/*
  Q Light Controller Plus
  surfacescript.h

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

#ifndef SURFACESCRIPT_H
#define SURFACESCRIPT_H

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "simsurface.h"

/**
 * What a person did on a surface, as a script -- one JSON object a line
 * (.jsonl, as the HID plugin's pad traces are), '#' lines are comments:
 *
 *   {"ev":"tap","control":"Go","hold_ms":120}
 *   {"ev":"ramp","control":"Master","from":0,"to":127,"ms":300}
 *   {"ev":"press","control":"r2"}   {"ev":"release","control":"r2"}
 *   {"ev":"set","control":"lx","value":200}
 *   {"ev":"chord","controls":["l1","r1"],"hold_ms":60}
 *   {"ev":"raw","bytes":"98 00 7f"}       (MIDI, or one HID report)
 *   {"ev":"wait","ms":50}
 *   {"ev":"unplug"}   {"ev":"replug","dump":[["Master",64],["Go",0]]}
 *
 * A line may carry "t" (ms since the start): a capture's real timing
 * (tools/surface-capture.py writes "raw" lines so); the player waits until
 * then, scaled by the device's timeScale(). The same file drives a
 * simulated board in a test and documents the bench step it stands for.
 */
class SurfaceScript
{
public:
    /** Every line of @p text; false (and error()) on the first bad one. */
    bool parse(const QByteArray &text)
    {
        m_lines.clear();
        int n = 0;
        for (const QByteArray &raw : text.split('\n'))
        {
            n++;
            const QByteArray line = raw.trimmed();
            if (line.isEmpty() || line.startsWith('#'))
                continue;
            QJsonParseError pe;
            const QJsonDocument d = QJsonDocument::fromJson(line, &pe);
            if (!d.isObject())
                return bad(n, pe.errorString());
            const QString ev = d.object().value(QStringLiteral("ev")).toString();
            static const QStringList known = { "press", "release", "set", "tap", "ramp", "chord",
                                               "raw", "wait", "unplug", "replug", "jitter" };
            if (!known.contains(ev))
                return bad(n, QStringLiteral("unknown event '%1'").arg(ev));
            m_lines << qMakePair(n, d.object());
        }
        return true;
    }
    bool load(const QString &file)
    {
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly))
            return bad(0, file + QStringLiteral(": cannot be read"));
        return parse(f.readAll());
    }
    int size() const { return int(m_lines.size()); }
    QString error() const { return m_error; }

    /** Plays every line on @p dev; false at the first that fails, with
     *  the line and the device's words in error(). */
    bool play(SimSurface *dev)
    {
        QElapsedTimer clock;
        clock.start();
        for (const auto &l : std::as_const(m_lines))
        {
            const QJsonObject o = l.second;
            if (o.contains(QStringLiteral("t")))
            {
                const qint64 due = qint64(o.value(QStringLiteral("t")).toDouble() * dev->timeScale());
                const qint64 left = due - clock.elapsed();
                if (left > 0)
                {
                    dev->settle();
                    QTest::qWait(int(left));
                }
            }
            if (!apply(dev, o))
                return bad(l.first, dev->lastError().isEmpty() ? QStringLiteral("failed") : dev->lastError());
        }
        return true;
    }

    /** "98 00 7f" / "98007f" as bytes. */
    static QByteArray hexBytes(const QString &hex)
    {
        QString h = hex;
        h.remove(QLatin1Char(' '));
        return QByteArray::fromHex(h.toLatin1());
    }

private:
    static bool apply(SimSurface *dev, const QJsonObject &o)
    {
        const QString ev = o.value(QStringLiteral("ev")).toString();
        const QString c = o.value(QStringLiteral("control")).toString();
        if (ev == QLatin1String("press"))
            return dev->press(c);
        if (ev == QLatin1String("release"))
            return dev->release(c);
        if (ev == QLatin1String("set"))
            return dev->set(c, o.value(QStringLiteral("value")).toInt());
        if (ev == QLatin1String("tap"))
            return dev->tap(c, o.value(QStringLiteral("hold_ms")).toInt(40));
        if (ev == QLatin1String("ramp"))
            return dev->ramp(c, o.value(QStringLiteral("from")).toInt(), o.value(QStringLiteral("to")).toInt(),
                             o.value(QStringLiteral("ms")).toInt(100));
        if (ev == QLatin1String("jitter"))
            return dev->jitter(c, o.value(QStringLiteral("value")).toInt(), o.value(QStringLiteral("count")).toInt(10));
        if (ev == QLatin1String("chord"))
        {
            QStringList cs;
            for (const QJsonValue &v : o.value(QStringLiteral("controls")).toArray())
                cs << v.toString();
            return dev->chord(cs, o.value(QStringLiteral("hold_ms")).toInt(40));
        }
        if (ev == QLatin1String("raw"))
            return dev->raw(hexBytes(o.value(QStringLiteral("bytes")).toString()));
        if (ev == QLatin1String("wait"))
        {
            dev->wait(o.value(QStringLiteral("ms")).toInt());
            return true;
        }
        if (ev == QLatin1String("unplug"))
            return dev->unplug();
        if (ev == QLatin1String("replug"))
        {
            QList<QPair<QString, int>> dump;
            for (const QJsonValue &v : o.value(QStringLiteral("dump")).toArray())
                dump << qMakePair(v.toArray().at(0).toString(), v.toArray().at(1).toInt());
            return dev->replug(dump);
        }
        return false;
    }

    bool bad(int line, const QString &why)
    {
        m_error = line > 0 ? QStringLiteral("line %1: %2").arg(line).arg(why) : why;
        return false;
    }

    QList<QPair<int, QJsonObject>> m_lines;
    QString m_error;
};

#endif
