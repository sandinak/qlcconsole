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
#include <QSet>
#include <QUdpSocket>
#include <QHostAddress>
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
                        "One universe per device, aimed at its IP -- or at several, a backup "
                        "mirroring the primary; the WLED Effect and WLED Lantern "
                        "definitions drive it unchanged."));
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
    removeFromMap(universe, output, Output);
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
        const int n = t.deviceCount();
        if (n == 0)
        {
            str += tr("<P>Universe %1 &rarr; ?: no device IP set</P>").arg(it.key() + 1);
            continue;
        }
        int ok = 0;
        QStringList lines;
        for (int i = 0; i < t.devices.size(); i++)
        {
            const Device &d = t.devices.at(i);
            if (d.ip.isEmpty())
                continue;
            QString status;
            if (d.answered < 0)
                status = tr("not asked yet");
            else if (d.answered == 0)
                status = tr("<B>not answering</B>");
            else
            {
                ok++;
                status = tr("%1, WLED %2, Wi-Fi %3 dBm")
                             .arg(d.deviceName.isEmpty() ? tr("answering") : d.deviceName.toHtmlEscaped())
                             .arg(d.version.toHtmlEscaped()).arg(d.rssi);
            }
            lines << tr("%1 (channels %2&ndash;%3): %4")
                         .arg(d.ip.toHtmlEscaped()).arg(t.addressOf(d))
                         .arg(t.addressOf(d) + t.channelsOf(d) - 1).arg(status);
        }
        if (n == 1)
            str += tr("<P>Universe %1 &rarr; %2</P>").arg(it.key() + 1).arg(lines.first());
        else
            str += tr("<P>Universe %1 &rarr; %2 devices, %3 of %2 answering:<BR>%4</P>")
                       .arg(it.key() + 1).arg(n).arg(ok).arg(lines.join(QStringLiteral("<BR>")));
    }
    return str;
}

int WledPlugin::parameterSlot(const QString &name, int *kind)
{
    static const char *keys[] = { WLED_OUTPUTIP, WLED_ADDRESS, WLED_CHANNELS };
    for (int k = 0; k < 3; k++)
    {
        const QLatin1String key(keys[k]);
        if (!name.startsWith(key))
            continue;
        const QString rest = name.mid(key.size());
        int slot = -1;
        if (rest.isEmpty())
            slot = 0;
        else
        {
            bool ok = false;
            const int n = rest.toInt(&ok);
            // "outputIP2" up to "outputIP16"; never "outputIP02" or "outputIP1"
            if (ok && n >= 2 && n <= 16 && rest == QString::number(n))
                slot = n - 1;
        }
        if (slot < 0)
            continue;
        if (kind != nullptr)
            *kind = k;
        return slot;
    }
    return -1;
}

WledPlugin::Device &WledPlugin::slot(Target &t, int index)
{
    if (t.devices.size() <= index)
        t.devices.resize(index + 1);
    return t.devices[index];
}

int WledPlugin::Target::deviceCount() const
{
    int n = 0;
    for (const Device &d : devices)
        if (!d.ip.isEmpty())
            n++;
    return n;
}

int WledPlugin::Target::liveSlot() const
{
    for (int i = 0; i < devices.size(); i++)
        if (!devices.at(i).ip.isEmpty() && devices.at(i).answered == 1)
            return i;
    for (int i = 0; i < devices.size(); i++)
        if (!devices.at(i).ip.isEmpty() && devices.at(i).answered < 0)
            return i;
    return -1;
}

QPair<int, int> WledPlugin::answering(quint32 universe) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_targets.constFind(universe);
    if (it == m_targets.constEnd())
        return qMakePair(0, 0);
    int ok = 0;
    for (const Device &d : it.value().devices)
        if (!d.ip.isEmpty() && d.answered == 1)
            ok++;
    return qMakePair(ok, it.value().deviceCount());
}

