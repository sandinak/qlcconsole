/*
  Q Light Controller Plus
  padtraceplayer.h

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

#ifndef PADTRACEPLAYER_H
#define PADTRACEPLAYER_H

#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QtTest>
#include <functional>

#include "fakehidbackend.h"

/**
 * Plays a pad trace (QLC_HID_RECORD's .jsonl, hidtrace.h) through the real
 * HID plugin: devices arrive and leave in a FakeHIDBackend (virtual pads
 * behind the plugin's test seam) and every input is injected on the pad
 * that path has, with the recorded timing divided by the speed. A mode flip
 * mid-trace replays as the real one did: the old pad leaves, the new ones
 * come, at the same system path with a new system entry each.
 *
 * Events: "arrived" (an optional "open_ms": how long its open takes; -1
 * never answers), "gone", "input", "open_failed" ("stuck": that path is
 * listed and its open never answers). A second "arrived" at a path still
 * listed is the system listing the pad twice. Header only, like the fake.
 */
class PadTracePlayer
{
public:
    struct Event
    {
        qint64 t = 0;
        QString ev;
        QJsonObject o;
    };

    /** @p plugin: the HID plugin (loaded as the app does), @p fake: the
     *  backend a test already gave it with setTestBackend(). */
    PadTracePlayer(QObject *plugin, FakeHIDBackend *fake) : m_plugin(plugin), m_fake(fake) {}

    bool load(const QString &file, QString *error = nullptr)
    {
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly))
        {
            if (error) *error = file + ": cannot be read";
            return false;
        }
        m_events.clear();
        m_next = 0;
        int n = 0;
        while (!f.atEnd())
        {
            const QByteArray line = f.readLine().trimmed();
            n++;
            if (line.isEmpty() || line.startsWith('#'))
                continue;
            QJsonParseError pe;
            const QJsonDocument d = QJsonDocument::fromJson(line, &pe);
            if (!d.isObject())
            {
                if (error) *error = QStringLiteral("%1:%2: %3").arg(file).arg(n).arg(pe.errorString());
                return false;
            }
            Event e;
            e.o = d.object();
            e.t = qint64(e.o.value(QStringLiteral("t")).toDouble());
            e.ev = e.o.value(QStringLiteral("ev")).toString();
            m_events << e;
        }
        return !m_events.isEmpty();
    }

    const QList<Event> &events() const { return m_events; }
    bool atEnd() const { return m_next >= m_events.size(); }
    int position() const { return m_next; }

    /** Events @p skip says yes to are left out (their time still passes):
     *  a trace replayed without one press, to see what that press did. */
    void setSkip(const std::function<bool(const Event &)> &skip) { m_skip = skip; }

    /** Times are divided by @p speed (1: as recorded; 0: no waiting). */
    void setSpeed(double speed) { m_speed = speed; }

    /** Wait for the next event's time and apply it; returns it, or null at
     *  the end. A device event is followed by the plugin settling: its scan
     *  and every open that can finish having finished. */
    const Event *step()
    {
        if (atEnd())
            return nullptr;
        const Event &e = m_events.at(m_next++);
        if (m_speed > 0 && e.t > m_last)
            QTest::qWait(int((e.t - m_last) / m_speed));
        m_last = qMax(m_last, e.t);
        if (!m_skip || !m_skip(e))
            apply(e);
        return &e;
    }

    /** Every event left. */
    void playAll()
    {
        while (step() != nullptr)
            ;
    }

    /** Play up to and including the first event that @p stop says is the
     *  one (null if none). */
    template <typename Pred> const Event *playUntil(Pred stop)
    {
        while (const Event *e = step())
            if (stop(*e))
                return e;
        return nullptr;
    }

    /** Inputs that found no pad (its device not there): a trace that is
     *  consistent has none. */
    int orphanInputs() const { return m_orphans; }

