/*
  Q Light Controller Plus
  hidtrace.cpp

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

#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include "hidtrace.h"
#include "hiddevice.h"

HIDTrace *HIDTrace::create(const QString &dir)
{
    if (dir.isEmpty() || !QDir().mkpath(dir))
        return nullptr;
    HIDTrace *t = new HIDTrace();
    const QString stem = QDir(dir).filePath(QStringLiteral("pads-")
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    QString name = stem + QStringLiteral(".jsonl");
    for (int n = 2; QFile::exists(name); ++n)
        name = QStringLiteral("%1-%2.jsonl").arg(stem).arg(n);
    t->m_file.setFileName(name);
    if (!t->m_file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        delete t;
        return nullptr;
    }
    t->m_clock.start();
    t->start(QThread::LowestPriority);
    return t;
}

HIDTrace::~HIDTrace()
{
    {
        QMutexLocker l(&m_lock);
        m_quit = true;
        if (m_dropped > 0)
            m_queue << QByteArray("{\"t\":") + QByteArray::number(now())
                       + ",\"ev\":\"dropped\",\"count\":" + QByteArray::number(m_dropped) + "}\n";
        m_wake.wakeAll();
    }
    wait();
}

void HIDTrace::push(QByteArray line)
{
    line += '\n';
    QMutexLocker l(&m_lock);
    if (m_queue.size() >= kMaxQueued)
    {
        m_dropped++;
        return;
    }
    m_queue << line;
    m_wake.wakeOne();
}

void HIDTrace::run()
{
    for (;;)
    {
        QList<QByteArray> batch;
        bool quit = false;
        {
            QMutexLocker l(&m_lock);
            while (m_queue.isEmpty() && !m_quit)
                m_wake.wait(&m_lock);
            batch.swap(m_queue);
            quit = m_quit;
        }
        for (const QByteArray &line : std::as_const(batch))
            m_file.write(line);
        m_file.flush();
        if (quit)
            break;
    }
    m_file.close();
}

static QString hex4(unsigned short v)
{
    return QStringLiteral("%1").arg(v, 4, 16, QLatin1Char('0'));
}

void HIDTrace::arrived(const HIDDevice *dev, const QString &serial)
{
    QJsonObject o;
    o[QStringLiteral("t")] = now();
    o[QStringLiteral("ev")] = QStringLiteral("arrived");
    o[QStringLiteral("path")] = dev->path();
    o[QStringLiteral("vid")] = hex4(dev->vendorId());
    o[QStringLiteral("pid")] = hex4(dev->productId());
    o[QStringLiteral("name")] = dev->name();
    o[QStringLiteral("serial")] = serial;
    o[QStringLiteral("location")] = dev->location();
    o[QStringLiteral("identity")] = identityFor(dev->name(), dev->location());
    o[QStringLiteral("axes")] = dev->axisCount();
    o[QStringLiteral("buttons")] = dev->buttonCount();
    push(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void HIDTrace::gone(const HIDDevice *dev)
{
    QJsonObject o;
    o[QStringLiteral("t")] = now();
    o[QStringLiteral("ev")] = QStringLiteral("gone");
    o[QStringLiteral("path")] = dev->path();
    o[QStringLiteral("name")] = dev->name();
    push(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void HIDTrace::input(const QString &path, quint32 channel, uchar value)
{
    QJsonObject o;
    o[QStringLiteral("t")] = now();
    o[QStringLiteral("ev")] = QStringLiteral("input");
    o[QStringLiteral("path")] = path;
    o[QStringLiteral("ch")] = int(channel);
    o[QStringLiteral("value")] = int(value);
    push(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void HIDTrace::openFailed(const QString &path, const QString &name, unsigned short vid,
                          unsigned short pid, const QString &why)
{
    QJsonObject o;
    o[QStringLiteral("t")] = now();
    o[QStringLiteral("ev")] = QStringLiteral("open_failed");
    o[QStringLiteral("path")] = path;
    o[QStringLiteral("name")] = name;
    o[QStringLiteral("vid")] = hex4(vid);
    o[QStringLiteral("pid")] = hex4(pid);
    o[QStringLiteral("why")] = why;
    push(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

/* Mirrors ControllerJobs::macIn() and identityFor(): "mac:<address>" for a
   name holding a Bluetooth address, else "port:HID:<location>", else
   "name:HID:<name>". */
QString HIDTrace::identityFor(const QString &name, const QString &location)
{
    static const QRegularExpression re(QStringLiteral(
        "(?<![0-9A-Fa-f:-])([0-9A-Fa-f]{2})([:-])([0-9A-Fa-f]{2})\\2([0-9A-Fa-f]{2})\\2"
        "([0-9A-Fa-f]{2})\\2([0-9A-Fa-f]{2})\\2([0-9A-Fa-f]{2})(?![0-9A-Fa-f:-])"));
    const QRegularExpressionMatch m = re.match(name);
    if (m.hasMatch())
    {
        QStringList parts;
        for (int i : { 1, 3, 4, 5, 6, 7 })
            parts << m.captured(i).toLower();
        return QStringLiteral("mac:") + parts.join(QLatin1Char(':'));
    }
    if (!location.trimmed().isEmpty())
        return QStringLiteral("port:HID:") + location.trimmed();
    return QStringLiteral("name:HID:") + name.trimmed();
}