void WledPlugin::setParameter(quint32 universe, quint32 line, Capability type,
                              QString name, QVariant value)
{
    if (type != Output || line != 0)
        return;
    int kind = 0;
    const int index = parameterSlot(name, &kind);
    if (index < 0)
        return;                             // not one of ours (e.g. Art-Net's outputUni)
    // A device's address: its gate decision, worked out before the lock
    // (the route probe must not hold up writeUniverse())
    const bool newIpOpen = kind == 0 ? routeAllowed(value.toString().trimmed()) : true;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_targets.contains(universe))
            m_targets.insert(universe, Target());
        Target &t = m_targets[universe];
        Device &d = slot(t, index);
        if (kind == 0)
        {
            const QString ip = value.toString().trimmed();
            if (d.ip != ip)
            {
                // A new device: tell it everything. A reply still on its way
                // from the old one is told apart by its IP.
                Device fresh;
                fresh.ip = ip;
                fresh.gateOpen = newIpOpen;
                fresh.address = d.address;
                fresh.channels = d.channels;
                d = fresh;
            }
        }
        else
        {
            int v = 0;
            if (kind == 1)
                v = qBound(1, value.toInt(), 512);
            else
                v = value.toInt() >= 15 ? 15 : value.toInt() >= 4 ? 4 : 3;
            if (index == 0)
                (kind == 1 ? t.address : t.channels) = v;
            else
                (kind == 1 ? d.address : d.channels) = v;
        }
    }
    QLCIOPlugin::setParameter(universe, line, type, name, value);
    problemsMaybeChanged();
}

void WledPlugin::unSetParameter(quint32 universe, quint32 line, Capability type,
                                QString name)
{
    int kind = 0;
    const int index = (type == Output && line == 0) ? parameterSlot(name, &kind) : -1;
    if (index >= 0)
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_targets.find(universe);
        if (it != m_targets.end() && index < it.value().devices.size())
        {
            Target &t = it.value();
            Device &d = t.devices[index];
            if (kind == 0)
            {
                Device gone;
                gone.address = d.address;
                gone.channels = d.channels;
                d = gone;
            }
            else if (kind == 1)
                (index == 0 ? t.address : d.address) = index == 0 ? 1 : 0;
            else
                (index == 0 ? t.channels : d.channels) = index == 0 ? 15 : 0;
            // A removed backup at the end is gone altogether.
            while (t.devices.size() > 1 && t.devices.last().ip.isEmpty()
                   && t.devices.last().address == 0 && t.devices.last().channels == 0)
                t.devices.removeLast();
        }
    }
    QLCIOPlugin::unSetParameter(universe, line, type, name);
    if (index >= 0)
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
    QList<QPair<quint32, int> > due, ask;
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        QMutexLocker locker(&m_mutex);
        for (auto it = m_targets.begin(); it != m_targets.end(); ++it)
        {
            Target &t = it.value();
            for (int i = 0; i < t.devices.size(); i++)
            {
                Device &d = t.devices[i];
                // One request at a time per device (the latest wins); one
                // that did not answer is tried again in a moment, not every
                // tick. Neither holds up the other devices.
                if (d.ip.isEmpty() || d.inFlight || d.infoInFlight || !d.gateOpen)
                    continue;
                /* Its segments first: nothing is sent before they are known,
                   so the first look reaches every one of them -- and a
                   device not known yet is asked again, not sent to. */
                if (d.pollDue || (!d.layoutKnown && now >= d.retryAt))
                {
                    d.pollDue = false;
                    d.infoInFlight = true;
                    ask << qMakePair(it.key(), i);
                    continue;
                }
                if (!d.layoutKnown || now < d.retryAt)
                    continue;
                const QJsonObject state = WledState::decode(t.data, t.addressOf(d),
                                                            t.channelsOf(d), m_sendInterval);
                if (state != d.sent || d.layoutGen != d.sentLayoutGen)
                    due << qMakePair(it.key(), i);
            }
        }
    }
    for (const auto &u : ask)
        poll(u.first, u.second);
    for (const auto &u : due)
        send(u.first, u.second);
}

