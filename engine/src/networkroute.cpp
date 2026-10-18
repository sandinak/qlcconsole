/*
  Q Light Controller Plus
  networkroute.cpp

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

#include <QNetworkInterface>
#include <QElapsedTimer>
#include <QUdpSocket>
#include <QMutex>
#include <QDebug>

#include "networkroute.h"

namespace
{
    std::function<QList<NetworkRoute::LocalAddress>()> s_addressesOverride;
    std::function<QHostAddress(const QHostAddress &)> s_sourceOverride;

    /* An address no machine has locally attached (TEST-NET-2, RFC 5737):
       the local address the OS picks to reach it is the default route's. */
    const QHostAddress kDefaultRouteProbe(QStringLiteral("198.51.100.1"));

    /* Art-Net's port. Nothing is ever sent -- connect() on a UDP socket only
       asks the kernel to choose a route and a source address -- but a real
       port keeps any policy routing keyed on it honest. */
    const quint16 kProbePort = 6454;

    QHostAddress parseV4(const QString &text)
    {
        QHostAddress a(text.trimmed());
        if (a.isNull() || a.protocol() != QAbstractSocket::IPv4Protocol)
            return QHostAddress();
        return a;
    }

    const NetworkRoute::LocalAddress *findLocal(const QList<NetworkRoute::LocalAddress> &all,
                                                const QHostAddress &ip)
    {
        for (int i = 0; i < all.count(); i++)
            if (all.at(i).ip == ip)
                return &all.at(i);
        return NULL;
    }

    int effectivePrefix(const NetworkRoute::LocalAddress &la)
    {
        /* No netmask known: assume the /24 nearly every lighting network
           uses (and the one Art-Net/sACN configuration tools default to). */
        return la.prefix > 0 ? la.prefix : 24;
    }

    QString describe(const NetworkRoute::LocalAddress &la)
    {
        if (la.iface.isEmpty())
            return la.ip.toString();
        return QString("%1 (%2)").arg(la.iface, la.ip.toString());
    }
}

QList<NetworkRoute::LocalAddress> NetworkRoute::localAddresses()
{
    if (s_addressesOverride)
        return s_addressesOverride();

    /* Asked per line on every Connections refresh and per patch on save;
       QNetworkInterface::allInterfaces() costs ~20 ms on Windows. Interfaces
       do not come and go that fast: walk them at most once a second. */
    static QMutex cacheMutex;
    static QElapsedTimer cacheAge;
    static QList<LocalAddress> cache;
    QMutexLocker locker(&cacheMutex);
    if (cacheAge.isValid() && cacheAge.elapsed() < 1000)
        return cache;

    QList<LocalAddress> list;
    foreach (const QNetworkInterface &iface, QNetworkInterface::allInterfaces())
    {
        if ((iface.flags() & QNetworkInterface::IsUp) == 0)
            continue;
        foreach (const QNetworkAddressEntry &entry, iface.addressEntries())
        {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            LocalAddress la;
            la.iface = iface.humanReadableName();
            la.ip = entry.ip();
            la.prefix = entry.prefixLength();
            list << la;
        }
    }
    cache = list;
    cacheAge.start();
    return list;
}

QHostAddress NetworkRoute::sourceFor(const QHostAddress &target)
{
    if (s_sourceOverride)
        return s_sourceOverride(target);

    if (target.isNull() || target.protocol() != QAbstractSocket::IPv4Protocol)
        return QHostAddress();

    /* The portable "which interface would the OS use" question: connecting
       a UDP socket sends nothing, it only makes the kernel pick a route and
       bind the matching source address -- the same answer a unicast send
       would get, on macOS, Linux and Windows alike, and it honours
       routed/VPN paths, not just directly attached subnets. */
    QUdpSocket sock;
    sock.connectToHost(target, kProbePort);
    if (sock.state() != QAbstractSocket::ConnectedState)
        sock.waitForConnected(100);
    if (sock.state() != QAbstractSocket::ConnectedState)
        return QHostAddress();

    QHostAddress local = sock.localAddress();
    sock.abort();
    if (local.protocol() != QAbstractSocket::IPv4Protocol)
    {
        bool ok = false;
        quint32 v4 = local.toIPv4Address(&ok);
        if (ok == false)
            return QHostAddress();
        local = QHostAddress(v4);
    }
    if (local == QHostAddress(QHostAddress::AnyIPv4))
        return QHostAddress();
    return local;
}

