/*
  Q Light Controller Plus
  fakewled.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef FAKEWLED_H
#define FAKEWLED_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QList>
#include <QPair>
#include <algorithm>
#include <utility>

/* A WLED on this computer for the tests (no network in the gate): it
   answers /json/cfg, /json/info, /json/state and /json/si (state and info
   in one, what the WLED output asks) with what it holds, and takes a POST
   the way WLED does -- a partial config merged in (a new hw.led rebuilds
   the outputs, the count taking at once or only after a reboot), "rb" a
   reboot, and segments as WLED's json.cpp reads "seg": an array element
   applies to the segment its "id" names or, with no id, the one at its
   INDEX in the array (`byte id = elem["id"] | it`); an id it does not have
   makes a new segment only with a stop, and stop 0 deletes one; a "seg"
   OBJECT without an id applies to every selected segment (all, here). */
class FakeWled : public QTcpServer
{
public:
    QJsonObject cfg;
    QJsonObject info{ { "ver", "16.0.1" },
                      { "leds", QJsonObject{ { "count", 30 }, { "maxseg", 32 } } } };
    QJsonObject state{ { "on", true },
                       { "seg", QJsonArray{ QJsonObject{ { "id", 0 }, { "start", 0 }, { "stop", 30 },
                                                         { "rev", false } } } } };
    /** Every POST: its path and body. */
    QList<QPair<QByteArray, QJsonObject> > posted;
    /** When each of them came (ms since the epoch), for a check of what
     *  the device showed when (the UX walker's Locate flash). */
    QList<qint64> postedAt;
    /** A new LED count takes only on a reboot (an old WLED). */
    bool countNeedsReboot = false;
    int reboots = 0;
    /** Hangs up on every request (a device gone wrong). */
    bool silent = false;

    FakeWled()
    {
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *s = nextPendingConnection())
                connect(s, &QTcpSocket::readyRead, s, [this, s]() { serve(s); });
        });
    }

    /** "127.0.0.1:port", as a fixture's IP address. */
    QString address() const { return QStringLiteral("127.0.0.1:%1").arg(serverPort()); }

    /** One output of @p len LEDs, as the real one says it. */
    static QJsonObject ledCfg(int len, int outputs = 1)
    {
        QJsonArray ins;
        for (int k = 0; k < outputs; ++k)
            ins << QJsonObject{ { "start", k * len }, { "len", len }, { "pin", QJsonArray{ 16 + k } },
                                { "order", 0 }, { "rev", false }, { "skip", 0 }, { "type", 22 },
                                { "ref", false }, { "rgbwm", 0 }, { "freq", 0 }, { "maxpwr", 850 },
                                { "ledma", 55 }, { "drv", 0 }, { "text", "" } };
        QJsonObject led{ { "total", len * outputs }, { "maxpwr", 850 }, { "cct", false }, { "cr", false },
                         { "ic", false }, { "cb", 0 }, { "fps", 42 }, { "rgbwm", 255 }, { "ins", ins } };
        return QJsonObject{ { "led", led } };
    }

    QJsonArray segments() const { return state.value("seg").toArray(); }

    /** As a real device answered (its /json/info, /json/state, /json/cfg
     *  as captured): the UX walker's office WLED. */
    void seed(const QJsonObject &i, const QJsonObject &st, const QJsonObject &c)
    {
        if (!i.isEmpty())
            info = i;
        if (!st.isEmpty())
            state = st;
        if (!c.isEmpty())
            cfg = c;
    }