QJsonObject WledPlugin::message(const QJsonObject &state, const Device &d,
                                QVector<QPair<int, bool> > *revs)
{
    if (d.segments.isEmpty())
        return state;
    /* One element per segment, each with its id: an element without one is
       the segment at its INDEX in the array (WLED's json.cpp), so a lone
       element was segment 0's alone. */
    const QJsonObject one = state.value(QStringLiteral("seg")).toArray().first().toObject();
    const bool hasRev = one.contains(QStringLiteral("rev"));
    const bool flip = one.value(QStringLiteral("rev")).toBool();
    QJsonArray segs;
    for (const Segment &s : d.segments)
    {
        QJsonObject o = one;
        o[QStringLiteral("id")] = s.id;
        if (hasRev)
        {
            // Reverse turns the segment round from ITS way, not to a way.
            const bool rev = s.layoutRev != flip;
            o[QStringLiteral("rev")] = rev;
            if (revs != nullptr)
                revs->append(qMakePair(s.id, rev));
        }
        segs.append(o);
    }
    QJsonObject out = state;
    out[QStringLiteral("seg")] = segs;
    return out;
}

void WledPlugin::learnSegments(Device &d, const QJsonArray &seg)
{
    QVector<Segment> now;
    QVector<bool> observed;
    for (int i = 0; i < seg.size(); i++)
    {
        const QJsonObject o = seg.at(i).toObject();
        Segment s;
        s.id = o.value(QStringLiteral("id")).toInt(i);
        s.start = o.value(QStringLiteral("start")).toInt();
        s.stop = o.value(QStringLiteral("stop")).toInt();
        if (s.stop <= s.start)
            continue;                       // deleted, or not in use
        s.layoutRev = o.value(QStringLiteral("rev")).toBool();
        now << s;
        observed << s.layoutRev;
    }

    bool same = d.layoutKnown && now.size() == d.segments.size();
    for (int k = 0; same && k < now.size(); k++)
        same = now.at(k).id == d.segments.at(k).id && now.at(k).start == d.segments.at(k).start
               && now.at(k).stop == d.segments.at(k).stop;
    d.layoutKnown = true;
    if (!same)
    {
        /* A new layout (Write Segments, or the device's own UI): its
           directions are its own, nothing of ours in them yet -- and a new
           segment has none of the look, so all of it goes again. */
        d.segments = now;
        d.sent = QJsonObject();
        d.layoutGen++;
        return;
    }
    bool changed = false;
    for (int k = 0; k < now.size(); k++)
    {
        Segment &s = d.segments[k];
        const bool rev = observed.at(k);
        /* Says back what it took from us: its layout is as we knew it.
           Otherwise someone else set its direction: that is its layout. */
        if (s.sentRev >= 0 ? rev == bool(s.sentRev) : rev == s.layoutRev)
            continue;
        s.layoutRev = rev;
        s.sentRev = -1;
        changed = true;
    }
    if (changed)
    {
        d.sent = QJsonObject();
        d.layoutGen++;
    }
}

void WledPlugin::send(quint32 universe, int index)
{
    QString ip;
    QJsonObject state, body;
    QVector<QPair<int, bool> > revs;
    quint32 gen = 0;
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_targets.find(universe);
        if (it == m_targets.end() || index >= it.value().devices.size())
            return;
        Target &t = it.value();
        Device &d = t.devices[index];
        ip = d.ip;
        state = WledState::decode(t.data, t.addressOf(d), t.channelsOf(d), m_sendInterval);
        body = message(state, d, &revs);
        gen = d.layoutGen;
        // Out of Art-Net/E1.31 realtime mode, the first time: a WLED still
        // listening to DMX ignores the API until its realtime timeout.
        if (!d.liveCleared)
            body[QStringLiteral("live")] = false;
        d.inFlight = true;
    }

    QNetworkRequest req(QUrl(QStringLiteral("http://%1/json/state").arg(ip)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(2000);
    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, universe, index, state, ip, revs, gen]() {
        reply->deleteLater();
        const bool ok = reply->error() == QNetworkReply::NoError;
        {
            QMutexLocker locker(&m_mutex);
            auto it = m_targets.find(universe);
            if (it == m_targets.end() || index >= it.value().devices.size()
                || it.value().devices.at(index).ip != ip)
                return;
            Device &d = it.value().devices[index];
            d.inFlight = false;
            setAnswered(d, ok);
            if (ok)
            {
                d.sent = state;
                d.sentLayoutGen = gen;
                d.liveCleared = true;
                if (gen == d.layoutGen)
                    for (const auto &r : revs)
                        for (Segment &s : d.segments)
                            if (s.id == r.first)
                                s.sentRev = r.second ? 1 : 0;
            }
            else
                d.retryAt = QDateTime::currentMSecsSinceEpoch() + retryMs;
        }
        problemsMaybeChanged();
    });
}

