/*
  Q Light Controller Plus
  wledplugin.cpp

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

#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QDateTime>
#include <QJsonArray>
#include <QTimer>
#include <QDebug>

#include "wledplugin.h"
#include "wledstate.h"

WledPlugin::~WledPlugin()
{
}

void WledPlugin::init()
{
    m_nam = new QNetworkAccessManager(this);
    /* A WLED is on the show LAN, never behind a proxy -- and asking the
       system proxy settings (WinHTTP auto-detect on Windows) on every
       request, up to ten a second, costs the GUI thread. */
    m_nam->setProxy(QNetworkProxy::NoProxy);
    m_sendTimer = new QTimer(this);
    m_sendTimer->setInterval(m_sendInterval);
    connect(m_sendTimer, &QTimer::timeout, this, &WledPlugin::tick);
    m_sendTimer->start();
    m_infoTimer = new QTimer(this);
    m_infoTimer->setInterval(5000);
    connect(m_infoTimer, &QTimer::timeout, this, &WledPlugin::pollInfo);
    m_infoTimer->start();
    m_liveTimer = new QTimer(this);
    m_liveTimer->setInterval(150);
    connect(m_liveTimer, &QTimer::timeout, this, &WledPlugin::pollLive);
    m_liveTimer->start();
}

QString WledPlugin::name() const
{
    return QStringLiteral("WLED");
}

int WledPlugin::capabilities() const
{
    return QLCIOPlugin::Output;
}

QString WledPlugin::pluginInfo() const
{
    return QStringLiteral("<HTML><HEAD><TITLE>%1</TITLE></HEAD><BODY>"
                          "<H3>%1</H3>"
                          "<P>%2</P></BODY></HTML>")
        .arg(name(), tr("Drives WLED devices through their own JSON API rather than "
                        "Art-Net: the look's state is sent only when it changes, over "
                        "HTTP, with WLED's own transition between -- for WLED on Wi-Fi. "
                        "One universe per device, aimed at its IP; the WLED Effect and "
                        "WLED Lantern definitions drive it unchanged."));
}

void WledPlugin::setSendInterval(int ms)
{
    m_sendInterval = qMax(10, ms);
    if (m_sendTimer != nullptr)
        m_sendTimer->setInterval(m_sendInterval);
}

void WledPlugin::setInfoInterval(int ms)
{
    if (m_infoTimer != nullptr)
        m_infoTimer->setInterval(qMax(10, ms));
}

void WledPlugin::setLiveInterval(int ms)
{
    if (m_liveTimer != nullptr)
        m_liveTimer->setInterval(qMax(10, ms));
}

/*********************************************************************
 * Outputs
 *********************************************************************/

QStringList WledPlugin::outputs()
{
    return QStringList() << tr("WLED (JSON over Wi-Fi)");
}

bool WledPlugin::openOutput(quint32 output, quint32 universe)
{
    if (output != 0)
        return false;
    addToMap(universe, output, Output);
    QMutexLocker locker(&m_mutex);
    if (!m_targets.contains(universe))
        m_targets.insert(universe, Target());
    return true;
}

void WledPlugin::closeOutput(quint32 output, quint32 universe)
{
    if (output != 0)
        return;
    removeFromMap(output, universe, Output);
    {
        QMutexLocker locker(&m_mutex);
        m_targets.remove(universe);
    }
    problemsMaybeChanged();
}

QString WledPlugin::outputInfo(quint32 output)
{
    if (output != 0)
        return QString();
    QString str = QStringLiteral("<H3>%1</H3>").arg(outputs().value(0));
    QMutexLocker locker(&m_mutex);
    if (m_targets.isEmpty())
        str += tr("<P>No universe is patched here.</P>");
    for (auto it = m_targets.constBegin(); it != m_targets.constEnd(); ++it)
    {
        const Target &t = it.value();
        QString status;
        if (t.ip.isEmpty())
            status = tr("no device IP set");
        else if (t.answered < 0)
            status = tr("not asked yet");
        else if (t.answered == 0)
            status = tr("<B>not answering</B>");
        else
            status = tr("%1, WLED %2, Wi-Fi %3 dBm")
                         .arg(t.deviceName.isEmpty() ? tr("answering") : t.deviceName.toHtmlEscaped())
                         .arg(t.version.toHtmlEscaped()).arg(t.rssi);
        str += tr("<P>Universe %1 &rarr; %2 (channels %3&ndash;%4): %5</P>")
                   .arg(it.key() + 1).arg(t.ip.isEmpty() ? QStringLiteral("?") : t.ip)
                   .arg(t.address).arg(t.address + t.channels - 1).arg(status);
    }
    return str;
}