private:
    void apply(const Event &e)
    {
        const QString path = e.o.value(QStringLiteral("path")).toString();
        if (e.ev == QLatin1String("arrived"))
        {
            // (A second arrival at a path still listed lists it twice.)
            const int openMs = e.o.value(QStringLiteral("open_ms")).toInt(0);
            m_fake->list(path, quint16(hex(e.o, "vid")), quint16(hex(e.o, "pid")),
                         e.o.value(QStringLiteral("name")).toString(), openMs, ++m_instance, false,
                         e.o.value(QStringLiteral("serial")).toString());
            m_listed[path]++;
            scan(openMs != FakeHIDBackend::Forever);
            if (openMs != FakeHIDBackend::Forever)
                waitFor(path, true, m_fake->opens(path));
        }
        else if (e.ev == QLatin1String("gone"))
        {
            m_fake->unlist(path);       // every listing of it
            m_listed[path] = 0;
            scan(true);
            waitFor(path, false, 0);
        }
        else if (e.ev == QLatin1String("open_failed")
                 && e.o.value(QStringLiteral("why")).toString() == QLatin1String("stuck"))
        {
            m_fake->list(path, quint16(hex(e.o, "vid")), quint16(hex(e.o, "pid")),
                         e.o.value(QStringLiteral("name")).toString(), FakeHIDBackend::Forever,
                         ++m_instance);
            m_listed[path]++;
            scan(false);
        }
        else if (e.ev == QLatin1String("input"))
        {
            int line = -1;
            QMetaObject::invokeMethod(m_plugin, "inputForPath", Qt::DirectConnection,
                                      Q_RETURN_ARG(int, line), Q_ARG(QString, path));
            if (line < 0)
            {
                m_orphans++;
                return;
            }
            bool ok = false;
            QMetaObject::invokeMethod(m_plugin, "injectVirtualInput", Qt::DirectConnection,
                                      Q_RETURN_ARG(bool, ok), Q_ARG(quint32, quint32(line)),
                                      Q_ARG(quint32, quint32(e.o.value(QStringLiteral("ch")).toInt())),
                                      Q_ARG(uint, uint(e.o.value(QStringLiteral("value")).toInt())));
            if (!ok)
                m_orphans++;
        }
    }

    static uint hex(const QJsonObject &o, const char *key)
    {
        return o.value(QLatin1String(key)).toString().toUInt(nullptr, 16);
    }

    /** Wait (1 s at most) until the plugin has a joystick at @p path (or has
     *  none), once the backend has been asked to open @p opens times. */
    void waitFor(const QString &path, bool present, int opens)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 1000)
        {
            int line = -1;
            QMetaObject::invokeMethod(m_plugin, "inputForPath", Qt::DirectConnection,
                                      Q_RETURN_ARG(int, line), Q_ARG(QString, path));
            int opening = 0;
            QMetaObject::invokeMethod(m_plugin, "openingCount", Qt::DirectConnection,
                                      Q_RETURN_ARG(int, opening));
            if ((line >= 0) == present && opening == 0 && (!present || m_fake->opens(path) >= opens))
                return;
            QTest::qWait(5);
        }
    }

    /** Ask for a scan and wait for it (and, @p opens, for what it opens). */
    void scan(bool opens)
    {
        QMetaObject::invokeMethod(m_plugin, "rescanDevices", Qt::DirectConnection);
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 3000)
        {
            QTest::qWait(5);
            bool scanning = false;
            int opening = 0;
            QMetaObject::invokeMethod(m_plugin, "isScanning", Qt::DirectConnection,
                                      Q_RETURN_ARG(bool, scanning));
            QMetaObject::invokeMethod(m_plugin, "openingCount", Qt::DirectConnection,
                                      Q_RETURN_ARG(int, opening));
            if (!scanning && (!opens || opening == 0))
                break;
        }
        /* The scan's and the opens' results come to the plugin as queued
           calls: take them. */
        for (int i = 0; i < 3; ++i)
            QTest::qWait(5);
        QCoreApplication::processEvents();
    }

    QObject *m_plugin;
    FakeHIDBackend *m_fake;
    QList<Event> m_events;
    int m_next = 0;
    qint64 m_last = 0;
    double m_speed = 1.0;
    quint64 m_instance = 0;
    int m_orphans = 0;
    std::function<bool(const Event &)> m_skip;
    QHash<QString, int> m_listed;
};

#endif