bool WledPlugin::routeAllowed(const QString &host) const
{
    if (transmitRestricted() == false)
        return true;

    const QHostAddress target(host.trimmed());
    if (target.isNull() || target.protocol() != QAbstractSocket::IPv4Protocol)
        return false;
    if (target.isLoopback())
        return true;

    /* Which local address would a request to it leave from? Connecting a
       UDP socket sends nothing -- the kernel only picks the route. */
    quint32 local = 0;
    QUdpSocket probe;
    probe.connectToHost(target, 80);
    if (probe.state() == QAbstractSocket::ConnectedState
        || probe.waitForConnected(100))
        local = probe.localAddress().toIPv4Address();
    probe.abort();
    return local != 0 && transmitAllowedFrom(local);
}

void WledPlugin::refreshTransmitGate()
{
    /* The route probes run here -- the networks changed, or the engine's
       reconnect tick -- with m_mutex released, never on the send tick:
       tick(), pollInfo() and pollLive() only read Device::gateOpen. */
    QSet<QString> hosts;
    {
        QMutexLocker locker(&m_mutex);
        for (const Target &t : std::as_const(m_targets))
            for (const Device &d : t.devices)
                if (!d.ip.isEmpty())
                    hosts.insert(d.ip);
    }
    QHash<QString, bool> open;
    for (const QString &host : std::as_const(hosts))
        open.insert(host, routeAllowed(host));

    QMutexLocker locker(&m_mutex);
    for (Target &t : m_targets)
        for (Device &d : t.devices)
        {
            auto it = open.constFind(d.ip);
            if (it != open.constEnd())
                d.gateOpen = it.value();
        }
}

void WledPlugin::pollInfo()
{
    // Asked on the next tick, in turn with what it is sent (see tick()).
    QMutexLocker locker(&m_mutex);
    for (Target &t : m_targets)
        for (Device &d : t.devices)
            if (!d.ip.isEmpty())
                d.pollDue = true;
}

void WledPlugin::poll(quint32 universe, int index)
{
    QString ip;
    bool si = true;
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_targets.find(universe);
        if (it == m_targets.end() || index >= it.value().devices.size())
            return;
        ip = it.value().devices.at(index).ip;
        si = !it.value().devices.at(index).noSi;
    }
    /* /json/si: the state (its segments) and the info in one answer -- one
       request every few seconds, as /json/info alone was. */
    QNetworkRequest req(QUrl(QStringLiteral("http://%1/json/%2").arg(ip, si ? QStringLiteral("si")
                                                                              : QStringLiteral("info"))));
    req.setTransferTimeout(2000);
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, universe, index, ip, si]() {
        reply->deleteLater();
        const bool ok = reply->error() == QNetworkReply::NoError;
        // It answered, but not this (an old WLED): asked /json/info from now on.
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool oldWled = !ok && si && status >= 400;
        const QJsonObject root = ok ? QJsonDocument::fromJson(reply->readAll()).object() : QJsonObject();
        const QJsonObject info = root.value(QStringLiteral("info")).isObject()
                               ? root.value(QStringLiteral("info")).toObject() : root;
        {
            QMutexLocker locker(&m_mutex);
            auto it = m_targets.find(universe);
            if (it == m_targets.end() || index >= it.value().devices.size()
                || it.value().devices.at(index).ip != ip)
                return;
            Device &d = it.value().devices[index];
            d.infoInFlight = false;
            if (oldWled)
            {
                d.noSi = true;
                setAnswered(d, true);
                d.pollDue = true;
                return;
            }
            setAnswered(d, ok);
            if (ok)
            {
                d.deviceName = info.value(QStringLiteral("name")).toString();
                d.version = info.value(QStringLiteral("ver")).toString();
                d.rssi = info.value(QStringLiteral("wifi")).toObject()
                             .value(QStringLiteral("rssi")).toInt();
                d.ledCount = info.value(QStringLiteral("leds")).toObject()
                                 .value(QStringLiteral("count")).toInt();
                learnSegments(d, root.value(QStringLiteral("state")).toObject()
                                     .value(QStringLiteral("seg")).toArray());
                d.pollDue = false;          // this was the answer
            }
            else
                d.retryAt = QDateTime::currentMSecsSinceEpoch() + retryMs;
        }
        problemsMaybeChanged();
    });
}

