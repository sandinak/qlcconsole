/*
  Q Light Controller Plus
  wleddiscovery.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QNetworkDatagram>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QUdpSocket>
#include <QDateTime>
#include <QtEndian>
#include <QTimer>
#include <QSet>

#include "wleddiscovery.h"
#include "wledconfigcheck.h"
#include "monitorproperties.h"
#include "qlcfixturedefcache.h"
#include "fixturecreator.h"
#include "inputoutputmap.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "patchplanner.h"
#include "rignetwork.h"
#include "showstatus.h"
#include "monitor.h"
#include "fixture.h"
#include "truss.h"
#include "doc.h"

WledDiscovery *WledDiscovery::s_instance = nullptr;

namespace
{
const char *kService = "_wled._tcp.local";
const quint16 kMdnsPort = 5353;
const int kInfoTimeoutMs = 3000;
/** A device's info is asked again after this long (its name or count may
 *  have changed), and an IP is never asked twice within kAskAgainMs. */
const qint64 kInfoStaleMs = 10 * 60 * 1000;
const qint64 kAskAgainMs = 20 * 1000;

enum { TypeA = 1, TypePtr = 12, TypeTxt = 16, TypeSrv = 33 };

QString normalMac(const QString &mac)
{
    QString out;
    for (const QChar c : mac)
        if (c.isLetterOrNumber())
            out += c.toLower();
    return out;
}

/** A name at @p pos in @p p, following compression pointers; @p pos moves
 *  past it (past the first pointer, as DNS has it). Empty on a broken one. */
QString readName(const QByteArray &p, int &pos, bool *ok)
{
    QStringList labels;
    int at = pos;
    bool jumped = false;
    int hops = 0;
    while (true)
    {
        if (at >= p.size() || ++hops > 64)
        {
            *ok = false;
            return QString();
        }
        const quint8 len = quint8(p.at(at));
        if (len == 0)
        {
            if (!jumped)
                pos = at + 1;
            break;
        }
        if ((len & 0xC0) == 0xC0)
        {
            if (at + 1 >= p.size())
            {
                *ok = false;
                return QString();
            }
            const int target = ((len & 0x3F) << 8) | quint8(p.at(at + 1));
            if (!jumped)
                pos = at + 2;
            jumped = true;
            at = target;
            continue;
        }
        if (at + 1 + len > p.size())
        {
            *ok = false;
            return QString();
        }
        labels << QString::fromUtf8(p.mid(at + 1, len));
        at += 1 + len;
    }
    return labels.join(QLatin1Char('.'));
}

quint16 u16(const QByteArray &p, int pos)
{
    return quint16((quint8(p.at(pos)) << 8) | quint8(p.at(pos + 1)));
}

/** "172.18.2.105" against "172.18.2.100": ".105" when only the last part
 *  differs, else all of it. */
QString shortIp(const QString &to, const QString &from)
{
    const int cut = to.lastIndexOf(QLatin1Char('.'));
    if (cut > 0 && !to.contains(QLatin1Char(':')) && from.left(from.lastIndexOf(QLatin1Char('.'))) == to.left(cut))
        return to.mid(cut);
    return to;
}
}

