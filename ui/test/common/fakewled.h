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
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QList>
#include <QPair>
#include <utility>

/* A WLED on this computer for the tests (no network in the gate): it
   answers /json/cfg, /json/info and /json/state with what it holds, and
   takes a POST the way WLED does -- a partial config merged in (a new
   hw.led rebuilds the outputs, the count taking at once or only after a
   reboot), segments by id (stop 0 deletes one), "rb" a reboot. */
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
    void takeState(const QJsonObject &body)
    {
        if (body.value("rb").toBool())
        {
            ++reboots;
            setCount(ledTotal());
        }
        if (!body.value("seg").isArray())
            return;
        QJsonArray segs = segments();
        const QJsonArray sent = body.value("seg").toArray();
        for (const QJsonValue &v : sent)
        {
            const QJsonObject o = v.toObject();
            int id = o.value("id").toInt();
            if (id >= segs.size())
            {
                if (o.value("stop").toInt() <= 0)
                    continue;
                id = segs.size();
                segs << QJsonObject{ { "id", id }, { "start", 0 }, { "stop", 0 }, { "rev", false } };
            }
            QJsonObject s = segs.at(id).toObject();
            merge(s, o);
            s["id"] = id;
            segs[id] = s;
        }
        // Deleted ones are gone from what it says (WLED lists the active only).
        QJsonArray active;
        for (const QJsonValue &v : std::as_const(segs))
            if (v.toObject().value("stop").toInt() > v.toObject().value("start").toInt())
                active << v;
        state["seg"] = active;
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