private:
    int ledTotal() const
    {
        return cfg.value("hw").toObject().value("led").toObject().value("total").toInt();
    }
    void setCount(int n)
    {
        QJsonObject l = info.value("leds").toObject();
        l["count"] = n;
        info["leds"] = l;
    }
    static void merge(QJsonObject &into, const QJsonObject &from)
    {
        for (auto it = from.begin(); it != from.end(); ++it)
        {
            if (it.value().isObject() && into.value(it.key()).isObject())
            {
                QJsonObject sub = into.value(it.key()).toObject();
                merge(sub, it.value().toObject());
                into[it.key()] = sub;
            }
            else
                into[it.key()] = it.value();
        }
    }
    /** Merges @p o into the segment @p id; a new one only with a stop. */
    static void takeSegment(QJsonArray &segs, int id, const QJsonObject &o)
    {
        for (int k = 0; k < segs.size(); ++k)
        {
            if (segs.at(k).toObject().value("id").toInt() != id)
                continue;
            QJsonObject s = segs.at(k).toObject();
            merge(s, o);
            s["id"] = id;
            segs[k] = s;
            return;
        }
        if (o.value("stop").toInt() <= 0)
            return;                 // none such, nothing to make: ignored
        QJsonObject s{ { "id", id }, { "start", 0 }, { "stop", 0 }, { "rev", false }, { "fx", 0 } };
        merge(s, o);
        s["id"] = id;
        segs << s;
    }
    void takeState(const QJsonObject &body)
    {
        if (body.value("rb").toBool())
        {
            ++reboots;
            setCount(ledTotal());
        }
        for (auto it = body.begin(); it != body.end(); ++it)
            if (it.key() == "on" || it.key() == "bri")
                state[it.key()] = it.value();
        QJsonArray segs = segments();
        const QJsonValue v = body.value("seg");
        if (v.isObject())
        {
            const QJsonObject o = v.toObject();
            if (o.contains("id"))
                takeSegment(segs, o.value("id").toInt(), o);
            else
                for (const QJsonValue &each : segments())
                    takeSegment(segs, each.toObject().value("id").toInt(), o);
        }
        else if (v.isArray())
        {
            const QJsonArray sent = v.toArray();
            for (int i = 0; i < sent.size(); ++i)
            {
                const QJsonObject o = sent.at(i).toObject();
                takeSegment(segs, o.value("id").toInt(i), o);
            }
        }
        else
            return;
        // Deleted ones are gone from what it says (WLED lists the active only).
        QList<QJsonObject> active;
        for (const QJsonValue &each : std::as_const(segs))
            if (each.toObject().value("stop").toInt() > each.toObject().value("start").toInt())
                active << each.toObject();
        std::sort(active.begin(), active.end(), [](const QJsonObject &a, const QJsonObject &b) {
            return a.value("id").toInt() < b.value("id").toInt();
        });
        QJsonArray out;
        for (const QJsonObject &each : std::as_const(active))
            out << each;
        state["seg"] = out;
    }

    void serve(QTcpSocket *s)
    {
        if (silent)
        {
            s->abort();             // hangs up without a word
            return;
        }
        m_buffer[s] += s->readAll();
        const QByteArray &req = m_buffer[s];
        const int head = req.indexOf("\r\n\r\n");
        if (head < 0)
            return;
        int length = 0;
        foreach (const QByteArray &line, req.left(head).split('\n'))
            if (line.toLower().startsWith("content-length:"))
                length = line.mid(15).trimmed().toInt();
        if (req.size() < head + 4 + length)
            return;
        const QByteArray path = req.split(' ').value(1);
        QByteArray out;
        if (req.startsWith("POST"))
        {
            const QJsonObject body = QJsonDocument::fromJson(req.mid(head + 4, length)).object();
            posted << qMakePair(path, body);
            postedAt << QDateTime::currentMSecsSinceEpoch();
            if (path.contains("cfg"))
            {
                merge(cfg, body);
                if (body.value("hw").toObject().contains("led") && !countNeedsReboot)
                    setCount(ledTotal());
            }
            else
                takeState(body);
            out = "{\"success\":true}";
        }
        else if (path.endsWith("/json/info"))
            out = QJsonDocument(info).toJson(QJsonDocument::Compact);
        else if (path.endsWith("/json/state"))
            out = QJsonDocument(state).toJson(QJsonDocument::Compact);
        else if (path.endsWith("/json/si"))
            out = QJsonDocument(QJsonObject{ { "state", state }, { "info", info } }).toJson(QJsonDocument::Compact);
        else
            out = QJsonDocument(cfg).toJson(QJsonDocument::Compact);
        s->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n"
                 "Content-Length: " + QByteArray::number(out.size()) + "\r\n\r\n" + out);
        s->flush();
        s->disconnectFromHost();
        m_buffer.remove(s);
    }
    QHash<QTcpSocket *, QByteArray> m_buffer;
};

#endif