WledDiscovery::WledDiscovery(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_nam(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
{
    if (s_instance == nullptr)
        s_instance = this;
    m_nam->setProxy(QNetworkProxy::NoProxy);    // WLEDs are on the show LAN
    m_timer->setInterval(60000);
    connect(m_timer, &QTimer::timeout, this, &WledDiscovery::browseNow);
    // What is in the show changes what is "found".
    connect(m_doc, &Doc::fixtureAdded, this, &WledDiscovery::reconcile);
    connect(m_doc, &Doc::fixtureRemoved, this, &WledDiscovery::reconcile);
    connect(m_doc, &Doc::fixtureChanged, this, &WledDiscovery::reconcile);
}

WledDiscovery::~WledDiscovery()
{
    if (s_instance == this)
        s_instance = nullptr;
}

/*********************************************************************
 * mDNS on the wire
 *********************************************************************/

QByteArray WledDiscovery::browseQuery()
{
    QByteArray q;
    // id 0, flags 0 (a standard query), one question.
    q.append("\0\0\0\0\0\x01\0\0\0\0\0\0", 12);
    foreach (const QString &label, QString::fromLatin1(kService).split(QLatin1Char('.')))
    {
        q.append(char(label.size()));
        q.append(label.toLatin1());
    }
    q.append('\0');
    q.append("\0\x0c", 2);          // PTR
    q.append("\x80\x01", 2);        // IN, unicast answer wanted
    return q;
}

QList<WledDiscovery::Answer> WledDiscovery::parseResponse(const QByteArray &p)
{
    QList<Answer> out;
    if (p.size() < 12)
        return out;
    const quint16 flags = u16(p, 2);
    if ((flags & 0x8000) == 0)
        return out;                 // a question, not an answer
    const int qd = u16(p, 4);
    const int records = u16(p, 6) + u16(p, 8) + u16(p, 10);
    int pos = 12;
    bool ok = true;
    for (int i = 0; i < qd && ok; i++)
    {
        readName(p, pos, &ok);
        pos += 4;
    }

    QStringList instances;                      // PTR targets
    QHash<QString, QPair<QString, quint16> > srv;   // instance -> host, port
    QHash<QString, QString> addr;               // host -> IPv4
    QHash<QString, QString> mac;                // instance -> TXT mac
    const QString service = QString::fromLatin1(kService);
    for (int i = 0; i < records && ok; i++)
    {
        const QString name = readName(p, pos, &ok);
        if (!ok || pos + 10 > p.size())
            break;
        const quint16 type = u16(p, pos);
        const int rdlen = u16(p, pos + 8);
        const int rd = pos + 10;
        if (rd + rdlen > p.size())
            break;
        if (type == TypePtr && name.compare(service, Qt::CaseInsensitive) == 0)
        {
            int at = rd;
            const QString target = readName(p, at, &ok);
            if (ok && !instances.contains(target, Qt::CaseInsensitive))
                instances << target;
        }
        else if (type == TypeSrv && rdlen >= 7)
        {
            int at = rd + 6;
            const QString host = readName(p, at, &ok);
            if (ok)
                srv.insert(name.toLower(), qMakePair(host, u16(p, rd + 4)));
        }
        else if (type == TypeA && rdlen == 4)
        {
            addr.insert(name.toLower(), QHostAddress(qFromBigEndian<quint32>(p.constData() + rd)).toString());
        }
        else if (type == TypeTxt)
        {
            for (int at = rd; at < rd + rdlen;)
            {
                const int len = quint8(p.at(at));
                const QByteArray kv = p.mid(at + 1, len);
                if (kv.startsWith("mac="))
                    mac.insert(name.toLower(), QString::fromLatin1(kv.mid(4)));
                at += 1 + len;
            }
        }
        pos = rd + rdlen;
    }

    // An answer may carry only the SRV (asked for one instance): it counts.
    for (auto it = srv.constBegin(); it != srv.constEnd(); ++it)
        if (it.key().endsWith(QLatin1Char('.') + service.toLower())
            && !instances.contains(it.key(), Qt::CaseInsensitive))
            instances << it.key();

    foreach (const QString &full, instances)
    {
        const QString key = full.toLower();
        Answer a;
        a.instance = full.endsWith(QLatin1Char('.') + service, Qt::CaseInsensitive)
                         ? full.left(full.size() - service.size() - 1) : full;
        a.host = srv.value(key).first;
        a.port = srv.value(key).second;
        a.ip = addr.value(a.host.toLower());
        a.mac = normalMac(mac.value(key));
        out << a;
    }
    return out;
}

QString WledDiscovery::describe(const Device &device)
{
    const QString where = tr("WLED (%1)").arg(device.ip);
    if (device.leds <= 0)
        return where;
    return device.leds == 1 ? tr("%1 · 1 LED").arg(where) : tr("%1 · %2 LEDs").arg(where).arg(device.leds);
}

QString WledDiscovery::hostName(const QString &name)
{
    QString out;
    for (const QChar c : name.toLower())
    {
        if ((c >= QLatin1Char('a') && c <= QLatin1Char('z')) || (c >= QLatin1Char('0') && c <= QLatin1Char('9')))
            out += c;
        else if (!out.isEmpty() && !out.endsWith(QLatin1Char('-')))
            out += QLatin1Char('-');
    }
    out = out.left(32);
    while (out.endsWith(QLatin1Char('-')))
        out.chop(1);
    return out;
}

/*********************************************************************
 * Asking
 *********************************************************************/

void WledDiscovery::start()
{
    m_timer->start();
    browseNow();
}

void WledDiscovery::setInterval(int ms)
{
    m_timer->setInterval(qMax(50, ms));
}

void WledDiscovery::setSender(const Sender &send)
{
    m_sender = send;
}

bool WledDiscovery::mayAskFrom(const QHostAddress &local) const
{
    const InputOutputMap *io = m_doc->inputOutputMap();
    if (io == nullptr || !io->transmitGateActive())
        return true;
    return RigNetwork::contains(io->rigNetworks(), local);
}

bool WledDiscovery::isRunning() const
{
    return m_timer->isActive() || bool(m_sender);
}

void WledDiscovery::browseSoon()
{
    if (isRunning() && QDateTime::currentMSecsSinceEpoch() - m_lastBrowse > 5000)
        browseNow();
}

void WledDiscovery::browseNow()
{
    m_lastBrowse = QDateTime::currentMSecsSinceEpoch();
    reconcile();                        // what has gone quiet goes first
    const QByteArray query = browseQuery();
    if (m_sender)
    {
        m_sender(query);
        return;
    }

    /* The last round's sockets: their answers are in. One socket per
       network, from a port of its own: a WLED answers such a question
       straight back to it (RFC 6762 6.7), so nothing needs port 5353, which
       the system's own mDNS responder holds. */
    qDeleteAll(m_sockets);
    m_sockets.clear();
    const QHostAddress group(QStringLiteral("224.0.0.251"));
    foreach (const QNetworkInterface &iface, QNetworkInterface::allInterfaces())
    {
        const QNetworkInterface::InterfaceFlags f = iface.flags();
        if (!(f & QNetworkInterface::IsUp) || !(f & QNetworkInterface::IsRunning)
            || !(f & QNetworkInterface::CanMulticast) || (f & QNetworkInterface::IsLoopBack))
            continue;
        foreach (const QNetworkAddressEntry &entry, iface.addressEntries())
        {
            const QHostAddress local = entry.ip();
            if (local.protocol() != QAbstractSocket::IPv4Protocol || !mayAskFrom(local))
                continue;
            QUdpSocket *s = new QUdpSocket(this);
            if (!s->bind(local, 0))
            {
                delete s;
                continue;
            }
            s->setMulticastInterface(iface);
            s->setSocketOption(QAbstractSocket::MulticastTtlOption, 255);
            connect(s, &QUdpSocket::readyRead, this, [this, s]() {
                while (s->hasPendingDatagrams())
                {
                    const QNetworkDatagram dg = s->receiveDatagram();
                    receive(dg.data(), dg.senderAddress());
                }
            });
            s->writeDatagram(query, group, kMdnsPort);
            m_sockets << s;
        }
    }
}

void WledDiscovery::receive(const QByteArray &packet, const QHostAddress &from)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool touched = false;
    foreach (const Answer &a, parseResponse(packet))
    {
        QString ip = a.ip;
        if (ip.isEmpty() && from.protocol() == QAbstractSocket::IPv4Protocol)
            ip = QHostAddress(from.toIPv4Address()).toString();     // it answered from there
        if (ip.isEmpty())
            continue;
        if (a.port != 0 && a.port != 80)
            ip += QStringLiteral(":%1").arg(a.port);

        // Known: still there (and maybe somewhere new).
        QString key;
        for (auto it = m_devices.begin(); it != m_devices.end(); ++it)
            if ((!a.mac.isEmpty() && it->mac == a.mac) || (a.mac.isEmpty() && it->ip == ip))
                key = it.key();
        if (!key.isEmpty())
        {
            Device &d = m_devices[key];
            d.seenAt = now;
            if (d.host.isEmpty())
                d.host = a.instance;
            const bool moved = d.ip != ip;
            d.ip = ip;
            touched = touched || moved;
            if (!moved && now - m_asked.value(ip) < kInfoStaleMs)
                continue;
        }
        if (now - m_asked.value(ip) < kAskAgainMs)
            continue;
        m_asked.insert(ip, now);
        // Asked once for what it is.
        QNetworkRequest req(QUrl(WledConfigCheck::baseUrl(ip) + QStringLiteral("/json/info")));
        req.setTransferTimeout(kInfoTimeoutMs);
        QNetworkReply *r = m_nam->get(req);
        const QString host = a.instance;
        connect(r, &QNetworkReply::finished, this, [this, r, ip, host]() {
            r->deleteLater();
            if (r->error() == QNetworkReply::NoError)
                takeInfo(ip, host, r->readAll());
        });
    }
    if (touched)
        reconcile();
}

void WledDiscovery::takeInfo(const QString &ip, const QString &host, const QByteArray &body)
{
    QJsonObject info = QJsonDocument::fromJson(body).object();
    if (info.value(QStringLiteral("info")).isObject())
        info = info.value(QStringLiteral("info")).toObject();
    if (info.isEmpty())
        return;
    Device d;
    d.mac = normalMac(info.value(QStringLiteral("mac")).toString());
    d.ip = ip;
    d.host = host;
    d.name = info.value(QStringLiteral("name")).toString();
    const QJsonObject leds = info.value(QStringLiteral("leds")).toObject();
    d.leds = leds.value(QStringLiteral("count")).toInt();
    d.rgbw = leds.value(QStringLiteral("rgbw")).toBool();
    d.version = info.value(QStringLiteral("ver")).toString();
    d.seenAt = QDateTime::currentMSecsSinceEpoch();
    const QString key = keyOf(d);
    // Whatever stood at that IP before is this one now.
    for (auto it = m_devices.begin(); it != m_devices.end();)
    {
        if (it.key() != key && it->ip == ip)
            it = m_devices.erase(it);
        else
            ++it;
    }
    m_devices.insert(key, d);
    reconcile();
}

/*********************************************************************
 * What is found, against the show
 *********************************************************************/

QString WledDiscovery::keyOf(const Device &device)
{
    return device.mac.isEmpty() ? device.ip : device.mac;
}

QList<WledDiscovery::Device> WledDiscovery::devices() const
{
    QList<Device> out = m_devices.values();
    std::sort(out.begin(), out.end(), [](const Device &a, const Device &b) {
        return a.name == b.name ? a.ip < b.ip : a.name.toLower() < b.name.toLower();
    });
    return out;
}

WledDiscovery::Device WledDiscovery::device(const QString &key) const
{
    return m_devices.value(key);
}

Fixture *WledDiscovery::fixtureFor(const Device &device) const
{
    Fixture *byIp = nullptr;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr || fx->connection() != Fixture::Network)
            continue;
        if (!device.mac.isEmpty() && normalMac(fx->macAddress()) == device.mac)
            return fx;
        if (byIp == nullptr && fx->macAddress().isEmpty() && fx->ipAddress() == device.ip
            && WledConfigCheck::isWled(fx))
            byIp = fx;
    }
    return byIp;
}