void WledPlugin::setParameter(quint32 universe, quint32 line, Capability type,
                              QString name, QVariant value)
{
    if (type != Output || line != 0)
        return;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_targets.contains(universe))
            m_targets.insert(universe, Target());
        Target &t = m_targets[universe];
        if (name == QLatin1String(WLED_OUTPUTIP))
        {
            if (t.ip != value.toString())
            {
                t.ip = value.toString().trimmed();
                t.sent = QJsonObject();     // a new device: tell it everything
                t.answered = -1;
                t.liveCleared = false;
            }
        }
        else if (name == QLatin1String(WLED_ADDRESS))
            t.address = qBound(1, value.toInt(), 512);
        else if (name == QLatin1String(WLED_CHANNELS))
            t.channels = value.toInt() >= 15 ? 15 : value.toInt() >= 4 ? 4 : 3;
        else
            return;                         // not one of ours (e.g. Art-Net's outputUni)
    }
    QLCIOPlugin::setParameter(universe, line, type, name, value);
    problemsMaybeChanged();
}

void WledPlugin::writeUniverse(quint32 universe, quint32 output, const QByteArray &data,
                               bool dataChanged)
{
    Q_UNUSED(dataChanged)
    if (output != 0)
        return;
    // Universe threads: only keep the bytes. tick() (the plugin's thread)
    // turns them into a state and sends it.
    QMutexLocker locker(&m_mutex);
    auto it = m_targets.find(universe);
    if (it != m_targets.end())
        it.value().data = data;
}

void WledPlugin::tick()
{
    QList<quint32> due;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_targets.begin(); it != m_targets.end(); ++it)
        {
            Target &t = it.value();
            if (t.ip.isEmpty() || t.inFlight)
                continue;               // one request at a time: the latest wins
            const QJsonObject state = WledState::decode(t.data, t.address, t.channels, m_sendInterval);
            if (state != t.sent)
                due << it.key();
        }
    }
    foreach (quint32 universe, due)
        send(universe);
}

void WledPlugin::send(quint32 universe)
{
    QString ip;
    QJsonObject state;
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_targets.find(universe);
        if (it == m_targets.end())
            return;
        Target &t = it.value();
        ip = t.ip;
        state = WledState::decode(t.data, t.address, t.channels, m_sendInterval);
        // Out of Art-Net/E1.31 realtime mode, the first time: a WLED still
        // listening to DMX ignores the API until its realtime timeout.
        QJsonObject body = state;
        if (!t.liveCleared)
            body[QStringLiteral("live")] = false;
        t.inFlight = true;
        state = body;
    }

    QNetworkRequest req(QUrl(QStringLiteral("http://%1/json/state").arg(ip)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(2000);
    QNetworkReply *reply = m_nam->post(req, QJsonDocument(state).toJson(QJsonDocument::Compact));
    state.remove(QStringLiteral("live"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, universe, state, ip]() {
        reply->deleteLater();
        const bool ok = reply->error() == QNetworkReply::NoError;
        {
            QMutexLocker locker(&m_mutex);
            auto it = m_targets.find(universe);
            if (it == m_targets.end() || it.value().ip != ip)
                return;
            Target &t = it.value();
            t.inFlight = false;
            if (ok)
            {
                t.sent = state;
                t.liveCleared = true;
            }
            setAnswered(t, ok);
        }
        problemsMaybeChanged();
    });
}

void WledPlugin::pollInfo()
{
    QList<QPair<quint32, QString> > ask;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_targets.begin(); it != m_targets.end(); ++it)
            if (!it.value().ip.isEmpty() && !it.value().infoInFlight)
            {
                it.value().infoInFlight = true;
                ask << qMakePair(it.key(), it.value().ip);
            }
    }
    for (const auto &a : ask)
    {
        const quint32 universe = a.first;
        const QString ip = a.second;
        QNetworkRequest req(QUrl(QStringLiteral("http://%1/json/info").arg(ip)));
        req.setTransferTimeout(2000);
        QNetworkReply *reply = m_nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, universe, ip]() {
            reply->deleteLater();
            const bool ok = reply->error() == QNetworkReply::NoError;
            const QJsonObject info = ok ? QJsonDocument::fromJson(reply->readAll()).object() : QJsonObject();
            {
                QMutexLocker locker(&m_mutex);
                auto it = m_targets.find(universe);
                if (it == m_targets.end() || it.value().ip != ip)
                    return;
                Target &t = it.value();
                t.infoInFlight = false;
                if (ok)
                {
                    t.deviceName = info.value(QStringLiteral("name")).toString();
                    t.version = info.value(QStringLiteral("ver")).toString();
                    t.rssi = info.value(QStringLiteral("wifi")).toObject()
                                 .value(QStringLiteral("rssi")).toInt();
                    t.ledCount = info.value(QStringLiteral("leds")).toObject()
                                     .value(QStringLiteral("count")).toInt();
                }
                setAnswered(t, ok);
            }
            problemsMaybeChanged();
        });
    }
}

