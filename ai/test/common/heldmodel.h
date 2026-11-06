/*
  Q Light Controller Plus - qlcconsole
  heldmodel.h

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

#ifndef HELDMODEL_H
#define HELDMODEL_H

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QPointer>
#include <QTimer>
#include <QHash>
#include <QList>

/**
 * For the UI tests of the command line's AI path: a model server on
 * 127.0.0.1 that counts the chat requests it gets and either holds each
 * one open (a model that is thinking) or answers it with the next of
 * replies (the last one repeats) after delayMs. The fuller fake is ai/test/aiassist's FakeModelServer.
 */
class HeldModel : public QTcpServer
{
public:
    HeldModel()
    {
        listen(QHostAddress::LocalHost, 0);
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *s = nextPendingConnection())
            {
                connect(s, &QTcpSocket::readyRead, this, [this, s]() { readFrom(s); });
                connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            }
        });
    }

    QString endpoint() const { return QStringLiteral("http://127.0.0.1:%1").arg(serverPort()); }

    /** Chat requests so far. */
    int requests = 0;
    /** Empty: hold every request open. Else the JSON to answer with, in
     *  order; the last repeats. */
    QList<QByteArray> replies;
    int delayMs = 0;

private:
    void readFrom(QTcpSocket *s)
    {
        QByteArray &buf = m_buffers[s];
        buf += s->readAll();
        const int end = buf.indexOf("\r\n\r\n");
        if (end < 0)
            return;
        int length = 0;
        for (const QByteArray &line : buf.left(end).split('\n'))
            if (line.toLower().startsWith("content-length:"))
                length = line.mid(15).trimmed().toInt();
        if (buf.size() < end + 4 + length)
            return;
        const bool chat = buf.startsWith("POST") && buf.left(end).contains("/chat/completions");
        m_buffers.remove(s);
        if (!chat)
        {
            s->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}");
            s->disconnectFromHost();
            return;
        }
        ++requests;
        if (replies.isEmpty())
            return;
        QPointer<QTcpSocket> later(s);
        const QByteArray body = replies.count() > 1 ? replies.takeFirst() : replies.first();
        QTimer::singleShot(delayMs, this, [later, body]() {
            if (later.isNull())
                return;
            later->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n"
                         "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
            later->disconnectFromHost();
        });
    }

    QHash<QTcpSocket *, QByteArray> m_buffers;
};

#endif