QList<WledDiscovery::Device> WledDiscovery::notInShow() const
{
    QList<Device> out;
    foreach (const Device &d, devices())
        if (fixtureFor(d) == nullptr)
            out << d;
    return out;
}

QList<WledDiscovery::Move> WledDiscovery::moves() const
{
    QList<Move> out;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == nullptr || fx->macAddress().isEmpty())
            continue;
        const Device d = m_devices.value(normalMac(fx->macAddress()));
        if (d.mac.isEmpty() || d.ip == fx->ipAddress())
            continue;
        Move m;
        m.fixtureId = fx->id();
        m.from = fx->ipAddress();
        m.to = d.ip;
        out << m;
    }
    return out;
}

void WledDiscovery::reconcile()
{
    // Gone quiet: not answered for three rounds.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 quiet = 3LL * m_timer->interval() + 10000;
    for (auto it = m_devices.begin(); it != m_devices.end();)
    {
        if (now - it->seenAt > quiet)
            it = m_devices.erase(it);
        else
            ++it;
    }

    /* A WLED fixture at a found device's IP that has no MAC yet learns it:
       from now on it is that device wherever it goes. Not a change to the
       show anyone made, so it is not marked as one; it is saved with it. */
    foreach (const Device &d, m_devices)
    {
        if (d.mac.isEmpty())
            continue;
        Fixture *fx = fixtureFor(d);
        if (fx != nullptr && fx->macAddress().isEmpty())
            fx->setMacAddress(d.mac);
    }
    publish();
}