void WledPlugin::pollLive()
{
    QList<QPair<quint32, QString> > ask;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_targets.begin(); it != m_targets.end(); ++it)
        {
            Target &t = it.value();
            if (m_watchers.value(it.key()) > 0 && !t.ip.isEmpty() && !t.liveInFlight && t.answered != 0)
            {
                t.liveInFlight = true;
                ask << qMakePair(it.key(), t.ip);
            }
        }
    }
    for (const auto &a : ask)
    {
        const quint32 universe = a.first;
        const QString ip = a.second;
        QNetworkRequest req(QUrl(QStringLiteral("http://%1/json/live").arg(ip)));
        req.setTransferTimeout(1000);
        QNetworkReply *reply = m_nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, universe, ip]() {
            reply->deleteLater();
            const bool ok = reply->error() == QNetworkReply::NoError;
            const QJsonArray leds = ok ? QJsonDocument::fromJson(reply->readAll()).object()
                                             .value(QStringLiteral("leds")).toArray()
                                       : QJsonArray();
            bool changed = false;
            {
                QMutexLocker locker(&m_mutex);
                auto it = m_targets.find(universe);
                if (it == m_targets.end() || it.value().ip != ip)
                    return;
                Target &t = it.value();
                t.liveInFlight = false;
                if (!leds.isEmpty())
                {
                    /* "FF8000" each, brightness already applied; a white
                       channel, where there is one, is already added in. */
                    QVector<quint32> px;
                    px.reserve(leds.size());
                    for (const QJsonValue &v : leds)
                        px << (v.toString().right(6).toUInt(nullptr, 16) & 0xFFFFFF);
                    changed = (px != t.pixels);
                    t.pixels = px;
                    t.pixelsAt = QDateTime::currentMSecsSinceEpoch();
                }
            }
            if (changed)
                emit targetPixelsChanged(universe);
        });
    }
}

void WledPlugin::watchTargetPixels(quint32 universe, bool watch)
{
    QMutexLocker locker(&m_mutex);
    const int n = qMax(0, m_watchers.value(universe) + (watch ? 1 : -1));
    if (n > 0)
        m_watchers[universe] = n;
    else
    {
        m_watchers.remove(universe);
        auto it = m_targets.find(universe);
        if (it != m_targets.end())
            it.value().pixels.clear();
    }
}

QVector<quint32> WledPlugin::targetPixels(quint32 universe) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_targets.constFind(universe);
    if (it == m_targets.constEnd()
        || QDateTime::currentMSecsSinceEpoch() - it.value().pixelsAt > liveStaleMs)
        return QVector<quint32>();
    return it.value().pixels;
}

int WledPlugin::targetPixelCount(quint32 universe) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_targets.constFind(universe);
    return it == m_targets.constEnd() ? 0 : it.value().ledCount;
}

void WledPlugin::setAnswered(Target &t, bool answered)
{
    t.answered = answered ? 1 : 0;
}

QStringList WledPlugin::targetProblems() const
{
    QStringList out;
    QMutexLocker locker(&m_mutex);
    for (auto it = m_targets.constBegin(); it != m_targets.constEnd(); ++it)
    {
        const Target &t = it.value();
        if (t.ip.isEmpty())
            continue;
        if (t.answered == 0)
            out << tr("%1 — not answering").arg(t.ip);
        else if (t.answered == 1 && t.rssi != 0 && t.rssi < weakSignalDbm)
            out << tr("%1 — weak Wi-Fi (%2 dBm)").arg(t.ip).arg(t.rssi);
    }
    out.sort();
    return out;
}

void WledPlugin::problemsMaybeChanged()
{
    /* Said afterwards, once, and as targetProblemsChanged() only. It was
       configurationChanged(), straight from inside closeOutput(): that
       reconnects every output, under InputOutputMap's lock -- and closing a
       WLED output called back in here, so the second reconnect waited on the
       lock the first was holding, and the app hung ("patched the lanterns
       and the app appears hung"). A lantern going quiet is not a new
       configuration, and nothing needs reconnecting for it. */
    if (!m_problemsCheckPending.testAndSetOrdered(0, 1))
        return;
    QMetaObject::invokeMethod(this, [this]() {
        m_problemsCheckPending.storeRelease(0);
        const QStringList now = targetProblems();
        if (now != m_lastProblems)
        {
            m_lastProblems = now;
            emit targetProblemsChanged();
        }
    }, Qt::QueuedConnection);
}