void WledPlugin::pollLive()
{
    QList<QPair<quint32, QString> > ask;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_targets.begin(); it != m_targets.end(); ++it)
        {
            Target &t = it.value();
            if (m_watchers.value(it.key()) <= 0 || t.liveInFlight)
                continue;
            // The first device that answers: a dead primary does not leave
            // the rig view dark while its backup is showing the look.
            const int s = t.liveSlot();
            if (s < 0 || !t.devices.at(s).gateOpen)
                continue;
            t.liveInFlight = true;
            ask << qMakePair(it.key(), t.devices.at(s).ip);
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
                if (it == m_targets.end())
                    return;
                Target &t = it.value();
                t.liveInFlight = false;
                bool stillOurs = false;
                for (const Device &d : t.devices)
                    stillOurs = stillOurs || d.ip == ip;
                if (stillOurs && !leds.isEmpty())
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
    if (it == m_targets.constEnd() || it.value().devices.isEmpty())
        return 0;
    const int s = it.value().liveSlot();
    return it.value().devices.at(s < 0 ? 0 : s).ledCount;
}

void WledPlugin::setAnswered(Device &d, bool answered)
{
    /* Back after not answering: it may have been switched off and on, and
       have forgotten the look -- send it again, in full, out of realtime
       mode. */
    if (answered && d.answered == 0)
    {
        d.sent = QJsonObject();
        d.liveCleared = false;
        d.retryAt = 0;
        d.pollDue = true;           // and its segments, which a reboot may have changed
    }
    d.answered = answered ? 1 : 0;
}

QStringList WledPlugin::targetProblems() const
{
    QStringList out;
    QMutexLocker locker(&m_mutex);
    for (auto it = m_targets.constBegin(); it != m_targets.constEnd(); ++it)
    {
        const Target &t = it.value();
        const int n = t.deviceCount();
        int ok = 0;
        for (const Device &d : t.devices)
            if (!d.ip.isEmpty() && d.answered == 1)
                ok++;
        // "1 of 2 answering": the universe is still on stage, from its other device.
        const QString of = n > 1 ? tr(" · %1 of %2 answering").arg(ok).arg(n) : QString();
        for (const Device &d : t.devices)
        {
            if (d.ip.isEmpty())
                continue;
            if (d.answered == 0)
                out << tr("%1 — not answering").arg(d.ip) + of;
            else if (d.answered == 1 && d.rssi != 0 && d.rssi < weakSignalDbm)
                out << tr("%1 — weak Wi-Fi (%2 dBm)").arg(d.ip).arg(d.rssi);
        }
    }
    out.sort();
    return out;
}

QStringList WledPlugin::outputProblems(quint32 output, quint32 universe) const
{
    if (output != 0)
        return QStringList();
    QMutexLocker locker(&m_mutex);
    auto it = m_targets.constFind(universe);
    if (it == m_targets.constEnd())
        return QStringList();
    const Target &t = it.value();
    QStringList silent;
    int n = 0, ok = 0;
    for (const Device &d : t.devices)
    {
        if (d.ip.isEmpty())
            continue;
        n++;
        if (d.answered == 0)
            silent << d.ip;
        else if (d.answered == 1)
            ok++;
    }
    if (silent.isEmpty())
        return QStringList();
    if (n == 1)
        return QStringList() << tr("%1 not answering").arg(silent.first());
    return QStringList() << tr("%1 of %2 answering — %3 not answering")
                                .arg(ok).arg(n).arg(silent.join(QStringLiteral(", ")));
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