void WledDiscovery::publish()
{
    QStringList items;
    QVariantList ids;
    foreach (const Move &m, moves())
    {
        const Fixture *fx = m_doc->fixture(m.fixtureId);
        items << tr("%1 is now at %2 (was %3)").arg(fx->name(), shortIp(m.to, m.from), m.from);
        ids << m.fixtureId;
    }

    // What changed: the found rows or the moves. Nothing else is news.
    QStringList state = items;
    foreach (const Device &d, notInShow())
        state << keyOf(d) + QLatin1Char('|') + d.ip + QLatin1Char('|') + d.name
                 + QLatin1Char('|') + QString::number(d.leds);
    if (state == m_lastPublished)
        return;
    m_lastPublished = state;

    if (ShowStatus *status = ShowStatus::instance())
    {
        if (items.isEmpty())
            status->clearStatus("wled.moved");
        else
            status->setStatus("wled.moved", ShowStatus::Warning,
                items.count() == 1 ? tr("1 WLED answering at a new address")
                                   : tr("%1 WLEDs answering at a new address").arg(items.count()),
                tr("Found on the network by its hardware address. Follow It sends to its new "
                   "address (one undo)."),
                items, ids);
    }
    emit changed();
}

/*********************************************************************
 * Patch, Follow It
 *********************************************************************/