QString NetworkRoute::networkOf(const QHostAddress &ip, int prefix)
{
    if (ip.isNull() || prefix < 0 || prefix > 32)
        return QString();
    const quint32 mask = prefix == 0 ? 0 : (0xFFFFFFFFu << (32 - prefix));
    return QString("%1/%2").arg(QHostAddress(ip.toIPv4Address() & mask).toString())
                           .arg(prefix);
}

QString NetworkRoute::targetFromParameters(const QMap<QString, QVariant> &params)
{
    static const char *keys[] = { "outputIP", "ucastIP", "feedbackIP" };
    for (const char *key : keys)
    {
        const QString value = params.value(key).toString();
        if (parseV4(value).isNull() == false)
            return value;
    }
    return QString();
}

bool NetworkRoute::isAddressIdentity(const QString &uid)
{
    return parseV4(uid).isNull() == false;
}

NetworkRoute::Resolution NetworkRoute::resolve(const QStringList &lines, const QString &uid,
                                               const QString &network, const QString &target)
{
    Resolution res;
    const QList<LocalAddress> locals = localAddresses();

    // Lines named after an address this machine has, in plugin line order.
    QList<QPair<int, LocalAddress> > candidates;
    for (int i = 0; i < lines.count(); i++)
    {
        const QHostAddress ip = parseV4(lines.at(i));
        if (ip.isNull())
            continue;
        const LocalAddress *la = findLocal(locals, ip);
        if (la != NULL)
            candidates << qMakePair(i, *la);
    }
    if (candidates.isEmpty())
        return res;

    auto pick = [&](int idx, const QString &how)
    {
        const LocalAddress &la = candidates.at(idx).second;
        res.line = candidates.at(idx).first;
        res.how = how;
        res.network = networkOf(la.ip, effectivePrefix(la));
        res.via = describe(la);
        return res;
    };

    /* 1. A unicast target: whatever the OS would route it through. */
    const QHostAddress targetIp = parseV4(target);
    if (targetIp.isNull() == false && targetIp != QHostAddress(QHostAddress::Broadcast))
    {
        const QHostAddress src = sourceFor(targetIp);
        for (int c = 0; src.isNull() == false && c < candidates.count(); c++)
        {
            const LocalAddress &la = candidates.at(c).second;
            if (la.ip != src)
                continue;
            const bool attached = targetIp.isInSubnet(la.ip, effectivePrefix(la));
            const bool justDefault = (sourceFor(kDefaultRouteProbe) == src);
            if (attached || justDefault == false)
                return pick(c, "route");
        }
    }

    /* 2. The very address the workspace was saved with. */
    const QHostAddress saved = parseV4(uid);
    for (int c = 0; saved.isNull() == false && c < candidates.count(); c++)
        if (candidates.at(c).second.ip == saved)
            return pick(c, "uid");

    /* 3. Same network: the stored one, else the saved address's. */
    const QPair<QHostAddress, int> net = QHostAddress::parseSubnet(network);
    if (net.first.isNull() == false && net.first.protocol() == QAbstractSocket::IPv4Protocol)
    {
        for (int c = 0; c < candidates.count(); c++)
            if (candidates.at(c).second.ip.isInSubnet(net))
                return pick(c, "network");
    }
    else if (saved.isNull() == false)
    {
        for (int c = 0; c < candidates.count(); c++)
            if (saved.isInSubnet(candidates.at(c).second.ip,
                                 effectivePrefix(candidates.at(c).second)))
                return pick(c, "network");
    }

    /* 4. A broadcast (or unrouted unicast) target on a local subnet. */
    for (int c = 0; targetIp.isNull() == false && c < candidates.count(); c++)
        if (targetIp.isInSubnet(candidates.at(c).second.ip,
                                effectivePrefix(candidates.at(c).second)))
            return pick(c, "target");

    return res;
}

QString NetworkRoute::networkForLine(const QString &lineName)
{
    const QHostAddress ip = parseV4(lineName);
    if (ip.isNull() || ip.isLoopback())
        return QString();
    const QList<LocalAddress> locals = localAddresses();
    const LocalAddress *la = findLocal(locals, ip);
    if (la == NULL)
        return QString();
    return networkOf(la->ip, effectivePrefix(*la));
}

QString NetworkRoute::describeLine(const QString &lineName)
{
    const QHostAddress ip = parseV4(lineName);
    if (ip.isNull())
        return lineName;
    const QList<LocalAddress> locals = localAddresses();
    const LocalAddress *la = findLocal(locals, ip);
    return la != NULL ? describe(*la) : lineName;
}

void NetworkRoute::setResolverForTest(std::function<QList<LocalAddress>()> addresses,
                                      std::function<QHostAddress(const QHostAddress &)> source)
{
    s_addressesOverride = addresses;
    s_sourceOverride = source;
}