quint32 WledDiscovery::patch(QWidget *parent, const QString &key, bool offerSetUp)
{
    const Device d = m_devices.value(key);
    if (keyOf(d).isEmpty() || fixtureFor(d) != nullptr)
        return Fixture::invalidId();
    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef(QStringLiteral("WLED"), QStringLiteral("Effect Mode"));
    QLCFixtureMode *mode = nullptr;
    if (def != nullptr)
        foreach (QLCFixtureMode *m, def->modes())
            if (mode == nullptr && m->name().startsWith(QLatin1String("Effect")))
                mode = m;
    if (mode == nullptr)
    {
        if (parent != nullptr)
        {
            QMessageBox box(QMessageBox::Warning, tr("Patch WLED"),
                            tr("The WLED Effect Mode definition is not on this computer, so the "
                               "WLED can't be patched from here."), QMessageBox::Ok, parent);
            box.setObjectName(QStringLiteral("wledPatchNoDefinition"));
            box.exec();
        }
        return Fixture::invalidId();
    }

    Fixture *fx = new Fixture(m_doc);
    fx->setName(m_doc->uniqueFixtureName(d.name.trimmed().isEmpty() ? QStringLiteral("WLED") : d.name.trimmed()));
    fx->setPatched(false);              // routeWled() gives it its universe
    fx->setUniverse(0);
    fx->setAddress(0);
    fx->setConnection(Fixture::Network);
    fx->setTransport(Fixture::WledTransport);
    fx->setIPAddress(d.ip);
    fx->setMacAddress(d.mac);
    fx->setFixtureDefinition(def, mode);
    if (!m_doc->addFixture(fx))
    {
        delete fx;
        return Fixture::invalidId();
    }
    const quint32 fid = fx->id();
    if (d.leds > 0)
    {
        // The string as the device drives it (Fixture Properties ▸ LEDs).
        FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(fid);
        rp.lightLeds = d.leds;
        m_doc->monitorProperties()->setFixtureRigProps(fid, rp);
    }
    // Its own universe, sent to it: one undo, which takes the fixture out too.
    FixtureCreator::routeWled(parent, m_doc, { fid }, true);
    m_doc->setModified();
    reconcile();

    if (offerSetUp)
        if (WledConfigCheck *check = WledConfigCheck::instance())
            check->offerSetUp(parent, fid);
    return fid;
}

bool WledDiscovery::follow(QWidget *parent, quint32 fixtureId)
{
    Fixture *fx = m_doc->fixture(fixtureId);
    if (fx == nullptr)
        return false;
    QString to;
    foreach (const Move &m, moves())
        if (m.fixtureId == fixtureId)
            to = m.to;
    if (to.isEmpty())
        return false;
    const QString was = fx->ipAddress();
    fx->setIPAddress(to);

    // Its universe's output aimed at the new address (in place: its own).
    std::function<void()> undoRoute;
    QStringList problems;
    if (fx->isPatched())
    {
        PatchPlanner planner(m_doc);
        planner.routeWled({ fixtureId }, &problems, &undoRoute);
    }
    Doc *doc = m_doc;
    if (Monitor *mon = Monitor::instance())
        mon->pushUndoCallback([doc, fixtureId, was, undoRoute]() {
            if (undoRoute)
                undoRoute();
            if (Fixture *f = doc->fixture(fixtureId))
            {
                f->setIPAddress(was);
                f->setID(f->id());          // changed: every view of it
            }
        });
    fx->setID(fx->id());                    // changed: every view of it
    m_doc->setModified();
    if (!problems.isEmpty() && parent != nullptr)
    {
        QMessageBox box(QMessageBox::Warning, tr("Follow It"),
                        tr("Things to look at:\n\n%1").arg(problems.join(QStringLiteral("\n"))),
                        QMessageBox::Ok, parent);
        box.setObjectName(QStringLiteral("wledFollowBox"));
        box.exec();
    }
    reconcile();
    return true;
}
